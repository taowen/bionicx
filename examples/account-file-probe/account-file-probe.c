#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <shadow.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

    unsetenv("BIONICX_ROOTFS");
    unsetenv("BIONICX_TMPDIR");
    stream = fopen("/etc/group", "r+");
    if (stream == NULL) return fail("fopen-after-unsetenv", strerror(errno));
    fclose(stream);
    printf("BXTEST PASS captured-rootfs-after-unsetenv\n");
    printf("BXSUMMARY account-file 4/4\n");
    return 0;
}
