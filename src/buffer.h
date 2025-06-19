#ifndef GLYPH_BUFFER_H
#define GLYPH_BUFFER_H

#include "glyph.h"

const int tex_w = 64;
const int tex_h = 64;

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
#endif // GLYPH_BUFFER_H
