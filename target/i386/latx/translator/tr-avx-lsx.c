/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "common.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate.h"
#include "hbr.h"
#include "tr-vpaes.h"
#include "pclmul.h"

#ifdef CONFIG_LATX_AVX_OPT

static bool translate_avx_fp_scalar_lsx(IR1_INST *pir1,
                                        IR2_INST *(*tr_inst)(IR2_OPND,
                                                             IR2_OPND,
                                                             IR2_OPND),
                                        bool is_double,
                                        bool track_fp_status);

bool translate_vmulsd_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_scalar_lsx(pir1, la_vfmul_d, true, true);
}

bool translate_vdivsd_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_scalar_lsx(pir1, la_vfdiv_d, true, true);
}

bool translate_vxorpd_lsx(IR1_INST * pir1) {
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);

    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
             (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    {
        /* LSX-only path */
        int dest_index = ir1_opnd_base_reg_num(opnd0);
        int src1_index = ir1_opnd_base_reg_num(opnd1);
        IR2_OPND src1_low = ra_alloc_ftemp();

        /* Materialize both sources before writing a possibly aliased dest. */
        la_vori_b(src1_low, ra_alloc_xmm(src1_index), 0);
        if (ir1_opnd_is_xmm(opnd0)) {
            IR2_OPND src2_low;

            if (ir1_opnd_is_mem(opnd2)) {
                src2_low = load_v128_from_ir1_mem_exact(opnd2);
            } else {
                lsassert(ir1_opnd_is_xmm(opnd2));
                src2_low = ra_alloc_ftemp();
                la_vori_b(src2_low,
                          ra_alloc_xmm(ir1_opnd_base_reg_num(opnd2)), 0);
            }
            la_vxor_v(src1_low, src1_low, src2_low);
            la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
            clear_ymm_high128_shadow(dest_index);
            ra_free_temp(src2_low);
            ra_free_temp(src1_low);
        } else {
            IR2_OPND src1_high;
            IR2_OPND src2_low;
            IR2_OPND src2_high;

            if (ir1_opnd_is_mem(opnd2)) {
                load_v256_from_ir1_mem_exact(opnd2, &src2_low, &src2_high);
            } else {
                int src2_index;

                lsassert(ir1_opnd_is_ymm(opnd2));
                src2_index = ir1_opnd_base_reg_num(opnd2);
                src2_low = ra_alloc_ftemp();
                la_vori_b(src2_low, ra_alloc_xmm(src2_index), 0);
                src2_high = load_ymm_high128_shadow(src2_index);
            }
            src1_high = load_ymm_high128_shadow(src1_index);
            la_vxor_v(src1_low, src1_low, src2_low);
            la_vxor_v(src1_high, src1_high, src2_high);
            la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
            store_ymm_high128_shadow(src1_high, dest_index);
            ra_free_temp(src2_high);
            ra_free_temp(src1_high);
            ra_free_temp(src2_low);
            ra_free_temp(src1_low);
        }
    }
    return true;
}

static IR2_OPND load_broadcast_scalar_lsx(IR1_OPND *opnd, int bits)
{
    if (ir1_opnd_is_mem(opnd)) {
        switch (bits) {
        case 8:
            return load_u8_from_ir1_mem_exact(opnd);
        case 16:
            return load_u16_from_ir1_mem_exact(opnd);
        case 32:
            return load_u32_from_ir1_mem_exact(opnd);
        case 64:
            return load_u64_from_ir1_mem_exact(opnd);
        default:
            lsassert(0);
        }
    }

    IR2_OPND src = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd));
    IR2_OPND value = ra_alloc_itemp();

    switch (bits) {
    case 8:
        la_vpickve2gr_bu(value, src, 0);
        break;
    case 16:
        la_vpickve2gr_hu(value, src, 0);
        break;
    case 32:
        la_vpickve2gr_w(value, src, 0);
        break;
    case 64:
        la_vpickve2gr_du(value, src, 0);
        break;
    default:
        lsassert(0);
    }
    return value;
}

static IR2_OPND replicate_scalar_lsx(IR2_OPND value, int bits)
{
    IR2_OPND result = ra_alloc_ftemp();
    int lanes;

    la_vxor_v(result, result, result);
    if (bits == 64) {
        la_vreplgr2vr_d(result, value);
        return result;
    }

    lanes = 128 / bits;
    for (int i = 0; i < lanes; ++i) {
        switch (bits) {
        case 8:
            la_vinsgr2vr_b(result, value, i);
            break;
        case 16:
            la_vinsgr2vr_h(result, value, i);
            break;
        case 32:
            la_vinsgr2vr_w(result, value, i);
            break;
        default:
            lsassert(0);
        }
    }
    return result;
}

static bool translate_vbroadcast_scalar_lsx(IR1_INST *pir1, int bits)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR2_OPND value = load_broadcast_scalar_lsx(ir1_get_opnd(pir1, 1), bits);
    IR2_OPND result = replicate_scalar_lsx(value, bits);
    int dest_index;

    lsassert(ir1_opnd_is_xmm(dest_opnd) || ir1_opnd_is_ymm(dest_opnd));
    dest_index = ir1_opnd_base_reg_num(dest_opnd);
    la_vori_b(ra_alloc_xmm(dest_index), result, 0);
    if (ir1_opnd_is_xmm(dest_opnd)) {
        clear_ymm_high128_shadow(dest_index);
    } else {
        store_ymm_high128_shadow(result, dest_index);
    }
    ra_free_temp(result);
    ra_free_temp(value);
    return true;
}

static bool translate_vbroadcast128_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    IR2_OPND src;
    int dest_index;

    lsassert(ir1_opnd_is_ymm(dest_opnd));
    lsassert(ir1_opnd_size(src_opnd) == 128);
    src = ir1_opnd_is_mem(src_opnd) ?
        load_v128_from_ir1_mem_exact(src_opnd) :
        ra_alloc_xmm(ir1_opnd_base_reg_num(src_opnd));
    dest_index = ir1_opnd_base_reg_num(dest_opnd);
    la_vori_b(ra_alloc_xmm(dest_index), src, 0);
    store_ymm_high128_shadow(src, dest_index);
    if (ir1_opnd_is_mem(src_opnd))
        ra_free_temp(src);
    return true;
}

bool translate_vbroadcastf128_lsx(IR1_INST *pir1)
{
    return translate_vbroadcast128_lsx(pir1);
}

bool translate_vbroadcasti128_lsx(IR1_INST *pir1)
{
    return translate_vbroadcast128_lsx(pir1);
}

bool translate_vbroadcastsd_lsx(IR1_INST *pir1)
{
    return translate_vbroadcast_scalar_lsx(pir1, 64);
}

bool translate_vbroadcastss_lsx(IR1_INST *pir1)
{
    return translate_vbroadcast_scalar_lsx(pir1, 32);
}

bool translate_vpbroadcastb_lsx(IR1_INST *pir1)
{
    return translate_vbroadcast_scalar_lsx(pir1, 8);
}

bool translate_vpbroadcastw_lsx(IR1_INST *pir1)
{
    return translate_vbroadcast_scalar_lsx(pir1, 16);
}

bool translate_vpbroadcastd_lsx(IR1_INST *pir1)
{
    return translate_vbroadcast_scalar_lsx(pir1, 32);
}

bool translate_vpbroadcastq_lsx(IR1_INST *pir1)
{
    return translate_vbroadcast_scalar_lsx(pir1, 64);
}

static bool translate_vextract128_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    uint8 imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 2)) & 0x1;
    int src_index = ir1_opnd_base_reg_num(src_opnd);
    IR2_OPND src = imm ? load_ymm_high128_shadow(src_index) :
        ra_alloc_xmm(src_index);

    lsassert(ir1_opnd_is_xmm(dest) || ir1_opnd_is_mem(dest));
    lsassert(ir1_opnd_is_ymm(src_opnd) &&
             ir1_opnd_is_imm(ir1_get_opnd(pir1, 2)));
    if (ir1_opnd_is_xmm(dest)) {
        int dest_index = ir1_opnd_base_reg_num(dest);

        la_vori_b(ra_alloc_xmm(dest_index), src, 0);
        clear_ymm_high128_shadow(dest_index);
    } else {
        store_v128_to_ir1_mem_exact(src, dest);
    }
    if (imm)
        ra_free_temp(src);
    return true;
}

bool translate_vextractf128_lsx(IR1_INST *pir1)
{
    return translate_vextract128_lsx(pir1);
}

bool translate_vextracti128_lsx(IR1_INST *pir1)
{
    return translate_vextract128_lsx(pir1);
}

bool translate_vextractps_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    uint8 imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 2));
    IR2_OPND src;
    IR2_OPND value = ra_alloc_itemp();

    lsassert(ir1_opnd_is_gpr(dest) || ir1_opnd_is_mem(dest));
    lsassert(ir1_opnd_is_xmm(src_opnd) || ir1_opnd_is_ymm(src_opnd));
    lsassert(ir1_opnd_is_imm(ir1_get_opnd(pir1, 2)));
    if (ir1_opnd_is_ymm(src_opnd) && (imm & 0x4)) {
        src = load_ymm_high128_shadow(ir1_opnd_base_reg_num(src_opnd));
    } else {
        src = ra_alloc_xmm(ir1_opnd_base_reg_num(src_opnd));
    }
    la_vpickve2gr_w(value, src, imm & 0x3);
    if (ir1_opnd_is_gpr(dest)) {
        store_ireg_to_ir1(value, dest, false);
    } else {
        store_u32_to_ir1_mem_exact(value, dest);
    }
    ra_free_temp(value);
    if (ir1_opnd_is_ymm(src_opnd) && (imm & 0x4))
        ra_free_temp(src);
    return true;
}

bool translate_vinsertps_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src1_opnd = ir1_get_opnd(pir1, 1);
    IR1_OPND *src2_opnd = ir1_get_opnd(pir1, 2);
    uint8 imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
    uint8 source_lane = (imm >> 6) & 0x3;
    uint8 dest_lane = (imm >> 4) & 0x3;
    uint8 zero_mask = imm & 0xf;
    IR2_OPND src1;
    IR2_OPND value;
    IR2_OPND result;
    int dest_index;

    lsassert(ir1_opnd_is_xmm(dest_opnd) && ir1_opnd_is_xmm(src1_opnd));
    lsassert(ir1_opnd_is_xmm(src2_opnd) ||
             (ir1_opnd_is_mem(src2_opnd) && ir1_opnd_size(src2_opnd) == 32));
    lsassert(ir1_opnd_is_imm(ir1_get_opnd(pir1, 3)));
    src1 = ra_alloc_xmm(ir1_opnd_base_reg_num(src1_opnd));
    if (ir1_opnd_is_mem(src2_opnd)) {
        value = load_u32_from_ir1_mem_exact(src2_opnd);
        source_lane = 0;
    } else {
        value = ra_alloc_itemp();
        la_vpickve2gr_w(value, ra_alloc_xmm(
            ir1_opnd_base_reg_num(src2_opnd)), source_lane);
    }
    result = ra_alloc_ftemp();
    la_vori_b(result, src1, 0);
    la_vinsgr2vr_w(result, value, dest_lane);
    for (int i = 0; i < 4; ++i) {
        if (zero_mask & (1 << i)) {
            la_vinsgr2vr_w(result, zero_ir2_opnd, i);
        }
    }
    dest_index = ir1_opnd_base_reg_num(dest_opnd);
    la_vori_b(ra_alloc_xmm(dest_index), result, 0);
    clear_ymm_high128_shadow(dest_index);
    ra_free_temp(result);
    ra_free_temp(value);
    return true;
}

static bool translate_vinsert128_lsx(IR1_INST *pir1)
{
    lsassert(ir1_opnd_num(pir1) == 4);
    lsassert(ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)) &&
        ir1_opnd_is_ymm(ir1_get_opnd(pir1, 1)) &&
        ir1_opnd_size(ir1_get_opnd(pir1, 2)) == 128 &&
        ir1_opnd_is_imm(ir1_get_opnd(pir1, 3)));
    {
        /* LSX-only path */
        IR1_OPND *dest = ir1_get_opnd(pir1, 0);
        IR1_OPND *src1 = ir1_get_opnd(pir1, 1);
        IR1_OPND *src2 = ir1_get_opnd(pir1, 2);
        int dest_index = ir1_opnd_base_reg_num(dest);
        int src1_index = ir1_opnd_base_reg_num(src1);
        uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3)) & 1;
        IR2_OPND src1_low = ra_alloc_ftemp();
        IR2_OPND src1_high = load_ymm_high128_shadow(src1_index);
        IR2_OPND src2_value;

        la_vori_b(src1_low, ra_alloc_xmm(src1_index), 0);
        if (ir1_opnd_is_mem(src2)) {
            src2_value = load_v128_from_ir1_mem_exact(src2);
        } else {
            src2_value = ra_alloc_ftemp();
            la_vori_b(src2_value,
                      ra_alloc_xmm(ir1_opnd_base_reg_num(src2)), 0);
        }

        if (imm == 0) {
            la_vori_b(ra_alloc_xmm(dest_index), src2_value, 0);
            store_ymm_high128_shadow(src1_high, dest_index);
        } else {
            la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
            store_ymm_high128_shadow(src2_value, dest_index);
        }
        ra_free_temp(src2_value);
        ra_free_temp(src1_high);
        ra_free_temp(src1_low);
    }
    return true;
}

bool translate_vinsertf128_lsx(IR1_INST *pir1)
{
    return translate_vinsert128_lsx(pir1);
}

bool translate_vinserti128_lsx(IR1_INST *pir1)
{
    return translate_vinsert128_lsx(pir1);
}

typedef IR2_INST *(*latx_avx_integer_3op_lsx_fn)(IR2_OPND, IR2_OPND,
                                                 IR2_OPND);

static bool translate_avx_integer_3op_lsx(
    IR1_INST *pir1, latx_avx_integer_3op_lsx_fn lsx_op)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int dest_index = ir1_opnd_base_reg_num(opnd0);
    int src1_index = ir1_opnd_base_reg_num(opnd1);
    IR2_OPND src1_low = ra_alloc_ftemp();
    IR2_OPND src2_low;

    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
             (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    lsassert(ir1_opnd_size(opnd0) == ir1_opnd_size(opnd1));
    lsassert(ir1_opnd_size(opnd0) == ir1_opnd_size(opnd2));

    la_vori_b(src1_low, ra_alloc_xmm(src1_index), 0);
    if (ir1_opnd_is_mem(opnd2)) {
        if (ir1_opnd_is_ymm(opnd0)) {
            IR2_OPND src1_high = load_ymm_high128_shadow(src1_index);
            IR2_OPND src2_high;

            load_v256_from_ir1_mem_exact(opnd2, &src2_low, &src2_high);
            lsx_op(src1_low, src1_low, src2_low);
            lsx_op(src1_high, src1_high, src2_high);
            la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
            store_ymm_high128_shadow(src1_high, dest_index);
            ra_free_temp(src1_high);
            ra_free_temp(src2_low);
            ra_free_temp(src2_high);
        } else {
            src2_low = load_v128_from_ir1_mem_exact(opnd2);
            lsx_op(src1_low, src1_low, src2_low);
            la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
            clear_ymm_high128_shadow(dest_index);
            ra_free_temp(src2_low);
        }
    } else {
        int src2_index = ir1_opnd_base_reg_num(opnd2);

        lsassert(ir1_opnd_is_xmm(opnd2) || ir1_opnd_is_ymm(opnd2));
        src2_low = ra_alloc_ftemp();
        la_vori_b(src2_low, ra_alloc_xmm(src2_index), 0);
        if (ir1_opnd_is_ymm(opnd0)) {
            IR2_OPND src1_high = load_ymm_high128_shadow(src1_index);
            IR2_OPND src2_high = load_ymm_high128_shadow(src2_index);

            lsx_op(src1_low, src1_low, src2_low);
            lsx_op(src1_high, src1_high, src2_high);
            la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
            store_ymm_high128_shadow(src1_high, dest_index);
            ra_free_temp(src1_high);
            ra_free_temp(src2_low);
            ra_free_temp(src2_high);
        } else {
            lsx_op(src1_low, src1_low, src2_low);
            la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
            clear_ymm_high128_shadow(dest_index);
            ra_free_temp(src2_low);
        }
    }

    ra_free_temp(src1_low);

    return true;
}

#define LATX_AVX_INTEGER_3OP_LSX_DEFINE(opcode, name, lsx_op) \
bool translate_v##name##_lsx(IR1_INST *pir1) \
{ \
    return translate_avx_integer_3op_lsx(pir1, lsx_op); \
}
LATX_AVX_INTEGER_3OP_LSX_TABLE(LATX_AVX_INTEGER_3OP_LSX_DEFINE)
#undef LATX_AVX_INTEGER_3OP_LSX_DEFINE

#define LATX_AVX_INTEGER_REMAINING_3OP_LSX_DEFINE(opcode, name, lsx_op) \
bool translate_v##name##_lsx(IR1_INST *pir1) \
{ \
    return translate_avx_integer_3op_lsx(pir1, lsx_op); \
}
LATX_AVX_INTEGER_REMAINING_3OP_LSX_TABLE(
    LATX_AVX_INTEGER_REMAINING_3OP_LSX_DEFINE)
#undef LATX_AVX_INTEGER_REMAINING_3OP_LSX_DEFINE

static IR2_INST *translate_vpsignb_lane_lsx(IR2_OPND dest, IR2_OPND src1,
                                            IR2_OPND src2)
{
    return la_vsigncov_b(dest, src2, src1);
}

static IR2_INST *translate_vpsignd_lane_lsx(IR2_OPND dest, IR2_OPND src1,
                                            IR2_OPND src2)
{
    return la_vsigncov_w(dest, src2, src1);
}

static IR2_INST *translate_vpsignw_lane_lsx(IR2_OPND dest, IR2_OPND src1,
                                            IR2_OPND src2)
{
    return la_vsigncov_h(dest, src2, src1);
}

bool translate_vpsignb_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_lsx(pir1, translate_vpsignb_lane_lsx);
}

bool translate_vpsignd_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_lsx(pir1, translate_vpsignd_lane_lsx);
}

bool translate_vpsignw_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_lsx(pir1, translate_vpsignw_lane_lsx);
}

typedef void (*latx_avx_integer_3op_lsx_custom_fn)(IR2_OPND, IR2_OPND,
                                                   IR2_OPND);

static bool translate_avx_integer_3op_custom_lsx(
    IR1_INST *pir1, latx_avx_integer_3op_lsx_custom_fn lsx_op)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int dest_index = ir1_opnd_base_reg_num(opnd0);
    int src1_index = ir1_opnd_base_reg_num(opnd1);
    IR2_OPND src1_low = ra_alloc_ftemp();
    IR2_OPND src2_low;
    IR2_OPND result_low = ra_alloc_ftemp();

    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
             (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    lsassert(ir1_opnd_size(opnd0) == ir1_opnd_size(opnd1));
    lsassert(ir1_opnd_size(opnd0) == ir1_opnd_size(opnd2));

    la_vori_b(src1_low, ra_alloc_xmm(src1_index), 0);
    if (ir1_opnd_is_mem(opnd2)) {
        if (ir1_opnd_is_ymm(opnd0)) {
            src2_low = load_v128_from_ir1_mem_exact(opnd2);
            lsx_op(result_low, src1_low, src2_low);
            la_vori_b(ra_alloc_xmm(dest_index), result_low, 0);
            ra_free_temp(result_low);
            ra_free_temp(src1_low);
            ra_free_temp(src2_low);

            IR2_OPND src1_high = load_ymm_high128_shadow(src1_index);
            IR2_OPND src2_high;
            src2_high = load_v256_high_from_ir1_mem_exact(opnd2);
            IR2_OPND result_high = ra_alloc_ftemp();
            lsx_op(result_high, src1_high, src2_high);
            store_ymm_high128_shadow(result_high, dest_index);
            ra_free_temp(src1_high);
            ra_free_temp(src2_high);
            ra_free_temp(result_high);
        } else {
            src2_low = load_v128_from_ir1_mem_exact(opnd2);
            lsx_op(result_low, src1_low, src2_low);
            la_vori_b(ra_alloc_xmm(dest_index), result_low, 0);
            clear_ymm_high128_shadow(dest_index);
            ra_free_temp(src2_low);
        }
    } else {
        int src2_index = ir1_opnd_base_reg_num(opnd2);

        lsassert(ir1_opnd_is_xmm(opnd2) || ir1_opnd_is_ymm(opnd2));
        src2_low = ra_alloc_ftemp();
        la_vori_b(src2_low, ra_alloc_xmm(src2_index), 0);
        if (ir1_opnd_is_ymm(opnd0)) {
            lsx_op(result_low, src1_low, src2_low);
            la_vori_b(ra_alloc_xmm(dest_index), result_low, 0);
            ra_free_temp(result_low);
            ra_free_temp(src1_low);
            ra_free_temp(src2_low);

            IR2_OPND src1_high = load_ymm_high128_shadow(src1_index);
            IR2_OPND src2_high = load_ymm_high128_shadow(src2_index);
            IR2_OPND result_high = ra_alloc_ftemp();
            lsx_op(result_high, src1_high, src2_high);
            store_ymm_high128_shadow(result_high, dest_index);
            ra_free_temp(src1_high);
            ra_free_temp(src2_high);
            ra_free_temp(result_high);
        } else {
            lsx_op(result_low, src1_low, src2_low);
            la_vori_b(ra_alloc_xmm(dest_index), result_low, 0);
            clear_ymm_high128_shadow(dest_index);
        }
        if (!ir1_opnd_is_ymm(opnd0)) {
            ra_free_temp(src2_low);
        }
    }
    if (!ir1_opnd_is_ymm(opnd0)) {
        ra_free_temp(result_low);
        ra_free_temp(src1_low);
    }
    return true;
}

static void translate_vpmaddwd_lane_lsx(IR2_OPND dest, IR2_OPND src1,
                                        IR2_OPND src2)
{
    if (dest._reg_num != src1._reg_num &&
        dest._reg_num != src2._reg_num) {
        la_vmulwev_w_h(dest, src1, src2);
        la_vmaddwod_w_h(dest, src1, src2);
        return;
    }

    IR2_OPND even = ra_alloc_ftemp();
    IR2_OPND odd = ra_alloc_ftemp();

    la_vmulwev_w_h(even, src1, src2);
    la_vmulwod_w_h(odd, src1, src2);
    la_vadd_w(dest, even, odd);
    ra_free_temp(odd);
    ra_free_temp(even);
}

static void translate_vpmaddubsw_lane_lsx(IR2_OPND dest, IR2_OPND src1,
                                          IR2_OPND src2)
{
    IR2_OPND even = ra_alloc_ftemp();
    IR2_OPND odd = ra_alloc_ftemp();

    /* Unsigned src1 multiplied by signed src2, then saturated to halfwords. */
    la_vmulwev_h_bu_b(even, src1, src2);
    la_vmulwod_h_bu_b(odd, src1, src2);
    la_vsadd_h(dest, even, odd);
    ra_free_temp(odd);
    ra_free_temp(even);
}

static void translate_vpmulhrsw_lane_lsx(IR2_OPND dest, IR2_OPND src1,
                                          IR2_OPND src2)
{
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    IR2_OPND temp3 = ra_alloc_ftemp();

    la_vmulwev_w_h(temp1, src1, src2);
    la_vmulwod_w_h(temp2, src1, src2);
    la_vsrai_w(temp1, temp1, 0xe);
    la_vsrai_w(temp2, temp2, 0xe);
    la_vxor_v(temp3, temp3, temp3);
    la_vandi_b(temp3, temp3, 0);
    la_vbitseti_w(temp3, temp3, 0);
    la_vadd_w(temp1, temp1, temp3);
    la_vadd_w(temp2, temp2, temp3);
    la_vsrai_w(temp1, temp1, 1);
    la_vsrai_w(temp2, temp2, 1);
    la_vpackev_h(dest, temp2, temp1);
    ra_free_temp(temp3);
    ra_free_temp(temp2);
    ra_free_temp(temp1);
}

