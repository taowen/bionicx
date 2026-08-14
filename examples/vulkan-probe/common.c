#include "common.h"

void result(ProbeEnv *env, const char *name, bool ok) {
    printf("BXTEST %s %s %s\n", ok ? "PASS" : "FAIL", name, env->details);
    fflush(stdout);
    ok ? env->passed++ : env->failed++;
}

uint32_t *read_spirv(const char *path, size_t *size) {
    FILE *stream = fopen(path, "rb");
    if (!stream) return NULL;
    if (fseek(stream, 0, SEEK_END) != 0) {
        fclose(stream);
        return NULL;
    }
    long length = ftell(stream);
    if (length <= 0 || length % 4 != 0 || fseek(stream, 0, SEEK_SET) != 0) {
        fclose(stream);
        return NULL;
    }
    uint32_t *words = malloc((size_t)length);
    if (!words || fread(words, 1, (size_t)length, stream) != (size_t)length) {
        free(words);
        fclose(stream);
        return NULL;
    }
    fclose(stream);
    *size = (size_t)length;
    return words;
}

VkResult upload_buffer(VkDevice device,
                       const VkPhysicalDeviceMemoryProperties *memory,
                       VkBufferUsageFlags usage, const void *data, size_t size,
                       VkBuffer *buffer, VkDeviceMemory *device_memory) {
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkResult status = vkCreateBuffer(device, &buffer_info, NULL, buffer);
    if (status != VK_SUCCESS) return status;

    VkMemoryRequirements requirements = {0};
    vkGetBufferMemoryRequirements(device, *buffer, &requirements);
    uint32_t memory_type = UINT32_MAX;
    VkMemoryPropertyFlags required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < memory->memoryTypeCount; i++) {
        if ((requirements.memoryTypeBits & (1u << i))
                && (memory->memoryTypes[i].propertyFlags & required)
                        == required) {
            memory_type = i;
            break;
        }
    }
    if (memory_type == UINT32_MAX) return VK_ERROR_FEATURE_NOT_PRESENT;

    VkMemoryAllocateInfo allocation = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = requirements.size,
        .memoryTypeIndex = memory_type,
    };
    status = vkAllocateMemory(device, &allocation, NULL, device_memory);
    if (status != VK_SUCCESS) return status;
    status = vkBindBufferMemory(device, *buffer, *device_memory, 0);
    if (status != VK_SUCCESS) return status;

    void *mapping = NULL;
    status = vkMapMemory(device, *device_memory, 0, size, 0, &mapping);
    if (status == VK_SUCCESS) {
        memcpy(mapping, data, size);
        vkUnmapMemory(device, *device_memory);
    }
    return status;
}

void probe_env_init(ProbeEnv *env) {
    memset(env, 0, sizeof(*env));
    env->graphics_family = UINT32_MAX;
    env->loader_version = VK_API_VERSION_1_0;
}

int probe_open_window(ProbeEnv *env) {
    env->display = XOpenDisplay(NULL);
    if (!env->display) return 0;
    env->window = XCreateSimpleWindow(env->display,
                                      DefaultRootWindow(env->display),
                                      80, 240, 640, 360, 0, 0, 0);
    if (!env->window) return 0;
    XStoreName(env->display, env->window, "BionicX Vulkan probe");
    XMapWindow(env->display, env->window);
    XSync(env->display, False);
    return 1;
}

int probe_create_instance(ProbeEnv *env) {
    const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
    };
    VkApplicationInfo application = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "bionicx-vulkan-probe",
        .applicationVersion = 1,
        .pEngineName = "bionicx",
        .engineVersion = 1,
        .apiVersion = env->loader_version < VK_API_VERSION_1_3
                ? env->loader_version : VK_API_VERSION_1_3,
    };
    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &application,
        .enabledExtensionCount = 3,
        .ppEnabledExtensionNames = extensions,
    };
    return vkCreateInstance(&create_info, NULL, &env->instance) == VK_SUCCESS
            && env->instance != VK_NULL_HANDLE;
}

int probe_pick_physical(ProbeEnv *env) {
    uint32_t physical_count = 0;
    if (vkEnumeratePhysicalDevices(env->instance, &physical_count, NULL)
                != VK_SUCCESS
            || physical_count == 0)
        return 0;
    uint32_t one = 1;
    VkResult status = vkEnumeratePhysicalDevices(
            env->instance, &one, &env->physical_device);
    if ((status != VK_SUCCESS && status != VK_INCOMPLETE)
            || env->physical_device == VK_NULL_HANDLE)
        return 0;
    vkGetPhysicalDeviceMemoryProperties(env->physical_device, &env->memory);
    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
            env->physical_device, &queue_count, NULL);
    VkQueueFamilyProperties *families = calloc(
            queue_count, sizeof(*families));
    vkGetPhysicalDeviceQueueFamilyProperties(
            env->physical_device, &queue_count, families);
    env->graphics_family = UINT32_MAX;
    for (uint32_t i = 0; i < queue_count; ++i) {
        if (families[i].queueCount > 0
                && (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            env->graphics_family = i;
            break;
        }
    }
    free(families);
    return env->graphics_family != UINT32_MAX;
}

