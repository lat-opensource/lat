/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file jrra.c
 * @author Hanlu Li <heuleehanlu@gmail.com>
 * @brief JRRA optimization implementation
 */
#include <translate.h>
#include <include/exec/tb-lookup.h>
#include <accel/tcg/internal.h>
#include "qemu/cacheflush.h"
#include "latx-options.h"
#include "reg-map.h"
#include "jrra.h"
#include "jrra-translator.h"
#include "reg-alloc.h"
#include "profile.h"
#ifdef CONFIG_LATX_TU
#include "tu.h"
#endif
#include "ts.h"
#include "aot_recover_tb.h"
#include "loongarch-extcontext.h"
#include <sys/mman.h>

#if defined(TARGET_X86_64) && defined(CONFIG_LATX_JRRA)
struct SignalReturnBridge {
    target_ulong guest_restorer;
    struct SignalReturnBridge *next;
    uint32_t code[];
};

/*
 * Protected by mmap_lock(), including publication. Published code is immutable
 * and retained for the process lifetime: a handler can return much later, or
 * leave through a non-local jump without notifying us. Do not put these entries
 * in the flushable TB code buffer. Each distinct restorer currently costs one
 * host page containing both metadata and instructions; repeated and nested
 * deliveries reuse the same entry. Initialize the entire entry before making
 * the page read-only and executable.
 */
static struct SignalReturnBridge *signal_return_bridges;

uintptr_t get_signal_return_bridge(target_ulong guest_restorer)
{
    struct SignalReturnBridge *bridge;
    TRANSLATION_DATA *lat_ctx;
    IR2_OPND next_pc, exit_target;
    uintptr_t result = 0;
    size_t code_size, page_size = qemu_host_page_size;
    size_t code_offset = offsetof(struct SignalReturnBridge, code);
    int insns, assembled;

    mmap_lock();
    for (bridge = signal_return_bridges; bridge; bridge = bridge->next) {
        if (bridge->guest_restorer == guest_restorer) {
            result = (uintptr_t)bridge->code;
            goto out;
        }
    }

    /* Called from guest signal delivery, not from an active translation. */
    if (!context_switch_native_to_bt_ret_0 || !lsenv || !lsenv->tr_data) {
        goto out;
    }
    lat_ctx = lsenv->tr_data;
    if (lat_ctx->curr_tb) {
        goto out;
    }
    bridge = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (bridge == MAP_FAILED) {
        goto out;
    }

    tr_init(NULL);
    next_pc = ra_alloc_dbt_arg2();
    exit_target = ra_alloc_itemp();
    /* RET already popped the guest stack. Only replace its host target with
     * the real guest PC, then use the existing full state-save exit path.
     */
    li_d(next_pc, guest_restorer);
    li_d(exit_target, context_switch_native_to_bt_ret_0);
    la_jirl(zero_ir2_opnd, exit_target, 0);
    insns = label_dispose(NULL, lat_ctx);
    if (insns <= 0 ||
        (size_t)insns > (page_size - code_offset) / sizeof(bridge->code[0])) {
        tr_fini(false);
        goto fail;
    }
    assembled = tr_ir2_assemble(bridge->code, lat_ctx->first_ir2);
    tr_fini(false);
    if (assembled != insns) {
        goto fail;
    }
    code_size = (size_t)insns * sizeof(uint32_t);
    flush_idcache_range((uintptr_t)bridge->code, (uintptr_t)bridge->code,
                       code_size);
    bridge->guest_restorer = guest_restorer;
    bridge->next = signal_return_bridges;
    if (mprotect(bridge, page_size, PROT_READ | PROT_EXEC) != 0) {
        goto fail;
    }

    /* No caller can observe a partially generated or non-executable entry. */
    signal_return_bridges = bridge;
    result = (uintptr_t)bridge->code;
    goto out;

fail:
    munmap(bridge, page_size);
out:
    mmap_unlock();
    return result;
}
#endif

#ifdef CONFIG_LATX_JRRA
static bool tb_ending_with_call(TranslationBlock *tb)
{
    if (!tb) {
        return false;
    }
    return tb->next_86_pc ? true : false;
}

