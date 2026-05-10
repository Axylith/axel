#pragma once
#include <X11/Xlib.h>


struct AppWindow
{
    Display* display;
    Window window;
    Atom wm_delete;
    Atom wm_protocols;
    int width;
    int height;
    bool running;
};

AppWindow create_window(int width, int height);