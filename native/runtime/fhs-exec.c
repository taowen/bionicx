#include "runtime-internal.h"

#include <dlfcn.h>
#include <link.h>
#include <pthread.h>
#include <pty.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>

#define SHELL_CHILD_SLOTS 64

struct shell_child {
    FILE *stream;
    pid_t pid;
};

static pthread_mutex_t shell_children_lock = PTHREAD_MUTEX_INITIALIZER;
static struct shell_child shell_children[SHELL_CHILD_SLOTS];

static int (*real_access_fn(void))(const char *, int) {
    static int (*real_access)(const char *, int);
    if (real_access == NULL) real_access = dlsym(RTLD_NEXT, "access");
    return real_access;
}

static void *dlopen_from_directory(const char *directory, const char *soname,
                                   int flags, void *(*next)(const char *, int)) {
    int (*real_access)(const char *, int) = real_access_fn();
    if (real_access == NULL || directory == NULL || directory[0] != '/')
        return NULL;
    char candidate[PATH_MAX];
    int count = snprintf(candidate, sizeof(candidate), "%s/%s",
                         directory, soname);
    if (count < 0 || count >= (int)sizeof(candidate)) return NULL;
    if (real_access(candidate, F_OK) == 0) return next(candidate, flags);
    return NULL;
}

struct nss3_directory {
    char path[PATH_MAX];
};

static int record_nss3_directory(struct dl_phdr_info *info, size_t size,
                                 void *data) {
    (void)size;
    struct nss3_directory *found = data;
    if (info->dlpi_name == NULL || info->dlpi_name[0] != '/') return 0;
    const char *slash = strrchr(info->dlpi_name, '/');
    const char *base = slash != NULL ? slash + 1 : info->dlpi_name;
    if (strcmp(base, "libnss3.so") != 0) return 0;
    if (snprintf(found->path, sizeof(found->path), "%s",
                 info->dlpi_name) >= (int)sizeof(found->path))
        return 0;
    char *dir_end = strrchr(found->path, '/');
    if (dir_end == NULL) {
        found->path[0] = '\0';
        return 0;
    }
    *dir_end = '\0';
    return 1;
}

/* Firefox maps GreD libnss3.so first, then PR_LoadLibrary("libsoftokn3.so").
 * A bare SONAME would otherwise hit the Debian multiarch copy, which is a
 * different NSS build and returns CKR_DEVICE_ERROR from PSM. */
static void *dlopen_from_loaded_nss(const char *soname, int flags,
                                    void *(*next)(const char *, int)) {
    struct nss3_directory found = { .path = "" };
    dl_iterate_phdr(record_nss3_directory, &found);
    if (found.path[0] == '\0') return NULL;
    return dlopen_from_directory(found.path, soname, flags, next);
}

static void *dlopen_from_executable_lib(const char *soname, int flags,
                                        void *(*next)(const char *, int)) {
    static ssize_t (*real_readlink)(const char *, char *, size_t);
    int (*real_access)(const char *, int) = real_access_fn();
    if (real_readlink == NULL)
        real_readlink = dlsym(RTLD_NEXT, "readlink");
    if (real_readlink == NULL || real_access == NULL) return NULL;

    char exe[PATH_MAX];
    ssize_t length = real_readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (length <= 0) return NULL;
    exe[length] = '\0';
    char *slash = strrchr(exe, '/');
    if (slash == NULL) return NULL;
    *slash = '\0';
    /* App payloads keep plugins in lib/ next to bin/. The Vulkan loader
     * dlopens ICD SONAMEs from libvulkan.so, so glibc never sees the
     * executable RUNPATH. Do not use interposed access(): host checkouts
     * under /var/home would be rewritten into the Debian rootfs. */
    static const char *const suffixes[] = { "/../lib/", "/lib/", "/" };
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        char candidate[PATH_MAX];
        int count = snprintf(candidate, sizeof(candidate), "%s%s%s",
                exe, suffixes[i], soname);
        if (count < 0 || count >= (int)sizeof(candidate)) continue;
        if (real_access(candidate, F_OK) == 0) return next(candidate, flags);
    }
    return NULL;
}

