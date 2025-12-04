/**
 * @file tu.c
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief TU optimization
 */
#include "common.h"
#include "exec/exec-all.h"
#include "tcg/tcg.h"
#include "translate.h"
#include "lsenv.h"
#include "accel/tcg/internal.h"
#include "ir1-optimization.h"
#include "latx-config.h"
#include "tu.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "aot_page.h"

void get_last_info(TranslationBlock *tb, IR1_INST* pir1)
{
    if (tb->icount == 0) {
        return;
    }
    tb->s_data->next_pc = ir1_addr_next(pir1);

    if (ir1_is_branch(pir1)) {
        if (likely(ir1_opnd_is_imm(ir1_get_opnd(pir1, 0)))) {
            tb->s_data->last_ir1_type = (int8)IR1_TYPE_BRANCH;
            tb->s_data->target_pc = ir1_target_addr(pir1);
        } else {
            assert(0);
        }
    }
    else if (ir1_is_call(pir1) && !ir1_is_indirect_call(pir1)) {
        tb->s_data->last_ir1_type = (int8)IR1_TYPE_CALL;
        tb->s_data->target_pc = ir1_target_addr(pir1);
    }
    else if (ir1_is_jump(pir1) && !ir1_is_indirect_jmp(pir1)) {
        tb->s_data->last_ir1_type = (int8)IR1_TYPE_JUMP;
        tb->s_data->target_pc = ir1_target_addr(pir1);
    }
    else if (ir1_is_return(pir1)) {
        tb->s_data->last_ir1_type = (int8)IR1_TYPE_RET;
    }
    else if (ir1_opcode(pir1) == dt_X86_INS_CALL &&
        ir1_is_indirect_call(pir1)) {
        tb->s_data->last_ir1_type = (int8)IR1_TYPE_CALLIN;
    } else if (ir1_opcode(pir1) == dt_X86_INS_JMP &&
        ir1_is_indirect_jmp(pir1)) {
        tb->s_data->last_ir1_type = (int8)IR1_TYPE_JUMPIN;
    } else {
        tb->s_data->last_ir1_type = (int8)IR1_TYPE_NORMAL;
    }
    return;
}

#ifdef CONFIG_LATX_TU

static __thread TUControl tu_data_rel;
__thread TUControl *tu_data;

#define TU_NOP 0x03400000
#ifdef CONFIG_SOFTMMU
#define assert_memory_lock()
#else
#define assert_memory_lock() tcg_debug_assert(have_mmap_lock())
#endif

static gint gpc_cmp(gconstpointer ap, gconstpointer bp)
{
    const TranslationBlock *a = (TranslationBlock *)ap;
    const TranslationBlock *b = (TranslationBlock *)bp;

    return (a->pc < b->pc) ? -1 : (a->pc > b->pc);
}

static inline void tu_trees_init(void)
{
    tu_data->tree = g_tree_new(gpc_cmp);
}

inline void tu_trees_reset(void)
{
    /* Increment the refcount first so that destroy acts as a reset */
    g_tree_ref(tu_data->tree);
    g_tree_destroy(tu_data->tree);
}

TranslationBlock *tu_tree_lookup(target_ulong pc)
{
    TranslationBlock key = {.pc = pc};
    return (TranslationBlock *)g_tree_lookup(tu_data->tree, &key);
}

void tu_control_init(void)
{
    tu_data = &tu_data_rel;
    tu_data->ir1_num_in_tu = 0;
    tu_data->tb_num = 0;
    tu_trees_init();

    return;
}

inline void tu_push_back(TranslationBlock *tb)
{
    if (!tb) {
        return;
    }
    TranslationBlock** tb_list = tu_data->tb_list;
    uint32_t* tb_num_in_tu = &tu_data->tb_num;

    tb_list[(*tb_num_in_tu)++] = tb;
}

void tu_enough_space(CPUState *cpu)
{
    if (unlikely((tcg_ctx->code_gen_ptr + MAX_TU_SIZE >= tcg_ctx->code_gen_highwater)
                || (tcg_ctx->tb_gen_ptr + MAX_TB_IN_CACHE * sizeof(TranslationBlock)
                    >= tcg_ctx->tb_gen_highwater))){
        tb_flush(cpu);
        mmap_unlock();
        /* Make the execution loop process the flush as soon as possible.  */
        cpu->exception_index = EXCP_INTERRUPT;
        cpu_loop_exit(cpu);
    }
}

