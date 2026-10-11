#include "qemu/osdep.h"

#include "qemu.h"
#include "latc-aot-v2-runner.h"

void latc_aot_v2_fork_start(void)
{
}

void latc_aot_v2_fork_end(CPUState *cpu, bool child)
{
    (void)cpu;
    (void)child;
}

bool latc_aot_v2_mapping_enabled(void)
{
    return false;
}

void latc_aot_v2_note_mmap(int fd, uint64_t guest_start,
                           uint64_t mapping_size, uint64_t file_offset)
{
    (void)fd;
    (void)guest_start;
    (void)mapping_size;
    (void)file_offset;
    close(fd);
}

void latc_aot_v2_drain_mmaps(void)
{
}

bool latc_aot_v2_revalidate_range(uint64_t guest_start,
                                  uint64_t mapping_size)
{
    (void)guest_start;
    (void)mapping_size;
    return false;
}

bool latc_aot_v2_note_mremap(CPUState *cpu, uint64_t old_start,
                             uint64_t old_size, uint64_t new_start,
                             uint64_t new_size, bool keep_old)
{
    (void)cpu;
    (void)old_start;
    (void)old_size;
    (void)new_start;
    (void)new_size;
    (void)keep_old;
    return false;
}

void latc_aot_v2_note_munmap(CPUState *cpu, uint64_t guest_start,
                             uint64_t mapping_size)
{
    (void)cpu;
    (void)guest_start;
    (void)mapping_size;
}

bool latc_aot_v2_invalidate_range(CPUState *cpu, uint64_t guest_start,
                                  uint64_t mapping_size,
                                  LatcAotV2InvalidationReason reason)
{
    (void)cpu;
    (void)guest_start;
    (void)mapping_size;
    (void)reason;
    return false;
}

void latc_aot_v2_consume_environment(void)
{
}

bool latc_aot_v2_strict_enabled(void)
{
    return false;
}

void latc_aot_v2_report_stats(void)
{
}

void latc_aot_v2_flush_pending_keys(void)
{
}

int latc_aot_v2_relocate_source_fd(int fd)
{
    (void)fd;
    return 0;
}

void latc_aot_v2_note_jit_key(target_ulong guest_pc, uint32_t cflags)
{
    (void)guest_pc;
    (void)cflags;
}

void latc_aot_v2_collect_existing_jit_tbs(target_ulong guest_begin,
                                          target_ulong guest_end)
{
    (void)guest_begin;
    (void)guest_end;
}

bool latc_aot_v2_is_file_pc(target_ulong guest_pc)
{
    (void)guest_pc;
    return false;
}

int latc_aot_v2_prepare(CPUArchState *env)
{
    (void)env;
    return 0;
}

bool latc_aot_v2_find_target(CPUState *cpu, target_ulong guest_pc,
                             uint32_t cflags, LatcAotV2Target *target)
{
    (void)cpu;
    (void)guest_pc;
    (void)cflags;
    (void)target;
    return false;
}

void latc_aot_v2_note_cached_miss(const LatcAotV2Target *target)
{
    (void)target;
}

bool latc_aot_v2_activate_target(CPUState *cpu,
                                 const LatcAotV2Target *target)
{
    (void)cpu;
    (void)target;
    return false;
}

void latc_aot_v2_release_target(CPUState *cpu, LatcAotV2Target *target)
{
    (void)cpu;
    (void)target;
}

void latc_aot_v2_release_current_execution(CPUState *cpu)
{
    (void)cpu;
}

bool latc_aot_v2_contains_host_pc(uintptr_t host_pc)
{
    (void)host_pc;
    return false;
}

bool latc_aot_v2_diagnose_host_pc(CPUState *cpu, uintptr_t host_pc,
                                  LatcAotV2SignalDiagnostic *diagnostic)
{
    (void)cpu;
    (void)host_pc;
    (void)diagnostic;
    return false;
}

bool latc_aot_v2_restore_state(CPUState *cpu, uintptr_t host_pc)
{
    (void)cpu;
    (void)host_pc;
    return false;
}

bool latc_aot_v2_defer_signal_recovery(CPUState *cpu, uintptr_t host_pc,
                                       target_ulong *guest_pc)
{
    (void)cpu;
    (void)host_pc;
    (void)guest_pc;
    return false;
}

void latc_aot_v2_drain_signal_recovery(CPUState *cpu)
{
    (void)cpu;
}
