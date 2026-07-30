#include "bedrock/bedrock_arena.h"
#include "bedrock/bedrock_freelist.h"
#include "bedrock_inc.h"
#include "testing/test.h"

#include <stdlib.h> // hr: for rand and srand

// hr: test arena module

static b32 test_make_arena(Arena **a) {
        u64 size = rand() % gb(1);

        *a = make_arena(size);

        if (!*a) {
                testing_push_error_input(size, string8_lit("size"));
                return 0;
        }
        return 1;
}

static b32 test_destroy_arena(Arena *a) {
        destroy_arena(a);
        return 1;
}

static b32 test_arena_alloc(Arena *a) {
        u64 size = rand();
        b32 res = 0;

        void *p = arena_alloc(a, size);

        if (p) {
                if (!arena_contains_mem(a, p)) {
                        testing_push_error_input(size, string8_lit("size"));
                        return res;
                }
        } else {
                printf("Allocation failed for size %llu unclear if enough "
                       "memory was available in the system for this.\n",
                       size);
        }
        res = 1;
        return res;
}

static b32 test_arena_pop_at(Arena *a) {
        u64 pos = rand();

        b32 within = a->top->base + a->top->pos >= pos;

        b32 res = arena_pop_at(a, pos);

        if (within) {
                if (!res) {
                        goto failed;
                }
        } else {
                if (res) {
                        goto failed;
                }
        }
        return 1;

failed:
        testing_push_error_input(pos, string8_lit("pos"));
        return 0;
}

static b32 test_arena(u64 seed, u64 reps) {
        Arena *a = 0;
        b32 res = 0;
        srand(seed);

        for (u64 i = 0; i < reps; i++) {
                if (!a) {
                        res = test_make_arena(&a);
                } else {
                        f64 r = (f64)rand() / (f64)RAND_MAX;
                        if (r < 0.45) {
                                res = test_arena_alloc(a);
                        } else if (r < 0.90) {
                                res = test_arena_pop_at(a);
                        } else {
                                res = test_destroy_arena(a);
                                a = 0;
                        }
                }

                if (!res) {
                        return res;
                }
        }

        return 1;
}

// hr: test freelist module

static b32 test_make_freelist(Freelist *f) {
        u64 size = rand() % gb(1);

        *f = make_freelist(size);

        if (!f->mem) {
                testing_push_error_input(size, string8_lit("size"));
                return 0;
        }
        return 1;
}

static b32 test_destroy_freelist(Freelist *f) {
        // destroy_freelist(f);
        return 1;
}

static b32 test_freelist_alloc(Freelist *f) {
        return 1;
}

static b32 test_freelist(u64 seed, u64 reps) {
        Arena *a = 0;
        b32 res = 0;
        srand(seed);

        for (u64 i = 0; i < reps; i++) {
                if (!a) {
                        res = test_make_arena(&a);
                } else {
                        f64 r = (f64)rand() / (f64)RAND_MAX;
                        if (r < 0.45) {
                                res = test_arena_alloc(a);
                        } else if (r < 0.90) {
                                res = test_arena_pop_at(a);
                        } else {
                                res = test_destroy_arena(a);
                                a = 0;
                        }
                }

                if (!res) {
                        return res;
                }
        }

        return 1;
}