static void translate_vphaddw_lane_lsx(IR2_OPND dest, IR2_OPND src1,
                                        IR2_OPND src2)
{
    IR2_OPND even = ra_alloc_ftemp();
    IR2_OPND odd = ra_alloc_ftemp();

    la_vpickev_h(even, src2, src1);
    la_vpickod_h(odd, src2, src1);
    la_vadd_h(dest, even, odd);
    ra_free_temp(odd);
    ra_free_temp(even);
}

static void translate_vphaddd_lane_lsx(IR2_OPND dest, IR2_OPND src1,
                                        IR2_OPND src2)
{
    IR2_OPND even = ra_alloc_ftemp();
    IR2_OPND odd = ra_alloc_ftemp();

    la_vpickev_w(even, src2, src1);
    la_vpickod_w(odd, src2, src1);
    la_vadd_w(dest, even, odd);
    ra_free_temp(odd);
    ra_free_temp(even);
}

static void translate_vphaddsw_lane_lsx(IR2_OPND dest, IR2_OPND src1,
                                         IR2_OPND src2)
{
    IR2_OPND even = ra_alloc_ftemp();
    IR2_OPND odd = ra_alloc_ftemp();

    la_vpickev_h(even, src2, src1);
    la_vpickod_h(odd, src2, src1);
    la_vsadd_h(dest, even, odd);
    ra_free_temp(odd);
    ra_free_temp(even);
}

static void translate_vphsubw_lane_lsx(IR2_OPND dest, IR2_OPND src1,
                                        IR2_OPND src2)
{
    IR2_OPND even = ra_alloc_ftemp();
    IR2_OPND odd = ra_alloc_ftemp();

    la_vpickev_h(even, src2, src1);
    la_vpickod_h(odd, src2, src1);
    la_vsub_h(dest, even, odd);
    ra_free_temp(odd);
    ra_free_temp(even);
}

static void translate_vphsubd_lane_lsx(IR2_OPND dest, IR2_OPND src1,
                                        IR2_OPND src2)
{
    IR2_OPND even = ra_alloc_ftemp();
    IR2_OPND odd = ra_alloc_ftemp();

    la_vpickev_w(even, src2, src1);
    la_vpickod_w(odd, src2, src1);
    la_vsub_w(dest, even, odd);
    ra_free_temp(odd);
    ra_free_temp(even);
}

static void translate_vphsubsw_lane_lsx(IR2_OPND dest, IR2_OPND src1,
                                         IR2_OPND src2)
{
    IR2_OPND even = ra_alloc_ftemp();
    IR2_OPND odd = ra_alloc_ftemp();

    la_vpickev_h(even, src2, src1);
    la_vpickod_h(odd, src2, src1);
    la_vssub_h(dest, even, odd);
    ra_free_temp(odd);
    ra_free_temp(even);
}

bool translate_vpmaddwd_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_custom_lsx(
        pir1, translate_vpmaddwd_lane_lsx);
}

bool translate_vpmaddubsw_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_custom_lsx(
        pir1, translate_vpmaddubsw_lane_lsx);
}

bool translate_vpmulhrsw_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_custom_lsx(
        pir1, translate_vpmulhrsw_lane_lsx);
}

bool translate_vphaddw_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_custom_lsx(
        pir1, translate_vphaddw_lane_lsx);
}

bool translate_vphaddd_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_custom_lsx(
        pir1, translate_vphaddd_lane_lsx);
}

bool translate_vphaddsw_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_custom_lsx(
        pir1, translate_vphaddsw_lane_lsx);
}

bool translate_vphsubw_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_custom_lsx(
        pir1, translate_vphsubw_lane_lsx);
}

bool translate_vphsubd_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_custom_lsx(
        pir1, translate_vphsubd_lane_lsx);
}

bool translate_vphsubsw_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_custom_lsx(
        pir1, translate_vphsubsw_lane_lsx);
}

static void translate_vpsadbw_lane_lsx(IR2_OPND dest, IR2_OPND src1,
                                        IR2_OPND src2)
{
    IR2_OPND temp = ra_alloc_ftemp();

    la_vabsd_bu(temp, src1, src2);
    la_vhaddw_hu_bu(temp, temp, temp);
    la_vhaddw_wu_hu(temp, temp, temp);
    la_vhaddw_du_wu(dest, temp, temp);
    ra_free_temp(temp);
}

bool translate_vpsadbw_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_custom_lsx(
        pir1, translate_vpsadbw_lane_lsx);
}

typedef IR2_INST *(*latx_vpmovx_extend_lsx_fn)(IR2_OPND, IR2_OPND);

static void translate_vpmovx_extend_lane_lsx(IR2_OPND dest, IR2_OPND src,
                                             bool is_unsigned, int src_bits,
                                             int dest_bits)
{
    latx_vpmovx_extend_lsx_fn extend_h;
    latx_vpmovx_extend_lsx_fn extend_w;
    latx_vpmovx_extend_lsx_fn extend_d;
    IR2_OPND temp = ra_alloc_ftemp();

    extend_h = is_unsigned ? la_vexth_hu_bu : la_vexth_h_b;
    extend_w = is_unsigned ? la_vexth_wu_hu : la_vexth_w_h;
    extend_d = is_unsigned ? la_vexth_du_wu : la_vexth_d_w;
    la_vori_b(temp, src, 0);
    if (src_bits == 8) {
        extend_h(temp, temp);
    } else if (src_bits == 16) {
        extend_w(temp, temp);
    } else {
        extend_d(temp, temp);
    }
    if (dest_bits == 16) {
        la_vori_b(dest, temp, 0);
    } else if (dest_bits == 32) {
        if (src_bits == 8) {
            extend_w(temp, temp);
        }
        la_vori_b(dest, temp, 0);
    } else {
        if (src_bits == 8) {
            extend_w(temp, temp);
        }
        if (src_bits != 32) {
            extend_d(temp, temp);
        }
        la_vori_b(dest, temp, 0);
    }
    ra_free_temp(temp);
}

static IR2_OPND load_vpmovx_source_lsx(IR1_OPND *src_opnd, int source_bytes)
{
    if (!ir1_opnd_is_mem(src_opnd)) {
        IR2_OPND src = ra_alloc_ftemp();
        la_vori_b(src, ra_alloc_xmm(ir1_opnd_base_reg_num(src_opnd)), 0);
        return src;
    }

    if (source_bytes == 16) {
        return load_v128_from_ir1_mem_exact(src_opnd);
    }
    IR2_OPND src = ra_alloc_ftemp();
    la_vxor_v(src, src, src);
    switch (source_bytes) {
    case 8:
        la_vinsgr2vr_d(src, load_u64_from_ir1_mem_exact(src_opnd), 0);
        break;
    case 4:
        la_vinsgr2vr_w(src, load_u32_from_ir1_mem_exact(src_opnd), 0);
        break;
    case 2:
        la_vinsgr2vr_h(src, load_u16_from_ir1_mem_exact(src_opnd), 0);
        break;
    default:
        lsassert(0);
    }
    return src;
}

static bool translate_vpmovx_lsx(IR1_INST *pir1, bool is_unsigned,
                                 int src_bits, int dest_bits)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    int source_bytes = (128 / dest_bits) * (src_bits / 8);
    IR2_OPND src;
    IR2_OPND low_src = ra_alloc_ftemp();
    IR2_OPND low_result = ra_alloc_ftemp();
    int dest_index = ir1_opnd_base_reg_num(dest_opnd);

    lsassert(ir1_opnd_is_xmm(dest_opnd) || ir1_opnd_is_ymm(dest_opnd));
    lsassert(ir1_opnd_is_xmm(src_opnd) || ir1_opnd_is_mem(src_opnd));
    src = load_vpmovx_source_lsx(
        src_opnd, source_bytes * (ir1_opnd_is_ymm(dest_opnd) ? 2 : 1));
    la_vbsll_v(low_src, src, 16 - source_bytes);
    translate_vpmovx_extend_lane_lsx(low_result, low_src, is_unsigned,
                                      src_bits, dest_bits);
    la_vori_b(ra_alloc_xmm(dest_index), low_result, 0);
    if (ir1_opnd_is_ymm(dest_opnd)) {
        IR2_OPND high_src = ra_alloc_ftemp();
        IR2_OPND high_result = ra_alloc_ftemp();

        la_vbsll_v(high_src, src, 16 - 2 * source_bytes);
        translate_vpmovx_extend_lane_lsx(high_result, high_src, is_unsigned,
                                          src_bits, dest_bits);
        store_ymm_high128_shadow(high_result, dest_index);
        ra_free_temp(high_result);
        ra_free_temp(high_src);
    } else {
        clear_ymm_high128_shadow(dest_index);
    }
    ra_free_temp(low_result);
    ra_free_temp(low_src);
    ra_free_temp(src);
    return true;
}

bool translate_vpmovsxxx_lsx(IR1_INST *pir1)
{
    switch (ir1_opcode(pir1)) {
    case dt_X86_INS_VPMOVSXBW:
        return translate_vpmovx_lsx(pir1, false, 8, 16);
    case dt_X86_INS_VPMOVSXBD:
        return translate_vpmovx_lsx(pir1, false, 8, 32);
    case dt_X86_INS_VPMOVSXBQ:
        return translate_vpmovx_lsx(pir1, false, 8, 64);
    case dt_X86_INS_VPMOVSXWD:
        return translate_vpmovx_lsx(pir1, false, 16, 32);
    case dt_X86_INS_VPMOVSXWQ:
        return translate_vpmovx_lsx(pir1, false, 16, 64);
    case dt_X86_INS_VPMOVSXDQ:
        return translate_vpmovx_lsx(pir1, false, 32, 64);
    default:
        lsassert(0);
        return false;
    }
}

bool translate_vpmovzxxx_lsx(IR1_INST *pir1)
{
    switch (ir1_opcode(pir1)) {
    case dt_X86_INS_VPMOVZXBW:
        return translate_vpmovx_lsx(pir1, true, 8, 16);
    case dt_X86_INS_VPMOVZXBD:
        return translate_vpmovx_lsx(pir1, true, 8, 32);
    case dt_X86_INS_VPMOVZXBQ:
        return translate_vpmovx_lsx(pir1, true, 8, 64);
    case dt_X86_INS_VPMOVZXWD:
        return translate_vpmovx_lsx(pir1, true, 16, 32);
    case dt_X86_INS_VPMOVZXWQ:
        return translate_vpmovx_lsx(pir1, true, 16, 64);
    case dt_X86_INS_VPMOVZXDQ:
        return translate_vpmovx_lsx(pir1, true, 32, 64);
    default:
        lsassert(0);
        return false;
    }
}

bool translate_vphminposuw_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    bool src_is_temp = ir1_opnd_is_mem(src_opnd);
    IR2_OPND src = src_is_temp ?
        load_v128_from_ir1_mem_exact(src_opnd) :
        ra_alloc_xmm(ir1_opnd_base_reg_num(src_opnd));
    IR2_OPND min = ra_alloc_itemp();
    IR2_OPND value = ra_alloc_itemp();
    IR2_OPND index = ra_alloc_itemp();
    IR2_OPND result = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_xmm(dest_opnd));
    la_vpickve2gr_hu(min, src, 0);
    li_wu(index, 0);
    for (int i = 1; i < 8; ++i) {
        IR2_OPND keep = ra_alloc_label();

        la_vpickve2gr_hu(value, src, i);
        la_bgeu(value, min, keep);
        la_or(min, value, zero_ir2_opnd);
        li_wu(index, i);
        la_label(keep);
    }
    la_vxor_v(result, result, result);
    la_vinsgr2vr_h(result, min, 0);
    la_vinsgr2vr_h(result, index, 1);
    la_vori_b(ra_alloc_xmm(ir1_opnd_base_reg_num(dest_opnd)), result, 0);
    clear_ymm_high128_shadow(ir1_opnd_base_reg_num(dest_opnd));
    ra_free_temp(result);
    ra_free_temp(index);
    ra_free_temp(value);
    ra_free_temp(min);
    if (src_is_temp) {
        ra_free_temp(src);
    }
    return true;
}

bool translate_vpabsx_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    IR2_INST *(*abs_op)(IR2_OPND, IR2_OPND, IR2_OPND);
    IR2_OPND src_low;
    IR2_OPND zero_low = ra_alloc_ftemp();
    int dest_index = ir1_opnd_base_reg_num(dest_opnd);

    lsassert(ir1_opnd_is_xmm(dest_opnd) || ir1_opnd_is_ymm(dest_opnd));
    lsassert(ir1_opnd_is_mem(src_opnd) || ir1_opnd_is_xmm(src_opnd) ||
             ir1_opnd_is_ymm(src_opnd));
    switch (ir1_opcode(pir1)) {
    case dt_X86_INS_VPABSB:
        abs_op = la_vabsd_b;
        break;
    case dt_X86_INS_VPABSW:
        abs_op = la_vabsd_h;
        break;
    case dt_X86_INS_VPABSD:
        abs_op = la_vabsd_w;
        break;
    default:
        lsassert(0);
        ra_free_temp(zero_low);
        return false;
    }

    la_vxor_v(zero_low, zero_low, zero_low);
    if (ir1_opnd_is_mem(src_opnd)) {
        if (ir1_opnd_is_ymm(dest_opnd)) {
            IR2_OPND src_high;
            IR2_OPND result_high = ra_alloc_ftemp();

            load_v256_from_ir1_mem_exact(src_opnd, &src_low, &src_high);
            abs_op(src_low, src_low, zero_low);
            abs_op(result_high, src_high, zero_low);
            la_vori_b(ra_alloc_xmm(dest_index), src_low, 0);
            store_ymm_high128_shadow(result_high, dest_index);
            ra_free_temp(result_high);
            ra_free_temp(src_high);
            ra_free_temp(src_low);
        } else {
            src_low = load_v128_from_ir1_mem_exact(src_opnd);
            abs_op(src_low, src_low, zero_low);
            la_vori_b(ra_alloc_xmm(dest_index), src_low, 0);
            clear_ymm_high128_shadow(dest_index);
            ra_free_temp(src_low);
        }
    } else {
        int src_index = ir1_opnd_base_reg_num(src_opnd);
        IR2_OPND src_high;

        src_low = ra_alloc_ftemp();
        la_vori_b(src_low, ra_alloc_xmm(src_index), 0);
        abs_op(src_low, src_low, zero_low);
        la_vori_b(ra_alloc_xmm(dest_index), src_low, 0);
        if (ir1_opnd_is_ymm(dest_opnd)) {
            src_high = load_ymm_high128_shadow(src_index);
            abs_op(src_high, src_high, zero_low);
            store_ymm_high128_shadow(src_high, dest_index);
            ra_free_temp(src_high);
        } else {
            clear_ymm_high128_shadow(dest_index);
        }
        ra_free_temp(src_low);
    }
    ra_free_temp(zero_low);
    return true;
}

bool translate_vpand_lsx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    lsassert(ir1_opnd_size(opnd0) == ir1_opnd_size(opnd2));

    {
        /* LSX-only path */
        int dest_index = ir1_opnd_base_reg_num(opnd0);
        int src1_index = ir1_opnd_base_reg_num(opnd1);
        IR2_OPND src1_low = ra_alloc_ftemp();
        IR2_OPND src2_low;

        la_vori_b(src1_low, ra_alloc_xmm(src1_index), 0);
        if (ir1_opnd_is_mem(opnd2)) {
            if (ir1_opnd_is_ymm(opnd0)) {
                IR2_OPND src1_high = load_ymm_high128_shadow(src1_index);
                IR2_OPND src2_high;

                load_v256_from_ir1_mem_exact(opnd2, &src2_low, &src2_high);
                la_vand_v(src1_low, src1_low, src2_low);
                la_vand_v(src1_high, src1_high, src2_high);
                la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
                store_ymm_high128_shadow(src1_high, dest_index);
            } else {
                src2_low = load_v128_from_ir1_mem_exact(opnd2);
                la_vand_v(src1_low, src1_low, src2_low);
                la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
                clear_ymm_high128_shadow(dest_index);
            }
        } else {
            int src2_index = ir1_opnd_base_reg_num(opnd2);

            src2_low = ra_alloc_ftemp();
            la_vori_b(src2_low, ra_alloc_xmm(src2_index), 0);
            if (ir1_opnd_is_ymm(opnd0)) {
                IR2_OPND src1_high = load_ymm_high128_shadow(src1_index);
                IR2_OPND src2_high = load_ymm_high128_shadow(src2_index);

                la_vand_v(src1_low, src1_low, src2_low);
                la_vand_v(src1_high, src1_high, src2_high);
                la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
                store_ymm_high128_shadow(src1_high, dest_index);
            } else {
                la_vand_v(src1_low, src1_low, src2_low);
                la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
                clear_ymm_high128_shadow(dest_index);
            }
        }
    }
    return true;
}

bool translate_vpandn_lsx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    lsassert(ir1_opnd_size(opnd0) == ir1_opnd_size(opnd2));

    int dest_index = ir1_opnd_base_reg_num(opnd0);
    int src1_index = ir1_opnd_base_reg_num(opnd1);
    IR2_OPND src1_low = ra_alloc_ftemp();
    IR2_OPND src2_low;

    la_vori_b(src1_low, ra_alloc_xmm(src1_index), 0);
    if (ir1_opnd_is_mem(opnd2)) {
        if (ir1_opnd_is_ymm(opnd0)) {
            IR2_OPND src1_high = load_ymm_high128_shadow(src1_index);
            IR2_OPND src2_high;

            load_v256_from_ir1_mem_exact(opnd2, &src2_low, &src2_high);
            la_vandn_v(src1_low, src1_low, src2_low);
            la_vandn_v(src1_high, src1_high, src2_high);
            la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
            store_ymm_high128_shadow(src1_high, dest_index);
        } else {
            src2_low = load_v128_from_ir1_mem_exact(opnd2);
            la_vandn_v(src1_low, src1_low, src2_low);
            la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
            clear_ymm_high128_shadow(dest_index);
        }
    } else {
        int src2_index = ir1_opnd_base_reg_num(opnd2);

        src2_low = ra_alloc_ftemp();
        la_vori_b(src2_low, ra_alloc_xmm(src2_index), 0);
        if (ir1_opnd_is_ymm(opnd0)) {
            IR2_OPND src1_high = load_ymm_high128_shadow(src1_index);
            IR2_OPND src2_high = load_ymm_high128_shadow(src2_index);

            la_vandn_v(src1_low, src1_low, src2_low);
            la_vandn_v(src1_high, src1_high, src2_high);
            la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
            store_ymm_high128_shadow(src1_high, dest_index);
        } else {
            la_vandn_v(src1_low, src1_low, src2_low);
            la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
            clear_ymm_high128_shadow(dest_index);
        }
    }
    return true;
}

bool translate_vpblendvb_lsx(IR1_INST * pir1) {
    lsassert(ir1_opnd_num(pir1) == 4);
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND * opnd3 = ir1_get_opnd(pir1, 3);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    lsassert(ir1_opnd_size(opnd0) == ir1_opnd_size(opnd2));
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd3)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd3)));
    lsassert(ir1_opnd_is_mem(opnd2) || ir1_opnd_is_xmm(opnd2) ||
        ir1_opnd_is_ymm(opnd2));

    {
        /* LSX-only path */
        int dest_index = ir1_opnd_base_reg_num(opnd0);
        int src1_index = ir1_opnd_base_reg_num(opnd1);
        int mask_index = ir1_opnd_base_reg_num(opnd3);
        bool is_ymm = ir1_opnd_is_ymm(opnd0);
        IR2_OPND src1_low = ra_alloc_ftemp();
        IR2_OPND src2_low;
        IR2_OPND mask_low = ra_alloc_ftemp();
        IR2_OPND src1_high = { 0 };
        IR2_OPND src2_high = { 0 };
        IR2_OPND mask_high = { 0 };

        /* Preserve every register source before writing an aliased dest. */
        la_vori_b(src1_low, ra_alloc_xmm(src1_index), 0);
        la_vori_b(mask_low, ra_alloc_xmm(mask_index), 0);
        if (is_ymm) {
            src1_high = load_ymm_high128_shadow(src1_index);
            mask_high = load_ymm_high128_shadow(mask_index);
        }

        if (ir1_opnd_is_mem(opnd2)) {
            if (is_ymm) {
                load_v256_from_ir1_mem_exact(opnd2, &src2_low, &src2_high);
            } else {
                src2_low = load_v128_from_ir1_mem_exact(opnd2);
            }
        } else {
            int src2_index = ir1_opnd_base_reg_num(opnd2);

            src2_low = ra_alloc_ftemp();
            la_vori_b(src2_low, ra_alloc_xmm(src2_index), 0);
            if (is_ymm) {
                src2_high = load_ymm_high128_shadow(src2_index);
            }
        }

        /* A negative signed byte means its high bit selects src2. */
        la_vslti_b(mask_low, mask_low, 0);
        la_vbitsel_v(src1_low, src1_low, src2_low, mask_low);
        if (is_ymm) {
            la_vslti_b(mask_high, mask_high, 0);
            la_vbitsel_v(src1_high, src1_high, src2_high, mask_high);
        }

        la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
        if (is_ymm) {
            store_ymm_high128_shadow(src1_high, dest_index);
        } else {
            clear_ymm_high128_shadow(dest_index);
        }
    }
    return true;
}

bool translate_vpextrx_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    uint8 imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 2));
    IR2_OPND src;
    IR2_OPND value = ra_alloc_itemp();
    bool is_reg = ir1_opnd_is_gpr(dest);

    lsassert(ir1_opnd_is_xmm(src_opnd));
    lsassert(is_reg || ir1_opnd_is_mem(dest));
    lsassert(ir1_opnd_is_imm(ir1_get_opnd(pir1, 2)));
    src = ra_alloc_xmm(ir1_opnd_base_reg_num(src_opnd));
    switch (ir1_opcode(pir1)) {
    case dt_X86_INS_VPEXTRB:
        imm &= 0xf;
        la_vpickve2gr_bu(value, src, imm);
        if (is_reg) {
            store_ireg_to_ir1(value, dest, false);
        } else {
            store_u8_to_ir1_mem_exact(value, dest);
        }
        break;
    case dt_X86_INS_VPEXTRW:
        imm &= 0x7;
        la_vpickve2gr_hu(value, src, imm);
        if (is_reg) {
            store_ireg_to_ir1(value, dest, false);
        } else {
            store_u16_to_ir1_mem_exact(value, dest);
        }
        break;
    case dt_X86_INS_VPEXTRD:
        imm &= 0x3;
        la_vpickve2gr_wu(value, src, imm);
        if (is_reg) {
            store_ireg_to_ir1(value, dest, false);
        } else {
            store_u32_to_ir1_mem_exact(value, dest);
        }
        break;
    case dt_X86_INS_VPEXTRQ:
        imm &= 0x1;
        la_vpickve2gr_du(value, src, imm);
        if (is_reg) {
            store_ireg_to_ir1(value, dest, false);
        } else {
            store_u64_to_ir1_mem_exact(value, dest);
        }
        break;
    default:
        lsassert(0);
    }
    ra_free_temp(value);
    return true;
}

