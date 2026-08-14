#ifndef VORTEK_XWINDOW_SWAPCHAIN_H
#define VORTEK_XWINDOW_SWAPCHAIN_H

#include <pthread.h>
#include <android/hardware_buffer.h>

#include "vortek.h"

typedef struct XWindowSwapchain_Image {
    VkImage image;
    VkDeviceMemory memory;
} XWindowSwapchain_Image;

/* Real WSI, not a mailbox of special cases:
 *   - client images are DEVICE_LOCAL in the requested format
 *   - present converts into a DEVICE_LOCAL RGBA image, then copies into
 *     a persistently mapped HOST_VISIBLE buffer
 *   - the GLES compositor uploads that buffer; it never locks the AHB
 *     the GPU is writing
 *   - acquire never returns an image that is the current blit source
 */
typedef struct XWindowSwapchain {
    int windowId;
    XWindowSwapchain_Image* images;
    XWindowSwapchain_Image convertTarget;
    int imageCount;
    VkFormat imageFormat;
    VkExtent2D imageExtent;
    VkImageUsageFlags imageUsage;
    VkDevice device;
    VkQueue queue;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkFence blitFence;
    VkBuffer publishBuffer;
    VkDeviceMemory publishMemory;
    void* publishMapped;
    VkDeviceSize publishSize;
    int blitInFlight;
    int blitSource;
    int pendingIndex;
    int destroying;
    int hasWaiter;
    pthread_mutex_t lock;
    pthread_t waiter;
    JMethods* jmethods;
    uint32_t acquireIndex;
} XWindowSwapchain;

extern void getWindowExtent(JMethods* jmethods, int windowId, VkExtent2D* extent);
extern int getSurfaceMinImageCount();
extern VkSurfaceFormatKHR* getSurfaceFormats(uint32_t* formatCount);

extern XWindowSwapchain* XWindowSwapchain_create(VkDevice device, VkQueue graphicsQueue, VkSwapchainCreateInfoKHR* swapchainInfo, JMethods* jmethods, int windowId);
extern void XWindowSwapchain_destroy(VkDevice device, XWindowSwapchain* swapchain);
extern VkResult XWindowSwapchain_acquireNextImage(XWindowSwapchain* swapchain, uint64_t timeout, VkSemaphore signalSemaphore, VkFence fence, uint32_t* imageIndex);
extern void XWindowSwapchain_presentImageIndex(XWindowSwapchain* swapchain, uint32_t imageIndex,
                                               uint32_t waitSemaphoreCount,
                                               const VkSemaphore* waitSemaphores);

#endif
