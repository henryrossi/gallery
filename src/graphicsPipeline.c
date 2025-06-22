#include "graphicsPipeline.h"

#include <stdio.h>

#include "shader.c"
#include "vulkan/vulkan_core.h"

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
static int createGraphicsPipeline(GraphicsPipelineCreateInfo *createInfo) {
        VkDevice device = createInfo->device;

        if (!createInfo->vertFile || !createInfo->fragFile) {
                fprintf(stderr, "Shader filename is null pointer.\n");
                return 0;
        }
        VkShaderModule vert
            = create_shader_module(device, createInfo->vertFile);
        VkShaderModule frag
            = create_shader_module(device, createInfo->fragFile);
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
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp = VK_BLEND_OP_ADD,
        };

        VkPipelineColorBlendStateCreateInfo color_blending = {
                .sType
                = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .logicOpEnable = VK_FALSE,
                .logicOp = VK_LOGIC_OP_COPY,
                .attachmentCount = 1,
                .pAttachments = &color_blend_attachment,
        };

        VkGraphicsPipelineCreateInfo pipelineInfo = {
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .stageCount = 2,
                .pStages = shader_stages,
                .pVertexInputState = createInfo->vertexInputInfo,
                .pInputAssemblyState = &input_assembly,
                .pViewportState = &viewport_state,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pDepthStencilState = NULL,
                .pColorBlendState = &color_blending,
                .pDynamicState = &dynamic_state,
                .layout = createInfo->layout,
                .renderPass = createInfo->renderPass,
                .subpass = 0,
                .basePipelineHandle = VK_NULL_HANDLE,
                .basePipelineIndex = -1,
        };

        VkResult res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                                                 &pipelineInfo, NULL,
                                                 createInfo->pipeline);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create graphics pipelines: %s\n",
                        string_VkResult(res));
                return 0;
        }

        vkDestroyShaderModule(device, vert, NULL);
        vkDestroyShaderModule(device, frag, NULL);

        return 1;
}
