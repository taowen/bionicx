#ifndef BIONICX_VULKAN_PROBE_COMMON_H
#define BIONICX_VULKAN_PROBE_COMMON_H

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

typedef struct ProbeEnv {
    unsigned passed;
    unsigned failed;
    char details[512];
    uint32_t loader_version;
    Display *display;
    Window window;
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkPhysicalDeviceMemoryProperties memory;
    uint32_t graphics_family;
    VkSurfaceKHR surface;
    VkSurfaceKHR xcb_surface;
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR formats[8];
    uint32_t format_count;
    VkSurfaceFormatKHR selected_format;
    int preferred_format;
    VkDevice device;
    VkQueue queue;
    VkSwapchainCreateInfoKHR swapchain_info;
    VkSwapchainKHR swapchain;
    VkImage *swapchain_images;
    uint32_t image_count;
} ProbeEnv;

void result(ProbeEnv *env, const char *name, bool ok);
uint32_t *read_spirv(const char *path, size_t *size);
VkResult upload_buffer(VkDevice device,
                       const VkPhysicalDeviceMemoryProperties *memory,
                       VkBufferUsageFlags usage, const void *data, size_t size,
                       VkBuffer *buffer, VkDeviceMemory *device_memory);

void probe_env_init(ProbeEnv *env);
int probe_open_window(ProbeEnv *env);
int probe_create_instance(ProbeEnv *env);
int probe_pick_physical(ProbeEnv *env);
void probe_create_surfaces(ProbeEnv *env);
void probe_query_surface(ProbeEnv *env);
int probe_create_device(ProbeEnv *env);
int probe_create_device_basic(ProbeEnv *env);
int probe_create_swapchain(ProbeEnv *env);
void probe_env_destroy(ProbeEnv *env);

#endif
