#include "bedrock/bedrock_inc.h"
#include "perf.h"

#include <dlfcn.h>
#include <sys/time.h>

// hr: class constants.
#define KPC_CLASS_FIXED (0)
#define KPC_CLASS_CONFIGURABLE (1)
#define KPC_CLASS_POWER (2)
#define KPC_CLASS_RAWPMU (3)

// hr: class mask constants.
#define KPC_CLASS_FIXED_MASK (1u << KPC_CLASS_FIXED)
#define KPC_CLASS_CONFIGURABLE_MASK (1u << KPC_CLASS_CONFIGURABLE)
#define KPC_CLASS_POWER_MASK (1u << KPC_CLASS_POWER)
#define KPC_CLASS_RAWPMU_MASK (1u << KPC_CLASS_RAWPMU)

// hr: KPEP CPU archtecture constants.
#define KPEP_ARCH_I386 0
#define KPEP_ARCH_X86_64 1
#define KPEP_ARCH_ARM 2
#define KPEP_ARCH_ARM64 3

#define KPC_MAX_COUNTERS 32

typedef struct kpep_event {
        const char *name;
        const char *description;
        const char *errata;
        const char *alias;
        const char *fallback;
        u32 mask;
        u8 number;
        u8 umask;
        u8 reserved;
        u8 is_fixed;
} kpep_event;

typedef struct kpep_db {
        const char *name;
        const char *cpu_id; // hr: Plist name, such as "cpu_7_8_10b282dc".
        const char *marketing_name;
        void *plist_data; // hr: Plist data (CFDataRef)
        void *event_map;  // hr: Events (CFDict<CFSTR(event_name), kpep_event).
        kpep_event *event_arr; // hr: Event struct buffer
                               //     size: (sizeof(kpep_event) * events_count).
        kpep_event **fixed_event_arr; // hr: Fixed counter events
                                      //     size: (sizeof(kpep_event) *
                                      //     fixed_counter_count)
        void *alias_map; // hr: Aliases (CFDict<CFSTR(event_name), kpep_event).
        size_t reserved_1;
        size_t reserved_2;
        size_t reserved_3;
        size_t event_count;
        size_t alias_count;
        size_t fixed_counter_count;
        size_t config_counter_count;
        size_t power_counter_count;
        u32 archtecture; // hr: see `KPEP CPU archtecture constants` above.
        u32 fixed_counter_bits;
        u32 config_counter_bits;
        u32 power_counter_bits;
} kpep_db;

typedef struct kpep_config_t {
        kpep_db *db;
        kpep_event **ev_arr; // hr: (sizeof(kpep_event *) * counter_count)
        size_t *ev_map;      // hr: (sizeof(usize_t *) * counter_count)
        size_t *ev_idx;      // hr: (sizeof(usize_t *) * counter_count)
        u32 *flags;          // hr: (sizeof(u32 *) * counter_count)
        u64 *kpc_periods;    // hr: (sizeof(u64 *) * counter_count)
        size_t event_count;  // hr: kpep_config_t_events_count()
        size_t counter_count;
        u32 classes; // hr: see `class mask constants` above.
        u32 config_counter;
        u32 power_counter;
        u32 reserved;
} kpep_config_t;

// hr: error codes for kpep_config_t_xxx() and kpep_db_xxx() functions.
typedef enum {
        kpep_config_t_ERROR_NONE = 0,
        kpep_config_t_ERROR_INVALID_ARGUMENT = 1,
        kpep_config_t_ERROR_OUT_OF_MEMORY = 2,
        kpep_config_t_ERROR_IO = 3,
        kpep_config_t_ERROR_BUFFER_TOO_SMALL = 4,
        kpep_config_t_ERROR_CUR_SYSTEM_UNKNOWN = 5,
        kpep_config_t_ERROR_DB_PATH_INVALID = 6,
        kpep_config_t_ERROR_DB_NOT_FOUND = 7,
        kpep_config_t_ERROR_DB_ARCH_UNSUPPORTED = 8,
        kpep_config_t_ERROR_DB_VERSION_UNSUPPORTED = 9,
        kpep_config_t_ERROR_DB_CORRUPT = 10,
        kpep_config_t_ERROR_EVENT_NOT_FOUND = 11,
        kpep_config_t_ERROR_CONFLICTING_EVENTS = 12,
        kpep_config_t_ERROR_COUNTERS_NOT_FORCED = 13,
        kpep_config_t_ERROR_EVENT_UNAVAILABLE = 14,
        kpep_config_t_ERROR_ERRNO = 15,
        kpep_config_t_ERROR_MAX
} kpep_config_t_error_code;

