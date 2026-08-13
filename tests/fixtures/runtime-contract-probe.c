#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
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
    snprintf(path, sizeof(path), "%s/opt", root);
    make_directory(path);
    snprintf(path, sizeof(path), "%s/var", root);
    make_directory(path);
    snprintf(path, sizeof(path), "%s/var/lib", root);
    make_directory(path);
    snprintf(path, sizeof(path), "%s/var/lib/dpkg", root);
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

    snprintf(path, sizeof(path), "%s/opt/bionicx-probe", root);
    write_file(path, "opted\n", 0600);
    descriptor = open("/opt/bionicx-probe", O_RDONLY);
    if (descriptor < 0) fail("redirected /opt open");
    memset(value, 0, sizeof(value));
    if (read(descriptor, value, sizeof(value) - 1) != 6 ||
            strcmp(value, "opted\n") != 0) return 5;
    close(descriptor);

    snprintf(path, sizeof(path), "%s/usr/bin/bionicx-opt-link", root);
    if (setenv("BIONICX_REWRITE_ABSOLUTE_SYMLINKS", "1", 1) != 0)
        fail("setenv symlink policy");
    if (symlink("/opt/bionicx-probe", path) != 0) fail("redirected symlink");
    char link_target[PATH_MAX];
    ssize_t link_length = readlink(path, link_target, sizeof(link_target) - 1);
    if (link_length < 0) fail("readlink redirected symlink");
    link_target[link_length] = '\0';
    char expected_target[PATH_MAX];
    snprintf(expected_target, sizeof(expected_target), "%s/opt/bionicx-probe",
             root);
    if (strcmp(link_target, expected_target) != 0) return 7;

    DIR *directory = opendir("/opt");
    if (directory == NULL) fail("redirected opendir");
    if (closedir(directory) != 0) fail("closedir");

    write_file("/var/lib/dpkg/status", "status\n", 0600);
    char canonical[PATH_MAX];
    if (realpath("/var/lib/dpkg/status", canonical) == NULL)
        fail("redirected realpath");
    if (strcmp(canonical, "/var/lib/dpkg/status") != 0) return 8;

    write_file("/opt/bionicx-rename-source", "rename\n", 0600);
    if (rename("/opt/bionicx-rename-source",
               "/opt/bionicx-rename-target") != 0)
        fail("redirected rename");
    snprintf(path, sizeof(path), "%s/opt/bionicx-rename-target", root);
    if (access(path, F_OK) != 0) fail("renamed backing file");

    pid_t stat_child = fork();
    if (stat_child < 0) fail("fork stat probe");
    if (stat_child == 0) {
        char *const arguments[] = {
            (char *)"sh", (char *)"-c",
            (char *)"test -f /opt/bionicx-probe", NULL
        };
        execvp(arguments[0], arguments);
        _exit(120);
    }
    int stat_status = 0;
    if (waitpid(stat_child, &stat_status, 0) != stat_child ||
            !WIFEXITED(stat_status) || WEXITSTATUS(stat_status) != 0)
        return 6;

    char template[] = "/tmp/bionicx.XXXXXX";
    descriptor = mkstemp(template);
    if (descriptor < 0) fail("redirected mkstemp");
    close(descriptor);
    snprintf(path, sizeof(path), "%s%s", temporary, template + 4);
    if (access(path, F_OK) != 0) fail("mkstemp backing file");

    if (chown("/etc/bionicx-probe", 0, 0) != 0)
        fail("rootless chown policy");

    struct passwd *current_user = getpwuid(geteuid());
    if (current_user == NULL || current_user->pw_uid != geteuid())
        fail("current app user mapping");

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
    puts("runtime contract probe: PASS");
    return 0;
}
