#define _GNU_SOURCE

#include <dlfcn.h>
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

int main(void) {
    const char *root = getenv("BIONICX_ROOTFS");
    if (root == NULL || root[0] != '/')
        root = "/data/user/0/io.taowen.bx/files/rootfs";

    char plugin_dir[512];
    char xcb_plugin[640];
    snprintf(plugin_dir, sizeof(plugin_dir),
             "%s/usr/lib/aarch64-linux-gnu/vlc/plugins", root);
    snprintf(xcb_plugin, sizeof(xcb_plugin),
             "%s/video_output/libxcb_x11_plugin.so", plugin_dir);

    check("xcb-plugin", access(xcb_plugin, R_OK) == 0, xcb_plugin);

    void *vlc = dlopen("libvlc.so.5", RTLD_NOW);
    check("dlopen-libvlc", vlc != NULL, vlc != NULL ? "libvlc.so.5" : dlerror());
    if (vlc == NULL) {
        printf("BXSUMMARY vlc-plugin passed=%d failed=%d\n", passed, failed);
        return 1;
    }

    void *(*libvlc_new)(int, const char *const *) = dlsym(vlc, "libvlc_new");
    void (*libvlc_release)(void *) = dlsym(vlc, "libvlc_release");
    check("libvlc-symbols", libvlc_new != NULL && libvlc_release != NULL,
          libvlc_new != NULL ? "3.0" : "missing");

    const char *args[] = {
        "--intf", "dummy",
        "--vout", "dummy",
        "--aout", "dummy",
        "--no-video-title-show",
        "--no-media-library",
        "--no-stats",
        "--quiet",
    };
    void *instance = libvlc_new != NULL
            ? libvlc_new((int)(sizeof(args) / sizeof(args[0])), args)
            : NULL;
    check("libvlc-new", instance != NULL, instance != NULL ? "dummy" : "null");
    if (instance != NULL && libvlc_release != NULL)
        libvlc_release(instance);

    printf("BXSUMMARY vlc-plugin passed=%d failed=%d\n", passed, failed);
    return failed ? 1 : 0;
}
