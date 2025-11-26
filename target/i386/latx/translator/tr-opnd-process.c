#include "common.h"
#include "reg-alloc.h"
#include "ir2.h"
#include "latx-options.h"
#include "lsenv.h"
#include "translate.h"
#include "larchintrin.h"

int have_scq(void)
{
    return (__cpucfg(0x2) & (1 << 30));
}

bool si12_overflow(long si12)
{
    if (si12 >= -2048 && si12 <= 2047) {
        return false;
    }
    return true;
}

/**
@convert an ir1 register operand to an ir2 register operand.
@param the ir1 register operand
@return the ir2 register operand. it can be temp register or a mapped register
*/
IR2_OPND convert_gpr_opnd(IR1_OPND *opnd1, EXTENSION_MODE em)
{
    lsassert(ir1_opnd_is_gpr(opnd1));
    int gpr_num = ir1_opnd_base_reg_num(opnd1);
    int opnd_size = ir1_opnd_size(opnd1);

    IR2_OPND gpr_opnd = ra_alloc_gpr(gpr_num);

    /* 1. 32 bits gpr needs SHIFT operation to handle the extension mode */
    if (opnd_size == 32) {
        /* 1.1. need sign-extended but gpr is not */
        if (em == SIGN_EXTENSION ) {
            IR2_OPND ret_opnd = ra_alloc_itemp_internal();
            la_mov32_sx(ret_opnd, gpr_opnd);
            return ret_opnd;
        }
        /* 1.2. need zero-extended but gpr is not */
        else if (em == ZERO_EXTENSION ) {
            IR2_OPND ret_opnd = ra_alloc_itemp_internal();
            la_mov32_zx(ret_opnd, gpr_opnd);
            return ret_opnd;
        }
        /* 1.3. gpr is what we need, or we need any extension, return gpr */
        /* directly */
        else {
            return gpr_opnd;
        }
    }

    /* 2. 64 bits gpr needs no extension */
    else if (opnd_size == 64) {
        return gpr_opnd;
    }

    /* 3. 16 bits gpr uses AND operation when zero extended */
    else if (opnd_size == 16) {
        IR2_OPND gpr_opnd = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd1));
        /* 3.1. need sign-extended but gpr is usually not */
        if (em == SIGN_EXTENSION) {
            IR2_OPND ret_opnd = ra_alloc_itemp_internal();
            la_ext_w_h(ret_opnd, gpr_opnd);
            return ret_opnd;
        }
        /* 3.2. need zero-extended but gpr is usually not */
        else if (em == ZERO_EXTENSION ) {
            IR2_OPND ret_opnd = ra_alloc_itemp_internal();
            la_bstrpick_d(ret_opnd, gpr_opnd, 15, 0);

            return ret_opnd;
        }
        /* 3.3. need any extension, return gpr directly */
        else {
            return gpr_opnd;
        }
    }

    /* 4. 8 bits gpr high */
    else if (ir1_opnd_is_8h(opnd1)) {
        IR2_OPND gpr_opnd = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd1));
        IR2_OPND ret_opnd = ra_alloc_itemp_internal();
        /* 4.1. we need sign extension */
        if (em == SIGN_EXTENSION) {
            la_srli_w(ret_opnd, gpr_opnd, 8);
            la_ext_w_b(ret_opnd, ret_opnd);
            return ret_opnd;
        } else {
            la_bstrpick_d(ret_opnd, gpr_opnd, 15, 8);
            return ret_opnd;
        }
    }

    /* 5. 8 bits gpr low */
    else {
        IR2_OPND gpr_opnd = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd1));
        /* 5.1. need sign-extended but gpr is usually not */
        if (em == SIGN_EXTENSION) {
            IR2_OPND ret_opnd = ra_alloc_itemp_internal();
            la_ext_w_b(ret_opnd, gpr_opnd);
            return ret_opnd;
        }
        /* 5.2. need zero-extended but gpr is usually not */
        else if (em == ZERO_EXTENSION ) {
            IR2_OPND ret_opnd = ra_alloc_itemp_internal();
            la_andi(ret_opnd, gpr_opnd, 0xff);
            return ret_opnd;
        }
        /* 5.3. need any extension, return gpr directly */
        else {
            return gpr_opnd;
        }
    }
}

#ifdef CONFIG_LATX_AOT
void load_ireg_from_host_addr(IR2_OPND opnd2, uint64 value)
{
    lsassert(ir2_opnd_is_ireg(&opnd2));

    int32 hi32 = value >> 32;
    int32 lo32 = value & 0xffffffff;

    la_lu12i_w(opnd2, lo32 >> 12);
    la_ori(opnd2, opnd2, lo32 & 0xfff);
    la_lu32i_d(opnd2, hi32 & 0xfffff);
    return;
}

void load_ireg_from_guest_addr(IR2_OPND opnd2, uint64 value)
{
    lsassert(ir2_opnd_is_ireg(&opnd2));

    int32 hi32 = value >> 32;
    int32 lo32 = value & 0xffffffff;
    la_lu12i_w(opnd2, lo32 >> 12);
    la_ori(opnd2, opnd2, lo32 & 0xfff);
    if (hi32 || (lo32 & 0x80000000)) {
        la_lu32i_d(opnd2, hi32 & 0xfffff);
    }
    return;
}
#endif

/**
@load an ir1 immediate operand to a specific ir2 register operand
@param the ir2 operand
@param the ir1 immediate operand
@return the ir2 register operand
*/
static void load_ireg_from_ir1_imm(IR2_OPND opnd2, IR1_OPND *opnd1,
                                   EXTENSION_MODE em)
{
    lsassert(ir2_opnd_is_ireg(&opnd2));
    lsassert(ir1_opnd_is_imm(opnd1));
    if (em == ZERO_EXTENSION) {
        ulongx value = ir1_opnd_uimm(opnd1);
        li_d(opnd2, value);
    } else {
        longx value = ir1_opnd_simm(opnd1);
        li_d(opnd2, value);
    }
}

