#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int checks;
static int passed;
static int x_errors;

static int handle_x_error(Display *display, XErrorEvent *event) {
    char text[128];
    XGetErrorText(display, event->error_code, text, sizeof(text));
    fprintf(stderr, "BXERROR code=%u request=%u minor=%u resource=0x%lx %s\n",
            event->error_code, event->request_code, event->minor_code,
            event->resourceid, text);
    x_errors++;
    return 0;
}

static void result(const char *name, bool ok, const char *detail) {
    checks++;
    if (ok) passed++;
    printf("BXTEST %s %-12s %s\n", ok ? "PASS" : "FAIL", name, detail);
    fflush(stdout);
}

static bool sync_without_error(Display *display, int before) {
    XSync(display, False);
    return x_errors == before;
}

static void probe_render(Display *display, Window window) {
    int event_base = 0, error_base = 0, major = 0, minor = 0;
    if (!XRenderQueryExtension(display, &event_base, &error_base)) {
        result("xrender", false, "extension-missing");
        return;
    }
    int before = x_errors;
    unsigned long in_add_pixel = 0;
    unsigned long out_reverse_pixel = 0;
    unsigned long saturate_pixel = 0;
    unsigned long create_repeat_pixel = 0;
    unsigned long pixmap_clip_inside = 0;
    unsigned long pixmap_clip_outside = 0;
    unsigned long title_pixel = 0;
    bool ok = XRenderQueryVersion(display, &major, &minor) != 0;
    XRenderPictFormat *format = XRenderFindVisualFormat(
        display, DefaultVisual(display, DefaultScreen(display)));
    XRenderPictFormat *a8 = XRenderFindStandardFormat(display, PictStandardA8);
    Picture picture = format ? XRenderCreatePicture(display, window, format, 0, NULL) : 0;
    Pixmap a8_pixmap = a8 ? XCreatePixmap(display, window, 8, 8, 8) : 0;
    Picture a8_picture = a8_pixmap
            ? XRenderCreatePicture(display, a8_pixmap, a8, 0, NULL) : 0;
    if (picture) {
        XRenderColor background = {.red = 0, .green = 0,
                                   .blue = 0xffff, .alpha = 0xffff};
        XRenderColor overlay = {.red = 0xffff, .green = 0,
                                .blue = 0, .alpha = 0x8000};
        XRenderFillRectangle(display, PictOpSrc, picture, &background,
                             24, 84, 300, 92);
        XRenderFillRectangle(display, PictOpOver, picture, &overlay,
                             24, 84, 300, 92);
        XRenderFillRectangle(display, PictOpSrc, picture, &background,
                             330, 84, 24, 24);
        XRenderFillRectangle(display, PictOpClear, picture, &overlay,
                             330, 84, 24, 24);
        XRenderFillRectangle(display, PictOpSrc, picture, &background,
                             420, 84, 8, 8);
        XRectangle clip = {.x = 420, .y = 84, .width = 4, .height = 8};
        XRenderSetPictureClipRectangles(display, picture, 0, 0, &clip, 1);
        XRenderColor opaque_red = {.red = 0xffff, .alpha = 0xffff};
        XRenderFillRectangle(display, PictOpSrc, picture, &opaque_red,
                             420, 84, 8, 8);
        XRenderPictureAttributes attributes = {.repeat = True,
                                                .clip_mask = None};
        XRenderChangePicture(display, picture, CPRepeat | CPClipMask,
                             &attributes);
        if (a8_picture) {
            XRenderColor half_mask = {.alpha = 0x8000};
            XRenderFillRectangle(display, PictOpSrc, a8_picture,
                                 &half_mask, 0, 0, 8, 8);
            XRenderFillRectangle(display, PictOpIn, a8_picture,
                                 &half_mask, 0, 0, 8, 8);
            XRenderColor quarter_mask = {.alpha = 0x4000};
            Picture quarter = XRenderCreateSolidFill(display, &quarter_mask);
            XRenderComposite(display, PictOpAdd, quarter, None, a8_picture,
                             0, 0, 0, 0, 0, 0, 8, 8);
            XImage *in_add_image = XGetImage(display, a8_pixmap, 2, 2, 1, 1,
                                             AllPlanes, ZPixmap);
            in_add_pixel = in_add_image ? XGetPixel(in_add_image, 0, 0) : 0;
            if (in_add_image) XDestroyImage(in_add_image);
            XRenderComposite(display, PictOpOutReverse, quarter, None,
                             a8_picture, 0, 0, 0, 0, 0, 0, 8, 8);
            XImage *out_reverse_image = XGetImage(
                    display, a8_pixmap, 2, 2, 1, 1, AllPlanes, ZPixmap);
            out_reverse_pixel = out_reverse_image
                    ? XGetPixel(out_reverse_image, 0, 0) : 0;
            if (out_reverse_image) XDestroyImage(out_reverse_image);
            XRenderFreePicture(display, quarter);
            XRenderColor three_quarter_mask = {.alpha = 0xc000};
            XRenderFillRectangle(display, PictOpSrc, a8_picture,
                                 &three_quarter_mask, 0, 0, 8, 8);
            Picture half = XRenderCreateSolidFill(display, &half_mask);
            XRenderComposite(display, PictOpSaturate, half, None, a8_picture,
                             0, 0, 0, 0, 0, 0, 8, 8);
            XImage *saturate_image = XGetImage(display, a8_pixmap, 2, 2,
                                               1, 1, AllPlanes, ZPixmap);
            saturate_pixel = saturate_image
                    ? XGetPixel(saturate_image, 0, 0) : 0;
            if (saturate_image) XDestroyImage(saturate_image);
            XRenderFreePicture(display, half);
            XRenderFillRectangle(display, PictOpSrc, a8_picture,
                                 &half_mask, 0, 0, 8, 8);
            XRenderSetPictureFilter(display, a8_picture, FilterNearest,
                                    NULL, 0);
            Picture solid = XRenderCreateSolidFill(display, &overlay);
            XRenderFillRectangle(display, PictOpSrc, picture, &background,
                                 380, 84, 8, 8);
            XRenderComposite(display, PictOpOver, solid, a8_picture, picture,
                             0, 0, 0, 0, 380, 84, 8, 8);
            XRenderFreePicture(display, solid);
        }
        XLinearGradient linear = {
            .p1 = {.x = XDoubleToFixed(460.0), .y = XDoubleToFixed(84.0)},
            .p2 = {.x = XDoubleToFixed(468.0), .y = XDoubleToFixed(84.0)}
        };
        XFixed stops[] = {XDoubleToFixed(0.0), XDoubleToFixed(1.0)};
        XRenderColor gradient_colors[] = {
            {.red = 0xffff, .alpha = 0xffff},
            {.blue = 0xffff, .alpha = 0xffff}
        };
        Picture gradient = XRenderCreateLinearGradient(
                display, &linear, stops, gradient_colors, 2);
        XRenderComposite(display, PictOpOver, gradient, None, picture,
                         460, 84, 0, 0, 460, 84, 8, 8);
        XRenderComposite(display, PictOpSrc, gradient, None, picture,
                         460, 84, 0, 0, 490, 84, 8, 8);
        XRenderFreePicture(display, gradient);
        Pixmap repeat_pixmap = XCreatePixmap(
                display, window, 1, 1,
                DefaultDepth(display, DefaultScreen(display)));
        XRenderPictureAttributes repeat_attributes = {.repeat = RepeatNormal};
        Picture repeat_picture = XRenderCreatePicture(
                display, repeat_pixmap, format, CPRepeat, &repeat_attributes);
        XRenderFillRectangle(display, PictOpSrc, repeat_picture, &opaque_red,
                             0, 0, 1, 1);
        XRenderComposite(display, PictOpSrc, repeat_picture, None, picture,
                         0, 0, 0, 0, 520, 84, 8, 8);
        XImage *repeat_image = XGetImage(display, window, 526, 86, 1, 1,
                                         AllPlanes, ZPixmap);
        create_repeat_pixel = repeat_image
                ? XGetPixel(repeat_image, 0, 0) : 0;
        if (repeat_image) XDestroyImage(repeat_image);
        XRenderFreePicture(display, repeat_picture);
        XFreePixmap(display, repeat_pixmap);

        Pixmap clip_pixmap = XCreatePixmap(display, window, 8, 8, 1);
        GC clip_gc = XCreateGC(display, clip_pixmap, 0, NULL);
        XSetForeground(display, clip_gc, 0);
        XFillRectangle(display, clip_pixmap, clip_gc, 0, 0, 8, 8);
        XSetForeground(display, clip_gc, 1);
        XFillRectangle(display, clip_pixmap, clip_gc, 0, 0, 4, 8);
        XRenderFillRectangle(display, PictOpSrc, picture, &background,
                             550, 84, 8, 8);
        XRenderPictureAttributes clip_attributes = {
            .clip_x_origin = 550,
            .clip_y_origin = 84,
            .clip_mask = clip_pixmap,
        };
        Picture pixmap_clip_picture = XRenderCreatePicture(display, window,
                format, CPClipXOrigin | CPClipYOrigin | CPClipMask,
                &clip_attributes);
        XRenderFillRectangle(display, PictOpSrc, pixmap_clip_picture,
                             &opaque_red, 550, 84, 8, 8);
        XImage *pixmap_clip_inside_image = XGetImage(display, window,
                551, 86, 1, 1, AllPlanes, ZPixmap);
        XImage *pixmap_clip_outside_image = XGetImage(display, window,
                556, 86, 1, 1, AllPlanes, ZPixmap);
        pixmap_clip_inside = pixmap_clip_inside_image
                ? XGetPixel(pixmap_clip_inside_image, 0, 0) : 0;
        pixmap_clip_outside = pixmap_clip_outside_image
                ? XGetPixel(pixmap_clip_outside_image, 0, 0) : 0;
        if (pixmap_clip_inside_image) XDestroyImage(pixmap_clip_inside_image);
        if (pixmap_clip_outside_image) XDestroyImage(pixmap_clip_outside_image);
        XRenderFreePicture(display, pixmap_clip_picture);
        XFreeGC(display, clip_gc);
        XFreePixmap(display, clip_pixmap);

        /* xfwm4 Default title tiles are near-transparent white Over a
         * core-drawn 24-in-32 fill. Unused dest alpha must stay opaque
         * or the compositor output is a black title bar. */
        XRenderPictFormat *argb32 = XRenderFindStandardFormat(
                display, PictStandardARGB32);
        Pixmap frame = argb32
                ? XCreatePixmap(display, window, 8, 8, 32) : None;
        Pixmap composed = argb32
                ? XCreatePixmap(display, window, 8, 8, 32) : None;
        GC frame_gc = frame != None ? XCreateGC(display, frame, 0, NULL) : None;
        Picture frame_pic = None;
        Picture composed_pic = None;
        if (frame != None && composed != None && frame_gc != None) {
            XSetForeground(display, frame_gc, 0xc0c0c0);
            XFillRectangle(display, frame, frame_gc, 0, 0, 8, 8);
            frame_pic = XRenderCreatePicture(display, frame, argb32, 0, NULL);
            composed_pic = XRenderCreatePicture(display, composed, argb32,
                                                0, NULL);
            if (frame_pic != None && composed_pic != None) {
                XRenderColor haze = {
                    .red = 0xffff, .green = 0xffff, .blue = 0xffff,
                    .alpha = 0x0404
                };
                XRenderColor black = {.alpha = 0xffff};
                XRenderFillRectangle(display, PictOpOver, frame_pic, &haze,
                                     0, 0, 8, 8);
                XRenderFillRectangle(display, PictOpSrc, composed_pic, &black,
                                     0, 0, 8, 8);
                XRenderComposite(display, PictOpOver, frame_pic, None,
                                 composed_pic, 0, 0, 0, 0, 0, 0, 8, 8);
                XImage *title_image = XGetImage(display, composed, 2, 2, 1, 1,
                                                AllPlanes, ZPixmap);
                title_pixel = title_image ? XGetPixel(title_image, 0, 0) : 0;
                if (title_image) XDestroyImage(title_image);
            }
        }
        if (composed_pic != None) XRenderFreePicture(display, composed_pic);
        if (frame_pic != None) XRenderFreePicture(display, frame_pic);
        if (frame_gc != None) XFreeGC(display, frame_gc);
        if (composed != None) XFreePixmap(display, composed);
        if (frame != None) XFreePixmap(display, frame);
        XRenderFreePicture(display, picture);
    }
    XImage *a8_image = a8_pixmap
            ? XGetImage(display, a8_pixmap, 2, 2, 1, 1, AllPlanes, ZPixmap)
            : NULL;
    unsigned long a8_pixel = a8_image ? XGetPixel(a8_image, 0, 0) : 0;
    if (a8_picture) XRenderFreePicture(display, a8_picture);
    if (a8_pixmap) XFreePixmap(display, a8_pixmap);
    ok = ok && format && a8 && a8->depth == 8 && a8->direct.alphaMask == 0xff
         && a8_image && a8_pixel >= 0x7f && a8_pixel <= 0x81
         && in_add_pixel >= 0x7f && in_add_pixel <= 0x81
         && out_reverse_pixel >= 0x5f && out_reverse_pixel <= 0x61
         && saturate_pixel >= 0xfe && saturate_pixel <= 0xff
         && (create_repeat_pixel & 0xffffff) == 0xff0000
         && picture && a8_picture && sync_without_error(display, before);
    if (a8_image) XDestroyImage(a8_image);
    XImage *image = ok ? XGetImage(display, window, 100, 120, 1, 1,
                                   AllPlanes, ZPixmap) : NULL;
    unsigned long pixel = image ? XGetPixel(image, 0, 0) : 0;
    XImage *clear_image = ok ? XGetImage(display, window, 334, 88, 1, 1,
                                         AllPlanes, ZPixmap) : NULL;
    unsigned long clear_pixel = clear_image
            ? XGetPixel(clear_image, 0, 0) : ~0UL;
    XImage *mask_image = ok ? XGetImage(display, window, 382, 86, 1, 1,
                                        AllPlanes, ZPixmap) : NULL;
    unsigned long mask_pixel = mask_image
            ? XGetPixel(mask_image, 0, 0) : 0;
    XImage *clip_inside_image = ok
            ? XGetImage(display, window, 421, 86, 1, 1, AllPlanes, ZPixmap)
            : NULL;
    XImage *clip_outside_image = ok
            ? XGetImage(display, window, 426, 86, 1, 1, AllPlanes, ZPixmap)
            : NULL;
    unsigned long clip_inside = clip_inside_image
            ? XGetPixel(clip_inside_image, 0, 0) : 0;
    unsigned long clip_outside = clip_outside_image
            ? XGetPixel(clip_outside_image, 0, 0) : 0;
    XImage *gradient_left_image = ok
            ? XGetImage(display, window, 460, 86, 1, 1, AllPlanes, ZPixmap)
            : NULL;
    XImage *gradient_right_image = ok
            ? XGetImage(display, window, 467, 86, 1, 1, AllPlanes, ZPixmap)
            : NULL;
    unsigned long gradient_left = gradient_left_image
            ? XGetPixel(gradient_left_image, 0, 0) : 0;
    unsigned long gradient_right = gradient_right_image
            ? XGetPixel(gradient_right_image, 0, 0) : 0;
    XImage *source_image = ok
            ? XGetImage(display, window, 490, 86, 1, 1, AllPlanes, ZPixmap)
            : NULL;
    unsigned long source_pixel = source_image
            ? XGetPixel(source_image, 0, 0) : 0;
    int red = (pixel >> 16) & 0xff;
    int green = (pixel >> 8) & 0xff;
    int blue = pixel & 0xff;
    ok = ok && image && red >= 0x7f && red <= 0x81 && green == 0
         && blue >= 0x7e && blue <= 0x80;
    if (image) XDestroyImage(image);
    ok = ok && clear_image && (clear_pixel & 0xffffff) == 0;
    if (clear_image) XDestroyImage(clear_image);
    int mask_red = (mask_pixel >> 16) & 0xff;
    int mask_blue = mask_pixel & 0xff;
    ok = ok && mask_image && mask_red >= 0x3f && mask_red <= 0x41
         && mask_blue >= 0xbe && mask_blue <= 0xc0;
    if (mask_image) XDestroyImage(mask_image);
    ok = ok && clip_inside_image && clip_outside_image
         && (clip_inside & 0xffffff) == 0xff0000
         && (clip_outside & 0xffffff) == 0x0000ff;
    if (clip_inside_image) XDestroyImage(clip_inside_image);
    if (clip_outside_image) XDestroyImage(clip_outside_image);
    ok = ok && gradient_left_image && gradient_right_image
         && ((gradient_left >> 16) & 0xff) > (gradient_left & 0xff)
         && (gradient_right & 0xff) > ((gradient_right >> 16) & 0xff);
    if (gradient_left_image) XDestroyImage(gradient_left_image);
    if (gradient_right_image) XDestroyImage(gradient_right_image);
    ok = ok && source_image && ((source_pixel >> 16) & 0xff)
         > (source_pixel & 0xff);
    if (source_image) XDestroyImage(source_image);
    ok = ok && (pixmap_clip_inside & 0xffffff) == 0xff0000
         && (pixmap_clip_outside & 0xffffff) == 0x0000ff;
    int title_red = (title_pixel >> 16) & 0xff;
    int title_green = (title_pixel >> 8) & 0xff;
    int title_blue = title_pixel & 0xff;
    ok = ok && title_red >= 0xb0 && title_green >= 0xb0 && title_blue >= 0xb0;
    char detail[384];
    snprintf(detail, sizeof(detail),
             "version=%d.%d event=%d error=%d a8=%d a8-picture=%d alpha-mask=0x%x in-add=0x%02lx out-reverse=0x%02lx saturate=0x%02lx create-repeat=0x%06lx over=0x%06lx clear=0x%lx mask-over=0x%06lx clip=0x%06lx/0x%06lx pixmap-clip=0x%06lx/0x%06lx gradient=0x%06lx/0x%06lx src=0x%06lx title=0x%06lx",
             major, minor, event_base, error_base, a8 != NULL,
             a8_picture != 0, a8 ? a8->direct.alphaMask : 0,
             in_add_pixel & 0xff, out_reverse_pixel & 0xff,
             saturate_pixel & 0xff,
             create_repeat_pixel & 0xffffff, pixel & 0xffffff, clear_pixel,
             mask_pixel & 0xffffff,
             clip_inside & 0xffffff, clip_outside & 0xffffff,
             pixmap_clip_inside & 0xffffff, pixmap_clip_outside & 0xffffff,
             gradient_left & 0xffffff, gradient_right & 0xffffff,
             source_pixel & 0xffffff, title_pixel & 0xffffff);
    result("xrender", ok, detail);
}

