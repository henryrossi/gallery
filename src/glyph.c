#include "glyph.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "debug.c"
#include "device.c"
#include "shader.c"
#include "surface.c"
#include "sync.c"
#include "vulkan/vulkan_core.h"

glyph_state state = {
        .current_frame = 0,
        .physical_device = VK_NULL_HANDLE,
        .surface = VK_NULL_HANDLE,
        .framebuffer_resized = 0,
};

typedef struct {
        float pos[2];
        float color[3];
} vertex;

const vertex vertices[] = {
        { { 0.0f, -0.5f }, { 1.0f, 1.0f, 1.0f } },
        { { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
        { { -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } },
};

static queue_family_indicies_t find_queue_families(glyph_state *state,
                                                   VkPhysicalDevice device) {
        queue_family_indicies_t indicies = { 0 };

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                                 NULL);
        VkQueueFamilyProperties *queue_families
            = malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                                 queue_families);

        for (uint32_t i = 0; i < queue_family_count; i++) {
                if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                        indicies.graphics.index = i;
                        indicies.graphics.valid = 1;
                }
                uint32_t supports_presentation = 0;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, state->surface,
                                                     &supports_presentation);
                if (supports_presentation) {
                        indicies.presentation.index = i;
                        indicies.presentation.valid = 1;
                }
        }

        free(queue_families);

        return indicies;
}

swapchain_support_details_t query_swapchain_support(glyph_state *state,
                                                    VkPhysicalDevice device) {
        swapchain_support_details_t details = { 0 };
        VkSurfaceKHR surface = state->surface;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                                  &details.capabilities);

        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface,
                                             &details.formats_count, NULL);
        details.formats
            = malloc(sizeof(VkSurfaceFormatKHR) * details.formats_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device, surface, &details.formats_count, details.formats);

        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface, &details.present_modes_count, NULL);
        details.present_modes
            = malloc(sizeof(VkPresentModeKHR) * details.present_modes_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
                                                  &details.present_modes_count,
                                                  details.present_modes);

        return details;
}

VkExtent2D choose_swap_extent(glyph_state *state,
                              VkSurfaceCapabilitiesKHR cap) {
        if (cap.currentExtent.width != UINT32_MAX) {
                return cap.currentExtent;
        }
        int width, height;
        glfwGetFramebufferSize(state->window, &width, &height);

        VkExtent2D actual = { width, height };

        if (actual.width < cap.minImageExtent.width) {
                actual.width = cap.minImageExtent.width;
        } else if (actual.width > cap.maxImageExtent.width) {
                actual.width = cap.maxImageExtent.width;
        }
        if (actual.height < cap.minImageExtent.height) {
                actual.height = cap.minImageExtent.height;
        } else if (actual.height > cap.maxImageExtent.height) {
                actual.height = cap.maxImageExtent.height;
        }

        return actual;
}

