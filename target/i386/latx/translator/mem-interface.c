/**
 * @file mem-interface.c
 * @author Hanlu Li <heuleehanlu@gmail.com>
 * @brief Add some mem-opnd covert functions
 */
#include "translate.h"
#include "reg-alloc.h"
#include "aot.h"
#include "latx-options.h"
#include "imm-cache.h"

__attribute__((unused))
static void print_debug_info(void)
{
    TranslationBlock *tb __attribute__((unused));
    IR1_INST *pir1;

    pir1 = lsenv->tr_data->curr_ir1_inst;
    target_ulong curr_pc __attribute__((unused))  = ir1_addr(pir1);
    tb = lsenv->tr_data->curr_tb;
}

/**
* @brief add_offset - dest = src + offset
*
* @param dest
* @param src
* @param offset
*/
static void add_offset(IR2_OPND dest, IR2_OPND src, longx offset)
{
    if (si12_overflow(offset)) {
        IR2_OPND temp = ra_alloc_itemp();
        li_guest_addr(temp, offset);
        la_add(dest, src, temp);
        ra_free_temp(temp);
    } else {
        la_addi_d(dest, src, offset);
    }
}

/**
* @brief index_add_base - dest = base + (index * scale)
*
* @param dest
* @param base
* @param index
* @param scale
*/
IR2_INST *(*la_alsl)(IR2_OPND, IR2_OPND, IR2_OPND, int);
static void index_add_base(IR2_OPND dest, IR2_OPND base,
                    IR2_OPND index, int scale)
{
#ifdef TARGET_X86_64
    if (!CODEIS64) {
        la_alsl = &la_alsl_wu;
    } else {
        la_alsl = &la_alsl_d;
    }
#else
    la_alsl = &la_alsl_wu;
#endif
    switch (scale) {
    case 1:
        la_add(dest, base, index);
        return;
    case 2:
        la_alsl(dest, index, base, 0);
        break;
    case 4:
        la_alsl(dest, index, base, 1);
        break;
    case 8:
        la_alsl(dest, index, base, 2);
        break;
    default:
        lsassert(0);
    }
}

/**
* @brief adjust_dest - store the dest based on the values of
*       dest_size and addr_size.
*
* @param dest_op
* @param arg_dest_op
* @param dest_size
* @param addr_size
*
* @return
*/
static IR2_OPND adjust_dest(IR2_OPND dest_op, IR2_OPND *arg_dest_op,
                        int dest_size, int addr_size)
{
    IR2_OPND ret_dest_op;

	if (dest_size == 0 && addr_size == 64) {
        lsassertm(!arg_dest_op, "%s:%d", __func__, __LINE__);
		return dest_op;
	}

    if (arg_dest_op) {
        ret_dest_op = *arg_dest_op;
    } else {
        if (!ir2_opnd_is_itemp(&dest_op)) {
            ret_dest_op = ra_alloc_itemp();
        } else {
            ret_dest_op = dest_op;
        }
    }

    switch (dest_size) {
    case 0:
        lsassertm(!arg_dest_op, "%s:%d", __func__, __LINE__);
        break;
    case 64:
        if (addr_size == 32) {
            la_bstrpick_d(ret_dest_op, dest_op, 31, 0);
        } else {
            if (!IR2_OPND_EQ(ret_dest_op, dest_op)) {
                la_or(ret_dest_op, dest_op, zero_ir2_opnd);
            }
        }
        return ret_dest_op;
    case 32:
        if (addr_size == 16) {
            la_bstrins_d(ret_dest_op, dest_op, 15, 0);
            la_bstrpick_d(ret_dest_op, ret_dest_op, 31, 0);
            return ret_dest_op;
        }
        break;
    case 16:
        la_bstrins_d(ret_dest_op, dest_op, 15, 0);
#ifdef TARGET_X86_64
        return ret_dest_op;
#endif
        la_bstrpick_d(ret_dest_op, ret_dest_op, 31, 0);
        return ret_dest_op;
    default:
        lsassertm(0, "%s:%d", __func__, __LINE__);
    }

    la_bstrpick_d(ret_dest_op, dest_op, 31, 0);
    return ret_dest_op;
}

