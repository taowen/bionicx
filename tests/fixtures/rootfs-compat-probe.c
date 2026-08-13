#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void fail(const char *message) {
    perror(message);
    exit(1);
}

static void make_directory(const char *path) {
    if (mkdir(path, 0700) != 0 && errno != EEXIST) fail(path);
}

static void write_file(const char *path, const char *contents, mode_t mode) {
    int descriptor = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (descriptor < 0) fail(path);
    size_t length = strlen(contents);
    if (write(descriptor, contents, length) != (ssize_t)length) fail("write");
    if (close(descriptor) != 0) fail("close");
    if (chmod(path, mode) != 0) fail("chmod");
}

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    const char *root = argv[1];
    const char *temporary = argv[2];
    char path[PATH_MAX];

    snprintf(path, sizeof(path), "%s/etc", root);
    make_directory(path);
    snprintf(path, sizeof(path), "%s/bin", root);
    make_directory(path);
    snprintf(path, sizeof(path), "%s/usr", root);
    make_directory(path);
    snprintf(path, sizeof(path), "%s/usr/bin", root);
    make_directory(path);
    snprintf(path, sizeof(path), "%s/bin/sh", root);
    if (symlink("/bin/sh", path) != 0 && errno != EEXIST) fail("symlink sh");

    snprintf(path, sizeof(path), "%s/etc/bionicx-probe", root);
    write_file(path, "rooted\n", 0600);
    int descriptor = open("/etc/bionicx-probe", O_RDONLY);
    if (descriptor < 0) fail("redirected open");
    char value[8] = {0};
    if (read(descriptor, value, sizeof(value) - 1) != 7 ||
            strcmp(value, "rooted\n") != 0) return 3;
    close(descriptor);

    char template[] = "/tmp/bionicx.XXXXXX";
    descriptor = mkstemp(template);
    if (descriptor < 0) fail("redirected mkstemp");
    close(descriptor);
    snprintf(path, sizeof(path), "%s%s", temporary, template + 4);
    if (access(path, F_OK) != 0) fail("mkstemp backing file");

    if (chown("/etc/bionicx-probe", 0, 0) != 0)
        fail("rootless chown policy");

    snprintf(path, sizeof(path), "%s/usr/bin/bionicx-script", root);
    write_file(path, "#!/bin/sh\nexit 23\n", 0700);
    pid_t child = fork();
    if (child < 0) fail("fork");
    if (child == 0) {
        char *const arguments[] = {(char *)"bionicx-script", NULL};
        execvp(arguments[0], arguments);
        _exit(120);
    }
    int status = 0;
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status) ||
            WEXITSTATUS(status) != 23) return 4;
    puts("rootfs compatibility probe: PASS");
    return 0;
}
