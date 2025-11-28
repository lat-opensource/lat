/**
 * @file latx-signal.c
 * @author Hanlu Li <heuleehanlu@gmail.com>
 * @brief For JRRA-unlink
 */
#include "common.h"
#include "reg-alloc.h"
#include "translate.h"
#include "latx-signal.h"
#include "qemu/cacheflush.h"
#include "exec/tb-lookup.h"
#include "loongarch-extcontext.h"

#ifdef CONFIG_LATX_TU
void unlink_tu_jmp(TranslationBlock *tb) 
{
    uintptr_t jmp_rx = (uintptr_t)tb->tc.ptr + tb->tu_jmp[TU_TB_INDEX_TARGET];
    uintptr_t jmp_rw = jmp_rx - tcg_splitwx_diff;
    uintptr_t addr = (uintptr_t)tb->tc.ptr + tb->tu_unlink.stub_offset;
    b_to_addr(jmp_rx, jmp_rw, addr);
}
#endif

bool tb_is_unlink(TranslationBlock *tb, int index)
{
    if (index == 0) {
        return (tb->bool_flags & SIGNAL_UNLINK0) ? true :false;
    } else {
        return (tb->bool_flags & SIGNAL_UNLINK1) ? true :false;
    }
}

bool tb_need_relink(TranslationBlock *tb, int index)
{
    if (index == 0) {
        return (tb->bool_flags & SIGNAL_RELINK0) ? true :false;
    } else {
        return (tb->bool_flags & SIGNAL_RELINK1) ? true :false;
    }
}

void set_tb_unlink_flag(TranslationBlock *tb, int index)
{
    if (index == 0) {
        tb->bool_flags &=  ~SIGNAL_RELINK0;
        tb->bool_flags |=  SIGNAL_UNLINK0;
    } else {
        tb->bool_flags &=  ~SIGNAL_RELINK1;
        tb->bool_flags |=  SIGNAL_UNLINK1;
    }
}

void set_tb_relink_flag(TranslationBlock *tb, int index)
{
    if (index == 0) {
        tb->bool_flags &=  ~SIGNAL_UNLINK0;
        tb->bool_flags |=  SIGNAL_RELINK0;
    } else {
        tb->bool_flags &=  ~SIGNAL_UNLINK1;
        tb->bool_flags |=  SIGNAL_RELINK1;
    }
}

void clear_signal_link_flag(TranslationBlock *tb, int index)
{
    if (index == 0) {
        tb->bool_flags &=  ~(SIGNAL_UNLINK0 | SIGNAL_RELINK0);
    } else {
        tb->bool_flags &=  ~(SIGNAL_UNLINK1 | SIGNAL_RELINK1);
    }
}

void unlink_direct_jmp(TranslationBlock *tb) /* TODO */
{
    if (tb->jmp_reset_offset[0] != TB_JMP_RESET_OFFSET_INVALID) {
        tb_reset_jump(tb, 0);
        qatomic_and(&tb->jmp_dest[0], (uintptr_t)NULL);
        set_tb_unlink_flag(tb, 0);
    }
    if (tb->jmp_reset_offset[1] != TB_JMP_RESET_OFFSET_INVALID) {
        tb_reset_jump(tb, 1);
        qatomic_and(&tb->jmp_dest[1], (uintptr_t)NULL);
        set_tb_unlink_flag(tb, 1);
    }
}

void link_indirect_jmp(CPUArchState *env)
{
    uintptr_t jmp_rw = env->insn_save[0];
    qatomic_set((uint32_t *)jmp_rw, env->insn_save[1]);
    env->insn_save[0] = 0;
    flush_idcache_range(jmp_rw, jmp_rw, 4);
}

