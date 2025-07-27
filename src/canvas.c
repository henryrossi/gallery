#include "buffer.h"
#include "coordTransform.h"
#include "glyph.h"
#include "graphicsPipeline.h"
#include "vulkan/vulkan_core.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
        float pos[2];
        float tex_coord[2];
} canvasVertex;

const canvasVertex vertices[] = {
        { { -0.5f, -0.5f }, { 0.0f, 0.0f } },
        { { 0.5f, -0.5f }, { 1.0f, 0.0f } },
        { { 0.5f, 0.5f }, { 1.0f, 1.0f } },
        { { -0.5f, 0.5f }, { 0.0f, 1.0f } },
};

static void updateCanvasUniformObject(Canvas *c, VkExtent2D screen) {
        c->scale.x = ((float)screen.width) * 0.75;
        c->scale.y = ((float)screen.height) * 0.75;
        c->scale.z = 1.0;
        c->pos.x = -((float)screen.width) * 0.125;

        getPosCoordTransform(&c->scale, &c->pos, screen, true, &c->uniform.mvp);
}

static void drawCanvas(Canvas *c, VkCommandBuffer cmdBuffer, uint32_t frame,
                       VkExtent2D *swapchainExtent, uint32_t indicesSize) {
        memcpy(c->uniformBuffersMapped[frame], &c->uniform, sizeof(c->uniform));

        VkDeviceSize offset = 0;

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          c->pipeline);
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &c->vertexBuffer, &offset);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                c->pipelineLayout, 0, 1,
                                &c->descriptorSets[frame], 0, NULL);

        vkCmdDrawIndexed(cmdBuffer, indicesSize, 1, 0, 0, 0);
}

// Create vertex buffers. Returns 1 on success, 0 on failure.
static int create_vertex_buffer(glyph_state *state) {
        VkDevice device = state->device;
        VkDeviceSize size = sizeof(vertices);

        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
        BufferCreateInfo staging = {
                .device = device,
                .physical_device = state->physical_device,
                .size = size,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                .buffer = &staging_buffer,
                .memory = &staging_buffer_memory,
        };
        if (!createBuffer(&staging)) {
                return 0;
        }

        void *data;
        vkMapMemory(device, staging_buffer_memory, 0, size, 0, &data);
        memcpy(data, vertices, size);
        vkUnmapMemory(device, staging_buffer_memory);

        BufferCreateInfo vertex = {
                .device = device,
                .physical_device = state->physical_device,
                .size = size,
                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                         | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .buffer = &state->canvas.vertexBuffer,
                .memory = &state->canvas.vertexBufferMemory,
        };
        if (!createBuffer(&vertex)) {
                return 0;
        }

        CopyBufferInfo params = {
                .device = device,
                .graphics_queue = state->graphics_queue,
                .cmdpool = state->command_pool,
                .src = staging_buffer,
                .dst = state->canvas.vertexBuffer,
                .size = size,
        };
        copyBuffer(&params);

        vkDestroyBuffer(device, staging_buffer, NULL);
        vkFreeMemory(device, staging_buffer_memory, NULL);

        return 1;
}

static int create_descriptor_set_layout(glyph_state *state) {
        VkDescriptorSetLayoutBinding tex_layout_binding = {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = NULL,
        };

        VkDescriptorSetLayoutBinding ubo_layout_binding = {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .pImmutableSamplers = NULL,
        };

        VkDescriptorSetLayoutBinding layout_bindings[2] = {
                tex_layout_binding,
                ubo_layout_binding,
        };

        VkDescriptorSetLayoutCreateInfo layout_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = 2,
                .pBindings = layout_bindings,
        };

        VkResult res
            = vkCreateDescriptorSetLayout(state->device, &layout_info, NULL,
                                          &state->canvas.descriptorSetLayout);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create descriptor set layout: %s\n",
                        string_VkResult(res));
                return 0;
        }
        return 1;
}

