#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdio>
#include <vulkan/vulkan.h>
#include "window.h"


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

    // ── 7. Show the window ──
    XMapWindow(app.display,app.window);

    printf("[axylith] Window created (%dx%d)\n", width, height);

    app.width = width;
    app.height = height;
    app.running = true;

    return app;
    
}
