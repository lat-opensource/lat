/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef KZT_PUBLIC_LOADER_OBSERVER_H
#define KZT_PUBLIC_LOADER_OBSERVER_H

#include <stddef.h>
#include <stdint.h>

#define KZT_X86_64_DT_NULL 0
#define KZT_X86_64_DT_DEBUG 21
#define KZT_PUBLIC_LOADER_MAX_OBJECTS 256

typedef enum kzt_loader_debug_state {
    KZT_LOADER_DEBUG_CONSISTENT = 0,
    KZT_LOADER_DEBUG_ADD = 1,
    KZT_LOADER_DEBUG_DELETE = 2,
} kzt_loader_debug_state_t;

/*
 * These are x86_64 wire layouts read from guest memory.  Only fields from
 * the public debugger interface are represented here.
 */
typedef struct kzt_x86_64_dynamic_entry {
    int64_t tag;
    uint64_t value;
} kzt_x86_64_dynamic_entry_t;

typedef struct kzt_x86_64_r_debug {
    int32_t version;
    uint32_t version_padding;
    uint64_t map;
    uint64_t brk;
    int32_t state;
    uint32_t state_padding;
    uint64_t loader_base;
} kzt_x86_64_r_debug_t;

typedef struct kzt_x86_64_link_map_prefix {
    uint64_t load_bias;
    uint64_t name;
    uint64_t dynamic_addr;
    uint64_t next;
    uint64_t previous;
} kzt_x86_64_link_map_prefix_t;

typedef int (*kzt_public_loader_read_fn)(uintptr_t guest_addr,
                                         void *dst,
                                         size_t size,
                                         void *opaque);

typedef struct kzt_public_loader_reader {
    kzt_public_loader_read_fn read_memory;
    void *opaque;
} kzt_public_loader_reader_t;

typedef struct kzt_public_loader_object {
    uintptr_t link_map_addr;
    uintptr_t load_bias;
    uintptr_t name_addr;
    uintptr_t dynamic_addr;
    uintptr_t next_addr;
    uintptr_t previous_addr;
} kzt_public_loader_object_t;

typedef struct kzt_public_loader_tls_object {
    uintptr_t link_map_addr;
    uintptr_t load_bias;
    uintptr_t dynamic_addr;
    uintptr_t image_addr;
    size_t file_size;
    size_t memory_size;
    size_t alignment;
    size_t first_byte_offset;
    intptr_t static_tls_offset;
    uintptr_t static_tls_symbol_value;
    uintptr_t static_tls_symbol_name_addr;
    size_t module_id;
    uint64_t load_generation;
    int static_tls_offset_valid;
    int static_tls_offset_needs_validation;
    int static_tls_offset_pending;
} kzt_public_loader_tls_object_t;

typedef int (*kzt_public_loader_visit_fn)(
    const kzt_public_loader_object_t *object,
    void *opaque);

typedef enum kzt_public_loader_result {
    KZT_PUBLIC_LOADER_OK = 0,
    KZT_PUBLIC_LOADER_BUSY,
    KZT_PUBLIC_LOADER_INVALID_INPUT,
    KZT_PUBLIC_LOADER_NOT_FOUND,
    KZT_PUBLIC_LOADER_READ_ERROR,
    KZT_PUBLIC_LOADER_INVALID_STATE,
    KZT_PUBLIC_LOADER_CYCLE,
    KZT_PUBLIC_LOADER_LIMIT,
    KZT_PUBLIC_LOADER_VISITOR_ERROR,
    KZT_PUBLIC_LOADER_OVERFLOW,
} kzt_public_loader_result_t;

typedef struct kzt_public_loader_observer {
    uintptr_t r_debug_addr;
    uintptr_t r_brk_addr;
    uintptr_t live_maps[KZT_PUBLIC_LOADER_MAX_OBJECTS];
    uint64_t live_map_generations[KZT_PUBLIC_LOADER_MAX_OBJECTS];
    size_t live_map_count;
    uint64_t next_load_generation;
    uintptr_t processed_maps[KZT_PUBLIC_LOADER_MAX_OBJECTS];
    size_t processed_map_count;
    uintptr_t fallback_reported_maps[KZT_PUBLIC_LOADER_MAX_OBJECTS];
    size_t fallback_reported_map_count;
    int active;
} kzt_public_loader_observer_t;

void kzt_public_loader_observer_reset(
    kzt_public_loader_observer_t *observer);

/*
 * Record an object already seen by the pre-protection path.  This prevents
 * the initial public snapshot from replaying it; successful KZT processing
 * must be recorded separately with mark_processed().
 */
kzt_public_loader_result_t kzt_public_loader_observer_remember(
    kzt_public_loader_observer_t *observer,
    uintptr_t link_map_addr);

int kzt_public_loader_observer_has_map(
    const kzt_public_loader_observer_t *observer,
    uintptr_t link_map_addr);

/*
 * Track successful KZT processing separately from the loader's live-object
 * inventory.  Merely observing an object must not suppress a later safe
 * pre-protection retry.
 */
kzt_public_loader_result_t kzt_public_loader_observer_mark_processed(
    kzt_public_loader_observer_t *observer,
    uintptr_t link_map_addr);

int kzt_public_loader_observer_is_processed(
    const kzt_public_loader_observer_t *observer,
    uintptr_t link_map_addr);

