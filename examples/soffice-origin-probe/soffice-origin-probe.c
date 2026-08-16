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
    if (ok)
        ++passed;
    else
        ++failed;
}

static char mapped_reglo[512];

static int note_reglo(struct dl_phdr_info *info, size_t size, void *data) {
    (void)size;
    (void)data;
    if (info->dlpi_name == NULL)
        return 0;
    if (strstr(info->dlpi_name, "libreglo.so") == NULL)
        return 0;
    snprintf(mapped_reglo, sizeof(mapped_reglo), "%s", info->dlpi_name);
    return 1;
}

int main(void) {
    const char *root = getenv("BIONICX_ROOTFS");
    if (root == NULL || root[0] != '/')
        root = "/data/user/0/io.taowen.bx/files/rootfs";

    char program_reglo[512];
    char system_reglo[512];
    char multiarch_reglo[512];
    char multiarch_cppu[512];
    snprintf(program_reglo, sizeof(program_reglo),
             "%s/usr/lib/libreoffice/program/libreglo.so", root);
    snprintf(system_reglo, sizeof(system_reglo),
             "%s/usr/lib/libreglo.so", root);
    snprintf(multiarch_reglo, sizeof(multiarch_reglo),
             "%s/usr/lib/aarch64-linux-gnu/libreglo.so", root);
    snprintf(multiarch_cppu, sizeof(multiarch_cppu),
             "%s/usr/lib/aarch64-linux-gnu/libuno_cppuhelpergcc3.so.3", root);

    check("reglo-present", access(program_reglo, R_OK) == 0, program_reglo);
    check("reglo-not-on-system",
          access(system_reglo, F_OK) != 0 && access(multiarch_reglo, F_OK) != 0,
          "only program/");

    char target[512] = "";
    ssize_t n = readlink(multiarch_cppu, target, sizeof(target) - 1);
    if (n > 0)
        target[n] = '\0';
    check("cppuhelper-symlink",
          n > 0 && strstr(target, "libreoffice/program") != NULL,
          n > 0 ? target : "not-a-symlink");

    /* soffice.bin searches system dirs first, so the loader opens the
     * multiarch symlink. $ORIGIN then becomes aarch64-linux-gnu unless
     * the real file's directory is a concrete RUNPATH entry. */
    void *cppu = dlopen("libuno_cppuhelpergcc3.so.3", RTLD_NOW);
    check("dlopen-cppuhelper", cppu != NULL,
          cppu != NULL ? "libuno_cppuhelpergcc3.so.3" : dlerror());

    mapped_reglo[0] = '\0';
    if (cppu != NULL)
        dl_iterate_phdr(note_reglo, NULL);
    check("reglo-mapped",
          mapped_reglo[0] != '\0' &&
                  strstr(mapped_reglo, "libreoffice/program") != NULL,
          mapped_reglo[0] != '\0' ? mapped_reglo : "libreglo.so not mapped");

    char gen_plugin[512];
    char gtk3_plugin[512];
    char swlo[512];
    snprintf(gen_plugin, sizeof(gen_plugin),
             "%s/usr/lib/libreoffice/program/libvclplug_genlo.so", root);
    snprintf(gtk3_plugin, sizeof(gtk3_plugin),
             "%s/usr/lib/libreoffice/program/libvclplug_gtk3lo.so", root);
    snprintf(swlo, sizeof(swlo),
             "%s/usr/lib/libreoffice/program/libswlo.so", root);
    check("gen-plugin", access(gen_plugin, R_OK) == 0, gen_plugin);
    check("gtk3-plugin", access(gtk3_plugin, R_OK) == 0, gtk3_plugin);
    void *writer = dlopen(swlo, RTLD_NOW);
    check("dlopen-swlo", writer != NULL, writer != NULL ? swlo : dlerror());

    printf("BXSUMMARY soffice-origin passed=%d failed=%d\n", passed, failed);
    return failed ? 1 : 0;
}
