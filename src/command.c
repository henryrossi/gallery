#include "engine.h"
#include "glyph.h"

#include <stdio.h>

static VkCommandBuffer begin_single_time_commands(VkDevice device,
                                                  VkCommandPool pool) {
        VkCommandBufferAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandPool = pool,
                .commandBufferCount = 1,
        };

        VkCommandBuffer cmd_buffer;
        vkAllocateCommandBuffers(device, &alloc_info, &cmd_buffer);

        VkCommandBufferBeginInfo begin_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(cmd_buffer, &begin_info);

        return cmd_buffer;
}

static void end_single_time_commands(VkDevice device, VkQueue graphics_queue,
                                     VkCommandPool pool,
                                     VkCommandBuffer buffer) {
        vkEndCommandBuffer(buffer);

        VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &buffer,
        };
        vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);

        vkQueueWaitIdle(graphics_queue);

        vkFreeCommandBuffers(device, pool, 1, &buffer);
}

// Creates a command pool. Returns 1 on success, 0 on failure.
static int create_command_pool(GlyphEngine *engine) {
        queue_family_indicies_t indicies
            = find_queue_families(engine, engine->physical_device);

        VkCommandPoolCreateInfo createinfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = indicies.graphics.index,
        };

        VkResult res = vkCreateCommandPool(engine->device, &createinfo, NULL,
                                           &engine->command_pool);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create command pool: %s\n",
                        string_VkResult(res));
                return 0;
        }

        return 1;
}

// Create command buffer. Return 1 on success, 0 on failure.
static int create_command_buffer(GlyphEngine *engine) {
        VkCommandBufferAllocateInfo allocinfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = engine->command_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
        };

        VkResult res = vkAllocateCommandBuffers(engine->device, &allocinfo,
                                                engine->command_buffer);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to allocate command buffers: %s\n",
                        string_VkResult(res));
                return 0;
        }

        return 1;
}