VkPresentModeKHR choose_sc_present_mode(swapchain_support_details_t details) {
        for (uint32_t i = 0; i < details.present_modes_count; i++) {
                if (details.present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                        return details.present_modes[i];
                }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
}

VkSurfaceFormatKHR
choose_sc_surface_format(swapchain_support_details_t details) {
        for (uint32_t i = 0; i < details.formats_count; i++) {
                VkSurfaceFormatKHR format = details.formats[i];
                if (format.format == VK_FORMAT_B8G8R8A8_SRGB
                    && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                        return format;
                }
        }
        return details.formats[0];
}

// Creates swapchain. Returns 1 on success, 0 on failure.
static int create_swapchain(glyph_state *state) {
        swapchain_support_details_t support
            = query_swapchain_support(state, state->physical_device);

        VkSurfaceFormatKHR surface_format = choose_sc_surface_format(support);
        VkPresentModeKHR present_mode = choose_sc_present_mode(support);
        VkExtent2D extent = choose_swap_extent(state, support.capabilities);

        uint32_t image_count = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount > 0
            && image_count > support.capabilities.maxImageCount) {
                image_count = support.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createinfo = {
                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                .surface = state->surface,
                .minImageCount = image_count,
                .imageFormat = surface_format.format,
                .imageColorSpace = surface_format.colorSpace,
                .imageExtent = extent,
                .imageArrayLayers = 1,
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        };

        queue_family_indicies_t indicies
            = find_queue_families(state, state->physical_device);
        uint32_t queue_family_indicies[]
            = { indicies.graphics.index, indicies.presentation.index };

        if (indicies.graphics.index != indicies.presentation.index) {
                createinfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                createinfo.queueFamilyIndexCount = 2;
                createinfo.pQueueFamilyIndices = queue_family_indicies;
        } else {
                createinfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createinfo.preTransform = support.capabilities.currentTransform;
        createinfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createinfo.presentMode = present_mode;
        createinfo.clipped = VK_TRUE;
        createinfo.oldSwapchain = VK_NULL_HANDLE;

        free(support.formats);
        free(support.present_modes);

        VkResult res = vkCreateSwapchainKHR(state->device, &createinfo, NULL,
                                            &state->swapchain);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create swapchain: %s\n",
                        string_VkResult(res));
                return 0;
        }

        vkGetSwapchainImagesKHR(state->device, state->swapchain,
                                &state->swapchain_images_count, NULL);
        state->swapchain_images
            = malloc(sizeof(VkImage) * state->swapchain_images_count);
        vkGetSwapchainImagesKHR(state->device, state->swapchain,
                                &state->swapchain_images_count,
                                state->swapchain_images);
        state->swapchain_format = surface_format.format;
        state->swapchain_extent = extent;

        return 1;
}

// Creates image views. Returns 1 on success, 0 on failure.
static int create_image_views(glyph_state *state) {
        state->swapchain_image_views_count = state->swapchain_images_count;
        state->swapchain_image_views
            = malloc(sizeof(VkImageView) * state->swapchain_image_views_count);

        for (uint32_t i = 0; i < state->swapchain_image_views_count; i++) {
                VkImageViewCreateInfo createinfo = {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                        .image = state->swapchain_images[i],
                        .viewType = VK_IMAGE_VIEW_TYPE_2D,
                        .format = state->swapchain_format,
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

                VkResult res
                    = vkCreateImageView(state->device, &createinfo, NULL,
                                        &state->swapchain_image_views[i]);
                if (res != VK_SUCCESS) {
                        fprintf(stderr, "Failed to create image view: %s\n",
                                string_VkResult(res));
                        return 0;
                }
        }

        return 1;
}

typedef struct {
        const char **names;
        uint32_t count;
} extensions_t;

static extensions_t get_required_extensions(void) {
        extensions_t exts = { 0 };

        // The following is useful for checking the existence of an
        // extension uint32_t supported_ext_count = 0;
        // vkEnumerateInstanceExtensionProperties(NULL,
        // &supported_ext_count,
        //                                        NULL);
        // VkExtensionProperties *supported_exts
        //     = malloc(sizeof(VkExtensionProperties) *
        //     supported_ext_count);
        // vkEnumerateInstanceExtensionProperties(NULL,
        // &supported_ext_count,
        //                                        supported_exts);
        // printf("Available supported extensions:\n");
        // for (uint32_t i = 0; i < supported_ext_count; i++) {
        //         printf("  %s\n", supported_exts[i].extensionName);
        // }
        // free(supported_exts);

        uint32_t glfw_ext_count = 0;
        const char **glfw_exts
            = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

        exts.count = enable_validation_layers ? glfw_ext_count + 3
                                              : glfw_ext_count + 2;
        exts.names = malloc(sizeof(const char *) * exts.count);

        for (int i = 0; i < glfw_ext_count; i++) {
                exts.names[i] = glfw_exts[i];
        }

        if (enable_validation_layers) {
                exts.names[exts.count - 3] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }
        exts.names[exts.count - 2]
            = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
        exts.names[exts.count - 1] = "VK_KHR_get_physical_device_properties2";

        return exts;
}

// Creates a Vulkan instance. Returns 1 on success, 0 on error.
static int create_instance(glyph_state *state) {
        VkApplicationInfo appinfo = {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName = "glyph",
                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                .pEngineName = "No Engine",
                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                .apiVersion = VK_API_VERSION_1_0,
        };

        extensions_t exts = get_required_extensions();
        VkInstanceCreateInfo createinfo = {
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pApplicationInfo = &appinfo,
                .enabledExtensionCount = exts.count,
                .ppEnabledExtensionNames = exts.names,
                .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
        };

        VkDebugUtilsMessengerCreateInfoEXT debug_createinfo = { 0 };
        if (enable_validation_layers) {
                if (!check_validation_layer_support()) {
                        return 0;
                }
                createinfo.enabledLayerCount = validation_layer_count;
                createinfo.ppEnabledLayerNames = validation_layers;
                populate_debug_messenger_createinfo(&debug_createinfo);
                createinfo.pNext = &debug_createinfo;
        } else {
                createinfo.enabledLayerCount = 0;
        }

        VkResult res = vkCreateInstance(&createinfo, NULL, &state->instance);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create instance. %s\n",
                        string_VkResult(res));
                return 0;
        }
        return 1;
}

