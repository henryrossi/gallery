#ifndef RENDER_CORE_H
#define RENDER_CORE_H

typedef struct {
        Vec2 pos0;
        Vec2 pos1;
        Vec4 colors[4];
} RRectInstanceData;

static void r_init_backend(void);

static void r_begin_frame(void);
static void r_add_rect_to_batch(RRectInstanceData *rect);
static void r_dispatch_batch(void);
static void r_end_frame(void);

static void r_destroy_backend(void);

// choose backend
#define GLFW_INCLUDE_VULKAN
#include "thirdparty/GLFW/glfw3.h"
#include "vulkan/vulkan_render.h"

#endif // RENDER_CORE_H
