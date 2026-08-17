#define _POSIX_C_SOURCE 200809L

#include <X11/Xatom.h>
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

static int map_state(Display *display, Window window) {
    XWindowAttributes attributes = {0};
    if (!XGetWindowAttributes(display, window, &attributes)) return -1;
    return attributes.map_state;
}

static bool wait_map_request(Display *manager, Window expected, int timeout_ms) {
    int waited = 0;
    while (waited <= timeout_ms) {
        XEvent event = {0};
        while (XCheckTypedEvent(manager, MapRequest, &event)) {
            if (event.xmaprequest.window == expected) return true;
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

    Window normal = XCreateSimpleWindow(client, root, 40, 40, 200, 120, 0,
                                        0, 0x336699);
    XStoreName(client, normal, "bionicx-map-normal");
    XMapWindow(client, normal);
    XSync(client, False);
    bool held = wait_state(client, normal, IsUnmapped, 200)
            || map_state(client, normal) != IsViewable;
    bool saw = wait_map_request(manager, normal, 1000);
    result("map-request-held", held && saw,
           held && saw ? "unmapped+MapRequest" : "mapped before WM or no event");
    RECORD(held && saw);
    XMapWindow(manager, normal);
    XSync(manager, False);
    bool shown = wait_state(client, normal, IsViewable, 1000);
    result("map-request-wm-map", shown, shown ? "viewable" : "still hidden");
    RECORD(shown);

    Window dock = XCreateSimpleWindow(client, root, 0, 0, 400, 28, 0,
                                      0, 0x222222);
    XStoreName(client, dock, "bionicx-map-dock");
    set_dock_type(client, dock);
    XMapWindow(client, dock);
    XSync(client, False);
    bool dock_held = map_state(client, dock) != IsViewable;
    bool dock_saw = wait_map_request(manager, dock, 1000);
    result("dock-request-held", dock_held && dock_saw,
           dock_held && dock_saw ? "unmapped+MapRequest"
                                 : "dock mapped before WM or no event");
    RECORD(dock_held && dock_saw);
    XMapWindow(manager, dock);
    XSync(manager, False);
    bool dock_shown = wait_state(client, dock, IsViewable, 1000);
    result("dock-request-wm-map", dock_shown,
           dock_shown ? "viewable" : "still hidden");
    RECORD(dock_shown);

    XGrabServer(client);
    Window grabbed = XCreateSimpleWindow(client, root, 80, 200, 160, 80, 0,
                                         0, 0x664422);
    XMapWindow(client, grabbed);
    XSync(client, False);
    bool grab_mapped = map_state(client, grabbed) == IsViewable;
    XUngrabServer(client);
    XSync(client, False);
    result("grab-owner-map", grab_mapped,
           grab_mapped ? "viewable under GrabServer"
                       : "blocked by SubstructureRedirect");
    RECORD(grab_mapped);

    XGrabServer(client);
    Window grab_cfg = XCreateSimpleWindow(client, root, 80, 300, 100, 50, 0,
                                          0, 0x226644);
    XResizeWindow(client, grab_cfg, 240, 90);
    XSync(client, False);
    XWindowAttributes grab_geo = {0};
    XGetWindowAttributes(client, grab_cfg, &grab_geo);
    bool grab_resized = grab_geo.width == 240 && grab_geo.height == 90;
    XUngrabServer(client);
    XSync(client, False);
    result("grab-owner-configure", grab_resized,
           grab_resized ? "240x90 under GrabServer"
                        : "ConfigureRequest blocked by SubstructureRedirect");
    RECORD(grab_resized);

    XGrabServer(manager);
    XSync(manager, False);
    Window during = XCreateSimpleWindow(client, root, 320, 80, 140, 70, 0,
                                        0, 0x224466);
    /* MapWindow has no reply. XSync here would deadlock: the manager still
     * holds GrabServer, so the client's round-trip never completes. */
    XMapWindow(client, during);
    XFlush(client);
    XUngrabServer(manager);
    XSync(manager, False);
    bool during_held = map_state(client, during) != IsViewable;
    bool during_request = wait_map_request(manager, during, 1000);
    XMapWindow(manager, during);
    XSync(manager, False);
    bool during_shown = wait_state(client, during, IsViewable, 1000);
    result("grab-other-map", during_held && during_request && during_shown,
           during_held && during_request && during_shown
                   ? "MapRequest after owner ungrab"
                   : "lost MapRequest while WM held GrabServer");
    RECORD(during_held && during_request && during_shown);

    printf("BXSUMMARY map-request-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(client, normal);
    XDestroyWindow(client, dock);
    XDestroyWindow(client, grabbed);
    XDestroyWindow(client, grab_cfg);
    XDestroyWindow(client, during);
    XCloseDisplay(client);
    XCloseDisplay(manager);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
