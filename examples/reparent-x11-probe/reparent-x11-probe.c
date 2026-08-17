#define _POSIX_C_SOURCE 200809L

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
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

static int map_state(Display *display, Window window) {
    XWindowAttributes attributes = {0};
    if (!XGetWindowAttributes(display, window, &attributes)) return -1;
    return attributes.map_state;
}

static Window query_parent(Display *display, Window window) {
    Window root = None;
    Window parent = None;
    Window *children = NULL;
    unsigned count = 0;
    if (!XQueryTree(display, window, &root, &parent, &children, &count))
        return None;
    if (children != NULL) XFree(children);
    return parent;
}

static bool wait_typed(Display *display, int type, Window expected,
                       int timeout_ms, XEvent *out) {
    int waited = 0;
    while (waited <= timeout_ms) {
        XEvent event = {0};
        while (XCheckTypedEvent(display, type, &event)) {
            Window window = None;
            if (type == MapRequest) window = event.xmaprequest.window;
            else if (type == UnmapNotify) window = event.xunmap.window;
            else if (type == ReparentNotify) window = event.xreparent.window;
            else if (type == MapNotify) window = event.xmap.window;
            if (expected == None || window == expected) {
                if (out != NULL) *out = event;
                return true;
            }
        }
        sleep_ms(20);
        waited += 20;
    }
    return false;
}

static bool wait_state(Display *display, Window window, int wanted,
                       int timeout_ms) {
    int waited = 0;
    while (waited <= timeout_ms) {
        if (map_state(display, window) == wanted) return true;
        sleep_ms(20);
        waited += 20;
    }
    return false;
}

static void set_dock_type(Display *display, Window window) {
    Atom net_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom dock = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(display, window, net_type, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&dock, 1);
}

static Window make_client(Display *client, Window root, int x, int y,
                          unsigned width, unsigned height,
                          unsigned long color, const char *name) {
    Window window = XCreateSimpleWindow(client, root, x, y, width, height,
                                        0, 0, color);
    XStoreName(client, window, name);
    XSelectInput(client, window,
                 StructureNotifyMask | ExposureMask | SubstructureNotifyMask);
    return window;
}

static Window make_frame(Display *manager, Window root, int x, int y,
                         unsigned width, unsigned height) {
    Window frame = XCreateSimpleWindow(manager, root, x, y, width, height, 0,
                                       0, 0x404040);
    /* Real WMs select SubstructureRedirect on the frame. Mapping the
     * client into that frame must still succeed for the same connection. */
    XSelectInput(manager, frame,
                 SubstructureRedirectMask | SubstructureNotifyMask);
    return frame;
}

static void frame_map(Display *manager, Window frame, Window client) {
    XReparentWindow(manager, client, frame, 4, 24);
    XMapWindow(manager, frame);
    XMapWindow(manager, client);
    XSync(manager, False);
}

