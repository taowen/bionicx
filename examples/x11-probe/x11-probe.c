#define _POSIX_C_SOURCE 200809L

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <errno.h>
#include <gnu/libc-version.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

static int x_errors;
static char last_x_error[256];

static int on_x_error(Display *display, XErrorEvent *event) {
    char message[128];
    XGetErrorText(display, event->error_code, message, sizeof(message));
    snprintf(last_x_error, sizeof(last_x_error),
             "%s request=%u minor=%u resource=0x%lx", message,
             event->request_code, event->minor_code, event->resourceid);
    ++x_errors;
    return 0;
}

static void result(const char *name, bool passed, const char *detail) {
    printf("BXTEST %s %s%s%s\n", passed ? "PASS" : "FAIL", name,
           detail && *detail ? " " : "", detail && *detail ? detail : "");
    fflush(stdout);
}

static bool sync_step(Display *display, const char *name, int before) {
    XSync(display, False);
    bool passed = x_errors == before;
    result(name, passed, passed ? "" : last_x_error);
    return passed;
}

static double monotonic_seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
}

static void paint(Display *display, Window window, GC gc, int width, int height,
                  int passed, int failed, int keys, int buttons, int motions) {
    unsigned long colors[] = {0x18233a, 0x157f3b, 0xc17b16, 0xb83a3a};
    XSetForeground(display, gc, colors[0]);
    XFillRectangle(display, window, gc, 0, 0, (unsigned)width, (unsigned)height);
    XSetForeground(display, gc, colors[1]);
    XFillRectangle(display, window, gc, 32, 100,
                   (unsigned)(width / 2 - 48), (unsigned)(height - 150));
    XSetForeground(display, gc, failed ? colors[3] : colors[2]);
    XFillRectangle(display, window, gc, width / 2 + 16, 100,
                   (unsigned)(width / 2 - 48), (unsigned)(height - 150));
    (void)passed;
    (void)keys;
    (void)buttons;
    (void)motions;
    XFlush(display);
}

int main(int argc, char **argv) {
    int duration = 12;
    if (argc == 3 && strcmp(argv[1], "--duration") == 0) {
        duration = atoi(argv[2]);
        if (duration < 1 || duration > 300) {
            fprintf(stderr, "duration must be in 1..300 seconds\n");
            return 2;
        }
    }

    XSetErrorHandler(on_x_error);
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        result("display-connect", false, getenv("DISPLAY"));
        return 1;
    }

    int passed = 0;
    int failed = 0;
