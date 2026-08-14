#include "runtime-internal.h"

#include <dlfcn.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>

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

ssize_t readlink(const char *path, char *value, size_t size) {
    static ssize_t (*next)(const char *, char *, size_t);
    if (next == NULL) next = dlsym(RTLD_NEXT, "readlink");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    return next(actual, value, size);
}

ssize_t readlinkat(int directory, const char *path, char *value, size_t size) {
    static ssize_t (*next)(int, const char *, char *, size_t);
    if (next == NULL) next = dlsym(RTLD_NEXT, "readlinkat");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    return next(directory, actual, value, size);
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

/* f2fs cannot hard-link. groupadd locks /etc/group by linking
 * group.PID -> group.lock and requiring st_nlink == 2. */
#define BIONICX_FAKE_LINKS 32

static struct {
    char first[PATH_MAX];
    char second[PATH_MAX];
    ino_t inode;
    int used;
} fake_links[BIONICX_FAKE_LINKS];

static void remember_fake_link(const char *source, const char *destination) {
    static int (*real_stat)(const char *, struct stat *);
    struct stat info;
    int index;
    if (source == NULL || destination == NULL) return;
    if (real_stat == NULL) real_stat = dlsym(RTLD_NEXT, "stat");
    if (real_stat(source, &info) != 0) return;
    for (index = 0; index < BIONICX_FAKE_LINKS; ++index) {
        if (!fake_links[index].used) break;
    }
    if (index == BIONICX_FAKE_LINKS) return;
    snprintf(fake_links[index].first, PATH_MAX, "%s", source);
    snprintf(fake_links[index].second, PATH_MAX, "%s", destination);
    fake_links[index].inode = info.st_ino;
    fake_links[index].used = 1;
}

static void apply_fake_nlink(const char *path, nlink_t *nlink, ino_t *inode) {
    int index;
    if (path == NULL || path[0] == '\0') return;
    for (index = 0; index < BIONICX_FAKE_LINKS; ++index) {
        if (!fake_links[index].used) continue;
        if (strcmp(path, fake_links[index].first) != 0 &&
                strcmp(path, fake_links[index].second) != 0)
            continue;
        if (nlink != NULL) *nlink = 2;
        if (inode != NULL) *inode = fake_links[index].inode;
        return;
    }
}

static void forget_fake_link(const char *path) {
    int index;
    if (path == NULL) return;
    for (index = 0; index < BIONICX_FAKE_LINKS; ++index) {
        if (!fake_links[index].used) continue;
        if (strcmp(path, fake_links[index].first) != 0 &&
                strcmp(path, fake_links[index].second) != 0)
            continue;
        fake_links[index].used = 0;
        fake_links[index].first[0] = '\0';
        fake_links[index].second[0] = '\0';
    }
}

int stat(const char *path, struct stat *value) {
    static int (*next)(const char *, struct stat *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "stat");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL || next(actual, value) != 0) return -1;
    apply_fake_nlink(actual, &value->st_nlink, &value->st_ino);
    return 0;
}

int stat64(const char *path, struct stat64 *value) {
    static int (*next)(const char *, struct stat64 *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "stat64");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL || next(actual, value) != 0) return -1;
    apply_fake_nlink(actual, &value->st_nlink, &value->st_ino);
    return 0;
}

int lstat(const char *path, struct stat *value) {
    static int (*next)(const char *, struct stat *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "lstat");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL || next(actual, value) != 0) return -1;
    apply_fake_nlink(actual, &value->st_nlink, &value->st_ino);
    return 0;
}

int lstat64(const char *path, struct stat64 *value) {
    static int (*next)(const char *, struct stat64 *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "lstat64");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL || next(actual, value) != 0) return -1;
    apply_fake_nlink(actual, &value->st_nlink, &value->st_ino);
    return 0;
}

int fstatat(int directory, const char *path, struct stat *value, int flags) {
    static int (*next)(int, const char *, struct stat *, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "fstatat");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL || next(directory, actual, value, flags) != 0)
        return -1;
    apply_fake_nlink(actual, &value->st_nlink, &value->st_ino);
    return 0;
}

int fstatat64(int directory, const char *path, struct stat64 *value,
              int flags) {
    static int (*next)(int, const char *, struct stat64 *, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "fstatat64");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL || next(directory, actual, value, flags) != 0)
        return -1;
    apply_fake_nlink(actual, &value->st_nlink, &value->st_ino);
    return 0;
}

