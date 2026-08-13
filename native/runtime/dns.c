#define _GNU_SOURCE
#include "runtime-internal.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int write_all(int descriptor, const char *buffer, size_t length) {
    while (length != 0) {
        ssize_t written = write(descriptor, buffer, length);
        if (written <= 0) return -1;
        buffer += written;
        length -= (size_t)written;
    }
    return 0;
}

static void publish_android_resolver_config(void) {
    const char *root = bionicx_getenv("BIONICX_ROOTFS");
    const char *configured = bionicx_getenv("BIONICX_DNS_SERVERS");
    if (root == NULL || root[0] != '/' || configured == NULL ||
            configured[0] == '\0') return;

    char path[PATH_MAX], temporary[PATH_MAX];
    if (snprintf(path, sizeof(path), "%s/etc/resolv.conf", root) >=
            (int)sizeof(path)) return;
    if (snprintf(temporary, sizeof(temporary),
                 "%s/etc/.resolv.conf.bionicx.%ld", root, (long)getpid()) >=
            (int)sizeof(temporary)) return;
    int descriptor = open(temporary,
                          O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (descriptor < 0) return;

    char servers[256];
    size_t length = strlen(configured);
    if (length >= sizeof(servers)) {
        close(descriptor);
        unlink(temporary);
        return;
    }
    memcpy(servers, configured, length + 1);
    int count = 0;
    char *save = NULL;
    for (char *item = strtok_r(servers, ",", &save); item != NULL;
         item = strtok_r(NULL, ",", &save)) {
        struct in6_addr address;
        if (inet_pton(AF_INET, item, &address) != 1 &&
                inet_pton(AF_INET6, item, &address) != 1) continue;
        char line[INET6_ADDRSTRLEN + 16];
        int line_length = snprintf(line, sizeof(line), "nameserver %s\n", item);
        if (line_length <= 0 ||
                write_all(descriptor, line, (size_t)line_length) != 0) {
            close(descriptor);
            unlink(temporary);
            return;
        }
        ++count;
    }
    if (close(descriptor) != 0 || count == 0 || rename(temporary, path) != 0)
        unlink(temporary);
}

__attribute__((constructor)) static void initialize_android_dns(void) {
    publish_android_resolver_config();
}
