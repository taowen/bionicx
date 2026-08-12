#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

static int x_errors;

static int on_x_error(Display *display, XErrorEvent *event) {
    char text[128];
    XGetErrorText(display, event->error_code, text, sizeof(text));
    fprintf(stderr, "BXERROR x11 code=%u request=%u resource=0x%lx text=%s\n",
            event->error_code, event->request_code, event->resourceid, text);
    ++x_errors;
    return 0;
}

static double now_seconds(void) {
    struct timespec value;
    clock_gettime(CLOCK_MONOTONIC, &value);
    return value.tv_sec + value.tv_nsec / 1000000000.0;
}

static Window make_window(Display *display, int x, unsigned long color,
                          const char *title, bool mapped) {
    int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
            x, 120, 700, 330, 3, BlackPixel(display, screen), color);
    XStoreName(display, window, title);
    XSelectInput(display, window, ExposureMask | KeyPressMask | KeyReleaseMask);
    if (mapped) XMapWindow(display, window);
    return window;
}

static void drain(Display *display) {
    XEvent event;
    while (XPending(display)) XNextEvent(display, &event);
}

static bool wait_for_key(Display *wanted_display, Window wanted_window,
                         Display *other_display, int wanted_keysym,
                         double timeout_seconds) {
    double deadline = now_seconds() + timeout_seconds;
    bool matched = false;
    bool leaked = false;
    while (now_seconds() < deadline && !matched) {
        Display *displays[] = {wanted_display, other_display};
        for (size_t i = 0; i < 2; ++i) {
            while (XPending(displays[i])) {
                XEvent event;
                XNextEvent(displays[i], &event);
                if (event.type != KeyPress) continue;
                int keysym = (int)XLookupKeysym(&event.xkey, 0);
                if (displays[i] == wanted_display
                        && event.xkey.window == wanted_window
                        && keysym == wanted_keysym) {
                    matched = true;
                }
                else if (keysym == wanted_keysym) {
                    leaked = true;
                }
            }
        }
        if (matched) break;
        fd_set descriptors;
        FD_ZERO(&descriptors);
        int wanted_fd = ConnectionNumber(wanted_display);
        int other_fd = ConnectionNumber(other_display);
        FD_SET(wanted_fd, &descriptors);
        FD_SET(other_fd, &descriptors);
        int max_fd = wanted_fd > other_fd ? wanted_fd : other_fd;
        struct timeval timeout = {.tv_sec = 0, .tv_usec = 50000};
        (void)select(max_fd + 1, &descriptors, NULL, NULL, &timeout);
    }
    return matched && !leaked;
}

static void label_window(Display *display, Window window, const char *role,
                         const char *status) {
    GC gc = XCreateGC(display, window, 0, NULL);
    XSetForeground(display, gc, 0x00172b4d);
    XDrawString(display, window, gc, 55, 105, role, (int)strlen(role));
    XDrawString(display, window, gc, 55, 175, status, (int)strlen(status));
    XFreeGC(display, gc);
    XFlush(display);
}

