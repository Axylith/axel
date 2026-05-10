#include "types.h"
#include "window.h"
#include "monitor.h"
#include "vulkan_init.h"
#include "swapchain.h"
#include <cstdio>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <unistd.h>
#include <thread>
#include <csetjmp>


static jmp_buf x_error_jmp;
static bool x_connection_lost = false;


static int x_io_error_handler(Display*) {
    x_connection_lost = true;
    longjmp(x_error_jmp, 1);
    return 0;  // never reached
}

int main(int, char**){
    XInitThreads();

    Timer t;
    Timer total;

    double last_sample_ms = 0.0;
    constexpr double MONITOR_INTERVAL_MS = 1000.0;

    AppWindow app = create_window(1280, 720);
    t.log("X11 window"); fflush(stdout);
    if(!app.running) return 1;

    // Create surface on main thread (touches X11)
    VkInstance instance = create_vulkan_instance();
    t.log("Vulkan instance"); fflush(stdout);
    if(instance == VK_NULL_HANDLE) return 1;

    VkSurfaceKHR surface = create_surface(instance, app);
    t.log("Vulkan surface"); fflush(stdout);
    if(surface == VK_NULL_HANDLE) return 1;
                

    ResourceMonitor monitor;
    monitor.open("axylith_resource.csv");

    VulkanState vk;
    vk.instance = instance;
    vk.surface = surface;

    std::thread init_thread([&vk, &app, &total]() {
        vk.gpu = pick_gpu(vk.instance, vk.surface);
        if (vk.gpu.device == VK_NULL_HANDLE) { vk.failed.store(true); return; }

        vk.vkdev = create_device(vk.gpu);
        if (vk.vkdev.device == VK_NULL_HANDLE) { vk.failed = true; return; }

        vk.swapchain = create_swapchain(vk.vkdev.device, vk.gpu.device, vk.surface, app, vk.gpu);
        if (vk.swapchain.handle == VK_NULL_HANDLE) { vk.failed = true; return; }

        vk.pipeline = create_pipeline(vk.vkdev.device, vk.swapchain.format);
        if (vk.pipeline.handle == VK_NULL_HANDLE) { vk.failed = true; return; }

        vk.renderer = create_renderer(vk.vkdev, vk.gpu, vk.pipeline);

        printf("[timer] %-40s %.3f ms\n", "TOTAL INIT", total.elapsed_ms());
        vk.ready = true;
    });

    Timer monitor_timer;

   XSetIOErrorHandler(x_io_error_handler);

    // Set jump point — longjmp returns here with value 1
    if (setjmp(x_error_jmp) != 0) {
        // We got here from longjmp — X connection is dead
        printf("[x11] Connection lost, shutting down cleanly\n");
        fflush(stdout);
        goto cleanup;
    }

    while (app.running){
        while (XPending(app.display)){
            XEvent ev;
            XNextEvent(app.display, &ev);

            switch (ev.type){
                case KeyPress: {
                    KeySym key = XLookupKeysym(&ev.xkey, 0);
                    if (key == XK_Escape){
                        app.running = false;
                    }
                    break;
                }

                
                case ClientMessage: {
                    printf("[x11] ClientMessage msg_type=%lu data[0]=%ld\n",
                           (unsigned long)ev.xclient.message_type,
                           (long)ev.xclient.data.l[0]);
                    fflush(stdout);
                    
                    if (ev.xclient.message_type == app.wm_protocols) {
                        printf("[x11]   matched WM_PROTOCOLS, data[0]=%ld vs wm_delete=%lu\n",
                               (long)ev.xclient.data.l[0],
                               (unsigned long)app.wm_delete);
                        fflush(stdout);
                        if ((Atom)ev.xclient.data.l[0] == app.wm_delete) {
                            printf("[x11]   closing window\n");
                            fflush(stdout);
                            app.running = false;
                        }
                    }
                    break;
                }

                

               case ConfigureNotify: {
                    if (ev.xconfigure.width != app.width || ev.xconfigure.height != app.height){
                        app.width = ev.xconfigure.width;
                        app.height = ev.xconfigure.height;
                        if (vk.ready.load()) {
                            recreate_swapchain(vk, app);
                            render_frame(vk.renderer, vk.vkdev, vk.swapchain, vk.pipeline);
                        } else {
                            vk.swapchain_dirty = true;  // queue for when ready
                        }
                    }
                    break;
                }
            }
        }

        if (vk.failed) {
            fprintf(stderr, "[vulkan] Init failed\n");
            app.running = false;
        }

        double now = monitor_timer.elapsed_ms();
        if (now - last_sample_ms >= 1000.0) {
            monitor.sample(now);
            last_sample_ms = now;
        }

        if (!vk.ready) {
            usleep(160000);
        } else {
            if(vk.swapchain_dirty){
                recreate_swapchain(vk, app);
                vk.swapchain_dirty = false;
            }
            render_frame(vk.renderer, vk.vkdev, vk.swapchain, vk.pipeline);
            if(vk.renderer.swapchain_dirty_local){
                vk.swapchain_dirty = true;
                vk.renderer.swapchain_dirty_local = false;
            }
        }
    }

cleanup:
    monitor.close();

    init_thread.join();

    if (vk.ready) {
        vkDeviceWaitIdle(vk.vkdev.device);
        vkDestroyPipeline(vk.vkdev.device, vk.pipeline.handle, nullptr);
        vkDestroyPipelineLayout(vk.vkdev.device, vk.pipeline.layout, nullptr);
        vkDestroyBuffer(vk.vkdev.device, vk.renderer.vertex_buffer, nullptr);
        vkFreeMemory(vk.vkdev.device, vk.renderer.vertex_memory, nullptr);
        vkDestroySemaphore(vk.vkdev.device, vk.renderer.render_finished, nullptr);
        vkDestroySemaphore(vk.vkdev.device, vk.renderer.image_available, nullptr);
        vkDestroyFence(vk.vkdev.device, vk.renderer.in_flight, nullptr);
        vkDestroyCommandPool(vk.vkdev.device, vk.renderer.command_pool, nullptr);
        vkDeviceWaitIdle(vk.vkdev.device);
        for(uint32_t i = 0; i < vk.swapchain.image_count; i++){
            vkDestroyImageView(vk.vkdev.device, vk.swapchain.views[i], nullptr);
        }
        vkDestroySwapchainKHR(vk.vkdev.device, vk.swapchain.handle, nullptr);
        vkDestroyDevice(vk.vkdev.device, nullptr);
    }
    vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);
    vkDestroyInstance(vk.instance, nullptr);

    // Only touch X11 if connection is still alive
    if (!x_connection_lost) {
        XDestroyWindow(app.display, app.window);
        XSync(app.display, False);
        //Note: XCloseDisplay can race with NVIDIA driver shutdown
        //The OS reclaims the connection on process exit anyway
        //XCloseDisplay(app.display);
    }

    printf("[cleanup] Shutdown complete\n"); fflush(stdout);
    return 0;
}