#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct ListNode {
    void *data;
    struct ListNode *next;
} ListNode;

typedef struct GdkEventButton {
    int type;
    void *window;
    signed char send_event;
    unsigned time;
    double x;
    double y;
    double *axes;
    unsigned state;
    unsigned button;
} GdkEventButton;

static int press_fired;
static int press_type;
static unsigned press_button;
static unsigned press_state;
static void *press_menu;
static void (*press_popup)(void *, void *, int, int, const void *);

static int on_button_press(void *widget, GdkEventButton *event, void *data) {
    (void)data;
    press_fired = 1;
    if (event != NULL) {
        press_type = event->type;
        press_button = event->button;
        press_state = event->state;
        printf("BXINFO press type=%d button=%u state=0x%x\n", press_type,
               press_button, press_state);
        fflush(stdout);
        if (event->button == 1 && event->type == 4
                && (event->state & 4) == 0 && press_popup != NULL
                && press_menu != NULL) {
            press_popup(press_menu, widget, 7, 1, event);
            return 1;
        }
    }
    return 0;
}

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

static Window wm_frame = None;

static void drain_wm(Display *manager) {
    if (manager == NULL) return;
    XEvent event = {0};
    while (XPending(manager)) {
        XNextEvent(manager, &event);
        if (event.type == MapRequest) {
            Window client = event.xmaprequest.window;
            XWindowAttributes attributes = {0};
            if (XGetWindowAttributes(manager, client, &attributes)
                    && !attributes.override_redirect && wm_frame == None) {
                wm_frame = XCreateSimpleWindow(manager,
                                               DefaultRootWindow(manager),
                                               attributes.x, attributes.y,
                                               (unsigned)attributes.width + 8,
                                               (unsigned)attributes.height + 4,
                                               0, 0, 0x404040);
                XSelectInput(manager, wm_frame,
                             SubstructureRedirectMask
                                     | SubstructureNotifyMask
                                     | ButtonPressMask);
                XReparentWindow(manager, client, wm_frame, 4, 2);
                XMapWindow(manager, wm_frame);
            }
            XMapWindow(manager, client);
        } else if (event.type == ConfigureRequest) {
            XWindowChanges changes = {
                .x = event.xconfigurerequest.x,
                .y = event.xconfigurerequest.y,
                .width = event.xconfigurerequest.width,
                .height = event.xconfigurerequest.height,
                .border_width = event.xconfigurerequest.border_width,
                .sibling = event.xconfigurerequest.above,
                .stack_mode = event.xconfigurerequest.detail,
            };
            XConfigureWindow(manager, event.xconfigurerequest.window,
                             (unsigned)event.xconfigurerequest.value_mask,
                             &changes);
        }
    }
    XFlush(manager);
}

static void pump(int (*pending)(void), int (*iterate)(int), Display *manager,
                 int milliseconds) {
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        drain_wm(manager);
        while (pending()) iterate(0);
        drain_wm(manager);
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - start.tv_sec) * 1000
                + (now.tv_nsec - start.tv_nsec) / 1000000;
        if (elapsed >= milliseconds) break;
        struct timespec delay = {.tv_sec = 0, .tv_nsec = 10 * 1000 * 1000};
        nanosleep(&delay, NULL);
    }
}

static int largest_temp(void *(*gdk_display_get_default)(void),
                        void *(*gdk_display_get_default_screen)(void *),
                        void *(*gdk_screen_get_root_window)(void *),
                        ListNode *(*gdk_window_peek_children)(void *),
                        int (*gdk_window_get_window_type)(void *),
                        int (*gdk_window_is_visible)(void *),
                        int (*gdk_window_get_width)(void *),
                        int (*gdk_window_get_height)(void *),
                        int *count_out, int *height_out) {
    void *display = gdk_display_get_default();
    void *screen = display != NULL
            ? gdk_display_get_default_screen(display) : NULL;
    void *root = screen != NULL ? gdk_screen_get_root_window(screen) : NULL;
    int count = 0;
    int width = 0;
    int height = 0;
    if (root != NULL) {
        for (ListNode *node = gdk_window_peek_children(root); node != NULL;
                node = node->next) {
            void *child = node->data;
            if (child == NULL || !gdk_window_is_visible(child)) continue;
            if (gdk_window_get_window_type(child) != 3) continue;
            int child_w = gdk_window_get_width(child);
            int child_h = gdk_window_get_height(child);
            printf("BXINFO temp-window %dx%d\n", child_w, child_h);
            fflush(stdout);
            ++count;
            if (child_w * child_h > width * height) {
                width = child_w;
                height = child_h;
            }
        }
    }
    if (count_out != NULL) *count_out = count;
    if (height_out != NULL) *height_out = height;
    return width;
}

