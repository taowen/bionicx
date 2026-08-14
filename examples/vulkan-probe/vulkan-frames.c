#include <pthread.h>

#include "common.h"

static volatile int frames_done;

static void *frames_watchdog(void *unused) {
    (void)unused;
    for (int i = 0; i < 80; ++i) {
        if (frames_done) return NULL;
        usleep(100000);
    }
    printf("BXTEST FAIL vulkan-frames "
           "hung after timeline wait plus present burst\n");
    fflush(stdout);
    _exit(2);
}

/* First present green, then present blue. Chrome's UI painted once and
 * then froze: the compositor kept the first AHB copy. A probe that only
 * presents one color cannot see that. */

static int bring_up(ProbeEnv *env) {
    if (!probe_open_window(env)) return 0;
    vkEnumerateInstanceVersion(&env->loader_version);
    if (!probe_create_instance(env)) return 0;
    if (!probe_pick_physical(env)) return 0;
    probe_create_surfaces(env);
    probe_query_surface(env);
    return probe_create_device(env) && probe_create_swapchain(env);
}

static VkResult present_color(ProbeEnv *env, VkCommandBuffer command_buffer,
                              float red, float green, float blue,
                              uint32_t *image_index) {
    uint32_t index = UINT32_MAX;
    VkResult status = vkAcquireNextImageKHR(
            env->device, env->swapchain, UINT64_MAX, VK_NULL_HANDLE,
            VK_NULL_HANDLE, &index);
    if (status != VK_SUCCESS || index >= env->image_count)
        return status != VK_SUCCESS ? status : VK_ERROR_OUT_OF_DATE_KHR;
    *image_index = index;

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
    VkClearColorValue color = {
        .float32 = {red, green, blue, 1.0f},
    };
    vkCmdClearColorImage(command_buffer, env->swapchain_images[index],
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1,
                         &range);
    VkImageMemoryBarrier to_present = to_transfer;
    to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_present.dstAccessMask = 0;
    to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
                         0, NULL, 0, NULL, 1, &to_present);
    status = vkEndCommandBuffer(command_buffer);
    if (status != VK_SUCCESS) return status;

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };
    status = vkQueueSubmit(env->queue, 1, &submit_info, VK_NULL_HANDLE);
    if (status != VK_SUCCESS) return status;

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount = 1,
        .pSwapchains = &env->swapchain,
        .pImageIndices = &index,
    };
    return vkQueuePresentKHR(env->queue, &present_info);
}

int main(void) {
    ProbeEnv env;
    probe_env_init(&env);
    if (!bring_up(&env)) {
        snprintf(env.details, sizeof(env.details), "bring-up failed");
        result(&env, "vulkan-frames", false);
        probe_env_destroy(&env);
        printf("BXSUMMARY vulkan-frames passed=%u failed=%u\n",
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
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkResult alloc_status = pool_status == VK_SUCCESS
            ? vkAllocateCommandBuffers(env.device, &alloc_info, &command_buffer)
            : pool_status;

    uint32_t first_index = UINT32_MAX;
    VkResult first_status = alloc_status == VK_SUCCESS
            ? present_color(&env, command_buffer, 0.10f, 0.75f, 0.25f,
                            &first_index)
            : alloc_status;
    XSync(env.display, False);
    /* Let the compositor copy the first (green) frame the way Chrome's
     * first NTP paint is copied. */
    usleep(400000);

    /* Chrome leaves a timeline wait on the graphics queue and keeps
     * presenting. Re-present the green image many times so a leaked
     * present command buffer or a stuck blit is visible as a frozen
     * first frame. */
    VkSemaphore timeline = VK_NULL_HANDLE;
    VkSemaphoreTypeCreateInfo timeline_type = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
    };
    VkSemaphoreCreateInfo timeline_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &timeline_type,
    };
    VkResult timeline_create = vkCreateSemaphore(
            env.device, &timeline_info, NULL, &timeline);
    uint64_t timeline_wait_value = 1;
    VkPipelineStageFlags timeline_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkTimelineSemaphoreSubmitInfo timeline_submit = {
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .waitSemaphoreValueCount = 1,
        .pWaitSemaphoreValues = &timeline_wait_value,
    };
    VkSubmitInfo timeline_wait_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = &timeline_submit,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &timeline,
        .pWaitDstStageMask = &timeline_stage,
    };
    VkResult timeline_wait_status = timeline_create == VK_SUCCESS
            ? vkQueueSubmit(env.queue, 1, &timeline_wait_info, VK_NULL_HANDLE)
            : timeline_create;
    frames_done = 0;
    pthread_t watchdog;
    if (pthread_create(&watchdog, NULL, frames_watchdog, NULL) == 0)
        pthread_detach(watchdog);
    unsigned burst = 0;
    VkResult burst_status = VK_ERROR_INITIALIZATION_FAILED;
    if (first_status == VK_SUCCESS && timeline_wait_status == VK_SUCCESS) {
        VkPresentInfoKHR burst_present = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .swapchainCount = 1,
            .pSwapchains = &env.swapchain,
            .pImageIndices = &first_index,
        };
        burst_status = VK_SUCCESS;
        for (burst = 0; burst < 2048 && burst_status == VK_SUCCESS; ++burst)
            burst_status = vkQueuePresentKHR(env.queue, &burst_present);
    }
    VkSemaphoreSignalInfo signal_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
        .semaphore = timeline,
        .value = 1,
    };
    VkResult timeline_signal = timeline_wait_status == VK_SUCCESS
            ? vkSignalSemaphore(env.device, &signal_info)
            : timeline_wait_status;

    uint32_t second_index = UINT32_MAX;
    VkResult second_status = first_status == VK_SUCCESS
            && burst_status == VK_SUCCESS
            && timeline_signal == VK_SUCCESS
            ? present_color(&env, command_buffer, 0.08f, 0.31f, 0.94f,
                            &second_index)
            : VK_ERROR_INITIALIZATION_FAILED;
    XSync(env.display, False);

    snprintf(env.details, sizeof(env.details),
             "first=%d index=%u burst=%u/%d second=%d index=%u "
             "timeline-wait=%d signal=%d "
             "background1=26,191,64 background2=20,80,240",
             first_status, first_index, burst, burst_status,
             second_status, second_index, timeline_wait_status,
             timeline_signal);
    frames_done = 1;
    result(&env, "vulkan-frames",
           first_status == VK_SUCCESS && burst == 2048
                   && burst_status == VK_SUCCESS
                   && second_status == VK_SUCCESS
                   && timeline_wait_status == VK_SUCCESS
                   && timeline_signal == VK_SUCCESS
                   && first_index < env.image_count
                   && second_index < env.image_count);
    if (second_status == VK_SUCCESS)
        usleep(4000000);

    if (timeline != VK_NULL_HANDLE)
        vkDestroySemaphore(env.device, timeline, NULL);

    if (env.device != VK_NULL_HANDLE) vkDeviceWaitIdle(env.device);
    if (command_pool != VK_NULL_HANDLE)
        vkDestroyCommandPool(env.device, command_pool, NULL);
    probe_env_destroy(&env);
    printf("BXSUMMARY vulkan-frames passed=%u failed=%u\n",
           env.passed, env.failed);
    return env.failed ? 1 : 0;
}
