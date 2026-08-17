#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XTest.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int filter_core;
static int filter_xi;
static int filter_send;
static int filter_allow;
static int filter_logs;
static Display *gdk_dpy;
static int xi_opcode;

static void result(const char *name, int passed, const char *detail) {
    printf("BXTEST %s %s%s%s\n", passed ? "PASS" : "FAIL", name,
           detail && *detail ? " " : "", detail ? detail : "");
    fflush(stdout);
}

static void *required_symbol(void *library, const char *name) {
    dlerror();
    void *symbol = dlsym(library, name);
    const char *error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "BXTEST FAIL gtk-symbol name=%s error=%s\n", name,
                error);
        exit(1);
    }
    return symbol;
}

static int on_x_event(void *xevent, void *gevent, void *data) {
    XEvent *event = xevent;
    int is_press = 0;
    (void)gevent;
    (void)data;
    if (filter_logs < 24 && (event->type == ButtonPress
            || event->type == ButtonRelease || event->type == GenericEvent)) {
        int send = event->xany.send_event ? 1 : 0;
        int ext = event->type == GenericEvent ? event->xcookie.extension : -1;
        int has_data = event->type == GenericEvent
                && event->xcookie.data != NULL;
        int evtype = -1;
        if (has_data) evtype = ((XIEvent *)event->xcookie.data)->evtype;
        printf("BXINFO gdk-filter type=%d send=%d ext=%d data=%d evtype=%d\n",
               event->type, send, ext, has_data, evtype);
        fflush(stdout);
        ++filter_logs;
    }
    if (event->type == ButtonPress && event->xbutton.button == 1) {
        filter_core = 1;
        filter_send = event->xbutton.send_event ? 1 : 0;
        is_press = 1;
    } else if (gdk_dpy != NULL && event->type == GenericEvent
            && event->xcookie.extension == xi_opcode
            && event->xcookie.data != NULL) {
        XIDeviceEvent *xi = event->xcookie.data;
        if (xi->evtype == XI_ButtonPress && xi->detail == 1
                && xi->buttons.mask != NULL && xi->buttons.mask_len >= 4
                && (xi->buttons.mask[0] & 2) != 0) {
            filter_xi = 1;
            is_press = 1;
        }
    }
    if (is_press && gdk_dpy != NULL) {
        XAllowEvents(gdk_dpy, ReplayPointer, CurrentTime);
        if (xi_opcode != 0)
            XIAllowEvents(gdk_dpy, 2, XIReplayDevice, CurrentTime);
        filter_allow = 1;
        XFlush(gdk_dpy);
        return 2;
    }
    return 0;
}

static void pump(int (*pending)(void), int (*iterate)(int), int milliseconds) {
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        while (pending()) iterate(0);
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - start.tv_sec) * 1000
                + (now.tv_nsec - start.tv_nsec) / 1000000;
        if (elapsed >= milliseconds) break;
        struct timespec delay = {.tv_sec = 0, .tv_nsec = 10 * 1000 * 1000};
        nanosleep(&delay, NULL);
    }
}

static int xtest_click(int root_x, int root_y) {
    Display *xtest = XOpenDisplay(NULL);
    int event_base = 0;
    int error_base = 0;
    int major = 0;
    int minor = 0;
    if (xtest == NULL || !XTestQueryExtension(xtest, &event_base, &error_base,
                                              &major, &minor)) {
        if (xtest != NULL) XCloseDisplay(xtest);
        return 0;
    }
    XTestFakeMotionEvent(xtest, DefaultScreen(xtest), root_x, root_y, 0);
    XTestFakeButtonEvent(xtest, 1, True, 20);
    XTestFakeButtonEvent(xtest, 1, False, 20);
    XSync(xtest, False);
    XCloseDisplay(xtest);
    return 1;
}

static int claim_compositor(Display *display) {
    if (display == NULL) return 0;
    void *library = dlopen("libXcomposite.so.1", RTLD_NOW);
    if (library == NULL) return 0;
    int (*query)(Display *, int *, int *) = NULL;
    void (*redirect)(Display *, Window, int) = NULL;
    *(void **)(&query) = dlsym(library, "XCompositeQueryExtension");
    *(void **)(&redirect) = dlsym(library, "XCompositeRedirectSubwindows");
    int event_base = 0;
    int error_base = 0;
    if (query == NULL || redirect == NULL
            || !query(display, &event_base, &error_base))
        return 0;
    Window root = DefaultRootWindow(display);
    Window owner = XCreateSimpleWindow(display, root, -8, -8, 1, 1, 0, 0, 0);
    Atom cm = XInternAtom(display, "_NET_WM_CM_S0", False);
    XSetSelectionOwner(display, cm, owner, CurrentTime);
    redirect(display, root, 1);
    XSync(display, False);
    return XGetSelectionOwner(display, cm) == owner;
}

