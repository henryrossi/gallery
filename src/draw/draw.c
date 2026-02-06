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

static void dr_img(Rng2f32 rng, Vec4f32 color, String8 filename,
                   Rng2f32 srcRng) {}

static void dr_text(FFont *f, f32 size, String8 text, Vec2f32 pos0,
                    Vec4f32 color) {}
