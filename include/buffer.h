#ifndef GLYPH_BUFFER_H
#define GLYPH_BUFFER_H

#include "glyph.h"

typedef struct {
        VkDevice device;
        VkPhysicalDevice physical_device;
        VkDeviceSize size;
        VkBufferUsageFlags usage;
        VkMemoryPropertyFlags props;
        VkBuffer *buffer;
        VkDeviceMemory *memory;
} BufferCreateInfo;

static int createBuffer(BufferCreateInfo *createInfo);

typedef struct {
        uint32_t width;
        uint32_t height;
        VkFormat format;
        VkImageTiling tiling;
        VkImageUsageFlags usage;
        VkMemoryPropertyFlags props;
        VkImage *image;
        VkDeviceMemory *imageMemory;
        VkDevice device;
        VkPhysicalDevice physicalDevice;
} ImageCreateInfo;

static int createImage(ImageCreateInfo *createInfo);

typedef struct {
        VkDevice device;
        VkQueue graphics_queue;
        VkCommandPool cmdpool;
        VkBuffer src;
        VkBuffer dst;
        VkDeviceSize size;
} CopyBufferInfo;

static void copyBuffer(CopyBufferInfo *copyInfo);

typedef struct {
        VkBuffer buffer;
        VkImage image;
        uint32_t width;
        uint32_t height;
        VkDevice device;
        VkCommandPool cmdPool;
        VkQueue graphicsQueue;
} CopyBufferToImageInfo;

static void copyBufferToImage(CopyBufferToImageInfo *copyInfo);

typedef struct {
        VkImage image;
        VkFormat format;
        VkImageLayout oldLayout;
        VkImageLayout newLayout;
        VkDevice device;
        VkCommandPool cmdPool;
        VkQueue graphicsQueue;
} TransitionImageLayoutInfo;

static void transitionImageLayout(TransitionImageLayoutInfo *params);
 
static int createImageView(VkDevice device, VkImage image, VkFormat format,
                             VkImageView *view);


typedef struct {
  VkBuffer *pBuffers[MAX_FRAMES_IN_FLIGHT];
  VkDeviceMemory *pMemory[MAX_FRAMES_IN_FLIGHT];
  void **pMappedMemory[MAX_FRAMES_IN_FLIGHT];
  VkDeviceSize uniformSize;
  VkDevice device;
  VkPhysicalDevice phyDevice;
} UniformBufferCreateInfo;

static int createUniformBuffer(UniformBufferCreateInfo *createInfo);

#endif // GLYPH_BUFFER_H
