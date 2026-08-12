#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

static int x_errors;
static bool expect_bad_access;
static int expected_bad_access;

static int on_x_error(Display *display, XErrorEvent *event) {
    if (expect_bad_access && event->error_code == BadAccess
            && event->request_code == 28) {
        ++expected_bad_access;
        return 0;
    }
    char text[128];
    XGetErrorText(display, event->error_code, text, sizeof(text));
    fprintf(stderr, "BXERROR x11 code=%u request=%u resource=0x%lx text=%s\n",
            event->error_code, event->request_code, event->resourceid, text);
    ++x_errors;
    return 0;
}

static double now_seconds(void) {
    struct timespec value;
    clock_gettime(CLOCK_MONOTONIC, &value);
    return value.tv_sec + value.tv_nsec / 1000000000.0;
}

static Window make_window(Display *display, int x, unsigned long color,
                          const char *title) {
    int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
            x, 120, 700, 330, 3, BlackPixel(display, screen), color);
    XStoreName(display, window, title);
    XSelectInput(display, window, ButtonPressMask | ButtonReleaseMask);
    XMapWindow(display, window);
    return window;
}

static void drain(Display *display) {
    XEvent event;
    while (XPending(display)) XNextEvent(display, &event);
}

static bool wait_for_click(Display *wanted_display, Window wanted_window,
                           Display *other_display, double timeout_seconds) {
    double deadline = now_seconds() + timeout_seconds;
    bool pressed = false;
    bool released = false;
    bool leaked = false;
    bool state_valid = true;
    while (now_seconds() < deadline && !released) {
        Display *displays[] = {wanted_display, other_display};
        for (size_t i = 0; i < 2; ++i) {
            while (XPending(displays[i])) {
                XEvent event;
                XNextEvent(displays[i], &event);
                if (event.type != ButtonPress && event.type != ButtonRelease)
                    continue;
                printf("BXEVENT pointer source=%s type=%s window=0x%lx button=%u state=0x%x wanted=0x%lx\n",
                        displays[i] == wanted_display ? "wanted" : "other",
                        event.type == ButtonPress ? "press" : "release",
                        event.xbutton.window, event.xbutton.button,
                        event.xbutton.state,
                        wanted_window);
                fflush(stdout);
                if (displays[i] == wanted_display
                        && event.xbutton.window == wanted_window
                        && event.xbutton.button == Button1) {
                    if (event.type == ButtonPress) pressed = true;
                    if (event.type == ButtonRelease) released = true;
                    if (event.type == ButtonPress && event.xbutton.state != 0)
                        state_valid = false;
                    if (event.type == ButtonRelease
                            && event.xbutton.state != Button1Mask)
                        state_valid = false;
                }
                else leaked = true;
            }
        }
        if (released) break;
        fd_set descriptors;
        FD_ZERO(&descriptors);
        int wanted_fd = ConnectionNumber(wanted_display);
        int other_fd = ConnectionNumber(other_display);
        FD_SET(wanted_fd, &descriptors);
        FD_SET(other_fd, &descriptors);
        int max_fd = wanted_fd > other_fd ? wanted_fd : other_fd;
        struct timeval timeout = {.tv_sec = 0, .tv_usec = 50000};
        (void)select(max_fd + 1, &descriptors, NULL, NULL, &timeout);
    }
    return pressed && released && !leaked && state_valid;
}

int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 4;
    Display *grabber = XOpenDisplay(NULL);
    Display *peer = XOpenDisplay(NULL);
    if (!grabber || !peer) {
        fprintf(stderr, "BXFAIL open two X11 connections\n");
        return 2;
    }
    XSetErrorHandler(on_x_error);
    Window grabber_window = make_window(grabber, 80, 0x00dff4ee,
            "BionicX passive pointer grabber");
    Window peer_window = make_window(peer, 850, 0x00e8efff,
            "BionicX normal pointer peer");
    Window root = RootWindow(grabber, DefaultScreen(grabber));
    XSync(grabber, False);
    XSync(peer, False);
    drain(grabber);
    drain(peer);

    Cursor grab_cursor = XCreateFontCursor(grabber, XC_crosshair);
    XSync(grabber, False);
    bool glyph_cursor_ok = grab_cursor != None && x_errors == 0;

    XGrabButton(grabber, AnyButton, AnyModifier, root, False,
            ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync,
            None, grab_cursor);
    XSync(grabber, False);
    expect_bad_access = true;
    XGrabButton(peer, AnyButton, AnyModifier,
            RootWindow(peer, DefaultScreen(peer)), False,
            ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync,
            None, None);
    XSync(peer, False);
    expect_bad_access = false;
    bool contention_ok = expected_bad_access == 1;
    printf("BXREADY passive-button-grab tap-peer\n");
    fflush(stdout);
    bool passive_route_ok = wait_for_click(grabber, root, peer, 5.0);

    XUngrabButton(grabber, AnyButton, AnyModifier, root);
    XSync(grabber, False);
    drain(grabber);
    drain(peer);

    XGrabButton(grabber, AnyButton, AnyModifier, root, True,
            ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync,
            None, None);
    XSync(grabber, False);
    printf("BXREADY passive-button-owner-events tap-grabber\n");
    fflush(stdout);
    bool owner_events_ok = wait_for_click(grabber, grabber_window, peer, 5.0);
    XUngrabButton(grabber, AnyButton, AnyModifier, root);
    XSync(grabber, False);
    drain(grabber);
    drain(peer);

    printf("BXREADY passive-button-ungrab tap-peer\n");
    fflush(stdout);
    bool ungrab_route_ok = wait_for_click(peer, peer_window, grabber, 5.0);

    printf("BXTEST %s passive-button-grab route=%d\n",
            passive_route_ok ? "PASS" : "FAIL", passive_route_ok);
    printf("BXTEST %s glyph-cursor-grab resource=%lu\n",
            glyph_cursor_ok ? "PASS" : "FAIL", grab_cursor);
    printf("BXTEST %s passive-button-contention bad-access=%d\n",
            contention_ok ? "PASS" : "FAIL", expected_bad_access);
    printf("BXTEST %s passive-button-owner-events normal-route=%d\n",
            owner_events_ok ? "PASS" : "FAIL", owner_events_ok);
    printf("BXTEST %s passive-button-ungrab normal-route=%d\n",
            ungrab_route_ok ? "PASS" : "FAIL", ungrab_route_ok);
    bool passed = glyph_cursor_ok && passive_route_ok && contention_ok && owner_events_ok
            && ungrab_route_ok && x_errors == 0;
    printf("BXSUMMARY pointer-grab-x11 passed=%d/5 xerrors=%d\n",
            passed ? 5 : 0, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 60 ? duration : 4));

    XDestroyWindow(peer, peer_window);
    XFreeCursor(grabber, grab_cursor);
    XDestroyWindow(grabber, grabber_window);
    XCloseDisplay(peer);
    XCloseDisplay(grabber);
    return passed ? 0 : 1;
}
