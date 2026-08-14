#include <pthread.h>
#include <time.h>

#include "common.h"

/* ANGLE/Chrome: acquire(sem+fence), wait the fence, submit a render-done
 * semaphore, present, then immediately acquire again. Sequential probes
 * sleep hundreds of milliseconds between colors, so they never overlap
 * the compositor blit. Chrome on Mali freezes after the first painted
 * frame; Adreno shows later frames. This probe is that overlap. */

#define FRAME_COUNT 16
#define IN_FLIGHT 3

static volatile int frames_done;

static void *frames_watchdog(void *unused) {
    (void)unused;
    for (int i = 0; i < 200; ++i) {
        if (frames_done) return NULL;
        usleep(100000);
    }
    printf("BXTEST FAIL vulkan-chrome-frames hung overlapping presents\n");
    fflush(stdout);
    _exit(2);
}

static int bring_up(ProbeEnv *env) {
    if (!probe_open_window(env)) return 0;
    vkEnumerateInstanceVersion(&env->loader_version);
    if (!probe_create_instance(env)) return 0;
    if (!probe_pick_physical(env)) return 0;
    probe_create_surfaces(env);
    probe_query_surface(env);
    return probe_create_device(env) && probe_create_swapchain(env);
}

static int recreate_at(ProbeEnv *env, uint32_t width, uint32_t height) {
    XResizeWindow(env->display, env->window, (unsigned)width,
                  (unsigned)height);
    XSync(env->display, False);
    usleep(50000);
    VkSurfaceCapabilitiesKHR caps = {0};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                env->physical_device, env->surface, &caps) != VK_SUCCESS)
        return 0;
    VkSwapchainKHR created = VK_NULL_HANDLE;
    env->swapchain_info.oldSwapchain = env->swapchain;
    env->swapchain_info.imageExtent = caps.currentExtent.width > 0
            ? caps.currentExtent : (VkExtent2D){width, height};
    env->swapchain_info.minImageCount = caps.minImageCount >= 2
            ? caps.minImageCount : 2;
    if (vkCreateSwapchainKHR(env->device, &env->swapchain_info, NULL,
                             &created) != VK_SUCCESS)
        return 0;
    if (env->swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(env->device, env->swapchain, NULL);
    env->swapchain = created;
    free(env->swapchain_images);
    env->swapchain_images = NULL;
    env->image_count = 0;
    if (vkGetSwapchainImagesKHR(env->device, env->swapchain,
                                &env->image_count, NULL) != VK_SUCCESS)
        return 0;
    env->swapchain_images = calloc(env->image_count,
                                   sizeof(*env->swapchain_images));
    return env->swapchain_images != NULL
            && vkGetSwapchainImagesKHR(env->device, env->swapchain,
                                       &env->image_count,
                                       env->swapchain_images) == VK_SUCCESS
            && env->image_count >= 2;
}

static VkResult present_color(ProbeEnv *env, VkCommandBuffer command_buffer,
                              VkSemaphore acquire_sem, VkFence acquire_fence,
                              VkFence submit_fence, VkSemaphore render_done,
                              float red, float green, float blue,
                              uint32_t *image_index, int frame) {
    uint32_t index = UINT32_MAX;
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    VkResult status = vkAcquireNextImageKHR(
            env->device, env->swapchain, 2000000000ull, acquire_sem,
            acquire_fence, &index);
    if (status != VK_SUCCESS || index >= env->image_count) {
        printf("BXTEST FAIL vulkan-chrome-frames acquire frame=%d status=%d\n",
               frame, status);
        fflush(stdout);
        return status != VK_SUCCESS ? status : VK_ERROR_OUT_OF_DATE_KHR;
    }
    status = vkWaitForFences(env->device, 1, &acquire_fence, VK_TRUE,
                             2000000000ull);
    if (status != VK_SUCCESS) {
        printf("BXTEST FAIL vulkan-chrome-frames acquire-fence frame=%d "
               "status=%d\n", frame, status);
        fflush(stdout);
        return status;
    }
    status = vkResetFences(env->device, 1, &acquire_fence);
    if (status != VK_SUCCESS) return status;
    *image_index = index;

    status = vkWaitForFences(env->device, 1, &submit_fence, VK_TRUE,
                             2000000000ull);
    if (status != VK_SUCCESS) {
        printf("BXTEST FAIL vulkan-chrome-frames submit-fence frame=%d "
               "status=%d\n", frame, status);
        fflush(stdout);
        return status;
    }
    status = vkResetFences(env->device, 1, &submit_fence);
    if (status != VK_SUCCESS) return status;
    status = vkResetCommandBuffer(command_buffer, 0);
    if (status != VK_SUCCESS) return status;
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    status = vkBeginCommandBuffer(command_buffer, &begin_info);
    if (status != VK_SUCCESS) return status;

    VkImageSubresourceRange range = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };
    VkImageMemoryBarrier to_transfer = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = env->swapchain_images[index],
        .subresourceRange = range,
    };
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, NULL, 0, NULL, 1, &to_transfer);
    VkClearColorValue color = {.float32 = {red, green, blue, 1.0f}};
    vkCmdClearColorImage(command_buffer, env->swapchain_images[index],
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1,
                         &range);
    VkImageMemoryBarrier to_present = to_transfer;
    to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
                         0, NULL, 0, NULL, 1, &to_present);
    status = vkEndCommandBuffer(command_buffer);
    if (status != VK_SUCCESS) return status;

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &acquire_sem,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &render_done,
    };
    status = vkQueueSubmit(env->queue, 1, &submit_info, submit_fence);
    if (status != VK_SUCCESS) return status;

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &render_done,
        .swapchainCount = 1,
        .pSwapchains = &env->swapchain,
        .pImageIndices = &index,
    };
    status = vkQueuePresentKHR(env->queue, &present_info);
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    long ms = (end.tv_sec - start.tv_sec) * 1000
            + (end.tv_nsec - start.tv_nsec) / 1000000;
    printf("BXTEST INFO vulkan-chrome-frames frame=%d index=%u present=%d "
           "ms=%ld\n", frame, index, status, ms);
    fflush(stdout);
    return status;
}

