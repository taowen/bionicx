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
} cmp_overlay_req;

typedef struct {
    uint8_t type;
    uint8_t pad;
    uint16_t sequenceNumber;
    uint32_t length;
    uint32_t overlay;
    uint32_t pad2[5];
} cmp_overlay_rep;

static void *reserve(Display *display, unsigned bytes) {
    if (display->bufptr + (int)bytes > display->bufmax) _XFlush(display);
    void *ptr = display->bufptr;
    display->last_req = (char *)ptr;
    display->bufptr += bytes;
    display->request++;
    return ptr;
}

static bool cmp_query(Display *display, int opcode, int *major, int *minor) {
    cmp_query_rep reply;
    LockDisplay(display);
    cmp_query_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->compositeReqType = 0;
    req->length = 3;
    req->major = 0;
    req->minor = 3;
    if (!_XReply(display, (xReply *)&reply, 0, xTrue)) {
        UnlockDisplay(display);
        return false;
    }
    UnlockDisplay(display);
    *major = (int)reply.major;
    *minor = (int)reply.minor;
    return true;
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

static uint32_t get_overlay_window(Display *display, int opcode,
                                   uint32_t window) {
    cmp_overlay_rep reply;
    LockDisplay(display);
    cmp_overlay_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->compositeReqType = 7;
    req->length = 2;
    req->window = window;
    if (!_XReply(display, (xReply *)&reply, 0, xTrue)) {
        UnlockDisplay(display);
        return 0;
    }
    UnlockDisplay(display);
    return reply.overlay;
}

static bool fill_window(Display *display, Window window, unsigned long color) {
    XWindowAttributes attributes;
    if (!XGetWindowAttributes(display, window, &attributes)) return false;
    unsigned width = (unsigned)attributes.width;
    unsigned height = (unsigned)attributes.height;
    if (width == 0 || height == 0) return false;
    char *pixels = calloc((size_t)width * height, 4u);
    if (pixels == NULL) return false;
    XImage *image = XCreateImage(display, attributes.visual,
                                 (unsigned)attributes.depth, ZPixmap, 0,
                                 pixels, width, height, 32, 0);
    GC gc = XCreateGC(display, window, 0, NULL);
    if (image == NULL || gc == None) {
        if (image != NULL) XDestroyImage(image);
        else free(pixels);
        if (gc != None) XFreeGC(display, gc);
        return false;
    }
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x)
            XPutPixel(image, (int)x, (int)y, color);
    }
    XPutImage(display, window, gc, image, 0, 0, 0, 0, width, height);
    XDestroyImage(image);
    XFreeGC(display, gc);
    return true;
}

static bool read_pixel(Display *display, Drawable drawable, int x, int y,
                       unsigned long *pixel) {
    XImage *got = XGetImage(display, drawable, x, y, 1, 1, AllPlanes, ZPixmap);
    if (got == NULL) return false;
    *pixel = XGetPixel(got, 0, 0);
    XDestroyImage(got);
    return true;
}

