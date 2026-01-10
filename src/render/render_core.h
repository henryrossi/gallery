#ifndef RENDER_CORE_H
#define RENDER_CORE_H

typedef struct {
        Vec2 pos0;
        Vec2 pos1;
        Vec2 src0;
        Vec2 src1;
        Vec4 colors[4];
        u32 texID;
        f32 cornerRadius;
        f32 edgeSoftness;
} RRectInstanceData;

// choose backend
#define GLFW_INCLUDE_VULKAN
#include "thirdparty/GLFW/glfw3.h"
#include "vulkan/vulkan_render.h"

static void r_init_backend(void);

static void r_begin_frame(void);
static void r_add_rect_to_batch(RRectInstanceData *rect, RTexture *tex);
static void r_dispatch_batch(void);
static void r_end_frame(void);

static void r_destroy_backend(void);

static void r_create_texture(u8 *pixels, u32 width, u32 height, RTexture *tex);
static void r_load_texture(const char *filename, RTexture *tex);

#endif // RENDER_CORE_H