bool translate_vpor_lsx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    lsassert(ir1_opnd_size(opnd0) == ir1_opnd_size(opnd2));

    {
        /* LSX-only path */
        int dest_index = ir1_opnd_base_reg_num(opnd0);
        int src1_index = ir1_opnd_base_reg_num(opnd1);
        IR2_OPND src1_low = ra_alloc_ftemp();
        IR2_OPND src2_low;

        la_vori_b(src1_low, ra_alloc_xmm(src1_index), 0);
        if (ir1_opnd_is_mem(opnd2)) {
            if (ir1_opnd_is_ymm(opnd0)) {
                IR2_OPND src1_high = load_ymm_high128_shadow(src1_index);
                IR2_OPND src2_high;

                load_v256_from_ir1_mem_exact(opnd2, &src2_low, &src2_high);
                la_vor_v(src1_low, src1_low, src2_low);
                la_vor_v(src1_high, src1_high, src2_high);
                la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
                store_ymm_high128_shadow(src1_high, dest_index);
            } else {
                src2_low = load_v128_from_ir1_mem_exact(opnd2);
                la_vor_v(src1_low, src1_low, src2_low);
                la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
                clear_ymm_high128_shadow(dest_index);
            }
        } else {
            int src2_index = ir1_opnd_base_reg_num(opnd2);

            src2_low = ra_alloc_ftemp();
            la_vori_b(src2_low, ra_alloc_xmm(src2_index), 0);
            if (ir1_opnd_is_ymm(opnd0)) {
                IR2_OPND src1_high = load_ymm_high128_shadow(src1_index);
                IR2_OPND src2_high = load_ymm_high128_shadow(src2_index);

                la_vor_v(src1_low, src1_low, src2_low);
                la_vor_v(src1_high, src1_high, src2_high);
                la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
                store_ymm_high128_shadow(src1_high, dest_index);
            } else {
                la_vor_v(src1_low, src1_low, src2_low);
                la_vori_b(ra_alloc_xmm(dest_index), src1_low, 0);
                clear_ymm_high128_shadow(dest_index);
            }
        }
    }
    return true;
}

static void load_avx_lsx_operand(IR1_OPND *opnd, bool ymm,
                                 IR2_OPND *low, IR2_OPND *high);

bool translate_vptest_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    bool ymm = ir1_opnd_is_ymm(dest_opnd);
    IR2_OPND dest_low;
    IR2_OPND dest_high;
    IR2_OPND src_low;
    IR2_OPND src_high;
    IR2_OPND and_result = ra_alloc_ftemp();
    IR2_OPND andn_result = ra_alloc_ftemp();
    IR2_OPND half_result = ra_alloc_ftemp();
    IR2_OPND n4095_opnd = ra_alloc_num_4095();
    IR2_OPND zf_done = ra_alloc_label();
    IR2_OPND cf_done = ra_alloc_label();

    lsassert(ir1_opnd_is_xmm(dest_opnd) || ymm);
    lsassert(ir1_opnd_is_xmm(src_opnd) == !ymm ||
             ir1_opnd_is_ymm(src_opnd) == ymm ||
             ir1_opnd_is_mem(src_opnd));
    load_avx_lsx_operand(dest_opnd, ymm, &dest_low, &dest_high);
    load_avx_lsx_operand(src_opnd, ymm, &src_low, &src_high);

    /* VPTEST sets ZF from dest & src and CF from dest & ~src. */
    la_vand_v(and_result, dest_low, src_low);
    la_vandn_v(andn_result, dest_low, src_low);
    if (ymm) {
        la_vand_v(half_result, dest_high, src_high);
        la_vor_v(and_result, and_result, half_result);
        la_vandn_v(half_result, dest_high, src_high);
        la_vor_v(andn_result, andn_result, half_result);
    }

    /* VPTEST clears OF, SF, AF and PF, then sets only ZF and CF. */
    la_x86mtflag(zero_ir2_opnd, 0x3f);
    la_vseteqz_v(fcc0_ir2_opnd, and_result);
    la_bceqz(fcc0_ir2_opnd, zf_done);
    la_x86mtflag(n4095_opnd, ZF_USEDEF_BIT);
    la_label(zf_done);

    la_vseteqz_v(fcc0_ir2_opnd, andn_result);
    la_bceqz(fcc0_ir2_opnd, cf_done);
    la_x86mtflag(n4095_opnd, CF_USEDEF_BIT);
    la_label(cf_done);

    ra_free_num_4095(n4095_opnd);
    ra_free_temp(half_result);
    ra_free_temp(andn_result);
    ra_free_temp(and_result);
    ra_free_temp(src_low);
    ra_free_temp(dest_low);
    if (ymm) {
        ra_free_temp(src_high);
        ra_free_temp(dest_high);
    }
    return true;
}

static bool translate_avx_vtest_lsx(IR1_INST *pir1, bool double_precision)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    bool ymm = ir1_opnd_is_ymm(dest_opnd);
    IR2_OPND dest_low;
    IR2_OPND dest_high;
    IR2_OPND src_low;
    IR2_OPND src_high;
    IR2_OPND and_result = ra_alloc_ftemp();
    IR2_OPND andn_result = ra_alloc_ftemp();
    IR2_OPND half_result = ra_alloc_ftemp();
    IR2_OPND n4095_opnd = ra_alloc_num_4095();
    IR2_OPND zf_done = ra_alloc_label();
    IR2_OPND cf_done = ra_alloc_label();

    lsassert(ir1_opnd_is_xmm(dest_opnd) || ymm);
    lsassert(ir1_opnd_is_xmm(src_opnd) == !ymm ||
             ir1_opnd_is_ymm(src_opnd) == ymm ||
             ir1_opnd_is_mem(src_opnd));
    load_avx_lsx_operand(dest_opnd, ymm, &dest_low, &dest_high);
    load_avx_lsx_operand(src_opnd, ymm, &src_low, &src_high);

    la_vand_v(and_result, dest_low, src_low);
    la_vandn_v(andn_result, dest_low, src_low);
    if (double_precision) {
        la_vsrli_d(and_result, and_result, 0x3f);
        la_vsrli_d(andn_result, andn_result, 0x3f);
    } else {
        la_vsrli_w(and_result, and_result, 0x1f);
        la_vsrli_w(andn_result, andn_result, 0x1f);
    }
    if (ymm) {
        la_vand_v(half_result, dest_high, src_high);
        if (double_precision) {
            la_vsrli_d(half_result, half_result, 0x3f);
        } else {
            la_vsrli_w(half_result, half_result, 0x1f);
        }
        la_vor_v(and_result, and_result, half_result);

        la_vandn_v(half_result, dest_high, src_high);
        if (double_precision) {
            la_vsrli_d(half_result, half_result, 0x3f);
        } else {
            la_vsrli_w(half_result, half_result, 0x1f);
        }
        la_vor_v(andn_result, andn_result, half_result);
    }

    la_x86mtflag(zero_ir2_opnd, 0x3f);
    la_vseteqz_v(fcc0_ir2_opnd, and_result);
    la_bceqz(fcc0_ir2_opnd, zf_done);
    la_x86mtflag(n4095_opnd, ZF_USEDEF_BIT);
    la_label(zf_done);
    la_vseteqz_v(fcc0_ir2_opnd, andn_result);
    la_bceqz(fcc0_ir2_opnd, cf_done);
    la_x86mtflag(n4095_opnd, CF_USEDEF_BIT);
    la_label(cf_done);

    ra_free_num_4095(n4095_opnd);
    ra_free_temp(half_result);
    ra_free_temp(andn_result);
    ra_free_temp(and_result);
    ra_free_temp(src_low);
    ra_free_temp(dest_low);
    if (ymm) {
        ra_free_temp(src_high);
        ra_free_temp(dest_high);
    }
    return true;
}

bool translate_vtestps_lsx(IR1_INST *pir1)
{
    return translate_avx_vtest_lsx(pir1, false);
}

bool translate_vtestpd_lsx(IR1_INST *pir1)
{
    return translate_avx_vtest_lsx(pir1, true);
}

typedef IR2_INST *(*avx_lsx_lane_3op_fn)(IR2_OPND, IR2_OPND, IR2_OPND);
typedef IR2_INST *(*avx_lsx_narrow_fn)(IR2_OPND, IR2_OPND, int);

static void load_avx_lsx_operand(IR1_OPND *opnd, bool ymm,
                                 IR2_OPND *low, IR2_OPND *high)
{
    lsassert(ir1_opnd_is_mem(opnd) || ir1_opnd_is_xmm(opnd) ||
             ir1_opnd_is_ymm(opnd));
    if (ir1_opnd_is_mem(opnd)) {
        if (ymm) {
            load_v256_from_ir1_mem_exact(opnd, low, high);
        } else {
            *low = load_v128_from_ir1_mem_exact(opnd);
        }
        return;
    }

    *low = ra_alloc_ftemp();
    la_vori_b(*low, ra_alloc_xmm(ir1_opnd_base_reg_num(opnd)), 0);
    if (ymm) {
        *high = load_ymm_high128_shadow(ir1_opnd_base_reg_num(opnd));
    }
}

static IR2_OPND load_avx_lsx_high_operand(IR1_OPND *opnd)
{
    if (ir1_opnd_is_mem(opnd)) {
        IR2_OPND low;
        IR2_OPND high;

        load_v256_from_ir1_mem_exact(opnd, &low, &high);
        ra_free_temp(low);
        return high;
    }

    return load_ymm_high128_shadow(ir1_opnd_base_reg_num(opnd));
}

static void store_avx_lsx_result(IR1_OPND *opnd, IR2_OPND low,
                                 IR2_OPND high)
{
    int dest_index = ir1_opnd_base_reg_num(opnd);

    la_vori_b(ra_alloc_xmm(dest_index), low, 0);
    if (ir1_opnd_is_ymm(opnd)) {
        store_ymm_high128_shadow(high, dest_index);
    } else {
        clear_ymm_high128_shadow(dest_index);
    }
}

static bool translate_avx_lane_3op_lsx(IR1_INST *pir1,
                                       avx_lsx_lane_3op_fn tr_inst)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    bool ymm = ir1_opnd_is_ymm(opnd0);
    IR2_OPND src1_low, src1_high, src2_low, src2_high;
    IR2_OPND result_low = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_xmm(opnd0) || ymm);
    lsassert(ir1_opnd_is_xmm(opnd1) == !ymm ||
             ir1_opnd_is_ymm(opnd1) == ymm);
    lsassert(ir1_opnd_size(opnd0) == ir1_opnd_size(opnd2));
    load_avx_lsx_operand(opnd1, ymm, &src1_low, &src1_high);
    load_avx_lsx_operand(opnd2, ymm, &src2_low, &src2_high);
    tr_inst(result_low, src2_low, src1_low);

    if (ymm) {
        IR2_OPND result_high = ra_alloc_ftemp();
        tr_inst(result_high, src2_high, src1_high);
        store_avx_lsx_result(opnd0, result_low, result_high);
        ra_free_temp(result_high);
        ra_free_temp(src1_high);
        ra_free_temp(src2_high);
    } else {
        store_avx_lsx_result(opnd0, result_low, result_low);
    }
    ra_free_temp(result_low);
    ra_free_temp(src1_low);
    ra_free_temp(src2_low);
    return true;
}

static bool translate_avx_pack_lsx(IR1_INST *pir1,
                                   avx_lsx_narrow_fn cvt_inst,
                                   avx_lsx_narrow_fn negative_cmp_inst)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    bool ymm = ir1_opnd_is_ymm(opnd0);
    IR2_OPND src1_low, src1_high, src2_low, src2_high;
    IR2_OPND result_low = ra_alloc_ftemp();
    IR2_OPND narrow1_low = ra_alloc_ftemp();
    IR2_OPND narrow2_low = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_xmm(opnd0) || ymm);
    lsassert(ir1_opnd_is_xmm(opnd1) == !ymm ||
             ir1_opnd_is_ymm(opnd1) == ymm);
    lsassert(ir1_opnd_size(opnd0) == ir1_opnd_size(opnd2));
    load_avx_lsx_operand(opnd1, ymm, &src1_low, &src1_high);
    load_avx_lsx_operand(opnd2, ymm, &src2_low, &src2_high);
    if (negative_cmp_inst) {
        negative_cmp_inst(narrow1_low, src1_low, 0);
        la_vandn_v(narrow1_low, narrow1_low, src1_low);
        negative_cmp_inst(narrow2_low, src2_low, 0);
        la_vandn_v(narrow2_low, narrow2_low, src2_low);
    } else {
        la_vori_b(narrow1_low, src1_low, 0);
        la_vori_b(narrow2_low, src2_low, 0);
    }
    cvt_inst(narrow1_low, narrow1_low, 0);
    cvt_inst(narrow2_low, narrow2_low, 0);
    la_vilvl_d(result_low, narrow2_low, narrow1_low);

    if (ymm) {
        /*
         * Keep copies of both high halves while writing the low result, then
         * release every low-half temporary before starting the high half.
         * The LSX backend has eight floating temporaries.  Keeping the five
         * low temporaries live while allocating the five high temporaries
         * aliases a high temporary with a live low value.
         */
        la_vori_b(ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0)), result_low, 0);
        ra_free_temp(result_low);
        ra_free_temp(narrow1_low);
        ra_free_temp(narrow2_low);
        ra_free_temp(src1_low);
        ra_free_temp(src2_low);

        IR2_OPND narrow1_high = ra_alloc_ftemp();
        IR2_OPND narrow2_high = ra_alloc_ftemp();
        IR2_OPND result_high = ra_alloc_ftemp();
        if (negative_cmp_inst) {
            negative_cmp_inst(narrow1_high, src1_high, 0);
            la_vandn_v(narrow1_high, narrow1_high, src1_high);
            negative_cmp_inst(narrow2_high, src2_high, 0);
            la_vandn_v(narrow2_high, narrow2_high, src2_high);
        } else {
            la_vori_b(narrow1_high, src1_high, 0);
            la_vori_b(narrow2_high, src2_high, 0);
        }
        cvt_inst(narrow1_high, narrow1_high, 0);
        cvt_inst(narrow2_high, narrow2_high, 0);
        la_vilvl_d(result_high, narrow2_high, narrow1_high);
        store_ymm_high128_shadow(result_high, ir1_opnd_base_reg_num(opnd0));
        ra_free_temp(result_high);
        ra_free_temp(narrow1_high);
        ra_free_temp(narrow2_high);
        ra_free_temp(src1_high);
        ra_free_temp(src2_high);
    } else {
        store_avx_lsx_result(opnd0, result_low, result_low);
        ra_free_temp(result_low);
        ra_free_temp(narrow1_low);
        ra_free_temp(narrow2_low);
        ra_free_temp(src1_low);
        ra_free_temp(src2_low);
    }
    return true;
}

bool translate_vpackssxx_lsx(IR1_INST *pir1)
{
    avx_lsx_narrow_fn cvt_inst;

    switch (ir1_opcode(pir1)) {
    case dt_X86_INS_VPACKSSDW:
        cvt_inst = la_vssrani_h_w;
        break;
    case dt_X86_INS_VPACKSSWB:
        cvt_inst = la_vssrani_b_h;
        break;
    default:
        lsassert(0);
        return false;
    }
    return translate_avx_pack_lsx(pir1, cvt_inst, NULL);
}

bool translate_vpackusxx_lsx(IR1_INST *pir1)
{
    avx_lsx_narrow_fn cvt_inst;
    avx_lsx_narrow_fn negative_cmp_inst;

    switch (ir1_opcode(pir1)) {
    case dt_X86_INS_VPACKUSDW:
        cvt_inst = la_vssrani_hu_w;
        negative_cmp_inst = la_vslti_w;
        break;
    case dt_X86_INS_VPACKUSWB:
        cvt_inst = la_vssrani_bu_h;
        negative_cmp_inst = la_vslti_h;
        break;
    default:
        lsassert(0);
        return false;
    }
    return translate_avx_pack_lsx(pir1, cvt_inst, negative_cmp_inst);
}

bool translate_vpunpckhxx_lsx(IR1_INST *pir1)
{
    avx_lsx_lane_3op_fn tr_inst;

    switch (ir1_opcode(pir1)) {
    case dt_X86_INS_VPUNPCKHBW:
        tr_inst = la_vilvh_b;
        break;
    case dt_X86_INS_VPUNPCKHWD:
        tr_inst = la_vilvh_h;
        break;
    case dt_X86_INS_VPUNPCKHDQ:
        tr_inst = la_vilvh_w;
        break;
    case dt_X86_INS_VPUNPCKHQDQ:
        tr_inst = la_vilvh_d;
        break;
    default:
        lsassert(0);
        return false;
    }
    return translate_avx_lane_3op_lsx(pir1, tr_inst);
}

bool translate_vpunpcklxx_lsx(IR1_INST *pir1)
{
    avx_lsx_lane_3op_fn tr_inst;

    switch (ir1_opcode(pir1)) {
    case dt_X86_INS_VPUNPCKLBW:
        tr_inst = la_vilvl_b;
        break;
    case dt_X86_INS_VPUNPCKLWD:
        tr_inst = la_vilvl_h;
        break;
    case dt_X86_INS_VPUNPCKLDQ:
        tr_inst = la_vilvl_w;
        break;
    case dt_X86_INS_VPUNPCKLQDQ:
        tr_inst = la_vilvl_d;
        break;
    default:
        lsassert(0);
        return false;
    }
    return translate_avx_lane_3op_lsx(pir1, tr_inst);
}

bool translate_vpunpcklqdq_lsx(IR1_INST *pir1)
{
    return translate_vpunpcklxx_lsx(pir1);
}

static bool translate_vunpckxx_lsx(IR1_INST *pir1, bool high,
                                   bool packed_double)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    bool ymm = ir1_opnd_is_ymm(opnd0);
    avx_lsx_lane_3op_fn tr_inst;
    IR2_OPND src1_low, src1_high, src2_low, src2_high;
    IR2_OPND result_low = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_xmm(opnd0) || ymm);
    lsassert(ir1_opnd_is_xmm(opnd1) == !ymm ||
             ir1_opnd_is_ymm(opnd1) == ymm);
    lsassert(ir1_opnd_size(opnd0) == ir1_opnd_size(opnd2));

    if (packed_double) {
        tr_inst = high ? la_vilvh_d : la_vilvl_d;
    } else {
        tr_inst = high ? la_vilvh_w : la_vilvl_w;
    }
    load_avx_lsx_operand(opnd1, ymm, &src1_low, &src1_high);
    load_avx_lsx_operand(opnd2, ymm, &src2_low, &src2_high);
    tr_inst(result_low, src2_low, src1_low);

    if (ymm) {
        IR2_OPND result_high = ra_alloc_ftemp();
        tr_inst(result_high, src2_high, src1_high);
        store_avx_lsx_result(opnd0, result_low, result_high);
        ra_free_temp(result_high);
        ra_free_temp(src1_high);
        ra_free_temp(src2_high);
    } else {
        store_avx_lsx_result(opnd0, result_low, result_low);
    }
    ra_free_temp(result_low);
    ra_free_temp(src1_low);
    ra_free_temp(src2_low);
    return true;
}

bool translate_vunpckhpd_lsx(IR1_INST *pir1)
{
    return translate_vunpckxx_lsx(pir1, true, true);
}

bool translate_vunpckhps_lsx(IR1_INST *pir1)
{
    return translate_vunpckxx_lsx(pir1, true, false);
}

bool translate_vunpcklpd_lsx(IR1_INST *pir1)
{
    return translate_vunpckxx_lsx(pir1, false, true);
}

bool translate_vunpcklps_lsx(IR1_INST *pir1)
{
    return translate_vunpckxx_lsx(pir1, false, false);
}

static uint8_t map_vshufpd_lsx_imm(uint8_t selector)
{
    static const uint8_t map[4] = { 0x8, 0x9, 0xc, 0xd };

    lsassert(selector < 4);
    return map[selector];
}

static void translate_vshufpd_lane_lsx(IR2_OPND result, IR2_OPND src1,
                                       IR2_OPND src2, uint8_t selector)
{
    la_vori_b(result, src1, 0);
    la_vshuf4i_d(result, src2, map_vshufpd_lsx_imm(selector));
}

bool translate_vshufpd_lsx(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    bool ymm = ir1_opnd_is_ymm(opnd0);
    uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
    IR2_OPND src1_low, src1_high, src2_low, src2_high;
    IR2_OPND result_low = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_xmm(opnd0) || ymm);
    lsassert(ir1_opnd_is_xmm(opnd1) == !ymm ||
             ir1_opnd_is_ymm(opnd1) == ymm);
    load_avx_lsx_operand(opnd1, ymm, &src1_low, &src1_high);
    load_avx_lsx_operand(opnd2, ymm, &src2_low, &src2_high);
    translate_vshufpd_lane_lsx(result_low, src1_low, src2_low, imm & 3);
    if (ymm) {
        IR2_OPND result_high = ra_alloc_ftemp();
        translate_vshufpd_lane_lsx(result_high, src1_high, src2_high,
                                   (imm >> 2) & 3);
        store_avx_lsx_result(opnd0, result_low, result_high);
        ra_free_temp(result_high);
        ra_free_temp(src1_high);
        ra_free_temp(src2_high);
    } else {
        store_avx_lsx_result(opnd0, result_low, result_low);
    }
    ra_free_temp(result_low);
    ra_free_temp(src1_low);
    ra_free_temp(src2_low);
    return true;
}

static void translate_vshufps_lane_lsx(IR2_OPND result, IR2_OPND src1,
                                       IR2_OPND src2, uint8_t imm)
{
    IR2_OPND src1_shuffled = ra_alloc_ftemp();
    IR2_OPND src2_shuffled = ra_alloc_ftemp();

    la_vshuf4i_w(src1_shuffled, src1, imm & 0xf);
    la_vshuf4i_w(src2_shuffled, src2, imm >> 4);
    la_vpickev_d(result, src2_shuffled, src1_shuffled);
    ra_free_temp(src1_shuffled);
    ra_free_temp(src2_shuffled);
}

bool translate_vshufps_lsx(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    bool ymm = ir1_opnd_is_ymm(opnd0);
    uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
    IR2_OPND src1_low, src1_high, src2_low, src2_high;
    IR2_OPND result_low = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_xmm(opnd0) || ymm);
    lsassert(ir1_opnd_is_xmm(opnd1) == !ymm ||
             ir1_opnd_is_ymm(opnd1) == ymm);
    load_avx_lsx_operand(opnd1, ymm, &src1_low, &src1_high);
    load_avx_lsx_operand(opnd2, ymm, &src2_low, &src2_high);
    translate_vshufps_lane_lsx(result_low, src1_low, src2_low, imm);
    if (ymm) {
        IR2_OPND result_high = ra_alloc_ftemp();
        translate_vshufps_lane_lsx(result_high, src1_high, src2_high, imm);
        store_avx_lsx_result(opnd0, result_low, result_high);
        ra_free_temp(result_high);
        ra_free_temp(src1_high);
        ra_free_temp(src2_high);
    } else {
        store_avx_lsx_result(opnd0, result_low, result_low);
    }
    ra_free_temp(result_low);
    ra_free_temp(src1_low);
    ra_free_temp(src2_low);
    return true;
}

static void translate_vpshufb_lane_lsx(IR2_OPND result, IR2_OPND src,
                                       IR2_OPND control)
{
    IR2_OPND index = ra_alloc_ftemp();
    IR2_OPND zero_mask = ra_alloc_ftemp();

    la_vandi_b(index, control, 0xf);
    la_vslti_b(zero_mask, control, 0);
    la_vshuf_b(result, src, src, index);
    la_vandn_v(result, zero_mask, result);
    ra_free_temp(zero_mask);
    ra_free_temp(index);
}