static unsigned long popup_interior_pixel(Display *display) {
    if (display == NULL) return 0;
    Window root = DefaultRootWindow(display);
    Window root_ret = None;
    Window parent = None;
    Window *children = NULL;
    unsigned count = 0;
    if (!XQueryTree(display, root, &root_ret, &parent, &children, &count))
        return 0;
    Window best = None;
    int best_area = 0;
    for (unsigned i = 0; i < count; i++) {
        XWindowAttributes attributes = {0};
        if (!XGetWindowAttributes(display, children[i], &attributes))
            continue;
        if (!attributes.override_redirect
                || attributes.map_state != IsViewable)
            continue;
        int area = attributes.width * attributes.height;
        if (area > best_area && attributes.width >= 40
                && attributes.height >= 16) {
            best = children[i];
            best_area = area;
        }
    }
    if (children != NULL) XFree(children);
    if (best == None) return 0;
    XImage *image = XGetImage(display, best, 8, 8, 1, 1, AllPlanes, ZPixmap);
    unsigned long pixel = image != NULL ? XGetPixel(image, 0, 0) : 0;
    if (image != NULL) XDestroyImage(image);
    return pixel;
}

int main(int argc, char **argv) {
    int passed = 0;
    int failed = 0;
#define RECORD(value) do { if (value) ++passed; else ++failed; } while (0)
    char detail[256];

    Display *manager = XOpenDisplay(NULL);
    if (manager != NULL) {
        Window root = DefaultRootWindow(manager);
        XSelectInput(manager, root,
                     SubstructureRedirectMask | SubstructureNotifyMask);
        XSync(manager, False);
    }

    void *gtk = dlopen("libgtk-3.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (gtk == NULL) {
        result("gtk-dlopen", 0, dlerror());
        return 1;
    }
    result("gtk-dlopen", 1, "libgtk-3.so.0");
    ++passed;

    int (*gtk_init_check)(int *, char ***) = NULL;
    void *(*gtk_window_new)(int) = NULL;
    void *(*gtk_box_new)(int, int) = NULL;
    void *(*gtk_label_new)(const char *) = NULL;
    void *(*gtk_menu_button_new)(void) = NULL;
    void (*gtk_menu_button_set_popup)(void *, void *) = NULL;
    void *(*gtk_button_new_with_label)(const char *) = NULL;
    unsigned long (*g_signal_connect_data)(void *, const char *, void *, void *,
                                           void *, int) = NULL;
    void (*gtk_container_add)(void *, void *) = NULL;
    void (*gtk_box_pack_start)(void *, void *, int, int, unsigned) = NULL;
    void (*gtk_window_set_default_size)(void *, int, int) = NULL;
    void (*gtk_widget_show_all)(void *) = NULL;
    int (*gtk_widget_get_mapped)(void *) = NULL;
    int (*gtk_widget_get_allocated_width)(void *) = NULL;
    int (*gtk_widget_get_allocated_height)(void *) = NULL;
    void *(*gtk_widget_get_window)(void *) = NULL;
    void *(*gtk_widget_get_toplevel)(void *) = NULL;
    int (*gtk_widget_translate_coordinates)(void *, void *, int, int, int *,
                                            int *) = NULL;
    void (*gdk_window_get_origin)(void *, int *, int *) = NULL;
    void *(*gtk_menu_new)(void) = NULL;
    void *(*gtk_menu_item_new_with_label)(const char *) = NULL;
    void (*gtk_menu_shell_append)(void *, void *) = NULL;
    void (*gtk_menu_popup_at_widget)(void *, void *, int, int, const void *) =
            NULL;
    void (*gtk_menu_popdown)(void *) = NULL;
    int (*gtk_events_pending)(void) = NULL;
    int (*gtk_main_iteration_do)(int) = NULL;
    void *(*gdk_display_get_default)(void) = NULL;
    void *(*gdk_display_get_default_screen)(void *) = NULL;
    void *(*gdk_screen_get_root_window)(void *) = NULL;
    ListNode *(*gdk_window_peek_children)(void *) = NULL;
    int (*gdk_window_get_window_type)(void *) = NULL;
    int (*gdk_window_is_visible)(void *) = NULL;
    int (*gdk_window_get_width)(void *) = NULL;
    int (*gdk_window_get_height)(void *) = NULL;
    *(void **)(&gtk_init_check) = required_symbol(gtk, "gtk_init_check");
    *(void **)(&gtk_window_new) = required_symbol(gtk, "gtk_window_new");
    *(void **)(&gtk_box_new) = required_symbol(gtk, "gtk_box_new");
    *(void **)(&gtk_label_new) = required_symbol(gtk, "gtk_label_new");
    *(void **)(&gtk_menu_button_new) =
            required_symbol(gtk, "gtk_menu_button_new");
    *(void **)(&gtk_menu_button_set_popup) =
            required_symbol(gtk, "gtk_menu_button_set_popup");
    *(void **)(&gtk_button_new_with_label) =
            required_symbol(gtk, "gtk_button_new_with_label");
    void *gobject = dlopen("libgobject-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (gobject == NULL) {
        result("gtk-gobject", 0, dlerror());
        return 1;
    }
    *(void **)(&g_signal_connect_data) =
            required_symbol(gobject, "g_signal_connect_data");
    *(void **)(&gtk_container_add) = required_symbol(gtk, "gtk_container_add");
    *(void **)(&gtk_box_pack_start) = required_symbol(gtk, "gtk_box_pack_start");
    *(void **)(&gtk_window_set_default_size) =
            required_symbol(gtk, "gtk_window_set_default_size");
    *(void **)(&gtk_widget_show_all) = required_symbol(gtk, "gtk_widget_show_all");
    *(void **)(&gtk_widget_get_mapped) =
            required_symbol(gtk, "gtk_widget_get_mapped");
    *(void **)(&gtk_widget_get_allocated_width) =
            required_symbol(gtk, "gtk_widget_get_allocated_width");
    *(void **)(&gtk_widget_get_allocated_height) =
            required_symbol(gtk, "gtk_widget_get_allocated_height");
    *(void **)(&gtk_widget_get_window) =
            required_symbol(gtk, "gtk_widget_get_window");
    *(void **)(&gtk_widget_get_toplevel) =
            required_symbol(gtk, "gtk_widget_get_toplevel");
    *(void **)(&gtk_widget_translate_coordinates) =
            required_symbol(gtk, "gtk_widget_translate_coordinates");
    *(void **)(&gdk_window_get_origin) =
            required_symbol(gtk, "gdk_window_get_origin");
    *(void **)(&gtk_menu_new) = required_symbol(gtk, "gtk_menu_new");
    *(void **)(&gtk_menu_item_new_with_label) =
            required_symbol(gtk, "gtk_menu_item_new_with_label");
    *(void **)(&gtk_menu_shell_append) =
            required_symbol(gtk, "gtk_menu_shell_append");
    *(void **)(&gtk_menu_popup_at_widget) =
            required_symbol(gtk, "gtk_menu_popup_at_widget");
    *(void **)(&gtk_menu_popdown) = required_symbol(gtk, "gtk_menu_popdown");
    *(void **)(&gtk_events_pending) = required_symbol(gtk, "gtk_events_pending");
    *(void **)(&gtk_main_iteration_do) =
            required_symbol(gtk, "gtk_main_iteration_do");
    *(void **)(&gdk_display_get_default) =
            required_symbol(gtk, "gdk_display_get_default");
    *(void **)(&gdk_display_get_default_screen) =
            required_symbol(gtk, "gdk_display_get_default_screen");
    *(void **)(&gdk_screen_get_root_window) =
            required_symbol(gtk, "gdk_screen_get_root_window");
    *(void **)(&gdk_window_peek_children) =
            required_symbol(gtk, "gdk_window_peek_children");
    *(void **)(&gdk_window_get_window_type) =
            required_symbol(gtk, "gdk_window_get_window_type");
    *(void **)(&gdk_window_is_visible) =
            required_symbol(gtk, "gdk_window_is_visible");
    *(void **)(&gdk_window_get_width) =
            required_symbol(gtk, "gdk_window_get_width");
    *(void **)(&gdk_window_get_height) =
            required_symbol(gtk, "gdk_window_get_height");

    int init_ok = gtk_init_check(&argc, &argv) != 0;
    result("gtk-init", init_ok, init_ok ? "DISPLAY connected" : "failed");
    RECORD(init_ok);
    if (!init_ok) {
        printf("BXSUMMARY gtk-menu passed=%d failed=%d\n", passed, failed);
        return 1;
    }

    void *window = gtk_window_new(0);
    void *box = gtk_box_new(1, 8);
    void *label = gtk_label_new("BionicX menu host");
    void *button = gtk_menu_button_new();
    void *click_menu = gtk_menu_new();
    void *click_item = gtk_menu_item_new_with_label("Open Terminal");
    void *press_button_w = gtk_button_new_with_label("Applications");
    void *press_item = gtk_menu_item_new_with_label("Open Terminal");
    press_menu = gtk_menu_new();
    press_popup = gtk_menu_popup_at_widget;
    if (window != NULL && box != NULL) {
        gtk_window_set_default_size(window, 320, 200);
        gtk_container_add(window, box);
        if (label != NULL) gtk_box_pack_start(box, label, 1, 1, 0);
        if (button != NULL && click_menu != NULL && click_item != NULL) {
            gtk_menu_shell_append(click_menu, click_item);
            gtk_widget_show_all(click_menu);
            gtk_menu_button_set_popup(button, click_menu);
            gtk_box_pack_start(box, button, 0, 0, 0);
        }
        if (press_button_w != NULL && press_menu != NULL && press_item != NULL) {
            gtk_menu_shell_append(press_menu, press_item);
            gtk_widget_show_all(press_menu);
            g_signal_connect_data(press_button_w, "button-press-event",
                                  (void *)on_button_press, NULL, NULL, 0);
            gtk_box_pack_start(box, press_button_w, 0, 0, 0);
        }
        gtk_widget_show_all(window);
    }
    pump(gtk_events_pending, gtk_main_iteration_do, manager, 600);
    int mapped = window != NULL && gtk_widget_get_mapped(window);
    result("gtk-window", mapped,
           mapped ? (manager != NULL ? "mapped under redirect" : "mapped")
                  : "not mapped");
    RECORD(mapped);
    if (!mapped) {
        printf("BXSUMMARY gtk-menu passed=%d failed=%d\n", passed, failed);
        return 1;
    }

    void *menu = gtk_menu_new();
    void *item = gtk_menu_item_new_with_label("Open Terminal");
    int menu_built = menu != NULL && item != NULL;
    if (menu_built) {
        gtk_menu_shell_append(menu, item);
        gtk_widget_show_all(menu);
        gtk_menu_popup_at_widget(menu, window, 7, 1, NULL);
    }
    result("gtk-menu-build", menu_built,
           menu_built ? "popup_at_widget" : "NULL");
    RECORD(menu_built);

    pump(gtk_events_pending, gtk_main_iteration_do, manager, 800);
    int alloc_w = menu != NULL ? gtk_widget_get_allocated_width(menu) : 0;
    int alloc_h = menu != NULL ? gtk_widget_get_allocated_height(menu) : 0;
    int temp_count = 0;
    int temp_height = 0;
    int temp_width = largest_temp(gdk_display_get_default,
                                  gdk_display_get_default_screen,
                                  gdk_screen_get_root_window,
                                  gdk_window_peek_children,
                                  gdk_window_get_window_type,
                                  gdk_window_is_visible, gdk_window_get_width,
                                  gdk_window_get_height, &temp_count,
                                  &temp_height);
    int sized = alloc_w >= 40 && alloc_h >= 16;
    int popup_ok = temp_count > 0 && temp_width >= 40 && temp_height >= 16;
    snprintf(detail, sizeof(detail), "alloc=%dx%d temp=%d %dx%d", alloc_w,
             alloc_h, temp_count, temp_width, temp_height);
    result("gtk-menu-sized", sized, detail);
    RECORD(sized);
    result("gtk-menu-mapped", popup_ok, detail);
    RECORD(popup_ok);

    unsigned long menu_pixel = popup_interior_pixel(manager);
    int menu_red = (int)((menu_pixel >> 16) & 0xff);
    int menu_green = (int)((menu_pixel >> 8) & 0xff);
    int menu_blue = (int)(menu_pixel & 0xff);
    int painted = menu_red + menu_green + menu_blue >= 0x180;
    snprintf(detail, sizeof(detail), "pixel=0x%06lx", menu_pixel & 0xffffff);
    result("gtk-menu-paint", painted, detail);
    RECORD(painted);

    if (menu != NULL) gtk_menu_popdown(menu);
    pump(gtk_events_pending, gtk_main_iteration_do, manager, 200);

    int click_ok = 0;
    if (button != NULL && gtk_widget_get_window(button) != NULL) {
        int local_x = 0;
        int local_y = 0;
        void *toplevel = gtk_widget_get_toplevel(button);
        gtk_widget_translate_coordinates(button, toplevel,
                                         gtk_widget_get_allocated_width(button)
                                                 / 2,
                                         gtk_widget_get_allocated_height(button)
                                                 / 2,
                                         &local_x, &local_y);
        int origin_x = 0;
        int origin_y = 0;
        gdk_window_get_origin(gtk_widget_get_window(toplevel), &origin_x,
                              &origin_y);
        int root_x = origin_x + local_x;
        int root_y = origin_y + local_y;
        Display *xtest = XOpenDisplay(NULL);
        int event_base = 0;
        int error_base = 0;
        int major = 0;
        int minor = 0;
        if (xtest != NULL && XTestQueryExtension(xtest, &event_base,
                                                 &error_base, &major, &minor)) {
            XTestFakeMotionEvent(xtest, DefaultScreen(xtest), root_x, root_y,
                                 0);
            XTestFakeButtonEvent(xtest, 1, True, 20);
            XTestFakeButtonEvent(xtest, 1, False, 20);
            XSync(xtest, False);
            pump(gtk_events_pending, gtk_main_iteration_do, manager, 800);
            int click_count = 0;
            int click_height = 0;
            int click_width = largest_temp(gdk_display_get_default,
                                           gdk_display_get_default_screen,
                                           gdk_screen_get_root_window,
                                           gdk_window_peek_children,
                                           gdk_window_get_window_type,
                                           gdk_window_is_visible,
                                           gdk_window_get_width,
                                           gdk_window_get_height, &click_count,
                                           &click_height);
            click_ok = click_count > 0 && click_width >= 40
                    && click_height >= 16;
            snprintf(detail, sizeof(detail), "click=%d,%d temp=%d %dx%d",
                     root_x, root_y, click_count, click_width, click_height);
        } else {
            snprintf(detail, sizeof(detail), "XTEST unavailable");
        }
        if (xtest != NULL) XCloseDisplay(xtest);
    } else {
        snprintf(detail, sizeof(detail), "menu button unrealized");
    }
    result("gtk-menu-click", click_ok, detail);
    RECORD(click_ok);

    if (click_menu != NULL) gtk_menu_popdown(click_menu);
    pump(gtk_events_pending, gtk_main_iteration_do, manager, 200);

    int press_ok = 0;
    if (press_button_w != NULL && gtk_widget_get_window(press_button_w) != NULL) {
        int local_x = 0;
        int local_y = 0;
        void *toplevel = gtk_widget_get_toplevel(press_button_w);
        gtk_widget_translate_coordinates(press_button_w, toplevel,
                                         gtk_widget_get_allocated_width(
                                                 press_button_w) / 2,
                                         gtk_widget_get_allocated_height(
                                                 press_button_w) / 2,
                                         &local_x, &local_y);
        int origin_x = 0;
        int origin_y = 0;
        gdk_window_get_origin(gtk_widget_get_window(toplevel), &origin_x,
                              &origin_y);
        int root_x = origin_x + local_x;
        int root_y = origin_y + local_y;
        Display *xtest = XOpenDisplay(NULL);
        int event_base = 0;
        int error_base = 0;
        int major = 0;
        int minor = 0;
        if (xtest != NULL && XTestQueryExtension(xtest, &event_base,
                                                 &error_base, &major, &minor)) {
            XTestFakeMotionEvent(xtest, DefaultScreen(xtest), root_x, root_y,
                                 0);
            XTestFakeButtonEvent(xtest, 1, True, 20);
            XTestFakeButtonEvent(xtest, 1, False, 20);
            XSync(xtest, False);
            pump(gtk_events_pending, gtk_main_iteration_do, manager, 800);
            int press_count = 0;
            int press_height = 0;
            int press_width = largest_temp(gdk_display_get_default,
                                           gdk_display_get_default_screen,
                                           gdk_screen_get_root_window,
                                           gdk_window_peek_children,
                                           gdk_window_get_window_type,
                                           gdk_window_is_visible,
                                           gdk_window_get_width,
                                           gdk_window_get_height, &press_count,
                                           &press_height);
            press_ok = press_fired && press_button == 1 && press_type == 4
                    && (press_state & 4) == 0 && press_count > 0
                    && press_width >= 40 && press_height >= 16;
            snprintf(detail, sizeof(detail),
                     "fired=%d type=%d button=%u state=0x%x temp=%d %dx%d",
                     press_fired, press_type, press_button, press_state,
                     press_count, press_width, press_height);
        } else {
            snprintf(detail, sizeof(detail), "XTEST unavailable");
        }
        if (xtest != NULL) XCloseDisplay(xtest);
    } else {
        snprintf(detail, sizeof(detail), "press button unrealized");
    }
    result("gtk-press-menu", press_ok, detail);
    RECORD(press_ok);

    if (manager != NULL) XCloseDisplay(manager);
    printf("BXSUMMARY gtk-menu passed=%d failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