void *dlopen(const char *path, int flags) {
    static void *(*next)(const char *, int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "dlopen");
    if (path == NULL) return next(NULL, flags);
    /* A bare SONAME is resolved from this preload object's return
     * address, so glibc ignores the executable RUNPATH. Search the
     * Debian multiarch directories ourselves. */
    if (strchr(path, '/') == NULL) {
        void *from_nss = dlopen_from_loaded_nss(path, flags, next);
        if (from_nss != NULL) return from_nss;
        const char *gred = bionicx_getenv("MOZILLA_FIVE_HOME");
        void *from_gred = dlopen_from_directory(gred, path, flags, next);
        if (from_gred != NULL) return from_gred;
        const char *app = bionicx_getenv("BIONICX_APP");
        if (app != NULL && app[0] == '/') {
            char app_lib[PATH_MAX];
            if (snprintf(app_lib, sizeof(app_lib), "%s/lib", app) <
                    (int)sizeof(app_lib)) {
                void *from_app_payload = dlopen_from_directory(
                        app_lib, path, flags, next);
                if (from_app_payload != NULL) return from_app_payload;
            }
        }
        const char *root = bionicx_captured_rootfs();
        if (root == NULL) root = bionicx_getenv("BIONICX_ROOTFS");
        if (root != NULL && root[0] == '/') {
            static const char *const directories[] = {
                "/usr/lib/aarch64-linux-gnu",
                "/lib/aarch64-linux-gnu",
                "/usr/lib",
                "/lib",
                NULL
            };
            for (int i = 0; directories[i] != NULL; ++i) {
                char candidate[PATH_MAX];
                int count = snprintf(candidate, sizeof(candidate), "%s%s/%s",
                        root, directories[i], path);
                if (count < 0 || count >= (int)sizeof(candidate)) continue;
                if (access(candidate, F_OK) == 0) return next(candidate, flags);
            }
        }
        void *from_app = dlopen_from_executable_lib(path, flags, next);
        if (from_app != NULL) return from_app;
    }
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    return actual != NULL ? next(actual, flags) : NULL;
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
    const char *actual = bionicx_redirect_path(interpreter, redirected);
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
    if (has_interpreter_argument) result[out++] = interpreter_argument;
    result[out++] = (char *)path;
    for (size_t i = 1; i < argument_count; ++i) result[out++] = arguments[i];
    return result;
}

static char **with_child_flags(char *const arguments[]);
static void free_child_flags(char **merged, char *const original[]);
static void log_exec(const char *path, char *const arguments[]);
static void restore_runtime_environment(void);

static void ensure_rootfs_path(void) {
    const char *root = bionicx_getenv("BIONICX_ROOTFS");
    const char *current = bionicx_getenv("PATH");
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

static void keep_standard_fds(void) {
    for (int fd = STDIN_FILENO; fd <= STDERR_FILENO; ++fd) {
        int flags = fcntl(fd, F_GETFD);
        if (flags >= 0 && (flags & FD_CLOEXEC) != 0)
            (void)fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC);
    }
    /* node-pty leaves stdin on the slave but stdout/stderr on a pipe and
     * /dev/null. Interactive bash then writes the prompt off the pty. */
    if (isatty(STDIN_FILENO)) {
        if (!isatty(STDOUT_FILENO))
            (void)dup2(STDIN_FILENO, STDOUT_FILENO);
        if (!isatty(STDERR_FILENO))
            (void)dup2(STDIN_FILENO, STDERR_FILENO);
        /* node-pty resets signals after forkpty() returns in the child and
         * can drop the controlling tty. bash -i then skips rc and exits 0
         * on EOF. Re-acquire /dev/tty before exec. */
        int devtty = open("/dev/tty", O_RDWR | O_NOCTTY);
        if (devtty >= 0) {
            close(devtty);
        } else {
            if (getsid(0) != getpid())
                (void)setsid();
            (void)ioctl(STDIN_FILENO, TIOCSCTTY, 1);
        }
    }
}

static int exec_script(const char *path, char *const arguments[],
                       int (*execute)(const char *, char *const[])) {
    char **child_args = with_child_flags(arguments);
    char *const *use_args = child_args != NULL ? child_args : arguments;
    char program[PATH_MAX], interpreter_argument[PATH_MAX];
    char **script = script_arguments(path, use_args, program,
                                     interpreter_argument);
    if (script == NULL) {
        /* A missing shebang is a real ELF. A present shebang must not reach
         * the kernel: #!/bin/sh would start Android's Bionic shell. */
        int descriptor = open(path, O_RDONLY | O_CLOEXEC);
        if (descriptor >= 0) {
            char magic[2] = {0, 0};
            ssize_t n = read(descriptor, magic, 2);
            close(descriptor);
            if (n == 2 && magic[0] == '#' && magic[1] == '!') {
                errno = ENOEXEC;
                free_child_flags(child_args, arguments);
                return -1;
            }
        }
        restore_runtime_environment();
        log_exec(path, (char *const *)use_args);
        keep_standard_fds();
        int result = execute(path, (char *const *)use_args);
        int saved_errno = errno;
        free_child_flags(child_args, arguments);
        errno = saved_errno;
        return result;
    }
    restore_runtime_environment();
    log_exec(program, script);
    keep_standard_fds();
    int result = execute(program, script);
    int saved_errno = errno;
    free(script);
    free_child_flags(child_args, arguments);
    errno = saved_errno;
    return result;
}

