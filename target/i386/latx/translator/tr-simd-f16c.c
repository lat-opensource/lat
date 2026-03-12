#include "common.h"
#include "reg-alloc.h"
#include "lsenv.h"
#include "latx-options.h"
#include "translate.h"

#ifdef CONFIG_LATX_AVX_OPT
bool translate_vcvtph2ps(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    int d = ir1_opnd_base_reg_num(opnd0);
    IR2_OPND dest = ra_alloc_xmm(ir1_opnd_base_reg_num(opnd0));
    ADDR helper_func;
    int rel_kind = 0;

    if (ir1_opnd_is_ymm(opnd0)) {
        helper_func = (ADDR)helper_cvtph2ps_ymm;
        rel_kind = LOAD_HELPER_CVTPH2PS_YMM;
    } else {
        helper_func = (ADDR)helper_cvtph2ps_xmm;
        rel_kind = LOAD_HELPER_CVTPH2PS_XMM;
    }

    if (!ir1_opnd_is_mem(opnd1)) {
        int s1 = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_pcmpxstrx(helper_func, d, s1, 0, rel_kind);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        la_xvor_v(temp, src, src);
        load_freg128_from_ir1_mem(src, opnd1);

        tr_gen_call_to_helper_pcmpxstrx(helper_func, d, s1, 0, rel_kind);
        la_xvor_v(src, temp, temp);
    }
    if (ir1_opnd_is_xmm(opnd0)) {
        set_high128_xreg_to_zero(dest);
    }
    return true;
}

bool translate_vcvtps2ph(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int imm = ir1_opnd_uimm(opnd2);
    int s = ir1_opnd_base_reg_num(opnd1);
    ADDR helper_func;
    int rel_kind = 0;

    if (ir1_opnd_is_ymm(opnd1)) {
        helper_func = (ADDR)helper_cvtps2ph_ymm;
        rel_kind = LOAD_HELPER_CVTPS2PH_YMM;
    } else {
        helper_func = (ADDR)helper_cvtps2ph_xmm;
        rel_kind = LOAD_HELPER_CVTPS2PH_XMM;
    }

    if (!ir1_opnd_is_mem(opnd0)) {
        int d = ir1_opnd_base_reg_num(opnd0);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_func, d, s, imm, rel_kind);
    } else {
        int d = (s + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND dest = ra_alloc_xmm(d);
        la_xvor_v(temp, dest, dest);
        tr_gen_call_to_helper_pcmpxstrx((ADDR)helper_func, d, s, imm, rel_kind);

        if (ir1_opnd_size(opnd0) == 128) {
            store_freg128_to_ir1_mem(dest, opnd0);
        } else {
            lsassert(ir1_opnd_size(opnd0) == 64);
            store_freg_to_ir1(dest, opnd0, false, false);
        }

        la_xvor_v(dest, temp, temp);
    }
    return true;
}
#endif