bool translate_vpshufb_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    IR1_OPND *control_opnd = ir1_get_opnd(pir1, 2);
    bool ymm = ir1_opnd_is_ymm(dest_opnd);
    IR2_OPND src_low;
    IR2_OPND src_high;
    IR2_OPND control_low;
    IR2_OPND control_high;
    IR2_OPND result_low = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_xmm(dest_opnd) || ymm);
    lsassert(ir1_opnd_is_xmm(src_opnd) == !ymm ||
             ir1_opnd_is_ymm(src_opnd) == ymm);
    load_avx_lsx_operand(src_opnd, ymm, &src_low, &src_high);
    load_avx_lsx_operand(control_opnd, ymm, &control_low, &control_high);
    translate_vpshufb_lane_lsx(result_low, src_low, control_low);
    if (ymm) {
        IR2_OPND result_high = ra_alloc_ftemp();

        translate_vpshufb_lane_lsx(result_high, src_high, control_high);
        store_avx_lsx_result(dest_opnd, result_low, result_high);
        ra_free_temp(result_high);
        ra_free_temp(src_high);
        ra_free_temp(control_high);
    } else {
        store_avx_lsx_result(dest_opnd, result_low, result_low);
    }
    ra_free_temp(result_low);
    ra_free_temp(src_low);
    ra_free_temp(control_low);
    return true;
}

bool translate_vpshufd_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    bool ymm = ir1_opnd_is_ymm(dest_opnd);
    uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 2));
    IR2_OPND src_low;
    IR2_OPND src_high;
    IR2_OPND result_low = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_xmm(dest_opnd) || ymm);
    lsassert(ir1_opnd_is_mem(src_opnd) ||
             ir1_opnd_is_xmm(src_opnd) == !ymm ||
             ir1_opnd_is_ymm(src_opnd) == ymm);
    load_avx_lsx_operand(src_opnd, ymm, &src_low, &src_high);
    la_vshuf4i_w(result_low, src_low, imm);
    if (ymm) {
        IR2_OPND result_high = ra_alloc_ftemp();

        la_vshuf4i_w(result_high, src_high, imm);
        store_avx_lsx_result(dest_opnd, result_low, result_high);
        ra_free_temp(result_high);
        ra_free_temp(src_high);
    } else {
        store_avx_lsx_result(dest_opnd, result_low, result_low);
    }
    ra_free_temp(result_low);
    ra_free_temp(src_low);
    return true;
}

static void translate_vpshufh_lane_lsx(IR2_OPND result, IR2_OPND src,
                                       uint8_t imm, bool high_half)
{
    IR2_OPND shuffled = ra_alloc_ftemp();
    IR2_OPND value = ra_alloc_itemp();

    la_vshuf4i_h(shuffled, src, imm);
    la_vpickve2gr_du(value, src, high_half ? 0 : 1);
    la_vxor_v(result, result, result);
    la_vinsgr2vr_d(result, value, high_half ? 0 : 1);
    la_vpickve2gr_du(value, shuffled, high_half ? 1 : 0);
    la_vinsgr2vr_d(result, value, high_half ? 1 : 0);
    ra_free_temp(value);
    ra_free_temp(shuffled);
}

static bool translate_vpshufh_lsx(IR1_INST *pir1, bool high_half)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    bool ymm = ir1_opnd_is_ymm(dest_opnd);
    uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 2));
    IR2_OPND src_low;
    IR2_OPND src_high;
    IR2_OPND result_low = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_xmm(dest_opnd) || ymm);
    lsassert(ir1_opnd_is_mem(src_opnd) ||
             ir1_opnd_is_xmm(src_opnd) == !ymm ||
             ir1_opnd_is_ymm(src_opnd) == ymm);
    load_avx_lsx_operand(src_opnd, ymm, &src_low, &src_high);
    translate_vpshufh_lane_lsx(result_low, src_low, imm, high_half);
    if (ymm) {
        IR2_OPND result_high = ra_alloc_ftemp();

        translate_vpshufh_lane_lsx(result_high, src_high, imm, high_half);
        store_avx_lsx_result(dest_opnd, result_low, result_high);
        ra_free_temp(result_high);
        ra_free_temp(src_high);
    } else {
        store_avx_lsx_result(dest_opnd, result_low, result_low);
    }
    ra_free_temp(result_low);
    ra_free_temp(src_low);
    return true;
}

bool translate_vpshufhw_lsx(IR1_INST *pir1)
{
    return translate_vpshufh_lsx(pir1, true);
}

bool translate_vpshuflw_lsx(IR1_INST *pir1)
{
    return translate_vpshufh_lsx(pir1, false);
}

static IR2_OPND build_blend_mask_lsx(int element_bits, uint8_t imm,
                                     int elements)
{
    IR2_OPND mask = ra_alloc_ftemp();
    IR2_OPND all_ones = ra_alloc_itemp();

    la_vxor_v(mask, mask, mask);
    li_d(all_ones, UINT64_MAX);
    for (int i = 0; i < elements; ++i) {
        if (!(imm & (1u << i))) {
            continue;
        }
        switch (element_bits) {
        case 16:
            la_vinsgr2vr_h(mask, all_ones, i);
            break;
        case 32:
            la_vinsgr2vr_w(mask, all_ones, i);
            break;
        case 64:
            la_vinsgr2vr_d(mask, all_ones, i);
            break;
        default:
            lsassert(0);
        }
    }
    ra_free_temp(all_ones);
    return mask;
}

static bool translate_blend_imm_lsx(IR1_INST *pir1, int element_bits)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src1_opnd = ir1_get_opnd(pir1, 1);
    IR1_OPND *src2_opnd = ir1_get_opnd(pir1, 2);
    bool ymm = ir1_opnd_is_ymm(dest_opnd);
    uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
    int elements = 128 / element_bits;
    IR2_OPND src1_low;
    IR2_OPND src1_high;
    IR2_OPND src2_low;
    IR2_OPND src2_high;
    IR2_OPND result_low = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_xmm(dest_opnd) || ymm);
    lsassert(ir1_opnd_is_xmm(src1_opnd) == !ymm ||
             ir1_opnd_is_ymm(src1_opnd) == ymm);
    lsassert(ir1_opnd_is_imm(ir1_get_opnd(pir1, 3)));
    load_avx_lsx_operand(src1_opnd, ymm, &src1_low, &src1_high);
    load_avx_lsx_operand(src2_opnd, ymm, &src2_low, &src2_high);
    IR2_OPND mask_low = build_blend_mask_lsx(element_bits, imm, elements);
    la_vbitsel_v(result_low, src1_low, src2_low, mask_low);
    ra_free_temp(mask_low);
    if (ymm) {
        IR2_OPND result_high = ra_alloc_ftemp();
        uint8_t high_imm = element_bits == 16 ? imm : imm >> elements;
        IR2_OPND mask_high = build_blend_mask_lsx(element_bits, high_imm,
                                                  elements);

        la_vbitsel_v(result_high, src1_high, src2_high, mask_high);
        ra_free_temp(mask_high);
        store_avx_lsx_result(dest_opnd, result_low, result_high);
        ra_free_temp(result_high);
        ra_free_temp(src1_high);
        ra_free_temp(src2_high);
    } else {
        store_avx_lsx_result(dest_opnd, result_low, result_low);
    }
    ra_free_temp(result_low);
    ra_free_temp(src1_low);
    ra_free_temp(src2_low);
    return true;
}

static bool translate_blend_variable_lsx(IR1_INST *pir1, int element_bits)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src1_opnd = ir1_get_opnd(pir1, 1);
    IR1_OPND *src2_opnd = ir1_get_opnd(pir1, 2);
    IR1_OPND *mask_opnd = ir1_get_opnd(pir1, 3);
    bool ymm = ir1_opnd_is_ymm(dest_opnd);
    IR2_OPND src1_low;
    IR2_OPND src1_high;
    IR2_OPND src2_low;
    IR2_OPND src2_high;
    IR2_OPND mask_low;
    IR2_OPND mask_high;
    IR2_OPND result_low = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_xmm(dest_opnd) || ymm);
    lsassert(ir1_opnd_is_xmm(src1_opnd) == !ymm ||
             ir1_opnd_is_ymm(src1_opnd) == ymm);
    lsassert(ir1_opnd_is_xmm(mask_opnd) == !ymm ||
             ir1_opnd_is_ymm(mask_opnd) == ymm);
    load_avx_lsx_operand(src1_opnd, ymm, &src1_low, &src1_high);
    load_avx_lsx_operand(src2_opnd, ymm, &src2_low, &src2_high);
    load_avx_lsx_operand(mask_opnd, ymm, &mask_low, &mask_high);
    if (element_bits == 32) {
        la_vslti_w(mask_low, mask_low, 0);
    } else {
        la_vslti_d(mask_low, mask_low, 0);
    }
    la_vbitsel_v(result_low, src1_low, src2_low, mask_low);
    if (ymm) {
        IR2_OPND result_high = ra_alloc_ftemp();
        if (element_bits == 32) {
            la_vslti_w(mask_high, mask_high, 0);
        } else {
            la_vslti_d(mask_high, mask_high, 0);
        }
        la_vbitsel_v(result_high, src1_high, src2_high, mask_high);
        store_avx_lsx_result(dest_opnd, result_low, result_high);
        ra_free_temp(result_high);
        ra_free_temp(src1_high);
        ra_free_temp(src2_high);
        ra_free_temp(mask_high);
    } else {
        store_avx_lsx_result(dest_opnd, result_low, result_low);
    }
    ra_free_temp(result_low);
    ra_free_temp(src1_low);
    ra_free_temp(src2_low);
    ra_free_temp(mask_low);
    return true;
}

bool translate_vblendpd_lsx(IR1_INST *pir1)
{
    return translate_blend_imm_lsx(pir1, 64);
}

bool translate_vblendps_lsx(IR1_INST *pir1)
{
    return translate_blend_imm_lsx(pir1, 32);
}

bool translate_vblendvpd_lsx(IR1_INST *pir1)
{
    return translate_blend_variable_lsx(pir1, 64);
}

bool translate_vblendvps_lsx(IR1_INST *pir1)
{
    return translate_blend_variable_lsx(pir1, 32);
}

bool translate_vpblendd_lsx(IR1_INST *pir1)
{
    return translate_blend_imm_lsx(pir1, 32);
}

bool translate_vpblendw_lsx(IR1_INST *pir1)
{
    return translate_blend_imm_lsx(pir1, 16);
}

static void translate_vpalignr_lane_lsx(IR2_OPND result, IR2_OPND src1,
                                        IR2_OPND src2, uint8_t imm)
{
    if (imm >= 32) {
        la_vxor_v(result, result, result);
    } else if (imm >= 16) {
        la_vbsrl_v(result, src1, imm - 16);
    } else if (imm == 0) {
        la_vori_b(result, src2, 0);
    } else {
        IR2_OPND shifted_src2 = ra_alloc_ftemp();

        la_vbsrl_v(shifted_src2, src2, imm);
        la_vbsll_v(result, src1, 16 - imm);
        la_vor_v(result, shifted_src2, result);
        ra_free_temp(shifted_src2);
    }
}

bool translate_vpalignr_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src1_opnd = ir1_get_opnd(pir1, 1);
    IR1_OPND *src2_opnd = ir1_get_opnd(pir1, 2);
    bool ymm = ir1_opnd_is_ymm(dest_opnd);
    uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
    IR2_OPND src1_low;
    IR2_OPND src1_high;
    IR2_OPND src2_low;
    IR2_OPND src2_high;
    IR2_OPND result_low = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_xmm(dest_opnd) || ymm);
    lsassert(ir1_opnd_is_xmm(src1_opnd) == !ymm ||
             ir1_opnd_is_ymm(src1_opnd) == ymm);
    lsassert(ir1_opnd_is_imm(ir1_get_opnd(pir1, 3)));
    load_avx_lsx_operand(src1_opnd, ymm, &src1_low, &src1_high);
    load_avx_lsx_operand(src2_opnd, ymm, &src2_low, &src2_high);
    translate_vpalignr_lane_lsx(result_low, src1_low, src2_low, imm);
    if (ymm) {
        IR2_OPND result_high = ra_alloc_ftemp();

        translate_vpalignr_lane_lsx(result_high, src1_high, src2_high, imm);
        store_avx_lsx_result(dest_opnd, result_low, result_high);
        ra_free_temp(result_high);
        ra_free_temp(src1_high);
        ra_free_temp(src2_high);
    } else {
        store_avx_lsx_result(dest_opnd, result_low, result_low);
    }
    ra_free_temp(result_low);
    ra_free_temp(src1_low);
    ra_free_temp(src2_low);
    return true;
}

static IR2_OPND select_128_lane_lsx(IR2_OPND src1_low, IR2_OPND src1_high,
                                    IR2_OPND src2_low, IR2_OPND src2_high,
                                    uint8_t selector, bool zero)
{
    IR2_OPND result = ra_alloc_ftemp();

    if (zero) {
        la_vxor_v(result, result, result);
    } else {
        switch (selector) {
        case 0:
            la_vori_b(result, src1_low, 0);
            break;
        case 1:
            la_vori_b(result, src1_high, 0);
            break;
        case 2:
            la_vori_b(result, src2_low, 0);
            break;
        case 3:
            la_vori_b(result, src2_high, 0);
            break;
        default:
            lsassert(0);
        }
    }
    return result;
}

static bool translate_vperm2f128_lsx_common(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src1_opnd = ir1_get_opnd(pir1, 1);
    IR1_OPND *src2_opnd = ir1_get_opnd(pir1, 2);
    uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
    IR2_OPND src1_low;
    IR2_OPND src1_high;
    IR2_OPND src2_low;
    IR2_OPND src2_high;
    IR2_OPND result_low;
    IR2_OPND result_high;

    lsassert(ir1_opnd_is_ymm(dest_opnd) && ir1_opnd_is_ymm(src1_opnd));
    lsassert(ir1_opnd_is_imm(ir1_get_opnd(pir1, 3)));
    load_avx_lsx_operand(src1_opnd, true, &src1_low, &src1_high);
    load_avx_lsx_operand(src2_opnd, true, &src2_low, &src2_high);
    result_low = select_128_lane_lsx(src1_low, src1_high, src2_low,
                                     src2_high, imm & 0x3, (imm & 0x8) != 0);
    result_high = select_128_lane_lsx(src1_low, src1_high, src2_low,
                                      src2_high, (imm >> 4) & 0x3,
                                      (imm & 0x80) != 0);
    store_avx_lsx_result(dest_opnd, result_low, result_high);
    ra_free_temp(result_low);
    ra_free_temp(result_high);
    ra_free_temp(src1_low);
    ra_free_temp(src1_high);
    ra_free_temp(src2_low);
    ra_free_temp(src2_high);
    return true;
}

bool translate_vperm2f128_lsx(IR1_INST *pir1)
{
    return translate_vperm2f128_lsx_common(pir1);
}

bool translate_vperm2i128_lsx(IR1_INST *pir1)
{
    return translate_vperm2f128_lsx_common(pir1);
}

static uint8_t map_vpermilpd_imm_lsx(uint8_t selector)
{
    return (selector & 1) | (((selector >> 1) & 1) << 2);
}

static void translate_vpermilpd_lane_lsx(IR2_OPND result, IR2_OPND src,
                                         IR2_OPND control, bool immediate,
                                         uint8_t imm)
{
    if (immediate) {
        la_vori_b(result, src, 0);
        la_vshuf4i_d(result, src, map_vpermilpd_imm_lsx(imm));
    } else {
        la_vsrli_d(control, control, 1);
        la_vandi_b(control, control, 1);
        la_vshuf_d(control, src, src);
        la_vori_b(result, control, 0);
    }
}

static void translate_vpermilps_lane_lsx(IR2_OPND result, IR2_OPND src,
                                         IR2_OPND control, bool immediate,
                                         uint8_t imm)
{
    if (immediate) {
        la_vshuf4i_w(result, src, imm);
    } else {
        la_vandi_b(control, control, 3);
        la_vshuf_w(control, src, src);
        la_vori_b(result, control, 0);
    }
}

static bool translate_vpermil_lsx(IR1_INST *pir1, bool pd)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    IR1_OPND *control_opnd = ir1_get_opnd(pir1, 2);
    bool ymm = ir1_opnd_is_ymm(dest_opnd);
    bool immediate = ir1_opnd_is_imm(control_opnd);
    uint8_t imm = immediate ? ir1_opnd_uimm(control_opnd) : 0;
    IR2_OPND src_low;
    IR2_OPND src_high;
    IR2_OPND control_low = { 0 };
    IR2_OPND control_high = { 0 };
    IR2_OPND result_low = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_xmm(dest_opnd) || ymm);
    lsassert(ir1_opnd_is_mem(src_opnd) ||
             ir1_opnd_is_xmm(src_opnd) == !ymm ||
             ir1_opnd_is_ymm(src_opnd) == ymm);
    load_avx_lsx_operand(src_opnd, ymm, &src_low, &src_high);
    if (!immediate) {
        load_avx_lsx_operand(control_opnd, ymm, &control_low, &control_high);
    }
    if (pd) {
        translate_vpermilpd_lane_lsx(result_low, src_low, control_low,
                                     immediate, imm & 0x3);
    } else {
        translate_vpermilps_lane_lsx(result_low, src_low, control_low,
                                     immediate, imm);
    }
    if (ymm) {
        IR2_OPND result_high = ra_alloc_ftemp();

        if (pd) {
            translate_vpermilpd_lane_lsx(result_high, src_high, control_high,
                                         immediate, (imm >> 2) & 0x3);
        } else {
            translate_vpermilps_lane_lsx(result_high, src_high, control_high,
                                         immediate, imm);
        }
        store_avx_lsx_result(dest_opnd, result_low, result_high);
        ra_free_temp(result_high);
        ra_free_temp(src_low);
        ra_free_temp(src_high);
        if (!immediate) {
            ra_free_temp(control_low);
            ra_free_temp(control_high);
        }
    } else {
        store_avx_lsx_result(dest_opnd, result_low, result_low);
        ra_free_temp(src_low);
        if (!immediate) {
            ra_free_temp(control_low);
        }
    }
    ra_free_temp(result_low);
    return true;
}

bool translate_vpermilpd_lsx(IR1_INST *pir1)
{
    return translate_vpermil_lsx(pir1, true);
}

bool translate_vpermilps_lsx(IR1_INST *pir1)
{
    return translate_vpermil_lsx(pir1, false);
}

static void translate_vpermute_q_imm_lsx(IR2_OPND result_low,
                                         IR2_OPND result_high,
                                         IR2_OPND src_low, IR2_OPND src_high,
                                         uint8_t imm)
{
    la_vxor_v(result_low, result_low, result_low);
    la_vxor_v(result_high, result_high, result_high);
    for (int i = 0; i < 4; ++i) {
        int selector = (imm >> (i * 2)) & 0x3;
        IR2_OPND value = ra_alloc_itemp();

        if (selector < 2) {
            la_vpickve2gr_du(value, src_low, selector);
        } else {
            la_vpickve2gr_du(value, src_high, selector - 2);
        }
        if (i < 2) {
            la_vinsgr2vr_d(result_low, value, i);
        } else {
            la_vinsgr2vr_d(result_high, value, i - 2);
        }
        ra_free_temp(value);
    }
}

static void translate_vpermute_w_dynamic_lsx(IR2_OPND result,
                                             IR2_OPND data_low,
                                             IR2_OPND data_high,
                                             IR2_OPND index)
{
    IR2_OPND control = ra_alloc_ftemp();

    la_vandi_b(control, index, 7);
    /* vshuf.w takes its selectors from the old destination and combines
     * vk[0..3] with vj[0..3].  Put the low half in vk and high half in vj. */
    la_vshuf_w(control, data_high, data_low);
    la_vori_b(result, control, 0);
    ra_free_temp(control);
}

bool translate_vpermd_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *index_opnd = ir1_get_opnd(pir1, 1);
    IR1_OPND *data_opnd = ir1_get_opnd(pir1, 2);
    IR2_OPND index_low;
    IR2_OPND index_high;
    IR2_OPND data_low;
    IR2_OPND data_high;
    IR2_OPND result_low = ra_alloc_ftemp();
    IR2_OPND result_high = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_ymm(dest_opnd) && ir1_opnd_is_ymm(index_opnd));
    load_avx_lsx_operand(index_opnd, true, &index_low, &index_high);
    load_avx_lsx_operand(data_opnd, true, &data_low, &data_high);
    translate_vpermute_w_dynamic_lsx(result_low, data_low, data_high,
                                     index_low);
    translate_vpermute_w_dynamic_lsx(result_high, data_low, data_high,
                                     index_high);
    store_avx_lsx_result(dest_opnd, result_low, result_high);
    ra_free_temp(result_low);
    ra_free_temp(result_high);
    ra_free_temp(index_low);
    ra_free_temp(index_high);
    ra_free_temp(data_low);
    ra_free_temp(data_high);
    return true;
}

bool translate_vpermpx_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *index_or_imm = ir1_get_opnd(pir1, 2);
    IR2_OPND data_low;
    IR2_OPND data_high;
    IR2_OPND result_low = ra_alloc_ftemp();
    IR2_OPND result_high = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_ymm(dest_opnd));
    if (ir1_opcode(pir1) == dt_X86_INS_VPERMPD) {
        lsassert(ir1_opnd_is_imm(index_or_imm));
        load_avx_lsx_operand(ir1_get_opnd(pir1, 1), true,
                             &data_low, &data_high);
        translate_vpermute_q_imm_lsx(result_low, result_high,
                                     data_low, data_high,
                                     ir1_opnd_uimm(index_or_imm));
        store_avx_lsx_result(dest_opnd, result_low, result_high);
        ra_free_temp(data_low);
        ra_free_temp(data_high);
    } else {
        IR1_OPND *index_opnd = ir1_get_opnd(pir1, 1);
        IR1_OPND *data_opnd = ir1_get_opnd(pir1, 2);
        IR2_OPND index_low;
        IR2_OPND index_high;

        load_avx_lsx_operand(data_opnd, true,
                             &data_low, &data_high);
        load_avx_lsx_operand(index_opnd, true, &index_low, &index_high);
        translate_vpermute_w_dynamic_lsx(result_low, data_low, data_high,
                                         index_low);
        translate_vpermute_w_dynamic_lsx(result_high, data_low, data_high,
                                         index_high);
        store_avx_lsx_result(dest_opnd, result_low, result_high);
        ra_free_temp(index_low);
        ra_free_temp(index_high);
        ra_free_temp(data_low);
        ra_free_temp(data_high);
    }
    ra_free_temp(result_low);
    ra_free_temp(result_high);
    return true;
}

bool translate_vpermq_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    IR1_OPND *imm_opnd = ir1_get_opnd(pir1, 2);
    IR2_OPND src_low;
    IR2_OPND src_high;
    IR2_OPND result_low = ra_alloc_ftemp();
    IR2_OPND result_high = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_ymm(dest_opnd) && ir1_opnd_is_imm(imm_opnd));
    load_avx_lsx_operand(src_opnd, true, &src_low, &src_high);
    translate_vpermute_q_imm_lsx(result_low, result_high, src_low, src_high,
                                 ir1_opnd_uimm(imm_opnd));
    store_avx_lsx_result(dest_opnd, result_low, result_high);
    ra_free_temp(result_low);
    ra_free_temp(result_high);
    ra_free_temp(src_low);
    ra_free_temp(src_high);
    return true;
}