int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 6;
    Display *grabber = XOpenDisplay(NULL);
    Display *peer = XOpenDisplay(NULL);
    if (grabber == NULL || peer == NULL) {
        fprintf(stderr, "BXFAIL open two X11 connections\n");
        return 2;
    }
    XSetErrorHandler(on_x_error);
    Window grab_window = make_window(grabber, 80, 0x00dff4ee,
            "BionicX keyboard grabber (glibc X11 client 1)", true);
    Window peer_window = make_window(peer, 850, 0x00e8efff,
            "BionicX keyboard peer (glibc X11 client 2)", true);
    Window hidden_window = make_window(grabber, 0, 0,
            "BionicX hidden keyboard target", false);
    Window owner_events_window = XCreateSimpleWindow(grabber,
            RootWindow(grabber, DefaultScreen(grabber)), 0, 0, 1, 1, 0, 0, 0);
    XSelectInput(grabber, owner_events_window, KeyPressMask | KeyReleaseMask);
    XMapWindow(grabber, owner_events_window);
    XSync(grabber, False);
    XSync(peer, False);
    drain(grabber);
    drain(peer);

    int first_status = XGrabKeyboard(grabber, grab_window, False,
            GrabModeAsync, GrabModeAsync, CurrentTime);
    int contention_status = XGrabKeyboard(peer, peer_window, False,
            GrabModeAsync, GrabModeAsync, CurrentTime);
    bool first_ok = first_status == GrabSuccess;
    bool contention_ok = contention_status == AlreadyGrabbed;
    XUngrabKeyboard(grabber, CurrentTime);
    XSync(grabber, False);

    int hidden_status = XGrabKeyboard(grabber, hidden_window, False,
            GrabModeAsync, GrabModeAsync, CurrentTime);
    bool hidden_ok = hidden_status == GrabNotViewable;

    XSetInputFocus(peer, peer_window, RevertToParent, CurrentTime);
    XSync(peer, False);
    drain(grabber);
    drain(peer);
    int route_status = XGrabKeyboard(grabber, grab_window, False,
            GrabModeAsync, GrabModeAsync, CurrentTime);
    printf("BXREADY keyboard-grab inject-a\n");
    fflush(stdout);
    bool grabbed_route_ok = route_status == GrabSuccess
            && wait_for_key(grabber, grab_window, peer, XK_a, 5.0);

    XUngrabKeyboard(grabber, CurrentTime);
    XSetInputFocus(peer, peer_window, RevertToParent, CurrentTime);
    XSync(grabber, False);
    XSync(peer, False);
    drain(grabber);
    drain(peer);
    printf("BXREADY keyboard-ungrab inject-b\n");
    fflush(stdout);
    bool ungrabbed_route_ok = wait_for_key(peer, peer_window, grabber, XK_b, 5.0);

    XSetInputFocus(grabber, owner_events_window, RevertToParent, CurrentTime);
    XSync(grabber, False);
    drain(grabber);
    drain(peer);
    int owner_events_status = XGrabKeyboard(grabber, grab_window, True,
            GrabModeAsync, GrabModeAsync, CurrentTime);
    printf("BXREADY keyboard-owner-events inject-c\n");
    fflush(stdout);
    bool owner_events_ok = owner_events_status == GrabSuccess
            && wait_for_key(grabber, owner_events_window, peer, XK_c, 5.0);
    XUngrabKeyboard(grabber, CurrentTime);
    XSync(grabber, False);

    Display *transient = XOpenDisplay(NULL);
    bool disconnect_ok = transient != NULL;
    if (transient != NULL) {
        Window transient_window = make_window(transient, 0, 0,
                "BionicX transient keyboard grabber", true);
        XSync(transient, False);
        disconnect_ok = XGrabKeyboard(transient, transient_window, False,
                GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess;
        XCloseDisplay(transient);
        int status = AlreadyGrabbed;
        double deadline = now_seconds() + 2.0;
        while (status == AlreadyGrabbed && now_seconds() < deadline) {
            status = XGrabKeyboard(peer, peer_window, False,
                    GrabModeAsync, GrabModeAsync, CurrentTime);
            if (status == AlreadyGrabbed) usleep(20000);
        }
        disconnect_ok = disconnect_ok && status == GrabSuccess;
        if (status == GrabSuccess) XUngrabKeyboard(peer, CurrentTime);
    }

    Window root = RootWindow(grabber, DefaultScreen(grabber));
    KeyCode d_keycode = XKeysymToKeycode(grabber, XK_d);
    XGrabKey(grabber, d_keycode, AnyModifier, root, False,
             GrabModeAsync, GrabModeAsync);
    XSetInputFocus(peer, peer_window, RevertToParent, CurrentTime);
    XSync(grabber, False);
    XSync(peer, False);
    drain(grabber);
    drain(peer);
    printf("BXREADY passive-key-grab inject-d\n");
    fflush(stdout);
    bool passive_route_ok = wait_for_key(grabber, root, peer, XK_d, 5.0);

    drain(grabber);
    drain(peer);
    printf("BXREADY passive-key-release inject-e\n");
    fflush(stdout);
    bool passive_release_ok = wait_for_key(peer, peer_window, grabber, XK_e,
                                            5.0);
    XUngrabKey(grabber, d_keycode, AnyModifier, root);
    XSync(grabber, False);

    bool functional = first_ok && contention_ok && hidden_ok
            && grabbed_route_ok && ungrabbed_route_ok && owner_events_ok
            && disconnect_ok && passive_route_ok && passive_release_ok;
    label_window(grabber, grab_window, "GRABBER CONNECTION",
            functional ? "A rerouted here while peer had focus" : "keyboard grab test failed");
    label_window(peer, peer_window, "FOCUSED PEER CONNECTION",
            functional ? "B arrived here after UngrabKeyboard" : "keyboard grab test failed");
    XSync(grabber, False);
    XSync(peer, False);

    printf("BXTEST %s keyboard-grab status=%d\n",
            first_ok ? "PASS" : "FAIL", first_status);
    printf("BXTEST %s keyboard-contention status=%d\n",
            contention_ok ? "PASS" : "FAIL", contention_status);
    printf("BXTEST %s keyboard-not-viewable status=%d\n",
            hidden_ok ? "PASS" : "FAIL", hidden_status);
    printf("BXTEST %s keyboard-grab-route exact=%d\n",
            grabbed_route_ok ? "PASS" : "FAIL", grabbed_route_ok);
    printf("BXTEST %s keyboard-ungrab-route exact=%d\n",
            ungrabbed_route_ok ? "PASS" : "FAIL", ungrabbed_route_ok);
    printf("BXTEST %s keyboard-owner-events normal-route=%d\n",
            owner_events_ok ? "PASS" : "FAIL", owner_events_ok);
    printf("BXTEST %s keyboard-owner-disconnect cleanup=%d\n",
            disconnect_ok ? "PASS" : "FAIL", disconnect_ok);
    printf("BXTEST %s passive-key-grab route=%d\n",
            passive_route_ok ? "PASS" : "FAIL", passive_route_ok);
    printf("BXTEST %s passive-key-grab auto-release=%d\n",
            passive_release_ok ? "PASS" : "FAIL", passive_release_ok);
    bool passed = functional && x_errors == 0;
    printf("BXSUMMARY keyboard-grab-x11 passed=%d/9 xerrors=%d\n",
            passed ? 9 : 0, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 60 ? duration : 6));

    XDestroyWindow(peer, peer_window);
    XDestroyWindow(grabber, owner_events_window);
    XDestroyWindow(grabber, hidden_window);
    XDestroyWindow(grabber, grab_window);
    XCloseDisplay(peer);
    XCloseDisplay(grabber);
    return passed ? 0 : 1;
}