void tu_reset_tb(TranslationBlock *tb)
{
    qemu_spin_init(&tb->jmp_lock);
    tb->jmp_dest[0] = (uintptr_t)NULL;
    tb->jmp_dest[1] = (uintptr_t)NULL;
    tb->jmp_list_head = (uintptr_t)NULL;
    tb->jmp_list_next[0] = (uintptr_t)NULL;
    tb->jmp_list_next[1] = (uintptr_t)NULL;

    /* init top in and top out */
    tb->s_data->_top_out = -1;
    tb->s_data->_top_in = -1;
    tb->jmp_reset_offset[0] = TB_JMP_RESET_OFFSET_INVALID;
    tb->jmp_reset_offset[1] = TB_JMP_RESET_OFFSET_INVALID;
    tb->jmp_indirect = TB_JMP_RESET_OFFSET_INVALID;
#ifdef CONFIG_LATX_INSTS_PATTERN
    tb->eflags_target_arg[0] = TB_JMP_RESET_OFFSET_INVALID;
    tb->eflags_target_arg[1] = TB_JMP_RESET_OFFSET_INVALID;
    tb->eflags_target_arg[2] = TB_JMP_RESET_OFFSET_INVALID;
#endif
    tb->bool_flags = OPT_BCC;
    tb->first_jmp_align = TB_JMP_RESET_OFFSET_INVALID;
    tb_set_page_addr0(tb, -1);
    tb_set_page_addr1(tb, -1);
#ifdef CONFIG_LATX_JRRA
    tb->next_86_pc = 0;
    tb->return_target_ptr = NULL;
#endif
#ifdef CONFIG_LATX_TU
    tb->s_data->offset_in_tu = 0;
    tb->s_data->next_pc = 0;
    tb->s_data->target_pc = 0;
    tb->s_data->tu_id = 0;
    tb->s_data->is_first_tb = 0;
    tb->s_data->last_ir1_type = 0;
    tb->s_data->tu_tb_mode = TU_TB_MODE_NONE;
    tb->tu_jmp[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;
    tb->tu_jmp[TU_TB_INDEX_NEXT] = TB_JMP_RESET_OFFSET_INVALID;
    tb->jmp_target_arg[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;
    tb->jmp_target_arg[TU_TB_INDEX_NEXT] = TB_JMP_RESET_OFFSET_INVALID;
    tb->jmp_stub_reset_offset[0] = TB_JMP_RESET_OFFSET_INVALID;
    tb->jmp_stub_reset_offset[1] = TB_JMP_RESET_OFFSET_INVALID;
#endif

#ifdef CONFIG_LATX_AOT
    tb->s_data->rel_start = -1;
    tb->s_data->rel_end = -1;
#endif
    tb->s_data->next_tb[TU_TB_INDEX_NEXT] = NULL;
    tb->s_data->next_tb[TU_TB_INDEX_TARGET] = NULL;
}

/* Create a TB and initialize it. */
static __thread TranslationBlock tu_tb_tmp;
static __thread struct separated_data tmp_s_data;
static __thread uint8_t search_buff[MAX_TB_IN_CACHE * TCG_MAX_INSNS];
static __thread uint32_t search_buff_offset[MAX_TB_IN_CACHE];

#include<sys/syscall.h>
TranslationBlock* tb_create(CPUState *cpu, target_ulong pc,
        target_ulong cs_base, uint32_t flags, int cflags,
        int max_insns, bool tb_is_code64, TU_TB_START_TYPE mode)
{
    TranslationBlock* tb;
    if (unlikely(pc == 0)) {
	return NULL;
    }

    /* todo: now check 16bit(should 15bit for x86) */
    uint32_t check_zero = cpu_lduw_code((CPUArchState *)cpu->env_ptr, pc);
    if (unlikely(check_zero == 0)){
	return NULL;
    }

    tb = &tu_tb_tmp;
    tb->s_data = &tmp_s_data;
    tu_reset_tb(tb);
    tb->pc = pc;
    /* tb->cs_base = cs_base; */
    tb->flags = flags;
    tb->cflags = cflags;
    /* tb->tu_start_mode = mode; */
    tcg_ctx->tb_cflags = cflags;
    tcg_ctx->tb_jmp_reset_offset = tb->jmp_reset_offset;
    if (TCG_TARGET_HAS_direct_jump) {
        tcg_ctx->tb_jmp_insn_offset = tb->jmp_target_arg;
        tcg_ctx->tb_jmp_target_addr = NULL;
    } else {
        tcg_ctx->tb_jmp_insn_offset = NULL;
        tcg_ctx->tb_jmp_target_addr = tb->jmp_target_arg;
    }

    if (in_pre_translate) {
        X86CPU *x86cpu = X86_CPU(cpu);
        CPUX86State *env = &x86cpu->env;
        if (tb_is_code64 && !CODEIS64) {
            env->hflags |= HF_LMA_MASK;
            cpu_x86_load_seg_cache(env, R_CS, 0xf000, 0xffff0000, 0xffff,
                    DESC_P_MASK | DESC_S_MASK | DESC_CS_MASK |
                    DESC_R_MASK | DESC_A_MASK | DESC_L_MASK);
        } else if (!tb_is_code64 && CODEIS64) {
            env->hflags &= ~HF_LMA_MASK;
            cpu_x86_load_seg_cache(env, R_CS, 0xf000, 0xffff0000, 0xffff,
                    DESC_P_MASK | DESC_S_MASK | DESC_CS_MASK |
                    DESC_R_MASK | DESC_A_MASK);
        }
        if (tb_is_code64) {
            tb->bool_flags |= IS_CODE64;
        }
    }

    target_disasm(tb, max_insns);

    if (tb->icount == 0 || tb->s_data->tu_tb_mode == TU_TB_MODE_BROKEN
			|| tb->s_data->tu_tb_mode == BAD_TB) {
        return NULL;
    }
    tb = tcg_tb_alloc_full(tcg_ctx);
    if (unlikely(!tb)) {
        qemu_log_mask(LAT_LOG_AOT, "tb flush in tu translate\n");
        /* exit(-1); */
        /* flush must be done */
        tb_flush(cpu);
        mmap_unlock();
        /* Make the execution loop process the flush as soon as possible.  */
        cpu->exception_index = EXCP_INTERRUPT;
        cpu_loop_exit(cpu);
    }
    tu_tb_tmp.s_data = tb->s_data;
    memcpy(tb, &tu_tb_tmp, sizeof(TranslationBlock));
    memcpy(tb->s_data, &tmp_s_data, sizeof(struct separated_data));
    aot_tb_insert(tb);

    return tb;
}

static inline ADDRX get_page(ADDRX pc)
{
    return pc & TARGET_PAGE_MASK;
}

static inline int tb_sort_cmp(const void *ap, const void *bp)
{
    const TranslationBlock *a = *(const TranslationBlock **)ap;
    const TranslationBlock *b = *(const TranslationBlock **)bp;
    return a->pc < b->pc ? -1 : a->pc > b->pc;
}

static inline void tb_add_jump(TranslationBlock *tb, int n,
                               TranslationBlock *tb_next)
{
    uintptr_t old;

    qemu_thread_jit_write();
    lsassert(n < ARRAY_SIZE(tb->jmp_list_next));
    qemu_spin_lock(&tb_next->jmp_lock);

    /* make sure the destination TB is valid */
    if (tb_next->cflags & CF_INVALID) {
        goto out_unlock_next;
    }
    /* Atomically claim the jump destination slot only if it was NULL */
    old = qatomic_cmpxchg(&tb->jmp_dest[n], (uintptr_t)NULL,
                          (uintptr_t)tb_next);
    if (old) {
        goto out_unlock_next;
    }

#ifdef CONFIG_LATX
    /* check fpu rotate and patch the native jump address */
    latx_tb_set_jmp_target(tb, n, tb_next);
#else
    /* patch the native jump address */
    tb_set_jmp_target(tb, n, (uintptr_t)tb_next->tc.ptr);
#endif

    /* add in TB jmp list */
    tb->jmp_list_next[n] = tb_next->jmp_list_head;
    tb_next->jmp_list_head = (uintptr_t)tb | n;

    qemu_spin_unlock(&tb_next->jmp_lock);

    qemu_log_mask_and_addr(CPU_LOG_EXEC, tb->pc,
                           "Linking TBs %p [" TARGET_FMT_lx
                           "] index %d -> %p [" TARGET_FMT_lx "]\n",
                           tb->tc.ptr, tb->pc, n,
                           tb_next->tc.ptr, tb_next->pc);
    return;

 out_unlock_next:
    qemu_spin_unlock(&tb_next->jmp_lock);
    return;
}
static void generate_tu_switch_native_tb(TranslationBlock *broken_tb)
{
    ADDR succ_x86_addr = broken_tb->pc;
    IR2_OPND succ_x86_addr_opnd = ra_alloc_dbt_arg2();
    IR2_OPND base = ra_alloc_data();
    IR2_OPND target = ra_alloc_data();
    IR2_OPND la_ret_opnd = V0_RENAME_OPND;
    IR2_OPND tb_ptr_opnd = LAST_TB_OPND;
    TranslationBlock *tb = broken_tb;

    /* set base_address data */
    la_data_li(base, (ADDR)tb->tc.ptr);
    aot_load_host_addr(tb_ptr_opnd, broken_tb, LOAD_TB_ADDR, 0);

    target_ulong call_offset __attribute__((unused)) =
            aot_get_call_offset(succ_x86_addr);
    lsassert(succ_x86_addr);
    aot_load_host_addr(succ_x86_addr_opnd, succ_x86_addr,
            LOAD_CALL_TARGET, call_offset);

#ifdef CONFIG_LATX_LAZYEXITPC
    /* here we store EIP to env
     * since context_switch_native_to_bt will not */
    la_store_addrx(succ_x86_addr_opnd, env_ir2_opnd,
            lsenv_offset_of_eip(lsenv));
#endif

    la_ori(la_ret_opnd, zero_ir2_opnd, 1);
    la_data_li(target, context_switch_native_to_bt);
    #ifdef TU_DEBUG //for debug
    la_ld_d(tb_ptr_opnd, zero_ir2_opnd, 0);
    #endif
    aot_la_append_ir2_jmp_far(target, base, B_EPILOGUE, 0);
}

static int tu_switch_native_tb(TranslationBlock *broken_tb)
{
    int code_nr = 0;
    TRANSLATION_DATA *lat_ctx = lsenv->tr_data;

    tr_init(NULL);
    generate_tu_switch_native_tb(broken_tb);
    label_dispose(NULL, lat_ctx);
    code_nr = tr_ir2_assemble((void *)broken_tb->tc.ptr,
                              lat_ctx->first_ir2);
    tr_fini(false);
    return code_nr * 4;
}

static void tu_set_tb_to_translate_context(TranslationBlock *tb)
{
    tb->s_data->next_tb[TU_TB_INDEX_NEXT] = NULL;
    tb->s_data->next_tb[TU_TB_INDEX_TARGET] = NULL;
    tb->s_data->tu_tb_mode = TU_TB_MODE_SWITCH_TO_TB;
    tb->tu_jmp[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;
    tb->tu_jmp[TU_TB_INDEX_NEXT] = TB_JMP_RESET_OFFSET_INVALID;
}

static bool tu_split_tb(TranslationBlock *pre_tb, TranslationBlock *tb)
{
    bool spilt_success = false;
    ADDRX pre_pc, tb_pc = ((IR1_INST *)(tb->s_data->ir1))[0].info->address;
    if (pre_tb->icount < tb->icount) {
        for (int i = 0; i < pre_tb->icount; i++) {
            pre_pc = ((IR1_INST *)(pre_tb->s_data->ir1))[i].info->address;
            if (pre_pc == tb_pc) {
                spilt_success = true;
                break;
            }
        }
    } else if (pre_tb->icount > tb->icount) {
        uint32_t th_firs_inst = pre_tb->icount - tb->icount;
        lsassert(th_firs_inst <= pre_tb->icount);
        pre_pc = ((IR1_INST *)(pre_tb->s_data->ir1))[th_firs_inst].info->address;
        if (pre_pc == tb_pc) {
            spilt_success = true;
        }
        #ifdef TU_DEBUG
        else {
            qemu_log_mask(LAT_LOG_AOT, "%d TU WARNING: tb->pc = "TARGET_FMT_lx" is error,"
                " skiped.\n", getpid(), tb->pc);
        }
        #endif
    } else {
        lsassert(0);
    }

    if (spilt_success) {
        /* tb->tu_start_mode = TU_TB_START_NORMAL; */
        pre_tb->s_data->next_tb[TU_TB_INDEX_NEXT] = NULL;
        pre_tb->s_data->next_tb[TU_TB_INDEX_TARGET] = NULL;
        pre_tb->size -= tb->size;
        pre_tb->icount -= tb->icount;
        pre_tb->s_data->next_pc = tb->pc;
        pre_tb->tu_jmp[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;
        pre_tb->tu_jmp[TU_TB_INDEX_NEXT] = TB_JMP_RESET_OFFSET_INVALID;
        unset_use_tu_jmp(pre_tb);
    } else {
        /*Can't judge pre_tb or tb was broken.*/
        tu_set_tb_to_translate_context(pre_tb);
        tu_set_tb_to_translate_context(tb);
    }
    return spilt_success;
}

static inline void get_tu_queue(CPUState *cpu,
		 target_ulong cs_base, uint32_t flags,
		 int cflags, int max_insns)
{
    ADDRX ir1_next_pc, ir1_target_pc;
    TranslationBlock** tb_list = tu_data->tb_list;
    uint32_t *tb_num_in_tu = &tu_data->tb_num;
    TranslationBlock *next_tb, *target_tb, *tb;
    for (int i = 0; i <  *tb_num_in_tu && *tb_num_in_tu < MAX_TB_IN_TU; i++) {
        tb = tb_list[i];
        ir1_next_pc = tb->s_data->next_pc;
        ir1_target_pc = tb->s_data->target_pc;
        switch (tb->s_data->last_ir1_type) {
        case IR1_TYPE_BRANCH:
            /* Jcc next tb should be translated without checking */
            lsassert(ir1_next_pc);
            if (get_page(tb->pc) == get_page(ir1_next_pc)) {
                next_tb = tu_tree_lookup(ir1_next_pc);
                if (!next_tb) {
                    next_tb = tb_htable_lookup(cpu,
                        ir1_next_pc, cs_base, flags, cflags);
                }
                if (!next_tb) {
                    next_tb = tb_create(cpu, ir1_next_pc, cs_base, flags,
                                   cflags, max_insns, CODEIS64, TU_TB_START_NORMAL);
                    tu_push_back(next_tb);
                }
                if (next_tb && next_tb->tc.ptr != NULL) {
                    tb->tu_jmp[TU_TB_INDEX_NEXT] = 0;
                }
                tb->s_data->next_tb[TU_TB_INDEX_NEXT] = next_tb;
            } else {
                tb->s_data->next_tb[TU_TB_INDEX_NEXT] = NULL;
            }
            if (tb->tu_jmp[TU_TB_INDEX_NEXT] == 0) {
                lsassert(tb->s_data->next_tb[TU_TB_INDEX_NEXT]);
            }

            lsassert(ir1_target_pc);
            /* Jcc target tb should not be translated if the distance is too far */
            if (get_page(tb->pc) == get_page(ir1_target_pc)) {
                target_tb = tu_tree_lookup(ir1_target_pc);
                if (!target_tb) {
                    target_tb = tb_htable_lookup(cpu,
                        ir1_target_pc, cs_base, flags, cflags);
                }
                if (!target_tb) {
                    target_tb = tb_create(cpu, ir1_target_pc, cs_base, flags,
                                     cflags, max_insns, CODEIS64, TU_TB_START_JMP);
                    tu_push_back(target_tb);
                }
                if (target_tb && target_tb->tc.ptr != NULL) {
                    tb->tu_jmp[TU_TB_INDEX_TARGET] = 0;
                }
                tb->s_data->next_tb[TU_TB_INDEX_TARGET] = target_tb;
            } else {
                tb->s_data->next_tb[TU_TB_INDEX_TARGET] = NULL;
            }

            if (tb->tu_jmp[TU_TB_INDEX_TARGET] == 0) {
                lsassert(tb->s_data->next_tb[TU_TB_INDEX_TARGET]);
            }
            break;
        case IR1_TYPE_JUMP:
            /* JMP target tb should not be translated if the distance is too far */
            if (get_page(tb->pc) == get_page(ir1_target_pc)) {
                target_tb = tu_tree_lookup(ir1_target_pc);
                if (!target_tb) {
                    target_tb = tb_htable_lookup(cpu,
                        ir1_target_pc, cs_base, flags, cflags);
                }
                if (get_page(tb->pc) == get_page(ir1_target_pc) && !target_tb) {
                    target_tb = tb_create(cpu, ir1_target_pc, cs_base, flags,
                                     cflags, max_insns, CODEIS64, TU_TB_START_JMP);
                    tu_push_back(target_tb);
                }
                tb->s_data->next_tb[TU_TB_INDEX_TARGET] = target_tb;
            } else {
                tb->s_data->next_tb[TU_TB_INDEX_TARGET] = NULL;
            }
            break;
        case IR1_TYPE_CALLIN:
        case IR1_TYPE_CALL:
        case IR1_TYPE_NORMAL:
        case IR1_TYPE_SYSCALL:
            lsassert(ir1_next_pc);
            if (get_page(tb->pc) == get_page(ir1_next_pc)) {
                next_tb = tu_tree_lookup(ir1_next_pc);
                if (!next_tb) {
                    next_tb = tb_htable_lookup(cpu,
                        ir1_next_pc, cs_base, flags, cflags);
                }
                if (!next_tb) {
                    next_tb = tb_create(cpu, ir1_next_pc, cs_base, flags,
                                   cflags, max_insns, CODEIS64, TU_TB_START_NORMAL);
                    tu_push_back(next_tb);
                }
                tb->s_data->next_tb[TU_TB_INDEX_NEXT] = next_tb;
            } else {
                tb->s_data->next_tb[TU_TB_INDEX_NEXT] = NULL;
            }
            break;
        case IR1_TYPE_JUMPIN:
        case IR1_TYPE_RET:
            break;
        default:
            lsassert(0);
        }
    }

/* Note: *tb_num_in_tu maybe greater than MAX_TB_IN_TU. */
    if (*tb_num_in_tu > MAX_TB_IN_TU + 1) {
        lsassert(0);
    }
}

void solve_tb_overlap(uint tb_num_in_tu,
		TranslationBlock **tb_list, int max_insns)
{
    TranslationBlock *pre_tb = tb_list[0];
    TranslationBlock *tb;
    for (int i = 1; i < tb_num_in_tu; i++) {
        tb = tb_list[i];
        if (pre_tb->s_data->tu_tb_mode == TU_TB_MODE_BROKEN
                || pre_tb->s_data->tu_tb_mode == BAD_TB) {
            assert(0);
            pre_tb = tb;
            continue;
        }
        /* judge if TB need split
         * pre_tb-------->pc + 0
         *                pc + 1
         *                pc + 2
         * tb------------>pc + 3
         *                pc + 4
         *                pc + 5
         * pre_tb_next--->pc + 6
         */
        if (tb->pc < pre_tb->s_data->next_pc && tb->pc > pre_tb->pc) {
            /* when split prefix ins, translate it two times.
             *
             * pre_tb--------> lock
             * tb------------> add
             *                 ...
             * tb_end--------> jmp
             *
             */
            if (pre_tb->icount == tb->icount) {
                /* pre_tb dont link in TU */
                pre_tb->s_data->next_tb[TU_TB_INDEX_NEXT] = NULL;
                pre_tb->s_data->next_tb[TU_TB_INDEX_TARGET] = NULL;
                pre_tb->tu_jmp[TU_TB_INDEX_TARGET] =
                    TB_JMP_RESET_OFFSET_INVALID;
                pre_tb->tu_jmp[TU_TB_INDEX_NEXT] =
                    TB_JMP_RESET_OFFSET_INVALID;
                pre_tb->s_data->tu_tb_mode = TU_TB_MODE_SWITCH_TO_TB;
                pre_tb = tb;
                continue;
            }
            if (pre_tb->icount < tb->icount) {
                /*Can't judge pre_tb or tb was broken.*/
                tu_set_tb_to_translate_context(pre_tb);
                tu_set_tb_to_translate_context(tb);
                pre_tb = tb;
                continue;
            }
            /*if pre_tb is splited, unlink pre_tb in tu */
            pre_tb->s_data->next_tb[TU_TB_INDEX_NEXT] = NULL;
            pre_tb->s_data->next_tb[TU_TB_INDEX_TARGET] = NULL;

            /*When pre_tb->icount achive max_insns, the end addresses of two TBS are different*/
            if (pre_tb->icount == max_insns) {
                pre_tb->tu_jmp[TU_TB_INDEX_TARGET] =
                    TB_JMP_RESET_OFFSET_INVALID;
                pre_tb->tu_jmp[TU_TB_INDEX_NEXT] =
                    TB_JMP_RESET_OFFSET_INVALID;
            } else {
                tu_split_tb(pre_tb, tb);
            }
            /* Note: some message of pre_tb last ir1 ins is not modify */
        }
        if (pre_tb->s_data->tu_tb_mode != TU_TB_MODE_BROKEN) {
            pre_tb = tb;
        }
    }
}

static TranslationBlock *tb_explore(CPUState *cpu,
                              target_ulong pc, target_ulong cs_base,
                              uint32_t flags, int cflags)
{
    TranslationBlock** tb_list = tu_data->tb_list;
    uint32_t *tb_num_in_tu = &tu_data->tb_num;
    uint32_t *ir1_num_in_tu = &tu_data->ir1_num_in_tu;
    int max_insns;

    max_insns = cflags & CF_COUNT_MASK;
    if (max_insns == 0) {
        max_insns = CF_COUNT_MASK;
    }
    if (max_insns > TCG_MAX_INSNS) {
        max_insns = TCG_MAX_INSNS;
    }
    if (cpu->singlestep_enabled || singlestep) {
        max_insns = 1;
    }
    *tb_num_in_tu = 0;
    *ir1_num_in_tu = 0;

    tu_enough_space(cpu);

    /* the entry used as return value*/
    TranslationBlock *entry = tb_create(cpu, pc, cs_base,
            flags, cflags, max_insns, CODEIS64 , TU_TB_START_ENTRY);
    tu_push_back(entry);

    /* search all tbs we can get */
    get_tu_queue(cpu, cs_base, flags, cflags, max_insns);

    /* sort tbs by PC */
    qsort(tb_list, *tb_num_in_tu, sizeof(TranslationBlock *), tb_sort_cmp);

    /* Some TBS may overlap. We split these overlapping TBS. */
    solve_tb_overlap(*tb_num_in_tu, tb_list, max_insns);

    for (int i = 0; i < *tb_num_in_tu; i++){
        /* record tu pc and tb_num */
        tb_list[i]->s_data->tu_id = tb_list[0]->pc;
    }

    /* flag reduction */
    tu_ir1_optimization(tb_list, *tb_num_in_tu);

    return entry;
}

uint bcc_ins_convert(uint convert_insn)
{
    uint fix_insn = convert_insn;
    switch (fix_insn & BCC_OPCODE) {
    case BNE_OPCODE:
        fix_insn &= 0x3ff;
        fix_insn |= BEQ_OPCODE;
        break;
    case BEQ_OPCODE:
        fix_insn &= 0x3ff;
        fix_insn |= BNE_OPCODE;
        break;
    case BLT_OPCODE:
        fix_insn &= 0x3ff;
        fix_insn |= BGE_OPCODE;
        break;
    case BGE_OPCODE:
        fix_insn &= 0x3ff;
        fix_insn |= BLT_OPCODE;
        break;
    case BLTU_OPCODE:
        fix_insn &= 0x3ff;
        fix_insn |= BGEU_OPCODE;
        break;
    case BGEU_OPCODE:
        fix_insn &= 0x3ff;
        fix_insn |= BLTU_OPCODE;
        break;
    case BNEZ_OPCODE:
        fix_insn &= 0x3ff;
        fix_insn |= BEQZ_OPCODE;
        break;
    case BEQZ_OPCODE:
        fix_insn &= 0x3ff;
        fix_insn |= BNEZ_OPCODE;
        break;
    case BCEQZ_OPCODE:
        if ((fix_insn & BFCC_OPCODE) == BCEQZ_OPCODE){
            fix_insn &= 0xe0;
            fix_insn |= BCNEZ_OPCODE;
        } else {
            fix_insn &= 0xe0;
            fix_insn |= BCEQZ_OPCODE;
        }
        break;
    default:
        lsassert(0);
        break;
    }
    return fix_insn;
}

static bool bcc_jmp_fail;

int tu_relocat_target_branch(TranslationBlock * tb)
{
    TranslationBlock *target_tb = tb->s_data->next_tb[TU_TB_INDEX_TARGET];

    uintptr_t fix_addr = (uintptr_t)tb->tc.ptr + tb->tu_jmp[TU_TB_INDEX_TARGET];
    uint fix_insn = *(uint *)fix_addr;
    long offset = (uintptr_t)target_tb->tc.ptr - fix_addr;
    lsassert(target_tb->tc.ptr);
    offset >>= 2;
    switch (fix_insn & BCC_OPCODE) {
    case BEQ_OPCODE:
    case BNE_OPCODE:
    case BLT_OPCODE:
    case BGE_OPCODE:
    case BLTU_OPCODE:
    case BGEU_OPCODE:
        /* clear bcc ins offset */
        if (offset < OFF16_MIN || offset > OFF16_MAX) {
            if (!bcc_jmp_fail) {
                bcc_jmp_fail = true;
                return -1;
            }
#ifdef CONFIG_LATX_LARGE_CC
            uintptr_t fix_b_addr = ROUND_UP(fix_addr + 4, CODE_GEN_ALIGN / 2);
#else
            uintptr_t fix_b_addr = fix_addr + 4;
#endif
            lsassert(fix_b_addr <= (uintptr_t)tb->tc.ptr + tb->tc.size - 4);
            fix_insn &= OFF16_MASK;
            fix_insn = bcc_ins_convert(fix_insn);
            /* change target and next */
            offset = fix_b_addr + 4 - fix_addr;
#ifdef CONFIG_LATX_LARGE_CC
            offset += 4;
#endif
            offset >>= 2;
            fix_insn |= (offset & BITS16_MASK) << 10;
            lsassert(fix_b_addr <= (uintptr_t)tb->tc.ptr + tb->tc.size - 4);
            tb_target_set_jmp_target((uintptr_t)tb->tc.ptr,
                fix_b_addr, fix_b_addr, (uintptr_t)target_tb->tc.ptr);
        }
        fix_insn &= OFF16_MASK;
        fix_insn |= (offset & BITS16_MASK) << OFF16_SHIFT;
        *(uint *)(fix_addr) = fix_insn;
        break;
    case B_OPCODE:
    case BL_OPCODE:
        tb_target_set_jmp_target((uintptr_t)tb->tc.ptr,
                                 fix_addr,
                                 fix_addr,
                                 (uintptr_t)target_tb->tc.ptr);
        break;
    /* BCNEZ_OPCODE is same as BCEQZ_OPCODE*/
    case BCEQZ_OPCODE:
    case BEQZ_OPCODE:
    case BNEZ_OPCODE:
        if (offset < OFF20_MIN || offset > OFF20_MAX){
            if (!bcc_jmp_fail) {
                bcc_jmp_fail = true;
                return -1;
            }
#ifdef CONFIG_LATX_LARGE_CC
            uintptr_t fix_b_addr = ROUND_UP(fix_addr + 4, CODE_GEN_ALIGN / 2);
#else
            uintptr_t fix_b_addr = fix_addr + 4;
#endif
            lsassert(fix_b_addr < (uintptr_t)tb->tc.ptr + tb->tc.size - 4);
            fix_insn &= OFF20_MASK;
            /* change target and next */
            fix_insn = bcc_ins_convert(fix_insn);
            offset = fix_b_addr + 4 - fix_addr;
#ifdef CONFIG_LATX_LARGE_CC
            offset += 4;
#endif
            offset >>= 2;
            fix_insn |= (offset & BITS16_MASK) << 10;
            lsassert(fix_b_addr < (uintptr_t)tb->tc.ptr + tb->tc.size - 4);
            tb_target_set_jmp_target((uintptr_t)tb->tc.ptr,
                fix_b_addr, fix_b_addr, (uintptr_t)target_tb->tc.ptr);
        }
        fix_insn &= OFF20_MASK;
        fix_insn |= (offset & BITS16_MASK) << OFF16_SHIFT;
        fix_insn |= (offset >> OFF20_HI_SHIFT) & OFF20_HI_BITS;
        *(uint *)(fix_addr) = fix_insn;
        break;
    default:
        lsassert(0);
        break;
    }
    return 0;
}

void tu_relocat_next_branch(TranslationBlock * tb)
{
    TranslationBlock *tb_next = tb->s_data->next_tb[TU_TB_INDEX_NEXT];
    uintptr_t fix_addr = (uintptr_t)tb->tc.ptr + tb->tu_jmp[TU_TB_INDEX_NEXT];
    uint fix_insn = *(uint *)fix_addr;

    lsassert(tb_next->tc.ptr);

    if ((fix_insn & BCC_OPCODE) == B_OPCODE) {/*b*/
        tb_target_set_jmp_target((uintptr_t)tb->tc.ptr,
            fix_addr, fix_addr, (uintptr_t)tb_next->tc.ptr);
    } else {
        lsassert(0);
    }
}

#ifdef CONFIG_LATX_AOT
void bcc_ins_recover(TranslationBlock *tb) {
    uintptr_t bcc_addr = (uintptr_t)tb->tc.ptr +
        tb->tu_jmp[TU_TB_INDEX_TARGET];
    uint fix_insn = *(uint *)bcc_addr;
    uintptr_t fix_b_addr;
    switch(fix_insn & BCC_OPCODE) {
        case BEQ_OPCODE:
        case BNE_OPCODE:
        case BLT_OPCODE:
        case BLTU_OPCODE:
        case BGE_OPCODE:
        case BGEU_OPCODE:
        case BCEQZ_OPCODE:
        case BEQZ_OPCODE:
        case BNEZ_OPCODE:
            /* fix_b_addr = ROUND_UP(bcc_addr + 4, CODE_GEN_ALIGN / 2); */
#ifdef CONFIG_LATX_LARGE_CC
            fix_b_addr = ROUND_UP(bcc_addr + 4, CODE_GEN_ALIGN / 2);
#else
            fix_b_addr = bcc_addr + 4;
#endif
            lsassert(fix_b_addr < (uintptr_t)tb->tc.ptr + tb->tc.size);
            if (*(uint *)fix_b_addr != 0x03400000) {
                *(uint *)bcc_addr = bcc_ins_convert(fix_insn);
            }
            *(uint *)fix_b_addr = 0x03400000;
            break;
        default:
            assert(0);
            break;
    }
}
#endif

#ifdef CONFIG_LATX_AOT
static void fix_rel_table(uint32_t tb_num_in_tu, TranslationBlock **tb_list)
{
    TranslationBlock *tb;
    for (int i = 0; i < tb_num_in_tu; i++) {
        tb = tb_list[i];
        for (int j = tb->s_data->rel_start; j != -1 && j <= tb->s_data->rel_end; j++) {
            if (rel_table[j].tc_offset >= tb->tc.size) {
                rel_table[j].tc_offset += tb->tu_unlink.stub_offset - tb->tc.size;
            }
        }
    }
}
#endif

static void mov_unlink_stub_to_end(uint32_t tb_num_in_tu, TranslationBlock **tb_list)
{
    if (tb_num_in_tu <= 1) {
        return;
    }
    qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
            ROUND_UP((uintptr_t)tcg_ctx->code_gen_ptr, CODE_GEN_ALIGN));
    uint32_t *tmp_buffer = (uint32_t *)tcg_ctx->code_gen_ptr;
    assert(tb_list[0]->s_data->tu_size % 4 == 0);
    uintptr_t tmp_buffer_size = 0;
    uintptr_t curr_pos;
    TranslationBlock *tb;
    for (int i = 0; i < tb_num_in_tu; i++) {
        tb = tb_list[i];
        if (use_tu_jmp(tb)) {
#ifdef CONFIG_LATX_INSTS_PATTERN
            tb->eflags_target_arg[0] = TB_JMP_RESET_OFFSET_INVALID;
            tb->eflags_target_arg[1] = TB_JMP_RESET_OFFSET_INVALID;
            tb->eflags_target_arg[2] = TB_JMP_RESET_OFFSET_INVALID;
            tb->bool_flags &= ~TARGET1_ELIMINATE;
#endif
            curr_pos = (uintptr_t)tb->tc.ptr + tb->tu_unlink.stub_offset;
            size_t unlink_stub_size = tb->tc.size - tb->tu_unlink.stub_offset;
            memcpy((void *)((uintptr_t)tmp_buffer + tmp_buffer_size),
                    (void *)curr_pos, unlink_stub_size);
            assert(curr_pos % 4 == 0);
            if (tb->tu_unlink.stub_offset != tb->tc.size - unlink_stub_size) {
                qemu_log_mask(LAT_LOG_AOT , " !!!!!! unlink_stub_size error: %x %lx\n",
                        tb->tu_unlink.stub_offset, tb->tc.size);
                assert(0);
            }
            tb->tu_unlink.stub_offset = tmp_buffer_size;
            tmp_buffer_size += unlink_stub_size;
            tb->tc.size -= unlink_stub_size;
        }
    }

    curr_pos = (uintptr_t)tb_list[0]->tc.ptr + tb_list[0]->tc.size;
    for (int i = 1; i < tb_num_in_tu; i++) {
        tb = tb_list[i];
        memmove((void *)curr_pos, tb->tc.ptr, tb->tc.size);
        assert(curr_pos % 4 == 0);
#ifdef CONFIG_LATX_JRRA
    	if (option_jr_ra || option_jr_ra_stack) {
            if (tb->next_86_pc && tb->return_target_ptr) {
                uintptr_t addr = (uintptr_t)tb->return_target_ptr;
                addr = addr - (uintptr_t)tb->tc.ptr;
                tb->return_target_ptr = (unsigned long *)(curr_pos + addr);
            }
        }
#endif
        tb->tc.ptr = (void *)curr_pos;
        tb->s_data->offset_in_tu = tb->tc.ptr - tb_list[0]->tc.ptr;
        curr_pos += tb->tc.size;
    }

    memcpy((void *)curr_pos, tmp_buffer, tmp_buffer_size);

    for (int i = 0; i < tb_num_in_tu; i++) {
        tb = tb_list[i];
        if (use_tu_jmp(tb)) {
            tb->tu_unlink.stub_offset = curr_pos  + tb->tu_unlink.stub_offset
                - (uintptr_t)tb->tc.ptr;
        }
    }

#ifdef CONFIG_LATX_AOT
    fix_rel_table(tb_num_in_tu, tb_list);
#endif
}

static char ir1_type_name[IR1_TYPE_COUNT][8] = {
    [IR1_TYPE_NORMAL]  = "normal",
    [IR1_TYPE_BRANCH]  = "branch",
    [IR1_TYPE_JUMP]    = "jmp",
    [IR1_TYPE_CALL]    = "call",
    [IR1_TYPE_RET]     = "ret",
    [IR1_TYPE_JUMPIN]  = "jmpin",
    [IR1_TYPE_CALLIN]  = "callin",
    [IR1_TYPE_SYSCALL] = "syscall",
};

static void dump_tu_ir2(uint32 tb_num_in_tu, TranslationBlock **tb_list)
{
    if (!option_dump_ir2) {
        return;
    }
    int all_ir2_num = 0;
    qemu_log("dump tu ir2, tb_num_in_tu %d pid %d \n", tb_num_in_tu, getpid());
    qemu_log("============================================================\n");
    for (int i = 0; i < tb_num_in_tu; i++) {
        TranslationBlock *tb = tb_list[i];
        qemu_log("tb %d pc 0x%lx next pc 0x%lx target_pc 0x%lx use_tu_jmp %d ",
                i, (uint64_t)tb->pc, (uint64_t)tb->s_data->next_pc,
                (uint64_t)tb->s_data->target_pc,  use_tu_jmp(tb));
        qemu_log("last_ir1_type %s -------------------\n",
                ir1_type_name[tb->s_data->last_ir1_type]);
        uint32_t *ir2_vec = (uint32_t *)tb->tc.ptr;
        for (int j = 0; j < tb->tc.size / 4; j++) {
            qemu_log("IR2[%04d][%03d] at %p 0x%08x ",
                    all_ir2_num, j, &ir2_vec[j], ir2_vec[j]);
            all_ir2_num++;
            la_disasm(ir2_vec[j]);
        }
    }
}

void translate_tu(uint32 tb_num_in_tu, TranslationBlock **tb_list)
{
    if (tb_num_in_tu == 0) {
        return;
    }
    int gen_code_size;
    uint32_t search_size = 0;
    TranslationBlock *tb;
    bcc_jmp_fail = false;

#if defined(CONFIG_LATX_TBMINI_ENABLE)
    uintptr_t last_tbmini_ptr =
        (uintptr_t)(ROUND_UP(sizeof(struct TBMini) * tb_num_in_tu +
        (uintptr_t)tcg_ctx->code_gen_ptr, qemu_icache_linesize));
    qatomic_set(&tcg_ctx->code_gen_ptr, (void *)last_tbmini_ptr);
#else
    qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
            ROUND_UP((uintptr_t)tcg_ctx->code_gen_ptr, CODE_GEN_ALIGN));
#endif

retry:
    for (uint32_t i = 0; i < tb_num_in_tu; i++) {
        tb = tb_list[i];
        if (tb->bool_flags & IS_CODE64) {
            CODEIS64 = 1;
        } else {
            CODEIS64 = 0;
        }
        gen_code_size = translate_tb_in_tu(tb);
        uintptr_t gen_code_buf = (uintptr_t)tcg_ctx->code_gen_ptr + gen_code_size;
        qatomic_set(&tcg_ctx->code_gen_ptr, (void *)gen_code_buf);
        tb->tc.size = gen_code_size;
        tb->s_data->offset_in_tu = tb->tc.ptr - tb_list[0]->tc.ptr;
        /* init original jump addresses which have been set during tcg_gen_code() */
        if (!use_tu_jmp(tb)) {
            if (tb->jmp_reset_offset[0] != TB_JMP_RESET_OFFSET_INVALID) {
                tb_reset_jump(tb, 0);
            }
            if (tb->jmp_reset_offset[1] != TB_JMP_RESET_OFFSET_INVALID) {
                tb_reset_jump(tb, 1);
            }
        }
        search_buff_offset[i] = search_size;
        search_size += encode_search(tb, search_buff + search_buff_offset[i]);
    }

    tb_list[0]->s_data->tu_size = tcg_ctx->code_gen_ptr - tb_list[0]->tc.ptr;

    mov_unlink_stub_to_end(tb_num_in_tu, tb_list);

    for (int i = 0; i < tb_num_in_tu; i++) {
        tb = tb_list[i];
        if (!use_tu_jmp(tb) || tb->s_data->last_ir1_type == IR1_TYPE_CALL) {
            continue;
        }
        if ((tb->tu_jmp[TU_TB_INDEX_TARGET] != TB_JMP_RESET_OFFSET_INVALID)) {
            if (tu_relocat_target_branch(tb)) {
                qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
                    ROUND_UP((uintptr_t)(tb_list[0]->tc.ptr), CODE_GEN_ALIGN));
                assert(bcc_jmp_fail);
                goto retry;
            }
            tb->tu_unlink.ins = *(uint32_t *)(tb->tc.ptr +
                    tb->tu_jmp[TU_TB_INDEX_TARGET]);
        }
        if (tb->tu_jmp[TU_TB_INDEX_NEXT] != TB_JMP_RESET_OFFSET_INVALID) {
            tu_relocat_next_branch(tb);
        }
    }

    /*search data*/
    for (int i = 0; i < tb_num_in_tu; i++) {
        tb = tb_list[i];
        tb->bool_flags |= IS_TU_TB;
        tb->tu_search_addr_offset = (uintptr_t)
            ((uintptr_t)tcg_ctx->code_gen_ptr + search_buff_offset[i] - (uintptr_t)tb->tc.ptr);
    }
    memcpy(tcg_ctx->code_gen_ptr, search_buff, search_size);
    qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
            ROUND_UP((uintptr_t)tcg_ctx->code_gen_ptr + search_size,
                CODE_GEN_ALIGN));
    tb_list[0]->s_data->tu_size = tcg_ctx->code_gen_ptr - tb_list[0]->tc.ptr;

    dump_tu_ir2(tb_num_in_tu, tb_list);
}

