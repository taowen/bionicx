#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <vulkan/vulkan.h>

static unsigned passed;
static unsigned failed;

static void result(const char *name, bool ok, const char *details) {
    printf("BXTEST %s %s %s\n", ok ? "PASS" : "FAIL", name, details);
    ok ? passed++ : failed++;
}

int main(void) {
    uint32_t loader_version = VK_API_VERSION_1_0;
    VkResult status = vkEnumerateInstanceVersion(&loader_version);
    char details[512];
    snprintf(details, sizeof(details), "status=%d version=%u.%u.%u",
             status, VK_VERSION_MAJOR(loader_version),
             VK_VERSION_MINOR(loader_version), VK_VERSION_PATCH(loader_version));
    result("host-vulkan-loader", status == VK_SUCCESS, details);

    uint32_t extension_count = 0;
    status = vkEnumerateInstanceExtensionProperties(
            NULL, &extension_count, NULL);
    snprintf(details, sizeof(details), "status=%d extensions=%u",
             status, extension_count);
    result("host-vulkan-instance-extensions",
           status == VK_SUCCESS && extension_count > 0, details);

    VkExtensionProperties *extensions = calloc(
            extension_count, sizeof(*extensions));
    uint32_t returned_extension_count = extension_count;
    status = vkEnumerateInstanceExtensionProperties(
            NULL, &returned_extension_count, extensions);
    bool has_surface = false;
    bool has_xlib_surface = false;
    for (uint32_t i = 0; i < returned_extension_count; ++i) {
        has_surface |= strcmp(extensions[i].extensionName,
                              VK_KHR_SURFACE_EXTENSION_NAME) == 0;
        has_xlib_surface |= strcmp(extensions[i].extensionName,
                                   VK_KHR_XLIB_SURFACE_EXTENSION_NAME) == 0;
    }
    snprintf(details, sizeof(details),
             "status=%d returned=%u surface=%u xlib=%u",
             status, returned_extension_count, has_surface, has_xlib_surface);
    result("host-vulkan-xlib-extensions",
           status == VK_SUCCESS && has_surface && has_xlib_surface, details);
    free(extensions);

    Display *display = XOpenDisplay(NULL);
    Window window = 0;
    if (display) {
        window = XCreateSimpleWindow(display, DefaultRootWindow(display),
                                     0, 0, 640, 360, 0, 0, 0);
        XStoreName(display, window, "BionicX Vulkan probe");
        XMapWindow(display, window);
        XSync(display, False);
    }
    snprintf(details, sizeof(details), "display=%s window=0x%lx size=640x360",
             display ? "open" : "closed", (unsigned long)window);
    result("host-vulkan-xlib-window", display && window != 0, details);

    const char *instance_extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
    };
    VkApplicationInfo application = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "bionicx-vulkan-probe",
        .applicationVersion = 1,
        .pEngineName = "bionicx",
        .engineVersion = 1,
        .apiVersion = loader_version < VK_API_VERSION_1_3
                ? loader_version : VK_API_VERSION_1_3,
    };
    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &application,
        .enabledExtensionCount = 2,
        .ppEnabledExtensionNames = instance_extensions,
    };
    VkInstance instance = VK_NULL_HANDLE;
    status = vkCreateInstance(&create_info, NULL, &instance);
    snprintf(details, sizeof(details), "status=%d handle=%s",
             status, instance != VK_NULL_HANDLE ? "valid" : "null");
    result("host-vulkan-create-instance",
           status == VK_SUCCESS && instance != VK_NULL_HANDLE, details);
    if (status != VK_SUCCESS || instance == VK_NULL_HANDLE) goto done;

    uint32_t physical_count = 0;
    status = vkEnumeratePhysicalDevices(instance, &physical_count, NULL);
    snprintf(details, sizeof(details), "status=%d devices=%u",
             status, physical_count);
    result("host-vulkan-physical-devices",
           status == VK_SUCCESS && physical_count > 0, details);
    if (status != VK_SUCCESS || physical_count == 0) goto destroy_instance;

    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    uint32_t one_device = 1;
    status = vkEnumeratePhysicalDevices(
            instance, &one_device, &physical_device);
    VkPhysicalDeviceProperties properties;
    memset(&properties, 0, sizeof(properties));
    if ((status == VK_SUCCESS || status == VK_INCOMPLETE)
            && physical_device != VK_NULL_HANDLE) {
        vkGetPhysicalDeviceProperties(physical_device, &properties);
    }
    snprintf(details, sizeof(details),
             "status=%d name=%s api=%u.%u.%u vendor=0x%04x device=0x%04x",
             status, properties.deviceName,
             VK_VERSION_MAJOR(properties.apiVersion),
             VK_VERSION_MINOR(properties.apiVersion),
             VK_VERSION_PATCH(properties.apiVersion),
             properties.vendorID, properties.deviceID);
    result("host-vulkan-device-properties",
           physical_device != VK_NULL_HANDLE && properties.deviceName[0],
           details);

    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
            physical_device, &queue_count, NULL);
    snprintf(details, sizeof(details), "families=%u", queue_count);
    result("host-vulkan-queue-families", queue_count > 0, details);

    VkPhysicalDeviceMemoryProperties memory;
    memset(&memory, 0, sizeof(memory));
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory);
    uint64_t heap_bytes = 0;
    for (uint32_t i = 0; i < memory.memoryHeapCount; ++i)
        heap_bytes += memory.memoryHeaps[i].size;
    snprintf(details, sizeof(details), "types=%u heaps=%u bytes=%llu",
             memory.memoryTypeCount, memory.memoryHeapCount,
             (unsigned long long)heap_bytes);
    result("host-vulkan-memory",
           memory.memoryTypeCount > 0 && memory.memoryHeapCount > 0
                   && heap_bytes > 0,
           details);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkXlibSurfaceCreateInfoKHR surface_create_info = {
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .dpy = display,
        .window = window,
    };
    status = display && window
            ? vkCreateXlibSurfaceKHR(instance, &surface_create_info,
                                     NULL, &surface)
            : VK_ERROR_INITIALIZATION_FAILED;
    snprintf(details, sizeof(details), "status=%d handle=%s window=0x%lx",
             status, surface != VK_NULL_HANDLE ? "valid" : "null",
             (unsigned long)window);
    result("host-vulkan-xlib-surface",
           status == VK_SUCCESS && surface != VK_NULL_HANDLE, details);

    VkBool32 surface_supported = VK_FALSE;
    VkResult support_status = surface != VK_NULL_HANDLE
            ? vkGetPhysicalDeviceSurfaceSupportKHR(
                    physical_device, 0, surface, &surface_supported)
            : VK_ERROR_SURFACE_LOST_KHR;
    VkBool32 xlib_supported = display
            ? vkGetPhysicalDeviceXlibPresentationSupportKHR(
                    physical_device, 0, display,
                    XVisualIDFromVisual(DefaultVisual(display,
                                                      DefaultScreen(display))))
            : VK_FALSE;
    snprintf(details, sizeof(details), "status=%d surface=%u xlib=%u",
             support_status, surface_supported, xlib_supported);
    result("host-vulkan-presentation-support",
           support_status == VK_SUCCESS && surface_supported
                   && xlib_supported,
           details);

    VkSurfaceCapabilitiesKHR capabilities;
    memset(&capabilities, 0, sizeof(capabilities));
    status = surface != VK_NULL_HANDLE
            ? vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                    physical_device, surface, &capabilities)
            : VK_ERROR_SURFACE_LOST_KHR;
    snprintf(details, sizeof(details),
             "status=%d extent=%ux%u images=%u..%u usage=0x%x",
             status, capabilities.currentExtent.width,
             capabilities.currentExtent.height, capabilities.minImageCount,
             capabilities.maxImageCount, capabilities.supportedUsageFlags);
    result("host-vulkan-surface-capabilities",
           status == VK_SUCCESS && capabilities.currentExtent.width == 640
                   && capabilities.currentExtent.height == 360
                   && capabilities.minImageCount > 0
                   && (capabilities.supportedUsageFlags
                       & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
           details);

    uint32_t format_count = 0;
    status = surface != VK_NULL_HANDLE
            ? vkGetPhysicalDeviceSurfaceFormatsKHR(
                    physical_device, surface, &format_count, NULL)
            : VK_ERROR_SURFACE_LOST_KHR;
    VkSurfaceFormatKHR formats[8];
    uint32_t returned_format_count = format_count > 8 ? 8 : format_count;
    if (status == VK_SUCCESS && returned_format_count > 0) {
        status = vkGetPhysicalDeviceSurfaceFormatsKHR(
                physical_device, surface, &returned_format_count, formats);
    }
    snprintf(details, sizeof(details), "status=%d advertised=%u returned=%u",
             status, format_count, returned_format_count);
    result("host-vulkan-surface-formats",
           status == VK_SUCCESS && format_count > 0
                   && returned_format_count > 0,
           details);

    uint32_t mode_count = 0;
    status = surface != VK_NULL_HANDLE
            ? vkGetPhysicalDeviceSurfacePresentModesKHR(
                    physical_device, surface, &mode_count, NULL)
            : VK_ERROR_SURFACE_LOST_KHR;
    VkPresentModeKHR modes[8];
    uint32_t returned_mode_count = mode_count > 8 ? 8 : mode_count;
    if (status == VK_SUCCESS && returned_mode_count > 0) {
        status = vkGetPhysicalDeviceSurfacePresentModesKHR(
                physical_device, surface, &returned_mode_count, modes);
    }
    bool has_fifo = false;
    for (uint32_t i = 0; i < returned_mode_count; ++i)
        has_fifo |= modes[i] == VK_PRESENT_MODE_FIFO_KHR;
    snprintf(details, sizeof(details),
             "status=%d advertised=%u returned=%u fifo=%u",
             status, mode_count, returned_mode_count, has_fifo);
    result("host-vulkan-present-modes",
           status == VK_SUCCESS && mode_count > 0 && has_fifo, details);

    if (surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance, surface, NULL);

destroy_instance:
    vkDestroyInstance(instance, NULL);
done:
    if (display && window) XDestroyWindow(display, window);
    if (display) XCloseDisplay(display);
    printf("BXSUMMARY host-vulkan passed=%u failed=%u\n", passed, failed);
    return failed ? 1 : 0;
}
