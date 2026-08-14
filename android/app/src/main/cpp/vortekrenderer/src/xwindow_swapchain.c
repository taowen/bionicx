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

static VkResult createImage(VkDevice device, XWindowSwapchain* swapchain, XWindowSwapchain_Image* swapchainImage) {
    /* Mali-G1 reports BGRA AHB as an external format that cannot be a
     * COLOR_ATTACHMENT. Allocate RGBA so the imported image is renderable
     * and the GLES compositor can sample it. */
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
    imageInfo.format = swapchain->imageFormat;
    imageInfo.extent.width = ahbDesc.width;
    imageInfo.extent.height = ahbDesc.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = swapchain->imageUsage;
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
    static const VkFormat supportedFormats[] = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB};
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
    result = createImage(device, swapchain, &swapchain->images[0]);
    if (result != VK_SUCCESS) goto error;
    /* Extra indices rotate at the protocol level. A second
     * AHardwareBuffer import of the same window buffer can invalidate
     * the first image, so every slot aliases the one imported image. */
    for (int i = 1; i < swapchain->imageCount; i++)
        swapchain->images[i] = swapchain->images[0];

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
    }
    return swapchain;

error:
    MEMFREE(swapchain->images);
    MEMFREE(swapchain);
    return NULL;
}

void XWindowSwapchain_destroy(VkDevice device, XWindowSwapchain* swapchain) {
    if (!swapchain) return;
    if (swapchain->commandPool)
        vulkanWrapper.vkDestroyCommandPool(device, swapchain->commandPool, NULL);
    if (swapchain->imageCount > 0 && swapchain->images) {
        vulkanWrapper.vkDestroyImage(device, swapchain->images[0].image, NULL);
        vulkanWrapper.vkFreeMemory(device, swapchain->images[0].memory, NULL);
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
    XWindowSwapchain_presentImageIndex(swapchain, 0);
}

void XWindowSwapchain_presentImageIndex(XWindowSwapchain* swapchain, uint32_t imageIndex) {
    if (imageIndex < (uint32_t)swapchain->imageCount)
        swapchain->presented = (int)imageIndex;
    /* Mali does not make PRESENT_SRC AHB contents visible to CPU/GLES
     * without an explicit host-read barrier. */
    if (swapchain->commandBuffer && swapchain->queue && swapchain->images) {
        vulkanWrapper.vkResetCommandBuffer(swapchain->commandBuffer, 0);
        VkCommandBufferBeginInfo beginInfo = {0};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vulkanWrapper.vkBeginCommandBuffer(swapchain->commandBuffer,
                                               &beginInfo) == VK_SUCCESS) {
            VkImageSubresourceRange range = {0};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.levelCount = 1;
            range.layerCount = 1;
            VkImageMemoryBarrier barrier = {0};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                    VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT |
                                    VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = swapchain->images[0].image;
            barrier.subresourceRange = range;
            vulkanWrapper.vkCmdPipelineBarrier(
                    swapchain->commandBuffer,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_HOST_BIT |
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0, 0, NULL, 0, NULL, 1, &barrier);
            vulkanWrapper.vkEndCommandBuffer(swapchain->commandBuffer);
            VkSubmitInfo submitInfo = {0};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &swapchain->commandBuffer;
            vulkanWrapper.vkQueueSubmit(swapchain->queue, 1, &submitInfo,
                                        VK_NULL_HANDLE);
            vulkanWrapper.vkQueueWaitIdle(swapchain->queue);
        }
    }
    (*swapchain->jmethods->env)->CallVoidMethod(swapchain->jmethods->env, swapchain->jmethods->obj,
                                                swapchain->jmethods->updateWindowContent, swapchain->windowId);
}