static String8 kpep_config_t_error_names[kpep_config_t_ERROR_MAX] = {
        ccstring8_lit("none"),
        ccstring8_lit("invalid argument"),
        ccstring8_lit("out of memory"),
        ccstring8_lit("I/O"),
        ccstring8_lit("buffer too small"),
        ccstring8_lit("current system unknown"),
        ccstring8_lit("database path invalid"),
        ccstring8_lit("database not found"),
        ccstring8_lit("database architecture unsupported"),
        ccstring8_lit("database version unsupported"),
        ccstring8_lit("database corrupt"),
        ccstring8_lit("event not found"),
        ccstring8_lit("conflicting events"),
        ccstring8_lit("all counters must be forced"),
        ccstring8_lit("event unavailable"),
        ccstring8_lit("check errno"),
};

static String8 kpep_config_t_error_desc(u32 code) {
        if (0 <= code && code < kpep_config_t_ERROR_MAX) {
                return kpep_config_t_error_names[code];
        }
        return string8_lit("unknown error");
}

#define lib_path_kperf "/System/Library/PrivateFrameworks/kperf.framework/kperf"
#define lib_path_kperfdata                                                     \
        "/System/Library/PrivateFrameworks/kperfdata.framework/kperfdata"

typedef u64 kpc_config_t;

static int (*kpc_force_all_ctrs_get)(int *val_out);
static int (*kpc_force_all_ctrs_set)(int val);
static int (*kpc_set_config)(u32 classes, kpc_config_t *config);
static int (*kpc_set_counting)(u32 classes);
static int (*kpc_set_thread_counting)(u32 classes);
static int (*kpc_get_thread_counters)(u32 tid, u32 buf_count, u64 *buf);

static int (*kpep_db_create)(const char *name, kpep_db **db_ptr);
static int (*kpep_config_create)(kpep_db *db, kpep_config_t **cfg_ptr);
static int (*kpep_config_force_counters)(kpep_config_t *cfg);
static int (*kpep_db_event)(kpep_db *db, const char *name, kpep_event **ev_ptr);
static int (*kpep_config_add_event)(kpep_config_t *cfg, kpep_event **ev_ptr,
                                    u32 flag, u32 *err);
static int (*kpep_config_kpc_classes)(kpep_config_t *cfg, u32 *classes);
static int (*kpep_config_kpc_count)(kpep_config_t *cfg, size_t *count_ptr);
static int (*kpep_config_kpc_map)(kpep_config_t *cfg, size_t *buf,
                                  size_t buf_size);
static int (*kpep_config_kpc)(kpep_config_t *cfg, kpc_config_t *buf,
                              size_t buf_size);

typedef enum {
        KPC_FORCE_ALL_CTRS_GET,
        KPC_FORCE_ALL_CTRS_SET,
        KPC_GET_CONFIG_COUNT,
        KPC_SET_CONFIG,
        KPC_SET_COUNTING,
        KPC_SET_THREAD_COUNTING,
        KPC_GET_THREAD_COUNTERS,
} KperfLibSymbol;

typedef enum {
        KPEP_DB_CREATE,
        kpep_config_t_CREATE,
        kpep_config_t_FORCE_COUNTERS,
        KPEP_DB_EVENT,
        kpep_config_t_ADD_EVENT,
        kpep_config_t_KPC_CLASSES,
        kpep_config_t_KPC_COUNT,
        kpep_config_t_KPC_MAP,
        kpep_config_t_KPC,
} KperfdataLibSymbol;

typedef struct {
        const char *name;
        void **impl;
} PerfLibSymbol;

static PerfLibSymbol kperf_lib_symbols[] = {
        { "kpc_force_all_ctrs_get", (void **)&kpc_force_all_ctrs_get },
        { "kpc_force_all_ctrs_set", (void **)&kpc_force_all_ctrs_set },
        { "kpc_set_config", (void **)&kpc_set_config },
        { "kpc_set_counting", (void **)&kpc_set_counting },
        { "kpc_set_thread_counting", (void **)&kpc_set_thread_counting },
        { "kpc_get_thread_counters", (void **)&kpc_get_thread_counters },
};

