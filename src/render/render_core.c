#include "render/render_core.h"
#include "os/os.h"

static void r_assert(b32 flag, char *msg) {
        if (!flag) {
                printf("ASSERT: %s\n", msg);
                os_abort(1);
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

static String8 r_read_shader_file(Arena *a, String8 filename) {
        String8 path = os_path(a, filename);

        OSFile file = { 0 };
        OS_FileCode code = os_open_file(path, &file, OS_FileAccess_Read);
        r_assert(code == OS_FileCode_Success, "Failed to open shader file");

        OSFileInfo info = os_file_info(file);
        String8 buf = string8_allocate_a(a, info.size);

        code = os_read_file(file, buf.data, buf.length);
        os_close_file(file);
        r_assert(code == OS_FileCode_Success, "Failed to read in shader file");

        return buf;
}