static const char *const runtime_environment_names[] = {
    "LD_PRELOAD",
    "BIONICX_ROOTFS",
    "BIONICX_TMPDIR",
    "BIONICX_DNS_SERVERS",
    "BIONICX_VIRTUAL_ROOT",
    "BIONICX_REWRITE_ABSOLUTE_SYMLINKS",
    "BIONICX_CHILD_FLAGS",
    "BIONICX_LOG_EXEC",
    "SSL_CERT_FILE",
    "SSL_CERT_DIR",
    "NODE_EXTRA_CA_CERTS",
    /* Zygote children clearenv(); VS Code's pty host still getenv()s these. */
    "SHELL",
    "HOME",
    "PATH",
    "LANG",
    "TERM",
};

static char captured_runtime_environment
        [sizeof(runtime_environment_names) /
         sizeof(runtime_environment_names[0])][PATH_MAX];

__attribute__((constructor(101)))
static void capture_runtime_environment(void) {
    for (size_t i = 0; i < sizeof(runtime_environment_names) /
            sizeof(runtime_environment_names[0]); ++i) {
        const char *value = getenv(runtime_environment_names[i]);
        if (value == NULL) {
            captured_runtime_environment[i][0] = '\0';
            continue;
        }
        snprintf(captured_runtime_environment[i],
                 sizeof(captured_runtime_environment[i]), "%s", value);
    }
}

static const char *captured_runtime_value(const char *name) {
    for (size_t i = 0; i < sizeof(runtime_environment_names) /
            sizeof(runtime_environment_names[0]); ++i) {
        if (strcmp(runtime_environment_names[i], name) != 0) continue;
        return captured_runtime_environment[i][0] != '\0'
                ? captured_runtime_environment[i] : NULL;
    }
    return NULL;
}

static void restore_runtime_environment(void) {
    for (size_t i = 0; i < sizeof(runtime_environment_names) /
            sizeof(runtime_environment_names[0]); ++i) {
        if (captured_runtime_environment[i][0] == '\0') continue;
        setenv(runtime_environment_names[i],
               captured_runtime_environment[i], 1);
    }
}

int clearenv(void) {
    static int (*next)(void);
    if (next == NULL) next = dlsym(RTLD_NEXT, "clearenv");
    int result = next != NULL ? next() : 0;
    /* Chromium's zygote clearenv()s before specializing Node utilities.
     * Node copies environ into process.env, so libc getenv hooks are not
     * enough for VS Code's pty host to see SHELL/HOME/PATH. */
    restore_runtime_environment();
    return result;
}

int unsetenv(const char *name) {
    static int (*next)(const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "unsetenv");
    /* Do not call setenv() here: glibc setenv() can recurse into unsetenv(). */
    if (captured_runtime_value(name) != NULL) return 0;
    return next != NULL ? next(name) : -1;
}

const char *bionicx_captured_rootfs(void) {
    return captured_runtime_value("BIONICX_ROOTFS");
}

const char *bionicx_captured_tmpdir(void) {
    return captured_runtime_value("BIONICX_TMPDIR");
}

char *getenv(const char *name) {
    static char *(*next)(const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "getenv");
    char *live = next(name);
    if (live != NULL && live[0] != '\0') return live;
    return (char *)captured_runtime_value(name);
}

char *secure_getenv(const char *name) {
    static char *(*next)(const char *);
    if (next == NULL) next = dlsym(RTLD_NEXT, "secure_getenv");
    char *live = next != NULL ? next(name) : NULL;
    if (live != NULL && live[0] != '\0') return live;
    return (char *)captured_runtime_value(name);
}

__attribute__((constructor(102)))
static void log_runtime_loaded(void) {
    if (captured_runtime_value("BIONICX_LOG_EXEC") == NULL) return;
    fputs("bionicx-runtime: exec log enabled\n", stderr);
    fflush(stderr);
}

