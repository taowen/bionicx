#include "xwindow_swapchain.h"
#include "vulkan_helper.h"

static void submit_blit(XWindowSwapchain* swapchain, uint32_t imageIndex,
                        uint32_t waitSemaphoreCount,
                        const VkSemaphore* waitSemaphores);
static void *flush_pending_blit(void *arg);

void getWindowExtent(JMethods* jmethods, int windowId, VkExtent2D* extent) {
    extent->width = (*jmethods->env)->CallIntMethod(jmethods->env, jmethods->obj, jmethods->getWindowWidth, windowId);
    extent->height = (*jmethods->env)->CallIntMethod(jmethods->env, jmethods->obj, jmethods->getWindowHeight, windowId);
}

static AHardwareBuffer* getWindowHardwareBuffer(JMethods* jmethods, int windowId, jboolean useHALPixelFormatBGRA8888) {
    jlong hardwareBufferPtr = (*jmethods->env)->CallLongMethod(jmethods->env, jmethods->obj, jmethods->getWindowHardwareBuffer, windowId, useHALPixelFormatBGRA8888);
    return (AHardwareBuffer*)hardwareBufferPtr;
}

static uint32_t memoryTypeForFlags(uint32_t typeBits, VkMemoryPropertyFlags flags) {
    uint32_t index = getMemoryTypeIndex(typeBits, flags);
    if (typeBits & (1u << index))
        return index;
    for (uint32_t i = 0; typeBits != 0; i++, typeBits >>= 1) {
        if (typeBits & 1)
            return i;
    }
    return 0;
}

