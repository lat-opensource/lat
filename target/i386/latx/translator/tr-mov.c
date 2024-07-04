#include "common.h"
#include "reg-alloc.h"
#include "lsenv.h"
#include "flag-lbt.h"
#include "latx-options.h"
#include "translate.h"
#include "hbr.h"
bool translate_pop(IR1_INST *pir1)
{
    IR2_OPND esp_opnd = ra_alloc_gpr(esp_index);
    IR1_OPND mem_ir1_opnd;
    int has_esp = ir1_opnd_is_gpr_used(ir1_get_opnd(pir1, 0), esp_index);
    int pop_size = ir1_opnd_size(ir1_get_opnd(pir1, 0));
    int esp_increment = pop_size >> 3;

    if (pop_size == 16 && ir1_opnd_is_seg(ir1_get_opnd(pir1, 0))) {
#ifdef TARGET_X86_64
        if (CODEIS64) {
            esp_increment = 64 >> 3;
        } else {
            esp_increment = 32 >> 3;
        }
#else
        esp_increment = 32 >> 3;
#endif
    }

    /*  2. pop value from mem */
#ifdef TARGET_X86_64
    if (CODEIS64) {
        if (pop_size == 64) {/* 64 bits */
            ir1_opnd_build_mem(&mem_ir1_opnd, 64, dt_X86_REG_RSP, 0);
            if (ir1_opnd_is_gpr(ir1_get_opnd(pir1, 0))) {
                /* when dest is gpr, load into gpr directly */
                IR2_OPND dest_opnd
                        = load_ireg_from_ir1(ir1_get_opnd(pir1, 0), UNKNOWN_EXTENSION, false);
                load_ireg_from_ir1_2(dest_opnd, &mem_ir1_opnd, UNKNOWN_EXTENSION, false);
                if (!has_esp) {
                    la_addi_addrx(esp_opnd, esp_opnd,
                                            esp_increment);
                }
            } else {
                /* load value */
                IR2_OPND value_opnd =
                        load_ireg_from_ir1(&mem_ir1_opnd, SIGN_EXTENSION, false);
                IR1_OPND *dest_ir1_opnd = ir1_get_opnd(pir1, 0);
                if (has_esp) {
                    dest_ir1_opnd->mem.disp += esp_increment;
                }
                store_ireg_to_ir1(value_opnd, dest_ir1_opnd, false);
                la_addi_addrx(esp_opnd, esp_opnd,
                                        esp_increment);
            }
        } else if (pop_size == 16) {
                ir1_opnd_build_mem(&mem_ir1_opnd, 16, dt_X86_REG_ESP, 0);
                /* load value */
                IR2_OPND value_opnd =
                    load_ireg_from_ir1(&mem_ir1_opnd, ZERO_EXTENSION, false);
                IR1_OPND *dest_ir1_opnd = ir1_get_opnd(pir1, 0);
                if (has_esp) {
                    dest_ir1_opnd->mem.disp += esp_increment;
                }
                store_ireg_to_ir1(value_opnd, dest_ir1_opnd, false);
                la_addi_addrx(esp_opnd, esp_opnd,
                                        esp_increment);
        } else {
            ir1_opnd_build_mem(&mem_ir1_opnd, 32, dt_X86_REG_ESP, 0);
            if (ir1_opnd_is_gpr(ir1_get_opnd(pir1, 0))) {
                IR2_OPND dest_opnd =
                    load_ireg_from_ir1(ir1_get_opnd(pir1, 0), UNKNOWN_EXTENSION, false);
                EXTENSION_MODE dest_em = SIGN_EXTENSION;
                // if (ir2_opnd_default_em(&dest_opnd) != SIGN_EXTENSION)
                //     dest_em = ZERO_EXTENSION;

                load_ireg_from_ir1_2(dest_opnd, &mem_ir1_opnd, dest_em, false);
                if (!has_esp) {
                    la_addi_addrx(esp_opnd, esp_opnd,
                                                                esp_increment);
                }
            } else {
                /* dest is mem, as normal */
                IR2_OPND value_opnd =
                    load_ireg_from_ir1(&mem_ir1_opnd, SIGN_EXTENSION, false);
                IR1_OPND *dest_ir1_opnd = ir1_get_opnd(pir1, 0);
                if (has_esp) {
                    dest_ir1_opnd->mem.disp += esp_increment;
                }
                store_ireg_to_ir1(value_opnd, dest_ir1_opnd, false);
                la_addi_addrx(esp_opnd, esp_opnd,
                                        esp_increment);
            }
        }
     } else {
        if (pop_size == 16) {
            ir1_opnd_build_mem(&mem_ir1_opnd, 16, dt_X86_REG_ESP, 0);
            /* load value */
            IR2_OPND value_opnd =
                load_ireg_from_ir1(&mem_ir1_opnd, ZERO_EXTENSION, false);
            IR1_OPND *dest_ir1_opnd = ir1_get_opnd(pir1, 0);
            if (has_esp) {
                dest_ir1_opnd->mem.disp += esp_increment;
            }
            store_ireg_to_ir1(value_opnd, dest_ir1_opnd, false);
            la_addi_addrx(esp_opnd, esp_opnd,
                                    esp_increment);
        } else {
            ir1_opnd_build_mem(&mem_ir1_opnd, 32, dt_X86_REG_ESP, 0);
            if (ir1_opnd_is_gpr(ir1_get_opnd(pir1, 0))) {
                IR2_OPND dest_opnd =
                    load_ireg_from_ir1(ir1_get_opnd(pir1, 0), UNKNOWN_EXTENSION, false);
                EXTENSION_MODE dest_em = SIGN_EXTENSION;
                // if (ir2_opnd_default_em(&dest_opnd) != SIGN_EXTENSION)
                //     dest_em = ZERO_EXTENSION;
        
                load_ireg_from_ir1_2(dest_opnd, &mem_ir1_opnd, dest_em, false);
                if (!has_esp) {
                    la_addi_addrx(esp_opnd, esp_opnd,
                                                                esp_increment);
                }
            } else {
                /* dest is mem, as normal */
                IR2_OPND value_opnd =
                    load_ireg_from_ir1(&mem_ir1_opnd, SIGN_EXTENSION, false);
                IR1_OPND *dest_ir1_opnd = ir1_get_opnd(pir1, 0);
                if (has_esp) {
                    dest_ir1_opnd->mem.disp += esp_increment;
                }
                store_ireg_to_ir1(value_opnd, dest_ir1_opnd, false);
                la_addi_addrx(esp_opnd, esp_opnd,
                                        esp_increment);
            }
        }
    }
#else
    if (pop_size == 16) {
        ir1_opnd_build_mem(&mem_ir1_opnd, 16, dt_X86_REG_ESP, 0);
        /* load value */
        IR2_OPND value_opnd =
            load_ireg_from_ir1(&mem_ir1_opnd, ZERO_EXTENSION, false);
        IR1_OPND *dest_ir1_opnd = ir1_get_opnd(pir1, 0);
        if (has_esp) {
            dest_ir1_opnd->mem.disp += esp_increment;
        }
        store_ireg_to_ir1(value_opnd, dest_ir1_opnd, false);
        la_addi_addrx(esp_opnd, esp_opnd,
                                esp_increment);
    } else {
        ir1_opnd_build_mem(&mem_ir1_opnd, 32, dt_X86_REG_ESP, 0);
        if (ir1_opnd_is_gpr(ir1_get_opnd(pir1, 0))) {
            IR2_OPND dest_opnd =
                load_ireg_from_ir1(ir1_get_opnd(pir1, 0), UNKNOWN_EXTENSION, false);
            EXTENSION_MODE dest_em = SIGN_EXTENSION;
            // if (ir2_opnd_default_em(&dest_opnd) != SIGN_EXTENSION)
            //     dest_em = ZERO_EXTENSION;

            load_ireg_from_ir1_2(dest_opnd, &mem_ir1_opnd, dest_em, false);
            if (!has_esp) {
                la_addi_addrx(esp_opnd, esp_opnd,
                                                            esp_increment);
            }
        } else {
            /* dest is mem, as normal */
            IR2_OPND value_opnd =
                load_ireg_from_ir1(&mem_ir1_opnd, SIGN_EXTENSION, false);
            IR1_OPND *dest_ir1_opnd = ir1_get_opnd(pir1, 0);
            if (has_esp) {
                dest_ir1_opnd->mem.disp += esp_increment;
            }
            store_ireg_to_ir1(value_opnd, dest_ir1_opnd, false);
            la_addi_addrx(esp_opnd, esp_opnd,
                                    esp_increment);
        }
    }
#endif
    return true;
}

bool translate_push(IR1_INST *pir1)
{
    IR2_OPND esp_opnd = ra_alloc_gpr(esp_index);
    int push_size = ir1_opnd_size(ir1_get_opnd(pir1, 0));
    int esp_decrement = push_size >> 3;

    if (push_size == 16 && ir1_opnd_is_seg(ir1_get_opnd(pir1, 0))) {
#ifdef TARGET_X86_64
        if (CODEIS64) {
            esp_decrement = 64 >> 3;
        } else {
            esp_decrement = 32 >> 3;
        }
#else
        esp_decrement = 32 >> 3;
#endif
    }

    /* 2. push value onto stack */
    IR1_OPND mem_ir1_opnd;

#ifndef TARGET_X86_64
    ir1_opnd_build_mem(&mem_ir1_opnd, esp_decrement << 3,
        dt_X86_REG_ESP, -esp_decrement);
#else
    if (CODEIS64) {
        ir1_opnd_build_mem(&mem_ir1_opnd, esp_decrement << 3,
            dt_X86_REG_RSP, -esp_decrement);
    } else {
        ir1_opnd_build_mem(&mem_ir1_opnd, esp_decrement << 3,
            dt_X86_REG_ESP, -esp_decrement);
    }
#endif
    IR2_OPND value_opnd =
        load_ireg_from_ir1(ir1_get_opnd(pir1, 0), UNKNOWN_EXTENSION, false);
    store_ireg_to_ir1(value_opnd, &mem_ir1_opnd, false);

    la_addi_addrx(esp_opnd, esp_opnd,
                    -esp_decrement);
    return true;
}

