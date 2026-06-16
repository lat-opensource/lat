/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
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

const uint8 latx_vpaes_enc_tables[9][16] __attribute__((aligned(16))) = {
    {0x00,0x70,0x2a,0x5a,0x98,0xe8,0xb2,0xc2,0x08,0x78,0x22,0x52,0x90,0xe0,0xba,0xca},
    {0x00,0x4d,0x7c,0x31,0x7d,0x30,0x01,0x4c,0x81,0xcc,0xfd,0xb0,0xfc,0xb1,0x80,0xcd},
    {0x10,0x01,0x08,0x0d,0x0f,0x06,0x05,0x0e,0x02,0x0c,0x0b,0x0a,0x09,0x03,0x07,0x04},
    {0x10,0x07,0x0b,0x0f,0x06,0x0a,0x04,0x01,0x09,0x08,0x05,0x02,0x0c,0x0e,0x0d,0x03},
    {0x00,0xc7,0xbd,0x6f,0x17,0x6d,0xd2,0xd0,0x78,0xa8,0x02,0xc5,0x7a,0xbf,0xaa,0x15},
    {0x00,0x6a,0xbb,0x5f,0xa5,0x74,0xe4,0xcf,0xfa,0x35,0x2b,0x41,0xd1,0x90,0x1e,0x8e},
    {0x00,0x05,0x0a,0x0f,0x04,0x09,0x0e,0x03,0x08,0x0d,0x02,0x07,0x0c,0x01,0x06,0x0b},
    {0x00,0x02,0x04,0x06,0x08,0x0a,0x0c,0x0e,0x10,0x12,0x14,0x16,0x18,0x1a,0x1c,0x1e},
    {0x00,0x20,0x40,0x60,0x80,0xa0,0xc0,0xe0,0x1b,0x3b,0x5b,0x7b,0x9b,0xbb,0xdb,0xfb},
};

const uint8 latx_vpaes_dec_tables[9][16] __attribute__((aligned(16))) = {
    {0x00,0x5f,0x54,0x0b,0x04,0x5b,0x50,0x0f,0x1a,0x45,0x4e,0x11,0x1e,0x41,0x4a,0x15},
    {0x00,0x65,0x05,0x60,0xe6,0x83,0xe3,0x86,0x94,0xf1,0x91,0xf4,0x72,0x17,0x77,0x12},
    {0x10,0x01,0x08,0x0d,0x0f,0x06,0x05,0x0e,0x02,0x0c,0x0b,0x0a,0x09,0x03,0x07,0x04},
    {0x10,0x07,0x0b,0x0f,0x06,0x0a,0x04,0x01,0x09,0x08,0x05,0x02,0x0c,0x0e,0x0d,0x03},
    {0x00,0x40,0xf9,0x7e,0x53,0xea,0x87,0x13,0x2d,0x3e,0x94,0xd4,0xb9,0x6d,0xaa,0xc7},
    {0x00,0x1d,0x44,0x93,0x0f,0x56,0xd7,0x12,0x9c,0x8e,0xc5,0xd8,0x59,0x81,0x4b,0xca},
    {0x00,0x0d,0x0a,0x07,0x04,0x01,0x0e,0x0b,0x08,0x05,0x02,0x0f,0x0c,0x09,0x06,0x03},
    {0x00,0x02,0x04,0x06,0x08,0x0a,0x0c,0x0e,0x10,0x12,0x14,0x16,0x18,0x1a,0x1c,0x1e},
    {0x00,0x20,0x40,0x60,0x80,0xa0,0xc0,0xe0,0x1b,0x3b,0x5b,0x7b,0x9b,0xbb,0xdb,0xfb},
};

