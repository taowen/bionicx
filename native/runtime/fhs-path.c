#include "runtime-internal.h"

#include <dlfcn.h>

/* Redirect Linux FHS paths into app-private Android directories. */
const char *bionicx_redirect_path(const char *path, char buffer[PATH_MAX]) {
    if (path == NULL) return NULL;
    const char *target = NULL;
    const char *suffix = NULL;
    if (strcmp(path, "/tmp") == 0 || strncmp(path, "/tmp/", 5) == 0) {
        target = bionicx_captured_tmpdir();
        if (target == NULL) target = bionicx_getenv("BIONICX_TMPDIR");
        if (target == NULL || target[0] != '/') return path;
        suffix = path + 4;
    } else if (strcmp(path, "/run") == 0 || strncmp(path, "/run/", 5) == 0) {
        target = bionicx_captured_tmpdir();
        if (target == NULL) target = bionicx_getenv("BIONICX_TMPDIR");
        if (target == NULL || target[0] != '/') return path;
        suffix = path;
    } else if (strcmp(path, "/usr") == 0 || strncmp(path, "/usr/", 5) == 0 ||
            strcmp(path, "/bin") == 0 || strncmp(path, "/bin/", 5) == 0 ||
            strcmp(path, "/sbin") == 0 || strncmp(path, "/sbin/", 6) == 0 ||
            strcmp(path, "/etc") == 0 || strncmp(path, "/etc/", 5) == 0 ||
            strcmp(path, "/opt") == 0 || strncmp(path, "/opt/", 5) == 0 ||
            strcmp(path, "/var") == 0 || strncmp(path, "/var/", 5) == 0) {
        target = bionicx_captured_rootfs();
        if (target == NULL) target = bionicx_getenv("BIONICX_ROOTFS");
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

static const char *redirect_symlink_target(const char *target,
                                           char buffer[PATH_MAX]) {
    const char *rewrite = bionicx_getenv("BIONICX_REWRITE_ABSOLUTE_SYMLINKS");
    if (rewrite == NULL || strcmp(rewrite, "1") != 0 || target == NULL ||
            target[0] != '/')
        return target;
    return bionicx_redirect_path(target, buffer);
}

int symlink(const char *target, const char *link_path) {
    static int (*next)(const char *, const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "symlink");
    char target_buffer[PATH_MAX], link_buffer[PATH_MAX];
    const char *actual_target = redirect_symlink_target(target, target_buffer);
    const char *actual_link = bionicx_redirect_path(link_path, link_buffer);
    if (actual_target == NULL || actual_link == NULL) return -1;
    return next(actual_target, actual_link);
}

int symlinkat(const char *target, int directory, const char *link_path) {
    static int (*next)(const char *, int, const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "symlinkat");
    char target_buffer[PATH_MAX], link_buffer[PATH_MAX];
    const char *actual_target = redirect_symlink_target(target, target_buffer);
    const char *actual_link = bionicx_redirect_path(link_path, link_buffer);
    if (actual_target == NULL || actual_link == NULL) return -1;
    return next(actual_target, directory, actual_link);
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

/* Fortified glibc open() compiles to __open_2 / __open64_2 and never
 * reaches the interposed open() symbol. shadow-utils groupadd uses this. */
int __open_2(const char *path, int flags) {
    return open(path, flags);
}

int __open64_2(const char *path, int flags) {
    return open64(path, flags);
}

int __openat_2(int directory, const char *path, int flags) {
    return openat(directory, path, flags);
}

int __openat64_2(int directory, const char *path, int flags) {
    return openat64(directory, path, flags);
}

static int password_lock_fd = -1;

int lckpwdf(void) {
    struct flock lock;
    if (password_lock_fd >= 0) return 0;
    password_lock_fd = open("/etc/.pwd.lock", O_WRONLY | O_CREAT, 0600);
    if (password_lock_fd < 0) return -1;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    if (fcntl(password_lock_fd, F_SETLKW, &lock) < 0) {
        close(password_lock_fd);
        password_lock_fd = -1;
        return -1;
    }
    return 0;
}

int ulckpwdf(void) {
    if (password_lock_fd < 0) {
        errno = EINVAL;
        return -1;
    }
    int result = close(password_lock_fd);
    password_lock_fd = -1;
    return result;
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

DIR *opendir(const char *path) {
    static DIR *(*next)(const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "opendir");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    return actual != NULL ? next(actual) : NULL;
}

char *realpath(const char *path, char *resolved_path) {
    static char *(*next)(const char *, char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "realpath");
    char redirected[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, redirected);
    if (actual == NULL) return NULL;

    char physical[PATH_MAX];
    char *result = next(actual, resolved_path == NULL ? NULL : physical);
    if (result == NULL) return NULL;

    const char *root = bionicx_getenv("BIONICX_ROOTFS");
    size_t root_length = root != NULL ? strlen(root) : 0;
    char canonical_root[PATH_MAX];
    const char *root_prefix = root;
    if (root != NULL && next(root, canonical_root) != NULL) {
        root_prefix = canonical_root;
        root_length = strlen(canonical_root);
    }
    const char *visible = result;
    if (actual != path && root_prefix != NULL && root_length > 0 &&
            strncmp(result, root_prefix, root_length) == 0 &&
            (result[root_length] == '/' || result[root_length] == '\0'))
        visible = result + root_length;

    if (resolved_path == NULL) {
        if (visible != result) memmove(result, visible, strlen(visible) + 1);
        return result;
    }
    strcpy(resolved_path, visible);
    return resolved_path;
}

char *canonicalize_file_name(const char *path) {
    return realpath(path, NULL);
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

int stat64(const char *path, struct stat64 *value) {
    static int (*next)(const char *, struct stat64 *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "stat64");
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

int lstat64(const char *path, struct stat64 *value) {
    static int (*next)(const char *, struct stat64 *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "lstat64");
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

int fstatat64(int directory, const char *path, struct stat64 *value,
              int flags) {
    static int (*next)(int, const char *, struct stat64 *, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "fstatat64");
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

int rename(const char *old_path, const char *new_path) {
    static int (*next)(const char *, const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "rename");
    char old_buffer[PATH_MAX], new_buffer[PATH_MAX];
    const char *actual_old = bionicx_redirect_path(old_path, old_buffer);
    const char *actual_new = bionicx_redirect_path(new_path, new_buffer);
    if (actual_old == NULL || actual_new == NULL) return -1;
    return next(actual_old, actual_new);
}

int renameat(int old_directory, const char *old_path, int new_directory,
             const char *new_path) {
    static int (*next)(int, const char *, int, const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "renameat");
    char old_buffer[PATH_MAX], new_buffer[PATH_MAX];
    const char *actual_old = bionicx_redirect_path(old_path, old_buffer);
    const char *actual_new = bionicx_redirect_path(new_path, new_buffer);
    if (actual_old == NULL || actual_new == NULL) return -1;
    return next(old_directory, actual_old, new_directory, actual_new);
}

int renameat2(int old_directory, const char *old_path, int new_directory,
              const char *new_path, unsigned int flags) {
    static int (*next)(int, const char *, int, const char *, unsigned int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "renameat2");
    char old_buffer[PATH_MAX], new_buffer[PATH_MAX];
    const char *actual_old = bionicx_redirect_path(old_path, old_buffer);
    const char *actual_new = bionicx_redirect_path(new_path, new_buffer);
    if (actual_old == NULL || actual_new == NULL) return -1;
    return next(old_directory, actual_old, new_directory, actual_new, flags);
}

int link(const char *old_path, const char *new_path) {
    static int (*next)(const char *, const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "link");
    char old_buffer[PATH_MAX], new_buffer[PATH_MAX];
    const char *actual_old = bionicx_redirect_path(old_path, old_buffer);
    const char *actual_new = bionicx_redirect_path(new_path, new_buffer);
    if (actual_old == NULL || actual_new == NULL) return -1;
    return next(actual_old, actual_new);
}

int linkat(int old_directory, const char *old_path, int new_directory,
           const char *new_path, int flags) {
    static int (*next)(int, const char *, int, const char *, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "linkat");
    char old_buffer[PATH_MAX], new_buffer[PATH_MAX];
    const char *actual_old = bionicx_redirect_path(old_path, old_buffer);
    const char *actual_new = bionicx_redirect_path(new_path, new_buffer);
    if (actual_old == NULL || actual_new == NULL) return -1;
    return next(old_directory, actual_old, new_directory, actual_new, flags);
}

int chdir(const char *path) {
    static int (*next)(const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "chdir");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    return actual != NULL ? next(actual) : -1;
}

int mkdir(const char *path, mode_t mode) {
    static int (*next)(const char *, mode_t);
    if (next == NULL) next = dlsym(RTLD_NEXT, "mkdir");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    return actual != NULL ? next(actual, mode) : -1;
}

int mkdirat(int directory, const char *path, mode_t mode) {
    static int (*next)(int, const char *, mode_t);
    if (next == NULL) next = dlsym(RTLD_NEXT, "mkdirat");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    return actual != NULL ? next(directory, actual, mode) : -1;
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
int chroot(const char *path) {
    const char *root = bionicx_getenv("BIONICX_ROOTFS");
    if (root != NULL && strcmp(path, root) == 0) {
        return setenv("BIONICX_VIRTUAL_ROOT", "1", 1);
    }
    errno = EPERM;
    return -1;
}
