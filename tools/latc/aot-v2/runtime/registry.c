#include "registry.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct LatAotRegistrySnapshotV2 {
    size_t count;
    LatAotModuleInstanceV2 **instances;
    uint64_t *prefix_max_end;
    LatAotRegistrySnapshotV2 *retired_next;
};

static int compare_instance(const void *left, const void *right)
{
    const LatAotModuleInstanceV2 *a =
        *(LatAotModuleInstanceV2 *const *)left;
    const LatAotModuleInstanceV2 *b =
        *(LatAotModuleInstanceV2 *const *)right;
    if (a->guest_begin != b->guest_begin) {
        return a->guest_begin < b->guest_begin ? -1 : 1;
    }
    return a->guest_end < b->guest_end ? -1 : a->guest_end > b->guest_end;
}

static LatAotRegistrySnapshotV2 *snapshot_new(size_t count)
{
    LatAotRegistrySnapshotV2 *snapshot = calloc(1, sizeof(*snapshot));
    if (!snapshot) {
        return NULL;
    }
    if (count) {
        snapshot->instances = calloc(count, sizeof(*snapshot->instances));
        snapshot->prefix_max_end = calloc(count,
                                          sizeof(*snapshot->prefix_max_end));
        if (!snapshot->instances || !snapshot->prefix_max_end) {
            free(snapshot->prefix_max_end);
            free(snapshot->instances);
            free(snapshot);
            return NULL;
        }
    }
    snapshot->count = count;
    return snapshot;
}

static void snapshot_build_range_index(LatAotRegistrySnapshotV2 *snapshot)
{
    uint64_t maximum = 0;
    for (size_t i = 0; i < snapshot->count; i++) {
        if (snapshot->instances[i]->guest_end > maximum) {
            maximum = snapshot->instances[i]->guest_end;
        }
        snapshot->prefix_max_end[i] = maximum;
    }
}

