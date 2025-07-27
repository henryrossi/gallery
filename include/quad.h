#ifndef GLYPH_QUAD_H
#define GLYPH_QUAD_H

#include "glyph.h"
#include "matrix.h"

typedef struct {
  VkBuffer *vertexBuffer;
  VkDeviceMemory *vertexMemory;
  VkDevice device;
  VkPhysicalDevice phyDevice;
  VkCommandPool cmdPool;
  VkQueue graphicsQueue;
} QuadVertexBufferRetrieveInfo;

static int retrieveQuadVertexBuffer(QuadVertexBufferRetrieveInfo *info);

static void
getQuadPipelineVertexInputInfo(VkPipelineVertexInputStateCreateInfo *info);

typedef struct {
  Vec2 pos;
  Vec2 extent;
} BoundingBox;

static BoundingBox getQuadBoundingBox(Mat4 *mvp);

#endif // GLYPH_QUAD_H
