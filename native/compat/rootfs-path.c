#include "rootfs-internal.h"

#include <dlfcn.h>

/* Redirect Linux FHS paths into app-private Android directories. */
const char *bionicx_redirect_path(const char *path, char buffer[PATH_MAX]) {
    const char *target = getenv("BIONICX_TMPDIR");
    const char *suffix = NULL;
    if (path == NULL) return NULL;
    if (target != NULL && target[0] == '/' &&
            (strcmp(path, "/tmp") == 0 || strncmp(path, "/tmp/", 5) == 0)) {
        suffix = path + 4;
    } else if (strcmp(path, "/usr") == 0 || strncmp(path, "/usr/", 5) == 0 ||
            strcmp(path, "/bin") == 0 || strncmp(path, "/bin/", 5) == 0 ||
            strcmp(path, "/sbin") == 0 || strncmp(path, "/sbin/", 6) == 0 ||
            strcmp(path, "/etc") == 0 || strncmp(path, "/etc/", 5) == 0 ||
            strcmp(path, "/var") == 0 || strncmp(path, "/var/", 5) == 0) {
        target = getenv("BIONICX_ROOTFS");
        if (target == NULL || target[0] != '/') return path;
        suffix = path;
    } else {
        return path;
    }
    int count = snprintf(buffer, PATH_MAX, "%s%s", target, suffix);
    if (count < 0 || count >= PATH_MAX) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    return buffer;
}

mode_t bionicx_optional_mode(int flags, va_list arguments) {
    return (flags & O_CREAT) || ((flags & O_TMPFILE) == O_TMPFILE)
            ? (mode_t)va_arg(arguments, int) : 0;
}

int open(const char *path, int flags, ...) {
    static int (*next)(const char *, int, ...);
    if (next == NULL) next = dlsym(RTLD_NEXT, "open");
    va_list arguments;
    va_start(arguments, flags);
    mode_t mode = bionicx_optional_mode(flags, arguments);
    va_end(arguments);
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    return (flags & O_CREAT) || ((flags & O_TMPFILE) == O_TMPFILE)
            ? next(actual, flags, mode) : next(actual, flags);
}

int open64(const char *path, int flags, ...) {
    static int (*next)(const char *, int, ...);
    if (next == NULL) next = dlsym(RTLD_NEXT, "open64");
    va_list arguments;
    va_start(arguments, flags);
    mode_t mode = bionicx_optional_mode(flags, arguments);
    va_end(arguments);
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    return (flags & O_CREAT) || ((flags & O_TMPFILE) == O_TMPFILE)
            ? next(actual, flags, mode) : next(actual, flags);
}

int openat(int directory, const char *path, int flags, ...) {
    static int (*next)(int, const char *, int, ...);
    if (next == NULL) next = dlsym(RTLD_NEXT, "openat");
    va_list arguments;
    va_start(arguments, flags);
    mode_t mode = bionicx_optional_mode(flags, arguments);
    va_end(arguments);
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    return (flags & O_CREAT) || ((flags & O_TMPFILE) == O_TMPFILE)
            ? next(directory, actual, flags, mode)
            : next(directory, actual, flags);
}

int openat64(int directory, const char *path, int flags, ...) {
    static int (*next)(int, const char *, int, ...);
    if (next == NULL) next = dlsym(RTLD_NEXT, "openat64");
    va_list arguments;
    va_start(arguments, flags);
    mode_t mode = bionicx_optional_mode(flags, arguments);
    va_end(arguments);
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    return (flags & O_CREAT) || ((flags & O_TMPFILE) == O_TMPFILE)
            ? next(directory, actual, flags, mode)
            : next(directory, actual, flags);
}

FILE *fopen(const char *path, const char *mode) {
    static FILE *(*next)(const char *, const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "fopen");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    return actual != NULL ? next(actual, mode) : NULL;
}

FILE *fopen64(const char *path, const char *mode) {
    static FILE *(*next)(const char *, const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "fopen64");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    return actual != NULL ? next(actual, mode) : NULL;
}

int access(const char *path, int mode) {
    static int (*next)(const char *, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "access");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    return actual != NULL ? next(actual, mode) : -1;
}

int faccessat(int directory, const char *path, int mode, int flags) {
    static int (*next)(int, const char *, int, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "faccessat");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    return actual != NULL ? next(directory, actual, mode, flags) : -1;
}

int stat(const char *path, struct stat *value) {
    static int (*next)(const char *, struct stat *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "stat");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    return actual != NULL ? next(actual, value) : -1;
}

int lstat(const char *path, struct stat *value) {
    static int (*next)(const char *, struct stat *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "lstat");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    return actual != NULL ? next(actual, value) : -1;
}