/**
@load an ir1 memory operand to a specific ir2 register operand. internal temp
registers may be used.
@param the ir1 memory operand
@return the ir2 register operand
*/
void load_ireg_from_ir1_mem(IR2_OPND opnd2, IR1_OPND *opnd1,
                                   EXTENSION_MODE em, bool is_xmm_hi)
{
    lsassert(ir2_opnd_is_ireg(&opnd2));
    lsassert(ir1_opnd_is_mem(opnd1));

    int mem_imm;
    IR2_OPND mem_opnd = convert_mem(opnd1, &mem_imm);
    if (is_xmm_hi) {
        mem_opnd = mem_imm_add_disp(mem_opnd, &mem_imm, 8);
    }

    gen_test_page_flag(mem_opnd, mem_imm, PAGE_READ);

    if ((mem_imm & 0xffff0000) == 0xdead0000) {
        IR2_OPND base, index;
        ir2_opnd_build(&base, IR2_OPND_GPR, (mem_imm & 0xff00) >> 8);
        ir2_opnd_build(&index, IR2_OPND_GPR, (mem_imm & 0xff));
        if (ir1_opnd_size(opnd1) == 32) {
            if (em == ZERO_EXTENSION) {
                la_ldx_wu(opnd2, base, index);
            } else {
                la_ldx_w(opnd2, base, index);
            }
        } else if (ir1_opnd_size(opnd1) == 64 || ir1_opnd_size(opnd1) == 128) {
            la_ldx_d(opnd2, base, index);
        } else if (ir1_opnd_size(opnd1) == 8) {
            if (em == ZERO_EXTENSION)
    	        la_ldx_bu(opnd2, base, index);
    	    else
    	        la_ldx_b(opnd2, base, index);
        } else if (ir1_opnd_size(opnd1) == 16) {
            if (em == ZERO_EXTENSION)
    	        la_ldx_hu(opnd2, base, index);
    	    else
    	        la_ldx_h(opnd2, base, index);
        } else {
            lsassert(0);
        }
    } else {
        if (ir1_opnd_size(opnd1) == 32) {
    	    if (em == ZERO_EXTENSION) {
    	        la_ld_wu(opnd2, mem_opnd, mem_imm);
            } else {
                la_ld_w(opnd2, mem_opnd, mem_imm);
            }
        } else if (ir1_opnd_size(opnd1) == 64 || ir1_opnd_size(opnd1) == 128) {
            la_ld_d(opnd2, mem_opnd, mem_imm);
        } else if (ir1_opnd_size(opnd1) == 8) {
            if (em == ZERO_EXTENSION)
    	        la_ld_bu(opnd2, mem_opnd, mem_imm);
    	    else
    	        la_ld_b(opnd2, mem_opnd, mem_imm);
        } else if (ir1_opnd_size(opnd1) == 16) {
            if (em == ZERO_EXTENSION)
    	        la_ld_hu(opnd2, mem_opnd, mem_imm);
    	    else
    	        la_ld_h(opnd2, mem_opnd, mem_imm);
    	} else {
    	    lsassert(0);
    	}
    }
    if (ir2_opnd_is_itemp(&mem_opnd) && !ir2_opnd_is_imm_reg(&mem_opnd)) {
        ra_free_temp(mem_opnd);
    }
}

IR2_OPND load_ireg_from_ir2_mem(IR2_OPND mem_opnd, int mem_imm, int mem_size,
                                   EXTENSION_MODE em, bool is_xmm_hi)
{
    IR2_OPND opnd2 = ra_alloc_itemp_internal();
#ifndef TARGET_X86_64
    la_mov32_zx(mem_opnd, mem_opnd);
#else
    if (!CODEIS64) {
        la_mov32_zx(mem_opnd, mem_opnd);
    }
#endif
    if (mem_size == 32) {
        if (em == ZERO_EXTENSION) {
            la_ld_wu(opnd2, mem_opnd, mem_imm);
        } else {
            la_ld_w(opnd2, mem_opnd, mem_imm);
        }
    } else if (mem_size == 64) {
        la_ld_d(opnd2, mem_opnd, mem_imm);
    } else if (mem_size == 8) {
        if (em == ZERO_EXTENSION) {
            la_ld_bu(opnd2, mem_opnd, mem_imm);
        } else {
            la_ld_b(opnd2, mem_opnd, mem_imm);
        }
    } else if (mem_size == 16) {
        if (em == ZERO_EXTENSION) {
            la_ld_hu(opnd2, mem_opnd, mem_imm);
        } else {
            la_ld_h(opnd2, mem_opnd, mem_imm);
        }
    } else {
        lsassert(0);
    }
    return opnd2;
}

/**
@load an ir1 register operand to a specific ir2 register operand.
@param the ir1 register operand
@return the ir2 register operand. it can be temp register or a mapped register
*/
static void load_ireg_from_ir1_gpr(IR2_OPND opnd2, IR1_OPND *opnd1,
                                   EXTENSION_MODE em)
{
    lsassert(ir2_opnd_is_ireg(&opnd2));
    lsassert(ir1_opnd_is_gpr(opnd1));
    lsassert(em == UNKNOWN_EXTENSION || em == SIGN_EXTENSION ||
             em == ZERO_EXTENSION);

    int gpr_num = ir1_opnd_base_reg_num(opnd1);
    IR2_OPND gpr_opnd = ra_alloc_gpr(gpr_num);

    /* 1. 32 bits gpr needs SHIFT operation to handle the extension mode */
    if (ir1_opnd_size(opnd1) == 32) {
        /* 1.1. need sign-extended but gpr is not */
        if (em == SIGN_EXTENSION ) {
            la_mov32_sx(opnd2, gpr_opnd);
            return;
        }
        /* 1.2. need zero-extended but gpr is not */
        else if (em == ZERO_EXTENSION ) {
            la_mov32_zx(opnd2, gpr_opnd);
            return;
        }
        /* 1.3. gpr is what we need, or we need any extension, return gpr */
        else {
            la_mov64(opnd2, gpr_opnd);
            return;
        }
    }

    /* 2. 64 bits gpr needs no extension */
    else if (ir1_opnd_size(opnd1) == 64) {
        la_mov64(opnd2, gpr_opnd);
        return;
    }

    /* 3. 16 bits gpr uses AND operation when zero extended */
    else if (ir1_opnd_size(opnd1) == 16) {
        /* 3.1. need sign-extended but gpr is usually not */
        if (em == SIGN_EXTENSION) {
            la_ext_w_h(opnd2, gpr_opnd);
            return;
        }
        /* 3.1. need zero-extended but gpr is usually not */
        else {
            la_bstrpick_d(opnd2, gpr_opnd, 15, 0);
            return;
        }
    }

    /* 4. 8 bits gpr high */
    else if (ir1_opnd_is_8h(opnd1)) {
        /* 4.1. we need sign extension */
        if (em == SIGN_EXTENSION) {
            la_srli_w(opnd2, gpr_opnd, 8);
            la_ext_w_b(opnd2, opnd2);
        }
        /* 4.2. we need zero extension */
        else {
            la_bstrpick_d(opnd2, gpr_opnd, 15, 8);
        }
        return;
    }

    /* 5. 8 bits gpr low */
    else {
        /* 5.1. need sign-extended but gpr is usually not */
        if (em == SIGN_EXTENSION) {
            la_ext_w_b(opnd2, gpr_opnd);
            return;
        }
        /* 5.2. zero extended */
        else {
            la_andi(opnd2, gpr_opnd, 0xff);
            return;
        }
    }
}

