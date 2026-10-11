#include "qemu/osdep.h"

#include "qemu.h"
#include "exec/exec-all.h"
#include "exec/tb-context.h"
#include "exec/tb-hash.h"
#ifdef CONFIG_LATX_FAST_JMPCACHE
#include "exec/fasttb.h"
#endif
#include "latc-aot-v2-runner.h"
#include "latc-bundle-loader.h"
#include "latc-build-id.h"
#include "latcd-client.h"
#include "latcd-protocol.h"
#include "auto-config.h"
#include "lat-tb-key-set.h"
#include "guest-elf-map.h"
#include "module-loader.h"
#include "qemu-def.h"
#include "translate.h"

#include <elf.h>
#include <glib.h>
#include <math.h>

/* Loongnix 20 predates the public LoongArch HWCAP names. */
#ifndef HWCAP_LOONGARCH_LSX
#define HWCAP_LOONGARCH_LSX (1UL << 4)
#endif
#ifndef HWCAP_LOONGARCH_LASX
#define HWCAP_LOONGARCH_LASX (1UL << 5)
#endif
#ifndef HWCAP_LOONGARCH_LBT_X86
#define HWCAP_LOONGARCH_LBT_X86 (1UL << 10)
#endif

typedef struct LatAotV2RuntimeModule {
    LatAotLoadedModuleV2 loaded;
    char *path;
    bool legacy_fast_cache;
    struct LatAotV2RuntimeModule *next;
} LatAotV2RuntimeModule;

typedef enum LatAotV2ModuleState {
    LAT_AOT_V2_MODULE_MISSING,
    LAT_AOT_V2_MODULE_REJECTED,
    LAT_AOT_V2_MODULE_REGISTERED,
    LAT_AOT_V2_MODULE_INACTIVE,
} LatAotV2ModuleState;

typedef struct LatAotV2ModuleStats {
    uint8_t source_sha256[32];
    _Atomic uint64_t guest_begin;
    _Atomic uint64_t guest_end;
    _Atomic uint64_t source_base;
    _Atomic LatAotV2ModuleState state;
    _Atomic uint64_t aot_lookups;
    _Atomic uint64_t jit_fallbacks;
    _Atomic uint64_t registration_ns;
    _Atomic size_t live_instances;
    int source_fd;
    GHashTable *jit_tbset;
    GHashTable *pending_tbset;
    struct LatAotV2ModuleStats *next;
} LatAotV2ModuleStats;

typedef struct LatAotV2TbsetEntry {
    uint64_t rva;
    uint32_t flags;
} LatAotV2TbsetEntry;

typedef struct LatAotV2RuntimeInstance {
    LatAotModuleInstanceV2 instance;
    LatAotV2RuntimeModule *runtime_module;
    LatAotV2ModuleStats *stats;
    uint64_t guest_slots[LAT_AOT_V2_CONTEXT_GUEST_SLOT_LIMIT];
    uint64_t *guest_pages;
    size_t guest_page_storage_count;
    uint64_t guest_slot_count;
    uint64_t handled_invalidation_generation;
    struct LatAotV2RuntimeInstance *next;
} LatAotV2RuntimeInstance;

typedef struct LatAotV2HostModuleSnapshot {
    size_t count;
    LatAotV2RuntimeModule **modules;
    struct LatAotV2HostModuleSnapshot *retired_next;
} LatAotV2HostModuleSnapshot;

#define LAT_AOT_V2_TARGET_CACHE_BITS 16
#define LAT_AOT_V2_TARGET_CACHE_SIZE (1u << LAT_AOT_V2_TARGET_CACHE_BITS)

typedef struct LatAotV2TargetCacheEntry {
    target_ulong guest_pc;
    uint32_t cflags;
    uint64_t generation;
    const void *host_address;
    LatAotModuleInstanceV2 *instance;
} LatAotV2TargetCacheEntry;

static LatAotRegistryV2 registry;
static LatAotV2RuntimeModule *runtime_modules;
static LatAotV2RuntimeInstance *runtime_instances;
static LatAotV2RuntimeInstance *runtime_instances_retired;
static LatAotV2RuntimeInstance *runtime_instances_free;
static _Atomic size_t runtime_instance_live_count;
static _Atomic size_t runtime_instance_retired_count;
static _Atomic size_t runtime_instance_free_count;
static _Atomic size_t runtime_instance_allocated_count;
static _Atomic(LatAotV2HostModuleSnapshot *) host_modules_current;
static LatAotV2HostModuleSnapshot *host_modules_retired;
static _Atomic size_t host_module_readers;
static _Atomic size_t host_modules_retired_count;
static _Atomic(LatAotV2ModuleStats *) module_stats;
static _Atomic size_t module_stats_count;
static bool prepared;
static _Atomic bool active;
/* Invalidates cached registry misses when module membership changes. */
static _Atomic uint64_t registry_generation = 1;
static const char tracked_nonfile_miss;
static bool registry_initialized;
static bool runtime_bound;
static LatGuestElfTrackerV2 *elf_tracker;
static pthread_mutex_t elf_tracker_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tbset_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t submission_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t submission_control_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t submission_control_cond = PTHREAD_COND_INITIALIZER;
static uint64_t submission_generation;
static bool submission_thread_started;
typedef struct LatAotV2PendingMapping {
    int fd;
    uint64_t guest_start;
    uint64_t mapping_size;
    uint64_t file_offset;
    struct LatAotV2PendingMapping *next;
} LatAotV2PendingMapping;
typedef struct LatAotV2SourceMapping {
    int fd;
    uint64_t guest_start;
    uint64_t mapping_size;
    uint64_t file_offset;
    struct LatAotV2SourceMapping *next;
} LatAotV2SourceMapping;
static LatAotV2PendingMapping *pending_mapping_head;
static LatAotV2PendingMapping **pending_mapping_tail = &pending_mapping_head;
static LatAotV2SourceMapping *source_mappings;
static __thread LatAotModuleInstanceV2 *aot_v2_current_instance;
static __thread uint64_t aot_v2_current_generation;
static __thread bool aot_v2_execution_active;
static __thread LatAotModuleInstanceV2 *aot_v2_execution_instance;
static __thread LatAotV2TargetCacheEntry *aot_v2_target_cache;
static _Atomic uint64_t direct_targets;
static _Atomic uint64_t file_dispatch_misses;
static _Atomic uint64_t nonfile_dispatch_misses;
static _Atomic uint64_t traced_dispatch_misses;
static _Atomic int dispatch_miss_tracking = -1;
static _Atomic uint64_t compiler_submissions;
static _Atomic uint64_t compiler_submission_failures;
static _Atomic uint64_t compiler_submission_duplicates;
static _Atomic uint64_t compiler_submission_throttled;
static _Atomic uint64_t compiler_request_sequence;
static _Atomic uint64_t precompile_requests;
static _Atomic uint64_t precompile_successes;
static _Atomic uint64_t precompile_failures;
static _Atomic uint64_t precompile_request_sequence;
static bool aot_v2_strict;
static bool aot_v2_reject_miss;
/* Collect keys without loading provisional modules into the live process. */
static bool aot_v2_collect_only;

void latc_aot_v2_consume_environment(void)
{
    aot_v2_strict = getenv("LATX_AOT_V2_STRICT") != NULL;
    aot_v2_reject_miss = getenv("LATC_STRICT_AOT") != NULL;
    aot_v2_collect_only = getenv("LATX_AOT_V2_COLLECT_ONLY") != NULL;
}

bool latc_aot_v2_strict_enabled(void)
{
    return aot_v2_strict;
}

static uint32_t aot_v2_semantic_flags(uint32_t cflags)
{
    (void)cflags;
    return LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL;
}
static _Atomic uint64_t invalidated_instances;
static _Atomic uint64_t invalidated_exec_ranges;
static _Atomic uint64_t revalidated_instances;
static _Atomic uint64_t revalidation_failures;

static bool submit_runtime_tbsets(void);
static void schedule_runtime_tbset_submission(void);

static bool next_compiler_request_id(uint64_t *request_id)
{
    uint64_t sequence = atomic_fetch_add(&compiler_request_sequence, 1) + 1;
    const char *text = getenv("LATX_AOT_V2_MAX_SUBMISSIONS");
    if (text && *text) {
        char *end = NULL;
        errno = 0;
        unsigned long long limit = strtoull(text, &end, 10);
        if (!errno && end && !*end && sequence > limit) {
            atomic_fetch_add(&compiler_submission_throttled, 1);
            return false;
        }
    }
    *request_id = ((uint64_t)getpid() << 32) ^ sequence;
    return true;
}

static _Atomic uint64_t invalidation_reasons[4];
static _Atomic uint64_t signal_pc_lookups;
static _Atomic uint64_t signal_pc_hits;
static _Atomic uint64_t signal_pc_misses;

typedef struct LatAotV2SignalRecovery {
    uintptr_t host_pc;
    target_ulong guest_begin;
    target_ulong guest_end;
    int32_t stack_pointer_delta;
    bool invalidate_all;
} LatAotV2SignalRecovery;

static _Thread_local LatAotV2SignalRecovery signal_recovery;
static _Thread_local volatile sig_atomic_t signal_recovery_pending;
static _Atomic uint64_t signal_invalidation_overlaps;
static _Atomic uint64_t signal_last_guest_pc;
static _Atomic uint64_t signal_last_generation;
static _Atomic uint64_t signal_last_guest_begin;
static _Atomic uint64_t signal_last_guest_end;
static _Atomic uint64_t signal_last_source[4];
static _Atomic(LatAotModuleInstanceV2 *) signal_invalidation_test_instance;
static _Atomic unsigned int signal_invalidation_test_state;
static bool signal_invalidation_test;
static bool signal_invalidation_test_worker_started;

typedef struct LatAotV2SignalDebugSnapshot {
    const void *instance;
    const void *instance_module;
    const void *runtime_module;
    const void *runtime_loaded;
    uintptr_t instance_text_begin;
    uintptr_t instance_text_end;
    uintptr_t runtime_text_begin;
    uintptr_t runtime_text_end;
    uint64_t guest_begin;
    uint64_t guest_end;
    uint64_t generation;
    uint32_t readers;
    int active;
    bool execution_active;
    bool module_match;
} LatAotV2SignalDebugSnapshot;

static bool signal_diagnostic_enabled(void)
{
    return getenv("LATX_AOT_V2_REPORT") ||
           getenv("LATX_AOT_V2_TEST_SIGNAL_DIAGNOSTIC");
}

static LatAotV2SignalDebugSnapshot capture_signal_debug_snapshot(
    LatAotModuleInstanceV2 *instance,
    LatAotV2RuntimeModule *runtime_module)
{
    const LatAotLoadedModuleV2 *instance_module =
        instance ? instance->module : NULL;
    const LatAotModuleV2 *instance_descriptor =
        instance_module ? instance_module->descriptor : NULL;
    const LatAotModuleV2 *runtime_descriptor =
        runtime_module ? runtime_module->loaded.descriptor : NULL;

    return (LatAotV2SignalDebugSnapshot) {
        .instance = instance,
        .instance_module = instance_module,
        .runtime_module = runtime_module,
        .runtime_loaded = runtime_module ? &runtime_module->loaded : NULL,
        .instance_text_begin = instance_descriptor ?
            (uintptr_t)instance_descriptor->text_begin : 0,
        .instance_text_end = instance_descriptor ?
            (uintptr_t)instance_descriptor->text_end : 0,
        .runtime_text_begin = runtime_descriptor ?
            (uintptr_t)runtime_descriptor->text_begin : 0,
        .runtime_text_end = runtime_descriptor ?
            (uintptr_t)runtime_descriptor->text_end : 0,
        .guest_begin = instance ? instance->guest_begin : 0,
        .guest_end = instance ? instance->guest_end : 0,
        .generation = instance ? atomic_load_explicit(
            &instance->generation, memory_order_acquire) : 0,
        .readers = instance ? atomic_load_explicit(
            &instance->readers, memory_order_acquire) : 0,
        .active = instance ? atomic_load_explicit(
            &instance->active, memory_order_acquire) : 0,
        .execution_active = aot_v2_execution_active,
        .module_match = instance_module && runtime_module &&
            instance_module == &runtime_module->loaded,
    };
}

static void report_signal_debug_snapshot(
    const char *event, const char *reason, uintptr_t host_pc,
    uintptr_t searched_pc, bool diagnostic_reader_held,
    const LatAotV2SignalDebugSnapshot *snapshot)
{
    fprintf(stderr,
            "latx: AOT v2 signal %s reason=%s host_pc=0x%llx "
            "searched_pc=0x%llx execution_active=%d instance=%p "
            "instance_module=%p instance_active=%d "
            "instance_generation=%llu instance_readers=%u "
            "diagnostic_reader_held=%d guest_range=0x%llx-0x%llx "
            "instance_text=0x%llx-0x%llx runtime_module=%p "
            "runtime_loaded=%p runtime_text=0x%llx-0x%llx "
            "module_match=%d\n",
            event ? event : "unknown", reason ? reason : "unknown",
            (unsigned long long)host_pc,
            (unsigned long long)searched_pc, snapshot->execution_active,
            (void *)snapshot->instance, (void *)snapshot->instance_module,
            snapshot->active, (unsigned long long)snapshot->generation,
            snapshot->readers, diagnostic_reader_held,
            (unsigned long long)snapshot->guest_begin,
            (unsigned long long)snapshot->guest_end,
            (unsigned long long)snapshot->instance_text_begin,
            (unsigned long long)snapshot->instance_text_end,
            (void *)snapshot->runtime_module,
            (void *)snapshot->runtime_loaded,
            (unsigned long long)snapshot->runtime_text_begin,
            (unsigned long long)snapshot->runtime_text_end,
            snapshot->module_match);
}

static uint64_t discovered_elfs;

void latc_aot_v2_fork_start(void)
{
    pthread_mutex_lock(&elf_tracker_lock);
    pthread_mutex_lock(&submission_control_lock);
    pthread_mutex_lock(&submission_lock);
    pthread_mutex_lock(&tbset_lock);
}