const uint8 latx_vpaes_keygen_tables[8][16] __attribute__((aligned(16))) = {
    {0x00,0x70,0x2a,0x5a,0x98,0xe8,0xb2,0xc2,0x08,0x78,0x22,0x52,0x90,0xe0,0xba,0xca},
    {0x00,0x4d,0x7c,0x31,0x7d,0x30,0x01,0x4c,0x81,0xcc,0xfd,0xb0,0xfc,0xb1,0x80,0xcd},
    {0x10,0x01,0x08,0x0d,0x0f,0x06,0x05,0x0e,0x02,0x0c,0x0b,0x0a,0x09,0x03,0x07,0x04},
    {0x10,0x07,0x0b,0x0f,0x06,0x0a,0x04,0x01,0x09,0x08,0x05,0x02,0x0c,0x0e,0x0d,0x03},
    {0x00,0xc7,0xbd,0x6f,0x17,0x6d,0xd2,0xd0,0x78,0xa8,0x02,0xc5,0x7a,0xbf,0xaa,0x15},
    {0x00,0x6a,0xbb,0x5f,0xa5,0x74,0xe4,0xcf,0xfa,0x35,0x2b,0x41,0xd1,0x90,0x1e,0x8e},
    {0x04,0x05,0x06,0x07,0x05,0x06,0x07,0x04,0x0c,0x0d,0x0e,0x0f,0x0d,0x0e,0x0f,0x0c},
    {0x00,0x00,0x00,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x00,0x00,0x00},
};

#define DUP16(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) \
    a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p

const uint8 latx_vpaes_enc_tables_xv[9][32] __attribute__((aligned(32))) = {
    {DUP16(0x00,0x70,0x2a,0x5a,0x98,0xe8,0xb2,0xc2,0x08,0x78,0x22,0x52,0x90,0xe0,0xba,0xca)},
    {DUP16(0x00,0x4d,0x7c,0x31,0x7d,0x30,0x01,0x4c,0x81,0xcc,0xfd,0xb0,0xfc,0xb1,0x80,0xcd)},
    {DUP16(0x10,0x01,0x08,0x0d,0x0f,0x06,0x05,0x0e,0x02,0x0c,0x0b,0x0a,0x09,0x03,0x07,0x04)},
    {DUP16(0x10,0x07,0x0b,0x0f,0x06,0x0a,0x04,0x01,0x09,0x08,0x05,0x02,0x0c,0x0e,0x0d,0x03)},
    {DUP16(0x00,0xc7,0xbd,0x6f,0x17,0x6d,0xd2,0xd0,0x78,0xa8,0x02,0xc5,0x7a,0xbf,0xaa,0x15)},
    {DUP16(0x00,0x6a,0xbb,0x5f,0xa5,0x74,0xe4,0xcf,0xfa,0x35,0x2b,0x41,0xd1,0x90,0x1e,0x8e)},
    {DUP16(0x00,0x05,0x0a,0x0f,0x04,0x09,0x0e,0x03,0x08,0x0d,0x02,0x07,0x0c,0x01,0x06,0x0b)},
    {DUP16(0x00,0x02,0x04,0x06,0x08,0x0a,0x0c,0x0e,0x10,0x12,0x14,0x16,0x18,0x1a,0x1c,0x1e)},
    {DUP16(0x00,0x20,0x40,0x60,0x80,0xa0,0xc0,0xe0,0x1b,0x3b,0x5b,0x7b,0x9b,0xbb,0xdb,0xfb)},
};

