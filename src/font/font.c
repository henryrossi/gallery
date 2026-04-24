#include "font/font.h"

#ifndef STB_TRUETYPE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "thirdparty/stb/stb_truetype.h"
#endif

#define F_TTF_BUFFER_SIZE 1 << 18
#define F_ATLAS_WIDTH 512

#define F_SPACE_SIZE 0.5

Arena *a;

static FFont *f_init_font(String8 filename) {
        a = make_arena(1 << 26);

        unsigned char *ttf_buffer = arena_alloc(a, F_TTF_BUFFER_SIZE);
        unsigned char *bitmap = arena_alloc(a, F_ATLAS_WIDTH * F_ATLAS_WIDTH);

        String8 cstr = string8_allocate(a, filename.length + 1);
        memcpy(cstr.data, filename.data, filename.length);
        string8_set(cstr, filename.length, '\0');

        // TODO: hr: implement os reads and writes as well as file size querying
        //           and filename handling
        FILE *fp = fopen((const char *)cstr.data, "rb");
        if (!fp)
                return 0;
        fread(ttf_buffer, 1, F_TTF_BUFFER_SIZE, fp);

        FFont *font = arena_alloc(a, sizeof(*font));

        font->bakedSize = 32.0f;
        stbtt_BakeFontBitmap(ttf_buffer, 0, font->bakedSize, bitmap,
                             F_ATLAS_WIDTH, F_ATLAS_WIDTH, 32, 96, font->cdata);

        stbtt_fontinfo fontinfo;
        stbtt_InitFont(&fontinfo, ttf_buffer, 0);
        s32 ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&fontinfo, &ascent, &descent, &lineGap);

        f32 scale = stbtt_ScaleForPixelHeight(&fontinfo, font->bakedSize);
        font->baseline = ascent * scale;

        u8 *atlas_tex = arena_alloc(a, F_ATLAS_WIDTH * F_ATLAS_WIDTH * 4);
        for (u32 i = 0; i < F_ATLAS_WIDTH * F_ATLAS_WIDTH; i++) {
                atlas_tex[i * 4] = 255;
                atlas_tex[i * 4 + 1] = 255;
                atlas_tex[i * 4 + 2] = 255;
                atlas_tex[i * 4 + 3] = bitmap[i];
        }

        r_create_texture(atlas_tex, F_ATLAS_WIDTH, F_ATLAS_WIDTH, 4,
                         &font->tex);
        font->name = filename;
        return font;
}

static Vec2f32 f_content_scale(void) {
        Vec2f32 res = { .x = 1, .y = 1 };
        glfwGetWindowContentScale(r_state.window, &res.x, &res.y);
        return res;
}

static void f_destroy_font(FFont *font) {
        r_destroy_texture(&font->tex);
}

static f32 f_text_length(FFont *font, f32 size, String8 str) {
        f32 x = 0.0f;
        f32 y = 0.0f;
        float cs = f_content_scale().x;
        f32 scale = size * cs / font->bakedSize;
        stbtt_aligned_quad q = { 0 };

        for (u32 i = 0; i < str.length; i++) {
                u8 c = string8_at(str, i);
                if (c == ' ') {
                        x += f_space_width(font, font->bakedSize) / cs;
                } else {
                        stbtt_GetBakedQuad(font->cdata, F_ATLAS_WIDTH,
                                           F_ATLAS_WIDTH, c - 32, &x, &y, &q,
                                           1);
                }
        }

        return q.x1 * scale;
}

static void f_char_draw_info(FFont *font, f32 size, u8 c, Rng2f32 *pos,
                             Rng2f32 *src) {
        f32 x = 0.0f;
        f32 y = 0.0f;
        Vec2f32 scale = f_content_scale();
        f32 sizeR = size / font->bakedSize;
        f32 shift = font->bakedSize - font->baseline;
        stbtt_aligned_quad q = { 0 };

        stbtt_GetBakedQuad(font->cdata, F_ATLAS_WIDTH, F_ATLAS_WIDTH, c - 32,
                           &x, &y, &q, 1);
        Vec2f32 p0 = { .x = q.s0, .y = q.t0 };
        Vec2f32 p1 = { .x = q.s1, .y = q.t1 };
        src->min = p0;
        src->max = p1;

        Vec2f32 p2 = { .x = q.x0 * sizeR, .y = (q.y0 - shift) * sizeR };
        Vec2f32 p3 = { .x = q.x1 * sizeR, .y = (q.y1 - shift) * sizeR };
        pos->min = mul_v2f32(p2, scale);
        pos->max = mul_v2f32(p3, scale);
}

static f32 f_space_width(FFont *font, f32 size) {
        Vec2f32 scale = f_content_scale();
        return F_SPACE_SIZE * size * scale.x;
}