static void translate_mov_imm_to_gpr(IR1_OPND *opnd0, IR1_OPND *opnd1)
{
    ulongx imm = ir1_opnd_uimm(opnd1);

    if (imm == 0) {
        store_ireg_to_ir1(zero_ir2_opnd, opnd0, false);
    } else {
        int opnd0_gpr_num = ir1_opnd_base_reg_num(opnd0);
        IR2_OPND dest  = ra_alloc_gpr(opnd0_gpr_num);
        IR2_OPND temp = ra_alloc_itemp();
        uint64_t useful_imm = imm;

        switch (ir1_opnd_size(opnd0)) {
        case 8:
            if (ir1_opnd_is_8l(opnd0)) {
                useful_imm = imm & 0xff;
                la_ori(temp, zero_ir2_opnd, useful_imm);
                la_bstrins_d(dest, temp, 7, 0);
            } else {
                /* 8h */
                useful_imm = imm & 0xff;
                la_ori(temp, zero_ir2_opnd, useful_imm);
                la_bstrins_d(dest, temp, 15, 8);
            }
            break;
        case 16:
            li_wu(temp, (uint16_t)useful_imm);
            la_bstrins_d(dest, temp, 15, 0);
            break;
        case 32:
            li_wu(dest, useful_imm);
            break;
        case 64:
            li_d(dest, useful_imm);
            break;
        default:
            assert(0);
            break;
        }
        ra_free_temp(temp);
    }
}

static void translate_mov_from_gpr(IR1_OPND *opnd0, IR1_OPND *opnd1)
{
    int opnd1_gpr_num = ir1_opnd_base_reg_num(opnd1);
    IR2_OPND src  = ra_alloc_gpr(opnd1_gpr_num);

    if (ir1_opnd_is_gpr(opnd0)) {
        /* MOV r8/16/32/64, r8/16/32/64 */
        int opnd0_gpr_num = ir1_opnd_base_reg_num(opnd0);
        IR2_OPND dest  = ra_alloc_gpr(opnd0_gpr_num);
        switch (ir1_opnd_size(opnd1)) {
        case 8:
            if (ir1_opnd_is_8l(opnd1)) {
                if (ir1_opnd_is_8l(opnd0)) {
                    la_bstrins_d(dest, src, 7, 0);
                } else {
                    la_bstrins_d(dest, src, 15, 8);
                }
            } else {
                IR2_OPND temp = ra_alloc_itemp();
                la_bstrpick_d(temp, src, 15, 8);
                if (ir1_opnd_is_8l(opnd0)) {
                    la_bstrins_d(dest, temp, 7, 0);
                } else {
                    la_bstrins_d(dest, temp, 15, 8);
                }
                ra_free_temp(temp);
            }
            break;
        case 16:
            la_bstrins_d(dest, src, 15, 0);
            break;
        case 32:
            la_mov32_zx(dest, src);
            break;
        case 64:
            la_mov64(dest, src);
            break;
        default:
            assert(0);
            break;
        }
    } else {
        /* MOV m8/16/32/64, r8/16/32/64 */
        if (ir1_opnd_is_8h(opnd1)) {
            IR2_OPND temp = ra_alloc_itemp();
            la_bstrpick_d(temp, src, 15, 8);
            store_ireg_to_ir1(temp, opnd0, false);
            ra_free_temp(temp);
        } else {
            store_ireg_to_ir1(src, opnd0, false);
        }
    }
}

static void translate_mov_from_mem(IR1_OPND *opnd0, IR1_OPND *opnd1)
{
    int opnd0_gpr_num = ir1_opnd_base_reg_num(opnd0);
    IR2_OPND dest  = ra_alloc_gpr(opnd0_gpr_num);

    IR2_OPND temp = ra_alloc_itemp();

    switch (ir1_opnd_size(opnd0)) {
    case 8:
        load_ireg_from_ir1_mem(temp, opnd1, UNKNOWN_EXTENSION, false);
        if (ir1_opnd_is_8l(opnd0)) {
            la_bstrins_d(dest, temp, 7, 0);
        } else {
            la_bstrins_d(dest, temp, 15, 8);
        }
        break;
    case 16:
        load_ireg_from_ir1_mem(temp, opnd1, UNKNOWN_EXTENSION, false);
        if (ir1_opnd_is_gpr(opnd0)) {
            la_bstrins_d(dest, temp, 15, 0);
        } else {
            store_ireg_to_ir1(temp, opnd0, false);
        }
        break;
    case 32:
        load_ireg_from_ir1_mem(dest, opnd1, ZERO_EXTENSION, false);
        break;
    case 64:
        load_ireg_from_ir1_mem(dest, opnd1, UNKNOWN_EXTENSION, false);
        break;
    default:
        assert(0);
        break;
    }
    ra_free_temp(temp);
}

bool translate_mov(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    switch (ir1_opnd_type(opnd1)) {
    case dt_X86_OP_IMM:
        if (ir1_opnd_is_gpr(opnd0)) {
            /* MOV r8/16/32/64, imm8/16/32/64 */
            translate_mov_imm_to_gpr(opnd0, opnd1);
            return true;
        }
        break;
    case dt_X86_OP_REG:
        if (ir1_opnd_is_gpr(opnd1)) {
            /*
             * MOV r8/16/32/64, r8/16/32/64
             * MOV m8/16/32/64, r8/16/32/64
             */
            translate_mov_from_gpr(opnd0, opnd1);
            return true;
        }
        break;
    case dt_X86_OP_MEM:
        /* MOV r8/16/32/64, m8/16/32/64 */
        translate_mov_from_mem(opnd0, opnd1);
        return true;
    default:
        break;
    }

    /*
     * MOV m8/16/32, imm8/16/32
     * MOV r/m16, Sreg**
     * MOV Sreg, r/m16**
     * MOV AL/AX/EAX, moffs8/16/32*
     * MOV moffs8/16/32*, AL/AX/EAX
     */
    IR2_OPND src_opnd = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    store_ireg_to_ir1(src_opnd, opnd0, false);
    return true;
}

bool translate_movzx(IR1_INST *pir1)
{
    IR1_OPND *source_ir1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *dest_ir1 = ir1_get_opnd(pir1, 0);
    lsassert(ir1_opnd_is_gpr(dest_ir1));

    int dest_base = ir1_opnd_base_reg_num(dest_ir1);
    IR2_OPND dest_opnd = ra_alloc_gpr(dest_base);
    /*
     * MOVZX in x86 have only three situation.
     *  --> MOVZX r16, r/m8, MOVZX r32, r/m8, MOVZX r32, r/m16
     * X64, we have other two cases
     *  --> MOVZX r64, r/m8*, MOVZX r64, r/m16
     */

    /* notice that x64 will have `RIP`, but it is not a GPR */
    if (ir1_opnd_is_gpr(source_ir1)) {
        /*
         * When the source is a GPR
         * MOVZX r16, r8
         * MOVZX r32, r8, MOVZX r32, r16
         * MOVZX r64, r8, MOVZX r64, r16
         */
        int source_base = ir1_opnd_base_reg_num(source_ir1);
        IR2_OPND src_opnd = ra_alloc_gpr(source_base);

        if (ir1_opnd_is_8h(source_ir1)) {
            /* MOVZX r16/32/64, r8h */
            if (ir1_opnd_is_16bit(dest_ir1)) {
                /*
                 * MOVZX r16, r8h
                 *
                 * temp <- ZERO(r8h[15:8])
                 * r16  <- temp[15:0]
                 */
                IR2_OPND tmp_opnd = ra_alloc_itemp();
                la_bstrpick_w(tmp_opnd, src_opnd, 15, 8);
                la_bstrins_d(dest_opnd, tmp_opnd, 15, 0);
                ra_free_temp(tmp_opnd);
            } else {
                /* MOVZX r32/64, r8h */
                /* 32 bits and 64 bits, all will be clean */
                la_bstrpick_w(dest_opnd, src_opnd, 15, 8);
            }
        } else {
            /* source is r8l/16 bits */
            if (ir1_opnd_is_16bit(dest_ir1)) {
                /*
                 * MOVZX r16, r8l
                 * r16 = r8 (same reg)
                 *   - set r16[15:8] = 0
                 * not same
                 *   - set r16[7:0] = r8l & r16[15:8] = 0
                 */
                if (source_base == dest_base) {
                    la_bstrins_d(dest_opnd, zero_ir2_opnd, 15, 8);
                } else {
                   IR2_OPND tmp_opnd = ra_alloc_itemp();
                   la_bstrpick_w(tmp_opnd, src_opnd, 7, 0);
                   la_bstrins_d(dest_opnd, tmp_opnd, 15, 0);
                   ra_free_temp(tmp_opnd);

                }
            } else {
                /* MOVZX r32/64, r8l/16 */
                /* 32 bits and 64 bits, all will be clean */
                int opnd_size = ir1_opnd_size(source_ir1);
#ifdef CONFIG_LATX_DEBUG
                if (opnd_size == 8) {
                    lsassert(ir1_opnd_is_8l(source_ir1) || ir1_opnd_is_8h(source_ir1));
                } else if (opnd_size == 16) {
                    lsassert(ir1_opnd_is_16bit(source_ir1));
                } else {
                    lsassert(0);
                }
#endif
                la_bstrpick_w(dest_opnd, src_opnd, opnd_size - 1, 0);
            }
        }
        return true;
    }

#ifndef TARGET_X86_64
    /* default segment is DS*/
    if (ir1_opnd_type(source_ir1) == dt_X86_OP_MEM &&
        source_ir1->mem.segment == dt_X86_REG_INVALID &&
        source_ir1->mem.base == dt_X86_REG_INVALID){
        source_ir1->mem.segment = dt_X86_REG_DS;
    }
#else
    if (!CODEIS64) {
        if (ir1_opnd_type(source_ir1) == dt_X86_OP_MEM &&
            source_ir1->mem.segment == dt_X86_REG_INVALID &&
            source_ir1->mem.base == dt_X86_REG_INVALID){
            source_ir1->mem.segment = dt_X86_REG_DS;
        }
    }
#endif
    /*
     * source is mem but:
     *   dest is r32/64
     *
     * MOVZX r32, m8, MOVZX r32, m16
     * MOVZX r64, m8, MOVZX r64, m16
     */
    if (ir1_opnd_size(dest_ir1) >= 32) {
        load_ireg_from_ir1_mem(dest_opnd, source_ir1, ZERO_EXTENSION, false);
    } else {
        /* MOVZX r16, m8 */
        IR2_OPND source_opnd =
            load_ireg_from_ir1(source_ir1, ZERO_EXTENSION, false);

        store_ireg_to_ir1(source_opnd, dest_ir1, false);
    }

    return true;
}