__attribute__((constructor(103)))
static void trace_bash_startup(void) {
    if (captured_runtime_value("BIONICX_LOG_EXEC") == NULL) return;
    char exe[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n <= 0) return;
    exe[n] = '\0';
    if (strstr(exe, "bash") == NULL) return;
    const char *tmp = captured_runtime_value("BIONICX_TMPDIR");
    if (tmp == NULL || tmp[0] != '/') tmp = bionicx_getenv("BIONICX_TMPDIR");
    const char *home = bionicx_getenv("HOME");
    char path[PATH_MAX];
    if (tmp != NULL && tmp[0] == '/' &&
            snprintf(path, sizeof(path), "%s/bash-ctor.log", tmp) < PATH_MAX) {
        /* Prefer the app cache so run-as can cat it. */
    } else if (home != NULL && home[0] == '/' &&
            snprintf(path, sizeof(path), "%s/bash-ctor.log", home) < PATH_MAX) {
    } else {
        return;
    }
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    char cmdline[256];
    int cmdfd = open("/proc/self/cmdline", O_RDONLY | O_CLOEXEC);
    ssize_t clen = cmdfd >= 0 ? read(cmdfd, cmdline, sizeof(cmdline) - 1) : 0;
    if (cmdfd >= 0) close(cmdfd);
    if (clen < 0) clen = 0;
    for (ssize_t i = 0; i < clen; ++i)
        if (cmdline[i] == '\0') cmdline[i] = ' ';
    cmdline[clen] = '\0';
    char rcpath[PATH_MAX];
    int rc_ok = 0, rc_err = 0;
    if (home != NULL &&
            snprintf(rcpath, sizeof(rcpath), "%s/.bashrc", home) < PATH_MAX) {
        int rcfd = open(rcpath, O_RDONLY | O_CLOEXEC);
        if (rcfd >= 0) {
            rc_ok = 1;
            close(rcfd);
        } else {
            rc_err = errno;
        }
    }
    char line[512];
    int w = snprintf(line, sizeof(line),
                     "exe=%s cmdline=%s isatty0=%d HOME=%s bashrc=%d errno=%d\n",
                     exe, cmdline, isatty(STDIN_FILENO),
                     home != NULL ? home : "", rc_ok, rc_err);
    if (w > 0) (void)write(fd, line, (size_t)w);
    close(fd);
}

static int environment_has_name(const char *entry, const char *name) {
    size_t length = strlen(name);
    return strncmp(entry, name, length) == 0 && entry[length] == '=';
}

static int is_runtime_environment_entry(const char *entry) {
    for (size_t i = 0; i < sizeof(runtime_environment_names) /
            sizeof(runtime_environment_names[0]); ++i) {
        if (environment_has_name(entry, runtime_environment_names[i]))
            return 1;
    }
    return 0;
}

/* dpkg execve()s maintainer helpers with a sanitized environment. The
 * mandatory runtime contract must still travel with every Debian process. */
static char **with_runtime_environment(char *const environment[],
                                       size_t *owned_from) {
    *owned_from = 0;
    if (environment == NULL) return NULL;

    size_t original = 0;
    while (environment[original] != NULL) ++original;

    size_t extra = 0;
    const char *values[sizeof(runtime_environment_names) /
                       sizeof(runtime_environment_names[0])];
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        values[i] = captured_runtime_value(runtime_environment_names[i]);
        if (values[i] != NULL) ++extra;
    }

    char **merged = calloc(original + extra + 1, sizeof(*merged));
    if (merged == NULL) return NULL;
    size_t out = 0;
    for (size_t i = 0; i < original; ++i) {
        if (!is_runtime_environment_entry(environment[i]))
            merged[out++] = environment[i];
    }
    *owned_from = out;
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        if (values[i] == NULL) continue;
        size_t needed = strlen(runtime_environment_names[i]) + 1 +
                        strlen(values[i]) + 1;
        merged[out] = malloc(needed);
        if (merged[out] == NULL) {
            while (out > *owned_from) free(merged[--out]);
            free(merged);
            return NULL;
        }
        memcpy(merged[out], runtime_environment_names[i],
               strlen(runtime_environment_names[i]));
        merged[out][strlen(runtime_environment_names[i])] = '=';
        memcpy(merged[out] + strlen(runtime_environment_names[i]) + 1,
               values[i], strlen(values[i]) + 1);
        ++out;
    }
    merged[out] = NULL;
    return merged;
}

static void free_runtime_environment(char **merged, size_t owned_from) {
    if (merged == NULL) return;
    for (size_t i = owned_from; merged[i] != NULL; ++i) free(merged[i]);
    free(merged);
}

/* Chromium-family helpers re-exec themselves with --type=. Browser argv
 * switches are not forwarded, and Lark/Feishu does not read
 * CHROME_EXTRA_FLAGS. Append captured BIONICX_CHILD_FLAGS only for those
 * helpers so dpkg/sh keep their original argv. */
#define CHILD_FLAG_SLOTS 16

static int arguments_have_prefix(char *const arguments[], const char *prefix) {
    size_t length = strlen(prefix);
    if (arguments == NULL) return 0;
    for (size_t i = 0; arguments[i] != NULL; ++i)
        if (strncmp(arguments[i], prefix, length) == 0)
            return 1;
    return 0;
}

