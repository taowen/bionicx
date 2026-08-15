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
    Bool locked = XkbLockModifiers(display, XkbUseCoreKbd, LockMask, LockMask);
    XkbStateRec state = {0};
    Status got = XkbGetState(display, XkbUseCoreKbd, &state);
    XSync(display, False);
    bool lock_ok = locked && got == Success && (state.locked_mods & LockMask)
            && x_errors == before;
    result("xkb-lock-mods", lock_ok,
           lock_ok ? "LockMask set" : "XkbLockModifiers failed");
    RECORD(lock_ok);

    before = x_errors;
    Bool unlocked = XkbLockModifiers(display, XkbUseCoreKbd, LockMask, 0);
    got = XkbGetState(display, XkbUseCoreKbd, &state);
    XSync(display, False);
    bool unlock_ok = unlocked && got == Success
            && (state.locked_mods & LockMask) == 0 && x_errors == before;
    result("xkb-unlock-mods", unlock_ok,
           unlock_ok ? "LockMask cleared" : "unlock failed");
    RECORD(unlock_ok);

    before = x_errors;
    Bool latched = XkbLatchModifiers(display, XkbUseCoreKbd, ShiftMask,
                                     ShiftMask);
    got = XkbGetState(display, XkbUseCoreKbd, &state);
    XSync(display, False);
    bool latch_ok = latched && got == Success
            && (state.latched_mods & ShiftMask) && x_errors == before;
    result("xkb-latch-mods", latch_ok,
           latch_ok ? "Shift latched" : "XkbLatchModifiers failed");
    RECORD(latch_ok);

    before = x_errors;
    XGrabServer(display);
    Bool grab_lock = XkbLockModifiers(display, XkbUseCoreKbd, LockMask,
                                      LockMask);
    XUngrabServer(display);
    got = XkbGetState(display, XkbUseCoreKbd, &state);
    XSync(display, False);
    bool grab_ok = grab_lock && got == Success
            && (state.locked_mods & LockMask) && x_errors == before;
    result("xkb-lock-under-server", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or error");
    RECORD(grab_ok);

    printf("BXSUMMARY xkb-latch-lock-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