static PerfLibSymbol kperfdata_lib_symbols[] = {
        { "kpep_db_create", (void **)&kpep_db_create },
        { "kpep_config_create", (void **)&kpep_config_create },
        { "kpep_config_force_counters", (void **)&kpep_config_force_counters },
        { "kpep_db_event", (void **)&kpep_db_event },
        { "kpep_config_add_event", (void **)&kpep_config_add_event },
        { "kpep_config_kpc_classes", (void **)&kpep_config_kpc_classes },
        { "kpep_config_kpc_count", (void **)&kpep_config_kpc_count },
        { "kpep_config_kpc_map", (void **)&kpep_config_kpc_map },
        { "kpep_config_kpc", (void **)&kpep_config_kpc },
};

static void *kperf_lib_handle = 0;
static void *kperfdata_lib_handle = 0;

#define PERF_EVENT_NAME_MAX 8
typedef struct {
        const char *alias;
        const char *names[PERF_EVENT_NAME_MAX];
} PerfEventAlias;

#define PERF_EVENT_COUNT 7
// hr: event names from /usr/share/kpep/<name>.plist
static const PerfEventAlias perf_profile_events[PERF_EVENT_COUNT] = {
        { "cycles",
          {
                  "FIXED_CYCLES",            // Apple A7-A15
                  "CPU_CLK_UNHALTED.THREAD", // Intel Core 1th-10th
                  "CPU_CLK_UNHALTED.CORE",   // Intel Yonah, Merom
          } },
        { "instructions",
          {
                  "FIXED_INSTRUCTIONS", // Apple A7-A15
                  "INST_RETIRED.ANY"    // Intel Yonah, Merom, Core 1th-10th
          } },
        { "branches",
          {
                  "INST_BRANCH",                  // Apple A7-A15
                  "BR_INST_RETIRED.ALL_BRANCHES", // Intel Core 1th-10th
                  "INST_RETIRED.ANY",             // Intel Yonah, Merom
          } },
        { "branch-misses",
          {
                  "BRANCH_MISPRED_NONSPEC", // Apple A7-A15, since iOS 15, macOS
                                            // 12
                  "BRANCH_MISPREDICT",      // Apple A7-A14
                  "BR_MISP_RETIRED.ALL_BRANCHES", // Intel Core 2th-10th
                  "BR_INST_RETIRED.MISPRED",      // Intel Yonah, Merom
          } },
        { "l1d-misses-load",
          {
                  "L1D_CACHE_MISS_LD", // Apple A15
          } },
        { "l1d-misses-store",
          {
                  "L1D_CACHE_MISS_ST", // Apple A15
          } },
        { "l1d-tlb-misses",
          {
                  "L1D_TLB_MISS", // Apple A15
          } },

};

static u32 perf_classes = 0;
static size_t perf_reg_count = 0;
static kpc_config_t perf_regs[KPC_MAX_COUNTERS] = { 0 };
static size_t perf_counter_map[KPC_MAX_COUNTERS] = { 0 };
static u64 perf_counters[KPC_MAX_COUNTERS] = { 0 };

static kpep_event *perf_get_event(kpep_db *db, const PerfEventAlias *alias) {
        for (size_t j = 0; j < PERF_EVENT_NAME_MAX; j++) {
                const char *name = alias->names[j];
                if (!name) {
                        break;
                }
                kpep_event *ev = 0;
                if (kpep_db_event(db, name, &ev) == 0) {
                        return ev;
                }
        }
        return 0;
}

static void perf_lib_init(void) {
        kperf_lib_handle = dlopen(lib_path_kperf, RTLD_LAZY);
        if (!kperf_lib_handle) {
                char *err = dlerror();
                String8 msg = { (u8 *)err, strlen(err) };
                log_message(msg);
                os_abort(1);
        }
        kperfdata_lib_handle = dlopen(lib_path_kperfdata, RTLD_LAZY);
        if (!kperfdata_lib_handle) {
                char *err = dlerror();
                String8 msg = { (u8 *)err, strlen(err) };
                log_message(msg);
                os_abort(1);
        }

        // hr:  load symbol address from dynamic library
        for (u32 i = 0; i < array_count(kperf_lib_symbols); i++) {
                PerfLibSymbol *symbol = &kperf_lib_symbols[i];
                *symbol->impl = dlsym(kperf_lib_handle, symbol->name);
                if (!symbol->impl) {
                        log_message(string8_lit(
                                "Failed to load kperf function.\n"));
                        os_abort(1);
                }
        }
        for (u32 i = 0; i < array_count(kperfdata_lib_symbols); i++) {
                PerfLibSymbol *symbol = &kperfdata_lib_symbols[i];
                *symbol->impl = dlsym(kperfdata_lib_handle, symbol->name);
                if (!symbol->impl) {
                        log_message(string8_lit(
                                "Failed to load kperfdata function.\n"));
                        os_abort(1);
                }
        }
}

