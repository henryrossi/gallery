#ifndef STB_TRUETYPE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "thirdparty/stb/stb_truetype.h"
#endif

#define TTF_BUFFER_SIZE 1 << 18
#define ATLAS_WIDTH 512

Arena a;

static FFont *f_init_font(String8 filename) {
        a = make_arena(1 << 26);

        unsigned char *ttf_buffer = arena_alloc(&a, TTF_BUFFER_SIZE);
        unsigned char *bitmap = arena_alloc(&a, ATLAS_WIDTH * ATLAS_WIDTH);

        String8 cstr = string8_allocate(&a, filename.length + 1);
        memcpy(cstr.data, filename.data, filename.length);
        cstr.data[filename.length] = '\0';

        // TODO: hr: implement os reads and writes as well as file size querying
        //           and filename handling
        FILE *fp = fopen((const char *)cstr.data, "rb");
        if (!fp)
                return 0;
        fread(ttf_buffer, 1, TTF_BUFFER_SIZE, fp);

        FFont *font = arena_alloc(&a, sizeof(*font));

        font->bakedSize = 32.0f;
        stbtt_BakeFontBitmap(ttf_buffer, 0, font->bakedSize, bitmap,
                             ATLAS_WIDTH, ATLAS_WIDTH, 32, 96, font->cdata);

        u8 *atlas_tex = arena_alloc(&a, ATLAS_WIDTH * ATLAS_WIDTH * 4);
        for (u32 i = 0; i < ATLAS_WIDTH * ATLAS_WIDTH; i++) {
                atlas_tex[i * 4] = 255;
                atlas_tex[i * 4 + 1] = 255;
                atlas_tex[i * 4 + 2] = 255;
                atlas_tex[i * 4 + 3] = bitmap[i];
        }

        r_create_texture(atlas_tex, ATLAS_WIDTH, ATLAS_WIDTH, 4, &font->tex);
        font->name = filename;
        return font;
}

static f32 f_text_length(FFont *font, f32 size, String8 str) {
        f32 res = 0.0f;
        f32 x = 0.0f;
        f32 y = 0.0f;
        f32 sizeR = size / font->bakedSize;
        stbtt_aligned_quad q = { 0 };

        for (u32 i = 0; i < str.length; i++) {
                u8 c = string8_at(str, i);
                if (c == ' ') {
                        res += size;
                } else {
                        stbtt_GetBakedQuad(font->cdata, ATLAS_WIDTH,
                                           ATLAS_WIDTH, c - 32, &x, &y, &q, 1);
                        res += (q.x1 - q.x0) * sizeR;
                }
        }

        return res;
}

static void f_char_draw_info(FFont *font, f32 size, u8 c, Rng2f32 *pos,
                             Rng2f32 *src) {
        f32 x = 0.0f;
        f32 y = 0.0f;
        f32 sizeR = size / font->bakedSize;
        stbtt_aligned_quad q = { 0 };

        stbtt_GetBakedQuad(font->cdata, ATLAS_WIDTH, ATLAS_WIDTH, c - 32, &x,
                           &y, &q, 1);
        Vec2f32 p0 = { .x = q.s0, .y = q.t0 };
        Vec2f32 p1 = { .x = q.s1, .y = q.t1 };
        src->min = p0;
        src->max = p1;

        Vec2f32 p2 = { .x = q.x0 * sizeR, .y = q.y0 * sizeR };
        Vec2f32 p3 = { .x = q.x1 * sizeR, .y = q.y1 * sizeR };
        pos->min = p2;
        pos->max = p3;
}
