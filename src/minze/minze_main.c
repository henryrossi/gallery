// clang-format off
#include "bedrock/bedrock_inc.h"
#include "os/os.h"

#include "bedrock/bedrock_inc.c"
#include "os/os.c"
// clang-format on

#include <assert.h>

typedef struct {
        char *filename;
        u8 *pixels;
        u32 width;
        u32 height;
        u32 channels;
} Image;

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "thirdparty/stb/stb_image.h"
#endif
#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "thirdparty/stb/stb_image_write.h"
#endif

static Image load_image_file(char *filename) {
        Image res = { .filename = filename };
        res.pixels = stbi_load(filename, (int *)&res.width, (int *)&res.height,
                               (int *)&res.channels, 0);
        if (!res.pixels) {
                fprintf(stderr, "Failed to read input file %s\n", filename);
                os_abort(1);
        }
        return res;
}

static Image create_downscaled_image(Arena *arena, Image *src, char *filename) {
        Image res = {
                .filename = filename,
                .width = 64,
                .channels = 4,
        };
        res.height = src->height * ((f32)res.width / (f32)src->width);
        res.pixels = arena_alloc(arena, res.width * res.height * res.channels);
        return res;
}

static void write_image_file(Image *image) {
        u32 res = stbi_write_png(image->filename, image->width, image->height,
                                 image->channels, image->pixels,
                                 image->width * image->channels);
        if (!res) {
                fprintf(stderr, "Failed to write output file %s\n",
                        image->filename);
                os_abort(1);
        }
}

int main(int argc, char *argv[]) {
        if (argc < 3) {
                printf("Usage: ./%s <input file> <output file>\n", argv[0]);
                os_abort(1);
        }

        Image src = load_image_file(argv[1]);
        Arena *a = make_arena(mb(48));
        Image dst = create_downscaled_image(a, &src, argv[2]);

        for (u32 y = 0; y < dst.height; y++) {
                for (u32 x = 0; x < dst.width; x++) {
                        u32 x0 = (f32)x * ((f32)src.width / (f32)dst.width);
                        u32 y0 = (f32)y * ((f32)src.height / (f32)dst.height);
                        u32 x1 = (x + 1.0f) * ((f32)src.width / (f32)dst.width);
                        u32 y1
                            = (y + 1.0f) * ((f32)src.height / (f32)dst.height);

                        u32 i = (y * dst.width + x) * dst.channels;
                        Vec3f32 sum = { 0 };
                        f32 weightSum = 0;
                        for (u32 sy = y0; sy < min(y1, src.height); sy++) {
                                for (u32 sx = x0; sx < min(x1, src.width);
                                     sx++) {
                                        u32 j = (sy * src.width + sx)
                                                * src.channels;

                                        sum.x += (f32)src.pixels[j];
                                        sum.y += (f32)src.pixels[j + 1];
                                        sum.z += (f32)src.pixels[j + 2];
                                        weightSum += 1.0f;
                                }
                        }
                        sum.x = sum.x / weightSum;
                        sum.y = sum.y / weightSum;
                        sum.z = sum.z / weightSum;

                        dst.pixels[i] = (u8)sum.x;
                        dst.pixels[i + 1] = (u8)sum.y;
                        dst.pixels[i + 2] = (u8)sum.z;
                        dst.pixels[i + 3] = 255;
                }
        }

        write_image_file(&dst);

        return 0;
}
