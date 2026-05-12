#include "render/vulkan/vulkan_render.h"
#include "render/render_core.h"

#include "os/os.h"

/* Vulkan validation layer and debug extension */
#ifdef VALIDATION_LAYERS
static u32 r_validation_layers_enabled = 1;
#else
static u32 r_validation_layers_enabled = 0;
#endif

static const char *r_validation_layers[] = {
        "VK_LAYER_KHRONOS_validation",
};
u32 r_validation_layer_count = array_count(r_validation_layers);

VkDebugUtilsMessengerEXT r_debug_messenger;

// Proxy functions for debug extension
static VkResult r_create_debug_utils_messenger_ext(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
        PFN_vkCreateDebugUtilsMessengerEXT func
            = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != NULL) {
                return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        } else {
                return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
}

// Proxy function for debug extension
static void
r_destroy_debug_utils_messenger_ext(VkInstance instance,
                                    VkDebugUtilsMessengerEXT debugMessenger,
                                    const VkAllocationCallbacks *pAllocator) {
        PFN_vkDestroyDebugUtilsMessengerEXT func
            = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != NULL) {
                func(instance, debugMessenger, pAllocator);
        }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
r_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                 VkDebugUtilsMessageTypeFlagsEXT messageType,
                 const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                 void *pUserData) {
        fprintf(stderr, "Validation Layer: %s\n", pCallbackData->pMessage);
        return VK_FALSE;
}

static void r_populate_debug_messenger_createinfo(
    VkDebugUtilsMessengerCreateInfoEXT *createinfo) {
        createinfo->sType
            = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createinfo->messageSeverity
            // = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
              | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createinfo->messageType
            = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
              | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
              | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createinfo->pfnUserCallback = r_debug_callback;
        createinfo->pUserData = NULL;
}

// Set up debug messenger. Returns 1 on success, 0 on failure
static int r_setup_debug_messenger(VkInstance instance) {
        VkDebugUtilsMessengerCreateInfoEXT createinfo = { 0 };
        r_populate_debug_messenger_createinfo(&createinfo);
        VkResult res = r_create_debug_utils_messenger_ext(
            instance, &createinfo, NULL, &r_debug_messenger);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create debug messenger. %s\n",
                        string_VkResult(res));
                return 0;
        }
        return 1;
}

static RState r_state = { 0 };

static void r_check_vkresult(VkResult res, char *msg) {
        if (res != VK_SUCCESS) {
                printf("ASSERT: %s %s\n", msg, string_VkResult(res));
                os_abort(1);
        }
}

static Vec2f32 r_get_window_size(void) {
        Vec2f32 res = {
                .x = (f32)r_state.resolution.width,
                .y = (f32)r_state.resolution.height,
        };
        return res;
}

static u64 r_get_frame_count(void) {
        u64 res = r_state.frameCount;
        return res;
}

static Arena *r_get_arena(void) {
        Arena *res = r_state.arena;
        return res;
}

static void r_framebuffer_resize_callback(GLFWwindow *window, int width,
                                          int height) {
        r_state.framebufferResized = 1;
}

static const char **r_get_required_extensions(Arena *a, u32 *extCount) {
        u32 glfwExtCount = 0;
        const char **glfwExts
            = glfwGetRequiredInstanceExtensions(&glfwExtCount);

        u32 platExtCount = 0;
#ifdef __MACH__
        platExtCount += 1;
#endif

        u32 count
            = r_validation_layers_enabled + glfwExtCount + platExtCount + 1;
        const char **extNames = arena_alloc(a, sizeof(char *) * count);

        for (int i = 0; i < glfwExtCount; i++) {
                extNames[i] = glfwExts[i];
        }

        if (r_validation_layers_enabled) {
                extNames[glfwExtCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }

#ifdef __MACH__
        extNames[glfwExtCount + r_validation_layers_enabled]
            = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
#endif

        extNames[count - 1]
            = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;

        *extCount = count;
        return extNames;
}

static b32 r_check_validation_layer_support(Arena *a) {
        u32 layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, 0);
        r_assert(layerCount != 0, "Failed to find any vulkan layers");

        VkLayerProperties *layersAvailable
            = arena_alloc(a, sizeof(VkLayerProperties *) * layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, layersAvailable);

        for (u32 i = 0; i < r_validation_layer_count; i++) {
                b32 layerFound = 0;
                const char *layer = r_validation_layers[i];
                for (int j = 0; j < layerCount; j++) {
                        if (strcmp(layer, layersAvailable[j].layerName) == 0) {
                                layerFound = 1;
                                break;
                        }
                }

                if (!layerFound) {
                        return 0;
                }
        }

        return 1;
}
static const char *r_device_exts[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef __MACH__
        "VK_KHR_portability_subset", // hr: macOs device extensions
#endif
        "VK_KHR_maintenance3",
        "VK_EXT_descriptor_indexing",
};
u32 r_device_ext_count = array_count(r_device_exts);

static b32 r_device_supports_extensions(Arena *a, VkPhysicalDevice device) {
        u32 availableCount = 0;
        vkEnumerateDeviceExtensionProperties(device, 0, &availableCount, 0);

        VkExtensionProperties *available
            = arena_alloc(a, sizeof(VkExtensionProperties) * availableCount);
        vkEnumerateDeviceExtensionProperties(device, 0, &availableCount,
                                             available);

        for (u32 i = 0; i < r_device_ext_count; i++) {
                const char *ext = r_device_exts[i];
                b32 found = 0;
                for (u32 j = 0; j < availableCount; j++) {
                        if (strcmp(ext, available[j].extensionName) == 0) {
                                found = 1;
                                break;
                        }
                }

                if (!found) {
                        return found;
                }
        }

        return 1;
}

static void r_choose_present_mode(VkPresentModeKHR *available,
                                  u32 availableCount, b32 vsync) {
        VkPresentModeKHR ideal = vsync ? VK_PRESENT_MODE_MAILBOX_KHR
                                       : VK_PRESENT_MODE_IMMEDIATE_KHR;

        r_state.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (u32 i = 0; i < availableCount; i++) {
                if (available[i] == ideal) {
                        r_state.presentMode = ideal;
                        return;
                }
        }
}

static void r_choose_surface_format(VkSurfaceFormatKHR *formats,
                                    u32 formatsCount) {
        r_state.colorFormat = formats[0].format;
        if (formatsCount == 0 && formats[0].format == VK_FORMAT_UNDEFINED) {
                r_state.colorFormat = VK_FORMAT_B8G8R8_UNORM;
        }
        r_state.colorSpace = formats[0].colorSpace;
}

static void
r_choose_swapchain_image_count(VkSurfaceCapabilitiesKHR capabilities) {
        r_state.imageCount = clamp_bot(capabilities.minImageCount, 2);
        if (capabilities.maxImageCount != 0) {
                r_state.imageCount
                    = clamp_top(r_state.imageCount, capabilities.maxImageCount);
        }
}

