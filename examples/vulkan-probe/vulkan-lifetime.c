#include "common.h"

static int bring_up(ProbeEnv *env) {
    if (!probe_open_window(env)) return 0;
    vkEnumerateInstanceVersion(&env->loader_version);
    if (!probe_create_instance(env)) return 0;
    if (!probe_pick_physical(env)) return 0;
    probe_create_surfaces(env);
    probe_query_surface(env);
    return probe_create_device(env) && probe_create_swapchain(env);
}

int main(void) {
    ProbeEnv env;
    probe_env_init(&env);
    if (!bring_up(&env)) {
        snprintf(env.details, sizeof(env.details), "bring-up failed");
        result(&env, "vulkan-lifetime", false);
        probe_env_destroy(&env);
        printf("BXSUMMARY vulkan-lifetime passed=%u failed=%u\n",
               env.passed, env.failed);
        return 1;
    }

    uint32_t first = UINT32_MAX, second = UINT32_MAX;
    VkResult first_acquire = vkAcquireNextImageKHR(
            env.device, env.swapchain, UINT64_MAX, VK_NULL_HANDLE,
            VK_NULL_HANDLE, &first);
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount = 1,
        .pSwapchains = &env.swapchain,
        .pImageIndices = &first,
    };
    if (first_acquire == VK_SUCCESS)
        vkQueuePresentKHR(env.queue, &present_info);
    VkResult second_acquire = vkAcquireNextImageKHR(
            env.device, env.swapchain, UINT64_MAX, VK_NULL_HANDLE,
            VK_NULL_HANDLE, &second);
    bool rotate_ok = first_acquire == VK_SUCCESS
            && second_acquire == VK_SUCCESS
            && env.image_count >= 2
            && second != first
            && second < env.image_count;

    XResizeWindow(env.display, env.window, 800, 400);
    XSync(env.display, False);
    usleep(50000);
    VkSurfaceCapabilitiesKHR resized = {0};
    VkResult resized_caps = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            env.physical_device, env.surface, &resized);
    uint32_t stale_index = 0;
    VkResult stale_acquire = vkAcquireNextImageKHR(
            env.device, env.swapchain, UINT64_MAX, VK_NULL_HANDLE,
            VK_NULL_HANDLE, &stale_index);
    bool outdated_ok = resized_caps == VK_SUCCESS
            && (stale_acquire == VK_ERROR_OUT_OF_DATE_KHR
                || resized.currentExtent.width != 640);

    VkSwapchainKHR recreated = VK_NULL_HANDLE;
    env.swapchain_info.oldSwapchain = env.swapchain;
    env.swapchain_info.imageExtent = resized.currentExtent.width > 0
            ? resized.currentExtent
            : (VkExtent2D){800, 400};
    env.swapchain_info.minImageCount = resized.minImageCount >= 2
            ? resized.minImageCount : 2;
    VkResult recreate_status = vkCreateSwapchainKHR(
            env.device, &env.swapchain_info, NULL, &recreated);
    if (env.swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(env.device, env.swapchain, NULL);
    env.swapchain = recreated;
    uint32_t recreated_count = 0, recreated_index = 0;
    VkResult recreated_acquire = VK_ERROR_INITIALIZATION_FAILED;
    if (recreate_status == VK_SUCCESS && env.swapchain != VK_NULL_HANDLE) {
        vkGetSwapchainImagesKHR(env.device, env.swapchain,
                                &recreated_count, NULL);
        recreated_acquire = vkAcquireNextImageKHR(
                env.device, env.swapchain, UINT64_MAX, VK_NULL_HANDLE,
                VK_NULL_HANDLE, &recreated_index);
        present_info.pSwapchains = &env.swapchain;
        present_info.pImageIndices = &recreated_index;
        if (recreated_acquire == VK_SUCCESS)
            vkQueuePresentKHR(env.queue, &present_info);
    }
    bool recreate_ok = recreate_status == VK_SUCCESS && recreated_count >= 2
            && recreated_acquire == VK_SUCCESS;

    XResizeWindow(env.display, env.window, 640, 360);
    XUnmapWindow(env.display, env.window);
    XSync(env.display, False);
    usleep(50000);
    XMapWindow(env.display, env.window);
    XSync(env.display, False);
    usleep(50000);
    VkSurfaceCapabilitiesKHR remapped = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            env.physical_device, env.surface, &remapped);
    snprintf(env.details, sizeof(env.details),
             "rotate=%u first=%u second=%u outdated=%u extent=%ux%u acquire=%d "
             "recreate=%u images=%u remap=%ux%u",
             rotate_ok, first, second, outdated_ok,
             resized.currentExtent.width, resized.currentExtent.height,
             stale_acquire, recreate_ok, recreated_count,
             remapped.currentExtent.width, remapped.currentExtent.height);
    result(&env, "vulkan-lifetime",
           rotate_ok && outdated_ok && recreate_ok
                   && remapped.currentExtent.width > 0
                   && remapped.currentExtent.height > 0);
    probe_env_destroy(&env);
    printf("BXSUMMARY vulkan-lifetime passed=%u failed=%u\n",
           env.passed, env.failed);
    return env.failed ? 1 : 0;
}
