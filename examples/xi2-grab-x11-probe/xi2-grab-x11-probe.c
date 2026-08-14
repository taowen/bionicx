#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int x_errors;

static int on_x_error(Display *display, XErrorEvent *event) {
    char text[128];
    XGetErrorText(display, event->error_code, text, sizeof(text));
    fprintf(stderr, "BXERROR code=%u request=%u minor=%u resource=0x%lx %s\n",
            event->error_code, event->request_code, event->minor_code,
            event->resourceid, text);
    ++x_errors;
    return 0;
}

static void result(const char *name, bool ok, const char *detail) {
    printf("BXTEST %s %s%s%s\n", ok ? "PASS" : "FAIL", name,
           detail && *detail ? " " : "", detail ? detail : "");
    fflush(stdout);
}

int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 3;
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "BXFAIL open X11 connection\n");
        return 2;
    }
    XSetErrorHandler(on_x_error);

    int opcode = 0;
    int event_base = 0;
    int error_base = 0;
    int major = 2;
    int minor = 0;
    if (!XQueryExtension(display, "XInputExtension", &opcode, &event_base,
                         &error_base)
            || XIQueryVersion(display, &major, &minor) != Success) {
        fprintf(stderr, "BXFAIL XI2 unavailable\n");
        XCloseDisplay(display);
        return 2;
    }

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    Window window = XCreateSimpleWindow(display, root, 40, 40, 200, 120, 0,
                                        0, 0x336699);
    XSelectInput(display, window, ButtonPressMask | ButtonReleaseMask);
    XMapWindow(display, window);
    XSync(display, False);

    unsigned char mask_bytes[XIMaskLen(XI_LASTEVENT)] = {0};
    XISetMask(mask_bytes, XI_ButtonPress);
    XISetMask(mask_bytes, XI_ButtonRelease);
    XIEventMask mask = {
        .deviceid = 2,
        .mask_len = sizeof(mask_bytes),
        .mask = mask_bytes,
    };

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int status = XIGrabDevice(display, 2, window, CurrentTime, None,
                              XIGrabModeAsync, XIGrabModeAsync, True, &mask);
    XSync(display, False);
    bool async_ok = status == GrabSuccess && x_errors == 0;
    result("xi2-grab-async", async_ok,
           async_ok ? "GrabSuccess" : "async XIGrabDevice failed");
    RECORD(async_ok);
    XIUngrabDevice(display, 2, CurrentTime);

    int before = x_errors;
    status = XIGrabDevice(display, 2, window, 1, None,
                          XIGrabModeSync, XIGrabModeSync, True, &mask);
    XSync(display, False);
    bool sync_ok = status == GrabSuccess && x_errors == before;
    result("xi2-grab-sync", sync_ok,
           sync_ok ? "XIGrabModeSync+timestamp" : "sync grab rejected");
    RECORD(sync_ok);

    before = x_errors;
    XIUngrabDevice(display, 2, 1);
    XSync(display, False);
    bool ungrab_ok = x_errors == before;
    result("xi2-ungrab-timestamp", ungrab_ok,
           ungrab_ok ? "nonzero timestamp" : "XIUngrabDevice rejected");
    RECORD(ungrab_ok);

    before = x_errors;
    XGrabServer(display);
    status = XIGrabDevice(display, 2, window, 1, None,
                          XIGrabModeSync, XIGrabModeSync, False, &mask);
    XIUngrabDevice(display, 2, 1);
    XUngrabServer(display);
    XSync(display, False);
    bool grab_ok = status == GrabSuccess && x_errors == before;
    result("xi2-grab-under-server", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or error");
    RECORD(grab_ok);

    printf("BXSUMMARY xi2-grab-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
