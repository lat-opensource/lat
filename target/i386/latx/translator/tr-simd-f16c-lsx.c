/* LSX implementation split from tr-simd-f16c.c. */
/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "common.h"
#include "reg-alloc.h"
#include "lsenv.h"
#include "latx-options.h"
#include "translate.h"

#ifdef CONFIG_LATX_AVX_OPT
static void call_f16c_helper_lsx(ADDR helper, int dest_index, int src_index,
                                 int imm, int rel_kind)
{
    IR2_OPND saved_fcsr[4];
    IR2_OPND fcsr[4] = {
        fcsr_ir2_opnd, fcsr1_ir2_opnd, fcsr2_ir2_opnd, fcsr3_ir2_opnd,
    };

    /* The C helper updates guest MXCSR itself; do not leak any host FCSR. */
    for (int i = 0; i < 4; ++i) {
        saved_fcsr[i] = ra_alloc_itemp();
        la_movfcsr2gr(saved_fcsr[i], fcsr[i]);
    }
    tr_gen_call_to_helper_pcmpxstrx(helper, dest_index, src_index, imm,
                                    rel_kind);
    for (int i = 0; i < 4; ++i) {
        la_movgr2fcsr(fcsr[i], saved_fcsr[i]);
        ra_free_temp(saved_fcsr[i]);
    }
}

static void sync_ymm_high128_from_env_lsx(int index)
{
    IR2_OPND address = ra_alloc_itemp();
    IR2_OPND high = ra_alloc_ftemp();

    li_d(address, lsenv_offset_of_xmm(lsenv, index) + 16);
    la_add_d(address, env_ir2_opnd, address);
    la_vld(high, address, 0);
    store_ymm_high128_shadow(high, index);
    ra_free_temp(high);
    ra_free_temp(address);
}

bool translate_vcvtph2ps_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    int dest_index = ir1_opnd_base_reg_num(dest_opnd);
    ADDR helper;
    int rel_kind;

    if (ir1_opnd_is_ymm(dest_opnd)) {
        helper = (ADDR)helper_cvtph2ps_ymm;
        rel_kind = LOAD_HELPER_CVTPH2PS_YMM;
        tr_save_ymm_to_env(UINT16_MAX);
    } else {
        helper = (ADDR)helper_cvtph2ps_xmm;
        rel_kind = LOAD_HELPER_CVTPH2PS_XMM;
    }

    if (!ir1_opnd_is_mem(src_opnd)) {
        call_f16c_helper_lsx(helper, dest_index,
                             ir1_opnd_base_reg_num(src_opnd), 0, rel_kind);
    } else {
        int scratch_index = (dest_index + 1) & 7;
        IR2_OPND saved = ra_alloc_ftemp();
        IR2_OPND saved_high = load_ymm_high128_shadow(scratch_index);
        IR2_OPND scratch = ra_alloc_xmm(scratch_index);

        la_vori_b(saved, scratch, 0);
        if (ir1_opnd_is_ymm(dest_opnd)) {
            load_freg128_from_ir1_mem(scratch, src_opnd);
        } else {
            load_freg_from_ir1_2(scratch, src_opnd, IS_INTEGER);
        }
        call_f16c_helper_lsx(helper, dest_index, scratch_index, 0, rel_kind);
        la_vori_b(scratch, saved, 0);
        store_ymm_high128_shadow(saved_high, scratch_index);
        ra_free_temp(saved_high);
        ra_free_temp(saved);
    }
    if (ir1_opnd_is_ymm(dest_opnd)) {
        sync_ymm_high128_from_env_lsx(dest_index);
    }
    if (ir1_opnd_is_xmm(dest_opnd)) {
        clear_ymm_high128_shadow(dest_index);
    }
    return true;
}

bool translate_vcvtps2ph_lsx(IR1_INST *pir1)
{
    IR1_OPND *dest_opnd = ir1_get_opnd(pir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(pir1, 1);
    int imm = ir1_opnd_uimm(ir1_get_opnd(pir1, 2));
    int src_index = ir1_opnd_base_reg_num(src_opnd);
    ADDR helper;
    int rel_kind;

    if (ir1_opnd_is_ymm(src_opnd)) {
        helper = (ADDR)helper_cvtps2ph_ymm;
        rel_kind = LOAD_HELPER_CVTPS2PH_YMM;
        tr_save_ymm_to_env(UINT16_MAX);
    } else {
        helper = (ADDR)helper_cvtps2ph_xmm;
        rel_kind = LOAD_HELPER_CVTPS2PH_XMM;
    }

    if (!ir1_opnd_is_mem(dest_opnd)) {
        call_f16c_helper_lsx(helper, ir1_opnd_base_reg_num(dest_opnd),
                             src_index, imm, rel_kind);
        /* VEX.128 writes clear the destination YMM high half. */
        clear_ymm_high128_shadow(ir1_opnd_base_reg_num(dest_opnd));
    } else {
        int scratch_index = (src_index + 1) & 7;
        IR2_OPND saved = ra_alloc_ftemp();
        IR2_OPND saved_high = load_ymm_high128_shadow(scratch_index);
        IR2_OPND scratch = ra_alloc_xmm(scratch_index);

        la_vori_b(saved, scratch, 0);
        call_f16c_helper_lsx(helper, scratch_index, src_index, imm, rel_kind);
        if (ir1_opnd_size(dest_opnd) == 128) {
            store_v128_to_ir1_mem_exact(scratch, dest_opnd);
        } else {
            lsassert(ir1_opnd_size(dest_opnd) == 64);
            store_freg_to_ir1(scratch, dest_opnd, false, false);
        }
        la_vori_b(scratch, saved, 0);
        store_ymm_high128_shadow(saved_high, scratch_index);
        ra_free_temp(saved_high);
        ra_free_temp(saved);
    }
    return true;
}
#endif
