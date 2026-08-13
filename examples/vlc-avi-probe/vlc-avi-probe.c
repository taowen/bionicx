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

enum {
    VLC_NothingSpecial = 0,
    VLC_Opening = 1,
    VLC_Buffering = 2,
    VLC_Playing = 3,
    VLC_Paused = 4,
    VLC_Stopped = 5,
    VLC_Ended = 6,
    VLC_Error = 7
};

int main(void) {
    const char *root = getenv("BIONICX_ROOTFS");
    if (root == NULL || root[0] != '/')
        root = "/data/user/0/io.taowen.bx/files/rootfs";
    const char *app = getenv("BIONICX_APP");
    char fixture[512];
    if (app != NULL && app[0] == '/')
        snprintf(fixture, sizeof(fixture),
                 "%s/fixtures/bionicx-motion-audio.avi", app);
    else
        snprintf(fixture, sizeof(fixture),
                 "/data/user/0/io.taowen.bx/files/apps/vlc/"
                 "fixtures/bionicx-motion-audio.avi");

    char plugin_dir[512];
    char xcb_plugin[640];
    snprintf(plugin_dir, sizeof(plugin_dir),
             "%s/usr/lib/aarch64-linux-gnu/vlc/plugins", root);
    snprintf(xcb_plugin, sizeof(xcb_plugin),
             "%s/video_output/libxcb_x11_plugin.so", plugin_dir);

    check("fixture", access(fixture, R_OK) == 0, fixture);
    check("xcb-plugin", access(xcb_plugin, R_OK) == 0, xcb_plugin);

    void *vlc = dlopen("libvlc.so.5", RTLD_NOW);
    check("dlopen-libvlc", vlc != NULL, vlc != NULL ? "libvlc.so.5" : dlerror());
    if (vlc == NULL) {
        printf("BXSUMMARY vlc-avi passed=%d failed=%d\n", passed, failed);
        return 1;
    }

    void *(*libvlc_new)(int, const char *const *) = dlsym(vlc, "libvlc_new");
    void (*libvlc_release)(void *) = dlsym(vlc, "libvlc_release");
    void *(*libvlc_media_new_path)(void *, const char *) =
            dlsym(vlc, "libvlc_media_new_path");
    void (*libvlc_media_release)(void *) = dlsym(vlc, "libvlc_media_release");
    void *(*libvlc_media_player_new_from_media)(void *) =
            dlsym(vlc, "libvlc_media_player_new_from_media");
    int (*libvlc_media_player_play)(void *) =
            dlsym(vlc, "libvlc_media_player_play");
    int (*libvlc_media_player_get_state)(void *) =
            dlsym(vlc, "libvlc_media_player_get_state");
    int (*libvlc_video_get_size)(void *, unsigned, unsigned *, unsigned *) =
            dlsym(vlc, "libvlc_video_get_size");
    long long (*libvlc_media_player_get_length)(void *) =
            dlsym(vlc, "libvlc_media_player_get_length");
    void (*libvlc_media_player_stop)(void *) =
            dlsym(vlc, "libvlc_media_player_stop");
    void (*libvlc_media_player_release)(void *) =
            dlsym(vlc, "libvlc_media_player_release");
    check("libvlc-symbols",
          libvlc_new != NULL && libvlc_media_new_path != NULL &&
                  libvlc_media_player_new_from_media != NULL &&
                  libvlc_media_player_play != NULL &&
                  libvlc_video_get_size != NULL,
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
    if (instance == NULL) {
        printf("BXSUMMARY vlc-avi passed=%d failed=%d\n", passed, failed);
        return 1;
    }

    void *media = libvlc_media_new_path(instance, fixture);
    check("media-path", media != NULL, fixture);
    void *player = media != NULL
            ? libvlc_media_player_new_from_media(media)
            : NULL;
    if (media != NULL && libvlc_media_release != NULL)
        libvlc_media_release(media);
    check("player", player != NULL, player != NULL ? "from-media" : "null");
    if (player == NULL) {
        if (libvlc_release != NULL)
            libvlc_release(instance);
        printf("BXSUMMARY vlc-avi passed=%d failed=%d\n", passed, failed);
        return 1;
    }

    int play_ok = libvlc_media_player_play(player) == 0;
    int state = VLC_NothingSpecial;
    unsigned width = 0;
    unsigned height = 0;
    long long length_ms = 0;
    for (int i = 0; i < 80 && play_ok; ++i) {
        state = libvlc_media_player_get_state(player);
        if (state == VLC_Playing || state == VLC_Ended) {
            libvlc_video_get_size(player, 0, &width, &height);
            if (libvlc_media_player_get_length != NULL)
                length_ms = libvlc_media_player_get_length(player);
            if (width == 320 && height == 180)
                break;
        }
        if (state == VLC_Error)
            break;
        usleep(50 * 1000);
    }
    char play_detail[64];
    snprintf(play_detail, sizeof(play_detail), "state=%d", state);
    check("playing",
          play_ok && (state == VLC_Playing || state == VLC_Ended),
          play_detail);

    char size_detail[32];
    snprintf(size_detail, sizeof(size_detail), "%ux%u", width, height);
    check("video-size", width == 320 && height == 180, size_detail);

    char length_detail[32];
    snprintf(length_detail, sizeof(length_detail), "ms=%lld", length_ms);
    check("duration", length_ms >= 2500 && length_ms <= 4000, length_detail);

    if (libvlc_media_player_stop != NULL)
        libvlc_media_player_stop(player);
    if (libvlc_media_player_release != NULL)
        libvlc_media_player_release(player);
    if (libvlc_release != NULL)
        libvlc_release(instance);

    printf("BXSUMMARY vlc-avi passed=%d failed=%d\n", passed, failed);
    return failed ? 1 : 0;
}
