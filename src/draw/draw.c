#include "draw/draw.h"

static void dr_rect(Rng2f32 rng, Vec4f32 *colors, f32 cornerRadius,
                    f32 edgeSoftness) {
        RRectInstanceData rect = {
                .pos = rng,
                .src = r2f32p(0, 0, 1, 1),
                .colors = { colors[0], colors[1], colors[2], colors[3] },
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

        if (!f || text.length == 0) {
                return;
        }

        String8 s = string8_lit(" ");
        f32 lineLen = rng.x1 - rng.x0;
        f32 remaining = lineLen;
        Vec2f32 scale = f_content_scale();
        Vec2f32 scaledSize = { .x = scale.x * size, .y = scale.y * size };
        f32 spaceWidth = f_space_width(f, size);
        Rng2f32 cRng = r2f32(rng.min, add_v2f32(rng.min, scaledSize));

        text = string8_skip_whitespace(text);
        for (b32 done = 0; done == 0;) {
                u64 space = string8_find_substr(text, s);
                if (space == UINT64_MAX) {
                        space = text.length;
                        done = 1;
                }

                String8 word = string8_prune(text, space);
                f32 len = f_text_length(f, size, word);
                if (remaining < len) {
                        goto next_line;
                }

                for (u32 i = 0; i < space; i++) {
                        Rng2f32 pos = { 0 };
                        Rng2f32 src = { 0 };
                        f_char_draw_info(f, size, string8_at(word, i), &pos,
                                         &src);

                        f32 shift = pos.x1;
                        pos = shift_r2f32(
                            pos, v2f32(cRng.x0, cRng.y0 + scaledSize.y));

                        dr_img(pos, color, &f->tex, src, 0, 0);

                        cRng = shift_r2f32(cRng, v2f32(shift, 0.0f));
                }
                remaining -= len;
                text = string8_skip(text, space);

                while (string8_at(text, 0) == ' ') {
                        if (remaining < spaceWidth) {
                                goto next_line;
                        } else {
                                remaining -= spaceWidth;
                                cRng = shift_r2f32(cRng,
                                                   v2f32(spaceWidth, 0.0f));
                        }
                        text = string8_skip(text, 1);
                }
                continue;

        next_line:
                if (cRng.max.y + scaledSize.y < rng.y1) {
                        cRng.min.x = rng.min.x;
                        cRng.max.x = rng.min.x + scaledSize.x;
                        cRng = shift_r2f32(cRng, v2f32(0.0f, scaledSize.y));
                        remaining = lineLen;
                        text = string8_skip_whitespace(text);
                } else {
                        done = 1;
                }
        }
}
