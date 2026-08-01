#ifndef BEDROCK_FREELIST_H
#define BEDROCK_FREELIST_H

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

static b32 freelist_contains_mem(Freelist *f, void *ptr);

static void destroy_freelist(Freelist *f);

#endif // BEDROCK_FREELIST_H
