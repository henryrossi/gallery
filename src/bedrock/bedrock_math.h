#ifndef BEDROCK_MATRIX_H
#define BEDROCK_MATRIX_H

typedef struct {
	float x;
	float y;
} Vec2;

typedef struct {
	float x;
	float y;
	float z;
} Vec3;

typedef struct {
	float x;
	float y;
	float z;
	float w;
} Vec4;

typedef struct {
	float m[4][4];
} Mat4;

typedef struct {
	Vec2 pos;
	Vec2 extent;
} Rect2D;

static void multMat4xMat4(Mat4 *a, Mat4 *b, Mat4 *product);
static void multMat4xVec4(Mat4 *a, Vec4 *b, Vec4 *product);
static void transposeMat4(Mat4 *mat);
static void printMat4(Mat4 *mat);
static void printVec4(Vec4 *vec);

#endif // BEDROCK_MATRIX_H