// Create render pass. Returns 1 on success, 0 on failure.
static int create_render_pass(glyph_state *state) {
        VkAttachmentDescription color_attachment = {
                .format = state->swapchain_format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        };

        VkAttachmentReference color_attach_ref = {
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };

        VkSubpassDescription subpass = {
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = 1,
                .pColorAttachments = &color_attach_ref,
        };

        VkSubpassDependency dependency = {
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        };

        VkRenderPassCreateInfo createinfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments = &color_attachment,
                .subpassCount = 1,
                .pSubpasses = &subpass,
                .dependencyCount = 1,
                .pDependencies = &dependency,
        };

        VkResult res = vkCreateRenderPass(state->device, &createinfo, NULL,
                                          &state->render_pass);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create render pass: %s\n",
                        string_VkResult(res));
                return 0;
        }
        return 1;
}

// Create graphics pipeline. Returns 1 on success, 0 on failure.
static int create_graphics_pipeline(glyph_state *state) {
        VkShaderModule vert
            = create_shader_module(state->device, "shaders/vert.spv");
        VkShaderModule frag
            = create_shader_module(state->device, "shaders/frag.spv");
        // need to properly clean up shader modules
        if (vert == VK_NULL_HANDLE || frag == VK_NULL_HANDLE) {
                return 0;
        }

        VkPipelineShaderStageCreateInfo vert_stage_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vert,
                .pName = "main",
        };

        VkPipelineShaderStageCreateInfo frag_stage_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = frag,
                .pName = "main",
        };

        VkPipelineShaderStageCreateInfo shader_stages[] = {
                vert_stage_info,
                frag_stage_info,
        };

        VkDynamicState dynamic_states[] = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo dynamic_state = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount
                = sizeof(dynamic_states) / sizeof(dynamic_states[0]),
                .pDynamicStates = dynamic_states,
        };

        VkVertexInputBindingDescription vertex_binding_desc = {
                .binding = 0,
                .stride = sizeof(vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };

        VkVertexInputAttributeDescription vertex_attr_desc[2] = {
                {
                        .binding = 0,
                        .location = 0,
                        .format = VK_FORMAT_R32G32_SFLOAT,
                        .offset = offsetof(vertex, pos),
                },
                {
                        .binding = 0,
                        .location = 1,
                        .format = VK_FORMAT_R32G32B32_SFLOAT,
                        .offset = offsetof(vertex, color),
                },
        };

        VkPipelineVertexInputStateCreateInfo vertex_input_info = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &vertex_binding_desc,
                .vertexAttributeDescriptionCount = 2,
                .pVertexAttributeDescriptions = vertex_attr_desc,
        };

        VkPipelineInputAssemblyStateCreateInfo input_assembly = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                .primitiveRestartEnable = VK_FALSE,
        };

        VkPipelineViewportStateCreateInfo viewport_state = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .scissorCount = 1,
        };

        VkPipelineRasterizationStateCreateInfo rasterizer = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .depthClampEnable = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .lineWidth = 1.0f,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .frontFace = VK_FRONT_FACE_CLOCKWISE,
                .depthBiasEnable = VK_FALSE,
        };

        VkPipelineMultisampleStateCreateInfo multisampling = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .sampleShadingEnable = VK_FALSE,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };

        VkPipelineColorBlendAttachmentState color_blend_attachment = {
                .colorWriteMask
                = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                  | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                .blendEnable = VK_FALSE,
        };

        VkPipelineColorBlendStateCreateInfo color_blending = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .logicOpEnable = VK_FALSE,
                .logicOp = VK_LOGIC_OP_COPY,
                .attachmentCount = 1,
                .pAttachments = &color_blend_attachment,
        };

        VkPipelineLayoutCreateInfo pipeline_layout_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = 0,
                .pushConstantRangeCount = 0,
        };

        VkResult res
            = vkCreatePipelineLayout(state->device, &pipeline_layout_info, NULL,
                                     &state->pipeline_layout);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create pipeline layout: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkGraphicsPipelineCreateInfo pipeline_info = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .stageCount = 2,
                .pStages = shader_stages,
                .pVertexInputState = &vertex_input_info,
                .pInputAssemblyState = &input_assembly,
                .pViewportState = &viewport_state,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pDepthStencilState = NULL,
                .pColorBlendState = &color_blending,
                .pDynamicState = &dynamic_state,
                .layout = state->pipeline_layout,
                .renderPass = state->render_pass,
                .subpass = 0,
                .basePipelineHandle = VK_NULL_HANDLE,
                .basePipelineIndex = -1,
        };

        res = vkCreateGraphicsPipelines(state->device, VK_NULL_HANDLE, 1,
                                        &pipeline_info, NULL,
                                        &state->graphics_pipeline);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create graphics pipelines: %s\n",
                        string_VkResult(res));
                return 0;
        }

        vkDestroyShaderModule(state->device, vert, NULL);
        vkDestroyShaderModule(state->device, frag, NULL);

        return 1;
}

