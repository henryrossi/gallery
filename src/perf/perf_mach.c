#include "bedrock/bedrock_inc.h"

#include <dlfcn.h>
#include <stddef.h>
#include <sys/time.h>

// Cross-platform class constants.
#define KPC_CLASS_FIXED (0)
#define KPC_CLASS_CONFIGURABLE (1)
#define KPC_CLASS_POWER (2)
#define KPC_CLASS_RAWPMU (3)

// Cross-platform class mask constants.
#define KPC_CLASS_FIXED_MASK (1u << KPC_CLASS_FIXED)               // 1
#define KPC_CLASS_CONFIGURABLE_MASK (1u << KPC_CLASS_CONFIGURABLE) // 2
#define KPC_CLASS_POWER_MASK (1u << KPC_CLASS_POWER)               // 4
#define KPC_CLASS_RAWPMU_MASK (1u << KPC_CLASS_RAWPMU)             // 8

// PMU version constants.
#define KPC_PMU_ERROR (0)     // Error
#define KPC_PMU_INTEL_V3 (1)  // Intel
#define KPC_PMU_ARM_APPLE (2) // ARM64
#define KPC_PMU_INTEL_V2 (3)  // Old Intel
#define KPC_PMU_ARM_V2 (4)    // Old ARM
//
#define KPC_MAX_COUNTERS 32

/// KPEP event (size: 48/28 bytes on 64/32 bit OS)
typedef struct kpep_event {
        const char
            *name; ///< Unique name of a event, such as "INST_RETIRED.ANY".
        const char *description; ///< Description for this event.
        const char *errata;      ///< Errata, currently NULL.
        const char *alias;    ///< Alias name, such as "Instructions", "Cycles".
        const char *fallback; ///< Fallback event name for fixed counter.
        uint32_t mask;
        uint8_t number;
        uint8_t umask;
        uint8_t reserved;
        uint8_t is_fixed;
} kpep_event;

/// KPEP database (size: 144/80 bytes on 64/32 bit OS)
typedef struct kpep_db {
        const char *name;   ///< Database name, such as "haswell".
        const char *cpu_id; ///< Plist name, such as "cpu_7_8_10b282dc".
        const char
            *marketing_name; ///< Marketing name, such as "Intel Haswell".
        void *plist_data;    ///< Plist data (CFDataRef), currently NULL.
        void *event_map; ///< All events (CFDict<CFSTR(event_name), kpep_event
                         ///< *>).
        kpep_event *event_arr; ///< Event struct buffer (sizeof(kpep_event) *
                               ///< events_count).
        kpep_event *
            *fixed_event_arr; ///< Fixed counter events (sizeof(kpep_event *) *
                              ///< fixed_counter_count)
        void *alias_map; ///< All aliases (CFDict<CFSTR(event_name), kpep_event
                         ///< *>).
        size_t reserved_1;
        size_t reserved_2;
        size_t reserved_3;
        size_t event_count; ///< All events count.
        size_t alias_count;
        size_t fixed_counter_count;
        size_t config_counter_count;
        size_t power_counter_count;
        uint32_t archtecture; ///< see `KPEP CPU archtecture constants` above.
        uint32_t fixed_counter_bits;
        uint32_t config_counter_bits;
        uint32_t power_counter_bits;
} kpep_db;

/// KPEP config (size: 80/44 bytes on 64/32 bit OS)
typedef struct kpep_config {
        kpep_db *db;
        kpep_event *
            *ev_arr;     ///< (sizeof(kpep_event *) * counter_count), init NULL
        size_t *ev_map;  ///< (sizeof(usize *) * counter_count), init 0
        size_t *ev_idx;  ///< (sizeof(usize *) * counter_count), init -1
        uint32_t *flags; ///< (sizeof(u32 *) * counter_count), init 0
        uint64_t *kpc_periods; ///< (sizeof(uint64_t *) * counter_count), init 0
        size_t event_count;    /// kpep_config_events_count()
        size_t counter_count;
        uint32_t classes; ///< See `class mask constants` above.
        uint32_t config_counter;
        uint32_t power_counter;
        uint32_t reserved;
} kpep_config;

