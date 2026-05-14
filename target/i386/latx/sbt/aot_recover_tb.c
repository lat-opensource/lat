/**
 * @file aot_recover_tb.c
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT optimization
 */
#include "aot_recover_tb.h"
#include "latx-options.h"
#include "lsenv.h"
#include "accel/tcg/internal.h"
#include "include/exec/cpu-all.h"
#include "aot_page.h"
#include "aot_smc.h"
#ifdef CONFIG_LATX_TU
#include "tu.h"
#endif

#ifdef CONFIG_LATX_AOT

inline static void copy_code_to_buff(void *tb_buff, aot_tb *p_aot_tb) {
    assert(p_aot_tb->tu_size >= p_aot_tb->tb_cache_size);
    memcpy(tb_buff, aot_buffer + p_aot_tb->tb_cache_offset,
        p_aot_tb->tu_size);
    uintptr_t new_ptr = (uintptr_t)tcg_ctx->code_gen_ptr + p_aot_tb->tu_size;
    qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
                ROUND_UP(new_ptr, CODE_GEN_ALIGN));
}

inline static TranslationBlock *creat_tb(aot_tb *p_aot_tb, abi_ulong start, 
        target_ulong base, void *tc_ptr) {
    TranslationBlock *tb = tcg_tb_alloc(tcg_ctx);
    if (unlikely(!tb)) {
	return NULL;
    }
    /* Initialize Tb */
    qemu_spin_init(&tb->jmp_lock);
    tb->lazypc[0] = p_aot_tb->lazypc[0];
    tb->lazypc[1] = p_aot_tb->lazypc[1];
    tb->canlink[0] = 1;
    tb->canlink[1] = 1;
    tb->lazylink[0] = 1;
    tb->lazylink[1] = 1;
    tb->cflags = p_aot_tb->cflags;
    tb->jmp_dest[0] = (uintptr_t)NULL;
    tb->jmp_dest[1] = (uintptr_t)NULL;
    tb->jmp_list_next[0] = (uintptr_t)NULL;
    tb->jmp_list_next[1] = (uintptr_t)NULL;
    tb->jmp_list_head = 0;
    tb->icount = p_aot_tb->icount;
    tb->tc.size = p_aot_tb->tb_cache_size;
    tb->tc.ptr = tc_ptr; 
    tb->pc = start + p_aot_tb->offset_in_segment;
    /* tb->cs_base = base; */
    tb->flags = p_aot_tb->flags;
    tb->size = p_aot_tb->size;
    tb->first_jmp_align = p_aot_tb->first_jmp_align;
    tb->bool_flags = p_aot_tb->bool_flags;
    tb->jmp_stub_target_arg[0] = p_aot_tb->jmp_stub_target_arg[0];
    tb->jmp_stub_target_arg[1] = p_aot_tb->jmp_stub_target_arg[1];
#ifdef CONFIG_LATX_TU
    if (is_tu_tb(tb)) {
        tb->tu_search_addr_offset = p_aot_tb->tu_search_addr_offset;
    }
#endif
    if (use_tu_jmp(tb)) {
        tb->tu_jmp[0] = p_aot_tb->tu_jmp[0];
        tb->tu_jmp[1] = p_aot_tb->tu_jmp[1];
        tb->tu_unlink.stub_offset = p_aot_tb->tu_unlink.stub_offset;
        tb->tu_unlink.ins = p_aot_tb->tu_unlink.ins;
    } else {
        tb->jmp_indirect = p_aot_tb->jmp_indirect;
        tb->jmp_reset_offset[0] = p_aot_tb->jmp_reset_offset[0];
        tb->jmp_reset_offset[1] = p_aot_tb->jmp_reset_offset[1];
        tb->jmp_target_arg[0] = p_aot_tb->jmp_target_arg[0];
        tb->jmp_target_arg[1] = p_aot_tb->jmp_target_arg[1];
        tb->jmp_stub_reset_offset[0] = p_aot_tb->jmp_stub_reset_offset[0];
        tb->jmp_stub_reset_offset[1] = p_aot_tb->jmp_stub_reset_offset[1];
    }
#if defined(CONFIG_LATX_JRRA) || defined(CONFIG_LATX_JRRA_STACK)
    tb->return_target_ptr = NULL;
    if (p_aot_tb->return_target_ptr_offset) {
        tb->next_86_pc = start + p_aot_tb->next_86_pc_offset;
    } else {
        tb->next_86_pc = 0;
    }
#endif
#ifdef CONFIG_LATX_INSTS_PATTERN
    tb->eflags_target_arg[0] = p_aot_tb->eflags_target_arg[0];
    tb->eflags_target_arg[1] = p_aot_tb->eflags_target_arg[1];
    tb->eflags_target_arg[2] = p_aot_tb->eflags_target_arg[2];
#endif
    tb->eflag_use = p_aot_tb->eflag_use;
#ifdef CONFIG_LATX_SMC_OPT
    if (tb->bool_flags & TBSMC_OPTED_MASK) {
        tb->smc_data = TBSMC_OPTED_MASK;
    } else {
        tb->smc_data = 0;
    }
#endif
    return tb;
}

