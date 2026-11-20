/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/* Exact-width guest accesses for hosts with strict vector alignment. */
#include "lsenv.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate.h"

static void latx_load_u64_bytes(IR2_OPND value, IR2_OPND byte,
                                IR2_OPND address, int disp)
{
    la_ld_bu(value, address, disp + 7);
    for (int i = 6; i >= 0; --i) {
        la_slli_d(value, value, 8);
        la_ld_bu(byte, address, disp + i);
        la_or(value, value, byte);
    }
}

void latx_load_f64(IR2_OPND dest, IR2_OPND base, int disp)
{
    if (!latx_no_lbt_mode_enabled()) {
        la_fld_d(dest, base, disp);
        return;
    }

    IR2_OPND address = ra_alloc_itemp();
    IR2_OPND value = ra_alloc_itemp();
    IR2_OPND byte = ra_alloc_itemp();
    IR2_OPND bytes = ra_alloc_label();
    IR2_OPND done = ra_alloc_label();

    if (si12_overflow(disp)) {
        li_d(value, disp);
        la_add_d(address, base, value);
    } else {
        la_addi_d(address, base, disp);
    }
    la_andi(value, address, 7);
    la_bne(value, zero_ir2_opnd, bytes);
    la_ld_d(value, address, 0);
    la_b(done);
    la_label(bytes);
    /* Read only the requested bytes, including at guest page boundaries. */
    latx_load_u64_bytes(value, byte, address, 0);
    la_label(done);
    la_movgr2fr_d(dest, value);

    ra_free_temp(byte);
    ra_free_temp(value);
    ra_free_temp(address);
}

static void latx_store_u64_bytes(IR2_OPND value, IR2_OPND address, int disp)
{
    for (int i = 0; i < 8; ++i) {
        la_st_b(value, address, disp + i);
        if (i != 7) {
            la_srli_d(value, value, 8);
        }
    }
}

void latx_load_v128(IR2_OPND dest, IR2_OPND base, int disp)
{
    IR2_OPND address;
    IR2_OPND scratch;
    IR2_OPND byte;
    IR2_OPND scalar;
    IR2_OPND done;

    if (!latx_no_lbt_mode_enabled()) {
        la_vld(dest, base, disp);
        return;
    }

    scalar = ra_alloc_label();
    done = ra_alloc_label();
    address = ra_alloc_itemp();
    scratch = ra_alloc_itemp();
    byte = ra_alloc_itemp();
    if (si12_overflow(disp)) {
        li_d(scratch, disp);
        la_add_d(address, base, scratch);
    } else if (disp) {
        la_addi_d(address, base, disp);
    } else {
        la_or(address, base, zero_ir2_opnd);
    }
    la_andi(scratch, address, 0xf);
    la_bne(scratch, zero_ir2_opnd, scalar);
    la_vld(dest, address, 0);
    la_b(done);

    la_label(scalar);
    /* Eight-byte alignment is sufficient for scalar doubleword accesses.
     * Touch exactly the original 16 bytes; never round the address down.
     */
    IR2_OPND bytes = ra_alloc_label();
    la_andi(scratch, address, 0x7);
    la_bne(scratch, zero_ir2_opnd, bytes);
    la_ld_d(scratch, address, 0);
    la_ld_d(byte, address, 8);
    la_vinsgr2vr_d(dest, scratch, 0);
    la_vinsgr2vr_d(dest, byte, 1);
    la_b(done);

    la_label(bytes);
    latx_load_u64_bytes(scratch, byte, address, 0);
    la_vinsgr2vr_d(dest, scratch, 0);
    latx_load_u64_bytes(scratch, byte, address, 8);
    la_vinsgr2vr_d(dest, scratch, 1);

    la_label(done);
    ra_free_temp(byte);
    ra_free_temp(scratch);
    ra_free_temp(address);
}

void latx_store_v128(IR2_OPND src, IR2_OPND base, int disp)
{
    IR2_OPND address;
    IR2_OPND scratch;
    IR2_OPND scalar;
    IR2_OPND done;

    if (!latx_no_lbt_mode_enabled()) {
        la_vst(src, base, disp);
        return;
    }

    scalar = ra_alloc_label();
    done = ra_alloc_label();
    address = ra_alloc_itemp();
    scratch = ra_alloc_itemp();
    if (si12_overflow(disp)) {
        li_d(scratch, disp);
        la_add_d(address, base, scratch);
    } else if (disp) {
        la_addi_d(address, base, disp);
    } else {
        la_or(address, base, zero_ir2_opnd);
    }
    la_andi(scratch, address, 0xf);
    la_bne(scratch, zero_ir2_opnd, scalar);
    la_vst(src, address, 0);
    la_b(done);

    la_label(scalar);
    IR2_OPND bytes = ra_alloc_label();
    la_andi(scratch, address, 0x7);
    la_bne(scratch, zero_ir2_opnd, bytes);
    la_vpickve2gr_du(scratch, src, 0);
    la_st_d(scratch, address, 0);
    la_vpickve2gr_du(scratch, src, 1);
    la_st_d(scratch, address, 8);
    la_b(done);

    la_label(bytes);
    la_vpickve2gr_du(scratch, src, 0);
    latx_store_u64_bytes(scratch, address, 0);
    la_vpickve2gr_du(scratch, src, 1);
    latx_store_u64_bytes(scratch, address, 8);

    la_label(done);
    ra_free_temp(scratch);
    ra_free_temp(address);
}
