static Vec2f32 vec2f32(f32 x, f32 y) {
        Vec2f32 res = { { x, y } };
        return res;
}

static Vec3f32 vec3f32(f32 x, f32 y, f32 z) {
        Vec3f32 res = { { x, y, z } };
        return res;
}

static Vec4f32 vec4f32(f32 x, f32 y, f32 z, f32 w) {
        Vec4f32 res = { { x, y, z, w } };
        return res;
}

static Vec2f32 add_2f32(Vec2f32 a, Vec2f32 b) {
        Vec2f32 res = { { a.x + b.x, a.y + b.y } };
        return res;
}

static Rng2f32 rng2f32(Vec2f32 a, Vec2f32 b) {
        Rng2f32 res = { .min = a, .max = b };
        return res;
}

static Rng2f32 shift_2f32(Rng2f32 r, Vec2f32 x) {
        Rng2f32 res = { .min = add_2f32(r.min, x), .max = add_2f32(r.max, x) };
        return res;
}