static void probe_window_picture_resize(Display *display) {
    int screen = DefaultScreen(display);
    Visual *visual = DefaultVisual(display, screen);
    XRenderPictFormat *format = XRenderFindVisualFormat(display, visual);
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
                                        8, 8, 1, 1, 0,
                                        BlackPixel(display, screen),
                                        BlackPixel(display, screen));
    XMapWindow(display, window);
    XSync(display, False);
    Picture picture = format
            ? XRenderCreatePicture(display, window, format, 0, NULL) : 0;
    XResizeWindow(display, window, 64, 32);
    XSync(display, False);
    XRenderColor white = {
        .red = 0xffff, .green = 0xffff, .blue = 0xffff, .alpha = 0xffff
    };
    if (picture)
        XRenderFillRectangle(display, PictOpSrc, picture, &white, 0, 0, 64, 32);
    XSync(display, False);
    XImage *image = XGetImage(display, window, 32, 16, 1, 1, AllPlanes, ZPixmap);
    unsigned long pixel = image ? XGetPixel(image, 0, 0) : 0;
    if (image) XDestroyImage(image);
    char detail[64];
    snprintf(detail, sizeof(detail), "pixel=0x%06lx", pixel & 0xffffff);
    result("window-resize", picture && (pixel & 0xffffff) == 0xffffff, detail);
    if (picture) XRenderFreePicture(display, picture);
    XDestroyWindow(display, window);
}

int main(int argc, char **argv) {
    int duration = argc > 1 ? atoi(argv[1]) : 2;
    XSetErrorHandler(handle_x_error);
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "BXTEST FAIL display-open DISPLAY=%s\n",
                getenv("DISPLAY") ? getenv("DISPLAY") : "(null)");
        return 2;
    }
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    Window window = XCreateSimpleWindow(display, root, 80, 80, 720, 420, 0,
                                        BlackPixel(display, screen),
                                        BlackPixel(display, screen));
    XMapWindow(display, window);
    XSync(display, False);
    probe_render(display, window);
    probe_window_picture_resize(display);
    printf("BXSUMMARY xrender-x11 passed=%d failed=%d xerrors=%d\n",
           passed, checks - passed, x_errors);
    fflush(stdout);
    sleep((unsigned)(duration > 0 && duration <= 30 ? duration : 2));
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return checks == passed && x_errors == 0 ? 0 : 1;
}
