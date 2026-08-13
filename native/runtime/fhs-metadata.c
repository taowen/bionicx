#include "runtime-internal.h"

#include <dlfcn.h>

int chmod(const char *path, mode_t mode) {
    static int (*next)(const char *, mode_t);
    if (next == NULL) next = dlsym(RTLD_NEXT, "chmod");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    return actual != NULL ? next(actual, mode) : -1;
}

int fchmodat(int directory, const char *path, mode_t mode, int flags) {
    static int (*next)(int, const char *, mode_t, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "fchmodat");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    int result = next(directory, actual, mode, flags);
#ifdef AT_EMPTY_PATH
    if (result < 0 && errno == EINVAL && (flags & AT_EMPTY_PATH) != 0 &&
            actual[0] == '\0') return fchmod(directory, mode);
#endif
#ifdef AT_SYMLINK_NOFOLLOW
    if (result < 0 && errno == EINVAL &&
            (flags & AT_SYMLINK_NOFOLLOW) != 0)
        return next(directory, actual, mode, flags & ~AT_SYMLINK_NOFOLLOW);
#endif
    return result;
}

int bionicx_ignore_ownership_failure(int result) {
    if (result < 0 && (errno == EPERM || errno == EACCES || errno == EINVAL)) {
        errno = 0;
        return 0;
    }
    return result;
}

int chown(const char *path, uid_t owner, gid_t group) {
    static int (*next)(const char *, uid_t, gid_t);
    if (next == NULL) next = dlsym(RTLD_NEXT, "chown");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    return bionicx_ignore_ownership_failure(next(actual, owner, group));
}

int lchown(const char *path, uid_t owner, gid_t group) {
    static int (*next)(const char *, uid_t, gid_t);
    if (next == NULL) next = dlsym(RTLD_NEXT, "lchown");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    return bionicx_ignore_ownership_failure(next(actual, owner, group));
}

int fchownat(int directory, const char *path, uid_t owner, gid_t group,
             int flags) {
    static int (*next)(int, const char *, uid_t, gid_t, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "fchownat");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    return bionicx_ignore_ownership_failure(
            next(directory, actual, owner, group, flags));
}

int fchown(int descriptor, uid_t owner, gid_t group) {
    static int (*next)(int, uid_t, gid_t);
    if (next == NULL) next = dlsym(RTLD_NEXT, "fchown");
    return bionicx_ignore_ownership_failure(next(descriptor, owner, group));
}
