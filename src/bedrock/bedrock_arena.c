// Allocates a memory chuck for the Arena. Returns 1 on success, 0 on failure.
Arena make_arena(u64 size) {
        Arena a = { 0 };
        u64 pageAligned = ((size / OS_PAGESIZE) + 1) * OS_PAGESIZE;
        a.pool = os_reserve(pageAligned);
        a.size = pageAligned;
        return a;
}

void *arena_alloc(Arena *a, u64 size) {
        if (a->offset + size > a->size) {
                printf("Allocation size (%llu) does not fit in arena (arena "
                       "size %llu, already allocated %llu). Returning NULL");
                return NULL;
        }

        void *ret = a->pool + a->offset;
        a->offset += size;
        return ret;
}

void arena_reset(Arena *a) { a->offset = 0; }

void arena_reset_at(Arena *a, void *p) {
        u64 offset = a->pool - p;
        if (offset > 0 && offset < a->offset) {
                a->offset = offset;
        }
}
