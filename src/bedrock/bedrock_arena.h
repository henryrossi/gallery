#ifndef BEDROCK_ARENA_H
#define BEDROCK_ARENA_H


typedef struct {
        void *pool;
        u64 size;
        u32 offset;
} Arena;

Arena make_arena(u64 size);
void *arena_alloc(Arena *a, u64 size);
void arena_reset(Arena *a);
void arena_reset_at(Arena *a, void *p);


#endif // BEDROCK_ARENA_H


