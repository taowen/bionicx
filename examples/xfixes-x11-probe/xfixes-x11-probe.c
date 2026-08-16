#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrender.h>
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

static unsigned long pixel_at(Display *display, Window window, int x, int y) {
    XImage *image = XGetImage(display, window, x, y, 1, 1, AllPlanes, ZPixmap);
    unsigned long pixel = 0;
    if (image != NULL) {
        pixel = XGetPixel(image, 0, 0);
        XDestroyImage(image);
    }
    return pixel;
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
    XRectangle first = {0, 0, 100, 100};
    XRectangle second = {50, 50, 100, 100};
    XserverRegion region_a = XFixesCreateRegion(display, &first, 1);
    XserverRegion region_b = XFixesCreateRegion(display, &second, 1);
    XserverRegion region_out = XFixesCreateRegion(display, NULL, 0);
    XFixesIntersectRegion(display, region_out, region_a, region_b);
    XFixesUnionRegion(display, region_out, region_a, region_b);
    XFixesSubtractRegion(display, region_out, region_a, region_b);
    XFixesDestroyRegion(display, region_a);
    XFixesDestroyRegion(display, region_b);
    XFixesDestroyRegion(display, region_out);
    XSync(display, False);
    XSync(peer, False);
    bool region_ok = region_a != None && region_b != None && region_out != None
            && x_errors == before;
    result("xfixes-region-combine", region_ok,
           region_ok ? "Intersect/Union/Subtract" : "region combine failed");
    RECORD(region_ok);

    before = x_errors;
    XRectangle seed = {10, 20, 80, 40};
    XserverRegion region = XFixesCreateRegion(display, &seed, 1);
    XFixesTranslateRegion(display, region, 5, -5);
    int fetched = 0;
    XRectangle *rects = XFixesFetchRegion(display, region, &fetched);
    bool translate_ok = fetched == 1 && rects != NULL && rects[0].x == 15
            && rects[0].y == 15 && rects[0].width == 80 && rects[0].height == 40;
    if (rects != NULL) XFree(rects);
    XFixesSetRegion(display, region, &seed, 1);
    rects = XFixesFetchRegion(display, region, &fetched);
    bool set_ok = fetched == 1 && rects != NULL && rects[0].x == 10
            && rects[0].y == 20 && rects[0].width == 80 && rects[0].height == 40;
    if (rects != NULL) XFree(rects);
    XserverRegion extents = XFixesCreateRegion(display, NULL, 0);
    XFixesRegionExtents(display, extents, region);
    rects = XFixesFetchRegion(display, extents, &fetched);
    bool extents_ok = fetched == 1 && rects != NULL && rects[0].x == 10
            && rects[0].y == 20 && rects[0].width == 80 && rects[0].height == 40;
    if (rects != NULL) XFree(rects);
    XRectangle box = {0, 0, 100, 100};
    XserverRegion inverted = XFixesCreateRegion(display, NULL, 0);
    XFixesInvertRegion(display, inverted, &box, region);
    rects = XFixesFetchRegion(display, inverted, &fetched);
    bool invert_ok = fetched >= 1 && rects != NULL;
    if (rects != NULL) XFree(rects);
    XserverRegion from_window = XFixesCreateRegionFromWindow(display, window,
            WindowRegionBounding);
    rects = XFixesFetchRegion(display, from_window, &fetched);
    bool from_ok = from_window != None && fetched == 1 && rects != NULL
            && rects[0].width == 200 && rects[0].height == 120;
    if (rects != NULL) XFree(rects);
    XFixesDestroyRegion(display, region);
    XFixesDestroyRegion(display, extents);
    XFixesDestroyRegion(display, inverted);
    XFixesDestroyRegion(display, from_window);
    XSync(display, False);
    bool transform_ok = translate_ok && set_ok && extents_ok && invert_ok
            && from_ok && x_errors == before;
    result("xfixes-region-transform", transform_ok,
           transform_ok ? "Set/Translate/Invert/Extents/FromWindow"
                        : "region transform failed");
    RECORD(transform_ok);

    before = x_errors;
    XRectangle clip = {10, 10, 40, 40};
    XserverRegion clip_region = XFixesCreateRegion(display, &clip, 1);
    GC gc = XCreateGC(display, window, 0, NULL);
    XSetForeground(display, gc, 0x224466);
    XFixesSetGCClipRegion(display, gc, 0, 0, None);
    XFillRectangle(display, window, gc, 0, 0, 200, 120);
    XSync(display, False);
    unsigned long base = pixel_at(display, window, 0, 0);
    XSetForeground(display, gc, 0xffffff);
    XFixesSetGCClipRegion(display, gc, 0, 0, clip_region);
    XFillRectangle(display, window, gc, 0, 0, 200, 120);
    XSync(display, False);
    unsigned long gc_out = pixel_at(display, window, 0, 0);
    unsigned long gc_in = pixel_at(display, window, 10, 10);
    bool gc_clip = gc_out == base && gc_in != gc_out;
    XSetForeground(display, gc, 0x224466);
    XFixesSetGCClipRegion(display, gc, 0, 0, None);
    XFillRectangle(display, window, gc, 0, 0, 200, 120);
    XRenderPictFormat *format = XRenderFindVisualFormat(display,
            DefaultVisual(display, screen));
    bool picture_clip = false;
    unsigned long pic_out = 0;
    unsigned long pic_in = 0;
    if (format != NULL) {
        Picture picture = XRenderCreatePicture(display, window, format, 0, NULL);
        XRenderColor white = {0xffff, 0xffff, 0xffff, 0xffff};
        XFixesSetPictureClipRegion(display, picture, 0, 0, clip_region);
        XRenderFillRectangle(display, PictOpSrc, picture, &white, 0, 0, 200, 120);
        XSync(display, False);
        pic_out = pixel_at(display, window, 0, 0);
        pic_in = pixel_at(display, window, 10, 10);
        picture_clip = pic_out == base && pic_in != pic_out;
        XRenderFreePicture(display, picture);
    }
    XFixesDestroyRegion(display, clip_region);
    XFreeGC(display, gc);
    XSync(display, False);
    bool clip_ok = gc_clip && picture_clip && x_errors == before;
    char clip_detail[160];
    if (clip_ok) {
        snprintf(clip_detail, sizeof(clip_detail),
                 "SetGCClipRegion/SetPictureClipRegion");
    } else {
        snprintf(clip_detail, sizeof(clip_detail),
                 "base=%lx gc=%d/%lx/%lx pic=%d/%lx/%lx fmt=%d err=%d",
                 base, gc_clip, gc_out, gc_in, picture_clip, pic_out, pic_in,
                 format != NULL, x_errors - before);
    }
    result("xfixes-region-clip", clip_ok, clip_detail);
    RECORD(clip_ok);

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
