#include "buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const uint16_t indices[] = {
        0, 1, 2, 2, 3, 0,
};
//
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

static int createUniformBuffer(UniformBufferCreateInfo *createInfo) {
        VkDeviceSize size = createInfo->uniformSize;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                BufferCreateInfo bufferInfo = {
                        .device = createInfo->device,
                        .buffer = createInfo->pBuffers[i],
                        .memory = createInfo->pMemory[i],
                        .size = size,
                        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        .props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                 | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        .physical_device = createInfo->phyDevice,
                };
                if (!createBuffer(&bufferInfo)) {
                        return 0;
                }

                vkMapMemory(createInfo->device, *createInfo->pMemory[i], 0,
                            size, 0, createInfo->pMappedMemory[i]);
        }

        return 1;
}
