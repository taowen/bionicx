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
    Window root = RootWindow(display, DefaultScreen(display));
    Window window = XCreateSimpleWindow(display, root, 40, 40, 200, 120, 0,
                                        0, 0x336699);
    XSelectInput(display, window, KeyPressMask | ButtonPressMask |
                 ButtonReleaseMask | PointerMotionMask);
    XMapWindow(display, window);
    XSync(display, False);

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int before = x_errors;
    XGrabKey(display, AnyKey, AnyModifier, window, True,
             GrabModeAsync, GrabModeSync);
    XSync(display, False);
    bool key_ok = x_errors == before;
    result("grab-key-sync", key_ok,
           key_ok ? "GrabModeSync" : "GrabKey rejected");
    RECORD(key_ok);
    XUngrabKey(display, AnyKey, AnyModifier, window);

    before = x_errors;
    int kb = XGrabKeyboard(display, window, True, GrabModeSync, GrabModeSync, 1);
    XUngrabKeyboard(display, 1);
    XSync(display, False);
    bool keyboard_ok = kb == GrabSuccess && x_errors == before;
    result("grab-keyboard-sync", keyboard_ok,
           keyboard_ok ? "Sync+timestamp" : "GrabKeyboard rejected");
    RECORD(keyboard_ok);

    before = x_errors;
    int ptr = XGrabPointer(display, window, True,
                           ButtonPressMask | ButtonReleaseMask,
                           GrabModeSync, GrabModeAsync, None, None, 1);
    XUngrabPointer(display, 1);
    XSync(display, False);
    bool pointer_ok = ptr == GrabSuccess && x_errors == before;
    result("grab-pointer-sync", pointer_ok,
           pointer_ok ? "Sync+timestamp" : "GrabPointer rejected");
    RECORD(pointer_ok);

    before = x_errors;
    XGrabButton(display, AnyButton, AnyModifier, window, True,
                ButtonPressMask | ButtonReleaseMask,
                GrabModeSync, GrabModeSync, None, None);
    XSync(display, False);
    bool button_ok = x_errors == before;
    result("grab-button-sync", button_ok,
           button_ok ? "GrabModeSync" : "GrabButton rejected");
    RECORD(button_ok);
    XUngrabButton(display, AnyButton, AnyModifier, window);

    before = x_errors;
    XAllowEvents(display, AsyncPointer, CurrentTime);
    XAllowEvents(display, AsyncKeyboard, CurrentTime);
    XAllowEvents(display, AsyncBoth, 1);
    XAllowEvents(display, ReplayKeyboard, CurrentTime);
    XAllowEvents(display, SyncPointer, CurrentTime);
    XAllowEvents(display, SyncKeyboard, 1);
    XAllowEvents(display, SyncBoth, CurrentTime);
    XSync(display, False);
    bool allow_ok = x_errors == before;
    result("allow-events-family", allow_ok,
           allow_ok ? "Async/Sync/Replay+timestamp" : "AllowEvents rejected");
    RECORD(allow_ok);

    before = x_errors;
    XGrabServer(display);
    XGrabKey(display, AnyKey, ShiftMask, window, False,
             GrabModeSync, GrabModeSync);
    int grab_kb = XGrabKeyboard(display, window, False,
                                GrabModeSync, GrabModeSync, 1);
    int grab_ptr = XGrabPointer(display, window, False, ButtonPressMask,
                                GrabModeSync, GrabModeSync, None, None, 1);
    XGrabButton(display, Button1, ShiftMask, window, False, ButtonPressMask,
                GrabModeSync, GrabModeSync, None, None);
    XAllowEvents(display, SyncBoth, 1);
    XUngrabPointer(display, 1);
    XUngrabKeyboard(display, 1);
    XUngrabServer(display);
    XSync(display, False);
    bool family_ok = grab_kb == GrabSuccess && grab_ptr == GrabSuccess
            && x_errors == before;
    result("grab-family-under-server", family_ok,
           family_ok ? "under GrabServer" : "blocked or error");
    RECORD(family_ok);

    printf("BXSUMMARY grab-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
