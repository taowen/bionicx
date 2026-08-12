#define _GNU_SOURCE

#include <X11/Xlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void sleep_milliseconds(long milliseconds) {
    struct timespec delay = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = milliseconds % 1000 * 1000 * 1000,
    };
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
}

static void paint(Display* display, Window window, GC gc, int width,
                  int height, unsigned long color, const char* title) {
    XSetForeground(display, gc, color);
    XFillRectangle(display, window, gc, 0, 0, (unsigned int)width,
                   (unsigned int)height);
    XSetForeground(display, gc, 0xffffff);
    XDrawString(display, window, gc, 32, 54, title, (int)strlen(title));
    const char* detail = "real glibc X11 client managed by IceWM";
    XDrawString(display, window, gc, 32, 86, detail, (int)strlen(detail));
    XFlush(display);
}

int main(int argc, char** argv) {
    const char* title = argc > 1 ? argv[1] : "BionicX desktop window";
    int x = argc > 2 ? atoi(argv[2]) : 100;
    int y = argc > 3 ? atoi(argv[3]) : 100;
    unsigned long color = argc > 4 ? strtoul(argv[4], NULL, 0) : 0x285078;
    int duration = argc > 5 ? atoi(argv[5]) : 20;

    Display* display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "BXICEWM FAIL client-display title=%s\n", title);
        return 2;
    }
    int screen = DefaultScreen(display);
    int width = 620;
    int height = 310;
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
            x, y, (unsigned int)width, (unsigned int)height, 0,
            BlackPixel(display, screen), color);
    XStoreName(display, window, title);
    XSelectInput(display, window, ExposureMask | StructureNotifyMask |
            FocusChangeMask);
    GC gc = XCreateGC(display, window, 0, NULL);
    XMapWindow(display, window);
    XFlush(display);

    int mapped = 0;
    int configured = 0;
    int focused = 0;
    for (int tick = 0; tick < duration * 20; ++tick) {
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == MapNotify) mapped = 1;
            if (event.type == FocusIn) focused++;
            if (event.type == ConfigureNotify) {
                width = event.xconfigure.width;
                height = event.xconfigure.height;
                configured++;
            }
            if (event.type == Expose || event.type == ConfigureNotify)
                paint(display, window, gc, width, height, color, title);
        }
        sleep_milliseconds(50);
    }

    printf("BXICEWM client title=%s mapped=%d configured=%d focused=%d size=%dx%d\n",
           title, mapped, configured, focused, width, height);
    fflush(stdout);
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return mapped && configured > 0 ? 0 : 3;
}
