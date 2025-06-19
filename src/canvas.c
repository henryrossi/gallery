#include "buffer.h"
#include "glyph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
                                     VK_FORMAT_R8G8B8A8_SRGB,
                                     &state->canvas.imageView[i])) {
                        return 0;
                }
        }
        return 1;
}

// Creates a texture image. Returns 1 on success, 0 on failure.
static int createCanvas(glyph_state *state) {
        Canvas canvas = {
                .size = tex_w * tex_h * 4,
                .width = tex_w,
                .height = tex_h,
        };

        // read image data from disk
        uint8_t *pixels = calloc(canvas.size, sizeof(uint8_t));
        for (int y = 0; y < tex_h; y++) {
                for (int x = 0; x < tex_w; x++) {
                        pixels[y * tex_h * 4 + x * 4] = (uint8_t)y + 64;
                }
        }

        // int texWidth, texHeight, texChannels;
        // stbi_uc *pixels = stbi_load("test.jpg", &texWidth, &texHeight,
        //                             &texChannels, STBI_rgb_alpha);
        // VkDeviceSize size = texWidth * texHeight * 4;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                BufferCreateInfo bufferInfo = {
                        .size = canvas.size,
                        .buffer = &canvas.stagingBuffer[i],
                        .memory = &canvas.stagingMemory[i],
                        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        .props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                 | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        .device = state->device,
                        .physical_device = state->physical_device,
                };
                if (!createBuffer(&bufferInfo)) {
                        return 0;
                }

                vkMapMemory(state->device, canvas.stagingMemory[i], 0,
                            canvas.size, 0, &canvas.mappedMemory[i]);

                ImageCreateInfo imageInfo = {
                        .width = tex_w,
                        .height = tex_h,
                        .format = VK_FORMAT_R8G8B8A8_SRGB,
                        .tiling = VK_IMAGE_TILING_LINEAR,
                        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                 | VK_IMAGE_USAGE_SAMPLED_BIT,
                        .props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                        .image = &canvas.image[i],
                        .imageMemory = &canvas.imageMemory[i],
                        .device = state->device,
                        .physicalDevice = state->physical_device,
                };
                if (!createImage(&imageInfo)) {
                        return 0;
                }

                // pull into write to canvas image function
                memcpy(canvas.mappedMemory[i], pixels, canvas.size);

                TransitionImageLayoutInfo transInfo = {
                        .image = canvas.image[i],
                        .format = VK_FORMAT_R8G8B8A8_SRGB,
                        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .device = state->device,
                        .cmdPool = state->command_pool,
                        .graphicsQueue = state->graphics_queue,
                };
                transitionImageLayout(&transInfo);

                CopyBufferToImageInfo copyInfo = {
                        .buffer = canvas.stagingBuffer[i],
                        .image = canvas.image[i],
                        .width = tex_w,
                        .height = tex_h,
                        .device = state->device,
                        .cmdPool = state->command_pool,
                        .graphicsQueue = state->graphics_queue,
                };
                copyBufferToImage(&copyInfo);

                transInfo.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                transInfo.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                transitionImageLayout(&transInfo);
        }
        state->canvas = canvas;

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
                vkDestroyBuffer(device, canvas.stagingBuffer[i], NULL);
                vkFreeMemory(device, canvas.stagingMemory[i], NULL);
                vkDestroyImageView(device, canvas.imageView[i], NULL);
                vkDestroyImage(device, canvas.image[i], NULL);
                vkFreeMemory(device, canvas.imageMemory[i], NULL);
        }
}