static void load_ireg_from_ir1_mmx(IR2_OPND opnd2, IR1_OPND *opnd1,
                                   EXTENSION_MODE em)
{
    IR2_OPND mmx_opnd = ra_alloc_mmx(ir1_opnd_base_reg_num(opnd1));

    if (em == SIGN_EXTENSION) {
        la_movfr2gr_s(opnd2, mmx_opnd);
    } else if (em == ZERO_EXTENSION) {
        la_movfr2gr_d(opnd2, mmx_opnd);
        la_mov32_zx(opnd2, opnd2);
    } else {
        la_movfr2gr_d(opnd2, mmx_opnd);
    }
}

/**
@load the value of an ir1 operand to an ir2 register operand
@param the ir1 operand (any type is ok)
@param the extension mode
@return the ir2 register operand. it can be temp register or a mapped register
*/
IR2_OPND load_ireg_from_ir1(IR1_OPND *opnd1, EXTENSION_MODE em, bool is_xmm_hi)
{
    lsassert(em == SIGN_EXTENSION || em == ZERO_EXTENSION ||
             em == UNKNOWN_EXTENSION);
    IR2_OPND ret_opnd;

    switch (ir1_opnd_type(opnd1)) {
    case dt_X86_OP_IMM: {
        if (ir1_opnd_simm(opnd1) == 0)
            ret_opnd = zero_ir2_opnd;
        else {
            ret_opnd = ra_alloc_itemp_internal();
            load_ireg_from_ir1_imm(ret_opnd, opnd1, em);
        }
        break;
    }

    case dt_X86_OP_REG:  {
        if(ir1_opnd_is_gpr(opnd1)){
            ret_opnd = convert_gpr_opnd(opnd1, em);
            /* Now it is OK for UNKNOWN_EXTENSION */
            break;
        }
        else if(ir1_opnd_is_mmx(opnd1)){
            ret_opnd = ra_alloc_itemp_internal();
            load_ireg_from_ir1_mmx(ret_opnd, opnd1, em);
            break;
        }
        else if(ir1_opnd_is_ymm(opnd1)){
            lsassert(0);
        }
        else if(ir1_opnd_is_seg(opnd1)){
            ret_opnd = ra_alloc_itemp_internal();
            int seg_num = ir1_opnd_base_reg_num(opnd1);
            la_ld_wu(ret_opnd, env_ir2_opnd,
                              lsenv_offset_of_seg_selector(lsenv, seg_num));
            break;
        }
        lsassert(0);
        break;
    }

    case dt_X86_OP_MEM: {
        ret_opnd = ra_alloc_itemp_internal();
        load_ireg_from_ir1_mem(ret_opnd, opnd1, em, is_xmm_hi);
        break;
    }
    default:
        lsassert(0);
        break;
    }

    return ret_opnd;
}

/**
@load the value of an ir1 operand to a specific ir2 register operand
@param the ir2 operand
@param the ir1 operand (any type is ok)
@param the extension mode
*/
void load_ireg_from_ir1_2(IR2_OPND opnd2, IR1_OPND *opnd1, EXTENSION_MODE em,
                          bool is_xmm_hi)
{
    lsassert(ir2_opnd_is_ireg(&opnd2));
    switch (ir1_opnd_type(opnd1)) {
    case dt_X86_OP_IMM:
        load_ireg_from_ir1_imm(opnd2, opnd1, em);
        break;
    case dt_X86_OP_REG:  {
        if(ir1_opnd_is_gpr(opnd1)){
            load_ireg_from_ir1_gpr(opnd2, opnd1, em);
            break;
        }
        else if(ir1_opnd_is_mmx(opnd1)){
            load_ireg_from_ir1_mmx(opnd2, opnd1, em);
            break;
        }
        else if(ir1_opnd_is_ymm(opnd1)){
            lsassert(0);
        }
        else if(ir1_opnd_is_seg(opnd1)){
            int seg_num = ir1_opnd_base_reg_num(opnd1);
            la_ld_wu(opnd2, env_ir2_opnd,
                              lsenv_offset_of_seg_selector(lsenv, seg_num));
            break;
        }
        lsassert(0);
        break;
    }
    case dt_X86_OP_MEM:  {
        load_ireg_from_ir1_mem(opnd2, opnd1, em, is_xmm_hi);
        break;
    }
    default:
        lsassert(0);
    }

    return;
}

void load_ireg_from_cf_opnd(IR2_OPND *opnd2)
{
    lsassert(ir2_opnd_is_ireg(opnd2));
    la_x86mfflag(*opnd2, 0x1);
    return;
}

static void store_ireg_to_ir1_gpr(IR2_OPND opnd2, IR1_OPND *opnd1)
{
    lsassert(ir2_opnd_is_ireg(&opnd2));
    lsassert(ir1_opnd_is_gpr(opnd1) && ir2_opnd_is_ireg(&opnd2));
    int gpr_num = ir1_opnd_base_reg_num(opnd1);
    IR2_OPND gpr_opnd = ra_alloc_gpr(gpr_num);

    /* 1. 32 bits gpr needs SHIFT operation to handle the extension mode */
    if (ir1_opnd_size(opnd1) == 32) {
        /* In x64, if opnd_size is 32 bits, high 32 bits will clean */
        la_mov32_zx(gpr_opnd, opnd2);
    } else if (ir1_opnd_size(opnd1) == 64) {
        if (!ir2_opnd_cmp(&opnd2, &gpr_opnd)) {
            la_mov64(gpr_opnd, opnd2);
        }
    } else if (ir1_opnd_size(opnd1) == 16) {
        /* when src and dest has same zero extend and is same reg */
        bool need_mov = (ir2_opnd_base_reg_num(&opnd2) !=
                        ir2_opnd_base_reg_num(&gpr_opnd));
        if (need_mov) {
            la_bstrins_d(gpr_opnd, opnd2, 15, 0);
        }
        return;
    } else if (ir1_opnd_is_8h(opnd1)) {
        la_bstrins_d(gpr_opnd, opnd2, 15, 8);
        return;
    } else {
        /* when src and dest has same zero extend and is same reg */
        bool need_mov = (ir2_opnd_base_reg_num(&opnd2) !=
                        ir2_opnd_base_reg_num(&gpr_opnd));
        if (need_mov) {
            la_bstrins_d(gpr_opnd, opnd2, 7, 0);
        }
        return;
    }
}

