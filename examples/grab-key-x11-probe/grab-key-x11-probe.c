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
    XSelectInput(display, window, KeyPressMask | KeyReleaseMask);
    XMapWindow(display, window);
    XSync(display, False);

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int before = x_errors;
    XGrabKey(display, AnyKey, AnyModifier, window, True,
             GrabModeAsync, GrabModeAsync);
    XSync(display, False);
    bool async_ok = x_errors == before;
    result("grab-key-async", async_ok,
           async_ok ? "GrabModeAsync" : "GrabKey async failed");
    RECORD(async_ok);
    XUngrabKey(display, AnyKey, AnyModifier, window);

    before = x_errors;
    XGrabKey(display, AnyKey, AnyModifier, window, True,
             GrabModeAsync, GrabModeSync);
    XSync(display, False);
    bool sync_ok = x_errors == before;
    result("grab-key-sync", sync_ok,
           sync_ok ? "GrabModeSync" : "GrabKey sync rejected");
    RECORD(sync_ok);

    before = x_errors;
    XGrabServer(display);
    XGrabKey(display, AnyKey, ShiftMask, window, False,
             GrabModeSync, GrabModeSync);
    XUngrabServer(display);
    XSync(display, False);
    bool grab_ok = x_errors == before;
    result("grab-key-under-server", grab_ok,
           grab_ok ? "GrabKey under GrabServer" : "blocked or error");
    RECORD(grab_ok);

    before = x_errors;
    XUngrabKey(display, AnyKey, AnyModifier, window);
    XUngrabKey(display, AnyKey, ShiftMask, window);
    XSync(display, False);
    bool ungrab_ok = x_errors == before;
    result("ungrab-key", ungrab_ok,
           ungrab_ok ? "released" : "UngrabKey error");
    RECORD(ungrab_ok);

    printf("BXSUMMARY grab-key-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
