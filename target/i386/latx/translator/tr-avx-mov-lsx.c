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
#include "latx-smc.h"

#ifdef CONFIG_LATX_AVX_OPT

bool translate_maskmovdqu_lsx(IR1_INST *pir1)
{
    IR2_OPND src = load_freg128_from_ir1(ir1_get_opnd(pir1, 0));
    IR2_OPND mask = load_freg128_from_ir1(ir1_get_opnd(pir1, 1));
    IR2_OPND zero = ra_alloc_ftemp();
    IR2_OPND base_opnd = ra_alloc_gpr(edi_index);
    IR2_OPND selected_mask = ra_alloc_ftemp();
    IR2_OPND unselected_mask = ra_alloc_ftemp();
    IR2_OPND mem_data;
    IR2_OPND selected_data = ra_alloc_ftemp();

    la_vxor_v(zero, zero, zero);
    la_vandi_b(selected_mask, mask, 0x80);
    la_vseq_b(unselected_mask, selected_mask, zero);
    la_vnor_v(selected_mask, unselected_mask, zero);
    mem_data = load_v128_from_guest_addr_exact(base_opnd);
    la_vand_v(selected_data, src, selected_mask);
    la_vand_v(mem_data, mem_data, unselected_mask);
    la_vor_v(mem_data, mem_data, selected_data);
    store_v128_to_guest_addr_exact(mem_data, base_opnd);

    ra_free_temp(selected_data);
    ra_free_temp(mem_data);
    ra_free_temp(unselected_mask);
    ra_free_temp(selected_mask);
    ra_free_temp(zero);
    return true;
}

static bool translate_vmovdqa_dqu_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);

    if (ir1_opnd_is_ymm(dest) && ir1_opnd_is_mem(src)) {
        int dest_index = ir1_opnd_base_reg_num(dest);
        IR2_OPND low;
        IR2_OPND high;

        load_v256_from_ir1_mem_exact(src, &low, &high);
        la_vori_b(ra_alloc_xmm(dest_index), low, 0);
        store_ymm_high128_shadow(high, dest_index);
        ra_free_temp(high);
        ra_free_temp(low);
    } else if (ir1_opnd_is_mem(dest) && ir1_opnd_is_ymm(src)) {
        int src_index = ir1_opnd_base_reg_num(src);
        IR2_OPND high;

        high = load_ymm_high128_shadow(src_index);
        store_v256_to_ir1_mem_exact(ra_alloc_xmm(src_index), high, dest);
        ra_free_temp(high);
    } else if (ir1_opnd_is_ymm(dest) && ir1_opnd_is_ymm(src)) {
        int dest_index = ir1_opnd_base_reg_num(dest);
        int src_index = ir1_opnd_base_reg_num(src);
        IR2_OPND high = load_ymm_high128_shadow(src_index);

        la_vori_b(ra_alloc_xmm(dest_index), ra_alloc_xmm(src_index), 0);
        store_ymm_high128_shadow(high, dest_index);
        ra_free_temp(high);
    } else if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_mem(src)) {
        int dest_index = ir1_opnd_base_reg_num(dest);
        IR2_OPND value;

        value = load_v128_from_ir1_mem_exact(src);
        la_vori_b(ra_alloc_xmm(dest_index), value, 0);
        clear_ymm_high128_shadow(dest_index);
        ra_free_temp(value);
    } else if (ir1_opnd_is_mem(dest) && ir1_opnd_is_xmm(src)) {
        store_v128_to_ir1_mem_exact(
            ra_alloc_xmm(ir1_opnd_base_reg_num(src)), dest);
    } else if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_xmm(src)) {
        int dest_index = ir1_opnd_base_reg_num(dest);
        IR2_OPND temp = ra_alloc_ftemp();

        la_vori_b(temp, ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 0);
        la_vori_b(ra_alloc_xmm(dest_index), temp, 0);
        clear_ymm_high128_shadow(dest_index);
        ra_free_temp(temp);
    } else {
#ifdef CONFIG_LATX_TS
        return false;
#endif
        lsassert(0);
    }
    return true;
}

bool translate_vmovdqa_lsx(IR1_INST *pir1)
{
    return translate_vmovdqa_dqu_lsx(pir1);
}

bool translate_vmovdqu_lsx(IR1_INST *pir1)
{
    return translate_vmovdqa_dqu_lsx(pir1);
}

