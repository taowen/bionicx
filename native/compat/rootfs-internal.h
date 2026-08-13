#ifndef BIONICX_ROOTFS_INTERNAL_H
#define BIONICX_ROOTFS_INTERNAL_H

#define _GNU_SOURCE
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
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define BIONICX_INTERNAL __attribute__((visibility("hidden")))

BIONICX_INTERNAL const char *bionicx_redirect_path(
        const char *path, char buffer[PATH_MAX]);
BIONICX_INTERNAL mode_t bionicx_optional_mode(
        int flags, va_list arguments);
BIONICX_INTERNAL int bionicx_ignore_ownership_failure(int result);

#endif
