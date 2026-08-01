#ifndef BEDROCK_ARENA_H
#define BEDROCK_ARENA_H

#define ARENA_HEADER_SIZE 56

#include "bedrock/bedrock_core.h"

// TODO: hr: allow for disabling of chaining arena blocks

typedef struct ArenaParams ArenaParams;
struct ArenaParams {
        u64 blockSize;
        char *createdFile;
        int createdLine;
};

typedef struct Arena Arena;
struct Arena {
        Arena *prev;
        Arena *top;
        u64 base;
        u64 pos;
        u64 size;

        char *createdFile;
        int createdLine;
};

compile_assert(sizeof(Arena) <= ARENA_HEADER_SIZE);

static Arena *make_arena_(ArenaParams *params);
#define make_arena(size)                                                       \
        make_arena_(&(ArenaParams){                                            \
                .blockSize = size,                                             \
                .createdFile = __FILE__,                                       \
                .createdLine = __LINE__,                                       \
        })

static void *arena_alloc(Arena *a, u64 size);
static u64 arena_pos(Arena *a);
static b32 arena_pop_at(Arena *a, u64 pos);

static b32 arena_contains_mem(Arena *a, void *ptr);

static void arena_reset(Arena *a);
static void arena_pop(Arena *a, u64 amt);

static void destroy_arena(Arena *a);

#endif // BEDROCK_ARENA_H
