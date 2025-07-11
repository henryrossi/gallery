#include "buffer.h"
#include "graphicsPipeline.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int createCanvasGraphicsPipeline(glyph_state *state) {
        // Render Pass needs to be create before this function.

        VkVertexInputBindingDescription vertexBindingDesc
            = get_vertex_binding_desc();

        VkVertexInputAttributeDescription vertexAttrDesc[2] = {
                get_vertex_attr_desc_pos(),
                get_vertex_attr_desc_tex_coord(),
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &vertexBindingDesc,
                .vertexAttributeDescriptionCount = 2,
                .pVertexAttributeDescriptions = vertexAttrDesc,
        };

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

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pSetLayouts = &state->canvas.descriptorSetLayout,
                .setLayoutCount = 1,
                .pushConstantRangeCount = 0,
        };

        VkResult res
            = vkCreatePipelineLayout(state->device, &pipelineLayoutInfo, NULL,
                                     &state->canvas.pipelineLayout);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create pipeline layout: %s\n",
                        string_VkResult(res));
                return 0;
        }

        GraphicsPipelineCreateInfo createInfo = {
                .device = state->device,
                .vertFile = "src/shaders/vert.spv",
                .fragFile = "src/shaders/frag.spv",
                .vertexInputInfo = &vertexInputInfo,
                .primativeTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .blendAttachmentStatesCount = 1,
                .blendAttachmentStates = &colorBlendAttachment,
                .depthStencilState = NULL,
                .pipelineLayout = state->canvas.pipelineLayout,
                .renderPass = state->render_pass,
        };
        state->canvas.pipeline = createGraphicsPipeline(&createInfo);
        if (state->canvas.pipeline == VK_NULL_HANDLE) {
                return 0;
        }

        return 1;
}

static int createCanvasSampler(glyph_state *state) {
        VkSamplerCreateInfo sampler_info = {
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = 0,
                .minFilter = 0,
                .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .anisotropyEnable = VK_FALSE,
                .maxAnisotropy = 1.0f,
                .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
                .unnormalizedCoordinates = VK_FALSE,
                .compareEnable = VK_FALSE,
                .compareOp = VK_COMPARE_OP_ALWAYS,
                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                .mipLodBias = 0.0f,
                .minLod = 0.0f,
                .maxLod = 0.0f,
        };

        VkResult res = vkCreateSampler(state->device, &sampler_info, NULL,
                                       &state->canvas.imageSampler);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create texture sampler: %s\n",
                        string_VkResult(res));
                return 0;
        }
        return 1;
}

static int createCanvasImageViews(glyph_state *state) {
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                if (!createImageView(state->device, state->canvas.image[i],
                                     VK_FORMAT_R8G8B8A8_UNORM,
                                     &state->canvas.imageView[i])) {
                        return 0;
                }
        }
        return 1;
}

static void writeCanvasDataToImage(glyph_state *state, uint32_t currentFrame) {
        Canvas canvas = state->canvas;
        memcpy(canvas.mappedStagingImages[currentFrame], canvas.data,
               canvas.size);

        TransitionImageLayoutInfo transInfo = {
                .image = canvas.image[currentFrame],
                .format = VK_FORMAT_R8G8B8A8_SRGB,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .device = state->device,
                .cmdPool = state->command_pool,
                .graphicsQueue = state->graphics_queue,
        };
        transitionImageLayout(&transInfo);

        CopyBufferToImageInfo copyInfo = {
                .buffer = canvas.stagingImageBuffers[currentFrame],
                .image = canvas.image[currentFrame],
                .width = canvas.width,
                .height = canvas.height,
                .device = state->device,
                .cmdPool = state->command_pool,
                .graphicsQueue = state->graphics_queue,
        };
        copyBufferToImage(&copyInfo);

        transInfo.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        transInfo.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        transitionImageLayout(&transInfo);
}

static int saveCanvasToPNG(Canvas *canvas) {
        return stbi_write_png(canvas->filename, canvas->width, canvas->height,
                              4, canvas->data, canvas->width * 4);
}

static int readCanvasInputFile(Canvas *canvas) {
        int w, h, n;
        uint8_t *data = stbi_load(canvas->filename, &w, &h, &n, 0);
        if (!data) {
                fprintf(stderr, "Failed to read input file: %s\n",
                        canvas->filename);
                return 0;
        }
        canvas->width = w;
        canvas->height = h;
        canvas->size = w * h * 4;
        canvas->data = malloc(canvas->size);
        memcpy(canvas->data, data, canvas->size);

        stbi_image_free(data);
        return 1;
}

// Creates a texture image. Returns 1 on success, 0 on failure.
static int createCanvas(glyph_state *state) {
        Canvas *canvas = &state->canvas;
        if (canvas->fileCreated) {
                canvas->size = canvas->width * canvas->height * 4;
                canvas->data = malloc(canvas->size);
                for (uint32_t i = 0; i < canvas->size; i++) {
                        canvas->data[i] = 255;
                }
        } else {
                if (!readCanvasInputFile(&state->canvas)) {
                        return 0;
                }
        }

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                BufferCreateInfo bufferInfo = {
                        .size = canvas->size,
                        .buffer = &canvas->stagingImageBuffers[i],
                        .memory = &canvas->stagingImagesMemory[i],
                        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        .props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                 | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        .device = state->device,
                        .physical_device = state->physical_device,
                };
                if (!createBuffer(&bufferInfo)) {
                        return 0;
                }

                vkMapMemory(state->device, canvas->stagingImagesMemory[i], 0,
                            canvas->size, 0, &canvas->mappedStagingImages[i]);

                ImageCreateInfo imageInfo = {
                        .width = canvas->width,
                        .height = canvas->height,
                        .format = VK_FORMAT_R8G8B8A8_UNORM,
                        .tiling = VK_IMAGE_TILING_LINEAR,
                        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                 | VK_IMAGE_USAGE_SAMPLED_BIT,
                        .props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        .image = &canvas->image[i],
                        .imageMemory = &canvas->imageMemory[i],
                        .device = state->device,
                        .physicalDevice = state->physical_device,
                };
                if (!createImage(&imageInfo)) {
                        return 0;
                }
        }

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                writeCanvasDataToImage(state, i);
        }

        if (!createCanvasImageViews(state)) {
                return 0;
        }

        if (!createCanvasSampler(state)) {
                return 0;
        }

        return 1;
}

static void destroyCanvas(glyph_state *state) {

        VkDevice device = state->device;
        Canvas canvas = state->canvas;

        vkDestroySampler(device, canvas.imageSampler, NULL);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroyBuffer(device, canvas.stagingImageBuffers[i], NULL);
                vkFreeMemory(device, canvas.stagingImagesMemory[i], NULL);
                vkDestroyImageView(device, canvas.imageView[i], NULL);
                vkDestroyImage(device, canvas.image[i], NULL);
                vkFreeMemory(device, canvas.imageMemory[i], NULL);
        }
}