static void register_tu(uint32 tb_num_in_tu, TranslationBlock **tb_list,
		CPUState *cpu, int cflags)
{
    for (int i = 0; i < tb_num_in_tu; i++) {
        TranslationBlock *tb = tb_list[i];
        if (tb->s_data->tu_tb_mode != TU_TB_MODE_BROKEN
				&& tb->s_data->tu_tb_mode != BAD_TB) {
            CPUArchState *env = cpu->env_ptr;
            tb_page_addr_t phys_pc;
            tb_page_addr_t phys_page2;
            target_ulong virt_page2;
            phys_pc = get_page_addr_code(env, tb->pc);
            if (phys_pc == -1) {
                /* Generate a one-shot TB with 1 insn in it */
                cflags = (cflags & ~CF_COUNT_MASK) | CF_LAST_IO | 1;
            }
            /* should inset TB to Qht */
            virt_page2 = (tb->pc + tb->size - 1) & TARGET_PAGE_MASK;
            phys_page2 = -1;
            if ((tb->pc & TARGET_PAGE_MASK) != virt_page2) {
                phys_page2 = get_page_addr_code(env, virt_page2);
            }
            tb_link_page(tb, phys_pc, phys_page2);
            tcg_tb_insert(tb);
        }
    }

}

/* Called with mmap_lock held for user mode emulation.  */
TranslationBlock *tu_gen_code(CPUState *cpu,
                              target_ulong pc, target_ulong cs_base,
                              uint32_t flags, int cflags)
{
    TranslationBlock** tb_list = tu_data->tb_list;
    uint32_t *tb_num_in_tu = &tu_data->tb_num;

    /* TODO: profilea support */
    assert_memory_lock();
    qemu_thread_jit_write();
    /*
     * remove write prot of the page in case of paralell code modification,
     * but the paired page cross 16K boundary is still not protected.
     */
    if (page_get_flags(pc) & PAGE_WRITE) {
        mprotect((void *)(pc & qemu_host_page_mask),
            qemu_host_page_size, PROT_READ);
    }

    TranslationBlock* entry;
    /* 1.Search all TBs in the function */
    entry = tb_explore(cpu, pc, cs_base, flags, cflags);
    if (unlikely(entry == NULL)) {
	qemu_log_mask(LAT_LOG_AOT, "error translate a bad pc");
	return tb_gen_code(cpu, pc, cs_base, flags, cflags);
    }

    translate_tu(*tb_num_in_tu, tb_list);
    register_tu(*tb_num_in_tu, tb_list, cpu, cflags);
    tu_trees_reset();

    return entry;
}

