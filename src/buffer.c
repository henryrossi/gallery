#include "glyph.h"
#include "vulkan/vulkan_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int tex_w = 64;
const int tex_h = 64;

typedef struct {
        float pos[2];
        float tex_coord[2];
} vertex;

const vertex vertices[] = {
        { { -1.0f, -1.0f }, { 0.0f, 0.0f } },
        { { 0.5f, -1.0f }, { 1.0f, 0.0f } },
        { { 0.5f, 1.0f }, { 1.0f, 1.0f } },
        { { -1.0f, 1.0f }, { 0.0f, 1.0f } },
};

const uint16_t indices[] = {
        0, 1, 2, 2, 3, 0,
};

static VkVertexInputBindingDescription get_vertex_binding_desc(void) {
        return (VkVertexInputBindingDescription){
                .binding = 0,
                .stride = sizeof(vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
}

static VkVertexInputAttributeDescription get_vertex_attr_desc_pos(void) {
        return (VkVertexInputAttributeDescription){
                .binding = 0,
                .location = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(vertex, pos),
        };
}

static VkVertexInputAttributeDescription get_vertex_attr_desc_tex_coord(void) {
        return (VkVertexInputAttributeDescription){
                .binding = 0,
                .location = 1,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(vertex, tex_coord),
        };
}

// ?
int64_t find_memory_type(VkPhysicalDevice phy_device, uint32_t type_filter,
                         VkMemoryPropertyFlags props) {
        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(phy_device, &mem_props);

        for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
                if (type_filter & (1 << i)
                    && (mem_props.memoryTypes[i].propertyFlags & props)
                           == props) {
                        return i;
                }
        }

        return -1;
}

typedef struct {
        VkDevice device;
        VkPhysicalDevice physical_device;
        VkDeviceSize size;
        VkBufferUsageFlags usage;
        VkMemoryPropertyFlags props;
        VkBuffer *buffer;
        VkDeviceMemory *memory;
} buffer_create_info;

// Create a buffer. Returns 1 on success, 0 on failure.
static int create_buffer(buffer_create_info params) {
        VkBufferCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = params.size,
                .usage = params.usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        VkResult res
            = vkCreateBuffer(params.device, &create_info, NULL, params.buffer);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create buffer: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkMemoryRequirements mem_requirements;
        vkGetBufferMemoryRequirements(params.device, *params.buffer,
                                      &mem_requirements);

        int64_t mem_type = find_memory_type(
            params.physical_device, mem_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (mem_type < 0) {
                fprintf(stderr, "Failed to find suitable memory type\n");
                return 0;
        }

        VkMemoryAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = mem_requirements.size,
                .memoryTypeIndex = mem_type,
        };

        res = vkAllocateMemory(params.device, &alloc_info, NULL, params.memory);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to allocate buffer memory: %s\n",
                        string_VkResult(res));
                return 0;
        }

        vkBindBufferMemory(params.device, *params.buffer, *params.memory, 0);

        return 1;
}

typedef struct {
        VkDevice device;
        VkQueue graphics_queue;
        VkCommandPool cmdpool;
        VkBuffer src;
        VkBuffer dst;
        VkDeviceSize size;
} copy_buffer_info;

static void copy_buffer(copy_buffer_info params) {
        VkCommandBuffer cmd_buffer
            = begin_single_time_commands(params.device, params.cmdpool);

        VkBufferCopy copy_region = {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = params.size,
        };
        vkCmdCopyBuffer(cmd_buffer, params.src, params.dst, 1, &copy_region);

        end_single_time_commands(params.device, params.graphics_queue,
                                 params.cmdpool, cmd_buffer);
}

