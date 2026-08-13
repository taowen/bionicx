#ifndef BIONICX_ROOTFS_INTERNAL_H
#define BIONICX_ROOTFS_INTERNAL_H

#define _GNU_SOURCE
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define BIONICX_INTERNAL __attribute__((visibility("hidden")))

static inline const char *bionicx_getenv(const char *name) {
    size_t name_length = 0;
    while (name[name_length] != '\0') ++name_length;
    for (char **item = environ; item != NULL && *item != NULL; ++item) {
        size_t index = 0;
        while (index < name_length && (*item)[index] == name[index]) ++index;
        if (index == name_length && (*item)[index] == '=') {
            return *item + index + 1;
        }
    }
    return NULL;
}

BIONICX_INTERNAL const char *bionicx_captured_rootfs(void);
BIONICX_INTERNAL const char *bionicx_captured_tmpdir(void);
BIONICX_INTERNAL const char *bionicx_redirect_path(
        const char *path, char buffer[PATH_MAX]);
BIONICX_INTERNAL mode_t bionicx_optional_mode(
        int flags, va_list arguments);
BIONICX_INTERNAL int bionicx_ignore_ownership_failure(int result);

#endif
