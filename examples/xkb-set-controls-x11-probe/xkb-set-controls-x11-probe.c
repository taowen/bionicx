#define _POSIX_C_SOURCE 200809L

#include <X11/XKBlib.h>
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

    int opcode = 0;
    int event_base = 0;
    int error_base = 0;
    int major = XkbMajorVersion;
    int minor = XkbMinorVersion;
    if (!XkbQueryExtension(display, &opcode, &event_base, &error_base,
                           &major, &minor)) {
        fprintf(stderr, "BXFAIL XKB unavailable\n");
        XCloseDisplay(display);
        return 2;
    }

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int before = x_errors;
    Bool set_ok = XkbSetAutoRepeatRate(display, XkbUseCoreKbd, 400, 25);
    XSync(display, False);
    bool set_pass = set_ok && x_errors == before;
    result("xkb-set-repeat", set_pass,
           set_pass ? "400/25" : "XkbSetAutoRepeatRate failed");
    RECORD(set_pass);

    unsigned int delay = 0;
    unsigned int interval = 0;
    before = x_errors;
    Bool get_ok = XkbGetAutoRepeatRate(display, XkbUseCoreKbd, &delay,
                                       &interval);
    bool get_pass = get_ok && delay == 400 && interval == 25
            && x_errors == before;
    result("xkb-get-repeat", get_pass,
           get_pass ? "echoed" : "GetAutoRepeatRate mismatch");
    RECORD(get_pass);

    before = x_errors;
    XGrabServer(display);
    Bool grab_set = XkbSetAutoRepeatRate(display, XkbUseCoreKbd, 500, 30);
    XUngrabServer(display);
    XSync(display, False);
    bool grab_pass = grab_set && x_errors == before;
    result("xkb-set-under-server", grab_pass,
           grab_pass ? "under GrabServer" : "blocked or error");
    RECORD(grab_pass);

    delay = 0;
    interval = 0;
    before = x_errors;
    get_ok = XkbGetAutoRepeatRate(display, XkbUseCoreKbd, &delay, &interval);
    bool after_pass = get_ok && delay == 500 && interval == 30
            && x_errors == before;
    result("xkb-get-after-grab", after_pass,
           after_pass ? "500/30" : "rate lost after GrabServer");
    RECORD(after_pass);

    printf("BXSUMMARY xkb-set-controls-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
