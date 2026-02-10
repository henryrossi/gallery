#ifndef BEDROCK_MATH_H
#define BEDROCK_MATH_H

// TODO: hr: fill out remaining vector, matrix, and range types, as well as
//           their functions and scalar functions

// 2 Vectors
typedef union Vec2f32 Vec2f32;
union Vec2f32 {
        struct {
                f32 x;
                f32 y;
        };
        f32 v[2];
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

#define v2f32(x, y) vec2f32(x, y)
static Vec2f32 vec2f32(f32 x, f32 y);
static Vec2f32 add_2f32(Vec2f32 a, Vec2f32 b);

#define v3f32(x, y, z) vec3f32(x, y, z)
static Vec3f32 vec3f32(f32 x, f32 y, f32 z);

#define v4f32(x, y, z, w) vec4f32(x, y, z, w)
static Vec4f32 vec4f32(f32 x, f32 y, f32 z, f32 w);

// hr: Range Ops
#define r2f32(a, b) rng2f32(a, b)
#define r2f32p(x0, y0, x1, y1) rng2f32(v2f32(x0, y0), v2f32(x1, y1))
static Rng2f32 rng2f32(Vec2f32 a, Vec2f32 b);
static Rng2f32 shift_2f32(Rng2f32 r, Vec2f32 x);

#endif // BEDROCK_MATH_H