static int arguments_have_flag(char *const arguments[], const char *flag) {
    if (arguments == NULL) return 0;
    for (size_t i = 0; arguments[i] != NULL; ++i)
        if (strcmp(arguments[i], flag) == 0)
            return 1;
    return 0;
}

static char **with_child_flags(char *const arguments[]) {
    const char *extra = captured_runtime_value("BIONICX_CHILD_FLAGS");
    if (extra == NULL || extra[0] == '\0' || arguments == NULL)
        return NULL;
    if (!arguments_have_prefix(arguments, "--type="))
        return NULL;

    char copy[PATH_MAX];
    snprintf(copy, sizeof(copy), "%s", extra);
    const char *flags[CHILD_FLAG_SLOTS];
    size_t flag_count = 0;
    char *cursor = copy;
    while (*cursor != '\0' && flag_count < CHILD_FLAG_SLOTS) {
        while (*cursor == ' ') ++cursor;
        if (*cursor == '\0') break;
        flags[flag_count++] = cursor;
        char *end = cursor;
        while (*end != '\0' && *end != ' ') ++end;
        if (*end != '\0') *end++ = '\0';
        cursor = end;
    }

    size_t original = 0;
    while (arguments[original] != NULL) ++original;
    size_t add = 0;
    for (size_t i = 0; i < flag_count; ++i)
        if (!arguments_have_flag(arguments, flags[i]))
            ++add;
    if (add == 0) return NULL;

    char **merged = calloc(original + add + 1, sizeof(*merged));
    if (merged == NULL) return NULL;
    for (size_t i = 0; i < original; ++i)
        merged[i] = arguments[i];
    size_t out = original;
    for (size_t i = 0; i < flag_count; ++i) {
        if (arguments_have_flag(arguments, flags[i])) continue;
        merged[out] = strdup(flags[i]);
        if (merged[out] == NULL) {
            while (out > original) free(merged[--out]);
            free(merged);
            return NULL;
        }
        ++out;
    }
    merged[out] = NULL;
    return merged;
}

static void free_child_flags(char **merged, char *const original[]) {
    if (merged == NULL) return;
    size_t i = 0;
    while (original[i] != NULL) ++i;
    for (; merged[i] != NULL; ++i) free(merged[i]);
    free(merged);
}

static void log_exec(const char *path, char *const arguments[]) {
    const char *enabled = captured_runtime_value("BIONICX_LOG_EXEC");
    if (enabled == NULL) enabled = getenv("BIONICX_LOG_EXEC");
    if (enabled == NULL || enabled[0] == '\0') return;
    int log_file = path != NULL && strstr(path, "bash") != NULL;
    char line[512];
    size_t used = 0;
    used = (size_t)snprintf(line, sizeof(line), "bionicx-execve:");
    if (path != NULL && used < sizeof(line))
        used += (size_t)snprintf(line + used, sizeof(line) - used, " %s", path);
    if (log_file && used < sizeof(line)) {
        int cloexec = fcntl(STDIN_FILENO, F_GETFD);
        int nenv = 0, has_ld = 0;
        const char *live_home = "";
        if (environ != NULL) {
            for (char **entry = environ; *entry != NULL; ++entry) {
                ++nenv;
                if (strncmp(*entry, "HOME=", 5) == 0) live_home = *entry + 5;
                if (strncmp(*entry, "LD_PRELOAD=", 11) == 0) has_ld = 1;
            }
        }
        used += (size_t)snprintf(line + used, sizeof(line) - used,
                                 " isatty0=%d isatty1=%d isatty2=%d cloexec0=%d nenv=%d ld=%d HOME=%s",
                                 isatty(STDIN_FILENO), isatty(STDOUT_FILENO),
                                 isatty(STDERR_FILENO),
                                 cloexec >= 0 && (cloexec & FD_CLOEXEC) ? 1 : 0,
                                 nenv, has_ld, live_home);
    }
    if (arguments != NULL) {
        for (size_t i = 0; arguments[i] != NULL && i < 8 && used < sizeof(line);
                ++i)
            used += (size_t)snprintf(line + used, sizeof(line) - used, " %s",
                                     arguments[i]);
    }
    if (used >= sizeof(line)) used = sizeof(line) - 1;
    line[used++] = '\n';
    if (enabled != NULL && enabled[0] != '\0') {
        fwrite(line, 1, used, stderr);
        fflush(stderr);
    }
    if (!log_file) return;
    const char *tmp = bionicx_captured_tmpdir();
    if (tmp == NULL || tmp[0] != '/') tmp = bionicx_getenv("BIONICX_TMPDIR");
    if (tmp == NULL || tmp[0] != '/') return;
    char log_path[PATH_MAX];
    if (snprintf(log_path, sizeof(log_path), "%s/exec.log", tmp) >= PATH_MAX)
        return;
    int fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    (void)write(fd, line, used);
    close(fd);
}

