#include "buffer.h"
#include "coordTransform.h"
#include "glyph.h"
#include "graphicsPipeline.h"
#include "matrix.h"
#include "quad.h"

#include <stdio.h>
#include <string.h>

// const float screenWidth = 2.0;
const float panelScale = 0.25;
// const float panelSize = panelScale * screenWidth;

const float currentScale = panelScale;
// const float currentSize = panelSize;

const float historyScale = panelScale / 8;
const static float previewScale = panelScale / 4;
const static float sliderScale = panelScale * 0.9;
// const float historySize = historyScale * screenWidth;

// const float previewSize = panelSize / 4;

// const float sliderWidth = panelSize * 0.9;

static void setCurrentColor(ControlPanel *cp, Vec3 *color) {
        for (int i = CONTROL_PANEL_HISTORY_16; i > 0; i--) {
                cp->quadUniforms[i].color = cp->quadUniforms[i - 1].color;
        }
        cp->quadUniforms[0].color = *color;
}

static void pickCurrentColorFromHistory(ControlPanel *panel) {
        uint32_t picked = panel->clicked;
        Vec3 tmp = panel->quadUniforms[picked].color;
        for (int i = picked; i > 0; i--) {
                panel->quadUniforms[i].color = panel->quadUniforms[i - 1].color;
        }
        panel->quadUniforms[0].color = tmp;
}

static void updateColorCreatorPreview(ControlPanel *panel, float mouseX,
                                      float mouseY) {
        uint32_t clicked = panel->clicked;
        BoundingBox bar
            = getQuadBoundingBox(&panel->quadUniforms[clicked - 1].mvp);

        float newButtonPos = mouseX;
        if (newButtonPos < bar.pos.x) {
                newButtonPos = bar.pos.x;
        } else if (newButtonPos > bar.pos.x + bar.extent.x) {
                newButtonPos = bar.pos.x + bar.extent.x;
        }

        float newColorValue = (newButtonPos - bar.pos.x) / bar.extent.x;

        if (clicked == CONTROL_PANEL_CREATOR_R_BUTTON) {
                panel->quadUniforms[CONTROL_PANEL_CREATOR_PREVIEW].color.x
                    = newColorValue;
        } else if (clicked == CONTROL_PANEL_CREATOR_G_BUTTON) {
                panel->quadUniforms[CONTROL_PANEL_CREATOR_PREVIEW].color.y
                    = newColorValue;
        } else if (clicked == CONTROL_PANEL_CREATOR_B_BUTTON) {
                panel->quadUniforms[CONTROL_PANEL_CREATOR_PREVIEW].color.z
                    = newColorValue;
        }
}

static void drawControlPanel(ControlPanel *cp, VkCommandBuffer cmdBuffer,
                             uint32_t frame, VkExtent2D *swapchainExtent,
                             uint32_t indicesSize) {

        memcpy(cp->uniformsMapped[frame], cp->quadUniforms,
               sizeof(cp->quadUniforms));

        VkDeviceSize offset = 0;
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          cp->Pipeline);
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &cp->vertexBuffer, &offset);

        for (int i = 0; i < CONTROL_PANEL_QUAD_COUNT; i++) {
                uint32_t uniformOffset = i * cp->alignedUniformSize;

                vkCmdBindDescriptorSets(
                    cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    cp->pipelineLayout, 0, 1, &cp->descriptorSets[frame], 1,
                    &uniformOffset);

                vkCmdDrawIndexed(cmdBuffer, indicesSize, 1, 0, 0, 0);
        }
}

static void
updateCreatorButtonUniformObject(ControlPanel *cp, VkExtent2D screen,
                                 GlyphControlPanelUniformIndex color) {
        //
        // float barWidth = 4.0 / swapchainExtent.height;
        // float buttonSize = barWidth * 4;
        // float leftEdge = 1.0 - panelScale - (sliderWidth / screenWidth);
        // float sliderX = 0.0;
        // float sliderY = -(1 / aspect) + currentSize + (3 * historySize)
        //                 + (previewSize * 1.5);
        //
        float panelOffset = (1.0 - panelScale) * (float)screen.width;
        float sliderWidth = sliderScale * (float)screen.width;
        Vec3 previewColor
            = cp->quadUniforms[CONTROL_PANEL_CREATOR_PREVIEW].color;
        cp->positions[color].x = panelOffset + (previewColor.x * sliderWidth);
        getPosCoordTransform(&cp->scales[color], &cp->positions[color], screen,
                             true, &cp->quadUniforms[color].mvp);
}

