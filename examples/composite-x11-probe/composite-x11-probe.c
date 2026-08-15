#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xmd.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

typedef struct {
    uint8_t reqType;
    uint8_t compositeReqType;
    uint16_t length;
    uint32_t major;
    uint32_t minor;
} cmp_query_req;

typedef struct {
    uint8_t type;
    uint8_t pad;
    uint16_t sequenceNumber;
    uint32_t length;
    uint32_t major;
    uint32_t minor;
    uint32_t pad2[4];
} cmp_query_rep;

typedef struct {
    uint8_t reqType;
    uint8_t compositeReqType;
    uint16_t length;
    uint32_t window;
    uint8_t update;
    uint8_t pad1;
    uint16_t pad2;
} cmp_redirect_req;

typedef struct {
    uint8_t reqType;
    uint8_t compositeReqType;
    uint16_t length;
    uint32_t window;
    uint32_t pixmap;
} cmp_name_pixmap_req;

static void *reserve(Display *display, unsigned bytes) {
    if (display->bufptr + (int)bytes > display->bufmax) _XFlush(display);
    void *ptr = display->bufptr;
    display->last_req = (char *)ptr;
    display->bufptr += bytes;
    display->request++;
    return ptr;
}

static bool query_version(Display *display, int opcode, int *major, int *minor) {
    cmp_query_rep reply;
    LockDisplay(display);
    cmp_query_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->compositeReqType = 0;
    req->length = 3;
    req->major = 0;
    req->minor = 4;
    if (!_XReply(display, (xReply *)&reply, 0, xTrue)) {
        UnlockDisplay(display);
        return false;
    }
    *major = (int)reply.major;
    *minor = (int)reply.minor;
    UnlockDisplay(display);
    return true;
}

static void redirect_window(Display *display, int opcode, uint32_t window,
                            uint8_t update) {
    LockDisplay(display);
    cmp_redirect_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->compositeReqType = 1;
    req->length = 3;
    req->window = window;
    req->update = update;
    req->pad1 = 0;
    req->pad2 = 0;
    UnlockDisplay(display);
    _XFlush(display);
}

static void redirect_subwindows(Display *display, int opcode, uint32_t window,
                                uint8_t update) {
    LockDisplay(display);
    cmp_redirect_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->compositeReqType = 2;
    req->length = 3;
    req->window = window;
    req->update = update;
    req->pad1 = 0;
    req->pad2 = 0;
    UnlockDisplay(display);
    _XFlush(display);
}

static void unredirect_window(Display *display, int opcode, uint32_t window,
                              uint8_t update) {
    LockDisplay(display);
    cmp_redirect_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->compositeReqType = 3;
    req->length = 3;
    req->window = window;
    req->update = update;
    req->pad1 = 0;
    req->pad2 = 0;
    UnlockDisplay(display);
    _XFlush(display);
}

static void unredirect_subwindows(Display *display, int opcode, uint32_t window,
                                  uint8_t update) {
    LockDisplay(display);
    cmp_redirect_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->compositeReqType = 4;
    req->length = 3;
    req->window = window;
    req->update = update;
    req->pad1 = 0;
    req->pad2 = 0;
    UnlockDisplay(display);
    _XFlush(display);
}

static void name_window_pixmap(Display *display, int opcode, uint32_t window,
                               uint32_t pixmap) {
    LockDisplay(display);
    cmp_name_pixmap_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->compositeReqType = 6;
    req->length = 3;
    req->window = window;
    req->pixmap = pixmap;
    UnlockDisplay(display);
    _XFlush(display);
}

