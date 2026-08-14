#include <dlfcn.h>
#include <stdbool.h>
#include <stddef.h>
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

typedef struct Vertex {
    float position[2];
    float color[3];
} Vertex;

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

static VkResult upload_buffer(
        VkDevice device, const VkPhysicalDeviceMemoryProperties *memory,
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

int main(void) {
    uint32_t loader_version = VK_API_VERSION_1_0;
    VkResult status = vkEnumerateInstanceVersion(&loader_version);
    char details[512];
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
    uint32_t returned_extension_count = extension_count;
    if (extension_status == VK_SUCCESS)
        extension_status = vkEnumerateInstanceExtensionProperties(
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
    free(extensions);
    snprintf(details, sizeof(details),
             "status=%d version=%u.%u.%u icd=%s extensions=%u surface=%u xlib=%u xcb=%u",
             status, VK_VERSION_MAJOR(loader_version),
             VK_VERSION_MINOR(loader_version), VK_VERSION_PATCH(loader_version),
             icd_ok ? icd_how : "missing", returned_extension_count,
             has_surface, has_xlib_surface, has_xcb_surface);
    result("host-vulkan-loader",
           status == VK_SUCCESS && icd_ok
                   && extension_status == VK_SUCCESS && extension_count > 0
                   && has_surface && has_xlib_surface && has_xcb_surface,
           details);

    Display *display = XOpenDisplay(NULL);
    Window window = 0;
    if (display) {
        window = XCreateSimpleWindow(display, DefaultRootWindow(display),
                                     80, 240, 640, 360, 0, 0, 0);
        XStoreName(display, window, "BionicX Vulkan probe");
        XMapWindow(display, window);
        XSync(display, False);
    }
    VisualID window_visual = 0;
    VisualID root_visual = 0;
    unsigned long red_mask = 0;
    unsigned long green_mask = 0;
    unsigned long blue_mask = 0;
    int depth = 0;
    int visual_class = 0;
    if (display && window) {
        XWindowAttributes attributes;
        if (XGetWindowAttributes(display, window, &attributes)
                && attributes.visual) {
            window_visual = XVisualIDFromVisual(attributes.visual);
            red_mask = attributes.visual->red_mask;
            green_mask = attributes.visual->green_mask;
            blue_mask = attributes.visual->blue_mask;
            depth = attributes.depth;
            visual_class = attributes.visual->class;
        }
        root_visual = XVisualIDFromVisual(
                DefaultVisual(display, DefaultScreen(display)));
    }
    snprintf(details, sizeof(details),
             "display=%s window=0x%lx visual=0x%lx root=0x%lx class=%d depth=%d "
             "red=0x%lx green=0x%lx blue=0x%lx",
             display ? "open" : "closed", (unsigned long)window,
             (unsigned long)window_visual, (unsigned long)root_visual,
             visual_class, depth, red_mask, green_mask, blue_mask);
    /* ANGLE maps this TrueColor layout to VK_FORMAT_B8G8R8A8_UNORM. */
    result("host-vulkan-window",
           display && window != 0
                   && window_visual != 0 && window_visual == root_visual
                   && visual_class == TrueColor && depth >= 24
                   && red_mask == 0xff0000UL
                   && green_mask == 0x00ff00UL
                   && blue_mask == 0x0000ffUL,
           details);

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
    if (status != VK_SUCCESS || instance == VK_NULL_HANDLE) {
        snprintf(details, sizeof(details), "instance=%d handle=null", status);
        result("host-vulkan-device", false, details);
        goto done;
    }

    uint32_t physical_count = 0;
    status = vkEnumeratePhysicalDevices(instance, &physical_count, NULL);
    if (status != VK_SUCCESS || physical_count == 0) {
        snprintf(details, sizeof(details), "devices=%d count=%u",
                 status, physical_count);
        result("host-vulkan-device", false, details);
        goto destroy_instance;
    }

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

    uint32_t device_extension_count = 0;
    VkResult device_extension_status = vkEnumerateDeviceExtensionProperties(
            physical_device, NULL, &device_extension_count, NULL);
    VkExtensionProperties *device_extensions_available = calloc(
            device_extension_count, sizeof(*device_extensions_available));
    uint32_t returned_device_extension_count = device_extension_count;
    if (device_extension_status == VK_SUCCESS
            && returned_device_extension_count > 0) {
        device_extension_status = vkEnumerateDeviceExtensionProperties(
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
    free(queue_families);

    VkPhysicalDeviceMemoryProperties memory;
    memset(&memory, 0, sizeof(memory));
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory);
    uint64_t heap_bytes = 0;
    for (uint32_t i = 0; i < memory.memoryHeapCount; ++i)
        heap_bytes += memory.memoryHeaps[i].size;
    snprintf(details, sizeof(details),
             "name=%s api=%u.%u.%u vendor=0x%04x swapchain=%u fsr=%u "
             "families=%u graphics=%u types=%u heaps=%u",
             properties.deviceName,
             VK_VERSION_MAJOR(properties.apiVersion),
             VK_VERSION_MINOR(properties.apiVersion),
             VK_VERSION_PATCH(properties.apiVersion),
             properties.vendorID, has_swapchain, has_fragment_shading_rate,
             queue_count, graphics_family, memory.memoryTypeCount,
             memory.memoryHeapCount);
    result("host-vulkan-device",
           physical_device != VK_NULL_HANDLE && properties.deviceName[0]
                   && device_extension_status == VK_SUCCESS && has_swapchain
                   && !has_fragment_shading_rate
                   && queue_count > 0 && graphics_family != UINT32_MAX
                   && memory.memoryTypeCount > 0 && memory.memoryHeapCount > 0
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
    VkBool32 xcb_supported = xcb_connection
            ? vkGetPhysicalDeviceXcbPresentationSupportKHR(
                    physical_device, graphics_family, xcb_connection,
                    (xcb_visualid_t)XVisualIDFromVisual(
                            DefaultVisual(display, DefaultScreen(display))))
            : VK_FALSE;
    snprintf(details, sizeof(details),
             "xlib=%d xcb=%d window=0x%lx surface=%u xlibPresent=%u xcbPresent=%u",
             status, xcb_surface_status, (unsigned long)window,
             surface_supported, xlib_supported, xcb_supported);
    result("host-vulkan-wsi",
           status == VK_SUCCESS && surface != VK_NULL_HANDLE
                   && xcb_surface_status == VK_SUCCESS
                   && xcb_surface != VK_NULL_HANDLE
                   && support_status == VK_SUCCESS && surface_supported
                   && xlib_supported && xcb_supported == VK_TRUE,
           details);

    VkSurfaceCapabilitiesKHR capabilities;
    memset(&capabilities, 0, sizeof(capabilities));
    status = surface != VK_NULL_HANDLE
            ? vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                    physical_device, surface, &capabilities)
            : VK_ERROR_SURFACE_LOST_KHR;
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
    bool has_bgra = false;
    for (uint32_t i = 0; i < returned_format_count; ++i)
        has_bgra |= formats[i].format == VK_FORMAT_B8G8R8A8_UNORM;

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
             "extent=%ux%u images=%u..%u usage=0x%x formats=%u bgra=%u fifo=%u",
             capabilities.currentExtent.width, capabilities.currentExtent.height,
             capabilities.minImageCount, capabilities.maxImageCount,
             capabilities.supportedUsageFlags, format_count, has_bgra, has_fifo);
    result("host-vulkan-surface",
           status == VK_SUCCESS && capabilities.currentExtent.width == 640
                   && capabilities.currentExtent.height == 360
                   && capabilities.minImageCount >= 2
                   && (capabilities.supportedUsageFlags
                       & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                   && format_count > 0 && has_bgra
                   && mode_count > 0 && has_fifo,
           details);

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
    VkQueue queue = VK_NULL_HANDLE;
    if (device != VK_NULL_HANDLE)
        vkGetDeviceQueue(device, graphics_family, 0, &queue);
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
    snprintf(details, sizeof(details),
             "device=%s queue=%s format=%d extent=%ux%u images=%u returned=%u",
             device != VK_NULL_HANDLE ? "valid" : "null",
             queue != VK_NULL_HANDLE ? "valid" : "null",
             selected_format.format, swapchain_create_info.imageExtent.width,
             swapchain_create_info.imageExtent.height,
             swapchain_image_count, returned_image_count);
    result("host-vulkan-swapchain",
           device != VK_NULL_HANDLE && queue != VK_NULL_HANDLE
                   && swapchain != VK_NULL_HANDLE && preferred_format
                   && selected_format.format == VK_FORMAT_B8G8R8A8_UNORM
                   && status == VK_SUCCESS && returned_image_count >= 2,
           details);

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

    const Vertex vertices[3] = {
        {{0.0f, -0.72f}, {0.90f, 0.08f, 0.04f}},
        {{0.72f, 0.62f}, {0.90f, 0.08f, 0.04f}},
        {{-0.72f, 0.62f}, {0.90f, 0.08f, 0.04f}},
    };
    VkBufferCreateInfo vertex_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(vertices),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkResult vertex_buffer_status = vkCreateBuffer(
            device, &vertex_buffer_info, NULL, &vertex_buffer);
    VkMemoryRequirements vertex_memory_requirements = {0};
    if (vertex_buffer_status == VK_SUCCESS)
        vkGetBufferMemoryRequirements(
                device, vertex_buffer, &vertex_memory_requirements);
    uint32_t vertex_memory_type = UINT32_MAX;
    for (uint32_t i = 0; i < memory.memoryTypeCount; i++) {
        VkMemoryPropertyFlags required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if ((vertex_memory_requirements.memoryTypeBits & (1u << i))
                && (memory.memoryTypes[i].propertyFlags & required)
                        == required) {
            vertex_memory_type = i;
            break;
        }
    }
    VkMemoryAllocateInfo vertex_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = vertex_memory_requirements.size,
        .memoryTypeIndex = vertex_memory_type,
    };
    VkDeviceMemory vertex_memory = VK_NULL_HANDLE;
    VkResult vertex_allocate_status = vertex_memory_type != UINT32_MAX
            ? vkAllocateMemory(device, &vertex_allocate_info, NULL,
                               &vertex_memory)
            : VK_ERROR_FEATURE_NOT_PRESENT;
    VkResult vertex_bind_status = vertex_allocate_status == VK_SUCCESS
            ? vkBindBufferMemory(device, vertex_buffer, vertex_memory, 0)
            : vertex_allocate_status;
    void *vertex_mapping = NULL;
    VkResult vertex_map_status = vertex_bind_status == VK_SUCCESS
            ? vkMapMemory(device, vertex_memory, 0, sizeof(vertices), 0,
                          &vertex_mapping)
            : vertex_bind_status;
    if (vertex_map_status == VK_SUCCESS) {
        memcpy(vertex_mapping, vertices, sizeof(vertices));
        vkUnmapMemory(device, vertex_memory);
    }
    snprintf(details, sizeof(details),
             "create=%d type=%u allocate=%d bind=%d map=%d bytes=%zu",
             vertex_buffer_status, vertex_memory_type,
             vertex_allocate_status, vertex_bind_status, vertex_map_status,
             sizeof(vertices));
    bool vertex_ok = vertex_buffer_status == VK_SUCCESS
            && vertex_allocate_status == VK_SUCCESS
            && vertex_bind_status == VK_SUCCESS
            && vertex_map_status == VK_SUCCESS;

    const uint16_t indices[3] = {0, 1, 2};
    const float tint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    VkBuffer index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory index_memory = VK_NULL_HANDLE;
    VkResult index_upload_status = upload_buffer(
            device, &memory, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            indices, sizeof(indices), &index_buffer, &index_memory);
    VkBuffer uniform_buffer = VK_NULL_HANDLE;
    VkDeviceMemory uniform_memory = VK_NULL_HANDLE;
    VkResult uniform_upload_status = upload_buffer(
            device, &memory, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            tint, sizeof(tint), &uniform_buffer, &uniform_memory);
    snprintf(details, sizeof(details),
             "index=%d indexBytes=%zu uniform=%d uniformBytes=%zu",
             index_upload_status, sizeof(indices), uniform_upload_status,
             sizeof(tint));
    bool uploads_ok = index_upload_status == VK_SUCCESS
            && uniform_upload_status == VK_SUCCESS;

    const uint8_t texture_pixels[16] = {
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
    };
    VkBuffer texture_staging = VK_NULL_HANDLE;
    VkDeviceMemory texture_staging_memory = VK_NULL_HANDLE;
    VkResult texture_staging_status = upload_buffer(
            device, &memory, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            texture_pixels, sizeof(texture_pixels),
            &texture_staging, &texture_staging_memory);
    VkImageCreateInfo texture_image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {2, 2, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage texture_image = VK_NULL_HANDLE;
    VkResult texture_image_status = vkCreateImage(
            device, &texture_image_info, NULL, &texture_image);
    VkMemoryRequirements texture_requirements = {0};
    if (texture_image_status == VK_SUCCESS)
        vkGetImageMemoryRequirements(
                device, texture_image, &texture_requirements);
    uint32_t texture_memory_type = UINT32_MAX;
    for (uint32_t i = 0; i < memory.memoryTypeCount; i++) {
        if ((texture_requirements.memoryTypeBits & (1u << i))
                && (memory.memoryTypes[i].propertyFlags
                        & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            texture_memory_type = i;
            break;
        }
    }
    VkMemoryAllocateInfo texture_allocation = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = texture_requirements.size,
        .memoryTypeIndex = texture_memory_type,
    };
    VkDeviceMemory texture_memory = VK_NULL_HANDLE;
    VkResult texture_allocate_status = texture_memory_type != UINT32_MAX
            ? vkAllocateMemory(device, &texture_allocation, NULL,
                               &texture_memory)
            : VK_ERROR_FEATURE_NOT_PRESENT;
    VkResult texture_bind_status = texture_allocate_status == VK_SUCCESS
            ? vkBindImageMemory(device, texture_image, texture_memory, 0)
            : texture_allocate_status;
    VkImageViewCreateInfo texture_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    VkImageView texture_view = VK_NULL_HANDLE;
    VkResult texture_view_status = texture_bind_status == VK_SUCCESS
            ? vkCreateImageView(device, &texture_view_info, NULL,
                                &texture_view)
            : texture_bind_status;
    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .maxLod = 1.0f,
    };
    VkSampler texture_sampler = VK_NULL_HANDLE;
    VkResult texture_sampler_status = texture_view_status == VK_SUCCESS
            ? vkCreateSampler(device, &sampler_info, NULL, &texture_sampler)
            : texture_view_status;
    snprintf(details, sizeof(details),
             "staging=%d image=%d type=%u allocate=%d bind=%d view=%d sampler=%d",
             texture_staging_status, texture_image_status,
             texture_memory_type, texture_allocate_status,
             texture_bind_status, texture_view_status,
             texture_sampler_status);
    bool sampled_ok = texture_staging_status == VK_SUCCESS
            && texture_image_status == VK_SUCCESS
            && texture_allocate_status == VK_SUCCESS
            && texture_bind_status == VK_SUCCESS
            && texture_view_status == VK_SUCCESS
            && texture_sampler_status == VK_SUCCESS;

    VkDescriptorSetLayoutBinding descriptor_bindings[2] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    VkDescriptorSetLayoutCreateInfo descriptor_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = descriptor_bindings,
    };
    VkDescriptorSetLayout descriptor_layout = VK_NULL_HANDLE;
    VkResult descriptor_layout_status = vkCreateDescriptorSetLayout(
            device, &descriptor_layout_info, NULL, &descriptor_layout);
    VkDescriptorPoolSize descriptor_pool_sizes[2] = {
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
        },
    };
    VkDescriptorPoolCreateInfo descriptor_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 2,
        .pPoolSizes = descriptor_pool_sizes,
    };
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VkResult descriptor_pool_status = descriptor_layout_status == VK_SUCCESS
            ? vkCreateDescriptorPool(device, &descriptor_pool_info, NULL,
                                     &descriptor_pool)
            : descriptor_layout_status;
    VkDescriptorSetAllocateInfo descriptor_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptor_layout,
    };
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    VkResult descriptor_allocate_status = descriptor_pool_status == VK_SUCCESS
            ? vkAllocateDescriptorSets(device, &descriptor_allocate_info,
                                       &descriptor_set)
            : descriptor_pool_status;
    VkDescriptorBufferInfo uniform_descriptor = {
        .buffer = uniform_buffer,
        .offset = 0,
        .range = sizeof(tint),
    };
    VkDescriptorImageInfo sampled_descriptor = {
        .sampler = texture_sampler,
        .imageView = texture_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet descriptor_writes[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &uniform_descriptor,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &sampled_descriptor,
        },
    };
    if (descriptor_allocate_status == VK_SUCCESS)
        vkUpdateDescriptorSets(device, 2, descriptor_writes, 0, NULL);
    snprintf(details, sizeof(details), "layout=%d pool=%d allocate=%d set=%s",
             descriptor_layout_status, descriptor_pool_status,
             descriptor_allocate_status,
             descriptor_set != VK_NULL_HANDLE ? "valid" : "null");
    bool descriptor_ok = descriptor_layout_status == VK_SUCCESS
            && descriptor_pool_status == VK_SUCCESS
            && descriptor_allocate_status == VK_SUCCESS
            && descriptor_set != VK_NULL_HANDLE;

    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = descriptor_layout != VK_NULL_HANDLE ? 1u : 0u,
        .pSetLayouts = descriptor_layout != VK_NULL_HANDLE
                ? &descriptor_layout : NULL,
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
    VkVertexInputBindingDescription vertex_binding = {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription vertex_attributes[2] = {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex, position),
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, color),
        },
    };
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &vertex_binding;
    vertex_input.vertexAttributeDescriptionCount = 2;
    vertex_input.pVertexAttributeDescriptions = vertex_attributes;
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
    bool pipeline_ok = layout_status == VK_SUCCESS
            && pipeline_status == VK_SUCCESS
            && pipeline != VK_NULL_HANDLE;

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
    snprintf(details, sizeof(details),
             "target=%d/%d/%d shaders=%d/%d vertex=%u uploads=%d sampled=%d "
             "descriptor=%d pipeline=%d cmd=%d/%d",
             image_view_status, render_pass_status, framebuffer_status,
             vertex_module_status, fragment_module_status, vertex_ok,
             uploads_ok, sampled_ok, descriptor_ok, pipeline_ok,
             pool_status, command_status);
    result("host-vulkan-pipeline",
           image_view_status == VK_SUCCESS
                   && render_pass_status == VK_SUCCESS
                   && framebuffer_status == VK_SUCCESS
                   && vertex_module_status == VK_SUCCESS
                   && fragment_module_status == VK_SUCCESS
                   && vertex_ok && uploads_ok && sampled_ok && descriptor_ok
                   && pipeline_ok
                   && pool_status == VK_SUCCESS && command_status == VK_SUCCESS
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

    VkResult record_status = VK_ERROR_INITIALIZATION_FAILED;
    if (command_buffer != VK_NULL_HANDLE
            && image_index < returned_image_count
            && pipeline_status == VK_SUCCESS
            && framebuffer_status == VK_SUCCESS
            && index_upload_status == VK_SUCCESS
            && texture_sampler_status == VK_SUCCESS
            && descriptor_allocate_status == VK_SUCCESS) {
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        record_status = vkBeginCommandBuffer(command_buffer, &begin_info);
        VkImageSubresourceRange texture_range = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        VkImageMemoryBarrier texture_to_transfer = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = texture_image,
            .subresourceRange = texture_range,
        };
        vkCmdPipelineBarrier(command_buffer,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, NULL, 0, NULL, 1, &texture_to_transfer);
        VkBufferImageCopy texture_copy = {
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .imageExtent = {2, 2, 1},
        };
        vkCmdCopyBufferToImage(command_buffer, texture_staging,
                               texture_image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1, &texture_copy);
        VkImageMemoryBarrier texture_to_sample = texture_to_transfer;
        texture_to_sample.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        texture_to_sample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        texture_to_sample.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        texture_to_sample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(command_buffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                             0, NULL, 0, NULL, 1, &texture_to_sample);
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
        vkCmdBindDescriptorSets(command_buffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout, 0, 1, &descriptor_set,
                                0, NULL);
        VkDeviceSize vertex_offset = 0;
        vkCmdBindVertexBuffers2(command_buffer, 0, 1, &vertex_buffer,
                               &vertex_offset, NULL, NULL);
        vkCmdBindIndexBuffer(command_buffer, index_buffer, 0,
                             VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(command_buffer, 3, 1, 0, 0, 0);
        vkCmdEndRenderPass(command_buffer);
        if (record_status == VK_SUCCESS)
            record_status = vkEndCommandBuffer(command_buffer);
    }
    snprintf(details, sizeof(details),
             "status=%d background=26,191,64 triangle=230,20,10",
             record_status);

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
             "status=%d acquire=%d record=%d semaphore=%d submit=%d index=%u "
             "bind2=null background=26,191,64 triangle=230,20,10",
             present_status, acquire_status, record_status, semaphore_status,
             submit_status, image_index);
    result("host-vulkan-present",
           acquire_status == VK_SUCCESS && image_index < returned_image_count
                   && record_status == VK_SUCCESS
                   && semaphore_status == VK_SUCCESS
                   && present_semaphore != VK_NULL_HANDLE
                   && submit_status == VK_SUCCESS
                   && present_status == VK_SUCCESS,
           details);

    if (present_status == VK_SUCCESS) {
        present_info.waitSemaphoreCount = 0;
        present_info.pWaitSemaphores = NULL;
        for (unsigned frame = 0; frame < 8; ++frame) {
            vkQueuePresentKHR(queue, &present_info);
            XSync(display, False);
            usleep(20000);
        }
        /* Hold the presented triangle long enough for a slow adb screencap.
         * Resize/unmap below would otherwise clear the pixels. */
        usleep(5000000);
    }

    uint32_t second_index = UINT32_MAX;
    VkResult second_acquire = swapchain != VK_NULL_HANDLE
            ? vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                    VK_NULL_HANDLE, VK_NULL_HANDLE,
                                    &second_index)
            : VK_ERROR_INITIALIZATION_FAILED;
    bool rotate_ok = second_acquire == VK_SUCCESS
            && returned_image_count >= 2
            && second_index != image_index
            && second_index < returned_image_count;

    if (display && window) {
        XResizeWindow(display, window, 800, 400);
        XSync(display, False);
        usleep(50000);
    }
    VkSurfaceCapabilitiesKHR resized = {0};
    VkResult resized_caps = surface != VK_NULL_HANDLE
            ? vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                    physical_device, surface, &resized)
            : VK_ERROR_SURFACE_LOST_KHR;
    uint32_t stale_index = 0;
    VkResult stale_acquire = swapchain != VK_NULL_HANDLE
            ? vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                    VK_NULL_HANDLE, VK_NULL_HANDLE,
                                    &stale_index)
            : VK_ERROR_INITIALIZATION_FAILED;
    bool outdated_ok = resized_caps == VK_SUCCESS
            && (stale_acquire == VK_ERROR_OUT_OF_DATE_KHR
                || resized.currentExtent.width != 640);

    VkSwapchainKHR recreated = VK_NULL_HANDLE;
    swapchain_create_info.oldSwapchain = swapchain;
    swapchain_create_info.imageExtent = resized.currentExtent.width > 0
            ? resized.currentExtent
            : (VkExtent2D){800, 400};
    swapchain_create_info.minImageCount = resized.minImageCount >= 2
            ? resized.minImageCount : 2;
    VkResult recreate_status = device != VK_NULL_HANDLE && surface != VK_NULL_HANDLE
            ? vkCreateSwapchainKHR(device, &swapchain_create_info, NULL,
                                   &recreated)
            : VK_ERROR_INITIALIZATION_FAILED;
    if (swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(device, swapchain, NULL);
    swapchain = recreated;
    uint32_t recreated_count = 0;
    uint32_t recreated_index = 0;
    VkResult recreated_acquire = VK_ERROR_INITIALIZATION_FAILED;
    if (recreate_status == VK_SUCCESS && swapchain != VK_NULL_HANDLE) {
        vkGetSwapchainImagesKHR(device, swapchain, &recreated_count, NULL);
        recreated_acquire = vkAcquireNextImageKHR(
                device, swapchain, UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE,
                &recreated_index);
        present_info.pSwapchains = &swapchain;
        present_info.pImageIndices = &recreated_index;
        present_info.waitSemaphoreCount = 0;
        if (recreated_acquire == VK_SUCCESS)
            vkQueuePresentKHR(queue, &present_info);
    }
    bool recreate_ok = recreate_status == VK_SUCCESS && recreated_count >= 2
            && recreated_acquire == VK_SUCCESS;

    if (display && window) {
        XResizeWindow(display, window, 640, 360);
        XUnmapWindow(display, window);
        XSync(display, False);
        usleep(50000);
        XMapWindow(display, window);
        XSync(display, False);
        usleep(50000);
    }
    VkSurfaceCapabilitiesKHR remapped = {0};
    if (surface != VK_NULL_HANDLE)
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                physical_device, surface, &remapped);
    snprintf(details, sizeof(details),
             "rotate=%u first=%u second=%u outdated=%u extent=%ux%u acquire=%d "
             "recreate=%u images=%u remap=%ux%u",
             rotate_ok, image_index, second_index, outdated_ok,
             resized.currentExtent.width, resized.currentExtent.height,
             stale_acquire, recreate_ok, recreated_count,
             remapped.currentExtent.width, remapped.currentExtent.height);
    result("host-vulkan-swapchain-lifetime",
           rotate_ok && outdated_ok && recreate_ok
                   && remapped.currentExtent.width > 0
                   && remapped.currentExtent.height > 0,
           details);

    if (device != VK_NULL_HANDLE) vkDeviceWaitIdle(device);
    if (present_semaphore != VK_NULL_HANDLE)
        vkDestroySemaphore(device, present_semaphore, NULL);
    if (command_pool != VK_NULL_HANDLE)
        vkDestroyCommandPool(device, command_pool, NULL);
    if (pipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(device, pipeline, NULL);
    if (pipeline_layout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device, pipeline_layout, NULL);
    if (descriptor_pool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(device, descriptor_pool, NULL);
    if (descriptor_layout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(device, descriptor_layout, NULL);
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
    if (vertex_buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(device, vertex_buffer, NULL);
    if (vertex_memory != VK_NULL_HANDLE)
        vkFreeMemory(device, vertex_memory, NULL);
    if (index_buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(device, index_buffer, NULL);
    if (index_memory != VK_NULL_HANDLE)
        vkFreeMemory(device, index_memory, NULL);
    if (uniform_buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(device, uniform_buffer, NULL);
    if (uniform_memory != VK_NULL_HANDLE)
        vkFreeMemory(device, uniform_memory, NULL);
    if (texture_sampler != VK_NULL_HANDLE)
        vkDestroySampler(device, texture_sampler, NULL);
    if (texture_view != VK_NULL_HANDLE)
        vkDestroyImageView(device, texture_view, NULL);
    if (texture_image != VK_NULL_HANDLE)
        vkDestroyImage(device, texture_image, NULL);
    if (texture_memory != VK_NULL_HANDLE)
        vkFreeMemory(device, texture_memory, NULL);
    if (texture_staging != VK_NULL_HANDLE)
        vkDestroyBuffer(device, texture_staging, NULL);
    if (texture_staging_memory != VK_NULL_HANDLE)
        vkFreeMemory(device, texture_staging_memory, NULL);
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
