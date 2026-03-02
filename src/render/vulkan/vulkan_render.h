#ifndef RENDER_VULKAN_VULKAN_RENDER_H
#define RENDER_VULKAN_VULKAN_RENDER_H

#include "vulkan/vulkan.h"
// #include "vulkan/vk_platform.h"
#include "vulkan/vk_enum_string_helper.h"

typedef struct {
        u64 key;
        VkImage image;
        VkDeviceMemory memory;
        VkImageView view;
        u32 lastBindingIndex;
        u32 width;
        u32 height;
} RTexture;

typedef struct {
        Vec4u8 *data; 
        VkBuffer *stagingBuffers;
        VkDeviceMemory *stagingMemory;
        RTexture *textures;
} RDynamicTexture;

typedef struct {
        RTexture **prev;
        RTexture **cur;
        u32 freeTop;
        u16 *free;
} RIndexingInfo;

typedef struct {
        u32 currentFrame;
        u64 frameCount;

        Arena arena;
        Arena scratch;
        Arena swapchainArena;

        b32 framebufferResized;
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

        u32 maxTextures;
        VkDescriptorPool descriptorPool;
        VkDescriptorSet *descriptorSets;
        VkSampler sampler;
        RIndexingInfo *indexingInfo;
        u32 writeDescriptorSetsCount;
        VkWriteDescriptorSet *writeDescriptorSets;
        VkDescriptorImageInfo *writeImageInfo;
        RTexture blankTex;
} RState;

#endif // RENDER_VULKAN_VULKAN_RENDER_H
