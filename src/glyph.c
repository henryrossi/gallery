#include "glyph.h"

#include "matrix.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "GLFW/glfw3.h"
#include "buffer.c"
#include "canvas.c"
#include "cli.c"
#include "command.c"
#include "controlPanel.c"
#include "debug.c"
#include "device.c"
#include "graphicsPipeline.c"
#include "matrix.c"
#include "quad.c"
#include "surface.c"
#include "swapchain.c"
#include "sync.c"

uint32_t frameCounter = 0;
uint32_t prevFrame = 0;
double prevTime = 0.0;

// Returns 1 if pos is within the bounding box, 0 if not
static inline int withinBoundingBox(BoundingBox *box, float xpos, float ypos) {
        if (xpos > box->pos.x && xpos < box->pos.x + box->extent.x
            && ypos > box->pos.y && ypos < box->pos.y + box->extent.y) {
                return 1;
        }
        return 0;
}

int savePressed = 0;
static void processInput(glyph_state *state) {
        if (glfwGetKey(state->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(state->window, GLFW_TRUE);
        }

        if (glfwGetKey(state->window, GLFW_KEY_S) == GLFW_PRESS) {
                savePressed = 1;
        }
        if (glfwGetKey(state->window, GLFW_KEY_S) == GLFW_RELEASE
            && savePressed) {
                int res = saveCanvasToPNG(&state->canvas);
                if (res) {
                        fprintf(stdout, "Saved file %s\n",
                                state->canvas.filename);
                } else {

                        fprintf(stdout, "Failed to save file %s\n",
                                state->canvas.filename);
                }
                savePressed = 0;
        }

        Canvas canvas = state->canvas;
        double x, y;
        glfwGetCursorPos(state->window, &x, &y);
        int extentX, extentY;
        glfwGetWindowSize(state->window, &extentX, &extentY);
        float vulkanX = (x / extentX) * 2 - 1.0;
        float vulkanY = (y / extentY) * 2 - 1.0;

        int mouseState = glfwGetMouseButton(state->window, GLFW_MOUSE_BUTTON_1);
        if (mouseState == GLFW_PRESS) {
                if (x > canvas.windowX
                    && x < (canvas.windowX + canvas.windowWidth)
                    && y > canvas.windowY
                    && y < (canvas.windowY + canvas.windowHeight)) {

                        int pixelX = ((x - canvas.windowX) / canvas.windowWidth)
                                     * canvas.width;
                        int pixelY
                            = ((y - canvas.windowY) / canvas.windowHeight)
                              * canvas.height;

                        Vec3 color = state->controlPanel.quadUniforms[0].color;
                        canvas.data[pixelY * (canvas.width * 4) + (pixelX * 4)]
                            = color.x * 255;
                        canvas.data[pixelY * (canvas.width * 4) + (pixelX * 4)
                                    + 1]
                            = color.y * 255;
                        canvas.data[pixelY * (canvas.width * 4) + (pixelX * 4)
                                    + 2]
                            = color.z * 255;
                }

                for (int i = 0; i < CONTROL_PANEL_QUAD_COUNT; i++) {
                        BoundingBox box = getQuadBoundingBox(
                            &state->controlPanel.quadUniforms[i].model,
                            &state->controlPanel.quadUniforms[i].proj);
                        if (withinBoundingBox(&box, vulkanX, vulkanY)) {
                                if (((i > 0 && i <= CONTROL_PANEL_HISTORY_16)
                                     || (i == CONTROL_PANEL_CREATOR_R_BUTTON)
                                     || (i == CONTROL_PANEL_CREATOR_G_BUTTON)
                                     || (i == CONTROL_PANEL_CREATOR_B_BUTTON)
                                     || (i == CONTROL_PANEL_CREATOR_PREVIEW))
                                    && state->controlPanel.clicked == 0) {
                                        state->controlPanel.clicked = i;
                                }
                        }
                }
        }

        if (mouseState == GLFW_RELEASE && state->controlPanel.clicked) {
                uint32_t clicked = state->controlPanel.clicked;
                if (clicked <= CONTROL_PANEL_HISTORY_16) {
                        pickCurrentColorFromHistory(&state->controlPanel);
                }
                if (clicked == CONTROL_PANEL_CREATOR_PREVIEW) {
                        pushCreatedColorToCurrent(&state->controlPanel);
                }
                state->controlPanel.clicked = 0;
        }

        if (state->controlPanel.clicked == CONTROL_PANEL_CREATOR_R_BUTTON
            || state->controlPanel.clicked == CONTROL_PANEL_CREATOR_G_BUTTON
            || state->controlPanel.clicked == CONTROL_PANEL_CREATOR_B_BUTTON) {
                updateColorCreatorPreview(&state->controlPanel, vulkanX,
                                          vulkanY);
                updateCreatorButtonModelMatrix(&state->controlPanel,
                                               state->swapchain_extent,
                                               state->controlPanel.clicked);
        }
}

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
                          state->canvas.pipeline);

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

        VkBuffer vertex_buffers[]
            = { state->canvas.vertexBuffer, state->controlPanel.vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd_buffer, 0, 1, vertex_buffers, offsets);
        vkCmdBindIndexBuffer(cmd_buffer, state->index_buffer, 0,
                             VK_INDEX_TYPE_UINT16);

        vkCmdBindDescriptorSets(
            cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            state->canvas.pipelineLayout, 0, 1,
            &state->canvas.descriptorSets[state->current_frame], 0, NULL);

        uint32_t indices_size = sizeof(indices) / sizeof(indices[0]);
        vkCmdDrawIndexed(cmd_buffer, indices_size, 1, 0, 0, 0);

        drawControlPanel(state, cmd_buffer, indices_size);

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

        frameCounter++;
        double time = glfwGetTime();
        double delta = time - prevTime;
        if (delta > 1.0) {
                uint32_t frameDelta = frameCounter - prevFrame;
                printf("FPS: %0.2f\n", (float)frameDelta / delta);
                prevFrame = frameCounter;
                prevTime = time;
        }

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
                return 1;
        } else if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to acquire swap chain image: %s\n",
                        string_VkResult(res));
                return 0;
        }

        writeCanvasDataToImage(state, current_frame);

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

        glyph_state state = { 0 };

        if (!parseCommandLineArgs(&state, argc, argv)) {
                return 1;
        }

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
        if (!createCanvas(&state)) {
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
        if (!createCanvasGraphicsPipeline(&state)) {
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
        if (!createControlPanel(&state)) {
                return 1;
        }
        if (!create_command_buffer(&state)) {
                return 1;
        }
        if (!create_sync_objects(&state)) {
                return 1;
        }

        while (!glfwWindowShouldClose(state.window)) {
                processInput(&state);
                draw_frame(&state);

                glfwPollEvents();
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

        vkDestroyBuffer(device, state.canvas.vertexBuffer, NULL);
        vkFreeMemory(device, state.canvas.vertexBufferMemory, NULL);
        vkDestroyBuffer(device, state.index_buffer, NULL);
        vkFreeMemory(device, state.index_buffer_memory, NULL);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroyBuffer(device, state.canvas.uniformBuffers[i], NULL);
                vkFreeMemory(device, state.canvas.uniformBuffersMemory[i],
                             NULL);
        }

        destroyCanvas(&state);
        destroyControlPanel(&state);
        destroyQuadVertexBuffer(&state);

        vkDestroyDescriptorPool(device, state.canvas.descriptorPool, NULL);
        vkDestroyDescriptorSetLayout(device, state.canvas.descriptorSetLayout,
                                     NULL);
        vkDestroyPipeline(device, state.canvas.pipeline, NULL);
        vkDestroyPipelineLayout(device, state.canvas.pipelineLayout, NULL);
        vkDestroyRenderPass(device, state.render_pass, NULL);

        vkDestroyDevice(device, NULL);
        vkDestroySurfaceKHR(state.instance, state.surface, NULL);
        vkDestroyInstance(state.instance, NULL);
        glfwDestroyWindow(state.window);

        glfwTerminate();
        return 0;
}
