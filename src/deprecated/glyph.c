#include "glyph.h"

#include "matrix.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "GLFW/glfw3.h"
#include "actionHistory.c"
#include "buffer.c"
#include "canvas.c"
#include "cli.c"
#include "command.c"
#include "controlPanel.c"
#include "coordTransform.c"
#include "device.c"
#include "engine.c"
#include "graphicsPipeline.c"
#include "matrix.c"
#include "quad.c"
#include "quad.h"
#include "surface.c"
#include "swapchain.c"
#include "sync.c"

uint32_t frameCounter = 0;
uint32_t prevFrame = 0;
double prevTime = 0.0;

// Returns 1 if pos is within the bounding box, 0 if not.
static inline int withinBoundingBox(BoundingBox *box, float xpos, float ypos) {
        if (xpos > box->pos.x && xpos < box->pos.x + box->extent.x
            && ypos > box->pos.y && ypos < box->pos.y + box->extent.y) {
                return 1;
        }
        return 0;
}

// Check out RAD Debugger for UI Library ideas
// They have ui  signal that holds information about whether or not a "UIBox"
// has been pressed, dragged, previously pressed and released, double clicked,
// and so on
bool savePressed = false;
bool colorPickedPressed = false;
bool undoPressed = false;
bool redoPressed = false;
bool drawing = false;

