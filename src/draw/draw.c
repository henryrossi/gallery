static RRectInstance *dr_rect(Vec2 pos0, Vec2 pos1, Vec4 color) {
        RRectInstanceData rect = {
                .pos0 = pos0,
                .pos1 = pos1,
                .src0 = vec2(0, 0),
                .src1 = vec2(1, 1),
                .colors = { color, color, color, color },
        };

        r_add_rect_to_batch(&rect);
        return rect;
}

static RRectInstance *dr_img() {
        RRectInstance rect = { 0 };
        return rect;
}

static RRectInstance *dr_text(Font *f, f32 size, String8 text, Vec2 pos0,
                              Vec4 color) {
        RRectInstance rect = { 0 };
        return rect;
}
