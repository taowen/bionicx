#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void result(const char *name, int passed, const char *detail) {
    printf("BXTEST %s %s%s%s\n", passed ? "PASS" : "FAIL", name,
           detail && *detail ? " " : "", detail ? detail : "");
    fflush(stdout);
}

static void *required_symbol(void *library, const char *name) {
    dlerror();
    void *symbol = dlsym(library, name);
    const char *error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "BXTEST FAIL gtk-symbol name=%s error=%s\n", name,
                error);
        exit(1);
    }
    return symbol;
}

static void pump(int (*pending)(void), int (*iterate)(int), int milliseconds) {
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        while (pending()) iterate(0);
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - start.tv_sec) * 1000
                + (now.tv_nsec - start.tv_nsec) / 1000000;
        if (elapsed >= milliseconds) break;
        struct timespec delay = {.tv_sec = 0, .tv_nsec = 10 * 1000 * 1000};
        nanosleep(&delay, NULL);
    }
}

struct Paint {
    int windows;
    int ink;
    int light;
    int mid;
    int dark;
    unsigned long pixel;
    unsigned long xid;
};

static void sample_window(Display *display, Window window, struct Paint *paint) {
    XWindowAttributes attributes = {0};
    if (!XGetWindowAttributes(display, window, &attributes)
            || attributes.map_state != IsViewable
            || attributes.width < 16 || attributes.height < 16)
        return;
    int crop_w = attributes.width > 160 ? 160 : attributes.width;
    int crop_h = attributes.height > 48 ? 48 : attributes.height;
    XImage *image = XGetImage(display, window, 0, 0, (unsigned)crop_w,
                              (unsigned)crop_h, AllPlanes, ZPixmap);
    int light = 0;
    int mid = 0;
    int dark = 0;
    unsigned long pixel = 0;
    if (image != NULL) {
        pixel = XGetPixel(image, 2, 2) & 0xffffff;
        for (int y = 0; y < crop_h; ++y) {
            for (int x = 0; x < crop_w; ++x) {
                unsigned long p = XGetPixel(image, x, y) & 0xffffff;
                int lum = (int)(((p >> 16) & 0xff) + ((p >> 8) & 0xff)
                        + (p & 0xff));
                if (lum >= 0x180) ++light;
                else if (lum <= 0x40) ++dark;
                else ++mid;
            }
        }
        XDestroyImage(image);
    }
    int ink = mid + dark;
    printf("BXINFO tv-paint 0x%lx %dx%d+%d+%d pixel=0x%06lx light=%d mid=%d "
           "dark=%d ink=%d\n",
           (unsigned long)window, attributes.width, attributes.height,
           attributes.x, attributes.y, pixel, light, mid, dark, ink);
    fflush(stdout);
    ++paint->windows;
    int paper = light >= 1000 && ink >= 40 && ink < light;
    if (paper && (paint->light < 1000 || ink > paint->ink)) {
        paint->ink = ink;
        paint->light = light;
        paint->mid = mid;
        paint->dark = dark;
        paint->pixel = pixel;
        paint->xid = (unsigned long)window;
    }
}

static void walk_paint(Display *display, Window window, struct Paint *paint,
                       int depth) {
    if (depth > 6) return;
    sample_window(display, window, paint);
    Window query_root = None;
    Window parent = None;
    Window *kids = NULL;
    unsigned count = 0;
    if (!XQueryTree(display, window, &query_root, &parent, &kids, &count)
            || kids == NULL)
        return;
    for (unsigned i = 0; i < count; ++i)
        walk_paint(display, kids[i], paint, depth + 1);
    XFree(kids);
}