static void updateControlPanelUniformObjects(ControlPanel *cp,
                                             VkExtent2D screen) {
        float screenWidth = (float)screen.width;
        float screenHeight = (float)screen.height;

        float yTop = -0.5 * screenHeight / screenWidth;

        float panelOffset = (0.5 - panelScale) * screenWidth;

        uint32_t i = CONTROL_PANEL_CURRENT_COLOR;
        cp->scales[i].x = currentScale * screenWidth;
        cp->scales[i].y = currentScale * screenHeight;
        cp->positions[i].x = panelOffset + (currentScale * 0.5 * screenWidth);
        cp->positions[i].y = (yTop + currentScale * 0.5) * screenHeight;
        getPosCoordTransform(&cp->scales[i], &cp->positions[i], screen, true,
                             &cp->quadUniforms[i].mvp);
        cp->quadUniforms[i].color.x = 1.0;
        cp->quadUniforms[i].color.y = 0.5;

        Vec3 historyScales
            = { historyScale * screenWidth, historyScale * screenHeight, 1.0 };
        Vec3 historyPos = {
                panelOffset + (historyScale * 0.5 * screenWidth),
                (yTop + currentScale + historyScale * 0.5) * screenHeight,
                0.0,
        };
        for (i++; i <= CONTROL_PANEL_HISTORY_16; i++) {
                cp->scales[i] = historyScales;
                cp->positions[i] = historyPos;
                getPosCoordTransform(&cp->scales[i], &cp->positions[i], screen,
                                     true, &cp->quadUniforms[i].mvp);
                if (i == CONTROL_PANEL_HISTORY_8) {
                        historyPos.x
                            = panelOffset + (historyScale * 0.5 * screenWidth);
                        historyPos.y += screenHeight * historyScale;
                } else {
                        historyPos.x += screenWidth * historyScale;
                }
        }

        float yPreview
            = (yTop + currentScale + (historyScale * 2) + previewScale);
        Vec3 previewScales
            = { previewScale * screenWidth, previewScale * screenHeight, 0.0 };
        Vec3 previewPos = {
                panelOffset + (previewScale * screenWidth),
                yPreview * screenHeight,
                0.0,
        };
        cp->scales[i] = previewScales;
        cp->positions[i] = previewPos;
        getPosCoordTransform(&cp->scales[i], &cp->positions[i], screen, true,
                             &cp->quadUniforms[i].mvp);
        i++;

        for (int j = 0; j < 3; j++) {
                float x = panelOffset * 1.05;
                float y = (yPreview + previewScale + (historyScale * j))
                          * screenHeight;

                Vec3 barScales = { sliderScale * screenWidth, 4.0, 0.0 };
                Vec3 barPos = {
                        x + (sliderScale * 0.5 * screenWidth),
                        y,
                        0.0,
                };
                cp->scales[i] = barScales;
                cp->positions[i] = barPos;
                getPosCoordTransform(&cp->scales[i], &cp->positions[i], screen,
                                     true, &cp->quadUniforms[i].mvp);
                i++;

                Vec3 buttonScales
                    = { 16.0 / screenHeight * screenWidth, 16.0, 0.0 };
                Vec3 buttonPos = { x, y, 0.0 };
                cp->scales[i] = buttonScales;
                cp->positions[i] = buttonPos;
                getPosCoordTransform(&cp->scales[i], &cp->positions[i], screen,
                                     true, &cp->quadUniforms[i].mvp);
                i++;
        }
}

