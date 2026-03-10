static u64 arena_default_block_size = mb(8);

static Arena *make_arena_(ArenaParams *p) {
        u64 blockSize = p->blockSize ? p->blockSize : arena_default_block_size;
        blockSize += ARENA_HEADER_SIZE;
        blockSize = align_pow2(blockSize, OS_PAGESIZE);
        Arena *res = os_commit(blockSize);
        if (!res) {
                printf("ERROR: Arena at %s:%d\n", p->createdFile,
                       p->createdLine);
                os_abort(1);
        }

        res->prev = 0;
        res->top = res;
        res->base = 0;
        res->pos = 0;
        res->size = blockSize;
        res->createdFile = p->createdFile;
        res->createdLine = p->createdLine;

        return res;
}

static void *arena_alloc(Arena *a, u64 size) {
        Arena *top = a->top;
        if (top->pos + size > top->size - ARENA_HEADER_SIZE) {
                ArenaParams params = {
                        .blockSize = size,
                        .createdFile = top->createdFile,
                        .createdLine = top->createdLine,
                };
                Arena *new = make_arena_(&params);

                new->prev = top;
                top = new;
                for (Arena *arena = top; arena; arena = arena->prev) {
                        arena->top = top;
                }
        }

        void *res = (void *)top + top->pos + ARENA_HEADER_SIZE;
        top->pos += size;

        mem_zero(res, size);

        return res;
}

static u64 arena_pos(Arena *a) {
        Arena *top = a->top;
        u64 res = top->base + top->pos;
        return res;
}

static void arena_pop_at(Arena *a, u64 pos) {
        Arena *top = a->top;
        if (pos >= top->base + top->size - ARENA_HEADER_SIZE) {
                printf("ERROR: Arena at %s:%d - popped beyond allocation\n",
                       a->createdFile, a->createdLine);
                os_abort(1);
        }

        while (pos < top->base) {
                Arena *rel = top;
                top = top->prev;
                os_release(rel, rel->size);
                // NOTE: hr: may improve performance to hold released
                // arenas in a free list for later use
        }
        top->pos = pos - top->base;

        for (Arena *arena = top; arena; arena = arena->prev) {
                arena->top = top;
        }
}

static void arena_reset(Arena *a) {
        arena_pop_at(a, 0);
}

static void arena_pop(Arena *a, u64 amt) {
        u64 pos = arena_pos(a);
        if (pos >= amt) {
                arena_pop_at(a, pos - amt);
        } else {
                printf("ERROR: Arena at %s:%d - popped an amount which is "
                       "larger than the arena\n",
                       a->createdFile, a->createdLine);
                os_abort(1);
        }
}
