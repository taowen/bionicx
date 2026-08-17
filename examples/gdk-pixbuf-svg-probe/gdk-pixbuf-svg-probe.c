#define _POSIX_C_SOURCE 200809L

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    unsigned domain;
    int code;
    char *message;
} GlibError;

static const char kLoader[] =
        "/usr/lib/aarch64-linux-gnu/gdk-pixbuf-2.0/2.10.0/loaders/"
        "libpixbufloader_svg.so";
static const char kMissingSvg[] =
        "/usr/share/icons/Adwaita/scalable/status/image-missing.svg";

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
        fprintf(stderr, "BXTEST FAIL gdk-symbol name=%s error=%s\n", name,
                error);
        exit(1);
    }
    return symbol;
}

int main(void) {
    int passed = 0;
    int failed = 0;
#define RECORD(value) do { if (value) ++passed; else ++failed; } while (0)
    char detail[256];

    int readable = access(kLoader, R_OK) == 0;
    result("svg-loader-access", readable, kLoader);
    RECORD(readable);

    dlerror();
    void *loader = dlopen(kLoader, RTLD_NOW);
    const char *loader_error = loader == NULL ? dlerror() : NULL;
    if (loader != NULL) {
        snprintf(detail, sizeof(detail), "handle=%p", loader);
    } else {
        snprintf(detail, sizeof(detail), "%s",
                 loader_error != NULL ? loader_error : "dlopen failed");
    }
    result("svg-loader-dlopen", loader != NULL, detail);
    RECORD(loader != NULL);
    if (loader != NULL) dlclose(loader);

    void *pixbuf = dlopen("libgdk_pixbuf-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (pixbuf == NULL) {
        result("pixbuf-dlopen", 0, dlerror());
        printf("BXSUMMARY gdk-pixbuf-svg passed=%d failed=%d\n", passed,
               failed + 1);
        return 1;
    }
    result("pixbuf-dlopen", 1, "libgdk_pixbuf-2.0.so.0");
    ++passed;

    void *(*gdk_pixbuf_new_from_file)(const char *, GlibError **) = NULL;
    int (*gdk_pixbuf_get_width)(void *) = NULL;
    int (*gdk_pixbuf_get_height)(void *) = NULL;
    void (*g_object_unref)(void *) = NULL;
    void (*g_error_free)(GlibError *) = NULL;
    *(void **)(&gdk_pixbuf_new_from_file) =
            required_symbol(pixbuf, "gdk_pixbuf_new_from_file");
    *(void **)(&gdk_pixbuf_get_width) =
            required_symbol(pixbuf, "gdk_pixbuf_get_width");
    *(void **)(&gdk_pixbuf_get_height) =
            required_symbol(pixbuf, "gdk_pixbuf_get_height");
    void *gobject = dlopen("libgobject-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    void *glib = dlopen("libglib-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (gobject != NULL)
        *(void **)(&g_object_unref) = required_symbol(gobject, "g_object_unref");
    if (glib != NULL)
        *(void **)(&g_error_free) = required_symbol(glib, "g_error_free");

    GlibError *error = NULL;
    void *image = gdk_pixbuf_new_from_file(kMissingSvg, &error);
    int loaded = image != NULL && gdk_pixbuf_get_width(image) > 0 &&
            gdk_pixbuf_get_height(image) > 0;
    if (loaded) {
        snprintf(detail, sizeof(detail), "%dx%d",
                 gdk_pixbuf_get_width(image), gdk_pixbuf_get_height(image));
    } else if (error != NULL && error->message != NULL) {
        snprintf(detail, sizeof(detail), "%s", error->message);
    } else {
        snprintf(detail, sizeof(detail), "pixbuf NULL");
    }
    result("adwaita-missing-svg", loaded, detail);
    RECORD(loaded);
    if (image != NULL && g_object_unref != NULL) g_object_unref(image);
    if (error != NULL && g_error_free != NULL) g_error_free(error);

    printf("BXSUMMARY gdk-pixbuf-svg passed=%d failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