/// Error code for kpep_config_xxx() and kpep_db_xxx() functions.
typedef enum {
        KPEP_CONFIG_ERROR_NONE = 0,
        KPEP_CONFIG_ERROR_INVALID_ARGUMENT = 1,
        KPEP_CONFIG_ERROR_OUT_OF_MEMORY = 2,
        KPEP_CONFIG_ERROR_IO = 3,
        KPEP_CONFIG_ERROR_BUFFER_TOO_SMALL = 4,
        KPEP_CONFIG_ERROR_CUR_SYSTEM_UNKNOWN = 5,
        KPEP_CONFIG_ERROR_DB_PATH_INVALID = 6,
        KPEP_CONFIG_ERROR_DB_NOT_FOUND = 7,
        KPEP_CONFIG_ERROR_DB_ARCH_UNSUPPORTED = 8,
        KPEP_CONFIG_ERROR_DB_VERSION_UNSUPPORTED = 9,
        KPEP_CONFIG_ERROR_DB_CORRUPT = 10,
        KPEP_CONFIG_ERROR_EVENT_NOT_FOUND = 11,
        KPEP_CONFIG_ERROR_CONFLICTING_EVENTS = 12,
        KPEP_CONFIG_ERROR_COUNTERS_NOT_FORCED = 13,
        KPEP_CONFIG_ERROR_EVENT_UNAVAILABLE = 14,
        KPEP_CONFIG_ERROR_ERRNO = 15,
        KPEP_CONFIG_ERROR_MAX
} kpep_config_error_code;

/// Error description for kpep_config_error_code.
static const char *kpep_config_error_names[KPEP_CONFIG_ERROR_MAX]
    = { "none",
        "invalid argument",
        "out of memory",
        "I/O",
        "buffer too small",
        "current system unknown",
        "database path invalid",
        "database not found",
        "database architecture unsupported",
        "database version unsupported",
        "database corrupt",
        "event not found",
        "conflicting events",
        "all counters must be forced",
        "event unavailable",
        "check errno" };

/// Error description.
static const char *kpep_config_error_desc(int code) {
        if (0 <= code && code < KPEP_CONFIG_ERROR_MAX) {
                return kpep_config_error_names[code];
        }
        return "unknown error";
}

#define lib_path_kperf "/System/Library/PrivateFrameworks/kperf.framework/kperf"
#define lib_path_kperfdata                                                     \
        "/System/Library/PrivateFrameworks/kperfdata.framework/kperfdata"

typedef uint64_t kpc_config_t;

static int (*kpc_force_all_ctrs_get)(int *val_out);
static int (*kpc_force_all_ctrs_set)(int val);
static int (*kpc_set_config)(uint32_t classes, kpc_config_t *config);
static int (*kpc_set_counting)(uint32_t classes);
static int (*kpc_set_thread_counting)(uint32_t classes);
static int (*kpc_get_thread_counters)(uint32_t tid, uint32_t buf_count,
                                      uint64_t *buf);

static int (*kpep_db_create)(const char *name, kpep_db **db_ptr);
static int (*kpep_config_create)(kpep_db *db, kpep_config **cfg_ptr);
static int (*kpep_config_force_counters)(kpep_config *cfg);
static int (*kpep_db_event)(kpep_db *db, const char *name, kpep_event **ev_ptr);
static int (*kpep_config_add_event)(kpep_config *cfg, kpep_event **ev_ptr,
                                    uint32_t flag, uint32_t *err);
static int (*kpep_config_kpc_classes)(kpep_config *cfg, uint32_t *classes);
static int (*kpep_config_kpc_count)(kpep_config *cfg, size_t *count_ptr);
static int (*kpep_config_kpc_map)(kpep_config *cfg, size_t *buf,
                                  size_t buf_size);
static int (*kpep_config_kpc)(kpep_config *cfg, kpc_config_t *buf,
                              size_t buf_size);

typedef enum {
        KPC_FORCE_ALL_CTRS_GET,
        KPC_FORCE_ALL_CTRS_SET,
        KPC_GET_CONFIG_COUNT,
        KPC_SET_CONFIG,
        KP_SET_COUNTING,
        KPC_SET_THREAD_COUNTING,
        KPC_GET_THREAD_COUNTERS,
} kperf_lib_symbol;

typedef enum {
        KPEP_DB_CREATE,
        KPEP_CONFIG_CREATE,
        KPEP_CONFIG_FORCE_COUNTERS,
        KPEP_DB_EVENT,
        KPEP_CONFIG_ADD_EVENT,
        KPEP_CONFIG_KPC_CLASSES,
        KPEP_CONFIG_KPC_COUNT,
        KPEP_CONFIG_KPC_MAP,
        KPEP_CONFIG_KPC,
} kperfdata_lib_symbol;

typedef struct {
        const char *name;
        void **impl;
} lib_symbol;

