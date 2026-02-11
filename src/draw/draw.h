#ifndef DRAW_H
#define DRAW_H

static void dr_rect(Rng2f32 rng, Vec4f32 color, f32 cornerRadius,
                    f32 edgeSoftness);

static void dr_img(Rng2f32 rng, Vec4f32 color, RTexture *tex, Rng2f32 srcRngf32,
                   f32 cornerRadius, f32 edgeSoftness);

static void dr_text(FFont *f, f32 size, String8 text, Rng2f32 rng,
                    Vec4f32 color);

#endif // DRAW_H