//
// static void
// updateCreatorButtonModelMatrix(ControlPanel *cp, VkExtent2D
// swapchainExtent,
//                                GlyphControlPanelUniformIndex
//                                color) {
//         float aspect
//             = (float)swapchainExtent.width /
//             (float)swapchainExtent.height;
//
//         float barWidth = 4.0 / swapchainExtent.height;
//         float buttonSize = barWidth * 4;
//         float leftEdge = 1.0 - panelScale - (sliderWidth /
//         screenWidth); float sliderX = 0.0; float sliderY =
//         -(1 / aspect) + currentSize + (3 * historySize)
//                         + (previewSize * 1.5);
//
//         Vec3 previewColor
//             =
//             cp->quadUniforms[CONTROL_PANEL_CREATOR_PREVIEW].color;
//         if (color == CONTROL_PANEL_CREATOR_R_BUTTON) {
//                 sliderX = leftEdge + (previewColor.x *
//                 sliderWidth);
//         } else if (color == CONTROL_PANEL_CREATOR_G_BUTTON) {
//                 sliderX = leftEdge + (previewColor.y *
//                 sliderWidth); sliderY += 2 * historySize;
//         } else if (color == CONTROL_PANEL_CREATOR_B_BUTTON) {
//                 sliderX = leftEdge + (previewColor.z *
//                 sliderWidth); sliderY += 4 * historySize;
//         }
//         Mat4 buttonModel = {
//                 {
//                         { buttonSize, 0.0, 0.0, 0.0 },
//                         { 0.0, buttonSize, 0.0, 0.0 },
//                         { 0.0, 0.0, 1.0, 0.0 },
//                         { sliderX, sliderY, 0.0, 1.0 },
//                 },
//         };
//
//         Mat4 *proj = calcControlPanelProjectionMatrix(cp,
//         swapchainExtent); Mat4 mvp = { 0 };
//         multMat4xMat4(proj, &buttonModel, &mvp);
//         cp->quadUniforms[color].mvp = mvp;
// }
//
// static void formatControlPanel(glyph_state *state) {
//         float aspect = (float)state->swapchain_extent.width
//                        /
//                        (float)state->swapchain_extent.height;
//         Mat4 *pProj =
//         calcControlPanelProjectionMatrix(&state->controlPanel,
//                                                        state->swapchain_extent);
//         Mat4 mvp = { 0 };
//
//         Mat4 currentModel = {
//                 {
//                         { currentScale, 0.0, 0.0, 0.0 },
//                         { 0.0, currentScale, 0.0, 0.0 },
//                         { 0.0, 0.0, 1.0, 0.0 },
//                         { 1.0 - panelSize + currentScale,
//                           -(1 / aspect) + currentScale,
//                           0.0, 1.0 },
//                 },
//         };
//
//         multMat4xMat4(pProj, &currentModel, &mvp);
//         state->controlPanel.quadUniforms[0].mvp = mvp;
//
//         for (int i = 0; i < COLOR_HISTORY_LENGTH; i++) {
//                 int secondRow = 0;
//                 if (i >= 8) {
//                         secondRow = 1;
//                 }
//                 /* Remember that vulkan clip space is a range
//                 of -1.0 to 1.0.
//                  * Therefore the width of the viewport
//                  is 2.0. The scale of our
//                  * objects are in the range 0.0 to 1.0. So if
//                  we want move our
//                  * quad half of is width we calculate it as
//                  "scale * screenWidth
//                  * (2.0) / 2" which simplifies to scale.
//                  */
//                 Mat4 historyModel = {
//                         {
//                                 { historyScale, 0.0, 0.0, 0.0
//                                 }, { 0.0, historyScale, 0.0,
//                                 0.0 }, { 0.0, 0.0, 1.0, 0.0
//                                 }, { 1.0 - panelSize +
//                                 historyScale
//                                       + ((i - (secondRow *
//                                       8)) * historySize),
//                                   -(1 / aspect) + currentSize
//                                   + historyScale
//                                       + (secondRow *
//                                       historySize),
//                                   0.0, 1.0 },
//                         },
//                 };
//                 multMat4xMat4(pProj, &historyModel, &mvp);
//                 state->controlPanel.quadUniforms[i + 1].mvp =
//                 mvp;
//         }
//
//         int idx = COLOR_HISTORY_LENGTH + 1;
//
//         float previewY = -(1 / aspect) + currentSize + (3 *
//         historySize)
//                          + (previewSize / 2);
//         Mat4 previewModel = {
//                 {
//                         { previewSize / screenWidth, 0.0,
//                         0.0, 0.0 }, { 0.0, previewSize /
//                         screenWidth, 0.0, 0.0 }, { 0.0,
//                         0.0, 1.0, 0.0 }, { 1.0 - panelSize +
//                         previewSize, previewY, 0.0, 1.0
//                         },
//                 },
//         };
//         multMat4xMat4(pProj, &previewModel, &mvp);
//         state->controlPanel.quadUniforms[idx].mvp = mvp;
//         Vec3 currentColor =
//         state->controlPanel.quadUniforms[0].color;
//         state->controlPanel.quadUniforms[idx].color =
//         currentColor; idx++;
//
//         float lineWidth = 4.0 /
//         state->swapchain_extent.height; for (int i = 0; idx +
//         i < CONTROL_PANEL_QUAD_COUNT; i += 2) {
//                 float sliderY = previewY + previewSize + (i *
//                 historySize); Mat4 lineModel = {
//                         {
//                                 { sliderWidth / screenWidth,
//                                 0.0, 0.0, 0.0 }, { 0.0,
//                                 lineWidth, 0.0, 0.0 }, { 0.0,
//                                 0.0, 1.0, 0.0 }, { 1.0 -
//                                 panelScale, sliderY, 0.0, 1.0
//                                 },
//                         },
//                 };
//                 multMat4xMat4(pProj, &lineModel, &mvp);
//                 state->controlPanel.quadUniforms[idx + i].mvp
//                 = mvp; Vec3 lineColor = { 1.0, 1.0, 1.0 };
//                 state->controlPanel.quadUniforms[idx +
//                 i].color = lineColor;
//
//                 float buttonSize = lineWidth * 4;
//                 float leftEdge = 1.0 - panelScale -
//                 (sliderWidth / screenWidth); float sliderX =
//                 0.0; if (i == 0) {
//                         sliderX = leftEdge + (currentColor.x
//                         * sliderWidth);
//                 } else if (i == 2) {
//                         sliderX = leftEdge + (currentColor.y
//                         * sliderWidth);
//                 } else {
//                         sliderX = leftEdge + (currentColor.z
//                         * sliderWidth);
//                 }
//                 Mat4 buttonModel = {
//                         {
//                                 { buttonSize, 0.0, 0.0, 0.0
//                                 }, { 0.0, buttonSize, 0.0,
//                                 0.0 }, { 0.0, 0.0, 1.0, 0.0
//                                 }, { sliderX, sliderY,
//                                 0.0, 1.0 },
//                         },
//                 };
//                 multMat4xMat4(pProj, &buttonModel, &mvp);
//                 state->controlPanel.quadUniforms[idx + i +
//                 1].mvp = mvp;
//         }
// }