int fstatat(int directory, const char *path, struct stat *value, int flags) {
    static int (*next)(int, const char *, struct stat *, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "fstatat");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    return actual != NULL ? next(directory, actual, value, flags) : -1;
}

int unlink(const char *path) {
    static int (*next)(const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "unlink");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    return actual != NULL ? next(actual) : -1;
}

int unlinkat(int directory, const char *path, int flags) {
    static int (*next)(int, const char *, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "unlinkat");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    return actual != NULL ? next(directory, actual, flags) : -1;
}

int chdir(const char *path) {
    static int (*next)(const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "chdir");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    return actual != NULL ? next(actual) : -1;
}

static void copy_random_suffix(char *path, const char *actual,
                               int suffix_length) {
    size_t path_length = strlen(path);
    size_t actual_length = strlen(actual);
    size_t random_length = 6;
    if (path_length >= random_length + (size_t)suffix_length &&
            actual_length >= random_length + (size_t)suffix_length) {
        memcpy(path + path_length - suffix_length - random_length,
               actual + actual_length - suffix_length - random_length,
               random_length);
    }
}

int mkstemp(char *path) {
    static int (*next)(char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "mkstemp");
    char buffer[PATH_MAX];
    char *actual = (char *)bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    int result = next(actual);
    if (result >= 0 && actual != path) copy_random_suffix(path, actual, 0);
    return result;
}

int mkostemp(char *path, int flags) {
    static int (*next)(char *, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "mkostemp");
    char buffer[PATH_MAX];
    char *actual = (char *)bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    int result = next(actual, flags);
    if (result >= 0 && actual != path) copy_random_suffix(path, actual, 0);
    return result;
}

int mkstemps(char *path, int suffix_length) {
    static int (*next)(char *, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "mkstemps");
    char buffer[PATH_MAX];
    char *actual = (char *)bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    int result = next(actual, suffix_length);
    if (result >= 0 && actual != path)
        copy_random_suffix(path, actual, suffix_length);
    return result;
}

int mkostemps(char *path, int suffix_length, int flags) {
    static int (*next)(char *, int, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "mkostemps");
    char buffer[PATH_MAX];
    char *actual = (char *)bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    int result = next(actual, suffix_length, flags);
    if (result >= 0 && actual != path)
        copy_random_suffix(path, actual, suffix_length);
    return result;
}

char *mkdtemp(char *path) {
    static char *(*next)(char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "mkdtemp");
    char buffer[PATH_MAX];
    char *actual = (char *)bionicx_redirect_path(path, buffer);
    if (actual == NULL || next(actual) == NULL) return NULL;
    if (actual != path) copy_random_suffix(path, actual, 0);
    return path;
}

static int redirect_socket_address(const struct sockaddr *address,
                                   socklen_t length,
                                   struct sockaddr_un *translated,
                                   const struct sockaddr **actual,
                                   socklen_t *actual_length) {
    *actual = address;
    *actual_length = length;
    if (address == NULL || address->sa_family != AF_UNIX) return 0;
    const struct sockaddr_un *unix_address = (const struct sockaddr_un *)address;
    if (unix_address->sun_path[0] == '\0') return 0;
    char buffer[PATH_MAX];
    const char *redirected = bionicx_redirect_path(unix_address->sun_path, buffer);
    if (redirected == NULL) return -1;
    if (redirected == unix_address->sun_path) return 0;
    size_t path_length = strlen(redirected);
    if (path_length >= sizeof(translated->sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memset(translated, 0, sizeof(*translated));
    translated->sun_family = AF_UNIX;
    memcpy(translated->sun_path, redirected, path_length + 1);
    *actual = (const struct sockaddr *)translated;
    *actual_length = (socklen_t)(offsetof(struct sockaddr_un, sun_path) +
                                 path_length + 1);
    return 0;
}

int bind(int socket, const struct sockaddr *address, socklen_t length) {
    static int (*next)(int, const struct sockaddr *, socklen_t);
    if (next == NULL) next = dlsym(RTLD_NEXT, "bind");
    struct sockaddr_un translated;
    const struct sockaddr *actual;
    socklen_t actual_length;
    if (redirect_socket_address(address, length, &translated, &actual,
                                &actual_length) < 0) return -1;
    return next(socket, actual, actual_length);
}

int connect(int socket, const struct sockaddr *address, socklen_t length) {
    static int (*next)(int, const struct sockaddr *, socklen_t);
    if (next == NULL) next = dlsym(RTLD_NEXT, "connect");
    struct sockaddr_un translated;
    const struct sockaddr *actual;
    socklen_t actual_length;
    if (redirect_socket_address(address, length, &translated, &actual,
                                &actual_length) < 0) return -1;
    return next(socket, actual, actual_length);
}
