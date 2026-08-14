#include <dlfcn.h>

#include "common.h"

static void check_loader(ProbeEnv *env) {
    VkResult status = vkEnumerateInstanceVersion(&env->loader_version);
    void *icd = dlopen("libvulkan_vortek.so", RTLD_NOW | RTLD_LOCAL);
    const char *icd_how = "soname";
    if (icd == NULL) {
        icd = dlopen("lib/libvulkan_vortek.so", RTLD_NOW | RTLD_LOCAL);
        icd_how = "app-relative";
    }
    bool icd_ok = icd != NULL;
    if (icd != NULL) dlclose(icd);
    uint32_t extension_count = 0;
    VkResult extension_status = vkEnumerateInstanceExtensionProperties(
            NULL, &extension_count, NULL);
    VkExtensionProperties *extensions = calloc(
            extension_count, sizeof(*extensions));
    uint32_t returned = extension_count;
    if (extension_status == VK_SUCCESS)
        extension_status = vkEnumerateInstanceExtensionProperties(
                NULL, &returned, extensions);
    bool has_surface = false, has_xlib = false, has_xcb = false;
    for (uint32_t i = 0; i < returned; ++i) {
        has_surface |= strcmp(extensions[i].extensionName,
                              VK_KHR_SURFACE_EXTENSION_NAME) == 0;
        has_xlib |= strcmp(extensions[i].extensionName,
                           VK_KHR_XLIB_SURFACE_EXTENSION_NAME) == 0;
        has_xcb |= strcmp(extensions[i].extensionName,
                          VK_KHR_XCB_SURFACE_EXTENSION_NAME) == 0;
    }
    free(extensions);
    snprintf(env->details, sizeof(env->details),
             "status=%d version=%u.%u.%u icd=%s extensions=%u surface=%u xlib=%u xcb=%u",
             status, VK_VERSION_MAJOR(env->loader_version),
             VK_VERSION_MINOR(env->loader_version),
             VK_VERSION_PATCH(env->loader_version),
             icd_ok ? icd_how : "missing", returned,
             has_surface, has_xlib, has_xcb);
    result(env, "vulkan-wsi-loader",
           status == VK_SUCCESS && icd_ok
                   && extension_status == VK_SUCCESS && extension_count > 0
                   && has_surface && has_xlib && has_xcb);
}

static void check_window(ProbeEnv *env) {
    VisualID window_visual = 0, root_visual = 0;
    unsigned long red_mask = 0, green_mask = 0, blue_mask = 0;
    int depth = 0, visual_class = 0;
    if (env->display && env->window) {
        XWindowAttributes attributes;
        if (XGetWindowAttributes(env->display, env->window, &attributes)
                && attributes.visual) {
            window_visual = XVisualIDFromVisual(attributes.visual);
            red_mask = attributes.visual->red_mask;
            green_mask = attributes.visual->green_mask;
            blue_mask = attributes.visual->blue_mask;
            depth = attributes.depth;
            visual_class = attributes.visual->class;
        }
        root_visual = XVisualIDFromVisual(
                DefaultVisual(env->display, DefaultScreen(env->display)));
    }
    snprintf(env->details, sizeof(env->details),
             "display=%s window=0x%lx visual=0x%lx root=0x%lx class=%d depth=%d "
             "red=0x%lx green=0x%lx blue=0x%lx",
             env->display ? "open" : "closed", (unsigned long)env->window,
             (unsigned long)window_visual, (unsigned long)root_visual,
             visual_class, depth, red_mask, green_mask, blue_mask);
    result(env, "vulkan-wsi-window",
           env->display && env->window != 0
                   && window_visual != 0 && window_visual == root_visual
                   && visual_class == TrueColor && depth >= 24
                   && red_mask == 0xff0000UL
                   && green_mask == 0x00ff00UL
                   && blue_mask == 0x0000ffUL);
}

static void check_device(ProbeEnv *env) {
    VkPhysicalDeviceProperties properties;
    memset(&properties, 0, sizeof(properties));
    if (env->physical_device != VK_NULL_HANDLE)
        vkGetPhysicalDeviceProperties(env->physical_device, &properties);
    uint32_t extension_count = 0;
    VkResult extension_status = vkEnumerateDeviceExtensionProperties(
            env->physical_device, NULL, &extension_count, NULL);
    VkExtensionProperties *extensions = calloc(
            extension_count, sizeof(*extensions));
    uint32_t returned = extension_count;
    if (extension_status == VK_SUCCESS && returned > 0)
        extension_status = vkEnumerateDeviceExtensionProperties(
                env->physical_device, NULL, &returned, extensions);
    bool has_swapchain = false, has_fsr = false;
    for (uint32_t i = 0; i < returned; ++i) {
        has_swapchain |= strcmp(extensions[i].extensionName,
                                VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
        has_fsr |= strcmp(extensions[i].extensionName,
                          VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME) == 0;
    }
    free(extensions);
    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
            env->physical_device, &queue_count, NULL);
    uint64_t heap_bytes = 0;
    for (uint32_t i = 0; i < env->memory.memoryHeapCount; ++i)
        heap_bytes += env->memory.memoryHeaps[i].size;
    snprintf(env->details, sizeof(env->details),
             "name=%s api=%u.%u.%u vendor=0x%04x swapchain=%u fsr=%u "
             "families=%u graphics=%u types=%u heaps=%u",
             properties.deviceName,
             VK_VERSION_MAJOR(properties.apiVersion),
             VK_VERSION_MINOR(properties.apiVersion),
             VK_VERSION_PATCH(properties.apiVersion),
             properties.vendorID, has_swapchain, has_fsr,
             queue_count, env->graphics_family, env->memory.memoryTypeCount,
             env->memory.memoryHeapCount);
    result(env, "vulkan-wsi-device",
           env->physical_device != VK_NULL_HANDLE && properties.deviceName[0]
                   && extension_status == VK_SUCCESS && has_swapchain
                   && !has_fsr
                   && queue_count > 0 && env->graphics_family != UINT32_MAX
                   && env->memory.memoryTypeCount > 0
                   && env->memory.memoryHeapCount > 0 && heap_bytes > 0);
}

