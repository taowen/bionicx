#define _GNU_SOURCE

#include <dlfcn.h>
#include <link.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int passed;
static int failed;

static void check(const char *name, int ok, const char *detail) {
    printf("BXTEST %s %s%s%s\n", ok ? "PASS" : "FAIL", name,
           detail != NULL && detail[0] != '\0' ? " " : "",
           detail != NULL ? detail : "");
    fflush(stdout);
    if (ok) ++passed;
    else ++failed;
}

static const char *mapped_path(void *handle) {
    struct link_map *map = NULL;
    if (handle == NULL) return NULL;
    if (dlinfo(handle, RTLD_DI_LINKMAP, &map) != 0 || map == NULL)
        return NULL;
    return map->l_name;
}

static int shared_rootfs_soname(const char *path, const char *soname) {
    return path != NULL
            && strstr(path, "/apps/") == NULL
            && strstr(path, "aarch64-linux-gnu") != NULL
            && strstr(path, soname) != NULL;
}

int main(void) {
    const char *root = getenv("BIONICX_ROOTFS");
    if (root == NULL || root[0] != '/')
        root = "";

    void *tiff = dlopen("libtiff.so.5", RTLD_NOW);
    const char *tiff_path = mapped_path(tiff);
    check("dlopen-libtiff5", tiff != NULL,
          tiff != NULL ? tiff_path : dlerror());
    check("shared-libtiff5",
          shared_rootfs_soname(tiff_path, "libtiff.so.5"),
          tiff_path != NULL ? tiff_path : "unmapped");

    char pdfmain[512];
    snprintf(pdfmain, sizeof(pdfmain),
             "%s/opt/kingsoft/wps-office/office6/libpdfmain.so", root);
    check("pdfmain-present", access(pdfmain, R_OK) == 0, pdfmain);

    void *main_so = access(pdfmain, R_OK) == 0
            ? dlopen(pdfmain, RTLD_NOW) : NULL;
    const char *main_path = mapped_path(main_so);
    check("dlopen-pdfmain", main_so != NULL,
          main_so != NULL ? main_path : dlerror());
    check("pdfmain-package-path",
          main_path != NULL
                  && strstr(main_path, "/apps/") == NULL
                  && strstr(main_path, "office6/libpdfmain.so") != NULL,
          main_path != NULL ? main_path : "unmapped");

    if (main_so != NULL) dlclose(main_so);
    if (tiff != NULL) dlclose(tiff);
    printf("BXSUMMARY wps-pdf-tiff passed=%d failed=%d\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