int main(int argc, char **argv) {
    int passed = 0;
    int failed = 0;
#define RECORD(value) do { if (value) ++passed; else ++failed; } while (0)

    void *gtk = dlopen("libgtk-3.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (gtk == NULL) {
        result("gtk-dlopen", 0, dlerror());
        return 1;
    }
    result("gtk-dlopen", 1, "libgtk-3.so.0");
    ++passed;

    int (*gtk_init_check)(int *, char ***) = NULL;
    void *(*gtk_window_new)(int) = NULL;
    void (*gtk_window_set_default_size)(void *, int, int) = NULL;
    void *(*gtk_text_view_new)(void) = NULL;
    void *(*gtk_text_view_get_buffer)(void *) = NULL;
    void (*gtk_text_buffer_set_text)(void *, const char *, int) = NULL;
    void *(*gtk_text_view_get_window)(void *, int) = NULL;
    void (*gtk_container_add)(void *, void *) = NULL;
    void (*gtk_widget_show_all)(void *) = NULL;
    int (*gtk_widget_get_mapped)(void *) = NULL;
    void *(*gtk_widget_get_window)(void *) = NULL;
    int (*gtk_events_pending)(void) = NULL;
    int (*gtk_main_iteration_do)(int) = NULL;
    void *(*gdk_display_get_default)(void) = NULL;
    Display *(*gdk_x11_display_get_xdisplay)(void *) = NULL;
    unsigned long (*gdk_x11_window_get_xid)(void *) = NULL;
    *(void **)(&gtk_init_check) = required_symbol(gtk, "gtk_init_check");
    *(void **)(&gtk_window_new) = required_symbol(gtk, "gtk_window_new");
    *(void **)(&gtk_window_set_default_size) =
            required_symbol(gtk, "gtk_window_set_default_size");
    *(void **)(&gtk_text_view_new) = required_symbol(gtk, "gtk_text_view_new");
    *(void **)(&gtk_text_view_get_buffer) =
            required_symbol(gtk, "gtk_text_view_get_buffer");
    *(void **)(&gtk_text_buffer_set_text) =
            required_symbol(gtk, "gtk_text_buffer_set_text");
    *(void **)(&gtk_text_view_get_window) =
            required_symbol(gtk, "gtk_text_view_get_window");
    *(void **)(&gtk_container_add) = required_symbol(gtk, "gtk_container_add");
    *(void **)(&gtk_widget_show_all) = required_symbol(gtk, "gtk_widget_show_all");
    *(void **)(&gtk_widget_get_mapped) =
            required_symbol(gtk, "gtk_widget_get_mapped");
    *(void **)(&gtk_widget_get_window) =
            required_symbol(gtk, "gtk_widget_get_window");
    *(void **)(&gtk_events_pending) = required_symbol(gtk, "gtk_events_pending");
    *(void **)(&gtk_main_iteration_do) =
            required_symbol(gtk, "gtk_main_iteration_do");
    *(void **)(&gdk_display_get_default) =
            required_symbol(gtk, "gdk_display_get_default");
    *(void **)(&gdk_x11_display_get_xdisplay) =
            required_symbol(gtk, "gdk_x11_display_get_xdisplay");
    *(void **)(&gdk_x11_window_get_xid) =
            required_symbol(gtk, "gdk_x11_window_get_xid");

    int init_ok = gtk_init_check(&argc, &argv) != 0;
    result("gtk-init", init_ok, init_ok ? "DISPLAY connected" : "failed");
    RECORD(init_ok);
    if (!init_ok) {
        printf("BXSUMMARY gtk-textview passed=%d failed=%d\n", passed, failed);
        return 1;
    }

    void *window = gtk_window_new(0);
    void *view = gtk_text_view_new();
    void *buffer = view != NULL ? gtk_text_view_get_buffer(view) : NULL;
    gtk_window_set_default_size(window, 320, 200);
    if (view != NULL) gtk_container_add(window, view);
    gtk_widget_show_all(window);
    pump(gtk_events_pending, gtk_main_iteration_do, 500);
    void *gdk_window = gtk_widget_get_window(window);
    int mapped = gtk_widget_get_mapped(window) && gdk_window != NULL;
    result("gtk-window", mapped, mapped ? "mapped" : "unmapped");
    RECORD(mapped);
    int view_ok = view != NULL && buffer != NULL;
    result("gtk-textview", view_ok, view_ok ? "buffer" : "null");
    RECORD(view_ok);
    if (!mapped || !view_ok) {
        printf("BXSUMMARY gtk-textview passed=%d failed=%d\n", passed, failed);
        return 1;
    }

    void *gdk_display = gdk_display_get_default();
    Display *display = gdk_x11_display_get_xdisplay(gdk_display);
    Window top = (Window)gdk_x11_window_get_xid(gdk_window);
    struct Paint empty = {0};
    walk_paint(display, top, &empty, 0);
    printf("BXINFO tv-empty windows=%d xid=0x%lx ink=%d light=%d mid=%d "
           "dark=%d pixel=0x%06lx\n",
           empty.windows, empty.xid, empty.ink, empty.light, empty.mid,
           empty.dark, empty.pixel);
    fflush(stdout);

    gtk_text_buffer_set_text(buffer, "BxGlyphs", -1);
    pump(gtk_events_pending, gtk_main_iteration_do, 800);
    void *text_gdk = gtk_text_view_get_window(view, 2);
    struct Paint glyphs = {0};
    walk_paint(display, top, &glyphs, 0);
    struct Paint textwin = {0};
    Window text = None;
    if (text_gdk != NULL)
        text = (Window)gdk_x11_window_get_xid(text_gdk);
    if (text != None) sample_window(display, text, &textwin);
    printf("BXINFO tv-textwin xid=0x%lx ink=%d light=%d mid=%d dark=%d "
           "pixel=0x%06lx paper=0x%lx\n",
           (unsigned long)text, textwin.ink, textwin.light, textwin.mid,
           textwin.dark, textwin.pixel, textwin.xid);
    fflush(stdout);
    if (text != None) {
        XWindowAttributes attributes = {0};
        if (XGetWindowAttributes(display, text, &attributes)) {
            printf("BXINFO tv-textattr 0x%lx class=%d map=%d mask=0x%lx "
                   "depth=%d\n",
                   (unsigned long)text, attributes.class,
                   attributes.map_state,
                   (unsigned long)attributes.your_event_mask,
                   attributes.depth);
            fflush(stdout);
        }
        XClearArea(display, text, 0, 0, 0, 0, True);
        pump(gtk_events_pending, gtk_main_iteration_do, 400);
        struct Paint cleared = {0};
        sample_window(display, text, &cleared);
        printf("BXINFO tv-clear xid=0x%lx ink=%d light=%d mid=%d dark=%d "
               "pixel=0x%06lx paper=0x%lx\n",
               (unsigned long)text, cleared.ink, cleared.light, cleared.mid,
               cleared.dark, cleared.pixel, cleared.xid);
        fflush(stdout);
    }
    char detail[160];
    int glyphs_ok = glyphs.light >= 1000 && glyphs.ink >= 40
            && glyphs.ink < glyphs.light;
    snprintf(detail, sizeof(detail),
             "xid=0x%lx ink=%d mid=%d dark=%d pixel=0x%06lx windows=%d",
             glyphs.xid, glyphs.ink, glyphs.mid, glyphs.dark, glyphs.pixel,
             glyphs.windows);
    result("textview-glyphs", glyphs_ok, detail);
    RECORD(glyphs_ok);

    printf("BXSUMMARY gtk-textview passed=%d failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