void latc_aot_v2_fork_end(CPUState *cpu, bool child)
{
    if (!child) {
        pthread_mutex_unlock(&tbset_lock);
        pthread_mutex_unlock(&submission_lock);
        pthread_mutex_unlock(&submission_control_lock);
        pthread_mutex_unlock(&elf_tracker_lock);
        return;
    }

    /*
     * fork_start holds both writer-side locks, so every immutable module,
     * registry snapshot and guest mapping inherited by the child is stable.
     * Reader counts can include threads that only exist in the parent; reset
     * those counts before allowing the single child thread to use AOT again.
     */
    if (registry_initialized) {
        atomic_store_explicit(&registry.readers, 0, memory_order_release);
    }
    atomic_store_explicit(&host_module_readers, 0, memory_order_release);
    for (LatAotV2RuntimeInstance *runtime = runtime_instances; runtime;
         runtime = runtime->next) {
        atomic_store_explicit(&runtime->instance.readers, 0,
                              memory_order_release);
    }
    for (LatAotV2RuntimeInstance *runtime = runtime_instances_retired; runtime;
         runtime = runtime->next) {
        atomic_store_explicit(&runtime->instance.readers, 0,
                              memory_order_release);
    }
    atomic_fetch_add_explicit(&registry_generation, 1, memory_order_release);
    if (getenv("LATX_AOT_V2_REPORT") &&
        atomic_load_explicit(&active, memory_order_acquire)) {
        static const char message[] =
            "latx: AOT v2 fork child retained AOT\n";
        ssize_t written = write(STDERR_FILENO, message, sizeof(message) - 1);
        (void)written;
    }
    aot_v2_current_instance = NULL;
    aot_v2_current_generation = 0;
    aot_v2_execution_active = false;
    aot_v2_execution_instance = NULL;
    aot_v2_target_cache = NULL;
    signal_recovery_pending = 0;
    submission_thread_started = false;
#ifdef CONFIG_LATX_FAST_JMPCACHE
    if (cpu) {
        latx_aot_v2_fast_jmp_cache_set_context(cpu, NULL);
        latx_fast_jmp_cache_clear_all(cpu);
    }
#else
    (void)cpu;
#endif

    pthread_mutex_unlock(&tbset_lock);
    pthread_mutex_unlock(&submission_lock);
    pthread_mutex_unlock(&submission_control_lock);
    pthread_mutex_unlock(&elf_tracker_lock);
}

enum {
    LAT_AOT_V2_SIGNAL_TEST_IDLE,
    LAT_AOT_V2_SIGNAL_TEST_READER,
    LAT_AOT_V2_SIGNAL_TEST_WRITER,
    LAT_AOT_V2_SIGNAL_TEST_DONE,
};

static int register_discovered_module(const LatGuestElfInfoV2 *info,
                                      LatAotV2RuntimeInstance **instance,
                                      char *error, size_t error_size);
static void digest_hex(const uint8_t digest[32], char output[65]);

static void recycle_retired_instances_locked(void);

static uint64_t available_aot_features(void)
{
    unsigned long hwcap = qemu_getauxval(AT_HWCAP);
    const char *override = getenv("LATX_AOT_V2_TEST_HWCAP");
    if (override && *override) {
        char *end = NULL;
        errno = 0;
        unsigned long parsed = strtoul(override, &end, 0);
        if (!errno && end && !*end) {
            hwcap = parsed;
        }
    }
    uint64_t features = 0;
    if (hwcap & HWCAP_LOONGARCH_LBT_X86) {
        features |= LAT_AOT_FEATURE_LBT;
    }
    if (hwcap & HWCAP_LOONGARCH_LSX) {
        features |= LAT_AOT_FEATURE_LSX;
    }
    if (hwcap & HWCAP_LOONGARCH_LASX) {
        features |= LAT_AOT_FEATURE_LASX;
    }
    return features;
}

static int compare_host_module(const void *left, const void *right)
{
    const LatAotV2RuntimeModule *a =
        *(LatAotV2RuntimeModule *const *)left;
    const LatAotV2RuntimeModule *b =
        *(LatAotV2RuntimeModule *const *)right;
    uintptr_t a_begin = (uintptr_t)a->loaded.descriptor->text_begin;
    uintptr_t b_begin = (uintptr_t)b->loaded.descriptor->text_begin;
    return a_begin < b_begin ? -1 : a_begin > b_begin;
}

static void free_host_module_snapshot(LatAotV2HostModuleSnapshot *snapshot)
{
    if (!snapshot) {
        return;
    }
    g_free(snapshot->modules);
    g_free(snapshot);
}

static void reclaim_host_module_snapshots(void)
{
    if (atomic_load_explicit(&host_module_readers,
                             memory_order_seq_cst)) {
        return;
    }
    LatAotV2HostModuleSnapshot *snapshot = host_modules_retired;
    host_modules_retired = NULL;
    atomic_store_explicit(&host_modules_retired_count, 0,
                          memory_order_release);
    while (snapshot) {
        LatAotV2HostModuleSnapshot *next = snapshot->retired_next;
        free_host_module_snapshot(snapshot);
        snapshot = next;
    }
}

static int register_host_module(LatAotV2RuntimeModule *module)
{
    LatAotV2HostModuleSnapshot *old = atomic_load_explicit(
        &host_modules_current, memory_order_acquire);
    size_t old_count = old ? old->count : 0;
    LatAotV2HostModuleSnapshot *replacement = g_new0(
        LatAotV2HostModuleSnapshot, 1);
    if (!replacement) {
        errno = ENOMEM;
        return -1;
    }
    replacement->modules = g_new0(LatAotV2RuntimeModule *, old_count + 1);
    if (!replacement->modules) {
        g_free(replacement);
        errno = ENOMEM;
        return -1;
    }
    replacement->count = old_count + 1;
    if (old_count) {
        memcpy(replacement->modules, old->modules,
               old_count * sizeof(*replacement->modules));
    }
    replacement->modules[old_count] = module;
    qsort(replacement->modules, replacement->count,
          sizeof(*replacement->modules), compare_host_module);
    for (size_t i = 1; i < replacement->count; i++) {
        const LatAotModuleV2 *previous =
            replacement->modules[i - 1]->loaded.descriptor;
        const LatAotModuleV2 *current =
            replacement->modules[i]->loaded.descriptor;
        if ((uintptr_t)previous->text_end >
            (uintptr_t)current->text_begin) {
            g_free(replacement->modules);
            g_free(replacement);
            errno = EEXIST;
            return -1;
        }
    }
    old = atomic_exchange_explicit(&host_modules_current, replacement,
                                   memory_order_seq_cst);
    if (old) {
        old->retired_next = host_modules_retired;
        host_modules_retired = old;
        atomic_fetch_add_explicit(&host_modules_retired_count, 1,
                                  memory_order_release);
    }
    reclaim_host_module_snapshots();
    return 0;
}

