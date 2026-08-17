#define _GNU_SOURCE
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
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <poll.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stddef.h>
#include <sys/types.h>
#include <spawn.h>
#include <sys/wait.h>
#include <pty.h>
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
    if (argc >= 2 && strncmp(argv[1], "--type=", 7) == 0) {
        int has_crashpad = 0;
        const char *last_angle = NULL;
        for (int i = 0; i < argc; ++i) {
            if (strcmp(argv[i], "--disable-crashpad-for-testing") == 0)
                has_crashpad = 1;
            if (strncmp(argv[i], "--use-angle=", 12) == 0)
                last_angle = argv[i];
        }
        if (strcmp(argv[1], "--type=gpu-process") == 0) {
            for (int i = 0; i < argc; ++i)
                if (strcmp(argv[i], "--disable-gpu-compositing") == 0)
                    return 25;
            if (last_angle != NULL &&
                    strcmp(last_angle, "--use-angle=vulkan") == 0)
                return 0;
            return 23;
        }
        if (has_crashpad) return 0;
        return 21;
    }
    if (argc >= 2 && strcmp(argv[1], "--print-home") == 0) {
        const char *home = getenv("HOME");
        if (home == NULL) home = "";
        fwrite(home, 1, strlen(home), stdout);
        return home[0] != '\0' ? 0 : 3;
    }
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

    void *soname_handle = dlopen("libbionicx-runtime-dlopen.so", RTLD_NOW);
    if (soname_handle == NULL) fail("soname dlopen");
    int (*soname_probe)(void) = dlsym(soname_handle,
                                      "bionicx_runtime_dlopen_probe");
    if (soname_probe == NULL || soname_probe() != 42)
        fail("soname dlopen symbol");
    dlclose(soname_handle);

    void *missing_path = dlopen(
            "/opt/no-such-app/libbionicx-runtime-dlopen.so", RTLD_NOW);
    if (missing_path == NULL) fail("missing-path multiarch dlopen");
    int (*missing_probe)(void) = dlsym(missing_path,
                                       "bionicx_runtime_dlopen_probe");
    if (missing_probe == NULL || missing_probe() != 42)
        fail("missing-path multiarch dlopen symbol");
    dlclose(missing_path);

    void *unversioned = dlopen("/opt/bytedance/feishu/libbionicx-so-one.so",
                               RTLD_NOW);
    if (unversioned == NULL) fail("libfoo.so to libfoo.so.1 dlopen");
    int (*unversioned_probe)(void) = dlsym(unversioned,
                                           "bionicx_runtime_dlopen_probe");
    if (unversioned_probe == NULL || unversioned_probe() != 42)
        fail("libfoo.so.1 fallback symbol");
    dlclose(unversioned);

    void *app_handle = dlopen("libbionicx-app-dlopen.so", RTLD_NOW);
    if (app_handle == NULL) fail("app-lib soname dlopen");
    int (*app_probe)(void) = dlsym(app_handle, "bionicx_runtime_dlopen_probe");
    if (app_probe == NULL || app_probe() != 42)
        fail("app-lib soname dlopen symbol");
    dlclose(app_handle);

    /* temporary is .../tmp; GreD fixtures sit at .../gred. */
    char gred_nss3[PATH_MAX];
    if (snprintf(gred_nss3, sizeof(gred_nss3), "%s/../gred/libnss3.so",
                 temporary) >= (int)sizeof(gred_nss3))
        fail("gred nss3 path");
    void *nss3 = dlopen(gred_nss3, RTLD_NOW);
    if (nss3 == NULL) fail("gred libnss3");
    void *softokn = dlopen("libsoftokn3.so", RTLD_NOW);
    if (softokn == NULL) fail("nss-adjacent libsoftokn3");
    int (*softokn_marker)(void) = dlsym(softokn, "bionicx_softokn_marker");
    if (softokn_marker == NULL || softokn_marker() != 1)
        fail("libsoftokn3 must come from GreD next to libnss3");
    dlclose(softokn);
    dlclose(nss3);

    void *app_gl = dlopen("libbionicx-app-gl.so", RTLD_NOW);
    if (app_gl == NULL) fail("app-payload libGL");
    int (*app_gl_marker)(void) = dlsym(app_gl, "bionicx_softokn_marker");
    if (app_gl_marker == NULL || app_gl_marker() != 7)
        fail("libGL must come from BIONICX_APP/lib before multiarch");
    dlclose(app_gl);

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
    snprintf(path, sizeof(path), "%s/usr/share", root);
    make_directory(path);
    snprintf(path, sizeof(path), "%s/usr/share/krita", root);
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

    int (*audit_open_fn)(void) = dlsym(RTLD_DEFAULT, "audit_open");
    int (*audit_close_fn)(int) = dlsym(RTLD_DEFAULT, "audit_close");
    int (*audit_log_fn)(int, int, const char *, const char *, const char *,
                        unsigned int, const char *, const char *, const char *,
                        int) = dlsym(RTLD_DEFAULT, "audit_log_acct_message");
    if (audit_open_fn == NULL || audit_close_fn == NULL || audit_log_fn == NULL)
        fail("audit symbols");
    int audit_fd = audit_open_fn();
    if (audit_fd < 0) fail("audit_open");
    if (audit_log_fn(audit_fd, 1100, "probe", "adding-group",
                     "bionicx", 0, NULL, NULL, NULL, 1) <= 0)
        fail("audit_log_acct_message");
    if (audit_close_fn(audit_fd) != 0) fail("audit_close");

    write_file("/usr/share/krita/marker", "bundles\n", 0600);
    struct statvfs vfs;
    if (statvfs("/usr/share/krita", &vfs) != 0)
        fail("redirected statvfs /usr/share/krita");
    if (vfs.f_blocks == 0) fail("statvfs /usr/share/krita has no blocks");
    if (statvfs("/", &vfs) != 0) fail("statvfs /");
    if (vfs.f_bavail == 0) fail("statvfs / must not report a full Android /");

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

    /* dpkg does link(status, status-old). Some Android f2fs app-data
     * volumes deny hard links; the runtime must copy instead. */
    if (setenv("BIONICX_FORCE_LINK_COPY", "1", 1) != 0)
        fail("setenv BIONICX_FORCE_LINK_COPY");
    if (link("/var/lib/dpkg/status", "/var/lib/dpkg/status-old") != 0)
        fail("dpkg status-old copy fallback");
    char status_path[PATH_MAX];
    char status_old[PATH_MAX];
    snprintf(status_path, sizeof(status_path), "%s/var/lib/dpkg/status", root);
    snprintf(status_old, sizeof(status_old), "%s/var/lib/dpkg/status-old", root);
    struct stat status_info, status_old_info;
    if (lstat(status_path, &status_info) != 0 ||
            lstat(status_old, &status_old_info) != 0)
        fail("status-old lstat");
    if (status_info.st_nlink != 2 || status_old_info.st_nlink != 2)
        fail("copied group lock must report nlink 2");
    if (status_info.st_ino != status_old_info.st_ino)
        fail("copied group lock must share inode");
    if (status_info.st_size != status_old_info.st_size)
        fail("status-old copy size");
    if (unlink("/var/lib/dpkg/status-old") != 0) fail("status-old unlink");
    if (linkat(AT_FDCWD, "/var/lib/dpkg/status", AT_FDCWD,
               "/var/lib/dpkg/status-old", 0) != 0)
        fail("dpkg status-old linkat copy fallback");
    if (lstat(status_old, &status_old_info) != 0) fail("status-old linkat lstat");
    if (status_old_info.st_nlink != 2)
        fail("status-old linkat must report nlink 2");
    if (status_info.st_ino != status_old_info.st_ino)
        fail("status-old linkat must share inode");
    if (unlink("/var/lib/dpkg/status-old") != 0) fail("status-old linkat unlink");
    int status_fd = open("/var/lib/dpkg/status", O_RDONLY);
    if (status_fd < 0) fail("open status for AT_EMPTY_PATH");
    if (linkat(status_fd, "", AT_FDCWD, "/var/lib/dpkg/status-tmpfile",
               AT_EMPTY_PATH) != 0)
        fail("QSaveFile AT_EMPTY_PATH copy fallback");
    close(status_fd);
    snprintf(status_old, sizeof(status_old), "%s/var/lib/dpkg/status-tmpfile",
             root);
    if (lstat(status_old, &status_old_info) != 0) fail("status-tmpfile lstat");
    if (status_old_info.st_nlink != 2)
        fail("status-tmpfile must report nlink 2");
    if (status_info.st_ino != status_old_info.st_ino)
        fail("status-tmpfile must share inode");
    if (unlink("/var/lib/dpkg/status-tmpfile") != 0)
        fail("status-tmpfile unlink");
    unsetenv("BIONICX_FORCE_LINK_COPY");

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
        char resolved[PATH_MAX];
        if (realpath("/var/lib/dpkg/status", resolved) == NULL ||
                strcmp(resolved, "/var/lib/dpkg/status") != 0)
            _exit(113);
        int sysfd = (int)syscall(SYS_openat, AT_FDCWD, "/etc/group",
                                 O_RDONLY, 0);
        if (sysfd < 0) _exit(112);
        close(sysfd);
        unsetenv("SSL_CERT_FILE");
        unsetenv("SSL_CERT_DIR");
        unsetenv("NODE_EXTRA_CA_CERTS");
        unsetenv("SHELL");
        unsetenv("HOME");
        unsetenv("PATH");
        unsetenv("LANG");
        if (getenv("SSL_CERT_FILE") == NULL ||
                strcmp(getenv("SSL_CERT_FILE"), "/captured-cert") != 0)
            _exit(111);
        if (getenv("SHELL") == NULL || strcmp(getenv("SHELL"), "/bin/sh") != 0)
            _exit(110);
        if (getenv("HOME") == NULL ||
                strcmp(getenv("HOME"), "/captured-home") != 0)
            _exit(109);
        clearenv();
        if (getenv("SHELL") == NULL || strcmp(getenv("SHELL"), "/bin/sh") != 0)
            _exit(108);
        int found_shell = 0;
        for (char **item = environ; item != NULL && *item != NULL; ++item) {
            if (strncmp(*item, "SHELL=", 6) == 0) found_shell = 1;
        }
        if (!found_shell) _exit(107); /* clearenv keeps SHELL */
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
    pid_t helper_child = fork();
    if (helper_child < 0) fail("fork child-flags helper");
    if (helper_child == 0) {
        char *const helper[] = {
            (char *)"/proc/self/exe", (char *)"--type=utility", NULL
        };
        execv("/proc/self/exe", helper);
        _exit(22);
    }
    int helper_status = 0;
    if (waitpid(helper_child, &helper_status, 0) != helper_child ||
            !WIFEXITED(helper_status) || WEXITSTATUS(helper_status) != 0)
        fail("BIONICX_CHILD_FLAGS on --type= helper");
    pid_t gpu_child = fork();
    if (gpu_child < 0) fail("fork child-flags gpu last-wins");
    if (gpu_child == 0) {
        char *const helper[] = {
            (char *)"/proc/self/exe",
            (char *)"--type=gpu-process",
            (char *)"--use-angle=vulkan",
            (char *)"--use-angle=swiftshader-webgl",
            (char *)"--disable-gpu-compositing",
            NULL
        };
        execv("/proc/self/exe", helper);
        _exit(24);
    }
    int gpu_status = 0;
    if (waitpid(gpu_child, &gpu_status, 0) != gpu_child ||
            !WIFEXITED(gpu_status) || WEXITSTATUS(gpu_status) != 0)
        fail("BIONICX_CHILD_FLAGS last-wins --use-angle=");
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

    struct stat shm_dir;
    if (stat("/dev/shm", &shm_dir) != 0) fail("stat /dev/shm");
    if (!S_ISDIR(shm_dir.st_mode) || (shm_dir.st_mode & S_ISVTX) == 0)
        fail("sticky /dev/shm");
    int shm_file = open("/dev/shm/bionicx-contract",
                        O_CREAT | O_RDWR | O_EXCL, 0600);
    if (shm_file < 0) fail("redirected /dev/shm open");
    close(shm_file);
    snprintf(path, sizeof(path), "%s/dev-shm/bionicx-contract", temporary);
    if (access(path, F_OK) != 0) fail("redirected /dev/shm backing file");
    if (unlink("/dev/shm/bionicx-contract") != 0)
        fail("redirected /dev/shm unlink");
    int posix_shm = shm_open("/bionicx-contract",
                             O_CREAT | O_EXCL | O_RDWR, 0600);
    if (posix_shm < 0) fail("shm_open");
    close(posix_shm);
    if (shm_unlink("/bionicx-contract") != 0) fail("shm_unlink");
    char shm_template[] = "/dev/shm/tmpfileXXXXXX";
    int shm_temp = mkstemp64(shm_template);
    if (shm_temp < 0) fail("mkstemp64 /dev/shm");
    close(shm_temp);
    if (unlink(shm_template) != 0) fail("unlink mkstemp64 /dev/shm");

    if (chown("/etc/bionicx-probe", 0, 0) != 0)
        fail("rootless chown policy");
    if (chown("/etc/bionicx-probe", 999, 999) != 0)
        fail("unmapped Debian ownership policy");

    struct passwd *current_user = getpwuid(geteuid());
    if (current_user == NULL || current_user->pw_uid != geteuid())
        fail("current app user mapping");
    if (current_user->pw_shell == NULL ||
            strcmp(current_user->pw_shell, "/bin/sh") != 0)
        fail("current app user shell");
    if (setenv("SHELL", "/bin/bash", 1) != 0)
        fail("setenv SHELL");
    current_user = getpwuid(geteuid());
    if (current_user == NULL || current_user->pw_shell == NULL ||
            strcmp(current_user->pw_shell, "/bin/bash") != 0)
        fail("SHELL overrides app user shell");
    if (setenv("SHELL", "/system/bin/sh", 1) != 0)
        fail("setenv Android SHELL");
    current_user = getpwuid(geteuid());
    if (current_user == NULL || current_user->pw_shell == NULL ||
            strcmp(current_user->pw_shell, "/bin/sh") != 0)
        fail("Android SHELL stays guest /bin/sh");
    if (setenv("SHELL", "/bin/sh", 1) != 0)
        fail("restore SHELL");
    struct statx shell_info;
    memset(&shell_info, 0, sizeof(shell_info));
    if (statx(AT_FDCWD, "/bin/sh", 0, STATX_TYPE | STATX_MODE, &shell_info) != 0)
        fail("statx guest /bin/sh");
    int watches = open("/proc/sys/fs/inotify/max_user_watches", O_RDONLY);
    if (watches < 0) fail("inotify max_user_watches");
    char watch_limit[16] = {0};
    if (read(watches, watch_limit, sizeof(watch_limit) - 1) <= 0 ||
            strstr(watch_limit, "65536") == NULL)
        fail("inotify max_user_watches value");
    close(watches);
    int proc_stat = open("/proc/stat", O_RDONLY);
    if (proc_stat < 0) fail("open /proc/stat");
    char proc_stat_text[256] = {0};
    if (read(proc_stat, proc_stat_text, sizeof(proc_stat_text) - 1) <= 0 ||
            strstr(proc_stat_text, "cpu ") == NULL ||
            strstr(proc_stat_text, "btime 0") == NULL)
        fail("read /proc/stat");
    close(proc_stat);
    int shells = open("/etc/shells", O_RDONLY);
    if (shells < 0) fail("open /etc/shells");
    char shell_list[512] = {0};
    if (read(shells, shell_list, sizeof(shell_list) - 1) <= 0)
        fail("read /etc/shells");
    close(shells);
    if (strstr(shell_list, root) == NULL ||
            strstr(shell_list, "/usr/bin/bash") == NULL)
        fail("rooted /etc/shells");
    int notify = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
    if (notify < 0) fail("inotify_init1");
    if (inotify_add_watch(notify, "/bin", IN_CREATE | IN_ATTRIB) < 0)
        fail("inotify_add_watch guest /bin");
    write_file("/bin/bionicx-inotify-marker", "watched\n", 0600);
    struct pollfd ready = { .fd = notify, .events = POLLIN };
    if (poll(&ready, 1, 1000) <= 0)
        fail("inotify event for guest /bin");
    close(notify);
    struct ifaddrs *interfaces = NULL;
    if (getifaddrs(&interfaces) != 0) fail("getifaddrs");
    if (interfaces == NULL || interfaces->ifa_name == NULL)
        fail("getifaddrs interfaces");
    freeifaddrs(interfaces);

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

    struct shminfo shm_limits;
    memset(&shm_limits, 0, sizeof(shm_limits));
    if (shmctl(0, IPC_INFO, (struct shmid_ds *)&shm_limits) != 0 ||
            shm_limits.shmmax < 4096)
        fail("shmctl IPC_INFO");
    int shm = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0600);
    if (shm < 0) fail("shmget");
    char *shm_bytes = shmat(shm, NULL, 0);
    if (shm_bytes == (void *)-1) fail("shmat");
    memcpy(shm_bytes, "BXSH", 4);
    if (shmctl(shm, IPC_RMID, NULL) != 0) fail("shmctl IPC_RMID");
    int shm_client = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (shm_client < 0) fail("SHM export socket");
    struct sockaddr_un shm_address;
    memset(&shm_address, 0, sizeof(shm_address));
    shm_address.sun_family = AF_UNIX;
    int shm_name = snprintf(&shm_address.sun_path[1],
                            sizeof(shm_address.sun_path) - 1,
                            "/dev/shm/%08x",
                            (unsigned int)shm / 0x10000u);
    if (shm_name <= 0) fail("SHM export name");
    socklen_t shm_length = (socklen_t)(offsetof(struct sockaddr_un, sun_path) +
                                       1 + shm_name);
    int connected = -1;
    for (int attempt = 0; attempt < 50 && connected != 0; ++attempt) {
        connected = connect(shm_client, (struct sockaddr *)&shm_address,
                            shm_length);
        if (connected != 0) usleep(20000);
    }
    if (connected != 0) fail("SHM export connect");
    if (send(shm_client, &shm, sizeof(shm), 0) != (ssize_t)sizeof(shm))
        fail("SHM export send id");
    key_t shm_key = 0;
    if (read(shm_client, &shm_key, sizeof(shm_key)) != (ssize_t)sizeof(shm_key))
        fail("SHM export key");
    char shm_payload = 0;
    struct iovec shm_iov = {.iov_base = &shm_payload, .iov_len = 1};
    union {
        struct cmsghdr header;
        char bytes[CMSG_SPACE(sizeof(int))];
    } shm_control;
    memset(&shm_control, 0, sizeof(shm_control));
    struct msghdr shm_message = {
        .msg_iov = &shm_iov,
        .msg_iovlen = 1,
        .msg_control = shm_control.bytes,
        .msg_controllen = sizeof(shm_control.bytes),
    };
    if (recvmsg(shm_client, &shm_message, 0) != 1)
        fail("SHM export fd");
    struct cmsghdr *shm_header = CMSG_FIRSTHDR(&shm_message);
    int shm_fd = -1;
    if (shm_header == NULL || shm_header->cmsg_level != SOL_SOCKET ||
            shm_header->cmsg_type != SCM_RIGHTS)
        fail("SHM export SCM_RIGHTS");
    memcpy(&shm_fd, CMSG_DATA(shm_header), sizeof(shm_fd));
    close(shm_client);
    if (shm_fd < 0) fail("SHM export descriptor");
    char *imported = mmap(NULL, 4096, PROT_READ, MAP_SHARED, shm_fd, 0);
    if (imported == MAP_FAILED) fail("SHM export mmap");
    if (memcmp(imported, "BXSH", 4) != 0)
        fail("MIT-SHM fd import");
    munmap(imported, 4096);
    close(shm_fd);
    if (shmdt(shm_bytes) != 0) fail("shmdt");

    char etc_link[PATH_MAX];
    if (readlink("/etc", etc_link, sizeof(etc_link) - 1) >= 0)
        fail("readlink /etc must use the rootfs directory, not a host symlink");

    snprintf(path, sizeof(path), "%s/usr/bin/bionicx-spawn-marker", root);
    write_file(path, "#!/bin/sh\nprintf spawned > /etc/bionicx-spawned\n", 0700);
    pid_t spawn_pid = -1;
    char *const spawn_arguments[] = {
        (char *)"bionicx-spawn-marker", NULL
    };
    char *const spawn_environment[] = {
        (char *)"PATH=/usr/bin:/bin",
        (char *)"HOME=/tmp",
        NULL
    };
    int saved_stdin = dup(STDIN_FILENO);
    int null_stdin = open("/dev/null", O_RDONLY);
    if (saved_stdin < 0 || null_stdin < 0) fail("forkpty stdin setup");
    if (dup2(null_stdin, STDIN_FILENO) < 0) fail("dup2 /dev/null");
    close(null_stdin);
    int pty_master = -1;
    pid_t pty_child = forkpty(&pty_master, NULL, NULL, NULL);
    if (pty_child == 0) {
        char tty_flag = isatty(STDIN_FILENO) ? '1' : '0';
        if (write(STDOUT_FILENO, &tty_flag, 1) != 1) _exit(121);
        _exit(0);
    }
    if (dup2(saved_stdin, STDIN_FILENO) < 0) fail("restore stdin");
    close(saved_stdin);
    if (pty_child < 0) fail("forkpty");
    char tty_flag = 0;
    if (read(pty_master, &tty_flag, 1) != 1) fail("forkpty master read");
    int pty_status = 0;
    if (waitpid(pty_child, &pty_status, 0) != pty_child ||
            !WIFEXITED(pty_status) || WEXITSTATUS(pty_status) != 0 ||
            tty_flag != '1')
        fail("forkpty child stdin is not a tty");
    close(pty_master);
    snprintf(path, sizeof(path), "%s/usr/bin/bionicx-isatty-std", root);
    write_file(path,
               "#!/bin/sh\n"
               "[ -t 0 ] && [ -t 1 ] && [ -t 2 ] || exit 9\n"
               "printf all-ttys\n",
               0700);
    int std_master = -1;
    pid_t std_child = forkpty(&std_master, NULL, NULL, NULL);
    if (std_child == 0) {
        int nullfd = open("/dev/null", O_RDWR);
        if (nullfd < 0) _exit(121);
        if (dup2(nullfd, STDOUT_FILENO) < 0 || dup2(nullfd, STDERR_FILENO) < 0)
            _exit(121);
        if (nullfd > STDERR_FILENO) close(nullfd);
        execl("/usr/bin/bionicx-isatty-std", "bionicx-isatty-std", (char *)NULL);
        _exit(121);
    }
    if (std_child < 0) fail("forkpty isatty-std");
    char std_buf[16] = {0};
    ssize_t std_n = read(std_master, std_buf, sizeof(std_buf) - 1);
    int std_status = 0;
    if (waitpid(std_child, &std_status, 0) != std_child ||
            !WIFEXITED(std_status) || WEXITSTATUS(std_status) != 0 ||
            std_n < 8 || memcmp(std_buf, "all-ttys", 8) != 0)
        fail("exec must copy the pty onto stdout and stderr");
    close(std_master);