lib_symbol kperf_lib_symbols[] = {
        { "kpc_force_all_ctrs_get", (void **)&kpc_force_all_ctrs_get },
        { "kpc_force_all_ctrs_set", (void **)&kpc_force_all_ctrs_set },
        { "kpc_set_config", (void **)&kpc_set_config },
        { "kpc_set_counting", (void **)&kpc_set_counting },
        { "kpc_set_thread_counting", (void **)&kpc_set_thread_counting },
        { "kpc_get_thread_counters", (void **)&kpc_get_thread_counters },
};

lib_symbol kperfdata_lib_symbols[] = {
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

static void *kperf_lib_handle = NULL;
static void *kperfdata_lib_handle = NULL;

typedef struct {
        uint64_t cycles;
        uint64_t branches;
        uint64_t missed_branches;
        uint64_t instructions;
} perf_counters;

#define EVENT_NAME_MAX 8
typedef struct {
        const char *alias;                 /// name for print
        const char *names[EVENT_NAME_MAX]; /// name from pmc db
} event_alias;

/// Event names from /usr/share/kpep/<name>.plist
#define EV_COUNT 4
static const event_alias profile_events[] = {
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
};

uint32_t classes = 0;
size_t reg_count = 0;
kpc_config_t regs[KPC_MAX_COUNTERS] = { 0 };
size_t counter_map[KPC_MAX_COUNTERS] = { 0 };
uint64_t counters_0[KPC_MAX_COUNTERS] = { 0 };
uint64_t counters_1[KPC_MAX_COUNTERS] = { 0 };

static kpep_event *get_event(kpep_db *db, const event_alias *alias) {
        for (size_t j = 0; j < EVENT_NAME_MAX; j++) {
                const char *name = alias->names[j];
                if (!name)
                        break;
                kpep_event *ev = NULL;
                if (kpep_db_event(db, name, &ev) == 0) {
                        return ev;
                }
        }
        return NULL;
}

static int lib_init(void) {
        kperf_lib_handle = dlopen(lib_path_kperf, RTLD_LAZY);
        if (!kperf_lib_handle) {
                fprintf(stderr, "Failed to load kperf.framework, message: %s.",
                        dlerror());
                return 1;
        }
        kperfdata_lib_handle = dlopen(lib_path_kperfdata, RTLD_LAZY);
        if (!kperfdata_lib_handle) {
                fprintf(stderr,
                        "Failed to load kperfdata.framework, message: %s.",
                        dlerror());
                return 1;
        }

        // load symbol address from dynamic library
        for (uint32_t i = 0;
             i < sizeof(kperf_lib_symbols) / sizeof(kperf_lib_symbols[0]);
             i++) {
                const lib_symbol *symbol = &kperf_lib_symbols[i];
                *symbol->impl = dlsym(kperf_lib_handle, symbol->name);
                if (!*symbol->impl) {
                        fprintf(stderr, "Failed to load kperf function: %s.",
                                symbol->name);
                        return 1;
                }
        }
        for (uint32_t i = 0; i < sizeof(kperfdata_lib_symbols)
                                     / sizeof(kperfdata_lib_symbols[0]);
             i++) {
                const lib_symbol *symbol = &kperfdata_lib_symbols[i];
                *symbol->impl = dlsym(kperfdata_lib_handle, symbol->name);
                if (!*symbol->impl) {
                        fprintf(stderr,
                                "Failed to load kperfdata function: %s.",
                                symbol->name);
                        return 1;
                }
        }

        return 0;
}

static u32 perf_setup_timer(void) {
        u32 ret = 0;
        if (lib_init()) {
                fprintf(stderr,
                        "Failed to initialize kperf and kperfdata libraries\n");
                return 1;
        }

        // check permission
        int force_ctrs = 0;
        if (kpc_force_all_ctrs_get(&force_ctrs)) {
                printf(
                    "Permission denied, xnu/kpc requires root privileges.\n");
                return 1;
        }

        // load pmc db
        kpep_db *db = NULL;
        if ((ret = kpep_db_create(NULL, &db))) {
                printf("Error: cannot load pmc database: %d.\n", ret);
                return 1;
        }
        // printf("loaded db: %s (%s)\n", db->name, db->marketing_name);
        // printf("number of fixed counters: %zu\n", db->fixed_counter_count);
        // printf("number of configurable counters: %zu\n",
        //        db->config_counter_count);

        // create a config
        kpep_config *cfg = NULL;
        if ((ret = kpep_config_create(db, &cfg))) {
                printf("Failed to create kpep config: %d (%s).\n", ret,
                       kpep_config_error_desc(ret));
                return 1;
        }
        if ((ret = kpep_config_force_counters(cfg))) {
                printf("Failed to force counters: %d (%s).\n", ret,
                       kpep_config_error_desc(ret));
                return 1;
        }

        // get events
        kpep_event *ev_arr[EV_COUNT] = { 0 };
        for (size_t i = 0; i < EV_COUNT; i++) {
                const event_alias *alias = profile_events + i;
                ev_arr[i] = get_event(db, alias);
                if (!ev_arr[i]) {
                        printf("Cannot find event: %s.\n", alias->alias);
                        return 1;
                }
        }

        // add event to config
        for (size_t i = 0; i < EV_COUNT; i++) {
                kpep_event *ev = ev_arr[i];
                if ((ret = kpep_config_add_event(cfg, &ev, 0, NULL))) {
                        printf("Failed to add event: %d (%s).\n", ret,
                               kpep_config_error_desc(ret));
                        return 1;
                }
        }

        // prepare buffer and config
        if ((ret = kpep_config_kpc_classes(cfg, &classes))) {
                printf("Failed get kpc classes: %d (%s).\n", ret,
                       kpep_config_error_desc(ret));
                return 1;
        }
        if ((ret = kpep_config_kpc_count(cfg, &reg_count))) {
                printf("Failed get kpc count: %d (%s).\n", ret,
                       kpep_config_error_desc(ret));
                return 1;
        }
        if ((ret
             = kpep_config_kpc_map(cfg, counter_map, sizeof(counter_map)))) {
                printf("Failed get kpc map: %d (%s).\n", ret,
                       kpep_config_error_desc(ret));
                return 1;
        }
        if ((ret = kpep_config_kpc(cfg, regs, sizeof(regs)))) {
                printf("Failed get kpc registers: %d (%s).\n", ret,
                       kpep_config_error_desc(ret));
                return 1;
        }

        // set config to kernel
        if ((ret = kpc_force_all_ctrs_set(1))) {
                printf("Failed force all ctrs: %d.\n", ret);
                return 1;
        }
        if ((classes & KPC_CLASS_CONFIGURABLE_MASK) && reg_count) {
                if ((ret = kpc_set_config(classes, regs))) {
                        printf("Failed set kpc config: %d.\n", ret);
                        return 1;
                }
        }

        // start counting
        if ((ret = kpc_set_counting(classes))) {
                printf("Failed set counting: %d.\n", ret);
                return 1;
        }
        if ((ret = kpc_set_thread_counting(classes))) {
                printf("Failed set thread counting: %d.\n", ret);
                return 1;
        }

        return 0;
}

static inline void get_counters(perf_counters *pc) {
        int ret = 0;
        if ((ret = kpc_get_thread_counters(0, KPC_MAX_COUNTERS, counters_0))) {
                fprintf(stderr, "Failed get thread counters before: %d.\n",
                        ret);
        }
        pc->cycles = counters_0[counter_map[0]];
        pc->branches = counters_0[counter_map[1]];
        pc->missed_branches = counters_0[counter_map[2]];
        pc->instructions = counters_0[counter_map[3]];
}

static u64 perf_read_cpu_timer(void) {
        perf_counters pc;
        get_counters(&pc);
        return pc.cycles;
}

static uint64_t perf_os_timer_freq(void) {
        return 1000000;
}

static uint64_t perf_read_os_timer(void) {
        struct timeval value;
        gettimeofday(&value, 0);

        uint64_t result = perf_os_timer_freq() * (uint64_t)value.tv_sec
                          + (uint64_t)value.tv_usec;
        return result;
}

// WARN: hr: does this return cycles per second?
static uint64_t perf_estimate_cpu_timer_freq(void) {
        uint64_t MillisecondsToWait = 100;
        uint64_t OSFreq = perf_os_timer_freq();

        uint64_t CPUStart = perf_read_cpu_timer();
        uint64_t OSStart = perf_read_os_timer();
        uint64_t OSEnd = 0;
        uint64_t OSElapsed = 0;
        uint64_t OSWaitTime = OSFreq * MillisecondsToWait / 1000;
        while (OSElapsed < OSWaitTime) {
                OSEnd = perf_read_os_timer();
                OSElapsed = OSEnd - OSStart;
        }

        uint64_t CPUEnd = perf_read_cpu_timer();
        uint64_t CPUElapsed = CPUEnd - CPUStart;

        uint64_t CPUFreq = 0;
        if (OSElapsed) {
                CPUFreq = OSFreq * CPUElapsed / OSElapsed;
        }

        return CPUFreq;
}
