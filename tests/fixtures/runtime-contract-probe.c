#include <errno.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <shadow.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sem.h>
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

    void *dlopen_handle = dlopen("/opt/bionicx-runtime-dlopen.so", RTLD_NOW);
    if (dlopen_handle == NULL) fail("redirected dlopen");
    int (*dlopen_probe)(void) = dlsym(dlopen_handle,
                                      "bionicx_runtime_dlopen_probe");
    if (dlopen_probe == NULL || dlopen_probe() != 42)
        fail("redirected dlopen symbol");
    dlclose(dlopen_handle);

    pid_t chroot_child = fork();
    if (chroot_child < 0) fail("fork chroot contract");
    if (chroot_child == 0) {
        if (chroot(root) != 0) _exit(125);
        _exit(getuid() == 0 && geteuid() == 0 && getgid() == 0 &&
                      getegid() == 0 ? 0 : 124);
    }
    int chroot_status = 0;
    if (waitpid(chroot_child, &chroot_status, 0) != chroot_child ||
            !WIFEXITED(chroot_status) || WEXITSTATUS(chroot_status) != 0)
        fail("rootfs chroot execution");
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

    snprintf(path, sizeof(path), "%s/etc/resolv.conf", root);
    descriptor = open(path, O_RDONLY);
    if (descriptor < 0) fail("runtime resolv.conf");
    char resolver_config[128] = {0};
    ssize_t resolver_length = read(descriptor, resolver_config,
                                   sizeof(resolver_config) - 1);
    close(descriptor);
    if (resolver_length <= 0 ||
            strstr(resolver_config, "nameserver 127.0.0.53\n") == NULL ||
            strstr(resolver_config, "nameserver 127.0.0.54\n") == NULL)
        fail("Android resolver publication");

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

    /* shadow-tools uses a hard-link lock before replacing account files. */
    snprintf(path, sizeof(path), "%s/etc/group", root);
    write_file(path, "root:x:0:\n", 0600);
    if (link("/etc/group", "/etc/group.lock") != 0)
        fail("redirected group lock link");
    snprintf(path, sizeof(path), "%s/etc/group.lock", root);
    if (access(path, F_OK) != 0) fail("group lock backing file");
    if (unlink("/etc/group.lock") != 0) fail("redirected group lock unlink");
    if (linkat(AT_FDCWD, "/etc/group", AT_FDCWD, "/etc/group.lock", 0) != 0)
        fail("redirected group lock linkat");
    if (unlinkat(AT_FDCWD, "/etc/group.lock", 0) != 0)
        fail("redirected group lock unlinkat");

    FILE *group_stream = fopen("/etc/group", "r+");
    if (group_stream == NULL) fail("fopen /etc/group r+");
    if (fseek(group_stream, 0, SEEK_END) != 0 ||
            fputs("bionicx-probe:x:12345:\n", group_stream) < 0)
        fail("write /etc/group");
    if (fclose(group_stream) != 0) fail("fclose /etc/group");
    snprintf(path, sizeof(path), "%s/etc/group", root);
    group_stream = fopen(path, "r");
    if (group_stream == NULL) fail("backing /etc/group");
    char group_file[256] = {0};
    if (fread(group_file, 1, sizeof(group_file) - 1, group_stream) <= 0 ||
            strstr(group_file, "bionicx-probe:x:12345:\n") == NULL)
        fail("group write stayed in rootfs");
    fclose(group_stream);

    int (*open2)(const char *, int) = dlsym(RTLD_DEFAULT, "__open_2");
    if (open2 == NULL) fail("__open_2 symbol");
    int group_fd = open2("/etc/group", O_RDWR);
    if (group_fd < 0) fail("fortified open /etc/group");
    close(group_fd);

    if (lckpwdf() != 0) fail("lckpwdf");
    snprintf(path, sizeof(path), "%s/etc/.pwd.lock", root);
    if (access(path, F_OK) != 0) fail("lckpwdf backing file");
    if (ulckpwdf() != 0) fail("ulckpwdf");

    pid_t unlocked_child = fork();
    if (unlocked_child < 0) fail("fork sanitized fopen");
    if (unlocked_child == 0) {
        unsetenv("LD_PRELOAD");
        unsetenv("BIONICX_ROOTFS");
        unsetenv("BIONICX_TMPDIR");
        group_stream = fopen("/etc/group", "r+");
        if (group_stream == NULL) _exit(116);
        fclose(group_stream);
        if (lckpwdf() != 0) _exit(115);
        if (ulckpwdf() != 0) _exit(114);
        _exit(0);
    }
    int unlocked_status = 0;
    if (waitpid(unlocked_child, &unlocked_status, 0) != unlocked_child ||
            !WIFEXITED(unlocked_status) || WEXITSTATUS(unlocked_status) != 0)
        fail("sanitized fopen/lckpwdf still uses captured rootfs");

    pid_t execve_child = fork();
    if (execve_child < 0) fail("fork execve contract");
    if (execve_child == 0) {
        char shell[PATH_MAX];
        snprintf(shell, sizeof(shell), "%s/bin/sh", root);
        char *const arguments[] = {
            (char *)"sh", (char *)"-c",
            (char *)"test -f /etc/bionicx-probe", NULL
        };
        char *const sanitized[] = {
            (char *)"PATH=/usr/bin:/bin",
            (char *)"HOME=/tmp",
            NULL
        };
        unsetenv("LD_PRELOAD");
        unsetenv("BIONICX_ROOTFS");
        unsetenv("BIONICX_TMPDIR");
        /* Path redirection must keep working after helpers sanitize environ. */
        if (access("/etc/bionicx-probe", R_OK) != 0) _exit(118);
        execve(shell, arguments, sanitized);
        _exit(119);
    }
    int execve_status = 0;
    if (waitpid(execve_child, &execve_status, 0) != execve_child ||
            !WIFEXITED(execve_status) || WEXITSTATUS(execve_status) != 0)
        fail("execve preserves runtime environment");

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

    FILE *shell_stream = popen("printf bionicx-popen", "r");
    if (shell_stream == NULL) fail("popen rootfs shell");
    char shell_output[32] = {0};
    const size_t shell_output_length = sizeof("bionicx-popen") - 1;
    if (fread(shell_output, 1, shell_output_length, shell_stream) !=
            shell_output_length ||
            strcmp(shell_output, "bionicx-popen") != 0 ||
            pclose(shell_stream) != 0)
        fail("rootfs popen result");
    int system_status = system("exit 17");
    if (!WIFEXITED(system_status) || WEXITSTATUS(system_status) != 17)
        fail("rootfs system result");

    char template[] = "/tmp/bionicx.XXXXXX";
    descriptor = mkstemp(template);
    if (descriptor < 0) fail("redirected mkstemp");
    close(descriptor);
    snprintf(path, sizeof(path), "%s%s", temporary, template + 4);
    if (access(path, F_OK) != 0) fail("mkstemp backing file");

    if (mkdir("/run/bionicx-contract", 0700) != 0)
        fail("redirected /run mkdir");
    snprintf(path, sizeof(path), "%s/run/bionicx-contract", temporary);
    if (access(path, F_OK) != 0) fail("redirected /run backing directory");

    if (chown("/etc/bionicx-probe", 0, 0) != 0)
        fail("rootless chown policy");
    if (chown("/etc/bionicx-probe", 999, 999) != 0)
        fail("unmapped Debian ownership policy");

    struct passwd *current_user = getpwuid(geteuid());
    if (current_user == NULL || current_user->pw_uid != geteuid())
        fail("current app user mapping");

    int semaphore = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    if (semaphore < 0) fail("semget");
    if (semctl(semaphore, 0, SETVAL, 0) != 0) fail("semctl SETVAL");
    pid_t semaphore_child = fork();
    if (semaphore_child < 0) fail("fork semaphore");
    if (semaphore_child == 0) {
        struct sembuf increment = {.sem_num = 0, .sem_op = 1};
        _exit(semop(semaphore, &increment, 1) == 0 ? 0 : 121);
    }
    int semaphore_status = 0;
    if (waitpid(semaphore_child, &semaphore_status, 0) != semaphore_child ||
            !WIFEXITED(semaphore_status) || WEXITSTATUS(semaphore_status) != 0 ||
            semctl(semaphore, 0, GETVAL) != 1 ||
            semctl(semaphore, 0, IPC_RMID) != 0)
        fail("cross-process System V semaphore");

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