static void r_choose_swapchain_extent(VkSurfaceCapabilitiesKHR cap) {
        r_state.resolution = cap.currentExtent;
        if (r_state.resolution.width == -1) {
                int width = 0;
                int height = 0;
                glfwGetFramebufferSize(r_state.window, &width, &height);

                r_state.resolution.width = clamp(
                    cap.minImageExtent.width, width, cap.maxImageExtent.width);
                r_state.resolution.height
                    = clamp(cap.minImageExtent.height, height,
                            cap.maxImageExtent.height);
        }
}

static void r_query_swapchain_support(VkPhysicalDevice device, Arena *a,
                                      VkSurfaceCapabilitiesKHR *capabilities,
                                      VkSurfaceFormatKHR **formats,
                                      u32 *formatsCount,
                                      VkPresentModeKHR **presentModes,
                                      u32 *presentModesCount) {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, r_state.surface,
                                                  capabilities);

        vkGetPhysicalDeviceSurfaceFormatsKHR(device, r_state.surface,
                                             formatsCount, 0);
        *formats = arena_alloc(a, sizeof(VkSurfaceFormatKHR) * (*formatsCount));
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, r_state.surface,
                                             formatsCount, *formats);

        vkGetPhysicalDeviceSurfacePresentModesKHR(device, r_state.surface,
                                                  presentModesCount, 0);
        *presentModes
            = arena_alloc(a, sizeof(VkPresentModeKHR) * (*presentModesCount));
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, r_state.surface, presentModesCount, *presentModes);
}

static void r_pick_physical_device(Arena *a) {
        u32 deviceCount = 0;
        vkEnumeratePhysicalDevices(r_state.instance, &deviceCount, 0);
        r_assert(deviceCount != 0, "No physical devices found");

        VkPhysicalDevice *devices
            = arena_alloc(a, sizeof(VkPhysicalDevice) * deviceCount);
        vkEnumeratePhysicalDevices(r_state.instance, &deviceCount, devices);

        for (u32 i = 0; i < deviceCount; i++) {
                VkPhysicalDevice device = devices[i];

                VkPhysicalDeviceDescriptorIndexingProperties indexingProps = {
                        .sType
                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES,
                };
                VkPhysicalDeviceProperties2 props = {
                        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                        .pNext = &indexingProps,
                };
                vkGetPhysicalDeviceProperties2(device, &props);

                b32 supportsExtensions
                    = r_device_supports_extensions(a, device);

                Arena *scratch = r_get_arena();
                u64 resetPos = arena_pos(scratch);
                VkSurfaceCapabilitiesKHR capabilities;
                VkSurfaceFormatKHR *formats;
                u32 formatsCount;
                VkPresentModeKHR *presentModes;
                u32 presentModesCount;
                r_query_swapchain_support(device, scratch, &capabilities,
                                          &formats, &formatsCount,
                                          &presentModes, &presentModesCount);

                b32 swapchainAdequate = formatsCount && presentModesCount;

                u32 queueFamilyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(device,
                                                         &queueFamilyCount, 0);
                VkQueueFamilyProperties *queueFamilyProperties = arena_alloc(
                    a, sizeof(VkQueueFamilyProperties) * queueFamilyCount);
                vkGetPhysicalDeviceQueueFamilyProperties(
                    device, &queueFamilyCount, queueFamilyProperties);
                for (u32 j = 0; j < queueFamilyCount; j++) {
                        b32 supportsPresent = 0;
                        vkGetPhysicalDeviceSurfaceSupportKHR(
                            device, j, r_state.surface, &supportsPresent);

                        b32 graphicsQueue = queueFamilyProperties[j].queueFlags
                                            & VK_QUEUE_GRAPHICS_BIT;

                        if (graphicsQueue && supportsPresent
                            && supportsExtensions && swapchainAdequate) {
                                r_state.physicalDevice = device;
                                r_state.physicalDeviceProps = props;
                                r_state.deviceIndexingProps = indexingProps;
                                r_state.graphicsQueueIdx = j;
                                r_state.presentQueueIdx = j;

                                r_choose_present_mode(presentModes,
                                                      presentModesCount, 1);
                                r_choose_surface_format(formats, formatsCount);
                                r_choose_swapchain_image_count(capabilities);
                                r_choose_swapchain_extent(capabilities);
                                r_state.preTransform
                                    = capabilities.currentTransform;

                                arena_pop_at(scratch, resetPos);
                                return;
                        }
                }
        }

        r_assert(0, "No suitable physical devices found");
}

s64 r_find_memory_type(u32 typeFilter, VkMemoryPropertyFlags props) {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(r_state.physicalDevice, &memProps);

        for (u32 i = 0; i < memProps.memoryTypeCount; i++) {
                if (typeFilter & (1 << i)
                    && (memProps.memoryTypes[i].propertyFlags & props)
                           == props) {
                        return i;
                }
        }

        return -1;
}

static void r_create_buffer(VkBuffer *buffer, VkDeviceMemory *memory,
                            VkDeviceSize size, VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags props) {
        VkBufferCreateInfo bufferCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = size,
                .usage = usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        VkResult res
            = vkCreateBuffer(r_state.device, &bufferCreateInfo, 0, buffer);
        r_check_vkresult(res, "Failed to create buffer");

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(r_state.device, *buffer,
                                      &memRequirements);

        s64 memType = r_find_memory_type(memRequirements.memoryTypeBits, props);
        r_assert(memType >= 0, "Failed to find suitable memory type");

        VkMemoryAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = memRequirements.size,
                .memoryTypeIndex = memType,
        };
        res = vkAllocateMemory(r_state.device, &allocInfo, 0, memory);
        r_check_vkresult(res, "Failed to allocate buffer memory");

        vkBindBufferMemory(r_state.device, *buffer, *memory, 0);
}

static void r_copy_buffer_to_image(VkBuffer buffer, RTexture *texture) {
        VkBufferImageCopy region = {
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                },
                .imageOffset = { 0, 0, 0},
                .imageExtent = { texture->width, texture->height, 1 },
        };
        vkCmdCopyBufferToImage(r_state.setupCmdBuffer, buffer, texture->image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &region);
}

static void r_transition_image_layout(VkImage image, VkImageLayout oldLayout,
                                      VkImageLayout newLayout) {
        VkImageMemoryBarrier barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                },
                .srcAccessMask = 0,
                .dstAccessMask = 0,
        };

        VkPipelineStageFlags srcStage = 0;
        VkPipelineStageFlags dstStage = 0;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
            && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                   && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
                r_assert(0, "Unsupported layout transition");
        }

        vkCmdPipelineBarrier(r_state.setupCmdBuffer, srcStage, dstStage, 0, 0,
                             0, 0, 0, 1, &barrier);
}

