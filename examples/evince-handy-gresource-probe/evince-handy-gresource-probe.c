#define _POSIX_C_SOURCE 200809L

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Evince 134: libhandy hdy_themes_update asserts
 * hdy_resource_exists("/sm/puri/handy/themes/shared.css"). That blob lives
 * in libhandy-1.so .gresource.hdy. patchelf --set-rpath relocates the
 * section and g_resources_get_info then fails. */

typedef struct {
    unsigned domain;
    int code;
    char *message;
} GlibError;

static int passed;
static int failed;

static void result(const char *name, int ok, const char *detail) {
    printf("BXTEST %s %s%s%s\n", ok ? "PASS" : "FAIL", name,
           detail && *detail ? " " : "", detail ? detail : "");
    fflush(stdout);
    if (ok) ++passed;
    else ++failed;
}

static void *required_symbol(void *library, const char *name) {
    dlerror();
    void *symbol = dlsym(library, name);
    const char *error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "BXTEST FAIL symbol name=%s error=%s\n", name, error);
        exit(1);
    }
    return symbol;
}

int main(int argc, char **argv) {
    char detail[256];

    void *gtk = dlopen("libgtk-3.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (gtk == NULL) {
        result("gtk-dlopen", 0, dlerror());
        printf("BXSUMMARY evince-handy-gresource passed=%d failed=%d\n",
               passed, failed);
        return 1;
    }
    result("gtk-dlopen", 1, "libgtk-3.so.0");

    void *handy = dlopen("libhandy-1.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (handy == NULL) {
        result("handy-dlopen", 0, dlerror());
        printf("BXSUMMARY evince-handy-gresource passed=%d failed=%d\n",
               passed, failed);
        return 1;
    }
    result("handy-dlopen", 1, "libhandy-1.so.0");

    void *gio = dlopen("libgio-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (gio == NULL) {
        result("gio-dlopen", 0, dlerror());
        printf("BXSUMMARY evince-handy-gresource passed=%d failed=%d\n",
               passed, failed);
        return 1;
    }

    int (*gtk_init_check)(int *, char ***) = required_symbol(gtk,
            "gtk_init_check");
    void *(*g_resources_lookup_data)(const char *, int, GlibError **) =
            required_symbol(gio, "g_resources_lookup_data");
    size_t (*g_bytes_get_size)(void *) = required_symbol(gio,
            "g_bytes_get_size");
    void (*g_bytes_unref)(void *) = required_symbol(gio, "g_bytes_unref");
    void (*g_error_free)(GlibError *) = required_symbol(gio, "g_error_free");
    void (*hdy_init)(void) = required_symbol(handy, "hdy_init");

    int init_ok = gtk_init_check(&argc, &argv) != 0;
    result("gtk-init", init_ok, init_ok ? "DISPLAY connected" : "failed");
    if (!init_ok) {
        printf("BXSUMMARY evince-handy-gresource passed=%d failed=%d\n",
               passed, failed);
        return 1;
    }

    static const char *const resources[][2] = {
        {"handy-shared-css", "/sm/puri/handy/themes/shared.css"},
        {"handy-fallback-css", "/sm/puri/handy/themes/fallback.css"},
    };
    for (size_t i = 0; i < sizeof(resources) / sizeof(resources[0]); ++i) {
        GlibError *error = NULL;
        void *bytes = g_resources_lookup_data(resources[i][1], 0, &error);
        int ok = bytes != NULL && g_bytes_get_size(bytes) > 0;
        if (ok) {
            snprintf(detail, sizeof(detail), "bytes=%zu",
                     g_bytes_get_size(bytes));
        } else {
            snprintf(detail, sizeof(detail), "%s",
                     error != NULL ? error->message : "missing");
        }
        result(resources[i][0], ok, detail);
        if (bytes != NULL) g_bytes_unref(bytes);
        if (error != NULL) g_error_free(error);
    }

    if (failed == 0) {
        hdy_init();
        result("hdy-init", 1, "no-assert");
    } else {
        result("hdy-init", 0, "skipped-missing-css");
    }

    printf("BXSUMMARY evince-handy-gresource passed=%d failed=%d\n",
           passed, failed);
    return failed ? 1 : 0;
}
