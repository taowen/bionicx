#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

destroy_instance:
    vkDestroyInstance(instance, NULL);
done:
    printf("BXSUMMARY host-vulkan passed=%u failed=%u\n", passed, failed);
    return failed ? 1 : 0;
}
