#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

static void sleep_ms(int milliseconds) {
    struct timespec delay = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (long)(milliseconds % 1000) * 1000000L,
    };
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {}
}

static void result(const char *name, bool ok, const char *detail) {
    printf("BXTEST %s %s%s%s\n", ok ? "PASS" : "FAIL", name,
           detail && *detail ? " " : "", detail ? detail : "");
    fflush(stdout);
}

static void drain_client_messages(Display *display, Window window) {
    XEvent event;
    while (XCheckTypedWindowEvent(display, window, ClientMessage, &event)) {}
}

static Status send_client_message(Display *display, Window destination,
                                  Window payload_window, Atom type,
                                  long data0, Bool propagate, long mask) {
    XEvent event = {0};
    event.xclient.type = ClientMessage;
    event.xclient.display = display;
    event.xclient.window = payload_window;
    event.xclient.message_type = type;
    event.xclient.format = 32;
    event.xclient.data.l[0] = data0;
    return XSendEvent(display, destination, propagate, mask, &event);
}

/* Do not XSync here: the other connection may hold GrabServer. */
static bool wait_client_message(Display *display, Window window, Atom type,
                                long data0, int timeout_ms) {
    int waited = 0;
    while (waited <= timeout_ms) {
        XEvent event = {0};
        while (XCheckTypedWindowEvent(display, window, ClientMessage, &event)) {
            if (event.xclient.message_type == type
                    && event.xclient.data.l[0] == data0
                    && event.xclient.send_event)
                return true;
        }
        sleep_ms(20);
        waited += 20;
    }
    return false;
}