static LatAotV2RuntimeModule *find_host_module(uintptr_t host_pc)
{
    atomic_fetch_add_explicit(&host_module_readers, 1,
                              memory_order_seq_cst);
    LatAotV2HostModuleSnapshot *snapshot = atomic_load_explicit(
        &host_modules_current, memory_order_seq_cst);
    size_t left = 0;
    size_t right = snapshot ? snapshot->count : 0;
    while (left < right) {
        size_t middle = left + (right - left) / 2;
        const LatAotModuleV2 *module =
            snapshot->modules[middle]->loaded.descriptor;
        if ((uintptr_t)module->text_begin <= host_pc) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    if (!left) {
        atomic_fetch_sub_explicit(&host_module_readers, 1,
                                  memory_order_seq_cst);
        return NULL;
    }
    LatAotV2RuntimeModule *module = snapshot->modules[left - 1];
    if (host_pc >= (uintptr_t)module->loaded.descriptor->text_end) {
        module = NULL;
    }
    atomic_fetch_sub_explicit(&host_module_readers, 1,
                              memory_order_seq_cst);
    return module;
}

static const char *module_state_name(LatAotV2ModuleState state)
{
    switch (state) {
    case LAT_AOT_V2_MODULE_REGISTERED: return "registered";
    case LAT_AOT_V2_MODULE_INACTIVE: return "inactive";
    case LAT_AOT_V2_MODULE_REJECTED: return "rejected";
    default: return "missing";
    }
}

static const char *invalidation_reason_name(LatcAotV2InvalidationReason reason)
{
    switch (reason) {
    case LATC_AOT_V2_INVALIDATE_UNMAP: return "unmap";
    case LATC_AOT_V2_INVALIDATE_MAP_FIXED: return "map-fixed";
    case LATC_AOT_V2_INVALIDATE_PROTECTION: return "protection";
    case LATC_AOT_V2_INVALIDATE_CODE_WRITE: return "code-write";
    default: return "unknown";
    }
}

void latc_aot_v2_report_stats(void)
{
    static _Atomic bool reported;
    if (atomic_exchange(&reported, true)) {
        return;
    }
    latc_bundle_flush_stats();
    submit_runtime_tbsets();
    if (!getenv("LATX_AOT_V2_REPORT")) return;
    LatAotRegistryCountsV2 registry_counts = {0};
    if (registry_initialized) {
        pthread_mutex_lock(&elf_tracker_lock);
        recycle_retired_instances_locked();
        reclaim_host_module_snapshots();
        pthread_mutex_unlock(&elf_tracker_lock);
        lat_aot_v2_registry_counts(&registry, &registry_counts);
    }
    fprintf(stderr,
            "latx: AOT v2 runtime stats direct_targets=%llu "
            "compat_tb_allocations=0 file_dispatch_misses=%llu "
            "nonfile_dispatch_misses=%llu compiler_submissions=%llu "
            "compiler_submission_failures=%llu "
            "compiler_submission_duplicates=%llu "
            "compiler_submission_throttled=%llu "
            "precompile_requests=%llu precompile_successes=%llu "
            "precompile_failures=%llu "
            "invalidated_instances=%llu invalidated_exec_ranges=%llu "
            "revalidated_instances=%llu revalidation_failures=%llu "
            "invalidation_unmap=%llu "
            "invalidation_map_fixed=%llu invalidation_protection=%llu "
            "invalidation_code_write=%llu signal_pc_lookups=%llu "
            "signal_pc_hits=%llu signal_pc_misses=%llu "
            "signal_invalidation_overlaps=%llu "
            "instance_live=%zu instance_retired=%zu instance_free=%zu "
            "instance_allocated=%zu registry_retired=%zu "
            "host_index_retired=%zu module_stats=%zu host_features=0x%llx\n",
            (unsigned long long)atomic_load(&direct_targets),
            (unsigned long long)atomic_load(&file_dispatch_misses),
            (unsigned long long)atomic_load(&nonfile_dispatch_misses),
            (unsigned long long)atomic_load(&compiler_submissions),
            (unsigned long long)atomic_load(&compiler_submission_failures),
            (unsigned long long)atomic_load(&compiler_submission_duplicates),
            (unsigned long long)atomic_load(&compiler_submission_throttled),
            (unsigned long long)atomic_load(&precompile_requests),
            (unsigned long long)atomic_load(&precompile_successes),
            (unsigned long long)atomic_load(&precompile_failures),
            (unsigned long long)atomic_load(&invalidated_instances),
            (unsigned long long)atomic_load(&invalidated_exec_ranges),
            (unsigned long long)atomic_load(&revalidated_instances),
            (unsigned long long)atomic_load(&revalidation_failures),
            (unsigned long long)atomic_load(
                &invalidation_reasons[LATC_AOT_V2_INVALIDATE_UNMAP]),
            (unsigned long long)atomic_load(
                &invalidation_reasons[LATC_AOT_V2_INVALIDATE_MAP_FIXED]),
            (unsigned long long)atomic_load(
                &invalidation_reasons[LATC_AOT_V2_INVALIDATE_PROTECTION]),
            (unsigned long long)atomic_load(
                &invalidation_reasons[LATC_AOT_V2_INVALIDATE_CODE_WRITE]),
            (unsigned long long)atomic_load(&signal_pc_lookups),
            (unsigned long long)atomic_load(&signal_pc_hits),
            (unsigned long long)atomic_load(&signal_pc_misses),
            (unsigned long long)atomic_load(&signal_invalidation_overlaps),
            atomic_load_explicit(&runtime_instance_live_count,
                                 memory_order_acquire),
            atomic_load_explicit(&runtime_instance_retired_count,
                                 memory_order_acquire),
            atomic_load_explicit(&runtime_instance_free_count,
                                 memory_order_acquire),
            atomic_load_explicit(&runtime_instance_allocated_count,
                                 memory_order_acquire),
            registry_counts.retired_snapshots,
            atomic_load_explicit(&host_modules_retired_count,
                                 memory_order_acquire),
            atomic_load_explicit(&module_stats_count,
                                 memory_order_acquire),
            (unsigned long long)available_aot_features());
    if (atomic_load_explicit(&signal_last_generation,
                             memory_order_acquire)) {
        char source[65];
        uint64_t source_words[4];
        for (size_t i = 0; i < 4; i++) {
            source_words[i] = atomic_load_explicit(&signal_last_source[i],
                                                   memory_order_acquire);
        }
        digest_hex((const uint8_t *)source_words, source);
        fprintf(stderr,
                "latx: AOT v2 signal diagnostic source=%s generation=%llu "
                "guest_pc=0x%llx range=0x%llx-0x%llx\n",
                source,
                (unsigned long long)atomic_load(&signal_last_generation),
                (unsigned long long)atomic_load(&signal_last_guest_pc),
                (unsigned long long)atomic_load(&signal_last_guest_begin),
                (unsigned long long)atomic_load(&signal_last_guest_end));
    }
    for (LatAotV2ModuleStats *stats = atomic_load_explicit(
             &module_stats, memory_order_acquire); stats;
         stats = stats->next) {
        char source[65];
        digest_hex(stats->source_sha256, source);
        fprintf(stderr,
                "latx: AOT v2 module stats source=%s range=0x%llx-0x%llx "
                "module=%s aot_lookups=%llu jit_fallbacks=%llu "
                "registration_ns=%llu\n",
                source,
                (unsigned long long)atomic_load(&stats->guest_begin),
                (unsigned long long)atomic_load(&stats->guest_end),
                module_state_name(atomic_load(&stats->state)),
                (unsigned long long)atomic_load(&stats->aot_lookups),
                (unsigned long long)atomic_load(&stats->jit_fallbacks),
                (unsigned long long)atomic_load(&stats->registration_ns));
    }
}

void latc_aot_v2_flush_pending_keys(void)
{
    submit_runtime_tbsets();
}

static int relocate_source_fd(int *owned_fd, int guest_fd)
{
    if (*owned_fd != guest_fd) {
        return 0;
    }
    int replacement = fcntl(guest_fd, F_DUPFD_CLOEXEC, 3);
    if (replacement < 0) {
        return -1;
    }
    *owned_fd = replacement;
    return 0;
}

int latc_aot_v2_relocate_source_fd(int fd)
{
    int result = 0;
    int saved_errno;
    bool submission_locked = false;

    if (fd < 0) {
        return 0;
    }
retry:
    pthread_mutex_lock(&elf_tracker_lock);
    for (LatAotV2ModuleStats *stats = atomic_load_explicit(
             &module_stats, memory_order_acquire); stats; stats = stats->next) {
        if (stats->source_fd != fd) {
            continue;
        }
        if (!submission_locked) {
            /*
             * Never hold the tracker lock while waiting for submission I/O.
             * Recheck ownership after waiting, preserving fork's lock order.
             */
            if (pthread_mutex_trylock(&submission_lock)) {
                pthread_mutex_unlock(&elf_tracker_lock);
                pthread_mutex_lock(&submission_lock);
                pthread_mutex_unlock(&submission_lock);
                goto retry;
            }
            submission_locked = true;
        }
        if (relocate_source_fd(&stats->source_fd, fd)) {
            result = -1;
            goto out;
        }
    }
    for (LatAotV2PendingMapping *pending = pending_mapping_head; pending;
         pending = pending->next) {
        if (relocate_source_fd(&pending->fd, fd)) {
            result = -1;
            goto out;
        }
    }
    for (LatAotV2SourceMapping *source = source_mappings; source;
         source = source->next) {
        if (relocate_source_fd(&source->fd, fd)) {
            result = -1;
            goto out;
        }
    }
out:
    saved_errno = errno;
    if (submission_locked) {
        pthread_mutex_unlock(&submission_lock);
    }
    pthread_mutex_unlock(&elf_tracker_lock);
    errno = saved_errno;
    return result;
}

static guint tbset_entry_hash(gconstpointer value)
{
    const LatAotV2TbsetEntry *entry = value;
    uint64_t mixed = entry->rva ^ ((uint64_t)entry->flags << 32);
    return (guint)(mixed ^ (mixed >> 32));
}

static gboolean tbset_entry_equal(gconstpointer left, gconstpointer right)
{
    const LatAotV2TbsetEntry *a = left;
    const LatAotV2TbsetEntry *b = right;
    return a->rva == b->rva && a->flags == b->flags;
}

static LatAotV2ModuleStats *add_module_stats(
    const LatGuestElfInfoV2 *info, LatAotV2ModuleState state, int source_fd)
{
    for (LatAotV2ModuleStats *stats = atomic_load_explicit(
             &module_stats, memory_order_acquire); stats;
         stats = stats->next) {
        if (!memcmp(stats->source_sha256, info->source_sha256,
                    sizeof(stats->source_sha256))) {
            atomic_store_explicit(&stats->guest_begin, info->guest_begin,
                                  memory_order_release);
            atomic_store_explicit(&stats->guest_end, info->guest_end,
                                  memory_order_release);
            atomic_store_explicit(&stats->source_base,
                                  info->load_bias + info->preferred_base,
                                  memory_order_release);
            atomic_store_explicit(&stats->state, state,
                                  memory_order_release);
            if (stats->source_fd < 0 && source_fd >= 0) {
                stats->source_fd = fcntl(source_fd, F_DUPFD_CLOEXEC, 3);
            }
            return stats;
        }
    }
    LatAotV2ModuleStats *stats = g_new0(LatAotV2ModuleStats, 1);
    if (!stats) {
        return NULL;
    }
    stats->source_fd = source_fd >= 0 ?
        fcntl(source_fd, F_DUPFD_CLOEXEC, 3) : -1;
    stats->jit_tbset = g_hash_table_new_full(tbset_entry_hash,
                                             tbset_entry_equal,
                                             g_free, NULL);
    stats->pending_tbset = g_hash_table_new_full(tbset_entry_hash,
                                                 tbset_entry_equal,
                                                 g_free, NULL);
    memcpy(stats->source_sha256, info->source_sha256,
           sizeof(stats->source_sha256));
    atomic_store_explicit(&stats->guest_begin, info->guest_begin,
                          memory_order_relaxed);
    atomic_store_explicit(&stats->guest_end, info->guest_end,
                          memory_order_relaxed);
    atomic_store_explicit(&stats->source_base,
                          info->load_bias + info->preferred_base,
                          memory_order_relaxed);
    atomic_store_explicit(&stats->state, state, memory_order_relaxed);
    stats->next = atomic_load_explicit(&module_stats, memory_order_relaxed);
    atomic_store_explicit(&module_stats, stats, memory_order_release);
    atomic_fetch_add_explicit(&module_stats_count, 1,
                              memory_order_release);
    return stats;
}

static LatAotV2ModuleStats *module_stats_for_pc(uint64_t guest_pc)
{
    for (LatAotV2ModuleStats *stats = atomic_load_explicit(
             &module_stats, memory_order_acquire); stats;
         stats = stats->next) {
        uint64_t begin = atomic_load_explicit(&stats->guest_begin,
                                              memory_order_acquire);
        uint64_t end = atomic_load_explicit(&stats->guest_end,
                                            memory_order_acquire);
        if (guest_pc >= begin && guest_pc < end) {
            return stats;
        }
    }
    return NULL;
}

static bool dispatch_miss_tracking_enabled(void)
{
    int enabled = atomic_load_explicit(&dispatch_miss_tracking,
                                       memory_order_acquire);
    if (enabled >= 0) {
        return enabled;
    }
    enabled = getenv("LATX_AOT_V2_REPORT") != NULL;
    atomic_store_explicit(&dispatch_miss_tracking, enabled,
                          memory_order_release);
    return enabled;
}

bool latc_aot_v2_is_file_pc(target_ulong guest_pc)
{
    return module_stats_for_pc(guest_pc) != NULL;
}

static void note_dispatch_miss(LatAotV2ModuleStats *stats,
                               uint64_t guest_pc, uint32_t cflags)
{
    /* Strict verification rejects an actual generated JIT TB in
     * latc_bundle_note_tb_generated().  Do not count decode attempts that
     * produce no code as fallbacks. */
    if (aot_v2_strict) {
        return;
    }
    if (stats) {
        atomic_fetch_add(&stats->jit_fallbacks, 1);
        atomic_fetch_add(&file_dispatch_misses, 1);
        uint64_t source_base = atomic_load_explicit(
            &stats->source_base, memory_order_acquire);
        uint64_t rva = guest_pc - source_base;
        uint32_t semantic_flags = aot_v2_semantic_flags(cflags);
        uint64_t trace_index = atomic_fetch_add(&traced_dispatch_misses, 1);
        if (trace_index < 1024 && getenv("LATX_AOT_V2_TRACE_MISSES")) {
            char source[65];
            digest_hex(stats->source_sha256, source);
            fprintf(stderr,
                    "latx: AOT v2 file miss source=%s rva=0x%llx "
                    "flags=0x%x guest_pc=0x%llx\n",
                    source, (unsigned long long)rva,
                    semantic_flags,
                    (unsigned long long)guest_pc);
        }
    } else {
        atomic_fetch_add(&nonfile_dispatch_misses, 1);
    }
}

bool latc_aot_v2_mapping_enabled(void)
{
    const char *cache = lat_aot_cache_path();
    const char *module = lat_aot_module_path();
    const char *socket = lat_aot_socket_path();
    return (cache && *cache) || (module && *module) || (socket && *socket);
}

static bool note_jit_key_locked(target_ulong guest_pc, uint32_t cflags)
{
    if (cflags & CF_INVALID) {
        return false;
    }
    LatAotV2ModuleStats *stats = module_stats_for_pc(guest_pc);
    if (!stats || !stats->jit_tbset ||
        g_hash_table_size(stats->jit_tbset) >= LAT_AOT_V2_TBSET_RECORD_LIMIT) {
        return false;
    }
    uint64_t source_base = atomic_load_explicit(&stats->source_base,
                                                memory_order_acquire);
    if (guest_pc < source_base) {
        return false;
    }
    LatAotV2TbsetEntry candidate = {
        .rva = guest_pc - source_base,
        .flags = aot_v2_semantic_flags(cflags),
    };
    if (g_hash_table_contains(stats->jit_tbset, &candidate)) {
        return false;
    }
    LatAotV2TbsetEntry *entry = g_new(LatAotV2TbsetEntry, 1);
    if (!entry) {
        return false;
    }
    *entry = candidate;
    g_hash_table_add(stats->jit_tbset, entry);
    LatAotV2TbsetEntry *pending = g_new(LatAotV2TbsetEntry, 1);
    if (!pending) {
        return false;
    }
    *pending = candidate;
    g_hash_table_add(stats->pending_tbset, pending);
    return true;
}

void latc_aot_v2_note_jit_key(target_ulong guest_pc, uint32_t cflags)
{
    const char *socket = lat_aot_socket_path();
    if (!socket || !*socket) {
        return;
    }
    pthread_mutex_lock(&tbset_lock);
    bool added = note_jit_key_locked(guest_pc, cflags);
    pthread_mutex_unlock(&tbset_lock);
    if (added) {
        schedule_runtime_tbset_submission();
    }
}

static int tbset_entry_compare(gconstpointer left, gconstpointer right)
{
    const LatAotV2TbsetEntry *a = left;
    const LatAotV2TbsetEntry *b = right;
    if (a->rva != b->rva) return a->rva < b->rva ? -1 : 1;
    if (a->flags != b->flags) return a->flags < b->flags ? -1 : 1;
    return 0;
}

static bool submit_runtime_tbsets(void)
{
    const char *socket = lat_aot_socket_path();
    if (!socket || !*socket) return true;
    pthread_mutex_lock(&submission_lock);
    bool pending = false;
    pthread_mutex_lock(&tbset_lock);
    for (LatAotV2ModuleStats *stats = atomic_load_explicit(
             &module_stats, memory_order_acquire); stats; stats = stats->next) {
        if (stats->source_fd >= 0 && stats->pending_tbset &&
            g_hash_table_size(stats->pending_tbset)) {
            pending = true;
            break;
        }
    }
    pthread_mutex_unlock(&tbset_lock);
    if (!pending) {
        pthread_mutex_unlock(&submission_lock);
        return true;
    }
    if (!lat_aot_ensure_daemon()) {
        atomic_fetch_add(&compiler_submission_failures, 1);
        pthread_mutex_unlock(&submission_lock);
        return false;
    }
    bool submitted = true;
    for (LatAotV2ModuleStats *stats = atomic_load_explicit(
             &module_stats, memory_order_acquire); stats;
         stats = stats->next) {
        pthread_mutex_lock(&tbset_lock);
        guint count = stats->pending_tbset ?
                      g_hash_table_size(stats->pending_tbset) : 0;
        GArray *entries = count ? g_array_sized_new(
            FALSE, FALSE, sizeof(LatAotV2TbsetEntry), count) : NULL;
        if (entries) {
            GHashTableIter iterator;
            gpointer key;
            g_hash_table_iter_init(&iterator, stats->pending_tbset);
            while (g_hash_table_iter_next(&iterator, &key, NULL)) {
                g_array_append_val(entries, *(LatAotV2TbsetEntry *)key);
            }
        }
        pthread_mutex_unlock(&tbset_lock);
        if (!entries || stats->source_fd < 0) {
            if (entries) g_array_free(entries, TRUE);
            continue;
        }
        g_array_sort(entries, tbset_entry_compare);
        char source[65];
        digest_hex(stats->source_sha256, source);
        gchar *path = NULL;
        GError *gerror = NULL;
        int output_fd = g_file_open_tmp("latc-tbset-XXXXXX", &path, &gerror);
        LatTbKeySet key_set = { .count = entries->len };
        memcpy(key_set.source_sha256, stats->source_sha256, 32);
        key_set.keys = g_new0(LatTbKey, key_set.count);
        for (guint i = 0; i < entries->len; i++) {
            const LatAotV2TbsetEntry *entry = &g_array_index(
                entries, LatAotV2TbsetEntry, i);
            key_set.keys[i].guest_rva = entry->rva;
            key_set.keys[i].flags = entry->flags;
        }
        int failed = output_fd < 0 || !key_set.keys;
        if (!failed && (lat_tb_key_set_write_fd(
                            output_fd, &key_set, NULL, 0) ||
                        fsync(output_fd))) {
            failed = 1;
        }
        if (output_fd >= 0 && close(output_fd)) failed = 1;
        g_free(key_set.keys);
        int tbset_fd = !failed ? open(path, O_RDONLY | O_CLOEXEC) : -1;
        if (tbset_fd >= 0) unlink(path);
        uint64_t request_id;
        if (!next_compiler_request_id(&request_id)) {
            if (tbset_fd >= 0) close(tbset_fd);
            if (path) {
                unlink(path);
                g_free(path);
            }
            g_clear_error(&gerror);
            g_array_free(entries, TRUE);
            continue;
        }
        char error[256] = {0};
        if (tbset_fd < 0 || latcd_client_submit_keys_fd(
                socket, stats->source_fd, tbset_fd,
                LATCD_PRIORITY_LIBRARY, request_id, request_id,
                error, sizeof(error))) {
            atomic_fetch_add(&compiler_submission_failures, 1);
            submitted = false;
            if (getenv("LATX_AOT_V2_REPORT")) {
                fprintf(stderr,
                        "latx: AOT v2 TB set submission failed source=%s: %s\n",
                        source, error[0] ? error : strerror(errno));
            }
        } else {
            atomic_fetch_add(&compiler_submissions, 1);
            pthread_mutex_lock(&tbset_lock);
            for (guint i = 0; i < entries->len; i++) {
                g_hash_table_remove(stats->pending_tbset, &g_array_index(
                    entries, LatAotV2TbsetEntry, i));
            }
            pthread_mutex_unlock(&tbset_lock);
            if (getenv("LATX_AOT_V2_REPORT")) {
                fprintf(stderr,
                        "latx: AOT v2 TB set submitted source=%s keys=%u\n",
                        source, count);
            }
        }
        if (tbset_fd >= 0) close(tbset_fd);
        if (path) {
            unlink(path);
            g_free(path);
        }
        g_clear_error(&gerror);
        g_array_free(entries, TRUE);
    }
    pthread_mutex_unlock(&submission_lock);
    return submitted;
}

static void *runtime_tbset_submission_thread(void *opaque)
{
    (void)opaque;
    uint64_t handled = 0;
    pthread_mutex_lock(&submission_control_lock);
    for (;;) {
        while (submission_generation == handled) {
            pthread_cond_wait(&submission_control_cond,
                              &submission_control_lock);
        }
        uint64_t target = submission_generation;
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_nsec += 100 * 1000 * 1000;
        if (deadline.tv_nsec >= 1000 * 1000 * 1000) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000 * 1000 * 1000;
        }
        while (pthread_cond_timedwait(&submission_control_cond,
                                      &submission_control_lock,
                                      &deadline) != ETIMEDOUT) {
            target = submission_generation;
        }
        pthread_mutex_unlock(&submission_control_lock);
        bool submitted = submit_runtime_tbsets();
        handled = target;
        pthread_mutex_lock(&submission_control_lock);
        if (!submitted && submission_generation == handled) {
            clock_gettime(CLOCK_REALTIME, &deadline);
            deadline.tv_sec += 5;
            pthread_cond_timedwait(&submission_control_cond,
                                  &submission_control_lock, &deadline);
            handled--;
        }
    }
    return NULL;
}

static void schedule_runtime_tbset_submission(void)
{
    pthread_mutex_lock(&submission_control_lock);
    submission_generation++;
    if (!submission_thread_started) {
        pthread_t thread;
        if (!pthread_create(&thread, NULL,
                            runtime_tbset_submission_thread, NULL)) {
            submission_thread_started = true;
            pthread_detach(thread);
        }
    }
    pthread_cond_signal(&submission_control_cond);
    pthread_mutex_unlock(&submission_control_lock);
}

static bool precompile_enabled(void)
{
    const char *value = getenv("LATX_AOT_V2_PRECOMPILE");
    return value && !strcmp(value, "1");
}

static int precompile_source(int source_fd, char *error, size_t error_size)
{
    pthread_mutex_lock(&submission_lock);
    bool ready = lat_aot_ensure_daemon();
    pthread_mutex_unlock(&submission_lock);
    if (!ready) {
        snprintf(error, error_size, "latcd unavailable for precompile");
        return -1;
    }
    const char *socket = lat_aot_socket_path();
    const char *cache = lat_aot_cache_path();
    if (!socket || !*socket || !cache || !*cache) {
        snprintf(error, error_size,
                 "LATX_AOT_V2_PRECOMPILE requires cache and latcd socket");
        errno = EINVAL;
        return -1;
    }
    uint64_t sequence = atomic_fetch_add(&precompile_request_sequence, 1) + 1;
    uint64_t request_id = ((uint64_t)getpid() << 32) ^
                          UINT64_C(0x8000000000000000) ^ sequence;
    atomic_fetch_add(&precompile_requests, 1);
    if (latcd_client_precompile_source(socket, source_fd, request_id,
                                       error, error_size)) {
        atomic_fetch_add(&precompile_failures, 1);
        return -1;
    }
    atomic_fetch_add(&precompile_successes, 1);
    return 0;
}

