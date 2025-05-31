#include "glyph.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "buffer.c"
#include "debug.c"
#include "device.c"
#include "shader.c"
#include "surface.c"
#include "swapchain.c"
#include "sync.c"

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
        vkCmdBindIndexBuffer(cmd_buffer, state->index_buffer, 0,
                             VK_INDEX_TYPE_UINT16);

        uint32_t indices_size = sizeof(indices) / sizeof(indices[0]);
        vkCmdDrawIndexed(cmd_buffer, indices_size, 1, 0, 0, 0);
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

int main(int argc, char **argv) {

        glyph_state state = {
                .current_frame = 0,
                .physical_device = VK_NULL_HANDLE,
                .surface = VK_NULL_HANDLE,
                .framebuffer_resized = 0,
        };

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
        if (!create_index_buffer(&state)) {
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
        vkDestroyBuffer(device, state.index_buffer, NULL);
        vkFreeMemory(device, state.index_buffer_memory, NULL);
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