static void r_create_image(RTexture *texture, VkFormat format,
                           VkImageTiling tiling, VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags props) {
        VkImageFormatProperties2 imageFormatProperties = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
        };
        VkPhysicalDeviceImageFormatInfo2 formatInfo = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
                .format = format,
                .type = VK_IMAGE_TYPE_2D,
                .tiling = tiling,
                .usage = usage,
        };
        VkResult result = vkGetPhysicalDeviceImageFormatProperties2(
            r_state.physicalDevice, &formatInfo, &imageFormatProperties);
        if (result != VK_SUCCESS) {
                // The format is not supported with the given settings.
                // Handle this scenario.
                r_check_vkresult(result, "Unsupported Image Format");
        }

        VkImageCreateInfo imageCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .extent = { .width = texture->width,
                            .height = texture->height,
                            .depth = 1 },
                .mipLevels = 1,
                .arrayLayers = 1,
                .format = format,
                .tiling = tiling,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .usage = usage,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        VkResult res = vkCreateImage(r_state.device, &imageCreateInfo, 0,
                                     &texture->image);
        r_check_vkresult(res, "Failed to create image");

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(r_state.device, texture->image, &memReqs);

        VkMemoryAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = memReqs.size,
                .memoryTypeIndex
                = r_find_memory_type(memReqs.memoryTypeBits, props),
        };
        res = vkAllocateMemory(r_state.device, &allocInfo, 0, &texture->memory);
        r_check_vkresult(res, "Failed to allocate image memory");

        vkBindImageMemory(r_state.device, texture->image, texture->memory, 0);
}

static void r_copy_to_image(void *pixels, u32 imageSize, VkBuffer buffer,
                            VkDeviceMemory stagingMem, RTexture *tex) {
        VkCommandBufferBeginInfo beginInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };
        vkBeginCommandBuffer(r_state.setupCmdBuffer, &beginInfo);

        void *data;
        vkMapMemory(r_state.device, stagingMem, 0, imageSize, 0, &data);
        memcpy(data, pixels, imageSize);
        vkUnmapMemory(r_state.device, stagingMem);

        r_transition_image_layout(tex->image, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        r_copy_buffer_to_image(buffer, tex);
        r_transition_image_layout(tex->image,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkEndCommandBuffer(r_state.setupCmdBuffer);
        VkSubmitInfo submitInfo = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &r_state.setupCmdBuffer,
        };
        vkQueueSubmit(r_state.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(r_state.graphicsQueue);
}

static void r_create_texture(u8 *pixels, u32 width, u32 height, u32 channels,
                             RTexture *texture) {
        VkDevice device = r_state.device;

        texture->width = width;
        texture->height = height;

        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        VkDeviceSize imageSize = width * height * 4;
        if (channels == 1) {
                format = VK_FORMAT_R8_UNORM;
                imageSize = width * height;
        }

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        r_create_buffer(&stagingBuffer, &stagingBufferMemory, imageSize,
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        r_create_image(texture, format, VK_IMAGE_TILING_LINEAR,
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT
                           | VK_IMAGE_USAGE_SAMPLED_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        r_copy_to_image(pixels, imageSize, stagingBuffer, stagingBufferMemory,
                        texture);

        VkImageViewCreateInfo viewCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = texture->image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = format,
                .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .subresourceRange.baseMipLevel = 0,
                .subresourceRange.levelCount = 1,
                .subresourceRange.baseArrayLayer = 0,
                .subresourceRange.layerCount = 1,
        };
        VkResult res = vkCreateImageView(r_state.device, &viewCreateInfo, 0,
                                         &texture->view);
        r_check_vkresult(res, "Failed to create image view)");

        vkDestroyBuffer(device, stagingBuffer, 0);
        vkFreeMemory(device, stagingBufferMemory, 0);
}

static void r_create_dynamic_texture(Arena *a, u32 width, u32 height,
                                     RDynamicTexture *dTex) {
        r_assert(dTex != 0, "Passed null pointer to r_create_dynamic_texture");

        u32 frames = r_state.maxFramesInFlight;
        dTex->data = arena_alloc(a, sizeof(Vec4u8) * width * height);
        dTex->stagingBuffers = arena_alloc(a, sizeof(VkBuffer) * frames);
        dTex->stagingMemory = arena_alloc(a, sizeof(VkDeviceMemory) * frames);
        dTex->textures = arena_alloc(a, sizeof(RTexture) * frames);

        for (u32 i = 0; i < frames; i++) {
                r_create_buffer(&dTex->stagingBuffers[i],
                                &dTex->stagingMemory[i], width * height * 4,
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                    | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                // NOTE: hr: this function call allocates unnecessary
                // staging buffers
                r_create_texture((u8 *)dTex->data, width, height, 4,
                                 &dTex->textures[i]);
        }
}

static RTexture *r_prep_dynamic_texture(RDynamicTexture *dTex) {
        u64 frame = r_get_frame_count() % r_state.maxFramesInFlight;
        RTexture *tex = &dTex->textures[frame];
        r_copy_to_image(dTex->data, tex->width * tex->height * 4,
                        dTex->stagingBuffers[frame], dTex->stagingMemory[frame],
                        tex);
        return tex;
}

static void r_destroy_dynamic_texture(RDynamicTexture *dTex) {
        for (u32 i = 0; i < r_state.maxFramesInFlight; i++) {
                vkDestroyBuffer(r_state.device, dTex->stagingBuffers[i], 0);
                vkFreeMemory(r_state.device, dTex->stagingMemory[i], 0);
                r_destroy_texture(&dTex->textures[i]);
        }
}

static VkShaderModule r_create_shader_module(String8 filename) {
        VkShaderModule shader = VK_NULL_HANDLE;

        String8 code = r_read_shader_file(r_state.arena, filename);
        VkShaderModuleCreateInfo createInfo = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = code.length,
                .pCode = (u32 *)code.data,
        };

        VkResult res
            = vkCreateShaderModule(r_state.device, &createInfo, 0, &shader);
        r_check_vkresult(res, "Failed to create shader module");

        return shader;
}

typedef struct {
        String8 vertFile;
        String8 fragFile;
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
} RGraphicsPipelineCreateInfo;

static VkPipeline
r_create_graphics_pipeline(RGraphicsPipelineCreateInfo *createInfo) {
        VkDevice device = r_state.device;

        r_assert(createInfo->vertFile.length && createInfo->fragFile.length,
                 "No filename given for shader");

        VkShaderModule vert = r_create_shader_module(createInfo->vertFile);
        VkShaderModule frag = r_create_shader_module(createInfo->fragFile);

        VkPipelineShaderStageCreateInfo vertStageInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vert,
                .pName = "main",
        };

        VkPipelineShaderStageCreateInfo fragStageInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = frag,
                .pName = "main",
        };

        VkPipelineShaderStageCreateInfo shaderStages[] = {
                vertStageInfo,
                fragStageInfo,
        };

        VkDynamicState dynamicStates[] = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo dynamicState = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount
                = sizeof(dynamicStates) / sizeof(dynamicStates[0]),
                .pDynamicStates = dynamicStates,
        };

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = createInfo->primativeTopology,
                .primitiveRestartEnable = VK_TRUE,
        };

        VkPipelineViewportStateCreateInfo viewportState = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .scissorCount = 1,
        };

        VkPipelineRasterizationStateCreateInfo rasterizer = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode = createInfo->polygonMode,
                .lineWidth = 1.0f,
                .cullMode = createInfo->cullMode,
                .frontFace = createInfo->frontFace,
        };

        VkPipelineMultisampleStateCreateInfo multisampling = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };

        VkPipelineColorBlendStateCreateInfo colorBlending = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = createInfo->blendAttachmentStatesCount,
                .pAttachments = createInfo->blendAttachmentStates,
        };

        VkGraphicsPipelineCreateInfo pipelineInfo = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .stageCount = 2,
                .pStages = shaderStages,
                .pVertexInputState = createInfo->vertexInputInfo,
                .pInputAssemblyState = &inputAssembly,
                .pViewportState = &viewportState,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pDepthStencilState = createInfo->depthStencilState,
                .pColorBlendState = &colorBlending,
                .pDynamicState = &dynamicState,
                .layout = createInfo->pipelineLayout,
                .renderPass = createInfo->renderPass,
                .basePipelineIndex = -1,
        };

        VkPipeline pipeline = VK_NULL_HANDLE;
        VkResult res = vkCreateGraphicsPipelines(
            device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &pipeline);
        r_check_vkresult(res, "Failed to create graphics pipelines");

        vkDestroyShaderModule(device, vert, NULL);
        vkDestroyShaderModule(device, frag, NULL);

        return pipeline;
}

