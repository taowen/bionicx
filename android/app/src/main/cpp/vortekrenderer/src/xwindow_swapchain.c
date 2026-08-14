#include "xwindow_swapchain.h"
#include "vulkan_helper.h"

void getWindowExtent(JMethods* jmethods, int windowId, VkExtent2D* extent) {
    extent->width = (*jmethods->env)->CallIntMethod(jmethods->env, jmethods->obj, jmethods->getWindowWidth, windowId);
    extent->height = (*jmethods->env)->CallIntMethod(jmethods->env, jmethods->obj, jmethods->getWindowHeight, windowId);
}

static AHardwareBuffer* getWindowHardwareBuffer(JMethods* jmethods, int windowId, jboolean useHALPixelFormatBGRA8888) {
    jlong hardwareBufferPtr = (*jmethods->env)->CallLongMethod(jmethods->env, jmethods->obj, jmethods->getWindowHardwareBuffer, windowId, useHALPixelFormatBGRA8888);
    return (AHardwareBuffer*)hardwareBufferPtr;
}

static uint32_t memoryTypeForAhb(uint32_t typeBits) {
    uint32_t index = getMemoryTypeIndex(typeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (typeBits & (1u << index))
        return index;
    for (uint32_t i = 0; typeBits != 0; i++, typeBits >>= 1) {
        if (typeBits & 1)
            return i;
    }
    return 0;
}

static VkResult createImageMemory(VkDevice device, VkImage image, AHardwareBuffer* hardwareBuffer, VkDeviceMemory* pMemory) {
    VkAndroidHardwareBufferFormatPropertiesANDROID formatProperties = {0};
    formatProperties.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID;

    VkAndroidHardwareBufferPropertiesANDROID ahbProperties = {0};
    ahbProperties.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
    ahbProperties.pNext = &formatProperties;
    VkResult result = vulkanWrapper.vkGetAndroidHardwareBufferPropertiesANDROID(
            device, hardwareBuffer, &ahbProperties);
    if (result != VK_SUCCESS) {
        println("vortek: AHB properties failed result=%d", result);
        return result;
    }

    VkImportAndroidHardwareBufferInfoANDROID memoryImportInfo = {0};
    memoryImportInfo.sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
    memoryImportInfo.buffer = hardwareBuffer;

    VkMemoryDedicatedAllocateInfo memoryDedicatedInfo = {0};
    memoryDedicatedInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    memoryDedicatedInfo.pNext = &memoryImportInfo;
    memoryDedicatedInfo.image = image;
    memoryDedicatedInfo.buffer = VK_NULL_HANDLE;

    VkMemoryAllocateInfo memoryInfo = {0};
    memoryInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryInfo.pNext = &memoryDedicatedInfo;
    memoryInfo.allocationSize = ahbProperties.allocationSize;
    memoryInfo.memoryTypeIndex = memoryTypeForAhb(ahbProperties.memoryTypeBits);

    VkDeviceMemory memory;
    result = vulkanWrapper.vkAllocateMemory(device, &memoryInfo, NULL, &memory);
    if (result != VK_SUCCESS) {
        println("vortek: AHB allocateMemory result=%d size=%llu type=%u bits=0x%x vkFormat=%d",
                result, (unsigned long long)ahbProperties.allocationSize,
                memoryInfo.memoryTypeIndex, ahbProperties.memoryTypeBits,
                formatProperties.format);
        return result;
    }

    result = vulkanWrapper.vkBindImageMemory(device, image, memory, 0);
    if (result != VK_SUCCESS) {
        println("vortek: AHB bindImageMemory result=%d", result);
        vulkanWrapper.vkFreeMemory(device, memory, NULL);
        return result;
    }

    *pMemory = memory;
    return VK_SUCCESS;
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
    memoryInfo.memoryTypeIndex = memoryTypeForAhb(requirements.memoryTypeBits);
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

static VkResult createAhbImage(VkDevice device, XWindowSwapchain* swapchain, XWindowSwapchain_Image* swapchainImage) {
    /* Mali cannot use BGRA AHB as a color attachment. The compositor
     * always samples an RGBA window buffer; client images blit into it. */
    AHardwareBuffer* hardwareBuffer = getWindowHardwareBuffer(swapchain->jmethods, swapchain->windowId, JNI_FALSE);
    if (hardwareBuffer == NULL)
        return VK_ERROR_INITIALIZATION_FAILED;

    AHardwareBuffer_Desc ahbDesc = {0};
    AHardwareBuffer_describe(hardwareBuffer, &ahbDesc);

    VkExternalFormatANDROID externalFormatAndroid = {0};
    externalFormatAndroid.sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID;
    externalFormatAndroid.externalFormat = 0;

    VkExternalMemoryImageCreateInfo externalMemoryImageInfo = {0};
    externalMemoryImageInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalMemoryImageInfo.pNext = &externalFormatAndroid;
    externalMemoryImageInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;

    VkImageCreateInfo imageInfo = {0};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = &externalMemoryImageInfo;
    imageInfo.flags = VK_IMAGE_CREATE_ALIAS_BIT;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent.width = ahbDesc.width;
    imageInfo.extent.height = ahbDesc.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image;
    VkDeviceMemory memory;
    VkResult result;

    result = vulkanWrapper.vkCreateImage(device, &imageInfo, NULL, &image);
    if (result != VK_SUCCESS) return result;

    result = createImageMemory(device, image, hardwareBuffer, &memory);
    if (result != VK_SUCCESS) return result;

    swapchainImage->image = image;
    swapchainImage->memory = memory;
    return VK_SUCCESS;
}

int getSurfaceMinImageCount() {
    return 2;
}

VkSurfaceFormatKHR* getSurfaceFormats(uint32_t* formatCount) {
    /* ANGLE on X11 prefers BGRA to match the root visual. Mali cannot
     * import BGRA AHB as a color attachment, so client images stay in
     * the requested format and blit into an RGBA window AHB. */
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

    VkResult result;
    for (int i = 0; i < swapchain->imageCount; i++) {
        result = createDeviceImage(device, swapchain, &swapchain->images[i]);
        if (result != VK_SUCCESS) goto error;
    }
    result = createAhbImage(device, swapchain, &swapchain->presentTarget);
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
    if (swapchain->blitInFlight && swapchain->blitFence)
        vulkanWrapper.vkWaitForFences(device, 1, &swapchain->blitFence,
                                      VK_TRUE, 1000000000ull);
    if (swapchain->blitFence)
        vulkanWrapper.vkDestroyFence(device, swapchain->blitFence, NULL);
    if (swapchain->commandPool)
        vulkanWrapper.vkDestroyCommandPool(device, swapchain->commandPool, NULL);
    if (swapchain->presentTarget.image)
        vulkanWrapper.vkDestroyImage(device, swapchain->presentTarget.image, NULL);
    if (swapchain->presentTarget.memory)
        vulkanWrapper.vkFreeMemory(device, swapchain->presentTarget.memory, NULL);
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

VkResult XWindowSwapchain_acquireNextImage(XWindowSwapchain* swapchain, uint64_t timeout, VkSemaphore signalSemaphore, VkFence fence, uint32_t* imageIndex) {
    if (signalSemaphore || fence) {
        VkSubmitInfo submitInfo = {0};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        if (signalSemaphore) {
            submitInfo.pSignalSemaphores = &signalSemaphore;
            submitInfo.signalSemaphoreCount = 1;
        }

        VkResult result = vulkanWrapper.vkQueueSubmit(swapchain->queue, 1, &submitInfo, fence);
        if (result == VK_ERROR_DEVICE_LOST) return result;
    }

    VkExtent2D windowSize;
    getWindowExtent(swapchain->jmethods, swapchain->windowId, &windowSize);

    if (windowSize.width == 0 || windowSize.height == 0)
        return VK_ERROR_OUT_OF_DATE_KHR;
    if (swapchain->imageExtent.width != windowSize.width || swapchain->imageExtent.height != windowSize.height)
        return VK_ERROR_OUT_OF_DATE_KHR;

    uint32_t index = swapchain->acquireIndex % (uint32_t)swapchain->imageCount;
    if (swapchain->imageCount > 1 && swapchain->presented >= 0
            && index == (uint32_t)swapchain->presented)
        index = (index + 1) % (uint32_t)swapchain->imageCount;
    swapchain->acquireIndex = (index + 1) % (uint32_t)swapchain->imageCount;
    *imageIndex = index;
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
    /* Mali does not make PRESENT_SRC AHB contents visible to CPU/GLES
     * without an explicit host-read barrier. Do not vkQueueWaitIdle:
     * in-flight timeline waits on this queue must not block later RPCs.
     * Mailbox: one blit in flight. Extra presents after a timeline wait
     * would fill the queue and block vkQueueSubmit on the RPC thread. */
    int canBlit = 1;
    if (swapchain->blitInFlight && swapchain->blitFence) {
        VkResult fenceStatus = vulkanWrapper.vkGetFenceStatus(
                swapchain->device, swapchain->blitFence);
        if (fenceStatus == VK_NOT_READY)
            canBlit = 0;
        else {
            swapchain->blitInFlight = 0;
            if (fenceStatus == VK_SUCCESS)
                vulkanWrapper.vkResetFences(swapchain->device, 1,
                                            &swapchain->blitFence);
        }
    }
    VkCommandBuffer commandBuffer = swapchain->commandBuffer;
    if (canBlit && commandBuffer && swapchain->commandPool && swapchain->queue
            && swapchain->images
            && imageIndex < (uint32_t)swapchain->imageCount
            && swapchain->presentTarget.image) {
        vulkanWrapper.vkResetCommandBuffer(commandBuffer, 0);
        VkCommandBufferBeginInfo beginInfo = {0};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vulkanWrapper.vkBeginCommandBuffer(commandBuffer,
                                               &beginInfo) == VK_SUCCESS) {
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
            barriers[1].image = swapchain->presentTarget.image;
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
                    swapchain->presentTarget.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &blit, VK_FILTER_NEAREST);
            VkImageMemoryBarrier hostBarrier = barriers[1];
            hostBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            hostBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            hostBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            hostBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            vulkanWrapper.vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_HOST_BIT,
                    0, 0, NULL, 0, NULL, 1, &hostBarrier);
            /* WSI leaves the presented image in PRESENT_SRC. ANGLE's next
             * frame barriers from that layout; Mali hangs if we leave
             * TRANSFER_SRC. */
            VkImageMemoryBarrier presentBarrier = barriers[0];
            presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            presentBarrier.dstAccessMask = 0;
            presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            vulkanWrapper.vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    0, 0, NULL, 0, NULL, 1, &presentBarrier);
            vulkanWrapper.vkEndCommandBuffer(commandBuffer);
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
            vulkanWrapper.vkQueueSubmit(swapchain->queue, 1, &submitInfo,
                                        swapchain->blitFence);
            swapchain->blitInFlight = swapchain->blitFence ? 1 : 0;
        }
    }
    (*swapchain->jmethods->env)->CallVoidMethod(swapchain->jmethods->env, swapchain->jmethods->obj,
                                                swapchain->jmethods->updateWindowContent, swapchain->windowId);
}
