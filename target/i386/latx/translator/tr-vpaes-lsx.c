/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "common.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate.h"
#include "tr-vpaes.h"

enum {
    VPAES_T0 = 0,
    VPAES_T1 = 1,
    VPAES_T2 = 2,
    VPAES_T3 = 3,
    VPAES_T4 = 4,
    VPAES_T5 = 5,
    VPAES_T6 = 6,
    VPAES_T7 = 7,
};

enum {
    VPAES_TABLE_ENC,
    VPAES_TABLE_DEC,
    VPAES_TABLE_KEYGEN,
    VPAES_TABLE_ENC_XV,
    VPAES_TABLE_DEC_XV,
};

#define DUP16(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) \
    a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p

#undef DUP16

static inline IR2_OPND vreg(int i)
{
    return ir2_opnd_new(IR2_OPND_FPR, i);
}

static void vpaes_spill_low_fprs(void)
{
    if (!option_softfpu) {
        IR2_OPND top = ra_alloc_itemp();
        la_x86mftop(top);
        la_st_w(top, env_ir2_opnd, lsenv_offset_of_top(lsenv));
        ra_free_temp(top);
    }
    tr_fpu_disable_top_mode();
    for (int i = 0; i < 8; ++i) {
        la_fst_d(ra_alloc_mmx(i), env_ir2_opnd, lsenv_offset_of_mmx(lsenv, i));
    }
}

static void vpaes_restore_low_fprs(void)
{
    for (int i = 0; i < 8; ++i) {
        la_fld_d(ra_alloc_mmx(i), env_ir2_opnd, lsenv_offset_of_mmx(lsenv, i));
    }
    tr_fpu_enable_top_mode();
    tr_load_top_from_env();
}

#ifdef CONFIG_LATX_AOT
static enum aot_rel_kind vpaes_table_rel_kind(int kind)
{
    switch (kind) {
    case VPAES_TABLE_ENC: return LOAD_VPAES_ENC_TABLES;
    case VPAES_TABLE_DEC: return LOAD_VPAES_DEC_TABLES;
    case VPAES_TABLE_KEYGEN: return LOAD_VPAES_KEYGEN_TABLES;
    case VPAES_TABLE_ENC_XV: return LOAD_VPAES_ENC_TABLES_XV;
    case VPAES_TABLE_DEC_XV: return LOAD_VPAES_DEC_TABLES_XV;
    default:
        lsassertm(0, "invalid vpaes table kind %d\n", kind);
        abort();
    }
}

#endif
static void vpaes_load_addr(IR2_OPND addr, int kind)
{
    aot_load_host_addr(addr, (ADDR)latx_vpaes_get_table_addr(kind),
                       vpaes_table_rel_kind(kind), 0);
}

static void vpaes_load_tables_lsx(int kind, int start, int count)
{
    IR2_OPND addr = ra_alloc_itemp();
    vpaes_load_addr(addr, kind);
    for (int i = 0; i < count; ++i) {
        la_vld(vreg(i), addr, (start + i) * 16);
    }
    ra_free_temp(addr);
}

static void vpaes_subbytes_lsx(IR2_OPND dst, IR2_OPND zero, IR2_OPND t0,
                               IR2_OPND t1, IR2_OPND t2, IR2_OPND t3)
{
    la_vxor_v(zero, zero, zero);
    la_vandi_b(t0, dst, 0x0f);
    la_vsrli_b(t1, dst, 4);
    la_vshuf_b(t2, zero, vreg(VPAES_T1), t1);
    la_vshuf_b(dst, zero, vreg(VPAES_T0), t0);
    la_vxor_v(dst, dst, t2);
    la_vandi_b(t0, dst, 0x0f);
    la_vshuf_b(t2, zero, vreg(VPAES_T3), t0);
    la_vsrli_b(t1, dst, 4);
    la_vshuf_b(dst, zero, vreg(VPAES_T2), t1);
    la_vxor_v(t3, t1, t0);
    la_vxor_v(dst, dst, t2);
    la_vshuf_b(t0, zero, vreg(VPAES_T2), t3);
    la_vshuf_b(dst, zero, vreg(VPAES_T2), dst);
    la_vxor_v(t2, t0, t2);
    la_vxor_v(t0, dst, t3);
    la_vshuf_b(t2, zero, vreg(VPAES_T2), t2);
    la_vshuf_b(t0, zero, vreg(VPAES_T4), t0);
    la_vxor_v(t2, t2, t1);
    la_vshuf_b(t2, zero, vreg(VPAES_T5), t2);
    la_vxor_v(dst, t0, t2);
    la_vxori_b(dst, dst, 99);
}