// Create vertex buffers. Returns 1 on success, 0 on failure.
static int create_vertex_buffer(glyph_state *state) {
        VkDevice device = state->device;
        VkDeviceSize size = sizeof(vertices);

        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
        buffer_create_info staging = {
                .device = device,
                .physical_device = state->physical_device,
                .size = size,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                .buffer = &staging_buffer,
                .memory = &staging_buffer_memory,
        };
        if (!create_buffer(staging)) {
                return 0;
        }

        void *data;
        vkMapMemory(device, staging_buffer_memory, 0, size, 0, &data);
        memcpy(data, vertices, size);
        vkUnmapMemory(device, staging_buffer_memory);

        buffer_create_info vertex = {
                .device = device,
                .physical_device = state->physical_device,
                .size = size,
                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                         | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .buffer = &state->vertex_buffer,
                .memory = &state->vertex_buffer_memory,
        };
        if (!create_buffer(vertex)) {
                return 0;
        }

        copy_buffer_info params = {
                .device = device,
                .graphics_queue = state->graphics_queue,
                .cmdpool = state->command_pool,
                .src = staging_buffer,
                .dst = state->vertex_buffer,
                .size = size,
        };
        copy_buffer(params);

        vkDestroyBuffer(device, staging_buffer, NULL);
        vkFreeMemory(device, staging_buffer_memory, NULL);

        return 1;
}

// Creates index buffer. Returns 1 on success, 0 on failure.
static int create_index_buffer(glyph_state *state) {
        VkDeviceSize size = sizeof(indices);

        VkBuffer staging_buffer;
        VkDeviceMemory staging_memory;

        buffer_create_info staging_params = {
                .size = size,
                .buffer = &staging_buffer,
                .memory = &staging_memory,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                .device = state->device,
                .physical_device = state->physical_device,
        };
        if (!create_buffer(staging_params)) {
                return 0;
        }

        void *data;
        vkMapMemory(state->device, staging_memory, 0, size, 0, &data);
        memcpy(data, indices, size);
        vkUnmapMemory(state->device, staging_memory);

        buffer_create_info index_params = {
                .size = size,
                .buffer = &state->index_buffer,
                .memory = &state->index_buffer_memory,
                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                         | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                .props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .device = state->device,
                .physical_device = state->physical_device,
        };
        if (!create_buffer(index_params)) {
                return 0;
        }

        copy_buffer_info copy_params = {
                .size = size,
                .src = staging_buffer,
                .dst = state->index_buffer,
                .device = state->device,
                .cmdpool = state->command_pool,
                .graphics_queue = state->graphics_queue,
        };
        copy_buffer(copy_params);

        vkDestroyBuffer(state->device, staging_buffer, NULL);
        vkFreeMemory(state->device, staging_memory, NULL);

        return 1;
}

typedef struct {
        uint32_t width;
        uint32_t height;
        VkFormat format;
        VkImageTiling tiling;
        VkImageUsageFlags usage;
        VkMemoryPropertyFlags props;
        VkImage *image;
        VkDeviceMemory *image_memory;
        VkDevice device;
        VkPhysicalDevice physical_device;
} image_create_info;

// Creates an image. Returns 1 on success, 0 on failure.
static int create_image(image_create_info *params) {
        VkImageCreateInfo image_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .extent = { .width = params->width,
                            .height = params->height,
                            .depth = 1 },
                .mipLevels = 1,
                .arrayLayers = 1,
                .format = params->format,
                .tiling = params->tiling,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .usage = params->usage,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VkResult res
            = vkCreateImage(params->device, &image_info, NULL, params->image);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create image: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkMemoryRequirements mem_reqs;
        vkGetImageMemoryRequirements(params->device, *params->image, &mem_reqs);

        VkMemoryAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = mem_reqs.size,
                .memoryTypeIndex
                = find_memory_type(params->physical_device,
                                   mem_reqs.memoryTypeBits, params->props),
        };

        res = vkAllocateMemory(params->device, &alloc_info, NULL,
                               params->image_memory);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to allocate image memory: %s\n",
                        string_VkResult(res));
                return 0;
        }

        vkBindImageMemory(params->device, *params->image, *params->image_memory,
                          0);

        return 1;
}