static void r_create_swapchain(void) {
        Arena *arena = r_state.swapchainArena;
        VkSwapchainCreateInfoKHR swapchainCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                .surface = r_state.surface,
                .minImageCount = r_state.imageCount,
                .imageFormat = r_state.colorFormat,
                .imageColorSpace = r_state.colorSpace,
                .imageExtent = r_state.resolution,
                .imageArrayLayers = 1,
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .imageSharingMode
                = VK_SHARING_MODE_EXCLUSIVE, // NOTE: hr: if graphics
                                             // and present queues are
                                             // different then we'll
                                             // have to use concurrent
                                             // sharing mode.
                .preTransform = r_state.preTransform,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode = r_state.presentMode,
                .clipped = VK_TRUE,
        };
        VkResult res = vkCreateSwapchainKHR(
            r_state.device, &swapchainCreateInfo, 0, &r_state.swapchain);
        r_check_vkresult(res, "Failed to create swapchain");

        vkGetSwapchainImagesKHR(r_state.device, r_state.swapchain,
                                &r_state.imageCount, 0);
        r_state.swapchainImages
            = arena_alloc(arena, sizeof(VkImage) * r_state.imageCount);
        vkGetSwapchainImagesKHR(r_state.device, r_state.swapchain,
                                &r_state.imageCount, r_state.swapchainImages);

        r_state.maxFramesInFlight = r_state.imageCount;

        r_state.swapchainImageViews
            = arena_alloc(arena, sizeof(VkImageView) * r_state.imageCount);
        for (u32 i = 0; i < r_state.imageCount; i++) {
                VkImageViewCreateInfo imageViewCreateInfo = {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                        .image = r_state.swapchainImages[i],
                        .viewType = VK_IMAGE_VIEW_TYPE_2D,
                        .format = r_state.colorFormat,
                        .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .subresourceRange.aspectMask
                        = VK_IMAGE_ASPECT_COLOR_BIT,
                        .subresourceRange.baseMipLevel = 0,
                        .subresourceRange.levelCount = 1,
                        .subresourceRange.baseArrayLayer = 0,
                        .subresourceRange.layerCount = 1,
                };
                res = vkCreateImageView(r_state.device, &imageViewCreateInfo, 0,
                                        &r_state.swapchainImageViews[i]);
                r_check_vkresult(res, "Failed to create swapchain image view");
        }
}

static void r_create_framebuffers(void) {
        Arena *arena = r_state.swapchainArena;
        r_state.swapchainFramebuffers
            = arena_alloc(arena, sizeof(VkFramebuffer) * r_state.imageCount);

        for (uint32_t i = 0; i < r_state.imageCount; i++) {
                VkImageView attachments[] = {
                        r_state.swapchainImageViews[i],
                };

                VkFramebufferCreateInfo framebufferCreateInfo = {
                        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                        .renderPass = r_state.renderPass,
                        .attachmentCount = array_count(attachments),
                        .pAttachments = attachments,
                        .width = r_state.resolution.width,
                        .height = r_state.resolution.height,
                        .layers = 1,
                };

                VkResult res = vkCreateFramebuffer(
                    r_state.device, &framebufferCreateInfo, 0,
                    &r_state.swapchainFramebuffers[i]);
                r_check_vkresult(res, "Failed to create framebuffer");
        }
}

static void r_destroy_swapchain(void) {
        for (u32 i = 0; i < r_state.maxFramesInFlight; i++) {
                vkDestroyFramebuffer(r_state.device,
                                     r_state.swapchainFramebuffers[i], 0);
                vkDestroyImageView(r_state.device,
                                   r_state.swapchainImageViews[i], 0);
        }
        vkDestroySwapchainKHR(r_state.device, r_state.swapchain, 0);

        arena_reset(r_state.swapchainArena);
}

static void r_recreate_swapchain(void) {
        int width = 0, height = 0;
        glfwGetFramebufferSize(r_state.window, &width, &height);
        while (width == 0 || height == 0) {
                glfwGetFramebufferSize(r_state.window, &width, &height);
                glfwPollEvents();
        }

        vkDeviceWaitIdle(r_state.device);
        r_destroy_swapchain();

        Arena *a = r_get_arena();
        u64 resetPos = arena_pos(a);
        VkSurfaceCapabilitiesKHR capabilities;
        VkSurfaceFormatKHR *formats;
        u32 formatsCount;
        VkPresentModeKHR *presentModes;
        u32 presentModesCount;
        r_query_swapchain_support(r_state.physicalDevice, a, &capabilities,
                                  &formats, &formatsCount, &presentModes,
                                  &presentModesCount);
        r_choose_present_mode(presentModes, presentModesCount, 1);
        r_choose_surface_format(formats, formatsCount);
        r_choose_swapchain_image_count(capabilities);
        r_choose_swapchain_extent(capabilities);
        r_state.preTransform = capabilities.currentTransform;
        arena_pop_at(a, resetPos);

        r_create_swapchain();
        r_create_framebuffers();
}

