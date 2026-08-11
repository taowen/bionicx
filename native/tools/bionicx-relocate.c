#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static const unsigned char *old_prefix;
static const unsigned char *new_prefix;
static size_t prefix_length;
static unsigned long files_changed;
static unsigned long replacements;

static int patch_file(const char *path) {
    int fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0) return errno == EACCES ? 0 : -1;
    struct stat info;
    if (fstat(fd, &info) != 0 || info.st_size < (off_t)prefix_length) {
        close(fd);
        return 0;
    }
    unsigned char *data = mmap(NULL, (size_t)info.st_size,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (data == MAP_FAILED) return -1;

    unsigned long in_file = 0;
    size_t limit = (size_t)info.st_size - prefix_length;
    for (size_t offset = 0; offset <= limit;) {
        if (memcmp(data + offset, old_prefix, prefix_length) == 0) {
            memcpy(data + offset, new_prefix, prefix_length);
            ++in_file;
            offset += prefix_length;
        }
        else ++offset;
    }
    if (in_file != 0) {
        msync(data, (size_t)info.st_size, MS_SYNC);
        ++files_changed;
        replacements += in_file;
        printf("%s: %lu\n", path, in_file);
    }
    munmap(data, (size_t)info.st_size);
    return 0;
}

static int walk(const char *path) {
    struct stat info;
    if (lstat(path, &info) != 0) return -1;
    if (S_ISREG(info.st_mode)) return patch_file(path);
    if (!S_ISDIR(info.st_mode)) return 0;

    DIR *directory = opendir(path);
    if (directory == NULL) return -1;
    int result = 0;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        size_t length = strlen(path) + strlen(entry->d_name) + 2;
        char *child = malloc(length);
        if (child == NULL) { result = -1; break; }
        snprintf(child, length, "%s/%s", path, entry->d_name);
        if (walk(child) != 0 && errno != EACCES)
            fprintf(stderr, "bionicx-relocate: skip %s: %s\n",
                    child, strerror(errno));
        free(child);
    }
    closedir(directory);
    return result;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s ROOT OLD_PREFIX NEW_PREFIX\n", argv[0]);
        return 2;
    }
    prefix_length = strlen(argv[2]);
    if (prefix_length == 0 || prefix_length != strlen(argv[3])) {
        fprintf(stderr, "bionicx-relocate: prefixes must have equal nonzero length\n");
        return 2;
    }
    old_prefix = (const unsigned char *)argv[2];
    new_prefix = (const unsigned char *)argv[3];
    if (walk(argv[1]) != 0) {
        fprintf(stderr, "bionicx-relocate: %s: %s\n", argv[1], strerror(errno));
        return 1;
    }
    fprintf(stderr, "bionicx-relocate: files=%lu replacements=%lu\n",
            files_changed, replacements);
    return 0;
}