bool translate_movsx(IR1_INST *pir1)
{
    IR2_OPND source_opnd =
        load_ireg_from_ir1(ir1_get_opnd(pir1, 0) + 1, SIGN_EXTENSION, false);

    store_ireg_to_ir1(source_opnd, ir1_get_opnd(pir1, 0), false);
    return true;
}

bool translate_movsxd(IR1_INST *pir1)
{
    lsassert(ir1_opnd_is_gpr(ir1_get_opnd(pir1, 0)));
    int gpr_num = ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0));
    IR2_OPND gpr_opnd = ra_alloc_gpr(gpr_num);
    load_ireg_from_ir1_2(gpr_opnd, ir1_get_opnd(pir1, 1), SIGN_EXTENSION, false);
#ifdef TARGET_X86_64
    if (CODEIS64 && ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 32)
        la_bstrpick_d(gpr_opnd, gpr_opnd, 31, 0);
#endif
    return true;
}

static void load_step_to_reg(IR2_OPND *p_step_opnd, IR1_INST *pir1)
{
    IR2_OPND df_opnd = ra_alloc_itemp();
    IR2_OPND eflags_opnd = ra_alloc_eflags();
    la_andi(df_opnd, eflags_opnd, 0x400);

    int bytes = ir1_opnd_size(ir1_get_opnd(pir1, 0)) >> 3;
    int bits = 0;
    switch (ir1_opnd_size(ir1_get_opnd(pir1, 0))) {
    case 8:
        bits = 9;
        break;
    case 16:
        bits = 8;
        break;
    case 32:
        bits = 7;
        break;
#ifdef TARGET_X86_64
    case 64:
        bits = 6;
        break;
#endif
    }
    lsassert(bits != 0);
    IR2_OPND tmp_step = ra_alloc_itemp();
    la_srai_w(tmp_step, df_opnd, bits);
    la_addi_w(*p_step_opnd, tmp_step, 0 - bytes);

    ra_free_temp(df_opnd);
    ra_free_temp(tmp_step);
}

static void load_step_to_reg_ir1(IR2_OPND *p_step_opnd, IR1_OPND *opnd_ir1)
{
    IR2_OPND df_opnd = ra_alloc_itemp();
    IR2_OPND eflags_opnd = ra_alloc_eflags();
    la_andi(df_opnd, eflags_opnd, 0x400);

    int bytes = ir1_opnd_size(opnd_ir1) >> 3;
    int bits = 0;
    switch (ir1_opnd_size(opnd_ir1)) {
    case 8:
        bits = 9;
        break;
    case 16:
        bits = 8;
        break;
    case 32:
        bits = 7;
        break;
#ifdef TARGET_X86_64
    case 64:
        bits = 6;
        break;
#endif
    }
    lsassert(bits != 0);
    IR2_OPND tmp_step = ra_alloc_itemp();
    la_srai_w(tmp_step, df_opnd, bits);
    la_addi_w(*p_step_opnd, tmp_step, 0 - bytes);

    ra_free_temp(df_opnd);
    ra_free_temp(tmp_step);
}

bool translate_movs(IR1_INST *pir1)
{
    IR2_OPND esi_opnd = ra_alloc_gpr(esi_index);
    IR2_OPND edi_opnd = ra_alloc_gpr(edi_index);

    /*
     * The rep-loop may not be executed, the em of EDI/ESI will be
     * set to EM_X86_ADDRESS after translation, it may cause a
     * segfault, so move the `and edi, edi, n1` out of the loop.
     */
#ifndef TARGET_X86_64
    clear_h32(&edi_opnd);
    clear_h32(&esi_opnd);
#else
    if (!CODEIS64) {
        clear_h32(&edi_opnd);
        clear_h32(&esi_opnd);
    }
#endif
    /* 1. exit when initial count is zero */
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND ecx_opnd = ra_alloc_gpr(ecx_index);
    if (ir1_prefix(pir1) != 0) {
#ifndef TARGET_X86_64
        clear_h32(&ecx_opnd);
#else
        if (!CODEIS64) {
            clear_h32(&ecx_opnd);
        }
#endif
        la_beq(ecx_opnd, zero_ir2_opnd, label_exit);
    }
    IR1_OPND *mem_si = NULL;
    IR1_OPND *mem_di = NULL;
    if (ir1_get_opnd_num(pir1) == 0) {
        switch (pir1->info->id) {
        case dt_X86_INS_MOVSB:
            mem_di = &edi_mem8_ir1_opnd;
            mem_si = &esi_mem8_ir1_opnd;
#ifdef TARGET_X86_64
            if (CODEIS64) {
                if (pir1->info->x86.prefix[3] != 0x67) {
                    mem_di = &rdi_mem8_ir1_opnd;
                    mem_si = &rsi_mem8_ir1_opnd;
                }
            } else {
                if (pir1->info->x86.prefix[3] == 0x67) {
                    mem_di = &di_mem8_ir1_opnd;
                    mem_si = &si_mem8_ir1_opnd;
                }
            }
#else
            if (pir1->info->x86.prefix[3] == 0x67) {
                mem_di = &di_mem8_ir1_opnd;
                mem_si = &si_mem8_ir1_opnd;
            }
#endif
            break;
        case dt_X86_INS_MOVSW:
            mem_di = &edi_mem16_ir1_opnd;
            mem_si = &esi_mem16_ir1_opnd;
#ifdef TARGET_X86_64
            if (CODEIS64) {
                if (pir1->info->x86.prefix[3] != 0x67) {
                    mem_di = &rdi_mem16_ir1_opnd;
                    mem_si = &rsi_mem16_ir1_opnd;
                }
            } else {
                if (pir1->info->x86.prefix[3] == 0x67) {
                    mem_di = &di_mem16_ir1_opnd;
                    mem_si = &si_mem16_ir1_opnd;
                }
            }
#else
            if (pir1->info->x86.prefix[3] == 0x67) {
                mem_di = &di_mem16_ir1_opnd;
                mem_si = &si_mem16_ir1_opnd;
            }
#endif
            break;
        case dt_X86_INS_MOVSD:
            mem_di = &edi_mem32_ir1_opnd;
            mem_si = &esi_mem32_ir1_opnd;
#ifdef TARGET_X86_64
            if (CODEIS64) {
                if (pir1->info->x86.prefix[3] != 0x67) {
                    mem_di = &rdi_mem32_ir1_opnd;
                    mem_si = &rsi_mem32_ir1_opnd;
                }

            } else {
                if (pir1->info->x86.prefix[3] == 0x67) {
                    mem_di = &di_mem32_ir1_opnd;
                    mem_si = &si_mem32_ir1_opnd;
                }
            }
#else
            if (pir1->info->x86.prefix[3] == 0x67) {
                mem_di = &di_mem32_ir1_opnd;
                mem_si = &si_mem32_ir1_opnd;
            }
#endif
            break;
#ifdef TARGET_X86_64
        case dt_X86_INS_MOVSQ:
            mem_di = &edi_mem64_ir1_opnd;
            mem_si = &esi_mem64_ir1_opnd;
            if (pir1->info->x86.prefix[3] != 0x67) {
                mem_di = &rdi_mem64_ir1_opnd;
                mem_si = &rsi_mem64_ir1_opnd;
            }
            break;
#endif
        default:
            lsassert(0);
        }
        mem_di->mem.segment = dt_X86_REG_ES;
    } else if (ir1_get_opnd_num(pir1) == 2) {
        mem_di = ir1_get_opnd(pir1, 0);
        mem_si = ir1_get_opnd(pir1, 0) + 1;
    } else {
        lsassert(0);
    }

    /* 2. preparations outside the loop */
    IR2_OPND step_opnd = ra_alloc_itemp();
    load_step_to_reg_ir1(&step_opnd, mem_di);

    /* 3. loop starts */
    IR2_OPND label_loop_begin = ra_alloc_label();
    la_label(label_loop_begin);

    /* 3.1 load memory value at ESI, and store into memory at EDI */
    IR2_OPND esi_mem_value =
        load_ireg_from_ir1(mem_si, SIGN_EXTENSION, false);
    store_ireg_to_ir1(esi_mem_value, mem_di, false);

    /* 3.2 adjust ESI and EDI */
    la_sub(esi_opnd, esi_opnd, step_opnd);
    la_sub(edi_opnd, edi_opnd, step_opnd);
#ifndef TARGET_X86_64
    clear_h32(&edi_opnd);
    clear_h32(&esi_opnd);
#else
    if (!CODEIS64) {
        clear_h32(&edi_opnd);
        clear_h32(&esi_opnd);
    }
#endif

    /* 4. loop ends? when ecx==0 */
    if (ir1_prefix(pir1) != 0) {
        lsassert(ir1_prefix(pir1) == dt_X86_PREFIX_REP);
#ifndef TARGET_X86_64
        la_addi_w(ecx_opnd, ecx_opnd, -1);
#else
        if (CODEIS64) {
            la_addi_d(ecx_opnd, ecx_opnd, -1);
        } else {
            la_addi_w(ecx_opnd, ecx_opnd, -1);
        }
#endif
        la_bne(ecx_opnd, zero_ir2_opnd, label_loop_begin);
        store_ireg_to_ir1(ecx_opnd, &ecx_ir1_opnd, false);
    }

    /* 5. exit */
    la_label(label_exit);

    ra_free_temp(step_opnd);
    return true;
}

