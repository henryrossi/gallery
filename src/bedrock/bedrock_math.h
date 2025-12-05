#ifndef BEDROCK_MATRIX_H
#define BEDROCK_MATRIX_H

typedef struct {
	f32 x;
	f32 y;
} Vec2;

typedef struct {
	f32 x;
	f32 y;
	f32 z;
} Vec3;

typedef struct {
	f32 x;
	f32 y;
	f32 z;
	f32 w;
} Vec4;

typedef struct {
	f32 m[4][4];
} Mat4;

typedef struct {
	Vec2 pos;
	Vec2 extent;
} Rect2D;

static Vec2 vec2(f32 x, f32 y);
static Vec3 vec3(f32 x, f32 y, f32 z);
static Vec4 vec4(f32 x, f32 y, f32 z, f32 w);

static void multMat4xMat4(Mat4 *a, Mat4 *b, Mat4 *product);
static void multMat4xVec4(Mat4 *a, Vec4 *b, Vec4 *product);
static void transposeMat4(Mat4 *mat);
static void printMat4(Mat4 *mat);
static void printVec4(Vec4 *vec);

#endif // BEDROCK_MATRIX_H
