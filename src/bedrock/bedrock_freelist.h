#ifndef BEDROCK_FREELIST_H
#define BEDROCK_FREELIST_H

#include "bedrock/bedrock_arena.h"
#include "bedrock/bedrock_core.h"

typedef struct FreelistNode FreelistNode;
typedef struct {
        void *mem;
        u64 size;
        u64 used;
        FreelistNode *rbtree;
} Freelist;

static Freelist make_freelist(u64 sizeHint);
static void *freelist_alloc(Freelist *f, u64 size);
static void freelist_free(Freelist *f, void *ptr);

#endif // BEDROCK_FREELIST_H