bool translate_stos(IR1_INST *pir1)
{
    IR2_OPND edi_opnd = ra_alloc_gpr(edi_index);

    /*
     * The rep-loop may not be executed, the em of EDI will be
     * set to EM_X86_ADDRESS after translation, it may cause a
     * segfault, so move the `and edi, edi, n1` out of the loop.
     */
#ifndef TARGET_X86_64
    clear_h32(&edi_opnd);
#else
    if (!CODEIS64) {
        clear_h32(&edi_opnd);
    }
#endif

    /* 1. exit when initial count is zero */
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND ecx_opnd = ra_alloc_gpr(ecx_index);
    if (ir1_prefix(pir1) != 0) {
#ifndef TARGET_X86_64
        clear_h32(&ecx_opnd);
#else
        if (!CODEIS64) {
            clear_h32(&ecx_opnd);
        }
#endif
        la_beq(ecx_opnd, zero_ir2_opnd, label_exit);
    }

    /* 2. preparations outside the loop */
    IR1_OPND *opnd_eax = NULL;
    IR1_OPND *opnd_di = NULL;
    if (ir1_get_opnd_num(pir1) > 0) {
        opnd_di = ir1_get_opnd(pir1, 0);
    }
    switch (ir1_get_opnd_num(pir1)) {
    case 0:
        switch (pir1->info->id) {
        case dt_X86_INS_STOSB:
            opnd_eax = &eax_ir1_opnd;
            opnd_di = &edi_mem8_ir1_opnd;
#ifdef TARGET_X86_64
            if (CODEIS64) {
                if (pir1->info->x86.prefix[3] != 0x67) {
                    opnd_eax = &rax_ir1_opnd;
                    opnd_di = &rdi_mem8_ir1_opnd;
                }
            } else {
                if (pir1->info->x86.prefix[3] == 0x67) {
                    opnd_eax = &ax_ir1_opnd;
                    opnd_di = &di_mem8_ir1_opnd;
                }
            }
#else
            if (pir1->info->x86.prefix[3] == 0x67) {
                opnd_eax = &ax_ir1_opnd;
                opnd_di = &di_mem8_ir1_opnd;
            }
#endif
            break;
        case dt_X86_INS_STOSW:
            opnd_eax = &eax_ir1_opnd;
            opnd_di = &edi_mem16_ir1_opnd;
#ifdef TARGET_X86_64
            if (CODEIS64) {
                if (pir1->info->x86.prefix[3] != 0x67) {
                    opnd_eax = &rax_ir1_opnd;
                    opnd_di = &rdi_mem16_ir1_opnd;
                }
            } else {
                if (pir1->info->x86.prefix[3] == 0x67) {
                    opnd_eax = &ax_ir1_opnd;
                    opnd_di = &di_mem16_ir1_opnd;
                }
            }
#else
            if (pir1->info->x86.prefix[3] == 0x67) {
                opnd_eax = &ax_ir1_opnd;
                opnd_di = &di_mem16_ir1_opnd;
            }
#endif
            break;
        case dt_X86_INS_STOSD:
            opnd_eax = &eax_ir1_opnd;
            opnd_di = &edi_mem32_ir1_opnd;
#ifdef TARGET_X86_64
            if (CODEIS64) {
                if (pir1->info->x86.prefix[3] != 0x67) {
                    opnd_eax = &rax_ir1_opnd;
                    opnd_di = &rdi_mem32_ir1_opnd;
                }
            } else {
                if (pir1->info->x86.prefix[3] == 0x67) {
                    opnd_eax = &ax_ir1_opnd;
                    opnd_di = &di_mem32_ir1_opnd;
                }
            }
#else
            if (pir1->info->x86.prefix[3] == 0x67) {
                opnd_eax = &ax_ir1_opnd;
                opnd_di = &di_mem32_ir1_opnd;
            }
#endif
            break;
#ifdef TARGET_X86_64
        case dt_X86_INS_STOSQ:
            if (CODEIS64) {
                opnd_eax = &eax_ir1_opnd;
                opnd_di = &edi_mem32_ir1_opnd;
                if (pir1->info->x86.prefix[3] != 0x67) {
                    opnd_eax = &rax_ir1_opnd;
                    opnd_di = &rdi_mem64_ir1_opnd;
                }
                break;
            } else {
                lsassert(0);
                break;
            }
#endif
        default:
            lsassert(0);
        }
        opnd_di->mem.segment = dt_X86_REG_ES;
        break;
    case 1:
        switch (ir1_opnd_size(ir1_get_opnd(pir1, 0))) {
        case 8:
            opnd_eax = &al_ir1_opnd;
            break;
        case 16:
            opnd_eax = &ax_ir1_opnd;
            break;
        case 32:
            opnd_eax = &eax_ir1_opnd;
            break;
        case 64:
            opnd_eax = &rax_ir1_opnd;
            break;
        default:
            lsassert(0);
        }
        break;
    case 2:
        opnd_eax = ir1_get_opnd(pir1, 0) + 1;
        break;
    default:
        lsassert(0);
    }
    IR2_OPND eax_value_opnd =
        load_ireg_from_ir1(opnd_eax, SIGN_EXTENSION, false);
    IR2_OPND step_opnd = ra_alloc_itemp();
    load_step_to_reg_ir1(&step_opnd, opnd_di);

    /* 3. loop starts */
    IR2_OPND label_loop_begin = ra_alloc_label();
    la_label(label_loop_begin);

    /* 3.1 store EAX into memory at EDI */
    store_ireg_to_ir1(eax_value_opnd, opnd_di, false);

    /* 3.2 adjust EDI */
    la_sub(edi_opnd, edi_opnd, step_opnd);
#ifndef TARGET_X86_64
    clear_h32(&edi_opnd);
#else
    if (!CODEIS64) {
        clear_h32(&edi_opnd);
    }
#endif

    /* 4. loop ends? when ecx==0 */
    if (ir1_prefix(pir1) != 0) {
        lsassert(ir1_prefix(pir1) == dt_X86_PREFIX_REP);
#ifndef TARGET_X86_64
        la_addi_w(ecx_opnd, ecx_opnd, -1);
#else
        if (CODEIS64) {
            la_addi_d(ecx_opnd, ecx_opnd, -1);
        } else {
            la_addi_w(ecx_opnd, ecx_opnd, -1);
        }
#endif
        la_bne(ecx_opnd, zero_ir2_opnd, label_loop_begin);
        store_ireg_to_ir1(ecx_opnd, &ecx_ir1_opnd, false);
    }

    /* 5. exit */
    la_label(label_exit);

    ra_free_temp(step_opnd);
    return true;
}

bool translate_lods(IR1_INST *pir1)
{
    IR2_OPND esi_opnd = ra_alloc_gpr(esi_index);
    /*
     * The rep-loop may not be executed, the em of ESI will be
     * set to EM_X86_ADDRESS after translation, it may cause a
     * segfault, so move the `and edi, edi, n1` out of the loop.
     */
#ifndef TARGET_X86_64
    clear_h32(&esi_opnd);
#else
    if (!CODEIS64) {
        clear_h32(&esi_opnd);
    }
#endif

    /* 1. exit when initial count is zero */
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND ecx_opnd = ra_alloc_gpr(ecx_index);
    if (ir1_prefix(pir1) != 0) {
#ifndef TARGET_X86_64
        clear_h32(&ecx_opnd);
#else
        if (!CODEIS64) {
            clear_h32(&ecx_opnd);
        }
#endif
        la_beq(ecx_opnd, zero_ir2_opnd, label_exit);
    }

    /* 2. preparations outside the loop */
    IR2_OPND step_opnd = ra_alloc_itemp();
    load_step_to_reg(&step_opnd, pir1);

    /* 3. loop starts */
    IR2_OPND label_loop_begin = ra_alloc_label();
    la_label(label_loop_begin);

    /* 3.1 load memory value at ESI */
    EXTENSION_MODE em = SIGN_EXTENSION;
    if (ir1_opnd_size(ir1_get_opnd(pir1, 0)) < 32)
        em = ZERO_EXTENSION;
    IR2_OPND esi_mem_value = load_ireg_from_ir1(ir1_get_opnd(pir1, 0) + 1, em, false);

    /* 3.2 adjust ESI */
    la_sub(esi_opnd, esi_opnd, step_opnd);

    /* 4. loop ends? when ecx==0 */
    if (ir1_prefix(pir1) != 0) {
        lsassert(ir1_prefix(pir1) == dt_X86_PREFIX_REP);
#ifndef TARGET_X86_64
        la_addi_w(ecx_opnd, ecx_opnd, -1);
#else
        if (CODEIS64) {
            la_addi_d(ecx_opnd, ecx_opnd, -1);
        } else {
            la_addi_w(ecx_opnd, ecx_opnd, -1);
        }
#endif
        la_bne(ecx_opnd, zero_ir2_opnd, label_loop_begin);
        store_ireg_to_ir1(ecx_opnd, &ecx_ir1_opnd, false);
    }

    store_ireg_to_ir1(esi_mem_value, ir1_get_opnd(pir1, 0), false);

    /* 5. exit */
    la_label(label_exit);

    ra_free_temp(step_opnd);
    return true;
}