static void r_init_backend(const char *name, u32 width, u32 height) {
        r_state.arena = make_arena(mb(6));
        r_state.swapchainArena = make_arena(mb(6));
        Arena *arena = r_get_arena();

        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        r_state.window = glfwCreateWindow(width, height, name, 0, 0);
        glfwSetFramebufferSizeCallback(r_state.window,
                                       r_framebuffer_resize_callback);

        // hr: Load vulkan functions from dynamic library on system
        VkApplicationInfo appInfo = {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName = name,
                .engineVersion = 1,
                .apiVersion = VK_MAKE_VERSION(1, 0, 0),
        };

        u32 extCount = 0;
        const char **extNames = r_get_required_extensions(arena, &extCount);
        VkInstanceCreateInfo instanceInfo = {
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pApplicationInfo = &appInfo,
                .enabledExtensionCount = extCount,
                .ppEnabledExtensionNames = extNames,
                .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
        };

        if (r_validation_layers_enabled) {
                r_assert(r_check_validation_layer_support(arena),
                         "Failed to find validation layers");
                instanceInfo.enabledLayerCount = r_validation_layer_count;
                instanceInfo.ppEnabledLayerNames = r_validation_layers;

                VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = { 0 };
                r_populate_debug_messenger_createinfo(&debugCreateInfo);
                instanceInfo.pNext = &debugCreateInfo;
        }

        VkResult res = vkCreateInstance(&instanceInfo, 0, &r_state.instance);
        r_check_vkresult(res, "Failed to create vulkan instance");

        if (r_validation_layers_enabled) {
                r_assert(r_setup_debug_messenger(r_state.instance), "");
        }

        res = glfwCreateWindowSurface(r_state.instance, r_state.window, 0,
                                      &r_state.surface);
        r_check_vkresult(res, "Failed to create surface");

        r_pick_physical_device(arena);

        float queuePriorities[] = { 1.0 };
        VkDeviceQueueCreateInfo queueCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = r_state.presentQueueIdx,
                .queueCount = 1,
                .pQueuePriorities = queuePriorities,
        };

        VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {
                .sType
                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
                .descriptorBindingPartiallyBound = VK_TRUE,
                .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
                .runtimeDescriptorArray = VK_TRUE,
        };
        VkPhysicalDeviceFeatures deviceFeatures = {
                .samplerAnisotropy = VK_TRUE,
        };
        VkDeviceCreateInfo deviceCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .pNext = &descriptorIndexingFeatures,
                .queueCreateInfoCount = 1,
                .pQueueCreateInfos = &queueCreateInfo,
                .enabledLayerCount = r_validation_layer_count,
                .ppEnabledLayerNames = r_validation_layers,
                .enabledExtensionCount = r_device_ext_count,
                .ppEnabledExtensionNames = r_device_exts,
                .pEnabledFeatures = &deviceFeatures,
        };
        res = vkCreateDevice(r_state.physicalDevice, &deviceCreateInfo, 0,
                             &r_state.device);
        r_check_vkresult(res, "Failed to create logical device");

        vkGetDeviceQueue(r_state.device, r_state.graphicsQueueIdx, 0,
                         &r_state.graphicsQueue);
        vkGetDeviceQueue(r_state.device, r_state.presentQueueIdx, 0,
                         &r_state.presentQueue);

        r_create_swapchain();

        VkAttachmentDescription passAttachments[] = {
                {
                        .format = r_state.colorFormat,
                        .samples = VK_SAMPLE_COUNT_1_BIT,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                },
        };

        VkAttachmentReference colorAttachmentReference = {
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };

        VkSubpassDescription subpass = {
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachmentReference,
        };

        VkSubpassDependency dependency = {
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        };

        VkRenderPassCreateInfo renderPassCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .attachmentCount = array_count(passAttachments),
                .pAttachments = passAttachments,
                .subpassCount = 1,
                .pSubpasses = &subpass,
                .dependencyCount = 1,
                .pDependencies = &dependency,
        };

        res = vkCreateRenderPass(r_state.device, &renderPassCreateInfo, 0,
                                 &r_state.renderPass);
        r_check_vkresult(res, "Failed to create render pass");

        r_create_framebuffers();

        VkCommandPoolCreateInfo commandPoolCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = r_state.presentQueueIdx,
        };
        res = vkCreateCommandPool(r_state.device, &commandPoolCreateInfo, 0,
                                  &r_state.commandPool);
        r_check_vkresult(res, "Failed to create command pool");

        VkCommandBufferAllocateInfo commandBufferAllocationInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = r_state.commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = r_state.maxFramesInFlight,
        };
        r_state.renderCmdBuffers = arena_alloc(
            arena, sizeof(VkCommandBuffer) * r_state.maxFramesInFlight);
        res = vkAllocateCommandBuffers(r_state.device,
                                       &commandBufferAllocationInfo,
                                       r_state.renderCmdBuffers);
        r_check_vkresult(res, "Failed to allocate render command buffer");

        commandBufferAllocationInfo.commandBufferCount = 1;
        res = vkAllocateCommandBuffers(r_state.device,
                                       &commandBufferAllocationInfo,
                                       &r_state.setupCmdBuffer);
        r_check_vkresult(res, "Failed to allocate setup command buffer");

        // NOTE: hr: on some devices that support descriptor indexing will
        // report no limit (MAX_INT) for  maxPerStageDescriptorSampledImages.
        // Better limits are  found in DeviceDescriptorIndexProps (which can
        // also return MAX_INT or "no limit").
        //
        // r_state.maxTextures = r_state.physicalDeviceProps.properties.limits
        //                           .maxPerStageDescriptorSampledImages;

        r_state.maxTextures
            = min(r_state.deviceIndexingProps
                      .maxPerStageDescriptorUpdateAfterBindSampledImages,
                  r_state.deviceIndexingProps
                          .maxDescriptorSetUpdateAfterBindSampledImages
                      / r_state.maxFramesInFlight);
        if (r_state.maxTextures > 1024) {
                r_state.maxTextures = 1024;
        }
        VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2] = {
                {
                        .binding = 0,
                        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        .descriptorCount = r_state.maxTextures,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
                {
                        .binding = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                        .descriptorCount = 1,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                },
        };
        VkDescriptorBindingFlags descriptorBindingFlags[2] = {
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT
                    | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT,
                0,
        };
        VkDescriptorSetLayoutBindingFlagsCreateInfo
            descriptorSetLayoutBindingFlags = {
                    .sType
                    = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                    .bindingCount = 2,
                    .pBindingFlags = descriptorBindingFlags,
            };
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &descriptorSetLayoutBindingFlags,
                .flags
                = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                .bindingCount = 2,
                .pBindings = descriptorSetLayoutBindings,
        };
        res = vkCreateDescriptorSetLayout(r_state.device,
                                          &descriptorSetLayoutCreateInfo, 0,
                                          &r_state.descriptorSetLayout);
        r_check_vkresult(res, "Failed to create descriptor set layout");

        VkPushConstantRange pushConstantRange = {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .offset = 0,
                .size = sizeof(Vec2f32),
        };
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 1,
                .pSetLayouts = &r_state.descriptorSetLayout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &pushConstantRange,
        };
        res = vkCreatePipelineLayout(r_state.device, &pipelineLayoutCreateInfo,
                                     0, &r_state.pipelineLayout);
        r_check_vkresult(res, "Failed to create pipeline layout");

        VkVertexInputBindingDescription vertexBindingDescription = {
                .binding = 0,
                .stride = sizeof(RRectInstanceData),
                .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
        };

        VkVertexInputAttributeDescription vertexAttributeDescriptions[] = {
                {
                        .location = 0,
                        .binding = 0,
                        .format = VK_FORMAT_R32G32_SFLOAT,
                        .offset = offsetof(RRectInstanceData, pos),
                },
                {
                        .location = 1,
                        .binding = 0,
                        .format = VK_FORMAT_R32G32_SFLOAT,
                        .offset
                        = offsetof(RRectInstanceData, pos) + sizeof(Vec2f32),
                },
                {
                        .location = 2,
                        .binding = 0,
                        .format = VK_FORMAT_R32G32_SFLOAT,
                        .offset = offsetof(RRectInstanceData, src),
                },
                {
                        .location = 3,
                        .binding = 0,
                        .format = VK_FORMAT_R32G32_SFLOAT,
                        .offset
                        = offsetof(RRectInstanceData, src) + sizeof(Vec2f32),
                },

                {
                        .location = 4,
                        .binding = 0,
                        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                        .offset = offsetof(RRectInstanceData, colors),
                },
                {
                        .location = 5,
                        .binding = 0,
                        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                        .offset
                        = offsetof(RRectInstanceData, colors) + sizeof(Vec4f32),
                },
                {
                        .location = 6,
                        .binding = 0,
                        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                        .offset = offsetof(RRectInstanceData, colors)
                                  + sizeof(Vec4f32) * 2,
                },
                {
                        .location = 7,
                        .binding = 0,
                        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                        .offset = offsetof(RRectInstanceData, colors)
                                  + sizeof(Vec4f32) * 3,
                },
                {
                        .location = 8,
                        .binding = 0,
                        .format = VK_FORMAT_R32_UINT,
                        .offset = offsetof(RRectInstanceData, texID),
                },
                {
                        .location = 9,
                        .binding = 0,
                        .format = VK_FORMAT_R32_SFLOAT,
                        .offset = offsetof(RRectInstanceData, cornerRadius),
                },
                {
                        .location = 10,
                        .binding = 0,
                        .format = VK_FORMAT_R32_SFLOAT,
                        .offset = offsetof(RRectInstanceData, edgeSoftness),
                },
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &vertexBindingDescription,
                .vertexAttributeDescriptionCount
                = array_count(vertexAttributeDescriptions),
                .pVertexAttributeDescriptions = vertexAttributeDescriptions,
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

        RGraphicsPipelineCreateInfo pipelineInfo = {
                .renderPass = r_state.renderPass,
                .pipelineLayout = r_state.pipelineLayout,
                .vertFile
                = os_path(arena, string8_lit("src/render/vulkan/vert.spv")),
                .fragFile
                = os_path(arena, string8_lit("src/render/vulkan/frag.spv")),
                .vertexInputInfo = &vertexInputInfo,
                .blendAttachmentStatesCount = 1,
                .blendAttachmentStates = &colorBlendAttachment,
                .depthStencilState = NULL,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .primativeTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        };
        r_state.pipeline = r_create_graphics_pipeline(&pipelineInfo);

        VkSemaphoreCreateInfo semaphoreInfo = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        VkFenceCreateInfo fenceInfo = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        r_state.imageAvailableSemaphore = arena_alloc(
            r_state.arena, sizeof(VkSemaphore) * r_state.maxFramesInFlight);
        r_state.renderFinishedSemaphore = arena_alloc(
            r_state.arena, sizeof(VkSemaphore) * r_state.maxFramesInFlight);
        r_state.inflightFence = arena_alloc(
            r_state.arena, sizeof(VkFence) * r_state.maxFramesInFlight);

        for (u32 i = 0; i < r_state.maxFramesInFlight; i++) {
                res = vkCreateSemaphore(r_state.device, &semaphoreInfo, 0,
                                        r_state.imageAvailableSemaphore + i);
                r_check_vkresult(res,
                                 "Failed to create image available semaphore");
                res = vkCreateSemaphore(r_state.device, &semaphoreInfo, 0,
                                        r_state.renderFinishedSemaphore + i);
                r_check_vkresult(res,
                                 "Failed to create render finished semaphore");
                res = vkCreateFence(r_state.device, &fenceInfo, 0,
                                    r_state.inflightFence + i);
                r_check_vkresult(res, "Failed to create in flight fence");
        }

        r_state.maxRects = 512;
        VkDeviceSize instanceSize
            = sizeof(RRectInstanceData) * r_state.maxRects;
        u32 frames = r_state.maxFramesInFlight;
        r_state.instancesData
            = arena_alloc(arena, sizeof(RRectInstanceData *) * frames);
        r_state.stagingBuffers = arena_alloc(arena, sizeof(VkBuffer) * frames);
        r_state.stagingsMemory
            = arena_alloc(arena, sizeof(VkDeviceMemory) * frames);
        r_state.instanceBuffers = arena_alloc(arena, sizeof(VkBuffer) * frames);
        r_state.instancesMemory
            = arena_alloc(arena, sizeof(VkDeviceMemory) * frames);
        for (u32 i = 0; i < r_state.maxFramesInFlight; i++) {
                r_create_buffer(r_state.stagingBuffers + i,
                                r_state.stagingsMemory + i, instanceSize,
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                    | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

                vkMapMemory(r_state.device, *(r_state.stagingsMemory + i), 0,
                            instanceSize, 0,
                            (void *)(r_state.instancesData + i));

                r_create_buffer(r_state.instanceBuffers + i,
                                r_state.instancesMemory + i, instanceSize,
                                VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                    | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        }

        VkDescriptorPoolSize descriptorPoolSizes[2] = {
                {
                        .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        .descriptorCount
                        = r_state.maxTextures * r_state.maxFramesInFlight,
                },
                {
                        .type = VK_DESCRIPTOR_TYPE_SAMPLER,
                        .descriptorCount = r_state.maxFramesInFlight,
                },
        };
        // WARN: hr: this fails on my pc using what device is chosen  (probably
        // integrated graphics not my card). Need to query if this is supported.
        // Also  probably need basic version of add_to_batch and draw_batch.
        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                .maxSets = r_state.maxFramesInFlight,
                .poolSizeCount = 2,
                .pPoolSizes = descriptorPoolSizes,
        };
        res = vkCreateDescriptorPool(r_state.device, &descriptorPoolCreateInfo,
                                     0, &r_state.descriptorPool);
        r_check_vkresult(res, "Failed to create descriptor pool");

        r_state.descriptorSets = arena_alloc(
            arena, sizeof(VkDescriptorSet) * r_state.maxFramesInFlight);
        for (u32 i = 0; i < r_state.maxFramesInFlight; i++) {
                VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
                        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                        .descriptorPool = r_state.descriptorPool,
                        .descriptorSetCount = 1,
                        .pSetLayouts = &r_state.descriptorSetLayout,
                };
                res = vkAllocateDescriptorSets(r_state.device,
                                               &descriptorSetAllocateInfo,
                                               r_state.descriptorSets + i);
                r_check_vkresult(res, "Failed to allocate descriptor set");
        }

        VkSamplerCreateInfo samplerCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = 0,
                .minFilter = 0,
                .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .anisotropyEnable = VK_TRUE,
                .maxAnisotropy = r_state.physicalDeviceProps.properties.limits
                                     .maxSamplerAnisotropy,
                .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
                .unnormalizedCoordinates = VK_FALSE,
                .compareEnable = VK_FALSE,
                .compareOp = VK_COMPARE_OP_ALWAYS,
                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                .mipLodBias = 0.0f,
                .minLod = 0.0f,
                .maxLod = 0.0f,
        };
        res = vkCreateSampler(r_state.device, &samplerCreateInfo, 0,
                              &r_state.sampler);
        r_check_vkresult(res, "Failed to create texture sampler");

        for (u32 i = 0; i < r_state.maxFramesInFlight; i++) {
                VkDescriptorImageInfo samplerInfo = {
                        .sampler = r_state.sampler,
                };
                VkWriteDescriptorSet samplerWriteDescriptorSet = {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = r_state.descriptorSets[i],
                        .dstBinding = 1,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                        .pImageInfo = &samplerInfo,
                };
                vkUpdateDescriptorSets(r_state.device, 1,
                                       &samplerWriteDescriptorSet, 0, 0);
        }

        r_state.indexingInfo = arena_alloc(
            arena, sizeof(RIndexingInfo) * r_state.maxFramesInFlight);
        for (u32 i = 0; i < r_state.maxFramesInFlight; i++) {
                RIndexingInfo ii = r_state.indexingInfo[i];
                u32 maxTexs = r_state.maxTextures;

                u64 prevSize = sizeof(*(ii.prev)) * maxTexs;
                ii.prev = arena_alloc(arena, prevSize);
                memset(ii.prev, 0, prevSize);

                u64 curSize = sizeof(*(ii.cur)) * maxTexs;
                ii.cur = arena_alloc(arena, curSize);
                memset(ii.cur, 0, curSize);

                ii.freeTop = maxTexs - 1;
                ii.free = arena_alloc(arena, sizeof(*(ii.free)) * maxTexs);
                for (u16 j = maxTexs; j > 0; j--) {
                        ii.free[maxTexs - j] = j;
                }
                r_state.indexingInfo[i] = ii;
        }

        r_state.writeDescriptorSetsCount = 0;
        r_state.writeDescriptorSets = arena_alloc(
            arena, sizeof(VkWriteDescriptorSet) * r_state.maxTextures);
        r_state.writeImageInfo = arena_alloc(
            arena, sizeof(VkDescriptorImageInfo) * r_state.maxTextures);

        u64 blankSize = 32 * 32 * 4;
        u8 *blankPixels = arena_alloc(arena, blankSize);
        memset(blankPixels, 255, blankSize);
        r_create_texture(blankPixels, 32, 32, 4, &r_state.blankTex);

        for (u32 i = 0; i < r_state.maxFramesInFlight; i++) {
                VkDescriptorImageInfo imageInfo = {
                        .imageView = r_state.blankTex.view,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };
                VkWriteDescriptorSet wds = {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = r_state.descriptorSets[i],
                        .dstBinding = 0,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        .pImageInfo = &imageInfo,
                };
                vkUpdateDescriptorSets(r_state.device, 1, &wds, 0, 0);
        }
}