#ifndef CLOSE_RANGE_CLOEXEC
#define CLOSE_RANGE_CLOEXEC 4u
#endif
#ifdef SYS_close_range
    if (syscall(SYS_close_range, 0, ~0U, CLOSE_RANGE_CLOEXEC) == 0) {
        int stdin_flags = fcntl(STDIN_FILENO, F_GETFD);
        if (stdin_flags < 0 || (stdin_flags & FD_CLOEXEC) != 0)
            fail("close_range must not CLOEXEC stdin");
    }
#endif

    if (posix_spawn(&spawn_pid, "/usr/bin/bionicx-spawn-marker", NULL, NULL,
                    spawn_arguments, spawn_environment) != 0)
        fail("posix_spawn rootfs helper");
    int spawn_status = 0;
    if (waitpid(spawn_pid, &spawn_status, 0) != spawn_pid ||
            !WIFEXITED(spawn_status) || WEXITSTATUS(spawn_status) != 0)
        fail("posix_spawn helper status");
    snprintf(path, sizeof(path), "%s/etc/bionicx-spawned", root);
    if (access(path, F_OK) != 0) fail("posix_spawn wrote outside the rootfs");

    snprintf(path, sizeof(path), "%s/usr/bin/bionicx-script", root);
    write_file(path, "#!/bin/sh\nexit 23\n", 0700);
    snprintf(path, sizeof(path), "%s/usr/bin/bionicx-print-home", root);
    write_file(path, "#!/bin/sh\nprintf '%s' \"$HOME\"\n", 0700);
    int env_pipe[2];
    if (pipe(env_pipe) != 0) fail("pipe environ exec");
    pid_t env_child = fork();
    if (env_child < 0) fail("fork environ exec");
    if (env_child == 0) {
        if (dup2(env_pipe[1], STDOUT_FILENO) < 0) _exit(127);
        close(env_pipe[0]);
        close(env_pipe[1]);
        char *minienv[] = { "PATH=/usr/bin:/bin", NULL };
        environ = minienv;
        char *args[] = { "bionicx-print-home", NULL };
        execv("/usr/bin/bionicx-print-home", args);
        _exit(127);
    }
    close(env_pipe[1]);
    char printed_home[256];
    ssize_t printed = read(env_pipe[0], printed_home, sizeof(printed_home) - 1);
    close(env_pipe[0]);
    int env_status = 0;
    if (printed < 0) printed = 0;
    printed_home[printed] = '\0';
    if (waitpid(env_child, &env_status, 0) != env_child ||
            !WIFEXITED(env_status) || WEXITSTATUS(env_status) != 0 ||
            strcmp(printed_home, "/captured-home") != 0)
        fail("exec after environ replace must restore HOME");

    pid_t execl_child = fork();
    if (execl_child < 0) fail("fork execl");
    if (execl_child == 0) {
        execl("/usr/bin/bionicx-script", "bionicx-script", (char *)NULL);
        _exit(121);
    }
    int execl_status = 0;
    if (waitpid(execl_child, &execl_status, 0) != execl_child ||
            !WIFEXITED(execl_status) || WEXITSTATUS(execl_status) != 23)
        fail("execl must run the rootfs script, not the host path");

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
