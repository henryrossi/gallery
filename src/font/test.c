#include "glyph.h"

#include "../buffer.c"
#include "../command.c"
#include "../device.c"
#include "../engine.c"
#include "../graphicsPipeline.c"
#include "../matrix.c"
#include "../quad.c"
#include "../surface.c"
#include "../swapchain.c"
#include "../sync.c"
#include "font.c"

#include "graphicsPipeline.h"
#include "quad.h"
#include "sync.h"
#include <stdlib.h>

#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 1000

GlyphEngine engine = { 0 };

static GlyphResult begin_frame(GlyphEngine *engine, VkClearValue *clearValue) {
        uint32_t current_frame = engine->current_frame;
        VkDevice device = engine->device;
        VkCommandBuffer cmdBuffer = engine->command_buffer[current_frame];

        vkWaitForFences(device, 1, engine->inflight_fence + current_frame,
                        VK_TRUE, UINT64_MAX);

        VkResult res = vkAcquireNextImageKHR(
            device, engine->swapchain, UINT64_MAX,
            engine->image_available_semaphore[current_frame], VK_NULL_HANDLE,
            &engine->current_image_index);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR
            || engine->framebuffer_resized) {
                engine->framebuffer_resized = 0;
                recreate_swapchain(engine);
                return GLYPH_FRAMEBUFFER_RESIZED;
        } else if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to acquire swap chain image: %s\n",
                        string_VkResult(res));
                return GLYPH_FAILURE;
        }

        vkResetFences(device, 1, engine->inflight_fence + current_frame);
        vkResetCommandBuffer(cmdBuffer, 0);
        VkCommandBufferBeginInfo begininfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = 0,
                .pInheritanceInfo = NULL,
        };

        res = vkBeginCommandBuffer(cmdBuffer, &begininfo);
        if (res != VK_SUCCESS) {
                fprintf(stderr,
                        "Failed to being command buffer recording: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkRenderPassBeginInfo passinfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = engine->render_pass,
                .framebuffer
                = engine->swapchain_framebuffers[engine->current_image_index],
                .renderArea.offset = { 0, 0 },
                .renderArea.extent = engine->swapchain_extent,
                .clearValueCount = 1,
                .pClearValues = clearValue,
        };

        vkCmdBeginRenderPass(cmdBuffer, &passinfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = {
                .x = 0.0f,
                .y = 0.0f,
                .width = (float)engine->swapchain_extent.width,
                .height = (float)engine->swapchain_extent.height,
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
        };
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

        VkRect2D scissor = {
                .offset = { 0, 0 },
                .extent = engine->swapchain_extent,
        };
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

        return GLYPH_SUCCESS;
}

static GlyphResult end_frame(GlyphEngine *engine) {
        uint32_t current_frame = engine->current_frame;
        VkCommandBuffer cmdBuffer = engine->command_buffer[current_frame];

        vkCmdEndRenderPass(cmdBuffer);

        VkResult res = vkEndCommandBuffer(cmdBuffer);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to record command buffer: %s\n",
                        string_VkResult(res));
                // I believe there's a potential to deadlock here since we are
                // returning early before sumbitting a fence we will later wait
                // on.
                return GLYPH_FAILURE;
        }

        VkPipelineStageFlags wait_stages[] = {
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        };
        VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores
                = engine->image_available_semaphore + current_frame,
                .pWaitDstStageMask = wait_stages,
                .commandBufferCount = 1,
                .pCommandBuffers = engine->command_buffer + current_frame,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores
                = engine->render_finished_semaphore + current_frame,
        };

        res = vkQueueSubmit(engine->graphics_queue, 1, &submit_info,
                            engine->inflight_fence[current_frame]);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to sumbit draw command buffer: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkSwapchainKHR swapchains[] = { engine->swapchain };
        VkPresentInfoKHR present_info = {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores
                = engine->render_finished_semaphore + current_frame,
                .swapchainCount = 1,
                .pSwapchains = swapchains,
                .pImageIndices = &engine->current_image_index,
                .pResults = NULL,
        };

        res = vkQueuePresentKHR(engine->presentation_queue, &present_info);
        engine->current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
                recreate_swapchain(engine);
                return GLYPH_FRAMEBUFFER_RESIZED;
        } else if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to present swap chain image: %s",
                        string_VkResult(res));
                return GLYPH_FAILURE;
        }

        glfwPollEvents();
        return GLYPH_SUCCESS;
}

