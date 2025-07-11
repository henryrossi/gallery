#include "buffer.h"
#include "glyph.h"
#include "graphicsPipeline.h"
#include "matrix.h"
#include "quad.h"
#include "vulkan/vulkan_core.h"

#include <stdio.h>
#include <string.h>

const float screenWidth = 2.0;
const float panelScale = 0.25;
const float panelSize = panelScale * screenWidth;

const float currentScale = panelScale;
const float currentSize = panelSize;

const float historyScale = panelScale / 8;
const float historySize = historyScale * screenWidth;

const float previewSize = panelSize / 4;

const float sliderWidth = panelSize * 0.9;

const Mat4 currentColorScale = { {
        { 0.4, 0.0, 0.0, 0.0 },
        { 0.0, 0.4, 0.0, 0.0 },
        { 0.0, 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 0.0, 1.0 },
} };

const Mat4 historyColorScale = { {
        { 0.2, 0.0, 0.0, 0.0 },
        { 0.0, 0.2, 0.0, 0.0 },
        { 0.0, 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 0.0, 1.0 },
} };

const Mat4 startLineModel = { {
        { 1.0, 0.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0, 0.0 },
        { 0.0, 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 0.0, 1.0 },
} };

// clang-format off
ControlPanelUniform start[CONTROL_PANEL_QUAD_COUNT] = {
        { currentColorScale, {}, { 1.0, 0.5, 0.0 }, },
        { historyColorScale, {}, { 0.2, 1.0, 0.4 }, },
        { historyColorScale, {}, { 0.2, 0.8, 0.6 }, },
        { historyColorScale, {}, { 0.2, 0.3, 1.0 }, },
        { historyColorScale, {}, { 0.5, 0.3, 0.6 }, },
        { historyColorScale, {}, { 1.0, 0.3, 0.4 }, },
        { historyColorScale, {}, { 0.2, 0.3, 0.4 }, },
        { historyColorScale, {}, { 0.2, 0.3, 0.4 }, },
        { historyColorScale, {}, { 0.2, 0.3, 0.4 }, },
        { historyColorScale, {}, { 0.2, 0.3, 0.4 }, },
        { historyColorScale, {}, { 0.2, 0.3, 0.4 }, },
        { historyColorScale, {}, { 0.2, 0.3, 0.4 }, },
        { historyColorScale, {}, { 0.2, 0.3, 0.4 }, },
        { historyColorScale, {}, { 0.2, 0.3, 0.4 }, },
        { historyColorScale, {}, { 0.2, 0.3, 0.4 }, },
        { historyColorScale, {}, { 0.0, 0.0, 0.0 }, },
        { historyColorScale, {}, { 0.2, 0.0, 0.0 }, },
        { startLineModel, {}, { 1.0, 1.0, 1.0 }, },
};
// clang-format on

