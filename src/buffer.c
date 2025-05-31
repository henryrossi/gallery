#include "glyph.h"

#include <stdio.h>
#include <string.h>

typedef struct {
        float pos[2];
        float color[3];
} vertex;

const vertex vertices[] = {
        { { -0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f } },
        { { 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },
        { { 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
        { { -0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f } },
};

const uint16_t indices[] = {
        0, 1, 2, 2, 3, 0,
};

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
        VkCommandBufferAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandPool = params.cmdpool,
                .commandBufferCount = 1,
        };

        VkCommandBuffer cmd_buffer;
        vkAllocateCommandBuffers(params.device, &alloc_info, &cmd_buffer);

        VkCommandBufferBeginInfo begin_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(cmd_buffer, &begin_info);

        VkBufferCopy copy_region = {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = params.size,
        };
        vkCmdCopyBuffer(cmd_buffer, params.src, params.dst, 1, &copy_region);

        vkEndCommandBuffer(cmd_buffer);

        VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &cmd_buffer,
        };
        vkQueueSubmit(params.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(params.graphics_queue);

        vkFreeCommandBuffers(params.device, params.cmdpool, 1, &cmd_buffer);
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