/* Android's / is often 100% full. glibc apps that statvfs("/") or an
 * unredirected /usr think there is no space and skip writes / resource
 * copies. Reuse the app-private rootfs numbers when the kernel reports
 * zero available blocks. */
static void replenish_statvfs(struct statvfs *info) {
    static int (*next)(const char *, struct statvfs *);
    if (info == NULL || info->f_bavail != 0) return;
    if (next == NULL) next = dlsym(RTLD_NEXT, "statvfs");
    const char *root = bionicx_captured_rootfs();
    if (root == NULL) root = bionicx_getenv("BIONICX_ROOTFS");
    if (root == NULL || root[0] != '/') return;
    struct statvfs fresh;
    if (next(root, &fresh) == 0 && fresh.f_bavail != 0) *info = fresh;
}

static void replenish_statfs(struct statfs *info) {
    static int (*next)(const char *, struct statfs *);
    if (info == NULL || info->f_bavail != 0) return;
    if (next == NULL) next = dlsym(RTLD_NEXT, "statfs");
    const char *root = bionicx_captured_rootfs();
    if (root == NULL) root = bionicx_getenv("BIONICX_ROOTFS");
    if (root == NULL || root[0] != '/') return;
    struct statfs fresh;
    if (next(root, &fresh) == 0 && fresh.f_bavail != 0) *info = fresh;
}

int statvfs(const char *path, struct statvfs *info) {
    static int (*next)(const char *, struct statvfs *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "statvfs");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    if (next(actual, info) != 0) return -1;
    replenish_statvfs(info);
    return 0;
}

int statvfs64(const char *path, struct statvfs64 *info) {
    return statvfs(path, (struct statvfs *)info);
}

int statfs(const char *path, struct statfs *info) {
    static int (*next)(const char *, struct statfs *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "statfs");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    if (next(actual, info) != 0) return -1;
    replenish_statfs(info);
    return 0;
}

int statfs64(const char *path, struct statfs64 *info) {
    return statfs(path, (struct statfs *)info);
}

int unlink(const char *path) {
    static int (*next)(const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "unlink");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    forget_fake_link(actual);
    return next(actual);
}

