#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

typedef struct {
        void *pool;
        uint32_t offset;
} arena;

int constructArena(arena *a);
void *arenaAllocate(arena *a, uint32_t size);
void arenaReset(arena *a);
void arenaResetAt(arena *a, void *p);


#endif // ARENA_H

#ifdef ARENA_IMPL

#define ARENA_BASE_SIZE 0x4000

// Allocates a memory chuck for the arena. Returns 1 on success, 0 on failure.
int constructArena(arena *a) {
        assert(!a->pool && "Error - Arena is initalized and already has a pool of memory"); 
        a->offset = 0;
        a->pool = malloc(ARENA_BASE_SIZE);
        return a->pool ? 1 : 0;
}

void *arenaAllocate(arena * a, uint32_t size) {
        assert(a && "Error - Arena pointer is NULL"); 
        if (a->offset + size > ARENA_BASE_SIZE) {
                return NULL;
        }

        void *ret = a->pool + a->offset;
        a->offset += size;
        return ret;
}

void arenaReset(arena *a) {
        assert(a && "Error - Arena pointer is NULL"); 
        a->offset = 0;
}

void arenaResetAt(arena *a, void *p) {
        assert(a && "Error - Arena pointer is NULL"); 
        ptrdiff_t offset = a->pool - p;
        if (offset > 0 && offset < a->offset) {
                a->offset = offset;
        }
}

#endif // ARENA_IMPL