static b32 r_begin_frame(void) {
        u32 currentFrame = r_state.currentFrame;
        VkDevice device = r_state.device;
        vkWaitForFences(device, 1, r_state.inflightFence + currentFrame,
                        VK_TRUE, UINT64_MAX);

        VkResult res = vkAcquireNextImageKHR(
            r_state.device, r_state.swapchain, UINT64_MAX,
            r_state.imageAvailableSemaphore[currentFrame], VK_NULL_HANDLE,
            &r_state.imageIdx);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR
            || r_state.framebufferResized) {
                r_state.framebufferResized = 0;
                r_recreate_swapchain();
                return 1;
        } else {
                r_check_vkresult(res, "Failed to aquire swap chain image");
        }

        vkResetFences(device, 1, r_state.inflightFence + currentFrame);

        VkCommandBuffer cmdBuffer = r_state.renderCmdBuffers[currentFrame];
        vkResetCommandBuffer(cmdBuffer, 0);

        VkCommandBufferBeginInfo beginInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };

        res = vkBeginCommandBuffer(cmdBuffer, &beginInfo);
        r_check_vkresult(res, "Failed to being command buffer recording");

        return 0;
}

static void r_write_texture_descriptor(RRectInstanceData *rect, RTexture *tex) {
        u32 currentFrame = r_state.currentFrame;
        RIndexingInfo *i = r_state.indexingInfo + currentFrame;
        if (i->prev[tex->lastBindingIndex] == tex) {
                rect->texID = tex->lastBindingIndex;
                i->cur[rect->texID] = tex;
                return;
        }
        if (i->freeTop != 0) {
                rect->texID = i->free[--i->freeTop];
                i->cur[rect->texID] = tex;
                tex->lastBindingIndex = rect->texID;

                u32 writesCount = r_state.writeDescriptorSetsCount;
                VkDescriptorImageInfo imageInfo = {
                        .imageView = tex->view,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };
                r_state.writeImageInfo[writesCount] = imageInfo;
                VkWriteDescriptorSet wds = {
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = r_state.descriptorSets[currentFrame],
                        .dstBinding = 0,
                        .dstArrayElement = rect->texID,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        .pImageInfo = r_state.writeImageInfo + writesCount,
                };
                r_state.writeDescriptorSets[writesCount] = wds;
                r_state.writeDescriptorSetsCount++;
        } else {
                // TODO: hr: reclaim indices
                rect->texID = 0;
                tex->lastBindingIndex = rect->texID;
        }
}

