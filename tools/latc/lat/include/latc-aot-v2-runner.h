#ifndef LATC_AOT_V2_RUNNER_H
#define LATC_AOT_V2_RUNNER_H

#include "qemu/typedefs.h"
#include "exec/cpu-defs.h"

#include <stdatomic.h>
#include <stdint.h>

typedef struct LatcAotV2Target {
    const void *host_address;
    const void *context;
    const _Atomic uint64_t *generation_address;
    target_ulong guest_pc;
    uint64_t generation;
    uint32_t cflags;
    const uint64_t *guest_slots_end;
    uint64_t guest_slot_count;
    bool reader_held;
} LatcAotV2Target;

typedef struct LatcAotV2SignalDiagnostic {
    uint8_t source_sha256[32];
    target_ulong guest_pc;
    uint64_t generation;
    target_ulong guest_begin;
    target_ulong guest_end;
    int32_t stack_pointer_delta;
} LatcAotV2SignalDiagnostic;

typedef enum LatcAotV2InvalidationReason {
    LATC_AOT_V2_INVALIDATE_UNMAP,
    LATC_AOT_V2_INVALIDATE_MAP_FIXED,
    LATC_AOT_V2_INVALIDATE_PROTECTION,
    LATC_AOT_V2_INVALIDATE_CODE_WRITE,
} LatcAotV2InvalidationReason;

/* Serialize AOT state across fork and retain registered modules in the child. */
void latc_aot_v2_consume_environment(void);
bool latc_aot_v2_strict_enabled(void);
void latc_aot_v2_fork_start(void);
void latc_aot_v2_fork_end(CPUState *cpu, bool child);
bool latc_aot_v2_mapping_enabled(void);
/* Takes ownership of fd. ELF inspection is deferred while mmap is locked. */
void latc_aot_v2_note_mmap(int fd, uint64_t guest_start,
                           uint64_t mapping_size, uint64_t file_offset);
void latc_aot_v2_note_munmap(CPUState *cpu, uint64_t guest_start,
                             uint64_t mapping_size);
bool latc_aot_v2_invalidate_range(CPUState *cpu, uint64_t guest_start,
                                  uint64_t mapping_size,
                                  LatcAotV2InvalidationReason reason);
void latc_aot_v2_drain_mmaps(void);
bool latc_aot_v2_revalidate_range(uint64_t guest_start,
                                  uint64_t mapping_size);
bool latc_aot_v2_note_mremap(CPUState *cpu, uint64_t old_start,
                             uint64_t old_size, uint64_t new_start,
                             uint64_t new_size, bool keep_old);
void latc_aot_v2_report_stats(void);
/* Preserve pending JIT keys before exec replaces the current process image. */
void latc_aot_v2_flush_pending_keys(void);
/* Preserve internal source references before a guest closes/overwrites fd. */
int latc_aot_v2_relocate_source_fd(int fd);
/* Record a file-backed JIT translation request without execution counters. */
void latc_aot_v2_note_jit_key(target_ulong guest_pc, uint32_t cflags);
/* Add TBs translated before a deferred ELF mapping was registered. */
void latc_aot_v2_collect_existing_jit_tbs(target_ulong guest_begin,
                                          target_ulong guest_end);
bool latc_aot_v2_is_file_pc(target_ulong guest_pc);
int latc_aot_v2_prepare(CPUArchState *env);
bool latc_aot_v2_find_target(CPUState *cpu, target_ulong guest_pc,
                             uint32_t cflags, LatcAotV2Target *target);
void latc_aot_v2_note_cached_miss(const LatcAotV2Target *target);
bool latc_aot_v2_activate_target(CPUState *cpu,
                                 const LatcAotV2Target *target);
void latc_aot_v2_release_target(CPUState *cpu, LatcAotV2Target *target);
void latc_aot_v2_release_current_execution(CPUState *cpu);
bool latc_aot_v2_contains_host_pc(uintptr_t host_pc);
bool latc_aot_v2_diagnose_host_pc(CPUState *cpu, uintptr_t host_pc,
                                  LatcAotV2SignalDiagnostic *diagnostic);
bool latc_aot_v2_restore_state(CPUState *cpu, uintptr_t host_pc);
bool latc_aot_v2_defer_signal_recovery(CPUState *cpu, uintptr_t host_pc,
                                       target_ulong *guest_pc);
void latc_aot_v2_drain_signal_recovery(CPUState *cpu);

#endif
