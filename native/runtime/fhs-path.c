#include "runtime-internal.h"

#include <dlfcn.h>
#include <sys/inotify.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <time.h>

static const char *inotify_sysctl_value(const char *name) {
    if (strcmp(name, "max_user_watches") == 0) return "65536\n";
    if (strcmp(name, "max_user_instances") == 0) return "128\n";
    if (strcmp(name, "max_queued_events") == 0) return "16384\n";
    return NULL;
}

/* Android has no /proc/sys/fs/inotify sysctls. Chromium FilePathWatcher reads
 * max_user_watches and otherwise disables the explorer/git file watcher. */
static const char *redirect_inotify_sysctl(const char *path,
                                           char buffer[PATH_MAX]) {
    if (path == NULL || strncmp(path, "/proc/sys/fs/inotify/", 21) != 0)
        return NULL;
    const char *name = path + 21;
    const char *value = inotify_sysctl_value(name);
    if (value == NULL || strchr(name, '/') != NULL) return NULL;
    const char *tmp = bionicx_captured_tmpdir();
    if (tmp == NULL) tmp = bionicx_getenv("BIONICX_TMPDIR");
    if (tmp == NULL || tmp[0] != '/') return path;
    char directory[PATH_MAX];
    if (snprintf(directory, PATH_MAX, "%s/inotify-sysctl", tmp) >= PATH_MAX ||
            snprintf(buffer, PATH_MAX, "%s/%s", directory, name) >= PATH_MAX) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    static int (*real_mkdir)(const char *, mode_t);
    static int (*real_open)(const char *, int, ...);
    static int (*real_stat)(const char *, struct stat *);
    if (real_mkdir == NULL) real_mkdir = dlsym(RTLD_NEXT, "mkdir");
    if (real_open == NULL) real_open = dlsym(RTLD_NEXT, "open");
    if (real_stat == NULL) real_stat = dlsym(RTLD_NEXT, "stat");
    struct stat info;
    if (real_stat(buffer, &info) == 0) return buffer;
    if (real_mkdir(directory, 0700) != 0 && errno != EEXIST) return path;
    int fd = real_open(buffer, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return errno == EEXIST ? buffer : path;
    size_t length = strlen(value);
    ssize_t wrote = write(fd, value, length);
    close(fd);
    if (wrote != (ssize_t)length) return path;
    return buffer;
}

/* VS Code reads /etc/shells then fs.watch()s dirname. Android /bin is
 * /system/bin (mode o+x only), so watch("/bin") returns EACCES. Point
 * login shells at the real rootfs paths instead. */
static const char *redirect_etc_shells(const char *path, char buffer[PATH_MAX]) {
    static const char *const shells[] = {
        "/bin/sh", "/usr/bin/sh", "/bin/bash", "/usr/bin/bash",
        "/bin/rbash", "/usr/bin/rbash", "/usr/bin/dash"
    };
    if (path == NULL || strcmp(path, "/etc/shells") != 0) return NULL;
    const char *root = bionicx_captured_rootfs();
    if (root == NULL) root = bionicx_getenv("BIONICX_ROOTFS");
    const char *tmp = bionicx_captured_tmpdir();
    if (tmp == NULL) tmp = bionicx_getenv("BIONICX_TMPDIR");
    if (root == NULL || root[0] != '/' || tmp == NULL || tmp[0] != '/')
        return NULL;
    if (snprintf(buffer, PATH_MAX, "%s/etc-shells", tmp) >= PATH_MAX) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    static int (*real_open)(const char *, int, ...);
    static int (*real_stat)(const char *, struct stat *);
    if (real_open == NULL) real_open = dlsym(RTLD_NEXT, "open");
    if (real_stat == NULL) real_stat = dlsym(RTLD_NEXT, "stat");
    struct stat info;
    if (real_stat(buffer, &info) == 0) return buffer;
    int fd = real_open(buffer, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return errno == EEXIST ? buffer : NULL;
    int ok = 1;
    for (size_t i = 0; i < sizeof(shells) / sizeof(shells[0]); ++i) {
        char line[PATH_MAX];
        int n = snprintf(line, sizeof(line), "%s%s\n", root, shells[i]);
        if (n < 0 || n >= (int)sizeof(line) ||
                write(fd, line, (size_t)n) != n) {
            ok = 0;
            break;
        }
    }
    close(fd);
    return ok ? buffer : NULL;
}

/* Android app UIDs cannot read /proc/stat. VS Code's cpuUsage.sh (and
 * `code --status`) then dies with division by zero. Rewrite a growing idle
 * counter so two samples in one script have a non-zero total delta. */
static const char *redirect_proc_stat(const char *path, char buffer[PATH_MAX]) {
    if (path == NULL || strcmp(path, "/proc/stat") != 0) return NULL;
    const char *tmp = bionicx_captured_tmpdir();
    if (tmp == NULL) tmp = bionicx_getenv("BIONICX_TMPDIR");
    if (tmp == NULL || tmp[0] != '/') return path;
    if (snprintf(buffer, PATH_MAX, "%s/proc-stat", tmp) >= PATH_MAX) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    static int (*real_open)(const char *, int, ...);
    if (real_open == NULL) real_open = dlsym(RTLD_NEXT, "open");
    int fd = real_open(buffer, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return path;
    static unsigned long ticks;
    unsigned long idle = 1000000UL + (unsigned long)time(NULL) + ++ticks * 10UL;
    char text[256];
    int length = snprintf(text, sizeof(text),
            "cpu  0 0 0 %lu 0 0 0 0 0 0\n"
            "cpu0 0 0 0 %lu 0 0 0 0 0 0\n"
            "intr 0\nctxt 0\nbtime 0\nprocesses 1\n"
            "procs_running 1\nprocs_blocked 0\nsoftirq 0\n",
            idle, idle);
    ssize_t wrote = length > 0 ? write(fd, text, (size_t)length) : -1;
    close(fd);
    if (wrote != (ssize_t)length) return path;
    return buffer;
}

/* Redirect Linux FHS paths into app-private Android directories. */
const char *bionicx_redirect_path(const char *path, char buffer[PATH_MAX]) {
    if (path == NULL) return NULL;
    const char *inotify = redirect_inotify_sysctl(path, buffer);
    if (inotify != NULL) return inotify;
    const char *proc_stat = redirect_proc_stat(path, buffer);
    if (proc_stat != NULL) return proc_stat;
    const char *shells = redirect_etc_shells(path, buffer);
    if (shells != NULL) return shells;
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

int inotify_add_watch(int fd, const char *path, uint32_t mask) {
    static int (*next)(int, const char *, uint32_t);
    if (next == NULL) next = dlsym(RTLD_NEXT, "inotify_add_watch");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    return next(fd, actual, mask);
}

/* Node/libuv and Chromium issue openat/statx/inotify via syscall(), not
 * libc open. Zygote children also clearenv(), so path rewrite must not
 * depend on the live environment. Call the kernel directly: forwarding
 * through libc syscall() uses the wrong variadic ABI on AArch64. */
static long kernel_syscall6(long number, long a1, long a2, long a3, long a4,
                            long a5, long a6) {
#if defined(__aarch64__)
    register long x8 __asm__("x8") = number;
    register long x0 __asm__("x0") = a1;
    register long x1 __asm__("x1") = a2;
    register long x2 __asm__("x2") = a3;
    register long x3 __asm__("x3") = a4;
    register long x4 __asm__("x4") = a5;
    register long x5 __asm__("x5") = a6;
    __asm__ volatile("svc 0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3),
                     "r"(x4), "r"(x5) : "memory", "cc");
    return x0;
#elif defined(__x86_64__)
    register long r10 __asm__("r10") = a4;
    register long r8 __asm__("r8") = a5;
    register long r9 __asm__("r9") = a6;
    long result;
    __asm__ volatile("syscall"
                     : "=a"(result)
                     : "a"(number), "D"(a1), "S"(a2), "d"(a3), "r"(r10),
                       "r"(r8), "r"(r9)
                     : "rcx", "r11", "memory", "cc");
    return result;
#else
    static long (*next)(long, long, long, long, long, long, long);
    if (next == NULL)
        next = (long (*)(long, long, long, long, long, long, long))
                dlsym(RTLD_NEXT, "syscall");
    return next(number, a1, a2, a3, a4, a5, a6);
#endif
}

long syscall(long number, ...) {
    va_list arguments;
    va_start(arguments, number);
    long a1 = va_arg(arguments, long);
    long a2 = va_arg(arguments, long);
    long a3 = va_arg(arguments, long);
    long a4 = va_arg(arguments, long);
    long a5 = va_arg(arguments, long);
    long a6 = va_arg(arguments, long);
    va_end(arguments);
    long *path_slot = NULL;
#ifdef SYS_openat
    if (number == SYS_openat) path_slot = &a2;
#endif
#ifdef SYS_openat2
    if (number == SYS_openat2) path_slot = &a2;
#endif
#ifdef SYS_statx
    if (number == SYS_statx) path_slot = &a2;
#endif
#ifdef SYS_inotify_add_watch
    if (number == SYS_inotify_add_watch) path_slot = &a2;
#endif
#ifdef SYS_newfstatat
    if (number == SYS_newfstatat) path_slot = &a2;
#endif
#ifdef SYS_faccessat
    if (number == SYS_faccessat) path_slot = &a2;
#endif
#ifdef SYS_faccessat2
    if (number == SYS_faccessat2) path_slot = &a2;
#endif
#ifdef SYS_readlinkat
    if (number == SYS_readlinkat) path_slot = &a2;
#endif
    if (path_slot != NULL) {
        const char *path = (const char *)*path_slot;
        if (path != NULL && path[0] != '\0') {
            char buffer[PATH_MAX];
            const char *actual = bionicx_redirect_path(path, buffer);
            if (actual == NULL) return -1;
            if (actual != path) *path_slot = (long)actual;
        }
    }
#ifdef SYS_close_range
    /* node-pty calls syscall(SYS_close_range, 3, ~0U, CLOSE_RANGE_CLOEXEC)
     * after forkpty. A mis-decoded first fd of 0 would CLOEXEC stdin, so
     * execve drops the slave and bash exits 0 on /dev/null. */
    if (number == SYS_close_range && a1 < 3) a1 = 3;
#endif
    long result = kernel_syscall6(number, a1, a2, a3, a4, a5, a6);
    if (result < 0 && result > -4096) {
        errno = (int)-result;
        return -1;
    }
    return result;
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

    const char *root = bionicx_captured_rootfs();
    if (root == NULL) root = bionicx_getenv("BIONICX_ROOTFS");
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

int statx(int directory, const char *path, int flags, unsigned int mask,
          struct statx *value) {
    static int (*next)(int, const char *, int, unsigned int, struct statx *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "statx");
    /* Node/libuv uses statx, not stat. Without a rewrite, existsSync("/bin/bash")
     * looks at Android and VS Code falls back to /bin/sh. */
    if ((flags & AT_EMPTY_PATH) != 0 && (path == NULL || path[0] == '\0'))
        return next(directory, path, flags, mask, value);
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    if (next(directory, actual, flags, mask, value) != 0) return -1;
    nlink_t nlink = (nlink_t)value->stx_nlink;
    apply_fake_nlink(actual, &nlink, NULL);
    value->stx_nlink = nlink;
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
    const char *root = bionicx_captured_rootfs();
    if (root == NULL) root = bionicx_getenv("BIONICX_ROOTFS");
    if (root != NULL && strcmp(path, root) == 0) {
        return setenv("BIONICX_VIRTUAL_ROOT", "1", 1);
    }
    errno = EPERM;
    return -1;
}