#ifdef CONFIG_LATX_DEBUG
void print_ir1(TranslationBlock* tb)
{
    if (tb == NULL) {return;}
    fprintf(stderr, "------------icount %d---------------\n", tb->icount);
    IR1_INST * pir1 = tb->s_data->ir1;
    for (int i = 0; i < tb->icount; ++i) {
        fprintf(stderr, "pc %lx %s\t\t%s\n", pir1->info->address,
        pir1->info->mnemonic, pir1->info->op_str);
        pir1++;
    }
}


static int ir1_id, ir2_id;
void print_tu_tb(TranslationBlock *tb)
{
    print_ir1(tb);
    TRANSLATION_DATA *t = lsenv->tr_data;
    qemu_log_mask(LAT_LOG_AOT, "IR2 num = %d\n", t->ir2_inst_num_current);
    ir1_id = ir2_id = 0;
    for (int i = 0; i < t->ir2_inst_num_current; ++i) {
        IR2_INST *ir2 = &t->ir2_inst_array[i];
        char str[64];
        /* an empty IR2_INST was inserted into the ir2 */
        /* list, but not assigned yet. */
        if (ir2_opcode(ir2) == 0) {
            continue;
        }
        ir2_to_string(ir2, str);
        if (str[0] == '-') {
            ir1_id ++;
                qemu_log_mask(LAT_LOG_AOT, "[%d, %d] %s\n", ir2_id, ir1_id, str);
        } else {
                qemu_log_mask(LAT_LOG_AOT, "%s\n", str);
        }
        ir2_id++;
    }
}
#endif