static TranslationBlock *get_next_tb(TranslationBlock *tb, CPUState *cpu,
                                     uint32_t flags, uint32_t cflags)
{

    TranslationBlock *n;
#ifdef CONFIG_LATX_AOT
    if (!in_pre_translate) {
        n = tb_lookup(cpu, tb->next_86_pc, 0, flags, cflags);
    } else {
        if ((tb->pc & TARGET_PAGE_MASK) != (tb->next_86_pc & TARGET_PAGE_MASK)) {
            return NULL;
        }
        return aot_tb_lookup(tb->next_86_pc, cflags);
    }
#else
    n = tb_lookup(cpu, tb->next_86_pc, 0, flags, cflags);
#endif
    if (!n) {
        n = tb_gen_code(cpu, tb->next_86_pc, 0, flags, cflags);
    }
    return n;
}

static void patch_jrra(TranslationBlock *tb, TranslationBlock *next_tb)
{
    int temp0, temp1;
    uint64_t patch_pcalau12i, patch_ori, patch_scr1, patch_scr0;
    ptrdiff_t offset_high, offset_low;
    tb_page_addr_t tb_page;

    if (!option_jr_ra) {
        return;
    }

    tb_page = tb_page_addr1(tb) == -1 ? tb_page_addr0(tb) : tb_page_addr1(tb);
    if (tb->return_target_ptr &&
            ((tb_page & TARGET_PAGE_MASK) == (tb_page_addr0(next_tb) & TARGET_PAGE_MASK))) {
        offset_high = (((uint64_t)next_tb->tc.ptr >> 12) -
                    ((uintptr_t)tb->return_target_ptr >> 12)) & 0xfffff;
        offset_low = (uint64_t)next_tb->tc.ptr & 0xfff;

        temp0 = reg_itemp_map[ITEMP0];
        temp1 = reg_itemp_map[ITEMP1];
        /* pcalau12i itemp0, offset_high */
        patch_pcalau12i = 0x1a000000 | temp0 | (offset_high << 5);
        /* ori itemp0, itemp0, offset_low */
        patch_ori = 0x03800000 | (offset_low << 10) |
                    (temp0 << 5) | temp0;
        /* gr2scr scr1, itemp0 */
        patch_scr1 = 0x801 | (temp0 << 5);
        /* gr2scr scr0, itemp1 */
        patch_scr0 = 0x800 | (temp1 << 5);
#ifdef CONFIG_LATX_AOT
        tb->bool_flags |= IS_ENABLE_JRRA;
#endif
        *tb->return_target_ptr = (patch_pcalau12i | (patch_ori << 32));
        *(tb->return_target_ptr + 1) = (patch_scr1 | (patch_scr0 << 32));
    }
}

static void patch_jrra_stack(TranslationBlock *tb, TranslationBlock *next_tb)
{
    int temp1;
    uint64_t patch_pcalau12i, patch_ori;
    ptrdiff_t offset_high, offset_low;

    if (!option_jr_ra_stack || !tb->return_target_ptr) {
        return;
    }

    offset_high = (((uint64_t)next_tb->tc.ptr >> 12) -
                    ((uintptr_t)tb->return_target_ptr >> 12)) & 0xfffff;
    offset_low = (uint64_t)next_tb->tc.ptr & 0xfff;

    /* pcalau12i itemp0, offset_high */
    temp1 = reg_itemp_map[ITEMP1];
    patch_pcalau12i = 0x1a000000 | temp1 | (offset_high << 5);
    /* ori itemp0, itemp0, offset_low */
    patch_ori = 0x03800000 | (offset_low << 10) |
                (temp1 << 5) | temp1;
#ifdef CONFIG_LATX_AOT
    tb->bool_flags |= IS_ENABLE_JRRA;
#endif
    *tb->return_target_ptr = (patch_pcalau12i | (patch_ori << 32));
}
#endif /* ifdef CONFIG_LATX_JRRA */

