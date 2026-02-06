#ifndef RENDER_CORE_H
#define RENDER_CORE_H

typedef struct {
        Rng2f32 pos;
        Rng2f32 src;
        Vec4f32 colors[4];
        u32 texID;
        f32 cornerRadius;
        f32 edgeSoftness;
} RRectInstanceData;

// choose backend
#define GLFW_INCLUDE_VULKAN
#include "thirdparty/GLFW/glfw3.h"
#include "vulkan/vulkan_render.h"

static void r_init_backend(void);

static u64 r_get_frame_count(void);

static void r_begin_frame(void);
static void r_add_rect_to_batch(RRectInstanceData *rect, RTexture *tex);
static void r_dispatch_batch(void);
static void r_end_frame(void);

static void r_destroy_backend(void);

static void r_create_texture(u8 *pixels, u32 width, u32 height, RTexture *tex);
static void r_load_texture(const char *filename, RTexture *tex);

#endif // RENDER_CORE_H
