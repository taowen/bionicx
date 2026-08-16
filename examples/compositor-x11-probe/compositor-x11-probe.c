#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xmd.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrender.h>
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
    uint8_t xfixesReqType;
    uint16_t length;
    uint32_t window;
    uint8_t shapeKind;
    uint8_t pad[3];
    int16_t xOff;
    int16_t yOff;
    uint32_t region;
} __attribute__((packed)) xfixes_shape_req;

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

static void release_overlay_window(Display *display, int opcode,
                                   uint32_t window) {
    LockDisplay(display);
    cmp_overlay_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->compositeReqType = 8;
    req->length = 2;
    req->window = window;
    UnlockDisplay(display);
    _XFlush(display);
}

static void set_window_shape_region(Display *display, int opcode,
                                    uint32_t window, uint8_t kind) {
    LockDisplay(display);
    xfixes_shape_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->xfixesReqType = 21;
    req->length = (uint16_t)(sizeof(*req) / 4);
    req->window = window;
    req->shapeKind = kind;
    req->pad[0] = req->pad[1] = req->pad[2] = 0;
    req->xOff = 0;
    req->yOff = 0;
    req->region = 0;
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

static bool blit_pixmap_to_overlay(Display *display, Pixmap pixmap,
                                   Window overlay) {
    XImage *got = XGetImage(display, pixmap, 0, 0, 8, 8, AllPlanes, ZPixmap);
    if (got == NULL) return false;
    GC gc = XCreateGC(display, overlay, 0, NULL);
    if (gc == None) {
        XDestroyImage(got);
        return false;
    }
    XPutImage(display, overlay, gc, got, 0, 0, 0, 0, 8, 8);
    XFreeGC(display, gc);
    XDestroyImage(got);
    return true;
}

typedef struct {
    uint8_t reqType;
    uint8_t shapeReqType;
    uint16_t length;
    uint32_t window;
    uint8_t enable;
    uint8_t pad[3];
} shape_select_req;

static void shape_select_input(Display *display, int opcode, uint32_t window) {
    LockDisplay(display);
    shape_select_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->shapeReqType = 6;
    req->length = (uint16_t)(sizeof(*req) / 4);
    req->window = window;
    req->enable = 1;
    req->pad[0] = req->pad[1] = req->pad[2] = 0;
    UnlockDisplay(display);
    _XFlush(display);
}

static bool put_a8_image(Display *display, Pixmap pixmap, unsigned width,
                         unsigned height, int value) {
    char *pixels = calloc(width * height, 1u);
    if (pixels == NULL) return false;
    memset(pixels, value, width * height);
    XImage *image = XCreateImage(display, DefaultVisual(display,
            DefaultScreen(display)), 8, ZPixmap, 0, pixels, width, height,
            8, (int)width);
    GC gc = XCreateGC(display, pixmap, 0, NULL);
    if (image == NULL || gc == None) {
        if (image != NULL) XDestroyImage(image);
        else free(pixels);
        if (gc != None) XFreeGC(display, gc);
        return false;
    }
    XPutImage(display, pixmap, gc, image, 0, 0, 0, 0, width, height);
    XDestroyImage(image);
    XFreeGC(display, gc);
    return true;
}

static bool paint_burst(Display *display, Drawable dest, Window shaped,
                        int shape_op) {
    int render_event = 0, render_error = 0;
    int major = 0, minor = 0;
    if (!XRenderQueryExtension(display, &render_event, &render_error)
            || !XRenderQueryVersion(display, &major, &minor))
        return false;
    XRenderPictFormat *argb = XRenderFindStandardFormat(display,
            PictStandardARGB32);
    XRenderPictFormat *a8 = XRenderFindStandardFormat(display, PictStandardA8);
    if (argb == NULL || a8 == NULL) return false;

    Pixmap back = XCreatePixmap(display, dest, 80, 80, 32);
    Pixmap solid_pm = XCreatePixmap(display, dest, 1, 1, 32);
    Pixmap mask_pm = XCreatePixmap(display, dest, 16, 16, 8);
    Pixmap shadow_pm = XCreatePixmap(display, dest, 80, 80, 8);
    if (back == None || solid_pm == None || mask_pm == None || shadow_pm == None)
        return false;

    XRenderPictureAttributes subwindow = {.subwindow_mode = IncludeInferiors};
    XRenderPictureAttributes repeat = {.repeat = RepeatNormal};
    Picture dest_pic = XRenderCreatePicture(display, back, argb,
                                            CPSubwindowMode, &subwindow);
    Picture solid = XRenderCreatePicture(display, solid_pm, argb, CPRepeat,
                                         &repeat);
    Picture mask = XRenderCreatePicture(display, mask_pm, a8, 0, NULL);
    if (dest_pic == None || solid == None || mask == None) return false;

    XRenderColor black = {.alpha = 0xffff};
    XRenderColor cover = {.red = 0xffff, .alpha = 0xffff};
    XRenderColor half = {.alpha = 0x8000};
    XGrabServer(display);
    XRenderFillRectangle(display, PictOpSrc, solid, &black, 0, 0, 1, 1);
    XRenderFillRectangle(display, PictOpSrc, dest_pic, &cover, 0, 0, 80, 80);
    XRenderFillRectangle(display, PictOpSrc, mask, &half, 0, 0, 16, 16);
    XRectangle clip = {.width = 16, .height = 16};
    XserverRegion region = XFixesCreateRegion(display, &clip, 1);
    if (region == None) {
        XUngrabServer(display);
        return false;
    }
    XFixesSetPictureClipRegion(display, dest_pic, 0, 0, region);
    if (shape_op != 0) shape_select_input(display, shape_op, (uint32_t)shaped);
    XRenderComposite(display, PictOpOver, solid, mask, dest_pic,
                     0, 0, 0, 0, 0, 0, 16, 16);
    XUngrabServer(display);
    bool uploaded = put_a8_image(display, shadow_pm, 80, 80, 0x80);
    Picture shadow = uploaded
            ? XRenderCreatePicture(display, shadow_pm, a8, 0, NULL) : None;
    if (shadow != None)
        XRenderComposite(display, PictOpOver, solid, shadow, dest_pic,
                         0, 0, 0, 0, 16, 0, 80, 80);
    Window focus = 0;
    int revert = 0;
    XGetInputFocus(display, &focus, &revert);
    XSync(display, False);
    unsigned long pixel = 0;
    bool read_ok = read_pixel(display, back, &pixel);

    XFixesDestroyRegion(display, region);
    if (shadow != None) XRenderFreePicture(display, shadow);
    XRenderFreePicture(display, mask);
    XRenderFreePicture(display, solid);
    XRenderFreePicture(display, dest_pic);
    XFreePixmap(display, shadow_pm);
    XFreePixmap(display, mask_pm);
    XFreePixmap(display, solid_pm);
    XFreePixmap(display, back);
    return uploaded && shadow != None && read_ok;
}

typedef struct {
    uint8_t reqType;
    uint8_t glxCode;
    uint16_t length;
    uint32_t major;
    uint32_t minor;
    uint32_t numbytes;
} glx_client_info_req;

typedef struct {
    uint8_t reqType;
    uint8_t glxCode;
    uint16_t length;
    uint32_t numVersions;
    uint32_t numGLExts;
    uint32_t numGLXExts;
    uint32_t verMajor;
    uint32_t verMinor;
} glx_set_client_info_arb_req;

typedef struct {
    uint8_t reqType;
    uint8_t glxCode;
    uint16_t length;
    uint32_t numVersions;
    uint32_t numGLExts;
    uint32_t numGLXExts;
    uint32_t verMajor;
    uint32_t verMinor;
    uint32_t profile;
} glx_set_client_info2_arb_req;

static void glx_client_info(Display *display, int opcode) {
    LockDisplay(display);
    glx_client_info_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->glxCode = 20;
    req->length = (uint16_t)(sizeof(*req) / 4);
    req->major = 1;
    req->minor = 4;
    req->numbytes = 0;
    UnlockDisplay(display);
    _XFlush(display);
}

static void glx_set_client_info_arb(Display *display, int opcode) {
    LockDisplay(display);
    glx_set_client_info_arb_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->glxCode = 33;
    req->length = (uint16_t)(sizeof(*req) / 4);
    req->numVersions = 1;
    req->numGLExts = 0;
    req->numGLXExts = 0;
    req->verMajor = 1;
    req->verMinor = 4;
    UnlockDisplay(display);
    _XFlush(display);
}

static void glx_set_client_info2_arb(Display *display, int opcode) {
    LockDisplay(display);
    glx_set_client_info2_arb_req *req = reserve(display, sizeof(*req));
    req->reqType = (uint8_t)opcode;
    req->glxCode = 35;
    req->length = (uint16_t)(sizeof(*req) / 4);
    req->numVersions = 1;
    req->numGLExts = 0;
    req->numGLXExts = 0;
    req->verMajor = 1;
    req->verMinor = 4;
    req->profile = 0;
    UnlockDisplay(display);
    _XFlush(display);
}

static bool is_top_child(Display *display, Window root, Window target) {
    Window ret_root = 0;
    Window parent = 0;
    Window *children = NULL;
    unsigned int count = 0;
    if (!XQueryTree(display, root, &ret_root, &parent, &children, &count))
        return false;
    bool top = count > 0 && children[count - 1] == target;
    XFree(children);
    return top;
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
    int fixes_op = 0, fixes_ev = 0, fixes_err = 0;
    if (!XQueryExtension(comp, "Composite", &cmp_op, &cmp_ev, &cmp_err)
            || cmp_op == 0
            || !XQueryExtension(comp, "DAMAGE", &dmg_op, &dmg_ev, &dmg_err)
            || dmg_op == 0 || dmg_ev <= 0
            || !XQueryExtension(comp, "XFIXES", &fixes_op, &fixes_ev,
                                &fixes_err)
            || fixes_op == 0) {
        fprintf(stderr, "BXFAIL Composite, DAMAGE or XFIXES unavailable\n");
        XCloseDisplay(comp);
        XCloseDisplay(app);
        return 2;
    }
    XESetWireToEvent(comp, dmg_ev, damage_wire_to_event);

    Window root = DefaultRootWindow(app);
    int passed = 0;
    int failed = 0;
#define RECORD(ok) do { if (ok) ++passed; else ++failed; } while (0)

    int before = x_errors;
    int cmp_major = 0, cmp_minor = 0, dmg_major = 0, dmg_minor = 0;
    bool versions_ok = cmp_query(comp, cmp_op, &cmp_major, &cmp_minor)
            && (cmp_major > 0 || cmp_minor >= 3)
            && dmg_query(comp, dmg_op, &dmg_major, &dmg_minor)
            && dmg_major >= 1 && x_errors == before;
    char version_detail[80];
    if (versions_ok)
        snprintf(version_detail, sizeof(version_detail),
                 "Composite %d.%d DAMAGE %d.%d",
                 cmp_major, cmp_minor, dmg_major, dmg_minor);
    else
        snprintf(version_detail, sizeof(version_detail), "QueryVersion failed");
    result("compositor-versions", versions_ok, version_detail);
    RECORD(versions_ok);

    before = x_errors;
    int glx_op = 0, glx_ev = 0, glx_err = 0;
    bool glx_ok = XQueryExtension(comp, "GLX", &glx_op, &glx_ev, &glx_err)
            && glx_op != 0;
    if (glx_ok) {
        glx_client_info(comp, glx_op);
        glx_set_client_info_arb(comp, glx_op);
        glx_set_client_info2_arb(comp, glx_op);
        XSync(comp, False);
        glx_ok = x_errors == before;
    }
    result("compositor-glx-client-info", glx_ok,
           glx_ok ? "ClientInfo/SetClientInfoARB/2ARB"
                  : "GLX client-info rejected");
    RECORD(glx_ok);

    before = x_errors;
    Window overlay = (Window)get_overlay_window(comp, cmp_op, (uint32_t)root);
    XWindowAttributes overlay_attrs;
    bool unmapped_ok = overlay != 0
            && XGetWindowAttributes(comp, overlay, &overlay_attrs) != 0
            && overlay_attrs.map_state == IsUnmapped
            && overlay_attrs.override_redirect
            && x_errors == before;
    result("overlay-initially-unmapped", unmapped_ok,
           unmapped_ok ? "GetOverlayWindow leaves overlay unmapped"
                       : "overlay mapped by GetOverlayWindow");
    RECORD(unmapped_ok);

    before = x_errors;
    if (overlay != 0) XMapWindow(comp, overlay);
    redirect_subwindows(comp, cmp_op, (uint32_t)root, 1);
    XSync(comp, False);
    bool overlay_painted = overlay != 0 && paint_window(comp, overlay, 0x113355);
    XSync(comp, False);
    unsigned long overlay_bg = 0;
    bool overlay_ok = overlay_painted && x_errors == before
            && read_pixel(comp, overlay, &overlay_bg)
            && overlay_bg == 0x113355 && x_errors == before;
    result("overlay-after-root-redirect", overlay_ok,
           overlay_ok ? "overlay still paintable"
                      : "overlay lost after RedirectSubwindows(root)");
    RECORD(overlay_ok);

    before = x_errors;
    if (overlay != 0) {
        set_window_shape_region(comp, fixes_op, (uint32_t)overlay, 0);
        set_window_shape_region(comp, fixes_op, (uint32_t)overlay, 1);
        set_window_shape_region(comp, fixes_op, (uint32_t)overlay, 2);
    }
    XSync(comp, False);
    bool shape_ok = overlay != 0 && x_errors == before;
    result("overlay-shape-regions", shape_ok,
           shape_ok ? "Bounding/Clip/Input None"
                    : "SetWindowShapeRegion failed");
    RECORD(shape_ok);

    before = x_errors;
    Window toplevel = XCreateSimpleWindow(app, root, 80, 80, 160, 100, 0,
                                          0, 0x336699);
    XMapWindow(app, toplevel);
    XSync(app, False);
    XSync(comp, False);
    bool top_ok = is_top_child(comp, root, overlay) && x_errors == before;
    result("overlay-on-top", top_ok,
           top_ok ? "overlay above mapped toplevel"
                  : "overlay not top after MapWindow");
    RECORD(top_ok);

    before = x_errors;
    XLowerWindow(comp, overlay);
    XSync(comp, False);
    bool restack_ok = is_top_child(comp, root, overlay) && x_errors == before;
    result("overlay-restack-stays-top", restack_ok,
           restack_ok ? "LowerWindow could not bury overlay"
                      : "overlay stayed below after LowerWindow");
    RECORD(restack_ok);

    before = x_errors;
    uint32_t named = (uint32_t)XAllocID(comp);
    name_window_pixmap(comp, cmp_op, (uint32_t)toplevel, named);
    uint32_t damage = (uint32_t)XAllocID(comp);
    damage_create(comp, dmg_op, damage, (uint32_t)toplevel, 3);
    XSync(comp, False);
    bool painted = paint_window(app, toplevel, 0x00cc44);
    XSync(app, False);
    XEvent event = {0};
    bool saw_damage = wait_damage(comp, dmg_ev, &event);
    unsigned long named_pixel = 0;
    bool blitted = blit_pixmap_to_overlay(comp, (Pixmap)named, overlay);
    XSync(comp, False);
    unsigned long overlay_pixel = 0;
    bool path_ok = painted && saw_damage && x_errors == before
            && read_pixel(comp, (Drawable)named, &named_pixel)
            && named_pixel == 0x00cc44
            && blitted && read_pixel(comp, overlay, &overlay_pixel)
            && overlay_pixel == 0x00cc44 && x_errors == before;
    result("root-toplevel-path", path_ok,
           path_ok ? "DamageNotify named pixmap to overlay"
                   : "root redirect path failed");
    RECORD(path_ok);

    before = x_errors;
    uint32_t overlay_pixmap = (uint32_t)XAllocID(comp);
    name_window_pixmap(comp, cmp_op, (uint32_t)overlay, overlay_pixmap);
    XSync(comp, False);
    bool overlay_plain = x_errors > before;
    if (overlay_plain) x_errors = before;
    result("overlay-not-redirected", overlay_plain,
           overlay_plain ? "NameWindowPixmap overlay BadMatch"
                         : "overlay treated as redirected");
    RECORD(overlay_plain);

    before = x_errors;
    XGrabServer(comp);
    damage_subtract(comp, dmg_op, damage);
    painted = paint_window(comp, toplevel, 0xee8822);
    XFlush(comp);
    saw_damage = wait_damage(comp, dmg_ev, &event);
    unsigned long grab_pixel = 0;
    bool grab_read = read_pixel(comp, (Drawable)named, &grab_pixel);
    bool grab_blit = blit_pixmap_to_overlay(comp, (Pixmap)named, overlay);
    unsigned long grab_overlay = 0;
    bool grab_overlay_ok = read_pixel(comp, overlay, &grab_overlay);
    XUngrabServer(comp);
    XSync(comp, False);
    bool grab_ok = painted && saw_damage && grab_read && grab_blit
            && grab_overlay_ok && grab_pixel == 0xee8822
            && grab_overlay == 0xee8822 && x_errors == before;
    result("root-path-under-server", grab_ok,
           grab_ok ? "under GrabServer" : "blocked or dropped under grab");
    RECORD(grab_ok);

    before = x_errors;
    Window output = 0;
    if (overlay != 0) {
        output = XCreateSimpleWindow(comp, overlay, 0, 0, 200, 120,
                                     0, 0, 0x224466);
        XMapWindow(comp, output);
    }
    XSync(comp, False);
    bool child_painted = output != 0 && paint_window(comp, output, 0xaabbcc);
    XSync(comp, False);
    unsigned long child_pixel = 0;
    bool child_ok = child_painted && x_errors == before
            && read_pixel(comp, output, &child_pixel)
            && child_pixel == 0xaabbcc && x_errors == before;
    result("overlay-child-output", child_ok,
           child_ok ? "overlay child output paintable"
                    : "overlay child lost after RedirectSubwindows(root)");
    RECORD(child_ok);

    before = x_errors;
    int shape_op = 0, shape_ev = 0, shape_err = 0;
    bool have_shape = XQueryExtension(comp, "SHAPE", &shape_op, &shape_ev,
                                      &shape_err) && shape_op != 0;
    Drawable burst_dest = output != 0 ? output : overlay;
    bool burst_ok = burst_dest != 0
            && paint_burst(comp, burst_dest, toplevel, have_shape ? shape_op : 0)
            && x_errors == before;
    result("compositor-paint-burst", burst_ok,
           burst_ok ? "A8/repeat/clip/Composite after UngrabServer"
                    : "paint burst desynced or rejected");
    RECORD(burst_ok);

    damage_destroy(comp, dmg_op, damage);
    if (output != 0) XDestroyWindow(comp, output);
    release_overlay_window(comp, cmp_op, (uint32_t)root);
    unredirect_subwindows(comp, cmp_op, (uint32_t)root, 1);
    XSync(comp, False);

    printf("BXSUMMARY compositor-x11 passed=%d failed=%d xerrors=%d\n",
           passed, failed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 3));
    XDestroyWindow(app, toplevel);
    XCloseDisplay(app);
    XCloseDisplay(comp);
    return failed == 0 && x_errors == 0 ? 0 : 1;
}
