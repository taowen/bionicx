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

static bool grab_button(Display *display, Window window, int grab_mode,
                        XIEventMask *mask, XIGrabModifiers *mods) {
    mods->modifiers = XIAnyModifier;
    mods->status = -1;
    int failed = XIGrabButton(display, 2, XIAnyButton, window, None,
                              grab_mode, grab_mode, True, mask, 1, mods);
    return failed == 0;
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
    XIGrabModifiers mods = {0};

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    bool async_ok = grab_button(display, window, XIGrabModeAsync, &mask, &mods)
            && x_errors == 0;
    XSync(display, False);
    async_ok = async_ok && x_errors == 0;
    result("xi2-passive-async", async_ok,
           async_ok ? "XIGrabButton Async" : "async passive grab failed");
    RECORD(async_ok);
    XIUngrabButton(display, 2, XIAnyButton, window, 1, &mods);

    int before = x_errors;
    bool sync_ok = grab_button(display, window, XIGrabModeSync, &mask, &mods)
            && x_errors == before;
    XSync(display, False);
    sync_ok = sync_ok && x_errors == before;
    result("xi2-passive-sync", sync_ok,
           sync_ok ? "XIGrabModeSync" : "sync passive grab rejected");
    RECORD(sync_ok);

    before = x_errors;
    XIUngrabButton(display, 2, XIAnyButton, window, 1, &mods);
    XSync(display, False);
    bool ungrab_ok = x_errors == before;
    result("xi2-passive-ungrab", ungrab_ok,
           ungrab_ok ? "XIUngrabButton" : "ungrab rejected");
    RECORD(ungrab_ok);

    before = x_errors;
    XGrabServer(display);
    bool grab_ok = grab_button(display, window, XIGrabModeSync, &mask, &mods);
    XIUngrabButton(display, 2, XIAnyButton, window, 1, &mods);
    XUngrabServer(display);
    XSync(display, False);
    grab_ok = grab_ok && x_errors == before;
    result("xi2-passive-under-server", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or error");
    RECORD(grab_ok);

    printf("BXSUMMARY xi2-passive-grab-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