static void calcControlPanelProjectionMatrix(ControlPanel *panel,
                                             VkExtent2D swapchainExtent) {
        float aspect
            = (float)swapchainExtent.width / (float)swapchainExtent.height;
        float left = -1.0;
        float right = 1.0;
        float bottom = -1.0 / aspect;
        float top = 1.0 / aspect;
        float near = 0.0;
        float far = 1.0;

        // clang-format off
        Mat4 ortho = {
                {
                        { 2.0 / (right - left), 0.0, 0.0, -(right + left) / (right - left) },
                        { 0.0, 2.0 / (top - bottom), 0.0f, -(top + bottom) / (top - bottom) },
                        { 0.0, 0.0, 1.0 / (far - near), -near / (far - near) },
                        { 0.0, 0.0, 0.0, 1.0 },
                },
        };
        // clang-format on
        transposeMat4(&ortho);

        for (int i = 0; i < CONTROL_PANEL_QUAD_COUNT; i++) {
                memcpy(&panel->quadUniforms[i].proj, &ortho, sizeof(Mat4));
        }
}
static void pushCreatedColorToCurrent(ControlPanel *cp) {
        for (int i = CONTROL_PANEL_HISTORY_16; i > 0; i--) {
                cp->quadUniforms[i].color = cp->quadUniforms[i - 1].color;
        }
        cp->quadUniforms[0].color
            = cp->quadUniforms[CONTROL_PANEL_CREATOR_PREVIEW].color;
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
            = getQuadBoundingBox(&panel->quadUniforms[clicked - 1].model,
                                 &panel->quadUniforms[clicked - 1].proj);

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

static void drawControlPanel(glyph_state *state, VkCommandBuffer cmdBuffer,
                             uint32_t indicesSize) {

        calcControlPanelProjectionMatrix(&state->controlPanel,
                                         state->swapchain_extent);

        memcpy(state->controlPanel.uniformsMapped[state->current_frame],
               state->controlPanel.quadUniforms,
               sizeof(state->controlPanel.quadUniforms));

        VkDeviceSize offset = 0;
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          state->controlPanel.Pipeline);
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1,
                               &state->controlPanel.vertexBuffer, &offset);

        for (int i = 0; i < CONTROL_PANEL_QUAD_COUNT; i++) {
                uint32_t uniformOffset
                    = i * state->controlPanel.alignedUniformSize;

                vkCmdBindDescriptorSets(
                    cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    state->controlPanel.pipelineLayout, 0, 1,
                    &state->controlPanel.descriptorSets[state->current_frame],
                    1, &uniformOffset);

                vkCmdDrawIndexed(cmdBuffer, indicesSize, 1, 0, 0, 0);
        }
}

static void
updateCreatorButtonModelMatrix(ControlPanel *panel, VkExtent2D swapchainExtent,
                               GlyphControlPanelUniformIndex color) {
        float aspect
            = (float)swapchainExtent.width / (float)swapchainExtent.height;

        float barWidth = 4.0 / swapchainExtent.height;
        float buttonSize = barWidth * 4;
        float leftEdge = 1.0 - panelScale - (sliderWidth / screenWidth);
        float sliderX = 0.0;
        float sliderY = -(1 / aspect) + currentSize + (3 * historySize)
                        + (previewSize * 1.5);

        Vec3 previewColor
            = panel->quadUniforms[CONTROL_PANEL_CREATOR_PREVIEW].color;
        if (color == CONTROL_PANEL_CREATOR_R_BUTTON) {
                sliderX = leftEdge + (previewColor.x * sliderWidth);
        } else if (color == CONTROL_PANEL_CREATOR_G_BUTTON) {
                sliderX = leftEdge + (previewColor.y * sliderWidth);
                sliderY += 2 * historySize;
        } else if (color == CONTROL_PANEL_CREATOR_B_BUTTON) {
                sliderX = leftEdge + (previewColor.z * sliderWidth);
                sliderY += 4 * historySize;
        }
        Mat4 buttonModel = {
                {
                        { buttonSize, 0.0, 0.0, 0.0 },
                        { 0.0, buttonSize, 0.0, 0.0 },
                        { 0.0, 0.0, 1.0, 0.0 },
                        { sliderX, sliderY, 0.0, 1.0 },
                },
        };
        panel->quadUniforms[color].model = buttonModel;
}