void jrra_pre_translate(void** list, int num, CPUState *cpu,
                        uint32_t flags, uint32_t cflags)
{
#ifdef CONFIG_LATX_JRRA
    TranslationBlock *next = NULL;
    TranslationBlock *curr = NULL;

    if (option_jr_ra || option_jr_ra_stack) {
        while (num--) {
            curr = *(TranslationBlock**)list;
            while (tb_ending_with_call(curr)) {
                if (!(next = get_next_tb(curr, cpu, flags, cflags))) {
                    break;
                }
                patch_jrra(curr, next);
                patch_jrra_stack(curr, next);
                curr = next;
            }
            list++;
        }
    }
#endif
}

void jrra_install_smc_sentinel(TranslationBlock *tb)
{
#ifdef CONFIG_LATX_JRRA
    if (!option_jr_ra) {
        return;
    }
    qatomic_set((uint32_t *)tb->tc.ptr, SMC_ILL_INST);
    flush_idcache_range((uintptr_t)tb->tc.ptr, (uintptr_t)tb->tc.ptr, 4);
#endif
}

void jrra_tb_reset(TranslationBlock *tb)
{
#ifdef CONFIG_LATX_JRRA
    tb->next_86_pc = 0;
    tb->return_target_ptr = NULL;
#else
    (void)tb;
#endif
}

bool jrra_preserve_current_tb_head(TranslationBlock *tb,
            TranslationBlock *current_tb,
            bool current_tb_modified, uint32_t *inst_out)
{
#ifdef CONFIG_LATX_JRRA
    if (option_jr_ra && current_tb == tb && !current_tb_modified && inst_out) {
        *inst_out = qatomic_read((uint32_t *)tb->tc.ptr);
        return true;
    }
#endif
    return false;
}

void jrra_restore_current_tb_head(TranslationBlock *tb,
            TranslationBlock *current_tb,
            bool current_tb_modified, uint32_t inst)
{
#ifdef CONFIG_LATX_JRRA
    if (option_jr_ra && current_tb == tb && !current_tb_modified) {
        qatomic_set((uint32_t *)tb->tc.ptr, inst);
        flush_idcache_range((uintptr_t)tb->tc.ptr, (uintptr_t)tb->tc.ptr, 4);
    }
#endif
}

bool jrra_handle_sigill(CPUArchState *env, ucontext_t *uc)
{
#ifdef CONFIG_LATX_JRRA
    (void)env;
    if ((option_jr_ra || option_jr_ra_stack) &&
        *(unsigned int *)UC_PC(uc) == SMC_ILL_INST) {
        TranslationBlock *current_tb = tcg_tb_lookup(UC_PC(uc));
        if (current_tb) {
            /* clear scr0 */
 #ifndef CONFIG_LOONGARCH_NEW_WORLD
            UC_SCR(uc)[0] = 0;
 #else
            struct extctx_layout extctx;
            memset(&extctx, 0, sizeof(extctx));
            parse_extcontext(uc, &extctx);
            uint64_t zero = 0;
            UC_SET_SCR(&extctx, 0, &zero, uint64_t);
 #endif
            /* set the next TB and point the epc to the epilogue */
            UC_GR(uc)[reg_statics_map[S_UD1]] = current_tb->pc;
            UC_PC(uc) = context_switch_native_to_bt_ret_0;
        }
        return true;
    }
#else
    (void)env;
    (void)uc;
#endif
    return false;
}