#include "latx-signal.h"
#include "qemu/atomic.h"
#ifdef CONFIG_LATX_TBMINI_ENABLE
static inline void aot_tbmini_set_pointer(uintptr_t tbm, uint64_t tb_addr,
        uint64_t tc_size)
{
    *(uint64_t *)tbm = (tb_addr & MAKE_64BIT_MASK(0, HOST_VIRT_ADDR_SPACE_BITS)) |
                (tc_size << HOST_VIRT_ADDR_SPACE_BITS);
}
#endif

static void recover_tb_range(target_ulong page, struct aot_tb *p_aot_tbs,
        int num, abi_long start, abi_long end) 
{
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    target_ulong base = env->segs[R_CS].base;

/* AOT + TB */
#ifndef CONFIG_LATX_TU
    for (int i = 0; i < num; i++) {
        if ((p_aot_tbs[i].offset_in_segment > end - start)
				|| (p_aot_tbs[i].cflags != tcg_ctx->tb_cflags)) {
            continue;
        }
        qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
				ROUND_UP((uintptr_t)(tcg_ctx->code_gen_ptr), qemu_icache_linesize));
        void *tb_buff = (void *)tcg_splitwx_to_rx(tcg_ctx->code_gen_ptr);
#if defined(CONFIG_LATX_TBMINI_ENABLE)
        /* set TBMini */
        uintptr_t tbmini_ptr = (uintptr_t)tb_buff;
        tb_buff = (void *)ROUND_UP((uintptr_t)tb_buff + sizeof(struct TBMini), qemu_icache_linesize);

        TranslationBlock *tb = creat_tb(&p_aot_tbs[i], start, base, tb_buff);
        qatomic_set(&tcg_ctx->code_gen_ptr, tb_buff);
        aot_tbmini_set_pointer(tbmini_ptr, (uint64_t)tb, TB_MAGIC);
#else
        TranslationBlock *tb = creat_tb(&p_aot_tbs[i], start, base, tb_buff);
#endif
        copy_code_to_buff(tb_buff, &p_aot_tbs[i]);
        /* Relocate this tb by traverse aot_rel_table. */
        aot_do_tb_reloc(tb, &p_aot_tbs[i], start, end);
        if (p_aot_tbs[i].last_ir1_type == IR1_TYPE_BRANCH) {
            lsassert(tb->jmp_target_arg[1] && tb->jmp_target_arg[1]
                < tb->tc.size);
            tb_reset_jump(tb, 1);
        }
#ifdef CONFIG_LATX_PROFILER
        CLN_TB_PROFILE(tb);
#endif
        aot_tb_register(tb);

#ifdef CONFIG_LATX_DEBUG
        assert((tb->pc & TARGET_PAGE_MASK) == page);
	if (i) {
            if (p_aot_tbs[i].offset_in_segment <= p_aot_tbs[i - 1].offset_in_segment) {
                assert((p_aot_tbs[i].offset_in_segment
                    == p_aot_tbs[i - 1].offset_in_segment)
                    && (p_aot_tbs[i].cflags != p_aot_tbs[i - 1].cflags));
	    }
	}
#endif

#if defined(CONFIG_LATX_JRRA) || defined(CONFIG_LATX_JRRA_STACK)
        if (p_aot_tbs[i].return_target_ptr_offset) {
            tb->return_target_ptr = p_aot_tbs[i].return_target_ptr_offset
                + tb->tc.ptr;
            aot_link_tree_insert(thread_cpu,
                tb, tb->next_86_pc, (uint32_t *)(tb->tc.ptr +
                p_aot_tbs[i].return_target_ptr_offset), tb->flags,
                tb->cflags, AOT_LINK_TYPE_JRRA);
        }
#endif
    }