static void vpaes_invsubbytes_lsx(IR2_OPND dst, IR2_OPND zero, IR2_OPND t0,
                                  IR2_OPND t1, IR2_OPND t2, IR2_OPND t3,
                                  IR2_OPND t4)
{
    la_vxor_v(zero, zero, zero);
    la_vxori_b(dst, dst, 99);
    la_vandi_b(t0, dst, 0x0f);
    la_vsrli_b(t1, dst, 4);
    la_vshuf_b(t1, zero, vreg(VPAES_T1), t1);
    la_vshuf_b(dst, zero, vreg(VPAES_T0), t0);
    la_vxor_v(dst, dst, t1);
    la_vandi_b(t0, dst, 0x0f);
    la_vsrli_b(t1, dst, 4);
    la_vshuf_b(t2, zero, vreg(VPAES_T2), t1);
    la_vshuf_b(t3, zero, vreg(VPAES_T3), t0);
    la_vxor_v(t0, t1, t0);
    la_vxor_v(t2, t2, t3);
    la_vshuf_b(t4, zero, vreg(VPAES_T2), t0);
    la_vshuf_b(t2, zero, vreg(VPAES_T2), t2);
    la_vxor_v(t3, t4, t3);
    la_vxor_v(t0, t2, t0);
    la_vshuf_b(t3, zero, vreg(VPAES_T2), t3);
    la_vxor_v(t3, t3, t1);
    la_vshuf_b(t0, zero, vreg(VPAES_T4), t0);
    la_vshuf_b(t1, zero, vreg(VPAES_T5), t3);
    la_vxor_v(dst, t0, t1);
}

static void vpaes_xtime_table_lsx(IR2_OPND dst, IR2_OPND src, IR2_OPND tmp,
                                  IR2_OPND tab_lo, IR2_OPND tab_hi)
{
    la_vandi_b(tmp, src, 0x0f);
    la_vsrli_b(dst, src, 4);
    la_vshuf_b(tmp, tab_lo, tab_lo, tmp);
    la_vshuf_b(dst, tab_hi, tab_hi, dst);
    la_vxor_v(dst, dst, tmp);
}

static void vpaes_mixcolumns_xtime_lsx(IR2_OPND dst, IR2_OPND tab_lo,
                                       IR2_OPND tab_hi, IR2_OPND t0,
                                       IR2_OPND t1, IR2_OPND t2, IR2_OPND t3)
{
    vpaes_xtime_table_lsx(t0, dst, t3, tab_lo, tab_hi);
    la_vshuf4i_b(t1, dst, 0x39);
    la_vshuf4i_b(t2, dst, 0x4e);
    vpaes_xtime_table_lsx(dst, t1, t3, tab_lo, tab_hi);
    la_vshuf4i_b(t3, t2, 0x39);
    la_vxor_v(t2, t2, t3);
    la_vxor_v(dst, dst, t1);
    la_vxor_v(dst, dst, t0);
    la_vxor_v(dst, dst, t2);
}

static void vpaes_invmixcolumns_xtime_lsx(IR2_OPND dst, IR2_OPND tab_lo,
                                          IR2_OPND tab_hi, IR2_OPND t0,
                                          IR2_OPND t1, IR2_OPND t2,
                                          IR2_OPND t3)
{
    la_vshuf4i_b(t0, dst, 0x4e);
    la_vxor_v(t0, t0, dst);
    vpaes_xtime_table_lsx(t0, t0, t1, tab_lo, tab_hi);
    vpaes_xtime_table_lsx(t0, t0, t1, tab_lo, tab_hi);
    la_vxor_v(dst, dst, t0);
    vpaes_mixcolumns_xtime_lsx(dst, tab_lo, tab_hi, t0, t1, t2, t3);
}

static IR2_OPND load_round_key(IR1_OPND *opnd, int need_copy)
{
    if (!ir1_opnd_is_mem(opnd)) {
        IR2_OPND key = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd));
        if (!need_copy) {
            return key;
        }
        IR2_OPND key_tmp = ra_alloc_ftemp();
        la_vor_v(key_tmp, key, key);
        return key_tmp;
    }

    lsassert(ir1_opnd_size(opnd) == 128);
    IR2_OPND key_tmp = ra_alloc_ftemp();
    load_freg128_from_ir1_mem(key_tmp, opnd);
    return key_tmp;
}

