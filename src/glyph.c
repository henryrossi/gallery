#include "glyph.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "buffer.c"
#include "command.c"
#include "debug.c"
#include "device.c"
#include "graphics_pipeline.c"
#include "surface.c"
#include "swapchain.c"
#include "sync.c"

static queue_family_indicies_t find_queue_families(glyph_state *state,
                                                   VkPhysicalDevice device) {
        queue_family_indicies_t indicies = { 0 };

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                                 NULL);
        VkQueueFamilyProperties *queue_families
            = malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                                 queue_families);

        for (uint32_t i = 0; i < queue_family_count; i++) {
                if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                        indicies.graphics.index = i;
                        indicies.graphics.valid = 1;
                }
                uint32_t supports_presentation = 0;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, state->surface,
                                                     &supports_presentation);
                if (supports_presentation) {
                        indicies.presentation.index = i;
                        indicies.presentation.valid = 1;
                }
        }

        free(queue_families);

        return indicies;
}

typedef struct {
        const char **names;
        uint32_t count;
} extensions_t;

static extensions_t get_required_extensions(void) {
        extensions_t exts = { 0 };

        uint32_t glfw_ext_count = 0;
        const char **glfw_exts
            = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

        exts.count = enable_validation_layers ? glfw_ext_count + 3
                                              : glfw_ext_count + 2;
        exts.names = malloc(sizeof(const char *) * exts.count);

        for (int i = 0; i < glfw_ext_count; i++) {
                exts.names[i] = glfw_exts[i];
        }

        if (enable_validation_layers) {
                exts.names[exts.count - 3] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }
        exts.names[exts.count - 2]
            = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
        exts.names[exts.count - 1] = "VK_KHR_get_physical_device_properties2";

        return exts;
}

// Creates a Vulkan instance. Returns 1 on success, 0 on error.
static int create_instance(glyph_state *state) {
        VkApplicationInfo appinfo = {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName = "glyph",
                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                .pEngineName = "No Engine",
                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                .apiVersion = VK_API_VERSION_1_0,
        };

        extensions_t exts = get_required_extensions();
        VkInstanceCreateInfo createinfo = {
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pApplicationInfo = &appinfo,
                .enabledExtensionCount = exts.count,
                .ppEnabledExtensionNames = exts.names,
                .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
        };

        VkDebugUtilsMessengerCreateInfoEXT debug_createinfo = { 0 };
        if (enable_validation_layers) {
                if (!check_validation_layer_support()) {
                        return 0;
                }
                createinfo.enabledLayerCount = validation_layer_count;
                createinfo.ppEnabledLayerNames = validation_layers;
                populate_debug_messenger_createinfo(&debug_createinfo);
                createinfo.pNext = &debug_createinfo;
        } else {
                createinfo.enabledLayerCount = 0;
        }

        VkResult res = vkCreateInstance(&createinfo, NULL, &state->instance);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create instance. %s\n",
                        string_VkResult(res));
                return 0;
        }
        return 1;
}

// Write commands into the command buffer. Returns 1 on success, 0 on failure
static int record_command_buffer(glyph_state *state, VkCommandBuffer cmd_buffer,
                                 uint32_t image_index) {
        VkCommandBufferBeginInfo begininfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = 0,
                .pInheritanceInfo = NULL,
        };

        VkResult res = vkBeginCommandBuffer(cmd_buffer, &begininfo);
        if (res != VK_SUCCESS) {
                fprintf(stderr,
                        "Failed to being command buffer recording: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkClearValue clear_value = { { { 0.3f, 0.3f, 0.3f, 1.0f } } };
        VkRenderPassBeginInfo passinfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = state->render_pass,
                .framebuffer = state->swapchain_framebuffers[image_index],
                .renderArea.offset = { 0, 0 },
                .renderArea.extent = state->swapchain_extent,
                .clearValueCount = 1,
                .pClearValues = &clear_value,
        };

        vkCmdBeginRenderPass(cmd_buffer, &passinfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          state->graphics_pipeline);

        VkViewport viewport = {
                .x = 0.0f,
                .y = 0.0f,
                .width = (float)state->swapchain_extent.width,
                .height = (float)state->swapchain_extent.height,
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
        };
        vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

        VkRect2D scissor = {
                .offset = { 0, 0 },
                .extent = state->swapchain_extent,
        };
        vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

        VkBuffer vertex_buffers[] = { state->vertex_buffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd_buffer, 0, 1, vertex_buffers, offsets);
        vkCmdBindIndexBuffer(cmd_buffer, state->index_buffer, 0,
                             VK_INDEX_TYPE_UINT16);

        vkCmdBindDescriptorSets(
            cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state->pipeline_layout,
            0, 1, &state->descriptor_sets[state->current_frame], 0, NULL);

        uint32_t indices_size = sizeof(indices) / sizeof(indices[0]);
        vkCmdDrawIndexed(cmd_buffer, indices_size, 1, 0, 0, 0);
        vkCmdEndRenderPass(cmd_buffer);

        res = vkEndCommandBuffer(cmd_buffer);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to record command buffer: %s\n",
                        string_VkResult(res));
                return 0;
        }

        return 1;
}

