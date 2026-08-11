#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gnu/libc-version.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void rectangle(uint32_t *pixels, int stride, int x, int y,
                      int width, int height, uint32_t color) {
    for (int row = y; row < y + height; ++row)
        for (int column = x; column < x + width; ++column)
            pixels[row * stride + column] = color;
}

int main(void) {
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "hello-x11: XOpenDisplay(%s) failed\n", getenv("DISPLAY"));
        return 1;
    }

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    const int width = 900;
    const int height = 480;
    Window window = XCreateSimpleWindow(display, root, 80, 80, width, height, 2,
            BlackPixel(display, screen), WhitePixel(display, screen));
    XStoreName(display, window, "BionicX: real glibc + libX11 client");
    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);

    uint32_t *pixels = calloc((size_t)width * height, sizeof(*pixels));
    if (pixels == NULL) return 2;
    rectangle(pixels, width, 0, 0, width, height, 0x00f4f7fb);
    rectangle(pixels, width, 80, 90, 740, 300, 0x00172b4d);
    rectangle(pixels, width, 120, 130, 660, 80, 0x0000a884);
    rectangle(pixels, width, 120, 250, 420, 28, 0x00ffffff);
    rectangle(pixels, width, 120, 305, 560, 28, 0x00ffffff);

    XImage *image = XCreateImage(display, DefaultVisual(display, screen),
            (unsigned)DefaultDepth(display, screen), ZPixmap, 0,
            (char *)pixels, width, height, 32, 0);
    if (image == NULL) return 3;
    GC gc = XCreateGC(display, window, 0, NULL);

    printf("hello-x11: glibc=%s pid=%ld DISPLAY=%s vendor=%s\n",
            gnu_get_libc_version(), (long)getpid(), DisplayString(display),
            ServerVendor(display));
    fflush(stdout);
    XPutImage(display, window, gc, image, 0, 0, 0, 0, width, height);
    XFlush(display);
    for (;;) {
        XEvent event;
        XNextEvent(display, &event);
        if (event.type == Expose && event.xexpose.count == 0) {
            XPutImage(display, window, gc, image, 0, 0, 0, 0, width, height);
            XFlush(display);
        }
        if (event.type == KeyPress) break;
    }

    XDestroyImage(image);
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}