static void check_wsi(ProbeEnv *env) {
    VkBool32 surface_supported = VK_FALSE;
    VkResult support_status = env->surface != VK_NULL_HANDLE
            ? vkGetPhysicalDeviceSurfaceSupportKHR(
                    env->physical_device, 0, env->surface, &surface_supported)
            : VK_ERROR_SURFACE_LOST_KHR;
    VkBool32 xlib_supported = env->display
            ? vkGetPhysicalDeviceXlibPresentationSupportKHR(
                    env->physical_device, 0, env->display,
                    XVisualIDFromVisual(DefaultVisual(
                            env->display, DefaultScreen(env->display))))
            : VK_FALSE;
    xcb_connection_t *connection = env->display
            ? XGetXCBConnection(env->display) : NULL;
    VkBool32 xcb_supported = connection
            ? vkGetPhysicalDeviceXcbPresentationSupportKHR(
                    env->physical_device, env->graphics_family, connection,
                    (xcb_visualid_t)XVisualIDFromVisual(DefaultVisual(
                            env->display, DefaultScreen(env->display))))
            : VK_FALSE;
    snprintf(env->details, sizeof(env->details),
             "xlib=%d xcb=%d window=0x%lx surface=%u xlibPresent=%u xcbPresent=%u",
             env->surface != VK_NULL_HANDLE ? 0 : -1,
             env->xcb_surface != VK_NULL_HANDLE ? 0 : -1,
             (unsigned long)env->window, surface_supported,
             xlib_supported, xcb_supported);
    result(env, "vulkan-wsi-present-support",
           env->surface != VK_NULL_HANDLE
                   && env->xcb_surface != VK_NULL_HANDLE
                   && support_status == VK_SUCCESS && surface_supported
                   && xlib_supported && xcb_supported == VK_TRUE);
}

static void check_surface(ProbeEnv *env) {
    uint32_t mode_count = 0;
    VkResult status = env->surface != VK_NULL_HANDLE
            ? vkGetPhysicalDeviceSurfacePresentModesKHR(
                    env->physical_device, env->surface, &mode_count, NULL)
            : VK_ERROR_SURFACE_LOST_KHR;
    VkPresentModeKHR modes[8];
    uint32_t returned_modes = mode_count > 8 ? 8 : mode_count;
    if (status == VK_SUCCESS && returned_modes > 0)
        vkGetPhysicalDeviceSurfacePresentModesKHR(
                env->physical_device, env->surface, &returned_modes, modes);
    bool has_bgra = false, has_fifo = false;
    for (uint32_t i = 0; i < env->format_count; ++i)
        has_bgra |= env->formats[i].format == VK_FORMAT_B8G8R8A8_UNORM;
    for (uint32_t i = 0; i < returned_modes; ++i)
        has_fifo |= modes[i] == VK_PRESENT_MODE_FIFO_KHR;
    snprintf(env->details, sizeof(env->details),
             "extent=%ux%u images=%u..%u usage=0x%x formats=%u bgra=%u fifo=%u",
             env->capabilities.currentExtent.width,
             env->capabilities.currentExtent.height,
             env->capabilities.minImageCount,
             env->capabilities.maxImageCount,
             env->capabilities.supportedUsageFlags, env->format_count,
             has_bgra, has_fifo);
    result(env, "vulkan-wsi-surface",
           env->capabilities.currentExtent.width == 640
                   && env->capabilities.currentExtent.height == 360
                   && env->capabilities.minImageCount >= 2
                   && (env->capabilities.supportedUsageFlags
                       & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                   && env->format_count > 0 && has_bgra
                   && mode_count > 0 && has_fifo);
}

int main(void) {
    ProbeEnv env;
    probe_env_init(&env);
    check_loader(&env);
    if (!probe_open_window(&env)) {
        snprintf(env.details, sizeof(env.details), "display=closed");
        result(&env, "vulkan-wsi-window", false);
    } else {
        check_window(&env);
    }
    if (!probe_create_instance(&env) || !probe_pick_physical(&env)) {
        snprintf(env.details, sizeof(env.details), "instance/physical failed");
        result(&env, "vulkan-wsi-device", false);
    } else {
        check_device(&env);
        probe_create_surfaces(&env);
        check_wsi(&env);
        probe_query_surface(&env);
        check_surface(&env);
    }
    probe_env_destroy(&env);
    printf("BXSUMMARY vulkan-wsi passed=%u failed=%u\n",
           env.passed, env.failed);
    return env.failed ? 1 : 0;
}
