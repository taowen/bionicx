#include <android/log.h>
#include <android/hardware_buffer.h>
#include <android/native_window.h>

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <jni.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>

#include "native_handle.h"
#include "winlator.h"

#define HAL_PIXEL_FORMAT_BGRA_8888 5

extern const native_handle_t* _Nullable AHardwareBuffer_getNativeHandle(const AHardwareBuffer* _Nonnull buffer);

int AHardwareBuffer_getFd(AHardwareBuffer* hardwareBuffer) {
    const native_handle_t* nativeHandle = AHardwareBuffer_getNativeHandle(hardwareBuffer);
    return nativeHandle->numFds > 0 ? nativeHandle->data[0] : -1;
}

EGLImageKHR createImageKHR(AHardwareBuffer* hardwareBuffer, int textureId) {
    const EGLint attribList[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};

    AHardwareBuffer_acquire(hardwareBuffer);

    EGLClientBuffer clientBuffer = eglGetNativeClientBufferANDROID(hardwareBuffer);
    if (!clientBuffer) {
        __android_log_print(ANDROID_LOG_ERROR, "BionicX",
                            "eglGetNativeClientBufferANDROID failed");
        return NULL;
    }

    /* The compositor's GLSurfaceView display, not a fresh DEFAULT_DISPLAY.
     * Mali will not sample an image created on a different EGLDisplay. */
    EGLDisplay eglDisplay = eglGetCurrentDisplay();
    if (eglDisplay == EGL_NO_DISPLAY)
        eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLImageKHR imageKHR = eglCreateImageKHR(eglDisplay, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, clientBuffer, attribList);
    if (!imageKHR) {
        __android_log_print(ANDROID_LOG_ERROR, "BionicX",
                            "eglCreateImageKHR failed err=0x%x", eglGetError());
        return NULL;
    }

    glBindTexture(GL_TEXTURE_2D, textureId);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, imageKHR);
    GLenum error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (error != GL_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, "BionicX",
                            "glEGLImageTargetTexture2DOES err=0x%x", error);
        eglDestroyImageKHR(eglDisplay, imageKHR);
        return NULL;
    }

    return imageKHR;
}

AHardwareBuffer* createHardwareBuffer(int width, int height, bool cpuAccess, bool useHALPixelFormatBGRA8888) {
    AHardwareBuffer_Desc buffDesc = {0};
    buffDesc.width = width;
    buffDesc.height = height;
    buffDesc.layers = 1;
    /* Mali samples COLOR_OUTPUT-only buffers as black. Vortek writes the
     * window AHB with Vulkan and the GLES compositor samples it. */
    buffDesc.usage = AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT |
                     AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                     AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN |
                     AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;
    buffDesc.format = useHALPixelFormatBGRA8888 ? HAL_PIXEL_FORMAT_BGRA_8888 : AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;

    AHardwareBuffer* hardwareBuffer = NULL;
    int status = AHardwareBuffer_allocate(&buffDesc, &hardwareBuffer);
    if (status != 0 || hardwareBuffer == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "BionicX",
                            "AHB allocate failed rc=%d format=%u usage=0x%llx %dx%d",
                            status, buffDesc.format,
                            (unsigned long long)buffDesc.usage, width, height);
        return NULL;
    }

    return hardwareBuffer;
}

JNIEXPORT jlong JNICALL
Java_com_winlator_renderer_GPUImage_createHardwareBuffer(JNIEnv *env, jclass obj, jshort width,
                                                         jshort height, jboolean cpuAccess, jboolean useHALPixelFormatBGRA8888) {
    AHardwareBuffer* hardwareBuffer = createHardwareBuffer(width, height, cpuAccess, useHALPixelFormatBGRA8888);
    if (hardwareBuffer) {
        jclass cls = (*env)->GetObjectClass(env, obj);

        AHardwareBuffer_Desc buffDesc = {0};
        AHardwareBuffer_describe(hardwareBuffer, &buffDesc);

        jmethodID setStride = (*env)->GetMethodID(env, cls, "setStride", "(S)V");
        (*env)->CallVoidMethod(env, obj, setStride, (jshort)buffDesc.stride);

        int fd = AHardwareBuffer_getFd(hardwareBuffer);
        if (fd != -1) {
            jmethodID setNativeHandle = (*env)->GetMethodID(env, cls, "setNativeHandle", "(I)V");
            (*env)->CallVoidMethod(env, obj, setNativeHandle, fd);
        }
    }
    return (jlong)hardwareBuffer;
}

JNIEXPORT jlong JNICALL
Java_com_winlator_renderer_GPUImage_createImageKHR(JNIEnv *env, jclass obj,
                                                   jlong hardwareBufferPtr, jint textureId) {
    return (jlong)createImageKHR((AHardwareBuffer*)hardwareBufferPtr, textureId);
}

JNIEXPORT void JNICALL
Java_com_winlator_renderer_GPUImage_destroyHardwareBuffer(JNIEnv *env, jclass obj,
                                                          jlong hardwareBufferPtr, jboolean locked) {
    AHardwareBuffer* hardwareBuffer = (AHardwareBuffer*)hardwareBufferPtr;
    if (hardwareBuffer) {
        if (locked) {
            AHardwareBuffer_unlock(hardwareBuffer, NULL);
            locked = false;
        }
        AHardwareBuffer_release(hardwareBuffer);
    }
}

JNIEXPORT jobject JNICALL
Java_com_winlator_renderer_GPUImage_lockHardwareBuffer(JNIEnv *env, jclass obj,
                                                       jlong hardwareBufferPtr) {
    AHardwareBuffer* hardwareBuffer = (AHardwareBuffer*)hardwareBufferPtr;
    void *virtualAddr;
    if (AHardwareBuffer_lock(hardwareBuffer,
                             AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN |
                                 AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
                             -1, NULL, &virtualAddr) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, "BionicX",
                            "AHB lock failed");
        return NULL;
    }

    AHardwareBuffer_Desc buffDesc = {0};
    AHardwareBuffer_describe(hardwareBuffer, &buffDesc);

    jlong size = buffDesc.stride * buffDesc.height * 4;
    return (*env)->NewDirectByteBuffer(env, virtualAddr, size);
}

JNIEXPORT void JNICALL
Java_com_winlator_renderer_GPUImage_unlockHardwareBuffer(JNIEnv *env, jclass obj,
                                                         jlong hardwareBufferPtr) {
    AHardwareBuffer* hardwareBuffer = (AHardwareBuffer*)hardwareBufferPtr;
    if (hardwareBuffer)
        AHardwareBuffer_unlock(hardwareBuffer, NULL);
}

JNIEXPORT void JNICALL
Java_com_winlator_renderer_GPUImage_destroyImageKHR(JNIEnv *env, jclass obj, jlong imageKHRPtr) {
    EGLImageKHR imageKHR = (EGLImageKHR)imageKHRPtr;
    if (imageKHR) {
        EGLDisplay eglDisplay = eglGetCurrentDisplay();
        if (eglDisplay == EGL_NO_DISPLAY)
            eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglDestroyImageKHR(eglDisplay, imageKHR);
    }
}