static void store_ireg_to_ir1_mem(IR2_OPND value_opnd, IR1_OPND *opnd1,
                                  bool is_xmm_hi)
{
    lsassert(ir2_opnd_is_ireg(&value_opnd));
    int mem_imm;
    IR2_OPND mem_opnd = convert_mem(opnd1, &mem_imm);
    if (is_xmm_hi) {
        mem_opnd = mem_imm_add_disp(mem_opnd, &mem_imm, 8);
    }

    gen_test_page_flag(mem_opnd, mem_imm, PAGE_WRITE | PAGE_WRITE_ORG);

    if((mem_imm&0xffff0000)==0xdead0000) {
        IR2_OPND base,index;
        ir2_opnd_build(&base, IR2_OPND_GPR,  (mem_imm&0xff00)>>8);
        ir2_opnd_build(&index, IR2_OPND_GPR, (mem_imm&0xff));

    	if (ir1_opnd_size(opnd1) == 32) {
    	    la_stx_w(value_opnd, base, index);
    	} else if (ir1_opnd_size(opnd1) == 64 || ir1_opnd_size(opnd1) == 128) {
    	    la_stx_d(value_opnd, base, index);
    	} else if (ir1_opnd_size(opnd1) == 8) {
    	    la_stx_b(value_opnd, base, index);
    	} else if (ir1_opnd_size(opnd1) == 16) {
    	    la_stx_h(value_opnd, base, index);
    	} else {
    	    lsassert(0);
    	}
	} else {
    	if (ir1_opnd_size(opnd1) == 32) {
    	    la_st_w(value_opnd, mem_opnd, mem_imm);
    	} else if (ir1_opnd_size(opnd1) == 64 || ir1_opnd_size(opnd1) == 128) {
    	    la_st_d(value_opnd, mem_opnd, mem_imm);
    	} else if (ir1_opnd_size(opnd1) == 8) {
    	    la_st_b(value_opnd, mem_opnd, mem_imm);
    	} else if (ir1_opnd_size(opnd1) == 16) {
    	    la_st_h(value_opnd, mem_opnd, mem_imm);
    	} else {
    	    lsassert(0);
    	}
	}
    if (ir2_opnd_is_itemp(&mem_opnd) && !ir2_opnd_is_imm_reg(&mem_opnd)) {
        ra_free_temp(mem_opnd);
    }
}

void store_ireg_to_ir1_seg(IR2_OPND seg_value_opnd, IR1_OPND *opnd1)
{
    lsassert(ir2_opnd_is_ireg(&seg_value_opnd));
    /* 1. set selector */
    int seg_num = ir1_opnd_base_reg_num(opnd1);
    /*lsassertm(((seg_num == 0) || (seg_num == 4) || (seg_num == 5)),
        "Modify segment selector %d is not supported (cs:1, ss:2, ds:3)\n", seg_num);*/
    la_st_w(seg_value_opnd, env_ir2_opnd,
                      lsenv_offset_of_seg_selector(lsenv, seg_num));

    /* 2. update seg cache : read data in GDT and store into seg cache */
    /* TI = 0 : GDT, TI = 1 : LDT */
    IR2_OPND label_ldt = ra_alloc_label();
    IR2_OPND label_base_end = ra_alloc_label();
    IR2_OPND is_ldt = ra_alloc_itemp_internal(); /* [51:48] [15: 0] limit */
    IR2_OPND dt_opnd = ra_alloc_itemp_internal();
#ifdef TARGET_X86_64
    if (seg_num == cs_index) {
        IR2_OPND ir2_tmp = ra_alloc_itemp();
        IR2_OPND label_x64 = ra_alloc_label();
        IR2_OPND label_csend = ra_alloc_label();//if (cs == 0x23) isx86 ; else is x64;
        li_d(ir2_tmp, 0x23);
        la_bne(seg_value_opnd, ir2_tmp,label_x64);
        la_st_w(zero_ir2_opnd, env_ir2_opnd,
            offsetof(CPUX86State, sys.codemode));
        la_b(label_csend);
        la_label(label_x64);
        li_d(ir2_tmp, 1);
        la_st_w(ir2_tmp, env_ir2_opnd,
            offsetof(CPUX86State, sys.codemode));
        la_label(label_csend);
        ra_free_temp(ir2_tmp);
    }
#endif
    la_andi(is_ldt, seg_value_opnd, 0x4);
    la_bne(is_ldt, zero_ir2_opnd, label_ldt);
    ra_free_temp(is_ldt);
    /* 2.1 get gdt base */
    la_ld_d(dt_opnd, env_ir2_opnd,
                      lsenv_offset_of_gdt_base(lsenv));
    la_b(label_base_end);
    /* 2.1 get ldt base */
    la_label(label_ldt);
    la_ld_d(dt_opnd, env_ir2_opnd,
                      lsenv_offset_of_ldt_base(lsenv));
    la_label(label_base_end);

    /* 2.2 get entry offset of gdt and add it on gdt-base */
    IR2_OPND offset_gdt = ra_alloc_itemp_internal();
    /* seg [15: 3] is offset, offset * 8 */
    la_bstrpick_d(offset_gdt, seg_value_opnd, 15, 3);
    la_slli_w(offset_gdt, offset_gdt, 3);
    la_add_d(dt_opnd, dt_opnd, offset_gdt);
    ra_free_temp(offset_gdt);

    /* 2.4 read segment entry */
    IR2_OPND gdt_entry = ra_alloc_itemp_internal();
#ifndef TARGET_X86_64
    la_bstrpick_d(dt_opnd, dt_opnd, 31, 0);
#endif
    la_ld_d(gdt_entry, dt_opnd, 0);
    ra_free_temp(dt_opnd);

    /*
     * [51:48] [15: 0] limit
     * [63:56] [39:16] base
     * [55:40] - [51:48] flags
     */
    IR2_OPND seg_limit = ra_alloc_itemp_internal();
    IR2_OPND seg_base = ra_alloc_itemp_internal();
    IR2_OPND seg_flags = ra_alloc_itemp_internal();

    /* 2.5.1 get new base */
    IR2_OPND tmp = ra_alloc_itemp_internal();
    /* [39:16] */
    la_bstrpick_d(seg_base, gdt_entry, 39, 16);
    /* [63:56] */
    la_bstrpick_d(tmp, gdt_entry, 63, 56);
    la_bstrins_d(seg_base, tmp, 31, 24);
    /* 2.5.2 get new limit */

    /*
     * la_bstrpick_d(seg_limit, gdt_entry, 15, 0);
     * la_bstrpick_d(tmp, gdt_entry, 51, 48);
     * la_bstrins_d(seg_limit, tmp, 19, 16);
     *
     * set seg_limit 0xffff_ffff, because limit isn't used in LA
     */
    li_wu(seg_limit, -1);

    /* 2.5.3 get flags in GDT */
    la_bstrpick_d(seg_flags, gdt_entry, 55, 40);
    /* TypeField in GDT represents accessed, which should be set 1 */
    la_ori(seg_flags, seg_flags, 1);
    la_slli_w(seg_flags, seg_flags, 8);
    ra_free_temp(tmp);
    ra_free_temp(gdt_entry);

    /* 2.6 write into seg cache */
    la_st_d(seg_base, env_ir2_opnd,
                      lsenv_offset_of_seg_base(lsenv, seg_num));
    la_st_w(seg_limit, env_ir2_opnd,
                      lsenv_offset_of_seg_limit(lsenv, seg_num));
    la_st_w(seg_flags, env_ir2_opnd,
                      lsenv_offset_of_seg_flags(lsenv, seg_num));
    ra_free_temp(seg_limit);
    ra_free_temp(seg_base);
    ra_free_temp(seg_flags);
}

