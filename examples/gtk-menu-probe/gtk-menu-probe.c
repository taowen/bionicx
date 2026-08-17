#define _POSIX_C_SOURCE 200809L

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct ListNode {
    void *data;
    struct ListNode *next;
} ListNode;

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

int main(int argc, char **argv) {
    int passed = 0;
    int failed = 0;
#define RECORD(value) do { if (value) ++passed; else ++failed; } while (0)
    char detail[256];

    void *gtk = dlopen("libgtk-3.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (gtk == NULL) {
        result("gtk-dlopen", 0, dlerror());
        return 1;
    }
    result("gtk-dlopen", 1, "libgtk-3.so.0");
    ++passed;

    int (*gtk_init_check)(int *, char ***) = NULL;
    void *(*gtk_window_new)(int) = NULL;
    void *(*gtk_label_new)(const char *) = NULL;
    void (*gtk_container_add)(void *, void *) = NULL;
    void (*gtk_window_set_default_size)(void *, int, int) = NULL;
    void (*gtk_widget_show_all)(void *) = NULL;
    int (*gtk_widget_get_mapped)(void *) = NULL;
    int (*gtk_widget_get_allocated_width)(void *) = NULL;
    int (*gtk_widget_get_allocated_height)(void *) = NULL;
    void *(*gtk_menu_new)(void) = NULL;
    void *(*gtk_menu_item_new_with_label)(const char *) = NULL;
    void (*gtk_menu_shell_append)(void *, void *) = NULL;
    void (*gtk_menu_popup_at_widget)(void *, void *, int, int, const void *) =
            NULL;
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
    *(void **)(&gtk_label_new) = required_symbol(gtk, "gtk_label_new");
    *(void **)(&gtk_container_add) = required_symbol(gtk, "gtk_container_add");
    *(void **)(&gtk_window_set_default_size) =
            required_symbol(gtk, "gtk_window_set_default_size");
    *(void **)(&gtk_widget_show_all) = required_symbol(gtk, "gtk_widget_show_all");
    *(void **)(&gtk_widget_get_mapped) =
            required_symbol(gtk, "gtk_widget_get_mapped");
    *(void **)(&gtk_widget_get_allocated_width) =
            required_symbol(gtk, "gtk_widget_get_allocated_width");
    *(void **)(&gtk_widget_get_allocated_height) =
            required_symbol(gtk, "gtk_widget_get_allocated_height");
    *(void **)(&gtk_menu_new) = required_symbol(gtk, "gtk_menu_new");
    *(void **)(&gtk_menu_item_new_with_label) =
            required_symbol(gtk, "gtk_menu_item_new_with_label");
    *(void **)(&gtk_menu_shell_append) =
            required_symbol(gtk, "gtk_menu_shell_append");
    *(void **)(&gtk_menu_popup_at_widget) =
            required_symbol(gtk, "gtk_menu_popup_at_widget");
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
    void *label = gtk_label_new("BionicX menu host");
    if (window != NULL) {
        gtk_window_set_default_size(window, 320, 200);
        if (label != NULL) gtk_container_add(window, label);
        gtk_widget_show_all(window);
    }
    pump(gtk_events_pending, gtk_main_iteration_do, 400);
    int mapped = window != NULL && gtk_widget_get_mapped(window);
    result("gtk-window", mapped, mapped ? "mapped" : "not mapped");
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
        /* GDK_GRAVITY_SOUTH_WEST / NORTH_WEST; NULL trigger is legal. */
        gtk_menu_popup_at_widget(menu, window, 7, 1, NULL);
    }
    result("gtk-menu-build", menu_built,
           menu_built ? "popup_at_widget" : "NULL");
    RECORD(menu_built);

    pump(gtk_events_pending, gtk_main_iteration_do, 800);
    int alloc_w = menu != NULL ? gtk_widget_get_allocated_width(menu) : 0;
    int alloc_h = menu != NULL ? gtk_widget_get_allocated_height(menu) : 0;
    void *display = gdk_display_get_default();
    void *screen = display != NULL
            ? gdk_display_get_default_screen(display) : NULL;
    void *root = screen != NULL ? gdk_screen_get_root_window(screen) : NULL;
    int temp_count = 0;
    int temp_width = 0;
    int temp_height = 0;
    if (root != NULL) {
        for (ListNode *node = gdk_window_peek_children(root); node != NULL;
                node = node->next) {
            void *child = node->data;
            if (child == NULL || !gdk_window_is_visible(child)) continue;
            /* GDK_WINDOW_TEMP: menus and popovers. */
            if (gdk_window_get_window_type(child) != 3) continue;
            int child_w = gdk_window_get_width(child);
            int child_h = gdk_window_get_height(child);
            printf("BXINFO temp-window %dx%d\n", child_w, child_h);
            fflush(stdout);
            ++temp_count;
            if (child_w * child_h > temp_width * temp_height) {
                temp_width = child_w;
                temp_height = child_h;
            }
        }
    }
    int sized = alloc_w >= 40 && alloc_h >= 16;
    int popup_ok = temp_count > 0 && temp_width >= 40 && temp_height >= 16;
    snprintf(detail, sizeof(detail), "alloc=%dx%d temp=%d %dx%d", alloc_w,
             alloc_h, temp_count, temp_width, temp_height);
    result("gtk-menu-sized", sized, detail);
    RECORD(sized);
    result("gtk-menu-mapped", popup_ok, detail);
    RECORD(popup_ok);

    printf("BXSUMMARY gtk-menu passed=%d failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