// Create framebuffers. Returns 1 on success, 0 on failure.
static int create_framebuffers(glyph_state *state) {
        state->swapchain_framebuffer_count = state->swapchain_image_views_count;
        state->swapchain_framebuffers = malloc(
            sizeof(VkFramebuffer) * state->swapchain_framebuffer_count);

        for (uint32_t i = 0; i < state->swapchain_framebuffer_count; i++) {
                VkImageView attachments[] = {
                        state->swapchain_image_views[i],
                };

                VkFramebufferCreateInfo createinfo = {
                        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                        .renderPass = state->render_pass,
                        .attachmentCount = 1,
                        .pAttachments = attachments,
                        .width = state->swapchain_extent.width,
                        .height = state->swapchain_extent.height,
                        .layers = 1,
                };

                VkResult res
                    = vkCreateFramebuffer(state->device, &createinfo, NULL,
                                          state->swapchain_framebuffers + i);
                if (res != VK_SUCCESS) {
                        fprintf(stderr, "Failed to create framebuffer: %s\n",
                                string_VkResult(res));
                        return 0;
                }
        }

        return 1;
}

static void cleanup_swapchain(glyph_state *state) {
        for (uint32_t i = 0; i < state->swapchain_framebuffer_count; i++) {
                vkDestroyFramebuffer(state->device,
                                     state->swapchain_framebuffers[i], NULL);
        }
        for (uint32_t i = 0; i < state->swapchain_image_views_count; i++) {
                vkDestroyImageView(state->device,
                                   state->swapchain_image_views[i], NULL);
        }
        vkDestroySwapchainKHR(state->device, state->swapchain, NULL);
}