// Submit a frame to be drawn. Returns 1 on success, 0 on failure.
static int draw_frame(glyph_state *state) {
        uint32_t current_frame = state->current_frame;
        VkDevice device = state->device;
        vkWaitForFences(device, 1, state->inflight_fence + current_frame,
                        VK_TRUE, UINT64_MAX);

        uint32_t image_index;
        VkResult res = vkAcquireNextImageKHR(
            device, state->swapchain, UINT64_MAX,
            state->image_available_semaphore[current_frame], VK_NULL_HANDLE,
            &image_index);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR
            || state->framebuffer_resized) {
                state->framebuffer_resized = 0;
                recreate_swapchain(state);
        } else if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to acquire swap chain image: %s\n",
                        string_VkResult(res));
                return 0;
        }

        vkResetFences(device, 1, state->inflight_fence + current_frame);

        vkResetCommandBuffer(state->command_buffer[current_frame], 0);
        record_command_buffer(state, state->command_buffer[current_frame],
                              image_index);

        VkPipelineStageFlags wait_stages[] = {
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        };
        VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores
                = state->image_available_semaphore + current_frame,
                .pWaitDstStageMask = wait_stages,
                .commandBufferCount = 1,
                .pCommandBuffers = state->command_buffer + current_frame,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores
                = state->render_finished_semaphore + current_frame,
        };

        res = vkQueueSubmit(state->graphics_queue, 1, &submit_info,
                            state->inflight_fence[current_frame]);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to sumbit draw command buffer: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkSwapchainKHR swapchains[] = { state->swapchain };
        VkPresentInfoKHR present_info = {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores
                = state->render_finished_semaphore + current_frame,
                .swapchainCount = 1,
                .pSwapchains = swapchains,
                .pImageIndices = &image_index,
                .pResults = NULL,
        };

        res = vkQueuePresentKHR(state->presentation_queue, &present_info);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
                recreate_swapchain(state);
        } else if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to present swap chain image: %s",
                        string_VkResult(res));
                return 0;
        }

        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

        return 1;
}

int main(int argc, char **argv) {

        glyph_state state = {
                .current_frame = 0,
                .physical_device = VK_NULL_HANDLE,
                .surface = VK_NULL_HANDLE,
                .framebuffer_resized = 0,
        };

        if (!init_window(&state)) {
                return 1;
        }
        if (!create_instance(&state)) {
                return 1;
        }
        if (enable_validation_layers
            && !setup_debug_messenger(state.instance)) {
                return 1;
        }
        if (!create_surface(&state)) {
                return 1;
        }
        if (!pick_physical_device(&state)) {
                return 1;
        }
        if (!create_logical_device(&state)) {
                return 1;
        }
        if (!create_swapchain(&state)) {
                return 1;
        }
        if (!create_image_views(&state)) {
                return 1;
        }
        if (!create_render_pass(&state)) {
                return 1;
        }
        if (!create_command_pool(&state)) {
                return 1;
        }
        if (!create_texture_image(&state)) {
                return 1;
        }
        if (!create_texture_image_view(&state)) {
                return 1;
        }
        if (!create_texture_sampler(&state)) {
                return 1;
        }
        if (!create_uniform_buffer(&state)) {
                return 1;
        }
        if (!create_descriptor_set_layout(&state)) {
                return 1;
        }
        if (!create_descriptor_pool(&state)) {
                return 1;
        }
        if (!create_descriptor_sets(&state)) {
                return 1;
        }
        if (!create_graphics_pipeline(&state)) {
                return 1;
        }
        if (!create_framebuffers(&state)) {
                return 1;
        }
        if (!create_vertex_buffer(&state)) {
                return 1;
        }
        if (!create_index_buffer(&state)) {
                return 1;
        }
        if (!create_command_buffer(&state)) {
                return 1;
        }
        if (!create_sync_objects(&state)) {
                return 1;
        }

        while (!glfwWindowShouldClose(state.window)) {
                glfwPollEvents();
                draw_frame(&state);
        }
        VkDevice device = state.device;
        vkDeviceWaitIdle(device);

        if (enable_validation_layers) {
                destroy_debug_utils_messenger_ext(state.instance,
                                                  debug_messenger, NULL);
        }
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroySemaphore(device, state.image_available_semaphore[i],
                                   NULL);
                vkDestroySemaphore(device, state.render_finished_semaphore[i],
                                   NULL);
                vkDestroyFence(device, state.inflight_fence[i], NULL);
        }
        vkDestroyCommandPool(device, state.command_pool, NULL);
        cleanup_swapchain(&state);

        vkDestroyBuffer(device, state.vertex_buffer, NULL);
        vkFreeMemory(device, state.vertex_buffer_memory, NULL);
        vkDestroyBuffer(device, state.index_buffer, NULL);
        vkFreeMemory(device, state.index_buffer_memory, NULL);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroyBuffer(device, state.uniform_buffers[i], NULL);
                vkFreeMemory(device, state.uniform_buffers_memory[i], NULL);
        }
        vkDestroySampler(device, state.texture_sampler, NULL);
        vkDestroyImageView(device, state.texture_view, NULL);
        vkDestroyImage(device, state.texture_image, NULL);
        vkFreeMemory(device, state.texture_memory, NULL);

        vkDestroyDescriptorPool(device, state.descriptor_pool, NULL);
        vkDestroyDescriptorSetLayout(device, state.descriptor_set_layout, NULL);
        vkDestroyPipeline(device, state.graphics_pipeline, NULL);
        vkDestroyPipelineLayout(device, state.pipeline_layout, NULL);
        vkDestroyRenderPass(device, state.render_pass, NULL);

        vkDestroyDevice(device, NULL);
        vkDestroySurfaceKHR(state.instance, state.surface, NULL);
        vkDestroyInstance(state.instance, NULL);
        glfwDestroyWindow(state.window);

        glfwTerminate();
        return 0;
}
