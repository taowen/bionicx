#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <vulkan/vulkan.h>

static unsigned passed;
static unsigned failed;

static void result(const char *name, bool ok, const char *details) {
    printf("BXTEST %s %s %s\n", ok ? "PASS" : "FAIL", name, details);
    fflush(stdout);
    ok ? passed++ : failed++;
}

static uint32_t *read_spirv(const char *path, size_t *size) {
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
    bool has_xcb_surface = false;
    for (uint32_t i = 0; i < returned_extension_count; ++i) {
        has_surface |= strcmp(extensions[i].extensionName,
                              VK_KHR_SURFACE_EXTENSION_NAME) == 0;
        has_xlib_surface |= strcmp(extensions[i].extensionName,
                                   VK_KHR_XLIB_SURFACE_EXTENSION_NAME) == 0;
        has_xcb_surface |= strcmp(extensions[i].extensionName,
                                  VK_KHR_XCB_SURFACE_EXTENSION_NAME) == 0;
    }
    snprintf(details, sizeof(details),
             "status=%d returned=%u surface=%u xlib=%u",
             status, returned_extension_count, has_surface, has_xlib_surface);
    result("host-vulkan-xlib-extensions",
           status == VK_SUCCESS && has_surface && has_xlib_surface, details);
    snprintf(details, sizeof(details), "status=%d xcb=%u",
             status, has_xcb_surface);
    result("host-vulkan-xcb-extension",
           status == VK_SUCCESS && has_xcb_surface, details);
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
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
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
        .enabledExtensionCount = 3,
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

    uint32_t device_extension_count = 0;
    status = vkEnumerateDeviceExtensionProperties(
            physical_device, NULL, &device_extension_count, NULL);
    VkExtensionProperties *device_extensions_available = calloc(
            device_extension_count, sizeof(*device_extensions_available));
    uint32_t returned_device_extension_count = device_extension_count;
    if (status == VK_SUCCESS && returned_device_extension_count > 0) {
        status = vkEnumerateDeviceExtensionProperties(
                physical_device, NULL, &returned_device_extension_count,
                device_extensions_available);
    }
    bool has_swapchain = false;
    bool has_fragment_shading_rate = false;
    for (uint32_t i = 0; i < returned_device_extension_count; ++i) {
        has_swapchain |= strcmp(device_extensions_available[i].extensionName,
                                VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
        has_fragment_shading_rate |= strcmp(
                device_extensions_available[i].extensionName,
                VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME) == 0;
    }
    snprintf(details, sizeof(details),
             "status=%d returned=%u swapchain=%u fragmentShadingRate=%u",
             status, returned_device_extension_count, has_swapchain,
             has_fragment_shading_rate);
    result("host-vulkan-device-extension-honesty",
           status == VK_SUCCESS && has_swapchain
                   && !has_fragment_shading_rate,
           details);
    free(device_extensions_available);

    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
            physical_device, &queue_count, NULL);
    VkQueueFamilyProperties *queue_families = calloc(
            queue_count, sizeof(*queue_families));
    vkGetPhysicalDeviceQueueFamilyProperties(
            physical_device, &queue_count, queue_families);
    uint32_t graphics_family = UINT32_MAX;
    for (uint32_t i = 0; i < queue_count; ++i) {
        if (queue_families[i].queueCount > 0
                && (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            graphics_family = i;
            break;
        }
    }
    snprintf(details, sizeof(details), "families=%u graphics=%u",
             queue_count, graphics_family);
    result("host-vulkan-queue-families",
           queue_count > 0 && graphics_family != UINT32_MAX, details);
    free(queue_families);

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

    VkSurfaceKHR xcb_surface = VK_NULL_HANDLE;
    xcb_connection_t *xcb_connection = display
            ? XGetXCBConnection(display) : NULL;
    VkXcbSurfaceCreateInfoKHR xcb_surface_create_info = {
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .connection = xcb_connection,
        .window = (xcb_window_t)window,
    };
    VkResult xcb_surface_status = xcb_connection && window
            ? vkCreateXcbSurfaceKHR(instance, &xcb_surface_create_info,
                                    NULL, &xcb_surface)
            : VK_ERROR_INITIALIZATION_FAILED;
    snprintf(details, sizeof(details), "status=%d handle=%s window=0x%lx",
             xcb_surface_status,
             xcb_surface != VK_NULL_HANDLE ? "valid" : "null",
             (unsigned long)window);
    result("host-vulkan-xcb-surface",
           xcb_surface_status == VK_SUCCESS
                   && xcb_surface != VK_NULL_HANDLE,
           details);

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
    VkBool32 xcb_supported = xcb_connection
            ? vkGetPhysicalDeviceXcbPresentationSupportKHR(
                    physical_device, graphics_family, xcb_connection,
                    (xcb_visualid_t)XVisualIDFromVisual(
                            DefaultVisual(display, DefaultScreen(display))))
            : VK_FALSE;
    snprintf(details, sizeof(details), "family=%u xcb=%u",
             graphics_family, xcb_supported);
    result("host-vulkan-xcb-presentation-support",
           xcb_supported == VK_TRUE, details);

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

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = graphics_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };
    const char *device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = device_extensions,
    };
    VkDevice device = VK_NULL_HANDLE;
    status = graphics_family != UINT32_MAX
            ? vkCreateDevice(physical_device, &device_create_info,
                             NULL, &device)
            : VK_ERROR_INITIALIZATION_FAILED;
    snprintf(details, sizeof(details), "status=%d handle=%s family=%u",
             status, device != VK_NULL_HANDLE ? "valid" : "null",
             graphics_family);
    result("host-vulkan-logical-device",
           status == VK_SUCCESS && device != VK_NULL_HANDLE, details);

    VkQueue queue = VK_NULL_HANDLE;
    if (device != VK_NULL_HANDLE)
        vkGetDeviceQueue(device, graphics_family, 0, &queue);
    snprintf(details, sizeof(details), "handle=%s family=%u index=0",
             queue != VK_NULL_HANDLE ? "valid" : "null", graphics_family);
    result("host-vulkan-device-queue", queue != VK_NULL_HANDLE, details);

    VkSurfaceFormatKHR selected_format = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };
    bool preferred_format = false;
    for (uint32_t i = 0; i < returned_format_count; ++i) {
        if (formats[i].format == selected_format.format
                && formats[i].colorSpace == selected_format.colorSpace) {
            selected_format = formats[i];
            preferred_format = true;
            break;
        }
    }
    if (!preferred_format && returned_format_count > 0)
        selected_format = formats[0];

    VkCompositeAlphaFlagBitsKHR composite_alpha =
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (!(capabilities.supportedCompositeAlpha & composite_alpha)) {
        composite_alpha = (VkCompositeAlphaFlagBitsKHR)
                (capabilities.supportedCompositeAlpha
                 & (0u - capabilities.supportedCompositeAlpha));
    }
    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = capabilities.minImageCount,
        .imageFormat = selected_format.format,
        .imageColorSpace = selected_format.colorSpace,
        .imageExtent = capabilities.currentExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = composite_alpha,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
    };
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    status = device != VK_NULL_HANDLE && surface != VK_NULL_HANDLE
            ? vkCreateSwapchainKHR(device, &swapchain_create_info,
                                   NULL, &swapchain)
            : VK_ERROR_INITIALIZATION_FAILED;
    snprintf(details, sizeof(details),
             "status=%d handle=%s format=%d extent=%ux%u images=%u",
             status, swapchain != VK_NULL_HANDLE ? "valid" : "null",
             selected_format.format, swapchain_create_info.imageExtent.width,
             swapchain_create_info.imageExtent.height,
             swapchain_create_info.minImageCount);
    result("host-vulkan-swapchain",
           status == VK_SUCCESS && swapchain != VK_NULL_HANDLE, details);

    uint32_t swapchain_image_count = 0;
    if (swapchain != VK_NULL_HANDLE)
        status = vkGetSwapchainImagesKHR(
                device, swapchain, &swapchain_image_count, NULL);
    else
        status = VK_ERROR_INITIALIZATION_FAILED;
    VkImage *swapchain_images = calloc(
            swapchain_image_count, sizeof(*swapchain_images));
    uint32_t returned_image_count = swapchain_image_count;
    if (status == VK_SUCCESS && returned_image_count > 0) {
        status = vkGetSwapchainImagesKHR(
                device, swapchain, &returned_image_count, swapchain_images);
    }
    snprintf(details, sizeof(details), "status=%d advertised=%u returned=%u",
             status, swapchain_image_count, returned_image_count);
    result("host-vulkan-swapchain-images",
           status == VK_SUCCESS && returned_image_count > 0, details);

    VkImageView image_view = VK_NULL_HANDLE;
    VkImageViewCreateInfo image_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = returned_image_count > 0
                ? swapchain_images[0] : VK_NULL_HANDLE,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = selected_format.format,
        .components = {
            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    VkResult image_view_status = returned_image_count > 0
            ? vkCreateImageView(device, &image_view_info, NULL, &image_view)
            : VK_ERROR_INITIALIZATION_FAILED;

    VkAttachmentDescription color_attachment = {
        .format = selected_format.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference color_reference = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_reference,
    };
    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkResult render_pass_status = image_view_status == VK_SUCCESS
            ? vkCreateRenderPass(device, &render_pass_info, NULL, &render_pass)
            : image_view_status;

    VkFramebufferCreateInfo framebuffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = render_pass,
        .attachmentCount = 1,
        .pAttachments = &image_view,
        .width = swapchain_create_info.imageExtent.width,
        .height = swapchain_create_info.imageExtent.height,
        .layers = 1,
    };
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkResult framebuffer_status = render_pass_status == VK_SUCCESS
            ? vkCreateFramebuffer(device, &framebuffer_info, NULL, &framebuffer)
            : render_pass_status;
    snprintf(details, sizeof(details), "view=%d renderPass=%d framebuffer=%d",
             image_view_status, render_pass_status, framebuffer_status);
    result("host-vulkan-render-target",
           image_view_status == VK_SUCCESS
                   && render_pass_status == VK_SUCCESS
                   && framebuffer_status == VK_SUCCESS,
           details);

    size_t vertex_code_size = 0;
    size_t fragment_code_size = 0;
    uint32_t *vertex_code = read_spirv(
            "share/vulkan-probe/triangle.vert.spv", &vertex_code_size);
    uint32_t *fragment_code = read_spirv(
            "share/vulkan-probe/triangle.frag.spv", &fragment_code_size);
    VkShaderModuleCreateInfo vertex_module_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vertex_code_size,
        .pCode = vertex_code,
    };
    VkShaderModuleCreateInfo fragment_module_info = vertex_module_info;
    fragment_module_info.codeSize = fragment_code_size;
    fragment_module_info.pCode = fragment_code;
    VkShaderModule vertex_module = VK_NULL_HANDLE;
    VkShaderModule fragment_module = VK_NULL_HANDLE;
    VkResult vertex_module_status = vertex_code
            ? vkCreateShaderModule(device, &vertex_module_info, NULL,
                                   &vertex_module)
            : VK_ERROR_INITIALIZATION_FAILED;
    VkResult fragment_module_status = fragment_code
            ? vkCreateShaderModule(device, &fragment_module_info, NULL,
                                   &fragment_module)
            : VK_ERROR_INITIALIZATION_FAILED;
    free(vertex_code);
    free(fragment_code);
    snprintf(details, sizeof(details), "vertex=%d fragment=%d",
             vertex_module_status, fragment_module_status);
    result("host-vulkan-shader-modules",
           vertex_module_status == VK_SUCCESS
                   && fragment_module_status == VK_SUCCESS,
           details);

    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkResult layout_status = vkCreatePipelineLayout(
            device, &layout_info, NULL, &pipeline_layout);
    VkPipelineShaderStageCreateInfo shader_stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertex_module,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragment_module,
            .pName = "main",
        },
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    VkViewport viewport = {
        .x = 0,
        .y = 0,
        .width = (float)swapchain_create_info.imageExtent.width,
        .height = (float)swapchain_create_info.imageExtent.height,
        .minDepth = 0,
        .maxDepth = 1,
    };
    VkRect2D scissor = {
        .extent = swapchain_create_info.imageExtent,
    };
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };
    VkPipelineRasterizationStateCreateInfo rasterization = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1,
    };
    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineColorBlendAttachmentState blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
                | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisample,
        .pColorBlendState = &blend,
        .layout = pipeline_layout,
        .renderPass = render_pass,
        .subpass = 0,
    };
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult pipeline_status = layout_status == VK_SUCCESS
            && vertex_module_status == VK_SUCCESS
            && fragment_module_status == VK_SUCCESS
            ? vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                                        &pipeline_info, NULL, &pipeline)
            : VK_ERROR_INITIALIZATION_FAILED;
    snprintf(details, sizeof(details), "layout=%d pipeline=%d handle=%s",
             layout_status, pipeline_status,
             pipeline != VK_NULL_HANDLE ? "valid" : "null");
    result("host-vulkan-graphics-pipeline",
           layout_status == VK_SUCCESS && pipeline_status == VK_SUCCESS
                   && pipeline != VK_NULL_HANDLE,
           details);

    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = graphics_family,
    };
    VkResult pool_status = device != VK_NULL_HANDLE
            ? vkCreateCommandPool(device, &pool_create_info,
                                  NULL, &command_pool)
            : VK_ERROR_INITIALIZATION_FAILED;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo command_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkResult command_status = pool_status == VK_SUCCESS
            ? vkAllocateCommandBuffers(
                    device, &command_allocate_info, &command_buffer)
            : pool_status;
    snprintf(details, sizeof(details), "pool=%d allocate=%d handle=%s",
             pool_status, command_status,
             command_buffer != VK_NULL_HANDLE ? "valid" : "null");
    result("host-vulkan-command-buffer",
           pool_status == VK_SUCCESS && command_status == VK_SUCCESS
                   && command_buffer != VK_NULL_HANDLE,
           details);

    uint32_t image_index = UINT32_MAX;
    VkResult acquire_status = swapchain != VK_NULL_HANDLE
            ? vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                    VK_NULL_HANDLE, VK_NULL_HANDLE,
                                    &image_index)
            : VK_ERROR_INITIALIZATION_FAILED;
    snprintf(details, sizeof(details), "status=%d index=%u count=%u",
             acquire_status, image_index, returned_image_count);
    result("host-vulkan-acquire",
           acquire_status == VK_SUCCESS && image_index < returned_image_count,
           details);

    VkResult record_status = VK_ERROR_INITIALIZATION_FAILED;
    if (command_buffer != VK_NULL_HANDLE
            && image_index < returned_image_count
            && pipeline_status == VK_SUCCESS
            && framebuffer_status == VK_SUCCESS) {
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        record_status = vkBeginCommandBuffer(command_buffer, &begin_info);
        VkClearValue clear_value = {
            .color.float32 = {0.10f, 0.75f, 0.25f, 1.0f},
        };
        VkRenderPassBeginInfo render_begin = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = render_pass,
            .framebuffer = framebuffer,
            .renderArea.extent = swapchain_create_info.imageExtent,
            .clearValueCount = 1,
            .pClearValues = &clear_value,
        };
        vkCmdBeginRenderPass(command_buffer, &render_begin,
                             VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline);
        vkCmdDraw(command_buffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(command_buffer);
        if (record_status == VK_SUCCESS)
            record_status = vkEndCommandBuffer(command_buffer);
    }
    snprintf(details, sizeof(details),
             "status=%d background=26,191,64 triangle=230,20,10",
             record_status);
    result("host-vulkan-record-triangle", record_status == VK_SUCCESS,
           details);

    VkSemaphore present_semaphore = VK_NULL_HANDLE;
    VkSemaphoreCreateInfo semaphore_create_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkResult semaphore_status = device != VK_NULL_HANDLE
            ? vkCreateSemaphore(device, &semaphore_create_info, NULL,
                                &present_semaphore)
            : VK_ERROR_INITIALIZATION_FAILED;
    snprintf(details, sizeof(details), "status=%d handle=%s",
             semaphore_status,
             present_semaphore != VK_NULL_HANDLE ? "valid" : "null");
    result("host-vulkan-present-semaphore",
           semaphore_status == VK_SUCCESS
                   && present_semaphore != VK_NULL_HANDLE,
           details);

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = command_buffer != VK_NULL_HANDLE ? 1u : 0u,
        .pCommandBuffers = command_buffer != VK_NULL_HANDLE
                ? &command_buffer : NULL,
        .signalSemaphoreCount = present_semaphore != VK_NULL_HANDLE ? 1u : 0u,
        .pSignalSemaphores = present_semaphore != VK_NULL_HANDLE
                ? &present_semaphore : NULL,
    };
    VkResult submit_status = queue != VK_NULL_HANDLE
            && record_status == VK_SUCCESS && semaphore_status == VK_SUCCESS
            ? vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE)
            : VK_ERROR_INITIALIZATION_FAILED;
    snprintf(details, sizeof(details), "submit=%d signal=present-semaphore",
             submit_status);
    result("host-vulkan-submit-graphics", submit_status == VK_SUCCESS,
           details);

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = present_semaphore != VK_NULL_HANDLE ? 1u : 0u,
        .pWaitSemaphores = present_semaphore != VK_NULL_HANDLE
                ? &present_semaphore : NULL,
        .swapchainCount = swapchain != VK_NULL_HANDLE ? 1u : 0u,
        .pSwapchains = swapchain != VK_NULL_HANDLE ? &swapchain : NULL,
        .pImageIndices = image_index < returned_image_count
                ? &image_index : NULL,
    };
    VkResult present_status = submit_status == VK_SUCCESS
            ? vkQueuePresentKHR(queue, &present_info)
            : VK_ERROR_INITIALIZATION_FAILED;
    XSync(display, False);
    snprintf(details, sizeof(details),
             "status=%d index=%u background=26,191,64 triangle=230,20,10",
             present_status, image_index);
    result("host-vulkan-present", present_status == VK_SUCCESS, details);

    if (present_status == VK_SUCCESS) {
        present_info.waitSemaphoreCount = 0;
        present_info.pWaitSemaphores = NULL;
        for (unsigned frame = 0; frame < 50; ++frame) {
            vkQueuePresentKHR(queue, &present_info);
            XSync(display, False);
            usleep(100000);
        }
    }
    if (device != VK_NULL_HANDLE) vkDeviceWaitIdle(device);
    if (present_semaphore != VK_NULL_HANDLE)
        vkDestroySemaphore(device, present_semaphore, NULL);
    if (command_pool != VK_NULL_HANDLE)
        vkDestroyCommandPool(device, command_pool, NULL);
    if (pipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(device, pipeline, NULL);
    if (pipeline_layout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device, pipeline_layout, NULL);
    if (vertex_module != VK_NULL_HANDLE)
        vkDestroyShaderModule(device, vertex_module, NULL);
    if (fragment_module != VK_NULL_HANDLE)
        vkDestroyShaderModule(device, fragment_module, NULL);
    if (framebuffer != VK_NULL_HANDLE)
        vkDestroyFramebuffer(device, framebuffer, NULL);
    if (render_pass != VK_NULL_HANDLE)
        vkDestroyRenderPass(device, render_pass, NULL);
    if (image_view != VK_NULL_HANDLE)
        vkDestroyImageView(device, image_view, NULL);
    free(swapchain_images);
    if (swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(device, swapchain, NULL);
    if (device != VK_NULL_HANDLE) vkDestroyDevice(device, NULL);

    if (xcb_surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(instance, xcb_surface, NULL);
    if (surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance, surface, NULL);

destroy_instance:
    vkDestroyInstance(instance, NULL);
done:
    if (display && window) XDestroyWindow(display, window);
    if (display) XCloseDisplay(display);
    printf("BXSUMMARY host-vulkan passed=%u failed=%u\n", passed, failed);
    return failed ? 1 : 0;
}