static void drain_mappings(void)
{
    if (have_mmap_lock()) {
        return;
    }
    for (;;) {
        pthread_mutex_lock(&elf_tracker_lock);
        LatAotV2PendingMapping *pending = pending_mapping_head;
        if (!pending) {
            pthread_mutex_unlock(&elf_tracker_lock);
            break;
        }
        pending_mapping_head = pending->next;
        if (!pending_mapping_head) {
            pending_mapping_tail = &pending_mapping_head;
        }
        if (!elf_tracker) {
            lat_guest_elf_set_cache_directory_v2(lat_aot_cache_path());
            elf_tracker = lat_guest_elf_tracker_new_v2();
        }
        const LatGuestElfInfoV2 *info = NULL;
        int added = 0;
        char error[256] = {0};
        int result = !elf_tracker || lat_guest_elf_tracker_note_v2(
            elf_tracker, pending->fd, pending->guest_start,
            pending->mapping_size, pending->file_offset, TARGET_PAGE_SIZE,
            &info, &added, error, sizeof(error));
        if (!result && added) {
            discovered_elfs++;
            LatAotV2RuntimeInstance *instance = NULL;
            int64_t registration_start = g_get_monotonic_time();
            int registered = register_discovered_module(
                info, &instance, error, sizeof(error));
            if (!registered && precompile_enabled()) {
                if (!precompile_source(pending->fd, error, sizeof(error))) {
                    error[0] = '\0';
                    registered = register_discovered_module(
                        info, &instance, error, sizeof(error));
                }
            }
            uint64_t registration_ns =
                (g_get_monotonic_time() - registration_start) * 1000;
            LatAotV2ModuleStats *stats = add_module_stats(
                info, registered > 0 ? LAT_AOT_V2_MODULE_REGISTERED :
                registered == 0 ? LAT_AOT_V2_MODULE_MISSING :
                                  LAT_AOT_V2_MODULE_REJECTED,
                pending->fd);
            if (stats) {
                atomic_store_explicit(&stats->registration_ns,
                                      registration_ns,
                                      memory_order_release);
                latc_aot_v2_collect_existing_jit_tbs(info->guest_begin,
                                                      info->guest_end);
            }
            if (instance) {
                instance->stats = stats;
                if (stats) {
                    atomic_fetch_add_explicit(&stats->live_instances, 1,
                                              memory_order_release);
                }
            }
            if (getenv("LATX_AOT_V2_REPORT")) {
                fprintf(stderr,
                        "latx: AOT v2 discovered ELF dev=%llu ino=%llu "
                        "bias=0x%llx range=0x%llx-0x%llx exec_ranges=%u "
                        "module=%s%s%s\n",
                        (unsigned long long)info->device,
                        (unsigned long long)info->inode,
                        (unsigned long long)info->load_bias,
                        (unsigned long long)info->guest_begin,
                        (unsigned long long)info->guest_end,
                        info->exec_range_count,
                        registered > 0 ? "registered" :
                        registered == 0 ? "missing" : "rejected",
                        error[0] ? " reason=" : "",
                        error);
            }
        } else if (result && getenv("LATX_AOT_V2_REPORT")) {
            fprintf(stderr,
                    "latx: AOT v2 ignored file mapping "
                    "start=0x%llx size=0x%llx offset=0x%llx: %s\n",
                    (unsigned long long)pending->guest_start,
                    (unsigned long long)pending->mapping_size,
                    (unsigned long long)pending->file_offset,
                    error[0] ? error : strerror(errno));
        }
        close(pending->fd);
        free(pending);
        pthread_mutex_unlock(&elf_tracker_lock);
    }
}

void latc_aot_v2_drain_mmaps(void)
{
    drain_mappings();
}

void latc_aot_v2_note_mmap(int fd, uint64_t guest_start,
                           uint64_t mapping_size, uint64_t file_offset)
{
    if (!latc_aot_v2_mapping_enabled()) {
        close(fd);
        return;
    }
    LatAotV2PendingMapping *pending = g_new0(LatAotV2PendingMapping, 1);
    if (!pending) {
        close(fd);
        return;
    }
    *pending = (LatAotV2PendingMapping) {
        .fd = fd,
        .guest_start = guest_start,
        .mapping_size = mapping_size,
        .file_offset = file_offset,
    };
    LatAotV2SourceMapping *source = g_new0(LatAotV2SourceMapping, 1);
    if (source) {
        source->fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        if (source->fd < 0) {
            g_free(source);
            source = NULL;
        } else {
            source->guest_start = guest_start;
            source->mapping_size = mapping_size;
            source->file_offset = file_offset;
        }
    }
    pthread_mutex_lock(&elf_tracker_lock);
    if (source) {
        source->next = source_mappings;
        source_mappings = source;
    }
    *pending_mapping_tail = pending;
    pending_mapping_tail = &pending->next;
    pthread_mutex_unlock(&elf_tracker_lock);
    drain_mappings();
}

static void remove_source_mappings_locked(uint64_t guest_start,
                                          uint64_t guest_end)
{
    LatAotV2SourceMapping **link = &source_mappings;
    while (*link) {
        LatAotV2SourceMapping *source = *link;
        uint64_t source_end = source->guest_start + source->mapping_size;
        if (guest_start < source_end && guest_end > source->guest_start) {
            *link = source->next;
            close(source->fd);
            g_free(source);
        } else {
            link = &source->next;
        }
    }
}

static uint32_t instance_exec_range_overlap_count(
    const LatAotModuleInstanceV2 *instance, uint64_t guest_start,
    uint64_t guest_end)
{
    if (!instance->exec_range_count) {
        return guest_start < instance->guest_end &&
               guest_end > instance->guest_begin;
    }
    uint32_t overlaps = 0;
    for (uint32_t i = 0; i < instance->exec_range_count; i++) {
        if (guest_start < instance->exec_ranges[i].end &&
            guest_end > instance->exec_ranges[i].begin) {
            overlaps++;
        }
    }
    return overlaps;
}

bool latc_aot_v2_invalidate_range(CPUState *cpu, uint64_t guest_start,
                                  uint64_t mapping_size,
                                  LatcAotV2InvalidationReason reason)
{
    if (!mapping_size || guest_start > UINT64_MAX - mapping_size ||
        reason > LATC_AOT_V2_INVALIDATE_CODE_WRITE) {
        return false;
    }
    uint64_t guest_end = guest_start + mapping_size;
    bool current_invalidated = false;
    pthread_mutex_lock(&elf_tracker_lock);
    if (elf_tracker && reason != LATC_AOT_V2_INVALIDATE_CODE_WRITE &&
        reason != LATC_AOT_V2_INVALIDATE_PROTECTION) {
        lat_guest_elf_tracker_remove_range_v2(elf_tracker, guest_start,
                                              mapping_size);
    }
    if (reason != LATC_AOT_V2_INVALIDATE_CODE_WRITE &&
        reason != LATC_AOT_V2_INVALIDATE_PROTECTION) {
        remove_source_mappings_locked(guest_start, guest_end);
    }
    size_t deactivated = 0;
    size_t deactivated_exec_ranges = 0;
#ifdef CONFIG_LATX_FAST_JMPCACHE
    bool clear_legacy_fast_cache = false;
#endif
    if (registry_initialized &&
        lat_aot_v2_registry_deactivate_range(&registry, guest_start, guest_end,
                                             &deactivated)) {
        pthread_mutex_unlock(&elf_tracker_lock);
        return false;
    }
    LatAotV2RuntimeInstance **runtime_link = &runtime_instances;
    while (*runtime_link) {
        LatAotV2RuntimeInstance *runtime = *runtime_link;
        LatAotModuleInstanceV2 *instance = &runtime->instance;
        uint32_t overlaps = instance_exec_range_overlap_count(
            instance, guest_start, guest_end);
        uint64_t generation = atomic_load_explicit(
            &instance->generation, memory_order_acquire);
        if (!overlaps ||
            atomic_load_explicit(&instance->active, memory_order_acquire)) {
            runtime_link = &runtime->next;
            continue;
        }
        runtime->handled_invalidation_generation = generation;
        deactivated_exec_ranges += overlaps;
#ifdef CONFIG_LATX_FAST_JMPCACHE
        clear_legacy_fast_cache |= runtime->runtime_module &&
            runtime->runtime_module->legacy_fast_cache;
#endif
        bool was_current = aot_v2_current_instance == instance;
#ifdef CONFIG_LATX_FAST_JMPCACHE
        if (cpu) {
            was_current |= ((CPUArchState *)cpu->env_ptr)
                ->aot_v2_current_context == instance;
        }
#endif
        current_invalidated |= was_current;
        if (runtime->stats) {
            size_t previous_live = atomic_fetch_sub_explicit(
                &runtime->stats->live_instances, 1, memory_order_acq_rel);
            if (previous_live == 1) {
                atomic_store_explicit(&runtime->stats->state,
                                      LAT_AOT_V2_MODULE_INACTIVE,
                                      memory_order_release);
            }
        }
        if (aot_v2_current_instance == instance) {
            aot_v2_current_instance = NULL;
            aot_v2_current_generation = 0;
        }
#ifdef CONFIG_LATX_FAST_JMPCACHE
        if (cpu && ((CPUArchState *)cpu->env_ptr)
                       ->aot_v2_current_context == instance) {
            latx_aot_v2_fast_jmp_cache_set_context(cpu, NULL);
        }
#endif
        if (getenv("LATX_AOT_V2_REPORT")) {
            fprintf(stderr,
                    "latx: AOT v2 deactivated range=0x%llx-0x%llx "
                    "generation=%llu reason=%s\n",
                    (unsigned long long)instance->guest_begin,
                    (unsigned long long)instance->guest_end,
                    (unsigned long long)generation,
                    invalidation_reason_name(reason));
        }
        *runtime_link = runtime->next;
        runtime->next = runtime_instances_retired;
        runtime_instances_retired = runtime;
        atomic_fetch_sub_explicit(&runtime_instance_live_count, 1,
                                  memory_order_release);
        atomic_fetch_add_explicit(&runtime_instance_retired_count, 1,
                                  memory_order_release);
    }
    if (deactivated) {
        atomic_fetch_add_explicit(&registry_generation, 1,
                                  memory_order_release);
        atomic_fetch_add(&invalidated_instances, deactivated);
        atomic_fetch_add(&invalidated_exec_ranges, deactivated_exec_ranges);
        atomic_fetch_add(&invalidation_reasons[reason], 1);
    }
#ifdef CONFIG_LATX_FAST_JMPCACHE
    if (clear_legacy_fast_cache) {
        CPUState *clear_cpu;
        if (cpu) {
            latx_fast_jmp_cache_clear_all(cpu);
        }
        CPU_FOREACH(clear_cpu) {
            if (clear_cpu != cpu) {
                latx_fast_jmp_cache_clear_all(clear_cpu);
            }
        }
    }
#endif
    recycle_retired_instances_locked();
    pthread_mutex_unlock(&elf_tracker_lock);
    return current_invalidated;
}

void latc_aot_v2_note_munmap(CPUState *cpu, uint64_t guest_start,
                             uint64_t mapping_size)
{
    latc_aot_v2_invalidate_range(cpu, guest_start, mapping_size,
                                 LATC_AOT_V2_INVALIDATE_UNMAP);
}

#if !defined(CONFIG_LATX_KZT)
static uintptr_t latc_aot_v2_identity_guest_pc(uintptr_t guest_pc)
{
    return guest_pc;
}
#endif

