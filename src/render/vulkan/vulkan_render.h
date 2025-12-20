#ifndef VULKAN_RENDER_H
#define VULKAN_RENDER_H

#include "vulkan/vulkan.h"
// #include "vulkan/vk_platform.h"
#include "vulkan/vk_enum_string_helper.h"

// Render code:
// interfaces with window
// begins/ends ui render pass (or later other kinds of render passes)
// allocates gpu buffers
// "batches" items to be rendered

typedef struct {
        u32 width;
        u32 height;

        u32 currentFrame;

        Arena arena;

        u32 maxFramesInFlight;
        VkInstance instance;
        GLFWwindow *window;
        VkSurfaceKHR surface;
        VkPhysicalDevice physicalDevice;
        VkPhysicalDeviceProperties physicalDeviceProps;
        VkQueue graphicsQueue;
        u32 graphicsQueueIdx;
        VkQueue presentQueue;
        u32 presentQueueIdx;
        u32 imageCount;
        VkExtent2D resolution;
        VkFormat colorFormat;
        VkColorSpaceKHR colorSpace;
        VkPresentModeKHR presentMode;
        VkSurfaceTransformFlagBitsKHR preTransform;
        VkDevice device;
        VkSwapchainKHR swapchain;
        VkImage *swapchainImages;
        VkImageView *swapchainImageViews;
        VkRenderPass renderPass;
        VkFramebuffer *swapchainFramebuffers;
        VkCommandPool commandPool;
        VkCommandBuffer *renderCmdBuffers;
        VkCommandBuffer setupCmdBuffer;
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexMemory;
        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
        VkSemaphore *imageAvailableSemaphore;
        VkSemaphore *renderFinishedSemaphore;
        VkFence *inflightFence;
} RState;

typedef struct {
        VkBuffer buffer;
        VkDeviceMemory memory;
} RShaderUniform;

typedef struct {
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
} RGraphicsPipelineCreateInfo;

static VkPipeline
r_create_graphics_pipeline(RGraphicsPipelineCreateInfo *createInfo);

#endif