/**
* @brief convert_mem_helper - The base interface
*
* @param opnd1
* @param arg_dest_op -
*       If the arg_dest_op is not a NULL pointer, load the mem addr into
*       the register specified by arg_dest_op, otherwise a GPR or a itemp
*       my be returned.
* @param dest_size
* @param host_off -
*       If the host_off is not a NULL pointer, we convert the mem with a
*       offset.
*
* @return
*/
__attribute__((unused))
static IR2_OPND convert_mem_helper(IR1_OPND *opnd1, IR2_OPND *arg_dest_op,
                        int dest_size, int *host_off)
{
    longx offset;
    bool has_index, has_base, has_seg;
    bool dest_need_itmp;
    int addr_size;
    int bitmap;
    IR2_OPND index_op, base_op, temp_op, seg_op;
    IR2_OPND dest_op;
    IR1_INST *pir1;
#ifdef CONFIG_LATX_IMM_REG
    IMM_CACHE *imm_cache = lsenv->tr_data->imm_cache;
    imm_cache->itemp_allocated = false;
    bool cache_skip = false;
#endif
    dest_need_itmp = false;
    if (!arg_dest_op) {
        /*
         * TODO OPT: base or base + offset
         */
        dest_need_itmp = true;
    } else {
        if (!ir2_opnd_is_itemp(arg_dest_op) && dest_size < 32) {
            dest_need_itmp = true;
        }
    }

    if (dest_need_itmp) {
#ifndef CONFIG_LATX_IMM_REG
        // not allocated if define CONFIG_LATX_IMM_REG
        dest_op = ra_alloc_itemp();
#endif
    } else {
        dest_op = *arg_dest_op;
    }

#ifdef CONFIG_LATX_IMM_REG
    /* imm cache skip specific ir1 */
    imm_cache_check_ir1_should_skip(&cache_skip);
#endif

    pir1 = lsenv->tr_data->curr_ir1_inst;
    lsassert(pir1 != NULL);
    offset = ir1_opnd_simm(opnd1);
    TranslationBlock *tb __attribute__((unused)) = lsenv->tr_data->curr_tb;

#ifdef TARGET_X86_64
    if (CODEIS64 && ir1_opnd_base_reg(opnd1) == dt_X86_REG_RIP) {
        /*
         * Intel SDM Volume 1 3.5.1
         *    In 64-bit mode, the RIP register becomes the instruction
         *    pointer. This register holds the 64-bit offset of the next
         *    instruction to be executed. 64-bit mode also supports a
         *    technique called RIP-relative addressing. Using this
         *    technique, the effective address is determined by adding a
         *    displacement to the RIP of the next instruction.
         */
        offset += ir1_addr_next(pir1);

#ifdef CONFIG_LATX_IMM_REG
        if (dest_need_itmp) {
            if (!option_imm_reg || !option_imm_rip || cache_skip) {
                dest_op = ra_alloc_itemp();
                imm_cache->itemp_allocated = true;
            } else {
                /* Determine if this RIP address can be optimized */

                IMM_CACHE_RES res =
                    imm_cache_allocate(imm_cache, -100, -1, -1, offset);
                dest_op = ra_alloc_imm_reg(res.itemp_num);
                if (ir2_opnd_is_none(&dest_op)) {
                    // itemp is not available skip directly
                    dest_op = ra_alloc_itemp();
                    imm_log("[alloc none] %d\n", res.itemp_num);
                    free_imm_reg(res.itemp_num);
                    imm_cache->itemp_allocated = true;
                } else {
                    // 1. cached?
                    if (res.cached) {
                        imm_log("[mem convert]pre_cache:%d\n", res.pre_cache);
                        if (res.pre_cache) {
                            imm_log("[mem convert exact]pre_cache:true\n");
                            // initialize
                            target_ulong call_offset __attribute__((unused)) =
                                aot_get_call_offset(res.offset);
                            aot_load_guest_addr(dest_op, res.offset,
                                                LOAD_CALL_TARGET, call_offset);
                            if (!option_imm_precache) {
                                fprintf(stderr,
                                        "pre_cache close but hit branch\n");
                                    lsassert(0);
                            }
                        } else {
                            imm_log(
                                "[mem convert exact]pre_cache:false\n[imm_cache hit] \n");
                        }

                        if (si12_overflow(res.diff)) {
                            imm_cache_print_tb_ir1(tb);
                            imm_cache_print_cache(imm_cache);
                            imm_log("tb:%lx\tres.diff:%lx\toffset:%lx\n",
                                    tb->pc, res.diff, offset);
                            lsassertm(0, "res.diff overflow!\n");
                        }
                        if (host_off) {
                            *host_off = res.diff;
                        } else if (res.diff != 0) {
                            // cache imm offset should be updated at the sametime if no host_off
                            imm_cache_update_by_diff(imm_cache, res.cache_id,
                                                     res.diff);
                            la_addi_d(dest_op, dest_op, res.diff);
                        } 
                        return dest_op;
                    }
                    /**
                     * else
                     * dest_op uses native method, but reg use the one in cache
                     */
                }
            }
        }
#endif
        target_ulong call_offset __attribute__((unused)) =
                aot_get_call_offset(offset);
        aot_load_guest_addr(dest_op, offset, LOAD_CALL_TARGET, call_offset);
        if (host_off) {
            *host_off = 0;
        }
        return dest_op;
    }
#endif

#define BIT_OFFSET      (1)
#define BIT_INDEX       (1 << 1)
#define BIT_BASE        (1 << 2)

    has_seg = ir1_opnd_has_seg(opnd1);
   /*
    * bitmap:
    *           base    index   offset
    *   case1    0        0       1
    *   case2    0        1       0
    *   case3    0        1       1
    *   case4    1        0       0
    *   case5    1        0       1
    *   case6    1        1       0
    *   case7    1        1       1
    */
    has_index = ir1_opnd_has_index(opnd1);
    has_base = ir1_opnd_has_base(opnd1);

#ifdef CONFIG_LATX_IMM_REG
    if (!option_imm_reg || !option_imm_complex) {
        cache_skip = true;
    }
    int base = -1;
    int index = -1;
    int scale = ir1_opnd_scale(opnd1);

    if (has_base) {
        base = ir1_opnd_base_reg_num(opnd1);
    }
    if (has_index) {
        index = ir1_opnd_index_reg_num(opnd1);
    }
    if (scale != 1 && scale != 2 && scale != 4 && scale != 8) {
        scale = -1;
    }
    if (!has_index) {
        scale = -1;
    }

#endif
    if (host_off) {
        if (si12_overflow(offset)) {
            *host_off = 0;
        } else {
#ifdef CONFIG_LATX_IMM_REG
            cache_skip = true;

#endif /* ifdef CONFIG_LATX_IMM_REG */
            *host_off = offset;
            offset = 0;
        }
    }

    bitmap = offset ? BIT_OFFSET : 0;
    if (has_base) {
        bitmap |= BIT_BASE;
        base_op = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd1));
    }
    if (has_index) {
        bitmap |= BIT_INDEX;
        index_op = ra_alloc_gpr(ir1_opnd_index_reg_num(opnd1));
    }
