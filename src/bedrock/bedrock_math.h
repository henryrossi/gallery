#ifndef BEDROCK_MATH_H
#define BEDROCK_MATH_H

#include "bedrock/bedrock_core.h"

// TODO: hr: fill out remaining vector, matrix, and range types, as well as
//           their functions and scalar functions

// 2 Vectors
typedef union Vec2u32 Vec2u32;
union Vec2u32 {
        struct {
                u32 x;
                u32 y;
        };
        u32 v[2];
};

typedef union Vec2s32 Vec2s32;
union Vec2s32 {
        struct {
                s32 x;
                s32 y;
        };
        s32 v[2];
};

typedef struct Vec2s32Node Vec2s32Node;
struct Vec2s32Node {
        Vec2s32Node *n;
        Vec2s32 v;
};

typedef union Vec2f32 Vec2f32;
union Vec2f32 {
        struct {
                f32 x;
                f32 y;
        };
        f32 v[2];
};

typedef struct Vec2f32Node Vec2f32Node;
struct Vec2f32Node {
        Vec2f32Node *n;
        Vec2f32 v;
};

// 3 Vectors
typedef union Vec3f32 Vec3f32;
union Vec3f32 {
        struct {
                f32 x;
                f32 y;
                f32 z;
        };
        f32 v[3];
};

// 4 Vectors
typedef union Vec4u8 Vec4u8;
union Vec4u8 {
        struct {
                u8 x;
                u8 y;
                u8 z;
                u8 w;
        };
        u8 v[4];
};

typedef union Vec4f32 Vec4f32;
union Vec4f32 {
        struct {
                f32 x;
                f32 y;
                f32 z;
                f32 w;
        };
        struct {
                Vec3f32 xyz;
                f32 _w;
        };
        f32 v[4];
};

typedef struct {
        f32 m[4][4];
} Mat4;

// 2 Ranges (Rectangles)
typedef union Rng2u32 Rng2u32;
union Rng2u32 {
        struct {
                Vec2u32 min;
                Vec2u32 max;
        };
        struct {
                Vec2u32 p0;
                Vec2u32 p1;
        };
        struct {
                u32 x0;
                u32 y0;
                u32 x1;
                u32 y1;
        };
        Vec2u32 v[2];
};

typedef union Rng2f32 Rng2f32;
union Rng2f32 {
        struct {
                Vec2f32 min;
                Vec2f32 max;
        };
        struct {
                Vec2f32 p0;
                Vec2f32 p1;
        };
        struct {
                f32 x0;
                f32 y0;
                f32 x1;
                f32 y1;
        };
        Vec2f32 v[2];
};

// hr: Vector Ops
#define v2u32(x, y) vec2u32(x, y)
static Vec2u32 vec2u32(u32 x, u32 y);

#define v2s32(x, y) vec2s32(x, y)
static Vec2s32 vec2s32(s32 x, s32 y);
static Vec2s32 add_v2s32(Vec2s32 a, Vec2s32 b);
static b32 equal_v2s32(Vec2s32 a, Vec2s32 b);

#define v2f32(x, y) vec2f32(x, y)
static Vec2f32 vec2f32(f32 x, f32 y);
static Vec2f32 add_v2f32(Vec2f32 a, Vec2f32 b);
static Vec2f32 sub_v2f32(Vec2f32 a, Vec2f32 b);
static Vec2f32 mul_v2f32(Vec2f32 a, Vec2f32 b);
static b32 equal_v2f32(Vec2f32 a, Vec2f32 b);

#define v3f32(x, y, z) vec3f32(x, y, z)
static Vec3f32 vec3f32(f32 x, f32 y, f32 z);

#define v4u8(x, y, z, w) vec4u8(x, y, z, w)
static Vec4u8 vec4u8(u8 x, u8 y, u8 z, u8 w);
static b32 equal_v4u8(Vec4u8 a, Vec4u8 b);

#define v4f32(x, y, z, w) vec4f32(x, y, z, w)
static Vec4f32 vec4f32(f32 x, f32 y, f32 z, f32 w);
static Vec4f32 add_v4f32(Vec4f32 a, Vec4f32 b);

// hr: Range Ops
#define r2u32(a, b) rng2u32(a, b)
#define r2u32p(x0, y0, x1, y1) rng2u32(v2u32(x0, y0), v2u32(x1, y1))
static Rng2u32 rng2u32(Vec2u32 a, Vec2u32 b);

#define r2f32(a, b) rng2f32(a, b)
#define r2f32p(x0, y0, x1, y1) rng2f32(v2f32(x0, y0), v2f32(x1, y1))
static Rng2f32 rng2f32(Vec2f32 a, Vec2f32 b);
static Rng2f32 shift_r2f32(Rng2f32 r, Vec2f32 a);
static b32 contains_r2f32(Rng2f32 r, Vec2f32 a);

#endif // BEDROCK_MATH_H
