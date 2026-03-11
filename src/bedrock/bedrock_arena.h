#ifndef BEDROCK_ARENA_H
#define BEDROCK_ARENA_H

#define ARENA_HEADER_SIZE 56

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

static_assert(sizeof(Arena) <= ARENA_HEADER_SIZE);

static Arena *make_arena_(ArenaParams *params);
#define make_arena(size)                                                       \
        make_arena_(&(ArenaParams){                                            \
                .blockSize = size,                                             \
                .createdFile = __FILE__,                                       \
                .createdLine = __LINE__,                                       \
        })

static void *arena_alloc(Arena *a, u64 size);
static u64 arena_pos(Arena *a);
static void arena_pop_at(Arena *a, u64 pos);

static void arena_reset(Arena *a);
static void arena_pop(Arena *a, u64 amt);

#endif // BEDROCK_ARENA_H