static int create_descriptor_pool(glyph_state *state) {
        VkDescriptorPoolSize pool_sizes[MAX_FRAMES_IN_FLIGHT] = {
                {
                        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .descriptorCount = MAX_FRAMES_IN_FLIGHT,
                },
                {
                        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        .descriptorCount = MAX_FRAMES_IN_FLIGHT,
                },
        };

        VkDescriptorPoolCreateInfo pool_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .poolSizeCount = 2,
                .pPoolSizes = pool_sizes,
                .maxSets = MAX_FRAMES_IN_FLIGHT,
        };

        VkResult res = vkCreateDescriptorPool(state->device, &pool_info, NULL,
                                              &state->canvas.descriptorPool);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create descriptor pool: %s\n",
                        string_VkResult(res));
                return 0;
        }

        return 1;
}

static int create_descriptor_sets(glyph_state *state) {
        VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                layouts[i] = state->canvas.descriptorSetLayout;
        }

        VkDescriptorSetAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = state->canvas.descriptorPool,
                .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
                .pSetLayouts = layouts,
        };

        VkResult res = vkAllocateDescriptorSets(state->device, &alloc_info,
                                                state->canvas.descriptorSets);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to allocate descriptor sets: %s\n",
                        string_VkResult(res));
                return 0;
        }

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                VkDescriptorImageInfo image_info = {
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        .imageView = state->canvas.imageView[i],
                        .sampler = state->canvas.imageSampler,
                };

                VkDescriptorBufferInfo buffer_info = {
                        .buffer = state->canvas.uniformBuffers[i],
                        .offset = 0,
                        .range = sizeof(state->canvas.uniform),
                };

                VkWriteDescriptorSet descriptor_write[2] = {
                        {
                                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                .dstSet = state->canvas.descriptorSets[i],
                                .dstBinding = 0,
                                .dstArrayElement = 0,
                                .descriptorType
                                = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                .descriptorCount = 1,
                                .pImageInfo = &image_info,
                        },
                        {
                                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                .dstSet = state->canvas.descriptorSets[i],
                                .dstBinding = 1,
                                .dstArrayElement = 0,
                                .descriptorType
                                = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                .descriptorCount = 1,
                                .pBufferInfo = &buffer_info,
                        },
                };

                vkUpdateDescriptorSets(state->device, 2, descriptor_write, 0,
                                       NULL);
        }

        return 1;
}

static int create_uniform_buffer(glyph_state *state) {
        VkDeviceSize size = sizeof(state->canvas.uniform);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                BufferCreateInfo params = {
                        .size = size,
                        .buffer = &state->canvas.uniformBuffers[i],
                        .memory = &state->canvas.uniformBuffersMemory[i],
                        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        .props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                 | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        .device = state->device,
                        .physical_device = state->physical_device,
                };
                if (!createBuffer(&params)) {
                        return 0;
                }

                vkMapMemory(state->device,
                            state->canvas.uniformBuffersMemory[i], 0, size, 0,
                            &state->canvas.uniformBuffersMapped[i]);
        }

        return 1;
}
static int createCanvasGraphicsPipeline(glyph_state *state) {
        // Render Pass needs to be create before this function.

        VkVertexInputBindingDescription vertexBindingDesc = {
                .binding = 0,
                .stride = sizeof(canvasVertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
        VkVertexInputAttributeDescription vertexAttrDesc[2] = {
                {
                        .binding = 0,
                        .location = 0,
                        .format = VK_FORMAT_R32G32_SFLOAT,
                        .offset = offsetof(canvasVertex, pos),
                },
                {
                        .binding = 0,
                        .location = 1,
                        .format = VK_FORMAT_R32G32_SFLOAT,
                        .offset = offsetof(canvasVertex, tex_coord),
                },
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

        updateCanvasUniformObject(canvas, state->swapchain_extent);

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
