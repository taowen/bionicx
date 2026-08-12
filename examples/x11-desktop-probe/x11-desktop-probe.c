#define _GNU_SOURCE
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlib-xcb.h>
#include <X11/keysym.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xcb/xkb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int checks;
static int passed;
static int x_errors;
static int xkb_event_type = -1;

static int handle_x_error(Display *display, XErrorEvent *event) {
    char text[128];
    XGetErrorText(display, event->error_code, text, sizeof(text));
    fprintf(stderr, "BXXERROR code=%u request=%u minor=%u resource=0x%lx %s\n",
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

static bool wait_xfixes_selection(Display *display, int event_type, Atom selection,
                                  int subtype, Window owner) {
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += 2;
    while (true) {
        XEvent event = {0};
        while (XCheckTypedEvent(display, event_type, &event)) {
            XFixesSelectionNotifyEvent *notify =
                (XFixesSelectionNotifyEvent *)&event;
            if (notify->selection == selection && notify->subtype == subtype &&
                    notify->owner == owner) return true;
        }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > deadline.tv_sec ||
                (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec))
            return false;
        struct timespec pause = {.tv_nsec = 10 * 1000 * 1000};
        nanosleep(&pause, NULL);
    }
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
    unsigned long create_repeat_pixel = 0;
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
    char detail[256];
    snprintf(detail, sizeof(detail),
             "version=%d.%d event=%d error=%d a8=%d a8-picture=%d alpha-mask=0x%x in-add=0x%02lx out-reverse=0x%02lx create-repeat=0x%06lx over=0x%06lx clear=0x%lx mask-over=0x%06lx clip=0x%06lx/0x%06lx gradient=0x%06lx/0x%06lx src=0x%06lx",
             major, minor, event_base, error_base, a8 != NULL,
             a8_picture != 0, a8 ? a8->direct.alphaMask : 0,
             in_add_pixel & 0xff, out_reverse_pixel & 0xff,
             create_repeat_pixel & 0xffffff, pixel & 0xffffff, clear_pixel,
             mask_pixel & 0xffffff,
             clip_inside & 0xffffff, clip_outside & 0xffffff,
             gradient_left & 0xffffff, gradient_right & 0xffffff,
             source_pixel & 0xffffff);
    result("xrender", ok, detail);
}

static void probe_xfixes(Display *display, Window window) {
    int event_base = 0, error_base = 0, major = 0, minor = 0;
    if (!XFixesQueryExtension(display, &event_base, &error_base)) {
        result("xfixes", false, "extension-missing");
        return;
    }
    int before = x_errors;
    bool ok = XFixesQueryVersion(display, &major, &minor) != 0;
    Atom clipboard = XInternAtom(display, "CLIPBOARD", False);
    XFixesSelectSelectionInput(display, window, clipboard,
                              XFixesSetSelectionOwnerNotifyMask |
                              XFixesSelectionWindowDestroyNotifyMask |
                              XFixesSelectionClientCloseNotifyMask);
    Window first_owner = XCreateSimpleWindow(
        display, DefaultRootWindow(display), 0, 0, 1, 1, 0, 0, 0);
    XSetSelectionOwner(display, clipboard, first_owner, CurrentTime);
    XSync(display, False);
    bool set_notify = wait_xfixes_selection(
        display, event_base + XFixesSelectionNotify, clipboard,
        XFixesSetSelectionOwnerNotify, first_owner);
    XDestroyWindow(display, first_owner);
    XSync(display, False);
    bool destroy_notify = wait_xfixes_selection(
        display, event_base + XFixesSelectionNotify, clipboard,
        XFixesSelectionWindowDestroyNotify, first_owner);

    Display *owner_display = XOpenDisplay(NULL);
    Window second_owner = owner_display
        ? XCreateSimpleWindow(owner_display, DefaultRootWindow(owner_display),
                              0, 0, 1, 1, 0, 0, 0)
        : None;
    if (owner_display) {
        XSetSelectionOwner(owner_display, clipboard, second_owner, CurrentTime);
        XSync(owner_display, False);
    }
    bool second_set_notify = owner_display && wait_xfixes_selection(
        display, event_base + XFixesSelectionNotify, clipboard,
        XFixesSetSelectionOwnerNotify, second_owner);
    if (owner_display) XCloseDisplay(owner_display);
    bool close_notify = owner_display && wait_xfixes_selection(
        display, event_base + XFixesSelectionNotify, clipboard,
        XFixesSelectionClientCloseNotify, second_owner);
    XRectangle source = {.x = 7, .y = 9, .width = 31, .height = 37};
    XserverRegion region = XFixesCreateRegion(display, &source, 1);
    int count = 0;
    XRectangle *fetched = XFixesFetchRegion(display, region, &count);
    ok = ok && region && fetched && count == 1 && fetched[0].x == source.x
         && fetched[0].y == source.y && fetched[0].width == source.width
         && fetched[0].height == source.height;
    XRectangle input = {.x = 0, .y = 0, .width = 360, .height = 420};
    XserverRegion input_region = XFixesCreateRegion(display, &input, 1);
    if (input_region)
        XFixesSetWindowShapeRegion(display, window, ShapeInput, 0, 0, input_region);
    if (fetched) XFree(fetched);
    if (region) XFixesDestroyRegion(display, region);
    if (input_region) XFixesDestroyRegion(display, input_region);
    ok = ok && input_region && sync_without_error(display, before);
    ok = ok && set_notify && destroy_notify && second_set_notify && close_notify;
    char detail[176];
    snprintf(detail, sizeof(detail),
             "version=%d.%d rectangles=%d mask=7 set=%d destroy=%d close=%d",
             major, minor, count, set_notify && second_set_notify,
             destroy_notify, close_notify);
    result("xfixes", ok, detail);
}

static void probe_randr(Display *display, Window root) {
    int event_base = 0, error_base = 0, major = 0, minor = 0;
    if (!XRRQueryExtension(display, &event_base, &error_base)) {
        result("randr", false, "extension-missing");
        return;
    }
    int before = x_errors;
    bool ok = XRRQueryVersion(display, &major, &minor) != 0;
    XRRSelectInput(display, root, RRScreenChangeNotifyMask |
                   RRCrtcChangeNotifyMask | RROutputChangeNotifyMask |
                   RROutputPropertyNotifyMask);
    XRRScreenResources *resources = XRRGetScreenResourcesCurrent(display, root);
    RROutput primary = XRRGetOutputPrimary(display, root);
    XRROutputInfo *output_info = resources && resources->noutput > 0
        ? XRRGetOutputInfo(display, resources, resources->outputs[0]) : NULL;
    XRRCrtcInfo *crtc_info = resources && resources->ncrtc > 0
        ? XRRGetCrtcInfo(display, resources, resources->crtcs[0]) : NULL;
    Atom missing_property = XInternAtom(display, "BIONICX_MISSING_OUTPUT_PROPERTY", False);
    Atom actual_type = None;
    int actual_format = -1;
    unsigned long property_items = 1, bytes_after = 1;
    unsigned char *property_data = NULL;
    int property_status = resources && resources->noutput > 0
        ? XRRGetOutputProperty(display, resources->outputs[0], missing_property,
                               0, 16, False, False, AnyPropertyType,
                               &actual_type, &actual_format, &property_items,
                               &bytes_after, &property_data)
        : BadValue;
    bool primary_found = false;
    for (int i = 0; resources && i < resources->noutput; i++) {
        if (resources->outputs[i] == primary) primary_found = true;
    }
    ok = ok && resources && resources->ncrtc > 0 && resources->noutput > 0
         && resources->nmode > 0
         && primary != None && primary_found
         && resources->modes[0].width
                == (unsigned int)DisplayWidth(display, DefaultScreen(display))
         && resources->modes[0].height
                == (unsigned int)DisplayHeight(display, DefaultScreen(display))
         && resources->modes[0].nameLength > 0
         && output_info && output_info->connection == RR_Connected
         && output_info->crtc == resources->crtcs[0]
         && output_info->mm_width > 0 && output_info->mm_height > 0
         && output_info->nmode == 1 && output_info->npreferred == 1
         && output_info->modes[0] == resources->modes[0].id
         && crtc_info && crtc_info->x == 0 && crtc_info->y == 0
         && crtc_info->width == resources->modes[0].width
         && crtc_info->height == resources->modes[0].height
         && crtc_info->mode == resources->modes[0].id
         && crtc_info->rotation == RR_Rotate_0
         && crtc_info->noutput == 1
         && crtc_info->outputs[0] == resources->outputs[0]
         && property_status == Success && actual_type == None
         && actual_format == 0 && property_items == 0 && bytes_after == 0
         && property_data == NULL
         && sync_without_error(display, before);
    char detail[240];
    snprintf(detail, sizeof(detail),
             "version=%d.%d crtcs=%d outputs=%d primary=0x%lx mode=%dx%d name=%.*s output=%d crtc=%d empty-property=%d",
             major, minor, resources ? resources->ncrtc : 0,
             resources ? resources->noutput : 0,
             (unsigned long)primary,
             resources ? resources->modes[0].width : 0,
             resources ? resources->modes[0].height : 0,
             resources ? resources->modes[0].nameLength : 0,
             resources ? resources->modes[0].name : "",
             output_info != NULL, crtc_info != NULL,
             property_status == Success && actual_type == None);
    if (property_data) XFree(property_data);
    if (crtc_info) XRRFreeCrtcInfo(crtc_info);
    if (output_info) XRRFreeOutputInfo(output_info);
    if (resources) XRRFreeScreenResources(resources);
    result("randr", ok, detail);
}

static void probe_xinput2(Display *display, Window window) {
    int opcode = 0, event_base = 0, error_base = 0;
    if (!XQueryExtension(display, "XInputExtension", &opcode,
                         &event_base, &error_base)) {
        result("xinput2", false, "extension-missing");
        return;
    }
    int before = x_errors;
    int major = 2, minor = 0;
    bool ok = XIQueryVersion(display, &major, &minor) == Success;
    int count = 0;
    XIDeviceInfo *devices = XIQueryDevice(display, XIAllDevices, &count);
    bool master_pointer = false, master_keyboard = false;
    for (int i = 0; devices && i < count; i++) {
        master_pointer |= devices[i].use == XIMasterPointer
                          && devices[i].attachment != devices[i].deviceid;
        master_keyboard |= devices[i].use == XIMasterKeyboard
                           && devices[i].attachment != devices[i].deviceid;
    }
    unsigned char selected_mask[XIMaskLen(XI_LASTEVENT)] = {0};
    XISetMask(selected_mask, XI_KeyPress);
    XISetMask(selected_mask, XI_KeyRelease);
    XISetMask(selected_mask, XI_ButtonPress);
    XISetMask(selected_mask, XI_ButtonRelease);
    XISetMask(selected_mask, XI_Motion);
    XIEventMask selection = {
        .deviceid = XIAllMasterDevices,
        .mask_len = sizeof(selected_mask),
        .mask = selected_mask,
    };
    int select_status = XISelectEvents(display, window, &selection, 1);
    int selected_count = 0;
    XIEventMask *selected = XIGetSelectedEvents(display, window, &selected_count);
    bool selected_ok = false;
    for (int i = 0; selected && i < selected_count; i++) {
        if (selected[i].deviceid == XIAllMasterDevices
            && XIMaskIsSet(selected[i].mask, XI_KeyPress)
            && XIMaskIsSet(selected[i].mask, XI_KeyRelease)
            && XIMaskIsSet(selected[i].mask, XI_ButtonPress)
            && XIMaskIsSet(selected[i].mask, XI_ButtonRelease)
            && XIMaskIsSet(selected[i].mask, XI_Motion)) {
            selected_ok = true;
        }
    }
    ok = ok && devices && count >= 2 && master_pointer && master_keyboard
         && select_status == Success && selected_ok
         && sync_without_error(display, before);
    if (devices) XIFreeDeviceInfo(devices);
    if (selected) XFree(selected);
    char detail[128];
    snprintf(detail, sizeof(detail),
             "version=%d.%d devices=%d masters=%d/%d selected=%d masks=%d",
             major, minor, count, master_pointer, master_keyboard,
             selected_ok, selected_count);
    result("xinput2", ok, detail);
}

static void probe_xkb(Display *display) {
    int opcode = 0, event_base = 0, error_base = 0;
    int major = XkbMajorVersion, minor = XkbMinorVersion;
    if (!XkbQueryExtension(display, &opcode, &event_base, &error_base,
                           &major, &minor)) {
        result("xkeyboard", false, "extension-missing");
        return;
    }
    xkb_event_type = event_base;
    int before = x_errors;
    bool selected = XkbSelectEvents(display, XkbUseCoreKbd,
                                    XkbStateNotifyMask,
                                    XkbStateNotifyMask);
    XkbDescPtr map = XkbGetMap(display, XkbAllClientInfoMask,
                               XkbUseCoreKbd);
    int names_status = map ? XkbGetNames(
        display, XkbComponentNamesMask | XkbKeyTypeNamesMask
                 | XkbKTLevelNamesMask | XkbKeyNamesMask
                 | XkbVirtualModNamesMask,
        map) : BadImplementation;
    XkbDeviceInfoPtr device = XkbGetDeviceInfo(
        display, XkbXI_AllDeviceFeaturesMask, XkbUseCoreKbd,
        XkbDfltXIClass, XkbDfltXIId);
    XkbDescPtr named = XkbGetKeyboard(
        display, XkbGBN_TypesMask | XkbGBN_ClientSymbolsMask,
        XkbUseCoreKbd);
    int map_status = map ? Success : BadImplementation;
    int key_syms = map && map->map && map->map->key_sym_map
                   ? XkbKeyNumSyms(map, 9) : 0;
    int sym_offset = map && map->map && map->map->key_sym_map
                     ? map->map->key_sym_map[9].offset : 0;
    KeySym escape = map && map->map && map->map->key_sym_map
                    ? XkbKeySymEntry(map, 9, 0, 0) : NoSymbol;
    KeySym lower_a = map && map->map && map->map->key_sym_map
                     ? XkbKeySymEntry(map, 38, 0, 0) : NoSymbol;
    KeySym upper_a = map && map->map && map->map->key_sym_map
                     ? XkbKeySymEntry(map, 38, 1, 0) : NoSymbol;
    bool named_map_ok = named && named->map
                        && XkbKeyNumSyms(named, 38) == 2
                        && XkbKeySymEntry(named, 38, 0, 0) == XK_a
                        && XkbKeySymEntry(named, 38, 1, 0) == XK_A;
    bool ok = selected && device && device->name
              && strcmp(device->name, "BionicX keyboard") == 0
              && device->device_spec == 3 && map_status == Success
              && map && map->map && names_status == Success && map->names
              && map->names->symbols != None && map->map->types[0].name != None
              && map->map->types[1].level_names
              && map->map->types[1].level_names[1] != None
              && memcmp(map->names->keys[9].name, "ESC\0", 4) == 0
              && map->map->num_types >= 1
              && map->min_key_code <= 9 && map->max_key_code >= 9
              && XkbKeyNumSyms(map, 9) == 1
              && escape == XK_Escape
              && XkbKeyNumSyms(map, 38) == 2
              && lower_a == XK_a && upper_a == XK_A
              && named_map_ok
              && sync_without_error(display, before);
    char detail[208];
    snprintf(detail, sizeof(detail),
             "version=%d.%d opcode=%d selected=%d device=%s map=%d names=%d keys=%u-%u types=%d esc-syms=%d offset=%d escape=0x%lx a-syms=%d a=0x%lx/0x%lx",
             major, minor, opcode, selected,
             device && device->name ? device->name : "(null)", map_status,
             names_status,
             map ? map->min_key_code : 0,
             map ? map->max_key_code : 0,
             map && map->map ? map->map->num_types : 0,
             key_syms, sym_offset, (unsigned long)escape,
             map && map->map ? XkbKeyNumSyms(map, 38) : 0,
             (unsigned long)lower_a, (unsigned long)upper_a);
    if (map) XkbFreeKeyboard(map, XkbAllComponentsMask, True);
    if (device) XkbFreeDeviceInfo(device, XkbXI_AllDeviceFeaturesMask, True);
    if (named) XkbFreeKeyboard(named, XkbAllComponentsMask, True);
    result("xkeyboard", ok, detail);
}

static void probe_xkbcommon(Display *display) {
    xcb_connection_t *connection = XGetXCBConnection(display);
    // A keymap read from X11 is self-contained; do not make this protocol
    // probe depend on the host's text keymap database being installed.
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_DEFAULT_INCLUDES);
    if (context) xkb_context_set_log_level(context, XKB_LOG_LEVEL_DEBUG);
    int32_t device = connection
        ? xkb_x11_get_core_keyboard_device_id(connection) : -1;
    struct xkb_keymap *keymap = context && device >= 0
        ? xkb_x11_keymap_new_from_device(context, connection, device,
                                         XKB_KEYMAP_COMPILE_NO_FLAGS)
        : NULL;
    struct xkb_state *state = keymap
        ? xkb_x11_state_new_from_device(keymap, connection, device) : NULL;
    xkb_keycode_t keycode = keymap
        ? xkb_keymap_key_by_name(keymap, "K038") : XKB_KEYCODE_INVALID;
    const xkb_keysym_t *symbols = NULL;
    int symbol_count = keymap && keycode != XKB_KEYCODE_INVALID
        ? xkb_keymap_key_get_syms_by_level(keymap, keycode, 0, 0, &symbols) : 0;
    xkb_keysym_t base_minus = state
        ? xkb_state_key_get_one_sym(state, 20) : XKB_KEY_NoSymbol;
    if (state) xkb_state_update_key(state, 50, XKB_KEY_DOWN);
    xkb_keysym_t shifted_minus = state
        ? xkb_state_key_get_one_sym(state, 20) : XKB_KEY_NoSymbol;
    if (state) xkb_state_update_key(state, 50, XKB_KEY_UP);
    bool ok = connection && context && device == 3 && keymap && state
              && keycode == 38 && symbol_count == 1
              && symbols && symbols[0] == XKB_KEY_a
              && xkb_state_key_get_one_sym(state, keycode) == XKB_KEY_a
              && base_minus == XKB_KEY_minus
              && shifted_minus == XKB_KEY_underscore;
    char detail[160];
    snprintf(detail, sizeof(detail),
             "device=%d keymap=%d state=%d keycode=%u syms=%d first=0x%x minus=0x%x/0x%x",
             device, keymap != NULL, state != NULL, keycode, symbol_count,
             symbols && symbol_count > 0 ? symbols[0] : 0,
             base_minus, shifted_minus);
    result("xkbcommon", ok, detail);
    if (state) xkb_state_unref(state);
    if (keymap) xkb_keymap_unref(keymap);
    if (context) xkb_context_unref(context);
}

