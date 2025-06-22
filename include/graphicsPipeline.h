#ifndef GLYPH_GRAPHICS_PIPELINE_H
#define GLYPH_GRAPHICS_PIPELINE_H

#include "glyph.h"

typedef struct {
        const VkDevice device;
        VkPipeline *pipeline;
        const char *vertFile;
        const char *fragFile;
        const VkPipelineLayout layout;
        const VkRenderPass renderPass;
        const VkPipelineVertexInputStateCreateInfo *vertexInputInfo;
} GraphicsPipelineCreateInfo;

static int createGraphicsPipeline(GraphicsPipelineCreateInfo *createInfo);
 

#endif // GLYPH_GRAPHICS_PIPELINE_H
