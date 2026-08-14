/*
 * Translation block flush coordination.
 *
 * Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 */

#include "qemu/osdep.h"
#include "qemu/plugin.h"
#include "exec/exec-all.h"
#include "tcg/tcg.h"
#include "sysemu/tcg.h"
#include "accel/tcg/internal.h"

#ifdef CONFIG_USER_ONLY
#include "qemu.h"
#endif

#ifdef CONFIG_LATX
#include "latx-options.h"
#include "smc_reload.h"
#endif
#ifdef CONFIG_LATX_AOT
#include "aot.h"
#include "ts.h"
#endif
#ifdef CONFIG_LATX_FAST_JMPCACHE
#include "exec/fasttb.h"
#endif

#ifdef DEBUG_TB_FLUSH
#define DEBUG_TB_FLUSH_GATE 1
#else
#define DEBUG_TB_FLUSH_GATE 0
#endif

static gboolean tb_host_size_iter(gpointer key, gpointer value, gpointer data)
{
    const TranslationBlock *tb = value;
    size_t *size = data;

    *size += tb->tc.size;
    return false;
}

/* flush all the translation blocks */
void do_tb_flush(CPUState *cpu, run_on_cpu_data tb_flush_count)
{
    bool did_flush = false;

#ifdef CONFIG_LATX_AOT
    if (option_aot && in_pre_translate) {
        qemu_log_mask(LAT_LOG_AOT, "FIXME: tb flush in pre translate\n");
        _exit(0);
    }
#endif

    mmap_lock();
    /*
     * If it is already been done on request of another CPU, just retry.
     */
    if (tb_ctx.tb_flush_count != tb_flush_count.host_int) {
        goto done;
    }
    did_flush = true;

#ifdef CONFIG_LATX
    if (option_smc_reload) {
        smc_reload_tree_clear();
    }
#endif

    if (DEBUG_TB_FLUSH_GATE) {
        size_t nb_tbs = tcg_nb_tbs();
        size_t host_size = 0;

        tcg_tb_foreach(tb_host_size_iter, &host_size);
        printf("qemu: flush code_size=%zu nb_tbs=%zu avg_tb_size=%zu\n",
               tcg_code_size(), nb_tbs, nb_tbs > 0 ? host_size / nb_tbs : 0);
    }

    CPU_FOREACH(cpu) {
#ifdef CONFIG_LATX_FAST_JMPCACHE
        latx_fast_jmp_cache_clear_all(cpu);
#endif
        cpu_tb_jmp_cache_clear(cpu);
    }

    qht_reset_size(&tb_ctx.htable, CODE_GEN_HTABLE_SIZE);
    tb_flush_remove_all();

    tcg_region_reset_all();
    /*
     * XXX: flush processor icache at this point if cache flush is expensive.
     */
    qatomic_mb_set(&tb_ctx.tb_flush_count, tb_ctx.tb_flush_count + 1);

done:
    mmap_unlock();
    if (did_flush) {
        qemu_plugin_flush_cb();
    }
#if defined(CONFIG_LATX_KZT)
    CPU_FOREACH(cpu) {
        /* The installer also checks the effective library-group mask. */
        if (cpu && option_kzt) {
            kzt_install_runtime_callbacks(cpu, &info1);
        }
    }
#endif
}

static void gen_aot_and_flush(CPUState *cpu, run_on_cpu_data tb_flush_count)
{
#ifdef CONFIG_LATX_AOT
    aot_exit_entry(cpu, AOT_EXIT_THREAD);
#endif
    do_tb_flush(cpu, tb_flush_count);
}

void tb_flush(CPUState *cpu)
{
    if (tcg_enabled()) {
        unsigned tb_flush_count = qatomic_mb_read(&tb_ctx.tb_flush_count);

        if (cpu_in_exclusive_context(cpu)) {
            gen_aot_and_flush(cpu, RUN_ON_CPU_HOST_INT(tb_flush_count));
        } else {
            async_safe_run_on_cpu(cpu, gen_aot_and_flush,
                                  RUN_ON_CPU_HOST_INT(tb_flush_count));
        }
    }
}
