#include "sync.h"
#include "vulkan/vulkan_core.h"

#include <stdio.h>

// Create semaphores and fences. Returns 1 on success, 0 on failure.
static int create_sync_objects(GlyphEngine *engine) {
        VkSemaphoreCreateInfo semaphore_info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        VkFenceCreateInfo fence_info = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                VkResult res
                    = vkCreateSemaphore(engine->device, &semaphore_info, NULL,
                                        engine->image_available_semaphore + i);
                if (res != VK_SUCCESS) {
                        fprintf(
                            stderr,
                            "Failed to create image available semaphore: %s\n",
                            string_VkResult(res));
                        return 0;
                }
                res = vkCreateSemaphore(engine->device, &semaphore_info, NULL,
                                        engine->render_finished_semaphore + i);
                if (res != VK_SUCCESS) {
                        fprintf(
                            stderr,
                            "Failed to create render finished semaphore: %s\n",
                            string_VkResult(res));
                        return 0;
                }
                res = vkCreateFence(engine->device, &fence_info, NULL,
                                    engine->inflight_fence + i);
                if (res != VK_SUCCESS) {
                        fprintf(stderr,
                                "Failed to create in flight fence: %s\n",
                                string_VkResult(res));
                        return 0;
                }
        }

        return 1;
}

static void cleanUnsafeSemaphore(VkQueue queue, VkSemaphore *semaphore) {
        const VkPipelineStageFlags dstStage
            = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        VkSubmitInfo submitInfo = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = semaphore,
                .pWaitDstStageMask = &dstStage,

        };
        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
}