bool translate_cmps(IR1_INST *pir1)
{
    /* 1. exit when initial count is zero */
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND ecx_opnd = ra_alloc_gpr(ecx_index);
    if (ir1_prefix(pir1) != 0) {
#ifndef TARGET_X86_64
        clear_h32(&ecx_opnd);
#else
        if (!CODEIS64) {
            clear_h32(&ecx_opnd);
        }
#endif
        la_beq(ecx_opnd, zero_ir2_opnd, label_exit);
    }

    /* 2. preparations outside the loop */
    IR2_OPND step_opnd = ra_alloc_itemp();
    load_step_to_reg(&step_opnd, pir1);

    /* 3. loop starts */
    IR2_OPND label_loop_begin = ra_alloc_label();
    la_label(label_loop_begin);

    /* 3.1 load memory value at ESI and EDI */
    IR2_OPND esi_mem_value =
        load_ireg_from_ir1(ir1_get_opnd(pir1, 0), SIGN_EXTENSION, false);
    IR2_OPND edi_mem_value =
        load_ireg_from_ir1(ir1_get_opnd(pir1, 0) + 1, SIGN_EXTENSION, false);

    /* 3.2 adjust ESI and EDI */
    IR2_OPND esi_opnd = ra_alloc_gpr(esi_index);
    IR2_OPND edi_opnd = ra_alloc_gpr(edi_index);
    la_sub(esi_opnd, esi_opnd, step_opnd);
    la_sub(edi_opnd, edi_opnd, step_opnd);
#ifndef TARGET_X86_64
    clear_h32(&edi_opnd);
    clear_h32(&esi_opnd);
#else
    if (!CODEIS64) {
        clear_h32(&edi_opnd);
        clear_h32(&esi_opnd);
    }
#endif

    /* 3.3 compare */
    IR2_OPND cmp_result = ra_alloc_itemp();
    la_sub_w(cmp_result, esi_mem_value, edi_mem_value);

    /* 4. loop ends? */
    if (ir1_prefix(pir1) != 0) {
#ifndef TARGET_X86_64
        la_addi_w(ecx_opnd, ecx_opnd, -1);
#else
        if (CODEIS64) {
            la_addi_d(ecx_opnd, ecx_opnd, -1);
        } else {
            la_addi_w(ecx_opnd, ecx_opnd, -1);
        }
#endif
        /* 4.1. loop ends when ecx==0 */
        IR2_OPND condition = ra_alloc_itemp();
        /* set 1 when ecx==0 */
        la_sltui(condition, ecx_opnd, 1);
        /* 4.2 loop ends when result != 0 (repe), and when result==0 (repne) */
        IR2_OPND condition2 = ra_alloc_itemp();
        if (ir1_prefix(pir1) == dt_X86_PREFIX_REPE) {
            /* set 1 when 0<result, i.e., result!=0 */
            la_sltu(condition2,
                                    zero_ir2_opnd, cmp_result);
        } else {
            /* set 1 when result<1, i.e., result==0 */
            la_sltui(condition2, cmp_result, 1);
        }

        /* 4.3 when none of the two conditions is satisfied, the loop continues */
        la_or(condition, condition, condition2);
        la_beq(condition, zero_ir2_opnd, label_loop_begin);

        ra_free_temp(condition);
        ra_free_temp(condition2);
        store_ireg_to_ir1(ecx_opnd, &ecx_ir1_opnd, false);
    }

    /* 5. calculate eflags */
    generate_eflag_calculation(cmp_result, esi_mem_value, edi_mem_value, pir1,
                               true);
    la_label(label_exit);

    ra_free_temp(step_opnd);
    ra_free_temp(edi_mem_value);
    ra_free_temp(cmp_result);
    return true;
}

bool translate_scas(IR1_INST *pir1)
{
    /* 1. exit when initial count is zero */
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND ecx_opnd = ra_alloc_gpr(ecx_index);
    if (ir1_prefix(pir1) != 0) {
#ifndef TARGET_X86_64
        clear_h32(&ecx_opnd);
#else
        if (!CODEIS64) {
            clear_h32(&ecx_opnd);
        }
#endif
        la_beq(ecx_opnd, zero_ir2_opnd, label_exit);
    }

    /* 2. preparations outside the loop */
    // IR2_OPND eax_value_opnd =
    //     load_ireg_from_ir1(ir1_get_opnd(pir1, 0) + 1, SIGN_EXTENSION, false);
    IR1_OPND *opnd_eax = NULL;
    switch (ir1_get_opnd_num(pir1)) {
    case 1:
        switch (ir1_opnd_size(ir1_get_opnd(pir1, 0))) {
        case 8:
            opnd_eax = &al_ir1_opnd;
            break;
        case 16:
            opnd_eax = &ax_ir1_opnd;
            break;
        case 32:
            opnd_eax = &eax_ir1_opnd;
            break;
        case 64:
            opnd_eax = &rax_ir1_opnd;
            break;
        default:
            lsassert(0);
        }
        break;
    case 2:
        opnd_eax = ir1_get_opnd(pir1, 0);
        break;
    default:
        lsassert(0);
    }
    IR2_OPND eax_value_opnd =         //capstone eax first opnd
        load_ireg_from_ir1(opnd_eax, SIGN_EXTENSION, false);
    IR2_OPND step_opnd = ra_alloc_itemp();
    load_step_to_reg(&step_opnd, pir1);

    /* 3. loop starts */
    IR2_OPND label_loop_begin = ra_alloc_label();
    la_label(label_loop_begin);

    /* 3.1 load memory value at EDI */
    // IR2_OPND edi_mem_value =       //capstone edi second opnd
    //     load_ireg_from_ir1(ir1_get_opnd(pir1, 0), SIGN_EXTENSION, false);
    uint8_t index = 0;
    switch (ir1_get_opnd_num(pir1)) {
    case 1:
        index = 0;
        break;
    case 2:
        index = 1;
        break;
    default:
        lsassert(0);
    }
    IR2_OPND edi_mem_value =
        load_ireg_from_ir1(ir1_get_opnd(pir1, index), SIGN_EXTENSION, false);
    /* 3.2 adjust edi */
    IR2_OPND edi_opnd = ra_alloc_gpr(edi_index);
    la_sub(edi_opnd, edi_opnd, step_opnd);
#ifndef TARGET_X86_64
    clear_h32(&edi_opnd);
#else
    if (!CODEIS64) {
        clear_h32(&edi_opnd);
    }
#endif

    /* 3.3 compare */
    IR2_OPND cmp_result = ra_alloc_itemp();
#ifndef TARGET_X86_64
    la_sub_w(cmp_result, eax_value_opnd, edi_mem_value);
#else
    if (CODEIS64) {
        la_sub_d(cmp_result, eax_value_opnd, edi_mem_value);
    } else {
        la_sub_w(cmp_result, eax_value_opnd, edi_mem_value);
    }
#endif

    /* 4. loop ends? */
    if (ir1_prefix(pir1) != 0) {
#ifndef TARGET_X86_64
        la_addi_w(ecx_opnd, ecx_opnd, -1);
#else
        if (CODEIS64) {
            la_addi_d(ecx_opnd, ecx_opnd, -1);
        } else {
            la_addi_w(ecx_opnd, ecx_opnd, -1);
        }
#endif

        /* 4.1. loop ends when ecx==0 */
        IR2_OPND condition = ra_alloc_itemp();
        /* set 1 when ecx==0 */
        la_sltui(condition, ecx_opnd, 1);

        /* 4.2 loop ends when result != 0 (repe), and when result==0 (repne) */
        IR2_OPND condition2 = ra_alloc_itemp();
        if (ir1_prefix(pir1) == dt_X86_PREFIX_REPE) {
            /* set 1 when 0<result, i.e., result!=0 */
            la_sltu(condition2,
                                    zero_ir2_opnd, cmp_result);
        } else {
            /* set 1 when result<1, i.e., result==0 */
            la_sltui(condition2, cmp_result, 1);
        }

        /* 4.3 when none of the two conditions is satisfied, the loop continues */
        la_or(condition, condition, condition2);
        la_beq(condition, zero_ir2_opnd, label_loop_begin);
        ra_free_temp(condition);
        ra_free_temp(condition2);
#ifndef TARGET_X86_64
        store_ireg_to_ir1(ecx_opnd, &ecx_ir1_opnd, false);
#else
        if (CODEIS64) {
            store_ireg_to_ir1(ecx_opnd, &rcx_ir1_opnd, false);
        } else {
            store_ireg_to_ir1(ecx_opnd, &ecx_ir1_opnd, false);
        }
#endif
    }

    /* 5. calculate eflags */
    generate_eflag_calculation(cmp_result, eax_value_opnd, edi_mem_value, pir1,
                               true);
    la_label(label_exit);

    ra_free_temp(step_opnd);
    ra_free_temp(edi_mem_value);
    ra_free_temp(cmp_result);
    return true;
}

/**
 * @brief CMOVcc translation
 *
 * CMOVcc has:
 * - CMOVO    // OF = 1
 * - CMOVNO   // OF = 0
 * - CMOVB    // CF = 1
 * - CMOVAE   // CF = 0
 * - CMOVE    // ZF = 1
 * - CMOVNE   // ZF = 0
 * - CMOVBE   // CF = 1 | ZF = 1
 * - CMOVA    // CF = O & ZF = O
 * - CMOVS    // SF = 1
 * - CMOVNS   // SF = 0
 * - CMOVP    // PF = 1
 * - CMOVNP   // PF = 0
 * - CMOVL    // SF <> OF
 * - CMOVGE   // SF = OF
 * - CMOVLE   // ZF = 1 | SF <> OF
 * - CMOVG    // ZF = 0 & SF = OF
 *
 * @param pir1 input current inst
 * @return true if transalte success
 * @return false translate failure
 */
bool translate_cmovcc(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    lsassertm(ir1_opnd_is_gpr(opnd0), "CMOVcc need used gpr as dest!");

    IR2_OPND neg_condition = ra_alloc_itemp();

    get_eflag_condition(&neg_condition, pir1);

    /* src maybe a mem */
    IR2_OPND src_opnd = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    IR2_OPND dest_opnd = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd0));
    IR2_OPND cond1 = ra_alloc_itemp();
    IR2_OPND cond2 = ra_alloc_itemp();

    /* CC = 1 */
    la_masknez(cond1, dest_opnd, neg_condition);
    la_maskeqz(cond2, src_opnd, neg_condition);

    if (ir1_opnd_size(opnd0) == 64) {
        la_or(dest_opnd, cond1, cond2);
    } else {
        la_or(cond1, cond1, cond2);
        store_ireg_to_ir1(cond1, opnd0, false);
    }
    ra_free_temp(cond1);
    ra_free_temp(cond2);
    ra_free_temp(neg_condition);

    return true;
}

bool translate_lea(IR1_INST *pir1)
{
    IR1_OPND *op0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *op1 = ir1_get_opnd(pir1, 1);
    int op0_size = ir1_opnd_size(op0);
    int op0_gpr_num = ir1_opnd_base_reg_num(op0);
    IR2_OPND dest_op = ra_alloc_gpr(op0_gpr_num);

    convert_mem_to_specific_gpr(op1, dest_op, op0_size);

    return true;
}

