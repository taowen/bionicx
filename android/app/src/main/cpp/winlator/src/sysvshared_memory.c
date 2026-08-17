#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/syscall.h>
#include <jni.h>
#include <android/log.h>
#include <android/sharedmem.h>
#include <errno.h>
#include <stddef.h>

#define __u32 uint32_t
#include <linux/ashmem.h>

int ashmemCreateRegion(const char* name, int64_t size) {
#if __ANDROID_API__ >= 26
    int fd = ASharedMemory_create(name, size);
    if (fd < 0) return -1;
    return fd;
#else
    int fd = open("/dev/ashmem", O_RDWR);
    if (fd < 0) return -1;

    char nameBuffer[ASHMEM_NAME_LEN] = {0};
    strncpy(nameBuffer, name, sizeof(nameBuffer));
    nameBuffer[sizeof(nameBuffer) - 1] = 0;

    int ret = ioctl(fd, ASHMEM_SET_NAME, nameBuffer);
    if (ret < 0) goto error;

    ret = ioctl(fd, ASHMEM_SET_SIZE, size);
    if (ret < 0) goto error;

    return fd;
error:
    close(fd);
    return -1;
#endif
}

static int memfd_create(const char *name, unsigned int flags) {
#ifdef __NR_memfd_create
    return syscall(__NR_memfd_create, name, flags);
#else
    return -1;
#endif
}

int createMemoryFd(const char* name, int64_t size) {
    int fd = memfd_create(name, MFD_ALLOW_SEALING);
    if (fd < 0) return -1;

    int res = ftruncate(fd, size);
    if (res < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

JNIEXPORT jint JNICALL
Java_com_winlator_sysvshm_SysVSharedMemory_ashmemCreateRegion(JNIEnv *env, jobject obj, jint index,
                                                              jlong size) {
    char name[32];
    sprintf(name, "sysvshm-%d", index);
    return ashmemCreateRegion(name, size);
}

static int receive_fd(int socket_fd) {
    char payload;
    struct iovec iov = {.iov_base = &payload, .iov_len = sizeof(payload)};
    union {
        struct cmsghdr header;
        char bytes[CMSG_SPACE(sizeof(int))];
    } control;
    memset(&control, 0, sizeof(control));
    struct msghdr message = {
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = control.bytes,
        .msg_controllen = sizeof(control.bytes),
    };
    if (recvmsg(socket_fd, &message, 0) != 1) return -1;
    struct cmsghdr *header = CMSG_FIRSTHDR(&message);
    if (header == NULL || header->cmsg_level != SOL_SOCKET ||
            header->cmsg_type != SCM_RIGHTS ||
            header->cmsg_len < CMSG_LEN(sizeof(int))) return -1;
    return *(int *)CMSG_DATA(header);
}

JNIEXPORT jint JNICALL
Java_com_winlator_sysvshm_SysVSharedMemory_openRemoteSHMSegment(JNIEnv *env, jclass obj,
                                                                jint shmid) {
    unsigned int socket_id = (unsigned int)shmid / 0x10000u;
    char abstract_name[64];
    int name_length = snprintf(abstract_name, sizeof(abstract_name),
                               "/dev/shm/%08x", socket_id);
    if (name_length <= 0 || name_length >= (int)sizeof(abstract_name)) return -1;

    int socket_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (socket_fd < 0) return -1;
    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    memcpy(&address.sun_path[1], abstract_name, name_length);
    socklen_t address_length = offsetof(struct sockaddr_un, sun_path) + 1 + name_length;
    if (connect(socket_fd, (struct sockaddr *)&address, address_length) != 0 ||
            send(socket_fd, &shmid, sizeof(shmid), 0) != sizeof(shmid)) {
        close(socket_fd);
        return -1;
    }

    // libandroid-shmem sends the segment key before its SCM_RIGHTS message.
    int key;
    size_t received = 0;
    while (received < sizeof(key)) {
        ssize_t count = read(socket_fd, (char *)&key + received, sizeof(key) - received);
        if (count <= 0) {
            close(socket_fd);
            return -1;
        }
        received += count;
    }
    int fd = receive_fd(socket_fd);
    close(socket_fd);
    return fd;
}

JNIEXPORT jlong JNICALL
Java_com_winlator_sysvshm_SysVSharedMemory_getMemorySize(JNIEnv *env, jclass obj, jint fd) {
    int size = ioctl(fd, ASHMEM_GET_SIZE, NULL);
    if (size > 0) return size;
    struct stat status;
    return fstat(fd, &status) == 0 ? status.st_size : -1;
}

JNIEXPORT jobject JNICALL
Java_com_winlator_sysvshm_SysVSharedMemory_mapSHMSegment(JNIEnv *env, jobject obj, jint fd, jlong size, jint offset, jboolean readonly) {
    char *data = mmap(NULL, size, readonly ? PROT_READ : PROT_WRITE | PROT_READ, MAP_SHARED, fd, offset);
    if (data == MAP_FAILED) return NULL;
    return (*env)->NewDirectByteBuffer(env, data, size);
}

JNIEXPORT void JNICALL
Java_com_winlator_sysvshm_SysVSharedMemory_unmapSHMSegment(JNIEnv *env, jobject obj, jobject data,
                                                           jlong size) {
    char *dataAddr = (*env)->GetDirectBufferAddress(env, data);
    munmap(dataAddr, size);
}

JNIEXPORT jint JNICALL
Java_com_winlator_sysvshm_SysVSharedMemory_createMemoryFd(JNIEnv *env, jclass obj, jstring name,
                                                          jint size) {
    const char *namePtr = (*env)->GetStringUTFChars(env, name, 0);

    int fd = createMemoryFd(namePtr, size);
    (*env)->ReleaseStringUTFChars(env, name, namePtr);
    if (fd < 0) return -1;

    return fd;
}
