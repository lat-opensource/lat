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
    size_t live_map_count;
    int active;
} kzt_public_loader_observer_t;

void kzt_public_loader_observer_reset(
    kzt_public_loader_observer_t *observer);

/*
 * Record an object already processed by the pre-protection path.  This
 * prevents the initial public snapshot from replaying it.
 */
kzt_public_loader_result_t kzt_public_loader_observer_remember(
    kzt_public_loader_observer_t *observer,
    uintptr_t link_map_addr);

int kzt_public_loader_observer_has_map(
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
 * While r_debug reports RT_ADD, identify the one mapped ELF whose public
 * PT_GNU_RELRO range contains, or is fully contained by, an imminent guest
 * protection change.  Ambiguous multi-object matches are rejected.
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

const char *kzt_public_loader_result_name(
    kzt_public_loader_result_t result);

#endif
