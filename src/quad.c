#include "quad.h"
#include "buffer.h"
#include "glyph.h"

#include <stdio.h>
#include <string.h>

// clang-format off
const Vec2 quadVertices[] = {
        { -0.5f, -0.5f },
        { 0.5f, -0.5f, },
        { 0.5f, 0.5f },
        { -0.5f, 0.5f },
};
// clang-format on

static BoundingBox getQuadBoundingBox(Mat4 *mvp) {
        Vec4 mpVertices[4] = { 0 };
        Vec2 vulkanCoords[4] = { 0 };

        for (int i = 0; i < 4; i++) {
                Vec4 quadPos
                    = { quadVertices[i].x, quadVertices[i].y, 0.0, 1.0 };
                multMat4xVec4(mvp, &quadPos, &mpVertices[i]);
                vulkanCoords[i].x = (mpVertices[i].x / mpVertices[i].w);
                vulkanCoords[i].y = (mpVertices[i].y / mpVertices[i].w);
        }

        BoundingBox bb = {
                .pos = { vulkanCoords[0].x, vulkanCoords[1].y },
                .extent = { vulkanCoords[1].x - vulkanCoords[0].x,
                            vulkanCoords[3].y - vulkanCoords[0].y },
        };
        return bb;
}

VkBuffer quadVertexBuffer = VK_NULL_HANDLE;
VkDeviceMemory quadVertexMemory = VK_NULL_HANDLE;

typedef struct {
        VkDevice device;
        VkPhysicalDevice phyDevice;
        VkCommandPool cmdPool;
        VkQueue graphicsQueue;
} QuadVertexBufferCreateInfo;

static int createQuadVertexBuffer(QuadVertexBufferCreateInfo *createInfo) {
        VkDevice device = createInfo->device;

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
                .physical_device = createInfo->phyDevice,
        };
        if (!createBuffer(&stagingInfo)) {
                return 0;
        }

        void *data;
        vkMapMemory(device, stagingMemory, 0, size, 0, &data);
        memcpy(data, quadVertices, size);
        vkUnmapMemory(device, stagingMemory);

        BufferCreateInfo bufferInfo = {
                .device = device,
                .buffer = &quadVertexBuffer,
                .memory = &quadVertexMemory,
                .size = sizeof(quadVertices),
                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                         | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .physical_device = createInfo->phyDevice,
        };
        if (!createBuffer(&bufferInfo)) {
                return 0;
        }

        CopyBufferInfo copyInfo = {
                .device = device,
                .dst = quadVertexBuffer,
                .src = stagingBuffer,
                .size = size,
                .cmdpool = createInfo->cmdPool,
                .graphics_queue = createInfo->graphicsQueue,
        };
        copyBuffer(&copyInfo);

        vkDestroyBuffer(device, stagingBuffer, NULL);
        vkFreeMemory(device, stagingMemory, NULL);

        return 1;
}

static int retrieveQuadVertexBuffer(QuadVertexBufferRetrieveInfo *info) {
        if (quadVertexBuffer == VK_NULL_HANDLE) {
                assert(quadVertexMemory == VK_NULL_HANDLE
                       && "quad vertex buffer is NULL but memory is not");

                QuadVertexBufferCreateInfo vertexInfo = {
                        .device = info->device,
                        .phyDevice = info->phyDevice,
                        .cmdPool = info->cmdPool,
                        .graphicsQueue = info->graphicsQueue,
                };
                if (!createQuadVertexBuffer(&vertexInfo)) {
                        return 0;
                }
        }

        *info->vertexBuffer = quadVertexBuffer;
        *info->vertexMemory = quadVertexMemory;

        return 1;
}

VkVertexInputBindingDescription vertexBindingDesc = {
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .binding = 0,
        .stride = sizeof(Vec2),
};
VkVertexInputAttributeDescription vertexAttrDesc = {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = 0,
};

static void
getQuadPipelineVertexInputInfo(VkPipelineVertexInputStateCreateInfo *info) {
        info->sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        info->vertexBindingDescriptionCount = 1;
        info->pVertexBindingDescriptions = &vertexBindingDesc;
        info->vertexAttributeDescriptionCount = 1;
        info->pVertexAttributeDescriptions = &vertexAttrDesc;
}

static void destroyQuadVertexBuffer(glyph_state *state) {
        vkDestroyBuffer(state->device, quadVertexBuffer, NULL);
        vkFreeMemory(state->device, quadVertexMemory, NULL);
}
