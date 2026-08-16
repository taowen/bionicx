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

typedef struct {
    uint8_t reqType;
    uint8_t damageReqType;
    uint16_t length;
    uint32_t major;
    uint32_t minor;
} dmg_query_req;

typedef struct {
    uint8_t type;
    uint8_t pad;
    uint16_t sequenceNumber;
    uint32_t length;
    uint32_t major;
    uint32_t minor;
    uint32_t pad2[4];
} dmg_query_rep;

typedef struct {
    uint8_t reqType;
    uint8_t damageReqType;
    uint16_t length;
    uint32_t damage;
    uint32_t drawable;
    uint8_t level;
    uint8_t pad1;
    uint16_t pad2;
} dmg_create_req;

typedef struct {
    uint8_t reqType;
    uint8_t damageReqType;
    uint16_t length;
    uint32_t damage;
} dmg_destroy_req;

typedef struct {
    uint8_t reqType;
    uint8_t damageReqType;
    uint16_t length;
    uint32_t damage;
    uint32_t repair;
    uint32_t parts;
} dmg_subtract_req;

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

static bool dmg_query(Display *display, int opcode, int *major, int *minor) {
    dmg_query_rep reply;
    LockDisplay(display);
    dmg_query_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->damageReqType = 0;
    req->length = 3;
    req->major = 1;
    req->minor = 1;
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

static void damage_create(Display *display, int opcode, uint32_t damage,
                          uint32_t drawable, uint8_t level) {
    LockDisplay(display);
    dmg_create_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->damageReqType = 1;
    req->length = 4;
    req->damage = damage;
    req->drawable = drawable;
    req->level = level;
    req->pad1 = 0;
    req->pad2 = 0;
    UnlockDisplay(display);
    _XFlush(display);
}

static void damage_subtract(Display *display, int opcode, uint32_t damage) {
    LockDisplay(display);
    dmg_subtract_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->damageReqType = 3;
    req->length = 4;
    req->damage = damage;
    req->repair = 0;
    req->parts = 0;
    UnlockDisplay(display);
    _XFlush(display);
}

static void damage_destroy(Display *display, int opcode, uint32_t damage) {
    LockDisplay(display);
    dmg_destroy_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->damageReqType = 2;
    req->length = 2;
    req->damage = damage;
    UnlockDisplay(display);
    _XFlush(display);
}

static Bool damage_wire_to_event(Display *display, XEvent *re, xEvent *event) {
    re->type = event->u.u.type & 0x7f;
    re->xany.serial = _XSetLastRequestRead(display, (xGenericReply *)event);
    re->xany.send_event = (event->u.u.type & 0x80) != 0;
    re->xany.display = display;
    re->xany.window = event->u.clientMessage.window;
    return True;
}

static bool wait_damage(Display *display, int type, XEvent *event) {
    XSync(display, False);
    if (XCheckTypedEvent(display, type, event)) return true;
    for (int i = 0; i < 50; ++i) {
        nanosleep(&(struct timespec){.tv_nsec = 20000000L}, NULL);
        XSync(display, False);
        if (XCheckTypedEvent(display, type, event)) return true;
    }
    return false;
}

static bool paint_window(Display *display, Window window,
                         unsigned long color) {
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
    XPutPixel(image, 0, 0, color);
    XPutPixel(image, 1, 0, color);
    XPutImage(display, window, gc, image, 0, 0, 0, 0, 8, 8);
    XDestroyImage(image);
    XFreeGC(display, gc);
    return true;
}

static bool read_pixel(Display *display, Drawable drawable,
                       unsigned long *pixel) {
    XImage *got = XGetImage(display, drawable, 0, 0, 8, 8, AllPlanes, ZPixmap);
    if (got == NULL) return false;
    *pixel = XGetPixel(got, 0, 0);
    XDestroyImage(got);
    return true;
}

static bool pixmap_size(Display *display, Pixmap pixmap,
                        unsigned int *width, unsigned int *height) {
    Window root = 0;
    int x = 0, y = 0;
    unsigned int border = 0, depth = 0;
    return XGetGeometry(display, pixmap, &root, &x, &y, width, height,
                        &border, &depth) != 0;
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
    int dmg_op = 0, dmg_ev = 0, dmg_err = 0;
    if (!XQueryExtension(comp, "Composite", &cmp_op, &cmp_ev, &cmp_err)
            || cmp_op == 0
            || !XQueryExtension(comp, "DAMAGE", &dmg_op, &dmg_ev, &dmg_err)
            || dmg_op == 0 || dmg_ev <= 0) {
        fprintf(stderr, "BXFAIL Composite or DAMAGE unavailable\n");
        XCloseDisplay(comp);
        XCloseDisplay(app);
        return 2;
    }
    XESetWireToEvent(comp, dmg_ev, damage_wire_to_event);

    Window root = DefaultRootWindow(app);
    const unsigned int old_w = 160, old_h = 100;
    const unsigned int new_w = 240, new_h = 180;
    Window window = XCreateSimpleWindow(app, root, 40, 40, old_w, old_h, 0,
                                        0, 0x224466);
    XSelectInput(app, window, StructureNotifyMask);
    XMapWindow(app, window);
    XSync(app, False);
    XSync(comp, False);

    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int before = x_errors;
    int cmp_major = 0, cmp_minor = 0, dmg_major = 0, dmg_minor = 0;
    bool versions_ok = cmp_query(comp, cmp_op, &cmp_major, &cmp_minor)
            && (cmp_major > 0 || cmp_minor >= 2)
            && dmg_query(comp, dmg_op, &dmg_major, &dmg_minor)
            && dmg_major >= 1 && x_errors == before;
    char version_detail[80];
    if (versions_ok)
        snprintf(version_detail, sizeof(version_detail),
                 "Composite %d.%d DAMAGE %d.%d",
                 cmp_major, cmp_minor, dmg_major, dmg_minor);
    else
        snprintf(version_detail, sizeof(version_detail), "QueryVersion failed");
    result("named-resize-versions", versions_ok, version_detail);
    RECORD(versions_ok);

    before = x_errors;
    redirect_window(comp, cmp_op, (uint32_t)window, 1);
    uint32_t named = (uint32_t)XAllocID(comp);
    name_window_pixmap(comp, cmp_op, (uint32_t)window, named);
    uint32_t damage = (uint32_t)XAllocID(comp);
    damage_create(comp, dmg_op, damage, (uint32_t)window, 3);
    XSync(comp, False);
    bool painted = paint_window(app, window, 0x00cc44);
    XSync(app, False);
    XEvent event = {0};
    bool saw_damage = wait_damage(comp, dmg_ev, &event);
    unsigned int named_w = 0, named_h = 0;
    unsigned long named_pixel = 0;
    bool live_ok = painted && saw_damage && x_errors == before
            && pixmap_size(comp, (Pixmap)named, &named_w, &named_h)
            && named_w == old_w && named_h == old_h
            && read_pixel(comp, (Drawable)named, &named_pixel)
            && named_pixel == 0x00cc44 && x_errors == before;
    result("named-live-before-resize", live_ok,
           live_ok ? "NameWindowPixmap tracks PutImage"
                   : "named pixmap missed live paint");
    RECORD(live_ok);

    before = x_errors;
    damage_subtract(comp, dmg_op, damage);
    XSync(comp, False);
    XResizeWindow(app, window, new_w, new_h);
    XSync(app, False);
    XSync(comp, False);
    painted = paint_window(app, window, 0xee8822);
    XSync(app, False);
    unsigned int stale_w = 0, stale_h = 0;
    unsigned long stale_pixel = 0;
    bool stale_ok = painted && x_errors == before
            && pixmap_size(comp, (Pixmap)named, &stale_w, &stale_h)
            && stale_w == old_w && stale_h == old_h
            && read_pixel(comp, (Drawable)named, &stale_pixel)
            && stale_pixel == 0x00cc44 && stale_pixel != 0xee8822
            && x_errors == before;
    result("named-stale-after-resize", stale_ok,
           stale_ok ? "old pixmap frozen at old size"
                    : "old pixmap tracked resize or new paint");
    RECORD(stale_ok);

    before = x_errors;
    uint32_t renamed = (uint32_t)XAllocID(comp);
    name_window_pixmap(comp, cmp_op, (uint32_t)window, renamed);
    XSync(comp, False);
    unsigned int new_named_w = 0, new_named_h = 0;
    unsigned long renamed_pixel = 0;
    bool rename_ok = x_errors == before
            && pixmap_size(comp, (Pixmap)renamed, &new_named_w, &new_named_h)
            && new_named_w == new_w && new_named_h == new_h
            && read_pixel(comp, (Drawable)renamed, &renamed_pixel)
            && renamed_pixel == 0xee8822 && x_errors == before;
    result("named-rename-after-resize", rename_ok,
           rename_ok ? "new NameWindowPixmap sees new backing"
                     : "re-name missed new size or pixels");
    RECORD(rename_ok);

    before = x_errors;
    painted = paint_window(app, window, 0x3366cc);
    XSync(app, False);
    unsigned long live_again = 0;
    bool after_ok = painted && x_errors == before
            && read_pixel(comp, (Drawable)renamed, &live_again)
            && live_again == 0x3366cc && x_errors == before;
    result("named-live-after-rename", after_ok,
           after_ok ? "re-named pixmap tracks PutImage"
                    : "re-named pixmap not live");
    RECORD(after_ok);

    before = x_errors;
    damage_subtract(comp, dmg_op, damage);
    XSync(comp, False);
    painted = paint_window(app, window, 0xaa44ee);
    XSync(app, False);
    saw_damage = wait_damage(comp, dmg_ev, &event);
    bool damage_ok = painted && saw_damage && x_errors == before;
    result("damage-after-resize", damage_ok,
           damage_ok ? "PutImage DamageNotify after resize"
                     : "no DamageNotify after resize paint");
    RECORD(damage_ok);

    before = x_errors;
    XGrabServer(comp);
    XResizeWindow(comp, window, old_w, old_h);
    XFlush(comp);
    uint32_t grab_named = (uint32_t)XAllocID(comp);
    name_window_pixmap(comp, cmp_op, (uint32_t)window, grab_named);
    painted = paint_window(comp, window, 0x11aa77);
    XFlush(comp);
    unsigned int grab_w = 0, grab_h = 0;
    unsigned long grab_pixel = 0;
    bool grab_geom = pixmap_size(comp, (Pixmap)grab_named, &grab_w, &grab_h);
    bool grab_read = read_pixel(comp, (Drawable)grab_named, &grab_pixel);
    XUngrabServer(comp);
    XSync(comp, False);
    bool grab_ok = painted && grab_geom && grab_read && x_errors == before
            && grab_w == old_w && grab_h == old_h
            && grab_pixel == 0x11aa77;
    result("named-resize-under-server", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or dropped under grab");
    RECORD(grab_ok);

    damage_destroy(comp, dmg_op, damage);
    XSync(comp, False);

    printf("BXSUMMARY named-pixmap-resize-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(app, window);
    XCloseDisplay(app);
    XCloseDisplay(comp);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
