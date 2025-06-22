#include "buffer.h"
#include "graphicsPipeline.h"
#include "vulkan/vulkan_core.h"

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
                .buffer = &state->Quad.vertexBuffer,
                .memory = &state->Quad.vertexMemory,
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
                .dst = state->Quad.vertexBuffer,
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

static int createQuadGraphicsPipeline(glyph_state *state) {
        VkPipelineLayoutCreateInfo layoutInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 0,
                .pushConstantRangeCount = 0,
                .flags = 0,
        };
        VkResult res = vkCreatePipelineLayout(state->device, &layoutInfo, NULL,
                                              &state->Quad.pipelineLayout);
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
                .pipeline = &state->Quad.grahpicsPipeline,
                .vertFile = "src/shaders/quadVert.spv",
                .fragFile = "src/shaders/quadFrag.spv",
                .layout = state->Quad.pipelineLayout,
                .vertexInputInfo = &vertexInputInfo,
                .renderPass = state->render_pass,
        };

        createGraphicsPipeline(&createInfo);
        return 1;
}

static void destroyQuad(glyph_state *state) {
        VkDevice device = state->device;

        vkDestroyBuffer(device, state->Quad.vertexBuffer, NULL);
        vkFreeMemory(device, state->Quad.vertexMemory, NULL);

        vkDestroyPipelineLayout(device, state->Quad.pipelineLayout, NULL);
        vkDestroyPipeline(device, state->Quad.grahpicsPipeline, NULL);
}
