#ifndef GLYPH_COORD_TRANSFORM_H
#define GLYPH_COORD_TRANSFORM_H

#include "glyph.h"

static void getPosCoordTransform(Vec3 *scale, Vec3 *translation,
                                 VkExtent2D screen, bool orthographic,
                                Mat4 *mvp);
 

#endif // GLYPH_COORD_TRANSFORM_H
