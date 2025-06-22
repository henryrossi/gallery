#include "buffer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
        float pos[2];
        float tex_coord[2];
} canvasVertex;

const canvasVertex vertices[] = {
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
                .stride = sizeof(canvasVertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
}

static VkVertexInputAttributeDescription get_vertex_attr_desc_pos(void) {
        return (VkVertexInputAttributeDescription){
                .binding = 0,
                .location = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(canvasVertex, pos),
        };
}

static VkVertexInputAttributeDescription get_vertex_attr_desc_tex_coord(void) {
        return (VkVertexInputAttributeDescription){
                .binding = 0,
                .location = 1,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(canvasVertex, tex_coord),
        };
}

typedef struct {
        float xAdjustment;
        float yAdjustment;
} uniformBufferObject;

// ?
int64_t findMemoryType(VkPhysicalDevice phyDevice, uint32_t typeFilter,
                       VkMemoryPropertyFlags props) {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(phyDevice, &memProps);

        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
                if (typeFilter & (1 << i)
                    && (memProps.memoryTypes[i].propertyFlags & props)
                           == props) {
                        return i;
                }
        }

        return -1;
}

// Create a buffer. Returns 1 on success, 0 on failure.
static int createBuffer(BufferCreateInfo *createInfo) {
        VkBuffer *pBuffer = createInfo->buffer;
        VkDeviceMemory *pMemory = createInfo->memory;
        VkDevice device = createInfo->device;
        VkPhysicalDevice physicalDevice = createInfo->physical_device;

        VkBufferCreateInfo vkCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = createInfo->size,
                .usage = createInfo->usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        VkResult res = vkCreateBuffer(device, &vkCreateInfo, NULL, pBuffer);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create buffer: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkMemoryRequirements mem_requirements;
        vkGetBufferMemoryRequirements(device, *pBuffer, &mem_requirements);

        int64_t mem_type
            = findMemoryType(physicalDevice, mem_requirements.memoryTypeBits,
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

        res = vkAllocateMemory(device, &alloc_info, NULL, pMemory);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to allocate buffer memory: %s\n",
                        string_VkResult(res));
                return 0;
        }

        vkBindBufferMemory(device, *pBuffer, *pMemory, 0);

        return 1;
}

static void copyBuffer(CopyBufferInfo *copyInfo) {
        VkCommandBuffer cmdBuffer
            = begin_single_time_commands(copyInfo->device, copyInfo->cmdpool);

        VkBufferCopy copyRegion = {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = copyInfo->size,
        };
        vkCmdCopyBuffer(cmdBuffer, copyInfo->src, copyInfo->dst, 1,
                        &copyRegion);

        end_single_time_commands(copyInfo->device, copyInfo->graphics_queue,
                                 copyInfo->cmdpool, cmdBuffer);
}

// Create vertex buffers. Returns 1 on success, 0 on failure.
static int create_vertex_buffer(glyph_state *state) {
        VkDevice device = state->device;
        VkDeviceSize size = sizeof(vertices);

        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
        BufferCreateInfo staging = {
                .device = device,
                .physical_device = state->physical_device,
                .size = size,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                .buffer = &staging_buffer,
                .memory = &staging_buffer_memory,
        };
        if (!createBuffer(&staging)) {
                return 0;
        }

        void *data;
        vkMapMemory(device, staging_buffer_memory, 0, size, 0, &data);
        memcpy(data, vertices, size);
        vkUnmapMemory(device, staging_buffer_memory);

        BufferCreateInfo vertex = {
                .device = device,
                .physical_device = state->physical_device,
                .size = size,
                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                         | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .buffer = &state->vertex_buffer,
                .memory = &state->vertex_buffer_memory,
        };
        if (!createBuffer(&vertex)) {
                return 0;
        }

        CopyBufferInfo params = {
                .device = device,
                .graphics_queue = state->graphics_queue,
                .cmdpool = state->command_pool,
                .src = staging_buffer,
                .dst = state->vertex_buffer,
                .size = size,
        };
        copyBuffer(&params);

        vkDestroyBuffer(device, staging_buffer, NULL);
        vkFreeMemory(device, staging_buffer_memory, NULL);

        return 1;
}

