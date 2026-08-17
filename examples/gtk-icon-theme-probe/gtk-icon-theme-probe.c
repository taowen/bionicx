#define _POSIX_C_SOURCE 200809L

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned domain;
    int code;
    char *message;
} GlibError;

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
    void *(*gtk_icon_theme_get_default)(void) = NULL;
    int (*gtk_icon_theme_has_icon)(void *, const char *) = NULL;
    void *(*gtk_icon_theme_load_icon)(void *, const char *, int, int,
                                      GlibError **) = NULL;
    int (*gdk_pixbuf_get_width)(void *) = NULL;
    int (*gdk_pixbuf_get_height)(void *) = NULL;
    void (*g_object_unref)(void *) = NULL;
    void (*g_error_free)(GlibError *) = NULL;
    *(void **)(&gtk_init_check) = required_symbol(gtk, "gtk_init_check");
    *(void **)(&gtk_icon_theme_get_default) =
            required_symbol(gtk, "gtk_icon_theme_get_default");
    *(void **)(&gtk_icon_theme_has_icon) =
            required_symbol(gtk, "gtk_icon_theme_has_icon");
    *(void **)(&gtk_icon_theme_load_icon) =
            required_symbol(gtk, "gtk_icon_theme_load_icon");

    void *pixbuf = dlopen("libgdk_pixbuf-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    void *gobject = dlopen("libgobject-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    void *glib = dlopen("libglib-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (pixbuf != NULL) {
        *(void **)(&gdk_pixbuf_get_width) =
                required_symbol(pixbuf, "gdk_pixbuf_get_width");
        *(void **)(&gdk_pixbuf_get_height) =
                required_symbol(pixbuf, "gdk_pixbuf_get_height");
    }
    if (gobject != NULL)
        *(void **)(&g_object_unref) = required_symbol(gobject, "g_object_unref");
    if (glib != NULL)
        *(void **)(&g_error_free) = required_symbol(glib, "g_error_free");

    int init_ok = gtk_init_check(&argc, &argv) != 0;
    result("gtk-init", init_ok, init_ok ? "DISPLAY connected" : "failed");
    RECORD(init_ok);
    if (!init_ok) {
        printf("BXSUMMARY gtk-icon-theme passed=%d failed=%d\n", passed,
               failed);
        return 1;
    }

    void *theme = gtk_icon_theme_get_default();
    int theme_ok = theme != NULL;
    result("icon-theme-default", theme_ok,
           theme_ok ? "GtkIconTheme" : "NULL");
    RECORD(theme_ok);
    if (!theme_ok) {
        printf("BXSUMMARY gtk-icon-theme passed=%d failed=%d\n", passed,
               failed);
        return 1;
    }

    static const char *names[] = {
        "go-previous-symbolic",
        "go-up-symbolic",
        "folder",
        "image-x-generic",
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        int present = gtk_icon_theme_has_icon(theme, names[i]);
        GlibError *error = NULL;
        void *image = gtk_icon_theme_load_icon(theme, names[i], 24, 0, &error);
        int loaded = image != NULL && gdk_pixbuf_get_width != NULL &&
                gdk_pixbuf_get_width(image) > 0 &&
                gdk_pixbuf_get_height(image) > 0;
        if (loaded) {
            snprintf(detail, sizeof(detail), "has=%d %dx%d", present,
                     gdk_pixbuf_get_width(image),
                     gdk_pixbuf_get_height(image));
        } else if (error != NULL && error->message != NULL) {
            snprintf(detail, sizeof(detail), "has=%d %s", present,
                     error->message);
        } else {
            snprintf(detail, sizeof(detail), "has=%d pixbuf NULL", present);
        }
        result(names[i], loaded, detail);
        RECORD(loaded);
        if (image != NULL && g_object_unref != NULL) g_object_unref(image);
        if (error != NULL && g_error_free != NULL) g_error_free(error);
    }

    printf("BXSUMMARY gtk-icon-theme passed=%d failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
