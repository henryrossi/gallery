#include "buffer.h"
#include "glyph.h"
#include "graphicsPipeline.h"

#include <stdio.h>
#include <string.h>

typedef struct {
        float xPos;
        float yPos;
} QuadVertex;

// clang-format off
const QuadVertex quadVertices[] = {
        { -1.0f, -1.0f },
        { 1.0f, -1.0f, },
        { 1.0f, 1.0f },
        { -1.0f, 1.0f },
};
// clang-format on

// Configured at drawtime
// graphics pipeline
// vertex buffer
// index buffer (reusable)
// descriptor sets (for uniform buffers)

static int createQuadVertexBuffer(glyph_state *state) {
        VkDevice device = state->device;

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkDeviceSize size = sizeof(quadVertices);

        BufferCreateInfo stagingInfo = {
                .device = device,
                .buffer = &stagingBuffer,
                .memory = &stagingMemory,
                .size = size,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                .physical_device = state->physical_device,
        };
        if (!createBuffer(&stagingInfo)) {
                return 0;
        }

        void *data;
        vkMapMemory(device, stagingMemory, 0, size, 0, &data);
        memcpy(data, quadVertices, size);
        vkUnmapMemory(device, stagingMemory);

        BufferCreateInfo createInfo = {
                .device = device,
                .buffer = &state->quad.vertexBuffer,
                .memory = &state->quad.vertexMemory,
                .size = sizeof(quadVertices),
                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                         | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .physical_device = state->physical_device,
        };
        if (!createBuffer(&createInfo)) {
                return 0;
        }

        CopyBufferInfo copyInfo = {
                .device = device,
                .dst = state->quad.vertexBuffer,
                .src = stagingBuffer,
                .size = size,
                .cmdpool = state->command_pool,
                .graphics_queue = state->graphics_queue,
        };
        copyBuffer(&copyInfo);

        vkDestroyBuffer(device, stagingBuffer, NULL);
        vkFreeMemory(device, stagingMemory, NULL);

        return 1;
}

// clang-format off
quadUniform translation = {
        .scale = {
                0.4, 0.0, 0.0, 0.0,
                0.0, 0.3, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 0.0, 0.0, 1.0
        },
        .color = { 0.3, 0.1, 0.2 },
        .trans = { 0.5, 0.5 },

};
// clang-format on

static int createQuadUniformBuffer(glyph_state *state) {
        VkDeviceSize size = sizeof(quadUniform);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                BufferCreateInfo createInfo = {
                        .device = state->device,
                        .buffer = &state->quad.uniformBuffers[i],
                        .memory = &state->quad.uniformsMemory[i],
                        .size = size,
                        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        .props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                 | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        .physical_device = state->physical_device,
                };
                if (!createBuffer(&createInfo)) {
                        return 0;
                }

                vkMapMemory(state->device, state->quad.uniformsMemory[i], 0,
                            size, 0, &state->quad.uniformsMapped[i]);
                memcpy(state->quad.uniformsMapped[i], &translation, size);
        }

        return 1;
}

static int createQuadDescriptorSetLayout(glyph_state *state) {
        VkDescriptorSetLayoutBinding uniformLayoutBinding = {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .pImmutableSamplers = NULL,
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = 1,
                .pBindings = &uniformLayoutBinding,
        };

        VkResult res = vkCreateDescriptorSetLayout(
            state->device, &layoutInfo, NULL, &state->quad.descriptorSetLayout);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create descriptor set layout: %s\n",
                        string_VkResult(res));
                return 0;
        }
        return 1;
}

static int createQuadDescriptorPool(glyph_state *state) {
        VkDescriptorPoolSize poolSize = {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = MAX_FRAMES_IN_FLIGHT,
        };

        VkDescriptorPoolCreateInfo poolInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .poolSizeCount = 1,
                .pPoolSizes = &poolSize,
                .maxSets = MAX_FRAMES_IN_FLIGHT,
        };

        VkResult res = vkCreateDescriptorPool(state->device, &poolInfo, NULL,
                                              &state->quad.descriptorPool);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create descriptor pool: %s\n",
                        string_VkResult(res));
                return 0;
        }

        return 1;
}

static int createQuadDescriptorSets(glyph_state *state) {
        VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                layouts[i] = state->quad.descriptorSetLayout;
        }

        VkDescriptorSetAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = state->quad.descriptorPool,
                .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
                .pSetLayouts = layouts,
        };

        VkResult res = vkAllocateDescriptorSets(state->device, &allocInfo,
                                                state->quad.descriptorSets);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to allocate descriptor sets: %s\n",
                        string_VkResult(res));
                return 0;
        }

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                VkDescriptorBufferInfo bufferInfo = {
                        .buffer = state->quad.uniformBuffers[i],
                        .offset = 0,
                        .range = sizeof(quadUniform),
                };

                VkWriteDescriptorSet descriptorWrite = {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = state->quad.descriptorSets[i],
                        .dstBinding = 0,
                        .dstArrayElement = 0,
                        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        .descriptorCount = 1,
                        .pBufferInfo = &bufferInfo,
                };

                vkUpdateDescriptorSets(state->device, 1, &descriptorWrite, 0,
                                       NULL);
        }

        return 1;
}

static int createQuadGraphicsPipeline(glyph_state *state) {
        if (!createQuadUniformBuffer(state)) {
                return 0;
        }
        if (!createQuadDescriptorSetLayout(state)) {
                return 0;
        }
        if (!createQuadDescriptorPool(state)) {
                return 0;
        }
        if (!createQuadDescriptorSets(state)) {
                return 0;
        }

        VkPipelineLayoutCreateInfo layoutInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 1,
                .pSetLayouts = &state->quad.descriptorSetLayout,
                .pushConstantRangeCount = 0,
        };
        VkResult res = vkCreatePipelineLayout(state->device, &layoutInfo, NULL,
                                              &state->quad.pipelineLayout);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create quad pipeline layout: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkVertexInputBindingDescription vertexBindingDesc = {
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                .binding = 0,
                .stride = sizeof(QuadVertex),
        };
        VkVertexInputAttributeDescription vertexAttrDesc = {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = 0,
        };
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &vertexBindingDesc,
                .vertexAttributeDescriptionCount = 1,
                .pVertexAttributeDescriptions = &vertexAttrDesc,
        };

        GraphicsPipelineCreateInfo createInfo = {
                .device = state->device,
                .pipeline = &state->quad.grahpicsPipeline,
                .vertFile = "src/shaders/quadVert.spv",
                .fragFile = "src/shaders/quadFrag.spv",
                .layout = state->quad.pipelineLayout,
                .vertexInputInfo = &vertexInputInfo,
                .renderPass = state->render_pass,
        };

        createGraphicsPipeline(&createInfo);
        return 1;
}

static void destroyQuad(glyph_state *state) {
        VkDevice device = state->device;

        vkDestroyDescriptorSetLayout(device, state->quad.descriptorSetLayout,
                                     NULL);
        vkDestroyDescriptorPool(device, state->quad.descriptorPool, NULL);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroyBuffer(device, state->quad.uniformBuffers[i], NULL);
                vkFreeMemory(device, state->quad.uniformsMemory[i], NULL);
        }

        vkDestroyBuffer(device, state->quad.vertexBuffer, NULL);
        vkFreeMemory(device, state->quad.vertexMemory, NULL);

        vkDestroyPipelineLayout(device, state->quad.pipelineLayout, NULL);
        vkDestroyPipeline(device, state->quad.grahpicsPipeline, NULL);
}
