#ifndef GLYPH_GRAPHICS_PIPELINE_H
#define GLYPH_GRAPHICS_PIPELINE_H

#include "glyph.h"

typedef struct {
        const VkDevice device;
        // PipelineCache
        const char *vertFile;
        const char *fragFile;
        VkPipelineVertexInputStateCreateInfo *vertexInputInfo;
        VkPrimitiveTopology primativeTopology;
        VkPolygonMode polygonMode;
        VkCullModeFlags cullMode;
        VkFrontFace frontFace;
        uint32_t blendAttachmentStatesCount;
        VkPipelineColorBlendAttachmentState *blendAttachmentStates;
        VkPipelineDepthStencilStateCreateInfo *depthStencilState;
        VkPipelineLayout pipelineLayout;
        VkRenderPass renderPass;
} GraphicsPipelineCreateInfo;

static VkPipeline createGraphicsPipeline(GraphicsPipelineCreateInfo *createInfo);
 

#endif // GLYPH_GRAPHICS_PIPELINE_H
