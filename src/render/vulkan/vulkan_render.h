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
        VkFramebuffer *swapchainFramebuffers;
        VkRenderPass renderPass;
        VkDescriptorSetLayout descriptorSetLayout;
        VkPipelineLayout pipelineLayout;
        VkPipeline pipeline;
        VkCommandPool commandPool;
        VkCommandBuffer setupCmdBuffer;

        VkCommandBuffer *renderCmdBuffers;
        VkSemaphore *imageAvailableSemaphore;
        VkSemaphore *renderFinishedSemaphore;
        VkFence *inflightFence;
        u32 imageIdx;

        u32 maxRects;
        u32 rectCount;
        RRectInstanceData **instancesData;
        VkBuffer *stagingBuffers;
        VkDeviceMemory *stagingsMemory;
        VkBuffer *instanceBuffers;
        VkDeviceMemory *instancesMemory;
} RState;

#endif
