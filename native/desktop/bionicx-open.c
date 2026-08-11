#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int hex_value(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    value = (char)tolower((unsigned char)value);
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

static int decode_file_target(const char *argument, char **target) {
    static const char prefix[] = "file://";
    const char *source = argument;
    if (strncmp(source, prefix, sizeof(prefix) - 1) == 0)
        source += sizeof(prefix) - 1;

    char *decoded = strdup(source);
    if (decoded == NULL) return -1;
    char *output = decoded;
    while (*source != '\0') {
        if (*source == '%' && source[1] != '\0' && source[2] != '\0') {
            int high = hex_value(source[1]);
            int low = hex_value(source[2]);
            if (high >= 0 && low >= 0) {
                int byte = (high << 4) | low;
                if (byte == 0) {
                    free(decoded);
                    errno = EINVAL;
                    return -1;
                }
                *output++ = (char)byte;
                source += 3;
                continue;
            }
        }
        *output++ = *source++;
    }
    *output = '\0';
    *target = decoded;
    return 0;
}

static int has_pdf_suffix(const char *path) {
    size_t length = strlen(path);
    return length >= 4 && strcasecmp(path + length - 4, ".pdf") == 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "bionicx-open: expected one URI or path\n");
        return 2;
    }

    char *target = NULL;
    if (decode_file_target(argv[1], &target) != 0) {
        perror("bionicx-open: decode");
        return 3;
    }
    if (!has_pdf_suffix(target)) {
        fprintf(stderr, "bionicx-open: no handler for %s\n", target);
        free(target);
        return 4;
    }

    const char *handler = getenv("BIONICX_OPEN_PDF");
    if (handler == NULL || *handler == '\0') {
        fprintf(stderr, "bionicx-open: BIONICX_OPEN_PDF is not set\n");
        free(target);
        return 5;
    }

    char *const arguments[] = {(char *)handler, target, NULL};
    execv(handler, arguments);
    fprintf(stderr, "bionicx-open: cannot execute %s: %s\n", handler,
            strerror(errno));
    free(target);
    return 6;
}