bool translate_vpxor_lsx(IR1_INST * pir1) {
    IR1_OPND * opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND * opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND * opnd2 = ir1_get_opnd(pir1, 2);
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) ||
        (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd1)));
    {
        /* LSX-only path */
        int dest_index = ir1_opnd_base_reg_num(opnd0);
        IR2_OPND dest = ra_alloc_xmm(dest_index);
        IR2_OPND src1 = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));

        if (ir1_opnd_is_xmm(opnd0)) {
            IR2_OPND src2 = load_freg128_from_ir1(opnd2);

            la_vxor_v(dest, src1, src2);
            clear_ymm_high128_shadow(dest_index);
        } else {
            IR2_OPND src1_high = load_ymm_high128_shadow(
                ir1_opnd_base_reg_num(opnd1));
            IR2_OPND src2;
            IR2_OPND src2_high;

            if (ir1_opnd_is_ymm(opnd2)) {
                int src2_index = ir1_opnd_base_reg_num(opnd2);

                src2 = ra_alloc_xmm(src2_index);
                src2_high = load_ymm_high128_shadow(src2_index);
            } else {
                int mem_imm;
                IR2_OPND mem_opnd = convert_mem(opnd2, &mem_imm);

                lsassert(ir1_opnd_is_mem(opnd2));
                src2 = ra_alloc_ftemp();
                src2_high = ra_alloc_ftemp();
                gen_test_page_flag(mem_opnd, mem_imm, PAGE_READ);
                la_vld(src2, mem_opnd, mem_imm);
                mem_opnd = mem_imm_add_disp(mem_opnd, &mem_imm, 16);
                gen_test_page_flag(mem_opnd, mem_imm, PAGE_READ);
                la_vld(src2_high, mem_opnd, mem_imm);
            }

            la_vxor_v(dest, src1, src2);
            la_vxor_v(src1_high, src1_high, src2_high);
            store_ymm_high128_shadow(src1_high, dest_index);
        }
    }
    return true;
}

bool translate_vzeroupper_lsx(IR1_INST *pir1)
{
    clear_all_ymm_high128_shadows();
    return true;
}

#if 0
bool translate_vpinsrb(IR1_INST *pir1)
{
    IR2_OPND dest = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND src0 = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND src1 = load_ireg_from_ir1(ir1_get_opnd(pir1, 2), UNKNOWN_EXTENSION, false);
    uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
    la_vand_v(dest, src0, src0);
    la_vinsgr2vr_b(dest, src1, imm);
    set_high128_xreg_to_zero(dest);
    return true;
}

#endif

bool translate_vpinsrx_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src1_opnd = ir1_get_opnd(pir1, 1);
    IR1_OPND *src2_opnd = ir1_get_opnd(pir1, 2);
    uint8 imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
    IR2_OPND src2;
    IR2_OPND dest;
    IR2_OPND src1;
    int dest_index;

    lsassert(ir1_opnd_is_xmm(dest_opnd) && ir1_opnd_is_xmm(src1_opnd));
    lsassert(ir1_opnd_is_gpr(src2_opnd) || ir1_opnd_is_mem(src2_opnd));
    lsassert(ir1_opnd_is_imm(ir1_get_opnd(pir1, 3)));
    switch (ir1_opcode(pir1)) {
    case dt_X86_INS_VPINSRB:
        lsassert(ir1_opnd_size(src2_opnd) == 8 ||
                 ir1_opnd_size(src2_opnd) == 32);
        imm &= 0xf;
        src2 = ir1_opnd_is_mem(src2_opnd) ?
            load_u8_from_ir1_mem_exact(src2_opnd) :
            load_ireg_from_ir1(src2_opnd, UNKNOWN_EXTENSION, false);
        break;
    case dt_X86_INS_VPINSRW:
        lsassert(ir1_opnd_size(src2_opnd) == 16 ||
                 ir1_opnd_size(src2_opnd) == 32);
        imm &= 0x7;
        src2 = ir1_opnd_is_mem(src2_opnd) ?
            load_u16_from_ir1_mem_exact(src2_opnd) :
            load_ireg_from_ir1(src2_opnd, UNKNOWN_EXTENSION, false);
        break;
    case dt_X86_INS_VPINSRD:
        lsassert(ir1_opnd_size(src2_opnd) == 32);
        imm &= 0x3;
        src2 = ir1_opnd_is_mem(src2_opnd) ?
            load_u32_from_ir1_mem_exact(src2_opnd) :
            load_ireg_from_ir1(src2_opnd, UNKNOWN_EXTENSION, false);
        break;
    default:
        lsassert(0);
    }
    src1 = ra_alloc_xmm(ir1_opnd_base_reg_num(src1_opnd));
    dest_index = ir1_opnd_base_reg_num(dest_opnd);
    dest = ra_alloc_xmm(dest_index);
    la_vori_b(dest, src1, 0);
    switch (ir1_opcode(pir1)) {
    case dt_X86_INS_VPINSRB:
        la_vinsgr2vr_b(dest, src2, imm);
        break;
    case dt_X86_INS_VPINSRW:
        la_vinsgr2vr_h(dest, src2, imm);
        break;
    case dt_X86_INS_VPINSRD:
        la_vinsgr2vr_w(dest, src2, imm);
        break;
    default:
        lsassert(0);
    }
    clear_ymm_high128_shadow(dest_index);
    if (ir1_opnd_is_mem(src2_opnd))
        ra_free_temp(src2);
    return true;
}

bool translate_vpinsrq_lsx(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND *opnd3 = ir1_get_opnd(pir1, 3);
    uint8_t imm = ir1_opnd_uimm(opnd3) & 0x1;

    lsassert(ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1));
    lsassert(ir1_opnd_is_gpr(opnd2) || ir1_opnd_is_mem(opnd2));
    lsassert(ir1_opnd_size(opnd2) == 64 && ir1_opnd_is_imm(opnd3));

    int dest_index = ir1_opnd_base_reg_num(opnd0);
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1(opnd1);
    IR2_OPND src2 = ir1_opnd_is_mem(opnd2) ?
        load_u64_from_ir1_mem_exact(opnd2) :
        load_ireg_from_ir1(opnd2, UNKNOWN_EXTENSION, false);

    la_vori_b(dest, src1, 0);
    la_vinsgr2vr_d(dest, src2, imm);
    clear_ymm_high128_shadow(dest_index);
    if (ir1_opnd_is_mem(opnd2))
        ra_free_temp(src2);

    return true;
}

static void vpmaskmov_lsx_apply(IR2_OPND result, IR2_OPND memory,
                                IR2_OPND mask, IR2_OPND value,
                                bool quadword, bool store)
{
    IR2_OPND selected = ra_alloc_ftemp();
    IR2_OPND unselected = ra_alloc_ftemp();

    if (quadword) {
        la_vclz_d(selected, mask);
        la_vseqi_d(selected, selected, 0);
    } else {
        la_vclz_w(selected, mask);
        la_vseqi_w(selected, selected, 0);
    }
    if (store) {
        la_vand_v(result, selected, value);
        la_vandn_v(unselected, selected, memory);
        la_vxor_v(result, result, unselected);
    } else {
        la_vand_v(result, selected, memory);
    }

    ra_free_temp(unselected);
    ra_free_temp(selected);
}

bool translate_vpmaskmovx_lsx(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    bool store = ir1_opnd_is_mem(opnd0);
    bool ymm = ir1_opnd_is_ymm(opnd1);
    bool quadword = ir1_opcode(pir1) == dt_X86_INS_VPMASKMOVQ;
    IR1_OPND *mem = store ? opnd0 : opnd2;
    int mask_index = ir1_opnd_base_reg_num(opnd1);
    IR2_OPND mask_low = ra_alloc_ftemp();

    lsassert(ir1_opcode(pir1) == dt_X86_INS_VPMASKMOVD ||
             ir1_opcode(pir1) == dt_X86_INS_VPMASKMOVQ);
    lsassert(ir1_opnd_is_xmm(opnd1) || ir1_opnd_is_ymm(opnd1));
    lsassert(ymm ? (ir1_opnd_is_ymm(opnd2) &&
                    (store || ir1_opnd_is_ymm(opnd0))) :
                    (ir1_opnd_is_xmm(opnd2) &&
                     (store || ir1_opnd_is_xmm(opnd0))));
    lsassert(store ? (ir1_opnd_is_mem(opnd0) &&
                      (ir1_opnd_is_xmm(opnd2) || ir1_opnd_is_ymm(opnd2))) :
                    (ir1_opnd_is_mem(opnd2) &&
                     (ir1_opnd_is_xmm(opnd0) || ir1_opnd_is_ymm(opnd0))));

    /* LASX accesses the complete vector before applying the mask. */
    la_vori_b(mask_low, ra_alloc_xmm(mask_index), 0);
    if (ymm) {
        IR2_OPND result_low = ra_alloc_ftemp();
        if (store) {
            int src_index = ir1_opnd_base_reg_num(opnd2);
            IR2_OPND memory_low;
            IR2_OPND unused_high;
            IR2_OPND value_low = ra_alloc_xmm(src_index);

            load_v256_from_ir1_mem_exact(mem, &memory_low, &unused_high);
            ra_free_temp(unused_high);
            vpmaskmov_lsx_apply(result_low, memory_low, mask_low,
                                 value_low, quadword, true);
            store_v256_low_to_ir1_mem_exact(result_low, mem);
            ra_free_temp(memory_low);
        } else {
            int dest_index = ir1_opnd_base_reg_num(opnd0);
            IR2_OPND memory_low;
            IR2_OPND unused_high;

            load_v256_from_ir1_mem_exact(mem, &memory_low, &unused_high);
            ra_free_temp(unused_high);
            vpmaskmov_lsx_apply(result_low, memory_low, mask_low, memory_low,
                                 quadword, false);
            la_vori_b(ra_alloc_xmm(dest_index), result_low, 0);
            ra_free_temp(memory_low);
        }
        ra_free_temp(result_low);

        IR2_OPND memory_high = load_v256_high_from_ir1_mem_exact(mem);
        IR2_OPND result_high = ra_alloc_ftemp();
        IR2_OPND mask_high = load_ymm_high128_shadow(mask_index);
        if (store) {
            IR2_OPND value_high = load_ymm_high128_shadow(
                ir1_opnd_base_reg_num(opnd2));

            vpmaskmov_lsx_apply(result_high, memory_high, mask_high,
                                 value_high, quadword, true);
            store_v256_high_to_ir1_mem_exact(result_high, mem);
            ra_free_temp(value_high);
        } else {
            int dest_index = ir1_opnd_base_reg_num(opnd0);

            vpmaskmov_lsx_apply(result_high, memory_high, mask_high,
                                 memory_high, quadword, false);
            store_ymm_high128_shadow(result_high, dest_index);
        }
        ra_free_temp(mask_high);
        ra_free_temp(result_high);
        ra_free_temp(memory_high);
    } else {
        IR2_OPND memory = load_v128_from_ir1_mem_exact(mem);
        IR2_OPND result = ra_alloc_ftemp();
        IR2_OPND value;

        if (store) {
            value = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd2));
            vpmaskmov_lsx_apply(result, memory, mask_low, value,
                                 quadword, true);
            store_v128_to_ir1_mem_exact(result, mem);
        } else {
            int dest_index = ir1_opnd_base_reg_num(opnd0);

            vpmaskmov_lsx_apply(result, memory, mask_low, memory,
                                 quadword, false);
            la_vori_b(ra_alloc_xmm(dest_index), result, 0);
            clear_ymm_high128_shadow(dest_index);
        }
        ra_free_temp(result);
        ra_free_temp(memory);
    }
    ra_free_temp(mask_low);
    return true;
}

bool translate_vzeroall_lsx(IR1_INST *pir1)
{
    int reg_xmm = 8;
    IR2_OPND zero = ra_alloc_ftemp();

#ifdef TARGET_X86_64
    reg_xmm = 16;
#endif
    la_vxor_v(zero, zero, zero);
    for (int i = 0; i < reg_xmm; ++i) {
        la_vori_b(ra_alloc_xmm(i), zero, 0);
    }
    clear_all_ymm_high128_shadows();
    ra_free_temp(zero);
    return true;
}

static void translate_vmpsadbw_lane_lsx(IR2_OPND result, IR2_OPND src1,
                                        IR2_OPND src2, uint8_t imm)
{
    IR2_OPND temp1 = ra_alloc_ftemp();
    IR2_OPND temp2 = ra_alloc_ftemp();
    IR2_OPND temp3 = ra_alloc_ftemp();
    IR2_OPND temp_dest = ra_alloc_ftemp();

    la_vreplvei_w(temp1, src2, imm & 3);
    la_vmepatmsk_v(temp2, 1, imm & 0x4);
    la_vmepatmsk_v(temp3, 2, imm & 0x4);
    la_vshuf_b(temp2, src1, src1, temp2);
    la_vshuf_b(temp3, src1, src1, temp3);
    la_vabsd_bu(temp2, temp2, temp1);
    la_vabsd_bu(temp3, temp3, temp1);
    la_vhaddw_hu_bu(temp2, temp2, temp2);
    la_vhaddw_wu_hu(temp2, temp2, temp2);
    la_vhaddw_hu_bu(temp3, temp3, temp3);
    la_vhaddw_wu_hu(temp_dest, temp3, temp3);
    la_vsrlni_h_w(temp_dest, temp2, 0);
    la_vori_b(result, temp_dest, 0);
    ra_free_temp(temp_dest);
    ra_free_temp(temp3);
    ra_free_temp(temp2);
    ra_free_temp(temp1);
}

bool translate_vmpsadbw_lsx(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND *opnd3 = ir1_get_opnd(pir1, 3);
    bool ymm = ir1_opnd_is_ymm(opnd0);
    uint8_t imm = ir1_opnd_uimm(opnd3);
    int dest_index = ir1_opnd_base_reg_num(opnd0);
    IR2_OPND src1_low;
    IR2_OPND src2_low;
    IR2_OPND result_low = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_xmm(opnd0) || ymm);
    lsassert(ir1_opnd_is_xmm(opnd1) == !ymm ||
             ir1_opnd_is_ymm(opnd1) == ymm);
    lsassert(ir1_opnd_size(opnd0) == ir1_opnd_size(opnd2));
    load_avx_lsx_operand(opnd1, false, &src1_low, NULL);
    load_avx_lsx_operand(opnd2, false, &src2_low, NULL);
    translate_vmpsadbw_lane_lsx(result_low, src1_low, src2_low, imm);
    la_vori_b(ra_alloc_xmm(dest_index), result_low, 0);
    ra_free_temp(result_low);
    ra_free_temp(src1_low);
    ra_free_temp(src2_low);

    if (ymm) {
        IR2_OPND src1_high = load_avx_lsx_high_operand(opnd1);
        IR2_OPND src2_high = load_avx_lsx_high_operand(opnd2);
        IR2_OPND result_high = ra_alloc_ftemp();

        translate_vmpsadbw_lane_lsx(result_high, src1_high, src2_high,
                                     (imm >> 3) & 7);
        store_ymm_high128_shadow(result_high, dest_index);
        ra_free_temp(result_high);
        ra_free_temp(src1_high);
        ra_free_temp(src2_high);
    } else {
        clear_ymm_high128_shadow(dest_index);
    }
    return true;
}

static bool translate_vpcmpxstrx_lsx(IR1_INST *pir1, ADDR helper_func,
                                     enum aot_rel_kind rel_kind,
                                     bool explicit_lengths, bool writes_mask)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int d = ir1_opnd_base_reg_num(opnd0);
    int imm = ir1_opnd_uimm(opnd2);

    lsassert(ir1_opnd_is_xmm(opnd0));
    lsassert(ir1_opnd_is_xmm(opnd1) || ir1_opnd_is_mem(opnd1));
    if (explicit_lengths) {
#ifdef TARGET_X86_64
        imm |= ir1_rex_w(pir1) << 8;
#endif
    }

    if (ir1_opnd_is_xmm(opnd1)) {
        tr_gen_call_to_helper_pcmpxstrx(helper_func, d,
                                        ir1_opnd_base_reg_num(opnd1), imm,
                                        rel_kind);
    } else {
        int temp_index = writes_mask ? (d + 1) % 7 + 1 : (d + 1) % 8;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(temp_index);

        la_vori_b(temp, src, 0);
        load_freg128_from_ir1_mem(src, opnd1);
        tr_gen_call_to_helper_pcmpxstrx(helper_func, d, temp_index, imm,
                                        rel_kind);
        la_vori_b(src, temp, 0);
        ra_free_temp(temp);
    }
    if (writes_mask) {
        clear_ymm_high128_shadow(0);
    }
    return true;
}

bool translate_vpcmpestri_lsx(IR1_INST *pir1)
{
    return translate_vpcmpxstrx_lsx(pir1, (ADDR)helper_pcmpestri_xmm,
                                    LOAD_HELPER_PCMPESTRI_XMM, true, false);
}

bool translate_vpcmpestrm_lsx(IR1_INST *pir1)
{
    return translate_vpcmpxstrx_lsx(pir1, (ADDR)helper_pcmpestrm_xmm,
                                    LOAD_HELPER_PCMPESTRM_XMM, true, true);
}

bool translate_vpcmpistri_lsx(IR1_INST *pir1)
{
    return translate_vpcmpxstrx_lsx(pir1, (ADDR)helper_pcmpistri_xmm,
                                    LOAD_HELPER_PCMPISTRI_XMM, false, false);
}

bool translate_vpcmpistrm_lsx(IR1_INST *pir1)
{
    return translate_vpcmpxstrx_lsx(pir1, (ADDR)helper_pcmpistrm_xmm,
                                    LOAD_HELPER_PCMPISTRM_XMM, false, true);
}

static void emit_pclmul_lsx_lane(IR2_OPND dest, IR2_OPND src1,
                                 IR2_OPND src2, uint8_t ctrl)
{
    IR2_OPND ftemp = ra_alloc_ftemp();
    IR2_OPND lhs = ra_alloc_itemp();
    IR2_OPND rhs = ra_alloc_itemp();
    IR2_OPND res_lo = ra_alloc_itemp();
    IR2_OPND res_hi = ra_alloc_itemp();
    IR2_OPND lhs_lane_op = ra_alloc_itemp();
    IR2_OPND rhs_lane_op = ra_alloc_itemp();
    int lhs_lane = (ctrl & 1) ? 1 : 0;
    int rhs_lane = (ctrl & 0x10) ? 1 : 0;

    li_d(lhs_lane_op, lhs_lane);
    la_vreplve_d(ftemp, src1, lhs_lane_op);
    la_vpickve2gr_d(lhs, ftemp, 0);
    li_d(rhs_lane_op, rhs_lane);
    la_vreplve_d(ftemp, src2, rhs_lane_op);
    la_vpickve2gr_d(rhs, ftemp, 0);

    /* The lane selectors are dead before the carry-less multiply loop.
     * Free them here: the loop needs two integer temporaries and the LSX
     * temporary pool only has seven registers on x86-64 builds. */
    ra_free_temp(lhs_lane_op);
    ra_free_temp(rhs_lane_op);
    emit_pclmul_ctz_loop(lhs, rhs, res_lo, res_hi);
    la_vxor_v(dest, dest, dest);
    la_vinsgr2vr_d(dest, res_lo, 0);
    la_vinsgr2vr_d(dest, res_hi, 1);

    ra_free_temp(ftemp);
    ra_free_temp(lhs);
    ra_free_temp(rhs);
    ra_free_temp(res_lo);
    ra_free_temp(res_hi);
}

bool translate_vpclmulqdq_lsx(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    IR1_OPND *opnd3 = ir1_get_opnd(pir1, 3);
    uint8_t ctrl = ir1_opnd_uimm(opnd3);

    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND src1 = load_freg128_from_ir1(opnd1);
        IR2_OPND src2 = load_freg128_from_ir1(opnd2);
        IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));

        emit_pclmul_lsx_lane(dest, src1, src2, ctrl);
        clear_ymm_high128_shadow(ir1_opnd_base_reg_num(opnd0));
        ra_free_temp_auto(src2);
        return true;
    }

    int dest_index = ir1_opnd_base_reg_num(opnd0);
    int src1_index = ir1_opnd_base_reg_num(opnd1);
    IR2_OPND src1_low = ra_alloc_xmm(src1_index);
    IR2_OPND src1_high = load_ymm_high128_shadow(src1_index);
    IR2_OPND src2_low;
    IR2_OPND src2_high;

    if (ir1_opnd_is_mem(opnd2)) {
        load_v256_from_ir1_mem_exact(opnd2, &src2_low, &src2_high);
    } else {
        int src2_index = ir1_opnd_base_reg_num(opnd2);
        src2_low = ra_alloc_xmm(src2_index);
        src2_high = load_ymm_high128_shadow(src2_index);
    }

    IR2_OPND dest_low = ra_alloc_xmm(dest_index);
    IR2_OPND dest_high = ra_alloc_ftemp();
    emit_pclmul_lsx_lane(dest_low, src1_low, src2_low, ctrl);
    emit_pclmul_lsx_lane(dest_high, src1_high, src2_high, ctrl);
    store_ymm_high128_shadow(dest_high, dest_index);
    ra_free_temp(dest_high);
    ra_free_temp(src1_high);
    ra_free_temp_auto(src2_low);
    ra_free_temp_auto(src2_high);
    return true;
}

