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
#include "latx-options.h"

#ifdef CONFIG_LATX_FAST_JMPCACHE
#include "exec/fasttb.h"
#endif

/* Might cause an exception, so have a longjmp destination ready */
static inline TranslationBlock *tb_lookup(CPUState *cpu, target_ulong pc,
                                          target_ulong cs_base,
                                          uint32_t flags, uint32_t cflags)
{
    TranslationBlock *tb;
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
    mmap_lock();
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

#endif /* EXEC_TB_LOOKUP_H */
