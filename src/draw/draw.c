static RTexture dr_font = { 0 };

#ifndef STB_TRUETYPE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "thirdparty/stb/stb_truetype.h"
#endif

#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "thirdparty/stb/stb_image_write.h"
#endif

#define TTF_BUFFER_SIZE 1 << 18
Arena a;
unsigned char *dr_ttf_buffer;

#define ATLAS_WIDTH 512
unsigned char bitmap[ATLAS_WIDTH * ATLAS_WIDTH];

stbtt_bakedchar cdata[96]; // ASCII 32..126 is 95 glyphs

static void dr_init_font(void) {
        a = make_arena(1 << 26);

        dr_ttf_buffer = arena_alloc(&a, TTF_BUFFER_SIZE);
        fread(dr_ttf_buffer, 1, TTF_BUFFER_SIZE,
              fopen("resources/RobotoMono-Regular.ttf", "rb"));

        stbtt_BakeFontBitmap(dr_ttf_buffer, 0, 32.0, bitmap, ATLAS_WIDTH,
                             ATLAS_WIDTH, 32, 96, cdata);

        stbi_write_png("resources/RobotoMono.png", ATLAS_WIDTH, ATLAS_WIDTH, 1,
                       bitmap, 0);

        u8 *atlas_tex = arena_alloc(&a, ATLAS_WIDTH * ATLAS_WIDTH * 4);
        for (u32 i = 0; i < ATLAS_WIDTH * ATLAS_WIDTH; i++) {
                u8 v = bitmap[i];
                atlas_tex[i * 4] = v;
                atlas_tex[i * 4 + 1] = v;
                atlas_tex[i * 4 + 2] = v;
                if (v < 5) {
                        atlas_tex[i * 4 + 3] = 0;
                } else {
                        atlas_tex[i * 4 + 3] = v;
                }
        }

        r_create_texture(atlas_tex, ATLAS_WIDTH, ATLAS_WIDTH, 4, &dr_font);
}

#define dr_TABLE_WIDTH 16
#define dr_TABLE_WIDTHf 16.0f

static stbtt_aligned_quad dr_find_glyph_in_table(u8 c, f32 x, f32 y) {
        // u8 rank = c - ' ';
        // f32 row = (f32)(rank / dr_TABLE_WIDTH);
        // f32 col = (f32)(rank % dr_TABLE_WIDTH);
        //
        // Vec2f32 p0 = v2f32(col / dr_TABLE_WIDTHf, row / dr_TABLE_WIDTHf);
        // Vec2f32 p1 = v2f32((col + 1.0f) / dr_TABLE_WIDTHf,
        //                    (row + 1.0f) / dr_TABLE_WIDTHf);
        // Rng2f32 res = { { p0, p1 } };
        // return res;

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(cdata, ATLAS_WIDTH, ATLAS_WIDTH, c - 32, &x, &y, &q,
                           1); // 1=opengl & d3d10+,0=d3d9
        Vec2f32 p0 = { .x = q.s0, .y = q.t0 };
        Vec2f32 p1 = { .x = q.s1, .y = q.t1 };
        Rng2f32 res = { .p0 = p0, .p1 = p1 };
        return q;
}

static void dr_rect(Rng2f32 rng, Vec4f32 color, f32 cornerRadius,
                    f32 edgeSoftness) {
        RRectInstanceData rect = {
                .pos = rng,
                .src = r2f32p(0, 0, 1, 1),
                .colors = { color, color, color, color },
                .cornerRadius = cornerRadius,
                .edgeSoftness = edgeSoftness,
        };
        r_add_rect_to_batch(&rect, 0);
}

static void dr_img(Rng2f32 rng, Vec4f32 color, RTexture *tex, Rng2f32 srcRng,
                   f32 cornerRadius, f32 edgeSoftness) {
        RRectInstanceData rect = {
                .pos = rng,
                .src = srcRng,
                .colors = { color, color, color, color },
                .cornerRadius = cornerRadius,
                .edgeSoftness = edgeSoftness,
        };
        r_add_rect_to_batch(&rect, tex);
}

static void dr_text(FFont *f, f32 size, String8 text, Rng2f32 rng,
                    Vec4f32 color) {
        if (dr_font.height == 0) {
                dr_init_font();
        }
        if (text.length == 0) {
                return;
        }

        String8 s = string8_lit(" ");
        u32 cpl = (rng.x1 - rng.x0) / size;
        u32 remaining = cpl;
        Rng2f32 cRng
            = r2f32(rng.min, v2f32(rng.min.x + size, rng.min.y + size));
        for (b32 done = 0; done == 0;) {
                // grab next word
                u64 space = string8_find_substr(text, s);
                if (space == UINT64_MAX) {
                        space = text.length;
                        done = 1;
                }
                // if it fits lay out left to right
                if (remaining < space) {
                        if (cRng.max.y + size < rng.y1) {
                                cRng.min.x = rng.min.x;
                                cRng.max.x = rng.min.x + size;
                                cRng = shift_2f32(cRng, v2f32(0.0f, size));
                                remaining = cpl;
                                continue;
                        } else {
                                return;
                        }
                }
                // loop and draw word
                for (u32 i = 0; i < space; i++) {
                        stbtt_aligned_quad q = dr_find_glyph_in_table(
                            text.data[i], cRng.x0, cRng.y0 + size);
                        Vec2f32 p0 = { .x = q.s0, .y = q.t0 };
                        Vec2f32 p1 = { .x = q.s1, .y = q.t1 };
                        Rng2f32 src = { .p0 = p0, .p1 = p1 };
                        Vec2f32 p2 = { .x = q.x0, .y = q.y0 };
                        Vec2f32 p3 = { .x = q.x1, .y = q.y1 };
                        Rng2f32 pos = { .p0 = p2, .p1 = p3 };

                        dr_img(pos, color, &dr_font, src, 0, 0);

                        f32 shift = pos.x1 - cRng.x0;
                        cRng = shift_2f32(cRng, v2f32(shift, 0.0f));
                }

                // remove excess whitespace
                remaining -= space;
                text = string8_skip(text, space);
                for (u32 i = 1; text.data[0] == ' '; i++) {
                        // duplicate code
                        if (remaining < i) {
                                if (cRng.max.y + size < rng.y1) {
                                        cRng.min.x = rng.min.x;
                                        cRng.max.x = rng.min.x + size;
                                        cRng = shift_2f32(cRng,
                                                          v2f32(0.0f, size));
                                        remaining = cpl;
                                } else {
                                        return;
                                }
                        } else {
                                remaining--;
                                cRng = shift_2f32(cRng, v2f32(size, 0.0f));
                        }
                        text = string8_skip(text, 1);
                }
        }
}