typedef struct {
        VkBuffer buffer;
        VkImage image;
        uint32_t width;
        uint32_t height;
        VkDevice device;
        VkCommandPool cmdpool;
        VkQueue graphics_queue;
} buffer_to_image_info;

void copy_buffer_to_image(buffer_to_image_info *params) {
        VkCommandBuffer cmd_buffer
            = begin_single_time_commands(params->device, params->cmdpool);

        VkBufferImageCopy region = {
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                },
                .imageOffset = { 0, 0, 0},
                .imageExtent = { params->width, params->height, 1 },
        };
        vkCmdCopyBufferToImage(cmd_buffer, params->buffer, params->image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &region);

        end_single_time_commands(params->device, params->graphics_queue,
                                 params->cmdpool, cmd_buffer);
}

typedef struct {
        VkImage image;
        VkFormat format;
        VkImageLayout old_layout;
        VkImageLayout new_layout;
        VkDevice device;
        VkCommandPool cmdpool;
        VkQueue graphics_queue;
} transition_image_layout_info;

void transition_image_layout(transition_image_layout_info *params) {
        VkCommandBuffer cmd_buffer
            = begin_single_time_commands(params->device, params->cmdpool);

        VkImageMemoryBarrier barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .oldLayout = params->old_layout,
                .newLayout = params->new_layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params->image,
                .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                },
                .srcAccessMask = 0,
                .dstAccessMask = 0,
        };

        VkPipelineStageFlags srcStage = 0;
        VkPipelineStageFlags dstStage = 0;

        if (params->old_layout == VK_IMAGE_LAYOUT_UNDEFINED
            && params->new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (params->old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                   && params->new_layout
                          == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
                fprintf(stderr, "Unsupported layout transition!\n");
        }

        vkCmdPipelineBarrier(cmd_buffer, srcStage, dstStage, 0, 0, NULL, 0,
                             NULL, 1, &barrier);

        end_single_time_commands(params->device, params->graphics_queue,
                                 params->cmdpool, cmd_buffer);
}

static int create_image_view(VkDevice device, VkImage image, VkFormat format,
                             VkImageView *view) {
        VkImageViewCreateInfo viewinfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = format,
                .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                },
        };
        VkResult res = vkCreateImageView(device, &viewinfo, NULL, view);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create image view: %s",
                        string_VkResult(res));
                return 0;
        }

        return 1;
}

static int create_texture_image_view(glyph_state *state) {
        if (!create_image_view(state->device, state->texture_image,
                               VK_FORMAT_R8G8B8A8_SRGB, &state->texture_view)) {
                return 0;
        }
        return 1;
}

// Creates a texture image. Returns 1 on success, 0 on failure.
static int create_texture_image(glyph_state *state) {
        VkDeviceSize size = tex_w * tex_h * 4;

        // read image data from disk
        uint8_t *pixels = calloc(size, sizeof(uint8_t));
        for (int y = 0; y < tex_h; y++) {
                pixels[y * tex_h + tex_w] = (uint8_t)y;
        }

        VkBuffer staging_buffer;
        VkDeviceMemory staging_memory;

        buffer_create_info params = {
                .size = size,
                .buffer = &staging_buffer,
                .memory = &staging_memory,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                .device = state->device,
                .physical_device = state->physical_device,
        };
        if (!create_buffer(params)) {
                return 0;
        }

        void *data;
        vkMapMemory(state->device, staging_memory, 0, size, 0, &data);
        memcpy(data, pixels, size);
        vkUnmapMemory(state->device, staging_memory);

        image_create_info image_info = {
                .width = tex_w,
                .height = tex_h,
                .format = VK_FORMAT_R8G8B8A8_SRGB,
                .tiling = VK_IMAGE_TILING_LINEAR,
                .usage
                = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .image = &state->texture_image,
                .image_memory = &state->texture_memory,
                .device = state->device,
                .physical_device = state->physical_device,
        };
        if (!create_image(&image_info)) {
                return 0;
        }

        transition_image_layout_info trans_info = {
                .image = state->texture_image,
                .format = VK_FORMAT_R8G8B8A8_SRGB,
                .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .device = state->device,
                .cmdpool = state->command_pool,
                .graphics_queue = state->graphics_queue,
        };
        transition_image_layout(&trans_info);

        buffer_to_image_info copy_info = {
                .buffer = staging_buffer,
                .image = state->texture_image,
                .width = tex_w,
                .height = tex_h,
                .device = state->device,
                .cmdpool = state->command_pool,
                .graphics_queue = state->graphics_queue,
        };
        copy_buffer_to_image(&copy_info);

        trans_info.old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        trans_info.new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        transition_image_layout(&trans_info);

        vkDestroyBuffer(state->device, staging_buffer, NULL);
        vkFreeMemory(state->device, staging_memory, NULL);

        return 1;
}