bool translate_vmovups_lsx(IR1_INST * pir1) {
    {
        /* LSX-only path */
        IR1_OPND *dest = ir1_get_opnd(pir1, 0);
        IR1_OPND *src = ir1_get_opnd(pir1, 1);

        if (ir1_opnd_size(dest) == 256 && ir1_opnd_is_mem(src)) {
            int dest_index = ir1_opnd_base_reg_num(dest);
            IR2_OPND low;
            IR2_OPND high;

            load_v256_from_ir1_mem_exact(src, &low, &high);
            la_vori_b(ra_alloc_xmm(dest_index), low, 0);
            store_ymm_high128_shadow(high, dest_index);
            ra_free_temp(low);
            ra_free_temp(high);
        } else if (ir1_opnd_is_mem(dest) && ir1_opnd_size(src) == 256) {
            int src_index = ir1_opnd_base_reg_num(src);
            IR2_OPND high = load_ymm_high128_shadow(src_index);

            store_v256_to_ir1_mem_exact(
                ra_alloc_xmm(src_index), high, dest);
            ra_free_temp(high);
        } else if (ir1_opnd_size(dest) == 256 &&
                   ir1_opnd_size(src) == 256) {
            int dest_index = ir1_opnd_base_reg_num(dest);
            int src_index = ir1_opnd_base_reg_num(src);
            IR2_OPND high = load_ymm_high128_shadow(src_index);

            la_vori_b(ra_alloc_xmm(dest_index), ra_alloc_xmm(src_index), 0);
            store_ymm_high128_shadow(high, dest_index);
            ra_free_temp(high);
        } else if (ir1_opnd_size(dest) == 128 && ir1_opnd_is_mem(src)) {
            int dest_index = ir1_opnd_base_reg_num(dest);
            IR2_OPND value = load_v128_from_ir1_mem_exact(src);

            la_vori_b(ra_alloc_xmm(dest_index), value, 0);
            clear_ymm_high128_shadow(dest_index);
            ra_free_temp(value);
        } else if (ir1_opnd_is_mem(dest) && ir1_opnd_size(src) == 128) {
            store_v128_to_ir1_mem_exact(
                ra_alloc_xmm(ir1_opnd_base_reg_num(src)), dest);
        } else if (ir1_opnd_size(dest) == 128 &&
                   ir1_opnd_size(src) == 128) {
            int dest_index = ir1_opnd_base_reg_num(dest);
            IR2_OPND value = ra_alloc_ftemp();

            la_vori_b(value, ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 0);
            la_vori_b(ra_alloc_xmm(dest_index), value, 0);
            clear_ymm_high128_shadow(dest_index);
            ra_free_temp(value);
        } else {
#ifdef CONFIG_LATX_TS
            return false;
#endif
            lsassert(0);
        }
    }
    return true;
}

bool translate_vmovupd_lsx(IR1_INST *pir1)
{
    return translate_vmovups_lsx(pir1);
}

bool translate_vmovaps_lsx(IR1_INST *pir1)
{
    {
        /* LSX-only path */
        IR1_OPND *dest = ir1_get_opnd(pir1, 0);
        IR1_OPND *src = ir1_get_opnd(pir1, 1);

        if (ir1_opnd_is_ymm(dest) && ir1_opnd_is_mem(src)) {
            int dest_index = ir1_opnd_base_reg_num(dest);
            IR2_OPND address;
            IR2_OPND low = ra_alloc_ftemp();
            IR2_OPND high = ra_alloc_ftemp();

            address = convert_mem_to_itemp(src);
            gen_test_page_flag(address, 0, PAGE_READ, 32);
            la_vld(low, address, 0);
            la_vld(high, address, 16);
            la_vori_b(ra_alloc_xmm(dest_index), low, 0);
            store_ymm_high128_shadow(high, dest_index);
            ra_free_temp(address);
            ra_free_temp(low);
            ra_free_temp(high);
        } else if (ir1_opnd_is_mem(dest) && ir1_opnd_is_ymm(src)) {
            int src_index = ir1_opnd_base_reg_num(src);
            IR2_OPND address;
            IR2_OPND low = ra_alloc_xmm(src_index);
            IR2_OPND high = load_ymm_high128_shadow(src_index);

            address = convert_mem_to_itemp(dest);
            gen_test_page_flag(address, 0,
                               PAGE_WRITE | PAGE_WRITE_ORG, 32);
            la_vst(low, address, 0);
            la_vst(high, address, 16);
            ra_free_temp(address);
            ra_free_temp(high);
        } else if (ir1_opnd_is_ymm(dest) && ir1_opnd_is_ymm(src)) {
            int dest_index = ir1_opnd_base_reg_num(dest);
            int src_index = ir1_opnd_base_reg_num(src);
            IR2_OPND high = load_ymm_high128_shadow(src_index);

            la_vori_b(ra_alloc_xmm(dest_index), ra_alloc_xmm(src_index), 0);
            store_ymm_high128_shadow(high, dest_index);
            ra_free_temp(high);
        } else if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_mem(src)) {
            int dest_index = ir1_opnd_base_reg_num(dest);
            IR2_OPND address;
            IR2_OPND value = ra_alloc_ftemp();

            address = convert_mem_to_itemp(src);
            gen_test_page_flag(address, 0, PAGE_READ, 16);
            la_vld(value, address, 0);
            la_vori_b(ra_alloc_xmm(dest_index), value, 0);
            clear_ymm_high128_shadow(dest_index);
            ra_free_temp(address);
            ra_free_temp(value);
        } else if (ir1_opnd_is_mem(dest) && ir1_opnd_is_xmm(src)) {
            IR2_OPND address;

            address = convert_mem_to_itemp(dest);
            gen_test_page_flag(address, 0,
                               PAGE_WRITE | PAGE_WRITE_ORG, 16);
            la_vst(ra_alloc_xmm(ir1_opnd_base_reg_num(src)),
                   address, 0);
            ra_free_temp(address);
        } else if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_xmm(src)) {
            int dest_index = ir1_opnd_base_reg_num(dest);
            IR2_OPND value = ra_alloc_ftemp();

            la_vori_b(value, ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 0);
            la_vori_b(ra_alloc_xmm(dest_index), value, 0);
            clear_ymm_high128_shadow(dest_index);
            ra_free_temp(value);
        } else {
#ifdef CONFIG_LATX_TS
            return false;
#endif
            lsassert(0);
        }
    }
    return true;
}

bool translate_vmovapd_lsx(IR1_INST *pir1)
{
    return translate_vmovaps_lsx(pir1);
}