static void perf_setup_counters(void) {
        perf_lib_init();

        // hr: check permission
        int forceCtrs = 0;
        if (kpc_force_all_ctrs_get(&forceCtrs)) {
                log_message(string8_lit("Permission denied, xnu/kpc requires "
                                        "root privileges.\n"));
                os_abort(1);
        }

        // hr: load pmc db
        kpep_db *db = 0;
        if (kpep_db_create(0, &db)) {
                goto error;
        }

        // hr: create a config
        kpep_config_t *cfg = 0;
        if (kpep_config_create(db, &cfg)) {
                goto error;
        }
        if (kpep_config_force_counters(cfg)) {
                goto error;
        }

        // hr: get events
        kpep_event *ev_arr[PERF_EVENT_COUNT] = { 0 };
        for (u32 i = 0; i < PERF_EVENT_COUNT; i++) {
                const PerfEventAlias *alias = perf_profile_events + i;
                ev_arr[i] = perf_get_event(db, alias);
                if (!ev_arr[i]) {
                        goto error;
                }
        }

        // hr: add event to config
        for (u32 i = 0; i < PERF_EVENT_COUNT; i++) {
                kpep_event *ev = ev_arr[i];
                if (kpep_config_add_event(cfg, &ev, 0, 0)) {
                        goto error;
                }
        }

        // hr: prepare buffer and config
        if (kpep_config_kpc_classes(cfg, &perf_classes)) {
                goto error;
        }
        if (kpep_config_kpc_count(cfg, &perf_reg_count)) {
                goto error;
        }
        if (kpep_config_kpc_map(cfg, perf_counter_map,
                                sizeof(perf_counter_map))) {
                goto error;
        }
        if (kpep_config_kpc(cfg, perf_regs, sizeof(perf_regs))) {
                goto error;
        }

        // hr: set config to kernel
        if (kpc_force_all_ctrs_set(1)) {
                goto error;
        }
        if ((perf_classes & KPC_CLASS_CONFIGURABLE_MASK) && perf_reg_count) {
                if (kpc_set_config(perf_classes, perf_regs)) {
                        goto error;
                }
        }

        // hr: start counting
        if (kpc_set_counting(perf_classes)) {
                goto error;
        }
        if (kpc_set_thread_counting(perf_classes)) {
                goto error;
        }

        return;
error:
        log_message(string8_lit("Failed to set up performance counters.\n"));
        os_abort(1);
}

static PerfMetrics perf_read_counters(void) {
        PerfMetrics res = { 0 };
        if (kpc_get_thread_counters(0, KPC_MAX_COUNTERS, perf_counters)) {
                log_message(string8_lit(
                        "Failed to get thread performance counters.\n"));
                log_dump(os_stderr(), 1);
        }
        res.cycles = perf_counters[perf_counter_map[0]];
        res.branches = perf_counters[perf_counter_map[1]];
        res.branchMisses = perf_counters[perf_counter_map[2]];
        res.l1dLoadMisses = perf_counters[perf_counter_map[3]];
        res.l1dStoreMisses = perf_counters[perf_counter_map[4]];
        res.l1dTLBMisses = perf_counters[perf_counter_map[5]];
        return res;
}

static u64 perf_os_timer_freq(void) {
        return 1000000;
}

static u64 perf_read_os_timer(void) {
        struct timeval value;
        gettimeofday(&value, 0);
        u64 freq = perf_os_timer_freq();
        u64 result = freq * (u64)value.tv_sec + (u64)value.tv_usec;
        return result;
}

static u64 perf_estimate_cpu_timer_freq(void) {
        u64 msToWait = 100;
        u64 osFreq = perf_os_timer_freq();

        u64 cpuStart = perf_read_counters().cycles;
        u64 osStart = perf_read_os_timer();
        u64 osEnd = 0;
        u64 osElapsed = 0;
        u64 osWaitTime = osFreq * msToWait / 1000;
        while (osElapsed < osWaitTime) {
                osEnd = perf_read_os_timer();
                osElapsed = osEnd - osStart;
        }

        u64 cpuEnd = perf_read_counters().cycles;
        u64 cpuElapsed = cpuEnd - cpuStart;

        u64 cpuFreq = 0;
        if (osElapsed) {
                cpuFreq = osFreq * cpuElapsed / osElapsed;
        }

        return cpuFreq;
}
