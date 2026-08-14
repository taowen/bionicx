#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
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
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    Window window = XCreateSimpleWindow(display, root, 40, 40, 200, 120, 0,
                                        0, 0x336699);
    XSelectInput(display, window, ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask);
    XMapWindow(display, window);
    XSync(display, False);

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int status = XGrabPointer(display, window, True,
                              ButtonPressMask | ButtonReleaseMask,
                              GrabModeAsync, GrabModeAsync, None, None,
                              CurrentTime);
    XSync(display, False);
    bool async_ok = status == GrabSuccess && x_errors == 0;
    result("grab-pointer-async", async_ok,
           async_ok ? "GrabSuccess" : "async grab failed");
    RECORD(async_ok);
    XUngrabPointer(display, CurrentTime);

    int before = x_errors;
    status = XGrabPointer(display, window, True,
                          ButtonPressMask | ButtonReleaseMask,
                          GrabModeSync, GrabModeAsync, None, None, 1);
    XSync(display, False);
    bool sync_ok = status == GrabSuccess && x_errors == before;
    result("grab-pointer-sync", sync_ok,
           sync_ok ? "GrabModeSync+timestamp" : "sync grab rejected");
    RECORD(sync_ok);

    before = x_errors;
    XUngrabPointer(display, 1);
    XSync(display, False);
    bool ungrab_ok = x_errors == before;
    result("ungrab-pointer-timestamp", ungrab_ok,
           ungrab_ok ? "nonzero timestamp" : "UngrabPointer rejected");
    RECORD(ungrab_ok);

    before = x_errors;
    XGrabServer(display);
    status = XGrabPointer(display, window, False,
                          ButtonPressMask, GrabModeSync, GrabModeSync,
                          None, None, 1);
    XUngrabPointer(display, 1);
    XUngrabServer(display);
    XSync(display, False);
    bool grab_ok = status == GrabSuccess && x_errors == before;
    result("grab-pointer-under-server", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or error");
    RECORD(grab_ok);

    printf("BXSUMMARY grab-pointer-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
