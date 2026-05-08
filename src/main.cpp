#include "types.h"
#include "window.h"
#include "monitor.h"
#include "vulkan_init.h"
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

    AppWindow app = create_window(1280, 720);
    t.log("X11 window"); fflush(stdout);
    if(!app.running) return 1;

    // Create surface on main thread (touches X11)
    VkInstance instance = create_vulkan_instance();
    t.log("Vulkan instance"); fflush(stdout);
    if(instance == VK_NULL_HANDLE) return 1;

    VkSurfaceKHR surface = create_surface(instance, app);
    t.log("Vulkan surface"); fflush(stdout);
    if(instance == VK_NULL_HANDLE) return 1;

    

    app.wm_delete = XInternAtom(app.display, "WM_DELETE_WINDOW", False);
    printf("[debug] wm_delete atom: %lu\n", (unsigned long)app.wm_delete);
                

    ResourceMonitor monitor;
    monitor.open("axylith_resource.csv");

    VulkanState vk;
    vk.instance = instance;
    vk.surface = surface;

    std::thread init_thread([&vk, &app, &total]() {
        vk.gpu = pick_gpu(vk.instance, vk.surface);
        if (vk.gpu.device == VK_NULL_HANDLE) { vk.failed = true; return; }

        vk.vkdev = create_device(vk.gpu);
        if (vk.vkdev.device == VK_NULL_HANDLE) { vk.failed = true; return; }

        vk.swapchain = create_swapchain(vk.vkdev.device, vk.gpu.device, vk.surface, app, vk.gpu);
        if (vk.swapchain.handle == VK_NULL_HANDLE) { vk.failed = true; return; };
        
        vk.renderer = create_renderer(vk.vkdev, vk.gpu);

        printf("[timer] %-40s %.3f ms\n", "TOTAL INIT", total.elapsed_ms());
        vk.ready = true;
    });

    Timer monitor_timer;
    int sample_counter = 0;

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
                    // Treat any ClientMessage as close request
                    app.running = false;
                    break;
                }

                case ConfigureNotify: {
                    app.width = ev.xconfigure.width;
                    app.height = ev.xconfigure.height;
                    break;
                }
            }
        }

        if (vk.failed) {
            fprintf(stderr, "[vulkan] Init failed\n");
            app.running = false;
        }

        sample_counter++;
        if (sample_counter >= 10) {
            monitor.sample(monitor_timer.elapsed_ms());
            sample_counter = 0;
        }

        if (!vk.ready) {
            usleep(160000);
        } else {
            render_frame(vk.renderer, vk.vkdev, vk.swapchain);
        }
    }

cleanup:
    monitor.close();

    init_thread.join();

    if (vk.ready) {
        vkDeviceWaitIdle(vk.vkdev.device);
        vkDestroySemaphore(vk.vkdev.device, vk.renderer.render_complete, nullptr);
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
        XCloseDisplay(app.display);
    }

    printf("[cleanup] Shutdown complete\n"); fflush(stdout);
    return 0;
}