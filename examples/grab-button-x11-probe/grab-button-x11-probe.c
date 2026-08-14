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
    XSelectInput(display, window, ButtonPressMask | ButtonReleaseMask);
    XMapWindow(display, window);
    XSync(display, False);

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int before = x_errors;
    XGrabButton(display, AnyButton, AnyModifier, window, True,
                ButtonPressMask | ButtonReleaseMask,
                GrabModeAsync, GrabModeAsync, None, None);
    XSync(display, False);
    bool async_ok = x_errors == before;
    result("grab-button-async", async_ok,
           async_ok ? "GrabModeAsync" : "GrabButton async failed");
    RECORD(async_ok);
    XUngrabButton(display, AnyButton, AnyModifier, window);

    before = x_errors;
    XGrabButton(display, AnyButton, AnyModifier, window, True,
                ButtonPressMask | ButtonReleaseMask,
                GrabModeSync, GrabModeSync, None, None);
    XSync(display, False);
    bool sync_ok = x_errors == before;
    result("grab-button-sync", sync_ok,
           sync_ok ? "GrabModeSync" : "GrabButton sync rejected");
    RECORD(sync_ok);

    before = x_errors;
    XGrabServer(display);
    XGrabButton(display, Button1, ShiftMask, window, False,
                ButtonPressMask, GrabModeSync, GrabModeSync, None, None);
    XUngrabServer(display);
    XSync(display, False);
    bool grab_ok = x_errors == before;
    result("grab-button-under-server", grab_ok,
           grab_ok ? "GrabButton under GrabServer" : "blocked or error");
    RECORD(grab_ok);

    before = x_errors;
    XUngrabButton(display, AnyButton, AnyModifier, window);
    XUngrabButton(display, Button1, ShiftMask, window);
    XSync(display, False);
    bool ungrab_ok = x_errors == before;
    result("ungrab-button", ungrab_ok,
           ungrab_ok ? "released" : "UngrabButton error");
    RECORD(ungrab_ok);

    printf("BXSUMMARY grab-button-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
