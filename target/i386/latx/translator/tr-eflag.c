#include "common.h"
#include "reg-alloc.h"
#include "lsenv.h"
#include "latx-options.h"
#include "translate.h"

bool translate_popf(IR1_INST *pir1)
{
#ifndef TARGET_X86_64
    const int sp_step = 4;
#else
    int sp_step = 8;
    if (!CODEIS64) {
        sp_step = 4;
    }
#endif

    IR2_OPND esp_opnd = ra_alloc_gpr(esp_index);
    IR2_OPND eflags_opnd = ra_alloc_eflags();
    IR2_OPND eflags_temp_opnd = ra_alloc_itemp();
    IR2_OPND label_miss = ra_alloc_label();
    /* because we use itemp, so we can ignore the EM */
#ifndef TARGET_X86_64
    la_bstrpick_d(esp_opnd, esp_opnd, 31, 0);
    la_ld_w(eflags_opnd, esp_opnd, 0);
#else
    if (CODEIS64) {
        la_ld_d(eflags_opnd, esp_opnd, 0);
    } else {
        la_bstrpick_d(esp_opnd, esp_opnd, 31, 0);
        la_ld_w(eflags_opnd, esp_opnd, 0);
    }
#endif

    la_andi(eflags_temp_opnd, eflags_opnd, TF_MASK);
    la_beqz(eflags_temp_opnd, label_miss);
    tr_gen_call_to_helper1((ADDR)helper_eflagtf, 1, LOAD_HELPER_EFLAGTF);

    la_label(label_miss);
    la_x86mtflag(eflags_opnd, 0x3f);
    /*
     * Some apps test eflags [22:12] bits for CPU feature detection.
     * Wine detect bit 21 to detemine whether CPU support SSE.
     * To make this kind of apps happy, we not only store the bit 0 to 12
     * but also store bit 12 to 22.
     */
    li_wu(eflags_temp_opnd, 0xfffff500);
    la_and(eflags_opnd, eflags_opnd, eflags_temp_opnd);
    la_ori(eflags_opnd, eflags_opnd, 0x202);
    ra_free_temp(eflags_temp_opnd);

    la_addi_addrx(esp_opnd, esp_opnd, sp_step);
    return true;
}

bool translate_pushf(IR1_INST *pir1)
{
#ifndef TARGET_X86_64
    const int sp_step = 4;
#else
    int sp_step = 8;
    if (!CODEIS64) {
        sp_step = 4;
    }
#endif
    IR2_OPND eflags_opnd = ra_alloc_eflags();
    IR2_OPND esp_opnd = ra_alloc_gpr(esp_index);
    IR2_OPND temp   = ra_alloc_itemp();
    la_x86mfflag(temp, 0x3f);
    la_or(temp, eflags_opnd, temp);
    ra_free_temp(temp);
#ifndef TARGET_X86_64
    la_bstrpick_d(esp_opnd, esp_opnd, 31, 0);
#else
    if (!CODEIS64) {
        la_bstrpick_d(esp_opnd, esp_opnd, 31, 0);
    }
#endif
    la_store_addrx(temp, esp_opnd, -sp_step);

    la_addi_addrx(esp_opnd, esp_opnd, -sp_step);

    return true;
}

bool translate_clc(IR1_INST *pir1) {
    la_x86mtflag(zero_ir2_opnd, 0x1);
    return true;
}

bool translate_cld(IR1_INST *pir1)
{
    IR2_OPND eflags = ra_alloc_eflags();

    la_bstrins_w(eflags, zero_ir2_opnd, 10, 10);

    return true;
}

bool translate_stc(IR1_INST *pir1) {
    IR2_OPND n4095_opnd = ra_alloc_num_4095();
    la_x86mtflag(n4095_opnd, 0x1);
    ra_free_num_4095(n4095_opnd);
    return true;
}

bool translate_std(IR1_INST *pir1)
{
    IR2_OPND eflags = ra_alloc_eflags();

    la_ori(eflags, eflags, 0x400);

    return true;
}
