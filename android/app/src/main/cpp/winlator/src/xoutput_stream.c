#include <malloc.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <jni.h>
#include <unistd.h>
#include <android/log.h>

#include "winlator.h"
#include "socket_utils.h"

typedef struct XOutputStream {
    int fd;
    int ancillaryFd;
    struct {
        void* data;
        int capacity;
        int position;
        int limit;
    } buffer;
} XOutputStream;

static void ensureSpaceIsAvailable(XOutputStream* outputStream, int length) {
    if ((outputStream->buffer.capacity - outputStream->buffer.position) >= length) return;

    int newCapacity = outputStream->buffer.capacity + length;
    void* newData = realloc(outputStream->buffer.data, newCapacity);
    outputStream->buffer.capacity = newCapacity;
    outputStream->buffer.limit = newCapacity;
    outputStream->buffer.data = newData;
}

static XOutputStream* XOutputStream_allocate(int fd, int initialCapacity) {
    XOutputStream* outputStream = calloc(1, sizeof(XOutputStream));
    outputStream->fd = fd;
    outputStream->buffer.data = calloc(initialCapacity, 1);
    outputStream->buffer.capacity = initialCapacity;
    outputStream->buffer.limit = initialCapacity;
    return outputStream;
}

static void XOutputStream_destroy(XOutputStream* outputStream) {
    if (!outputStream) return;
    MEMFREE(outputStream->buffer.data);
    MEMFREE(outputStream);
}

static jboolean XOutputStream_send(XOutputStream* outputStream) {
    if (outputStream->buffer.position == 0) return JNI_TRUE;
    outputStream->buffer.limit = outputStream->buffer.position;
    outputStream->buffer.position = 0;

    int totalSent = 0;
    int ancillaryFd = outputStream->ancillaryFd;
    outputStream->ancillaryFd = 0;

    while (totalSent < outputStream->buffer.limit) {
        int bytesSent;
        if (ancillaryFd > 0) {
            bytesSent = send_fds(outputStream->fd, &ancillaryFd, 1,
                                 outputStream->buffer.data + totalSent,
                                 outputStream->buffer.limit - totalSent);
            // SCM_RIGHTS must only accompany the first successful send.
            if (bytesSent > 0) ancillaryFd = 0;
        }
        else {
            bytesSent = write(outputStream->fd,
                              outputStream->buffer.data + totalSent,
                              outputStream->buffer.limit - totalSent);
        }

        if (bytesSent > 0) {
            totalSent += bytesSent;
            continue;
        }
        if (bytesSent < 0 && errno == EINTR) continue;
        if (bytesSent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            struct pollfd pollFd = {.fd = outputStream->fd, .events = POLLOUT};
            int pollResult;
            do pollResult = poll(&pollFd, 1, -1);
            while (pollResult < 0 && errno == EINTR);
            if (pollResult > 0 && (pollFd.revents & POLLOUT)) continue;
        }

        int err = errno;
        int remaining = outputStream->buffer.limit - totalSent;
        if (remaining > 0) {
            if (totalSent > 0) {
                memmove(outputStream->buffer.data,
                        outputStream->buffer.data + totalSent,
                        (size_t)remaining);
            }
            outputStream->buffer.position = remaining;
        }
        else {
            outputStream->buffer.position = 0;
        }
        outputStream->buffer.limit = outputStream->buffer.capacity;
        __android_log_print(ANDROID_LOG_INFO, "BionicX",
                "BXINFO xsend-fail fd=%d sent=%d left=%d errno=%d",
                outputStream->fd, totalSent, remaining, err);
        return JNI_FALSE;
    }

    outputStream->buffer.limit = outputStream->buffer.capacity;
    return JNI_TRUE;
}

JNIEXPORT jlong JNICALL
Java_com_winlator_xconnector_XOutputStream_nativeAllocate(JNIEnv *env, jobject obj, jint fd,
                                                          jint initialCapacity) {
    XOutputStream* outputStream = XOutputStream_allocate(fd, initialCapacity);
    return (jlong)outputStream;
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_setAncillaryFd(jlong nativePtr, jint ancillaryFd) {
    ((XOutputStream*)nativePtr)->ancillaryFd = ancillaryFd;
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_writeByte(jlong nativePtr, jbyte value) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    ensureSpaceIsAvailable(outputStream, 1);
    *(jbyte*)(outputStream->buffer.data + outputStream->buffer.position++) = value;
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_writeShort(jlong nativePtr, jshort value) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    ensureSpaceIsAvailable(outputStream, 2);
    *(jshort*)(outputStream->buffer.data + outputStream->buffer.position) = value;
    outputStream->buffer.position += 2;
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_writeInt(jlong nativePtr, jint value) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    ensureSpaceIsAvailable(outputStream, 4);
    *(jint*)(outputStream->buffer.data + outputStream->buffer.position) = value;
    outputStream->buffer.position += 4;
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_writeLong(jlong nativePtr, jlong value) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    ensureSpaceIsAvailable(outputStream, 8);
    *(jlong*)(outputStream->buffer.data + outputStream->buffer.position) = value;
    outputStream->buffer.position += 8;
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_writePad(jlong nativePtr, jint length) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    ensureSpaceIsAvailable(outputStream, length);
    memset(outputStream->buffer.data + outputStream->buffer.position, 0, length);
    outputStream->buffer.position += length;
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_writeAt(JNIEnv *env, jclass obj,
                                                   jlong nativePtr, jint position, jbyteArray data) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    jbyte* dataPtr = (*env)->GetByteArrayElements(env, data, 0);
    jsize length = (*env)->GetArrayLength(env, data);
    memcpy(outputStream->buffer.data + position, dataPtr, length);
    (*env)->ReleaseByteArrayElements(env, data, dataPtr, JNI_ABORT);
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_writeByteBuffer(JNIEnv *env, jclass obj,
                                                           jlong nativePtr, jobject data,
                                                           jint offset, jint length) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    void* dataAddr = (*env)->GetDirectBufferAddress(env, data);
    ensureSpaceIsAvailable(outputStream, length);
    memcpy(outputStream->buffer.data + outputStream->buffer.position, dataAddr + offset, length);
    outputStream->buffer.position += length;
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_xconnector_XOutputStream_sendData(JNIEnv *env, jclass obj, jlong nativePtr) {
    return XOutputStream_send((XOutputStream*)nativePtr);
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XOutputStream_destroy(JNIEnv *env, jclass obj, jlong nativePtr) {
    XOutputStream_destroy((XOutputStream*)nativePtr);
}

JNIEXPORT jint JNICALL
Java_com_winlator_xconnector_XOutputStream_length(jlong nativePtr) {
    XOutputStream* outputStream = (XOutputStream*)nativePtr;
    return outputStream->buffer.position;
}
