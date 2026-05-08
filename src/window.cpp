#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xresource.h>
#include <cstdio>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib.h>
#include <chrono>




// Put this helper at the top after includes
struct Timer {
    std::chrono::high_resolution_clock::time_point start;
    
    Timer() : start(std::chrono::high_resolution_clock::now()) {}
    
    double elapsed_ms() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - start).count();
    }
    
    void log(const char* label) {
        printf("[timer] %-40s %.3f ms\n", label, elapsed_ms());
        start = std::chrono::high_resolution_clock::now(); // reset for next measurement
    }
};


struct AppWindow
{
    Display* display;
    Window window;
    Atom wm_delete;
    int width;
    int height;
    bool running;
};



struct GPU {
    VkPhysicalDevice device;
    uint32_t graphics_queue_family;
    uint32_t present_queue_family;
};

struct VulkanDevice {
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;  
};


struct Swapchain {
    VkSwapchainKHR handle;
    VkImage images[4];
    VkImageView views[4];
    uint32_t image_count;
    VkFormat format;
    VkExtent2D extent;
};

Swapchain create_swapchain(VkDevice device, VkPhysicalDevice physical, VkSurfaceKHR surface, AppWindow& app, GPU& gpu){
    //Query what the surface supports
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps);

    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &format_count, nullptr);
    VkSurfaceFormatKHR formats[16];
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &format_count, formats);

    VkSurfaceFormatKHR chosen_format = formats[0]; //fallback
    for (uint32_t i =0; i < format_count; i++){
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){
            chosen_format = formats[i];
            break;
        }
    }

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

    //Pick extent (size)
    VkExtent2D extent;
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent.width = app.width;
        extent.height = app.height;
    }

    // Pick image count (double or triple buffering)
    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = chosen_format.format;
    create_info.imageColorSpace = chosen_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.preTransform = caps.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    uint32_t family_indices[] = { gpu.graphics_queue_family, gpu.present_queue_family};

    if (gpu.graphics_queue_family != gpu.present_queue_family){
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    Swapchain sc{};
    sc.format = chosen_format.format;
    sc.extent = extent;

    VkResult result = vkCreateSwapchainKHR(device, &create_info, nullptr, &sc.handle);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "[vulkan] Failed to create swapchain: %d\n", result);
        return sc;
    }

    // Get swapchain images
    vkGetSwapchainImagesKHR(device, sc.handle, &sc.image_count, nullptr);
    vkGetSwapchainImagesKHR(device, sc.handle, &sc.image_count, sc.images);

    for (uint32_t i = 0; i < sc.image_count; i++){
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = sc.images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = sc.format;
        view_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY,
                             VK_COMPONENT_SWIZZLE_IDENTITY,
                             VK_COMPONENT_SWIZZLE_IDENTITY,
                             VK_COMPONENT_SWIZZLE_IDENTITY };
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        
        vkCreateImageView(device, &view_info, nullptr, &sc.views[i]);
                            
    }

    printf("[vulkan] Swapchain created (%ux%u %u images)\n", extent.width, extent.height, sc.image_count);
    return sc;
}



GPU pick_gpu(VkInstance instance, VkSurfaceKHR surface){
    //how many gpu:
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count,nullptr);
    //First cal with nullptr = "just tell me how many"

    VkPhysicalDevice devices[8]; //8 GPUS max, more than enough
    vkEnumeratePhysicalDevices(instance, &count, devices);

    printf("[vulkan] Found %u GPU(s)\n", count);

    for (uint32_t i = 0; i < count; i++){
        //Get GPU Name
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);
        printf("[vulkan] %u: %s\n", i, props.deviceName);

        //Find queue families
        uint32_t family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &family_count, nullptr);
        
        VkQueueFamilyProperties families[16];
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &family_count, families);

        GPU gpu{};
        gpu.device = devices[i];
        gpu.graphics_queue_family = UINT32_MAX;
        gpu.present_queue_family = UINT32_MAX;

        for (uint32_t f = 0; f < family_count; f++){
            //Can this family do graphics
            if (families[f].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                gpu.graphics_queue_family = f;
            }

            //Can this family present to our surface?
            VkBool32 can_present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], f, surface, &can_present);
            if(can_present){
                gpu.present_queue_family = f;
            }
        }
    

        // Found a GPU that can do Both? USE IT
        if(gpu.graphics_queue_family != UINT32_MAX && gpu.present_queue_family != UINT32_MAX){
            printf("[vulkan] Using: %s\n", props.deviceName);
            return gpu;
        }
    }

    fprintf(stderr, "[vulkan] No suitable GPU found \n");
    return GPU{};
}