static GlyphResult glyph_startup_engine(GlyphEngine *engine) {
        if (!init_window(engine, "font", WINDOW_WIDTH, WINDOW_HEIGHT)) {
                return GLYPH_FAILURE;
        }

        if (!create_instance(engine)) {
                return GLYPH_FAILURE;
        }
        if (enable_validation_layers
            && !setup_debug_messenger(engine->instance)) {
                return GLYPH_FAILURE;
        }

        // Surface creation

        if (!create_surface(engine)) {
                return GLYPH_FAILURE;
        }

        // Device creation
        if (!pick_physical_device(engine)) {
                return GLYPH_FAILURE;
        }
        if (!create_logical_device(engine)) {
                return GLYPH_FAILURE;
        }

        // swapchain creation
        if (!create_swapchain(engine)) {
                return GLYPH_FAILURE;
        }
        if (!create_swapchain_image_views(engine)) {
                return GLYPH_FAILURE;
        }
        if (!create_render_pass(engine)) {
                return GLYPH_FAILURE;
        }
        if (!create_swapchain_framebuffers(engine)) {
                return GLYPH_FAILURE;
        }

        if (!create_command_pool(engine)) {
                return GLYPH_FAILURE;
        }
        if (!create_command_buffer(engine)) {
                return GLYPH_FAILURE;
        }
        if (!create_sync_objects(engine)) {
                return GLYPH_FAILURE;
        }

        return GLYPH_SUCCESS;
}

typedef struct {
        GlyphEngine *engine;
        VkBuffer *pVertexBuffer;
        uint32_t verticesSize;
        VkDeviceSize vertexOffset;
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkDescriptorSet *pDescriptorSet;
} GlyphDrawInfo;

static GlyphResult glyph_draw(GlyphDrawInfo *drawInfo) {
        VkCommandBuffer cmdBuffer
            = drawInfo->engine->command_buffer[drawInfo->engine->current_frame];

        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, drawInfo->pVertexBuffer,
                               &drawInfo->vertexOffset);
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          drawInfo->pipeline);
        if (drawInfo->pDescriptorSet != NULL
            && drawInfo->pipelineLayout != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(cmdBuffer,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        drawInfo->pipelineLayout, 0, 1,
                                        drawInfo->pDescriptorSet, 0, NULL);
        }

        vkCmdDraw(cmdBuffer, drawInfo->verticesSize, 1, 0, 0);

        return GLYPH_SUCCESS;
}

typedef struct {
        GlyphEngine *engine;
        VkBuffer indexBuffer;
        uint32_t indicesSize;
        VkBuffer *pVertexBuffer;
        VkDeviceSize vertexOffset;
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkDescriptorSet *pDescriptorSet;
} GlyphDrawIndexedInfo;

static GlyphResult glyph_draw_indexed(GlyphDrawIndexedInfo *drawInfo) {
        VkCommandBuffer cmdBuffer
            = drawInfo->engine->command_buffer[drawInfo->engine->current_frame];

        vkCmdBindIndexBuffer(cmdBuffer, drawInfo->indexBuffer, 0,
                             VK_INDEX_TYPE_UINT16);
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, drawInfo->pVertexBuffer,
                               &drawInfo->vertexOffset);
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          drawInfo->pipeline);
        if (drawInfo->pDescriptorSet != NULL
            && drawInfo->pipelineLayout != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(cmdBuffer,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        drawInfo->pipelineLayout, 0, 1,
                                        drawInfo->pDescriptorSet, 0, NULL);
        }

        vkCmdDrawIndexed(cmdBuffer, drawInfo->indicesSize, 1, 0, 0, 0);

        return GLYPH_SUCCESS;
}