static int bind_runtime_targets(void)
{
    LatAotRuntimeTargetsV2 targets = {
        .struct_size = sizeof(targets),
    };
    targets.target[LAT_AOT_TARGET_EPILOGUE_RET_ID_1] =
        context_switch_native_to_bt_ret_id_1;
    targets.target[LAT_AOT_TARGET_EPILOGUE_RET_ID_0] =
        context_switch_native_to_bt_ret_id_0;
    targets.target[LAT_AOT_TARGET_JIRL_EPILOGUE_RET_ID_1] =
        context_switch_native_to_bt_ret_id_1;
    targets.target[LAT_AOT_TARGET_JIRL_EPILOGUE_RET_ID_0] =
        context_switch_native_to_bt_ret_id_0;
    targets.target[LAT_AOT_TARGET_EPILOGUE_RET_0] =
        context_switch_native_to_bt_ret_0;
    targets.target[LAT_AOT_TARGET_UPDATE_MXCSR_STATUS] =
        (uintptr_t)update_mxcsr_status;
    targets.target[LAT_AOT_TARGET_FXSAVE] = (uintptr_t)helper_fxsave;
    targets.target[LAT_AOT_TARGET_FXRSTOR] = (uintptr_t)helper_fxrstor;
    targets.target[LAT_AOT_TARGET_FPREGS_X80_TO_64] =
        (uintptr_t)convert_fpregs_x80_to_64;
    targets.target[LAT_AOT_TARGET_FPREGS_64_TO_X80] =
        (uintptr_t)convert_fpregs_64_to_x80;
    targets.target[LAT_AOT_TARGET_UPDATE_FP_STATUS] =
        (uintptr_t)update_fp_status;
    targets.target[LAT_AOT_TARGET_CPUID] = (uintptr_t)helper_cpuid;
    targets.target[LAT_AOT_TARGET_RAISE_ILLOP] =
        (uintptr_t)helper_raise_illop;
    targets.target[LAT_AOT_TARGET_RAISE_GPF] =
        (uintptr_t)helper_raise_gpf;
    targets.target[LAT_AOT_TARGET_PCMPISTRI_XMM] =
        (uintptr_t)helper_pcmpistri_xmm;
    targets.target[LAT_AOT_TARGET_PCMPISTRM_XMM] =
        (uintptr_t)helper_pcmpistrm_xmm;
    targets.target[LAT_AOT_TARGET_EFLAGTF] = (uintptr_t)helper_eflagtf;
    targets.target[LAT_AOT_TARGET_LOG2] = (uintptr_t)log2;
    targets.target[LAT_AOT_TARGET_POW] = (uintptr_t)pow;
    targets.target[LAT_AOT_TARGET_SIN] = (uintptr_t)sin;
    targets.target[LAT_AOT_TARGET_COS] = (uintptr_t)cos;
    targets.target[LAT_AOT_TARGET_ATAN2] = (uintptr_t)atan2;
    targets.target[LAT_AOT_TARGET_LOGB] = (uintptr_t)logb;
    targets.target[LAT_AOT_TARGET_SINCOS] = (uintptr_t)sincos;
    targets.target[LAT_AOT_TARGET_FPATAN] = (uintptr_t)helper_fpatan;
    targets.target[LAT_AOT_TARGET_FPTAN] = (uintptr_t)helper_fptan;
    targets.target[LAT_AOT_TARGET_FPREM] = (uintptr_t)helper_fprem;
    targets.target[LAT_AOT_TARGET_FPREM1] = (uintptr_t)helper_fprem1;
    targets.target[LAT_AOT_TARGET_FRNDINT] = (uintptr_t)helper_frndint;
    targets.target[LAT_AOT_TARGET_F2XM1] = (uintptr_t)helper_f2xm1;
    targets.target[LAT_AOT_TARGET_FXTRACT] = (uintptr_t)helper_fxtract;
    targets.target[LAT_AOT_TARGET_FYL2X] = (uintptr_t)helper_fyl2x;
    targets.target[LAT_AOT_TARGET_FYL2XP1] = (uintptr_t)helper_fyl2xp1;
    targets.target[LAT_AOT_TARGET_FSINCOS] = (uintptr_t)helper_fsincos;
    targets.target[LAT_AOT_TARGET_FSIN] = (uintptr_t)helper_fsin;
    targets.target[LAT_AOT_TARGET_FCOS] = (uintptr_t)helper_fcos;
    targets.target[LAT_AOT_TARGET_FBLD_ST0] = (uintptr_t)helper_fbld_ST0;
    targets.target[LAT_AOT_TARGET_FBST_ST0] = (uintptr_t)helper_fbst_ST0;
    targets.target[LAT_AOT_TARGET_AESIMC_XMM] =
        (uintptr_t)helper_aesimc_xmm;
    targets.target[LAT_AOT_TARGET_AESKEYGENASSIST_XMM] =
        (uintptr_t)helper_aeskeygenassist_xmm;
    targets.target[LAT_AOT_TARGET_AESDEC_XMM] =
        (uintptr_t)helper_aesdec_xmm;
    targets.target[LAT_AOT_TARGET_AESDECLAST_XMM] =
        (uintptr_t)helper_aesdeclast_xmm;
    targets.target[LAT_AOT_TARGET_AESENC_XMM] =
        (uintptr_t)helper_aesenc_xmm;
    targets.target[LAT_AOT_TARGET_AESENCLAST_XMM] =
        (uintptr_t)helper_aesenclast_xmm;
    targets.target[LAT_AOT_TARGET_SHA1NEXTE] = (uintptr_t)helper_sha1nexte;
    targets.target[LAT_AOT_TARGET_SHA1MSG1] = (uintptr_t)helper_sha1msg1;
    targets.target[LAT_AOT_TARGET_SHA1MSG2] = (uintptr_t)helper_sha1msg2;
    targets.target[LAT_AOT_TARGET_SHA256MSG1] = (uintptr_t)helper_sha256msg1;
    targets.target[LAT_AOT_TARGET_SHA256MSG2] = (uintptr_t)helper_sha256msg2;
    targets.target[LAT_AOT_TARGET_SHA1RNDS4_F0] =
        (uintptr_t)helper_sha1rnds4_f0;
    targets.target[LAT_AOT_TARGET_SHA1RNDS4_F1] =
        (uintptr_t)helper_sha1rnds4_f1;
    targets.target[LAT_AOT_TARGET_SHA1RNDS4_F2] =
        (uintptr_t)helper_sha1rnds4_f2;
    targets.target[LAT_AOT_TARGET_SHA1RNDS4_F3] =
        (uintptr_t)helper_sha1rnds4_f3;
    targets.target[LAT_AOT_TARGET_SHA256RNDS2_XMM0] =
        (uintptr_t)helper_sha256rnds2_xmm0;
    targets.target[LAT_AOT_TARGET_RAISE_INT] = (uintptr_t)helper_raise_int;
    targets.target[LAT_AOT_TARGET_RAISE_TRAPOP] =
        (uintptr_t)helper_raise_trapop;
    targets.target[LAT_AOT_TARGET_RAISE_INTO] = (uintptr_t)helper_raise_into;
    targets.target[LAT_AOT_TARGET_RAISE_BOUND] =
        (uintptr_t)helper_raise_bound;
    targets.target[LAT_AOT_TARGET_XGETBV] = (uintptr_t)helper_xgetbv;
    targets.target[LAT_AOT_TARGET_XSAVE] = (uintptr_t)helper_xsave;
    targets.target[LAT_AOT_TARGET_XSAVEOPT] = (uintptr_t)helper_xsaveopt;
    targets.target[LAT_AOT_TARGET_XRSTOR] = (uintptr_t)helper_xrstor;
    targets.target[LAT_AOT_TARGET_XSETBV] = (uintptr_t)helper_xsetbv;
#if defined(CONFIG_LATX_KZT)
    targets.target[LAT_AOT_TARGET_KZT_GET_ALTERNATE] =
        (uintptr_t)kzt_get_alternate_pc;
#else
    targets.target[LAT_AOT_TARGET_KZT_GET_ALTERNATE] =
        (uintptr_t)latc_aot_v2_identity_guest_pc;
#endif
    return lat_aot_runtime_bind_targets(&targets);
}

static void raise_syscall_through_lat(void *opaque)
{
    (void)opaque;
    helper_raise_syscall();
    abort();
}

static void digest_bytes(const void *data, size_t size, uint8_t digest[32])
{
    GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(checksum, data, size);
    gsize digest_size = 32;
    g_checksum_get_digest(checksum, digest, &digest_size);
    g_checksum_free(checksum);
}

static void digest_hex(const uint8_t digest[32], char output[65])
{
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < 32; i++) {
        output[i * 2] = digits[digest[i] >> 4];
        output[i * 2 + 1] = digits[digest[i] & 15];
    }
    output[64] = '\0';
}

static int ensure_registry(void)
{
    if (!registry_initialized) {
        if (lat_aot_v2_registry_init(&registry)) {
            return -1;
        }
        registry_initialized = true;
    }
    if (!runtime_bound) {
        if (bind_runtime_targets() ||
            lat_aot_runtime_bind_syscall(raise_syscall_through_lat, NULL)) {
            return -1;
        }
        runtime_bound = true;
    }
    return 0;
}

static void *signal_invalidation_test_worker(void *opaque)
{
    (void)opaque;
    /*
     * Keep the production signal path read-only except for the test
     * handshake.  The writer uses the normal range invalidation path while
     * the reader retains the immutable module and instance records needed to
     * translate the faulting host PC.
     */
    while (atomic_load_explicit(&signal_invalidation_test_state,
                                memory_order_acquire) !=
           LAT_AOT_V2_SIGNAL_TEST_READER) {
        sched_yield();
    }
    LatAotModuleInstanceV2 *instance = atomic_load_explicit(
        &signal_invalidation_test_instance, memory_order_acquire);
    if (instance &&
        atomic_load_explicit(&instance->active, memory_order_acquire)) {
        latc_aot_v2_invalidate_range(
            NULL, instance->guest_begin,
            instance->guest_end - instance->guest_begin,
            LATC_AOT_V2_INVALIDATE_UNMAP);
        if (!atomic_load_explicit(&instance->active, memory_order_acquire)) {
            atomic_fetch_add(&signal_invalidation_overlaps, 1);
        }
    }
    atomic_store_explicit(&signal_invalidation_test_state,
                          LAT_AOT_V2_SIGNAL_TEST_WRITER,
                          memory_order_release);
    return NULL;
}

static int ensure_signal_invalidation_test_worker(void)
{
    if (!signal_invalidation_test ||
        signal_invalidation_test_worker_started) {
        return 0;
    }
    pthread_t worker;
    if (pthread_create(&worker, NULL, signal_invalidation_test_worker, NULL) ||
        pthread_detach(worker)) {
        errno = EBUSY;
        return -1;
    }
    signal_invalidation_test_worker_started = true;
    return 0;
}

static LatAotV2RuntimeModule *find_runtime_module(const uint8_t digest[32])
{
    for (LatAotV2RuntimeModule *module = runtime_modules; module;
         module = module->next) {
        if (!memcmp(module->loaded.note.source_sha256, digest, 32)) {
            return module;
        }
    }
    return NULL;
}

static char *read_owned_manifest(const char *path, size_t *size)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        return NULL;
    }
    struct stat status;
    if (fstat(fd, &status) || !S_ISREG(status.st_mode) ||
        status.st_uid != geteuid() || status.st_nlink != 1 ||
        (status.st_mode & 0222) || status.st_size <= 0 ||
        status.st_size >= 4096) {
        close(fd);
        errno = ENOEXEC;
        return NULL;
    }
    char *contents = g_malloc((size_t)status.st_size + 1);
    size_t done = 0;
    while (contents && done < (size_t)status.st_size) {
        ssize_t count = read(fd, contents + done,
                             (size_t)status.st_size - done);
        if (count > 0) {
            done += (size_t)count;
        } else if (count < 0 && errno == EINTR) {
            continue;
        } else {
            g_free(contents);
            contents = NULL;
            errno = count == 0 ? ENOEXEC : errno;
            break;
        }
    }
    close(fd);
    if (!contents) {
        return NULL;
    }
    contents[done] = '\0';
    *size = done;
    return contents;
}

static GPtrArray *read_module_manifest(const char *cache,
                                       const char source_hex[65],
                                       char *error, size_t error_size)
{
    char *index_name = g_strdup_printf("%s.current", source_hex);
    char *index_path = g_build_filename(cache, index_name, NULL);
    g_free(index_name);
    size_t size = 0;
    char *contents = read_owned_manifest(index_path, &size);
    if (!contents) {
        g_free(index_path);
        return NULL;
    }
    g_free(index_path);
    if (!size || size >= 4096 || !strstr(contents, "\"version\":2")) {
        snprintf(error, error_size, "AOT v2 module manifest is invalid");
        g_free(contents);
        errno = ENOEXEC;
        return NULL;
    }
    char *cursor = strstr(contents, "\"module\":\"");
    if (!cursor) {
        snprintf(error, error_size, "AOT v2 module manifest has no module");
        g_free(contents);
        errno = ENOEXEC;
        return NULL;
    }
    cursor += strlen("\"module\":\"");
    GPtrArray *modules = g_ptr_array_new_with_free_func(g_free);
    char *end = strchr(cursor, '\"');
    if (end && end != cursor && end - cursor < 192 &&
        !memchr(cursor, '/', end - cursor) && end - cursor > 3 &&
        !memcmp(end - 3, ".so", 3) && !strncmp(cursor, source_hex, 64)) {
        g_ptr_array_add(modules, g_strndup(cursor, end - cursor));
    }
    g_free(contents);
    if (modules->len != 1) {
        snprintf(error, error_size, "AOT v2 module manifest is invalid");
        g_ptr_array_free(modules, TRUE);
        errno = ENOEXEC;
        return NULL;
    }
    return modules;
}

static void recycle_retired_instances_locked(void)
{
    if (registry_initialized) {
        lat_aot_v2_registry_drain(&registry);
        LatAotRegistryCountsV2 counts;
        lat_aot_v2_registry_counts(&registry, &counts);
        if (counts.readers || counts.retired_snapshots) {
            return;
        }
    }
    LatAotV2RuntimeInstance **link = &runtime_instances_retired;
    while (*link) {
        LatAotV2RuntimeInstance *runtime = *link;
        if (atomic_load_explicit(&runtime->instance.readers,
                                 memory_order_seq_cst)) {
            link = &runtime->next;
            continue;
        }
        *link = runtime->next;
        runtime->next = runtime_instances_free;
        runtime_instances_free = runtime;
        atomic_fetch_sub_explicit(&runtime_instance_retired_count, 1,
                                  memory_order_release);
        atomic_fetch_add_explicit(&runtime_instance_free_count, 1,
                                  memory_order_release);
    }
}

static LatAotV2RuntimeInstance *allocate_runtime_instance_locked(void)
{
    recycle_retired_instances_locked();
    LatAotV2RuntimeInstance *runtime = runtime_instances_free;
    if (runtime) {
        runtime_instances_free = runtime->next;
        atomic_fetch_sub_explicit(&runtime_instance_free_count, 1,
                                  memory_order_release);
        /*
         * Cached targets retain this stable address.  Do not overwrite the
         * atomic active/generation/readers fields while a stale cache can
         * still probe them.  Inactive plus the bumped generation prevents a
         * stale cache from reading the fields reset below.
         */
        runtime->instance.module = NULL;
        runtime->instance.guest_load_bias = 0;
        runtime->instance.guest_begin = 0;
        runtime->instance.guest_end = 0;
        runtime->instance.exec_range_count = 0;
        memset(runtime->instance.exec_ranges, 0,
               sizeof(runtime->instance.exec_ranges));
        runtime->runtime_module = NULL;
        runtime->stats = NULL;
        g_free(runtime->guest_pages);
        runtime->guest_pages = NULL;
        runtime->guest_page_storage_count = 0;
        memset(runtime->guest_slots, 0, sizeof(runtime->guest_slots));
        runtime->guest_slot_count = 0;
        runtime->handled_invalidation_generation = 0;
        runtime->next = NULL;
    } else {
        runtime = g_new0(LatAotV2RuntimeInstance, 1);
        if (runtime) {
            atomic_fetch_add_explicit(&runtime_instance_allocated_count, 1,
                                      memory_order_release);
        }
    }
    return runtime;
}

static void release_unused_runtime_instance_locked(
    LatAotV2RuntimeInstance *runtime)
{
    runtime->next = runtime_instances_free;
    runtime_instances_free = runtime;
    atomic_fetch_add_explicit(&runtime_instance_free_count, 1,
                              memory_order_release);
}

