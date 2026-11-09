/*
 * Copyright (C) 2017, Emilio G. Cota <cota@braap.org>
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */
#ifndef EXEC_TB_LOOKUP_H
#define EXEC_TB_LOOKUP_H

#ifdef NEED_CPU_H
#include "cpu.h"
#else
#include "exec/poison.h"
#endif

#include "exec/exec-all.h"
#include "exec/tb-hash.h"
#if defined(CONFIG_USER_ONLY) && defined(CONFIG_LATX)
#include "exec/tb-context.h"
TranslationBlock *tb_htable_lookup_snapshot(CPUState *, target_ulong, target_ulong, uint32_t, uint32_t, struct qht_miss_snapshot *);
#endif
#include "latx-options.h"

#ifdef CONFIG_LATX_FAST_JMPCACHE
#include "exec/fasttb.h"
#endif

#if defined(CONFIG_USER_ONLY) && defined(CONFIG_LATX)
#include "latx-signal.h"
#endif

/* Might cause an exception, so have a longjmp destination ready */
static inline TranslationBlock *tb_lookup_internal(CPUState *cpu, target_ulong pc,
                                          target_ulong cs_base,
                                          uint32_t flags, uint32_t cflags,
                                          bool use_tb_lock)
{
    TranslationBlock *tb;
#if defined(CONFIG_USER_ONLY) && defined(CONFIG_LATX)
    struct qht_miss_snapshot miss = {0};
    uint32_t miss_trace = 0;
#endif
    uint32_t hash;
#ifdef CONFIG_LATX_PROFILER
    TCGProfile *prof = &tcg_ctx->prof;
#endif

    /* we should never be trying to look up an INVALID tb */
    tcg_debug_assert(!(cflags & CF_INVALID));
    /* return the tb pointer of HPC */
    if(pc > (size_t)tcg_ctx->code_gen_buffer){
        return tb = tcg_tb_lookup(pc);
    }

    hash = tb_jmp_cache_hash_func(pc);
    tb = qatomic_rcu_read(&cpu->tb_jmp_cache[hash]);

    if (likely(tb &&
               tb->pc == pc &&
               /* tb->cs_base == cs_base && */
               tb->flags == flags &&
               tb_cflags(tb) == cflags)) {
#ifdef CONFIG_LATX_PROFILER
        qatomic_inc(&prof->hash_count);
#endif
        return tb;
    }
#if defined(CONFIG_USER_ONLY) && defined(CONFIG_LATX)
    /*
     * Only tb_find inside cpu_exec's RCU read-side section enables this path.
     * RCU protects the QHT lookup; exclusive CPU work protects full TB resets.
     * Match invalidation's jmp_lock before publishing either jump-cache entry.
     * Never acquire mmap_lock while holding jmp_lock.
     */
    /* AOT and non-Quicktrans/TU blocks retain the original locked lookup. */
    if (use_tb_lock && !option_aot) {
        if (!option_smc_reload) {
            miss_trace = *cpu->trace_dstate;
            tb = tb_htable_lookup_snapshot(cpu, pc, cs_base, flags, cflags, &miss);
        } else {
            tb = tb_htable_lookup(cpu, pc, cs_base, flags, cflags);
        }
        if (tb && tb_page_addr1(tb) == -1 &&
            (qatomic_read(&tb->bool_flags) &
             (IS_QUICKTRANS_TB | IS_AOT_TB | IS_TU_TB | IS_TU_JMP |
              IS_TU_SPLIT | IS_TUNNEL_LIB)) == IS_QUICKTRANS_TB) {
            /* Also defer notifications while waiting for the block lock. */
            latx_lookup_lock_active = 1;
            qemu_spin_lock(&tb->jmp_lock);
            if (tb_cflags(tb) == cflags) {
#ifdef CONFIG_LATX_FAST_JMPCACHE
                latx_fast_jmp_cache_add(cpu, hash, tb);
#endif
                qatomic_set(&cpu->tb_jmp_cache[hash], tb);
                qemu_spin_unlock(&tb->jmp_lock);
                latx_lookup_lock_active = 0;
                if (latx_lookup_pending_page) {
                    latx_lookup_drain_notification();
                    /* The notification may have invalidated this very TB. */
                    return NULL;
                }
                return tb;
            }
            qemu_spin_unlock(&tb->jmp_lock);
            latx_lookup_lock_active = 0;
            if (latx_lookup_pending_page) {
                latx_lookup_drain_notification();
                return NULL;
            }
        }
    }
#else
    (void)use_tb_lock;
#endif
    mmap_lock();
#if defined(CONFIG_USER_ONLY) && defined(CONFIG_LATX)
    if (*cpu->trace_dstate == miss_trace &&
        qht_miss_snapshot_unchanged(&tb_ctx.htable, &miss)) {
        mmap_unlock();
        return NULL;
    }
#endif
    tb = tb_htable_lookup(cpu, pc, cs_base, flags, cflags);
    if (tb == NULL) {
        mmap_unlock();
        return NULL;
    }
#ifdef CONFIG_LATX_FAST_JMPCACHE
    latx_fast_jmp_cache_add(cpu, hash, tb);
#endif
    qatomic_set(&cpu->tb_jmp_cache[hash], tb);
    mmap_unlock();
#ifdef CONFIG_LATX_PROFILER
    qatomic_inc(&prof->qht_count);
#endif
    return tb;
}

static inline TranslationBlock *tb_lookup(CPUState *cpu, target_ulong pc,
                                         target_ulong cs_base, uint32_t flags,
                                         uint32_t cflags)
{
    return tb_lookup_internal(cpu, pc, cs_base, flags, cflags, false);
}
static inline TranslationBlock *tb_lookup_rcu(CPUState *cpu, target_ulong pc,
                                             target_ulong cs_base,
                                             uint32_t flags, uint32_t cflags)
{
    return tb_lookup_internal(cpu, pc, cs_base, flags, cflags, true);
}

#endif /* EXEC_TB_LOOKUP_H */