int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 3;
    Display *comp = XOpenDisplay(NULL);
    Display *app = XOpenDisplay(NULL);
    if (comp == NULL || app == NULL) {
        fprintf(stderr, "BXFAIL open two X11 connections\n");
        return 2;
    }
    XSetErrorHandler(on_x_error);

    int cmp_op = 0, cmp_ev = 0, cmp_err = 0;
    if (!XQueryExtension(comp, "Composite", &cmp_op, &cmp_ev, &cmp_err)
            || cmp_op == 0) {
        fprintf(stderr, "BXFAIL Composite unavailable\n");
        return 2;
    }

    Window root = DefaultRootWindow(app);
    const int win_x = 360, win_y = 220;
    const unsigned int old_w = 320, old_h = 200;
    const unsigned int new_w = 480, new_h = 280;
    const unsigned long before_color = 0x00cc2244;
    const unsigned long after_color = 0x002266cc;

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int before = x_errors;
    int cmp_major = 0, cmp_minor = 0;
    bool versions_ok = cmp_query(comp, cmp_op, &cmp_major, &cmp_minor)
            && (cmp_major > 0 || cmp_minor >= 2) && x_errors == before;
    result("resize-screen-versions", versions_ok,
           versions_ok ? "Composite" : "QueryVersion failed");
    RECORD(versions_ok);

    before = x_errors;
    Window overlay = (Window)get_overlay_window(comp, cmp_op, (uint32_t)root);
    if (overlay != 0) XMapWindow(comp, overlay);
    redirect_subwindows(comp, cmp_op, (uint32_t)root, 1);
    XSync(comp, False);
    bool overlay_ok = overlay != 0 && x_errors == before;
    result("resize-screen-overlay", overlay_ok,
           overlay_ok ? "GetOverlayWindow + RedirectSubwindows(root)"
                      : "overlay or root redirect failed");
    RECORD(overlay_ok);

    Window window = XCreateSimpleWindow(app, root, win_x, win_y, old_w, old_h,
                                        0, 0, before_color);
    XSelectInput(app, window, StructureNotifyMask | ExposureMask);
    XMapWindow(app, window);
    XSync(app, False);
    fill_window(app, window, before_color);
    XSync(app, False);

    unsigned long live = 0;
    bool live_ok = read_pixel(app, window, 16, 16, &live)
            && (live & 0xffffff) == (before_color & 0xffffff)
            && x_errors == before;
    char live_detail[80];
    if (live_ok)
        snprintf(live_detail, sizeof(live_detail),
                 "GetImage sees fill before resize");
    else
        snprintf(live_detail, sizeof(live_detail),
                 "got=0x%lx want=0x%lx", live, before_color);
    result("resize-screen-before", live_ok, live_detail);
    RECORD(live_ok);

    before = x_errors;
    XGCValues values = {.foreground = after_color};
    GC fill = XCreateGC(app, window, GCForeground, &values);
    bool filled = fill != None;
    if (filled) {
        XFillRectangle(app, window, fill, 0, 0, old_w, old_h);
        XFreeGC(app, fill);
    }
    XSync(app, False);
    unsigned long filled_pixel = 0;
    bool fill_ok = filled && read_pixel(app, window, 16, 16, &filled_pixel)
            && (filled_pixel & 0xffffff) == (after_color & 0xffffff)
            && x_errors == before;
    char fill_detail[80];
    if (fill_ok)
        snprintf(fill_detail, sizeof(fill_detail),
                 "XFillRectangle visible to GetImage");
    else
        snprintf(fill_detail, sizeof(fill_detail),
                 "got=0x%lx want=0x%lx", filled_pixel, after_color);
    result("resize-screen-fillrect", fill_ok, fill_detail);
    RECORD(fill_ok);

    before = x_errors;
    XResizeWindow(app, window, new_w, new_h);
    XSync(app, False);
    fill_window(app, window, after_color);
    XSync(app, False);
    unsigned long resized = 0;
    bool resize_ok = read_pixel(app, window, 16, 16, &resized)
            && (resized & 0xffffff) == (after_color & 0xffffff)
            && x_errors == before;
    char resize_detail[80];
    if (resize_ok)
        snprintf(resize_detail, sizeof(resize_detail),
                 "GetImage sees fill after resize");
    else
        snprintf(resize_detail, sizeof(resize_detail),
                 "got=0x%lx want=0x%lx", resized, after_color);
    result("resize-screen-after", resize_ok, resize_detail);
    RECORD(resize_ok);

    printf("BXGEOM x=%d y=%d w=%u h=%u color=0x%06lx\n",
           win_x, win_y, new_w, new_h, after_color & 0xffffff);
    printf("BXSUMMARY resize-screen-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(app, window);
    XCloseDisplay(app);
    XCloseDisplay(comp);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