static int create_texture_sampler(glyph_state *state) {
        VkSamplerCreateInfo sampler_info = {
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = 0,
                .minFilter = 0,
                .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .anisotropyEnable = VK_FALSE,
                .maxAnisotropy = 1.0f,
                .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
                .unnormalizedCoordinates = VK_FALSE,
                .compareEnable = VK_FALSE,
                .compareOp = VK_COMPARE_OP_ALWAYS,
                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                .mipLodBias = 0.0f,
                .minLod = 0.0f,
                .maxLod = 0.0f,
        };

        VkResult res = vkCreateSampler(state->device, &sampler_info, NULL,
                                       &state->texture_sampler);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create texture sampler: %s\n",
                        string_VkResult(res));
                return 0;
        }
        return 1;
}

static int create_descriptor_set_layout(glyph_state *state) {
        VkDescriptorSetLayoutBinding tex_layout_binding = {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL,
        };

        VkDescriptorSetLayoutCreateInfo layout_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = 1,
                .pBindings = &tex_layout_binding,
        };

        VkResult res = vkCreateDescriptorSetLayout(
            state->device, &layout_info, NULL, &state->descriptor_set_layout);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create descriptor set layout: %s\n",
                        string_VkResult(res));
                return 0;
        }
        return 1;
}

static int create_descriptor_pool(glyph_state *state) {
        VkDescriptorPoolSize pool_size = {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = MAX_FRAMES_IN_FLIGHT,
        };

        VkDescriptorPoolCreateInfo pool_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .poolSizeCount = 1,
                .pPoolSizes = &pool_size,
                .maxSets = MAX_FRAMES_IN_FLIGHT,
        };

        VkResult res = vkCreateDescriptorPool(state->device, &pool_info, NULL,
                                              &state->descriptor_pool);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create descriptor pool: %s\n",
                        string_VkResult(res));
                return 0;
        }

        return 1;
}

static int create_descriptor_sets(glyph_state *state) {
        VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                layouts[i] = state->descriptor_set_layout;
        }

        VkDescriptorSetAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = state->descriptor_pool,
                .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
                .pSetLayouts = layouts,
        };

        VkResult res = vkAllocateDescriptorSets(state->device, &alloc_info,
                                                state->descriptor_sets);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to allocate descriptor sets: %s\n",
                        string_VkResult(res));
                return 0;
        }

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                VkDescriptorImageInfo image_info = {
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        .imageView = state->texture_view,
                        .sampler = state->texture_sampler,
                };

                VkWriteDescriptorSet descriptor_write = {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = state->descriptor_sets[i],
                        .dstBinding = 0,
                        .dstArrayElement = 0,
                        .descriptorType
                        = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .descriptorCount = 1,
                        .pBufferInfo = 0,
                        .pImageInfo = &image_info,
                        .pTexelBufferView = 0,
                };
                vkUpdateDescriptorSets(state->device, 1, &descriptor_write, 0,
                                       NULL);
        }

        return 1;
}