bool translate_vmovntdq_lsx(IR1_INST *pir1)
{
    return translate_vmovaps_lsx(pir1);
}

bool translate_vmovntpd_lsx(IR1_INST *pir1)
{
    return translate_vmovaps_lsx(pir1);
}

bool translate_vmovntps_lsx(IR1_INST *pir1)
{
    return translate_vmovaps_lsx(pir1);
}

bool translate_vlddqu_lsx(IR1_INST *pir1)
{
    return translate_vmovups_lsx(pir1);
}

static bool translate_vmovmsk_lsx(IR1_INST *pir1, bool is_pd)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    int src_index;
    IR2_OPND result = ra_alloc_itemp();
    IR2_OPND low_mask = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_gpr(dest_opnd));
    lsassert(ir1_opnd_is_xmm(src_opnd) || ir1_opnd_is_ymm(src_opnd));

    src_index = ir1_opnd_base_reg_num(src_opnd);
    if (is_pd) {
        la_vmskltz_d(low_mask, ra_alloc_xmm(src_index));
    } else {
        la_vmskltz_w(low_mask, ra_alloc_xmm(src_index));
    }
    la_movfr2gr_d(result, low_mask);

    if (ir1_opnd_is_ymm(src_opnd)) {
        IR2_OPND high_mask = ra_alloc_ftemp();
        IR2_OPND high_bits = ra_alloc_itemp();
        IR2_OPND high = load_ymm_high128_shadow(src_index);

        if (is_pd) {
            la_vmskltz_d(high_mask, high);
            la_movfr2gr_d(high_bits, high_mask);
            la_slli_d(high_bits, high_bits, 2);
        } else {
            la_vmskltz_w(high_mask, high);
            la_movfr2gr_d(high_bits, high_mask);
            la_slli_d(high_bits, high_bits, 4);
        }
        la_or(result, result, high_bits);
        ra_free_temp(high);
        ra_free_temp(high_bits);
        ra_free_temp(high_mask);
    }

    store_ireg_to_ir1(result, dest_opnd, false);
    ra_free_temp(low_mask);
    ra_free_temp(result);
    return true;
}

bool translate_vmovmskpd_lsx(IR1_INST *pir1)
{
    return translate_vmovmsk_lsx(pir1, true);
}

bool translate_vmovmskps_lsx(IR1_INST *pir1)
{
    return translate_vmovmsk_lsx(pir1, false);
}

static bool translate_vmovsdup_lsx(IR1_INST *pir1, bool odd)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);
    int dest_index;

    lsassert(ir1_opnd_is_xmm(dest) || ir1_opnd_is_ymm(dest));
    dest_index = ir1_opnd_base_reg_num(dest);

    if (ir1_opnd_is_ymm(dest)) {
        IR2_OPND src_low;
        IR2_OPND src_high;
        IR2_OPND result_low = ra_alloc_ftemp();
        IR2_OPND result_high = ra_alloc_ftemp();

        lsassert(ir1_opnd_is_ymm(src) ||
                 (ir1_opnd_is_mem(src) && ir1_opnd_size(src) == 256));
        if (ir1_opnd_is_mem(src)) {
            load_v256_from_ir1_mem_exact(src, &src_low, &src_high);
        } else {
            int src_index = ir1_opnd_base_reg_num(src);

            src_low = ra_alloc_ftemp();
            la_vori_b(src_low, ra_alloc_xmm(src_index), 0);
            src_high = load_ymm_high128_shadow(src_index);
        }

        if (odd) {
            la_vpackod_w(result_low, src_low, src_low);
            la_vpackod_w(result_high, src_high, src_high);
        } else {
            la_vpackev_w(result_low, src_low, src_low);
            la_vpackev_w(result_high, src_high, src_high);
        }
        la_vori_b(ra_alloc_xmm(dest_index), result_low, 0);
        store_ymm_high128_shadow(result_high, dest_index);
        ra_free_temp(result_high);
        ra_free_temp(result_low);
        ra_free_temp(src_high);
        ra_free_temp(src_low);
    } else {
        IR2_OPND src_value;
        IR2_OPND result = ra_alloc_ftemp();

        lsassert(ir1_opnd_is_xmm(src) ||
                 (ir1_opnd_is_mem(src) && ir1_opnd_size(src) == 128));
        if (ir1_opnd_is_mem(src)) {
            src_value = load_v128_from_ir1_mem_exact(src);
        } else {
            src_value = ra_alloc_ftemp();
            la_vori_b(src_value,
                      ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 0);
        }

        if (odd) {
            la_vpackod_w(result, src_value, src_value);
        } else {
            la_vpackev_w(result, src_value, src_value);
        }
        la_vori_b(ra_alloc_xmm(dest_index), result, 0);
        clear_ymm_high128_shadow(dest_index);
        ra_free_temp(result);
        ra_free_temp(src_value);
    }
    return true;
}

bool translate_vmovshdup_lsx(IR1_INST *pir1)
{
    return translate_vmovsdup_lsx(pir1, true);
}

bool translate_vmovsldup_lsx(IR1_INST *pir1)
{
    return translate_vmovsdup_lsx(pir1, false);
}