static int createControlPanelDescriptorSets(ControlPanel *self,
                                            VkDevice device) {
        VkDescriptorPoolSize poolSize = {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                .descriptorCount = MAX_FRAMES_IN_FLIGHT,
        };

        VkDescriptorPoolCreateInfo poolInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .poolSizeCount = 1,
                .pPoolSizes = &poolSize,
                .maxSets = MAX_FRAMES_IN_FLIGHT,
        };

        VkResult res = vkCreateDescriptorPool(device, &poolInfo, NULL,
                                              &self->descriptorPool);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create descriptor pool: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                layouts[i] = self->descriptorSetLayout;
        }

        VkDescriptorSetAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = self->descriptorPool,
                .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
                .pSetLayouts = layouts,
        };

        res = vkAllocateDescriptorSets(device, &allocInfo,
                                       self->descriptorSets);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to allocate descriptor sets: %s\n",
                        string_VkResult(res));
                return 0;
        }

        for (int frame = 0; frame < MAX_FRAMES_IN_FLIGHT; frame++) {
                VkDescriptorBufferInfo bufferInfo = {
                        .buffer = self->uniformBuffers[frame],
                        .offset = 0,
                        .range = self->alignedUniformSize,
                };

                VkWriteDescriptorSet descriptorWrite = {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = self->descriptorSets[frame],
                        .dstBinding = 0,
                        .dstArrayElement = 0,
                        .descriptorType
                        = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                        .descriptorCount = 1,
                        .pBufferInfo = &bufferInfo,
                };

                vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, NULL);
        }

        return 1;
}

