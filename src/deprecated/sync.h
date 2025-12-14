#ifndef GLYPH_SYNC_H
#define GLYPH_SYNC_H

#include "glyph.h"

static int create_sync_objects(GlyphEngine *engine);

static void cleanUnsafeSemaphore(VkQueue queue, VkSemaphore *semaphore);

#endif // GLYPH_SYNC_H
