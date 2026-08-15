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
    Display *peer = XOpenDisplay(NULL);
    if (display == NULL || peer == NULL) {
        fprintf(stderr, "BXFAIL open X11 connections\n");
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

    int num = 0;
    int den = 0;
    int threshold = 0;
    int before = x_errors;
    XChangePointerControl(display, True, True, 3, 1, 5);
    XChangePointerControl(display, True, False, 4, 2, 99);
    XGetPointerControl(display, &num, &den, &threshold);
    XSync(display, False);
    bool pointer_ok = num == 4 && den == 2 && threshold == 5
            && x_errors == before;
    result("pointer-control", pointer_ok,
           pointer_ok ? "4/2 keep threshold 5" : "pointer control mismatch");
    RECORD(pointer_ok);

    before = x_errors;
    XKeyboardControl kbd = {
        .bell_percent = 80,
        .auto_repeat_mode = AutoRepeatModeOn,
    };
    XChangeKeyboardControl(display, KBBellPercent | KBAutoRepeatMode, &kbd);
    XKeyboardState state;
    XGetKeyboardControl(display, &state);
    XSync(display, False);
    bool kbd_ok = state.bell_percent == 80
            && state.global_auto_repeat == AutoRepeatModeOn
            && x_errors == before;
    result("keyboard-control", kbd_ok,
           kbd_ok ? "bell 80 + repeat on" : "keyboard control mismatch");
    RECORD(kbd_ok);

    before = x_errors;
    unsigned char map[3] = {1, 3, 2};
    int map_status = XSetPointerMapping(display, map, 3);
    unsigned char out[3] = {0};
    int nmap = XGetPointerMapping(display, out, 3);
    XSync(display, False);
    bool map_ok = map_status == MappingSuccess && nmap == 3 && out[0] == 1
            && out[1] == 3 && out[2] == 2 && x_errors == before;
    result("pointer-mapping", map_ok,
           map_ok ? "1,3,2" : "Set/GetPointerMapping failed");
    RECORD(map_ok);

    before = x_errors;
    Bool set_repeat = XkbSetAutoRepeatRate(display, XkbUseCoreKbd, 400, 25);
    unsigned int delay = 0;
    unsigned int interval = 0;
    Bool get_repeat = XkbGetAutoRepeatRate(display, XkbUseCoreKbd, &delay,
                                           &interval);
    XSync(display, False);
    bool repeat_ok = set_repeat && get_repeat && delay == 400 && interval == 25
            && x_errors == before;
    result("xkb-repeat", repeat_ok,
           repeat_ok ? "400/25" : "XkbSetAutoRepeatRate failed");
    RECORD(repeat_ok);

    before = x_errors;
    Bool locked = XkbLockModifiers(display, XkbUseCoreKbd, LockMask, LockMask);
    XkbStateRec xkb = {0};
    Status got = XkbGetState(display, XkbUseCoreKbd, &xkb);
    Bool latched = XkbLatchModifiers(display, XkbUseCoreKbd, ShiftMask,
                                     ShiftMask);
    got = XkbGetState(display, XkbUseCoreKbd, &xkb);
    XSync(display, False);
    bool lock_ok = locked && latched && got == Success
            && (xkb.locked_mods & LockMask) && (xkb.latched_mods & ShiftMask)
            && x_errors == before;
    result("xkb-latch-lock", lock_ok,
           lock_ok ? "Lock+Shift" : "LatchLockState failed");
    RECORD(lock_ok);

    before = x_errors;
    Atom num_lock = XInternAtom(display, "Num Lock", False);
    Bool num_on = False;
    int num_ndx = -1;
    Bool num_found = XkbSetNamedIndicator(display, num_lock, True, True,
                                          False, NULL)
            && XkbGetNamedIndicator(display, num_lock, &num_ndx, &num_on,
                                    NULL, NULL);
    unsigned int leds = 0;
    Status led_got = XkbGetIndicatorState(display, XkbUseCoreKbd, &leds);
    got = XkbGetState(display, XkbUseCoreKbd, &xkb);
    XkbStateRec peer_state = {0};
    Bool peer_num = False;
    XkbGetState(peer, XkbUseCoreKbd, &peer_state);
    XkbGetNamedIndicator(peer, num_lock, NULL, &peer_num, NULL, NULL);
    XSync(display, False);
    XSync(peer, False);
    bool named_ok = num_found && num_on && num_ndx == 1 && led_got == Success
            && got == Success && (leds & 2) && (xkb.locked_mods & Mod2Mask)
            && peer_num && (peer_state.locked_mods & Mod2Mask)
            && x_errors == before;
    result("xkb-named-indicator", named_ok,
           named_ok ? "Num Lock Set/Get" : "named indicator failed");
    RECORD(named_ok);

    before = x_errors;
    Atom caps_lock = XInternAtom(display, "Caps Lock", False);
    Bool caps_on = False;
    int caps_ndx = -1;
    XkbLockModifiers(display, XkbUseCoreKbd, LockMask, LockMask);
    Bool caps_found = XkbGetNamedIndicator(display, caps_lock, &caps_ndx,
                                           &caps_on, NULL, NULL);
    XSync(display, False);
    bool caps_ok = caps_found && caps_on && caps_ndx == 0 && x_errors == before;
    result("xkb-caps-indicator", caps_ok,
           caps_ok ? "Caps from LockModifiers" : "Caps indicator failed");
    RECORD(caps_ok);

    before = x_errors;
    Bool bell = XkbBell(display, None, 50, None);
    Bool detectable = False;
    XkbSetDetectableAutoRepeat(display, True, &detectable);
    XSync(display, False);
    bool extras_ok = bell && x_errors == before;
    result("xkb-bell-detectable", extras_ok,
           extras_ok ? "XkbBell+detectable" : "bell or detectable failed");
    RECORD(extras_ok);

    before = x_errors;
    XGrabServer(display);
    XChangePointerControl(display, True, True, 2, 1, 4);
    XChangeKeyboardControl(display, KBBellPercent | KBAutoRepeatMode, &kbd);
    XkbSetAutoRepeatRate(display, XkbUseCoreKbd, 500, 30);
    XkbLockModifiers(display, XkbUseCoreKbd, LockMask, 0);
    XkbSetNamedIndicator(display, num_lock, True, False, False, NULL);
    XUngrabServer(display);
    XGetPointerControl(display, &num, &den, &threshold);
    get_repeat = XkbGetAutoRepeatRate(display, XkbUseCoreKbd, &delay, &interval);
    XSync(display, False);
    bool grab_ok = num == 2 && den == 1 && threshold == 4 && get_repeat
            && delay == 500 && interval == 30 && x_errors == before;
    result("input-under-server", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or error");
    RECORD(grab_ok);

    printf("BXSUMMARY input-settings-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XCloseDisplay(peer);
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