static const LatAotTbV2 *instance_find_tb(
    const LatAotModuleInstanceV2 *instance, uint64_t guest_pc, uint32_t flags)
{
    if (guest_pc < instance->guest_load_bias) {
        return NULL;
    }
    uint64_t guest_rva = guest_pc - instance->guest_load_bias;
    const LatAotModuleV2 *module = instance->module->descriptor;
    size_t left = 0;
    size_t right = (size_t)(module->tb_end - module->tb_begin);
    while (left < right) {
        size_t middle = left + (right - left) / 2;
        const LatAotTbV2 *tb = &module->tb_begin[middle];
        if (tb->guest_rva < guest_rva ||
            (tb->guest_rva == guest_rva && tb->flags < flags)) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    if (left < (size_t)(module->tb_end - module->tb_begin)) {
        const LatAotTbV2 *tb = &module->tb_begin[left];
        if (tb->guest_rva == guest_rva && tb->flags == flags) {
            return tb;
        }
    }
    return NULL;
}

static void snapshot_free(LatAotRegistrySnapshotV2 *snapshot)
{
    if (!snapshot) {
        return;
    }
    free(snapshot->instances);
    free(snapshot->prefix_max_end);
    free(snapshot);
}

static void reclaim_retired_locked(LatAotRegistryV2 *registry)
{
    if (atomic_load_explicit(&registry->readers,
                             memory_order_seq_cst)) {
        return;
    }
    LatAotRegistrySnapshotV2 *snapshot = registry->retired;
    registry->retired = NULL;
    atomic_store_explicit(&registry->retired_count, 0,
                          memory_order_release);
    while (snapshot) {
        LatAotRegistrySnapshotV2 *next = snapshot->retired_next;
        snapshot_free(snapshot);
        snapshot = next;
    }
}

int lat_aot_v2_registry_init(LatAotRegistryV2 *registry)
{
    if (!registry) {
        errno = EINVAL;
        return -1;
    }
    memset(registry, 0, sizeof(*registry));
    if (pthread_mutex_init(&registry->write_lock, NULL)) {
        errno = EBUSY;
        return -1;
    }
    LatAotRegistrySnapshotV2 *empty = snapshot_new(0);
    if (!empty) {
        pthread_mutex_destroy(&registry->write_lock);
        return -1;
    }
    atomic_store_explicit(&registry->current, empty, memory_order_release);
    return 0;
}

void lat_aot_v2_registry_destroy(LatAotRegistryV2 *registry)
{
    if (!registry) {
        return;
    }
    snapshot_free(atomic_load_explicit(&registry->current,
                                      memory_order_acquire));
    LatAotRegistrySnapshotV2 *snapshot = registry->retired;
    while (snapshot) {
        LatAotRegistrySnapshotV2 *next = snapshot->retired_next;
        snapshot_free(snapshot);
        snapshot = next;
    }
    pthread_mutex_destroy(&registry->write_lock);
    memset(registry, 0, sizeof(*registry));
}

static int publish_snapshot(LatAotRegistryV2 *registry,
                            LatAotRegistrySnapshotV2 *replacement)
{
    LatAotRegistrySnapshotV2 *old = atomic_exchange_explicit(
        &registry->current, replacement, memory_order_seq_cst);
    old->retired_next = registry->retired;
    registry->retired = old;
    atomic_fetch_add_explicit(&registry->retired_count, 1,
                              memory_order_release);
    reclaim_retired_locked(registry);
    return 0;
}

int lat_aot_v2_registry_register(LatAotRegistryV2 *registry,
                                 LatAotModuleInstanceV2 *instance)
{
    if (!registry || !instance || !instance->module ||
        !instance->module->descriptor ||
        instance->guest_begin >= instance->guest_end) {
        errno = EINVAL;
        return -1;
    }
    if (instance->exec_range_count > LAT_AOT_V2_MAX_EXEC_RANGES) {
        errno = EINVAL;
        return -1;
    }
    for (uint32_t i = 0; i < instance->exec_range_count; i++) {
        const LatAotGuestRangeV2 *range = &instance->exec_ranges[i];
        if (range->begin < instance->guest_begin ||
            range->end > instance->guest_end || range->begin >= range->end ||
            (i && instance->exec_ranges[i - 1].end > range->begin)) {
            errno = EINVAL;
            return -1;
        }
    }
    pthread_mutex_lock(&registry->write_lock);
    LatAotRegistrySnapshotV2 *old = atomic_load_explicit(
        &registry->current, memory_order_acquire);
    LatAotRegistrySnapshotV2 *replacement = snapshot_new(old->count + 1);
    if (!replacement) {
        pthread_mutex_unlock(&registry->write_lock);
        return -1;
    }
    memcpy(replacement->instances, old->instances,
           old->count * sizeof(*old->instances));
    replacement->instances[old->count] = instance;
    qsort(replacement->instances, replacement->count,
          sizeof(*replacement->instances), compare_instance);
    snapshot_build_range_index(replacement);
    atomic_store_explicit(&instance->active, 1, memory_order_release);
    if (!atomic_load_explicit(&instance->generation, memory_order_relaxed)) {
        atomic_store_explicit(&instance->generation, 1, memory_order_relaxed);
    }
    publish_snapshot(registry, replacement);
    pthread_mutex_unlock(&registry->write_lock);
    return 0;
}

int lat_aot_v2_registry_deactivate(LatAotRegistryV2 *registry,
                                   LatAotModuleInstanceV2 *instance)
{
    if (!registry || !instance) {
        errno = EINVAL;
        return -1;
    }
    pthread_mutex_lock(&registry->write_lock);
    LatAotRegistrySnapshotV2 *old = atomic_load_explicit(
        &registry->current, memory_order_acquire);
    size_t retained = 0;
    for (size_t i = 0; i < old->count; i++) {
        retained += old->instances[i] != instance;
    }
    if (retained == old->count) {
        pthread_mutex_unlock(&registry->write_lock);
        errno = ENOENT;
        return -1;
    }
    LatAotRegistrySnapshotV2 *replacement = snapshot_new(retained);
    if (!replacement) {
        pthread_mutex_unlock(&registry->write_lock);
        return -1;
    }
    for (size_t i = 0, output = 0; i < old->count; i++) {
        if (old->instances[i] != instance) {
            replacement->instances[output++] = old->instances[i];
        }
    }
    snapshot_build_range_index(replacement);
    atomic_store_explicit(&instance->active, 0, memory_order_release);
    atomic_fetch_add_explicit(&instance->generation, 1, memory_order_acq_rel);
    publish_snapshot(registry, replacement);
    pthread_mutex_unlock(&registry->write_lock);
    return 0;
}

static int instance_exec_range_overlaps(const LatAotModuleInstanceV2 *instance,
                                        uint64_t guest_begin,
                                        uint64_t guest_end)
{
    if (!instance->exec_range_count) {
        return guest_begin < instance->guest_end &&
               guest_end > instance->guest_begin;
    }
    for (uint32_t i = 0; i < instance->exec_range_count; i++) {
        if (guest_begin < instance->exec_ranges[i].end &&
            guest_end > instance->exec_ranges[i].begin) {
            return 1;
        }
    }
    return 0;
}

int lat_aot_v2_registry_deactivate_range(LatAotRegistryV2 *registry,
                                         uint64_t guest_begin,
                                         uint64_t guest_end,
                                         size_t *deactivated)
{
    if (!registry || guest_begin >= guest_end) {
        errno = EINVAL;
        return -1;
    }
    pthread_mutex_lock(&registry->write_lock);
    LatAotRegistrySnapshotV2 *old = atomic_load_explicit(
        &registry->current, memory_order_acquire);
    size_t removed = 0;
    for (size_t i = 0; i < old->count; i++) {
        removed += instance_exec_range_overlaps(old->instances[i],
                                                guest_begin, guest_end);
    }
    if (!removed) {
        if (deactivated) {
            *deactivated = 0;
        }
        pthread_mutex_unlock(&registry->write_lock);
        return 0;
    }
    LatAotRegistrySnapshotV2 *replacement = snapshot_new(old->count - removed);
    if (!replacement) {
        pthread_mutex_unlock(&registry->write_lock);
        return -1;
    }
    for (size_t i = 0, output = 0; i < old->count; i++) {
        LatAotModuleInstanceV2 *instance = old->instances[i];
        if (instance_exec_range_overlaps(instance, guest_begin, guest_end)) {
            atomic_store_explicit(&instance->active, 0, memory_order_release);
            atomic_fetch_add_explicit(&instance->generation, 1,
                                      memory_order_acq_rel);
        } else {
            replacement->instances[output++] = instance;
        }
    }
    snapshot_build_range_index(replacement);
    publish_snapshot(registry, replacement);
    if (deactivated) {
        *deactivated = removed;
    }
    pthread_mutex_unlock(&registry->write_lock);
    return 0;
}

int lat_aot_v2_registry_lookup(LatAotRegistryV2 *registry,
                               uint64_t guest_pc, uint32_t flags,
                               LatAotTargetV2 *target)
{
    if (!registry || !target) {
        errno = EINVAL;
        return -1;
    }
    memset(target, 0, sizeof(*target));
    /*
     * These operations are sequentially consistent on purpose.  A writer
     * exchanges current before observing readers.  Therefore it either sees
     * this reader, or this reader observes the replacement snapshot.
     */
    atomic_fetch_add_explicit(&registry->readers, 1,
                              memory_order_seq_cst);
    LatAotRegistrySnapshotV2 *snapshot = atomic_load_explicit(
        &registry->current, memory_order_seq_cst);
    size_t left = 0;
    size_t right = snapshot->count;
    while (left < right) {
        size_t middle = left + (right - left) / 2;
        if (snapshot->instances[middle]->guest_begin <= guest_pc) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    while (left) {
        size_t index = --left;
        LatAotModuleInstanceV2 *instance = snapshot->instances[index];
        if (guest_pc < instance->guest_end &&
            atomic_load_explicit(&instance->active, memory_order_acquire)) {
            const LatAotTbV2 *tb = instance_find_tb(instance, guest_pc, flags);
            if (tb) {
                atomic_fetch_add_explicit(&instance->readers, 1,
                                          memory_order_seq_cst);
                if (!atomic_load_explicit(&instance->active,
                                          memory_order_acquire)) {
                    atomic_fetch_sub_explicit(&instance->readers, 1,
                                              memory_order_seq_cst);
                } else {
                    target->host_address =
                        instance->module->descriptor->text_begin +
                        tb->host_offset;
                    target->instance = instance;
                    target->generation = atomic_load_explicit(
                        &instance->generation, memory_order_acquire);
                    atomic_fetch_sub_explicit(&registry->readers, 1,
                                              memory_order_seq_cst);
                    return 0;
                }
            }
        }
        if (!index || snapshot->prefix_max_end[index - 1] <= guest_pc) {
            break;
        }
    }
    atomic_fetch_sub_explicit(&registry->readers, 1,
                              memory_order_seq_cst);
    errno = ENOENT;
    return -1;
}

void lat_aot_v2_registry_target_release(LatAotTargetV2 *target)
{
    if (!target || !target->instance) {
        return;
    }
    atomic_fetch_sub_explicit(&target->instance->readers, 1,
                              memory_order_seq_cst);
    target->instance = NULL;
}

void lat_aot_v2_registry_drain(LatAotRegistryV2 *registry)
{
    if (!registry) {
        return;
    }
    pthread_mutex_lock(&registry->write_lock);
    reclaim_retired_locked(registry);
    pthread_mutex_unlock(&registry->write_lock);
}

void lat_aot_v2_registry_counts(const LatAotRegistryV2 *registry,
                                LatAotRegistryCountsV2 *counts)
{
    if (!registry || !counts) {
        return;
    }
    counts->current_snapshots = atomic_load_explicit(
        &registry->current, memory_order_acquire) != NULL;
    counts->retired_snapshots = atomic_load_explicit(
        &registry->retired_count, memory_order_acquire);
    counts->readers = atomic_load_explicit(&registry->readers,
                                            memory_order_acquire);
}

int lat_aot_v2_context_apply_guest_slots(const LatAotModuleV2 *module,
                                         uint64_t guest_load_bias,
                                         void *jump_cache)
{
    if (!module || !jump_cache) {
        errno = EINVAL;
        return -1;
    }
    uintptr_t begin = (uintptr_t)module->guest_slot_begin;
    uintptr_t end = (uintptr_t)module->guest_slot_end;
    if ((!begin != !end) || end < begin ||
        (end - begin) % sizeof(LatAotGuestSlotV2)) {
        errno = EINVAL;
        return -1;
    }
    size_t count = (end - begin) / sizeof(LatAotGuestSlotV2);
    if (module->module_flags & (LAT_AOT_MODULE_TWO_LEVEL_GUEST_SLOTS |
                                LAT_AOT_MODULE_THREE_LEVEL_GUEST_SLOTS)) {
        errno = ENOTSUP;
        return -1;
    }
    if (count > LAT_AOT_V2_CONTEXT_GUEST_SLOT_LIMIT) {
        errno = E2BIG;
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        const LatAotGuestSlotV2 *slot = &module->guest_slot_begin[i];
        if (slot->reserved || slot->fp_offset != -(int32_t)((i + 1) * 8) ||
            guest_load_bias > UINT64_MAX - slot->guest_rva) {
            errno = ENOEXEC;
            return -1;
        }
        *(uint64_t *)((unsigned char *)jump_cache + slot->fp_offset) =
            guest_load_bias + slot->guest_rva;
    }
    return 0;
}

int lat_aot_v2_context_apply_guest_table(const LatAotModuleV2 *module,
                                         uint64_t guest_load_bias,
                                         void *jump_cache,
                                         uint64_t *page_storage,
                                         size_t page_storage_count,
                                         size_t *context_slot_count)
{
    if (!module || !jump_cache || !context_slot_count) {
        errno = EINVAL;
        return -1;
    }
    uintptr_t begin = (uintptr_t)module->guest_slot_begin;
    uintptr_t end = (uintptr_t)module->guest_slot_end;
    if ((!begin != !end) || end < begin ||
        (end - begin) % sizeof(LatAotGuestSlotV2)) {
        errno = EINVAL;
        return -1;
    }
    size_t count = (end - begin) / sizeof(LatAotGuestSlotV2);
    int two_level = !!(module->module_flags &
                       LAT_AOT_MODULE_TWO_LEVEL_GUEST_SLOTS);
    int three_level = !!(module->module_flags &
                         LAT_AOT_MODULE_THREE_LEVEL_GUEST_SLOTS);
    if (two_level && three_level) {
        errno = ENOEXEC;
        return -1;
    }
    if (!two_level && !three_level) {
        *context_slot_count = count;
        return lat_aot_v2_context_apply_guest_slots(module, guest_load_bias,
                                                    jump_cache);
    }
    size_t limit = three_level ? LAT_AOT_V2_GUEST_ADDRESS_LIMIT :
                                LAT_AOT_V2_TWO_LEVEL_GUEST_ADDRESS_LIMIT;
    if (count > limit) {
        errno = E2BIG;
        return -1;
    }
    size_t leaf_page_count =
        (count + LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT - 1) /
        LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
    size_t root_count = three_level ?
        (leaf_page_count + LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT - 1) /
            LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT :
        leaf_page_count;
    size_t root_storage_count = three_level ?
        root_count * LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT : 0;
    size_t required = root_storage_count +
        leaf_page_count * LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
    if (required && (!page_storage || page_storage_count < required)) {
        errno = ENOSPC;
        return -1;
    }
    if (required) {
        memset(page_storage, 0, required * sizeof(*page_storage));
    }
    uint64_t *context = jump_cache;
    for (size_t root = 0; root < root_count; root++) {
        context[-(ptrdiff_t)(root + 1)] = (uintptr_t)&page_storage[
            root * LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT];
    }
    if (three_level) {
        for (size_t page = 0; page < leaf_page_count; page++) {
            size_t root = page / LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
            size_t middle = page % LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
            page_storage[root * LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT + middle] =
                (uintptr_t)&page_storage[root_storage_count +
                    page * LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT];
        }
    }
    for (size_t i = 0; i < count; i++) {
        const LatAotGuestSlotV2 *slot = &module->guest_slot_begin[i];
        size_t page = i / LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
        size_t entry = i % LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
        size_t root = three_level ?
            page / LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT : page;
        size_t middle = page % LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
        uint32_t reserved = three_level ?
            (uint32_t)((middle * 8) << 16) | (uint32_t)(entry * 8) :
            (uint32_t)(entry * 8);
        if (slot->fp_offset != -(int32_t)((root + 1) * 8) ||
            slot->reserved != reserved ||
            guest_load_bias > UINT64_MAX - slot->guest_rva) {
            errno = ENOEXEC;
            return -1;
        }
        size_t value_index = (three_level ? root_storage_count : 0) +
            page * LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT + entry;
        page_storage[value_index] = guest_load_bias + slot->guest_rva;
    }
    *context_slot_count = root_count;
    return 0;
}