static bool paint_and_read(Display *display, Window window, Pixmap pixmap,
                           unsigned long *pixel) {
    int screen = DefaultScreen(display);
    Visual *visual = DefaultVisual(display, screen);
    int depth = DefaultDepth(display, screen);
    char *pixels = calloc(8u * 8u, 4u);
    if (pixels == NULL) return false;
    XImage *image = XCreateImage(display, visual, (unsigned)depth, ZPixmap, 0,
                                 pixels, 8, 8, 32, 0);
    GC gc = XCreateGC(display, window, 0, NULL);
    if (image == NULL || gc == None) {
        if (image != NULL) XDestroyImage(image);
        else free(pixels);
        if (gc != None) XFreeGC(display, gc);
        return false;
    }
    XPutPixel(image, 0, 0, 0x00cc44);
    XPutPixel(image, 1, 0, 0x00cc44);
    XPutImage(display, window, gc, image, 0, 0, 0, 0, 8, 8);
    XDestroyImage(image);
    XFreeGC(display, gc);
    XSync(display, False);
    XImage *got = XGetImage(display, pixmap, 0, 0, 8, 8, AllPlanes, ZPixmap);
    if (got == NULL) return false;
    *pixel = XGetPixel(got, 0, 0);
    XDestroyImage(got);
    return true;
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

    int opcode = 0;
    int event_base = 0;
    int error_base = 0;
    if (!XQueryExtension(display, "Composite", &opcode, &event_base,
                         &error_base) || opcode == 0) {
        fprintf(stderr, "BXFAIL Composite unavailable\n");
        XCloseDisplay(display);
        XCloseDisplay(peer);
        return 2;
    }

    Window root = DefaultRootWindow(display);
    Window window = XCreateSimpleWindow(display, root, 40, 40, 160, 100, 0,
                                        0, 0x224466);
    Window parent = XCreateSimpleWindow(display, root, 220, 40, 180, 120, 0,
                                        0, 0x112233);
    Window child = XCreateSimpleWindow(display, parent, 8, 8, 80, 60, 0,
                                       0, 0x336699);
    XMapWindow(display, window);
    XMapWindow(display, parent);
    XMapWindow(display, child);
    XSync(display, False);

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int before = x_errors;
    int major = 0;
    int minor = 0;
    bool version_ok = query_version(display, opcode, &major, &minor)
            && (major > 0 || minor >= 2) && x_errors == before;
    char version_detail[64];
    if (version_ok)
        snprintf(version_detail, sizeof(version_detail),
                 "Composite %d.%d", major, minor);
    else
        snprintf(version_detail, sizeof(version_detail),
                 "QueryVersion failed %d.%d", major, minor);
    result("composite-version", version_ok, version_detail);
    RECORD(version_ok);

    before = x_errors;
    redirect_window(display, opcode, (uint32_t)window, 1);
    XSync(display, False);
    bool redirect_ok = x_errors == before;
    result("composite-redirect", redirect_ok,
           redirect_ok ? "RedirectWindow Manual" : "RedirectWindow failed");
    RECORD(redirect_ok);

    before = x_errors;
    uint32_t pixmap = (uint32_t)XAllocID(display);
    name_window_pixmap(display, opcode, (uint32_t)window, pixmap);
    XSync(display, False);
    unsigned long pixel = 0;
    bool name_ok = x_errors == before
            && paint_and_read(display, window, (Pixmap)pixmap, &pixel)
            && pixel == 0x00cc44 && x_errors == before;
    result("composite-name-pixmap", name_ok,
           name_ok ? "NameWindowPixmap GetImage" : "name or pixel failed");
    RECORD(name_ok);

    before = x_errors;
    unredirect_window(display, opcode, (uint32_t)window, 1);
    XSync(display, False);
    bool unredirect_ok = x_errors == before;
    result("composite-unredirect", unredirect_ok,
           unredirect_ok ? "UnredirectWindow" : "UnredirectWindow failed");
    RECORD(unredirect_ok);

    before = x_errors;
    redirect_subwindows(display, opcode, (uint32_t)parent, 1);
    uint32_t child_pixmap = (uint32_t)XAllocID(display);
    name_window_pixmap(display, opcode, (uint32_t)child, child_pixmap);
    XSync(display, False);
    unsigned long child_pixel = 0;
    bool sub_ok = x_errors == before
            && paint_and_read(display, child, (Pixmap)child_pixmap, &child_pixel)
            && child_pixel == 0x00cc44 && x_errors == before;
    unredirect_subwindows(display, opcode, (uint32_t)parent, 1);
    XSync(display, False);
    result("composite-subwindows", sub_ok,
           sub_ok ? "RedirectSubwindows NameWindowPixmap"
                  : "subwindows name failed");
    RECORD(sub_ok);

    before = x_errors;
    XGrabServer(display);
    redirect_window(display, opcode, (uint32_t)window, 1);
    uint32_t grab_pixmap = (uint32_t)XAllocID(display);
    name_window_pixmap(display, opcode, (uint32_t)window, grab_pixmap);
    unsigned long grab_pixel = 0;
    bool grab_read = paint_and_read(display, window, (Pixmap)grab_pixmap,
                                    &grab_pixel);
    unredirect_window(display, opcode, (uint32_t)window, 1);
    XUngrabServer(display);
    XSync(display, False);
    bool grab_ok = grab_read && grab_pixel == 0x00cc44 && x_errors == before;
    result("composite-under-server", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or error");
    RECORD(grab_ok);

    printf("BXSUMMARY composite-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(display, child);
    XDestroyWindow(display, parent);
    XDestroyWindow(display, window);
    XCloseDisplay(peer);
    XCloseDisplay(display);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