// Creates index buffer. Returns 1 on success, 0 on failure.
static int create_index_buffer(glyph_state *state) {
        VkDeviceSize size = sizeof(indices);

        VkBuffer staging_buffer;
        VkDeviceMemory staging_memory;

        BufferCreateInfo staging_params = {
                .size = size,
                .buffer = &staging_buffer,
                .memory = &staging_memory,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                .device = state->device,
                .physical_device = state->physical_device,
        };
        if (!createBuffer(&staging_params)) {
                return 0;
        }

        void *data;
        vkMapMemory(state->device, staging_memory, 0, size, 0, &data);
        memcpy(data, indices, size);
        vkUnmapMemory(state->device, staging_memory);

        BufferCreateInfo index_params = {
                .size = size,
                .buffer = &state->index_buffer,
                .memory = &state->index_buffer_memory,
                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                         | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                .props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .device = state->device,
                .physical_device = state->physical_device,
        };
        if (!createBuffer(&index_params)) {
                return 0;
        }

        CopyBufferInfo copy_params = {
                .size = size,
                .src = staging_buffer,
                .dst = state->index_buffer,
                .device = state->device,
                .cmdpool = state->command_pool,
                .graphics_queue = state->graphics_queue,
        };
        copyBuffer(&copy_params);

        vkDestroyBuffer(state->device, staging_buffer, NULL);
        vkFreeMemory(state->device, staging_memory, NULL);

        return 1;
}

// Creates an image. Returns 1 on success, 0 on failure.
static int createImage(ImageCreateInfo *createInfo) {
        VkImageCreateInfo image_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .extent = { .width = createInfo->width,
                            .height = createInfo->height,
                            .depth = 1 },
                .mipLevels = 1,
                .arrayLayers = 1,
                .format = createInfo->format,
                .tiling = createInfo->tiling,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .usage = createInfo->usage,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VkResult res = vkCreateImage(createInfo->device, &image_info, NULL,
                                     createInfo->image);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create image: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(createInfo->device, *createInfo->image,
                                     &memReqs);

        VkMemoryAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = memReqs.size,
                .memoryTypeIndex
                = findMemoryType(createInfo->physicalDevice,
                                 memReqs.memoryTypeBits, createInfo->props),
        };

        res = vkAllocateMemory(createInfo->device, &alloc_info, NULL,
                               createInfo->imageMemory);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to allocate image memory: %s\n",
                        string_VkResult(res));
                return 0;
        }

        vkBindImageMemory(createInfo->device, *createInfo->image,
                          *createInfo->imageMemory, 0);

        return 1;
}

void copyBufferToImage(CopyBufferToImageInfo *copyInfo) {
        VkCommandBuffer cmd_buffer
            = begin_single_time_commands(copyInfo->device, copyInfo->cmdPool);

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
                .imageExtent = { copyInfo->width, copyInfo->height, 1 },
        };
        vkCmdCopyBufferToImage(cmd_buffer, copyInfo->buffer, copyInfo->image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &region);

        end_single_time_commands(copyInfo->device, copyInfo->graphicsQueue,
                                 copyInfo->cmdPool, cmd_buffer);
}