const uint8 latx_vpaes_dec_tables_xv[9][32] __attribute__((aligned(32))) = {
    {DUP16(0x00,0x5f,0x54,0x0b,0x04,0x5b,0x50,0x0f,0x1a,0x45,0x4e,0x11,0x1e,0x41,0x4a,0x15)},
    {DUP16(0x00,0x65,0x05,0x60,0xe6,0x83,0xe3,0x86,0x94,0xf1,0x91,0xf4,0x72,0x17,0x77,0x12)},
    {DUP16(0x10,0x01,0x08,0x0d,0x0f,0x06,0x05,0x0e,0x02,0x0c,0x0b,0x0a,0x09,0x03,0x07,0x04)},
    {DUP16(0x10,0x07,0x0b,0x0f,0x06,0x0a,0x04,0x01,0x09,0x08,0x05,0x02,0x0c,0x0e,0x0d,0x03)},
    {DUP16(0x00,0x40,0xf9,0x7e,0x53,0xea,0x87,0x13,0x2d,0x3e,0x94,0xd4,0xb9,0x6d,0xaa,0xc7)},
    {DUP16(0x00,0x1d,0x44,0x93,0x0f,0x56,0xd7,0x12,0x9c,0x8e,0xc5,0xd8,0x59,0x81,0x4b,0xca)},
    {DUP16(0x00,0x0d,0x0a,0x07,0x04,0x01,0x0e,0x0b,0x08,0x05,0x02,0x0f,0x0c,0x09,0x06,0x03)},
    {DUP16(0x00,0x02,0x04,0x06,0x08,0x0a,0x0c,0x0e,0x10,0x12,0x14,0x16,0x18,0x1a,0x1c,0x1e)},
    {DUP16(0x00,0x20,0x40,0x60,0x80,0xa0,0xc0,0xe0,0x1b,0x3b,0x5b,0x7b,0x9b,0xbb,0xdb,0xfb)},
};

#undef DUP16

const void *latx_vpaes_get_table_addr(int kind)
{
    switch (kind) {
    case VPAES_TABLE_ENC: return latx_vpaes_enc_tables;
    case VPAES_TABLE_DEC: return latx_vpaes_dec_tables;
    case VPAES_TABLE_KEYGEN: return latx_vpaes_keygen_tables;
    case VPAES_TABLE_ENC_XV: return latx_vpaes_enc_tables_xv;
    case VPAES_TABLE_DEC_XV: return latx_vpaes_dec_tables_xv;
    default: return NULL;
    }
}

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

