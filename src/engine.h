#ifndef GLYPH_ENGINE_H
#define GLYPH_ENGINE_H

#include "glyph.h"

typedef struct {
  uint32_t index;
  uint32_t valid;
} queue_family_index_t;

typedef struct {
  queue_family_index_t graphics;
  queue_family_index_t presentation;
} queue_family_indicies_t;


static queue_family_indicies_t find_queue_families(GlyphEngine *engine,
                                                   VkPhysicalDevice device);
static int create_instance(GlyphEngine *engine);

#endif // GLYPH_ENGINE_H