static int createControlPanelGraphicsPipelines(ControlPanel *cp,
                                               VkDevice device,
                                               VkRenderPass renderPass) {

        VkDescriptorSetLayoutBinding uniformLayoutBinding = {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        };

        VkDescriptorSetLayoutCreateInfo descLayoutInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = 1,
                .pBindings = &uniformLayoutBinding,
        };

        VkResult res = vkCreateDescriptorSetLayout(
            device, &descLayoutInfo, NULL, &cp->descriptorSetLayout);
        if (res != VK_SUCCESS) {
                fprintf(stderr,
                        "Failed to create descriptor set "
                        "layout: %s\n",
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

        VkPipelineLayoutCreateInfo layoutInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 1,
                .pSetLayouts = &cp->descriptorSetLayout,
                .pushConstantRangeCount = 0,
        };
        res = vkCreatePipelineLayout(device, &layoutInfo, NULL,
                                     &cp->pipelineLayout);
        if (res != VK_SUCCESS) {
                fprintf(stderr,
                        "Failed to create quad pipeline "
                        "layout: %s\n",
                        string_VkResult(res));
                return 0;
        }

        GraphicsPipelineCreateInfo createInfo = {
                .device = device,
                .vertFile = "src/shaders/quadVert.spv",
                .fragFile = "src/shaders/quadFrag.spv",
                .vertexInputInfo = &vertexInputInfo,
                .primativeTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .blendAttachmentStatesCount = 1,
                .blendAttachmentStates = &colorBlendAttachment,
                .depthStencilState = NULL,
                .pipelineLayout = cp->pipelineLayout,
                .renderPass = renderPass,
        };
        cp->Pipeline = createGraphicsPipeline(&createInfo);

        createInfo.fragFile = "src/shaders/quadFrag.spv";
        cp->circlePipeline = createGraphicsPipeline(&createInfo);

        if (cp->Pipeline == VK_NULL_HANDLE
            || cp->circlePipeline == VK_NULL_HANDLE) {
                return 0;
        }

        return 1;
}

static int createControlPanel(glyph_state *state) {

        VkPhysicalDeviceProperties props = { 0 };
        vkGetPhysicalDeviceProperties(state->physical_device, &props);
        VkDeviceSize alignment = props.limits.minUniformBufferOffsetAlignment;
        state->controlPanel.alignedUniformSize
            = ((sizeof(ControlPanelUniform) + alignment - 1) / alignment)
              * alignment;

        UniformBufferCreateInfo uniformInfo = {
                .uniformSize = sizeof(state->controlPanel.quadUniforms),
                .device = state->device,
                .phyDevice = state->physical_device,
        };
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                uniformInfo.pBuffers[i]
                    = &state->controlPanel.uniformBuffers[i];
                uniformInfo.pMemory[i] = &state->controlPanel.uniformsMemory[i];
                uniformInfo.pMappedMemory[i]
                    = &state->controlPanel.uniformsMapped[i];
        }
        if (!createUniformBuffer(&uniformInfo)) {
                return 0;
        }

        if (!createControlPanelGraphicsPipelines(
                &state->controlPanel, state->device, state->render_pass)) {
                return 0;
        }

        if (!createControlPanelDescriptorSets(&state->controlPanel,
                                              state->device)) {
                return 0;
        }

        QuadVertexBufferRetrieveInfo vertexRetrieveInfo = {
                .vertexBuffer = &state->controlPanel.vertexBuffer,
                .vertexMemory = &state->controlPanel.vertexMemory,
                .device = state->device,
                .phyDevice = state->physical_device,
                .cmdPool = state->command_pool,
                .graphicsQueue = state->graphics_queue,
        };
        retrieveQuadVertexBuffer(&vertexRetrieveInfo);
        updateControlPanelUniformObjects(&state->controlPanel,
                                         state->swapchain_extent);
        // formatControlPanel(state);

        return 1;
}

static void destroyControlPanel(glyph_state *state) {
        VkDevice device = state->device;

        vkDestroyDescriptorSetLayout(
            device, state->controlPanel.descriptorSetLayout, NULL);
        vkDestroyDescriptorPool(device, state->controlPanel.descriptorPool,
                                NULL);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroyBuffer(device, state->controlPanel.uniformBuffers[i],
                                NULL);
                vkFreeMemory(device, state->controlPanel.uniformsMemory[i],
                             NULL);
        }

        vkDestroyPipelineLayout(device, state->controlPanel.pipelineLayout,
                                NULL);
        vkDestroyPipeline(device, state->controlPanel.Pipeline, NULL);
        vkDestroyPipeline(device, state->controlPanel.circlePipeline, NULL);
}
