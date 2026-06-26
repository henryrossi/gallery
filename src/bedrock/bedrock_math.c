#include "bedrock/bedrock_math.h"

static b32 nequal_f32(f32 a, f32 b, f32 epsilon) {
        f32 d = fabs(a - b);
        if (d == 0) {
                return 1;
        }
        return d < epsilon * (fabs(a) + fabs(b));
}

static Vec2u32 vec2u32(u32 x, u32 y) {
        Vec2u32 res = { .x = x, .y = y };
        return res;
}

static Vec2s32 vec2s32(s32 x, s32 y) {
        Vec2s32 res = { .x = x, .y = y };
        return res;
}

static Vec2s32 add_v2s32(Vec2s32 a, Vec2s32 b) {
        Vec2s32 res = { .x = a.x + b.x, .y = a.y + b.y };
        return res;
}

static b32 equal_v2s32(Vec2s32 a, Vec2s32 b) {
        return a.x == b.x && a.y == b.y;
}

static Vec2f32 vec2f32(f32 x, f32 y) {
        Vec2f32 res = { .x = x, .y = y };
        return res;
}

static Vec2f32 add_v2f32(Vec2f32 a, Vec2f32 b) {
        Vec2f32 res = { .x = a.x + b.x, .y = a.y + b.y };
        return res;
}

static Vec2f32 sub_v2f32(Vec2f32 a, Vec2f32 b) {
        Vec2f32 res = { .x = a.x - b.x, .y = a.y - b.y };
        return res;
}

static Vec2f32 mul_v2f32(Vec2f32 a, Vec2f32 b) {
        Vec2f32 res = { .x = a.x * b.x, .y = a.y * b.y };
        return res;
}

static b32 equal_v2f32(Vec2f32 a, Vec2f32 b) {
        f32 e = 0.00001;
        b32 res = nequal_f32(a.x, b.x, e) && nequal_f32(a.y, b.y, e);
        return res;
}

static Vec3f32 vec3f32(f32 x, f32 y, f32 z) {
        Vec3f32 res = { .x = x, .y = y, .z = z };
        return res;
}

static Vec4u8 vec4u8(u8 x, u8 y, u8 z, u8 w) {
        Vec4u8 res = { .x = x, .y = y, .z = z, .w = w };
        return res;
}

static b32 equal_v4u8(Vec4u8 a, Vec4u8 b) {
        return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

static Vec4f32 vec4f32(f32 x, f32 y, f32 z, f32 w) {
        Vec4f32 res = { .x = x, .y = y, .z = z, .w = w };
        return res;
}

static Vec4f32 add_v4f32(Vec4f32 a, Vec4f32 b) {
        Vec4f32 res = {
                .x = a.x + b.x,
                .y = a.y + b.y,
                .z = a.z + b.z,
                .w = a.w + b.w,
        };
        return res;
}

static b32 equal_v4f32(Vec4f32 a, Vec4f32 b) {
        return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

static Rng2u32 rng2u32(Vec2u32 a, Vec2u32 b) {
        Rng2u32 res = { .min = a, .max = b };
        return res;
}

static Rng2f32 rng2f32(Vec2f32 a, Vec2f32 b) {
        Rng2f32 res = { .min = a, .max = b };
        return res;
}

static Rng2f32 shift_r2f32(Rng2f32 r, Vec2f32 a) {
        Rng2f32 res
            = { .min = add_v2f32(r.min, a), .max = add_v2f32(r.max, a) };
        return res;
}

static b32 contains_r2f32(Rng2f32 r, Vec2f32 a) {
        b32 res = (a.x > r.min.x && a.x < r.max.x)
                  && (a.y > r.min.y && a.y < r.max.y);
        return res;
}