static int register_module_instance(LatAotV2RuntimeModule *module,
                                    uint64_t load_bias,
                                    uint64_t guest_begin,
                                    uint64_t guest_end,
                                    const LatGuestElfRangeV2 *exec_ranges,
                                    uint32_t exec_range_count,
                                    LatAotV2RuntimeInstance **result)
{
    LatAotV2RuntimeInstance *runtime_instance =
        allocate_runtime_instance_locked();
    if (!runtime_instance) {
        errno = ENOMEM;
        return -1;
    }
    runtime_instance->runtime_module = module;
    runtime_instance->instance.module = &module->loaded;
    runtime_instance->instance.guest_load_bias = load_bias;
    runtime_instance->instance.guest_begin = guest_begin;
    runtime_instance->instance.guest_end = guest_end;
    if (exec_range_count > LAT_AOT_V2_MAX_EXEC_RANGES) {
        release_unused_runtime_instance_locked(runtime_instance);
        errno = E2BIG;
        return -1;
    }
    if (exec_range_count) {
        runtime_instance->instance.exec_range_count = exec_range_count;
        for (uint32_t i = 0; i < exec_range_count; i++) {
            runtime_instance->instance.exec_ranges[i] =
                (LatAotGuestRangeV2) {
                    .begin = exec_ranges[i].begin,
                    .end = exec_ranges[i].end,
                };
        }
    } else {
        runtime_instance->instance.exec_range_count = 1;
        runtime_instance->instance.exec_ranges[0] = (LatAotGuestRangeV2) {
            .begin = guest_begin,
            .end = guest_end,
        };
    }
    const LatAotModuleV2 *descriptor = module->loaded.descriptor;
    size_t address_count = descriptor->guest_slot_end -
                           descriptor->guest_slot_begin;
    int two_level = !!(descriptor->module_flags &
                       LAT_AOT_MODULE_TWO_LEVEL_GUEST_SLOTS);
    int three_level = !!(descriptor->module_flags &
                         LAT_AOT_MODULE_THREE_LEVEL_GUEST_SLOTS);
    if (two_level || three_level) {
        size_t leaf_page_count =
            (address_count + LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT - 1) /
            LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
        size_t root_count = three_level ?
            (leaf_page_count + LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT - 1) /
                LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT : 0;
        runtime_instance->guest_page_storage_count =
            (leaf_page_count + root_count) *
            LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
        runtime_instance->guest_pages = g_new0(
            uint64_t, runtime_instance->guest_page_storage_count);
        if (runtime_instance->guest_page_storage_count &&
            !runtime_instance->guest_pages) {
            release_unused_runtime_instance_locked(runtime_instance);
            errno = ENOMEM;
            return -1;
        }
    }
    size_t context_slot_count = 0;
    if (lat_aot_v2_context_apply_guest_table(
            descriptor, load_bias,
            runtime_instance->guest_slots +
                LAT_AOT_V2_CONTEXT_GUEST_SLOT_LIMIT,
            runtime_instance->guest_pages,
            runtime_instance->guest_page_storage_count,
            &context_slot_count)) {
        release_unused_runtime_instance_locked(runtime_instance);
        return -1;
    }
    runtime_instance->guest_slot_count = context_slot_count;
    if (lat_aot_v2_registry_register(&registry,
                                     &runtime_instance->instance)) {
        release_unused_runtime_instance_locked(runtime_instance);
        return -1;
    }
    runtime_instance->next = runtime_instances;
    runtime_instances = runtime_instance;
    atomic_fetch_add_explicit(&runtime_instance_live_count, 1,
                              memory_order_release);
    if (result) {
        *result = runtime_instance;
    }
    atomic_store_explicit(&active, true, memory_order_release);
    atomic_fetch_add_explicit(&registry_generation, 1, memory_order_release);
    return 0;
}

static int run_lifecycle_stress(LatAotV2RuntimeModule *module)
{
    const char *value = getenv("LATX_AOT_V2_TEST_LIFECYCLE_ROUNDS");
    if (!value || !*value) {
        return 0;
    }
    char *end = NULL;
    errno = 0;
    unsigned long long rounds = strtoull(value, &end, 10);
    if (errno || !end || *end || !rounds) {
        errno = EINVAL;
        return -1;
    }
    const uint64_t guest_begin = UINT64_C(0x400000000000);
    const uint64_t guest_end = guest_begin + UINT64_C(0x200000);
    LatGuestElfRangeV2 exec_range = {
        .begin = guest_begin,
        .end = guest_end,
    };
    size_t allocated_before = atomic_load_explicit(
        &runtime_instance_allocated_count, memory_order_acquire);
    size_t live_before = atomic_load_explicit(
        &runtime_instance_live_count, memory_order_acquire);
    for (unsigned long long i = 0; i < rounds; i++) {
        if (register_module_instance(module, guest_begin, guest_begin,
                                     guest_end, &exec_range, 1, NULL)) {
            return -1;
        }
        latc_aot_v2_invalidate_range(
            NULL, guest_begin, guest_end - guest_begin,
            LATC_AOT_V2_INVALIDATE_CODE_WRITE);
    }
    pthread_mutex_lock(&elf_tracker_lock);
    recycle_retired_instances_locked();
    pthread_mutex_unlock(&elf_tracker_lock);
    LatAotRegistryCountsV2 registry_counts;
    lat_aot_v2_registry_counts(&registry, &registry_counts);
    size_t live = atomic_load_explicit(&runtime_instance_live_count,
                                       memory_order_acquire);
    size_t retired = atomic_load_explicit(&runtime_instance_retired_count,
                                          memory_order_acquire);
    size_t free_count = atomic_load_explicit(&runtime_instance_free_count,
                                             memory_order_acquire);
    size_t allocated = atomic_load_explicit(
        &runtime_instance_allocated_count, memory_order_acquire);
    if (live != live_before || retired || free_count != 1 ||
        allocated > allocated_before + 1 || registry_counts.readers ||
        registry_counts.retired_snapshots) {
        fprintf(stderr,
                "latx: AOT v2 lifecycle stress failed rounds=%llu "
                "live=%zu retired=%zu free=%zu allocated=%zu "
                "registry_readers=%zu registry_retired=%zu\n",
                rounds, live, retired, free_count, allocated,
                registry_counts.readers,
                registry_counts.retired_snapshots);
        errno = EBUSY;
        return -1;
    }
    fprintf(stderr,
            "latx: AOT v2 lifecycle stress PASS rounds=%llu "
            "live=%zu retired=%zu free=%zu allocated=%zu "
            "registry_retired=%zu\n",
            rounds, live, retired, free_count, allocated,
            registry_counts.retired_snapshots);
    return 0;
}

static int register_discovered_module(const LatGuestElfInfoV2 *info,
                                      LatAotV2RuntimeInstance **instance,
                                      char *error, size_t error_size)
{
    const char *cache = lat_aot_cache_path();
    if (!cache || !*cache) {
        return 0;
    }
    if (instance) {
        *instance = NULL;
    }
    if (ensure_registry()) {
        snprintf(error, error_size, "cannot initialize AOT v2 registry: %s",
                 strerror(errno));
        return -1;
    }
    if (aot_v2_collect_only) {
        return 0;
    }
    LatAotV2RuntimeModule *module = find_runtime_module(info->source_sha256);
    if (!module) {
        char source_hex[65];
        digest_hex(info->source_sha256, source_hex);
        LatAotExpectedV2 expected = {0};
        memcpy(expected.source_sha256, info->source_sha256,
               sizeof(expected.source_sha256));
        digest_bytes(LATC_BUILD_ID, strlen(LATC_BUILD_ID),
                     expected.codegen_id);
        expected.available_features = available_aot_features();
        for (unsigned int attempt = 0; attempt < 2; attempt++) {
            GPtrArray *basenames = read_module_manifest(
                cache, source_hex, error, error_size);
            if (!basenames) {
                if (errno == ENOENT) {
                    error[0] = '\0';
                    return 0;
                }
                return -1;
            }
            bool retry_manifest = false;
            for (guint i = 0; i < basenames->len; i++) {
                const char *basename = g_ptr_array_index(basenames, i);
                char *path = g_build_filename(cache, basename, NULL);
                module = g_new0(LatAotV2RuntimeModule, 1);
                if (!path || !module) {
                    g_free(module);
                    g_free(path);
                    g_ptr_array_free(basenames, TRUE);
                    errno = ENOMEM;
                    return -1;
                }
                errno = 0;
                if (lat_aot_v2_module_open(path, &expected, &module->loaded,
                                           error, error_size)) {
                    int saved_errno = errno;
                    g_free(module);
                    g_free(path);
                    if (saved_errno == ENOENT && attempt == 0) {
                        retry_manifest = true;
                        break;
                    }
                    g_ptr_array_free(basenames, TRUE);
                    errno = saved_errno;
                    return -1;
                }
                if (register_host_module(module)) {
                    int saved_errno = errno;
                    if (!error[0]) {
                        snprintf(error, error_size,
                                 "cannot index AOT v2 host module: %s",
                                 strerror(saved_errno));
                    }
                    g_free(module);
                    g_free(path);
                    g_ptr_array_free(basenames, TRUE);
                    errno = saved_errno;
                    return -1;
                }
                /* The untagged legacy cache is only safe for immutable
                 * non-PIE ET_EXEC mappings. */
                module->legacy_fast_cache =
                    info->elf_type == ET_EXEC && info->load_bias == 0;
                module->path = path;
                module->next = runtime_modules;
                runtime_modules = module;
            }
            g_ptr_array_free(basenames, TRUE);
            if (!retry_manifest) {
                break;
            }
            error[0] = '\0';
            if (attempt == 1) {
                errno = ENOENT;
                return -1;
            }
        }
    }
    if (info->load_bias > UINT64_MAX - info->preferred_base) {
        snprintf(error, error_size, "AOT v2 descriptor load bias overflows");
        errno = EOVERFLOW;
        return -1;
    }
    int registered = 0;
    for (module = runtime_modules; module; module = module->next) {
        if (memcmp(module->loaded.note.source_sha256,
                   info->source_sha256, 32)) continue;
        LatAotV2RuntimeInstance *current = NULL;
        if (register_module_instance(module,
                                     info->load_bias + info->preferred_base,
                                     info->guest_begin, info->guest_end,
                                     info->exec_ranges,
                                     info->exec_range_count, &current)) {
            snprintf(error, error_size, "cannot register AOT v2 instance: %s",
                     strerror(errno));
            return -1;
        }
        if (!registered && instance) *instance = current;
        registered++;
    }
    return registered ? 1 : 0;
}

static int mapped_exec_bytes_match(int fd, const LatGuestElfInfoV2 *info)
{
    Elf64_Ehdr header;
    if (pread(fd, &header, sizeof(header), 0) != sizeof(header) ||
        header.e_phentsize != sizeof(Elf64_Phdr) || !header.e_phnum ||
        header.e_phnum > 4096) {
        return 0;
    }
    size_t phdr_size = (size_t)header.e_phnum * sizeof(Elf64_Phdr);
    Elf64_Phdr *phdrs = g_malloc(phdr_size);
    if (!phdrs || pread(fd, phdrs, phdr_size, header.e_phoff) !=
                      (ssize_t)phdr_size) {
        g_free(phdrs);
        return 0;
    }
    unsigned char file_bytes[64 * 1024];
    int matches = 1;
    for (uint16_t i = 0; i < header.e_phnum && matches; i++) {
        const Elf64_Phdr *phdr = &phdrs[i];
        if (phdr->p_type != PT_LOAD || !(phdr->p_flags & PF_X) ||
            !phdr->p_filesz || phdr->p_vaddr > UINT64_MAX - info->load_bias ||
            phdr->p_filesz > UINT64_MAX - (info->load_bias + phdr->p_vaddr)) {
            continue;
        }
        uint64_t guest = info->load_bias + phdr->p_vaddr;
        if (guest < info->guest_begin ||
            guest + phdr->p_filesz > info->guest_end) {
            matches = 0;
            break;
        }
        uint64_t done = 0;
        while (done < phdr->p_filesz) {
            size_t count = MIN((uint64_t)sizeof(file_bytes),
                               phdr->p_filesz - done);
            if (pread(fd, file_bytes, count, phdr->p_offset + done) !=
                    (ssize_t)count ||
                memcmp(g2h_untagged(guest + done), file_bytes, count)) {
                matches = 0;
                break;
            }
            done += count;
        }
    }
    g_free(phdrs);
    return matches;
}

bool latc_aot_v2_revalidate_range(uint64_t guest_start,
                                  uint64_t mapping_size)
{
    if (!mapping_size || guest_start > UINT64_MAX - mapping_size ||
        !latc_aot_v2_mapping_enabled()) {
        return false;
    }
    uint64_t guest_end = guest_start + mapping_size;
    bool registered = false;
    pthread_mutex_lock(&elf_tracker_lock);
    for (LatAotV2SourceMapping *source = source_mappings; source;
         source = source->next) {
        uint64_t source_end = source->guest_start + source->mapping_size;
        if (guest_start >= source_end || guest_end <= source->guest_start) {
            continue;
        }
        LatGuestElfInfoV2 info;
        char error[256] = {0};
        if (lat_guest_elf_inspect_mapping_v2(
                source->fd, source->guest_start, source->mapping_size,
                source->file_offset, TARGET_PAGE_SIZE, &info,
                error, sizeof(error)) ||
            !mapped_exec_bytes_match(source->fd, &info)) {
            atomic_fetch_add(&revalidation_failures, 1);
            continue;
        }
        bool overlaps_exec = false;
        for (uint16_t i = 0; i < info.exec_range_count; i++) {
            overlaps_exec |= guest_start < info.exec_ranges[i].end &&
                             guest_end > info.exec_ranges[i].begin;
        }
        if (!overlaps_exec) {
            continue;
        }
        LatAotV2RuntimeInstance *instance = NULL;
        int64_t registration_start = g_get_monotonic_time();
        int result = register_discovered_module(&info, &instance,
                                                error, sizeof(error));
        uint64_t registration_ns =
            (g_get_monotonic_time() - registration_start) * 1000;
        if (result > 0 && instance) {
            LatAotV2ModuleStats *stats = add_module_stats(
                &info, LAT_AOT_V2_MODULE_REGISTERED, source->fd);
            instance->stats = stats;
            if (stats) {
                atomic_store_explicit(&stats->registration_ns,
                                      registration_ns,
                                      memory_order_release);
                atomic_fetch_add_explicit(&stats->live_instances, 1,
                                          memory_order_release);
            }
            atomic_fetch_add(&revalidated_instances, 1);
            registered = true;
            break;
        }
        if (result < 0 && errno != EEXIST) {
            atomic_fetch_add(&revalidation_failures, 1);
            if (getenv("LATX_AOT_V2_REPORT")) {
                fprintf(stderr,
                        "latx: AOT v2 revalidation rejected source: %s\n",
                        error[0] ? error : strerror(errno));
            }
        }
    }
    pthread_mutex_unlock(&elf_tracker_lock);
    return registered;
}

bool latc_aot_v2_note_mremap(CPUState *cpu, uint64_t old_start,
                             uint64_t old_size, uint64_t new_start,
                             uint64_t new_size, bool keep_old)
{
    LatAotV2SourceMapping *moved = NULL;

    if (!old_size || old_size != new_size ||
        old_start > UINT64_MAX - old_size ||
        new_start > UINT64_MAX - new_size) {
        if (!keep_old) {
            latc_aot_v2_invalidate_range(cpu, old_start, old_size,
                                         LATC_AOT_V2_INVALIDATE_UNMAP);
        }
        latc_aot_v2_invalidate_range(cpu, new_start, new_size,
                                     LATC_AOT_V2_INVALIDATE_MAP_FIXED);
        return false;
    }

    pthread_mutex_lock(&elf_tracker_lock);
    for (LatAotV2SourceMapping *source = source_mappings; source;
         source = source->next) {
        if (source->guest_start != old_start ||
            source->mapping_size != old_size) {
            continue;
        }
        moved = g_new0(LatAotV2SourceMapping, 1);
        if (moved) {
            moved->fd = fcntl(source->fd, F_DUPFD_CLOEXEC, 3);
            if (moved->fd < 0) {
                g_free(moved);
                moved = NULL;
            } else {
                moved->guest_start = new_start;
                moved->mapping_size = new_size;
                moved->file_offset = source->file_offset;
            }
        }
        break;
    }
    pthread_mutex_unlock(&elf_tracker_lock);

    if (!keep_old) {
        latc_aot_v2_invalidate_range(cpu, old_start, old_size,
                                     LATC_AOT_V2_INVALIDATE_UNMAP);
    }
    if (new_start != old_start || keep_old) {
        latc_aot_v2_invalidate_range(cpu, new_start, new_size,
                                     LATC_AOT_V2_INVALIDATE_MAP_FIXED);
    }
    if (!moved) {
        return false;
    }
    pthread_mutex_lock(&elf_tracker_lock);
    moved->next = source_mappings;
    source_mappings = moved;
    pthread_mutex_unlock(&elf_tracker_lock);
    return latc_aot_v2_revalidate_range(new_start, new_size);
}