#ifndef CONFIG_LATX_LLSC
static bool translate_xchg_spinlock(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR2_OPND mem_opnd;
    int imm;

    /* load REG opnand and MEM opnand */
    if (ir1_opnd_is_mem(opnd0)) {
        mem_opnd = convert_mem(opnd0, &imm);
    } else {
        mem_opnd = convert_mem(opnd1, &imm);
    }
    ir2_set_opnd_type(&mem_opnd, IR2_OPND_GPR);
    IR2_OPND lat_lock_addr = tr_lat_spin_lock(mem_opnd, imm);

    if (ir1_opnd_is_mem(opnd0)) {
        IR2_OPND src_opnd_0 =
            load_ireg_from_ir2_mem(mem_opnd, imm, ir1_opnd_size(opnd0),
                                   UNKNOWN_EXTENSION, false);
        IR2_OPND src_opnd_1 =
            load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
        store_ireg_to_ir2_mem(src_opnd_1, mem_opnd, imm, ir1_opnd_size(opnd0), false);
        store_ireg_to_ir1(src_opnd_0, opnd1, false);
    } else if (ir1_opnd_is_mem(opnd1)) {
        IR2_OPND src_opnd_0 =
            load_ireg_from_ir1(opnd0, UNKNOWN_EXTENSION, false);
        IR2_OPND src_opnd_1 =
            load_ireg_from_ir2_mem(mem_opnd, imm, ir1_opnd_size(opnd1),
                                   UNKNOWN_EXTENSION, false);
        store_ireg_to_ir1(src_opnd_1, opnd0, false);
        store_ireg_to_ir2_mem(src_opnd_0, mem_opnd, imm, ir1_opnd_size(opnd1), false);
    } else { /* none src is temp reg */
        lsassertm(0, "Invalid case in translate_xchg.\n");
    }

    tr_lat_spin_unlock(lat_lock_addr);
    return true;
}
#endif

bool translate_xchg(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND src0, src1;
    IR2_OPND dest __attribute__((unused));
    IR2_OPND tmp __attribute__((unused));
    IR2_OPND mem_opnd __attribute__((unused));
    int opnd0_size;
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);

    opnd0_size = ir1_opnd_size(opnd0);

    if ((ir1_opnd_is_gpr(opnd0) && ir1_opnd_is_gpr(opnd1))) {
        /* get src0 */
        src0 = load_ireg_from_ir1(opnd0, UNKNOWN_EXTENSION, false);

        /* get src1 */
        src1 = ra_alloc_itemp();
        load_ireg_from_ir1_2(src1, opnd1, UNKNOWN_EXTENSION, false);

        if (ir1_opnd_is_same_reg(opnd0, opnd1)) {
#ifdef TARGET_X86_64
            if (!GHBR_ON(pir1) && CODEIS64 && opnd0_size == 32) {
                la_mov32_zx(src0, src0);
            }
#endif
            return true;
        }

        store_ireg_to_ir1(src0, opnd1, false);
        store_ireg_to_ir1(src1, opnd0, false);
        return true;
    }

    if (!close_latx_parallel && !(cpu->tcg_cflags & CF_PARALLEL)) {
        /* get src0 and src1*/
        src0 = load_ireg_from_ir1(opnd0, UNKNOWN_EXTENSION, false);
        src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);

        if (ir1_opnd_is_gpr(opnd0)) {
            store_ireg_to_ir1(src0, opnd1, false);
            store_ireg_to_ir1(src1, opnd0, false);
        } else {
            store_ireg_to_ir1(src1, opnd0, false);
            store_ireg_to_ir1(src0, opnd1, false);
        }
        return true;
    }
    /*
     * If a memory operand is referenced, the processor’s locking protocol
     * is automatically implemented for the duration of the exchange
     * operation, regardless of the presence or absence of the LOCK prefix
     * or of the value of the IOPL.
     */
#ifndef CONFIG_LATX_LLSC
    return translate_xchg_spinlock(pir1);
#else
    IR2_OPND label_interpret = ra_alloc_label();
    IR2_OPND label_aligned = ra_alloc_label();
    IR2_OPND label_not_aligned = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND label_ll = ra_alloc_label();
    IR1_OPND *reg_opnd;

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();
    tmp = ra_alloc_itemp();

    /* get src1 and mem_opnd */
    if (ir1_opnd_is_mem(opnd0)) {
#ifdef CONFIG_LATX_IMM_REG
        /**
         * in case of changing mem_opnd later(bstrins),
         * we pass mem_opnd with a copyed one.
         */
        mem_opnd = ra_alloc_itemp();
        IR2_OPND imm_cache_mem_opnd = convert_mem_to_itemp(opnd0);
        la_or(mem_opnd, imm_cache_mem_opnd, zero_ir2_opnd);
#else
        mem_opnd = convert_mem_to_itemp(opnd0);
#endif
        src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
        reg_opnd = opnd1;
    } else {
#ifdef CONFIG_LATX_IMM_REG
        /**
         * in case of changing mem_opnd later(bstrins),
         * we pass mem_opnd with a copyed one.
         */
        mem_opnd = ra_alloc_itemp();
        IR2_OPND imm_cache_mem_opnd = convert_mem_to_itemp(opnd0);
        la_or(mem_opnd, imm_cache_mem_opnd, zero_ir2_opnd);
#else
        mem_opnd = convert_mem_to_itemp(opnd1);
#endif
        src1 = load_ireg_from_ir1(opnd0, UNKNOWN_EXTENSION, false);
        reg_opnd = opnd0;
    }

#ifdef TARGET_X86_64
    if (CODEIS64 && opnd0_size == 64) {
        la_amswap_db_d(src0, src1, mem_opnd);
        la_or(src1, src0, zero_ir2_opnd);
        return true;
    }
#endif

    /*
     * (original-address % 8) => the target-byte's offset in
     * the 8-bytes loaded.
     */
    la_bstrpick_d(tmp, mem_opnd, 2, 0);
    if (opnd0_size == 32) {
        li_wu(dest, 4);
        la_blt(dest, tmp, label_interpret);
        la_beq(tmp, zero_ir2_opnd, label_aligned);
        la_blt(tmp, dest, label_not_aligned);

        /* The following code is used when memory is aligned. */
        la_label(label_aligned);
        la_amswap_db_w(src0, src1, mem_opnd);
        la_b(label_exit);
    } else if (opnd0_size == 16) {
        li_wu(dest, 7);
        la_beq(tmp, dest, label_interpret);
    }

    /* The following code is used when memory is not aligned*/
    IR2_OPND src0_cpy = ra_alloc_itemp();
    uint32 mask;
    la_label(label_not_aligned);

    la_bstrins_d(mem_opnd, zero_ir2_opnd, 2, 0);
    la_slli_d(tmp, tmp, 3);
    la_label(label_ll);
    la_ll_d(src0, mem_opnd, 0);
    la_or(src0_cpy, src0, zero_ir2_opnd);
    la_srl_d(src0, src0, tmp);

    if (opnd0_size == 32) {
        mask = 0xFFFFFFFF;
    } else if (opnd0_size == 16) {
        mask = 0xFFFF;
    } else {
        mask = 0xFF;
    }

    /* use dest as mask_opnd*/
    li_wu(dest, mask);
    la_sll_d(dest, dest, tmp);
    la_nor(dest, zero_ir2_opnd, dest);
    la_and(src0_cpy, src0_cpy, dest);

    /* calculate */
    la_or(dest, zero_ir2_opnd, src1);

    /* rebuild dest */
    la_bstrpick_d(dest, dest, opnd0_size - 1, 0);
    la_sll_d(dest, dest, tmp);
    la_or(src0_cpy, src0_cpy, dest);

    /* write back */
    la_sc_d(src0_cpy, mem_opnd, 0);
    la_beq(src0_cpy, zero_ir2_opnd, label_ll);
    la_b(label_exit);

    /*
     * interpret path
     */
    la_andi(zero_ir2_opnd, zero_ir2_opnd, opnd0_size);
    la_label(label_interpret);
    la_amswap_d(src0, src1, mem_opnd);

    /*
     * exit
     */
    la_label(label_exit);
    store_ireg_to_ir1(src0, reg_opnd, false);
    return true;
#endif
}

bool translate_cmpxchg(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    CPUState *cpu = env_cpu(env);
    bool is_lock = ir1_is_prefix_lock(pir1) && ir1_opnd_is_mem(opnd0);
    if (!close_latx_parallel) {
        is_lock = is_lock && (cpu->tcg_cflags & CF_PARALLEL);
    }

#ifdef CONFIG_LATX_LLSC
    if (is_lock) {
        return translate_lock_cmpxchg(pir1);
    }
#endif

    IR2_OPND lat_lock_addr;
    IR2_OPND src0, src1, eax_opnd;
    IR2_OPND mem_opnd;
    int imm, opnd0_size;
    IR1_OPND *reg_ir1 = NULL;

    opnd0_size = ir1_opnd_size(opnd0);

    IR2_OPND label_unequal = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    /* get eax */
    if (ir1_opnd_size(opnd0) == 64) {
        reg_ir1 = &rax_ir1_opnd;
    } else if (ir1_opnd_size(opnd0) == 32) {
        reg_ir1 = &eax_ir1_opnd;
    } else if (ir1_opnd_size(opnd0) == 16) {
        reg_ir1 = &ax_ir1_opnd;
    } else if (ir1_opnd_size(opnd0) == 8) {
        reg_ir1 = &al_ir1_opnd;
    }

    /* fast path: cmpxchg rax/eax/ax,r64/r32/r16 */
    if (ir1_opnd_is_same_reg(opnd0, reg_ir1)) {
        eax_opnd = ra_alloc_gpr(eax_index);
        generate_eflag_calculation(eax_opnd, eax_opnd, eax_opnd, pir1, true);
        src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
        store_ireg_to_ir1(src1, opnd0, false);
        return true;
    }

    eax_opnd = load_ireg_from_ir1(reg_ir1, SIGN_EXTENSION, false);

    /* get src0 */
    if (ir1_opnd_is_gpr(opnd0)) {
        src0 = convert_gpr_opnd(opnd0, SIGN_EXTENSION);
    } else {
        src0 = ra_alloc_itemp();
        mem_opnd = convert_mem(opnd0, &imm);
        if (is_lock) {
            lat_lock_addr = tr_lat_spin_lock(mem_opnd, imm);
        }
        la_ld_by_op_size(src0, mem_opnd, imm, opnd0_size);
    }

    generate_eflag_calculation(src0, eax_opnd, src0, pir1, true);
    la_bne(src0, eax_opnd, label_unequal);
    /* equal */
    src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    store_ireg_to_ir1(src1, opnd0, false);
    la_b(label_exit);
    /* unequal */
    la_label(label_unequal);
    la_dbar(0);
    store_ireg_to_ir1(src0, reg_ir1, false);
    la_label(label_exit);

    /* unlock */
    if (!ir1_opnd_is_gpr(opnd0) && is_lock) {
        tr_lat_spin_unlock(lat_lock_addr);
    }

    return true;
}