static void formatControlPanel(glyph_state *state) {
        float aspect = (float)state->swapchain_extent.width
                       / (float)state->swapchain_extent.height;
        Mat4 currentModel = {
                {
                        { currentScale, 0.0, 0.0, 0.0 },
                        { 0.0, currentScale, 0.0, 0.0 },
                        { 0.0, 0.0, 1.0, 0.0 },
                        { 1.0 - panelSize + currentScale,
                          -(1 / aspect) + currentScale, 0.0, 1.0 },
                },
        };

        state->controlPanel.quadUniforms[0].model = currentModel;

        for (int i = 0; i < COLOR_HISTORY_LENGTH; i++) {
                int secondRow = 0;
                if (i >= 8) {
                        secondRow = 1;
                }
                /* Remember that vulkan clip space is a range of -1.0 to 1.0.
                 * Therefore the width of the viewport is 2.0. The scale of our
                 * objects are in the range 0.0 to 1.0. So if we want move our
                 * quad half of is width we calculate it as "scale * screenWidth
                 * (2.0) / 2" which simplifies to scale.
                 */
                Mat4 histroyModel = {
                        {
                                { historyScale, 0.0, 0.0, 0.0 },
                                { 0.0, historyScale, 0.0, 0.0 },
                                { 0.0, 0.0, 1.0, 0.0 },
                                { 1.0 - panelSize + historyScale
                                      + ((i - (secondRow * 8)) * historySize),
                                  -(1 / aspect) + currentSize + historyScale
                                      + (secondRow * historySize),
                                  0.0, 1.0 },
                        },
                };
                state->controlPanel.quadUniforms[i + 1].model = histroyModel;
        }

        int idx = COLOR_HISTORY_LENGTH + 1;

        float previewY = -(1 / aspect) + currentSize + (3 * historySize)
                         + (previewSize / 2);
        Mat4 previewModel = {
                {
                        { previewSize / screenWidth, 0.0, 0.0, 0.0 },
                        { 0.0, previewSize / screenWidth, 0.0, 0.0 },
                        { 0.0, 0.0, 1.0, 0.0 },
                        { 1.0 - panelSize + previewSize, previewY, 0.0, 1.0 },
                },
        };
        state->controlPanel.quadUniforms[idx].model = previewModel;
        Vec3 currentColor = state->controlPanel.quadUniforms[0].color;
        state->controlPanel.quadUniforms[idx].color = currentColor;
        idx++;

        float lineWidth = 4.0 / state->swapchain_extent.height;
        for (int i = 0; idx + i < CONTROL_PANEL_QUAD_COUNT; i += 2) {
                float sliderY = previewY + previewSize + (i * historySize);
                Mat4 lineModel = {
                        {
                                { sliderWidth / screenWidth, 0.0, 0.0, 0.0 },
                                { 0.0, lineWidth, 0.0, 0.0 },
                                { 0.0, 0.0, 1.0, 0.0 },
                                { 1.0 - panelScale, sliderY, 0.0, 1.0 },
                        },
                };
                state->controlPanel.quadUniforms[idx + i].model = lineModel;
                Vec3 lineColor = { 1.0, 1.0, 1.0 };
                state->controlPanel.quadUniforms[idx + i].color = lineColor;

                // { 1.0, 0.5, 0.0 }
                float buttonSize = lineWidth * 4;
                float leftEdge = 1.0 - panelScale - (sliderWidth / screenWidth);
                float sliderX = 0.0;
                if (i == 0) {
                        sliderX = leftEdge + (currentColor.x * sliderWidth);
                } else if (i == 2) {
                        sliderX = leftEdge + (currentColor.y * sliderWidth);
                } else {
                        sliderX = leftEdge + (currentColor.z * sliderWidth);
                }
                Mat4 buttonModel = {
                        {
                                { buttonSize, 0.0, 0.0, 0.0 },
                                { 0.0, buttonSize, 0.0, 0.0 },
                                { 0.0, 0.0, 1.0, 0.0 },
                                { sliderX, sliderY, 0.0, 1.0 },
                        },
                };
                state->controlPanel.quadUniforms[idx + i + 1].model
                    = buttonModel;
        }
}

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

static int createControlPanelGraphicsPipelines(ControlPanel *self,
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
            device, &descLayoutInfo, NULL, &self->descriptorSetLayout);
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

        VkPipelineLayoutCreateInfo layoutInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 1,
                .pSetLayouts = &self->descriptorSetLayout,
                .pushConstantRangeCount = 0,
        };
        res = vkCreatePipelineLayout(device, &layoutInfo, NULL,
                                     &self->pipelineLayout);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create quad pipeline layout: %s\n",
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
                .pipelineLayout = self->pipelineLayout,
                .renderPass = renderPass,
        };
        self->Pipeline = createGraphicsPipeline(&createInfo);

        createInfo.fragFile = "src/shaders/quadFrag.spv";
        self->circlePipeline = createGraphicsPipeline(&createInfo);

        if (self->Pipeline == VK_NULL_HANDLE
            || self->circlePipeline == VK_NULL_HANDLE) {
                return 0;
        }

        return 1;
}

static int createControlPanel(glyph_state *state) {
        memcpy(&state->controlPanel.quadUniforms, &start, sizeof(start));

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

        formatControlPanel(state);

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