void store_ireg_to_ir1(IR2_OPND opnd2, IR1_OPND *opnd1, bool is_xmm_hi)
{
    lsassert(ir2_opnd_is_ireg(&opnd2));
    if (!ir2_opnd_is_ireg(&opnd2)) {
        lsassertm(0, "error");
    }

    switch (ir1_opnd_type(opnd1)) {
    case dt_X86_OP_REG:  {
        if(ir1_opnd_is_gpr(opnd1)){
            store_ireg_to_ir1_gpr(opnd2, opnd1);
            return;
        }
        else if(ir1_opnd_is_mmx(opnd1)){
            IR2_OPND mmx_opnd = ra_alloc_mmx(ir1_opnd_base_reg_num(opnd1));
            la_movgr2fr_d(mmx_opnd, opnd2);
            return;
        }
        else if(ir1_opnd_is_ymm(opnd1)){
            lsassert(0);
        }
        else if (ir1_opnd_is_seg(opnd1)){
            store_ireg_to_ir1_seg(opnd2, opnd1);
            return;
        }
#ifdef CONFIG_LATX_TU
        TranslationBlock *tb =
            (TranslationBlock *)lsenv->tr_data->curr_tb;
        tb->s_data->tu_tb_mode = BAD_TB;
        return;
#endif
        lsassert(0);
        return;
    }

    case dt_X86_OP_MEM:
        store_ireg_to_ir1_mem(opnd2, opnd1, is_xmm_hi);
        return;
    default:
        lsassert(0);
        return;
    }
}

void store_ireg_to_ir2_mem(IR2_OPND value_opnd, IR2_OPND mem_opnd,
                                  int mem_imm, int mem_size, bool is_xmm_hi)
{
    lsassert(ir2_opnd_is_ireg(&value_opnd));

#ifndef TARGET_X86_64
    la_bstrpick_d(mem_opnd, mem_opnd, 31, 0);
#else
    if (!CODEIS64) {
        la_bstrpick_d(mem_opnd, mem_opnd, 31, 0);
    }
#endif

    if (mem_size == 32) {
        la_st_w(value_opnd, mem_opnd, mem_imm);
    } else if (mem_size == 64) {
        la_st_d(value_opnd, mem_opnd, mem_imm);
    } else if (mem_size == 8) {
        la_st_b(value_opnd, mem_opnd, mem_imm);
    } else if (mem_size == 16) {
        la_st_h(value_opnd, mem_opnd, mem_imm);
    } else {
        lsassert(0);
    }

    return;
}
/**
    load 80bit float from memory and convert to 64bit
*/
void load_64_bit_freg_from_ir1_80_bit_mem(IR2_OPND opnd2,
                                                 IR2_OPND mem_opnd, int mem_imm)
{
    /* load 80bit float from memory and convert it to 64bit float */
    IR2_OPND ir2_sign_exp = ra_alloc_ftemp();
    IR2_OPND ir2_fraction = ra_alloc_ftemp();

    lsassert(mem_imm + 8 <= 2047);

#ifndef TARGET_X86_64
    la_bstrpick_d(mem_opnd, mem_opnd, 31, 0);
#else
    if (!CODEIS64) {
        la_bstrpick_d(mem_opnd, mem_opnd, 31, 0);
    }
#endif

    la_fld_s(ir2_sign_exp, mem_opnd, mem_imm + 8);
    la_fld_d(ir2_fraction, mem_opnd, mem_imm);

    IR2_OPND itemp_reg = ra_alloc_itemp();
    IR2_OPND itemp1_reg = ra_alloc_itemp();
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND label_no_excp1 = ra_alloc_label();
    IR2_OPND label_no_excp2 = ra_alloc_label();

    /* Temporarily mask V to make the conversion don't trigger SIGFPE */
    la_movfcsr2gr(itemp_reg, fcsr_ir2_opnd);
    la_bstrpick_d(itemp1_reg, itemp_reg, 4, 4);
    la_beq(itemp1_reg, zero_ir2_opnd, label_no_excp1);
    la_xori(itemp_reg, itemp_reg, 0x10);
    la_movgr2fcsr(fcsr_ir2_opnd, itemp_reg);
    la_label(label_no_excp1);

    la_fcvt_d_ld(opnd2, ir2_fraction, ir2_sign_exp);
    ra_free_temp(ir2_sign_exp);
    ra_free_temp(ir2_fraction);

    la_movfcsr2gr(itemp_reg, fcsr_ir2_opnd);
    /* unmask V if necessary */
    la_beq(itemp1_reg, zero_ir2_opnd, label_no_excp2);
    la_xori(itemp_reg, itemp_reg, 0x10);
    la_movgr2fcsr(fcsr_ir2_opnd, itemp_reg);
    la_label(label_no_excp2);

    la_bstrpick_d(itemp_reg, itemp_reg, 28, 28);
    la_beq(itemp_reg, zero_ir2_opnd, label_exit);
    /* Identify SNAN and change opnd2 to SNAN
     * exp==0x7fff && bit[62]==0 && bit[61:0]!=0
     *
     * FIXME: assume SNAN->QNAN when V, should check other cases
     */
    la_movfr2gr_d(itemp_reg, opnd2);
    la_bstrins_d(itemp_reg, zero_ir2_opnd, 51, 51);
    la_movgr2fr_d(opnd2, itemp_reg);
    ra_free_temp(itemp_reg);
    ra_free_temp(itemp1_reg);

    la_label(label_exit);
}