// Recreates swap chain. Returns 1 on success, 0 on failure.
static int recreate_swapchain(glyph_state *state) {
        int width = 0, height = 0;
        glfwGetFramebufferSize(state->window, &width, &height);
        while (width == 0 || height == 0) {
                glfwGetFramebufferSize(state->window, &width, &height);
                glfwWaitEvents();
        }

        vkDeviceWaitIdle(state->device);

        cleanup_swapchain(state);

        int res = create_swapchain(state);
        if (!res)
                return 0;

        res = create_image_views(state);
        if (!res)
                return 0;

        res = create_framebuffers(state);
        if (!res)
                return 0;

        return 1;
}

// Creates a command pool. Returns 1 on success, 0 on failure.
static int create_command_pool(glyph_state *state) {
        queue_family_indicies_t indicies
            = find_queue_families(state, state->physical_device);

        VkCommandPoolCreateInfo createinfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = indicies.graphics.index,
        };

        VkResult res = vkCreateCommandPool(state->device, &createinfo, NULL,
                                           &state->command_pool);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create command pool: %s\n",
                        string_VkResult(res));
                return 0;
        }

        return 1;
}

// Create command buffer. Return 1 on success, 0 on failure.
static int create_command_buffer(glyph_state *state) {
        VkCommandBufferAllocateInfo allocinfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = state->command_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
        };

        VkResult res = vkAllocateCommandBuffers(state->device, &allocinfo,
                                                state->command_buffer);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to allocate command buffers: %s\n",
                        string_VkResult(res));
                return 0;
        }

        return 1;
}

// Write commands into the command buffer. Returns 1 on success, 0 on failure
static int record_command_buffer(glyph_state *state, VkCommandBuffer cmd_buffer,
                                 uint32_t image_index) {
        VkCommandBufferBeginInfo begininfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = 0,
                .pInheritanceInfo = NULL,
        };

        VkResult res = vkBeginCommandBuffer(cmd_buffer, &begininfo);
        if (res != VK_SUCCESS) {
                fprintf(stderr,
                        "Failed to being command buffer recording: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkClearValue clear_value = { { { 0.3f, 0.3f, 0.3f, 1.0f } } };
        VkRenderPassBeginInfo passinfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = state->render_pass,
                .framebuffer = state->swapchain_framebuffers[image_index],
                .renderArea.offset = { 0, 0 },
                .renderArea.extent = state->swapchain_extent,
                .clearValueCount = 1,
                .pClearValues = &clear_value,
        };

        vkCmdBeginRenderPass(cmd_buffer, &passinfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          state->graphics_pipeline);

        VkViewport viewport = {
                .x = 0.0f,
                .y = 0.0f,
                .width = (float)state->swapchain_extent.width,
                .height = (float)state->swapchain_extent.height,
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
        };
        vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

        VkRect2D scissor = {
                .offset = { 0, 0 },
                .extent = state->swapchain_extent,
        };
        vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

        VkBuffer vertex_buffers[] = { state->vertex_buffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd_buffer, 0, 1, vertex_buffers, offsets);

        uint32_t vertices_size = sizeof(vertices) / sizeof(vertices[0]);
        vkCmdDraw(cmd_buffer, vertices_size, 1, 0, 0);
        vkCmdEndRenderPass(cmd_buffer);

        res = vkEndCommandBuffer(cmd_buffer);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to record command buffer: %s\n",
                        string_VkResult(res));
                return 0;
        }

        return 1;
}

