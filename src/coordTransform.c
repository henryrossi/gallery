#include "coordTransform.h"

// Calculates a mvp matrix for coordinate transformation
static void getPosCoordTransform(Vec3 *scale, Vec3 *translation,
                                 VkExtent2D screen, bool orthographic,
                                 Mat4 *mvp) {
        assert(translation && "Translation must not be a NULL pointer");
        Mat4 model = { {
                { scale->x, 0.0, 0.0, translation->x },
                { 0.0, scale->y, 0.0, translation->y },
                { 0.0, 0.0, scale->z, translation->z },
                { 0.0, 0.0, 0.0, 1.0 },
        } };

        Mat4 proj = { 0 };
        float screenWidth = (float)screen.width;
        float screenHeight = (float)screen.height;
        float aspect = screenWidth / screenHeight;

        if (orthographic) {
                float left = 0.0;
                float right;
                float bottom;
                float top = 0.0;
                float near = 0.0;
                float far = 1.0;

                right = screenWidth;
                bottom = screenHeight / aspect;

                proj.m[0][0] = 2.0 / (right - left);
                proj.m[1][1] = -2.0 / (top - bottom);
                proj.m[2][2] = -2.0 / (far - near);
                proj.m[3][3] = 1.0;
        } else {
                assert(false && "Prospective projection not implemented");
        }

        multMat4xMat4(&proj, &model, mvp);
        transposeMat4(mvp);
}
