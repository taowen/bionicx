#include "common.h"

static bool format_usable(const VkFormatProperties *properties) {
    const VkFormatFeatureFlags needed =
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
            | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    return (properties->optimalTilingFeatures & needed) == needed;
}

static VkResult create_bc_image(VkDevice device, VkFormat format,
                                VkImage *image) {
    VkImageCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {4, 4, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    return vkCreateImage(device, &info, NULL, image);
}

static void check_format(ProbeEnv *env, const char *name, VkFormat format,
                         bool must_work) {
    VkFormatProperties properties = {0};
    vkGetPhysicalDeviceFormatProperties(env->physical_device, format,
                                        &properties);
    VkImageFormatProperties image_properties = {0};
    VkResult image_status = vkGetPhysicalDeviceImageFormatProperties(
            env->physical_device, format, VK_IMAGE_TYPE_2D,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            0, &image_properties);
    bool advertised = format_usable(&properties);
    VkImage image = VK_NULL_HANDLE;
    VkResult create_status = VK_ERROR_FORMAT_NOT_SUPPORTED;
    if (env->device != VK_NULL_HANDLE)
        create_status = create_bc_image(env->device, format, &image);
    if (image != VK_NULL_HANDLE)
        vkDestroyImage(env->device, image, NULL);
    bool create_ok = create_status == VK_SUCCESS
            && image_status == VK_SUCCESS;
    snprintf(env->details, sizeof(env->details),
             "optimal=0x%x imageProps=%d create=%d advertised=%u",
             properties.optimalTilingFeatures, image_status, create_status,
             advertised);
    bool ok;
    if (must_work)
        ok = advertised && create_ok;
    else
        ok = advertised == create_ok;
    result(env, name, ok);
}

int main(void) {
    ProbeEnv env;
    probe_env_init(&env);
    if (!probe_create_instance(&env) || !probe_pick_physical(&env)) {
        snprintf(env.details, sizeof(env.details), "instance/physical failed");
        result(&env, "vulkan-bcn-feature", false);
        result(&env, "vulkan-bcn-bc1", false);
        result(&env, "vulkan-bcn-bc7", false);
        result(&env, "vulkan-bcn-timeline", false);
        printf("BXSUMMARY vulkan-bcn passed=%u failed=%u\n",
               env.passed, env.failed);
        return 1;
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(env.physical_device, &properties);
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(env.physical_device, &features);
    snprintf(env.details, sizeof(env.details),
             "name=%s bc=%u", properties.deviceName,
             features.textureCompressionBC);
    result(&env, "vulkan-bcn-feature", features.textureCompressionBC == VK_TRUE);

    bool timeline_ok = probe_create_device(&env);
    uint64_t initial = 0;
    uint64_t after = 0;
    uint64_t submitted = 0;
    VkResult get = VK_ERROR_UNKNOWN;
    VkResult get2 = VK_ERROR_UNKNOWN;
    VkResult get3 = VK_ERROR_UNKNOWN;
    VkResult sig = VK_ERROR_UNKNOWN;
    VkResult qs = VK_ERROR_UNKNOWN;
    if (timeline_ok) {
        VkSemaphoreTypeCreateInfo type = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
            .initialValue = 7,
        };
        VkSemaphoreCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &type,
        };
        VkSemaphore semaphore = VK_NULL_HANDLE;
        timeline_ok = vkCreateSemaphore(env.device, &info, NULL, &semaphore)
                == VK_SUCCESS;
        if (timeline_ok) {
            get = vkGetSemaphoreCounterValue(env.device, semaphore, &initial);
            VkSemaphoreSignalInfo signal = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
                .semaphore = semaphore,
                .value = 42,
            };
            sig = vkSignalSemaphore(env.device, &signal);
            get2 = vkGetSemaphoreCounterValue(env.device, semaphore, &after);
            uint64_t submit_value = 100;
            VkTimelineSemaphoreSubmitInfo timeline_submit = {
                .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
                .signalSemaphoreValueCount = 1,
                .pSignalSemaphoreValues = &submit_value,
            };
            VkSubmitInfo submit = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .pNext = &timeline_submit,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &semaphore,
            };
            qs = vkQueueSubmit(env.queue, 1, &submit, VK_NULL_HANDLE);
            if (qs == VK_SUCCESS)
                vkQueueWaitIdle(env.queue);
            get3 = vkGetSemaphoreCounterValue(env.device, semaphore, &submitted);
            vkDestroySemaphore(env.device, semaphore, NULL);
            timeline_ok = get == VK_SUCCESS && initial == 7
                    && sig == VK_SUCCESS && get2 == VK_SUCCESS && after == 42
                    && qs == VK_SUCCESS && get3 == VK_SUCCESS
                    && submitted == 100;
        }
    }
    snprintf(env.details, sizeof(env.details),
             "name=%s get=%d value=%llu signal=%d after=%llu "
             "submit=%d submitted=%llu",
             properties.deviceName, get, (unsigned long long)initial, sig,
             (unsigned long long)after, qs, (unsigned long long)submitted);
    result(&env, "vulkan-bcn-timeline", timeline_ok);
    if (!timeline_ok && !probe_create_device_basic(&env)) {
        snprintf(env.details, sizeof(env.details),
                 "name=%s createDevice failed", properties.deviceName);
        result(&env, "vulkan-bcn-bc1", false);
        result(&env, "vulkan-bcn-bc7", false);
        probe_env_destroy(&env);
        printf("BXSUMMARY vulkan-bcn passed=%u failed=%u\n",
               env.passed, env.failed);
        return 1;
    }
    check_format(&env, "vulkan-bcn-bc1", VK_FORMAT_BC1_RGBA_UNORM_BLOCK, true);
    check_format(&env, "vulkan-bcn-bc7", VK_FORMAT_BC7_UNORM_BLOCK, false);

    probe_env_destroy(&env);
    printf("BXSUMMARY vulkan-bcn passed=%u failed=%u\n",
           env.passed, env.failed);
    return env.failed ? 1 : 0;
}