/*
 * FIXME: This implementation is NOT thread safe.
 */
bool translate_cmpxchg8b(IR1_INST *pir1)
{
    IR1_OPND *reg_eax = &eax_ir1_opnd, *reg_edx = &edx_ir1_opnd;
    lsassert(ir1_opnd_size(ir1_get_opnd(pir1, 0)) == 64);

    IR2_OPND mem_opnd;
    int imm = 0;
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);

    /* LL vaddr=sign_ext(mem[GR[rj] + sign_ext(si14<<2)]) */
    if (ir1_opnd_simm(opnd0) & 0x3) {
        mem_opnd = convert_mem_no_offset(opnd0);
    } else {
        mem_opnd = convert_mem(opnd0, &imm);
    }
    ir2_set_opnd_type(&mem_opnd, IR2_OPND_GPR);

    /*
     * There is only one parameter from IR1.
     * if EDX:EAX == m64
     *      set ZF and m64 = ECX:EBX
     *  else
     *      clear ZF and EDX:EAX = m64
    */
    IR2_OPND edx_eax_opnd = ra_alloc_itemp();
    IR2_OPND ecx_ebx_opnd = ra_alloc_itemp();
    IR2_OPND src_opnd_0 = ra_alloc_itemp();
    IR2_OPND label_unequal = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

     /*
      * Got EAX/EDX from IR1
      */
    IR2_OPND eax_opnd = ra_alloc_gpr(eax_index);
    IR2_OPND edx_opnd = ra_alloc_gpr(edx_index);
    IR2_OPND ecx_opnd = ra_alloc_gpr(ecx_index);
    IR2_OPND ebx_opnd = ra_alloc_gpr(ebx_index);

     /*
      * Merge EDX:EAX as a 64bit data
      */
    la_or(edx_eax_opnd, edx_opnd, zero_ir2_opnd);
    la_slli_d(edx_eax_opnd, edx_eax_opnd, 32);
    la_bstrins_d(edx_eax_opnd, eax_opnd, 31, 0);

    la_or(ecx_ebx_opnd, ecx_opnd, zero_ir2_opnd);
    la_slli_d(ecx_ebx_opnd, ecx_ebx_opnd, 32);
    la_bstrins_d(ecx_ebx_opnd, ebx_opnd, 31, 0);

    IR2_OPND sc_opnd = ra_alloc_itemp();
    IR2_OPND label_ll = ra_alloc_label();

    if (ir1_is_prefix_lock(pir1)) {
        la_label(label_ll);
        la_or(sc_opnd, zero_ir2_opnd, ecx_ebx_opnd);
        la_ll_d(src_opnd_0, mem_opnd, imm);
        la_bne(src_opnd_0, edx_eax_opnd, label_unequal);
        la_sc_d(sc_opnd, mem_opnd, imm);
        la_beq(sc_opnd, zero_ir2_opnd, label_ll);
    } else {
        la_ld_d(src_opnd_0, mem_opnd, imm);
        la_bne(src_opnd_0, edx_eax_opnd, label_unequal);
        la_st_d(ecx_ebx_opnd, mem_opnd, imm);
    }
    /* equal */
    generate_eflag_calculation(zero_ir2_opnd, zero_ir2_opnd, zero_ir2_opnd,
                                pir1, true);
    la_b(label_exit);
    ra_free_temp(ecx_ebx_opnd);

     /* unequal */
    la_label(label_unequal);
    la_dbar(0);
    generate_eflag_calculation(mem_opnd, mem_opnd, zero_ir2_opnd,
                                pir1, true);
    store_ireg_to_ir1(src_opnd_0, reg_eax, false);
    la_srli_d(src_opnd_0, src_opnd_0, 32);
    store_ireg_to_ir1(src_opnd_0, reg_edx, false);

    la_label(label_exit);
    ra_free_temp(src_opnd_0);

    return true;
}

static bool translate_cmpxchg16b_scq(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    /*
     * Got RAX/RDX/RCX/RBX from IR1
     */
    IR2_OPND rax_opnd = ra_alloc_gpr(eax_index);
    IR2_OPND rdx_opnd = ra_alloc_gpr(edx_index);
    IR2_OPND rcx_opnd = ra_alloc_gpr(ecx_index);
    IR2_OPND rbx_opnd = ra_alloc_gpr(ebx_index);

    /*
     * label
     */
    IR2_OPND label_ll = ra_alloc_label();
    IR2_OPND label_unequal = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    /*
     * itemp
     */
    IR2_OPND sc_opnd = ra_alloc_itemp();
    IR2_OPND mem_lo = ra_alloc_itemp();
    IR2_OPND mem_hi = ra_alloc_itemp();

    la_label(label_ll);
    la_or(sc_opnd, zero_ir2_opnd, rbx_opnd);
    la_ll_d(mem_lo, mem_opnd, 0);
    la_ld_d(mem_hi, mem_opnd, 8);
    la_bne(mem_lo, rax_opnd, label_unequal);
    la_bne(mem_hi, rdx_opnd, label_unequal);
    la_sc_q(sc_opnd, rcx_opnd, mem_opnd);
    la_beq(sc_opnd, zero_ir2_opnd, label_ll);

    /* equal */
    generate_eflag_calculation(zero_ir2_opnd, zero_ir2_opnd, zero_ir2_opnd,
                                pir1, true);
    la_b(label_exit);

    /* unequal */
    la_label(label_unequal);
    la_dbar(0);
    generate_eflag_calculation(mem_opnd, mem_opnd, zero_ir2_opnd,
                                pir1, true);
    la_or(rax_opnd, zero_ir2_opnd, mem_lo);
    la_or(rdx_opnd, zero_ir2_opnd, mem_hi);

    la_label(label_exit);
    ra_free_temp(sc_opnd);
    ra_free_temp(mem_lo);
    ra_free_temp(mem_hi);
    return true;
}

/*
 * Note that CMPXCHG16B requires that the destination (memory)
 * operand be 16-byte aligned.
 */
bool translate_cmpxchg16b(IR1_INST *pir1)
{
    if (have_scq()) {
        return translate_cmpxchg16b_scq(pir1);
    }

    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);

    IR2_OPND zf_opnd = ra_alloc_itemp();

    la_ll_d(zero_ir2_opnd, zero_ir2_opnd, 4);
    la_amswap_d(zf_opnd, zero_ir2_opnd, mem_opnd);

    generate_eflag_calculation(zf_opnd, zf_opnd, zf_opnd,
                                pir1, true);

    ra_free_temp(zf_opnd);
    return true;
}

bool translate_movq(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);
    if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_mem(src)) {
        IR2_OPND temp = load_freg_from_ir1_1(src, false, IS_INTEGER);
        if (option_enable_lasx) {
            la_xvpickve_d(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), temp, 0);
        } else {
            la_vandi_b(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                       ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), 0);
            la_vextrins_d(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), temp, 0);
        }
        return true;
    } else if (ir1_opnd_is_mem(dest) && ir1_opnd_is_xmm(src)) {
        store_freg_to_ir1(ra_alloc_xmm(ir1_opnd_base_reg_num(src)), dest,
                          false, false);
        return true;
    } else if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_xmm(src)) {
        if (option_enable_lasx) {
            la_xvpickve_d(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                          ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 0);
        } else {
            if (ir1_opnd_base_reg_num(dest) == ir1_opnd_base_reg_num(src)) {
                la_vinsgr2vr_d(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                               zero_ir2_opnd, 1);
            } else {
                la_vandi_b(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                           ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), 0);
                la_vextrins_d(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                              ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 0);
            }
        }
        return true;
    }
#ifdef TARGET_X86_64
    else if (CODEIS64) {
        if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_gpr(src)) {
                lsassert(ir1_opnd_size(src) == 64);
                /* gpr -> xmm */

                la_vandi_b(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                           ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), 0);
                la_vinsgr2vr_d(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                              ra_alloc_gpr(ir1_opnd_base_reg_num(src)), 0);

                return true;
            } else if (ir1_opnd_is_xmm(src) && ir1_opnd_is_gpr(dest)) {
                lsassert(ir1_opnd_size(dest) == 64);
                la_vpickve2gr_du(ra_alloc_gpr(ir1_opnd_base_reg_num(dest)),
                                 ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 0);
                return true;
            } else if (ir1_opnd_is_mmx(src) && ir1_opnd_is_gpr(dest)) {
                lsassert(ir1_opnd_size(dest) == 64);
                /* transfer to mmx mode */
                transfer_to_mmx_mode();
                IR2_OPND ir2_src =
                    load_ireg_from_ir1(src, UNKNOWN_EXTENSION, false);
                store_ireg_to_ir1(ir2_src, dest, false);
                return true;
             } else if (ir1_opnd_is_mmx(dest) && ir1_opnd_is_gpr(src)) {
                lsassert(ir1_opnd_size(src) == 64);
                /* transfer to mmx mode */
                transfer_to_mmx_mode();
                IR2_OPND ir2_src =
                    load_ireg_from_ir1(src, UNKNOWN_EXTENSION, false);
                store_ireg_to_ir1(ir2_src, dest, false);
                return true;
             }
    }