static void vpaes_load_tables_lasx(int kind, int start, int count)
{
    IR2_OPND addr = ra_alloc_itemp();
    vpaes_load_addr(addr, kind);
    for (int i = 0; i < count; ++i) {
        la_xvld(vreg(i), addr, (start + i) * 32);
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

static void vpaes_subbytes_lasx(IR2_OPND dst, IR2_OPND zero, IR2_OPND t0,
                                IR2_OPND t1, IR2_OPND t2, IR2_OPND t3)
{
    la_xvxor_v(zero, zero, zero);
    la_xvandi_b(t0, dst, 0x0f);
    la_xvsrli_b(t1, dst, 4);
    la_xvshuf_b(t2, zero, vreg(VPAES_T1), t1);
    la_xvshuf_b(dst, zero, vreg(VPAES_T0), t0);
    la_xvxor_v(dst, dst, t2);
    la_xvandi_b(t0, dst, 0x0f);
    la_xvshuf_b(t2, zero, vreg(VPAES_T3), t0);
    la_xvsrli_b(t1, dst, 4);
    la_xvshuf_b(dst, zero, vreg(VPAES_T2), t1);
    la_xvxor_v(t3, t1, t0);
    la_xvxor_v(dst, dst, t2);
    la_xvshuf_b(t0, zero, vreg(VPAES_T2), t3);
    la_xvshuf_b(dst, zero, vreg(VPAES_T2), dst);
    la_xvxor_v(t2, t0, t2);
    la_xvxor_v(t0, dst, t3);
    la_xvshuf_b(t2, zero, vreg(VPAES_T2), t2);
    la_xvshuf_b(t0, zero, vreg(VPAES_T4), t0);
    la_xvxor_v(t2, t2, t1);
    la_xvshuf_b(t2, zero, vreg(VPAES_T5), t2);
    la_xvxor_v(dst, t0, t2);
    la_xvxori_b(dst, dst, 99);
}

static void vpaes_invsubbytes_lasx(IR2_OPND dst, IR2_OPND zero, IR2_OPND t0,
                                   IR2_OPND t1, IR2_OPND t2, IR2_OPND t3,
                                   IR2_OPND t4)
{
    la_xvxor_v(zero, zero, zero);
    la_xvxori_b(dst, dst, 99);
    la_xvandi_b(t0, dst, 0x0f);
    la_xvsrli_b(t1, dst, 4);
    la_xvshuf_b(t1, zero, vreg(VPAES_T1), t1);
    la_xvshuf_b(dst, zero, vreg(VPAES_T0), t0);
    la_xvxor_v(dst, dst, t1);
    la_xvandi_b(t0, dst, 0x0f);
    la_xvsrli_b(t1, dst, 4);
    la_xvshuf_b(t2, zero, vreg(VPAES_T2), t1);
    la_xvshuf_b(t3, zero, vreg(VPAES_T3), t0);
    la_xvxor_v(t0, t1, t0);
    la_xvxor_v(t2, t2, t3);
    la_xvshuf_b(t4, zero, vreg(VPAES_T2), t0);
    la_xvshuf_b(t2, zero, vreg(VPAES_T2), t2);
    la_xvxor_v(t3, t4, t3);
    la_xvxor_v(t0, t2, t0);
    la_xvshuf_b(t3, zero, vreg(VPAES_T2), t3);
    la_xvxor_v(t3, t3, t1);
    la_xvshuf_b(t0, zero, vreg(VPAES_T4), t0);
    la_xvshuf_b(t1, zero, vreg(VPAES_T5), t3);
    la_xvxor_v(dst, t0, t1);
}

static void vpaes_xtime_table_lasx(IR2_OPND dst, IR2_OPND src, IR2_OPND tmp,
                                   IR2_OPND tab_lo, IR2_OPND tab_hi)
{
    la_xvandi_b(tmp, src, 0x0f);
    la_xvsrli_b(dst, src, 4);
    la_xvshuf_b(tmp, tab_lo, tab_lo, tmp);
    la_xvshuf_b(dst, tab_hi, tab_hi, dst);
    la_xvxor_v(dst, dst, tmp);
}

static void vpaes_mixcolumns_xtime_lasx(IR2_OPND dst, IR2_OPND tab_lo,
                                        IR2_OPND tab_hi, IR2_OPND t0,
                                        IR2_OPND t1, IR2_OPND t2, IR2_OPND t3)
{
    vpaes_xtime_table_lasx(t0, dst, t3, tab_lo, tab_hi);
    la_xvshuf4i_b(t1, dst, 0x39);
    la_xvshuf4i_b(t2, dst, 0x4e);
    vpaes_xtime_table_lasx(dst, t1, t3, tab_lo, tab_hi);
    la_xvshuf4i_b(t3, t2, 0x39);
    la_xvxor_v(t2, t2, t3);
    la_xvxor_v(dst, dst, t1);
    la_xvxor_v(dst, dst, t0);
    la_xvxor_v(dst, dst, t2);
}

static void vpaes_invmixcolumns_xtime_lasx(IR2_OPND dst, IR2_OPND tab_lo,
                                           IR2_OPND tab_hi, IR2_OPND t0,
                                           IR2_OPND t1, IR2_OPND t2,
                                           IR2_OPND t3)
{
    la_xvshuf4i_b(t0, dst, 0x4e);
    la_xvxor_v(t0, t0, dst);
    vpaes_xtime_table_lasx(t0, t0, t1, tab_lo, tab_hi);
    vpaes_xtime_table_lasx(t0, t0, t1, tab_lo, tab_hi);
    la_xvxor_v(dst, dst, t0);
    vpaes_mixcolumns_xtime_lasx(dst, tab_lo, tab_hi, t0, t1, t2, t3);
}

static void vpaes_keygenassist_lsx(IR2_OPND dst, IR2_OPND zero, IR2_OPND t0,
                                   IR2_OPND t1, IR2_OPND t2, IR2_OPND t3,
                                   IR2_OPND rcon, uint8 imm)
{
    vpaes_subbytes_lsx(dst, zero, t0, t1, t2, t3);
    la_vshuf_b(dst, zero, dst, vreg(VPAES_T6));
    la_vldi(rcon, ((0 << 12) | imm));
    la_vand_v(rcon, rcon, vreg(VPAES_T7));
    la_vxor_v(dst, dst, rcon);
}

static IR2_OPND load_round_key(IR1_OPND *opnd, int need_copy, int lasx)
{
    if (!ir1_opnd_is_mem(opnd)) {
        IR2_OPND key = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd));
        if (!need_copy) {
            return key;
        }
        IR2_OPND key_tmp = ra_alloc_ftemp();
        if (lasx) {
            la_xvor_v(key_tmp, key, key);
        } else {
            la_vor_v(key_tmp, key, key);
        }
        return key_tmp;
    }

    IR2_OPND key_tmp = ra_alloc_ftemp();
    if (ir1_opnd_size(opnd) == 256) {
        load_freg256_from_ir1_mem(key_tmp, opnd);
    } else {
        load_freg128_from_ir1_mem(key_tmp, opnd);
    }
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
}