void transitionImageLayout(TransitionImageLayoutInfo *params) {
        VkCommandBuffer cmd_buffer
            = begin_single_time_commands(params->device, params->cmdPool);

        VkImageMemoryBarrier barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .oldLayout = params->oldLayout,
                .newLayout = params->newLayout,
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

        if (params->oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
            && params->newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (params->oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                   && params->newLayout
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

        end_single_time_commands(params->device, params->graphicsQueue,
                                 params->cmdPool, cmd_buffer);
}

static int createImageView(VkDevice device, VkImage image, VkFormat format,
                           VkImageView *view) {
        VkImageViewCreateInfo viewInfo = {
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
        VkResult res = vkCreateImageView(device, &viewInfo, NULL, view);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create image view: %s",
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

        VkDescriptorSetLayoutBinding ubo_layout_binding = {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .pImmutableSamplers = NULL,
        };

        VkDescriptorSetLayoutBinding layout_bindings[2] = {
                tex_layout_binding,
                ubo_layout_binding,
        };

        VkDescriptorSetLayoutCreateInfo layout_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = 2,
                .pBindings = layout_bindings,
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
        VkDescriptorPoolSize pool_sizes[MAX_FRAMES_IN_FLIGHT] = {
                {
                        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .descriptorCount = MAX_FRAMES_IN_FLIGHT,
                },
                {
                        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        .descriptorCount = MAX_FRAMES_IN_FLIGHT,
                },
        };

        VkDescriptorPoolCreateInfo pool_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .poolSizeCount = 2,
                .pPoolSizes = pool_sizes,
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
                        .imageView = state->canvas.imageView[i],
                        .sampler = state->canvas.imageSampler,
                };

                VkDescriptorBufferInfo buffer_info = {
                        .buffer = state->uniform_buffers[i],
                        .offset = 0,
                        .range = sizeof(uniformBufferObject),
                };

                VkWriteDescriptorSet descriptor_write[2] = {
                        {
                                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                .dstSet = state->descriptor_sets[i],
                                .dstBinding = 0,
                                .dstArrayElement = 0,
                                .descriptorType
                                = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                .descriptorCount = 1,
                                .pImageInfo = &image_info,
                        },
                        {
                                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                .dstSet = state->descriptor_sets[i],
                                .dstBinding = 1,
                                .dstArrayElement = 0,
                                .descriptorType
                                = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                .descriptorCount = 1,
                                .pBufferInfo = &buffer_info,
                        },
                };

                vkUpdateDescriptorSets(state->device, 2, descriptor_write, 0,
                                       NULL);
        }

        return 1;
}

static int create_uniform_buffer(glyph_state *state) {
        VkDeviceSize size = sizeof(uniformBufferObject);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                BufferCreateInfo params = {
                        .size = size,
                        .buffer = &state->uniform_buffers[i],
                        .memory = &state->uniform_buffers_memory[i],
                        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        .props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                 | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        .device = state->device,
                        .physical_device = state->physical_device,
                };
                if (!createBuffer(&params)) {
                        return 0;
                }

                vkMapMemory(state->device, state->uniform_buffers_memory[i], 0,
                            size, 0, &state->uniform_buffers_mapped[i]);
                update_uniform_buffer(state, i);
        }

        return 1;
}

static double convertVulkanScreenPosToGLFW(double pos, int windowLength) {
        return ((pos + 1.0f) / 2) * windowLength;
}

static void update_uniform_buffer(glyph_state *state, uint32_t currentFrame) {
        uniformBufferObject ubo = { 1.0, 1.0 };

        float drawingAreaWidthProportion
            = (vertices[1].pos[0] - vertices[0].pos[0]) / 2;

        float drawingAreaHeightProportion
            = (vertices[2].pos[1] - vertices[1].pos[1]) / 2;

        float surfaceWidth = state->swapchain_extent.width;
        surfaceWidth *= drawingAreaWidthProportion;

        float surfaceHeight = state->swapchain_extent.height;
        surfaceHeight *= drawingAreaHeightProportion;

        // this only works for square canvas sizes
        if (surfaceWidth > surfaceHeight) {
                ubo.xAdjustment = surfaceHeight / surfaceWidth;
        } else {
                ubo.yAdjustment = surfaceWidth / surfaceHeight;
        }

        memcpy(state->uniform_buffers_mapped[currentFrame], &ubo, sizeof(ubo));

        int w, h;
        glfwGetWindowSize(state->window, &w, &h);
        // update Canvas' window position data
        state->canvas.windowX = convertVulkanScreenPosToGLFW(
            vertices[0].pos[0] * ubo.xAdjustment, w);
        state->canvas.windowY = convertVulkanScreenPosToGLFW(
            vertices[0].pos[1] * ubo.yAdjustment, h);
        state->canvas.windowWidth
            = convertVulkanScreenPosToGLFW(vertices[1].pos[0] * ubo.xAdjustment,
                                           w)
              - convertVulkanScreenPosToGLFW(
                  vertices[0].pos[0] * ubo.xAdjustment, w);
        state->canvas.windowHeight
            = convertVulkanScreenPosToGLFW(vertices[2].pos[1] * ubo.yAdjustment,
                                           h)
              - convertVulkanScreenPosToGLFW(
                  vertices[1].pos[1] * ubo.yAdjustment, h);
}