#ifdef CONFIG_LATX_IMM_REG
    // i need base, index, scale offset
    if (!dest_need_itmp) {
        goto imm_cache_exit;
    }
    if (cache_skip) {
        dest_op = ra_alloc_itemp();
        imm_cache->itemp_allocated = true;
        goto imm_cache_exit;
    }

    /**
     * ready to use cache
     * 1. host_off, 100%si12_overflow
     * 2. no host_off, a,si12 b si12_overflow
     */
    IMM_CACHE_RES res =
        imm_cache_allocate(imm_cache, base, index, scale, offset);
    dest_op = ra_alloc_imm_reg(res.itemp_num);
    imm_log("[complex]bitmap:%d\n", bitmap);
    // check if alloc reg conflict with itemp alloc
    if (ir2_opnd_is_none(&dest_op)) {
        imm_log("[alloc none] %d \n", res.itemp_num);
        // itemp is not available skip directly
        dest_op = ra_alloc_itemp();
        free_imm_reg(res.itemp_num);
        imm_cache->itemp_allocated = true;
        goto imm_cache_exit;
    }
    // reg allocate success.
    if (res.cached) {
        imm_log("[mem convert]pre_cache:%d\n", res.pre_cache);
        // cached during scan and need initialize
        if (res.pre_cache) {
            imm_log("[mem convert exact]pre_cache:true\n");

            /* initialize */
            switch (bitmap) {
            /* index * scale */
            /* index * scale + offset */
            case 2:
            case 3:
                if (ir2_opnd_is_itemp(&dest_op)) {
                    li_guest_addr(dest_op, res.offset);
                    index_add_base(dest_op, dest_op, index_op,
                                   ir1_opnd_scale(opnd1));
                } else {
                    temp_op = ra_alloc_itemp();
                    li_guest_addr(temp_op, res.offset);
                    index_add_base(dest_op, temp_op, index_op,
                                   ir1_opnd_scale(opnd1));
                    ra_free_temp(temp_op);
                }
                break;
            /* base */
            /* base + offset */
            case 4:
            case 5:
                add_offset(dest_op, base_op, res.offset);
                break;
            /* base + index * scale */
            /* base + index * scale + offset */
            case 6:
            case 7:
                index_add_base(dest_op, base_op, index_op,
                               ir1_opnd_scale(opnd1));
                add_offset(dest_op, dest_op, res.offset);
                break;
            default:
                break;
            }
        } else {
            imm_log("[mem convert exact]pre_cache:false\n[imm_cache hit] \n");
        }

        if (si12_overflow(res.diff)) {
            imm_cache_print_tb_ir1(tb);
            imm_cache_print_cache(imm_cache);
            imm_log("tb:" TARGET_FMT_lx "\tres.diff:%lx\toffset:%" PRILONGX "\n",
                    tb->pc, res.diff, offset);
            lsassertm(0, "res.diff overflow!\n");
        }
        //?
        if (host_off) {
            *host_off = res.diff;
            offset = 0;
        } else if (res.diff != 0) {
            imm_cache_update_by_diff(imm_cache, res.cache_id, res.diff);
            la_addi_d(dest_op, dest_op, res.diff);
        }

        goto skip;
    }

