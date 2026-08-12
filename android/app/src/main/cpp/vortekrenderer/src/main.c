#include <jni.h>
#include "vk_context.h"
#include "vortek_serializer.h"
#include "request_handler.h"
#include "vulkan_helper.h"
#include "jni_utils.h"

VulkanWrapper vulkanWrapper = {0};
bool vortekSerializerCastVkObject = true;

static void* openVulkanLibrary(JNIEnv* env, jstring nativeLibraryDir, jstring libvulkanPath) {
    (void)env;
    (void)nativeLibraryDir;
    (void)libvulkanPath;
    void* libvulkan = dlopen(LIBVULKAN_PATH, RTLD_NOW | RTLD_LOCAL);

    if (!libvulkan) println("vortek: unable to open libvulkan: %s", dlerror());
    return libvulkan;
}

JNIEXPORT jlong JNICALL
Java_com_winlator_xenvironment_components_VortekRendererComponent_createVkContext(JNIEnv *env,
                                                                                  jobject obj,
                                                                                  jint clientFd,
                                                                                  jobject options) {
    VkContext* context = createVkContext(env, obj, clientFd, options);
    return context ? (jlong)context : 0;
}

JNIEXPORT void JNICALL
Java_com_winlator_xenvironment_components_VortekRendererComponent_destroyVkContext(JNIEnv *env,
                                                                                   jobject obj,
                                                                                   jlong contextPtr) {
    destroyVkContext(env, (VkContext*)contextPtr);
}

JNIEXPORT void JNICALL
Java_com_winlator_xenvironment_components_VortekRendererComponent_initVulkanWrapper(JNIEnv *env,
                                                                                    jobject obj,
                                                                                    jstring nativeLibraryDir,
                                                                                    jstring libvulkanPath) {
    void* libvulkan = openVulkanLibrary(env, nativeLibraryDir, libvulkanPath);
    initVulkanWrapper(&vulkanWrapper, libvulkan);
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_xenvironment_components_VortekRendererComponent_handleExtraDataRequest(JNIEnv *env,
                                                                                         jobject obj,
                                                                                         jlong contextPtr,
                                                                                         int requestId,
                                                                                         int requestLength) {
    return handleExtraDataRequest((VkContext*)contextPtr, requestId, requestLength);
}