static void adjust_vsib_index(IR2_OPND dest, IR2_OPND base,
                    IR2_OPND index, int scale)
{
    IR2_INST *(*la_alsl)(IR2_OPND, IR2_OPND, IR2_OPND, int);
#ifdef TARGET_X86_64
    la_alsl = &la_alsl_d;
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

bool translate_vaesdec_lsx(IR1_INST *pir1)
{
    return latx_translate_vaesdec_lsx(pir1);
}

bool translate_vaesdeclast_lsx(IR1_INST *pir1)
{
    return latx_translate_vaesdeclast_lsx(pir1);
}

bool translate_vaesenc_lsx(IR1_INST *pir1)
{
    return latx_translate_vaesenc_lsx(pir1);
}

bool translate_vaesenclast_lsx(IR1_INST *pir1)
{
    return latx_translate_vaesenclast_lsx(pir1);
}

bool translate_vaesimc_lsx(IR1_INST *pir1)
{
    return latx_translate_vaesimc_vpaes(pir1);
}

bool translate_vaeskeygenassist_lsx(IR1_INST *pir1)
{
    return latx_translate_vaeskeygenassist_vpaes(pir1);
}

typedef IR2_INST *(*avx_lsx_fp_binary_fn)(IR2_OPND, IR2_OPND, IR2_OPND);

typedef struct LsxFpStatus {
    IR2_OPND mxcsr;
    IR2_OPND flags;
    IR2_OPND saved_fcsr;
} LsxFpStatus;

static void lsx_fp_status_begin(LsxFpStatus *status)
{
    status->saved_fcsr = set_fpu_fcsr_rounding_field_by_x86();
    status->mxcsr = zero_ir2_opnd;
    status->flags = zero_ir2_opnd;
}

static void lsx_fp_apply_daz(IR2_OPND value, IR2_OPND mxcsr,
                             IR2_OPND flags, bool double_precision,
                             int lanes)
{
    (void)value;
    (void)mxcsr;
    (void)flags;
    (void)double_precision;
    (void)lanes;
    return;

    uint64_t exponent_mask = double_precision ?
        UINT64_C(0x7ff0000000000000) : UINT64_C(0x000000007f800000);
    uint64_t fraction_mask = double_precision ?
        UINT64_C(0x000fffffffffffff) : UINT64_C(0x00000000007fffff);
    uint64_t sign_mask = double_precision ?
        UINT64_C(0x8000000000000000) : UINT64_C(0x0000000080000000);
    IR2_INST *(*pick)(IR2_OPND, IR2_OPND, int) = double_precision ?
        la_vpickve2gr_du : la_vpickve2gr_w;
    IR2_INST *(*insert)(IR2_OPND, IR2_OPND, int) = double_precision ?
        la_vinsgr2vr_d : la_vinsgr2vr_w;
    IR2_OPND exponent = ra_alloc_itemp();

    li_d(exponent, exponent_mask);

    for (int lane = 0; lane < lanes; lane++) {
        IR2_OPND bits = ra_alloc_itemp();
        IR2_OPND field = ra_alloc_itemp();
        IR2_OPND not_nan = ra_alloc_label();
        IR2_OPND keep_denormal = ra_alloc_label();
        IR2_OPND done = ra_alloc_label();

        pick(bits, value, lane);
        la_and(field, bits, exponent);
        la_bne(field, exponent, not_nan);
        li_d(field, fraction_mask);
        la_and(field, bits, field);
        la_beq(field, zero_ir2_opnd, done);
        li_d(field, double_precision ? UINT64_C(0x0008000000000000) :
             UINT64_C(0x0000000000400000));
        la_and(field, bits, field);
        la_bne(field, zero_ir2_opnd, done);
        la_ori(flags, flags, 0x1);
        la_b(done);

        la_label(not_nan);
        la_bne(field, zero_ir2_opnd, done);
        li_d(field, fraction_mask);
        la_and(field, bits, field);
        la_beq(field, zero_ir2_opnd, done);

        la_andi(field, mxcsr, 0x40);
        la_beq(field, zero_ir2_opnd, keep_denormal);
        li_d(field, sign_mask);
        la_and(bits, bits, field);
        insert(value, bits, lane);
        la_b(done);

        la_label(keep_denormal);
        la_ori(flags, flags, 0x2);
        la_label(done);
        ra_free_temp(done);
        ra_free_temp(keep_denormal);
        ra_free_temp(not_nan);
        ra_free_temp(field);
        ra_free_temp(bits);
    }
    ra_free_temp(exponent);
}

static void lsx_fp_apply_fz(IR2_OPND value, IR2_OPND mxcsr,
                            IR2_OPND flags, bool double_precision,
                            int lanes)
{
    (void)value;
    (void)mxcsr;
    (void)flags;
    (void)double_precision;
    (void)lanes;
    return;

    uint64_t exponent_mask = double_precision ?
        UINT64_C(0x7ff0000000000000) : UINT64_C(0x000000007f800000);
    uint64_t fraction_mask = double_precision ?
        UINT64_C(0x000fffffffffffff) : UINT64_C(0x00000000007fffff);
    uint64_t sign_mask = double_precision ?
        UINT64_C(0x8000000000000000) : UINT64_C(0x0000000080000000);
    IR2_INST *(*pick)(IR2_OPND, IR2_OPND, int) = double_precision ?
        la_vpickve2gr_du : la_vpickve2gr_w;
    IR2_INST *(*insert)(IR2_OPND, IR2_OPND, int) = double_precision ?
        la_vinsgr2vr_d : la_vinsgr2vr_w;

    for (int lane = 0; lane < lanes; lane++) {
        IR2_OPND bits = ra_alloc_itemp();
        IR2_OPND field = ra_alloc_itemp();
        IR2_OPND check_subnormal = ra_alloc_label();
        IR2_OPND done = ra_alloc_label();

        la_andi(field, mxcsr, 0x8000);
        la_beq(field, zero_ir2_opnd, done);
        la_andi(field, mxcsr, 0x800);
        la_beq(field, zero_ir2_opnd, done);
        pick(bits, value, lane);
        li_d(field, exponent_mask);
        la_and(field, bits, field);
        la_beq(field, zero_ir2_opnd, check_subnormal);
        la_b(done);
        la_label(check_subnormal);
        li_d(field, fraction_mask);
        la_and(field, bits, field);
        la_beq(field, zero_ir2_opnd, done);
        li_d(field, sign_mask);
        la_and(bits, bits, field);
        insert(value, bits, lane);
        la_ori(flags, flags, 0x30);
        la_label(done);
        ra_free_temp(done);
        ra_free_temp(check_subnormal);
        ra_free_temp(field);
        ra_free_temp(bits);
    }
}

static void lsx_fp_status_finish(IR1_INST *pir1, LsxFpStatus *status)
{
    (void)pir1;
    set_fpu_rounding_mode(status->saved_fcsr);
    ra_free_temp_auto(status->saved_fcsr);
}

/*
 * LSX floating arithmetic returns a positive default qNaN for invalid
 * operations such as 0 / 0 and 0 * infinity.  x86 uses the negative
 * indefinite qNaN in those cases, while a qNaN supplied by either operand
 * must retain its payload.  Repair only a NaN result with no NaN input.
 */
static void lsx_fp_fix_invalid_nan_lane(IR2_OPND result, IR2_OPND src1,
                                        IR2_OPND src2, bool double_precision,
                                        int lane)
{
    uint64_t exponent_mask = double_precision ?
        UINT64_C(0x7ff0000000000000) : UINT64_C(0x000000007f800000);
    uint64_t fraction_mask = double_precision ?
        UINT64_C(0x000fffffffffffff) : UINT64_C(0x00000000007fffff);
    uint64_t indefinite = double_precision ?
        UINT64_C(0xfff8000000000000) : UINT64_C(0x00000000ffc00000);
    uint64_t quiet_bit = double_precision ?
        UINT64_C(0x0008000000000000) : UINT64_C(0x0000000000400000);
    IR2_INST *(*pick)(IR2_OPND, IR2_OPND, int) = double_precision ?
        la_vpickve2gr_du : la_vpickve2gr_w;
    IR2_INST *(*insert)(IR2_OPND, IR2_OPND, int) = double_precision ?
        la_vinsgr2vr_d : la_vinsgr2vr_w;
    IR2_OPND bits = ra_alloc_itemp();
    IR2_OPND field = ra_alloc_itemp();
    IR2_OPND exponent = ra_alloc_itemp();
    IR2_OPND source2 = ra_alloc_label();
    IR2_OPND canonicalize = ra_alloc_label();
    IR2_OPND done = ra_alloc_label();

    li_d(exponent, exponent_mask);
    pick(bits, result, lane);
    la_and(field, bits, exponent);
    la_bne(field, exponent, done);
    li_d(field, fraction_mask);
    la_and(field, bits, field);
    la_beq(field, zero_ir2_opnd, done);

    /* Preserve an input NaN's sign and payload.  LSX may canonicalize the
     * result, while x86 arithmetic propagates the first NaN operand. */
    pick(bits, src1, lane);
    la_and(field, bits, exponent);
    la_bne(field, exponent, source2);
    li_d(field, fraction_mask);
    la_and(field, bits, field);
    la_beq(field, zero_ir2_opnd, source2);
    li_d(field, quiet_bit);
    la_or(bits, bits, field);
    insert(result, bits, lane);
    la_b(done);

    la_label(source2);
    pick(bits, src2, lane);
    la_and(field, bits, exponent);
    la_bne(field, exponent, canonicalize);
    li_d(field, fraction_mask);
    la_and(field, bits, field);
    la_beq(field, zero_ir2_opnd, canonicalize);
    li_d(field, quiet_bit);
    la_or(bits, bits, field);
    insert(result, bits, lane);
    la_b(done);

    la_label(canonicalize);
    li_d(bits, indefinite);
    insert(result, bits, lane);
    la_label(done);
    ra_free_temp(done);
    ra_free_temp(canonicalize);
    ra_free_temp(source2);
    ra_free_temp(exponent);
    ra_free_temp(field);
    ra_free_temp(bits);
}

static void lsx_fp_fix_invalid_nan(IR2_OPND result, IR2_OPND src1,
                                   IR2_OPND src2, bool double_precision,
                                   int lanes)
{
    for (int lane = 0; lane < lanes; lane++) {
        lsx_fp_fix_invalid_nan_lane(result, src1, src2,
                                    double_precision, lane);
    }
}

static void lsx_fp_fix_sqrt_invalid_nan_lane(IR2_OPND result, IR2_OPND src,
                                             bool double_precision, int lane)
{
    uint64_t exponent_mask = double_precision ?
        UINT64_C(0x7ff0000000000000) : UINT64_C(0x000000007f800000);
    uint64_t fraction_mask = double_precision ?
        UINT64_C(0x000fffffffffffff) : UINT64_C(0x00000000007fffff);
    uint64_t indefinite = double_precision ?
        UINT64_C(0xfff8000000000000) : UINT64_C(0x00000000ffc00000);
    IR2_INST *(*pick)(IR2_OPND, IR2_OPND, int) = double_precision ?
        la_vpickve2gr_du : la_vpickve2gr_w;
    IR2_INST *(*insert)(IR2_OPND, IR2_OPND, int) = double_precision ?
        la_vinsgr2vr_d : la_vinsgr2vr_w;
    IR2_OPND bits = ra_alloc_itemp();
    IR2_OPND field = ra_alloc_itemp();
    IR2_OPND exponent = ra_alloc_itemp();
    IR2_OPND canonicalize = ra_alloc_label();
    IR2_OPND done = ra_alloc_label();

    li_d(exponent, exponent_mask);
    pick(bits, result, lane);
    la_and(field, bits, exponent);
    la_bne(field, exponent, done);
    li_d(field, fraction_mask);
    la_and(field, bits, field);
    la_beq(field, zero_ir2_opnd, done);

    /* Keep a source NaN's sign and payload; only sqrt of a negative value
     * needs x86's negative indefinite qNaN. */
    pick(bits, src, lane);
    la_and(field, bits, exponent);
    la_bne(field, exponent, canonicalize);
    li_d(field, fraction_mask);
    la_and(field, bits, field);
    la_bne(field, zero_ir2_opnd, done);

    la_label(canonicalize);
    li_d(bits, indefinite);
    insert(result, bits, lane);
    la_label(done);
    ra_free_temp(done);
    ra_free_temp(canonicalize);
    ra_free_temp(exponent);
    ra_free_temp(field);
    ra_free_temp(bits);
}

static void lsx_fp_fix_sqrt_invalid_nan(IR2_OPND result, IR2_OPND src,
                                        bool double_precision, int lanes)
{
    for (int lane = 0; lane < lanes; lane++) {
        lsx_fp_fix_sqrt_invalid_nan_lane(result, src, double_precision,
                                         lane);
    }
}

static bool translate_avx_fp_binary_lsx(IR1_INST *pir1,
                                        avx_lsx_fp_binary_fn tr_inst,
                                        bool double_precision,
                                        bool track_fp_status)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    bool ymm = ir1_opnd_is_ymm(opnd0);
    IR2_OPND src1_low, src1_high, src2_low, src2_high;
    IR2_OPND result_low = ra_alloc_ftemp();
    LsxFpStatus status;

    lsassert(ir1_opnd_is_xmm(opnd0) || ymm);
    lsassert(ir1_opnd_is_xmm(opnd1) == !ymm ||
             ir1_opnd_is_ymm(opnd1) == ymm);
    load_avx_lsx_operand(opnd1, ymm, &src1_low, &src1_high);
    load_avx_lsx_operand(opnd2, ymm, &src2_low, &src2_high);
    if (track_fp_status) {
        lsx_fp_status_begin(&status);
        lsx_fp_apply_daz(src1_low, status.mxcsr, status.flags,
                         double_precision, double_precision ? 2 : 4);
        lsx_fp_apply_daz(src2_low, status.mxcsr, status.flags,
                         double_precision, double_precision ? 2 : 4);
        if (ymm) {
            lsx_fp_apply_daz(src1_high, status.mxcsr, status.flags,
                             double_precision, double_precision ? 2 : 4);
            lsx_fp_apply_daz(src2_high, status.mxcsr, status.flags,
                             double_precision, double_precision ? 2 : 4);
        }
    }
    tr_inst(result_low, src1_low, src2_low);
    if (track_fp_status) {
        lsx_fp_fix_invalid_nan(result_low, src1_low, src2_low,
                               double_precision,
                               double_precision ? 2 : 4);
        lsx_fp_apply_fz(result_low, status.mxcsr, status.flags,
                        double_precision, double_precision ? 2 : 4);
    }

    if (ymm) {
        IR2_OPND result_high = ra_alloc_ftemp();

        tr_inst(result_high, src1_high, src2_high);
        if (track_fp_status) {
            lsx_fp_fix_invalid_nan(result_high, src1_high, src2_high,
                                   double_precision,
                                   double_precision ? 2 : 4);
            lsx_fp_apply_fz(result_high, status.mxcsr, status.flags,
                            double_precision, double_precision ? 2 : 4);
            lsx_fp_status_finish(pir1, &status);
        }
        store_avx_lsx_result(opnd0, result_low, result_high);
        ra_free_temp(result_high);
    } else {
        if (track_fp_status) {
            lsx_fp_status_finish(pir1, &status);
        }
        store_avx_lsx_result(opnd0, result_low, result_low);
    }
    ra_free_temp(result_low);
    ra_free_temp(src1_low);
    ra_free_temp(src2_low);
    if (ymm) {
        ra_free_temp(src1_high);
        ra_free_temp(src2_high);
    }
    return true;
}

static IR2_OPND load_avx_lsx_scalar_operand(IR1_OPND *opnd,
                                            bool double_precision,
                                            bool *is_temp)
{
    if (!ir1_opnd_is_mem(opnd)) {
        *is_temp = false;
        return load_freg128_from_ir1(opnd);
    }

    IR2_OPND value = double_precision ?
        load_u64_from_ir1_mem_exact(opnd) :
        load_u32_from_ir1_mem_exact(opnd);
    IR2_OPND result = ra_alloc_ftemp();

    la_vxor_v(result, result, result);
    if (double_precision) {
        la_vinsgr2vr_d(result, value, 0);
    } else {
        la_vinsgr2vr_w(result, value, 0);
    }
    ra_free_temp(value);
    *is_temp = true;
    return result;
}

static void translate_avx_round_lane_lsx(IR2_OPND result, IR2_OPND src,
                                         uint8_t imm, bool double_precision)
{
    IR2_OPND probe = ra_alloc_ftemp();
    IR2_OPND fcsr = ra_alloc_itemp();
    IR2_OPND fcsr_save = ra_alloc_itemp();
    IR2_OPND mxcsr = ra_alloc_itemp();

    if (imm & 0x8) {
        if (double_precision) {
            la_vfcmp_cond_d(probe, src, src, 0x8);
        } else {
            la_vfcmp_cond_s(probe, src, src, 0x8);
        }
    } else if (double_precision) {
        la_vfrint_d(probe, src);
    } else {
        la_vfrint_s(probe, src);
    }

    la_movfcsr2gr(fcsr, fcsr_ir2_opnd);
    la_bstrpick_w(fcsr_save, fcsr, 31, 0);
    la_ld_wu(mxcsr, env_ir2_opnd, lsenv_offset_of_mxcsr(lsenv));
    la_bstrins_w(fcsr, zero_ir2_opnd, 4, 0);
    if (imm & 0x4) {
        IR2_OPND rounding = ra_alloc_itemp();
        IR2_OPND rounding_low = ra_alloc_itemp_internal();
        IR2_OPND rounding_ready = ra_alloc_label();

        la_bstrpick_w(rounding, mxcsr, 14, 13);
        la_andi(rounding_low, rounding, 0x1);
        la_beq(rounding_low, zero_ir2_opnd, rounding_ready);
        la_xori(rounding, rounding, 0x2);
        la_label(rounding_ready);
        la_bstrins_w(fcsr, rounding, 9, 8);
        la_movgr2fcsr(fcsr_ir2_opnd, fcsr);
        if (double_precision) {
            la_vfrint_d(result, src);
        } else {
            la_vfrint_s(result, src);
        }
        ra_free_temp(rounding_low);
        ra_free_temp(rounding);
    } else {
        la_movgr2fcsr(fcsr_ir2_opnd, fcsr);
        switch (imm & 0x3) {
        case 0:
            if (double_precision) {
                la_vfrintrne_d(result, src);
            } else {
                la_vfrintrne_s(result, src);
            }
            break;
        case 1:
            if (double_precision) {
                la_vfrintrm_d(result, src);
            } else {
                la_vfrintrm_s(result, src);
            }
            break;
        case 2:
            if (double_precision) {
                la_vfrintrp_d(result, src);
            } else {
                la_vfrintrp_s(result, src);
            }
            break;
        default:
            if (double_precision) {
                la_vfrintrz_d(result, src);
            } else {
                la_vfrintrz_s(result, src);
            }
            break;
        }
    }
    la_movgr2fcsr(fcsr_ir2_opnd, fcsr_save);
    ra_free_temp(mxcsr);
    ra_free_temp(fcsr_save);
    ra_free_temp(fcsr);
    ra_free_temp(probe);
}

static bool translate_avx_round_packed_lsx(IR1_INST *pir1,
                                           bool double_precision)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 2));
    bool ymm = ir1_opnd_is_ymm(opnd0);
    IR2_OPND src_low;
    IR2_OPND src_high;
    IR2_OPND result_low = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_xmm(opnd0) || ymm);
    load_avx_lsx_operand(opnd1, ymm, &src_low, &src_high);
    translate_avx_round_lane_lsx(result_low, src_low, imm, double_precision);
    if (ymm) {
        IR2_OPND result_high = ra_alloc_ftemp();

        translate_avx_round_lane_lsx(result_high, src_high, imm,
                                     double_precision);
        store_avx_lsx_result(opnd0, result_low, result_high);
        ra_free_temp(result_high);
    } else {
        store_avx_lsx_result(opnd0, result_low, result_low);
    }
    ra_free_temp(result_low);
    ra_free_temp(src_low);
    if (ymm) {
        ra_free_temp(src_high);
    }
    return true;
}

static bool translate_avx_round_scalar_lsx(IR1_INST *pir1,
                                           bool double_precision)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1(opnd1);
    bool src2_is_temp;
    IR2_OPND src2 = load_avx_lsx_scalar_operand(opnd2, double_precision,
                                                &src2_is_temp);
    IR2_OPND scalar = ra_alloc_ftemp();
    IR2_OPND rounded = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1));
    if (double_precision) {
        la_vreplvei_d(scalar, src2, 0);
    } else {
        la_vreplvei_w(scalar, src2, 0);
    }
    translate_avx_round_lane_lsx(rounded, scalar, imm, double_precision);
    la_vori_b(dest, src1, 0);
    if (double_precision) {
        la_vextrins_d(dest, rounded, 0);
    } else {
        la_vextrins_w(dest, rounded, 0);
    }
    store_avx_lsx_result(opnd0, dest, dest);
    ra_free_temp(rounded);
    ra_free_temp(scalar);
    if (src2_is_temp) {
        ra_free_temp(src2);
    }
    return true;
}

bool translate_vroundps_lsx(IR1_INST *pir1)
{
    return translate_avx_round_packed_lsx(pir1, false);
}

bool translate_vroundpd_lsx(IR1_INST *pir1)
{
    return translate_avx_round_packed_lsx(pir1, true);
}

bool translate_vroundss_lsx(IR1_INST *pir1)
{
    return translate_avx_round_scalar_lsx(pir1, false);
}

bool translate_vroundsd_lsx(IR1_INST *pir1)
{
    return translate_avx_round_scalar_lsx(pir1, true);
}

static bool translate_avx_fp_scalar_lsx(IR1_INST *pir1,
                                        avx_lsx_fp_binary_fn tr_inst,
                                        bool is_double,
                                        bool track_fp_status)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    IR2_OPND dest;
    IR2_OPND src1;
    IR2_OPND src2;
    IR2_OPND temp = ra_alloc_ftemp();
    bool src2_is_temp;
    LsxFpStatus status;

    lsassert(ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1));
    dest = load_freg128_from_ir1(opnd0);
    src1 = load_freg128_from_ir1(opnd1);
    src2 = load_avx_lsx_scalar_operand(opnd2, is_double, &src2_is_temp);
    if (track_fp_status) {
        lsx_fp_status_begin(&status);
        lsx_fp_apply_daz(src1, status.mxcsr, status.flags,
                         is_double, 1);
        lsx_fp_apply_daz(src2, status.mxcsr, status.flags,
                         is_double, 1);
    }
    tr_inst(temp, src1, src2);
    lsx_fp_fix_invalid_nan(temp, src1, src2, is_double, 1);
    if (track_fp_status) {
        lsx_fp_apply_fz(temp, status.mxcsr, status.flags, is_double, 1);
    }
    la_vori_b(dest, src1, 0);
    if (is_double) {
        la_vextrins_d(dest, temp, 0);
    } else {
        la_vextrins_w(dest, temp, 0);
    }
    if (track_fp_status) {
        lsx_fp_status_finish(pir1, &status);
    }
    store_avx_lsx_result(opnd0, dest, dest);
    ra_free_temp(temp);
    if (src2_is_temp) {
        ra_free_temp(src2);
    }
    return true;
}

bool translate_vaddpd_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_binary_lsx(pir1, la_vfadd_d, true, true);
}

bool translate_vaddps_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_binary_lsx(pir1, la_vfadd_s, false, true);
}

bool translate_vaddsd_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_scalar_lsx(pir1, la_vfadd_d, true, true);
}

bool translate_vaddss_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_scalar_lsx(pir1, la_vfadd_s, false, true);
}

bool translate_vsubpd_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_binary_lsx(pir1, la_vfsub_d, true, true);
}

bool translate_vsubps_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_binary_lsx(pir1, la_vfsub_s, false, true);
}

bool translate_vsubsd_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_scalar_lsx(pir1, la_vfsub_d, true, true);
}

bool translate_vsubss_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_scalar_lsx(pir1, la_vfsub_s, false, true);
}

bool translate_vmulpd_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_binary_lsx(pir1, la_vfmul_d, true, true);
}

bool translate_vmulps_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_binary_lsx(pir1, la_vfmul_s, false, true);
}

bool translate_vmulss_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_scalar_lsx(pir1, la_vfmul_s, false, true);
}

bool translate_vdivpd_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_binary_lsx(pir1, la_vfdiv_d, true, true);
}

bool translate_vdivps_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_binary_lsx(pir1, la_vfdiv_s, false, true);
}

bool translate_vdivss_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_scalar_lsx(pir1, la_vfdiv_s, false, true);
}

bool translate_vandnpd_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_binary_lsx(pir1, la_vandn_v, true, false);
}

bool translate_vandnps_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_binary_lsx(pir1, la_vandn_v, false, false);
}

bool translate_vandpd_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_binary_lsx(pir1, la_vand_v, true, false);
}

bool translate_vandps_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_binary_lsx(pir1, la_vand_v, false, false);
}

bool translate_vorpd_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_binary_lsx(pir1, la_vor_v, true, false);
}

bool translate_vorps_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_binary_lsx(pir1, la_vor_v, false, false);
}

bool translate_vxorps_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_binary_lsx(pir1, la_vxor_v, false, false);
}

typedef void (*avx_lsx_lane_fn)(IR2_OPND, IR2_OPND, IR2_OPND);

static void fix_vhaddpd_invalid_nan_lsx(IR2_OPND result, IR2_OPND lhs,
                                        IR2_OPND rhs, int lane)
{
    IR2_OPND lhs_value = ra_alloc_itemp();
    IR2_OPND rhs_value = ra_alloc_itemp();
    IR2_OPND mask = ra_alloc_itemp();
    IR2_OPND done = ra_alloc_label();

    la_vpickve2gr_du(lhs_value, lhs, lane);
    la_vpickve2gr_du(rhs_value, rhs, lane);
    la_xor(mask, lhs_value, rhs_value);
    la_srli_d(mask, mask, 63);
    la_beq(mask, zero_ir2_opnd, done);

    li_d(mask, UINT64_C(0x7ff0000000000000));
    la_and(lhs_value, lhs_value, mask);
    la_bne(lhs_value, mask, done);
    la_and(rhs_value, rhs_value, mask);
    la_bne(rhs_value, mask, done);

    /* x86 returns its negative indefinite qNaN for +inf + -inf. */
    li_d(lhs_value, UINT64_C(0xfff8000000000000));
    la_vinsgr2vr_d(result, lhs_value, lane);
    la_label(done);
    ra_free_temp(mask);
    ra_free_temp(rhs_value);
    ra_free_temp(lhs_value);
}

