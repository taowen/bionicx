#include "runtime-internal.h"

#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>

#define SHELL_CHILD_SLOTS 64

struct shell_child {
    FILE *stream;
    pid_t pid;
};

static pthread_mutex_t shell_children_lock = PTHREAD_MUTEX_INITIALIZER;
static struct shell_child shell_children[SHELL_CHILD_SLOTS];

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

static int exec_script(const char *path, char *const arguments[],
                       int (*execute)(const char *, char *const[])) {
    char program[PATH_MAX], interpreter_argument[PATH_MAX];
    char **script = script_arguments(path, arguments, program,
                                     interpreter_argument);
    if (script == NULL) return execute(path, arguments);
    int result = execute(program, script);
    int saved_errno = errno;
    free(script);
    errno = saved_errno;
    return result;
}

int execv(const char *path, char *const arguments[]) {
    static int (*next)(const char *, char *const[]);
    if (next == NULL) next = dlsym(RTLD_NEXT, "execv");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
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
    char program[PATH_MAX], interpreter_argument[PATH_MAX];
    char **script = script_arguments(actual, arguments, program,
                                     interpreter_argument);
    if (script == NULL) return next(actual, arguments, environment);
    int result = next(program, script, environment);
    int saved_errno = errno;
    free(script);
    errno = saved_errno;
    return result;
}

int execvp(const char *path, char *const arguments[]) {
    static int (*next)(const char *, char *const[]);
    if (next == NULL) next = dlsym(RTLD_NEXT, "execvp");
    char buffer[PATH_MAX];
    const char *actual = bionicx_redirect_path(path, buffer);
    if (actual == NULL) return -1;
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