int main(int argc, char *argv[]) {
        glyph_startup_engine(&engine);

        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

        // VkDescriptorSetLayoutBinding uniformLayoutBinding = {
        //         .binding = 0,
        //         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        //         .descriptorCount = 1,
        //         .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        // };

        VkDescriptorSetLayoutCreateInfo descLayoutInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = 0,
                // .pBindings = &uniformLayoutBinding,
        };

        VkResult res = vkCreateDescriptorSetLayout(
            engine.device, &descLayoutInfo, NULL, &descriptorSetLayout);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create descriptor set layout: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkPipelineVertexInputStateCreateInfo vertexInputInfo = { 0 };
        getQuadPipelineVertexInputInfo(&vertexInputInfo);

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {
                .colorWriteMask
                = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                  | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
        };

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipelineLayoutCreateInfo layoutInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 0,
                // .pSetLayouts = &cp->descriptorSetLayout,
                .pushConstantRangeCount = 0,
        };
        res = vkCreatePipelineLayout(engine.device, &layoutInfo, NULL,
                                     &pipelineLayout);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create quad pipeline layout: %s\n",
                        string_VkResult(res));
                return 0;
        }

        GraphicsPipelineCreateInfo pipelineInfo = {
                .device = engine.device,
                .renderPass = engine.render_pass,
                // pipeline layout needs descriptor set layouts
                .pipelineLayout = pipelineLayout,
                .vertFile = "vert.spv",
                .fragFile = "frag.spv",
                .vertexInputInfo = &vertexInputInfo,
                .blendAttachmentStatesCount = 1,
                .blendAttachmentStates = &colorBlendAttachment,
                .depthStencilState = NULL,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .polygonMode = VK_POLYGON_MODE_LINE,
                .primativeTopology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
        };
        VkPipeline pipeline = createGraphicsPipeline(&pipelineInfo);

        // VkBuffer indexBuffer = VK_NULL_HANDLE;
        // VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
        // if (!create_index_buffer(&engine, &indexBuffer, &indexBufferMemory))
        // {
        //         return 1;
        // }

        float xscale, yscale;
        glfwGetWindowContentScale(engine.window, &xscale, &yscale);

        TTFParser p = { 0 };
        TTFFont font = { 0 };
        if (parse_font(&font, &p)) {
                fprintf(stderr, "Failed to parse font\n");
                return 1;
        }
        Glyph glyph = font.glyphData[0];
        float unitsPerEm = (float)font.head.unitsPerEm;
        // float pointSize = xscale * 14;

        VkDeviceSize contour0NumPoints = glyph.contours[0].numberOfPoints;
        VkDeviceSize contour1NumPoints = glyph.contours[1].numberOfPoints;
        u16 numPoints = contour0NumPoints + contour1NumPoints;

        VkDeviceSize size = sizeof(Vec2) * (numPoints + 2);
        Vec2 *vertices = malloc(size);
        for (u32 i = 0; i < contour0NumPoints; i++) {
                vertices[i].x
                    = (float)glyph.contours[0].xPoints[i] / unitsPerEm;
                vertices[i].y
                    = (float)glyph.contours[0].yPoints[i] / unitsPerEm;
        }
        vertices[contour0NumPoints].x = vertices[0].x;
        vertices[contour0NumPoints].y = vertices[0].y;
        for (u32 i = 0; i < contour1NumPoints + 1; i++) {
                vertices[contour0NumPoints + 1 + i].x
                    = (float)glyph.contours[1].xPoints[i] / unitsPerEm;
                vertices[contour0NumPoints + 1 + i].y
                    = (float)glyph.contours[1].yPoints[i] / unitsPerEm;
        }
        vertices[numPoints + 1].x = vertices[contour0NumPoints + 1].x;
        vertices[numPoints + 1].y = vertices[contour0NumPoints + 1].y;

        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;

        // QuadVertexBufferRetrieveInfo vertexRetrieveInfo = {
        //         .device = engine.device,
        //         .phyDevice = engine.physical_device,
        //         .vertexBuffer = &vertexBuffer,
        //         .vertexMemory = &vertexBufferMemory,
        //         .cmdPool = engine.command_pool,
        //         .graphicsQueue = engine.graphics_queue,
        // };
        // if (!retrieveQuadVertexBuffer(&vertexRetrieveInfo)) {
        //         return 1;
        // }

        VkDevice device = engine.device;

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        BufferCreateInfo stagingInfo = {
                .device = device,
                .buffer = &stagingBuffer,
                .memory = &stagingMemory,
                .size = size,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                .physical_device = engine.physical_device,
        };
        if (!createBuffer(&stagingInfo)) {
                return 0;
        }

        void *data;
        vkMapMemory(device, stagingMemory, 0, size, 0, &data);
        memcpy(data, vertices, size);
        vkUnmapMemory(device, stagingMemory);

        BufferCreateInfo bufferInfo = {
                .device = device,
                .buffer = &vertexBuffer,
                .memory = &vertexBufferMemory,
                .size = size,
                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                         | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .physical_device = engine.physical_device,
        };
        if (!createBuffer(&bufferInfo)) {
                return 0;
        }

        CopyBufferInfo copyInfo = {
                .device = device,
                .dst = vertexBuffer,
                .src = stagingBuffer,
                .size = size,
                .cmdpool = engine.command_pool,
                .graphics_queue = engine.graphics_queue,
        };
        copyBuffer(&copyInfo);

        vkDestroyBuffer(device, stagingBuffer, NULL);
        vkFreeMemory(device, stagingMemory, NULL);

        while (!glfwWindowShouldClose(engine.window)) {
                VkClearValue clearValue = { { { 0.3f, 0.3f, 0.3f, 1.0f } } };
                GlyphResult res = begin_frame(&engine, &clearValue);
                if (res == GLYPH_FRAMEBUFFER_RESIZED) {
                        continue;
                } else if (res == GLYPH_FAILURE) {
                        return 0;
                }

                // uint32_t indicesSize = sizeof(indices) / sizeof(indices[0]);
                GlyphDrawInfo drawInfo = {
                        .engine = &engine,
                        .vertexOffset = 0,
                        .pVertexBuffer = &vertexBuffer,
                        .verticesSize = size,
                        .pipeline = pipeline,
                        .pipelineLayout = VK_NULL_HANDLE,
                        .pDescriptorSet = VK_NULL_HANDLE,
                };
                glyph_draw(&drawInfo);

                // use GOTO!!

                // drawInfo.vertexOffset = sizeof(Vec2) * (contour0NumPoints +
                // 1); drawInfo.verticesSize = sizeof(Vec2) * (contour1NumPoints
                // + 1); glyph_draw(&drawInfo);

                res = end_frame(&engine);
                if (res == GLYPH_FRAMEBUFFER_RESIZED) {
                        continue;
                } else if (res == GLYPH_FAILURE) {
                        return 0;
                }
        }

        return 0;
}