/**
@load an ir1 memory operand to a specific ir2 register operand. internal temp
registers may be used.
@param the ir1 memory operand
@return the ir2 register operand
*/
static void load_freg_from_ir1_mem(IR2_OPND opnd2, IR1_OPND *opnd1,
                                   bool is_xmm_hi, uint32 options)
{
    IR2_OPND mem_opnd;
    bool is_convert = (options & IS_CONVERT);
    bool is_integer = (options & IS_INTEGER) >> 3;

    int mem_imm;
    mem_opnd = convert_mem(opnd1, &mem_imm);
    if (is_xmm_hi) {
        mem_opnd = mem_imm_add_disp(mem_opnd, &mem_imm, 8);
    }

    gen_test_page_flag(mem_opnd, mem_imm, PAGE_READ);

    if (ir1_opnd_size(opnd1) == 32) {
        la_fld_s(opnd2, mem_opnd, mem_imm);
        if (is_convert)
            la_fcvt_d_s(opnd2, opnd2);
    } else if (ir1_opnd_size(opnd1) == 64 || ir1_opnd_size(opnd1) == 128) {
        IR2_OPND ftemp = ra_alloc_ftemp_internal();
        la_fld_d(opnd2, mem_opnd, mem_imm);
        //64->80->64 to handle the implicit SNAN->QNAN of fld
        if (!is_integer) {
            la_fcvt_ld_d(ftemp, opnd2);
            la_fcvt_ud_d(opnd2, opnd2);
            la_fcvt_d_ld(opnd2, ftemp, opnd2);
        }
        ra_free_temp(ftemp);
    } else if (ir1_opnd_size(opnd1) == 16) {
        IR2_OPND itemp = ra_alloc_itemp_internal();
        la_ld_h(itemp, mem_opnd, mem_imm);
        la_movgr2fr_d(opnd2, itemp);
        ra_free_temp(itemp);

        lsassertm(!is_convert, "convert 16-bit floating point?\n");
    } else if (ir1_opnd_size(opnd1) == 8) {
        IR2_OPND itemp = ra_alloc_itemp_internal();
        la_ld_b(itemp, mem_opnd, mem_imm);
        la_movgr2fr_d(opnd2, itemp);
        ra_free_temp(itemp);

        lsassertm(!is_convert, "convert 8-bit floating point?\n");
    } else {
        load_64_bit_freg_from_ir1_80_bit_mem(opnd2, mem_opnd, mem_imm);
    }
}

/* Clear lsenv->mode_trans_mmx_fpu to transfer to MMX mode */
void transfer_to_mmx_mode(void)
{
    la_st_d(zero_ir2_opnd, env_ir2_opnd, lsenv_offset_of_mode_fpu(lsenv));
}

/* Set lsenv->mode_trans_mmx_fpu to transfer to FPU mode */
void transfer_to_fpu_mode(void)
{
    IR2_OPND temp = ra_alloc_itemp();
    li_wu(temp, 0x1ULL);
    la_st_d(temp, env_ir2_opnd, lsenv_offset_of_mode_fpu(lsenv));
    ra_free_temp(temp);
}

/**
@load an ir1 register operand to a specific ir2 register operand.
@param the ir1 register operand
@return the ir2 register operand. it can be temp register or a mapped register
*/

static void load_freg_from_ir1_fpr(IR2_OPND opnd2, IR1_OPND *opnd1)
{
    lsassert(ir2_opnd_is_freg(&opnd2));
    IR2_OPND value_opnd = ra_alloc_st(ir1_opnd_base_reg_num(opnd1));
    if (!ir2_opnd_cmp(&opnd2, &value_opnd))
        la_fmov_d(opnd2, value_opnd);
}

static void load_freg_from_ir1_mmx(IR2_OPND opnd2, IR1_OPND *opnd1)
{
    lsassert(ir2_opnd_is_freg(&opnd2));
    lsassert(ir1_opnd_is_mmx(opnd1));
    IR2_OPND mmx_opnd = ra_alloc_mmx(ir1_opnd_base_reg_num(opnd1));
    if (!ir2_opnd_cmp(&opnd2, &mmx_opnd))
        la_fmov_d(opnd2, mmx_opnd);
}

IR2_OPND load_freg_from_ir1_1(IR1_OPND *opnd1, bool is_xmm_hi, uint32_t options)
{
    /*
     * NOTE: Previous arg support is_covert only, to minimum modification,
     * here is a around to support new options.
     * bit0: false
     * bit1: true (is_covert)
     * bit3: (1 << 3) to avoid float covertion
     */

    switch (ir1_opnd_type(opnd1)) {
    case dt_X86_OP_REG: {
        if (ir1_opnd_is_fpr(opnd1)){
            return ra_alloc_st(ir1_opnd_base_reg_num(opnd1));

        }
        else if(ir1_opnd_is_mmx(opnd1)){
            return ra_alloc_mmx(ir1_opnd_base_reg_num(opnd1));

        }
        else if(ir1_opnd_is_ymm(opnd1)){
            lsassert(0);
        }
        lsassert(0);
        break;
    }
    case dt_X86_OP_MEM: {
        IR2_OPND ret_opnd = ra_alloc_ftemp_internal();
        load_freg_from_ir1_mem(ret_opnd, opnd1, is_xmm_hi, options);
        return ret_opnd;
    }
    default:
        lsassert(0);
    }
    abort();
}

void load_freg_from_ir1_2(IR2_OPND opnd2, IR1_OPND *opnd1, uint32_t options)
{
    lsassert(ir2_opnd_is_freg(&opnd2));
    bool is_xmm_hi = (options & IS_XMM_HI) >> 2;
    assert(is_xmm_hi == 0);

    switch (ir1_opnd_type(opnd1)) {
    case dt_X86_OP_REG: {
        if (ir1_opnd_is_fpr(opnd1)){
            load_freg_from_ir1_fpr(opnd2, opnd1);
            return;
        }
        else if(ir1_opnd_is_mmx(opnd1)){
            load_freg_from_ir1_mmx(opnd2, opnd1);
            return;
        }
        else if(ir1_opnd_is_ymm(opnd1)){
            lsassert(0);
        }
        lsassertm(0,"REG:%s\n", ir1_reg_name(opnd1->reg));
        return;
    }

    case dt_X86_OP_MEM: {
        load_freg_from_ir1_mem(opnd2, opnd1, is_xmm_hi,
                               options & (IS_CONVERT | IS_INTEGER));
        break;
    }
    default:
        lsassert(0);
        break;
    }

    return;
}

void load_singles_from_ir1_pack(IR2_OPND single0, IR2_OPND single1,
                                IR1_OPND *opnd1, bool is_xmm_hi)
{
    switch (ir1_opnd_type(opnd1)) {
    case dt_X86_OP_MEM:  {
        /* retrieve the 1th single */
        IR2_OPND itemp0 = ra_alloc_itemp();
        load_ireg_from_ir1_mem(itemp0, opnd1, ZERO_EXTENSION, is_xmm_hi);
        IR2_OPND itemp1 = ra_alloc_itemp();
        la_srli_d(itemp1, itemp0, 32);
        la_movgr2fr_d(single1, itemp1);
        ra_free_temp(itemp1);
        /* retrive the 0th single */
        la_slli_d(itemp0, itemp0, 32);
        la_srli_d(itemp0, itemp0, 32);
        la_movgr2fr_d(single0, itemp0);
        ra_free_temp(itemp0);
        break;
    }
    default:
        lsassert(0);
        break;
    }
    return;
}

