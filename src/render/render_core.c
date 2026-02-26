static void r_assert(b32 flag, char *msg) {
        if (!flag) {
                printf("ASSERT: %s\n", msg);
                u32 *bomb = 0;
                *bomb = 1;
        }
}

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "thirdparty/stb/stb_image.h"
#endif
static void r_load_texture(const char *filename, RTexture *texture) {
        int width, height, n;
        u8 *pixels = stbi_load(filename, &width, &height, &n, STBI_rgb_alpha);
        r_assert(pixels != 0, "Failed to load texture image");

        r_create_texture(pixels, width, height, n, texture);

        stbi_image_free(pixels);
}

// TODO: hr: the whole shader section needs to be updated to be os independent
// #include <sys/stat.h>

static String8 r_read_shader_file(const char *filename, String8 buf) {
        FILE *fp = fopen(filename, "rb");
        r_assert(fp != NULL, "Failed to open shader file");

        size_t res = fread(buf.data, 1, buf.length, fp);
        fclose(fp);
        r_assert(res > 0, "Failed to read in shader file");

        return buf;
}

static String8 r_alloc_shader_buffer(Arena *a, const char *filename) {
        String8 res;
        OSFileInfo info = os_file_info(filename);
        res = string8_allocate(a, info.size);
        return res;
}

// hr: choose backend
#include "render/vulkan/vulkan_render.c"