int unlinkat(int directory, const char *path, int flags) {
    static int (*next)(int, const char *, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "unlinkat");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    forget_fake_link(actual);
    return next(directory, actual, flags);
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

static int link_copy_forced(void) {
    const char *force = bionicx_getenv("BIONICX_FORCE_LINK_COPY");
    return force != NULL && strcmp(force, "1") == 0;
}

static int link_copy_errno(int error) {
    return error == EACCES || error == EPERM || error == EOPNOTSUPP ||
            error == ENOSYS || error == EXDEV;
}

/* Android app-data f2fs often denies link(2). dpkg backs up status with
 * link(status, status-old); a same-directory copy is enough for that. */
static int copy_regular_file(const char *old_path, const char *new_path,
                             int follow) {
    static int (*real_open)(const char *, int, ...);
    static int (*real_unlink)(const char *);
    if (real_open == NULL) real_open = dlsym(RTLD_NEXT, "open");
    if (real_unlink == NULL) real_unlink = dlsym(RTLD_NEXT, "unlink");
    int flags = O_RDONLY | O_CLOEXEC;
    if (!follow) flags |= O_NOFOLLOW;
    int input = real_open(old_path, flags);
    if (input < 0) return -1;
    struct stat info;
    if (fstat(input, &info) != 0) {
        close(input);
        return -1;
    }
    if (!S_ISREG(info.st_mode)) {
        close(input);
        errno = EPERM;
        return -1;
    }
    int output = real_open(new_path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC,
                           info.st_mode & 0777);
    if (output < 0) {
        close(input);
        return -1;
    }
    char block[8192];
    for (;;) {
        ssize_t got = read(input, block, sizeof(block));
        if (got == 0) break;
        if (got < 0) {
            int saved = errno;
            close(input);
            close(output);
            real_unlink(new_path);
            errno = saved;
            return -1;
        }
        ssize_t sent = 0;
        while (sent < got) {
            ssize_t wrote = write(output, block + sent, (size_t)(got - sent));
            if (wrote < 0) {
                int saved = errno;
                close(input);
                close(output);
                real_unlink(new_path);
                errno = saved;
                return -1;
            }
            sent += wrote;
        }
    }
    close(input);
    if (close(output) != 0) {
        int saved = errno;
        real_unlink(new_path);
        errno = saved;
        return -1;
    }
    return 0;
}

int link(const char *old_path, const char *new_path) {
    static int (*next)(const char *, const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "link");
    char old_buffer[PATH_MAX], new_buffer[PATH_MAX];
    const char *actual_old = bionicx_redirect_path(old_path, old_buffer);
    const char *actual_new = bionicx_redirect_path(new_path, new_buffer);
    if (actual_old == NULL || actual_new == NULL) return -1;
    if (link_copy_forced()) {
        if (copy_regular_file(actual_old, actual_new, 0) != 0) return -1;
        remember_fake_link(actual_old, actual_new);
        return 0;
    }
    int result = next(actual_old, actual_new);
    if (result == 0 || !link_copy_errno(errno)) return result;
    if (copy_regular_file(actual_old, actual_new, 0) != 0) return -1;
    remember_fake_link(actual_old, actual_new);
    return 0;
}

static int resolve_at_path(int directory, const char *path, char out[PATH_MAX]) {
    if (path != NULL && path[0] == '/') {
        if (snprintf(out, PATH_MAX, "%s", path) >= PATH_MAX) {
            errno = ENAMETOOLONG;
            return -1;
        }
        return 0;
    }
    if (directory == AT_FDCWD) {
        if (getcwd(out, PATH_MAX) == NULL) return -1;
        if (path != NULL && path[0] != '\0') {
            size_t used = strlen(out);
            if (snprintf(out + used, PATH_MAX - used, "/%s", path) >=
                    (int)(PATH_MAX - used)) {
                errno = ENAMETOOLONG;
                return -1;
            }
        }
        return 0;
    }
    static ssize_t (*real_readlink)(const char *, char *, size_t);
    if (real_readlink == NULL) real_readlink = dlsym(RTLD_NEXT, "readlink");
    char fd_path[64];
    snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d", directory);
    char directory_path[PATH_MAX];
    ssize_t length = real_readlink(fd_path, directory_path,
                                   sizeof(directory_path) - 1);
    if (length < 0) return -1;
    directory_path[length] = '\0';
    if (path != NULL && path[0] != '\0') {
        if (snprintf(out, PATH_MAX, "%s/%s", directory_path, path) >= PATH_MAX) {
            errno = ENAMETOOLONG;
            return -1;
        }
    } else if (snprintf(out, PATH_MAX, "%s", directory_path) >= PATH_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }
    return 0;
}

static int copy_linkat(int old_directory, const char *old_path,
                       int new_directory, const char *new_path, int flags) {
    char old_resolved[PATH_MAX], new_resolved[PATH_MAX];
    char old_redirect[PATH_MAX], new_redirect[PATH_MAX];
    const char *source;
    if ((flags & AT_EMPTY_PATH) != 0 &&
            (old_path == NULL || old_path[0] == '\0')) {
        if (snprintf(old_resolved, sizeof(old_resolved), "/proc/self/fd/%d",
                     old_directory) >= (int)sizeof(old_resolved)) {
            errno = ENAMETOOLONG;
            return -1;
        }
        source = old_resolved;
    } else {
        if (resolve_at_path(old_directory, old_path, old_resolved) != 0)
            return -1;
        source = bionicx_redirect_path(old_resolved, old_redirect);
        if (source == NULL) return -1;
    }
    if (resolve_at_path(new_directory, new_path, new_resolved) != 0) return -1;
    const char *destination = bionicx_redirect_path(new_resolved, new_redirect);
    if (destination == NULL) return -1;
    int follow = (flags & (AT_SYMLINK_FOLLOW | AT_EMPTY_PATH)) != 0;
    if (copy_regular_file(source, destination, follow) != 0) return -1;
    remember_fake_link(source, destination);
    return 0;
}

int linkat(int old_directory, const char *old_path, int new_directory,
           const char *new_path, int flags) {
    static int (*next)(int, const char *, int, const char *, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "linkat");
    char old_buffer[PATH_MAX], new_buffer[PATH_MAX];
    const char *actual_old = bionicx_redirect_path(old_path, old_buffer);
    const char *actual_new = bionicx_redirect_path(new_path, new_buffer);
    if (actual_old == NULL || actual_new == NULL) return -1;
    if (link_copy_forced())
        return copy_linkat(old_directory, actual_old, new_directory,
                           actual_new, flags);
    int result = next(old_directory, actual_old, new_directory, actual_new,
                      flags);
    if (result == 0 || !link_copy_errno(errno)) return result;
    return copy_linkat(old_directory, actual_old, new_directory, actual_new,
                       flags);
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