bool translate_vmovddup_lsx(IR1_INST * pir1) {
    lsassert(ir1_opnd_is_xmm(ir1_get_opnd(pir1, 0)) ||
        ir1_opnd_is_ymm(ir1_get_opnd(pir1, 0)));

    {
        /* LSX-only path */
        IR1_OPND *dest = ir1_get_opnd(pir1, 0);
        IR1_OPND *src = ir1_get_opnd(pir1, 1);
        int dest_index = ir1_opnd_base_reg_num(dest);

        if (ir1_opnd_is_ymm(dest)) {
            IR2_OPND src_low;
            IR2_OPND src_high;

            lsassert(ir1_opnd_is_ymm(src) ||
                     (ir1_opnd_is_mem(src) && ir1_opnd_size(src) == 256));
            if (ir1_opnd_is_mem(src)) {
                load_v256_from_ir1_mem_exact(src, &src_low, &src_high);
            } else {
                int src_index = ir1_opnd_base_reg_num(src);

                src_low = ra_alloc_ftemp();
                la_vori_b(src_low, ra_alloc_xmm(src_index), 0);
                src_high = load_ymm_high128_shadow(src_index);
            }

            la_vreplvei_d(src_low, src_low, 0);
            la_vreplvei_d(src_high, src_high, 0);
            la_vori_b(ra_alloc_xmm(dest_index), src_low, 0);
            store_ymm_high128_shadow(src_high, dest_index);
            ra_free_temp(src_low);
            ra_free_temp(src_high);
        } else {
            IR2_OPND result = ra_alloc_ftemp();

            lsassert(ir1_opnd_is_xmm(src) ||
                     (ir1_opnd_is_mem(src) && ir1_opnd_size(src) == 64));
            if (ir1_opnd_is_mem(src)) {
                IR2_OPND value = load_u64_from_ir1_mem_exact(src);

                la_vreplgr2vr_d(result, value);
                ra_free_temp(value);
            } else {
                IR2_OPND src_low = ra_alloc_ftemp();

                la_vori_b(src_low,
                          ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 0);
                la_vreplvei_d(result, src_low, 0);
                ra_free_temp(src_low);
            }

            la_vori_b(ra_alloc_xmm(dest_index), result, 0);
            clear_ymm_high128_shadow(dest_index);
            ra_free_temp(result);
        }
    }
    return true;
}

bool translate_vmovss_lsx(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    {
        /* LSX-only path */
        if (ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_xmm(opnd1)) {
            int dest_index = ir1_opnd_base_reg_num(opnd0);
            IR2_OPND dest = ra_alloc_xmm(dest_index);
            IR2_OPND temp = ra_alloc_ftemp();
            IR2_OPND src1 = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));
            IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
            IR2_OPND src2;
            bool src2_is_temp = false;

            if (ir1_opnd_is_mem(opnd2)) {
                IR2_OPND value = load_u32_from_ir1_mem_exact(opnd2);

                src2 = ra_alloc_ftemp();
                la_vxor_v(src2, src2, src2);
                la_vinsgr2vr_w(src2, value, 0);
                ra_free_temp(value);
                src2_is_temp = true;
            } else {
                lsassert(ir1_opnd_is_xmm(opnd2));
                src2 = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd2));
            }

            la_vori_b(temp, src1, 0);
            la_vextrins_w(temp, src2, VEXTRINS_IMM_4_0(0, 0));
            la_vori_b(dest, temp, 0);
            ra_free_temp(temp);
            if (src2_is_temp) {
                ra_free_temp(src2);
            }
            clear_ymm_high128_shadow(dest_index);
        } else if (ir1_opnd_is_xmm(opnd0) && ir1_opnd_is_mem(opnd1)) {
            int dest_index = ir1_opnd_base_reg_num(opnd0);
            IR2_OPND value = load_u32_from_ir1_mem_exact(opnd1);
            IR2_OPND dest = ra_alloc_xmm(dest_index);

            la_vxor_v(dest, dest, dest);
            la_vinsgr2vr_w(dest, value, 0);
            ra_free_temp(value);
            clear_ymm_high128_shadow(dest_index);
        } else if (ir1_opnd_is_mem(opnd0) && ir1_opnd_is_xmm(opnd1)) {
            IR2_OPND value = ra_alloc_itemp();

            la_vpickve2gr_wu(value,
                             ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1)), 0);
            store_u32_to_ir1_mem_exact(value, opnd0);
            ra_free_temp(value);
        } else {
            lsassert(0);
        }
    }
    return true;
}

bool translate_vmovd_lsx(IR1_INST *pir1)
{
    /*
     * vmovd r/m32, xmm1
     * vmovd xmm1, r/m32
     *   dest[255:32] = 0
     */
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    {
        /* LSX-only path */
        if (ir1_opnd_is_xmm(opnd0) &&
            (ir1_opnd_is_mem(opnd1) || ir1_opnd_is_gpr(opnd1))) {
            int dest_index = ir1_opnd_base_reg_num(opnd0);
            bool value_is_temp = ir1_opnd_is_mem(opnd1);
            IR2_OPND value = ir1_opnd_is_mem(opnd1) ?
                load_u32_from_ir1_mem_exact(opnd1) :
                load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
            IR2_OPND dest = ra_alloc_xmm(dest_index);

            la_vxor_v(dest, dest, dest);
            la_vinsgr2vr_w(dest, value, 0);
            clear_ymm_high128_shadow(dest_index);
            if (value_is_temp) {
                ra_free_temp(value);
            }
        } else if (ir1_opnd_is_mem(opnd0) && ir1_opnd_is_xmm(opnd1)) {
            IR2_OPND value = ra_alloc_itemp();
            IR2_OPND src = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));

            la_vpickve2gr_wu(value, src, 0);
            store_u32_to_ir1_mem_exact(value, opnd0);
            ra_free_temp(value);
        } else if (ir1_opnd_is_gpr(opnd0) && ir1_opnd_is_xmm(opnd1)) {
            IR2_OPND dest = ra_alloc_gpr(ir1_opnd_base_reg_num(opnd0));
            IR2_OPND src = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd1));

            la_vpickve2gr_wu(dest, src, 0);
        } else {
            lsassert(0);
        }
    }
    return true;
}

