#include "registry.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct LookupThread {
    LatAotRegistryV2 *registry;
    _Atomic int *stop;
    _Atomic int *failed;
} LookupThread;

static void *lookup_unchanged_instance(void *opaque)
{
    LookupThread *thread = opaque;
    while (!atomic_load_explicit(thread->stop, memory_order_acquire)) {
        LatAotTargetV2 target;
        if (lat_aot_v2_registry_lookup(thread->registry, 0x701000,
                                       LAT_AOT_TB_CODE64 |
                                       LAT_AOT_TB_PARALLEL, &target) ||
            target.host_address == NULL || target.instance == NULL) {
            atomic_store_explicit(thread->failed, 1, memory_order_release);
            break;
        }
        lat_aot_v2_registry_target_release(&target);
    }
    return NULL;
}

int main(void)
{
    static const unsigned char text[32];
    static const LatAotTbV2 tbs[] = {
        { .guest_rva = 0x1000, .host_offset = 4, .host_size = 4,
          .flags = LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL },
    };
    static const LatAotGuestSlotV2 guest_slots[] = {
        { .guest_rva = 0x1234, .fp_offset = -8 },
        { .guest_rva = 0x5678, .fp_offset = -16 },
    };
    static const LatAotModuleV2 descriptor = {
        .text_begin = text,
        .text_end = text + sizeof(text),
        .tb_begin = tbs,
        .tb_end = tbs + 1,
        .guest_slot_begin = guest_slots,
        .guest_slot_end = guest_slots + 2,
    };
    static const LatAotLoadedModuleV2 module = {
        .descriptor = &descriptor,
    };
    static const LatAotTbV2 shard_tbs[] = {
        { .guest_rva = 0x2000, .host_offset = 12, .host_size = 4,
          .flags = LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL },
    };
    static const LatAotModuleV2 shard_descriptor = {
        .text_begin = text,
        .text_end = text + sizeof(text),
        .tb_begin = shard_tbs,
        .tb_end = shard_tbs + 1,
    };
    static const LatAotLoadedModuleV2 shard_module = {
        .descriptor = &shard_descriptor,
    };
    LatAotModuleInstanceV2 first = {
        .module = &module,
        .guest_load_bias = 0x400000,
        .guest_begin = 0x400000,
        .guest_end = 0x500000,
        .exec_range_count = 1,
        .exec_ranges = { { .begin = 0x401000, .end = 0x410000 } },
    };
    LatAotModuleInstanceV2 second = {
        .module = &module,
        .guest_load_bias = 0x700000,
        .guest_begin = 0x700000,
        .guest_end = 0x800000,
        .exec_range_count = 1,
        .exec_ranges = { { .begin = 0x701000, .end = 0x710000 } },
    };
    LatAotModuleInstanceV2 overlap = {
        .module = &module,
        .guest_load_bias = 0x400000,
        .guest_begin = 0x480000,
        .guest_end = 0x580000,
    };
    LatAotModuleInstanceV2 shard = {
        .module = &shard_module,
        .guest_load_bias = 0x400000,
        .guest_begin = 0x400000,
        .guest_end = 0x500000,
        .exec_range_count = 1,
        .exec_ranges = { { .begin = 0x401000, .end = 0x410000 } },
    };
    LatAotRegistryV2 registry;
    if (lat_aot_v2_registry_init(&registry) ||
        lat_aot_v2_registry_register(&registry, &first) ||
        lat_aot_v2_registry_register(&registry, &second)) {
        perror("registry setup");
        return 1;
    }
    LatAotTargetV2 target;
    if (lat_aot_v2_registry_lookup(&registry, 0x401000,
                                   LAT_AOT_TB_CODE64, &target) == 0) {
        lat_aot_v2_registry_target_release(&target);
        fprintf(stderr, "registry accepted removed non-parallel variant\n");
        return 1;
    }
    if (lat_aot_v2_registry_lookup(&registry, 0x401000,
                                   LAT_AOT_TB_CODE64 |
                                   LAT_AOT_TB_PARALLEL, &target) ||
        target.host_address != text + 4 || target.instance != &first) {
        fprintf(stderr, "registry lookup failed\n");
        return 1;
    }
    lat_aot_v2_registry_target_release(&target);
    if (lat_aot_v2_registry_register(&registry, &shard) ||
        lat_aot_v2_registry_lookup(&registry, 0x402000,
                                   LAT_AOT_TB_CODE64 |
                                   LAT_AOT_TB_PARALLEL, &target) ||
        target.host_address != text + 12 || target.instance != &shard) {
        fprintf(stderr, "registry combined shard lookup failed\n");
        return 1;
    }
    lat_aot_v2_registry_target_release(&target);
    if (lat_aot_v2_registry_deactivate(&registry, &shard)) {
        fprintf(stderr, "registry shard deactivation failed\n");
        return 1;
    }
    if (lat_aot_v2_registry_lookup(&registry, 0x701000,
                                   LAT_AOT_TB_CODE64 |
                                   LAT_AOT_TB_PARALLEL, &target) ||
        target.host_address != text + 4 || target.instance != &second) {
        fprintf(stderr, "registry parallel lookup failed\n");
        return 1;
    }
    lat_aot_v2_registry_target_release(&target);
    uint64_t context_words[4] = {0};
    void *jump_cache = &context_words[2];
    if (lat_aot_v2_context_apply_guest_slots(&descriptor, 0x400000,
                                             jump_cache) ||
        context_words[1] != 0x401234 || context_words[0] != 0x405678) {
        fprintf(stderr, "guest context slots were not populated\n");
        return 1;
    }
    LatAotGuestSlotV2 *large_slots = calloc(257, sizeof(*large_slots));
    uint64_t *pages = calloc(512, sizeof(*pages));
    uint64_t large_context[LAT_AOT_V2_CONTEXT_GUEST_SLOT_LIMIT] = {0};
    if (!large_slots || !pages) {
        perror("allocate two-level guest table test");
        return 1;
    }
    for (size_t i = 0; i < 257; i++) {
        large_slots[i] = (LatAotGuestSlotV2) {
            .guest_rva = 0x1000 + i * 8,
            .fp_offset = -(int32_t)((i / 256 + 1) * 8),
            .reserved = (i % 256) * 8,
        };
    }
    LatAotModuleV2 large_descriptor = {
        .module_flags = LAT_AOT_MODULE_TWO_LEVEL_GUEST_SLOTS,
        .guest_slot_begin = large_slots,
        .guest_slot_end = large_slots + 257,
    };
    size_t context_slot_count = 0;
    if (lat_aot_v2_context_apply_guest_table(
            &large_descriptor, 0x400000,
            large_context + LAT_AOT_V2_CONTEXT_GUEST_SLOT_LIMIT,
            pages, 512, &context_slot_count) || context_slot_count != 2 ||
        large_context[255] != (uintptr_t)pages ||
        large_context[254] != (uintptr_t)(pages + 256) ||
        pages[0] != 0x401000 || pages[256] != 0x401800) {
        fprintf(stderr, "two-level guest context table was not populated\n");
        return 1;
    }
    free(pages);
    free(large_slots);
    const size_t three_level_count =
        LAT_AOT_V2_TWO_LEVEL_GUEST_ADDRESS_LIMIT + 1;
    const size_t three_level_leaf_pages =
        (three_level_count + LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT - 1) /
        LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
    const size_t three_level_roots =
        (three_level_leaf_pages + LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT - 1) /
        LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
    const size_t three_level_storage_count =
        (three_level_leaf_pages + three_level_roots) *
        LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
    large_slots = calloc(three_level_count, sizeof(*large_slots));
    pages = calloc(three_level_storage_count, sizeof(*pages));
    if (!large_slots || !pages) {
        perror("allocate three-level guest table test");
        return 1;
    }
    for (size_t i = 0; i < three_level_count; i++) {
        size_t page = i / LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
        size_t root = page / LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
        size_t middle = page % LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
        size_t entry = i % LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
        large_slots[i] = (LatAotGuestSlotV2) {
            .guest_rva = 0x1000 + i * 8,
            .fp_offset = -(int32_t)((root + 1) * 8),
            .reserved = (uint32_t)((middle * 8) << 16) |
                        (uint32_t)(entry * 8),
        };
    }
    large_descriptor = (LatAotModuleV2) {
        .module_flags = LAT_AOT_MODULE_THREE_LEVEL_GUEST_SLOTS,
        .guest_slot_begin = large_slots,
        .guest_slot_end = large_slots + three_level_count,
    };
    context_slot_count = 0;
    memset(large_context, 0, sizeof(large_context));
    if (lat_aot_v2_context_apply_guest_table(
            &large_descriptor, 0x400000,
            large_context + LAT_AOT_V2_CONTEXT_GUEST_SLOT_LIMIT,
            pages, three_level_storage_count, &context_slot_count) ||
        context_slot_count != 2 ||
        large_context[255] != (uintptr_t)pages ||
        large_context[254] != (uintptr_t)(pages + 256) ||
        pages[0] != (uintptr_t)(pages + 512) ||
        pages[256] != (uintptr_t)(pages + 512 + 256 * 256) ||
        pages[512] != 0x401000 || pages[512 + 256 * 256] != 0x481000) {
        fprintf(stderr, "three-level guest context table was not populated\n");
        return 1;
    }
    free(pages);
    free(large_slots);
    if (lat_aot_v2_registry_register(&registry, &overlap)) {
        fprintf(stderr, "overlapping shard instance was rejected\n");
        return 1;
    }
    if (lat_aot_v2_registry_deactivate(&registry, &overlap)) {
        fprintf(stderr, "overlapping shard deactivation failed\n");
        return 1;
    }
    uint64_t generation = atomic_load(&first.generation);
    size_t deactivated = 99;
    if (lat_aot_v2_registry_deactivate_range(&registry, 0x480000, 0x481000,
                                             &deactivated) || deactivated ||
        lat_aot_v2_registry_lookup(&registry, 0x401000,
                                   LAT_AOT_TB_CODE64 |
                                   LAT_AOT_TB_PARALLEL, &target)) {
        fprintf(stderr, "registry no-op range deactivation failed\n");
        return 1;
    }
    lat_aot_v2_registry_target_release(&target);
    if (lat_aot_v2_registry_deactivate_range(&registry, 0x401800, 0x401900,
                                             &deactivated) ||
        deactivated != 1 ||
        atomic_load(&first.generation) != generation + 1 ||
        lat_aot_v2_registry_lookup(&registry, 0x401000,
                                   LAT_AOT_TB_CODE64 |
                                   LAT_AOT_TB_PARALLEL, &target) == 0 ||
        lat_aot_v2_registry_lookup(&registry, 0x701000,
                                   LAT_AOT_TB_CODE64 |
                                   LAT_AOT_TB_PARALLEL, &target)) {
        fprintf(stderr, "registry range deactivation failed\n");
        return 1;
    }
    lat_aot_v2_registry_target_release(&target);

    enum { LOOKUP_THREADS = 4, INVALIDATION_ROUNDS = 2000 };
    pthread_t lookup_threads[LOOKUP_THREADS];
    _Atomic int stop = 0;
    _Atomic int failed = 0;
    LookupThread thread = {
        .registry = &registry,
        .stop = &stop,
        .failed = &failed,
    };
    LatAotModuleInstanceV2 *transients = calloc(
        INVALIDATION_ROUNDS, sizeof(*transients));
    if (!transients) {
        perror("calloc transient instances");
        return 1;
    }
    for (size_t i = 0; i < LOOKUP_THREADS; i++) {
        if (pthread_create(&lookup_threads[i], NULL,
                           lookup_unchanged_instance, &thread)) {
            perror("pthread_create");
            return 1;
        }
    }
    for (uint64_t i = 0; i < INVALIDATION_ROUNDS; i++) {
        uint64_t base = 0x900000 + i * 0x200000;
        LatAotModuleInstanceV2 *transient = &transients[i];
        *transient = (LatAotModuleInstanceV2) {
            .module = &module,
            .guest_load_bias = base,
            .guest_begin = base,
            .guest_end = base + 0x100000,
            .exec_range_count = 1,
            .exec_ranges = { { .begin = base + 0x1000,
                               .end = base + 0x10000 } },
        };
        int register_result = lat_aot_v2_registry_register(&registry,
                                                            transient);
        int register_errno = errno;
        int deactivate_result = register_result ? -1 :
            lat_aot_v2_registry_deactivate_range(
                &registry, base + 0x1800, base + 0x1900, &deactivated);
        if (register_result || deactivate_result || deactivated != 1 ||
            atomic_load(&transient->active) ||
            atomic_load(&transient->generation) != 2 ||
            atomic_load_explicit(&failed, memory_order_acquire)) {
            fprintf(stderr,
                    "concurrent registry invalidation failed round=%llu "
                    "register=%d errno=%d deactivate=%d removed=%zu "
                    "active=%d generation=%llu lookup_failed=%d\n",
                    (unsigned long long)i, register_result, register_errno,
                    deactivate_result, deactivated,
                    atomic_load(&transient->active),
                    (unsigned long long)atomic_load(&transient->generation),
                    atomic_load_explicit(&failed, memory_order_acquire));
            atomic_store_explicit(&failed, 1, memory_order_release);
            break;
        }
    }
    atomic_store_explicit(&stop, 1, memory_order_release);
    for (size_t i = 0; i < LOOKUP_THREADS; i++) {
        pthread_join(lookup_threads[i], NULL);
    }
    if (atomic_load_explicit(&failed, memory_order_acquire)) {
        free(transients);
        return 1;
    }
    lat_aot_v2_registry_drain(&registry);
    LatAotRegistryCountsV2 counts;
    lat_aot_v2_registry_counts(&registry, &counts);
    if (counts.current_snapshots != 1 || counts.retired_snapshots ||
        counts.readers || atomic_load_explicit(&second.readers,
                                                memory_order_acquire)) {
        fprintf(stderr,
                "registry reclamation failed current=%zu retired=%zu "
                "readers=%zu instance_readers=%u\n",
                counts.current_snapshots, counts.retired_snapshots,
                counts.readers, atomic_load_explicit(&second.readers,
                                                     memory_order_acquire));
        free(transients);
        return 1;
    }
    lat_aot_v2_registry_destroy(&registry);
    free(transients);
    puts("test-aot-v2-registry: PASS");
    return 0;
}
