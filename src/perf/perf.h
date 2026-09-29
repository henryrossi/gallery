#ifndef PERF_PERF_H
#define PERF_PERF_H

#include "bedrock/bedrock_inc.h"

// Profiler
//
//
// Repetition tester
//
//

typedef struct {
        u64 cycles;
        u64 branches;
        u64 branchMisses;
        u64 l1dLoadMisses;
        u64 l1dStoreMisses;
        u64 l1dTLBMisses;
} PerfMetrics;

typedef struct PFNode PerfNode;
struct PFNode {
        PerfNode *first;
        PerfNode *last;
        PerfNode *next;
        PerfNode *prev;
        String8 name;
        PerfMetrics metrics;
        u32 id;
        u32 hits;
};

static void perf_setup_counters(void);
static PerfMetrics perf_read_counters(void);
static u64 perf_estimate_cpu_timer_freq(void);

#define SETUP_PROFILER perf_setup_profiler()
#define START_BLOCK(name)                                                      \
        perf_start_profiler_block(string8_lit(name), __COUNTER__ + 1)
#define END_BLOCK perf_end_profiler_block()
#define RESET_PROFILER perf_reset_profiler()
#define PRINT_PROFILER perf_print_profiler()

#endif // PERF_PERF_H