bool translate_vpmovmskb_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    int src_index = ir1_opnd_base_reg_num(src_opnd);
    IR2_OPND dest = ra_alloc_gpr(ir1_opnd_base_reg_num(dest_opnd));
    IR2_OPND low = ra_alloc_ftemp();
    IR2_OPND low_mask = ra_alloc_ftemp();

    lsassert(ir1_opnd_is_gpr(dest_opnd));
    lsassert(ir1_opnd_is_xmm(src_opnd) || ir1_opnd_is_ymm(src_opnd));
    la_vori_b(low, ra_alloc_xmm(src_index), 0);
    la_vmskltz_b(low_mask, low);
    la_movfr2gr_d(dest, low_mask);
    if (ir1_opnd_is_ymm(src_opnd)) {
        IR2_OPND high = load_ymm_high128_shadow(src_index);
        IR2_OPND high_mask = ra_alloc_ftemp();
        IR2_OPND high_bits = ra_alloc_itemp();

        la_vmskltz_b(high_mask, high);
        la_movfr2gr_d(high_bits, high_mask);
        la_slli_d(high_bits, high_bits, 16);
        la_or(dest, dest, high_bits);
        la_bstrpick_d(dest, dest, 31, 0);
        ra_free_temp(high_bits);
        ra_free_temp(high_mask);
        ra_free_temp(high);
    } else {
        la_bstrpick_d(dest, dest, 15, 0);
    }
    ra_free_temp(low_mask);
    ra_free_temp(low);
    return true;
}

/*
 * Emit a masked memory access for one 128-bit vector half.
 *
 * VMASKMOVPS/PD and VPMASKMOVD/Q only access the elements whose mask MSB is
 * set.  Loading or storing the complete vector and applying the mask in
 * registers is not equivalent: a masked-off element may live in an unmapped
 * page and must not fault.  Emit one scalar access per selected element so
 * that both the fault suppression and the fault address follow the x86
 * semantics.
 */
void latx_masked_mem_op_128(IR2_OPND address, IR2_OPND mask, IR2_OPND value,
                            int elem_bits, int lanes, int base_off, bool store)
{
    IR2_OPND mask_tmp = ra_alloc_ftemp();
    IR2_OPND mask_bits = ra_alloc_itemp();
    IR2_OPND selected = ra_alloc_itemp();
    IR2_OPND all_bits = ra_alloc_itemp();
    IR2_OPND element = ra_alloc_itemp();
    IR2_OPND label_none = ra_alloc_label();
    IR2_OPND label_full = ra_alloc_label();
    IR2_OPND label_done = ra_alloc_label();
    IR2_OPND label_skip[8];
    int all = (1 << lanes) - 1;
    int i;

    lsassert(elem_bits == 32 || elem_bits == 64);
    lsassert(lanes > 0 && lanes <= 8);

    if (elem_bits == 32) {
        la_vmskltz_w(mask_tmp, mask);
    } else {
        la_vmskltz_d(mask_tmp, mask);
    }
    la_movfr2gr_d(mask_bits, mask_tmp);

    if (!store) {
        la_vxor_v(value, value, value);
    }

    la_bstrpick_d(selected, mask_bits, lanes - 1, 0);
    la_beqz(selected, label_none);
    li_d(all_bits, all);
    la_beq(selected, all_bits, label_full);

    for (i = 0; i < lanes; i++) {
        int off = base_off + i * (elem_bits / 8);

        label_skip[i] = ra_alloc_label();
        la_bstrpick_d(selected, mask_bits, i, i);
        la_beqz(selected, label_skip[i]);
        if (store) {
            if (elem_bits == 32) {
                la_vpickve2gr_w(element, value, i);
                la_st_w(element, address, off);
            } else {
                la_vpickve2gr_d(element, value, i);
                la_st_d(element, address, off);
            }
        } else {
            if (elem_bits == 32) {
                la_ld_wu(element, address, off);
                la_vinsgr2vr_w(value, element, i);
            } else {
                la_ld_d(element, address, off);
                la_vinsgr2vr_d(value, element, i);
            }
        }
        la_label(label_skip[i]);
    }
    la_b(label_done);

    la_label(label_full);
    if (store) {
        la_vst(value, address, base_off);
    } else {
        la_vld(value, address, base_off);
    }
    la_b(label_done);

    la_label(label_none);
    la_label(label_done);

    ra_free_temp(element);
    ra_free_temp(all_bits);
    ra_free_temp(selected);
    ra_free_temp(mask_bits);
    ra_free_temp(mask_tmp);
}

/* Same as latx_masked_mem_op_128(), but using LASX registers.  For a 128-bit
 * instruction, only the low 128 bits of the value and mask are accessed. */