/* AOT + TU */
#else
    TranslationBlock *tb_in_order[MAX_TB_IN_CACHE];
    for (int i = 0; i < num;) {
#ifdef CONFIG_LATX_DEBUG
        /* Dump this tb. */
        if (option_debug_aot) {
            qemu_log_mask(LAT_LOG_AOT, "Loading tb: offset 0x%x, x86_size 0x%x\n",
                    p_aot_tbs[i].offset_in_segment, p_aot_tbs[i].size);
        }
#endif
        /* find first tb in tu */
        while ((i < num) && (p_aot_tbs[i].is_first_tb == 0)) {
            i++;
        }
        int j;
        for (j = i; j < num; j++) {
            if ((p_aot_tbs[i].tu_id != p_aot_tbs[j].tu_id) || 
			((j != i) && p_aot_tbs[j].is_first_tb)) {
                break;
            }
            tb_in_order[j - i] = creat_tb(&p_aot_tbs[j], start, base, NULL);
	    if (unlikely(!tb_in_order[j - i])) {
		return;
	    }
        }
#if defined(CONFIG_LATX_TBMINI_ENABLE)
        int tb_num_in_tu = j - i;
        /* Reserve space for tu tbmin table. */
        TBMini *tbmini_ptr= (TBMini *)
            ROUND_UP((uintptr_t)tcg_ctx->code_gen_ptr, CODE_GEN_ALIGN);
        qatomic_set(&tcg_ctx->code_gen_ptr, ROUND_UP((uintptr_t)tbmini_ptr +
                    sizeof(struct TBMini) * (tb_num_in_tu + 1), qemu_icache_linesize));
        /* First tbm save tb_num_in_tu and TB_MAGIC. */
        aot_tbmini_set_pointer((uintptr_t)tbmini_ptr, tb_num_in_tu, TB_MAGIC);
        for (int k = i; k < j; k++) {
            TranslationBlock *tb = tb_in_order[k - i];
            aot_tbmini_set_pointer((uintptr_t)++tbmini_ptr, (uint64_t)tb, (uint64_t)tb->tc.size);
        }
#else
    	qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
            ROUND_UP((uintptr_t)tcg_ctx->code_gen_ptr, CODE_GEN_ALIGN));
#endif

        void *tu_begin_ptr = tcg_ctx->code_gen_ptr;
        memcpy(tcg_ctx->code_gen_ptr, aot_buffer + p_aot_tbs[i].tb_cache_offset,
                    p_aot_tbs[i].tu_size);
        uintptr_t new_buf_ptr = 
            (uintptr_t)(tcg_ctx->code_gen_ptr + p_aot_tbs[i].tu_size);
        qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
                ROUND_UP(new_buf_ptr, CODE_GEN_ALIGN));
        for (int k = i; k < j; k++) {
            TranslationBlock *tb = tb_in_order[k - i];
            aot_tb *curr_aot_tb = &p_aot_tbs[k];
            tb->tc.ptr = tu_begin_ptr + curr_aot_tb->offset_in_tu;
            assert((tb->pc & TARGET_PAGE_MASK) == page);
            assert((tb->cflags & CF_PARALLEL) == (tcg_ctx->tb_cflags & CF_PARALLEL));
            aot_do_tb_reloc(tb, curr_aot_tb, start, end);
            target_ulong next_pc = 0, target_pc = 0;
            if (curr_aot_tb->next_tb_pc_offset != -1) {
               next_pc = curr_aot_tb->next_tb_pc_offset + start;
            }
            if (curr_aot_tb->target_tb_pc_offset != -1) {
               target_pc = curr_aot_tb->target_tb_pc_offset + start;
            }
            if (next_pc || target_pc) {
                assert(!use_tu_jmp(tb) && !use_indirect_jmp(tb));
                if (curr_aot_tb->last_ir1_type == IR1_TYPE_CALL) {
                    aot_link_tree_insert(tb, target_pc, 0);
                } else {
                    aot_link_tree_insert(tb, next_pc, target_pc);
                }
            }
#if defined(CONFIG_LATX_JRRA) || defined(CONFIG_LATX_JRRA_STACK)
            else if (curr_aot_tb->return_target_ptr_offset) {
                tb->return_target_ptr =
                (unsigned long *)(curr_aot_tb->return_target_ptr_offset + tb->tc.ptr);
                aot_link_tree_insert(tb, 0, 0);
            }
#endif
            aot_tb_register(tb);
        }
        i = j;
    }
#endif
}

void do_recover_segment(aot_segment *p_segment,
        abi_long start, abi_long end) 
{
    struct aot_tb *p_aot_tbs =
        (struct aot_tb *)(aot_buffer + p_segment->segment_tbs_offset);
    int num = p_segment->segment_tbs_num;

    recover_tb_range(0, p_aot_tbs, num, start, end);
}

static bool check_ir1(target_ulong seg_begin, struct aot_tb *p_aot_tbs, int num,
        void *curr_aot_buffer)
{
    uintptr_t pc;
    aot_tb *tb;
    uintptr_t p_ir1;
    assert(curr_aot_buffer);
    for (int i = 0; i < num; i++) {
        tb = &p_aot_tbs[i];
        pc = seg_begin + tb->offset_in_segment;
        p_ir1 = (uintptr_t)curr_aot_buffer + tb->ir1_code_offset;
        if (memcmp((void *)pc, (void *)p_ir1, tb->size)) {
            return false;
        }
    }
    return true;
}

