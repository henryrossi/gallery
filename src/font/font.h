#ifndef FONT_H
#define FONT_H

#include "thirdparty/stb/stb_truetype.h"

typedef struct {
        String8 name;
        RTexture tex;
        stbtt_bakedchar cdata[96];
        f32 bakedSize;
        f32 baseline;
} FFont;

static FFont *f_init_font(String8 filename);

static f32 f_text_length(FFont *font, f32 size, String8 str);

static void f_char_draw_info(FFont *font, f32 size, u8 c, Rng2f32 *pos,
                             Rng2f32 *src);

#endif // FONT_H