static int grab_target(Display *display, Window target) {
    unsigned char mask_bytes[XIMaskLen(XI_LASTEVENT)] = {0};
    XISetMask(mask_bytes, XI_ButtonPress);
    XIEventMask xi_mask = {
        .deviceid = 2,
        .mask_len = (int)sizeof(mask_bytes),
        .mask = mask_bytes,
    };
    XIGrabModifiers mods = {.modifiers = XIAnyModifier};
    XISelectEvents(display, target, &xi_mask, 1);
    XGrabButton(display, 1, AnyModifier, target, False, ButtonPressMask,
                GrabModeSync, GrabModeAsync, None, None);
    int failed = XIGrabButton(display, 2, 1, target, None, XIGrabModeSync,
                              XIGrabModeAsync, False, &xi_mask, 1, &mods);
    XSync(display, False);
    return failed == 0;
}

static void ungrab_target(Display *display, Window target) {
    XIGrabModifiers mods = {.modifiers = XIAnyModifier};
    XUngrabButton(display, 1, AnyModifier, target);
    XIUngrabButton(display, 2, 1, target, 1, &mods);
    XSync(display, False);
}

int main(int argc, char **argv) {
    int passed = 0;
    int failed = 0;
#define RECORD(value) do { if (value) ++passed; else ++failed; } while (0)
    char detail[160];

    void *gtk = dlopen("libgtk-3.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (gtk == NULL) {
        result("gtk-dlopen", 0, dlerror());
        return 1;
    }
    result("gtk-dlopen", 1, "libgtk-3.so.0");
    ++passed;

    int (*gtk_init_check)(int *, char ***) = NULL;
    void *(*gtk_window_new)(int) = NULL;
    void (*gtk_window_set_default_size)(void *, int, int) = NULL;
    void (*gtk_widget_show_all)(void *) = NULL;
    int (*gtk_widget_get_mapped)(void *) = NULL;
    void *(*gtk_widget_get_window)(void *) = NULL;
    int (*gtk_events_pending)(void) = NULL;
    int (*gtk_main_iteration_do)(int) = NULL;
    void *(*gdk_display_get_default)(void) = NULL;
    Display *(*gdk_x11_display_get_xdisplay)(void *) = NULL;
    unsigned long (*gdk_x11_window_get_xid)(void *) = NULL;
    void (*gdk_window_add_filter)(void *, void *, void *) = NULL;
    void (*gdk_window_get_origin)(void *, int *, int *) = NULL;
    int (*gdk_window_get_width)(void *) = NULL;
    int (*gdk_window_get_height)(void *) = NULL;
    *(void **)(&gtk_init_check) = required_symbol(gtk, "gtk_init_check");
    *(void **)(&gtk_window_new) = required_symbol(gtk, "gtk_window_new");
    *(void **)(&gtk_window_set_default_size) =
            required_symbol(gtk, "gtk_window_set_default_size");
    *(void **)(&gtk_widget_show_all) = required_symbol(gtk, "gtk_widget_show_all");
    *(void **)(&gtk_widget_get_mapped) =
            required_symbol(gtk, "gtk_widget_get_mapped");
    *(void **)(&gtk_widget_get_window) =
            required_symbol(gtk, "gtk_widget_get_window");
    *(void **)(&gtk_events_pending) = required_symbol(gtk, "gtk_events_pending");
    *(void **)(&gtk_main_iteration_do) =
            required_symbol(gtk, "gtk_main_iteration_do");
    *(void **)(&gdk_display_get_default) =
            required_symbol(gtk, "gdk_display_get_default");
    *(void **)(&gdk_x11_display_get_xdisplay) =
            required_symbol(gtk, "gdk_x11_display_get_xdisplay");
    *(void **)(&gdk_x11_window_get_xid) =
            required_symbol(gtk, "gdk_x11_window_get_xid");
    *(void **)(&gdk_window_add_filter) =
            required_symbol(gtk, "gdk_window_add_filter");
    *(void **)(&gdk_window_get_origin) =
            required_symbol(gtk, "gdk_window_get_origin");
    *(void **)(&gdk_window_get_width) =
            required_symbol(gtk, "gdk_window_get_width");
    *(void **)(&gdk_window_get_height) =
            required_symbol(gtk, "gdk_window_get_height");

    int init_ok = gtk_init_check(&argc, &argv) != 0;
    result("gtk-init", init_ok, init_ok ? "DISPLAY connected" : "failed");
    RECORD(init_ok);
    if (!init_ok) {
        printf("BXSUMMARY gtk-gdk-grab passed=%d failed=%d\n", passed, failed);
        return 1;
    }

    void *window = gtk_window_new(0);
    gtk_window_set_default_size(window, 240, 160);
    gtk_widget_show_all(window);
    pump(gtk_events_pending, gtk_main_iteration_do, 400);
    void *gdk_window = gtk_widget_get_window(window);
    int mapped = gtk_widget_get_mapped(window) && gdk_window != NULL;
    result("gtk-window", mapped, mapped ? "mapped" : "unmapped");
    RECORD(mapped);
    if (!mapped) {
        printf("BXSUMMARY gtk-gdk-grab passed=%d failed=%d\n", passed, failed);
        return 1;
    }

    void *gdk_display = gdk_display_get_default();
    gdk_dpy = gdk_x11_display_get_xdisplay(gdk_display);
    int xi_event = 0;
    int xi_error = 0;
    int xi_ok = gdk_dpy != NULL
            && XQueryExtension(gdk_dpy, "XInputExtension", &xi_opcode,
                               &xi_event, &xi_error);
    int major = 2;
    int minor = 0;
    if (xi_ok) xi_ok = XIQueryVersion(gdk_dpy, &major, &minor) == Success;
    gdk_window_add_filter(NULL, (void *)on_x_event, NULL);

    Window self = (Window)gdk_x11_window_get_xid(gdk_window);
    int origin_x = 0;
    int origin_y = 0;
    gdk_window_get_origin(gdk_window, &origin_x, &origin_y);
    int self_x = origin_x + gdk_window_get_width(gdk_window) / 2;
    int self_y = origin_y + gdk_window_get_height(gdk_window) / 2;

    filter_core = filter_xi = filter_send = filter_allow = filter_logs = 0;
    int self_ok = 0;
    if (!xi_ok) {
        snprintf(detail, sizeof(detail), "XI2 unavailable");
    } else if (!grab_target(gdk_dpy, self)) {
        snprintf(detail, sizeof(detail), "grab failed");
    } else if (!xtest_click(self_x, self_y)) {
        snprintf(detail, sizeof(detail), "XTEST unavailable");
        ungrab_target(gdk_dpy, self);
    } else {
        pump(gtk_events_pending, gtk_main_iteration_do, 800);
        ungrab_target(gdk_dpy, self);
        self_ok = filter_allow && (filter_core || filter_xi);
        snprintf(detail, sizeof(detail), "core=%d send=%d xi=%d allow=%d",
                 filter_core, filter_send, filter_xi, filter_allow);
    }
    result("gdk-grab-self", self_ok, detail);
    RECORD(self_ok);

    Display *peer = XOpenDisplay(NULL);
    Window peer_win = None;
    filter_core = filter_xi = filter_send = filter_allow = filter_logs = 0;
    int peer_ok = 0;
    if (peer == NULL) {
        snprintf(detail, sizeof(detail), "peer display failed");
    } else {
        peer_win = XCreateSimpleWindow(peer, DefaultRootWindow(peer), 80, 80,
                                       200, 120, 0, 0, 0x336699);
        XMapWindow(peer, peer_win);
        XSync(peer, False);
        if (!xi_ok) {
            snprintf(detail, sizeof(detail), "XI2 unavailable");
        } else if (!grab_target(gdk_dpy, peer_win)) {
            snprintf(detail, sizeof(detail), "grab failed");
        } else if (!xtest_click(180, 140)) {
            snprintf(detail, sizeof(detail), "XTEST unavailable");
            ungrab_target(gdk_dpy, peer_win);
        } else {
            pump(gtk_events_pending, gtk_main_iteration_do, 800);
            ungrab_target(gdk_dpy, peer_win);
            peer_ok = filter_allow && (filter_core || filter_xi);
            snprintf(detail, sizeof(detail), "core=%d send=%d xi=%d allow=%d",
                     filter_core, filter_send, filter_xi, filter_allow);
        }
        XDestroyWindow(peer, peer_win);
        XCloseDisplay(peer);
    }
    result("gdk-grab-peer", peer_ok, detail);
    RECORD(peer_ok);

    int composited = claim_compositor(gdk_dpy);
    result("gtk-compositor", composited,
           composited ? "_NET_WM_CM_S0 RedirectSubwindows" : "unclaimed");
    RECORD(composited);

    Display *redirected = XOpenDisplay(NULL);
    Window redirected_win = None;
    filter_core = filter_xi = filter_send = filter_allow = filter_logs = 0;
    int redirect_ok = 0;
    if (redirected == NULL) {
        snprintf(detail, sizeof(detail), "peer display failed");
    } else {
        redirected_win = XCreateSimpleWindow(redirected,
                                             DefaultRootWindow(redirected),
                                             320, 80, 200, 120, 0, 0,
                                             0x993366);
        XMapWindow(redirected, redirected_win);
        XSync(redirected, False);
        if (!composited) {
            snprintf(detail, sizeof(detail), "no compositor");
        } else if (!grab_target(gdk_dpy, redirected_win)) {
            snprintf(detail, sizeof(detail), "grab failed");
        } else if (!xtest_click(420, 140)) {
            snprintf(detail, sizeof(detail), "XTEST unavailable");
            ungrab_target(gdk_dpy, redirected_win);
        } else {
            pump(gtk_events_pending, gtk_main_iteration_do, 800);
            ungrab_target(gdk_dpy, redirected_win);
            redirect_ok = filter_allow && (filter_core || filter_xi);
            snprintf(detail, sizeof(detail), "core=%d send=%d xi=%d allow=%d",
                     filter_core, filter_send, filter_xi, filter_allow);
        }
        XDestroyWindow(redirected, redirected_win);
        XCloseDisplay(redirected);
    }
    result("gdk-grab-redirect", redirect_ok, detail);
    RECORD(redirect_ok);

    Display *hover = XOpenDisplay(NULL);
    Window hover_win = None;
    filter_core = filter_xi = filter_send = filter_allow = filter_logs = 0;
    int hover_ok = 0;
    if (hover == NULL) {
        snprintf(detail, sizeof(detail), "peer display failed");
    } else {
        hover_win = XCreateSimpleWindow(hover, DefaultRootWindow(hover),
                                        80, 400, 200, 40, 0, 0, 0x226622);
        XMapWindow(hover, hover_win);
        XSync(hover, False);
        if (!grab_target(gdk_dpy, hover_win)) {
            snprintf(detail, sizeof(detail), "grab failed");
        } else {
            Display *motion = XOpenDisplay(NULL);
            if (motion != NULL) {
                int i;
                for (i = 0; i < 8; i++) {
                    XTestFakeMotionEvent(motion, DefaultScreen(motion),
                                         90 + i * 4, 410, 0);
                }
                XSync(motion, False);
                XCloseDisplay(motion);
            }
            pump(gtk_events_pending, gtk_main_iteration_do, 400);
            filter_core = filter_xi = filter_send = filter_allow = filter_logs = 0;
            if (!xtest_click(100, 420)) {
                snprintf(detail, sizeof(detail), "XTEST unavailable");
            } else {
                pump(gtk_events_pending, gtk_main_iteration_do, 800);
                hover_ok = filter_allow && (filter_core || filter_xi);
                snprintf(detail, sizeof(detail), "core=%d send=%d xi=%d allow=%d",
                         filter_core, filter_send, filter_xi, filter_allow);
            }
            ungrab_target(gdk_dpy, hover_win);
        }
        XDestroyWindow(hover, hover_win);
        XCloseDisplay(hover);
    }
    result("gdk-grab-hover", hover_ok, detail);
    RECORD(hover_ok);

    /* Locating only: a session IM can make XFilterEvent eat GenericEvent
     * before default filters. Do not fail the 8/8 expect on this dump. */
    filter_core = filter_xi = filter_send = filter_allow = filter_logs = 0;
    XIM im = XOpenIM(gdk_dpy, NULL, NULL, NULL);
    XIC ic = NULL;
    if (im != NULL) {
        ic = XCreateIC(im, XNInputStyle,
                       XIMPreeditNothing | XIMStatusNothing,
                       XNClientWindow, self, XNFocusWindow, self, NULL);
        if (ic != NULL) XSetICFocus(ic);
    }
    if (xi_ok && grab_target(gdk_dpy, self) && xtest_click(self_x, self_y)) {
        pump(gtk_events_pending, gtk_main_iteration_do, 800);
        ungrab_target(gdk_dpy, self);
        printf("BXINFO gdk-grab-im core=%d send=%d xi=%d allow=%d im=%d ic=%d\n",
               filter_core, filter_send, filter_xi, filter_allow,
               im != NULL, ic != NULL);
        fflush(stdout);
    } else {
        printf("BXINFO gdk-grab-im skipped im=%d ic=%d\n",
               im != NULL, ic != NULL);
        fflush(stdout);
        if (xi_ok) ungrab_target(gdk_dpy, self);
    }
    if (ic != NULL) XDestroyIC(ic);
    if (im != NULL) XCloseIM(im);

    printf("BXSUMMARY gtk-gdk-grab passed=%d failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