#include<sys/syscall.h>

inline int load_page_4(target_ulong pc, uint32_t cflags, seg_info *info)
{
    int ret = load_page(pc, cflags, info);
    pc += TARGET_PAGE_SIZE;
    load_page(pc, cflags, info);
    pc += TARGET_PAGE_SIZE;
    load_page(pc, cflags, info);
    pc += TARGET_PAGE_SIZE;
    load_page(pc, cflags, info);
    try_aot_link();
    return ret;
}


static int load_all_seg(uint32_t cflags, seg_info *info)
{
    info->seg_flag |= SEG_RUNNING;
    for (target_ulong addr = info->seg_begin; addr < info->seg_end;
            addr += TARGET_PAGE_SIZE) {
        load_page(addr, cflags, info);
    }
    try_aot_link();
    return 1;
}

inline int load_page(target_ulong pc, uint32_t cflags, seg_info *info)
{
    if (pc < info->seg_begin || pc >= info->seg_end) {
        return 0;
    }
    int page_state = page_get_page_state(pc);
    if (page_state >= PAGE_LOADED || ((page_state == PAGE_SMC)
                && (((aot_header *)info->buffer)->aot_file_type == ELF_AOT_FILE))) {
        return 0;
    }
    page_set_page_state(pc, PAGE_LOADED);

    aot_buffer = info->buffer;
    aot_segment *p_segment = (aot_segment *)info->p_segment;
    page_table_info *pt = (page_table_info *)
        (p_segment->page_table_offset + (uintptr_t)aot_buffer);

    int pt_id = (pc - info->seg_begin) / TARGET_PAGE_SIZE;
    assert(pt_id < ((p_segment->details.seg_end
                    - p_segment->details.seg_begin) / TARGET_PAGE_SIZE));

    int tb_num_in_page;
    struct aot_tb *p_aot_tbs;
    if (cflags & CF_PARALLEL) {
        tb_num_in_page = pt[pt_id].tb_num_in_page_parallel;
        p_aot_tbs = 
            (struct aot_tb *)(aot_buffer + pt[pt_id].page_tbs_offset_parallel);
    } else {
        tb_num_in_page = pt[pt_id].tb_num_in_page;
        p_aot_tbs = (struct aot_tb *)(aot_buffer + pt[pt_id].page_tbs_offset);
    }

    if (tb_num_in_page == 0) {
        return 0;
    }

    target_ulong p1 = pc & TARGET_PAGE_MASK;

    if (p_segment->aot_file_type & (PE_AOT_FILE | CACHE_AOT_FILE)) {
        if (!check_ir1(info->seg_begin, p_aot_tbs, tb_num_in_page, aot_buffer)) {
            page_set_page_state(pc, PAGE_SMC);
            return 0;
        }
    } else {
        target_ulong p2 =
            (p_aot_tbs[tb_num_in_page - 1].offset_in_segment + info->seg_begin
             + p_aot_tbs[tb_num_in_page - 1].size) & TARGET_PAGE_MASK;
        while ((p1 != p2) && (page_get_page_state(p2) == PAGE_SMC)) {
#ifdef CONFIG_LATX_TU
            while (tb_num_in_page && !p_aot_tbs[tb_num_in_page - 1].is_first_tb) {
                tb_num_in_page--;
            }
#endif
            tb_num_in_page--;
            if (tb_num_in_page <= 0) {
                return 0;
            }
            p2 = (p_aot_tbs[tb_num_in_page - 1].offset_in_segment + info->seg_begin
                    + p_aot_tbs[tb_num_in_page - 1].size) & TARGET_PAGE_MASK;
        }
    }

    tcg_ctx->tb_cflags = cflags;

    recover_tb_range(p1, p_aot_tbs, tb_num_in_page, info->seg_begin, info->seg_end);
    return 1;
}

int load_aot(target_ulong pc, uint32_t cflags)
{
    if (in_pre_translate) {
        return 0;
    }

    seg_info *info = segment_tree_lookup(pc);
    if (info == NULL || info->buffer == NULL) {
        page_set_page_state(pc, PAGE_NOINFO);
        return 0;
    }

    if (option_aot == 2 && (!(info->seg_flag & SEG_RUNNING)
                || (page_get_page_state(pc) == PAGE_FLUSH))) {
        return load_all_seg(cflags, info);
    } else {
        return load_page_4(pc, cflags, info);
    }

    return 0;
}

#endif
