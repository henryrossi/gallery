#ifndef PERF_PERF_H
#define PERF_PERF_H

#include "bedrock/bedrock_inc.h"
#include <assert.h>

typedef struct PFNode PFNode;
struct PFNode {
        u32 id;
        u32 hits;
        u64 duration;
        const char *name;
        PFNode *first;
        PFNode *last;
        PFNode *next;
        PFNode *prev;
};

#define SETUP_PROFILER perf_setup_profiler()
#define START_BLOCK(name) perf_start_block(name, __COUNTER__ + 1)
#define END_BLOCK perf_end_block()
#define RESET_PROFILER perf_reset_profiler()
#define PRINT_PROFILER perf_print_profiler(&perf_state.root, 0)

#endif // PERF_PERF_H

#define PERF_IMPLEMENTATION
#ifdef PERF_IMPLEMENTATION

static u32 perf_setup_timer(void);
static u64 perf_read_cpu_timer(void);

#if __MACH__
#include "perf/perf_mach.c"
#elif _WIN32
#include "perf/perf_windows.h"
#elif __linux__
#error perf not implemented for Linux.
#else
#error perf not implemented for this operating system.
#endif

#define PERF_MAX_DEPTH 64

struct PERFState {
        Arena *arena;
        PFNode root;
        f64 timerFreq;
        PFNode *nodes[PERF_MAX_DEPTH];
        u64 starts[PERF_MAX_DEPTH];
        u32 stackTop;
} perf_state = { 0 };

static void perf_setup_profiler(void) {
        u32 res = perf_setup_timer();
        assert(res == 0 && "Failed to set up timer");

        perf_state.arena = make_arena(mb(4));
        perf_state.nodes[0] = &perf_state.root;
        perf_state.starts[0] = perf_read_cpu_timer();
        perf_state.timerFreq = perf_estimate_cpu_timer_freq();
}

static void perf_start_block(const char *name, u32 id) {
        assert(perf_state.stackTop + 1 < PERF_MAX_DEPTH
               && "Profiler max depth exceeded");

        PFNode *parent = perf_state.nodes[perf_state.stackTop++];
        PFNode *last = parent->last;
        if (last && last->id == id) {
                perf_state.nodes[perf_state.stackTop] = last;
        } else {
                PFNode *n = arena_alloc(perf_state.arena, sizeof(*n));
                n->name = name;
                n->id = id;

                if (last) {
                        last->next = n;
                        n->prev = last;
                } else {
                        parent->first = n;
                }
                parent->last = n;

                perf_state.nodes[perf_state.stackTop] = n;
        }
        perf_state.starts[perf_state.stackTop] = perf_read_cpu_timer();
}

static void perf_end_block(void) {
        assert(perf_state.stackTop > 0
               && "Profiler error: ended with no started block");

        u64 end = perf_read_cpu_timer();
        u64 start = perf_state.starts[perf_state.stackTop];
        PFNode *n = perf_state.nodes[perf_state.stackTop--];
        n->duration += end - start;
        n->hits++;
}

static void perf_reset_profiler(void) {
        arena_reset(perf_state.arena);
        perf_state.root.first = 0;
        perf_state.root.last = 0;
        perf_state.stackTop = 0;
}

static void perf_print_profiler(PFNode *n, u32 depth) {
        if (!n) {
                return;
        }

        printf("%*s %s duration: %.2fms hits: %u\n", depth * 2, "", n->name,
               n->duration / perf_state.timerFreq * 1000, n->hits);

        for (PFNode *child = n->first; child; child = child->next) {
                perf_print_profiler(child, depth + 1);
        }
}

#endif // PERF_IMPLEMENTATION
