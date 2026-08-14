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

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int before = x_errors;
    XIAllowEvents(display, 2, XIAsyncDevice, CurrentTime);
    XIAllowEvents(display, 3, XIAsyncPairedDevice, CurrentTime);
    XSync(display, False);
    bool async_ok = x_errors == before;
    result("xi2-allow-async", async_ok,
           async_ok ? "XIAsyncDevice" : "async XIAllowEvents failed");
    RECORD(async_ok);

    before = x_errors;
    XIAllowEvents(display, 2, XIAsyncDevice, 1);
    XSync(display, False);
    bool time_ok = x_errors == before;
    result("xi2-allow-timestamp", time_ok,
           time_ok ? "nonzero timestamp" : "timestamp rejected");
    RECORD(time_ok);

    before = x_errors;
    XIAllowEvents(display, 2, XIReplayDevice, CurrentTime);
    XIAllowEvents(display, 2, XISyncDevice, 1);
    XSync(display, False);
    bool replay_ok = x_errors == before;
    result("xi2-allow-replay-sync", replay_ok,
           replay_ok ? "XIReplayDevice/XISyncDevice"
                     : "replay or sync rejected");
    RECORD(replay_ok);

    before = x_errors;
    XGrabServer(display);
    XIAllowEvents(display, 2, XIAsyncDevice, 1);
    XUngrabServer(display);
    XSync(display, False);
    bool grab_ok = x_errors == before;
    result("xi2-allow-under-server", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or error");
    RECORD(grab_ok);

    printf("BXSUMMARY xi2-allow-events-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