static void r_add_rect_to_batch(RRectInstanceData *rect, RTexture *tex) {
        u32 currentFrame = r_state.currentFrame;
        u32 rectCount = r_state.rectCount;
        if (r_state.rectCount >= r_state.maxRects) {
                // TODO: hr: implement
                r_assert(0, "Need to implement, no more rect space");
        }
        if (tex) {
                r_write_texture_descriptor(rect, tex);
        } else {
                rect->texID = 0;
        }
        r_state.instancesData[currentFrame][rectCount] = *rect;
        r_state.rectCount++;
}

static void r_dispatch_batch(void) {
        u32 currentFrame = r_state.currentFrame;
        VkCommandBuffer cmdBuffer = r_state.renderCmdBuffers[currentFrame];

        VkBufferCopy copyRegion = {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = sizeof(RRectInstanceData) * r_state.rectCount,
        };
        vkCmdCopyBuffer(cmdBuffer, r_state.stagingBuffers[currentFrame],
                        r_state.instanceBuffers[currentFrame], 1, &copyRegion);
}

static void r_end_frame(void) {
        u32 currentFrame = r_state.currentFrame;
        VkCommandBuffer cmdBuffer = r_state.renderCmdBuffers[currentFrame];

        vkUpdateDescriptorSets(r_state.device, r_state.writeDescriptorSetsCount,
                               r_state.writeDescriptorSets, 0, 0);
        r_state.writeDescriptorSetsCount = 0;

        RIndexingInfo i = r_state.indexingInfo[currentFrame];
        for (u32 k = 0; k < r_state.maxTextures; k++) {
                if (i.prev[k] != 0 && i.prev[k] != i.cur[k]) {
                        i.free[i.freeTop++] = k;
                }
        }
        RTexture **tmp = i.prev;
        i.prev = i.cur;
        i.cur = tmp;
        memset(i.cur, 0, sizeof(*(i.cur)) * r_state.maxTextures);
        r_state.indexingInfo[currentFrame] = i;

        VkClearValue clearValue = { { { 0.3f, 0.3f, 0.3f, 1.0f } } };
        VkRenderPassBeginInfo passInfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = r_state.renderPass,
                .framebuffer = r_state.swapchainFramebuffers[r_state.imageIdx],
                .renderArea.offset = { 0, 0 },
                .renderArea.extent
                = { r_state.resolution.width, r_state.resolution.height },
                .clearValueCount = 1,
                .pClearValues = &clearValue,
        };

        Vec2f32 resolution = v2f32((float)r_state.resolution.width,
                                   (float)r_state.resolution.height);
        vkCmdPushConstants(cmdBuffer, r_state.pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(resolution),
                           &resolution);

        vkCmdBeginRenderPass(cmdBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          r_state.pipeline);

        VkViewport viewport = {
                .x = 0.0f,
                .y = 0.0f,
                .width = resolution.x,
                .height = resolution.y,
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
        };
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

        VkRect2D scissor = {
                .offset = { 0, 0 },
                .extent
                = { r_state.resolution.width, r_state.resolution.height },
        };
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

        VkDeviceSize instanceOffsets = {};
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1,
                               r_state.instanceBuffers + currentFrame,
                               &instanceOffsets);

        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                r_state.pipelineLayout, 0, 1,
                                &r_state.descriptorSets[currentFrame], 0, 0);

        vkCmdDraw(cmdBuffer, 4, r_state.rectCount, 0, 0);

        vkCmdEndRenderPass(cmdBuffer);

        VkResult res = vkEndCommandBuffer(cmdBuffer);
        r_check_vkresult(res, "Failed to record command buffern");

        VkPipelineStageFlags waitStages[] = {
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        };
        VkSubmitInfo submitInfo = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores
                = r_state.imageAvailableSemaphore + currentFrame,
                .pWaitDstStageMask = waitStages,
                .commandBufferCount = 1,
                .pCommandBuffers = &cmdBuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores
                = r_state.renderFinishedSemaphore + currentFrame,
        };

        res = vkQueueSubmit(r_state.graphicsQueue, 1, &submitInfo,
                            r_state.inflightFence[currentFrame]);
        r_check_vkresult(res, "Failed to sumbit draw command buffer");

        VkPresentInfoKHR presentInfo = {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores
                = r_state.renderFinishedSemaphore + currentFrame,
                .swapchainCount = 1,
                .pSwapchains = &r_state.swapchain,
                .pImageIndices = &r_state.imageIdx,
        };
        vkQueuePresentKHR(r_state.presentQueue, &presentInfo);

        r_state.frameCount++;
        r_state.currentFrame = (currentFrame + 1) % r_state.maxFramesInFlight;
        r_state.rectCount = 0;

        glfwPollEvents();
}