static void emit_aes_round_lasx(IR2_OPND dst, IR2_OPND key, int enc, int last)
{
    IR2_OPND lo_nibble = ra_alloc_ftemp();
    IR2_OPND hi_nibble = ra_alloc_ftemp();
    IR2_OPND table_mix = ra_alloc_ftemp();
    IR2_OPND mix_tmp0 = ra_alloc_ftemp();
    IR2_OPND mix_tmp1 = ra_alloc_ftemp();
    IR2_OPND zero = ra_alloc_ftemp();

    int table_kind = enc ? VPAES_TABLE_ENC_XV : VPAES_TABLE_DEC_XV;

    vpaes_load_tables_lasx(table_kind, 0, 7);
    if (enc) {
        vpaes_subbytes_lasx(dst, zero, lo_nibble, hi_nibble, table_mix,
                            mix_tmp0);
        la_xvshuf_b(dst, dst, dst, vreg(VPAES_T6));
        if (!last) {
            vpaes_load_tables_lasx(table_kind, 7, 2);
            vpaes_mixcolumns_xtime_lasx(dst, vreg(VPAES_T0), vreg(VPAES_T1),
                                        lo_nibble, hi_nibble, table_mix,
                                        mix_tmp0);
        }
    } else {
        la_xvshuf_b(dst, dst, dst, vreg(VPAES_T6));
        vpaes_invsubbytes_lasx(dst, zero, lo_nibble, hi_nibble, table_mix,
                               mix_tmp0, mix_tmp1);
        if (!last) {
            vpaes_load_tables_lasx(table_kind, 7, 2);
            vpaes_invmixcolumns_xtime_lasx(dst, vreg(VPAES_T0), vreg(VPAES_T1),
                                           lo_nibble, hi_nibble, table_mix,
                                           mix_tmp0);
        }
    }
    la_xvxor_v(dst, dst, key);
}

static bool translate_aes_round(IR1_INST *pir1, int enc, int last)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    int d = ir1_opnd_base_reg_num(opnd0);
    int lasx = option_enable_lasx;
    IR2_OPND dst = ra_alloc_xmm(d);
    int key_aliases_dst = !ir1_opnd_is_mem(opnd1) &&
                          d == ir1_opnd_base_reg_num(opnd1);
    IR2_OPND round_key = load_round_key(opnd1, key_aliases_dst, lasx);
    vpaes_spill_low_fprs();
    if (lasx) {
        emit_aes_round_lasx(dst, round_key, enc, last);
    } else {
        emit_aes_round_lsx(dst, round_key, enc, last);
    }
    vpaes_restore_low_fprs();
    return true;
}

static bool translate_vaes_round(IR1_INST *pir1, int enc, int last)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int d = ir1_opnd_base_reg_num(opnd0);
    int s1 = ir1_opnd_base_reg_num(opnd1);
    int ymm = ir1_opnd_is_ymm(opnd0);
    IR2_OPND dst = ra_alloc_xmm(d);
    IR2_OPND src1 = ra_alloc_xmm(s1);
    int key_aliases_dst = !ir1_opnd_is_mem(opnd2) &&
                          d == ir1_opnd_base_reg_num(opnd2);
    IR2_OPND round_key = load_round_key(opnd2, key_aliases_dst, ymm);

    if (dst._reg_num != src1._reg_num) {
        if (ymm) {
            la_xvor_v(dst, src1, src1);
        } else {
            la_vor_v(dst, src1, src1);
        }
    }

    vpaes_spill_low_fprs();
    if (ymm) {
        emit_aes_round_lasx(dst, round_key, enc, last);
    } else {
        emit_aes_round_lsx(dst, round_key, enc, last);
    }
    vpaes_restore_low_fprs();
    if (!ymm) {
        set_high128_xreg_to_zero(dst);
    }
    return true;
}