static void processInput(glyph_state *state) {
        if (glfwGetKey(state->engine.window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(state->engine.window, GLFW_TRUE);
        }

        if (glfwGetKey(state->engine.window, GLFW_KEY_S) == GLFW_PRESS) {
                savePressed = 1;
        }
        if (glfwGetKey(state->engine.window, GLFW_KEY_S) == GLFW_RELEASE
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

        Canvas *canvas = &state->canvas;
        double x, y;
        glfwGetCursorPos(state->engine.window, &x, &y);
        int extentX, extentY;
        glfwGetWindowSize(state->engine.window, &extentX, &extentY);
        float vulkanX = (x / extentX) * 2 - 1.0;
        float vulkanY = (y / extentY) * 2 - 1.0;

        BoundingBox canvasBox = getQuadBoundingBox(&canvas->uniform.mvp);
        int pixelX = ((vulkanX - canvasBox.pos.x) / canvasBox.extent.x)
                     * canvas->width;
        int pixelY = ((vulkanY - canvasBox.pos.y) / canvasBox.extent.y)
                     * canvas->height;

        int mouseState
            = glfwGetMouseButton(state->engine.window, GLFW_MOUSE_BUTTON_1);
        if (glfwGetKey(state->engine.window, GLFW_KEY_U) == GLFW_PRESS) {
                undoPressed = true;
        }
        if (glfwGetKey(state->engine.window, GLFW_KEY_R) == GLFW_PRESS) {
                redoPressed = true;
        }
        if (glfwGetKey(state->engine.window, GLFW_KEY_U) == GLFW_RELEASE
            && undoPressed) {
                undoAction(canvas);
                undoPressed = false;
        }
        if (glfwGetKey(state->engine.window, GLFW_KEY_R) == GLFW_RELEASE
            && redoPressed) {
                redoAction(canvas);
                redoPressed = false;
        }

        if (glfwGetKey(state->engine.window, GLFW_KEY_P) == GLFW_PRESS) {
                colorPickedPressed = 1;
        }
        if (glfwGetKey(state->engine.window, GLFW_KEY_P) == GLFW_RELEASE
            && colorPickedPressed) {
                Vec3 color = {
                        .x = canvas->data[pixelY * (canvas->width * 4)
                                          + (pixelX * 4)]
                             / 255.0,
                        .y = canvas->data[pixelY * (canvas->width * 4)
                                          + (pixelX * 4) + 1]
                             / 255.0,
                        .z = canvas->data[pixelY * (canvas->width * 4)
                                          + (pixelX * 4) + 2]
                             / 255.0,
                };
                setCurrentColor(&state->controlPanel, &color);
                colorPickedPressed = 0;
        }

        if (mouseState == GLFW_PRESS) {
                if (withinBoundingBox(&canvasBox, vulkanX, vulkanY)) {
                        if (!drawing) {
                                drawing = true;
                        }
                        Vec3 color
                            = state->controlPanel
                                  .quadUniforms[CONTROL_PANEL_CURRENT_COLOR]
                                  .color;

                        uint8_t r = color.x * 255;
                        uint8_t g = color.y * 255;
                        uint8_t b = color.z * 255;

                        uint32_t pos
                            = pixelY * (canvas->width * 4) + (pixelX * 4);

                        if (r != canvas->data[pos] || g != canvas->data[pos + 1]
                            || b != canvas->data[pos + 2]) {
                                DrawingActionChange exe = { pos, r, g, b, 255 };
                                DrawingActionChange rev = {
                                        pos,
                                        canvas->data[pos],
                                        canvas->data[pos + 1],
                                        canvas->data[pos + 2],
                                        canvas->data[pos + 3],
                                };
                                recordDrawingAction(&exe, &rev);

                                canvas->data[pos] = r;
                                canvas->data[pos + 1] = g;
                                canvas->data[pos + 2] = b;
                        }
                }

                for (int i = 0; i < CONTROL_PANEL_QUAD_COUNT; i++) {
                        BoundingBox box = getQuadBoundingBox(
                            &state->controlPanel.quadUniforms[i].mvp);
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

        if (drawing && mouseState == GLFW_RELEASE) {
                drawing = false;
                if (!finalizeDrawingAction()) {
                        abort();
                }
        }

        if (mouseState == GLFW_RELEASE && state->controlPanel.clicked) {
                uint32_t clicked = state->controlPanel.clicked;
                if (clicked <= CONTROL_PANEL_HISTORY_16) {
                        pickCurrentColorFromHistory(&state->controlPanel);
                }
                if (clicked == CONTROL_PANEL_CREATOR_PREVIEW) {
                        setCurrentColor(
                            &state->controlPanel,
                            &state->controlPanel
                                 .quadUniforms[CONTROL_PANEL_CREATOR_PREVIEW]
                                 .color);
                }
                state->controlPanel.clicked = 0;
        }

        if (state->controlPanel.clicked == CONTROL_PANEL_CREATOR_R_BUTTON
            || state->controlPanel.clicked == CONTROL_PANEL_CREATOR_G_BUTTON
            || state->controlPanel.clicked == CONTROL_PANEL_CREATOR_B_BUTTON) {
                updateColorCreatorPreview(&state->controlPanel, vulkanX,
                                          vulkanY);
                updateCreatorButtonUniformObject(&state->controlPanel,
                                                 state->engine.swapchain_extent,
                                                 state->controlPanel.clicked);
        }
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
                .renderPass = state->engine.render_pass,
                .framebuffer
                = state->engine.swapchain_framebuffers[image_index],
                .renderArea.offset = { 0, 0 },
                .renderArea.extent = state->engine.swapchain_extent,
                .clearValueCount = 1,
                .pClearValues = &clear_value,
        };

        vkCmdBeginRenderPass(cmd_buffer, &passinfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = {
                .x = 0.0f,
                .y = 0.0f,
                .width = (float)state->engine.swapchain_extent.width,
                .height = (float)state->engine.swapchain_extent.height,
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
        };
        vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

        VkRect2D scissor = {
                .offset = { 0, 0 },
                .extent = state->engine.swapchain_extent,
        };
        vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

        vkCmdBindIndexBuffer(cmd_buffer, state->index_buffer, 0,
                             VK_INDEX_TYPE_UINT16);
        uint32_t indices_size = sizeof(indices) / sizeof(indices[0]);
        drawCanvas(&state->canvas, cmd_buffer, state->engine.current_frame,
                   &state->engine.swapchain_extent, indices_size);

        drawControlPanel(&state->controlPanel, cmd_buffer,
                         state->engine.current_frame,
                         &state->engine.swapchain_extent, indices_size);

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

        uint32_t current_frame = state->engine.current_frame;
        VkDevice device = state->engine.device;
        vkWaitForFences(device, 1, state->engine.inflight_fence + current_frame,
                        VK_TRUE, UINT64_MAX);

        uint32_t image_index;
        VkResult res = vkAcquireNextImageKHR(
            device, state->engine.swapchain, UINT64_MAX,
            state->engine.image_available_semaphore[current_frame],
            VK_NULL_HANDLE, &image_index);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR
            || state->engine.framebuffer_resized) {
                state->engine.framebuffer_resized = 0;
                recreate_swapchain(&state->engine);

                updateControlPanelUniformObjects(
                    &state->controlPanel, state->engine.swapchain_extent);
                updateCanvasUniformObject(&state->canvas,
                                          state->engine.swapchain_extent);

                return 1;
        } else if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to acquire swap chain image: %s\n",
                        string_VkResult(res));
                return 0;
        }

        writeCanvasDataToImage(state, current_frame);

        vkResetFences(device, 1, state->engine.inflight_fence + current_frame);

        vkResetCommandBuffer(state->engine.command_buffer[current_frame], 0);
        record_command_buffer(
            state, state->engine.command_buffer[current_frame], image_index);

        VkPipelineStageFlags wait_stages[] = {
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        };
        VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores
                = state->engine.image_available_semaphore + current_frame,
                .pWaitDstStageMask = wait_stages,
                .commandBufferCount = 1,
                .pCommandBuffers = state->engine.command_buffer + current_frame,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores
                = state->engine.render_finished_semaphore + current_frame,
        };

        res = vkQueueSubmit(state->engine.graphics_queue, 1, &submit_info,
                            state->engine.inflight_fence[current_frame]);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to sumbit draw command buffer: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkSwapchainKHR swapchains[] = { state->engine.swapchain };
        VkPresentInfoKHR present_info = {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores
                = state->engine.render_finished_semaphore + current_frame,
                .swapchainCount = 1,
                .pSwapchains = swapchains,
                .pImageIndices = &image_index,
                .pResults = NULL,
        };

        res = vkQueuePresentKHR(state->engine.presentation_queue,
                                &present_info);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
                recreate_swapchain(&state->engine);
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

        if (!init_window(&state.engine, "Cross Stitch :)", 1000, 1000)) {
                return 1;
        }
        if (!create_instance(&state.engine)) {
                return 1;
        }
        if (enable_validation_layers
            && !setup_debug_messenger(state.engine.instance)) {
                return 1;
        }
        if (!create_surface(&state.engine)) {
                return 1;
        }
        if (!pick_physical_device(&state.engine)) {
                return 1;
        }
        if (!create_logical_device(&state.engine)) {
                return 1;
        }
        if (!create_swapchain(&state.engine)) {
                return 1;
        }
        if (!create_swapchain_image_views(&state.engine)) {
                return 1;
        }
        if (!create_render_pass(&state.engine)) {
                return 1;
        }
        if (!create_command_pool(&state.engine)) {
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
        if (!create_swapchain_framebuffers(&state.engine)) {
                return 1;
        }
        if (!create_vertex_buffer(&state)) {
                return 1;
        }
        if (!create_index_buffer(&state.engine, &state.index_buffer,
                                 &state.index_buffer_memory)) {
                return 1;
        }
        if (!createControlPanel(&state)) {
                return 1;
        }
        if (!create_command_buffer(&state.engine)) {
                return 1;
        }
        if (!create_sync_objects(&state.engine)) {
                return 1;
        }

        initActionHistory(&state.canvas);
        while (!glfwWindowShouldClose(state.engine.window)) {
                processInput(&state);
                draw_frame(&state);

                glfwPollEvents();
        }
        VkDevice device = state.engine.device;
        vkDeviceWaitIdle(device);

        if (enable_validation_layers) {
                destroy_debug_utils_messenger_ext(state.engine.instance,
                                                  debug_messenger, NULL);
        }
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroySemaphore(
                    device, state.engine.image_available_semaphore[i], NULL);
                vkDestroySemaphore(
                    device, state.engine.render_finished_semaphore[i], NULL);
                vkDestroyFence(device, state.engine.inflight_fence[i], NULL);
        }
        vkDestroyCommandPool(device, state.engine.command_pool, NULL);
        cleanup_swapchain(&state.engine);

        vkDestroyBuffer(device, state.canvas.vertexBuffer, NULL);
        vkFreeMemory(device, state.canvas.vertexBufferMemory, NULL);
        vkDestroyBuffer(device, state.index_buffer, NULL);
        vkFreeMemory(device, state.index_buffer_memory, NULL);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroyBuffer(device, state.canvas.uniformBuffers[i], NULL);
                vkFreeMemory(device, state.canvas.uniformBuffersMemory[i],
                             NULL);
        }

        cleanupActionHistory();
        destroyCanvas(&state);
        destroyControlPanel(&state);
        destroyQuadVertexBuffer(&state.engine);

        vkDestroyDescriptorPool(device, state.canvas.descriptorPool, NULL);
        vkDestroyDescriptorSetLayout(device, state.canvas.descriptorSetLayout,
                                     NULL);
        vkDestroyPipeline(device, state.canvas.pipeline, NULL);
        vkDestroyPipelineLayout(device, state.canvas.pipelineLayout, NULL);
        vkDestroyRenderPass(device, state.engine.render_pass, NULL);

        vkDestroyDevice(device, NULL);
        vkDestroySurfaceKHR(state.engine.instance, state.engine.surface, NULL);
        vkDestroyInstance(state.engine.instance, NULL);
        glfwDestroyWindow(state.engine.window);

        glfwTerminate();
        return 0;
}