static int inspect_source(const char *path, LatAotExpectedV2 *expected,
                          uint64_t *guest_base, uint64_t *guest_end,
                          LatGuestElfRangeV2 *exec_ranges,
                          uint32_t *exec_range_count,
                          char *error, size_t error_size)
{
    int bundled = latc_bundle_verified_guest(expected->source_sha256,
                                              guest_base, guest_end);
    *exec_range_count = 0;
    if (bundled < 0) {
        snprintf(error, error_size, "cannot read verified bundle identity");
        return -1;
    }
    if (bundled) {
        digest_bytes(LATC_BUILD_ID, strlen(LATC_BUILD_ID),
                     expected->codegen_id);
        expected->available_features = available_aot_features();
    }
    gchar *file = NULL;
    gsize size = 0;
    if (!g_file_get_contents(path, &file, &size, NULL) ||
        size < sizeof(Elf64_Ehdr)) {
        if (bundled) {
            exec_ranges[0] = (LatGuestElfRangeV2) {
                .begin = *guest_base,
                .end = *guest_end,
            };
            *exec_range_count = 1;
            g_free(file);
            return 0;
        }
        snprintf(error, error_size, "cannot read x86 source ELF");
        g_free(file);
        return -1;
    }
    const Elf64_Ehdr *elf = (const void *)file;
    if (memcmp(elf->e_ident, ELFMAG, SELFMAG) ||
        elf->e_ident[EI_CLASS] != ELFCLASS64 ||
        elf->e_machine != EM_X86_64 ||
        elf->e_type != ET_EXEC ||
        elf->e_phentsize != sizeof(Elf64_Phdr) || !elf->e_phnum ||
        elf->e_phoff > size ||
        elf->e_phnum > (size - elf->e_phoff) / sizeof(Elf64_Phdr)) {
        snprintf(error, error_size, "source is not a supported x86-64 ELF");
        g_free(file);
        return -1;
    }
    const Elf64_Phdr *phdrs = (const void *)(file + elf->e_phoff);
    uint64_t begin = UINT64_MAX;
    uint64_t end = 0;
    for (uint16_t i = 0; i < elf->e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) {
            continue;
        }
        begin = MIN(begin, phdrs[i].p_vaddr);
        if (phdrs[i].p_vaddr > UINT64_MAX - phdrs[i].p_memsz) {
            snprintf(error, error_size, "source load range overflows");
            g_free(file);
            return -1;
        }
        end = MAX(end, phdrs[i].p_vaddr + phdrs[i].p_memsz);
        if (phdrs[i].p_flags & PF_X) {
            uint64_t exec_begin = phdrs[i].p_vaddr & TARGET_PAGE_MASK;
            uint64_t exec_end = TARGET_PAGE_ALIGN(
                phdrs[i].p_vaddr + phdrs[i].p_memsz);
            if (!exec_end || exec_end <= exec_begin ||
                (*exec_range_count &&
                 exec_begin < exec_ranges[*exec_range_count - 1].begin)) {
                snprintf(error, error_size,
                         "source executable ranges are invalid");
                g_free(file);
                return -1;
            }
            if (*exec_range_count &&
                exec_begin <= exec_ranges[*exec_range_count - 1].end) {
                exec_ranges[*exec_range_count - 1].end = MAX(
                    exec_ranges[*exec_range_count - 1].end, exec_end);
            } else {
                if (*exec_range_count == LAT_GUEST_ELF_MAX_EXEC_RANGES) {
                    snprintf(error, error_size,
                             "source has too many executable ranges");
                    g_free(file);
                    return -1;
                }
                exec_ranges[(*exec_range_count)++] = (LatGuestElfRangeV2) {
                    .begin = exec_begin,
                    .end = exec_end,
                };
            }
        }
    }
    if (begin != UINT64_MAX) {
        begin &= TARGET_PAGE_MASK;
        end = TARGET_PAGE_ALIGN(end);
    }
    if (begin == UINT64_MAX || end <= begin || !*exec_range_count) {
        snprintf(error, error_size,
                 "source has no loadable executable range");
        g_free(file);
        return -1;
    }
    if (!bundled) {
        digest_bytes(file, size, expected->source_sha256);
        digest_bytes(LATC_BUILD_ID, strlen(LATC_BUILD_ID),
                     expected->codegen_id);
        expected->available_features = available_aot_features();
    }
    *guest_base = begin;
    *guest_end = end;
    g_free(file);
    return 0;
}

int latc_aot_v2_prepare(CPUArchState *env)
{
    const char *module_path = lat_aot_module_path();
    const char *source_path = getenv("LATX_AOT_V2_SOURCE");
    bool strict = aot_v2_strict;
    signal_invalidation_test =
        getenv("LATX_AOT_V2_TEST_SIGNAL_INVALIDATION_RACE") != NULL;
    if (latc_aot_v2_mapping_enabled() &&
        (ensure_registry() || ensure_signal_invalidation_test_worker())) {
        fprintf(stderr, "latx: cannot initialize AOT v2 registry: %s\n",
                strerror(errno));
        return strict ? -1 : 0;
    }
    drain_mappings();
    if (prepared || !module_path || !*module_path) {
        return 0;
    }
    prepared = true;
    char error[256] = {0};
    LatAotExpectedV2 expected = {0};
    uint64_t guest_base;
    uint64_t guest_end;
    LatGuestElfRangeV2 exec_ranges[LAT_GUEST_ELF_MAX_EXEC_RANGES];
    uint32_t exec_range_count;
    LatAotV2RuntimeModule *module = g_new0(LatAotV2RuntimeModule, 1);
    if (!source_path || !*source_path ||
        !module ||
        inspect_source(source_path, &expected, &guest_base, &guest_end,
                       exec_ranges, &exec_range_count,
                       error, sizeof(error)) ||
        lat_aot_v2_module_open(module_path, &expected, &module->loaded,
                               error, sizeof(error))) {
        fprintf(stderr, "latx: cannot prepare AOT v2 module: %s\n",
                error[0] ? error : strerror(errno));
        g_free(module);
        return strict ? -1 : 0;
    }
    module->path = g_strdup(module_path);
    module->legacy_fast_cache = true;
    if (register_host_module(module)) {
        fprintf(stderr, "latx: cannot index AOT v2 host module: %s\n",
                strerror(errno));
        return strict ? -1 : 0;
    }
    module->next = runtime_modules;
    runtime_modules = module;
    if (register_module_instance(module, guest_base, guest_base, guest_end,
                                 exec_ranges, exec_range_count,
                                 NULL) ||
        run_lifecycle_stress(module) ||
        (!(module->loaded.descriptor->module_flags &
           (LAT_AOT_MODULE_TWO_LEVEL_GUEST_SLOTS |
            LAT_AOT_MODULE_THREE_LEVEL_GUEST_SLOTS)) &&
         lat_aot_v2_context_apply_guest_slots(
             module->loaded.descriptor, guest_base,
             env->tb_jmp_cache_ptr))) {
        fprintf(stderr, "latx: cannot register AOT v2 module: %s\n",
                strerror(errno));
        return strict ? -1 : 0;
    }

    if (getenv("LATX_AOT_V2_REPORT")) {
        size_t count = (size_t)(module->loaded.descriptor->tb_end -
                                module->loaded.descriptor->tb_begin);
        fprintf(stderr, "latx: AOT v2 registered module with %zu TBs from %s\n",
                count, module_path);
    }
    return 0;
}

static void set_registry_miss_target(LatcAotV2Target *result,
                                     target_ulong guest_pc, uint32_t cflags,
                                     uint64_t generation,
                                     LatAotV2ModuleStats *stats,
                                     bool track)
{
    *result = (LatcAotV2Target) {
        .context = track ? (stats ? (const void *)stats :
                           (const void *)&tracked_nonfile_miss) : NULL,
        .generation_address = &registry_generation,
        .guest_pc = guest_pc,
        .generation = generation,
        .cflags = cflags,
    };
}

void latc_aot_v2_note_cached_miss(const LatcAotV2Target *target)
{
    if (!target || !target->context || target->host_address) {
        return;
    }
    LatAotV2ModuleStats *stats =
        target->context == (const void *)&tracked_nonfile_miss ? NULL :
        (LatAotV2ModuleStats *)target->context;
    note_dispatch_miss(stats, target->guest_pc, target->cflags);
}

bool latc_aot_v2_find_target(CPUState *cpu, target_ulong guest_pc,
                             uint32_t cflags, LatcAotV2Target *result)
{
    if (!registry_initialized || !result) {
        if (aot_v2_reject_miss) {
            fprintf(stderr,
                    "latc: strict AOT rejected runtime TB generation at "
                    "0x%llx cflags=0x%x file=%d program=0\n",
                    (unsigned long long)guest_pc, cflags,
                    latc_aot_v2_is_file_pc(guest_pc));
            _exit(125);
        }
        return false;
    }
    if (aot_v2_collect_only) {
        if (aot_v2_reject_miss) {
            fprintf(stderr,
                    "latc: strict AOT rejected runtime TB generation at "
                    "0x%llx cflags=0x%x file=%d program=0\n",
                    (unsigned long long)guest_pc, cflags,
                    latc_aot_v2_is_file_pc(guest_pc));
            _exit(125);
        }
        return false;
    }
    uint64_t inactive_generation = atomic_load_explicit(
        &registry_generation, memory_order_acquire);
    if (!atomic_load_explicit(&active, memory_order_acquire)) {
        bool track = dispatch_miss_tracking_enabled();
        LatAotV2ModuleStats *stats = track ? module_stats_for_pc(guest_pc) :
                                           NULL;
        set_registry_miss_target(result, guest_pc, cflags,
                                 inactive_generation, stats, track);
        if (track) {
            note_dispatch_miss(stats, guest_pc, cflags);
        }
        if (aot_v2_reject_miss) {
            fprintf(stderr,
                    "latc: strict AOT rejected runtime TB generation at "
                    "0x%llx cflags=0x%x file=%d program=0\n",
                    (unsigned long long)guest_pc, cflags,
                    latc_aot_v2_is_file_pc(guest_pc));
            _exit(125);
        }
        latc_aot_v2_note_jit_key(guest_pc, cflags);
        return false;
    }
    LatAotV2ModuleStats *stats = NULL;
    if (!aot_v2_target_cache) {
        aot_v2_target_cache = g_new0(LatAotV2TargetCacheEntry,
                                     LAT_AOT_V2_TARGET_CACHE_SIZE);
        if (!aot_v2_target_cache) {
            note_dispatch_miss(stats, guest_pc, cflags);
            if (aot_v2_reject_miss) {
                fprintf(stderr,
                        "latc: strict AOT rejected runtime TB generation at "
                        "0x%llx cflags=0x%x file=%d program=0\n",
                        (unsigned long long)guest_pc, cflags,
                        latc_aot_v2_is_file_pc(guest_pc));
                _exit(125);
            }
            latc_aot_v2_note_jit_key(guest_pc, cflags);
            return false;
        }
    }
    uint32_t hash = (uint32_t)((guest_pc ^
        (guest_pc >> LAT_AOT_V2_TARGET_CACHE_BITS) ^ cflags) &
        (LAT_AOT_V2_TARGET_CACHE_SIZE - 1));
    LatAotV2TargetCacheEntry *entry = &aot_v2_target_cache[hash];
    LatAotModuleInstanceV2 *instance = entry->instance;
    bool target_held = false;
    if (entry->guest_pc == guest_pc && entry->cflags == cflags && instance) {
        atomic_fetch_add_explicit(&instance->readers, 1,
                                  memory_order_seq_cst);
        if (atomic_load_explicit(&instance->active, memory_order_acquire) &&
            entry->generation == atomic_load_explicit(
                &instance->generation, memory_order_acquire)) {
            target_held = true;
        } else {
            atomic_fetch_sub_explicit(&instance->readers, 1,
                                      memory_order_seq_cst);
        }
    }
    LatAotTargetV2 target = {0};
    if (!target_held) {
        uint64_t generation;
        for (;;) {
            generation = atomic_load_explicit(&registry_generation,
                                              memory_order_acquire);
            if (!lat_aot_v2_registry_lookup(&registry, guest_pc,
                                            aot_v2_semantic_flags(cflags),
                                            &target)) {
                break;
            }
            if (generation != atomic_load_explicit(
                    &registry_generation, memory_order_acquire)) {
                continue;
            }
            bool track = dispatch_miss_tracking_enabled();
            if (track) {
                stats = module_stats_for_pc(guest_pc);
            }
            set_registry_miss_target(result, guest_pc, cflags, generation,
                                     stats, track);
            if (track) {
                note_dispatch_miss(stats, guest_pc, cflags);
            }
            if (aot_v2_reject_miss) {
                fprintf(stderr,
                        "latc: strict AOT rejected runtime TB generation at "
                        "0x%llx cflags=0x%x file=%d program=0\n",
                        (unsigned long long)guest_pc, cflags,
                        latc_aot_v2_is_file_pc(guest_pc));
                _exit(125);
            }
            latc_aot_v2_note_jit_key(guest_pc, cflags);
            return false;
        }
        entry->guest_pc = guest_pc;
        entry->cflags = cflags;
        entry->generation = target.generation;
        entry->host_address = (const void *)target.host_address;
        entry->instance = target.instance;
        instance = target.instance;
        target_held = true;
    }
    LatAotV2RuntimeInstance *runtime_instance = (void *)instance;
    if (runtime_instance->stats) {
        stats = runtime_instance->stats;
    }
    result->host_address = entry->host_address;
    result->context = instance;
    result->generation_address = &instance->generation;
    result->guest_pc = guest_pc;
    result->generation = entry->generation;
    result->cflags = cflags;
    result->guest_slots_end = runtime_instance->guest_slots +
        LAT_AOT_V2_CONTEXT_GUEST_SLOT_LIMIT;
    result->guest_slot_count = runtime_instance->guest_slot_count;
    result->reader_held = target_held;
    if (!latc_aot_v2_activate_target(cpu, result)) {
        if (target_held) {
            atomic_fetch_sub_explicit(&instance->readers, 1,
                                      memory_order_seq_cst);
            result->reader_held = false;
        }
        note_dispatch_miss(stats, guest_pc, cflags);
        if (aot_v2_reject_miss) {
            fprintf(stderr,
                    "latc: strict AOT rejected runtime TB generation at "
                    "0x%llx cflags=0x%x file=%d program=0\n",
                    (unsigned long long)guest_pc, cflags,
                    latc_aot_v2_is_file_pc(guest_pc));
            _exit(125);
        }
        latc_aot_v2_note_jit_key(guest_pc, cflags);
        return false;
    }
    if (stats) {
        atomic_fetch_add(&stats->aot_lookups, 1);
    }
#ifdef CONFIG_LATX_FAST_JMPCACHE
    /* PIE and DSO targets keep using the generation-checked AOT v2 cache. */
    bool legacy_fast_cache =
        !lat_aot_cache_path() ||
        (runtime_instance->runtime_module &&
         runtime_instance->runtime_module->legacy_fast_cache);
    if (legacy_fast_cache) {
        uint32_t jump_hash = tb_jmp_cache_hash_func(guest_pc);
        FastTB *jump_cache = ((CPUArchState *)cpu->env_ptr)->tb_jmp_cache_ptr;
        qatomic_set(&jump_cache[jump_hash].ptr, result->host_address);
        qatomic_set(&jump_cache[jump_hash].pc, guest_pc);
    }
#endif
    atomic_fetch_add(&direct_targets, 1);
    return true;
}