void latx_masked_mem_op_256(IR2_OPND address, IR2_OPND mask, IR2_OPND value,
                            int elem_bits, int lanes, int bits, int base_off,
                            bool store)
{
    IR2_OPND mask_tmp = ra_alloc_ftemp();
    IR2_OPND mask_bits = ra_alloc_itemp();
    IR2_OPND selected = ra_alloc_itemp();
    IR2_OPND all_bits = ra_alloc_itemp();
    IR2_OPND element = ra_alloc_itemp();
    IR2_OPND label_none = ra_alloc_label();
    IR2_OPND label_full = ra_alloc_label();
    IR2_OPND label_done = ra_alloc_label();
    IR2_OPND label_skip[8];
    int all = (1 << lanes) - 1;
    int i;

    lsassert(elem_bits == 32 || elem_bits == 64);
    lsassert(lanes > 0 && lanes <= 8);
    lsassert(bits == 128 || bits == 256);

    if (elem_bits == 32) {
        la_xvmskltz_w(mask_tmp, mask);
    } else {
        la_xvmskltz_d(mask_tmp, mask);
    }
    if (bits == 256) {
        IR2_OPND mask_high = ra_alloc_ftemp();

        la_xvpermi_q(mask_high, mask_tmp, VEXTRINS_IMM_4_0(1, 1));
        la_vslli_b(mask_high, mask_high, elem_bits == 32 ? 4 : 2);
        la_vor_v(mask_tmp, mask_tmp, mask_high);
        ra_free_temp(mask_high);
    }
    la_movfr2gr_d(mask_bits, mask_tmp);

    if (!store) {
        la_xvxor_v(value, value, value);
    }

    la_bstrpick_d(selected, mask_bits, lanes - 1, 0);
    la_beqz(selected, label_none);
    li_d(all_bits, all);
    la_beq(selected, all_bits, label_full);

    for (i = 0; i < lanes; i++) {
        int off = base_off + i * (elem_bits / 8);

        label_skip[i] = ra_alloc_label();
        la_bstrpick_d(selected, mask_bits, i, i);
        la_beqz(selected, label_skip[i]);
        if (store) {
            if (elem_bits == 32) {
                la_xvpickve2gr_w(element, value, i);
                la_st_w(element, address, off);
            } else {
                la_xvpickve2gr_d(element, value, i);
                la_st_d(element, address, off);
            }
        } else {
            if (elem_bits == 32) {
                la_ld_wu(element, address, off);
                la_xvinsgr2vr_w(value, element, i);
            } else {
                la_ld_d(element, address, off);
                la_xvinsgr2vr_d(value, element, i);
            }
        }
        la_label(label_skip[i]);
    }
    la_b(label_done);

    la_label(label_full);
    if (bits == 256) {
        if (store) {
            la_xvst(value, address, base_off);
        } else {
            la_xvld(value, address, base_off);
        }
    } else {
        if (store) {
            la_vst(value, address, base_off);
        } else {
            la_vld(value, address, base_off);
        }
    }
    la_b(label_done);

    la_label(label_none);
    la_label(label_done);

    ra_free_temp(element);
    ra_free_temp(all_bits);
    ra_free_temp(selected);
    ra_free_temp(mask_bits);
    ra_free_temp(mask_tmp);
}

bool translate_vmaskmovpx_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *mask_opnd = ir1_get_opnd(pir1, 1);
    IR1_OPND *value_opnd = ir1_get_opnd(pir1, 2);
    bool is_ymm = ir1_opnd_is_ymm(dest) ||
                  (ir1_opnd_is_mem(dest) && ir1_opnd_size(dest) == 256);
    bool is_pd = ir1_opcode(pir1) == dt_X86_INS_VMASKMOVPD;
    bool store = ir1_opnd_is_mem(dest);
    IR1_OPND *mem = store ? dest : value_opnd;
    int mask_index = ir1_opnd_base_reg_num(mask_opnd);
    int elem_bits = is_pd ? 64 : 32;
    int lanes = is_pd ? 2 : 4;
    IR2_OPND mask_low = ra_alloc_xmm(mask_index);
    IR2_OPND address;

    lsassert(ir1_opcode(pir1) == dt_X86_INS_VMASKMOVPS || is_pd);
    lsassert(ir1_opnd_is_xmm(mask_opnd) || ir1_opnd_is_ymm(mask_opnd));
    lsassert(ir1_opnd_is_xmm(value_opnd) || ir1_opnd_is_ymm(value_opnd) ||
             ir1_opnd_is_mem(value_opnd));
    lsassert(is_ymm ? ir1_opnd_is_ymm(mask_opnd) :
                      ir1_opnd_is_xmm(mask_opnd));

    address = convert_mem_to_itemp(mem);
    if (store) {
        int value_index = ir1_opnd_base_reg_num(value_opnd);
        IR2_OPND value_low = ra_alloc_xmm(value_index);

        latx_masked_mem_op_128(address, mask_low, value_low, elem_bits,
                               lanes, 0, true);
        if (is_ymm) {
            IR2_OPND mask_high = load_ymm_high128_shadow(mask_index);
            IR2_OPND value_high = load_ymm_high128_shadow(value_index);

            latx_masked_mem_op_128(address, mask_high, value_high, elem_bits,
                                   lanes, 16, true);
            ra_free_temp(value_high);
            ra_free_temp(mask_high);
        }
    } else {
        int dest_index = ir1_opnd_base_reg_num(dest);
        IR2_OPND result_low = ra_alloc_ftemp();

        latx_masked_mem_op_128(address, mask_low, result_low, elem_bits,
                               lanes, 0, false);
        if (is_ymm) {
            IR2_OPND mask_high = load_ymm_high128_shadow(mask_index);
            IR2_OPND result_high = ra_alloc_ftemp();

            latx_masked_mem_op_128(address, mask_high, result_high, elem_bits,
                                   lanes, 16, false);
            la_vori_b(ra_alloc_xmm(dest_index), result_low, 0);
            store_ymm_high128_shadow(result_high, dest_index);
            ra_free_temp(result_high);
            ra_free_temp(mask_high);
        } else {
            la_vori_b(ra_alloc_xmm(dest_index), result_low, 0);
            clear_ymm_high128_shadow(dest_index);
        }
        ra_free_temp(result_low);
    }
    ra_free_temp(address);
    return true;
}

