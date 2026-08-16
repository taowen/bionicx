#define _POSIX_C_SOURCE 200809L

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
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

static bool wait_typed(Display *display, int type, XEvent *event) {
    XSync(display, False);
    if (XCheckTypedEvent(display, type, event)) return true;
    for (int i = 0; i < 30; ++i) {
        nanosleep(&(struct timespec){.tv_nsec = 10000000L}, NULL);
        XSync(display, False);
        if (XCheckTypedEvent(display, type, event)) return true;
    }
    return false;
}

static bool reply_utf8(Display *display, const XSelectionRequestEvent *request,
                       Atom utf8, const char *payload) {
    if (request->property == None) return false;
    XChangeProperty(display, request->requestor, request->property, utf8, 8,
                    PropModeReplace, (const unsigned char *)payload,
                    (int)strlen(payload));
    XSelectionEvent reply = {0};
    reply.type = SelectionNotify;
    reply.display = display;
    reply.requestor = request->requestor;
    reply.selection = request->selection;
    reply.target = request->target;
    reply.property = request->property;
    reply.time = request->time;
    return XSendEvent(display, request->requestor, False, NoEventMask,
                      (XEvent *)&reply) != 0;
}

static bool read_utf8(Display *display, Window window, Atom property,
                      Atom utf8, const char *payload) {
    Atom actual = None;
    int format = 0;
    unsigned long nitems = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;
    int rc = XGetWindowProperty(display, window, property, 0, 1024, True, utf8,
                                &actual, &format, &nitems, &after, &data);
    bool ok = rc == Success && actual == utf8 && format == 8
            && nitems == strlen(payload) && after == 0 && data != NULL
            && memcmp(data, payload, strlen(payload)) == 0;
    if (data != NULL) XFree(data);
    return ok;
}

int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 3;
    Display *manager = XOpenDisplay(NULL);
    Display *peer = XOpenDisplay(NULL);
    if (manager == NULL || peer == NULL) {
        fprintf(stderr, "BXFAIL open X11 connections\n");
        return 2;
    }
    XSetErrorHandler(on_x_error);

    int event_base = 0, error_base = 0;
    if (!XFixesQueryExtension(manager, &event_base, &error_base)) {
        fprintf(stderr, "BXFAIL XFixes missing\n");
        return 2;
    }

    int screen = DefaultScreen(manager);
    Window root = RootWindow(manager, screen);
    Window mgr = XCreateSimpleWindow(manager, root, -1, -1, 1, 1, 0, 0, 0);
    Window owner = XCreateSimpleWindow(peer, root, -1, -1, 1, 1, 0, 0, 0);
    XSelectInput(manager, mgr, PropertyChangeMask);
    XSelectInput(peer, owner, PropertyChangeMask);
    XSync(manager, False);
    XSync(peer, False);

    Atom clipboard = XInternAtom(manager, "CLIPBOARD", False);
    Atom manager_sel = XInternAtom(manager, "CLIPBOARD_MANAGER", False);
    Atom utf8 = XInternAtom(manager, "UTF8_STRING", False);
    Atom cache_prop = XInternAtom(manager, "BX_CM_CACHE", False);
    Atom serve_prop = XInternAtom(peer, "BX_CM_SERVE", False);
    const char payload[] = "BionicX_clipboard_manager_persist";

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int before = x_errors;
    XSetSelectionOwner(manager, manager_sel, mgr, CurrentTime);
    XSync(manager, False);
    bool sel_ok = XGetSelectionOwner(manager, manager_sel) == mgr
            && XGetSelectionOwner(peer, manager_sel) == mgr
            && x_errors == before;
    result("clipboard-manager-selection", sel_ok,
           sel_ok ? "CLIPBOARD_MANAGER" : "selection owner failed");
    RECORD(sel_ok);

    before = x_errors;
    XFixesSelectSelectionInput(manager, mgr, clipboard,
                               XFixesSetSelectionOwnerNotifyMask |
                               XFixesSelectionWindowDestroyNotifyMask |
                               XFixesSelectionClientCloseNotifyMask);
    XSync(manager, False);
    XSetSelectionOwner(peer, clipboard, owner, CurrentTime);
    XSync(peer, False);
    XEvent event;
    bool watch_ok = XGetSelectionOwner(manager, clipboard) == owner
            && wait_typed(manager, event_base + XFixesSelectionNotify, &event)
            && event.xany.window == mgr
            && x_errors == before;
    result("clipboard-manager-watch", watch_ok,
           watch_ok ? "XFixes SetOwner" : "no selection notify");
    RECORD(watch_ok);

    before = x_errors;
    XConvertSelection(manager, clipboard, utf8, cache_prop, mgr, CurrentTime);
    XFlush(manager);
    bool request_ok = wait_typed(peer, SelectionRequest, &event)
            && event.xselectionrequest.owner == owner
            && event.xselectionrequest.requestor == mgr
            && event.xselectionrequest.target == utf8
            && reply_utf8(peer, &event.xselectionrequest, utf8, payload);
    XFlush(peer);
    bool cache_ok = request_ok
            && wait_typed(manager, SelectionNotify, &event)
            && event.xselection.property == cache_prop
            && read_utf8(manager, mgr, cache_prop, utf8, payload)
            && x_errors == before;
    result("clipboard-manager-cache", cache_ok,
           cache_ok ? "UTF8_STRING cached" : "manager ConvertSelection failed");
    RECORD(cache_ok);

    before = x_errors;
    XDestroyWindow(peer, owner);
    XSync(peer, False);
    bool destroy_ok = wait_typed(manager,
                                 event_base + XFixesSelectionNotify, &event);
    XSetSelectionOwner(manager, clipboard, mgr, CurrentTime);
    XSync(manager, False);
    bool takeover_ok = destroy_ok
            && XGetSelectionOwner(peer, clipboard) == mgr
            && x_errors == before;
    result("clipboard-manager-takeover", takeover_ok,
           takeover_ok ? "manager owns CLIPBOARD" : "takeover failed");
    RECORD(takeover_ok);

    before = x_errors;
    Window requestor = XCreateSimpleWindow(peer, root, -1, -1, 1, 1, 0, 0, 0);
    XSelectInput(peer, requestor, PropertyChangeMask);
    XConvertSelection(peer, clipboard, utf8, serve_prop, requestor,
                      CurrentTime);
    XFlush(peer);
    bool serve_req = wait_typed(manager, SelectionRequest, &event)
            && event.xselectionrequest.owner == mgr
            && event.xselectionrequest.requestor == requestor
            && reply_utf8(manager, &event.xselectionrequest, utf8, payload);
    XFlush(manager);
    bool persist_ok = serve_req
            && wait_typed(peer, SelectionNotify, &event)
            && event.xselection.property == serve_prop
            && read_utf8(peer, requestor, serve_prop, utf8, payload)
            && x_errors == before;
    result("clipboard-manager-persist", persist_ok,
           persist_ok ? "served after owner destroy" : "persist failed");
    RECORD(persist_ok);

    before = x_errors;
    XGrabServer(manager);
    XSetSelectionOwner(manager, clipboard, mgr, CurrentTime);
    XUngrabServer(manager);
    XSync(manager, False);
    bool grab_ok = XGetSelectionOwner(peer, clipboard) == mgr
            && x_errors == before;
    result("clipboard-manager-under-server", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or error");
    RECORD(grab_ok);

    printf("BXSUMMARY clipboard-manager-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(peer, requestor);
    XDestroyWindow(manager, mgr);
    XCloseDisplay(peer);
    XCloseDisplay(manager);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
