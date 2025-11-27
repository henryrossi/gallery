static void multMat4xMat4(Mat4 *a, Mat4 *b, Mat4 *product) {

        for (int n = 0; n < 4; n++) {
                for (int p = 0; p < 4; p++) {
                        product->m[n][p] = 0.0;
                        for (int m = 0; m < 4; m++) {
                                product->m[n][p] += a->m[n][m] * b->m[m][p];
                        }
                }
        }
}

static void multMat4xVec4(Mat4 *a, Vec4 *b, Vec4 *product) {
        product->x = a->m[0][0] * b->x + a->m[0][1] * b->y + a->m[0][2] * b->z
                     + a->m[0][3] * b->w;

        product->y = a->m[1][0] * b->x + a->m[1][1] * b->y + a->m[1][2] * b->z
                     + a->m[1][3] * b->w;

        product->z = a->m[2][0] * b->x + a->m[2][1] * b->y + a->m[2][2] * b->z
                     + a->m[2][3] * b->w;

        product->w = a->m[3][0] * b->x + a->m[3][1] * b->y + a->m[3][2] * b->z
                     + a->m[3][3] * b->w;
        if (0) {
                printMat4(a);
                printVec4(b);
        }
}

static void transposeMat4(Mat4 *mat) {
        for (int i = 0; i < 4; i++) {
                for (int j = i + 1; j < 4; j++) {
                        float tmp = mat->m[i][j];
                        mat->m[i][j] = mat->m[j][i];
                        mat->m[j][i] = tmp;
                }
        }
}

static void printMat4(Mat4 *mat) {
        for (int i = 0; i < 4; i++) {
                printf("[ %.4f, %.4f, %.4f, %.4f ]\n", mat->m[i][0],
                       mat->m[i][1], mat->m[i][2], mat->m[i][3]);
        }
        printf("\n");
}

static void printVec4(Vec4 *vec) {
        printf("[ %.4f, %.4f, %.4f, %.4f ]\n", vec->x, vec->y, vec->z, vec->w);
}
