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

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int before = x_errors;
    XAllowEvents(display, AsyncPointer, CurrentTime);
    XAllowEvents(display, AsyncKeyboard, CurrentTime);
    XAllowEvents(display, AsyncBoth, CurrentTime);
    XSync(display, False);
    bool async_ok = x_errors == before;
    result("allow-async", async_ok,
           async_ok ? "AsyncPointer/Keyboard/Both" : "async AllowEvents failed");
    RECORD(async_ok);

    before = x_errors;
    XAllowEvents(display, AsyncKeyboard, 1);
    XSync(display, False);
    bool time_ok = x_errors == before;
    result("allow-timestamp", time_ok,
           time_ok ? "nonzero timestamp" : "timestamp rejected");
    RECORD(time_ok);

    before = x_errors;
    XAllowEvents(display, ReplayKeyboard, CurrentTime);
    XSync(display, False);
    bool replay_ok = x_errors == before;
    result("allow-replay-keyboard", replay_ok,
           replay_ok ? "ReplayKeyboard" : "ReplayKeyboard rejected");
    RECORD(replay_ok);

    before = x_errors;
    XGrabServer(display);
    XAllowEvents(display, AsyncKeyboard, 1);
    XUngrabServer(display);
    XSync(display, False);
    bool grab_ok = x_errors == before;
    result("allow-under-server", grab_ok,
           grab_ok ? "AllowEvents under GrabServer" : "blocked or error");
    RECORD(grab_ok);

    printf("BXSUMMARY allow-events-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