void probe_create_surfaces(ProbeEnv *env) {
    VkXlibSurfaceCreateInfoKHR xlib_info = {
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .dpy = env->display,
        .window = env->window,
    };
    if (env->display && env->window)
        vkCreateXlibSurfaceKHR(env->instance, &xlib_info, NULL, &env->surface);
    xcb_connection_t *connection = env->display
            ? XGetXCBConnection(env->display) : NULL;
    VkXcbSurfaceCreateInfoKHR xcb_info = {
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .connection = connection,
        .window = (xcb_window_t)env->window,
    };
    if (connection && env->window)
        vkCreateXcbSurfaceKHR(env->instance, &xcb_info, NULL,
                              &env->xcb_surface);
}

void probe_query_surface(ProbeEnv *env) {
    memset(&env->capabilities, 0, sizeof(env->capabilities));
    if (env->surface == VK_NULL_HANDLE) return;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            env->physical_device, env->surface, &env->capabilities);
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
            env->physical_device, env->surface, &format_count, NULL);
    env->format_count = format_count > 8 ? 8 : format_count;
    if (env->format_count > 0)
        vkGetPhysicalDeviceSurfaceFormatsKHR(
                env->physical_device, env->surface, &env->format_count,
                env->formats);
    env->selected_format = (VkSurfaceFormatKHR){
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };
    env->preferred_format = 0;
    for (uint32_t i = 0; i < env->format_count; ++i) {
        if (env->formats[i].format == env->selected_format.format
                && env->formats[i].colorSpace
                        == env->selected_format.colorSpace) {
            env->selected_format = env->formats[i];
            env->preferred_format = 1;
            break;
        }
    }
}

int probe_create_device(ProbeEnv *env) {
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = env->graphics_family,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };
    const char *extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
    };
    VkPhysicalDeviceTimelineSemaphoreFeatures timeline = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
        .timelineSemaphore = VK_TRUE,
    };
    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &timeline,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = 2,
        .ppEnabledExtensionNames = extensions,
    };
    if (env->graphics_family == UINT32_MAX) return 0;
    if (vkCreateDevice(env->physical_device, &create_info, NULL,
                       &env->device) != VK_SUCCESS)
        return 0;
    vkGetDeviceQueue(env->device, env->graphics_family, 0, &env->queue);
    return env->queue != VK_NULL_HANDLE;
}

int probe_create_swapchain(ProbeEnv *env) {
    VkCompositeAlphaFlagBitsKHR composite =
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (!(env->capabilities.supportedCompositeAlpha & composite)) {
        composite = (VkCompositeAlphaFlagBitsKHR)
                (env->capabilities.supportedCompositeAlpha
                 & (0u - env->capabilities.supportedCompositeAlpha));
    }
    env->swapchain_info = (VkSwapchainCreateInfoKHR){
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = env->surface,
        .minImageCount = env->capabilities.minImageCount,
        .imageFormat = env->selected_format.format,
        .imageColorSpace = env->selected_format.colorSpace,
        .imageExtent = env->capabilities.currentExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = env->capabilities.currentTransform,
        .compositeAlpha = composite,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
    };
    if (env->device == VK_NULL_HANDLE || env->surface == VK_NULL_HANDLE)
        return 0;
    if (vkCreateSwapchainKHR(env->device, &env->swapchain_info, NULL,
                             &env->swapchain) != VK_SUCCESS)
        return 0;
    uint32_t count = 0;
    if (vkGetSwapchainImagesKHR(env->device, env->swapchain, &count, NULL)
            != VK_SUCCESS)
        return 0;
    env->swapchain_images = calloc(count, sizeof(*env->swapchain_images));
    env->image_count = count;
    return vkGetSwapchainImagesKHR(env->device, env->swapchain,
                                   &env->image_count,
                                   env->swapchain_images) == VK_SUCCESS
            && env->image_count >= 2;
}

void probe_env_destroy(ProbeEnv *env) {
    if (env->device != VK_NULL_HANDLE) vkDeviceWaitIdle(env->device);
    free(env->swapchain_images);
    env->swapchain_images = NULL;
    if (env->swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(env->device, env->swapchain, NULL);
    if (env->device != VK_NULL_HANDLE) vkDestroyDevice(env->device, NULL);
    if (env->xcb_surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(env->instance, env->xcb_surface, NULL);
    if (env->surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(env->instance, env->surface, NULL);
    if (env->instance != VK_NULL_HANDLE)
        vkDestroyInstance(env->instance, NULL);
    if (env->display && env->window)
        XDestroyWindow(env->display, env->window);
    if (env->display) XCloseDisplay(env->display);
}
