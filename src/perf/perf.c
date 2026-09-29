#include "perf/perf.h"

#if __MACH__
#include "perf/perf_mach.c"
#elif _WIN32
#include "perf/perf_windows.h"
#elif __linux__
#error perf not implemented for Linux.
#else
#error perf not implemented for this operating system.
#endif

#define PERF_MAX_DEPTH 24

static PerfMetrics perf_metrics_diff(PerfMetrics *start, PerfMetrics *end) {
        PerfMetrics res = {
                .cycles = end->cycles - start->cycles,
                .branches = end->branches - start->branches,
                .branchMisses = end->branchMisses - start->branchMisses,
                .l1dLoadMisses = end->l1dLoadMisses - start->l1dLoadMisses,
                .l1dStoreMisses = end->l1dStoreMisses - start->l1dStoreMisses,
                .l1dTLBMisses = end->l1dTLBMisses - start->l1dTLBMisses,
        };
        return res;
}

static void perf_metrics_accum(PerfMetrics *accum, PerfMetrics *addition) {
        accum->cycles += addition->cycles;
        accum->branches += addition->branches;
        accum->branchMisses += addition->branchMisses;
        accum->l1dLoadMisses += addition->l1dLoadMisses;
        accum->l1dStoreMisses += addition->l1dStoreMisses;
        accum->l1dTLBMisses += addition->l1dTLBMisses;
}

struct PerfProfilerState {
        Arena *arena;
        PerfNode root;
        f64 timerFreq;
        PerfNode *nodes[PERF_MAX_DEPTH];
        PerfMetrics starts[PERF_MAX_DEPTH];
        u32 stackTop;
} perf_pro_state = { 0 };

static void perf_setup_profiler(void) {
        perf_setup_counters();

        perf_pro_state.arena = make_arena(mb(4));
        perf_pro_state.nodes[0] = &perf_pro_state.root;
        perf_pro_state.starts[0] = perf_read_counters();
        perf_pro_state.timerFreq = perf_estimate_cpu_timer_freq();
}

static void perf_start_profiler_block(String8 name, u32 id) {
        if (perf_pro_state.stackTop + 1 > PERF_MAX_DEPTH) {
                log_message(
                        string8_lit("Profiler maximum stack depth exceeded\n"));
                os_abort(1);
        }

        PerfNode *parent = perf_pro_state.nodes[perf_pro_state.stackTop++];
        PerfNode *last = parent->last;
        if (last && last->id == id) {
                perf_pro_state.nodes[perf_pro_state.stackTop] = last;
        } else {
                PerfNode *n = arena_alloc(perf_pro_state.arena, sizeof(*n));
                n->name = name;
                n->id = id;

                DLLPushBack(parent->first, parent->last, n);
                perf_pro_state.nodes[perf_pro_state.stackTop] = n;
        }
        perf_pro_state.starts[perf_pro_state.stackTop] = perf_read_counters();
}

static void perf_end_profiler_block(void) {
        if (perf_pro_state.stackTop < 0) {
                log_message(
                        string8_lit("Ended profiler with no started block"));
                os_abort(1);
        }

        PerfMetrics end = perf_read_counters();
        PerfMetrics start = perf_pro_state.starts[perf_pro_state.stackTop];
        PerfNode *n = perf_pro_state.nodes[perf_pro_state.stackTop--];
        PerfMetrics diff = perf_metrics_diff(&start, &end);
        perf_metrics_accum(&n->metrics, &diff);
        n->hits++;
}

static void perf_reset_profiler(void) {
        arena_reset(perf_pro_state.arena);
        perf_pro_state.root.first = 0;
        perf_pro_state.root.last = 0;
        perf_pro_state.stackTop = 0;
}

#define PERF_PRINT_EDGE                                                        \
        printf("+------------------------------------------------------------" \
               "------------------+\n");
#define PERF_PRINT_DIV                                                         \
        printf("|------------------------------------------------------------" \
               "------------------|\n");

static void perf__print_profiler(PerfNode *n) {
        if (!n) {
                return;
        }

        const char *units = "\0";
        const char *seconds = "s ";
        const char *ms = "ms";
        const char *us = "us";
        const char *kilo = "k";
        const char *mega = "m";
        const char *giga = "g";

        PerfMetrics met = n->metrics;

        printf("| %18.18s %2d %5d ", n->name.data, n->id, n->hits);

        f64 elapsed = (f64)met.cycles / (f64)perf_pro_state.timerFreq;
        units = seconds;
        if (elapsed < 0.001) {
                elapsed *= 1000000;
                units = us;
        } else if (elapsed < 1.0) {
                elapsed *= 1000;
                units = ms;
        }
        printf("%6.1f%s ", elapsed, units);

        f64 branchMisses = (f64)met.branchMisses;
        f64 per = 100.0 * branchMisses / (f64)met.branches;
        if (branchMisses > 999999999) {
                branchMisses *= 0.000000001;
                units = giga;
        } else if (branchMisses > 999999) {
                branchMisses *= 0.000001;
                units = mega;
        } else if (branchMisses > 999) {
                branchMisses *= 0.001;
                units = kilo;
        }
        printf("%6.1f%s (%4.1f%%) ", branchMisses, units, per);

        f64 ld1misses = (f64)met.l1dLoadMisses + (f64)met.l1dStoreMisses;
        per = 100.0 * (f64)met.l1dLoadMisses / ld1misses;
        if (ld1misses > 999999999) {
                ld1misses *= 0.000000001;
                units = giga;
        } else if (ld1misses > 999999) {
                ld1misses *= 0.000001;
                units = mega;
        } else if (ld1misses > 999) {
                ld1misses *= 0.001;
                units = kilo;
        }
        printf("%6.1f%s (%4.1f%%) ", ld1misses, units, per);

        f64 ld1tlbmisses = (f64)met.l1dTLBMisses;
        if (ld1tlbmisses > 999999999) {
                ld1tlbmisses *= 0.000000001;
                units = giga;
        } else if (ld1tlbmisses > 999999) {
                ld1tlbmisses *= 0.000001;
                units = mega;
        } else if (ld1tlbmisses > 999) {
                ld1tlbmisses *= 0.001;
                units = kilo;
        }
        printf("%6.1f%s |\n", ld1tlbmisses, units);

        for (PerfNode *child = n->first; child; child = child->next) {
                perf__print_profiler(child);
        }
}

static void perf_print_profiler(void) {
        PERF_PRINT_EDGE;
        printf("|               name id  hits    time     branch miss      L1d "
               "miss    L1d TLB |\n");
        PERF_PRINT_DIV;
        for (PerfNode *child = perf_pro_state.root.first; child;
             child = child->next) {
                perf__print_profiler(child);
        }
        PERF_PRINT_EDGE
}