bool translate_vmovq_lsx(IR1_INST * pir1) {
    {
        /* LSX-only path */
        IR1_OPND *dest = ir1_get_opnd(pir1, 0);
        IR1_OPND *src = ir1_get_opnd(pir1, 1);

        if (ir1_opnd_is_xmm(dest) &&
            (ir1_opnd_is_gpr(src) || ir1_opnd_is_xmm(src) ||
             ir1_opnd_is_mem(src))) {
            int dest_index = ir1_opnd_base_reg_num(dest);
            bool value_is_temp = !ir1_opnd_is_gpr(src);
            IR2_OPND value;

            if (ir1_opnd_is_xmm(src)) {
                value = ra_alloc_itemp();
                la_vpickve2gr_du(
                    value, ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 0);
            } else if (ir1_opnd_is_mem(src)) {
                value = load_u64_from_ir1_mem_exact(src);
            } else {
                value = load_ireg_from_ir1(src, UNKNOWN_EXTENSION, false);
            }

            IR2_OPND dest_reg = ra_alloc_xmm(dest_index);
            la_vxor_v(dest_reg, dest_reg, dest_reg);
            la_vinsgr2vr_d(dest_reg, value, 0);
            clear_ymm_high128_shadow(dest_index);
            if (value_is_temp) {
                ra_free_temp(value);
            }
        } else if (ir1_opnd_is_gpr(dest) && ir1_opnd_is_xmm(src)) {
            IR2_OPND dest_reg = ra_alloc_gpr(ir1_opnd_base_reg_num(dest));
            IR2_OPND src_reg = ra_alloc_xmm(ir1_opnd_base_reg_num(src));

            la_vpickve2gr_du(dest_reg, src_reg, 0);
        } else if (ir1_opnd_is_mem(dest) && ir1_opnd_is_xmm(src)) {
            IR2_OPND value = ra_alloc_itemp();

            la_vpickve2gr_du(
                value, ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 0);
            store_u64_to_ir1_mem_exact(value, dest);
            ra_free_temp(value);
        } else {
#ifdef CONFIG_LATX_TS
            return false;
#endif
            lsassert(0);
        }
    }
    return true;
}

static bool translate_vmovhpd_lpd_lsx(IR1_INST *pir1, bool high_lane)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);

    if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_xmm(src)) {
        IR1_OPND *memory = ir1_get_opnd(pir1, 2);
        IR2_OPND value = load_u64_from_ir1_mem_exact(memory);
        IR2_OPND dest_reg = ra_alloc_xmm(ir1_opnd_base_reg_num(dest));
        IR2_OPND src_reg = ra_alloc_xmm(ir1_opnd_base_reg_num(src));

        la_vori_b(dest_reg, src_reg, 0);
        la_vinsgr2vr_d(dest_reg, value, high_lane ? 1 : 0);
        clear_ymm_high128_shadow(ir1_opnd_base_reg_num(dest));
        ra_free_temp(value);
        return true;
    }

    if (ir1_opnd_is_mem(dest) && ir1_opnd_is_xmm(src)) {
        IR2_OPND value = ra_alloc_itemp();
        la_vpickve2gr_du(value, ra_alloc_xmm(ir1_opnd_base_reg_num(src)),
                         high_lane ? 1 : 0);
        store_u64_to_ir1_mem_exact(value, dest);
        ra_free_temp(value);
        return true;
    }

#ifdef CONFIG_LATX_TS
    return false;
#else
    lsassert(0);
    return true;
#endif
}

bool translate_vmovhpd_lsx(IR1_INST *pir1)
{
    return translate_vmovhpd_lpd_lsx(pir1, true);
}

bool translate_vmovlpd_lsx(IR1_INST *pir1)
{
    return translate_vmovhpd_lpd_lsx(pir1, false);
}

bool translate_vmovhps_lsx(IR1_INST *pir1)
{
    return translate_vmovhpd_lpd_lsx(pir1, true);
}

bool translate_vmovlps_lsx(IR1_INST *pir1)
{
    return translate_vmovhpd_lpd_lsx(pir1, false);
}

static void emit_vmovhlps_lsx_lane(IR2_OPND dest, IR2_OPND src1,
                                   IR2_OPND src2, bool high_to_low)
{
    if (high_to_low) {
        /* HLPS: src2.high -> dest.low, src1.high -> dest.high. */
        la_vilvh_d(dest, src1, src2);
    } else {
        /* LHPS: src1.low -> dest.low, src2.low -> dest.high. */
        la_vpickev_d(dest, src2, src1);
    }
}