void tu_ir1_optimization(TranslationBlock **tb_list, int tb_num_in_tu)
{
    int option_over_tb_rfd = 1;

    if (option_over_tb_rfd) {
        over_tb_rfd(tb_list, tb_num_in_tu);
    } else {
        for (int i = 0; i < tb_num_in_tu; i++) {
            ir1_optimization(tb_list[i]);
        }
    }

}

void tu_jcc_nop_gen(TranslationBlock *tb)
{
    if (bcc_jmp_fail) {
#ifdef CONFIG_LATX_LARGE_CC
        la_code_align(2, 0x03400000);
        la_nop();
        la_nop();
#else
        TranslationBlock *target_tb = tb->s_data->next_tb[TU_TB_INDEX_TARGET];
        uintptr_t fix_addr = (uintptr_t)tb->tc.ptr;
        long offset = (uintptr_t)target_tb->tc.ptr - fix_addr;
        if (offset < BCC_OFF16_MIN || offset > BCC_OFF16_MAX) {
            la_nop();
        }
#endif
    }
}

int translate_tb_in_tu(struct TranslationBlock *tb)
{
    tb->tc.ptr = tcg_splitwx_to_rx(tcg_ctx->code_gen_ptr);
    if (tb->s_data->tu_tb_mode == TU_TB_MODE_BROKEN) {
        return tu_switch_native_tb(tb);
    } else {
        return tr_translate_tb(tb);
    }
}
#endif

