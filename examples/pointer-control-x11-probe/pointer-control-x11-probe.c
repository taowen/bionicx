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

    int num = 0;
    int den = 0;
    int threshold = 0;
    int before = x_errors;
    XGetPointerControl(display, &num, &den, &threshold);
    XSync(display, False);
    bool get_ok = den != 0 && x_errors == before;
    result("pointer-control-get", get_ok,
           get_ok ? "GetPointerControl" : "get failed");
    RECORD(get_ok);

    before = x_errors;
    XChangePointerControl(display, True, True, 3, 1, 5);
    XGetPointerControl(display, &num, &den, &threshold);
    XSync(display, False);
    bool set_ok = num == 3 && den == 1 && threshold == 5 && x_errors == before;
    result("pointer-control-set", set_ok,
           set_ok ? "3/1/5" : "set/get mismatch");
    RECORD(set_ok);

    before = x_errors;
    XChangePointerControl(display, True, False, 4, 2, 99);
    XGetPointerControl(display, &num, &den, &threshold);
    XSync(display, False);
    bool partial_ok = num == 4 && den == 2 && threshold == 5
            && x_errors == before;
    result("pointer-control-partial", partial_ok,
           partial_ok ? "accel only" : "threshold overwritten");
    RECORD(partial_ok);

    before = x_errors;
    XGrabServer(display);
    XChangePointerControl(display, True, True, 2, 1, 4);
    XUngrabServer(display);
    XGetPointerControl(display, &num, &den, &threshold);
    XSync(display, False);
    bool grab_ok = num == 2 && den == 1 && threshold == 4 && x_errors == before;
    result("pointer-control-under-server", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or error");
    RECORD(grab_ok);

    printf("BXSUMMARY pointer-control-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