static bool translate_vmovhlps_lhps_lsx(IR1_INST *pir1, bool high_to_low)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *src2 = ir1_get_opnd(pir1, 2);
    bool is_ymm = ir1_opnd_is_ymm(dest);
    int dest_index;
    IR2_OPND src1_low;
    IR2_OPND src2_low;
    IR2_OPND low_result;

    /* These instructions have register-only, same-width operands. */
    lsassert(ir1_opnd_is_xmm(dest) || is_ymm);
    lsassert(is_ymm ? ir1_opnd_is_ymm(src1) : ir1_opnd_is_xmm(src1));
    lsassert(is_ymm ? ir1_opnd_is_ymm(src2) : ir1_opnd_is_xmm(src2));

    dest_index = ir1_opnd_base_reg_num(dest);
    src1_low = ra_alloc_xmm(ir1_opnd_base_reg_num(src1));
    src2_low = ra_alloc_xmm(ir1_opnd_base_reg_num(src2));
    low_result = ra_alloc_ftemp();
    emit_vmovhlps_lsx_lane(low_result, src1_low, src2_low, high_to_low);

    if (is_ymm) {
        IR2_OPND src1_high = load_ymm_high128_shadow(
            ir1_opnd_base_reg_num(src1));
        IR2_OPND src2_high = load_ymm_high128_shadow(
            ir1_opnd_base_reg_num(src2));
        IR2_OPND high_result = ra_alloc_ftemp();

        emit_vmovhlps_lsx_lane(high_result, src1_high, src2_high,
                               high_to_low);
        la_vori_b(ra_alloc_xmm(dest_index), low_result, 0);
        store_ymm_high128_shadow(high_result, dest_index);
        ra_free_temp(high_result);
        ra_free_temp(src2_high);
        ra_free_temp(src1_high);
    } else {
        la_vori_b(ra_alloc_xmm(dest_index), low_result, 0);
        clear_ymm_high128_shadow(dest_index);
    }
    ra_free_temp(low_result);
    return true;
}

bool translate_vmovhlps_lsx(IR1_INST *pir1)
{
    return translate_vmovhlps_lhps_lsx(pir1, true);
}

bool translate_vmovlhps_lsx(IR1_INST *pir1)
{
    return translate_vmovhlps_lhps_lsx(pir1, false);
}

bool translate_vmovntdqa_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);
    int dest_index = ir1_opnd_base_reg_num(dest);

    lsassert(ir1_opnd_is_mem(src));
    if (ir1_opnd_is_xmm(dest)) {
        IR2_OPND value = load_v128_from_ir1_mem_exact(src);

        la_vori_b(ra_alloc_xmm(dest_index), value, 0);
        clear_ymm_high128_shadow(dest_index);
        ra_free_temp(value);
    } else if (ir1_opnd_is_ymm(dest)) {
        IR2_OPND low;
        IR2_OPND high;

        load_v256_from_ir1_mem_exact(src, &low, &high);
        la_vori_b(ra_alloc_xmm(dest_index), low, 0);
        store_ymm_high128_shadow(high, dest_index);
        ra_free_temp(high);
        ra_free_temp(low);
    } else {
        lsassert(0);
    }
    return true;
}

bool translate_vmovsd_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest = ir1_get_opnd(pir1, 0);
    IR1_OPND *src = ir1_get_opnd(pir1, 1);

    {
        /* LSX-only path */
        if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_xmm(src)) {
            IR1_OPND *src2 = ir1_get_opnd(pir1, 2);
            int dest_index = ir1_opnd_base_reg_num(dest);
            IR2_OPND low;
            IR2_OPND high = ra_alloc_itemp();
            IR2_OPND result = ra_alloc_ftemp();
            bool low_is_temp = false;

            if (ir1_opnd_is_mem(src2)) {
                low = load_u64_from_ir1_mem_exact(src2);
                low_is_temp = true;
            } else {
                lsassert(ir1_opnd_is_xmm(src2));
                low = ra_alloc_itemp();
                la_vpickve2gr_du(
                    low, ra_alloc_xmm(ir1_opnd_base_reg_num(src2)), 0);
                low_is_temp = true;
            }
            la_vpickve2gr_du(
                high, ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 1);
            la_vxor_v(result, result, result);
            la_vinsgr2vr_d(result, low, 0);
            la_vinsgr2vr_d(result, high, 1);
            la_vori_b(ra_alloc_xmm(dest_index), result, 0);
            clear_ymm_high128_shadow(dest_index);
            if (low_is_temp) {
                ra_free_temp(low);
            }
            ra_free_temp(high);
            ra_free_temp(result);
        } else if (ir1_opnd_is_xmm(dest) && ir1_opnd_is_mem(src)) {
            int dest_index = ir1_opnd_base_reg_num(dest);
            IR2_OPND low = load_u64_from_ir1_mem_exact(src);
            IR2_OPND result = ra_alloc_ftemp();

            la_vxor_v(result, result, result);
            la_vinsgr2vr_d(result, low, 0);
            la_vori_b(ra_alloc_xmm(dest_index), result, 0);
            clear_ymm_high128_shadow(dest_index);
            ra_free_temp(low);
            ra_free_temp(result);
        } else if (ir1_opnd_is_mem(dest) && ir1_opnd_is_xmm(src)) {
            IR2_OPND low = ra_alloc_itemp();

            la_vpickve2gr_du(
                low, ra_alloc_xmm(ir1_opnd_base_reg_num(src)), 0);
            store_u64_to_ir1_mem_exact(low, dest);
            ra_free_temp(low);
        } else {
            lsassert(0);
        }
    }
    return true;
}

#endif