static void translate_avx_addsub_lane_lsx(IR2_OPND result,
                                          IR2_OPND src1,
                                          IR2_OPND src2,
                                          bool is_double)
{
    if (is_double) {
        IR2_OPND sub1 = ra_alloc_ftemp();
        IR2_OPND sub2 = ra_alloc_ftemp();
        IR2_OPND add1 = ra_alloc_ftemp();
        IR2_OPND add2 = ra_alloc_ftemp();

        la_vpickev_d(sub1, src1, src1);
        la_vpickev_d(sub2, src2, src2);
        la_vpickod_d(add1, src1, src1);
        la_vpickod_d(add2, src2, src2);
        la_vfsub_d(sub1, sub1, sub2);
        la_vfadd_d(add1, add1, add2);
        la_vpickev_d(result, add1, sub1);
        lsx_fp_fix_invalid_nan(result, src1, src2, true, 2);
        ra_free_temp(add2);
        ra_free_temp(add1);
        ra_free_temp(sub2);
        ra_free_temp(sub1);
    } else {
        IR2_OPND subtract = ra_alloc_ftemp();
        IR2_OPND add = ra_alloc_ftemp();
        IR2_OPND even_mask = ra_alloc_ftemp();
        IR2_OPND mask_value = ra_alloc_itemp();

        /* Keep the original NaN payload and sign in the subtract lanes. */
        la_vfsub_s(subtract, src1, src2);
        la_vfadd_s(add, src1, src2);
        li_d(mask_value, UINT64_C(0x00000000ffffffff));
        la_vreplgr2vr_d(even_mask, mask_value);
        la_vbitsel_v(result, add, subtract, even_mask);
        lsx_fp_fix_invalid_nan(result, src1, src2, false, 4);
        ra_free_temp(mask_value);
        ra_free_temp(even_mask);
        ra_free_temp(add);
        ra_free_temp(subtract);
    }
}

static void translate_avx_hadd_lane_lsx(IR2_OPND result,
                                        IR2_OPND src1,
                                        IR2_OPND src2,
                                        bool is_double,
                                        bool subtract)
{
    IR2_OPND even = ra_alloc_ftemp();
    IR2_OPND odd = ra_alloc_ftemp();

    if (is_double) {
        la_vpickev_d(even, src2, src1);
        la_vpickod_d(odd, src2, src1);
        if (subtract) {
            la_vfsub_d(result, even, odd);
            lsx_fp_fix_invalid_nan(result, even, odd, true, 2);
        } else {
            la_vfadd_d(result, even, odd);
            fix_vhaddpd_invalid_nan_lsx(result, even, odd, 0);
            fix_vhaddpd_invalid_nan_lsx(result, even, odd, 1);
            lsx_fp_fix_invalid_nan(result, even, odd, true, 2);
        }
    } else {
        la_vpickev_w(even, src2, src1);
        la_vpickod_w(odd, src2, src1);
        if (subtract) {
            la_vfsub_s(result, even, odd);
            lsx_fp_fix_invalid_nan(result, even, odd, false, 4);
        } else {
            la_vfadd_s(result, even, odd);
            lsx_fp_fix_invalid_nan(result, even, odd, false, 4);
        }
    }
    ra_free_temp(odd);
    ra_free_temp(even);
}

static bool translate_avx_pairwise_lsx(IR1_INST *pir1,
                                       avx_lsx_lane_fn tr_inst,
                                       bool double_precision)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    bool ymm = ir1_opnd_is_ymm(opnd0);
    IR2_OPND src1_low, src1_high, src2_low, src2_high;
    IR2_OPND result_low = ra_alloc_ftemp();
    LsxFpStatus status;
    IR2_OPND saved_fcsr = set_fpu_fcsr_rounding_field_by_x86();

    if (ir1_opnd_is_mem(opnd1)) {
        src1_low = load_v128_from_ir1_mem_exact(opnd1);
    } else {
        src1_low = ra_alloc_ftemp();
        la_vori_b(src1_low, ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1)), 0);
    }
    if (ir1_opnd_is_mem(opnd2)) {
        src2_low = load_v128_from_ir1_mem_exact(opnd2);
    } else {
        src2_low = ra_alloc_ftemp();
        la_vori_b(src2_low, ra_alloc_xmm(ir1_opnd_base_reg_num(opnd2)), 0);
    }
    lsx_fp_status_begin(&status);
    lsx_fp_apply_daz(src1_low, status.mxcsr, status.flags,
                     double_precision, double_precision ? 2 : 4);
    lsx_fp_apply_daz(src2_low, status.mxcsr, status.flags,
                     double_precision, double_precision ? 2 : 4);
    tr_inst(result_low, src1_low, src2_low);
    lsx_fp_apply_fz(result_low, status.mxcsr, status.flags,
                    double_precision, double_precision ? 2 : 4);
    if (ymm) {
        la_vori_b(ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0)), result_low, 0);
        ra_free_temp(result_low);
        ra_free_temp(src1_low);
        ra_free_temp(src2_low);

        if (ir1_opnd_is_mem(opnd1)) {
            src1_high = load_v256_high_from_ir1_mem_exact(opnd1);
        } else {
            src1_high = load_ymm_high128_shadow(
                ir1_opnd_base_reg_num(opnd1));
        }
        if (ir1_opnd_is_mem(opnd2)) {
            src2_high = load_v256_high_from_ir1_mem_exact(opnd2);
        } else {
            src2_high = load_ymm_high128_shadow(
                ir1_opnd_base_reg_num(opnd2));
        }
        IR2_OPND result_high = ra_alloc_ftemp();

        lsx_fp_apply_daz(src1_high, status.mxcsr, status.flags,
                         double_precision, double_precision ? 2 : 4);
        lsx_fp_apply_daz(src2_high, status.mxcsr, status.flags,
                         double_precision, double_precision ? 2 : 4);
        tr_inst(result_high, src1_high, src2_high);
        lsx_fp_apply_fz(result_high, status.mxcsr, status.flags,
                        double_precision, double_precision ? 2 : 4);
        lsx_fp_status_finish(pir1, &status);
        store_ymm_high128_shadow(result_high, ir1_opnd_base_reg_num(opnd0));
        ra_free_temp(result_high);
    } else {
        lsx_fp_status_finish(pir1, &status);
        store_avx_lsx_result(opnd0, result_low, result_low);
    }
    if (!ymm) {
        ra_free_temp(result_low);
        ra_free_temp(src1_low);
        ra_free_temp(src2_low);
    }
    if (ymm) {
        ra_free_temp(src1_high);
        ra_free_temp(src2_high);
    }
    set_fpu_rounding_mode(saved_fcsr);
    ra_free_temp_auto(saved_fcsr);
    return true;
}

static void translate_avx_haddpd_lane_lsx(IR2_OPND result,
                                          IR2_OPND src1,
                                          IR2_OPND src2)
{
    translate_avx_hadd_lane_lsx(result, src1, src2, true, false);
}

static void translate_avx_addsubpd_lane_lsx(IR2_OPND result,
                                            IR2_OPND src1,
                                            IR2_OPND src2)
{
    translate_avx_addsub_lane_lsx(result, src1, src2, true);
}

static void translate_avx_addsubps_lane_lsx(IR2_OPND result,
                                            IR2_OPND src1,
                                            IR2_OPND src2)
{
    translate_avx_addsub_lane_lsx(result, src1, src2, false);
}

static void translate_avx_haddps_lane_lsx(IR2_OPND result,
                                          IR2_OPND src1,
                                          IR2_OPND src2)
{
    translate_avx_hadd_lane_lsx(result, src1, src2, false, false);
}

static void translate_avx_hsubpd_lane_lsx(IR2_OPND result,
                                          IR2_OPND src1,
                                          IR2_OPND src2)
{
    translate_avx_hadd_lane_lsx(result, src1, src2, true, true);
}

static void translate_avx_hsubps_lane_lsx(IR2_OPND result,
                                          IR2_OPND src1,
                                          IR2_OPND src2)
{
    translate_avx_hadd_lane_lsx(result, src1, src2, false, true);
}

bool translate_vaddsubpd_lsx(IR1_INST *pir1)
{
    return translate_avx_pairwise_lsx(pir1, translate_avx_addsubpd_lane_lsx,
                                      true);
}

bool translate_vaddsubps_lsx(IR1_INST *pir1)
{
    return translate_avx_pairwise_lsx(pir1, translate_avx_addsubps_lane_lsx,
                                      false);
}

bool translate_vhaddpd_lsx(IR1_INST *pir1)
{
    return translate_avx_pairwise_lsx(pir1, translate_avx_haddpd_lane_lsx,
                                      true);
}

bool translate_vhaddps_lsx(IR1_INST *pir1)
{
    return translate_avx_pairwise_lsx(pir1, translate_avx_haddps_lane_lsx,
                                      false);
}

bool translate_vhsubpd_lsx(IR1_INST *pir1)
{
    return translate_avx_pairwise_lsx(pir1, translate_avx_hsubpd_lane_lsx,
                                      true);
}

bool translate_vhsubps_lsx(IR1_INST *pir1)
{
    return translate_avx_pairwise_lsx(pir1, translate_avx_hsubps_lane_lsx,
                                      false);
}

static void translate_avx_minmax_lane_lsx(IR2_OPND result,
                                          IR2_OPND src1,
                                          IR2_OPND src2,
                                          bool is_double,
                                          bool is_max)
{
    IR2_OPND mask = ra_alloc_ftemp();
    IR2_OPND selected1 = ra_alloc_ftemp();
    IR2_OPND selected2 = ra_alloc_ftemp();

    if (is_double) {
        if (is_max) {
            la_vfcmp_cond_d(mask, src2, src1, FCMP_COND_CLT);
        } else {
            la_vfcmp_cond_d(mask, src1, src2, FCMP_COND_CLT);
        }
    } else if (is_max) {
        la_vfcmp_cond_s(mask, src2, src1, FCMP_COND_CLT);
    } else {
        la_vfcmp_cond_s(mask, src1, src2, FCMP_COND_CLT);
    }
    la_vand_v(selected1, src1, mask);
    la_vandn_v(selected2, mask, src2);
    la_vor_v(result, selected1, selected2);
    ra_free_temp(selected2);
    ra_free_temp(selected1);
    ra_free_temp(mask);
}

static bool translate_avx_minmax_lsx(IR1_INST *pir1,
                                     bool is_double,
                                     bool is_max,
                                     bool scalar)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    IR2_OPND src1;
    IR2_OPND src2;
    IR2_OPND result;
    LsxFpStatus status;

    if (scalar) {
        IR2_OPND dest = load_freg128_from_ir1(opnd0);
        bool src2_is_temp;

        src1 = load_freg128_from_ir1(opnd1);
        src2 = load_avx_lsx_scalar_operand(opnd2, is_double, &src2_is_temp);
        result = ra_alloc_ftemp();
        lsx_fp_status_begin(&status);
        lsx_fp_apply_daz(src1, status.mxcsr, status.flags,
                         is_double, 1);
        lsx_fp_apply_daz(src2, status.mxcsr, status.flags,
                         is_double, 1);
        translate_avx_minmax_lane_lsx(result, src1, src2,
                                      is_double, is_max);
        lsx_fp_apply_fz(result, status.mxcsr, status.flags,
                        is_double, 1);
        la_vori_b(dest, src1, 0);
        if (is_double) {
            la_vextrins_d(dest, result, 0);
        } else {
            la_vextrins_w(dest, result, 0);
        }
        lsx_fp_status_finish(pir1, &status);
        store_avx_lsx_result(opnd0, dest, dest);
        ra_free_temp(result);
        if (src2_is_temp) {
            ra_free_temp(src2);
        }
        return true;
    }

    bool ymm = ir1_opnd_is_ymm(opnd0);
    IR2_OPND src1_low, src2_low;
    IR2_OPND result_low = ra_alloc_ftemp();

    load_avx_lsx_operand(opnd1, false, &src1_low, NULL);
    load_avx_lsx_operand(opnd2, false, &src2_low, NULL);
    lsx_fp_status_begin(&status);
    lsx_fp_apply_daz(src1_low, status.mxcsr, status.flags,
                     is_double, is_double ? 2 : 4);
    lsx_fp_apply_daz(src2_low, status.mxcsr, status.flags,
                     is_double, is_double ? 2 : 4);
    translate_avx_minmax_lane_lsx(result_low, src1_low, src2_low,
                                  is_double, is_max);
    lsx_fp_apply_fz(result_low, status.mxcsr, status.flags,
                    is_double, is_double ? 2 : 4);
    if (ymm) {
        IR2_OPND src1_high;
        IR2_OPND src2_high;
        IR2_OPND result_high;

        /* Release the low half before allocating the high-half work set. */
        la_vori_b(ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0)), result_low, 0);
        ra_free_temp(result_low);
        ra_free_temp(src1_low);
        ra_free_temp(src2_low);

        src1_high = ir1_opnd_is_mem(opnd1) ?
            load_v256_high_from_ir1_mem_exact(opnd1) :
            load_ymm_high128_shadow(ir1_opnd_base_reg_num(opnd1));
        src2_high = ir1_opnd_is_mem(opnd2) ?
            load_v256_high_from_ir1_mem_exact(opnd2) :
            load_ymm_high128_shadow(ir1_opnd_base_reg_num(opnd2));
        result_high = ra_alloc_ftemp();
        lsx_fp_apply_daz(src1_high, status.mxcsr, status.flags,
                         is_double, is_double ? 2 : 4);
        lsx_fp_apply_daz(src2_high, status.mxcsr, status.flags,
                         is_double, is_double ? 2 : 4);
        translate_avx_minmax_lane_lsx(result_high, src1_high, src2_high,
                                      is_double, is_max);
        lsx_fp_apply_fz(result_high, status.mxcsr, status.flags,
                        is_double, is_double ? 2 : 4);
        lsx_fp_status_finish(pir1, &status);
        store_ymm_high128_shadow(result_high,
                                 ir1_opnd_base_reg_num(opnd0));
        ra_free_temp(result_high);
        ra_free_temp(src1_high);
        ra_free_temp(src2_high);
    } else {
        lsx_fp_status_finish(pir1, &status);
        store_avx_lsx_result(opnd0, result_low, result_low);
        ra_free_temp(result_low);
        ra_free_temp(src1_low);
        ra_free_temp(src2_low);
    }
    return true;
}

bool translate_vmaxpd_lsx(IR1_INST *pir1)
{
    return translate_avx_minmax_lsx(pir1, true, true, false);
}

bool translate_vmaxps_lsx(IR1_INST *pir1)
{
    return translate_avx_minmax_lsx(pir1, false, true, false);
}

bool translate_vmaxsd_lsx(IR1_INST *pir1)
{
    return translate_avx_minmax_lsx(pir1, true, true, true);
}

bool translate_vmaxss_lsx(IR1_INST *pir1)
{
    return translate_avx_minmax_lsx(pir1, false, true, true);
}

bool translate_vminpd_lsx(IR1_INST *pir1)
{
    return translate_avx_minmax_lsx(pir1, true, false, false);
}

bool translate_vminps_lsx(IR1_INST *pir1)
{
    return translate_avx_minmax_lsx(pir1, false, false, false);
}

bool translate_vminsd_lsx(IR1_INST *pir1)
{
    return translate_avx_minmax_lsx(pir1, true, false, true);
}

bool translate_vminss_lsx(IR1_INST *pir1)
{
    return translate_avx_minmax_lsx(pir1, false, false, true);
}

typedef IR2_INST *(*avx_lsx_fp_unary_fn)(IR2_OPND, IR2_OPND);

/* x86 RCPPS/RSQRTPS retain only about 12 mantissa bits. */
static void lsx_fp_truncate_rcp_estimate(IR2_OPND value)
{
    IR2_OPND mask = ra_alloc_itemp();
    IR2_OPND vector_mask = ra_alloc_ftemp();

    li_d(mask, UINT64_C(0x00000000fffff000));
    la_vreplgr2vr_w(vector_mask, mask);
    la_vand_v(value, value, vector_mask);
    ra_free_temp(vector_mask);
    ra_free_temp(mask);
}

/*
 * The x86 estimate ROM is not simply a truncated exact reciprocal.  Its
 * power-of-two entries sit just below the mathematical value, and denormal
 * operands are treated as signed zero.  Match those architectural estimate
 * values after the LSX instruction has supplied the normal approximation.
 */
static void lsx_fp_fix_rcp_estimate_lane(IR2_OPND value, IR2_OPND src,
                                         bool rsqrt, int lane)
{
    IR2_OPND source = ra_alloc_itemp();
    IR2_OPND result = ra_alloc_itemp();
    IR2_OPND field = ra_alloc_itemp();
    IR2_OPND subnormal = ra_alloc_label();
    IR2_OPND normal = ra_alloc_label();
    IR2_OPND done = ra_alloc_label();

    la_vpickve2gr_w(source, src, lane);
    li_d(field, UINT64_C(0x7f800000));
    la_and(field, source, field);
    la_beq(field, zero_ir2_opnd, subnormal);
    li_d(result, UINT64_C(0x7f800000));
    la_beq(field, result, done);

    /* A normal power of two has no fraction bits. */
    li_d(result, UINT64_C(0x007fffff));
    la_and(result, source, result);
    la_beq(result, zero_ir2_opnd, normal);
    la_b(done);

    la_label(subnormal);
    li_d(result, UINT64_C(0x007fffff));
    la_and(result, source, result);
    la_beq(result, zero_ir2_opnd, done);
    li_d(result, UINT64_C(0xff800000));
    la_and(field, source, result);
    li_d(result, UINT64_C(0x7f800000));
    la_or(result, result, field);
    la_vinsgr2vr_w(value, result, lane);
    la_b(done);

    la_label(normal);
    la_vpickve2gr_w(result, value, lane);
    li_d(field, UINT64_C(0x7fffffff));
    la_and(result, result, field);
    if (rsqrt) {
        li_d(field, UINT64_C(0x00800000));
        la_and(field, source, field);
        IR2_OPND even_exponent = ra_alloc_label();
        IR2_OPND adjusted = ra_alloc_label();

        la_beq(field, zero_ir2_opnd, even_exponent);
        li_d(field, 0x1000);
        la_sub_w(result, result, field);
        la_b(adjusted);
        la_label(even_exponent);
        la_addi_w(result, result, -0x800);
        la_label(adjusted);
        ra_free_temp(adjusted);
        ra_free_temp(even_exponent);
    } else {
        li_d(field, 0x1000);
        la_sub_w(result, result, field);
    }
    li_d(field, UINT64_C(0x80000000));
    la_and(field, source, field);
    la_or(result, result, field);
    la_vinsgr2vr_w(value, result, lane);
    la_label(done);
    ra_free_temp(done);
    ra_free_temp(normal);
    ra_free_temp(subnormal);
    ra_free_temp(field);
    ra_free_temp(result);
    ra_free_temp(source);
}

static void lsx_fp_fix_rcp_estimate(IR2_OPND value, IR2_OPND src,
                                    bool rsqrt)
{
    for (int lane = 0; lane < 4; lane++) {
        lsx_fp_fix_rcp_estimate_lane(value, src, rsqrt, lane);
    }
}

static bool translate_avx_fp_unary_lsx(IR1_INST *pir1,
                                       avx_lsx_fp_unary_fn tr_inst,
                                       bool double_precision,
                                       bool approximate, bool rsqrt)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    bool ymm = ir1_opnd_is_ymm(opnd0);
    IR2_OPND src_low, src_high;
    IR2_OPND result_low = ra_alloc_ftemp();
    LsxFpStatus status;

    load_avx_lsx_operand(opnd1, ymm, &src_low, &src_high);
    lsx_fp_status_begin(&status);
    lsx_fp_apply_daz(src_low, status.mxcsr, status.flags,
                     double_precision, double_precision ? 2 : 4);
    tr_inst(result_low, src_low);
    if (ir1_opcode(pir1) == dt_X86_INS_VSQRTPD ||
        ir1_opcode(pir1) == dt_X86_INS_VSQRTPS) {
        lsx_fp_fix_sqrt_invalid_nan(result_low, src_low, double_precision,
                                    double_precision ? 2 : 4);
    }
    if (approximate) {
        lsx_fp_truncate_rcp_estimate(result_low);
        lsx_fp_fix_rcp_estimate(result_low, src_low, rsqrt);
    }
    lsx_fp_apply_fz(result_low, status.mxcsr, status.flags,
                    double_precision, double_precision ? 2 : 4);
    if (ymm) {
        IR2_OPND result_high = ra_alloc_ftemp();

        lsx_fp_apply_daz(src_high, status.mxcsr, status.flags,
                         double_precision, double_precision ? 2 : 4);
        tr_inst(result_high, src_high);
        if (ir1_opcode(pir1) == dt_X86_INS_VSQRTPD ||
            ir1_opcode(pir1) == dt_X86_INS_VSQRTPS) {
            lsx_fp_fix_sqrt_invalid_nan(result_high, src_high,
                                        double_precision,
                                        double_precision ? 2 : 4);
        }
        if (approximate) {
            lsx_fp_truncate_rcp_estimate(result_high);
            lsx_fp_fix_rcp_estimate(result_high, src_high, rsqrt);
        }
        lsx_fp_apply_fz(result_high, status.mxcsr, status.flags,
                        double_precision, double_precision ? 2 : 4);
        lsx_fp_status_finish(pir1, &status);
        store_avx_lsx_result(opnd0, result_low, result_high);
        ra_free_temp(result_high);
    } else {
        lsx_fp_status_finish(pir1, &status);
        store_avx_lsx_result(opnd0, result_low, result_low);
    }
    ra_free_temp(result_low);
    ra_free_temp(src_low);
    if (ymm) {
        ra_free_temp(src_high);
    }
    return true;
}

static bool translate_avx_fp_scalar_unary_lsx(IR1_INST *pir1,
                                              avx_lsx_fp_unary_fn tr_inst,
                                              bool is_double)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    IR2_OPND dest = load_freg128_from_ir1(opnd0);
    IR2_OPND src1 = load_freg128_from_ir1(opnd1);
    bool src2_is_temp;
    IR2_OPND src2 = load_avx_lsx_scalar_operand(opnd2, is_double,
                                                &src2_is_temp);
    IR2_OPND temp = ra_alloc_ftemp();
    LsxFpStatus status;

    lsx_fp_status_begin(&status);
    lsx_fp_apply_daz(src2, status.mxcsr, status.flags, is_double, 1);
    tr_inst(temp, src2);
    if (ir1_opcode(pir1) == dt_X86_INS_VSQRTSD ||
        ir1_opcode(pir1) == dt_X86_INS_VSQRTSS) {
        lsx_fp_fix_sqrt_invalid_nan(temp, src2, is_double, 1);
    }
    lsx_fp_apply_fz(temp, status.mxcsr, status.flags, is_double, 1);
    la_vori_b(dest, src1, 0);
    if (is_double) {
        la_vextrins_d(dest, temp, 0);
    } else {
        la_vextrins_w(dest, temp, 0);
    }
    lsx_fp_status_finish(pir1, &status);
    store_avx_lsx_result(opnd0, dest, dest);
    ra_free_temp(temp);
    if (src2_is_temp) {
        ra_free_temp(src2);
    }
    return true;
}

bool translate_vsqrtpd_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_unary_lsx(pir1, la_vfsqrt_d, true, false, false);
}

bool translate_vsqrtps_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_unary_lsx(pir1, la_vfsqrt_s, false, false, false);
}

bool translate_vsqrtsd_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_scalar_unary_lsx(pir1, la_vfsqrt_d, true);
}

bool translate_vsqrtss_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_scalar_unary_lsx(pir1, la_vfsqrt_s, false);
}

bool translate_vrcpps_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_unary_lsx(pir1, la_vfrecip_s, false, true, false);
}

bool translate_vrcpss_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_scalar_unary_lsx(pir1, la_vfrecip_s, false);
}

bool translate_vrsqrtps_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_unary_lsx(pir1, la_vfrsqrt_s, false, true, true);
}

bool translate_vrsqrtss_lsx(IR1_INST *pir1)
{
    return translate_avx_fp_scalar_unary_lsx(pir1, la_vfrsqrt_s, false);
}

