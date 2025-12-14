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
	
	Arena arena;

        VkInstance instance;
} RState;

#endif
