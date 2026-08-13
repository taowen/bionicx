#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

/* Redirect Linux FHS paths into app-private Android directories. */
static const char *redirect_path(const char *path, char buffer[PATH_MAX]) {
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

static mode_t optional_mode(int flags, va_list arguments) {
    return (flags & O_CREAT) || ((flags & O_TMPFILE) == O_TMPFILE)
            ? (mode_t)va_arg(arguments, int) : 0;
}

int open(const char *path, int flags, ...) {
    static int (*next)(const char *, int, ...);
    if (next == NULL) next = dlsym(RTLD_NEXT, "open");
    va_list arguments;
    va_start(arguments, flags);
    mode_t mode = optional_mode(flags, arguments);
    va_end(arguments);
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    if (actual == NULL) return -1;
    return (flags & O_CREAT) || ((flags & O_TMPFILE) == O_TMPFILE)
            ? next(actual, flags, mode) : next(actual, flags);
}

int open64(const char *path, int flags, ...) {
    static int (*next)(const char *, int, ...);
    if (next == NULL) next = dlsym(RTLD_NEXT, "open64");
    va_list arguments;
    va_start(arguments, flags);
    mode_t mode = optional_mode(flags, arguments);
    va_end(arguments);
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    if (actual == NULL) return -1;
    return (flags & O_CREAT) || ((flags & O_TMPFILE) == O_TMPFILE)
            ? next(actual, flags, mode) : next(actual, flags);
}

int openat(int directory, const char *path, int flags, ...) {
    static int (*next)(int, const char *, int, ...);
    if (next == NULL) next = dlsym(RTLD_NEXT, "openat");
    va_list arguments;
    va_start(arguments, flags);
    mode_t mode = optional_mode(flags, arguments);
    va_end(arguments);
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
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
    mode_t mode = optional_mode(flags, arguments);
    va_end(arguments);
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    if (actual == NULL) return -1;
    return (flags & O_CREAT) || ((flags & O_TMPFILE) == O_TMPFILE)
            ? next(directory, actual, flags, mode)
            : next(directory, actual, flags);
}

FILE *fopen(const char *path, const char *mode) {
    static FILE *(*next)(const char *, const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "fopen");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    return actual != NULL ? next(actual, mode) : NULL;
}

FILE *fopen64(const char *path, const char *mode) {
    static FILE *(*next)(const char *, const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "fopen64");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    return actual != NULL ? next(actual, mode) : NULL;
}

int access(const char *path, int mode) {
    static int (*next)(const char *, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "access");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    return actual != NULL ? next(actual, mode) : -1;
}

int faccessat(int directory, const char *path, int mode, int flags) {
    static int (*next)(int, const char *, int, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "faccessat");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    return actual != NULL ? next(directory, actual, mode, flags) : -1;
}

int stat(const char *path, struct stat *value) {
    static int (*next)(const char *, struct stat *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "stat");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    return actual != NULL ? next(actual, value) : -1;
}

int lstat(const char *path, struct stat *value) {
    static int (*next)(const char *, struct stat *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "lstat");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    return actual != NULL ? next(actual, value) : -1;
}

int fstatat(int directory, const char *path, struct stat *value, int flags) {
    static int (*next)(int, const char *, struct stat *, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "fstatat");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    return actual != NULL ? next(directory, actual, value, flags) : -1;
}

int unlink(const char *path) {
    static int (*next)(const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "unlink");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    return actual != NULL ? next(actual) : -1;
}

int unlinkat(int directory, const char *path, int flags) {
    static int (*next)(int, const char *, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "unlinkat");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    return actual != NULL ? next(directory, actual, flags) : -1;
}

int chmod(const char *path, mode_t mode) {
    static int (*next)(const char *, mode_t);
    if (next == NULL) next = dlsym(RTLD_NEXT, "chmod");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    return actual != NULL ? next(actual, mode) : -1;
}

int fchmodat(int directory, const char *path, mode_t mode, int flags) {
    static int (*next)(int, const char *, mode_t, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "fchmodat");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    return actual != NULL ? next(directory, actual, mode, flags) : -1;
}

static int ignore_rootless_ownership_failure(int result) {
    if (result < 0 && (errno == EPERM || errno == EACCES)) {
        errno = 0;
        return 0;
    }
    return result;
}

int chown(const char *path, uid_t owner, gid_t group) {
    static int (*next)(const char *, uid_t, gid_t);
    if (next == NULL) next = dlsym(RTLD_NEXT, "chown");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    if (actual == NULL) return -1;
    return ignore_rootless_ownership_failure(next(actual, owner, group));
}

int lchown(const char *path, uid_t owner, gid_t group) {
    static int (*next)(const char *, uid_t, gid_t);
    if (next == NULL) next = dlsym(RTLD_NEXT, "lchown");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    if (actual == NULL) return -1;
    return ignore_rootless_ownership_failure(next(actual, owner, group));
}

int fchownat(int directory, const char *path, uid_t owner, gid_t group,
             int flags) {
    static int (*next)(int, const char *, uid_t, gid_t, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "fchownat");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    if (actual == NULL) return -1;
    return ignore_rootless_ownership_failure(
            next(directory, actual, owner, group, flags));
}

int fchown(int descriptor, uid_t owner, gid_t group) {
    static int (*next)(int, uid_t, gid_t);
    if (next == NULL) next = dlsym(RTLD_NEXT, "fchown");
    return ignore_rootless_ownership_failure(next(descriptor, owner, group));
}

int chdir(const char *path) {
    static int (*next)(const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "chdir");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    return actual != NULL ? next(actual) : -1;
}

static char **script_arguments(const char *path, char *const arguments[],
                               char program[PATH_MAX],
                               char interpreter_argument[PATH_MAX]) {
    char line[512];
    int descriptor = open(path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) return NULL;
    ssize_t length = read(descriptor, line, sizeof(line) - 1);
    close(descriptor);
    if (length < 3 || line[0] != '#' || line[1] != '!') return NULL;
    line[length] = '\0';
    char *cursor = line + 2;
    while (*cursor == ' ' || *cursor == '\t') ++cursor;
    char *interpreter = cursor;
    while (*cursor != '\0' && *cursor != '\n' && *cursor != ' ' &&
            *cursor != '\t') ++cursor;
    if (cursor == interpreter) return NULL;
    char separator = *cursor;
    *cursor = '\0';
    char redirected[PATH_MAX];
    const char *actual = redirect_path(interpreter, redirected);
    if (actual == NULL || snprintf(program, PATH_MAX, "%s", actual) >= PATH_MAX)
        return NULL;

    int has_interpreter_argument = 0;
    if (separator != '\0' && separator != '\n') {
        ++cursor;
        while (*cursor == ' ' || *cursor == '\t') ++cursor;
        char *end = cursor;
        while (*end != '\0' && *end != '\n') ++end;
        while (end > cursor && (end[-1] == ' ' || end[-1] == '\t')) --end;
        if (end > cursor) {
            size_t argument_length = (size_t)(end - cursor);
            if (argument_length >= PATH_MAX) return NULL;
            memcpy(interpreter_argument, cursor, argument_length);
            interpreter_argument[argument_length] = '\0';
            has_interpreter_argument = 1;
        }
    }
    size_t argument_count = 0;
    while (arguments[argument_count] != NULL) ++argument_count;
    char **result = calloc(argument_count + 2 + has_interpreter_argument,
                           sizeof(*result));
    if (result == NULL) return NULL;
    result[0] = program;
    size_t out = 1;
    if (has_interpreter_argument)
        result[out++] = interpreter_argument;
    result[out++] = (char *)path;
    for (size_t i = 1; i < argument_count; ++i)
        result[out++] = arguments[i];
    return result;
}

static void ensure_rootfs_path(void) {
    const char *root = getenv("BIONICX_ROOTFS");
    const char *current = getenv("PATH");
    if (root == NULL || root[0] != '/' ||
            (current != NULL && strncmp(current, root, strlen(root)) == 0))
        return;
    char value[PATH_MAX * 2];
    int count = snprintf(value, sizeof(value),
                         "%s/usr/sbin:%s/usr/bin:%s/sbin:%s/bin:%s",
                         root, root, root, root,
                         current != NULL ? current : "/system/bin");
    if (count > 0 && count < (int)sizeof(value)) setenv("PATH", value, 1);
}

int execv(const char *path, char *const arguments[]) {
    static int (*next)(const char *, char *const[]);
    if (next == NULL) next = dlsym(RTLD_NEXT, "execv");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    if (actual == NULL) return -1;
    ensure_rootfs_path();
    char program[PATH_MAX], interpreter_argument[PATH_MAX];
    char **script = script_arguments(actual, arguments, program,
                                     interpreter_argument);
    if (script != NULL) {
        int result = next(program, script);
        int saved_errno = errno;
        free(script);
        errno = saved_errno;
        return result;
    }
    return next(actual, arguments);
}

int execve(const char *path, char *const arguments[],
           char *const environment[]) {
    static int (*next)(const char *, char *const[], char *const[]);
    if (next == NULL) next = dlsym(RTLD_NEXT, "execve");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    if (actual == NULL) return -1;
    char program[PATH_MAX], interpreter_argument[PATH_MAX];
    char **script = script_arguments(actual, arguments, program,
                                     interpreter_argument);
    if (script != NULL) {
        int result = next(program, script, environment);
        int saved_errno = errno;
        free(script);
        errno = saved_errno;
        return result;
    }
    return next(actual, arguments, environment);
}

int execvp(const char *path, char *const arguments[]) {
    static int (*next)(const char *, char *const[]);
    if (next == NULL) next = dlsym(RTLD_NEXT, "execvp");
    char buffer[PATH_MAX];
    const char *actual = redirect_path(path, buffer);
    if (actual == NULL) return -1;
    ensure_rootfs_path();
    if (actual == path && strchr(path, '/') == NULL) {
        const char *root = getenv("BIONICX_ROOTFS");
        static const char *directories[] = {
            "/usr/sbin", "/usr/bin", "/sbin", "/bin"
        };
        if (root != NULL && root[0] == '/') {
            for (size_t i = 0;
                    i < sizeof(directories) / sizeof(directories[0]); ++i) {
                int count = snprintf(buffer, sizeof(buffer), "%s%s/%s", root,
                                     directories[i], path);
                if (count > 0 && count < (int)sizeof(buffer) &&
                        access(buffer, X_OK) == 0) {
                    actual = buffer;
                    break;
                }
            }
        }
    }
    char program[PATH_MAX], interpreter_argument[PATH_MAX];
    char **script = script_arguments(actual, arguments, program,
                                     interpreter_argument);
    if (script != NULL) {
        int result = next(program, script);
        int saved_errno = errno;
        free(script);
        errno = saved_errno;
        return result;
    }
    return next(actual, arguments);
}

int execlp(const char *path, const char *argument, ...) {
    va_list values;
    va_start(values, argument);
    va_list count_values;
    va_copy(count_values, values);
    size_t count = 1;
    while (va_arg(count_values, const char *) != NULL) ++count;
    va_end(count_values);

    char **arguments = calloc(count + 1, sizeof(*arguments));
    if (arguments == NULL) {
        va_end(values);
        return -1;
    }
    arguments[0] = (char *)argument;
    for (size_t i = 1; i < count; ++i)
        arguments[i] = va_arg(values, char *);
    (void)va_arg(values, char *);
    va_end(values);
    arguments[count] = NULL;
    int result = execvp(path, arguments);
    int saved_errno = errno;
    free(arguments);
    errno = saved_errno;
    return result;
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
    char *actual = (char *)redirect_path(path, buffer);
    if (actual == NULL) return -1;
    int result = next(actual);
    if (result >= 0 && actual != path) copy_random_suffix(path, actual, 0);
    return result;
}

int mkostemp(char *path, int flags) {
    static int (*next)(char *, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "mkostemp");
    char buffer[PATH_MAX];
    char *actual = (char *)redirect_path(path, buffer);
    if (actual == NULL) return -1;
    int result = next(actual, flags);
    if (result >= 0 && actual != path) copy_random_suffix(path, actual, 0);
    return result;
}

int mkstemps(char *path, int suffix_length) {
    static int (*next)(char *, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "mkstemps");
    char buffer[PATH_MAX];
    char *actual = (char *)redirect_path(path, buffer);
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
    char *actual = (char *)redirect_path(path, buffer);
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
    char *actual = (char *)redirect_path(path, buffer);
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
    const char *redirected = redirect_path(unix_address->sun_path, buffer);
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