int execv(const char *path, char *const arguments[]) {
    static int (*next)(const char *, char *const[]);
    if (next == NULL) next = dlsym(RTLD_NEXT, "execv");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    restore_runtime_environment();
    ensure_rootfs_path();
    return exec_script(actual, arguments, next);
}

int execve(const char *path, char *const arguments[],
           char *const environment[]) {
    static int (*next)(const char *, char *const[], char *const[]);
    if (next == NULL) next = dlsym(RTLD_NEXT, "execve");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    size_t owned_from = 0;
    char **merged = with_runtime_environment(environment, &owned_from);
    if (environment != NULL && merged == NULL) return -1;
    char *const *actual_environment = merged != NULL ? merged : environment;
    char **child_args = with_child_flags(arguments);
    char *const *use_args = child_args != NULL ? child_args : arguments;
    char program[PATH_MAX], interpreter_argument[PATH_MAX];
    char **script = script_arguments(actual, use_args, program,
                                     interpreter_argument);
    log_exec(script == NULL ? actual : program,
             script == NULL ? use_args : script);
    keep_standard_fds();
    int result = script == NULL
            ? next(actual, use_args, actual_environment)
            : next(program, script, actual_environment);
    int saved_errno = errno;
    free(script);
    free_child_flags(child_args, arguments);
    free_runtime_environment(merged, owned_from);
    errno = saved_errno;
    return result;
}

int execveat(int dirfd, const char *path, char *const arguments[],
             char *const environment[], int flags) {
    static int (*next)(int, const char *, char *const[], char *const[], int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "execveat");
    char buffer[PATH_MAX];
    const char *actual = path;
    if (path[0] == '/') {
        actual = bionicx_redirect_path(path, buffer);
        if (actual == NULL) return -1;
    }
    size_t owned_from = 0;
    char **merged = with_runtime_environment(environment, &owned_from);
    if (environment != NULL && merged == NULL) return -1;
    char *const *actual_environment = merged != NULL ? merged : environment;
    char **child_args = with_child_flags(arguments);
    char *const *use_args = child_args != NULL ? child_args : arguments;
    log_exec(actual[0] != '\0' ? actual : "execveat", use_args);
    keep_standard_fds();
    int result = next(dirfd, actual, use_args, actual_environment, flags);
    int saved_errno = errno;
    free_child_flags(child_args, arguments);
    free_runtime_environment(merged, owned_from);
    errno = saved_errno;
    return result;
}

int posix_spawn(pid_t *pid, const char *path,
                const posix_spawn_file_actions_t *file_actions,
                const posix_spawnattr_t *attrp,
                char *const arguments[], char *const environment[]) {
    static int (*next)(pid_t *, const char *,
                       const posix_spawn_file_actions_t *,
                       const posix_spawnattr_t *,
                       char *const[], char *const[]);
    if (next == NULL) next = dlsym(RTLD_NEXT, "posix_spawn");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return errno != 0 ? errno : ENOENT;
    size_t owned_from = 0;
    char **merged = with_runtime_environment(environment, &owned_from);
    if (environment != NULL && merged == NULL) return ENOMEM;
    char *const *actual_environment = merged != NULL ? merged : environment;
    char **child_args = with_child_flags(arguments);
    char *const *use_args = child_args != NULL ? child_args : arguments;
    char program[PATH_MAX], interpreter_argument[PATH_MAX];
    char **script = script_arguments(actual, use_args, program,
                                     interpreter_argument);
    log_exec(script == NULL ? actual : program,
             script == NULL ? use_args : script);
    int result = script == NULL
            ? next(pid, actual, file_actions, attrp, use_args,
                   actual_environment)
            : next(pid, program, file_actions, attrp, script,
                   actual_environment);
    free(script);
    free_child_flags(child_args, arguments);
    free_runtime_environment(merged, owned_from);
    return result;
}