int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 3;
    Display *owner = XOpenDisplay(NULL);
    Display *peer = XOpenDisplay(NULL);
    if (owner == NULL || peer == NULL) {
        fprintf(stderr, "BXFAIL open two X11 connections\n");
        return 2;
    }
    XSetErrorHandler(on_x_error);

    int screen = DefaultScreen(owner);
    Window root = RootWindow(owner, screen);
    Atom type = XInternAtom(owner, "BIONICX_SEND_EVENT", False);

    Window owner_win = XCreateSimpleWindow(owner, root, 40, 40, 200, 120, 0,
                                           0, 0x336699);
    Window peer_win = XCreateSimpleWindow(peer, root, 400, 40, 200, 120, 0,
                                          0, 0x993333);
    XSelectInput(owner, owner_win, StructureNotifyMask);
    XSelectInput(peer, peer_win, StructureNotifyMask);
    XMapWindow(owner, owner_win);
    XMapWindow(peer, peer_win);
    XSync(owner, False);
    XSync(peer, False);

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int before = x_errors;
    drain_client_messages(owner, owner_win);
    drain_client_messages(peer, peer_win);
    XWarpPointer(owner, None, peer_win, 0, 0, 0, 0, 20, 20);
    Status sent = send_client_message(owner, PointerWindow, peer_win, type,
                                      1, False, NoEventMask);
    XFlush(owner);
    bool peer_got = wait_client_message(peer, peer_win, type, 1, 1000);
    bool owner_got = wait_client_message(owner, owner_win, type, 1, 200);
    bool pointer_ok = sent != 0 && peer_got && !owner_got
            && x_errors == before;
    result("pointer-window", pointer_ok,
           pointer_ok ? "origin of pointed window"
                      : "PointerWindow dropped or misrouted");
    RECORD(pointer_ok);

    before = x_errors;
    drain_client_messages(owner, owner_win);
    drain_client_messages(peer, peer_win);
    XSetInputFocus(peer, owner_win, RevertToParent, CurrentTime);
    XWarpPointer(peer, None, peer_win, 0, 0, 0, 0, 20, 20);
    sent = send_client_message(peer, InputFocus, owner_win, type, 2,
                               False, NoEventMask);
    XFlush(peer);
    owner_got = wait_client_message(owner, owner_win, type, 2, 1000);
    peer_got = wait_client_message(peer, peer_win, type, 2, 200);
    bool focus_ok = sent != 0 && owner_got && !peer_got
            && x_errors == before;
    result("input-focus", focus_ok,
           focus_ok ? "origin of focused window"
                    : "InputFocus dropped or followed the pointer");
    RECORD(focus_ok);

    before = x_errors;
    drain_client_messages(owner, owner_win);
    drain_client_messages(peer, peer_win);
    XSetInputFocus(peer, None, RevertToNone, CurrentTime);
    sent = send_client_message(peer, InputFocus, owner_win, type, 3,
                               False, NoEventMask);
    XFlush(peer);
    owner_got = wait_client_message(owner, owner_win, type, 3, 200);
    peer_got = wait_client_message(peer, peer_win, type, 3, 200);
    XSync(peer, False);
    bool none_ok = sent != 0 && !owner_got && !peer_got
            && x_errors == before;
    result("input-focus-none", none_ok,
           none_ok ? "discarded" : "InputFocus None leaked or errored");
    RECORD(none_ok);

    before = x_errors;
    drain_client_messages(owner, owner_win);
    drain_client_messages(peer, peer_win);
    XSetInputFocus(peer, PointerRoot, RevertToPointerRoot, CurrentTime);
    XWarpPointer(peer, None, peer_win, 0, 0, 0, 0, 20, 20);
    sent = send_client_message(peer, InputFocus, peer_win, type, 4,
                               False, NoEventMask);
    XFlush(peer);
    peer_got = wait_client_message(peer, peer_win, type, 4, 1000);
    owner_got = wait_client_message(owner, owner_win, type, 4, 200);
    bool root_ok = sent != 0 && peer_got && !owner_got
            && x_errors == before;
    result("pointer-root-focus", root_ok,
           root_ok ? "InputFocus via PointerRoot"
                   : "PointerRoot InputFocus dropped");
    RECORD(root_ok);

    before = x_errors;
    drain_client_messages(owner, owner_win);
    drain_client_messages(peer, peer_win);
    XSelectInput(owner, peer_win, PropertyChangeMask);
    XSync(owner, False);
    XWarpPointer(peer, None, peer_win, 0, 0, 0, 0, 20, 20);
    sent = send_client_message(peer, PointerWindow, peer_win, type, 5,
                               False, PropertyChangeMask);
    XFlush(peer);
    owner_got = wait_client_message(owner, peer_win, type, 5, 1000);
    peer_got = wait_client_message(peer, peer_win, type, 5, 200);
    bool mask_ok = sent != 0 && owner_got && !peer_got
            && x_errors == before;
    result("pointer-window-mask", mask_ok,
           mask_ok ? "mask not origin"
                   : "mask delivery used origin or dropped");
    RECORD(mask_ok);
    XSelectInput(owner, peer_win, NoEventMask);

    before = x_errors;
    drain_client_messages(owner, owner_win);
    drain_client_messages(peer, peer_win);
    XGrabServer(owner);
    XWarpPointer(owner, None, peer_win, 0, 0, 0, 0, 20, 20);
    sent = send_client_message(owner, PointerWindow, peer_win, type, 6,
                               False, NoEventMask);
    XFlush(owner);
    peer_got = wait_client_message(peer, peer_win, type, 6, 1000);
    XUngrabServer(owner);
    XSync(owner, False);
    bool grab_ok = sent != 0 && peer_got && x_errors == before;
    result("pointer-window-grab", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or dropped under grab");
    RECORD(grab_ok);

    Window child = XCreateSimpleWindow(peer, owner_win, 20, 20, 80, 80, 0,
                                       0, 0x228822);
    XSelectInput(peer, child, StructureNotifyMask);
    XMapWindow(peer, child);
    XSync(peer, False);
    XSync(owner, False);

    before = x_errors;
    drain_client_messages(owner, owner_win);
    drain_client_messages(owner, child);
    drain_client_messages(peer, peer_win);
    drain_client_messages(peer, child);
    XSetInputFocus(peer, owner_win, RevertToParent, CurrentTime);
    XWarpPointer(peer, None, child, 0, 0, 0, 0, 10, 10);
    XSync(peer, False);
    sent = send_client_message(peer, InputFocus, child, type, 7,
                               False, NoEventMask);
    XFlush(peer);
    peer_got = wait_client_message(peer, child, type, 7, 1000);
    owner_got = wait_client_message(owner, child, type, 7, 200);
    bool inside_ok = sent != 0 && peer_got && !owner_got
            && x_errors == before;
    result("input-focus-pointer-inside", inside_ok,
           inside_ok ? "pointer window inside focused ancestor"
                     : "InputFocus ignored pointer inside focus");
    RECORD(inside_ok);

    before = x_errors;
    drain_client_messages(owner, child);
    drain_client_messages(peer, child);
    XSelectInput(owner, owner_win, PropertyChangeMask);
    XSync(owner, False);
    sent = send_client_message(peer, child, child, type, 8,
                               False, PropertyChangeMask);
    XFlush(peer);
    bool prop_false = wait_client_message(owner, child, type, 8, 200)
            || wait_client_message(peer, child, type, 8, 200);
    Status sent_true = send_client_message(peer, child, child, type, 8,
                                           True, PropertyChangeMask);
    XFlush(peer);
    owner_got = wait_client_message(owner, child, type, 8, 1000);
    peer_got = wait_client_message(peer, child, type, 8, 200);
    bool prop_ok = sent != 0 && sent_true != 0 && !prop_false && owner_got
            && !peer_got && x_errors == before;
    result("send-event-propagate", prop_ok,
           prop_ok ? "False drops True ancestor mask"
                   : "propagate ignored or misrouted");
    RECORD(prop_ok);
    XSelectInput(owner, owner_win, NoEventMask);

    printf("BXSUMMARY send-event-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(peer, child);
    XDestroyWindow(owner, owner_win);
    XDestroyWindow(peer, peer_win);
    XCloseDisplay(owner);
    XCloseDisplay(peer);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