#define RECORD(value) do { if (value) ++passed; else ++failed; } while (0)
    char detail[512];
    snprintf(detail, sizeof(detail), "glibc=%s vendor=%s protocol=%d.%d screens=%d",
             gnu_get_libc_version(), ServerVendor(display),
             ProtocolVersion(display), ProtocolRevision(display),
             ScreenCount(display));
    result("display-connect", true, detail);
    ++passed;

    int extension_count = 0;
    char **extensions = XListExtensions(display, &extension_count);
    snprintf(detail, sizeof(detail), "count=%d", extension_count);
    bool extensions_ok = extensions != NULL && extension_count > 0;
    result("list-extensions", extensions_ok,
           extensions_ok ? detail : "server returned no advertised extensions");
    RECORD(extensions_ok);
    bool has_dri3 = false;
    for (int index = 0; index < extension_count; ++index) {
        printf("BXINFO extension %s\n", extensions[index]);
        if (strcmp(extensions[index], "DRI3") == 0) has_dri3 = true;
    }
    result("extension-capability-honesty", !has_dri3,
           has_dri3 ? "incomplete DRI3 advertised"
                    : "DRI3 hidden until Open can return a DRM fd");
    RECORD(!has_dri3);
    if (extensions) XFreeExtensionList(extensions);

    int before = x_errors;
    Atom absent_atom = XInternAtom(display,
                                   "BIONICX_ONLY_IF_EXISTS_PROBE", True);
    Atom created_atom = XInternAtom(display,
                                    "BIONICX_ONLY_IF_EXISTS_PROBE", False);
    Atom existing_atom = XInternAtom(display,
                                     "BIONICX_ONLY_IF_EXISTS_PROBE", True);
    XSync(display, False);
    bool intern_atom_ok = x_errors == before && absent_atom == None &&
                          created_atom != None && existing_atom == created_atom;
    snprintf(detail, sizeof(detail), "absent=%lu created=%lu existing=%lu",
             absent_atom, created_atom, existing_atom);
    result("intern-atom-only-if-exists", intern_atom_ok,
           intern_atom_ok ? detail : last_x_error);
    RECORD(intern_atom_ok);

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    int width = DisplayWidth(display, screen);
    int height = DisplayHeight(display, screen);

    before = x_errors;
    Window initial_focus = None;
    int initial_revert = 0;
    XGetInputFocus(display, &initial_focus, &initial_revert);
    XSync(display, False);
    bool initial_focus_ok = x_errors == before &&
            initial_focus == PointerRoot;
    snprintf(detail, sizeof(detail), "focus=0x%lx expected=0x%lx revert=%d",
             initial_focus, (unsigned long)PointerRoot, initial_revert);
    result("initial-pointer-root-focus", initial_focus_ok,
           initial_focus_ok ? detail : last_x_error);
    RECORD(initial_focus_ok);

    Window window = XCreateSimpleWindow(display, root, 0, 0,
                                         (unsigned)width, (unsigned)height, 0,
                                         BlackPixel(display, screen),
                                         BlackPixel(display, screen));
    XSelectInput(display, window, ExposureMask | StructureNotifyMask |
                 KeyPressMask | KeyReleaseMask |
                 ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask | FocusChangeMask | PropertyChangeMask);
    XStoreName(display, window, "BionicX X11 integration probe");
    XClassHint class_hint = {.res_name = "bionicx-probe",
                             .res_class = "BionicXProbe"};
    XSetClassHint(display, window, &class_hint);
    XWMHints wm_hints = {.flags = WindowGroupHint, .window_group = window};
    XSetWMHints(display, window, &wm_hints);
    Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    Atom wm_protocols = XInternAtom(display, "WM_PROTOCOLS", False);
    Atom utf8 = XInternAtom(display, "UTF8_STRING", False);
    Atom clipboard = XInternAtom(display, "CLIPBOARD", False);
    Atom probe_property = XInternAtom(display, "BIONICX_TEST_PROPERTY", False);
    XSetWMProtocols(display, window, &wm_delete, 1);
    before = x_errors;
    XMapWindow(display, window);
    RECORD(sync_step(display, "window-create-map", before));

    before = x_errors;
    XWindowAttributes inherited_class_attributes = {0};
    Status inherited_class_status = XGetWindowAttributes(
            display, window, &inherited_class_attributes);
    XSync(display, False);
    bool inherited_class_ok = x_errors == before && inherited_class_status
            && inherited_class_attributes.class == InputOutput;
    snprintf(detail, sizeof(detail), "class=%d expected=%d",
             inherited_class_attributes.class, InputOutput);
    result("copy-from-parent-class", inherited_class_ok,
           inherited_class_ok ? detail : last_x_error);
    RECORD(inherited_class_ok);

    before = x_errors;
    XSetInputFocus(display, window, RevertToParent, CurrentTime);
    RECORD(sync_step(display, "input-focus", before));

    before = x_errors;
    Window focused_window = None;
    int focus_revert = 0;
    XSetInputFocus(display, window, RevertToPointerRoot, CurrentTime);
    XGetInputFocus(display, &focused_window, &focus_revert);
    XSync(display, False);
    bool focus_policy_ok = x_errors == before && focused_window == window &&
            focus_revert == RevertToPointerRoot;
    snprintf(detail, sizeof(detail), "window=0x%lx expected=0x%lx revert=%d",
             focused_window, window, focus_revert);
    result("input-focus-revert-policy", focus_policy_ok,
           focus_policy_ok ? detail : last_x_error);
    RECORD(focus_policy_ok);

    before = x_errors;
    int keysyms_per_keycode = 0;
    KeySym *keyboard_map = XGetKeyboardMapping(display, 24, 2,
                                                &keysyms_per_keycode);
    XSync(display, False);
    bool keyboard_map_ok = x_errors == before && keyboard_map != NULL &&
            keysyms_per_keycode == 2 && keyboard_map[0] == XK_q &&
            keyboard_map[1] == XK_Q && keyboard_map[2] == XK_w &&
            keyboard_map[3] == XK_W;
    snprintf(detail, sizeof(detail), "width=%d q=0x%lx Q=0x%lx w=0x%lx W=0x%lx",
             keysyms_per_keycode,
             keyboard_map != NULL ? keyboard_map[0] : 0,
             keyboard_map != NULL ? keyboard_map[1] : 0,
             keyboard_map != NULL ? keyboard_map[2] : 0,
             keyboard_map != NULL ? keyboard_map[3] : 0);
    result("keyboard-mapping-levels", keyboard_map_ok,
           keyboard_map_ok ? detail : last_x_error);
    RECORD(keyboard_map_ok);
    if (keyboard_map != NULL) XFree(keyboard_map);

    const char payload[] = "bionicx-property-roundtrip";
    before = x_errors;
    XChangeProperty(display, window, probe_property, utf8, 8, PropModeReplace,
                    (const unsigned char *)payload, (int)strlen(payload));
    Atom actual_type = None;
    int actual_format = 0;
    unsigned long items = 0;
    unsigned long remaining = 0;
    unsigned char *property = NULL;
    int property_status = XGetWindowProperty(
            display, window, probe_property, 0, 1024, False, utf8,
            &actual_type, &actual_format, &items, &remaining, &property);
    XSync(display, False);
    bool property_ok = x_errors == before && property_status == Success &&
            actual_type == utf8 && actual_format == 8 &&
            items == strlen(payload) && property &&
            memcmp(property, payload, items) == 0;
    result("property-roundtrip", property_ok,
           property_ok ? "UTF8_STRING" : last_x_error);
    RECORD(property_ok);
    if (property) XFree(property);

    before = x_errors;
    int property_count = 0;
    Atom *property_names = XListProperties(display, window, &property_count);
    bool found_probe_property = false;
    for (int i = 0; i < property_count; ++i) {
        if (property_names[i] == probe_property) found_probe_property = true;
    }
    if (property_names) XFree(property_names);
    XSync(display, False);
    bool list_properties_ok = x_errors == before && found_probe_property;
    char list_properties_detail[80];
    snprintf(list_properties_detail, sizeof(list_properties_detail),
             "count=%d probe-atom=%d", property_count,
             found_probe_property ? 1 : 0);
    result("list-properties", list_properties_ok, list_properties_detail);
    RECORD(list_properties_ok);

    before = x_errors;
    Font cursor_font = XLoadFont(display, "cursor");
    XSync(display, False);
    bool font_open_ok = x_errors == before && cursor_font != None;
    if (cursor_font != None) XUnloadFont(display, cursor_font);
    XSync(display, False);
    bool font_lifecycle_ok = font_open_ok && x_errors == before;
    result("font-open-close", font_lifecycle_ok,
           font_lifecycle_ok ? "cursor resource released" : last_x_error);
    RECORD(font_lifecycle_ok);

    before = x_errors;
    XFontStruct *fixed_font = XLoadQueryFont(display, "fixed");
    XSync(display, False);
    bool fixed_font_ok = x_errors == before && fixed_font != NULL &&
            fixed_font->min_bounds.width > 0 &&
            fixed_font->ascent > 0 && fixed_font->descent >= 0;
    snprintf(detail, sizeof(detail), "width=%d ascent=%d descent=%d",
             fixed_font ? fixed_font->min_bounds.width : -1,
             fixed_font ? fixed_font->ascent : -1,
             fixed_font ? fixed_font->descent : -1);
    result("core-font-query", fixed_font_ok,
           fixed_font_ok ? detail : last_x_error);
    RECORD(fixed_font_ok);

    int direction = -1;
    int font_ascent = -1;
    int font_descent = -1;
    XCharStruct overall = {0};
    before = x_errors;
    if (fixed_font) XQueryTextExtents(display, fixed_font->fid,
                                      "BionicX", 7, &direction,
                                      &font_ascent, &font_descent, &overall);
    XSync(display, False);
    bool text_extents_ok = x_errors == before && fixed_font &&
            direction == FontLeftToRight && overall.width > 0 &&
            font_ascent == fixed_font->ascent &&
            font_descent == fixed_font->descent;
    snprintf(detail, sizeof(detail), "width=%d direction=%d",
             overall.width, direction);
    result("core-font-text-extents", text_extents_ok,
           text_extents_ok ? detail : last_x_error);
    RECORD(text_extents_ok);

    int fixed_name_count = 0;
    before = x_errors;
    char **fixed_names = XListFonts(display, "fixed", 8,
                                    &fixed_name_count);
    XSync(display, False);
    bool list_fonts_ok = x_errors == before && fixed_names &&
            fixed_name_count == 1 && strcmp(fixed_names[0], "fixed") == 0;
    snprintf(detail, sizeof(detail), "count=%d", fixed_name_count);
    result("core-font-list", list_fonts_ok,
           list_fonts_ok ? detail : last_x_error);
    RECORD(list_fonts_ok);
    if (fixed_names) XFreeFontNames(fixed_names);
    if (fixed_font) XFreeFont(display, fixed_font);

    XColor allocated_color = {.red = 0x1234, .green = 0x5678,
                              .blue = 0x9abc,
                              .flags = DoRed | DoGreen | DoBlue};
    before = x_errors;
    Status alloc_color_status = XAllocColor(
            display, DefaultColormap(display, screen), &allocated_color);
    XColor queried_color = {.pixel = allocated_color.pixel};
    XQueryColor(display, DefaultColormap(display, screen), &queried_color);
    XSync(display, False);
    bool truecolor_ok = x_errors == before && alloc_color_status &&
            allocated_color.pixel == 0x12569a &&
            allocated_color.red == 0x1212 &&
            allocated_color.green == 0x5656 &&
            allocated_color.blue == 0x9a9a &&
            queried_color.red == allocated_color.red &&
            queried_color.green == allocated_color.green &&
            queried_color.blue == allocated_color.blue;
    snprintf(detail, sizeof(detail),
             "pixel=0x%lx rgb=%04x/%04x/%04x",
             allocated_color.pixel, queried_color.red,
             queried_color.green, queried_color.blue);
    result("truecolor-alloc-query", truecolor_ok,
           truecolor_ok ? detail : last_x_error);
    RECORD(truecolor_ok);

    before = x_errors;
    Pixmap pixmap = XCreatePixmap(display, window, 160, 100,
                                  (unsigned)DefaultDepth(display, screen));
    GC gc = XCreateGC(display, window, 0, NULL);
    XSetForeground(display, gc, 0x3264c8);
    XFillRectangle(display, pixmap, gc, 0, 0, 160, 100);
    XCopyArea(display, pixmap, window, gc, 0, 0, 160, 100, 40, 160);
    XImage *pixmap_image = XGetImage(display, pixmap, 0, 0, 1, 1,
                                     AllPlanes, ZPixmap);
    XImage *window_image = XGetImage(display, window, 40, 160, 1, 1,
                                     AllPlanes, ZPixmap);
    XSync(display, False);
    bool drawing_ok = x_errors == before && pixmap_image && window_image &&
                      (XGetPixel(pixmap_image, 0, 0) & 0x00ffffff) == 0x3264c8 &&
                      (XGetPixel(window_image, 0, 0) & 0x00ffffff) == 0x3264c8;
    if (pixmap_image) XDestroyImage(pixmap_image);
    if (window_image) XDestroyImage(window_image);
    result("pixmap-gc-copy", drawing_ok,
           drawing_ok ? "pixel=0x3264c8" : "pixel readback mismatch");
    RECORD(drawing_ok);

    before = x_errors;
    XSetWindowBackgroundPixmap(display, window, pixmap);
    XClearArea(display, window, 400, 160, 80, 60, False);
    XImage *background_image = XGetImage(display, window, 420, 180, 1, 1,
                                         AllPlanes, ZPixmap);
    XSync(display, False);
    bool background_ok = x_errors == before && background_image &&
            (XGetPixel(background_image, 0, 0) & 0x00ffffff) == 0x3264c8;
    if (background_image) XDestroyImage(background_image);
    result("background-pixmap", background_ok,
           background_ok ? "clear-pixel=0x3264c8"
                         : "background tile mismatch");
    RECORD(background_ok);

    before = x_errors;
    Window title = XCreateSimpleWindow(display, window, 0, 0, 1, 1, 0,
                                       BlackPixel(display, screen),
                                       BlackPixel(display, screen));
    XSetWindowBackgroundPixmap(display, title, pixmap);
    XMapWindow(display, title);
    XMoveResizeWindow(display, title, 8, 8, 80, 29);
    XSync(display, False);
    XImage *title_image = XGetImage(display, title, 4, 4, 1, 1,
                                    AllPlanes, ZPixmap);
    unsigned long title_pixel = title_image
            ? XGetPixel(title_image, 0, 0) : 0;
    bool title_ok = x_errors == before && title_image
            && (title_pixel & 0x00ffffff) == 0x3264c8;
    if (title_image) XDestroyImage(title_image);
    result("map-resize-background", title_ok,
           title_ok ? "pixel=0x3264c8" : "map+resize left background unpainted");
    RECORD(title_ok);
    XDestroyWindow(display, title);

    before = x_errors;
    XSetForeground(display, gc, 0xa02020);
    XFillRectangle(display, window, gc, 240, 160, 80, 60);
    XSetForeground(display, gc, 0x2060c0);
    XFillRectangle(display, pixmap, gc, 0, 0, 80, 60);
    XRectangle copy_clip = {.x = 260, .y = 175, .width = 20, .height = 20};
    XSetClipRectangles(display, gc, 0, 0, &copy_clip, 1, Unsorted);
    XCopyArea(display, pixmap, window, gc, 0, 0, 80, 60, 240, 160);
    XImage *clip_inside = XGetImage(display, window, 265, 180, 1, 1,
                                    AllPlanes, ZPixmap);
    XImage *clip_outside = XGetImage(display, window, 245, 165, 1, 1,
                                     AllPlanes, ZPixmap);
    XSetClipMask(display, gc, None);
    XSync(display, False);
    bool copy_clip_ok = x_errors == before && clip_inside && clip_outside
            && (XGetPixel(clip_inside, 0, 0) & 0xffffff) == 0x2060c0
            && (XGetPixel(clip_outside, 0, 0) & 0xffffff) == 0xa02020;
    if (clip_inside) XDestroyImage(clip_inside);
    if (clip_outside) XDestroyImage(clip_outside);
    result("copy-area-clip", copy_clip_ok,
           copy_clip_ok ? "inside=0x2060c0 outside=0xa02020"
                        : "GC clip mismatch");
    RECORD(copy_clip_ok);

    before = x_errors;
    XSetForeground(display, gc, 0x000000);
    XFillRectangle(display, window, gc, 340, 160, 24, 24);
    XSetForeground(display, gc, 0x20c060);
    XPoint origin_points[] = {{342, 162}, {345, 165}};
    XPoint previous_points[] = {{350, 170}, {3, 2}};
    XDrawPoints(display, window, gc, origin_points, 2, CoordModeOrigin);
    XDrawPoints(display, window, gc, previous_points, 2, CoordModePrevious);
    XImage *origin_point_image = XGetImage(display, window, 345, 165, 1, 1,
                                           AllPlanes, ZPixmap);
    XImage *previous_point_image = XGetImage(display, window, 353, 172, 1, 1,
                                             AllPlanes, ZPixmap);
    XSync(display, False);
    bool poly_point_ok = x_errors == before && origin_point_image &&
            previous_point_image &&
            (XGetPixel(origin_point_image, 0, 0) & 0xffffff) == 0x20c060 &&
            (XGetPixel(previous_point_image, 0, 0) & 0xffffff) == 0x20c060;
    if (origin_point_image) XDestroyImage(origin_point_image);
    if (previous_point_image) XDestroyImage(previous_point_image);
    result("poly-point", poly_point_ok,
           poly_point_ok ? "origin=0x20c060 previous=0x20c060"
                         : "point readback mismatch");
    RECORD(poly_point_ok);

    before = x_errors;
    XSetForeground(display, gc, WhitePixel(display, screen));
    XDrawString(display, window, gc, 32, 42, "BionicX PolyText8", 17);
    XImage *text_image = XGetImage(display, window, 28, 20, 220, 30,
                                   AllPlanes, ZPixmap);
    XSync(display, False);
    int text_pixels = 0;
    if (text_image) {
        for (int text_y = 0; text_y < text_image->height; ++text_y)
            for (int text_x = 0; text_x < text_image->width; ++text_x)
                if ((XGetPixel(text_image, text_x, text_y) & 0x00ffffff) != 0)
                    ++text_pixels;
        XDestroyImage(text_image);
    }
    bool text_ok = x_errors == before && text_pixels > 0;
    snprintf(detail, sizeof(detail), "foreground-pixels=%d", text_pixels);
    result("poly-text8", text_ok, text_ok ? detail : last_x_error);
    RECORD(text_ok);

    before = x_errors;
    Window child = XCreateSimpleWindow(display, window, 220, 160, 160, 100, 1,
                                        WhitePixel(display, screen), 0x6c3ba8);
    XMapWindow(display, child);
    Window query_root = None;
    Window query_parent = None;
    Window *children = NULL;
    unsigned child_count = 0;
    Status tree_status = XQueryTree(display, window, &query_root, &query_parent,
                                    &children, &child_count);
    XSync(display, False);
    bool tree_ok = x_errors == before && tree_status && query_root == root &&
                   query_parent == root && child_count >= 1;
    snprintf(detail, sizeof(detail), "children=%u", child_count);
    result("window-tree", tree_ok, tree_ok ? detail : last_x_error);
    RECORD(tree_ok);
    if (children) XFree(children);

    before = x_errors;
    Window input_only = XCreateWindow(display, window, 3, 4, 40, 30, 0, 0,
                                      InputOnly, CopyFromParent, 0, NULL);
    XWindowAttributes input_attributes = {0};
    Window geometry_root = None;
    int geometry_x = 0;
    int geometry_y = 0;
    unsigned geometry_width = 0;
    unsigned geometry_height = 0;
    unsigned geometry_border = 0;
    unsigned geometry_depth = 99;
    Status attributes_status =
            XGetWindowAttributes(display, input_only, &input_attributes);
    Status geometry_status = XGetGeometry(
            display, input_only, &geometry_root, &geometry_x, &geometry_y,
            &geometry_width, &geometry_height, &geometry_border,
            &geometry_depth);
    XSync(display, False);
    bool input_geometry_ok = x_errors == before && attributes_status &&
            geometry_status && input_attributes.class == InputOnly &&
            geometry_root == root && geometry_x == 3 && geometry_y == 4 &&
            geometry_width == 40 && geometry_height == 30 &&
            geometry_border == 0 && geometry_depth == 0;
    snprintf(detail, sizeof(detail),
             "class=%d geometry=%ux%u%+d%+d border=%u depth=%u",
             input_attributes.class, geometry_width, geometry_height,
             geometry_x, geometry_y, geometry_border, geometry_depth);
    result("input-only-geometry", input_geometry_ok,
           input_geometry_ok ? detail : last_x_error);
    RECORD(input_geometry_ok);

    while (XPending(display)) {
        XEvent ignored;
        XNextEvent(display, &ignored);
    }
    before = x_errors;
    Window map_parent = XCreateSimpleWindow(display, window, 400, 160,
            120, 80, 0, 0, 0x202020);
    Window map_child = XCreateSimpleWindow(display, map_parent, 4, 4,
            80, 50, 0, 0, 0x408060);
    Window map_grandchild = XCreateSimpleWindow(display, map_child, 2, 2,
            30, 20, 0, 0, 0x804060);
    XSelectInput(display, map_child, ExposureMask | VisibilityChangeMask);
    XSelectInput(display, map_grandchild, ExposureMask);
    XMapSubwindows(display, map_parent);
    XSync(display, False);
    XWindowAttributes parent_before_map = {0};
    XWindowAttributes child_before_map = {0};
    XWindowAttributes grandchild_before_map = {0};
    XGetWindowAttributes(display, map_parent, &parent_before_map);
    XGetWindowAttributes(display, map_child, &child_before_map);
    XGetWindowAttributes(display, map_grandchild, &grandchild_before_map);
    bool premature_expose = false;
    bool premature_visibility = false;
    while (XPending(display)) {
        XEvent event;
        XNextEvent(display, &event);
        if (event.type == Expose && (event.xexpose.window == map_child
                || event.xexpose.window == map_grandchild))
            premature_expose = true;
        if (event.type == VisibilityNotify
                && event.xvisibility.window == map_child)
            premature_visibility = true;
    }
    XMapWindow(display, map_parent);
    XSync(display, False);
    XWindowAttributes child_after_map = {0};
    XWindowAttributes grandchild_after_map = {0};
    XGetWindowAttributes(display, map_child, &child_after_map);
    XGetWindowAttributes(display, map_grandchild, &grandchild_after_map);
    bool child_exposed = false;
    bool child_visible = false;
    bool grandchild_exposed = false;
    while (XPending(display)) {
        XEvent event;
        XNextEvent(display, &event);
        if (event.type == Expose && event.xexpose.window == map_child)
            child_exposed = true;
        if (event.type == VisibilityNotify
                && event.xvisibility.window == map_child
                && event.xvisibility.state == VisibilityUnobscured)
            child_visible = true;
        if (event.type == Expose && event.xexpose.window == map_grandchild)
            grandchild_exposed = true;
    }
    bool map_subwindows_ok = x_errors == before
            && parent_before_map.map_state == IsUnmapped
            && child_before_map.map_state == IsUnviewable
            && grandchild_before_map.map_state == IsUnmapped
            && !premature_expose && !premature_visibility
            && child_after_map.map_state == IsViewable
            && grandchild_after_map.map_state == IsUnmapped
            && child_exposed && child_visible && !grandchild_exposed;
    snprintf(detail, sizeof(detail),
             "before=%d/%d/%d premature=%d/%d after=%d/%d expose=%d/%d visible=%d",
             parent_before_map.map_state, child_before_map.map_state,
             grandchild_before_map.map_state, premature_expose,
             premature_visibility,
             child_after_map.map_state, grandchild_after_map.map_state,
             child_exposed, grandchild_exposed, child_visible);
    result("map-subwindows-exposure", map_subwindows_ok, detail);
    RECORD(map_subwindows_ok);
    XDestroyWindow(display, map_parent);

    int translated_x = 0;
    int translated_y = 0;
    Window translated_child = None;
    before = x_errors;
    Bool translated = XTranslateCoordinates(display, child, root, 0, 0,
                                             &translated_x, &translated_y,
                                             &translated_child);
    XSync(display, False);
    bool translate_ok = x_errors == before && translated;
    snprintf(detail, sizeof(detail), "root=%d,%d", translated_x, translated_y);
    result("translate-coordinates", translate_ok,
           translate_ok ? detail : last_x_error);
    RECORD(translate_ok);

    before = x_errors;
    static const char cursor_bits[] = {0x03, 0x03};
    Pixmap cursor_source = XCreateBitmapFromData(display, window, cursor_bits, 2, 2);
    XColor foreground = {.red = 0xffff, .green = 0xffff, .blue = 0xffff};
    XColor background = {0};
    Cursor cursor = XCreatePixmapCursor(display, cursor_source, cursor_source,
                                        &foreground, &background, 0, 0);
    XDefineCursor(display, window, cursor);
    RECORD(sync_step(display, "cursor-create-define", before));

    XColor recolor_foreground = {.red = 0xffff, .green = 0x2222,
                                 .blue = 0x1111};
    XColor recolor_background = {.red = 0x1111, .green = 0x2222,
                                 .blue = 0xffff};
    before = x_errors;
    XRecolorCursor(display, cursor, &recolor_foreground,
                   &recolor_background);
    RECORD(sync_step(display, "cursor-recolor", before));

    before = x_errors;
    XSetSelectionOwner(display, clipboard, window, CurrentTime);
    XSync(display, False);
    bool selection_ok = x_errors == before &&
                        XGetSelectionOwner(display, clipboard) == window;
    result("selection-owner", selection_ok,
           selection_ok ? "CLIPBOARD" : last_x_error);
    RECORD(selection_ok);

    XEvent sent = {0};
    sent.xclient.type = ClientMessage;
    sent.xclient.display = display;
    sent.xclient.window = window;
    sent.xclient.message_type = wm_protocols;
    sent.xclient.format = 32;
    sent.xclient.data.l[0] = (long)wm_delete;
    sent.xclient.data.l[1] = CurrentTime;
    before = x_errors;
    Status send_status = XSendEvent(display, window, False, NoEventMask, &sent);
    bool send_ok = send_status && sync_step(display, "client-message-send", before);
    if (!send_status) result("client-message-send", false, "XSendEvent returned 0");
    RECORD(send_ok);

    int keys = 0;
    int key_releases = 0;
    int buttons = 0;
    int motions = 0;
    bool saw_lower_a = false;
    bool saw_shift_press_before_mask = false;
    bool saw_shifted_underscore = false;
    bool saw_shift_release_with_mask = false;
    bool client_message_received = false;
    double deadline = monotonic_seconds() + duration;
    paint(display, window, gc, width, height, passed, failed,
          keys, buttons, motions);
    while (monotonic_seconds() < deadline) {
        while (XPending(display)) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == KeyPress) {
                ++keys;
                KeySym base = XLookupKeysym(&event.xkey, 0);
                KeySym shifted = XLookupKeysym(&event.xkey, 1);
                printf("BXINPUT keycode=%u state=0x%x base=0x%lx shifted=0x%lx\n",
                       event.xkey.keycode, event.xkey.state,
                       (unsigned long)base, (unsigned long)shifted);
                fflush(stdout);
                if (event.xkey.keycode == 38 && event.xkey.state == 0)
                    saw_lower_a = base == XK_a;
                if (event.xkey.keycode == 50 && event.xkey.state == 0)
                    saw_shift_press_before_mask = true;
                if (event.xkey.keycode == 20 &&
                    (event.xkey.state & ShiftMask) != 0)
                    saw_shifted_underscore = shifted == XK_underscore;
            }
            else if (event.type == KeyRelease) {
                ++key_releases;
                printf("BXINPUT release keycode=%u state=0x%x\n",
                       event.xkey.keycode, event.xkey.state);
                fflush(stdout);
                if (event.xkey.keycode == 50 &&
                    (event.xkey.state & ShiftMask) != 0)
                    saw_shift_release_with_mask = true;
            }
            else if (event.type == ButtonPress) ++buttons;
            else if (event.type == MotionNotify) ++motions;
            else if (event.type == ClientMessage &&
                     event.xclient.message_type == wm_protocols &&
                     (Atom)event.xclient.data.l[0] == wm_delete)
                client_message_received = true;
            if (event.type == Expose || event.type == ConfigureNotify ||
                event.type == KeyPress || event.type == ButtonPress ||
                event.type == MotionNotify)
                paint(display, window, gc, width, height, passed, failed,
                      keys, buttons, motions);
        }
        fd_set read_set;
        FD_ZERO(&read_set);
        int fd = ConnectionNumber(display);
        FD_SET(fd, &read_set);
        struct timeval timeout = {.tv_sec = 0, .tv_usec = 100000};
        if (select(fd + 1, &read_set, NULL, NULL, &timeout) < 0 && errno != EINTR)
            break;
    }

    result("client-message-receive", client_message_received,
           client_message_received ? "WM_DELETE_WINDOW" : "event not delivered");
    RECORD(client_message_received);
    bool modifier_state_ok = saw_lower_a && saw_shift_press_before_mask &&
                             saw_shifted_underscore &&
                             saw_shift_release_with_mask;
    snprintf(detail, sizeof(detail),
             "lower=%d shift-press=%d underscore=%d shift-release=%d releases=%d",
             saw_lower_a, saw_shift_press_before_mask, saw_shifted_underscore,
             saw_shift_release_with_mask, key_releases);
    result("modifier-event-state", modifier_state_ok, detail);
    RECORD(modifier_state_ok);
    snprintf(detail, sizeof(detail), "keys=%d buttons=%d motions=%d",
             keys, buttons, motions);
    printf("BXOBS input-events %s\n", detail);
    printf("BXSUMMARY passed=%d failed=%d observational_input=%s\n",
           passed, failed, keys + buttons + motions > 0 ? "yes" : "no");

    XFreeCursor(display, cursor);
    XFreePixmap(display, cursor_source);
    XDestroyWindow(display, input_only);
    XDestroyWindow(display, child);
    XFreePixmap(display, pixmap);
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return failed == 0 ? 0 : 1;
}