#if defined(CONFIG_LATX_TU) || defined(CONFIG_LATX_AOT)

#endif
#if defined(CONFIG_LATX_TU) && defined(CONFIG_LATX_INSTS_PATTERN)
bool judge_tu_eflag_gen(void *tb_in_tu) {
    TranslationBlock *tb = tb_in_tu;
    if (tb->s_data->next_tb[TU_TB_INDEX_NEXT] && tb->s_data->next_tb[TU_TB_INDEX_TARGET]) {
        /* when tb_next & tb_target exit, get eflags_target_arg */
        TranslationBlock *tb_target = tb->s_data->next_tb[TU_TB_INDEX_TARGET];
        TranslationBlock *tb_next = tb->s_data->next_tb[TU_TB_INDEX_NEXT];
        if (!tb_target->eflag_use && !tb_next->eflag_use){
            tb->eflags_target_arg[0] = TB_JMP_RESET_OFFSET_INVALID;
            tb->eflags_target_arg[1] = TB_JMP_RESET_OFFSET_INVALID;
            return true;
        } else {
            /* TODO */
        }
    }
    tb->tu_jmp[TU_TB_INDEX_NEXT] = TB_JMP_RESET_OFFSET_INVALID;
    tb->tu_jmp[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;
    return false;
}
#endif
#ifdef CONFIG_LATX_TU
bool is_tu_tb(TranslationBlock *tb)
{
    return (tb->bool_flags & IS_TU_TB) ? true : false;
}

bool use_tu_jmp(TranslationBlock *tb)
{
    return (tb->bool_flags & IS_TU_JMP) ? true : false;
}

void set_use_tu_jmp(TranslationBlock *tb)
{
    tb->bool_flags |= IS_TU_JMP;
}

void unset_use_tu_jmp(TranslationBlock *tb)
{
    tb->bool_flags &= ~IS_TU_JMP;
}
#else
bool use_tu_jmp(TranslationBlock *tb)
{
    return false;
}
#endif