void jrra_emit_call_metadata(TranslationBlock *tb, target_ulong next_pc,
                             JrraCallType call_type)
{
#ifdef CONFIG_LATX_JRRA
    if (!tb) {
        return;
    }

    if (option_jr_ra_stack) {
        la_code_align(2, 0x03400000);
        IR2_OPND target_ptr = ra_alloc_data();
        IR2_OPND tb_base = ra_alloc_data();
        IR2_OPND curr_ptr = ra_alloc_label();
        /* set return_target_ptr */
        /* tb->return_target_ptr = tb->tc.ptr + CURRENT_INST_COUNTER; */
        la_label(curr_ptr);
        la_data_li(target_ptr, (uint64_t)&tb->return_target_ptr);
        la_data_li(tb_base, (uint64_t)tb->tc.ptr);
        la_data_add(tb_base, tb_base, curr_ptr);
        la_data_st(target_ptr, tb_base);
        /* set next_86_pc */
        tb->next_86_pc = next_pc;

        int pad_count = call_type == JRRA_CALL_DIRECT ? 1 : 2;
        for (int i = 0; i < pad_count; i++) {
            la_ori(zero_ir2_opnd, zero_ir2_opnd, 0);
        }
        if (call_type == JRRA_CALL_DIRECT) {
            la_gr2scr(scr0_ir2_opnd, zero_ir2_opnd);
        }
    }

    if (option_jr_ra) {
        la_code_align(2, 0x03400000);
        IR2_OPND target_ptr = ra_alloc_data();
        IR2_OPND tb_base = ra_alloc_data();
        IR2_OPND curr_ptr = ra_alloc_label();
        /* set return_target_ptr */
        /* tb->return_target_ptr = tb->tc.ptr + CURRENT_INST_COUNTER; */
        la_label(curr_ptr);
        la_data_li(target_ptr, (uint64_t)&tb->return_target_ptr);
        la_data_li(tb_base, (uint64_t)tb->tc.ptr);
        la_data_add(tb_base, tb_base, curr_ptr);
        la_data_st(target_ptr, tb_base);
        /* set next_86_pc */
        tb->next_86_pc = next_pc;
        /*
         * pcalau12i itemp0, offset_high
         * ori itemp0, itemp0, offset_low
         * gr2scr scr1, itemp0
         * gr2scr scr0, itemp1
         */
        la_ori(zero_ir2_opnd, zero_ir2_opnd, 0);
        la_ori(zero_ir2_opnd, zero_ir2_opnd, 0);
        la_ori(zero_ir2_opnd, zero_ir2_opnd, 0);
        la_gr2scr(scr0_ir2_opnd, zero_ir2_opnd);
    }
#endif
}

bool jrra_translate_ret(TranslationBlock *tb, IR2_OPND return_addr_opnd,
                        IR1_INST *pir1)
{
#ifdef CONFIG_LATX_JRRA
    (void)pir1;
    if (option_jr_ra) {
        IR2_OPND scr0 = ra_alloc_scr(0);
        IR2_OPND scr1 = ra_alloc_scr(1);
        IR2_OPND saved_x86_pc = ra_alloc_itemp();
        IR2_OPND target_label_opnd = ra_alloc_label();

        PER_TB_COUNT((void *)&((tb->profile).jrra_in), 1);

        la_scr2gr(saved_x86_pc, scr0);
        la_bne(saved_x86_pc, return_addr_opnd, target_label_opnd);
        la_scr2gr(saved_x86_pc, scr1);
        la_jirl(zero_ir2_opnd, saved_x86_pc, 0);
        la_label(target_label_opnd);

        PER_TB_COUNT((void *)&((tb->profile).jrra_miss), 1);
    }
    if (option_jr_ra_stack) {
        la_jirl(zero_ir2_opnd, return_addr_opnd, 0);
        return true;
    }
#endif
    return false;
}

void jrra_context_switch_bt_to_native(void)
{
#ifdef CONFIG_LATX_JRRA
    if (option_jr_ra || option_jr_ra_stack) {
        la_gr2scr(scr0_ir2_opnd, zero_ir2_opnd);
    }
#endif
}

void jrra_relocate_return_target(TranslationBlock *tb, uintptr_t new_base)
{
#ifdef CONFIG_LATX_JRRA
    if (!tb || !tb->next_86_pc || !tb->return_target_ptr ||
        (!option_jr_ra && !option_jr_ra_stack)) {
        return;
    }
    uintptr_t addr = (uintptr_t)tb->return_target_ptr;
    addr -= (uintptr_t)tb->tc.ptr;
    tb->return_target_ptr = (unsigned long *)(new_base + addr);
#endif
}
