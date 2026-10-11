#ifndef LAT_AOT_V2_REGISTRY_H
#define LAT_AOT_V2_REGISTRY_H

#include "lat-aot-v2.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

typedef struct LatAotLoadedModuleV2 {
    void *dl_handle;
    int backing_fd;
    const LatAotModuleV2 *descriptor;
    LatAotNoteV2 note;
} LatAotLoadedModuleV2;

#define LAT_AOT_V2_MAX_EXEC_RANGES 16

typedef struct LatAotGuestRangeV2 {
    uint64_t begin;
    uint64_t end;
} LatAotGuestRangeV2;

typedef struct LatAotModuleInstanceV2 {
    const LatAotLoadedModuleV2 *module;
    uint64_t guest_load_bias;
    uint64_t guest_begin;
    uint64_t guest_end;
    uint32_t exec_range_count;
    LatAotGuestRangeV2 exec_ranges[LAT_AOT_V2_MAX_EXEC_RANGES];
    _Atomic uint64_t generation;
    _Atomic int active;
    _Atomic uint32_t readers;
} LatAotModuleInstanceV2;

/*
 * Registered instances and their immutable range fields must remain alive
 * until registry destruction. Readers may still hold an older snapshot after
 * a writer has published a replacement.
 */

typedef struct LatAotRegistrySnapshotV2 LatAotRegistrySnapshotV2;

typedef struct LatAotRegistryV2 {
    pthread_mutex_t write_lock;
    _Atomic(LatAotRegistrySnapshotV2 *) current;
    LatAotRegistrySnapshotV2 *retired;
    _Atomic size_t readers;
    _Atomic size_t retired_count;
} LatAotRegistryV2;

typedef struct LatAotRegistryCountsV2 {
    size_t current_snapshots;
    size_t retired_snapshots;
    size_t readers;
} LatAotRegistryCountsV2;

typedef struct LatAotTargetV2 {
    const void *host_address;
    LatAotModuleInstanceV2 *instance;
    uint64_t generation;
} LatAotTargetV2;

int lat_aot_v2_registry_init(LatAotRegistryV2 *registry);
void lat_aot_v2_registry_destroy(LatAotRegistryV2 *registry);
int lat_aot_v2_registry_register(LatAotRegistryV2 *registry,
                                 LatAotModuleInstanceV2 *instance);
int lat_aot_v2_registry_deactivate(LatAotRegistryV2 *registry,
                                   LatAotModuleInstanceV2 *instance);
int lat_aot_v2_registry_deactivate_range(LatAotRegistryV2 *registry,
                                         uint64_t guest_begin,
                                         uint64_t guest_end,
                                         size_t *deactivated);
int lat_aot_v2_registry_lookup(LatAotRegistryV2 *registry,
                               uint64_t guest_pc, uint32_t flags,
                               LatAotTargetV2 *target);
void lat_aot_v2_registry_target_release(LatAotTargetV2 *target);
void lat_aot_v2_registry_drain(LatAotRegistryV2 *registry);
void lat_aot_v2_registry_counts(const LatAotRegistryV2 *registry,
                                LatAotRegistryCountsV2 *counts);
int lat_aot_v2_context_apply_guest_slots(const LatAotModuleV2 *module,
                                         uint64_t guest_load_bias,
                                         void *jump_cache);
int lat_aot_v2_context_apply_guest_table(const LatAotModuleV2 *module,
                                         uint64_t guest_load_bias,
                                         void *jump_cache,
                                         uint64_t *page_storage,
                                         size_t page_storage_count,
                                         size_t *context_slot_count);

#endif
