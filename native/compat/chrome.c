#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

typedef int (*execvp_function)(const char *file, char *const argv[]);

static execvp_function real_execvp;
static const char disable_crashpad[] = "--disable-crashpad-for-testing";

__attribute__((constructor)) static void initialize_chrome_compatibility(void) {
    real_execvp = (execvp_function)dlsym(RTLD_NEXT, "execvp");
}

/*
 * Official Chrome consumes --disable-crashpad-for-testing in the browser but
 * does not forward it to Linux subprocesses.  Those subprocesses initialize
 * Crashpad around inherited descriptors and trip ScopedFD ownership checking
 * in the Android app environment.  Propagate the existing browser policy to
 * Chrome children; do not disable Chromium's ownership enforcement itself.
 */
int execvp(const char *file, char *const argv[]) {
    if (real_execvp == NULL) {
        errno = ENOSYS;
        return -1;
    }

    size_t count = 0;
    int is_chrome_child = 0;
    int already_disabled = 0;
    while (argv[count] != NULL) {
        if (strncmp(argv[count], "--type=", 7) == 0) is_chrome_child = 1;
        if (strcmp(argv[count], disable_crashpad) == 0) already_disabled = 1;
        count++;
    }
    if (!is_chrome_child || already_disabled)
        return real_execvp(file, argv);

    char *child_argv[count + 2];
    for (size_t index = 0; index < count; index++)
        child_argv[index] = argv[index];
    child_argv[count] = (char *)disable_crashpad;
    child_argv[count + 1] = NULL;
    static const char message[] =
            "BIONICX propagated --disable-crashpad-for-testing to Chrome child\n";
    (void)write(STDERR_FILENO, message, sizeof(message) - 1);
    return real_execvp(file, child_argv);
}
