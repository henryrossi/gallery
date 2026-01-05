static void dr_rect(Vec2 pos0, Vec2 pos1, Vec4 color) {
        RRectInstanceData rect = {
                .pos0 = pos0,
                .pos1 = pos1,
                .src0 = vec2(0, 0),
                .src1 = vec2(1, 1),
                .texID = 0,
                .colors = { color, color, color, color },
        };

        r_add_rect_to_batch(&rect);
}

static void dr_img() {}

static void dr_text() {}