// Submit a frame to be drawn. Returns 1 on success, 0 on failure.
static int draw_frame(glyph_state *state) {
        uint32_t current_frame = state->current_frame;
        VkDevice device = state->device;
        vkWaitForFences(device, 1, state->inflight_fence + current_frame,
                        VK_TRUE, UINT64_MAX);

        uint32_t image_index;
        VkResult res = vkAcquireNextImageKHR(
            device, state->swapchain, UINT64_MAX,
            state->image_available_semaphore[current_frame], VK_NULL_HANDLE,
            &image_index);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR
            || state->framebuffer_resized) {
                state->framebuffer_resized = 0;
                recreate_swapchain(state);
        } else if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to acquire swap chain image: %s\n",
                        string_VkResult(res));
                return 0;
        }

        vkResetFences(device, 1, state->inflight_fence + current_frame);

        vkResetCommandBuffer(state->command_buffer[current_frame], 0);
        record_command_buffer(state, state->command_buffer[current_frame],
                              image_index);

        VkPipelineStageFlags wait_stages[] = {
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        };
        VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores
                = state->image_available_semaphore + current_frame,
                .pWaitDstStageMask = wait_stages,
                .commandBufferCount = 1,
                .pCommandBuffers = state->command_buffer + current_frame,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores
                = state->render_finished_semaphore + current_frame,
        };

        res = vkQueueSubmit(state->graphics_queue, 1, &submit_info,
                            state->inflight_fence[current_frame]);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to sumbit draw command buffer: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkSwapchainKHR swapchains[] = { state->swapchain };
        VkPresentInfoKHR present_info = {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores
                = state->render_finished_semaphore + current_frame,
                .swapchainCount = 1,
                .pSwapchains = swapchains,
                .pImageIndices = &image_index,
                .pResults = NULL,
        };

        res = vkQueuePresentKHR(state->presentation_queue, &present_info);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
                recreate_swapchain(state);
        } else if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to present swap chain image: %s",
                        string_VkResult(res));
                return 0;
        }

        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

        return 1;
}

// ?
int64_t find_memory_type(VkPhysicalDevice phy_device, uint32_t type_filter,
                         VkMemoryPropertyFlags props) {
        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(phy_device, &mem_props);

        for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
                if (type_filter & (1 << i)
                    && (mem_props.memoryTypes[i].propertyFlags & props)
                           == props) {
                        return i;
                }
        }

        return -1;
}

typedef struct {
        VkDevice device;
        VkPhysicalDevice physical_device;
        VkDeviceSize size;
        VkBufferUsageFlags usage;
        VkMemoryPropertyFlags props;
        VkBuffer *buffer;
        VkDeviceMemory *memory;
} buffer_create_info;

// Create a buffer. Returns 1 on success, 0 on failure.
static int create_buffer(buffer_create_info params) {
        VkBufferCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = params.size,
                .usage = params.usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        VkResult res
            = vkCreateBuffer(params.device, &create_info, NULL, params.buffer);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create buffer: %s\n",
                        string_VkResult(res));
                return 0;
        }

        VkMemoryRequirements mem_requirements;
        vkGetBufferMemoryRequirements(params.device, *params.buffer,
                                      &mem_requirements);

        int64_t mem_type = find_memory_type(
            params.physical_device, mem_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (mem_type < 0) {
                fprintf(stderr, "Failed to find suitable memory type\n");
                return 0;
        }

        VkMemoryAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = mem_requirements.size,
                .memoryTypeIndex = mem_type,
        };

        res = vkAllocateMemory(params.device, &alloc_info, NULL, params.memory);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to allocate buffer memory: %s\n",
                        string_VkResult(res));
                return 0;
        }

        vkBindBufferMemory(params.device, *params.buffer, *params.memory, 0);

        return 1;
}

typedef struct {
        VkDevice device;
        VkQueue graphics_queue;
        VkCommandPool cmdpool;
        VkBuffer src;
        VkBuffer dst;
        VkDeviceSize size;
} copy_buffer_info;

static void copy_buffer(copy_buffer_info params) {
        VkCommandBufferAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandPool = params.cmdpool,
                .commandBufferCount = 1,
        };

        VkCommandBuffer cmd_buffer;
        vkAllocateCommandBuffers(params.device, &alloc_info, &cmd_buffer);

        VkCommandBufferBeginInfo begin_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(cmd_buffer, &begin_info);

        VkBufferCopy copy_region = {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = params.size,
        };
        vkCmdCopyBuffer(cmd_buffer, params.src, params.dst, 1, &copy_region);

        vkEndCommandBuffer(cmd_buffer);

        VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &cmd_buffer,
        };
        vkQueueSubmit(params.graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(params.graphics_queue);

        vkFreeCommandBuffers(params.device, params.cmdpool, 1, &cmd_buffer);
}