static void r_destroy_texture(RTexture *texture) {
        VkDevice device = r_state.device;
        vkDestroyImage(device, texture->image, 0);
        vkFreeMemory(device, texture->memory, 0);
        vkDestroyImageView(device, texture->view, 0);
}

static void r_destroy_backend(void) {
        VkDevice device = r_state.device;
        vkDeviceWaitIdle(device);

        r_destroy_texture(&r_state.blankTex);
        vkDestroySampler(device, r_state.sampler, 0);
        vkDestroyDescriptorPool(device, r_state.descriptorPool, 0);
        vkDestroyDescriptorSetLayout(device, r_state.descriptorSetLayout, 0);
        vkDestroyPipelineLayout(device, r_state.pipelineLayout, 0);
        vkDestroyPipeline(device, r_state.pipeline, 0);
        vkDestroyCommandPool(device, r_state.commandPool, 0);

        vkDestroyRenderPass(device, r_state.renderPass, 0);
        for (u32 i = 0; i < r_state.maxFramesInFlight; i++) {
                vkDestroyBuffer(device, r_state.stagingBuffers[i], 0);
                vkDestroyBuffer(device, r_state.instanceBuffers[i], 0);
                vkFreeMemory(device, r_state.stagingsMemory[i], 0);
                vkFreeMemory(device, r_state.instancesMemory[i], 0);
                vkDestroySemaphore(device, r_state.imageAvailableSemaphore[i],
                                   0);
                vkDestroySemaphore(device, r_state.renderFinishedSemaphore[i],
                                   0);
                vkDestroyFence(device, r_state.inflightFence[i], 0);
        }

        r_destroy_swapchain();

        vkDestroyDevice(device, 0);
        vkDestroySurfaceKHR(r_state.instance, r_state.surface, 0);
        if (r_validation_layers_enabled) {
                r_destroy_debug_utils_messenger_ext(r_state.instance,
                                                    r_debug_messenger, 0);
        }
        vkDestroyInstance(r_state.instance, 0);
        glfwDestroyWindow(r_state.window);
}