bool latc_aot_v2_activate_target(CPUState *cpu,
                                 const LatcAotV2Target *target)
{
    LatAotModuleInstanceV2 *instance = (void *)target->context;
    if (!cpu || !instance) {
        return false;
    }
    bool acquired = false;
    if (!target->reader_held) {
        atomic_fetch_add_explicit(&instance->readers, 1,
                                  memory_order_seq_cst);
        acquired = true;
    }
    CPUArchState *env = (CPUArchState *)cpu->env_ptr;
    if (!atomic_load_explicit(&instance->active, memory_order_acquire) ||
        target->generation != atomic_load_explicit(
            &instance->generation, memory_order_acquire)) {
        if (acquired) {
            atomic_fetch_sub_explicit(&instance->readers, 1,
                                      memory_order_seq_cst);
        }
        return false;
    }
    const void *slot_context = aot_v2_current_instance;
#ifdef CONFIG_LATX_FAST_JMPCACHE
    slot_context = env->aot_v2_current_context;
#endif
    if (slot_context != instance) {
        uint64_t count = target->guest_slot_count;
        memcpy((uint64_t *)env->tb_jmp_cache_ptr - count,
               target->guest_slots_end - count,
               count * sizeof(uint64_t));
    }
    aot_v2_current_instance = instance;
    aot_v2_current_generation = target->generation;
    aot_v2_execution_instance = instance;
    aot_v2_execution_active = true;
#ifdef CONFIG_LATX_FAST_JMPCACHE
    latx_aot_v2_fast_jmp_cache_set_context(cpu, instance);
    if (lat_aot_cache_path()) {
        uint32_t hash = tb_jmp_cache_hash_func(target->guest_pc);
        latx_aot_v2_fast_jmp_cache_add(
            cpu, hash, target->guest_pc, target->host_address,
            target->context, (const uint64_t *)target->generation_address,
            target->generation, target->guest_slots_end,
            target->guest_slot_count);
    }
#endif
    return true;
}

void latc_aot_v2_release_current_execution(CPUState *cpu)
{
    LatAotModuleInstanceV2 *instance = aot_v2_execution_instance;
    if (!aot_v2_execution_active || !instance) {
        return;
    }
    atomic_fetch_sub_explicit(&instance->readers, 1,
                              memory_order_seq_cst);
    aot_v2_execution_instance = NULL;
    aot_v2_current_instance = NULL;
    aot_v2_current_generation = 0;
    aot_v2_execution_active = false;
#ifdef CONFIG_LATX_FAST_JMPCACHE
    if (cpu) {
        latx_aot_v2_fast_jmp_cache_set_context(cpu, NULL);
    }
#else
    (void)cpu;
#endif
}

void latc_aot_v2_release_target(CPUState *cpu, LatcAotV2Target *target)
{
    if (!target || !target->reader_held || !target->context) {
        return;
    }
    LatAotModuleInstanceV2 *instance = (void *)target->context;
    if (aot_v2_execution_active &&
        aot_v2_execution_instance == instance) {
        atomic_fetch_sub_explicit(&instance->readers, 1,
                                  memory_order_seq_cst);
        aot_v2_execution_instance = NULL;
    }
    aot_v2_current_instance = NULL;
    aot_v2_current_generation = 0;
    target->reader_held = false;
    aot_v2_execution_active = false;
#ifdef CONFIG_LATX_FAST_JMPCACHE
    if (cpu) {
        latx_aot_v2_fast_jmp_cache_set_context(cpu, NULL);
    }
#else
    (void)cpu;
#endif
}

bool latc_aot_v2_contains_host_pc(uintptr_t host_pc)
{
    return atomic_load_explicit(&active, memory_order_acquire) &&
           find_host_module(host_pc);
}

bool latc_aot_v2_diagnose_host_pc(CPUState *cpu, uintptr_t host_pc,
                                  LatcAotV2SignalDiagnostic *diagnostic)
{
    bool test_reader = false;
    bool instance_held = false;
    bool lookup_counted = false;
    const char *miss_reason = NULL;
    uintptr_t searched_pc =
        host_pc >= GETPC_ADJ ? host_pc - GETPC_ADJ : 0;
    LatAotV2RuntimeModule *runtime_module = NULL;
    LatAotModuleInstanceV2 *instance = NULL;
    LatAotModuleInstanceV2 *reported_instance = NULL;
    LatAotV2SignalDebugSnapshot snapshot = {0};
    uint64_t execution_generation = 0;
    bool report = false;

    if (!atomic_load_explicit(&active, memory_order_acquire)) {
        miss_reason = "runtime-inactive";
        goto miss;
    }
    if (!cpu) {
        miss_reason = "cpu-null";
        goto miss;
    }
    if (!diagnostic) {
        miss_reason = "diagnostic-null";
        goto miss;
    }
    if (host_pc < GETPC_ADJ) {
        miss_reason = "host-pc-underflow";
        goto miss;
    }
    atomic_fetch_add(&signal_pc_lookups, 1);
    lookup_counted = true;
    runtime_module = find_host_module(searched_pc);
    if (!runtime_module) {
        miss_reason = "host-module-miss";
        goto miss;
    }
    instance = aot_v2_execution_instance;
    if (!instance) {
        miss_reason = "execution-instance-null";
        goto miss;
    }
    atomic_fetch_add_explicit(&instance->readers, 1,
                              memory_order_seq_cst);
    instance_held = true;
    execution_generation = atomic_load_explicit(
        &instance->generation, memory_order_acquire);
    if (instance->module != &runtime_module->loaded) {
        miss_reason = "execution-module-mismatch";
        goto miss;
    }
    if (signal_invalidation_test) {
        unsigned int expected = LAT_AOT_V2_SIGNAL_TEST_IDLE;
        atomic_store_explicit(&signal_invalidation_test_instance, instance,
                              memory_order_release);
        test_reader = atomic_compare_exchange_strong_explicit(
            &signal_invalidation_test_state, &expected,
            LAT_AOT_V2_SIGNAL_TEST_READER,
            memory_order_acq_rel, memory_order_acquire);
        if (test_reader) {
            for (unsigned int spin = 0; spin < 100000000; spin++) {
                if (atomic_load_explicit(&signal_invalidation_test_state,
                                         memory_order_acquire) ==
                    LAT_AOT_V2_SIGNAL_TEST_WRITER) {
                    break;
                }
                atomic_signal_fence(memory_order_seq_cst);
            }
            if (atomic_load_explicit(&signal_invalidation_test_state,
                                     memory_order_acquire) !=
                LAT_AOT_V2_SIGNAL_TEST_WRITER) {
                miss_reason = "signal-invalidation-test-timeout";
                goto miss;
            }
        }
    }
    const LatAotModuleV2 *module = runtime_module->loaded.descriptor;
    uintptr_t text_begin = (uintptr_t)module->text_begin;
    uint64_t host_offset = searched_pc - text_begin;
    size_t count = (size_t)(module->pc_map_end - module->pc_map_begin);
    size_t left = 0;
    size_t right = count;
    while (left < right) {
        size_t middle = left + (right - left) / 2;
        if (module->pc_map_begin[middle].host_offset_begin <= host_offset) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    if (!left) {
        miss_reason = "pc-map-before-first";
        goto miss;
    }
    const LatAotPcMapV2 *map = &module->pc_map_begin[left - 1];
    const uint32_t known_map_flags = LAT_AOT_PC_MAP_DYNAMIC_STATE |
        LAT_AOT_PC_MAP_STACK_POINTER_DELTA;
    const bool has_stack_delta = map->flags &
        LAT_AOT_PC_MAP_STACK_POINTER_DELTA;
    if (host_offset >= map->host_offset_end) {
        miss_reason = "pc-map-host-range";
        goto miss;
    }
    if (!(map->flags & LAT_AOT_PC_MAP_DYNAMIC_STATE)) {
        miss_reason = "pc-map-missing-dynamic-state";
        goto miss;
    }
    if (map->flags & ~known_map_flags) {
        miss_reason = "pc-map-unknown-flags";
        goto miss;
    }
    if (!!map->state_record_offset != has_stack_delta) {
        miss_reason = "pc-map-stack-state-mismatch";
        goto miss;
    }
    if (map->guest_rva > UINT64_MAX - instance->guest_load_bias) {
        miss_reason = "guest-rva-overflow";
        goto miss;
    }
    target_ulong guest_pc = instance->guest_load_bias + map->guest_rva;
    if (!execution_generation) {
        miss_reason = "generation-zero";
        goto miss;
    }
    if (guest_pc < instance->guest_begin || guest_pc >= instance->guest_end) {
        miss_reason = "guest-pc-out-of-range";
        goto miss;
    }
    memcpy(diagnostic->source_sha256,
           runtime_module->loaded.note.source_sha256,
           sizeof(diagnostic->source_sha256));
    diagnostic->guest_pc = guest_pc;
    diagnostic->generation = execution_generation;
    diagnostic->guest_begin = instance->guest_begin;
    diagnostic->guest_end = instance->guest_end;
    diagnostic->stack_pointer_delta = has_stack_delta ?
        (int32_t)map->state_record_offset : 0;
    atomic_store(&signal_last_guest_pc, guest_pc);
    atomic_store(&signal_last_generation, execution_generation);
    atomic_store(&signal_last_guest_begin, instance->guest_begin);
    atomic_store(&signal_last_guest_end, instance->guest_end);
    uint64_t source_words[4];
    memcpy(source_words, runtime_module->loaded.note.source_sha256,
           sizeof(source_words));
    for (size_t i = 0; i < 4; i++) {
        atomic_store_explicit(&signal_last_source[i], source_words[i],
                              memory_order_release);
    }
    if (test_reader) {
        atomic_store_explicit(&signal_invalidation_test_state,
                              LAT_AOT_V2_SIGNAL_TEST_DONE,
                              memory_order_release);
    }
    atomic_fetch_add(&signal_pc_hits, 1);
    atomic_fetch_sub_explicit(&instance->readers, 1,
                              memory_order_seq_cst);
    return true;

miss:
    report = signal_diagnostic_enabled();
    reported_instance = instance ? instance : aot_v2_execution_instance;
    if (report) {
        snapshot = capture_signal_debug_snapshot(reported_instance,
                                                 runtime_module);
    }
    if (test_reader) {
        atomic_store_explicit(&signal_invalidation_test_state,
                              LAT_AOT_V2_SIGNAL_TEST_DONE,
                              memory_order_release);
    }
    if (lookup_counted) {
        atomic_fetch_add(&signal_pc_misses, 1);
    }
    if (instance_held) {
        atomic_fetch_sub_explicit(&instance->readers, 1,
                                  memory_order_seq_cst);
    }
    if (report) {
        report_signal_debug_snapshot(
            "diagnose-miss", miss_reason ? miss_reason : "unknown",
            host_pc, searched_pc, instance_held, &snapshot);
    }
    return false;
}

bool latc_aot_v2_restore_state(CPUState *cpu, uintptr_t host_pc)
{
    LatcAotV2SignalDiagnostic diagnostic;

    if (!latc_aot_v2_diagnose_host_pc(cpu, host_pc, &diagnostic)) {
        return false;
    }
    target_ulong data[TARGET_INSN_START_WORDS] = {
        diagnostic.guest_pc,
        CC_OP_DYNAMIC,
    };
    TranslationBlock tb = {
        .pc = data[0],
    };
    restore_state_to_opc(cpu->env_ptr, &tb, data);
#ifdef CONFIG_LATX_OPT_PUSH_POP_TRANS
    ((CPUX86State *)cpu->env_ptr)->regs[R_ESP] +=
        diagnostic.stack_pointer_delta;
#endif
    return true;
}

/*
 * The signal handler only records an AOT host PC.  Module lookup, PC-map
 * diagnosis, state restoration, and invalidation run after returning to the
 * normal execution context in drain_signal_recovery().
 */
bool latc_aot_v2_defer_signal_recovery(CPUState *cpu, uintptr_t host_pc,
                                       target_ulong *guest_pc)
{
    if (!cpu || !guest_pc || !aot_v2_execution_active ||
        !aot_v2_execution_instance) {
        return false;
    }
    signal_recovery.host_pc = host_pc;
    signal_recovery_pending = 1;
    *guest_pc = 0;
    return true;
}

void latc_aot_v2_drain_signal_recovery(CPUState *cpu)
{
    if (!signal_recovery_pending) {
        return;
    }
    signal_recovery_pending = 0;

    const char *abort_reason = NULL;
    uintptr_t saved_host_pc = signal_recovery.host_pc;
    LatcAotV2SignalDiagnostic diagnostic;
    LatAotModuleInstanceV2 *instance = aot_v2_execution_instance;

    if (!instance) {
        abort_reason = "execution-instance-null";
    } else if (!instance->module) {
        abort_reason = "execution-module-null";
    } else if (saved_host_pc <
               (uintptr_t)instance->module->descriptor->text_begin) {
        abort_reason = "saved-host-pc-before-text";
    } else if (saved_host_pc >=
               (uintptr_t)instance->module->descriptor->text_end) {
        abort_reason = "saved-host-pc-after-text";
    } else if (saved_host_pc > UINTPTR_MAX - GETPC_ADJ) {
        abort_reason = "saved-host-pc-overflow";
    } else if (!latc_aot_v2_diagnose_host_pc(
                   cpu, saved_host_pc + GETPC_ADJ, &diagnostic)) {
        abort_reason = "diagnose-host-pc";
    }

    if (abort_reason) {
        bool report = signal_diagnostic_enabled();
        LatAotV2SignalDebugSnapshot snapshot = {0};
        if (report) {
            LatAotV2RuntimeModule *runtime_module =
                find_host_module(saved_host_pc);
            snapshot = capture_signal_debug_snapshot(instance,
                                                     runtime_module);
        }
        latc_aot_v2_release_current_execution(cpu);
        if (report) {
            report_signal_debug_snapshot(
                "recovery-abort",
                abort_reason ? abort_reason : "unknown",
                saved_host_pc, saved_host_pc, false, &snapshot);
        }
        abort();
    }
    target_ulong data[TARGET_INSN_START_WORDS] = {
        diagnostic.guest_pc,
        CC_OP_DYNAMIC,
    };
    TranslationBlock tb = { .pc = data[0] };
    restore_state_to_opc(cpu->env_ptr, &tb, data);
#ifdef CONFIG_LATX_OPT_PUSH_POP_TRANS
    ((CPUX86State *)cpu->env_ptr)->regs[R_ESP] +=
        diagnostic.stack_pointer_delta;
#endif
    latc_aot_v2_invalidate_range(
        cpu, diagnostic.guest_begin,
        diagnostic.guest_end - diagnostic.guest_begin,
        LATC_AOT_V2_INVALIDATE_CODE_WRITE);
}