// Create vertex buffers. Returns 1 on success, 0 on failure.
static int create_vertex_buffer(glyph_state *state) {
        VkDevice device = state->device;
        VkDeviceSize size = sizeof(vertices);

        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
        buffer_create_info staging = {
                .device = device,
                .physical_device = state->physical_device,
                .size = size,
                .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                .buffer = &staging_buffer,
                .memory = &staging_buffer_memory,
        };
        if (!create_buffer(staging)) {
                return 0;
        }

        void *data;
        vkMapMemory(device, staging_buffer_memory, 0, size, 0, &data);
        memcpy(data, vertices, size);
        vkUnmapMemory(device, staging_buffer_memory);

        buffer_create_info vertex = {
                .device = device,
                .physical_device = state->physical_device,
                .size = size,
                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                         | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                .buffer = &state->vertex_buffer,
                .memory = &state->vertex_buffer_memory,
        };
        if (!create_buffer(vertex)) {
                return 0;
        }

        copy_buffer_info params = {
                .device = device,
                .graphics_queue = state->graphics_queue,
                .cmdpool = state->command_pool,
                .src = staging_buffer,
                .dst = state->vertex_buffer,
                .size = size,
        };
        copy_buffer(params);

        vkDestroyBuffer(device, staging_buffer, NULL);
        vkFreeMemory(device, staging_buffer_memory, NULL);

        return 1;
}

int main(int argc, char **argv) {

        if (!init_window(&state)) {
                return 1;
        }
        if (!create_instance(&state)) {
                return 1;
        }
        if (enable_validation_layers
            && !setup_debug_messenger(state.instance)) {
                return 1;
        }
        if (!create_surface(&state)) {
                return 1;
        }
        if (!pick_physical_device(&state)) {
                return 1;
        }
        if (!create_logical_device(&state)) {
                return 1;
        }
        if (!create_swapchain(&state)) {
                return 1;
        }
        if (!create_image_views(&state)) {
                return 1;
        }
        if (!create_render_pass(&state)) {
                return 1;
        }
        if (!create_graphics_pipeline(&state)) {
                return 1;
        }
        if (!create_framebuffers(&state)) {
                return 1;
        }
        if (!create_command_pool(&state)) {
                return 1;
        }
        if (!create_vertex_buffer(&state)) {
                return 1;
        }
        if (!create_command_buffer(&state)) {
                return 1;
        }
        if (!create_sync_objects(&state)) {
                return 1;
        }

        while (!glfwWindowShouldClose(state.window)) {
                glfwPollEvents();
                draw_frame(&state);
        }
        VkDevice device = state.device;
        vkDeviceWaitIdle(device);

        if (enable_validation_layers) {
                destroy_debug_utils_messenger_ext(state.instance,
                                                  debug_messenger, NULL);
        }
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroySemaphore(device, state.image_available_semaphore[i],
                                   NULL);
                vkDestroySemaphore(device, state.render_finished_semaphore[i],
                                   NULL);
                vkDestroyFence(device, state.inflight_fence[i], NULL);
        }
        vkDestroyCommandPool(device, state.command_pool, NULL);
        cleanup_swapchain(&state);
        vkDestroyBuffer(device, state.vertex_buffer, NULL);
        vkFreeMemory(device, state.vertex_buffer_memory, NULL);
        vkDestroyPipeline(device, state.graphics_pipeline, NULL);
        vkDestroyPipelineLayout(device, state.pipeline_layout, NULL);
        vkDestroyRenderPass(device, state.render_pass, NULL);
        vkDestroyDevice(device, NULL);
        vkDestroySurfaceKHR(state.instance, state.surface, NULL);
        vkDestroyInstance(state.instance, NULL);
        glfwDestroyWindow(state.window);

        glfwTerminate();
        return 0;
}
