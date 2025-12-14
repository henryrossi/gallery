#include "graphicsPipeline.h"

#include <stdio.h>

#include "shader.c"
#include "vulkan/vulkan_core.h"

// Create render pass. Returns 1 on success, 0 on failure.
static int create_render_pass(GlyphEngine *engine) {
        VkAttachmentDescription color_attachment = {
                .format = engine->swapchain_format,
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

        VkResult res = vkCreateRenderPass(engine->device, &createinfo, NULL,
                                          &engine->render_pass);
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create render pass: %s\n",
                        string_VkResult(res));
                return 0;
        }
        return 1;
}

/* State required to request a graphics pipeline:
 *
 * Shader filenames
 * Color blending information?
 * Vertex input state
 *      Vertex binding
 *      Vertex attr
 * Pipeline layout (Handle)
 *      Descriptor set layouts (Handle)
 *              Uniform layout binding
 * Render pass (Handle)
 */

// Create graphics pipeline. Returns 1 on success, 0 on failure.
static VkPipeline
createGraphicsPipeline(GraphicsPipelineCreateInfo *createInfo) {
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
                return VK_NULL_HANDLE;
        }

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
        if (res != VK_SUCCESS) {
                fprintf(stderr, "Failed to create graphics pipelines: %s\n",
                        string_VkResult(res));
                return VK_NULL_HANDLE;
        }

        vkDestroyShaderModule(device, vert, NULL);
        vkDestroyShaderModule(device, frag, NULL);

        return pipeline;
}
