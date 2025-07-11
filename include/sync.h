#ifndef GLYPH_SYNC_H
#define GLYPH_SYNC_H

#include "glyph.h"

static int create_sync_objects(glyph_state *state);

static void cleanUnsafeSemaphore(VkQueue queue, VkSemaphore *semaphore);

#endif // GLYPH_SYNC_H
