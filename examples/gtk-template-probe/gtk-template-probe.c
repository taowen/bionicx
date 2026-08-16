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

typedef struct {
    const char *name;
    const char *path;
} ResourceCase;

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

#define LOAD(function) \
    do { *(void **)(&(function)) = required_symbol(glib, #function); } while (0)

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

    void *glib = dlopen("libgio-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (glib == NULL) {
        result("gio-dlopen", 0, dlerror());
        return 1;
    }
    result("gio-dlopen", 1, "libgio-2.0.so.0");
    ++passed;

    int (*gtk_init_check)(int *, char ***) = NULL;
    void *(*gtk_statusbar_new)(void) = NULL;
    void *(*gtk_window_new)(int) = NULL;
    void (*gtk_container_add)(void *, void *) = NULL;
    void (*gtk_widget_show_all)(void *) = NULL;
    void (*gtk_widget_destroy)(void *) = NULL;
    void *(*g_resources_lookup_data)(const char *, int, GlibError **) = NULL;
    size_t (*g_bytes_get_size)(void *) = NULL;
    void (*g_bytes_unref)(void *) = NULL;
    void (*g_error_free)(GlibError *) = NULL;
    *(void **)(&gtk_init_check) = required_symbol(gtk, "gtk_init_check");
    *(void **)(&gtk_statusbar_new) = required_symbol(gtk, "gtk_statusbar_new");
    *(void **)(&gtk_window_new) = required_symbol(gtk, "gtk_window_new");
    *(void **)(&gtk_container_add) = required_symbol(gtk,
            "gtk_container_add");
    *(void **)(&gtk_widget_show_all) = required_symbol(gtk,
            "gtk_widget_show_all");
    *(void **)(&gtk_widget_destroy) = required_symbol(gtk,
            "gtk_widget_destroy");
    LOAD(g_resources_lookup_data);
    LOAD(g_bytes_get_size);
    LOAD(g_bytes_unref);
    LOAD(g_error_free);

    char *(*g_convert)(const char *, long, const char *, const char *,
            size_t *, size_t *, GlibError **) = NULL;
    void (*g_free)(void *) = NULL;
    void *libglib = dlopen("libglib-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (libglib != NULL) {
        *(void **)(&g_convert) = dlsym(libglib, "g_convert");
        *(void **)(&g_free) = dlsym(libglib, "g_free");
    }
    static const char latin1[] = { (char)0xC4, 'p', 'f', 'e', 'l', 0 };
    static const char utf8[] = {
        (char)0xC3, (char)0x84, 'p', 'f', 'e', 'l', 0
    };
    GlibError *convert_error = NULL;
    char *converted = g_convert != NULL
            ? g_convert(latin1, -1, "UTF-8", "ISO-8859-1", NULL, NULL,
                    &convert_error)
            : NULL;
    int convert_ok = converted != NULL && strcmp(converted, utf8) == 0;
    if (convert_ok) {
        snprintf(detail, sizeof(detail), "ISO-8859-1");
    }
    else if (convert_error != NULL && convert_error->message != NULL) {
        snprintf(detail, sizeof(detail), "%s", convert_error->message);
    }
    else {
        snprintf(detail, sizeof(detail), "g_convert unavailable");
    }
    result("gtk-latin1-convert", convert_ok, detail);
    RECORD(convert_ok);
    if (converted != NULL && g_free != NULL) g_free(converted);
    if (convert_error != NULL) g_error_free(convert_error);

    int init_ok = gtk_init_check(&argc, &argv) != 0;
    result("gtk-init", init_ok, init_ok ? "DISPLAY connected" : "failed");
    RECORD(init_ok);
    if (!init_ok) {
        printf("BXSUMMARY gtk-template passed=%d failed=%d\n", passed, failed);
        return 1;
    }

    static const ResourceCase resources[] = {
        {"gtk-statusbar-ui", "/org/gtk/libgtk/ui/gtkstatusbar.ui"},
        {"gtk-dialog-ui", "/org/gtk/libgtk/ui/gtkdialog.ui"},
    };
    for (size_t i = 0; i < sizeof(resources) / sizeof(resources[0]); ++i) {
        GlibError *error = NULL;
        void *bytes = g_resources_lookup_data(resources[i].path, 0, &error);
        int ok = bytes != NULL && g_bytes_get_size(bytes) > 0;
        if (ok) {
            snprintf(detail, sizeof(detail), "bytes=%zu",
                     g_bytes_get_size(bytes));
        }
        else {
            snprintf(detail, sizeof(detail), "%s",
                     error != NULL ? error->message : "missing");
        }
        result(resources[i].name, ok, detail);
        RECORD(ok);
        if (bytes != NULL) g_bytes_unref(bytes);
        if (error != NULL) g_error_free(error);
    }

    void *window = gtk_window_new(0);
    void *statusbar = gtk_statusbar_new();
    int statusbar_ok = window != NULL && statusbar != NULL;
    if (statusbar_ok) {
        gtk_container_add(window, statusbar);
        gtk_widget_show_all(window);
    }
    result("gtk-statusbar", statusbar_ok,
           statusbar_ok ? "GtkStatusbar mapped" : "constructor failed");
    RECORD(statusbar_ok);
    if (window != NULL) gtk_widget_destroy(window);

    printf("BXSUMMARY gtk-template passed=%d failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
