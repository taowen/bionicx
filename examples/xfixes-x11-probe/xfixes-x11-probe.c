#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xfixes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

    int event_base = 0;
    int error_base = 0;
    int major = 4;
    int minor = 0;
    if (!XFixesQueryExtension(display, &event_base, &error_base)
            || !XFixesQueryVersion(display, &major, &minor) || major < 4) {
        fprintf(stderr, "BXFAIL XFixes 4 unavailable major=%d\n", major);
        XCloseDisplay(display);
        if (peer != NULL) XCloseDisplay(peer);
        return 2;
    }

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    Window window = XCreateSimpleWindow(display, root, 40, 40, 200, 120, 0,
                                        0, 0x336699);
    XMapWindow(display, window);
    Window peer_window = XCreateSimpleWindow(peer, RootWindow(peer, screen),
                                             80, 80, 80, 80, 0, 0, 0x224466);
    XMapWindow(peer, peer_window);
    XSync(display, False);
    XSync(peer, False);

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int before = x_errors;
    XFixesSelectCursorInput(display, window, XFixesDisplayCursorNotifyMask);
    XSync(display, False);
    bool select_ok = x_errors == before;
    result("xfixes-select-cursor", select_ok,
           select_ok ? "SelectCursorInput" : "select failed");
    RECORD(select_ok);

    before = x_errors;
    XFixesCursorImage *image = XFixesGetCursorImage(display);
    XSync(display, False);
    bool image_ok = image != NULL && image->width >= 1 && image->height >= 1
            && x_errors == before;
    result("xfixes-cursor-image", image_ok,
           image_ok ? "GetCursorImageAndName" : "GetCursorImage failed");
    RECORD(image_ok);
    if (image != NULL) XFree(image);

    before = x_errors;
    Cursor cursor = XCreateFontCursor(display, XC_left_ptr);
    XFixesSetCursorName(display, cursor, "bionicx-left-ptr");
    Atom name_atom = None;
    const char *name = XFixesGetCursorName(display, cursor, &name_atom);
    XSync(display, False);
    bool name_ok = name != NULL && strcmp(name, "bionicx-left-ptr") == 0
            && name_atom != None && x_errors == before;
    result("xfixes-cursor-name", name_ok,
           name_ok ? "Set/GetCursorName" : "cursor name failed");
    RECORD(name_ok);

    before = x_errors;
    Cursor watch = XCreateFontCursor(display, XC_watch);
    XDefineCursor(display, window, cursor);
    XFixesChangeCursor(display, cursor, watch);
    XFixesChangeCursorByName(display, watch, "bionicx-left-ptr");
    XSync(display, False);
    bool change_ok = x_errors == before;
    result("xfixes-change-cursor", change_ok,
           change_ok ? "ChangeCursor family" : "ChangeCursor failed");
    RECORD(change_ok);

    before = x_errors;
    XFixesHideCursor(display, window);
    XFixesShowCursor(display, window);
    XSync(display, False);
    bool hide_ok = x_errors == before;
    result("xfixes-hide-show", hide_ok,
           hide_ok ? "Hide/ShowCursor" : "Hide/Show failed");
    RECORD(hide_ok);

    before = x_errors;
    XFixesChangeSaveSet(display, peer_window, SetModeInsert, SaveSetNearest,
                        SaveSetMap);
    XSync(display, False);
    bool save_ok = x_errors == before;
    result("xfixes-saveset", save_ok,
           save_ok ? "ChangeSaveSet insert" : "ChangeSaveSet failed");
    RECORD(save_ok);

    before = x_errors;
    XGrabServer(display);
    XFixesSelectCursorInput(display, window, XFixesDisplayCursorNotifyMask);
    image = XFixesGetCursorImage(display);
    XFixesHideCursor(display, window);
    XFixesShowCursor(display, window);
    XFixesChangeSaveSet(display, peer_window, SetModeDelete, SaveSetNearest,
                        SaveSetMap);
    XUngrabServer(display);
    XSync(display, False);
    bool grab_ok = image != NULL && x_errors == before;
    result("xfixes-under-server", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or error");
    RECORD(grab_ok);
    if (image != NULL) XFree(image);

    printf("BXSUMMARY xfixes-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XFreeCursor(display, watch);
    XFreeCursor(display, cursor);
    XDestroyWindow(display, window);
    XDestroyWindow(peer, peer_window);
    XCloseDisplay(peer);
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