VulkanDevice create_device(GPU& gpu){
    //We need one queue from the graphics family
    float priority = 1.0f;

    VkDeviceQueueCreateInfo queue_info[2]{};
    uint32_t queue_count = 1;

    queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[0].queueFamilyIndex = gpu.graphics_queue_family;
    queue_info[0].queueCount = 1;
    queue_info[0].pQueuePriorities = &priority;

    // If present is a different family, request it too
    if (gpu.present_queue_family != gpu.graphics_queue_family) {
        queue_info[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[1].queueFamilyIndex = gpu.present_queue_family;
        queue_info[1].queueCount = 1;
        queue_info[1].pQueuePriorities = &priority;
        queue_count = 2;
    }

    //Enable swapchain extension (required to present to screen)
    const char* extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkPhysicalDeviceSynchronization2Features sync2{};
    sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2.synchronization2 = VK_TRUE;

    VkPhysicalDeviceDynamicRenderingFeatures dyn_rendering{};
    dyn_rendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dyn_rendering.dynamicRendering = VK_TRUE;
    dyn_rendering.pNext = &sync2; // chain them to gether

    VkDeviceCreateInfo device_info{};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pNext = &dyn_rendering; //Attach chain feature;
    device_info.queueCreateInfoCount = queue_count;
    device_info.pQueueCreateInfos = queue_info;
    device_info.enabledExtensionCount = 1;
    device_info.ppEnabledExtensionNames = extensions;

    VulkanDevice vkdev{};
    VkResult result = vkCreateDevice(gpu.device, &device_info, nullptr, &vkdev.device);
    
    if (result != VK_SUCCESS){
        fprintf(stderr, "[vulkan] Failed to create device: %d\n", result);
        return vkdev;
    }


    //Get Queue handlers
    vkGetDeviceQueue(vkdev.device, gpu.graphics_queue_family, 0, &vkdev.graphics_queue);
    vkGetDeviceQueue(vkdev.device, gpu.present_queue_family, 0, &vkdev.present_queue);

    printf("[vulkan] Device created (dynamic rendering + sync2 enabled)\n");
    return vkdev;



}




AppWindow create_window(int width, int height){
    AppWindow app{};

    // -- 1. Connect to X11 display server
    app.display = XOpenDisplay(NULL);
    
    if (!app.display) {
        fprintf(stderr, "Unable to connect to display\n");
        app.running = false;
        return app;
    }

    // -- 2. Get Screen Information --
    int screen = DefaultScreen(app.display);


    app.window = XCreateSimpleWindow(
        app.display,                        // Connection to the Display server 
        DefaultRootWindow(app.display),     // Parent
        0,0,                        // x, y position
        width, height,              // Size
        1,                          // Border size
        WhitePixel(app.display, screen),    // Border color
        BlackPixel(app.display, screen)     // Background color
    );

    // ── 4. Set window title ──
    XStoreName(app.display, app.window, "Axylith");


    // ── 4. Set window title ──
    XSelectInput(app.display, app.window,
        KeyPressMask |
        ButtonPressMask |
        ExposureMask |
        StructureNotifyMask
    );

    app.wm_delete = XInternAtom(app.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(app.display, app.window, &app.wm_delete,1);

    // ── 7. Show the window ──
    XMapWindow(app.display,app.window);

    printf("[axylith] Window created (%dx%d)\n", width, height);

    app.width = width;
    app.height = height;
    app.running = true;

    return app;
    
}


VkInstance create_vulkan_instance(){
    //App info -tells drivers who we are
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "axylith";
    appInfo.applicationVersion = VK_MAKE_VERSION(0,1,0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0,1,0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    const char* extension[] = {
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME
    };

    

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = 2;
    createInfo.ppEnabledExtensionNames = extension;

    // In create_vulkan_instance, make layers conditional:
    #ifdef NDEBUG
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
    #else
        const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = layers;
    #endif

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

    if(result != VK_SUCCESS){
        fprintf(stderr, "[vulkan] failed to create instance: %d\n", result);
        return VK_NULL_HANDLE;
    }

    printf("[vulkan] Instance created (Vulkan 1.3)\n");
    return instance;


}

VkSurfaceKHR create_surface(VkInstance instance, AppWindow& app){
    VkXlibSurfaceCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    create_info.dpy = app.display;
    create_info.window = app.window;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult result = vkCreateXlibSurfaceKHR(instance, &create_info, nullptr, &surface);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "[vulkan] Failed to create surface: %d\n", result);
        return VK_NULL_HANDLE;
    }

    printf("[vulkan] Surface created\n");
    return surface;
}




int main(int argc, char **argv){

    Timer t;
    Timer total;

    AppWindow app = create_window(1280, 720);
    t.log("X11 window"); fflush(stdout);
    if (!app.running) return 1;

    VkInstance instance = create_vulkan_instance();
    t.log("Vulkan instance"); fflush(stdout);
    if (instance == VK_NULL_HANDLE) return 1;

    VkSurfaceKHR surface = create_surface(instance, app);
    t.log("Vulkan surface"); fflush(stdout);
    if (surface == VK_NULL_HANDLE) return 1;

    GPU gpu = pick_gpu(instance, surface);
    t.log("GPU selection"); fflush(stdout);
    if (gpu.device == VK_NULL_HANDLE) return 1;

    VulkanDevice vkdev = create_device(gpu);
    t.log("Device creation"); fflush(stdout);
    if (vkdev.device == VK_NULL_HANDLE) return 1;

    Swapchain swapchain = create_swapchain(vkdev.device, gpu.device, surface, app, gpu);
    t.log("Swapchain Creation"); fflush(stdout);
    if (swapchain.handle == VK_NULL_HANDLE) return 1;

    printf("[timer] %-40s %.3f ms\n", "TOTAL INIT", total.elapsed_ms());

    // event loop...


    // --- 8. Event Loop ---
    
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
                    // X button clicked
                    if ((Atom)ev.xclient.data.l[0] == app.wm_delete){
                        app.running = false;
                    }
                    break;
                }

                case ConfigureNotify: {
                    app.width = ev.xconfigure.width;
                    app.height = ev.xconfigure.height;
                    break;
                }
                
            }
        }
    }

    //---CLEAN UP----
    // Destroy in REVERSE order of creation
    //---CLEAN UP----
    for(uint32_t i = 0; i < swapchain.image_count; i++){
        vkDestroyImageView(vkdev.device, swapchain.views[i], nullptr);
    }
    vkDestroySwapchainKHR(vkdev.device, swapchain.handle, nullptr);

    printf("[cleanup] Waiting for device idle...\n"); fflush(stdout);
    vkDeviceWaitIdle(vkdev.device);

    printf("[cleanup] Destroying device...\n"); fflush(stdout);
    vkDestroyDevice(vkdev.device, nullptr);

    printf("[cleanup] Destroying surface...\n"); fflush(stdout);
    vkDestroySurfaceKHR(instance, surface, nullptr);

    printf("[cleanup] Destroying instance...\n"); fflush(stdout);
    vkDestroyInstance(instance, nullptr);

    //printf("[cleanup] Destroying X11 window...\n"); fflush(stdout);
    //XDestroyWindow(app.display, app.window);

    //printf("[cleanup] Closing X11 display...\n"); fflush(stdout);
    //XCloseDisplay(app.display);

    printf("[cleanup] Shutdown complete\n"); fflush(stdout);
    return 0;



    return 0;

}
