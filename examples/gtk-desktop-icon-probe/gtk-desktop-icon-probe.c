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

static int pixbuf_has_colored(void *image,
                              int (*get_width)(void *),
                              int (*get_height)(void *),
                              unsigned char *(*get_pixels)(void *),
                              int (*get_rowstride)(void *),
                              int (*get_n_channels)(void *),
                              int want_blue, int want_cyan) {
    if (image == NULL || get_width == NULL || get_pixels == NULL)
        return 0;
    int width = get_width(image);
    int height = get_height(image);
    int stride = get_rowstride(image);
    int channels = get_n_channels(image);
    unsigned char *pixels = get_pixels(image);
    if (width <= 0 || height <= 0 || stride <= 0 || channels < 3
            || pixels == NULL)
        return 0;
    for (int y = 0; y < height; ++y) {
        unsigned char *row = pixels + (size_t)y * (size_t)stride;
        for (int x = 0; x < width; ++x) {
            int r = row[x * channels];
            int g = row[x * channels + 1];
            int b = row[x * channels + 2];
            int a = channels > 3 ? row[x * channels + 3] : 255;
            if (a < 128) continue;
            if (want_blue && b > 120 && b > r + 30) return 1;
            if (want_cyan && r < 90 && g > 100 && b > 160) return 1;
        }
    }
    return 0;
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
    void (*gtk_icon_theme_set_custom_theme)(void *, const char *) = NULL;
    void *(*gtk_settings_get_default)(void) = NULL;
    void (*g_object_set)(void *, const char *, ...) = NULL;
    int (*gdk_pixbuf_get_width)(void *) = NULL;
    int (*gdk_pixbuf_get_height)(void *) = NULL;
    unsigned char *(*gdk_pixbuf_get_pixels)(void *) = NULL;
    int (*gdk_pixbuf_get_rowstride)(void *) = NULL;
    int (*gdk_pixbuf_get_n_channels)(void *) = NULL;
    void (*g_object_unref)(void *) = NULL;
    void (*g_error_free)(GlibError *) = NULL;
    *(void **)(&gtk_init_check) = required_symbol(gtk, "gtk_init_check");
    *(void **)(&gtk_icon_theme_get_default) =
            required_symbol(gtk, "gtk_icon_theme_get_default");
    *(void **)(&gtk_icon_theme_has_icon) =
            required_symbol(gtk, "gtk_icon_theme_has_icon");
    *(void **)(&gtk_icon_theme_load_icon) =
            required_symbol(gtk, "gtk_icon_theme_load_icon");
    *(void **)(&gtk_icon_theme_set_custom_theme) =
            required_symbol(gtk, "gtk_icon_theme_set_custom_theme");
    *(void **)(&gtk_settings_get_default) =
            required_symbol(gtk, "gtk_settings_get_default");

    void *pixbuf = dlopen("libgdk_pixbuf-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    void *gobject = dlopen("libgobject-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    void *glib = dlopen("libglib-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (pixbuf != NULL) {
        *(void **)(&gdk_pixbuf_get_width) =
                required_symbol(pixbuf, "gdk_pixbuf_get_width");
        *(void **)(&gdk_pixbuf_get_height) =
                required_symbol(pixbuf, "gdk_pixbuf_get_height");
        *(void **)(&gdk_pixbuf_get_pixels) =
                required_symbol(pixbuf, "gdk_pixbuf_get_pixels");
        *(void **)(&gdk_pixbuf_get_rowstride) =
                required_symbol(pixbuf, "gdk_pixbuf_get_rowstride");
        *(void **)(&gdk_pixbuf_get_n_channels) =
                required_symbol(pixbuf, "gdk_pixbuf_get_n_channels");
    }
    if (gobject != NULL) {
        *(void **)(&g_object_unref) = required_symbol(gobject, "g_object_unref");
        *(void **)(&g_object_set) = required_symbol(gobject, "g_object_set");
    }
    if (glib != NULL)
        *(void **)(&g_error_free) = required_symbol(glib, "g_error_free");

    int init_ok = gtk_init_check(&argc, &argv) != 0;
    result("gtk-init", init_ok, init_ok ? "DISPLAY connected" : "failed");
    RECORD(init_ok);
    if (!init_ok) {
        printf("BXSUMMARY gtk-desktop-icon passed=%d failed=%d\n", passed,
               failed);
        return 1;
    }

    void *theme = gtk_icon_theme_get_default();
    int theme_ok = theme != NULL;
    result("icon-theme-default", theme_ok,
           theme_ok ? "GtkIconTheme" : "NULL");
    RECORD(theme_ok);
    if (!theme_ok) {
        printf("BXSUMMARY gtk-desktop-icon passed=%d failed=%d\n", passed,
               failed);
        return 1;
    }

    void *settings = gtk_settings_get_default();
    if (settings != NULL && g_object_set != NULL)
        g_object_set(settings, "gtk-icon-theme-name", "Adwaita", NULL);
    gtk_icon_theme_set_custom_theme(theme, "Adwaita");
    GlibError *error = NULL;
    void *image = NULL;

    struct {
        const char *name;
        int want_blue;
        int want_cyan;
    } icons[] = {
        {"user-home", 1, 0},
        {"drive-harddisk", 0, 0},
        {"org.xfce.panel.applicationsmenu", 0, 1},
    };
    for (size_t i = 0; i < sizeof(icons) / sizeof(icons[0]); ++i) {
        int present = gtk_icon_theme_has_icon(theme, icons[i].name);
        image = gtk_icon_theme_load_icon(theme, icons[i].name, 48, 0, &error);
        int loaded = image != NULL && gdk_pixbuf_get_width != NULL &&
                gdk_pixbuf_get_width(image) > 0 &&
                gdk_pixbuf_get_height(image) > 0;
        int colored = 1;
        if (loaded && (icons[i].want_blue || icons[i].want_cyan)) {
            colored = pixbuf_has_colored(image, gdk_pixbuf_get_width,
                                         gdk_pixbuf_get_height,
                                         gdk_pixbuf_get_pixels,
                                         gdk_pixbuf_get_rowstride,
                                         gdk_pixbuf_get_n_channels,
                                         icons[i].want_blue,
                                         icons[i].want_cyan);
        }
        int ok = loaded && colored;
        if (ok) {
            snprintf(detail, sizeof(detail), "has=%d %dx%d", present,
                     gdk_pixbuf_get_width(image),
                     gdk_pixbuf_get_height(image));
        } else if (error != NULL && error->message != NULL) {
            snprintf(detail, sizeof(detail), "has=%d %s", present,
                     error->message);
        } else if (loaded) {
            snprintf(detail, sizeof(detail), "has=%d %dx%d wrong colors",
                     present, gdk_pixbuf_get_width(image),
                     gdk_pixbuf_get_height(image));
        } else {
            snprintf(detail, sizeof(detail), "has=%d pixbuf NULL", present);
        }
        result(icons[i].name, ok, detail);
        RECORD(ok);
        if (image != NULL && g_object_unref != NULL) g_object_unref(image);
        if (error != NULL && g_error_free != NULL) {
            g_error_free(error);
            error = NULL;
        }
    }

    printf("BXSUMMARY gtk-desktop-icon passed=%d failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