static void probe_optional_shm(Display *display) {
    int major = 0, minor = 0;
    Bool pixmaps = False;
    Bool present = XShmQueryVersion(display, &major, &minor, &pixmaps);
    printf("BXCAP mit-shm %s version=%d.%d pixmaps=%d\n",
           present ? "available" : "unavailable", major, minor, pixmaps);
}

int main(int argc, char **argv) {
    int duration = 8;
    if (argc == 3 && strcmp(argv[1], "--duration") == 0) duration = atoi(argv[2]);
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
    XStoreName(display, window, "BionicX X11 desktop extension probe");
    XSelectInput(display, window, ExposureMask | StructureNotifyMask | ButtonPressMask);
    XMapWindow(display, window);
    XSync(display, False);

    GC gc = XCreateGC(display, window, 0, NULL);
    XSetForeground(display, gc, WhitePixel(display, screen));
    const char *title = "BionicX: real glibc desktop X11 extension probe";
    XDrawString(display, window, gc, 24, 42, title, (int)strlen(title));

    probe_render(display, window);
    probe_xfixes(display, window);
    probe_randr(display, root);
    probe_xinput2(display, window);
    probe_xkb(display);
    probe_xkbcommon(display);
    probe_optional_shm(display);

    char summary[128];
    snprintf(summary, sizeof(summary), "desktop extensions: %d/%d strict pass",
             passed, checks);
    XDrawString(display, window, gc, 24, 68, summary, (int)strlen(summary));
    XFlush(display);
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += duration;
    bool saw_shift_set = false;
    bool saw_shift_clear = false;
    int shaped_button_presses = 0;
    xcb_connection_t *event_connection = XGetXCBConnection(display);
    XFlush(display);
    XSetEventQueueOwner(display, XCBOwnsEventQueue);
    while (true) {
        xcb_generic_event_t *raw_event;
        while ((raw_event = xcb_poll_for_event(event_connection)) != NULL) {
            if ((raw_event->response_type & 0x7f) == xkb_event_type) {
                xcb_xkb_state_notify_event_t *state_event =
                    (xcb_xkb_state_notify_event_t *)raw_event;
                if (state_event->xkbType == XCB_XKB_STATE_NOTIFY &&
                    state_event->keycode == 50) {
                    if ((state_event->baseMods & ShiftMask) != 0)
                        saw_shift_set = true;
                    else
                        saw_shift_clear = true;
                    printf("BXINPUT xkb-state keycode=%u event=%d mods=0x%x base=0x%x locked=0x%x changed=0x%x\n",
                           state_event->keycode, state_event->eventType,
                           state_event->mods, state_event->baseMods,
                           state_event->lockedMods, state_event->changed);
                    fflush(stdout);
                }
            }
            else if ((raw_event->response_type & 0x7f) == XCB_BUTTON_PRESS) {
                xcb_button_press_event_t *button = (xcb_button_press_event_t *)raw_event;
                if (button->event == window) {
                    shaped_button_presses++;
                    printf("BXINPUT xfixes-input-shape event-x=%d event-y=%d root-x=%d root-y=%d\n",
                           button->event_x, button->event_y,
                           button->root_x, button->root_y);
                    fflush(stdout);
                }
            }
            free(raw_event);
        }
        struct timespec now = {0};
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > deadline.tv_sec ||
            (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)) break;
        struct timespec pause = {.tv_nsec = 20 * 1000 * 1000};
        nanosleep(&pause, NULL);
    }
    char state_detail[96];
    snprintf(state_detail, sizeof(state_detail), "shift-set=%d shift-clear=%d",
             saw_shift_set, saw_shift_clear);
    result("xkb-state-notify", saw_shift_set && saw_shift_clear, state_detail);
    char shape_detail[96];
    snprintf(shape_detail, sizeof(shape_detail), "inside-presses=%d expected=1",
             shaped_button_presses);
    result("xfixes-input-shape", shaped_button_presses == 1, shape_detail);
    printf("BXSUMMARY desktop-x11 passed=%d failed=%d xerrors=%d\n",
           passed, checks - passed, x_errors);
    fflush(stdout);
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return checks == passed ? 0 : 1;
}
