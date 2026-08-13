#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Minimal CUPS file: backend. Copies the job to DEVICE_URI / file: path. */
static const char *destination_path(void) {
    const char *uri = getenv("DEVICE_URI");
    if (uri == NULL || strncmp(uri, "file:", 5) != 0) return NULL;
    uri += 5;
    if (uri[0] == '/' && uri[1] == '/') {
        const char *slash = strchr(uri + 2, '/');
        return slash;
    }
    return uri;
}

static int copy_fd(int from, int to) {
    char buffer[4096];
    for (;;) {
        ssize_t n = read(from, buffer, sizeof(buffer));
        if (n == 0) return 0;
        if (n < 0) return -1;
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(to, buffer + off, (size_t)(n - off));
            if (w < 0) return -1;
            off += w;
        }
    }
}

int main(int argc, char **argv) {
    if (argc == 1) {
        printf("file file \"Unknown\" \"BionicX file destination\"\n");
        return 0;
    }
    const char *path = destination_path();
    if (path == NULL || path[0] != '/') {
        fprintf(stderr, "file-backend: DEVICE_URI must be file:/path\n");
        return 1;
    }
    int in = 0;
    if (argc > 6) {
        in = open(argv[6], O_RDONLY);
        if (in < 0) {
            fprintf(stderr, "file-backend: open %s: %s\n", argv[6],
                    strerror(errno));
            return 1;
        }
    }
    int out = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) {
        fprintf(stderr, "file-backend: create %s: %s\n", path, strerror(errno));
        if (in > 0) close(in);
        return 1;
    }
    int rc = copy_fd(in, out);
    if (in > 0) close(in);
    if (close(out) != 0) rc = -1;
    if (rc != 0) {
        fprintf(stderr, "file-backend: copy failed: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}
