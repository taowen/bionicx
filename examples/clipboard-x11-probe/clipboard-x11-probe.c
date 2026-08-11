#include <X11/Xatom.h>
#include <X11/Xlib.h>
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

static bool wait_event(Display *display, int type, XEvent *event, double seconds) {
    double deadline = now_seconds() + seconds;
    while (now_seconds() < deadline) {
        if (XCheckTypedEvent(display, type, event)) return true;
        fd_set descriptors;
        FD_ZERO(&descriptors);
        int fd = ConnectionNumber(display);
        FD_SET(fd, &descriptors);
        struct timeval timeout = {.tv_sec = 0, .tv_usec = 50000};
        (void)select(fd + 1, &descriptors, NULL, NULL, &timeout);
    }
    return false;
}

static Window make_window(Display *display, int x, unsigned long color,
                          const char *title) {
    int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
            x, 120, 700, 330, 3, BlackPixel(display, screen), color);
    XStoreName(display, window, title);
    XSelectInput(display, window, ExposureMask | PropertyChangeMask);
    XMapWindow(display, window);
    return window;
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
    Display *owner_display = XOpenDisplay(NULL);
    Display *requestor_display = XOpenDisplay(NULL);
    if (owner_display == NULL || requestor_display == NULL) {
        fprintf(stderr, "BXFAIL open two X11 connections\n");
        return 2;
    }
    XSetErrorHandler(on_x_error);
    Window owner_window = make_window(owner_display, 80, 0x00dff4ee,
            "BionicX clipboard owner (glibc X11 client 1)");
    Window requestor_window = make_window(requestor_display, 850, 0x00e8efff,
            "BionicX clipboard requestor (glibc X11 client 2)");
    Atom clipboard = XInternAtom(owner_display, "CLIPBOARD", False);
    Atom utf8 = XInternAtom(owner_display, "UTF8_STRING", False);
    Atom property = XInternAtom(owner_display, "BIONICX_CLIPBOARD_PAYLOAD", False);
    const char payload[] = "BionicX_cross_client_clipboard_2026";
    XSync(owner_display, False);
    XSync(requestor_display, False);

    XSetSelectionOwner(owner_display, clipboard, owner_window, CurrentTime);
    XSync(owner_display, False);
    bool owner_ok = XGetSelectionOwner(requestor_display, clipboard) == owner_window;

    XConvertSelection(requestor_display, clipboard, utf8, property,
                      requestor_window, CurrentTime);
    XFlush(requestor_display);
    XEvent request_event = {0};
    bool request_ok = wait_event(owner_display, SelectionRequest,
                                 &request_event, 2.0);
    XSelectionRequestEvent *request = &request_event.xselectionrequest;
    request_ok = request_ok && request->owner == owner_window
            && request->requestor == requestor_window
            && request->selection == clipboard && request->target == utf8
            && request->property == property;
    if (request_ok) {
        XChangeProperty(owner_display, request->requestor, request->property,
                utf8, 8, PropModeReplace, (const unsigned char *)payload,
                (int)strlen(payload));
        XSelectionEvent reply = {0};
        reply.type = SelectionNotify;
        reply.display = owner_display;
        reply.requestor = request->requestor;
        reply.selection = request->selection;
        reply.target = request->target;
        reply.property = request->property;
        reply.time = request->time;
        XSendEvent(owner_display, request->requestor, False, NoEventMask,
                   (XEvent *)&reply);
        XFlush(owner_display);
    }

    XEvent notify_event = {0};
    bool transfer_ok = wait_event(requestor_display, SelectionNotify,
                                  &notify_event, 2.0)
            && notify_event.xselection.property == property;
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0, bytes_after = 0;
    unsigned char *received = NULL;
    if (transfer_ok) {
        transfer_ok = XGetWindowProperty(requestor_display, requestor_window,
                property, 0, 1024, True, utf8, &actual_type, &actual_format,
                &items, &bytes_after, &received) == Success
                && actual_type == utf8 && actual_format == 8
                && items == strlen(payload) && bytes_after == 0
                && received != NULL
                && memcmp(received, payload, strlen(payload)) == 0;
    }
    if (received != NULL) XFree(received);

    XSetSelectionOwner(requestor_display, clipboard, requestor_window, CurrentTime);
    XSync(requestor_display, False);
    XEvent clear_event = {0};
    bool clear_ok = wait_event(owner_display, SelectionClear, &clear_event, 2.0)
            && clear_event.xselectionclear.window == owner_window
            && clear_event.xselectionclear.selection == clipboard;

    XSetSelectionOwner(requestor_display, clipboard, None, CurrentTime);
    XSync(requestor_display, False);
    bool none_ok = XGetSelectionOwner(owner_display, clipboard) == None;
    XConvertSelection(requestor_display, clipboard, utf8, property,
                      requestor_window, CurrentTime);
    XFlush(requestor_display);
    memset(&notify_event, 0, sizeof(notify_event));
    none_ok = none_ok && wait_event(requestor_display, SelectionNotify,
            &notify_event, 2.0) && notify_event.xselection.property == None;

    Display *transient_display = XOpenDisplay(NULL);
    bool disconnect_ok = transient_display != NULL;
    if (transient_display != NULL) {
        Window transient_window = XCreateSimpleWindow(transient_display,
                RootWindow(transient_display, DefaultScreen(transient_display)),
                0, 0, 1, 1, 0, 0, 0);
        XSetSelectionOwner(transient_display, clipboard, transient_window,
                           CurrentTime);
        XSync(transient_display, False);
        disconnect_ok = XGetSelectionOwner(owner_display, clipboard)
                == transient_window;
        XCloseDisplay(transient_display);
        double deadline = now_seconds() + 2.0;
        while (disconnect_ok && now_seconds() < deadline
                && XGetSelectionOwner(requestor_display, clipboard) != None)
            usleep(20000);
        disconnect_ok = disconnect_ok
                && XGetSelectionOwner(requestor_display, clipboard) == None;
    }

    bool functional = owner_ok && request_ok && transfer_ok && clear_ok
            && none_ok && disconnect_ok;
    label_window(owner_display, owner_window, "OWNER CONNECTION",
                 functional ? "UTF8_STRING sent: 35 exact bytes" : "clipboard test failed");
    label_window(requestor_display, requestor_window, "REQUESTOR CONNECTION",
                 functional ? "SelectionNotify + property verified" : "clipboard test failed");
    XSync(owner_display, False);
    XSync(requestor_display, False);

    printf("BXTEST %s clipboard-owner owner=0x%lx\n",
            owner_ok ? "PASS" : "FAIL", owner_window);
    printf("BXTEST %s selection-request routed=%d\n",
            request_ok ? "PASS" : "FAIL", request_ok);
    printf("BXTEST %s clipboard-transfer bytes=%zu exact=%d\n",
            transfer_ok ? "PASS" : "FAIL", strlen(payload), transfer_ok);
    printf("BXTEST %s selection-clear-none clear=%d none=%d\n",
            clear_ok && none_ok ? "PASS" : "FAIL", clear_ok, none_ok);
    printf("BXTEST %s owner-disconnect cleanup=%d\n",
            disconnect_ok ? "PASS" : "FAIL", disconnect_ok);
    bool passed = functional && x_errors == 0;
    printf("BXSUMMARY clipboard-x11 passed=%d/5 xerrors=%d\n",
            passed ? 5 : 0, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 60 ? duration : 6));

    XDestroyWindow(requestor_display, requestor_window);
    XDestroyWindow(owner_display, owner_window);
    XCloseDisplay(requestor_display);
    XCloseDisplay(owner_display);
    return passed ? 0 : 1;
}