/*
 * DPPD's horizontal NaN forwarding is lane-specific on the x86 executor.
 * LSX's vector add chooses the low product for both result lanes.  Preserve
 * the product NaN from each source lane when both products are NaNs; when
 * exactly one product is a NaN, that NaN is the horizontal sum for every
 * enabled destination lane.
 */
static void fix_vdppd_input_nan_lsx(IR2_OPND result, IR2_OPND product,
                                    uint8_t imm)
{
    IR2_OPND low = ra_alloc_itemp();
    IR2_OPND high = ra_alloc_itemp();
    IR2_OPND field = ra_alloc_itemp();
    IR2_OPND exponent = ra_alloc_itemp();
    IR2_OPND low_not_nan = ra_alloc_label();
    IR2_OPND only_low = ra_alloc_label();
    IR2_OPND only_high = ra_alloc_label();
    IR2_OPND done = ra_alloc_label();

    if (!(imm & 0x3)) {
        ra_free_temp(done);
        ra_free_temp(only_high);
        ra_free_temp(only_low);
        ra_free_temp(low_not_nan);
        ra_free_temp(exponent);
        ra_free_temp(field);
        ra_free_temp(high);
        ra_free_temp(low);
        return;
    }

    li_d(exponent, UINT64_C(0x7ff0000000000000));
    la_vpickve2gr_du(low, product, 0);
    la_and(field, low, exponent);
    la_bne(field, exponent, low_not_nan);
    li_d(field, UINT64_C(0x000fffffffffffff));
    la_and(field, low, field);
    la_beq(field, zero_ir2_opnd, low_not_nan);

    la_vpickve2gr_du(high, product, 1);
    la_and(field, high, exponent);
    la_bne(field, exponent, only_low);
    li_d(field, UINT64_C(0x000fffffffffffff));
    la_and(field, high, field);
    la_beq(field, zero_ir2_opnd, only_low);

    /* Both products are NaNs: preserve their corresponding output lane. */
    la_vpickve2gr_du(low, product, 0);
    if (imm & 0x1) {
        la_vinsgr2vr_d(result, low, 0);
    }
    if (imm & 0x2) {
        la_vinsgr2vr_d(result, high, 1);
    }
    la_b(done);

    la_label(low_not_nan);
    la_vpickve2gr_du(high, product, 1);
    la_and(field, high, exponent);
    la_bne(field, exponent, done);
    li_d(field, UINT64_C(0x000fffffffffffff));
    la_and(field, high, field);
    la_beq(field, zero_ir2_opnd, done);
    la_b(only_high);

    la_label(only_low);
    la_vpickve2gr_du(low, product, 0);
    if (imm & 0x1) {
        la_vinsgr2vr_d(result, low, 0);
    }
    if (imm & 0x2) {
        la_vinsgr2vr_d(result, low, 1);
    }
    la_b(done);

    la_label(only_high);
    if (imm & 0x1) {
        la_vinsgr2vr_d(result, high, 0);
    }
    if (imm & 0x2) {
        la_vinsgr2vr_d(result, high, 1);
    }
    la_label(done);
    ra_free_temp(done);
    ra_free_temp(only_high);
    ra_free_temp(only_low);
    ra_free_temp(low_not_nan);
    ra_free_temp(exponent);
    ra_free_temp(field);
    ra_free_temp(high);
    ra_free_temp(low);
}

static void translate_avx_dppd_lane_lsx(IR2_OPND result,
                                        IR2_OPND src1,
                                        IR2_OPND src2,
                                        uint8_t imm)
{
    IR2_OPND selected1 = ra_alloc_ftemp();
    IR2_OPND selected2 = ra_alloc_ftemp();
    IR2_OPND product = ra_alloc_ftemp();
    IR2_OPND even = ra_alloc_ftemp();
    IR2_OPND odd = ra_alloc_ftemp();
    IR2_OPND sum = ra_alloc_ftemp();

    la_vxor_v(selected1, selected1, selected1);
    la_vxor_v(selected2, selected2, selected2);
    if (imm & 0x10) {
        la_vextrins_d(selected1, src1, VEXTRINS_IMM_4_0(0, 0));
        la_vextrins_d(selected2, src2, VEXTRINS_IMM_4_0(0, 0));
    }
    if (imm & 0x20) {
        la_vextrins_d(selected1, src1, VEXTRINS_IMM_4_0(1, 1));
        la_vextrins_d(selected2, src2, VEXTRINS_IMM_4_0(1, 1));
    }
    la_vfmul_d(product, selected1, selected2);
    lsx_fp_fix_invalid_nan(product, selected1, selected2, true, 2);
    la_vpickev_d(even, product, product);
    la_vpickod_d(odd, product, product);
    la_vfadd_d(sum, even, odd);
    lsx_fp_fix_invalid_nan(sum, even, odd, true, 2);
    la_vxor_v(result, result, result);
    if (imm & 0x1) {
        la_vextrins_d(result, sum, VEXTRINS_IMM_4_0(0, 0));
    }
    if (imm & 0x2) {
        la_vextrins_d(result, sum, VEXTRINS_IMM_4_0(1, 1));
    }
    fix_vdppd_input_nan_lsx(result, product, imm);
    ra_free_temp(sum);
    ra_free_temp(odd);
    ra_free_temp(even);
    ra_free_temp(product);
    ra_free_temp(selected2);
    ra_free_temp(selected1);
}

/*
 * DPPS forwards NaNs through its two horizontal pairs.  On x86, destination
 * lanes 0/1 prefer product lanes 0/1 and destination lanes 2/3 prefer product
 * lanes 2/3.  Within each pair an even destination prefers the high lane and
 * an odd destination prefers the low lane.  The other pair is only a fallback.
 */
static void fix_vdpps_input_nan_output_lsx(IR2_OPND result, IR2_OPND product,
                                           int output_lane)
{
    int primary_base = output_lane & 0x2;
    int fallback_base = primary_base ^ 0x2;
    int candidates[4];
    IR2_OPND bits = ra_alloc_itemp();
    IR2_OPND field = ra_alloc_itemp();
    IR2_OPND exponent = ra_alloc_itemp();
    IR2_OPND done = ra_alloc_label();

    if (output_lane & 1) {
        candidates[0] = primary_base;
        candidates[1] = primary_base + 1;
        candidates[2] = fallback_base;
        candidates[3] = fallback_base + 1;
    } else {
        candidates[0] = primary_base + 1;
        candidates[1] = primary_base;
        candidates[2] = fallback_base + 1;
        candidates[3] = fallback_base;
    }

    li_d(exponent, UINT64_C(0x000000007f800000));
    for (int i = 0; i < 4; i++) {
        IR2_OPND next = ra_alloc_label();

        la_vpickve2gr_w(bits, product, candidates[i]);
        la_and(field, bits, exponent);
        la_bne(field, exponent, next);
        li_d(field, UINT64_C(0x00000000007fffff));
        la_and(field, bits, field);
        la_beq(field, zero_ir2_opnd, next);
        la_vinsgr2vr_w(result, bits, output_lane);
        la_b(done);
        la_label(next);
        ra_free_temp(next);
    }
    la_label(done);
    ra_free_temp(done);
    ra_free_temp(exponent);
    ra_free_temp(field);
    ra_free_temp(bits);
}

static void fix_vdpps_input_nan_lsx(IR2_OPND result, IR2_OPND product,
                                    uint8_t imm)
{
    for (int lane = 0; lane < 4; lane++) {
        if (imm & (1 << lane)) {
            fix_vdpps_input_nan_output_lsx(result, product, lane);
        }
    }
}

static void translate_avx_dpps_lane_lsx(IR2_OPND result,
                                        IR2_OPND src1,
                                        IR2_OPND src2,
                                        uint8_t imm)
{
    IR2_OPND selected1 = ra_alloc_ftemp();
    IR2_OPND selected2 = ra_alloc_ftemp();
    IR2_OPND product;

    la_vxor_v(selected1, selected1, selected1);
    la_vxor_v(selected2, selected2, selected2);
    if (imm & 0x10) {
        la_vextrins_w(selected1, src1, VEXTRINS_IMM_4_0(0, 0));
        la_vextrins_w(selected2, src2, VEXTRINS_IMM_4_0(0, 0));
    }
    if (imm & 0x20) {
        la_vextrins_w(selected1, src1, VEXTRINS_IMM_4_0(1, 1));
        la_vextrins_w(selected2, src2, VEXTRINS_IMM_4_0(1, 1));
    }
    if (imm & 0x40) {
        la_vextrins_w(selected1, src1, VEXTRINS_IMM_4_0(2, 2));
        la_vextrins_w(selected2, src2, VEXTRINS_IMM_4_0(2, 2));
    }
    if (imm & 0x80) {
        la_vextrins_w(selected1, src1, VEXTRINS_IMM_4_0(3, 3));
        la_vextrins_w(selected2, src2, VEXTRINS_IMM_4_0(3, 3));
    }
    product = ra_alloc_ftemp();
    la_vfmul_s(product, selected1, selected2);
    lsx_fp_fix_invalid_nan(product, selected1, selected2, false, 4);
    ra_free_temp(selected2);
    ra_free_temp(selected1);

    IR2_OPND even = ra_alloc_ftemp();
    IR2_OPND odd = ra_alloc_ftemp();
    la_vshuf4i_w(even, product, 0x88);
    la_vshuf4i_w(odd, product, 0xdd);
    IR2_OPND sum = ra_alloc_ftemp();
    la_vfadd_s(sum, even, odd);
    lsx_fp_fix_invalid_nan(sum, even, odd, false, 4);
    la_vshuf4i_w(even, sum, 0x00);
    la_vshuf4i_w(odd, sum, 0x55);
    la_vfadd_s(sum, even, odd);
    lsx_fp_fix_invalid_nan(sum, even, odd, false, 4);
    la_vxor_v(result, result, result);
    if (imm & 0x1) {
        la_vextrins_w(result, sum, VEXTRINS_IMM_4_0(0, 0));
    }
    if (imm & 0x2) {
        la_vextrins_w(result, sum, VEXTRINS_IMM_4_0(1, 1));
    }
    if (imm & 0x4) {
        la_vextrins_w(result, sum, VEXTRINS_IMM_4_0(2, 2));
    }
    if (imm & 0x8) {
        la_vextrins_w(result, sum, VEXTRINS_IMM_4_0(3, 3));
    }
    fix_vdpps_input_nan_lsx(result, product, imm);
    ra_free_temp(sum);
    ra_free_temp(odd);
    ra_free_temp(even);
    ra_free_temp(product);
}

bool translate_vdppd_lsx(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
    IR2_OPND src1 = load_freg128_from_ir1(opnd1);
    IR2_OPND src2 = load_freg128_from_ir1(opnd2);
    IR2_OPND result = ra_alloc_ftemp();
    LsxFpStatus status;

    lsassert(ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1));
    lsx_fp_status_begin(&status);
    lsx_fp_apply_daz(src1, status.mxcsr, status.flags, true, 2);
    lsx_fp_apply_daz(src2, status.mxcsr, status.flags, true, 2);
    translate_avx_dppd_lane_lsx(result, src1, src2, imm);
    lsx_fp_apply_fz(result, status.mxcsr, status.flags, true, 2);
    lsx_fp_status_finish(pir1, &status);
    store_avx_lsx_result(opnd0, result, result);
    ra_free_temp(result);
    return true;
}

bool translate_vdpps_lsx(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 3));
    bool ymm = ir1_opnd_is_ymm(opnd0);
    IR2_OPND src1_low, src1_high, src2_low, src2_high;
    IR2_OPND result_low = ra_alloc_ftemp();
    LsxFpStatus status;

    if (ir1_opnd_is_mem(opnd1)) {
        src1_low = load_v128_from_ir1_mem_exact(opnd1);
    } else {
        src1_low = ra_alloc_ftemp();
        la_vori_b(src1_low, ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1)), 0);
    }
    if (ir1_opnd_is_mem(opnd2)) {
        src2_low = load_v128_from_ir1_mem_exact(opnd2);
    } else {
        src2_low = ra_alloc_ftemp();
        la_vori_b(src2_low, ra_alloc_xmm(ir1_opnd_base_reg_num(opnd2)), 0);
    }
    lsx_fp_status_begin(&status);
    lsx_fp_apply_daz(src1_low, status.mxcsr, status.flags, false, 4);
    lsx_fp_apply_daz(src2_low, status.mxcsr, status.flags, false, 4);
    translate_avx_dpps_lane_lsx(result_low, src1_low, src2_low, imm);
    lsx_fp_apply_fz(result_low, status.mxcsr, status.flags, false, 4);
    if (ymm) {
        la_vori_b(ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0)), result_low, 0);
        ra_free_temp(result_low);
        ra_free_temp(src1_low);
        ra_free_temp(src2_low);

        src1_high = load_ymm_high128_shadow(ir1_opnd_base_reg_num(opnd1));
        if (ir1_opnd_is_mem(opnd2)) {
            src2_high = load_v256_high_from_ir1_mem_exact(opnd2);
        } else {
            src2_high = load_ymm_high128_shadow(
                ir1_opnd_base_reg_num(opnd2));
        }
        IR2_OPND result_high = ra_alloc_ftemp();

        lsx_fp_apply_daz(src1_high, status.mxcsr, status.flags, false, 4);
        lsx_fp_apply_daz(src2_high, status.mxcsr, status.flags, false, 4);
        translate_avx_dpps_lane_lsx(result_high, src1_high, src2_high, imm);
        lsx_fp_apply_fz(result_high, status.mxcsr, status.flags, false, 4);
        lsx_fp_status_finish(pir1, &status);
        store_ymm_high128_shadow(result_high, ir1_opnd_base_reg_num(opnd0));
        ra_free_temp(result_high);
    } else {
        lsx_fp_status_finish(pir1, &status);
        store_avx_lsx_result(opnd0, result_low, result_low);
    }
    if (!ymm) {
        ra_free_temp(result_low);
        ra_free_temp(src1_low);
        ra_free_temp(src2_low);
    }
    if (ymm) {
        ra_free_temp(src1_high);
        ra_free_temp(src2_high);
    }
    return true;
}

static void translate_avx_gather_lane_lsx(IR2_OPND dest,
                                          IR2_OPND mask_values,
                                          IR2_OPND index_values,
                                          IR2_OPND base_addr,
                                          IR2_OPND address,
                                          int scale,
                                          int lane,
                                          int index_lane,
                                          bool index64,
                                          bool value64)
{
    IR2_OPND mask_value = ra_alloc_itemp();
    IR2_OPND index_value = ra_alloc_itemp();
    IR2_OPND loaded = ra_alloc_itemp();
    IR2_OPND select = ra_alloc_label();
    IR2_OPND done = ra_alloc_label();

    if (index64) {
        la_vpickve2gr_d(index_value, index_values, index_lane);
    } else {
        la_vpickve2gr_w(index_value, index_values, index_lane);
    }

    /*
     * The mask has one element per gathered value, not per VSIB index.
     * VPGATHERQD/VGATHERQPS use qword indices with dword masks, while
     * VPGATHERDQ/VGATHERDPD use dword indices with qword masks.
     */
    if (value64) {
        la_vpickve2gr_d(mask_value, mask_values, lane);
    } else {
        la_vpickve2gr_w(mask_value, mask_values, lane);
    }

    la_blt(mask_value, zero_ir2_opnd, select);
    la_b(done);
    la_label(select);
    /* x86 gathers access memory only for lanes with a negative mask. */
    adjust_vsib_index(address, base_addr, index_value, scale);
    if (value64) {
        la_ld_d(loaded, address, 0);
    } else {
        la_ld_w(loaded, address, 0);
    }
    if (value64) {
        la_vinsgr2vr_d(dest, loaded, lane);
    } else {
        la_vinsgr2vr_w(dest, loaded, lane);
    }

    la_label(done);
    ra_free_temp(loaded);
    ra_free_temp(index_value);
    ra_free_temp(mask_value);
}

static bool translate_avx_gather_lsx(IR1_INST *pir1,
                                     bool index64,
                                     bool value64,
                                     bool ymm_allowed)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    bool ymm = ir1_opnd_is_ymm(opnd0);
    int dest_index = ir1_opnd_base_reg_num(opnd0);
    int mask_index = ir1_opnd_base_reg_num(opnd2);
    int index_index = ir1_opnd_vsib_index_reg_num(opnd1);
    bool has_base;
    longx offset;
    IR2_OPND base_addr = ra_alloc_itemp();
    IR2_OPND address = ra_alloc_itemp();
    IR2_OPND index_low = ra_alloc_ftemp();
    IR2_OPND index_high_values = { 0 };
    IR2_OPND mask_low_values = ra_alloc_ftemp();
    IR2_OPND mask_high_values = { 0 };
    IR2_OPND mask_low_store = ra_alloc_xmm(mask_index);
    IR2_OPND dest_low = ra_alloc_ftemp();
    IR2_OPND dest_high_values = { 0 };
    bool index_ymm;

    lsassert(ir1_opnd_is_mem(opnd1) && ir1_opnd_has_index(opnd1));
    lsassert((ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd2)) ||
             (ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd2)));
    lsassert(!ymm || ymm_allowed);
    if (ymm) {
        lsassert(ir1_opnd_is_ymm(opnd0) && ir1_opnd_is_ymm(opnd2));
    }
    index_ymm = ir1_index_reg_is_ymm(opnd1);

    has_base = ir1_opnd_has_base(opnd1);
    offset = ir1_opnd_simm(opnd1);
    li_guest_addr(base_addr, offset);
    if (has_base) {
        IR2_OPND base = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd1));

        la_add(base_addr, base_addr, base);
    }
    la_vori_b(index_low, ra_alloc_xmm(index_index), 0);
    la_vori_b(mask_low_values, mask_low_store, 0);
    la_vori_b(dest_low, ra_alloc_xmm(dest_index), 0);
    if (index_ymm) {
        index_high_values = load_ymm_high128_shadow(index_index);
    }
    if (ymm) {
        mask_high_values = load_ymm_high128_shadow(mask_index);
        dest_high_values = load_ymm_high128_shadow(dest_index);
    }

    /*
     * The value width determines the number of destination lanes.  The
     * vm64x forms have only two qword indices, while vm64y provides four.
     */
    int lanes_per_half = value64 ? 2 : 4;
    int half_count = ymm ? 2 : 1;

    /* VPGATHERQD and VGATHERQPS define the unused XMM high qword as zero. */
    if (index64 && !value64 && !index_ymm) {
        lanes_per_half = 2;
        la_vinsgr2vr_d(dest_low, zero_ir2_opnd, 1);
    }
    for (int half = 0; half < half_count; ++half) {
        IR2_OPND index_values = index_low;
        IR2_OPND mask_values = mask_low_values;
        IR2_OPND dest = dest_low;
        bool high = half != 0;

        if (high) {
            index_values = index_ymm ? index_high_values : index_low;
            mask_values = mask_high_values;
            dest = dest_high_values;
        }
        for (int lane = 0; lane < lanes_per_half; ++lane) {
            int index_lane = lane;

            /* vm64y has four qword indices split across two LSX values. */
            if (!high && index64 && index_ymm && lane >= 2) {
                index_values = index_high_values;
                index_lane -= 2;
            }

            if (high && !index_ymm) {
                index_lane += lanes_per_half;
            }
            translate_avx_gather_lane_lsx(
                dest, mask_values, index_values, base_addr,
                address, ir1_opnd_scale(opnd1), lane, index_lane,
                index64, value64);
        }
    }

    /* A successfully completed gather clears every mask element. */
    la_vori_b(ra_alloc_xmm(dest_index), dest_low, 0);
    la_vxor_v(mask_low_store, mask_low_store, mask_low_store);
    if (ymm) {
        store_ymm_high128_shadow(dest_high_values, dest_index);
        clear_ymm_high128_shadow(mask_index);
    } else {
        clear_ymm_high128_shadow(mask_index);
        clear_ymm_high128_shadow(dest_index);
    }
    if (ymm) {
        ra_free_temp(dest_high_values);
        ra_free_temp(mask_high_values);
    }
    if (index_ymm) {
        ra_free_temp(index_high_values);
    }
    ra_free_temp(dest_low);
    ra_free_temp(mask_low_values);
    ra_free_temp(index_low);
    ra_free_temp(address);
    ra_free_temp(base_addr);
    return true;
}

bool translate_vpgatherdd_lsx(IR1_INST *pir1)
{
    return translate_avx_gather_lsx(pir1, false, false, true);
}

bool translate_vpgatherqd_lsx(IR1_INST *pir1)
{
    return translate_avx_gather_lsx(pir1, true, false, false);
}

bool translate_vpgatherdq_lsx(IR1_INST *pir1)
{
    return translate_avx_gather_lsx(pir1, false, true, true);
}

bool translate_vpgatherqq_lsx(IR1_INST *pir1)
{
    return translate_avx_gather_lsx(pir1, true, true, true);
}

bool translate_vgatherdps_lsx(IR1_INST *pir1)
{
    return translate_vpgatherdd_lsx(pir1);
}

bool translate_vgatherqps_lsx(IR1_INST *pir1)
{
    return translate_vpgatherqd_lsx(pir1);
}

bool translate_vgatherdpd_lsx(IR1_INST *pir1)
{
    return translate_vpgatherdq_lsx(pir1);
}

bool translate_vgatherqpd_lsx(IR1_INST *pir1)
{
    return translate_vpgatherqq_lsx(pir1);
}

static bool translate_avx_integer_3op_opcode_lsx(IR1_INST *pir1)
{
    switch (ir1_opcode(pir1)) {
#define LATX_AVX_INTEGER_3OP_LSX_CASE(opcode, name, lsx_op) \
    case dt_X86_INS_##opcode: \
        return translate_v##name##_lsx(pir1);
        LATX_AVX_INTEGER_3OP_LSX_TABLE(LATX_AVX_INTEGER_3OP_LSX_CASE)
#undef LATX_AVX_INTEGER_3OP_LSX_CASE
#define LATX_AVX_INTEGER_REMAINING_3OP_LSX_CASE(opcode, name, lsx_op) \
    case dt_X86_INS_##opcode: \
        return translate_v##name##_lsx(pir1);
        LATX_AVX_INTEGER_REMAINING_3OP_LSX_TABLE(
            LATX_AVX_INTEGER_REMAINING_3OP_LSX_CASE)
#undef LATX_AVX_INTEGER_REMAINING_3OP_LSX_CASE
    default:
        return false;
    }
}

bool translate_vpaddx_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_opcode_lsx(pir1);
}

bool translate_vpsubx_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_opcode_lsx(pir1);
}

bool translate_vpminux_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_opcode_lsx(pir1);
}

bool translate_vpminxx_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_opcode_lsx(pir1);
}

bool translate_vpmaxxx_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_opcode_lsx(pir1);
}

bool translate_vpmullx_lsx(IR1_INST *pir1)
{
    return translate_avx_integer_3op_opcode_lsx(pir1);
}

bool translate_vmaxpx_lsx(IR1_INST *pir1)
{
    if (ir1_opcode(pir1) == dt_X86_INS_VMAXPD) {
        return translate_vmaxpd_lsx(pir1);
    }
    return translate_vmaxps_lsx(pir1);
}

bool translate_vmaxsx_lsx(IR1_INST *pir1)
{
    if (ir1_opcode(pir1) == dt_X86_INS_VMAXSD) {
        return translate_vmaxsd_lsx(pir1);
    }
    return translate_vmaxss_lsx(pir1);
}

bool translate_vminpx_lsx(IR1_INST *pir1)
{
    if (ir1_opcode(pir1) == dt_X86_INS_VMINPD) {
        return translate_vminpd_lsx(pir1);
    }
    return translate_vminps_lsx(pir1);
}

bool translate_vminsx_lsx(IR1_INST *pir1)
{
    if (ir1_opcode(pir1) == dt_X86_INS_VMINSD) {
        return translate_vminsd_lsx(pir1);
    }
    return translate_vminss_lsx(pir1);
}
#endif