static VkResult createDeviceImage(VkDevice device, XWindowSwapchain* swapchain,
                                  XWindowSwapchain_Image* swapchainImage) {
    VkImageCreateInfo imageInfo = {0};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = swapchain->imageFormat;
    imageInfo.extent.width = swapchain->imageExtent.width;
    imageInfo.extent.height = swapchain->imageExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = swapchain->imageUsage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image;
    VkResult result = vulkanWrapper.vkCreateImage(device, &imageInfo, NULL, &image);
    if (result != VK_SUCCESS) return result;

    VkMemoryRequirements requirements;
    vulkanWrapper.vkGetImageMemoryRequirements(device, image, &requirements);
    VkMemoryAllocateInfo memoryInfo = {0};
    memoryInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryInfo.allocationSize = requirements.size;
    memoryInfo.memoryTypeIndex = memoryTypeForFlags(
            requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceMemory memory;
    result = vulkanWrapper.vkAllocateMemory(device, &memoryInfo, NULL, &memory);
    if (result != VK_SUCCESS) {
        vulkanWrapper.vkDestroyImage(device, image, NULL);
        return result;
    }
    result = vulkanWrapper.vkBindImageMemory(device, image, memory, 0);
    if (result != VK_SUCCESS) {
        vulkanWrapper.vkFreeMemory(device, memory, NULL);
        vulkanWrapper.vkDestroyImage(device, image, NULL);
        return result;
    }
    swapchainImage->image = image;
    swapchainImage->memory = memory;
    return VK_SUCCESS;
}

static VkResult createConvertImage(VkDevice device, XWindowSwapchain* swapchain,
                                   XWindowSwapchain_Image* target) {
    VkImageCreateInfo imageInfo = {0};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent.width = swapchain->imageExtent.width;
    imageInfo.extent.height = swapchain->imageExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image;
    VkResult result = vulkanWrapper.vkCreateImage(device, &imageInfo, NULL, &image);
    if (result != VK_SUCCESS) return result;

    VkMemoryRequirements requirements;
    vulkanWrapper.vkGetImageMemoryRequirements(device, image, &requirements);
    VkMemoryAllocateInfo memoryInfo = {0};
    memoryInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryInfo.allocationSize = requirements.size;
    memoryInfo.memoryTypeIndex = memoryTypeForFlags(
            requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceMemory memory;
    result = vulkanWrapper.vkAllocateMemory(device, &memoryInfo, NULL, &memory);
    if (result != VK_SUCCESS) {
        vulkanWrapper.vkDestroyImage(device, image, NULL);
        return result;
    }
    result = vulkanWrapper.vkBindImageMemory(device, image, memory, 0);
    if (result != VK_SUCCESS) {
        vulkanWrapper.vkFreeMemory(device, memory, NULL);
        vulkanWrapper.vkDestroyImage(device, image, NULL);
        return result;
    }
    target->image = image;
    target->memory = memory;
    return VK_SUCCESS;
}

static VkResult createPublishBuffer(VkDevice device, XWindowSwapchain* swapchain) {
    swapchain->publishSize = (VkDeviceSize)swapchain->imageExtent.width *
                             swapchain->imageExtent.height * 4u;
    if (swapchain->publishSize == 0)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkBufferCreateInfo bufferInfo = {0};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = swapchain->publishSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = vulkanWrapper.vkCreateBuffer(device, &bufferInfo, NULL,
                                                   &swapchain->publishBuffer);
    if (result != VK_SUCCESS) return result;

    VkMemoryRequirements requirements;
    vulkanWrapper.vkGetBufferMemoryRequirements(device, swapchain->publishBuffer,
                                                &requirements);
    VkMemoryAllocateInfo memoryInfo = {0};
    memoryInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryInfo.allocationSize = requirements.size;
    memoryInfo.memoryTypeIndex = memoryTypeForFlags(
            requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    result = vulkanWrapper.vkAllocateMemory(device, &memoryInfo, NULL,
                                            &swapchain->publishMemory);
    if (result != VK_SUCCESS) return result;
    result = vulkanWrapper.vkBindBufferMemory(device, swapchain->publishBuffer,
                                              swapchain->publishMemory, 0);
    if (result != VK_SUCCESS) return result;
    return vulkanWrapper.vkMapMemory(device, swapchain->publishMemory, 0,
                                     swapchain->publishSize, 0,
                                     &swapchain->publishMapped);
}

int getSurfaceMinImageCount() {
    return 2;
}

VkSurfaceFormatKHR* getSurfaceFormats(uint32_t* formatCount) {
    /* ANGLE on X11 prefers BGRA to match the root visual. Conversion
     * to RGBA happens in the present blit, not in the client image. */
    static const VkFormat supportedFormats[] = {
        VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB
    };
    int supportedFormatCount = ARRAY_SIZE(supportedFormats);
    VkSurfaceFormatKHR* surfaceFormats = calloc(supportedFormatCount, sizeof(VkSurfaceFormatKHR));

    if (formatCount) *formatCount = supportedFormatCount;

    for (int i = 0; i < supportedFormatCount; i++) {
        surfaceFormats[i].format = supportedFormats[i];
        surfaceFormats[i].colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }

    return surfaceFormats;
}

XWindowSwapchain* XWindowSwapchain_create(VkDevice device, VkQueue graphicsQueue, VkSwapchainCreateInfoKHR* swapchainInfo, JMethods* jmethods, int windowId) {
    XWindowSwapchain* swapchain = calloc(1, sizeof(XWindowSwapchain));
    swapchain->windowId = windowId;
    int imageCount = (int)swapchainInfo->minImageCount;
    if (imageCount < 2) imageCount = 2;
    if (imageCount > 3) imageCount = 3;
    swapchain->imageCount = imageCount;
    swapchain->images = calloc(swapchain->imageCount, sizeof(XWindowSwapchain_Image));
    swapchain->imageFormat = swapchainInfo->imageFormat;
    swapchain->imageUsage = swapchainInfo->imageUsage;
    memcpy(&swapchain->imageExtent, &swapchainInfo->imageExtent, sizeof(VkExtent2D));
    swapchain->jmethods = jmethods;
    swapchain->acquireIndex = 0;
    swapchain->presented = -1;
    swapchain->pendingIndex = -1;
    swapchain->blitSource = -1;
    pthread_mutex_init(&swapchain->lock, NULL);

    /* Ensure the window has a GLES texture. Present no longer writes
     * that AHB; it publishes a host-visible copy. */
    if (jmethods)
        getWindowHardwareBuffer(jmethods, windowId, JNI_FALSE);

    VkResult result;
    for (int i = 0; i < swapchain->imageCount; i++) {
        result = createDeviceImage(device, swapchain, &swapchain->images[i]);
        if (result != VK_SUCCESS) goto error;
    }
    result = createConvertImage(device, swapchain, &swapchain->convertTarget);
    if (result != VK_SUCCESS) goto error;
    result = createPublishBuffer(device, swapchain);
    if (result != VK_SUCCESS) goto error;

    swapchain->device = device;
    swapchain->queue = graphicsQueue;
    VkCommandPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = 0;
    if (vulkanWrapper.vkCreateCommandPool(device, &poolInfo, NULL,
                                          &swapchain->commandPool) == VK_SUCCESS) {
        VkCommandBufferAllocateInfo allocInfo = {0};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = swapchain->commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        vulkanWrapper.vkAllocateCommandBuffers(device, &allocInfo,
                                               &swapchain->commandBuffer);
        VkFenceCreateInfo fenceInfo = {0};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vulkanWrapper.vkCreateFence(device, &fenceInfo, NULL,
                                    &swapchain->blitFence);
    }
    return swapchain;

error:
    XWindowSwapchain_destroy(device, swapchain);
    return NULL;
}

void XWindowSwapchain_destroy(VkDevice device, XWindowSwapchain* swapchain) {
    if (!swapchain) return;
    pthread_mutex_lock(&swapchain->lock);
    swapchain->destroying = 1;
    swapchain->pendingIndex = -1;
    pthread_mutex_unlock(&swapchain->lock);
    if (swapchain->hasWaiter)
        pthread_join(swapchain->waiter, NULL);
    else if (swapchain->blitInFlight && swapchain->blitFence)
        vulkanWrapper.vkWaitForFences(device, 1, &swapchain->blitFence,
                                      VK_TRUE, 1000000000ull);
    pthread_mutex_destroy(&swapchain->lock);
    if (swapchain->publishMapped && swapchain->publishMemory)
        vulkanWrapper.vkUnmapMemory(device, swapchain->publishMemory);
    if (swapchain->publishBuffer)
        vulkanWrapper.vkDestroyBuffer(device, swapchain->publishBuffer, NULL);
    if (swapchain->publishMemory)
        vulkanWrapper.vkFreeMemory(device, swapchain->publishMemory, NULL);
    if (swapchain->blitFence)
        vulkanWrapper.vkDestroyFence(device, swapchain->blitFence, NULL);
    if (swapchain->commandPool)
        vulkanWrapper.vkDestroyCommandPool(device, swapchain->commandPool, NULL);
    if (swapchain->convertTarget.image)
        vulkanWrapper.vkDestroyImage(device, swapchain->convertTarget.image, NULL);
    if (swapchain->convertTarget.memory)
        vulkanWrapper.vkFreeMemory(device, swapchain->convertTarget.memory, NULL);
    if (swapchain->images) {
        for (int i = 0; i < swapchain->imageCount; i++) {
            if (swapchain->images[i].image)
                vulkanWrapper.vkDestroyImage(device, swapchain->images[i].image, NULL);
            if (swapchain->images[i].memory)
                vulkanWrapper.vkFreeMemory(device, swapchain->images[i].memory, NULL);
        }
    }

    MEMFREE(swapchain->images);
    MEMFREE(swapchain);
}

static int image_is_busy(const XWindowSwapchain* swapchain, uint32_t index) {
    return (swapchain->blitInFlight && swapchain->blitSource == (int)index)
            || swapchain->pendingIndex == (int)index;
}

static int pick_free_image(XWindowSwapchain* swapchain) {
    uint32_t start = swapchain->acquireIndex % (uint32_t)swapchain->imageCount;
    for (int i = 0; i < swapchain->imageCount; i++) {
        uint32_t index = (start + (uint32_t)i) % (uint32_t)swapchain->imageCount;
        if (!image_is_busy(swapchain, index))
            return (int)index;
    }
    return -1;
}

static void publish_pixels(XWindowSwapchain* swapchain, JNIEnv* env) {
    if (!swapchain->jmethods || !env || !swapchain->publishMapped
            || !swapchain->jmethods->publishWindowPixels)
        return;
    jobject buffer = (*env)->NewDirectByteBuffer(
            env, swapchain->publishMapped, (jlong)swapchain->publishSize);
    if (!buffer)
        return;
    (*env)->CallVoidMethod(env, swapchain->jmethods->obj,
                           swapchain->jmethods->publishWindowPixels,
                           swapchain->windowId, buffer,
                           (jint)swapchain->imageExtent.width,
                           (jint)swapchain->imageExtent.height);
    (*env)->DeleteLocalRef(env, buffer);
}

static int record_blit(XWindowSwapchain* swapchain, uint32_t imageIndex,
                       uint32_t waitSemaphoreCount,
                       const VkSemaphore* waitSemaphores) {
    VkCommandBuffer commandBuffer = swapchain->commandBuffer;
    if (!commandBuffer || !swapchain->commandPool || !swapchain->queue
            || !swapchain->images
            || imageIndex >= (uint32_t)swapchain->imageCount
            || !swapchain->convertTarget.image
            || !swapchain->publishBuffer)
        return 0;
    vulkanWrapper.vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vulkanWrapper.vkBeginCommandBuffer(commandBuffer, &beginInfo)
            != VK_SUCCESS)
        return 0;

    VkImageSubresourceRange range = {0};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.levelCount = 1;
    range.layerCount = 1;
    VkImageMemoryBarrier barriers[2] = {0};
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = swapchain->images[imageIndex].image;
    barriers[0].subresourceRange = range;
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].srcAccessMask = 0;
    barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = swapchain->convertTarget.image;
    barriers[1].subresourceRange = range;
    vulkanWrapper.vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, NULL, 0, NULL, 2, barriers);

    VkImageBlit blit = {0};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[1].x = (int32_t)swapchain->imageExtent.width;
    blit.srcOffsets[1].y = (int32_t)swapchain->imageExtent.height;
    blit.srcOffsets[1].z = 1;
    blit.dstSubresource = blit.srcSubresource;
    blit.dstOffsets[1] = blit.srcOffsets[1];
    vulkanWrapper.vkCmdBlitImage(
            commandBuffer,
            swapchain->images[imageIndex].image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapchain->convertTarget.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, VK_FILTER_NEAREST);

    VkImageMemoryBarrier convertToSrc = barriers[1];
    convertToSrc.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    convertToSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    convertToSrc.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    convertToSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vulkanWrapper.vkCmdPipelineBarrier(
            commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
            &convertToSrc);

    VkBufferImageCopy copy = {0};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent.width = swapchain->imageExtent.width;
    copy.imageExtent.height = swapchain->imageExtent.height;
    copy.imageExtent.depth = 1;
    vulkanWrapper.vkCmdCopyImageToBuffer(
            commandBuffer, swapchain->convertTarget.image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapchain->publishBuffer, 1, &copy);

    VkBufferMemoryBarrier hostBarrier = {0};
    hostBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    hostBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    hostBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    hostBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hostBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hostBarrier.buffer = swapchain->publishBuffer;
    hostBarrier.size = VK_WHOLE_SIZE;
    vulkanWrapper.vkCmdPipelineBarrier(
            commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &hostBarrier, 0,
            NULL);

    /* WSI leaves the presented image in PRESENT_SRC. */
    VkImageMemoryBarrier presentBarrier = barriers[0];
    presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    presentBarrier.dstAccessMask = 0;
    presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vulkanWrapper.vkCmdPipelineBarrier(
            commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1,
            &presentBarrier);
    if (vulkanWrapper.vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        return 0;

    VkPipelineStageFlags waitStages[8];
    uint32_t waitCount = waitSemaphoreCount;
    if (waitCount > 8) waitCount = 8;
    for (uint32_t i = 0; i < waitCount; i++)
        waitStages[i] = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submitInfo = {0};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = waitCount;
    submitInfo.pWaitSemaphores = waitCount ? waitSemaphores : NULL;
    submitInfo.pWaitDstStageMask = waitCount ? waitStages : NULL;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    return vulkanWrapper.vkQueueSubmit(swapchain->queue, 1, &submitInfo,
                                       swapchain->blitFence) == VK_SUCCESS;
}

static void start_waiter(XWindowSwapchain* swapchain) {
    pthread_mutex_lock(&swapchain->lock);
    if (swapchain->hasWaiter || swapchain->destroying) {
        pthread_mutex_unlock(&swapchain->lock);
        return;
    }
    swapchain->hasWaiter = 1;
    pthread_mutex_unlock(&swapchain->lock);
    if (pthread_create(&swapchain->waiter, NULL, flush_pending_blit,
                       swapchain) != 0) {
        pthread_mutex_lock(&swapchain->lock);
        swapchain->hasWaiter = 0;
        pthread_mutex_unlock(&swapchain->lock);
    }
}

static void submit_blit(XWindowSwapchain* swapchain, uint32_t imageIndex,
                        uint32_t waitSemaphoreCount,
                        const VkSemaphore* waitSemaphores) {
    if (!record_blit(swapchain, imageIndex, waitSemaphoreCount,
                     waitSemaphores)) {
        pthread_mutex_lock(&swapchain->lock);
        swapchain->blitInFlight = 0;
        swapchain->blitSource = -1;
        pthread_mutex_unlock(&swapchain->lock);
        return;
    }
    if (swapchain->blitFence)
        start_waiter(swapchain);
}

static int finish_blit_locked(XWindowSwapchain* swapchain, JNIEnv* env) {
    if (swapchain->blitFence)
        vulkanWrapper.vkResetFences(swapchain->device, 1,
                                    &swapchain->blitFence);
    swapchain->blitSource = -1;
    swapchain->blitInFlight = 0;
    int pending = swapchain->pendingIndex;
    swapchain->pendingIndex = -1;
    pthread_mutex_unlock(&swapchain->lock);
    publish_pixels(swapchain, env);
    return pending;
}

static void *flush_pending_blit(void *arg) {
    XWindowSwapchain* swapchain = arg;
    JNIEnv* env = NULL;
    if (swapchain->jmethods && swapchain->jmethods->jvm)
        (*swapchain->jmethods->jvm)->AttachCurrentThread(
                swapchain->jmethods->jvm, &env, NULL);
    for (;;) {
        if (swapchain->blitFence) {
            VkResult waitStatus = vulkanWrapper.vkWaitForFences(
                    swapchain->device, 1, &swapchain->blitFence, VK_TRUE,
                    100000000ull);
            pthread_mutex_lock(&swapchain->lock);
            if (swapchain->destroying) {
                swapchain->blitInFlight = 0;
                swapchain->hasWaiter = 0;
                pthread_mutex_unlock(&swapchain->lock);
                return NULL;
            }
            pthread_mutex_unlock(&swapchain->lock);
            if (waitStatus == VK_TIMEOUT)
                continue;
        }
        pthread_mutex_lock(&swapchain->lock);
        if (swapchain->destroying) {
            swapchain->blitInFlight = 0;
            swapchain->hasWaiter = 0;
            pthread_mutex_unlock(&swapchain->lock);
            return NULL;
        }
        if (!swapchain->blitInFlight) {
            swapchain->hasWaiter = 0;
            pthread_mutex_unlock(&swapchain->lock);
            return NULL;
        }
        int pending = finish_blit_locked(swapchain, env);
        if (pending < 0) {
            pthread_mutex_lock(&swapchain->lock);
            pending = swapchain->pendingIndex;
            swapchain->pendingIndex = -1;
            if (pending < 0) {
                swapchain->hasWaiter = 0;
                pthread_mutex_unlock(&swapchain->lock);
                return NULL;
            }
            pthread_mutex_unlock(&swapchain->lock);
        }
        pthread_mutex_lock(&swapchain->lock);
        swapchain->blitInFlight = 1;
        swapchain->blitSource = pending;
        pthread_mutex_unlock(&swapchain->lock);
        if (!record_blit(swapchain, (uint32_t)pending, 0, NULL)) {
            pthread_mutex_lock(&swapchain->lock);
            swapchain->blitInFlight = 0;
            swapchain->blitSource = -1;
            swapchain->hasWaiter = 0;
            pthread_mutex_unlock(&swapchain->lock);
            return NULL;
        }
    }
}

VkResult XWindowSwapchain_acquireNextImage(XWindowSwapchain* swapchain, uint64_t timeout, VkSemaphore signalSemaphore, VkFence fence, uint32_t* imageIndex) {
    VkExtent2D windowSize;
    getWindowExtent(swapchain->jmethods, swapchain->windowId, &windowSize);
    if (windowSize.width == 0 || windowSize.height == 0)
        return VK_ERROR_OUT_OF_DATE_KHR;
    if (swapchain->imageExtent.width != windowSize.width
            || swapchain->imageExtent.height != windowSize.height)
        return VK_ERROR_OUT_OF_DATE_KHR;

    pthread_mutex_lock(&swapchain->lock);
    int chosen = pick_free_image(swapchain);
    if (chosen < 0 && swapchain->blitInFlight && swapchain->blitFence) {
        pthread_mutex_unlock(&swapchain->lock);
        VkResult waitStatus = vulkanWrapper.vkWaitForFences(
                swapchain->device, 1, &swapchain->blitFence, VK_TRUE,
                timeout);
        if (waitStatus == VK_TIMEOUT)
            return VK_NOT_READY;
        if (waitStatus != VK_SUCCESS)
            return waitStatus;
        JNIEnv* env = swapchain->jmethods ? swapchain->jmethods->env : NULL;
        pthread_mutex_lock(&swapchain->lock);
        if (swapchain->blitInFlight) {
            int pending = finish_blit_locked(swapchain, env);
            pthread_mutex_lock(&swapchain->lock);
            if (pending >= 0) {
                swapchain->blitInFlight = 1;
                swapchain->blitSource = pending;
                pthread_mutex_unlock(&swapchain->lock);
                submit_blit(swapchain, (uint32_t)pending, 0, NULL);
                pthread_mutex_lock(&swapchain->lock);
            }
        }
        chosen = pick_free_image(swapchain);
    }
    if (chosen < 0) {
        pthread_mutex_unlock(&swapchain->lock);
        return VK_NOT_READY;
    }
    swapchain->acquireIndex = ((uint32_t)chosen + 1)
            % (uint32_t)swapchain->imageCount;
    pthread_mutex_unlock(&swapchain->lock);
    *imageIndex = (uint32_t)chosen;

    if (signalSemaphore || fence) {
        VkSubmitInfo submitInfo = {0};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        if (signalSemaphore) {
            submitInfo.pSignalSemaphores = &signalSemaphore;
            submitInfo.signalSemaphoreCount = 1;
        }
        VkResult result = vulkanWrapper.vkQueueSubmit(swapchain->queue, 1,
                                                      &submitInfo, fence);
        if (result == VK_ERROR_DEVICE_LOST) return result;
    }
    return VK_SUCCESS;
}

void XWindowSwapchain_presentImage(XWindowSwapchain* swapchain) {
    XWindowSwapchain_presentImageIndex(swapchain, 0, 0, NULL);
}

void XWindowSwapchain_presentImageIndex(XWindowSwapchain* swapchain, uint32_t imageIndex,
                                        uint32_t waitSemaphoreCount,
                                        const VkSemaphore* waitSemaphores) {
    if (imageIndex < (uint32_t)swapchain->imageCount)
        swapchain->presented = (int)imageIndex;
    int submit_now = 0;
    pthread_mutex_lock(&swapchain->lock);
    if (swapchain->blitInFlight) {
        swapchain->pendingIndex = (int)imageIndex;
    } else {
        swapchain->pendingIndex = -1;
        swapchain->blitInFlight = 1;
        swapchain->blitSource = (int)imageIndex;
        submit_now = 1;
    }
    pthread_mutex_unlock(&swapchain->lock);
    if (submit_now)
        submit_blit(swapchain, imageIndex, waitSemaphoreCount,
                    waitSemaphores);
}