void unlink_indirect_jmp(CPUArchState *env, TranslationBlock *tb, ucontext_t *uc)
{
    IR2_INST *pir2 = generate_andi(zero_ir2_opnd, zero_ir2_opnd, 0);
    uint32_t nop = ir2_assemble(pir2);
    uintptr_t offset = tb->jmp_indirect;
    uintptr_t tc_ptr = (uintptr_t)tb->tc.ptr;
    uintptr_t jmp_rx = tc_ptr + offset;
    uintptr_t jmp_rw = jmp_rx - tcg_splitwx_diff;

#define INS_SIZE 4
#ifdef CONFIG_LATX_JRRA
#define SCR_MASK 0xffffff80
#define SCR_CODE 0x00000c00

#ifndef CONFIG_LOONGARCH_NEW_WORLD
    /* clear scr0 */
    UC_SCR(uc)[0] = 0;
#else
    struct extctx_layout extctx;
    memset(&extctx, 0, sizeof(extctx));
    parse_extcontext(uc, &extctx);
    uint64_t zero = 0;
    UC_SET_SCR(&extctx, 0, &zero, uint64_t);
#endif
    /*
     *                  scr2gr    itmp0,$scr0
     *                / bne       itmp0,$x,3
     * set epc to b -   scr2gr    itmp0,$scr1
     *                \ jirl      $zero,itmp0,0
     *    jmp_rx  -->   b ...
     */

    if ((UC_PC(uc) > jmp_rx - INS_SIZE * 4) && UC_PC(uc) < jmp_rx) {
        uint32_t insn_scr0 = qatomic_read((uint32_t *)(jmp_rx - INS_SIZE * 4));
        uint32_t insn_scr1 = qatomic_read((uint32_t *)(jmp_rx - INS_SIZE * 2));
        if ((insn_scr0 & SCR_MASK) == SCR_CODE &&
            (insn_scr1 & SCR_MASK) == SCR_CODE) {
                UC_PC(uc) = jmp_rx;
        }
    }
#endif

    uint32_t insn = qatomic_read((uint32_t *)jmp_rx);
#ifdef CONFIG_LATX_LARGE_CC
    if ((insn & 0xfe000000) == 0x1e000000) {
        /*
         * pcaddu18i
         * jirl
         */
        insn = qatomic_read((uint32_t *)(jmp_rx + INS_SIZE));
        jmp_rw += INS_SIZE;
    } 
#ifdef CONFIG_LATX_AOT
    else {
        qatomic_set((uint32_t *)(jmp_rw + INS_SIZE), nop);
    }
#endif
#endif

#ifdef CONFIG_LATX_FAST_JMPCACHE
    insn = qatomic_read((uint32_t *)(jmp_rx + INS_SIZE * 11));
    jmp_rw += INS_SIZE * 11;
#else
    CPUState *cpu = env_cpu(env);
    uint32_t parallel = cpu->tcg_cflags & CF_PARALLEL;
    if (!close_latx_parallel && !parallel) {
        /*
         * srli.d    itmp6,$x,16
         * xor       itmp6,$x,itmp6
         * bstrpick.d  itmp6,itmp6,15,0
         * alsl.d    itmp6,itmp6,$fp,3
         * ld.d      itmp1,itmp6,0
         * bne       itmp1,$x,3
         * ld.d      itmp1,itmp6,8
         * jirl      $zero,itmp1,0
         */
        insn = qatomic_read((uint32_t *)(jmp_rx + INS_SIZE * 7));
        jmp_rw += INS_SIZE * 7;
    }
#endif

    env->insn_save[0] = jmp_rw;
    env->insn_save[1] = insn;
    qatomic_set((uint32_t *)jmp_rw, nop);

    flush_idcache_range(jmp_rx, jmp_rw, 4);
}

bool signal_in_glue(CPUArchState *env, ucontext_t *uc)
{
    TranslationBlock *tb;
    target_ulong pc;
    uint32_t hash;
    uintptr_t epc;
    CPUState *cpu;

    epc = (uintptr_t)UC_PC(uc);
    if (epc < indirect_jmp_glue || epc >= ss_match_fail_native) {
        return false;
    }

    pc = uc->uc_mcontext.__gregs[reg_statics_map[S_UD1]];
    hash = tb_jmp_cache_hash_func(pc);
    cpu = env_cpu(env);
    tb = qatomic_rcu_read(&cpu->tb_jmp_cache[hash]);

    if (likely(tb && tb->pc == pc)) {
        if (use_indirect_jmp(tb) && tb->jmp_indirect != TB_JMP_RESET_OFFSET_INVALID) {
            unlink_indirect_jmp(env, tb, uc);
        } else {
#ifdef CONFIG_LATX_TU
            if (use_tu_jmp(tb)) {
                unlink_tu_jmp(tb);
            } else
#endif
            {
                unlink_direct_jmp(tb);
            }
        }
    }

    return true;
}

void tb_exit_to_qemu(CPUArchState *env, ucontext_t *uc)
{
    TranslationBlock *current_tb = NULL;
    uintptr_t pc = (uintptr_t)UC_PC(uc);

    current_tb = tcg_tb_lookup(pc);
    if (current_tb) {
#ifdef CONFIG_LATX_TU
        if (use_tu_jmp(current_tb)) {
            assert(current_tb->tu_jmp[TU_TB_INDEX_TARGET] != TB_JMP_RESET_OFFSET_INVALID);
            unlink_tu_jmp(current_tb);
            return;
        }
#endif
        if (use_indirect_jmp(current_tb) &&
                current_tb->jmp_indirect != TB_JMP_RESET_OFFSET_INVALID) {
            unlink_indirect_jmp(env, current_tb, uc);
        } else {
            unlink_direct_jmp(current_tb);
        }
    } else {
        if (!signal_in_glue(env, uc)) {
#ifdef SIGNAL_UNLINK_DBG
            fprintf(stderr, "lhl-debug %s current_tb NULL pid %d\n", __func__, getpid());
#endif
        }
    }
}
