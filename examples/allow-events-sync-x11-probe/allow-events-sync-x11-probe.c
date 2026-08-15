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
    XAllowEvents(display, SyncPointer, CurrentTime);
    XSync(display, False);
    bool pointer_ok = x_errors == before;
    result("allow-sync-pointer", pointer_ok,
           pointer_ok ? "SyncPointer" : "SyncPointer rejected");
    RECORD(pointer_ok);

    before = x_errors;
    XAllowEvents(display, SyncKeyboard, 1);
    XSync(display, False);
    bool keyboard_ok = x_errors == before;
    result("allow-sync-keyboard", keyboard_ok,
           keyboard_ok ? "SyncKeyboard+timestamp" : "SyncKeyboard rejected");
    RECORD(keyboard_ok);

    before = x_errors;
    XAllowEvents(display, SyncBoth, CurrentTime);
    XSync(display, False);
    bool both_ok = x_errors == before;
    result("allow-sync-both", both_ok,
           both_ok ? "SyncBoth" : "SyncBoth rejected");
    RECORD(both_ok);

    before = x_errors;
    XGrabServer(display);
    XAllowEvents(display, SyncPointer, 1);
    XUngrabServer(display);
    XSync(display, False);
    bool grab_ok = x_errors == before;
    result("allow-sync-under-server", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or error");
    RECORD(grab_ok);

    printf("BXSUMMARY allow-events-sync-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