void store_singles_to_ir2_pack(IR2_OPND single0, IR2_OPND single1,
                               IR2_OPND pack)
{
    lsassert(ir2_opnd_is_freg(&single0) && ir2_opnd_is_freg(&single1) &&
             ir2_opnd_is_freg(&pack));
    IR2_OPND itemp1 = ra_alloc_itemp();
    la_movfr2gr_d(itemp1, single1);
    la_slli_d(itemp1, itemp1, 32);
    IR2_OPND itemp0 = ra_alloc_itemp();
    la_movfr2gr_d(itemp0, single0);
    la_or(itemp1, itemp1, itemp0);
    la_movgr2fr_d(pack, itemp1);
    ra_free_temp(itemp1);
    ra_free_temp(itemp0);
}

void store_64_bit_freg_to_ir1_80_bit_mem(IR2_OPND opnd2,
        IR2_OPND mem_opnd, int mem_imm)
{
    IR2_OPND ir2_sign_exp = ra_alloc_ftemp();
    IR2_OPND ir2_fraction = ra_alloc_ftemp();
    IR2_OPND itemp = ra_alloc_itemp();
    IR2_OPND itemp1 = ra_alloc_itemp();
    IR2_OPND label_ok = ra_alloc_label();
    IR2_OPND label_no_excp1 = ra_alloc_label();
    IR2_OPND label_no_excp2 = ra_alloc_label();

#ifndef TARGET_X86_64
    la_bstrpick_d(mem_opnd, mem_opnd, 31, 0);
#else
    if (!CODEIS64) {
        la_bstrpick_d(mem_opnd, mem_opnd, 31, 0);
    }
#endif

    /* Temporarily mask V to make the conversion don't trigger SIGFPE */
    la_movfcsr2gr(itemp, fcsr_ir2_opnd);
    la_bstrpick_d(itemp1, itemp, 4, 4);
    la_beq(itemp1, zero_ir2_opnd, label_no_excp1);
    la_xori(itemp, itemp, 0x10);
    la_movgr2fcsr(fcsr_ir2_opnd, itemp);
    la_label(label_no_excp1);

    lsassert(mem_imm + 8 <= 2047);
    la_fcvt_ld_d(ir2_fraction, opnd2);
    la_fcvt_ud_d(ir2_sign_exp, opnd2);
    la_movfcsr2gr(itemp, fcsr_ir2_opnd);
    /* unmask V if necessary */
    la_beq(itemp1, zero_ir2_opnd, label_no_excp2);
    la_xori(itemp, itemp, 0x10);
    la_movgr2fcsr(fcsr_ir2_opnd, itemp);
    la_label(label_no_excp2);

    la_bstrpick_d(itemp, itemp, 28, 28);
    la_movfr2gr_s(itemp1, ir2_sign_exp);
    la_st_h(itemp1, mem_opnd, mem_imm + 8);
    la_movfr2gr_d(itemp1, ir2_fraction);
    la_beq(itemp, zero_ir2_opnd, label_ok);
    ra_free_temp(ir2_sign_exp);
    ra_free_temp(ir2_fraction);

    //Identify SNAN and write snan to opnd2
    la_bstrins_d(itemp1, zero_ir2_opnd, 62, 62);

    la_label(label_ok);
    la_st_d(itemp1, mem_opnd, mem_imm);

    ra_free_temp(itemp);
    ra_free_temp(itemp1);
}

static void store_freg_to_ir1_mem(IR2_OPND opnd2, IR1_OPND *opnd1,
                                  bool is_xmm_hi, bool is_convert)
{
    int mem_imm;
    IR2_OPND mem_opnd = convert_mem(opnd1, &mem_imm);
    if (is_xmm_hi) {
        mem_opnd = mem_imm_add_disp(mem_opnd, &mem_imm, 8);
    }

    gen_test_page_flag(mem_opnd, mem_imm, PAGE_WRITE | PAGE_WRITE_ORG);

    if (ir1_opnd_size(opnd1) == 32) {
        IR2_OPND ftemp = ra_alloc_ftemp_internal();
        if (is_convert) {
            la_fcvt_s_d(ftemp, opnd2);
            la_fst_s(ftemp, mem_opnd, mem_imm);
        } else
            la_fst_s(opnd2, mem_opnd, mem_imm);
        ra_free_temp(ftemp);
    } else if (ir1_opnd_size(opnd1) == 64 || ir1_opnd_size(opnd1) == 128) {
        la_fst_d(opnd2, mem_opnd, mem_imm);
    } else {
        store_64_bit_freg_to_ir1_80_bit_mem(opnd2, mem_opnd, mem_imm);
    }
}

static void store_freg_to_ir1_gpr(IR2_OPND opnd2, IR1_OPND *opnd1)
{
    lsassert(ir2_opnd_is_freg(&opnd2));
    IR2_OPND target_opnd = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd1));
    if (ir1_opnd_size(opnd1) == 32) {
        la_movfr2gr_s(target_opnd, opnd2);
    } else {
        la_movfr2gr_d(target_opnd, opnd2);
    }
}

static void store_freg_to_ir1_fpr(IR2_OPND opnd2, IR1_OPND *opnd1)
{
    lsassert(ir2_opnd_is_freg(&opnd2));
    IR2_OPND target_opnd = ra_alloc_st(ir1_opnd_base_reg_num(opnd1));
    if (!ir2_opnd_cmp(&opnd2, &target_opnd))
        la_fmov_d(target_opnd, opnd2);
}

/**
@store an ir1 mmx operand
@param the ir2 fp register operand
@param the ir1 mmx operand
*/
static void store_freg_to_ir1_mmx(IR2_OPND opnd2, IR1_OPND *opnd1)
{
    lsassert(ir2_opnd_is_ireg(&opnd2));
    lsassert(ir1_opnd_is_mmx(opnd1));

    // IR2_OPND mmx_opnd = ra_alloc_mmx(opnd1->_reg_num);
    IR2_OPND mmx_opnd = ra_alloc_mmx(ir1_opnd_base_reg_num(opnd1));


    if (ir2_opnd_cmp(&opnd2, &mmx_opnd))
        la_fmov_d(mmx_opnd, opnd2);
}