#endif
    if (ir1_opnd_is_xmm(dest) || ir1_opnd_is_xmm(src)){
        lsassert(0);
    }
    if (ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0))) { /* dest xmm */
        IR2_OPND dest_lo = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, false);
        load_freg_from_ir1_2(dest_lo, ir1_get_opnd(pir1, 0) + 1, 0);
        store_ireg_to_ir1(zero_ir2_opnd, ir1_get_opnd(pir1, 0), true);
    } else if (ir1_opnd_is_mmx(ir1_get_opnd(pir1, 0))) { /* dest mmx */
        /* transfer to mmx mode */
        transfer_to_mmx_mode();
        IR2_OPND dest_opnd = load_freg_from_ir1_1(ir1_get_opnd(pir1, 0), false, false);
        load_freg_from_ir1_2(dest_opnd, ir1_get_opnd(pir1, 0) + 1, IS_INTEGER);
    } else { /* dest mem */
        IR2_OPND source_opnd =
            load_freg_from_ir1_1(ir1_get_opnd(pir1, 0) + 1, false, false);
        store_freg_to_ir1(source_opnd, ir1_get_opnd(pir1, 0), false, false);
    }
    return true;
}

bool translate_movd(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);
    if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_mem(src)) {
        IR2_OPND temp = load_freg_from_ir1_1(src, false, false);
        if (option_enable_lasx) {
            la_xvpickve_w(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), temp, 0);
        } else {
            la_vandi_b(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                       ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), 0);
            la_vextrins_w(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), temp, 0);
        }
        return true;
    }
    if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_gpr(src)) {
        if (option_enable_lasx) {
            IR2_OPND temp = ra_alloc_ftemp();
            la_movgr2fr_w(temp, ra_alloc_gpr(ir1_opnd_base_reg_num(src)));
            la_xvpickve_w(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), temp, 0);
        } else {
            la_vandi_b(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                       ra_alloc_xmm(ir1_opnd_base_reg_num(dest)), 0);
            la_vinsgr2vr_w(ra_alloc_xmm(ir1_opnd_base_reg_num(dest)),
                          ra_alloc_gpr(ir1_opnd_base_reg_num(src)), 0);
        }
        return true;
    }
    if (ir1_opnd_is_mem(dest) && ir1_opnd_is_xmm(src)) {
        store_freg_to_ir1(ra_alloc_xmm(ir1_opnd_base_reg_num(src)), dest,
                          false, false);
        return true;
    }
    if (ir1_opnd_is_gpr(dest) && ir1_opnd_is_xmm(src)) {
        la_movfr2gr_s(ra_alloc_gpr(ir1_opnd_base_reg_num(dest)),
                      ra_alloc_xmm(ir1_opnd_base_reg_num(src)));
#ifdef TARGET_X86_64
        if (!GHBR_ON(pir1) && CODEIS64) {
            IR2_OPND ir2_dest = ra_alloc_gpr(ir1_opnd_base_reg_num(dest));
            la_mov32_zx(ir2_dest, ir2_dest);
        }
#endif
        return true;
    }
    if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_xmm(src)) {
        lsassert(0);
        // no movd xmm,xmm
    }

    if (ir1_opnd_is_gpr(ir1_get_opnd(pir1, 0)) || ir1_opnd_is_mem(ir1_get_opnd(pir1, 0))) {
        lsassert(ir1_opnd_is_mmx(ir1_get_opnd(pir1, 0) + 1) ||
                 ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0) + 1));

        if (ir1_opnd_is_mmx(ir1_get_opnd(pir1, 0) + 1)) {
            /* transfer to mmx mode */
            transfer_to_mmx_mode();
        }

        IR2_OPND src =
            load_ireg_from_ir1(ir1_get_opnd(pir1, 0) + 1, UNKNOWN_EXTENSION, false);
        store_ireg_to_ir1(src, ir1_get_opnd(pir1, 0), false);
    } else if (ir1_opnd_is_gpr(ir1_get_opnd(pir1, 0) + 1) || ir1_opnd_is_mem(ir1_get_opnd(pir1, 0) + 1)) {
        lsassert(ir1_opnd_is_mmx(ir1_get_opnd(pir1, 0)) ||
                 ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)));

        if (ir1_opnd_is_mmx(ir1_get_opnd(pir1, 0))) {
            /* transfer to mmx mode */
            transfer_to_mmx_mode();
        }

        IR2_OPND src =
            load_ireg_from_ir1(ir1_get_opnd(pir1, 0) + 1, ZERO_EXTENSION, false);
        store_ireg_to_ir1(src, ir1_get_opnd(pir1, 0), false);
        if (ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)))
            store_ireg_to_ir1(zero_ir2_opnd, ir1_get_opnd(pir1, 0), true);
    }
    return true;
}

bool translate_pushaw(IR1_INST *pir1) {
    IR2_OPND esp_opnd = ra_alloc_gpr(esp_index);

    int esp_decrement = 2;

    IR1_OPND mem_ir1_opnd;
    for (int i = 0; i < 8; i++) {
        ir1_opnd_build_mem(&mem_ir1_opnd, esp_decrement << 3, dt_X86_REG_ESP,
                           -esp_decrement * (i + 1));
        IR2_OPND src_opnd = ra_alloc_gpr(i);
        store_ireg_to_ir1(src_opnd, &mem_ir1_opnd, false);
    }
    la_addi_addrx(esp_opnd, esp_opnd, -esp_decrement * 8);

    return true;
}

bool translate_pushal(IR1_INST *pir1) {
    IR2_OPND esp_opnd = ra_alloc_gpr(esp_index);

    int esp_decrement = 4;

    IR1_OPND mem_ir1_opnd;
    for (int i = 0; i < 8; i++) {
        ir1_opnd_build_mem(&mem_ir1_opnd, esp_decrement << 3, dt_X86_REG_ESP,
                           -esp_decrement * (i + 1));
        IR2_OPND src_opnd = ra_alloc_gpr(i);
        store_ireg_to_ir1(src_opnd, &mem_ir1_opnd, false);
    }
    la_addi_addrx(esp_opnd, esp_opnd, -esp_decrement * 8);

    return true;
}

bool translate_popaw(IR1_INST *pir1) {
    IR2_OPND esp_opnd = ra_alloc_gpr(esp_index);

    int esp_increment = 2;

    IR1_OPND mem_ir1_opnd;
    IR2_OPND dst_opnd;

    for (int i = 7; i >= 0; i--) {
        if (i == esp_index) {
            // skip esp
            continue;
        }
        dst_opnd = ra_alloc_gpr(i);
        ir1_opnd_build_mem(&mem_ir1_opnd, esp_increment << 3, dt_X86_REG_ESP,
                           esp_increment * (7 - i));
        load_ireg_from_ir1_2(dst_opnd, &mem_ir1_opnd, UNKNOWN_EXTENSION, false);
    }
    la_addi_addrx(esp_opnd, esp_opnd, esp_increment * 8);

    return true;
}

bool translate_popal(IR1_INST *pir1) {
    IR2_OPND esp_opnd = ra_alloc_gpr(esp_index);

    int esp_increment = 4;

    IR1_OPND mem_ir1_opnd;
    IR2_OPND dst_opnd;

    for (int i = 7; i >= 0; i--) {
        if (i == esp_index) {
            // skip esp
            continue;
        }
        dst_opnd = ra_alloc_gpr(i);
        ir1_opnd_build_mem(&mem_ir1_opnd, esp_increment << 3, dt_X86_REG_ESP,
                           esp_increment * (7 - i));
        load_ireg_from_ir1_2(dst_opnd, &mem_ir1_opnd, UNKNOWN_EXTENSION, false);
    }
    la_addi_addrx(esp_opnd, esp_opnd, esp_increment * 8);

    return true;
}

/*
* int hacker_popcnt(uint32_t n)
* {
* 	n -= (n>>1) & 0x55555555;
* 	n  = (n & 0x33333333) + ((n>>2) & 0x33333333);
* 	n  = ((n>>4) + n) & 0x0F0F0F0F;
* 	n += n>>8;
* 	n += n>>16;
* 	return n & 0x0000003F;
* }
*/
bool translate_popcnt(IR1_INST *pir1) {
    IR2_OPND src_opnd =
        load_ireg_from_ir1(ir1_get_opnd(pir1, 1), ZERO_EXTENSION, false);
    IR2_OPND count = ra_alloc_itemp();
    IR2_OPND temp = ra_alloc_itemp();
    IR2_OPND src_temp = ra_alloc_itemp();
    IR2_OPND pop_count_ir2_opnd = ra_alloc_itemp();
    int pop_count = ir1_opnd_size(ir1_get_opnd(pir1, 0));
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND label_zero = ra_alloc_label();
    IR2_OPND label_start = ra_alloc_label();

    /* set O S Z A C P = 0 */
    la_x86mtflag(zero_ir2_opnd, 0x3f);
    la_ori(count, zero_ir2_opnd, 0);

    la_beq(src_opnd, zero_ir2_opnd, label_zero);

    li_d(pop_count_ir2_opnd, pop_count);

    la_or(src_temp, zero_ir2_opnd, src_opnd);
/* label_start: */
    la_label(label_start);
    la_andi(temp, src_temp, 1);
    la_add_d(count, count, temp);
    la_srli_d(src_temp, src_temp, 1);
    la_addi_d(pop_count_ir2_opnd, pop_count_ir2_opnd, -1);
    la_bne(pop_count_ir2_opnd, zero_ir2_opnd, label_start);

    ra_free_temp(temp);
    /* 5. finish all steps, jump to exit */
    la_b(label_exit);

/* label_zero: */
    la_label(label_zero);
    /* set zf = 1 */
    IR2_OPND n4095_opnd = ra_alloc_num_4095();
    la_x86mtflag(n4095_opnd, 0x8);
    ra_free_num_4095(n4095_opnd);

/* label_exit: */
    la_label(label_exit);
    store_ireg_to_ir1(count, ir1_get_opnd(pir1, 0), false);
    ra_free_temp(count);
    return true;
}