int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 3;
    Display *client = XOpenDisplay(NULL);
    Display *manager = XOpenDisplay(NULL);
    if (client == NULL || manager == NULL) {
        fprintf(stderr, "BXFAIL open two X11 connections\n");
        return 2;
    }
    XSetErrorHandler(on_x_error);
    int screen = DefaultScreen(client);
    Window root = RootWindow(client, screen);
    XSelectInput(manager, root,
                 SubstructureRedirectMask | SubstructureNotifyMask);
    XSync(manager, False);

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    Window normal = make_client(client, root, 40, 40, 200, 120, 0x336699,
                                "bionicx-reparent-normal");
    XMapWindow(client, normal);
    XSync(client, False);
    bool held = map_state(client, normal) != IsViewable;
    bool saw_request = wait_typed(manager, MapRequest, normal, 1000, NULL);
    Window frame = make_frame(manager, root, 30, 30, 220, 160);
    frame_map(manager, frame, normal);
    bool framed = query_parent(client, normal) == frame
            && wait_state(client, normal, IsViewable, 1000);
    result("frame-unmapped-client", held && saw_request && framed,
           held && saw_request && framed ? "MapRequest then framed"
                                         : "no MapRequest or not viewable");
    RECORD(held && saw_request && framed);

    Window mapped = make_client(client, root, 80, 200, 180, 100, 0x664422,
                                "bionicx-reparent-mapped");
    XMapWindow(client, mapped);
    XSync(client, False);
    /* Override-redirect so this map is not held by the manager connection. */
    XSetWindowAttributes attrs = {0};
    attrs.override_redirect = True;
    XChangeWindowAttributes(client, mapped, CWOverrideRedirect, &attrs);
    XMapWindow(client, mapped);
    XSync(client, False);
    wait_state(client, mapped, IsViewable, 1000);
    XEvent leftover = {0};
    while (XCheckTypedEvent(client, UnmapNotify, &leftover)) {}
    Window mapped_frame = make_frame(manager, root, 70, 190, 200, 140);
    XReparentWindow(manager, mapped, mapped_frame, 4, 24);
    XSync(manager, False);
    XEvent unmap = {0};
    bool from_configure = wait_typed(client, UnmapNotify, mapped, 1000, &unmap)
            && unmap.xunmap.from_configure;
    int after_reparent = map_state(client, mapped);
    bool unviewable = after_reparent == IsUnviewable
            || after_reparent == IsUnmapped;
    XMapWindow(manager, mapped_frame);
    XSync(manager, False);
    bool remapped = wait_state(client, mapped, IsViewable, 1000);
    result("reparent-mapped-from-configure",
           from_configure && unviewable && remapped,
           from_configure && unviewable && remapped
                   ? "UnmapNotify from-configure then framed"
                   : "missing from-configure unmap or not remapped");
    RECORD(from_configure && unviewable && remapped);

    bool parent_ok = query_parent(client, mapped) == mapped_frame;
    result("reparent-notify", parent_ok,
           parent_ok ? "parent is frame" : "parent is not frame");
    RECORD(parent_ok);

    Window grabbed = make_client(client, root, 300, 40, 160, 80, 0x226644,
                                 "bionicx-reparent-grab");
    XMapWindow(client, grabbed);
    XSync(client, False);
    wait_typed(manager, MapRequest, grabbed, 1000, NULL);
    Window grab_frame = make_frame(manager, root, 290, 30, 180, 120);
    XGrabServer(manager);
    frame_map(manager, grab_frame, grabbed);
    XUngrabServer(manager);
    XSync(manager, False);
    bool grab_ok = query_parent(client, grabbed) == grab_frame
            && wait_state(client, grabbed, IsViewable, 1000);
    result("grab-frame-map", grab_ok,
           grab_ok ? "framed under GrabServer" : "blocked or hidden");
    RECORD(grab_ok);

    Window dock = make_client(client, root, 0, 0, 400, 28, 0x222222,
                              "bionicx-reparent-dock");
    set_dock_type(client, dock);
    XMapWindow(client, dock);
    XSync(client, False);
    bool dock_held = map_state(client, dock) != IsViewable;
    bool dock_request = wait_typed(manager, MapRequest, dock, 1000, NULL);
    Window dock_frame = make_frame(manager, root, 0, 0, 420, 48);
    frame_map(manager, dock_frame, dock);
    bool dock_ok = dock_held && dock_request
            && query_parent(client, dock) == dock_frame
            && wait_state(client, dock, IsViewable, 1000);
    result("dock-frame", dock_ok,
           dock_ok ? "dock MapRequest then framed"
                   : "dock mapped early or not framed");
    RECORD(dock_ok);

    int click_ok = 0;
    if (dock_ok) {
        XSelectInput(client, dock,
                     StructureNotifyMask | ExposureMask | ButtonPressMask);
        XSelectInput(manager, dock_frame,
                     SubstructureRedirectMask | SubstructureNotifyMask
                             | ButtonPressMask);
        XSync(client, False);
        XSync(manager, False);
        int event_base = 0;
        int error_base = 0;
        int major = 0;
        int minor = 0;
        int root_x = 0;
        int root_y = 0;
        Window child = None;
        XTranslateCoordinates(client, dock, root, 36, 13, &root_x, &root_y,
                              &child);
        if (XTestQueryExtension(client, &event_base, &error_base, &major,
                                &minor)) {
            XTestFakeMotionEvent(client, screen, root_x, root_y, 0);
            XTestFakeButtonEvent(client, 1, True, 20);
            XTestFakeButtonEvent(client, 1, False, 20);
            XSync(client, False);
        }
        XEvent press = {0};
        int waited = 0;
        while (waited <= 500) {
            if (XCheckTypedWindowEvent(client, dock, ButtonPress, &press)) {
                click_ok = 1;
                break;
            }
            while (XPending(manager)) {
                XEvent ignored = {0};
                XNextEvent(manager, &ignored);
            }
            sleep_ms(20);
            waited += 20;
        }
        char click_detail[128];
        snprintf(click_detail, sizeof(click_detail),
                 click_ok ? "ButtonPress at %d,%d" : "no ButtonPress at %d,%d",
                 root_x, root_y);
        result("dock-click", click_ok, click_detail);
    } else {
        result("dock-click", 0, "dock not framed");
    }
    RECORD(click_ok);

    int replay_ok = 0;
    if (dock_ok) {
        XGrabButton(manager, 1, AnyModifier, dock_frame, False,
                    ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);
        XSync(manager, False);
        int event_base = 0;
        int error_base = 0;
        int major = 0;
        int minor = 0;
        int root_x = 0;
        int root_y = 0;
        Window child = None;
        XTranslateCoordinates(client, dock, root, 36, 13, &root_x, &root_y,
                              &child);
        while (XPending(client)) {
            XEvent ignored = {0};
            XNextEvent(client, &ignored);
        }
        if (XTestQueryExtension(client, &event_base, &error_base, &major,
                                &minor)) {
            XTestFakeMotionEvent(client, screen, root_x, root_y, 0);
            XTestFakeButtonEvent(client, 1, True, 20);
            XTestFakeButtonEvent(client, 1, False, 20);
            XSync(client, False);
        }
        int waited = 0;
        int saw_frame = 0;
        while (waited <= 800) {
            XEvent event = {0};
            while (XCheckTypedWindowEvent(manager, dock_frame, ButtonPress,
                                          &event)) {
                saw_frame = 1;
                XAllowEvents(manager, ReplayPointer, CurrentTime);
                XFlush(manager);
            }
            if (XCheckTypedWindowEvent(client, dock, ButtonPress, &event)) {
                replay_ok = 1;
                break;
            }
            sleep_ms(20);
            waited += 20;
        }
        XUngrabButton(manager, 1, AnyModifier, dock_frame);
        char replay_detail[128];
        snprintf(replay_detail, sizeof(replay_detail),
                 replay_ok ? "ReplayPointer to dock frame=%d"
                           : "no replay press frame=%d",
                 saw_frame);
        result("dock-replay", replay_ok, replay_detail);
    } else {
        result("dock-replay", 0, "dock not framed");
    }
    RECORD(replay_ok);

    printf("BXSUMMARY reparent-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(client, normal);
    XDestroyWindow(client, mapped);
    XDestroyWindow(client, grabbed);
    XDestroyWindow(client, dock);
    XDestroyWindow(manager, frame);
    XDestroyWindow(manager, mapped_frame);
    XDestroyWindow(manager, grab_frame);
    XDestroyWindow(manager, dock_frame);
    XCloseDisplay(client);
    XCloseDisplay(manager);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