void store_freg_to_ir1(IR2_OPND opnd2, IR1_OPND *opnd1, bool is_xmm_hi,
                       bool is_convert)
{
    lsassert(ir2_opnd_is_freg(&opnd2));

    switch (ir1_opnd_type(opnd1)) {

    case dt_X86_OP_REG:  {
        if(ir1_opnd_is_fpr(opnd1)){
            store_freg_to_ir1_fpr(opnd2, opnd1);
            return;
        }
        else if(ir1_opnd_is_mmx(opnd1)){
            store_freg_to_ir1_mmx(opnd2, opnd1);
            return;
        }
        else if(ir1_opnd_is_ymm(opnd1)){
            lsassert(0);
        }
        if(ir1_opnd_is_gpr(opnd1)){
            store_freg_to_ir1_gpr(opnd2, opnd1);
            return;
        }
        lsassertm(0, "REG:%s\n", ir1_reg_name(opnd1->reg));
        return;
    }
    case dt_X86_OP_MEM: {
        store_freg_to_ir1_mem(opnd2, opnd1, is_xmm_hi, is_convert);
        return;
    }
    default:
        lsassert(0);
        return;
    }
}

/* save old fcsr in fcsr_opnd temporary register  for reload , then set fcsr
 * according to x86 MXCSR register */

IR2_OPND set_fpu_fcsr_rounding_field_by_x86(void)
{
    if (option_set_rounding_opt) return zero_ir2_opnd;
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    if (!close_latx_parallel && !(cpu->tcg_cflags & CF_PARALLEL)) {
        return zero_ir2_opnd;
    }

    IR2_OPND fcsr_opnd = ra_alloc_itemp_internal();
    la_movfcsr2gr(fcsr_opnd, fcsr3_ir2_opnd);

    /* set fcsr according to x86 MXCSR register */
    IR2_OPND temp_mxcsr = ra_alloc_itemp_internal();
    la_ld_wu(temp_mxcsr, env_ir2_opnd,
        lsenv_offset_of_mxcsr(lsenv));
    la_bstrpick_w(temp_mxcsr, temp_mxcsr, 14, 13);
    IR2_OPND temp_int = ra_alloc_itemp_internal();
    la_andi(temp_int, temp_mxcsr, 0x1);
    IR2_OPND label1 = ra_alloc_label();
    la_beq(temp_int, zero_ir2_opnd, label1);
    la_xori(temp_mxcsr, temp_mxcsr, 0x2);
    la_label(label1);
    la_bstrins_w(temp_mxcsr, temp_mxcsr, 9, 8);
    la_movgr2fcsr(fcsr3_ir2_opnd, temp_mxcsr);

    ra_free_temp(temp_mxcsr);
    ra_free_temp(temp_int);
    return fcsr_opnd;
}

void set_fpu_rounding_mode(IR2_OPND rm)
{
    if (option_set_rounding_opt) return;
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    if (close_latx_parallel) {
        la_movgr2fcsr(fcsr3_ir2_opnd, rm);
    } else if (cpu->tcg_cflags & CF_PARALLEL) {
        la_movgr2fcsr(fcsr3_ir2_opnd, rm);
    }
}

void load_freg128_from_ir1_mem(IR2_OPND opnd2, IR1_OPND *opnd1){
    int little_disp;

    lsassert(ir1_opnd_is_mem(opnd1));
    lsassert(ir2_opnd_is_freg(&opnd2));

    IR2_OPND mem_opnd = convert_mem(opnd1, &little_disp);
    gen_test_page_flag(mem_opnd, little_disp, PAGE_READ);
    la_vld(opnd2, mem_opnd, little_disp);
    return;
}

void store_freg128_to_ir1_mem(IR2_OPND opnd2, IR1_OPND *opnd1){
    int little_disp;

    lsassert(ir1_opnd_is_mem(opnd1));
    lsassert(ir2_opnd_is_freg(&opnd2));

    IR2_OPND mem_opnd = convert_mem(opnd1, &little_disp);
    gen_test_page_flag(mem_opnd, little_disp, PAGE_WRITE | PAGE_WRITE_ORG);
    la_vst(opnd2, mem_opnd, little_disp);
    return;
}

IR2_OPND load_freg128_from_ir1(IR1_OPND *opnd1){
    lsassert(ir1_opnd_is_xmm(opnd1) || ir1_opnd_is_mem(opnd1));

    if (ir1_opnd_is_xmm(opnd1)) {
        return ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
    } else if (ir1_opnd_is_mem(opnd1)) {
        IR2_OPND ret_opnd = ra_alloc_ftemp();
        if (ir1_opnd_size(opnd1) == 128) {
            load_freg128_from_ir1_mem(ret_opnd, opnd1);
        } else {
            load_freg_from_ir1_mem(ret_opnd, opnd1, 0, IS_INTEGER);
        }
        return ret_opnd;
    }
    g_assert_not_reached();
}

void clear_h32(IR2_OPND *opnd)
{
     if (!CODEIS64) {
         la_mov32_zx(*opnd, *opnd);
     }
}

/* load/store 256 bit mem or register */
void store_freg256_to_ir1_mem(IR2_OPND opnd2, IR1_OPND * opnd1) {
    int little_disp;
    lsassert(ir1_opnd_is_mem(opnd1));
    lsassert(ir2_opnd_is_freg( & opnd2));
    IR2_OPND mem_opnd = convert_mem(opnd1, & little_disp);
    gen_test_page_flag(mem_opnd, little_disp, PAGE_WRITE | PAGE_WRITE_ORG);
    la_xvst(opnd2, mem_opnd, little_disp);
}

void load_freg256_from_ir1_mem(IR2_OPND opnd2, IR1_OPND * opnd1) {
    int little_disp;
    lsassert(ir1_opnd_is_mem(opnd1));
    lsassert(ir2_opnd_is_freg( & opnd2));

    IR2_OPND mem_opnd = convert_mem(opnd1, & little_disp);
    gen_test_page_flag(mem_opnd, little_disp, PAGE_READ);
    la_xvld(opnd2, mem_opnd, little_disp);
}

IR2_OPND load_freg256_from_ir1(IR1_OPND * opnd1) {
    if (ir1_opnd_is_ymm(opnd1)) {
        return ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
    } else if (ir1_opnd_is_mem(opnd1) || ir1_opnd_is_xmm(opnd1)) {
        IR2_OPND ret_opnd;
        if (ir1_opnd_size(opnd1) == 256) {
            ret_opnd = ra_alloc_ftemp();
            load_freg256_from_ir1_mem(ret_opnd, opnd1);
        } else {
            ret_opnd = load_freg128_from_ir1(opnd1);
        }
        return ret_opnd;
    }
    g_assert_not_reached();
}

void set_high128_xreg_to_zero(IR2_OPND opnd2) {
    lsassert(ir2_opnd_is_freg( & opnd2));
    if (option_enable_lasx) {
        la_xvinsgr2vr_d(opnd2, zero_ir2_opnd, 2);
        la_xvinsgr2vr_d(opnd2, zero_ir2_opnd, 3);
    }
}
