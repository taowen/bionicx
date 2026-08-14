#include <pthread.h>

#include "common.h"

typedef struct Vertex {
    float position[2];
    float color[3];
} Vertex;

static volatile int timeline_present_done;

static void *timeline_present_watchdog(void *unused) {
    (void)unused;
    for (int i = 0; i < 40; ++i) {
        if (timeline_present_done) return NULL;
        usleep(100000);
    }
    printf("BXTEST FAIL vulkan-present "
           "timeline-present hung after vkQueueWaitIdle\n");
    fflush(stdout);
    _exit(2);
}

static int bring_up(ProbeEnv *env) {
    if (!probe_open_window(env)) return 0;
    vkEnumerateInstanceVersion(&env->loader_version);
    if (!probe_create_instance(env)) return 0;
    if (!probe_pick_physical(env)) return 0;
    probe_create_surfaces(env);
    probe_query_surface(env);
    return probe_create_device(env);
}

int main(void) {
    ProbeEnv env;
    probe_env_init(&env);
    if (!bring_up(&env) || !probe_create_swapchain(&env)) {
        snprintf(env.details, sizeof(env.details),
                 "device=%s queue=%s swapchain=failed",
                 env.device != VK_NULL_HANDLE ? "valid" : "null",
                 env.queue != VK_NULL_HANDLE ? "valid" : "null");
        result(&env, "vulkan-present-swapchain", false);
        result(&env, "vulkan-present-pipeline", false);
        result(&env, "vulkan-present", false);
        probe_env_destroy(&env);
        printf("BXSUMMARY vulkan-present passed=%u failed=%u\n",
               env.passed, env.failed);
        return 1;
    }
    snprintf(env.details, sizeof(env.details),
             "device=%s queue=%s format=%d extent=%ux%u images=%u returned=%u",
             "valid", "valid", env.selected_format.format,
             env.swapchain_info.imageExtent.width,
             env.swapchain_info.imageExtent.height,
             env.image_count, env.image_count);
    result(&env, "vulkan-present-swapchain",
           env.preferred_format
                   && env.selected_format.format == VK_FORMAT_B8G8R8A8_UNORM
                   && env.image_count >= 2);

    VkImageView image_view = VK_NULL_HANDLE;
    VkImageViewCreateInfo image_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = env.swapchain_images[0],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = env.selected_format.format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    VkResult view_status = vkCreateImageView(
            env.device, &image_view_info, NULL, &image_view);
    VkAttachmentDescription color_attachment = {
        .format = env.selected_format.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference color_reference = {
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
    VkResult render_pass_status = view_status == VK_SUCCESS
            ? vkCreateRenderPass(env.device, &render_pass_info, NULL,
                                 &render_pass)
            : view_status;
    VkFramebufferCreateInfo framebuffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = render_pass,
        .attachmentCount = 1,
        .pAttachments = &image_view,
        .width = env.swapchain_info.imageExtent.width,
        .height = env.swapchain_info.imageExtent.height,
        .layers = 1,
    };
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkResult framebuffer_status = render_pass_status == VK_SUCCESS
            ? vkCreateFramebuffer(env.device, &framebuffer_info, NULL,
                                  &framebuffer)
            : render_pass_status;

    size_t vert_size = 0, frag_size = 0;
    uint32_t *vert_code = read_spirv("share/vulkan-probe/triangle.vert.spv",
                                     &vert_size);
    uint32_t *frag_code = read_spirv("share/vulkan-probe/triangle.frag.spv",
                                     &frag_size);
    VkShaderModuleCreateInfo vert_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vert_size,
        .pCode = vert_code,
    };
    VkShaderModuleCreateInfo frag_info = vert_info;
    frag_info.codeSize = frag_size;
    frag_info.pCode = frag_code;
    VkShaderModule vert_module = VK_NULL_HANDLE, frag_module = VK_NULL_HANDLE;
    VkResult vert_status = vert_code
            ? vkCreateShaderModule(env.device, &vert_info, NULL, &vert_module)
            : VK_ERROR_INITIALIZATION_FAILED;
    VkResult frag_status = frag_code
            ? vkCreateShaderModule(env.device, &frag_info, NULL, &frag_module)
            : VK_ERROR_INITIALIZATION_FAILED;
    free(vert_code);
    free(frag_code);

    const Vertex vertices[3] = {
        {{0.0f, -0.72f}, {0.90f, 0.08f, 0.04f}},
        {{0.72f, 0.62f}, {0.90f, 0.08f, 0.04f}},
        {{-0.72f, 0.62f}, {0.90f, 0.08f, 0.04f}},
    };
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vertex_memory = VK_NULL_HANDLE;
    VkResult vertex_upload = upload_buffer(
            env.device, &env.memory, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            vertices, sizeof(vertices), &vertex_buffer, &vertex_memory);
    const uint16_t indices[3] = {0, 1, 2};
    const float tint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    VkBuffer index_buffer = VK_NULL_HANDLE, uniform_buffer = VK_NULL_HANDLE;
    VkDeviceMemory index_memory = VK_NULL_HANDLE, uniform_memory = VK_NULL_HANDLE;
    VkResult index_upload = upload_buffer(
            env.device, &env.memory, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            indices, sizeof(indices), &index_buffer, &index_memory);
    VkResult uniform_upload = upload_buffer(
            env.device, &env.memory, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            tint, sizeof(tint), &uniform_buffer, &uniform_memory);

    const uint8_t texture_pixels[16] = {
        255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 255,
    };
    VkBuffer texture_staging = VK_NULL_HANDLE;
    VkDeviceMemory texture_staging_memory = VK_NULL_HANDLE;
    upload_buffer(env.device, &env.memory, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
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
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage texture_image = VK_NULL_HANDLE;
    vkCreateImage(env.device, &texture_image_info, NULL, &texture_image);
    VkMemoryRequirements texture_requirements = {0};
    vkGetImageMemoryRequirements(env.device, texture_image,
                                 &texture_requirements);
    uint32_t texture_type = UINT32_MAX;
    for (uint32_t i = 0; i < env.memory.memoryTypeCount; i++) {
        if ((texture_requirements.memoryTypeBits & (1u << i))
                && (env.memory.memoryTypes[i].propertyFlags
                    & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            texture_type = i;
            break;
        }
    }
    VkMemoryAllocateInfo texture_alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = texture_requirements.size,
        .memoryTypeIndex = texture_type,
    };
    VkDeviceMemory texture_memory = VK_NULL_HANDLE;
    if (texture_type != UINT32_MAX)
        vkAllocateMemory(env.device, &texture_alloc, NULL, &texture_memory);
    if (texture_memory != VK_NULL_HANDLE)
        vkBindImageMemory(env.device, texture_image, texture_memory, 0);
    VkImageViewCreateInfo texture_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    VkImageView texture_view = VK_NULL_HANDLE;
    vkCreateImageView(env.device, &texture_view_info, NULL, &texture_view);
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
    vkCreateSampler(env.device, &sampler_info, NULL, &texture_sampler);

    VkDescriptorSetLayoutBinding bindings[2] = {
        { .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
        { .binding = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
    };
    VkDescriptorSetLayoutCreateInfo set_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings,
    };
    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    vkCreateDescriptorSetLayout(env.device, &set_layout_info, NULL, &set_layout);
    VkDescriptorPoolSize pool_sizes[2] = {
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1 },
        { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1 },
    };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 2,
        .pPoolSizes = pool_sizes,
    };
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    vkCreateDescriptorPool(env.device, &pool_info, NULL, &descriptor_pool);
    VkDescriptorSetAllocateInfo set_alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &set_layout,
    };
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    vkAllocateDescriptorSets(env.device, &set_alloc, &descriptor_set);
    VkDescriptorBufferInfo uniform_descriptor = {
        .buffer = uniform_buffer, .range = sizeof(tint),
    };
    VkDescriptorImageInfo sampled_descriptor = {
        .sampler = texture_sampler,
        .imageView = texture_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet writes[2] = {
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = descriptor_set, .dstBinding = 0, .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .pBufferInfo = &uniform_descriptor },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = descriptor_set, .dstBinding = 1, .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = &sampled_descriptor },
    };
    vkUpdateDescriptorSets(env.device, 2, writes, 0, NULL);

    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &set_layout,
    };
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkResult layout_status = vkCreatePipelineLayout(
            env.device, &layout_info, NULL, &pipeline_layout);
    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert_module,
          .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag_module,
          .pName = "main" },
    };
    VkVertexInputBindingDescription vertex_binding = {
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription vertex_attributes[2] = {
        { .location = 0, .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = offsetof(Vertex, position) },
        { .location = 1, .format = VK_FORMAT_R32G32B32_SFLOAT,
          .offset = offsetof(Vertex, color) },
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertex_binding,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = vertex_attributes,
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    VkViewport viewport = {
        .width = (float)env.swapchain_info.imageExtent.width,
        .height = (float)env.swapchain_info.imageExtent.height,
        .maxDepth = 1,
    };
    VkRect2D scissor = { .extent = env.swapchain_info.imageExtent };
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .pViewports = &viewport,
        .scissorCount = 1, .pScissors = &scissor,
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
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2, .pStages = stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisample,
        .pColorBlendState = &blend,
        .layout = pipeline_layout,
        .renderPass = render_pass,
    };
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult pipeline_status = layout_status == VK_SUCCESS
            && vert_status == VK_SUCCESS && frag_status == VK_SUCCESS
            ? vkCreateGraphicsPipelines(env.device, VK_NULL_HANDLE, 1,
                                        &pipeline_info, NULL, &pipeline)
            : VK_ERROR_INITIALIZATION_FAILED;
    VkCommandPoolCreateInfo cmd_pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = env.graphics_family,
    };
    VkCommandPool command_pool = VK_NULL_HANDLE;
    vkCreateCommandPool(env.device, &cmd_pool_info, NULL, &command_pool);
    VkCommandBufferAllocateInfo cmd_alloc = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(env.device, &cmd_alloc, &command_buffer);
    snprintf(env.details, sizeof(env.details),
             "target=%d/%d/%d shaders=%d/%d vertex=%u uploads=%d sampled=%d "
             "descriptor=%d pipeline=%d cmd=%d",
             view_status, render_pass_status, framebuffer_status,
             vert_status, frag_status, vertex_upload == VK_SUCCESS,
             index_upload == VK_SUCCESS && uniform_upload == VK_SUCCESS,
             texture_sampler != VK_NULL_HANDLE,
             descriptor_set != VK_NULL_HANDLE,
             pipeline_status, command_buffer != VK_NULL_HANDLE);
    result(&env, "vulkan-present-pipeline",
           view_status == VK_SUCCESS && render_pass_status == VK_SUCCESS
                   && framebuffer_status == VK_SUCCESS
                   && vert_status == VK_SUCCESS && frag_status == VK_SUCCESS
                   && vertex_upload == VK_SUCCESS
                   && index_upload == VK_SUCCESS
                   && uniform_upload == VK_SUCCESS
                   && pipeline_status == VK_SUCCESS
                   && command_buffer != VK_NULL_HANDLE);

    uint32_t image_index = UINT32_MAX;
    VkResult acquire_status = vkAcquireNextImageKHR(
            env.device, env.swapchain, UINT64_MAX, VK_NULL_HANDLE,
            VK_NULL_HANDLE, &image_index);
    VkResult record_status = VK_ERROR_INITIALIZATION_FAILED;
    if (command_buffer != VK_NULL_HANDLE
            && image_index < env.image_count
            && pipeline_status == VK_SUCCESS) {
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        record_status = vkBeginCommandBuffer(command_buffer, &begin_info);
        VkImageSubresourceRange texture_range = {
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
            .image = texture_image,
            .subresourceRange = texture_range,
        };
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, NULL, 0, NULL, 1, &to_transfer);
        VkBufferImageCopy texture_copy = {
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
            .imageExtent = {2, 2, 1},
        };
        vkCmdCopyBufferToImage(command_buffer, texture_staging, texture_image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1, &texture_copy);
        VkImageMemoryBarrier to_sample = to_transfer;
        to_sample.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        to_sample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        to_sample.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_sample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                             0, NULL, 0, NULL, 1, &to_sample);
        VkClearValue clear_value = {
            .color.float32 = {0.10f, 0.75f, 0.25f, 1.0f},
        };
        VkRenderPassBeginInfo render_begin = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = render_pass,
            .framebuffer = framebuffer,
            .renderArea.extent = env.swapchain_info.imageExtent,
            .clearValueCount = 1,
            .pClearValues = &clear_value,
        };
        vkCmdBeginRenderPass(command_buffer, &render_begin,
                             VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline);
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout, 0, 1, &descriptor_set, 0,
                                NULL);
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

    VkSemaphore present_semaphore = VK_NULL_HANDLE;
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkResult semaphore_status = vkCreateSemaphore(
            env.device, &semaphore_info, NULL, &present_semaphore);
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &present_semaphore,
    };
    VkResult submit_status = record_status == VK_SUCCESS
            && semaphore_status == VK_SUCCESS
            ? vkQueueSubmit(env.queue, 1, &submit_info, VK_NULL_HANDLE)
            : VK_ERROR_INITIALIZATION_FAILED;
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &present_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &env.swapchain,
        .pImageIndices = &image_index,
    };
    VkResult present_status = submit_status == VK_SUCCESS
            ? vkQueuePresentKHR(env.queue, &present_info)
            : VK_ERROR_INITIALIZATION_FAILED;
    XSync(env.display, False);

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
    pthread_t watchdog;
    timeline_present_done = 0;
    if (pthread_create(&watchdog, NULL, timeline_present_watchdog, NULL) == 0)
        pthread_detach(watchdog);
    if (timeline_wait_status == VK_SUCCESS && present_status == VK_SUCCESS) {
        VkPresentInfoKHR idle_present = present_info;
        idle_present.waitSemaphoreCount = 0;
        idle_present.pWaitSemaphores = NULL;
        vkQueuePresentKHR(env.queue, &idle_present);
    }
    VkSemaphoreSignalInfo signal_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
        .semaphore = timeline,
        .value = 1,
    };
    VkResult timeline_signal = timeline_wait_status == VK_SUCCESS
            ? vkSignalSemaphore(env.device, &signal_info)
            : timeline_wait_status;
    timeline_present_done = 1;
    snprintf(env.details, sizeof(env.details),
             "status=%d acquire=%d record=%d semaphore=%d submit=%d index=%u "
             "bind2=null background=26,191,64 triangle=230,20,10 "
             "timeline-present=%d signal=%d",
             present_status, acquire_status, record_status, semaphore_status,
             submit_status, image_index, timeline_wait_status, timeline_signal);
    result(&env, "vulkan-present",
           acquire_status == VK_SUCCESS && image_index < env.image_count
                   && record_status == VK_SUCCESS
                   && semaphore_status == VK_SUCCESS
                   && submit_status == VK_SUCCESS
                   && present_status == VK_SUCCESS
                   && timeline_create == VK_SUCCESS
                   && timeline_wait_status == VK_SUCCESS
                   && timeline_signal == VK_SUCCESS);

    if (present_status == VK_SUCCESS) {
        present_info.waitSemaphoreCount = 0;
        present_info.pWaitSemaphores = NULL;
        for (unsigned frame = 0; frame < 8; ++frame) {
            vkQueuePresentKHR(env.queue, &present_info);
            XSync(env.display, False);
            usleep(20000);
        }
        usleep(5000000);
    }

    if (env.device != VK_NULL_HANDLE) vkDeviceWaitIdle(env.device);
    if (timeline != VK_NULL_HANDLE)
        vkDestroySemaphore(env.device, timeline, NULL);
    if (present_semaphore != VK_NULL_HANDLE)
        vkDestroySemaphore(env.device, present_semaphore, NULL);
    if (command_pool != VK_NULL_HANDLE)
        vkDestroyCommandPool(env.device, command_pool, NULL);
    if (pipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(env.device, pipeline, NULL);
    if (pipeline_layout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(env.device, pipeline_layout, NULL);
    if (descriptor_pool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(env.device, descriptor_pool, NULL);
    if (set_layout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(env.device, set_layout, NULL);
    if (vert_module != VK_NULL_HANDLE)
        vkDestroyShaderModule(env.device, vert_module, NULL);
    if (frag_module != VK_NULL_HANDLE)
        vkDestroyShaderModule(env.device, frag_module, NULL);
    if (framebuffer != VK_NULL_HANDLE)
        vkDestroyFramebuffer(env.device, framebuffer, NULL);
    if (render_pass != VK_NULL_HANDLE)
        vkDestroyRenderPass(env.device, render_pass, NULL);
    if (image_view != VK_NULL_HANDLE)
        vkDestroyImageView(env.device, image_view, NULL);
    if (vertex_buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(env.device, vertex_buffer, NULL);
    if (vertex_memory != VK_NULL_HANDLE)
        vkFreeMemory(env.device, vertex_memory, NULL);
    if (index_buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(env.device, index_buffer, NULL);
    if (index_memory != VK_NULL_HANDLE)
        vkFreeMemory(env.device, index_memory, NULL);
    if (uniform_buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(env.device, uniform_buffer, NULL);
    if (uniform_memory != VK_NULL_HANDLE)
        vkFreeMemory(env.device, uniform_memory, NULL);
    if (texture_sampler != VK_NULL_HANDLE)
        vkDestroySampler(env.device, texture_sampler, NULL);
    if (texture_view != VK_NULL_HANDLE)
        vkDestroyImageView(env.device, texture_view, NULL);
    if (texture_image != VK_NULL_HANDLE)
        vkDestroyImage(env.device, texture_image, NULL);
    if (texture_memory != VK_NULL_HANDLE)
        vkFreeMemory(env.device, texture_memory, NULL);
    if (texture_staging != VK_NULL_HANDLE)
        vkDestroyBuffer(env.device, texture_staging, NULL);
    if (texture_staging_memory != VK_NULL_HANDLE)
        vkFreeMemory(env.device, texture_staging_memory, NULL);
    probe_env_destroy(&env);
    printf("BXSUMMARY vulkan-present passed=%u failed=%u\n",
           env.passed, env.failed);
    return env.failed ? 1 : 0;
}