imm_cache_exit:
#endif

    switch (bitmap) {
    case 0:
        ra_free_temp_auto(dest_op);
        dest_op = zero_ir2_opnd;
        if (!has_seg) {
            return dest_op;
        }
        break;
    /* offset */
    case 1:
        li_guest_addr(dest_op, offset);
        break;
    /* index * scale */
    case 2:
        index_add_base(dest_op, zero_ir2_opnd, index_op,
                    ir1_opnd_scale(opnd1));
        break;
    /* index * scale + offset */
    case 3:
        if (ir2_opnd_is_itemp(&dest_op)) {
            li_guest_addr(dest_op, offset);
            index_add_base(dest_op, dest_op,
                    index_op, ir1_opnd_scale(opnd1));
        } else {
            temp_op = ra_alloc_itemp();
            li_guest_addr(temp_op, offset);
            index_add_base(dest_op, temp_op,
                    index_op, ir1_opnd_scale(opnd1));
            ra_free_temp(temp_op);
        }
        break;
    /* base */
    case 4:
        // use cache but not cached in reg
        // dest_op can be:
        // cached: itemp(alloc imm_reg fail) or imm_reg
        // no cache: itemp
        // when itemp is alloc need free

#ifdef CONFIG_LATX_IMM_REG
        if (!host_off && !imm_cache->itemp_allocated) {
            // cache curret to dest_op
            la_or(dest_op, zero_ir2_opnd, base_op);
        } else {
            /**
             * 1.host off
             * 2.host_off but cache reg unavaialbe, cache allocate an
             * itemp
             */
            imm_cache_free_temp_helper(dest_op);
            dest_op = base_op;
        }
#else
        ra_free_temp_auto(dest_op);
        dest_op = base_op;
#endif
        break;
    /* base + offset */
    case 5:
        add_offset(dest_op, base_op, offset);
        break;
    /* base + index * scale */
    case 6:
        index_add_base(dest_op, base_op, index_op, ir1_opnd_scale(opnd1));
        break;
    /* base + index * scale + offset */
    case 7:
        index_add_base(dest_op, base_op, index_op, ir1_opnd_scale(opnd1));
        add_offset(dest_op, dest_op, offset);
        break;
    default:
        break;
    }
#ifdef CONFIG_LATX_IMM_REG
skip:
#endif
    if (has_seg) {
        seg_op = ra_alloc_itemp();
        int index = ir1_opnd_get_seg_index(opnd1);
        la_load_addrx(seg_op, env_ir2_opnd,
                offsetof(CPUX86State, segs[index].base));
#ifdef CONFIG_LATX_IMM_REG
        imm_cache_free_temp_helper(dest_op);
#else
        ra_free_temp_auto(dest_op);
#endif 
       if (bitmap) {
            la_add(seg_op, seg_op, dest_op);
        }
        dest_op = seg_op;
    } else if (!bitmap) {
        /* no base, no index, no seg, no offset */
#ifndef CONFIG_LATX_TU
        lsassert(ir1_opnd_simm(opnd1));
#else
        /* fprintf(stderr, "maybe have bug in convert_mem_helper()\n"); */
#endif
    }

    addr_size = ir1_addr_size(pir1);
    return adjust_dest(dest_op, arg_dest_op, dest_size, addr_size);
}

void convert_mem_to_specific_gpr(IR1_OPND *opnd, IR2_OPND dest, int dest_size)
{
    convert_mem_helper(opnd, &dest, dest_size, NULL);
}

IR2_OPND convert_mem_no_offset(IR1_OPND *opnd1)
{
    return convert_mem_helper(opnd1, NULL, 0, NULL);
}

IR2_OPND convert_mem(IR1_OPND *opnd1, int *host_off)
{
    return convert_mem_helper(opnd1, NULL, 0, host_off);
}

IR2_OPND mem_imm_add_disp(IR2_OPND mem_op, int *old_imm, int disp)
{
    int new_imm = *old_imm + disp;
    if (si12_overflow(new_imm)) {
        IR2_OPND new_mem_op = ra_alloc_itemp();
        la_addi_d(new_mem_op, mem_op, *old_imm);
        if (ir2_opnd_is_itemp(&mem_op)) {
            ra_free_temp(mem_op);
        }
        *old_imm = disp;
        return new_mem_op;
    }
    *old_imm = new_imm;
    return mem_op;
}