int posix_spawnp(pid_t *pid, const char *path,
                 const posix_spawn_file_actions_t *file_actions,
                 const posix_spawnattr_t *attrp,
                 char *const arguments[], char *const environment[]) {
    char buffer[PATH_MAX];
    const char *actual = path;
    if (strchr(path, '/') == NULL) {
        const char *root = bionicx_getenv("BIONICX_ROOTFS");
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
    return posix_spawn(pid, actual, file_actions, attrp, arguments,
                       environment);
}

int execvp(const char *path, char *const arguments[]) {
    static int (*next)(const char *, char *const[]);
    if (next == NULL) next = dlsym(RTLD_NEXT, "execvp");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
    restore_runtime_environment();
    ensure_rootfs_path();
    if (actual == path && strchr(path, '/') == NULL) {
        const char *root = bionicx_getenv("BIONICX_ROOTFS");
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
    return exec_script(actual, arguments, next);
}

static char **vararg_arguments(const char *argument, va_list values,
                               size_t *count_out) {
    va_list count_values;
    va_copy(count_values, values);
    size_t count = 1;
    while (va_arg(count_values, const char *) != NULL) ++count;
    va_end(count_values);
    char **arguments = calloc(count + 1, sizeof(*arguments));
    if (arguments == NULL) return NULL;
    arguments[0] = (char *)argument;
    for (size_t i = 1; i < count; ++i)
        arguments[i] = va_arg(values, char *);
    (void)va_arg(values, char *);
    arguments[count] = NULL;
    *count_out = count;
    return arguments;
}

int execl(const char *path, const char *argument, ...) {
    va_list values;
    va_start(values, argument);
    size_t count = 0;
    char **arguments = vararg_arguments(argument, values, &count);
    va_end(values);
    if (arguments == NULL) return -1;
    int result = execv(path, arguments);
    int saved_errno = errno;
    free(arguments);
    errno = saved_errno;
    return result;
}

int execle(const char *path, const char *argument, ...) {
    va_list values;
    va_start(values, argument);
    size_t count = 0;
    char **arguments = vararg_arguments(argument, values, &count);
    char *const *environment = va_arg(values, char *const *);
    va_end(values);
    if (arguments == NULL) return -1;
    int result = execve(path, arguments, (char *const *)environment);
    int saved_errno = errno;
    free(arguments);
    errno = saved_errno;
    return result;
}

int execvpe(const char *path, char *const arguments[],
            char *const environment[]) {
    char buffer[PATH_MAX];
    const char *actual = path;
    if (strchr(path, '/') == NULL) {
        const char *root = bionicx_getenv("BIONICX_ROOTFS");
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
    return execve(actual, arguments, environment);
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

/* libutil forkpty() calls login_tty() in the child. If TIOCSCTTY fails,
 * glibc leaves stdin on the inherited /dev/null and the child either
 * _exit(1) or execs bash, which then sees EOF and exits 0. Always attach
 * the slave to 0/1/2 even when TIOCSCTTY is denied. */
static void trace_forkpty(const char *phase, pid_t pid, int err) {
    if (captured_runtime_value("BIONICX_LOG_EXEC") == NULL &&
            getenv("BIONICX_LOG_EXEC") == NULL)
        return;
    const char *tmp = bionicx_captured_tmpdir();
    if (tmp == NULL || tmp[0] != '/') tmp = bionicx_getenv("BIONICX_TMPDIR");
    if (tmp == NULL || tmp[0] != '/') return;
    char path[PATH_MAX];
    if (snprintf(path, sizeof(path), "%s/forkpty.log", tmp) >= PATH_MAX)
        return;
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    char line[160];
    int n = snprintf(line, sizeof(line), "%s pid=%d errno=%d\n",
                     phase, (int)pid, err);
    if (n > 0) (void)write(fd, line, (size_t)n);
    close(fd);
}

pid_t forkpty(int *amaster, char *name, const struct termios *termp,
              const struct winsize *winp) {
    static int (*real_openpty)(int *, int *, char *, const struct termios *,
                               const struct winsize *);
    if (real_openpty == NULL)
        real_openpty = dlsym(RTLD_NEXT, "openpty");
    if (real_openpty == NULL || amaster == NULL) {
        errno = ENOSYS;
        trace_forkpty("enosys", -1, errno);
        return -1;
    }
    int master = -1, slave = -1;
    if (real_openpty(&master, &slave, name, termp, winp) < 0) {
        trace_forkpty("openpty", -1, errno);
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        int saved = errno;
        close(master);
        close(slave);
        errno = saved;
        trace_forkpty("fork", -1, saved);
        return -1;
    }
    if (pid == 0) {
        close(master);
        (void)setsid();
        (void)ioctl(slave, TIOCSCTTY, 0);
        if (dup2(slave, STDIN_FILENO) < 0 ||
                dup2(slave, STDOUT_FILENO) < 0 ||
                dup2(slave, STDERR_FILENO) < 0)
            _exit(127);
        if (slave > STDERR_FILENO) close(slave);
        trace_forkpty("child", 0, 0);
        return 0;
    }
    close(slave);
    *amaster = master;
    trace_forkpty("parent", pid, 0);
    return pid;
}

char *ptsname(int fd) {
    static __thread char buffer[64];
    unsigned int ptyno = 0;
    if (ioctl(fd, TIOCGPTN, &ptyno) == 0) {
        int count = snprintf(buffer, sizeof(buffer), "/dev/pts/%u", ptyno);
        if (count > 0 && count < (int)sizeof(buffer)) return buffer;
    }
    static char *(*next)(int);
    if (next == NULL) next = dlsym(RTLD_NEXT, "ptsname");
    return next != NULL ? next(fd) : NULL;
}

int ptsname_r(int fd, char *buffer, size_t length) {
    unsigned int ptyno = 0;
    if (ioctl(fd, TIOCGPTN, &ptyno) == 0) {
        int count = snprintf(buffer, length, "/dev/pts/%u", ptyno);
        if (count > 0 && (size_t)count < length) return 0;
        errno = ERANGE;
        return ERANGE;
    }
    static int (*next)(int, char *, size_t);
    if (next == NULL) next = dlsym(RTLD_NEXT, "ptsname_r");
    if (next == NULL) return errno != 0 ? errno : ENOSYS;
    return next(fd, buffer, length);
}

static const char *rootfs_shell(char path[PATH_MAX]) {
    const char *root = bionicx_getenv("BIONICX_ROOTFS");
    if (root == NULL || root[0] != '/' ||
            snprintf(path, PATH_MAX, "%s/bin/sh", root) >= PATH_MAX) {
        errno = ENOENT;
        return NULL;
    }
    return path;
}

FILE *popen(const char *command, const char *type) {
    if (command == NULL || type == NULL ||
            (strcmp(type, "r") != 0 && strcmp(type, "re") != 0 &&
             strcmp(type, "w") != 0 && strcmp(type, "we") != 0)) {
        errno = EINVAL;
        return NULL;
    }
    int descriptors[2];
    if (pipe2(descriptors, O_CLOEXEC) != 0) return NULL;
    int reading = type[0] == 'r';
    pid_t child = fork();
    if (child < 0) {
        int saved = errno;
        close(descriptors[0]);
        close(descriptors[1]);
        errno = saved;
        return NULL;
    }
    if (child == 0) {
        int child_end = reading ? descriptors[1] : descriptors[0];
        int standard = reading ? STDOUT_FILENO : STDIN_FILENO;
        close(reading ? descriptors[0] : descriptors[1]);
        if (dup2(child_end, standard) < 0) _exit(126);
        close(child_end);
        char shell[PATH_MAX];
        if (rootfs_shell(shell) == NULL) _exit(126);
        char *const arguments[] = {
            (char *)"sh", (char *)"-c", (char *)"--", (char *)command, NULL
        };
        execv(shell, arguments);
        _exit(127);
    }
    int parent_end = reading ? descriptors[0] : descriptors[1];
    close(reading ? descriptors[1] : descriptors[0]);
    if (type[1] != 'e') {
        int flags = fcntl(parent_end, F_GETFD);
        if (flags >= 0) (void)fcntl(parent_end, F_SETFD, flags & ~FD_CLOEXEC);
    }
    FILE *stream = fdopen(parent_end, reading ? "r" : "w");
    if (stream == NULL) {
        int saved = errno;
        close(parent_end);
        (void)waitpid(child, NULL, 0);
        errno = saved;
        return NULL;
    }
    pthread_mutex_lock(&shell_children_lock);
    for (size_t index = 0; index < SHELL_CHILD_SLOTS; ++index) {
        if (shell_children[index].stream != NULL) continue;
        shell_children[index].stream = stream;
        shell_children[index].pid = child;
        pthread_mutex_unlock(&shell_children_lock);
        return stream;
    }
    pthread_mutex_unlock(&shell_children_lock);
    fclose(stream);
    (void)waitpid(child, NULL, 0);
    errno = EMFILE;
    return NULL;
}

int pclose(FILE *stream) {
    pid_t child = -1;
    pthread_mutex_lock(&shell_children_lock);
    for (size_t index = 0; index < SHELL_CHILD_SLOTS; ++index) {
        if (shell_children[index].stream != stream) continue;
        child = shell_children[index].pid;
        shell_children[index].stream = NULL;
        shell_children[index].pid = 0;
        break;
    }
    pthread_mutex_unlock(&shell_children_lock);
    if (child < 0) { errno = EINVAL; return -1; }
    int close_result = fclose(stream);
    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) return -1;
    }
    return close_result == 0 ? status : -1;
}

int system(const char *command) {
    if (command == NULL) {
        char shell[PATH_MAX];
        return rootfs_shell(shell) != NULL && access(shell, X_OK) == 0;
    }
    pid_t child = fork();
    if (child < 0) return -1;
    if (child == 0) {
        char shell[PATH_MAX];
        if (rootfs_shell(shell) == NULL) _exit(126);
        char *const arguments[] = {
            (char *)"sh", (char *)"-c", (char *)"--", (char *)command, NULL
        };
        execv(shell, arguments);
        _exit(127);
    }
    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) return -1;
    }
    return status;
}
