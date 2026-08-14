#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <shadow.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fail(const char *name, const char *detail) {
    fprintf(stderr, "BXTEST FAIL %s %s errno=%d\n", name,
            detail != NULL ? detail : "", errno);
    return 1;
}

int main(void) {
    const char *root = getenv("BIONICX_ROOTFS");
    if (root == NULL || root[0] != '/')
        return fail("rootfs-env", "BIONICX_ROOTFS missing");

    FILE *stream = fopen("/etc/group", "r+");
    if (stream == NULL) return fail("fopen-group-rw", strerror(errno));
    if (fseek(stream, 0, SEEK_END) != 0 ||
            fputs("bionicx-account-probe:x:12346:\n", stream) < 0) {
        fclose(stream);
        return fail("fwrite-group", strerror(errno));
    }
    if (fclose(stream) != 0) return fail("fclose-group", strerror(errno));

    char backing[512];
    snprintf(backing, sizeof(backing), "%s/etc/group", root);
    stream = fopen(backing, "r");
    if (stream == NULL) return fail("backing-group", strerror(errno));
    char buffer[4096];
    size_t n = fread(buffer, 1, sizeof(buffer) - 1, stream);
    fclose(stream);
    buffer[n] = '\0';
    if (strstr(buffer, "bionicx-account-probe:x:12346:\n") == NULL)
        return fail("group-stayed-in-rootfs", backing);
    printf("BXTEST PASS fopen-group-rw\n");

    int (*open2)(const char *, int) = dlsym(RTLD_DEFAULT, "__open_2");
    if (open2 == NULL) return fail("open2-symbol", "missing");
    int fd = open2("/etc/group", O_RDWR);
    if (fd < 0) return fail("open2-group", strerror(errno));
    close(fd);
    printf("BXTEST PASS fortified-open-group\n");

    if (lckpwdf() != 0) return fail("lckpwdf", strerror(errno));
    snprintf(backing, sizeof(backing), "%s/etc/.pwd.lock", root);
    if (access(backing, F_OK) != 0) {
        ulckpwdf();
        return fail("lckpwdf-backing", strerror(errno));
    }
    if (ulckpwdf() != 0) return fail("ulckpwdf", strerror(errno));
    printf("BXTEST PASS lckpwdf\n");

    int (*audit_open_fn)(void) = dlsym(RTLD_DEFAULT, "audit_open");
    int (*audit_close_fn)(int) = dlsym(RTLD_DEFAULT, "audit_close");
    int (*audit_log_fn)(int, int, const char *, const char *, const char *,
                        unsigned int, const char *, const char *, const char *,
                        int) = dlsym(RTLD_DEFAULT, "audit_log_acct_message");
    if (audit_open_fn == NULL || audit_close_fn == NULL || audit_log_fn == NULL)
        return fail("audit-symbols", "missing");
    int audit_fd = audit_open_fn();
    if (audit_fd < 0) return fail("audit_open", strerror(errno));
    if (audit_log_fn(audit_fd, 1100, "probe", "adding-group",
                     "bionicx", 0, NULL, NULL, NULL, 1) <= 0) {
        audit_close_fn(audit_fd);
        return fail("audit_log_acct_message", "stub failed");
    }
    if (audit_close_fn(audit_fd) != 0)
        return fail("audit_close", strerror(errno));
    printf("BXTEST PASS audit-stubs\n");

    if (link("/etc/group", "/etc/group.lock") != 0)
        return fail("group-lock-link", strerror(errno));
    struct stat group_info;
    struct stat lock_info;
    if (stat("/etc/group", &group_info) != 0 ||
            stat("/etc/group.lock", &lock_info) != 0) {
        unlink("/etc/group.lock");
        return fail("group-lock-stat", strerror(errno));
    }
    if (group_info.st_nlink != 2 || lock_info.st_nlink != 2 ||
            group_info.st_ino != lock_info.st_ino) {
        unlink("/etc/group.lock");
        return fail("group-lock-nlink", "nlink/ino mismatch");
    }
    if (unlink("/etc/group.lock") != 0)
        return fail("group-lock-unlink", strerror(errno));
    printf("BXTEST PASS group-lock-nlink\n");

    unsetenv("BIONICX_ROOTFS");
    unsetenv("BIONICX_TMPDIR");
    stream = fopen("/etc/group", "r+");
    if (stream == NULL) return fail("fopen-after-unsetenv", strerror(errno));
    fclose(stream);
    printf("BXTEST PASS captured-rootfs-after-unsetenv\n");
    printf("BXSUMMARY account-file 6/6\n");
    return 0;
}
