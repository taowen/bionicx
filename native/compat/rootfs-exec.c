#include "rootfs-internal.h"

#include <dlfcn.h>

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