/* Return nonzero once for each live object generation. */
int kzt_public_loader_observer_mark_fallback_reported(
    kzt_public_loader_observer_t *observer,
    uintptr_t link_map_addr);

int kzt_public_loader_observer_fallback_was_reported(
    const kzt_public_loader_observer_t *observer,
    uintptr_t link_map_addr);

/*
 * Inspect an object's in-memory ELF program headers without reopening its
 * path.  A successful result distinguishes RELRO-present from no-RELRO.
 */
kzt_public_loader_result_t kzt_public_loader_object_has_relro(
    const kzt_public_loader_object_t *object,
    const kzt_public_loader_reader_t *reader,
    int *has_relro);

/*
 * While r_debug reports RT_ADD or RT_CONSISTENT, identify the one mapped ELF
 * whose public PT_GNU_RELRO range contains, or is fully contained by, an
 * imminent guest protection change.  RT_DELETE and ambiguous multi-object
 * matches are rejected.
 */
kzt_public_loader_result_t kzt_public_loader_find_relro_object(
    uintptr_t dynamic_addr,
    size_t max_dynamic_entries,
    uintptr_t protect_start,
    size_t protect_size,
    size_t page_size,
    const kzt_public_loader_reader_t *reader,
    kzt_public_loader_object_t *object);

/*
 * Resolve DT_DEBUG from the live PT_DYNAMIC array, read r_debug, and deliver
 * every object not already remembered.  The observer becomes active only
 * after a complete RT_CONSISTENT snapshot.
 */
kzt_public_loader_result_t kzt_public_loader_observer_activate(
    kzt_public_loader_observer_t *observer,
    uintptr_t dynamic_addr,
    size_t max_dynamic_entries,
    const kzt_public_loader_reader_t *reader,
    kzt_public_loader_visit_fn visit,
    void *visit_opaque);

/*
 * Re-read r_debug and its public link_map chain.  RT_ADD/RT_DELETE return
 * BUSY without changing the last complete snapshot.
 */
kzt_public_loader_result_t kzt_public_loader_observer_refresh(
    kzt_public_loader_observer_t *observer,
    const kzt_public_loader_reader_t *reader,
    kzt_public_loader_visit_fn visit,
    void *visit_opaque);

/* Probe the current public loader state without walking the link_map chain. */
kzt_public_loader_result_t kzt_public_loader_state_is_consistent(
    const kzt_public_loader_observer_t *observer,
    const kzt_public_loader_reader_t *reader);

/* Resolve one unique defined symbol from the live in-memory link_map set. */
kzt_public_loader_result_t kzt_public_loader_find_symbol(
    const kzt_public_loader_observer_t *observer,
    const kzt_public_loader_reader_t *reader,
    const char *symbol_name,
    uintptr_t *symbol_addr);

kzt_public_loader_result_t kzt_public_loader_find_symbol_in_object(
    const kzt_public_loader_object_t *object,
    const kzt_public_loader_reader_t *reader,
    const char *symbol_name,
    uintptr_t *symbol_addr);

/*
 * Resolve one observed object whose exact, unaligned PT_LOAD memory range
 * contains guest_addr.  A link_map already remembered by the pre-protection
 * path may be resolved during RT_ADD; an address not found during RT_ADD stays
 * BUSY until the loader publishes a complete snapshot.  RT_DELETE is rejected.
 * PT_LOAD file-to-BSS gaps are included through p_memsz, while inter-segment
 * gaps and segment end addresses are excluded.
 */
kzt_public_loader_result_t kzt_public_loader_find_object_by_address(
    const kzt_public_loader_observer_t *observer,
    const kzt_public_loader_reader_t *reader,
    uintptr_t guest_addr,
    kzt_public_loader_object_t *object);

kzt_public_loader_result_t kzt_public_loader_object_contains_address(
    const kzt_public_loader_object_t *object,
    const kzt_public_loader_reader_t *reader,
    uintptr_t guest_addr,
    int *contains);

kzt_public_loader_result_t kzt_public_loader_collect_tls(
    const kzt_public_loader_observer_t *observer,
    const kzt_public_loader_reader_t *reader,
    kzt_public_loader_tls_object_t *objects,
    size_t object_capacity,
    size_t *object_count);

kzt_public_loader_result_t kzt_public_loader_read_tls_object(
    const kzt_public_loader_object_t *object,
    const kzt_public_loader_reader_t *reader,
    kzt_public_loader_tls_object_t *tls_object,
    int *has_tls);
kzt_public_loader_result_t kzt_public_loader_materialize_tls_image(
    const kzt_public_loader_tls_object_t *object,
    const kzt_public_loader_reader_t *reader,
    void *destination,
    size_t destination_size);

/* Refresh a private observer copy and collect TLS without committing state. */
kzt_public_loader_result_t kzt_public_loader_snapshot_tls(
    const kzt_public_loader_observer_t *observer,
    uintptr_t dynamic_addr,
    size_t max_dynamic_entries,
    const kzt_public_loader_reader_t *reader,
    uintptr_t link_map_filter,
    kzt_public_loader_tls_object_t *objects,
    size_t object_capacity,
    size_t *object_count);

const char *kzt_public_loader_result_name(
    kzt_public_loader_result_t result);

#endif
