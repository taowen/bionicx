#define _POSIX_C_SOURCE 200809L

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <stdbool.h>
#include <stdint.h>
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

static void result(const char *name, bool ok, const char *detail) {
    printf("BXTEST %s %s%s%s\n", ok ? "PASS" : "FAIL", name,
           detail && *detail ? " " : "", detail ? detail : "");
    fflush(stdout);
}

/* Empty XSETTINGS body: byte-order, pad, serial, n_settings=0. */
static void write_empty_settings(unsigned char blob[12], uint32_t serial) {
    uint32_t order = 0x01020304;
    blob[0] = (*(char *)&order == 1) ? MSBFirst : LSBFirst;
    blob[1] = blob[2] = blob[3] = 0;
    memcpy(blob + 4, &serial, 4);
    memset(blob + 8, 0, 4);
}

struct wait_match {
    Window window;
    int type;
};

static Bool match_win_type(Display *display, XEvent *event, XPointer arg) {
    const struct wait_match *match = (const struct wait_match *)arg;
    (void)display;
    return event->xany.window == match->window
            && (event->type & 0x7f) == match->type;
}

static bool wait_typed(Display *display, Window window, int type, XEvent *event) {
    struct wait_match match = {.window = window, .type = type};
    XSync(display, False);
    if (XCheckIfEvent(display, event, match_win_type, (XPointer)&match))
        return true;
    for (int i = 0; i < 20; ++i) {
        nanosleep(&(struct timespec){.tv_nsec = 10000000L}, NULL);
        XSync(display, False);
        if (XCheckIfEvent(display, event, match_win_type, (XPointer)&match))
            return true;
    }
    return false;
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

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    Window manager = XCreateSimpleWindow(display, root, -1, -1, 1, 1, 0, 0, 0);
    XSelectInput(display, manager, PropertyChangeMask);
    XSelectInput(peer, root, StructureNotifyMask | PropertyChangeMask);
    XSync(display, False);
    XSync(peer, False);

    Atom timestamp_atom = XInternAtom(display, "_TIMESTAMP_PROP", False);
    Atom settings_atom = XInternAtom(display, "_XSETTINGS_SETTINGS", False);
    Atom selection = XInternAtom(display, "_XSETTINGS_S0", False);
    Atom manager_atom = XInternAtom(display, "MANAGER", False);
    Atom resource = XA_RESOURCE_MANAGER;

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int before = x_errors;
    unsigned char stamp = 'a';
    XChangeProperty(display, manager, timestamp_atom, timestamp_atom, 8,
                    PropModeReplace, &stamp, 1);
    XEvent event;
    bool stamp_ok = wait_typed(display, manager, PropertyNotify, &event)
            && event.xproperty.window == manager
            && event.xproperty.atom == timestamp_atom
            && x_errors == before;
    result("xsettings-timestamp", stamp_ok,
           stamp_ok ? "PropertyNotify" : "no PropertyNotify");
    RECORD(stamp_ok);
    Time server_time = stamp_ok ? event.xproperty.time : CurrentTime;

    before = x_errors;
    XSetSelectionOwner(display, selection, manager, server_time);
    Window owner = XGetSelectionOwner(display, selection);
    Window peer_owner = XGetSelectionOwner(peer, selection);
    XSync(display, False);
    XSync(peer, False);
    bool sel_ok = owner == manager && peer_owner == manager
            && x_errors == before;
    result("xsettings-selection", sel_ok,
           sel_ok ? "_XSETTINGS_S0" : "selection owner failed");
    RECORD(sel_ok);

    before = x_errors;
    unsigned char blob[12];
    write_empty_settings(blob, 1);
    XChangeProperty(display, manager, settings_atom, settings_atom, 8,
                    PropModeReplace, blob, sizeof(blob));
    XSync(display, False);
    Atom actual = None;
    int format = 0;
    unsigned long nitems = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;
    int rc = XGetWindowProperty(peer, manager, settings_atom, 0, 12, False,
                                settings_atom, &actual, &format, &nitems,
                                &after, &data);
    XSync(peer, False);
    bool settings_ok = rc == Success && actual == settings_atom && format == 8
            && nitems >= 12 && data != NULL && data[8] == 0 && data[9] == 0
            && data[10] == 0 && data[11] == 0 && x_errors == before;
    result("xsettings-settings", settings_ok,
           settings_ok ? "peer GetProperty" : "settings property failed");
    RECORD(settings_ok);
    if (data != NULL) XFree(data);

    before = x_errors;
    XClientMessageEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.window = root;
    xev.message_type = manager_atom;
    xev.format = 32;
    xev.data.l[0] = (long)server_time;
    xev.data.l[1] = (long)selection;
    xev.data.l[2] = (long)manager;
    Status sent = XSendEvent(display, root, False, StructureNotifyMask,
                             (XEvent *)&xev);
    XSync(display, False);
    bool manager_ok = sent != 0
            && wait_typed(peer, root, ClientMessage, &event)
            && event.xclient.message_type == manager_atom
            && (Window)event.xclient.data.l[2] == manager
            && x_errors == before;
    result("xsettings-manager-event", manager_ok,
           manager_ok ? "MANAGER ClientMessage" : "SendEvent MANAGER failed");
    RECORD(manager_ok);

    before = x_errors;
    const char *rm = "Xft.dpi:\t144\n";
    XChangeProperty(display, root, resource, XA_STRING, 8, PropModeReplace,
                    (unsigned char *)rm, (int)strlen(rm));
    XSync(display, False);
    bool rm_notify = wait_typed(peer, root, PropertyNotify, &event)
            && event.xproperty.atom == resource;
    Atom rm_actual = None;
    int rm_format = 0;
    unsigned long rm_nitems = 0;
    unsigned long rm_after = 0;
    unsigned char *rm_data = NULL;
    int rm_rc = XGetWindowProperty(peer, root, resource, 0, 64, False,
                                   XA_STRING, &rm_actual, &rm_format,
                                   &rm_nitems, &rm_after, &rm_data);
    char *rm_copy = NULL;
    if (rm_data != NULL) {
        rm_copy = malloc(rm_nitems + 1);
        if (rm_copy != NULL) {
            memcpy(rm_copy, rm_data, rm_nitems);
            rm_copy[rm_nitems] = '\0';
        }
    }
    bool rm_ok = rm_notify && rm_rc == Success && rm_actual == XA_STRING
            && rm_copy != NULL && strstr(rm_copy, "Xft.dpi") != NULL
            && x_errors == before;
    result("xsettings-resource-manager", rm_ok,
           rm_ok ? "RESOURCE_MANAGER" : "resource manager failed");
    RECORD(rm_ok);
    free(rm_copy);
    if (rm_data != NULL) XFree(rm_data);

    before = x_errors;
    XGrabServer(display);
    unsigned char stamp2 = 'b';
    XChangeProperty(display, manager, timestamp_atom, timestamp_atom, 8,
                    PropModeReplace, &stamp2, 1);
    bool grab_stamp = wait_typed(display, manager, PropertyNotify, &event);
    write_empty_settings(blob, 2);
    XChangeProperty(display, manager, settings_atom, settings_atom, 8,
                    PropModeReplace, blob, sizeof(blob));
    XSetSelectionOwner(display, selection, manager, event.xproperty.time);
    XUngrabServer(display);
    XSync(display, False);
    owner = XGetSelectionOwner(peer, selection);
    data = NULL;
    rc = XGetWindowProperty(peer, manager, settings_atom, 0, 12, False,
                            settings_atom, &actual, &format, &nitems,
                            &after, &data);
    bool grab_ok = grab_stamp && owner == manager && rc == Success
            && data != NULL && x_errors == before;
    result("xsettings-under-server", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or error");
    RECORD(grab_ok);
    if (data != NULL) XFree(data);

    printf("BXSUMMARY xsettings-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(display, manager);
    XCloseDisplay(peer);
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