bool latx_translate_aesenc_vpaes(IR1_INST *pir1) { return translate_aes_round(pir1, 1, 0); }
bool latx_translate_aesenclast_vpaes(IR1_INST *pir1) { return translate_aes_round(pir1, 1, 1); }
bool latx_translate_aesdec_vpaes(IR1_INST *pir1) { return translate_aes_round(pir1, 0, 0); }
bool latx_translate_aesdeclast_vpaes(IR1_INST *pir1) { return translate_aes_round(pir1, 0, 1); }
bool latx_translate_vaesenc_vpaes(IR1_INST *pir1) { return translate_vaes_round(pir1, 1, 0); }
bool latx_translate_vaesenclast_vpaes(IR1_INST *pir1) { return translate_vaes_round(pir1, 1, 1); }
bool latx_translate_vaesdec_vpaes(IR1_INST *pir1) { return translate_vaes_round(pir1, 0, 0); }
bool latx_translate_vaesdeclast_vpaes(IR1_INST *pir1) { return translate_vaes_round(pir1, 0, 1); }

bool latx_translate_aesimc_vpaes(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    int d = ir1_opnd_base_reg_num(opnd0);
    IR2_OPND dst = ra_alloc_xmm(d);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    if (dst._reg_num != src._reg_num) {
        la_vor_v(dst, src, src);
    }
    IR2_OPND d0 = ra_alloc_ftemp();
    IR2_OPND d1 = ra_alloc_ftemp();
    IR2_OPND d2 = ra_alloc_ftemp();
    IR2_OPND v0 = ra_alloc_ftemp();
    vpaes_spill_low_fprs();
    vpaes_load_tables_lsx(VPAES_TABLE_DEC, 7, 2);
    vpaes_invmixcolumns_xtime_lsx(dst, vreg(VPAES_T0), vreg(VPAES_T1),
                                  d0, d1, d2, v0);
    vpaes_restore_low_fprs();
    return true;
}

bool latx_translate_vaesimc_vpaes(IR1_INST *pir1)
{
    bool ret = latx_translate_aesimc_vpaes(pir1);
    set_high128_xreg_to_zero(ra_alloc_xmm(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0))));
    return ret;
}

bool latx_translate_aeskeygenassist_vpaes(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int d = ir1_opnd_base_reg_num(opnd0);
    uint8 imm = ir1_opnd_uimm(opnd2);
    IR2_OPND dst = ra_alloc_xmm(d);
    IR2_OPND src = load_freg128_from_ir1(opnd1);
    if (dst._reg_num != src._reg_num) {
        la_vor_v(dst, src, src);
    }
    IR2_OPND zero = ra_alloc_ftemp();
    IR2_OPND d0 = ra_alloc_ftemp();
    IR2_OPND d1 = ra_alloc_ftemp();
    IR2_OPND d2 = ra_alloc_ftemp();
    IR2_OPND v0 = ra_alloc_ftemp();
    IR2_OPND v1 = ra_alloc_ftemp();
    vpaes_spill_low_fprs();
    vpaes_load_tables_lsx(VPAES_TABLE_KEYGEN, 0, 8);
    vpaes_keygenassist_lsx(dst, zero, d0, d1, d2, v0, v1, imm);
    vpaes_restore_low_fprs();
    return true;
}

bool latx_translate_vaeskeygenassist_vpaes(IR1_INST *pir1)
{
    bool ret = latx_translate_aeskeygenassist_vpaes(pir1);
    set_high128_xreg_to_zero(ra_alloc_xmm(ir1_opnd_base_reg_num(ir1_get_opnd(pir1, 0))));
    return ret;
}
