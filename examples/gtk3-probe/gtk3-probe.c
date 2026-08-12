#define _POSIX_C_SOURCE 200809L

#include <dlfcn.h>
#include <gnu/libc-version.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef void GtkWidget;
typedef void GtkWindow;
typedef void GtkDialog;
typedef void GtkContainer;
typedef void GdkPixbufLoader;
typedef void PangoFontMap;
typedef void PangoContext;
typedef void PangoFontDescription;
typedef void PangoFont;
typedef struct ListNode {
    void *data;
    struct ListNode *next;
    struct ListNode *previous;
} ListNode;
typedef struct {
    unsigned domain;
    int code;
    char *message;
} GlibError;

static void result(const char *name, bool passed, const char *detail) {
    printf("BXTEST %s %s%s%s\n", passed ? "PASS" : "FAIL", name,
           detail != NULL && *detail != '\0' ? " " : "",
           detail != NULL ? detail : "");
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

static unsigned char *read_file(const char *path, size_t *size_out) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    unsigned char *contents = malloc((size_t)length);
    if (contents == NULL || fread(contents, 1, (size_t)length, file)
            != (size_t)length) {
        free(contents);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *size_out = (size_t)length;
    return contents;
}

#define LOAD(function) \
    do { *(void **)(&(function)) = required_symbol(gtk, #function); } while (0)

int main(int argc, char **argv) {
    int passed = 0;
    int failed = 0;
#define RECORD(value) do { if (value) ++passed; else ++failed; } while (0)

    void *gtk = dlopen("libgtk-3.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (gtk == NULL) {
        result("gtk-dlopen", false, dlerror());
        return 1;
    }
    char detail[256];
    snprintf(detail, sizeof(detail), "glibc=%s", gnu_get_libc_version());
    result("gtk-dlopen", true, detail);
    ++passed;

    const char *(*gtk_check_version)(unsigned, unsigned, unsigned) = NULL;
    int (*gtk_init_check)(int *, char ***) = NULL;
    GtkWidget *(*gtk_window_new)(int) = NULL;
    GtkWidget *(*gtk_label_new)(const char *) = NULL;
    void (*gtk_container_add)(GtkContainer *, GtkWidget *) = NULL;
    void (*gtk_window_set_title)(GtkWindow *, const char *) = NULL;
    void (*gtk_window_set_default_size)(GtkWindow *, int, int) = NULL;
    GtkWidget *(*gtk_file_chooser_dialog_new)(const char *, GtkWindow *, int,
                                               const char *, ...) = NULL;
    GtkWidget *(*gtk_dialog_add_button)(GtkDialog *, const char *, int) = NULL;
    void (*gtk_widget_show_all)(GtkWidget *) = NULL;
    void (*gtk_widget_destroy)(GtkWidget *) = NULL;
    int (*gtk_events_pending)(void) = NULL;
    int (*gtk_main_iteration_do)(int) = NULL;
    ListNode *(*gdk_pixbuf_get_formats)(void) = NULL;
    char *(*gdk_pixbuf_format_get_name)(void *) = NULL;
    void (*g_list_free)(ListNode *) = NULL;
    void *(*gdk_pixbuf_new_from_file)(const char *, GlibError **) = NULL;
    GdkPixbufLoader *(*gdk_pixbuf_loader_new_with_type)(
            const char *, GlibError **) = NULL;
    int (*gdk_pixbuf_loader_write)(GdkPixbufLoader *, const unsigned char *,
                                   size_t, GlibError **) = NULL;
    int (*gdk_pixbuf_loader_close)(GdkPixbufLoader *, GlibError **) = NULL;
    void (*g_object_unref)(void *) = NULL;
    void (*g_error_free)(GlibError *) = NULL;
    PangoFontMap *(*pango_cairo_font_map_get_default)(void) = NULL;
    PangoContext *(*pango_font_map_create_context)(PangoFontMap *) = NULL;
    PangoFontDescription *(*pango_font_description_from_string)(
            const char *) = NULL;
    void (*pango_font_description_free)(PangoFontDescription *) = NULL;
    PangoFont *(*pango_font_map_load_font)(PangoFontMap *, PangoContext *,
                                           const PangoFontDescription *) = NULL;
    PangoFontDescription *(*pango_font_describe)(PangoFont *) = NULL;
    char *(*pango_font_description_to_string)(
            const PangoFontDescription *) = NULL;
    void (*g_free)(void *) = NULL;

    LOAD(gtk_check_version);
    LOAD(gtk_init_check);
    LOAD(gtk_window_new);
    LOAD(gtk_label_new);
    LOAD(gtk_container_add);
    LOAD(gtk_window_set_title);
    LOAD(gtk_window_set_default_size);
    LOAD(gtk_file_chooser_dialog_new);
    LOAD(gtk_dialog_add_button);
    LOAD(gtk_widget_show_all);
    LOAD(gtk_widget_destroy);
    LOAD(gtk_events_pending);
    LOAD(gtk_main_iteration_do);
    LOAD(gdk_pixbuf_get_formats);
    LOAD(gdk_pixbuf_format_get_name);
    LOAD(g_list_free);
    LOAD(gdk_pixbuf_new_from_file);
    LOAD(gdk_pixbuf_loader_new_with_type);
    LOAD(gdk_pixbuf_loader_write);
    LOAD(gdk_pixbuf_loader_close);
    LOAD(g_object_unref);
    LOAD(g_error_free);
    LOAD(pango_cairo_font_map_get_default);
    LOAD(pango_font_map_create_context);
    LOAD(pango_font_description_from_string);
    LOAD(pango_font_description_free);
    LOAD(pango_font_map_load_font);
    LOAD(pango_font_describe);
    LOAD(pango_font_description_to_string);
    LOAD(g_free);

    const char *version_error = gtk_check_version(3, 20, 0);
    bool version_ok = version_error == NULL;
    result("gtk-version", version_ok,
           version_ok ? ">=3.20" : version_error);
    RECORD(version_ok);

    bool init_ok = gtk_init_check(&argc, &argv) != 0;
    result("gtk-init", init_ok, init_ok ? "DISPLAY connected" : "failed");
    RECORD(init_ok);
    if (!init_ok) {
        dlclose(gtk);
        printf("BXSUMMARY gtk3 passed=%d/10 failed=%d\n", passed, failed + 7);
        return 1;
    }

    PangoFontMap *font_map = pango_cairo_font_map_get_default();
    PangoContext *font_context = font_map != NULL
            ? pango_font_map_create_context(font_map) : NULL;
    PangoFontDescription *requested_font =
            pango_font_description_from_string("sans-serif 14");
    PangoFont *font = font_map != NULL && font_context != NULL
            && requested_font != NULL
            ? pango_font_map_load_font(font_map, font_context, requested_font)
            : NULL;
    PangoFontDescription *matched_font = font != NULL
            ? pango_font_describe(font) : NULL;
    char *matched_font_name = matched_font != NULL
            ? pango_font_description_to_string(matched_font) : NULL;
    bool font_ok = font != NULL && matched_font_name != NULL;
    result("pango-font", font_ok,
           font_ok ? matched_font_name : "sans-serif unavailable");
    RECORD(font_ok);
    if (matched_font_name != NULL) g_free(matched_font_name);
    if (matched_font != NULL) pango_font_description_free(matched_font);
    if (font != NULL) g_object_unref(font);
    if (requested_font != NULL) pango_font_description_free(requested_font);
    if (font_context != NULL) g_object_unref(font_context);

    ListNode *formats = gdk_pixbuf_get_formats();
    int format_count = 0;
    bool have_png = false;
    for (ListNode *item = formats; item != NULL; item = item->next) {
        char *name = gdk_pixbuf_format_get_name(item->data);
        printf("BXINFO pixbuf-format %s\n", name != NULL ? name : "-");
        if (name != NULL && strcmp(name, "png") == 0) have_png = true;
        format_count++;
    }
    g_list_free(formats);
    snprintf(detail, sizeof(detail), "count=%d png=%d", format_count,
             have_png);
    result("pixbuf-formats", have_png, detail);
    RECORD(have_png);

    const char *test_png = getenv("BIONICX_TEST_PNG");
    GlibError *pixbuf_error = NULL;
    void *pixbuf = test_png != NULL
            ? gdk_pixbuf_new_from_file(test_png, &pixbuf_error) : NULL;
    bool png_ok = pixbuf != NULL;
    result("pixbuf-png", png_ok,
           png_ok ? test_png : (pixbuf_error != NULL
                   ? pixbuf_error->message : "BIONICX_TEST_PNG unset"));
    RECORD(png_ok);
    if (pixbuf != NULL) g_object_unref(pixbuf);
    if (pixbuf_error != NULL) g_error_free(pixbuf_error);

    size_t png_size = 0;
    unsigned char *png_bytes = test_png != NULL
            ? read_file(test_png, &png_size) : NULL;
    GlibError *loader_error = NULL;
    GdkPixbufLoader *loader = png_bytes != NULL
            ? gdk_pixbuf_loader_new_with_type("png", &loader_error) : NULL;
    bool explicit_png_ok = loader != NULL
            && gdk_pixbuf_loader_write(loader, png_bytes, png_size,
                                       &loader_error) != 0
            && gdk_pixbuf_loader_close(loader, &loader_error) != 0;
    result("pixbuf-explicit-png", explicit_png_ok,
           explicit_png_ok ? "type=png"
                   : (loader_error != NULL ? loader_error->message
                                           : "loader unavailable"));
    RECORD(explicit_png_ok);
    if (loader != NULL) g_object_unref(loader);
    if (loader_error != NULL) g_error_free(loader_error);
    free(png_bytes);

    GtkWidget *window = gtk_window_new(0);
    GtkWidget *label = window != NULL
            ? gtk_label_new("BionicX GTK3 text rendering") : NULL;
    bool window_ok = window != NULL;
    if (window_ok) {
        gtk_window_set_title((GtkWindow *)window, "BionicX GTK3 probe");
        gtk_window_set_default_size((GtkWindow *)window, 640, 360);
        if (label != NULL)
            gtk_container_add((GtkContainer *)window, label);
        gtk_widget_show_all(window);
    }
    result("gtk-window", window_ok, window_ok ? "640x360" : "null");
    RECORD(window_ok);
    bool label_ok = label != NULL;
    result("gtk-label", label_ok,
           label_ok ? "BionicX GTK3 text rendering" : "null");
    RECORD(label_ok);

    GtkWidget *dialog = gtk_file_chooser_dialog_new(
            "BionicX GTK Save", (GtkWindow *)window, 1, NULL);
    bool dialog_ok = dialog != NULL;
    if (dialog_ok) {
        gtk_dialog_add_button((GtkDialog *)dialog, "Cancel", -6);
        gtk_dialog_add_button((GtkDialog *)dialog, "Save", -3);
        gtk_widget_show_all(dialog);
    }
    result("gtk-file-chooser", dialog_ok,
           dialog_ok ? "GtkFileChooserDialog" : "null");
    RECORD(dialog_ok);

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        while (gtk_events_pending()) {
            gtk_main_iteration_do(0);
        }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - start.tv_sec >= 8) break;
        struct timespec delay = {.tv_sec = 0, .tv_nsec = 10 * 1000 * 1000};
        nanosleep(&delay, NULL);
    }

    if (dialog != NULL) gtk_widget_destroy(dialog);
    if (window != NULL) gtk_widget_destroy(window);
    while (gtk_events_pending()) gtk_main_iteration_do(0);
    dlclose(gtk);
    printf("BXSUMMARY gtk3 passed=%d/10 failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