int main(void) {
    ProbeEnv env;
    probe_env_init(&env);
    if (!bring_up(&env) || !recreate_at(&env, 1920, 1080)) {
        snprintf(env.details, sizeof(env.details), "bring-up failed");
        result(&env, "vulkan-chrome-frames", false);
        probe_env_destroy(&env);
        printf("BXSUMMARY vulkan-chrome-frames passed=%u failed=%u\n",
               env.passed, env.failed);
        return 1;
    }

    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = env.graphics_family,
    };
    VkResult pool_status = vkCreateCommandPool(
            env.device, &pool_info, NULL, &command_pool);
    VkCommandBuffer command_buffers[IN_FLIGHT];
    memset(command_buffers, 0, sizeof(command_buffers));
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = IN_FLIGHT,
    };
    VkResult alloc_status = pool_status == VK_SUCCESS
            ? vkAllocateCommandBuffers(env.device, &alloc_info, command_buffers)
            : pool_status;

    VkSemaphore acquire_sems[IN_FLIGHT];
    VkSemaphore render_sems[IN_FLIGHT];
    VkFence acquire_fences[IN_FLIGHT];
    VkFence submit_fences[IN_FLIGHT];
    memset(acquire_sems, 0, sizeof(acquire_sems));
    memset(render_sems, 0, sizeof(render_sems));
    memset(acquire_fences, 0, sizeof(acquire_fences));
    memset(submit_fences, 0, sizeof(submit_fences));
    VkSemaphoreCreateInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    VkFenceCreateInfo signaled_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    int sync_ok = alloc_status == VK_SUCCESS;
    for (int i = 0; sync_ok && i < IN_FLIGHT; ++i) {
        sync_ok = vkCreateSemaphore(env.device, &sem_info, NULL,
                                    &acquire_sems[i]) == VK_SUCCESS
                && vkCreateSemaphore(env.device, &sem_info, NULL,
                                     &render_sems[i]) == VK_SUCCESS
                && vkCreateFence(env.device, &fence_info, NULL,
                                 &acquire_fences[i]) == VK_SUCCESS
                && vkCreateFence(env.device, &signaled_info, NULL,
                                 &submit_fences[i]) == VK_SUCCESS;
    }

    frames_done = 0;
    pthread_t watchdog;
    if (pthread_create(&watchdog, NULL, frames_watchdog, NULL) == 0)
        pthread_detach(watchdog);

    uint32_t first_index = UINT32_MAX;
    uint32_t last_index = UINT32_MAX;
    VkResult first_status = VK_ERROR_INITIALIZATION_FAILED;
    VkResult last_status = VK_ERROR_INITIALIZATION_FAILED;
    int completed = 0;
    if (sync_ok) {
        for (int frame = 0; frame < FRAME_COUNT; ++frame) {
            int slot = frame % IN_FLIGHT;
            float red = frame == 0 ? 0.10f : 0.08f;
            float green = frame == 0 ? 0.75f : 0.31f;
            float blue = frame == 0 ? 0.25f : 0.94f;
            uint32_t index = UINT32_MAX;
            VkResult status = present_color(
                    &env, command_buffers[slot], acquire_sems[slot],
                    acquire_fences[slot], submit_fences[slot],
                    render_sems[slot], red, green, blue, &index, frame);
            if (frame == 0) {
                first_status = status;
                first_index = index;
            }
            last_status = status;
            last_index = index;
            if (status != VK_SUCCESS)
                break;
            completed = frame + 1;
        }
    }

    snprintf(env.details, sizeof(env.details),
             "extent=%ux%u completed=%d/%d first=%d index=%u last=%d "
             "index=%u background1=26,191,64 background2=20,80,240",
             env.swapchain_info.imageExtent.width,
             env.swapchain_info.imageExtent.height, completed, FRAME_COUNT,
             first_status, first_index, last_status, last_index);
    result(&env, "vulkan-chrome-frames",
           first_status == VK_SUCCESS && last_status == VK_SUCCESS
                   && completed == FRAME_COUNT
                   && env.swapchain_info.imageExtent.width >= 1800
                   && env.swapchain_info.imageExtent.height >= 1000);
    frames_done = 1;
    if (last_status == VK_SUCCESS) {
        printf("BXTEST HOLD vulkan-chrome-frames\n");
        fflush(stdout);
        usleep(4000000);
    }

    if (env.device != VK_NULL_HANDLE) vkDeviceWaitIdle(env.device);
    for (int i = 0; i < IN_FLIGHT; ++i) {
        if (acquire_sems[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(env.device, acquire_sems[i], NULL);
        if (render_sems[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(env.device, render_sems[i], NULL);
        if (acquire_fences[i] != VK_NULL_HANDLE)
            vkDestroyFence(env.device, acquire_fences[i], NULL);
        if (submit_fences[i] != VK_NULL_HANDLE)
            vkDestroyFence(env.device, submit_fences[i], NULL);
    }
    if (command_pool != VK_NULL_HANDLE)
        vkDestroyCommandPool(env.device, command_pool, NULL);
    probe_env_destroy(&env);
    printf("BXSUMMARY vulkan-chrome-frames passed=%u failed=%u\n",
           env.passed, env.failed);
    return env.failed ? 1 : 0;
}