static void emit_aes_round_lsx(IR2_OPND dst, IR2_OPND key, int enc, int last)
{
    IR2_OPND lo_nibble = ra_alloc_ftemp();
    IR2_OPND hi_nibble = ra_alloc_ftemp();
    IR2_OPND table_mix = ra_alloc_ftemp();
    IR2_OPND mix_tmp0 = ra_alloc_ftemp();
    IR2_OPND mix_tmp1 = ra_alloc_ftemp();
    IR2_OPND zero = ra_alloc_ftemp();

    int table_kind = enc ? VPAES_TABLE_ENC : VPAES_TABLE_DEC;

    vpaes_load_tables_lsx(table_kind, 0, 7);
    if (enc) {
        vpaes_subbytes_lsx(dst, zero, lo_nibble, hi_nibble, table_mix,
                           mix_tmp0);
        la_vshuf_b(dst, dst, dst, vreg(VPAES_T6));
        if (!last) {
            vpaes_load_tables_lsx(table_kind, 7, 2);
            vpaes_mixcolumns_xtime_lsx(dst, vreg(VPAES_T0), vreg(VPAES_T1),
                                       lo_nibble, hi_nibble, table_mix,
                                       mix_tmp0);
        }
    } else {
        la_vshuf_b(dst, dst, dst, vreg(VPAES_T6));
        vpaes_invsubbytes_lsx(dst, zero, lo_nibble, hi_nibble, table_mix,
                              mix_tmp0, mix_tmp1);
        if (!last) {
            vpaes_load_tables_lsx(table_kind, 7, 2);
            vpaes_invmixcolumns_xtime_lsx(dst, vreg(VPAES_T0), vreg(VPAES_T1),
                                          lo_nibble, hi_nibble, table_mix,
                                          mix_tmp0);
        }
    }
    la_vxor_v(dst, dst, key);
    ra_free_temp(zero);
    ra_free_temp(mix_tmp1);
    ra_free_temp(mix_tmp0);
    ra_free_temp(table_mix);
    ra_free_temp(hi_nibble);
    ra_free_temp(lo_nibble);
}

static bool translate_vaes_round_lsx(IR1_INST *pir1, int enc, int last)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int d = ir1_opnd_base_reg_num(opnd0);
    int s1 = ir1_opnd_base_reg_num(opnd1);

    if (ir1_opnd_is_xmm(opnd0)) {
        IR2_OPND dst = ra_alloc_xmm(d);
        IR2_OPND src = ra_alloc_xmm(s1);
        bool key_aliases_dst = !ir1_opnd_is_mem(opnd2) &&
                               d == ir1_opnd_base_reg_num(opnd2);
        IR2_OPND key = load_round_key(opnd2, key_aliases_dst);

        if (d != s1) {
            la_vor_v(dst, src, src);
        }
        vpaes_spill_low_fprs();
        emit_aes_round_lsx(dst, key, enc, last);
        vpaes_restore_low_fprs();
        ra_free_temp_auto(key);
        clear_ymm_high128_shadow(d);
        return true;
    }

    IR2_OPND src_low = ra_alloc_xmm(s1);
    IR2_OPND key_low;

    if (ir1_opnd_is_mem(opnd2)) {
        key_low = load_v128_from_ir1_mem_exact(opnd2);
    } else {
        int key_index = ir1_opnd_base_reg_num(opnd2);

        key_low = ra_alloc_xmm(key_index);
        if (d == key_index) {
            IR2_OPND key_low_copy = ra_alloc_ftemp();

            la_vor_v(key_low_copy, key_low, key_low);
            key_low = key_low_copy;
        }
    }

    IR2_OPND dst = ra_alloc_xmm(d);
    if (d != s1) {
        la_vor_v(dst, src_low, src_low);
    }

    vpaes_spill_low_fprs();
    emit_aes_round_lsx(dst, key_low, enc, last);
    ra_free_temp_auto(key_low);

    /* Process the high half after releasing low-half temporaries. */
    IR2_OPND dst_high = load_ymm_high128_shadow(s1);
    IR2_OPND key_high;

    if (ir1_opnd_is_mem(opnd2)) {
        key_high = load_v256_high_from_ir1_mem_exact(opnd2);
    } else {
        key_high = load_ymm_high128_shadow(ir1_opnd_base_reg_num(opnd2));
    }
    emit_aes_round_lsx(dst_high, key_high, enc, last);
    vpaes_restore_low_fprs();
    store_ymm_high128_shadow(dst_high, d);
    ra_free_temp(dst_high);
    ra_free_temp(key_high);
    return true;
}

bool latx_translate_vaesenc_lsx(IR1_INST *pir1) { return translate_vaes_round_lsx(pir1, 1, 0); }
bool latx_translate_vaesenclast_lsx(IR1_INST *pir1) { return translate_vaes_round_lsx(pir1, 1, 1); }
bool latx_translate_vaesdec_lsx(IR1_INST *pir1) { return translate_vaes_round_lsx(pir1, 0, 0); }
bool latx_translate_vaesdeclast_lsx(IR1_INST *pir1) { return translate_vaes_round_lsx(pir1, 0, 1); }
