#include "env.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "flag-lbt.h"
#include "translate.h"
#include "hbr.h"

/**
* @brief convert_mem_to_itemp
*
* @param opnd0
*
* @return - it must be an itemp
*/
IR2_OPND convert_mem_to_itemp(IR1_OPND *opnd0)
{
    IR2_OPND mem_op;

    mem_op = convert_mem_no_offset(opnd0);
    if (!ir2_opnd_is_itemp(&mem_op)) {
        IR2_OPND mem_tmp = ra_alloc_itemp();
        la_or(mem_tmp, zero_ir2_opnd, mem_op);
        return mem_tmp;
    }
    return mem_op;
}

/**
* @brief translate_lock_sbb - use ll-sc/am* to translate lock sbb
*
* @param pir1
*
* @return
*/
bool translate_lock_sbb(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    /*
     * sbb 计算 eflag 的指令有 bug，需要手动计算 af
     * 具体看 generate_eflag_calculation 函数里。
     */
    IR2_OPND src0, src1, dest, tmp;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* label */
    IR2_OPND label_interpret = ra_alloc_label();
    IR2_OPND label_aligned = ra_alloc_label();
    IR2_OPND label_not_aligned = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    IR2_OPND label_ll = ra_alloc_label();

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();
    tmp = ra_alloc_itemp();

    /* get src1 */
    src1 = load_ireg_from_ir1(opnd1, SIGN_EXTENSION, false);

    /* get mem_opnd */
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

    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

    if (CODEIS64 && opnd0_size == 64) {
        la_sbc_d(dest, zero_ir2_opnd, src1);
        la_amadd_db_d(src0, dest, mem_opnd);
        generate_eflag_calculation(dest, src0, src1, pir1, true);
        return true;
    }

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
        la_sbc_w(dest, zero_ir2_opnd, src1);
        la_amadd_db_w(src0, dest, mem_opnd);
        generate_eflag_calculation(dest, src0, src1, pir1, true);
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
    la_sbc_w(dest, src0, src1);
    generate_eflag_calculation(dest, src0, src1, pir1, true);

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
    la_sbc_d(dest, zero_ir2_opnd, src1);
    la_amadd_d(src0, dest, mem_opnd);
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    /*
     * exit
     */
    la_label(label_exit);

    /* free */
    ra_free_temp(src0);
    ra_free_temp(dest);
    ra_free_temp(tmp);
    return true;
}

/**
* @brief translate_lock_add - use ll-sc/am* to translate lock add
*
* @param pir1
*
* @return
*/
bool translate_lock_add(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND src0, src1, dest, tmp;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* label */
    IR2_OPND label_interpret = ra_alloc_label();
    IR2_OPND label_aligned = ra_alloc_label();
    IR2_OPND label_not_aligned = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    IR2_OPND label_ll = ra_alloc_label();

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();
    tmp = ra_alloc_itemp();

    /* get src1 */
    src1 = load_ireg_from_ir1(opnd1, SIGN_EXTENSION, false);

    /* get mem_opnd */
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

    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

#ifdef TARGET_X86_64
    if (CODEIS64 && opnd0_size == 64) {
        la_amadd_db_d(src0, src1, mem_opnd);
        generate_eflag_calculation(dest, src0, src1, pir1, true);
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
        la_amadd_db_w(src0, src1, mem_opnd);
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
    la_add_w(dest, src0, src1);

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
    /* 请不要删除这两条指令，interpret_add 会从 inst[-2] 读取 opnd0_size*/
    la_andi(zero_ir2_opnd, zero_ir2_opnd, opnd0_size);
    la_andi(zero_ir2_opnd, zero_ir2_opnd, opnd0_size);
    la_label(label_interpret);
    la_amadd_d(src0, src1, mem_opnd);

    /*
     * exit
     */
    la_label(label_exit);
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    /* free */
    ra_free_temp(src0);
    ra_free_temp(dest);
    ra_free_temp(tmp);
    return true;
}

/**
* @brief translate_lock_adc - use ll-sc/am* to translate lock adc
*
* @param pir1
*
* @return
*/
bool translate_lock_adc(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND src0, src1, dest, tmp;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* label */
    IR2_OPND label_interpret = ra_alloc_label();
    IR2_OPND label_aligned = ra_alloc_label();
    IR2_OPND label_not_aligned = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    IR2_OPND label_ll = ra_alloc_label();

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();
    tmp = ra_alloc_itemp();

    /* get src1 */
    src1 = load_ireg_from_ir1(opnd1, SIGN_EXTENSION, false);

    /* get mem_opnd */
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
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

#ifdef TARGET_X86_64
    if (CODEIS64 && opnd0_size == 64) {
        la_adc_d(dest, zero_ir2_opnd, src1);
        la_amadd_db_d(src0, dest, mem_opnd);
        generate_eflag_calculation(dest, src0, src1, pir1, true);
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
        la_adc_w(dest, zero_ir2_opnd, src1);
        la_amadd_db_w(src0, dest, mem_opnd);
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
    la_adc_w(dest, src0, src1);

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
    la_adc_d(dest, zero_ir2_opnd, src1);
    la_amadd_d(src0, dest, mem_opnd);

    /*
     * exit
     */
    la_label(label_exit);
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    /* free */
    ra_free_temp(src0);
    ra_free_temp(dest);
    ra_free_temp(tmp);
    return true;
}

/**
* @brief translate_lock_and - use ll-sc/am* to translate lock and
*
* @param pir1
*
* @return
*/
bool translate_lock_and(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND src0, src1, dest, tmp;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* label */
    IR2_OPND label_interpret = ra_alloc_label();
    IR2_OPND label_aligned = ra_alloc_label();
    IR2_OPND label_not_aligned = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    IR2_OPND label_ll = ra_alloc_label();

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();
    tmp = ra_alloc_itemp();

    /* get src1 */
    src1 = load_ireg_from_ir1(opnd1, SIGN_EXTENSION, false);

    /* get mem_opnd */
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
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

#ifdef TARGET_X86_64
    if (CODEIS64 && opnd0_size == 64) {
        la_amand_db_d(src0, src1, mem_opnd);
        generate_eflag_calculation(dest, src0, src1, pir1, true);
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
        la_amand_db_w(src0, src1, mem_opnd);
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
    la_and(dest, src0, src1);

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
    la_amand_d(src0, src1, mem_opnd);

    /*
     * exit
     */
    la_label(label_exit);
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    /* free */
    ra_free_temp(src0);
    ra_free_temp(dest);
    ra_free_temp(tmp);
    return true;
}

/**
* @brief translate_lock_inc - use ll-sc/am* to translate lock inc
*
* @param pir1
*
* @return
*/
bool translate_lock_inc(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);

    IR2_OPND src0, dest, tmp;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* label */
    IR2_OPND label_interpret = ra_alloc_label();
    IR2_OPND label_aligned = ra_alloc_label();
    IR2_OPND label_not_aligned = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    IR2_OPND label_ll = ra_alloc_label();

    /* alloc itemp */
    src0 = ra_alloc_itemp();
    tmp = ra_alloc_itemp();

    /* get mem_opnd */
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
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

#ifdef TARGET_X86_64
    if (CODEIS64 && opnd0_size == 64) {
        la_addi_d(tmp, zero_ir2_opnd, 1);
        la_amadd_db_d(src0, tmp, mem_opnd);
        generate_eflag_calculation(dest, src0, src0, pir1, true);
        return true;
    }
#endif

    /* alloc dest */
    dest = ra_alloc_itemp();

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
        la_addi_d(tmp, zero_ir2_opnd, 1);
        la_amadd_db_w(src0, tmp, mem_opnd);
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
    la_addi_w(dest, src0, 1);

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
    la_addi_d(tmp, zero_ir2_opnd, 1);
    la_amadd_d(src0, tmp, mem_opnd);

    /*
     * exit
     */
    la_label(label_exit);
    generate_eflag_calculation(dest, src0, src0, pir1, true);

    /* free */
    ra_free_temp(src0);
    ra_free_temp(dest);
    ra_free_temp(tmp);
    return true;
}

/**
* @brief translate_lock_dec - use ll-sc/am* to translate lock dec
*
* @param pir1
*
* @return
*/
bool translate_lock_dec(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);

    IR2_OPND src0, dest, tmp;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* label */
    IR2_OPND label_interpret = ra_alloc_label();
    IR2_OPND label_aligned = ra_alloc_label();
    IR2_OPND label_not_aligned = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    IR2_OPND label_ll = ra_alloc_label();

    /* alloc itemp */
    src0 = ra_alloc_itemp();
    tmp = ra_alloc_itemp();

    /* get mem_opnd */
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
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

#ifdef TARGET_X86_64
    if (CODEIS64 && opnd0_size == 64) {
        la_addi_d(tmp, zero_ir2_opnd, -1);
        la_amadd_db_d(src0, tmp, mem_opnd);
        generate_eflag_calculation(dest, src0, src0, pir1, true);
        return true;
    }
#endif

    /* alloc dest */
    dest = ra_alloc_itemp();

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
        la_addi_d(tmp, zero_ir2_opnd, -1);
        la_amadd_db_w(src0, tmp, mem_opnd);
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
    la_addi_w(dest, src0, -1);

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
    la_addi_d(tmp, zero_ir2_opnd, -1);
    la_amadd_d(src0, tmp, mem_opnd);

    /*
     * exit
     */
    la_label(label_exit);
    generate_eflag_calculation(dest, src0, src0, pir1, true);

    /* free */
    ra_free_temp(src0);
    ra_free_temp(dest);
    ra_free_temp(tmp);
    return true;
}

/**
* @brief translate_lock_sub - use ll-sc/am* to translate lock sub
*
* @param pir1
*
* @return
*/
bool translate_lock_sub(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND src0, src1, dest, tmp;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* label */
    IR2_OPND label_interpret = ra_alloc_label();
    IR2_OPND label_aligned = ra_alloc_label();
    IR2_OPND label_not_aligned = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    IR2_OPND label_ll = ra_alloc_label();

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();
    tmp = ra_alloc_itemp();

    /* get src1 */
    src1 = load_ireg_from_ir1(opnd1, SIGN_EXTENSION, false);

    /* get mem_opnd */
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
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

#ifdef TARGET_X86_64
    if (CODEIS64 && opnd0_size == 64) {
        la_sub_d(dest, zero_ir2_opnd, src1);
        la_amadd_db_d(src0, dest, mem_opnd);
        generate_eflag_calculation(dest, src0, src1, pir1, true);
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
        la_sub_w(dest, zero_ir2_opnd, src1);
        la_amadd_db_w(src0, dest, mem_opnd);
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
    la_sub_w(dest, src0, src1);

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
    la_sub_d(dest, zero_ir2_opnd, src1);
    la_amadd_d(src0, dest, mem_opnd);

    /*
     * exit
     */
    la_label(label_exit);
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    /* free */
    ra_free_temp(src0);
    ra_free_temp(dest);
    ra_free_temp(tmp);
    return true;
}

/**
* @brief translate_lock_neg - use ll-sc/am* to translate lock neg
*
* @param pir1
*
* @return
*/
bool translate_lock_neg(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);

    IR2_OPND src0, dest, tmp;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* label */
    IR2_OPND label_interpret = ra_alloc_label();
    IR2_OPND label_not_aligned = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    IR2_OPND label_ll = ra_alloc_label();
    IR2_OPND label_ll_w = ra_alloc_label();

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();
    tmp = ra_alloc_itemp();

    /* get mem_opnd */
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
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

#ifdef TARGET_X86_64
    IR2_OPND label_ll_d = ra_alloc_label();
    if (CODEIS64 && opnd0_size == 64) {
        la_label(label_ll_d);
        la_ll_d(src0, mem_opnd, 0);
        la_sub_d(dest, zero_ir2_opnd, src0);
        la_sc_d(dest, mem_opnd, 0);
        la_beq(dest, zero_ir2_opnd, label_ll_d);
        generate_eflag_calculation(dest, zero_ir2_opnd, src0, pir1, true);
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
        la_beq(tmp, zero_ir2_opnd, label_ll_w);
        la_blt(tmp, dest, label_not_aligned);

        /* The following code is used when memory is aligned. */
        la_label(label_ll_w);
        la_ll_w(src0, mem_opnd, 0);
        la_sub_w(dest, zero_ir2_opnd, src0);
        la_sc_w(dest, mem_opnd, 0);
        la_beq(dest, zero_ir2_opnd, label_ll_w);
        la_b(label_exit);
    } else if (opnd0_size == 16) {
        li_wu(dest, 7);
        la_beq(tmp, dest, label_interpret);
    }

    /*
     * The following code is used when memory is not aligned
     */
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
    la_sub_w(dest, zero_ir2_opnd, src0);

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
    la_ll_d(src0, mem_opnd, 0);
    la_sub_d(dest, zero_ir2_opnd, src0);
    la_sc_d(dest, mem_opnd, 0);
    la_beq(dest, zero_ir2_opnd, label_interpret);

    /*
     * exit
     */
    la_label(label_exit);
    generate_eflag_calculation(dest, zero_ir2_opnd, src0, pir1, true);

    /* free */
    ra_free_temp(src0);
    ra_free_temp(dest);
    ra_free_temp(tmp);
    return true;
}

/**
* @brief translate_lock_or - use ll-sc/am* to translate lock or
*
* @param pir1
*
* @return
*/
bool translate_lock_or(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND src0, src1, dest, tmp;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* label */
    IR2_OPND label_interpret = ra_alloc_label();
    IR2_OPND label_aligned = ra_alloc_label();
    IR2_OPND label_not_aligned = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    IR2_OPND label_ll = ra_alloc_label();

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();
    tmp = ra_alloc_itemp();

    /* get src1 */
    src1 = load_ireg_from_ir1(opnd1, SIGN_EXTENSION, false);

    /* get mem_opnd */
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
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

#ifdef TARGET_X86_64
    if (CODEIS64 && opnd0_size == 64) {
        la_amor_db_d(src0, src1, mem_opnd);
        generate_eflag_calculation(dest, src0, src1, pir1, true);
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
        la_amor_db_w(src0, src1, mem_opnd);
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
    la_or(dest, src0, src1);

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
    la_amor_d(src0, src1, mem_opnd);

    /*
     * exit
     */
    la_label(label_exit);
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    /* free */
    ra_free_temp(src0);
    ra_free_temp(dest);
    ra_free_temp(tmp);
    return true;
}

/**
* @brief translate_lock_not - use ll-sc/am* to translate lock not
*
* @param pir1
*
* @return
*/
bool translate_lock_not(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);

    IR2_OPND src0, dest, tmp;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* label */
    IR2_OPND label_interpret = ra_alloc_label();
    IR2_OPND label_aligned = ra_alloc_label();
    IR2_OPND label_not_aligned = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    IR2_OPND label_ll = ra_alloc_label();

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();
    tmp = ra_alloc_itemp();

    /* get mem_opnd */
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
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

#ifdef TARGET_X86_64
    if (CODEIS64 && opnd0_size == 64) {
        la_addi_d(tmp, zero_ir2_opnd, -1);
        la_amxor_db_d(src0, tmp, mem_opnd);
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
        la_addi_w(tmp, zero_ir2_opnd, -1);
        la_amxor_db_w(src0, tmp, mem_opnd);
        la_b(label_exit);
    } else if (opnd0_size == 16) {
        li_wu(dest, 7);
        la_beq(tmp, dest, label_interpret);
    }

    /*
     * The following code is used when memory is not aligned
     */
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
    la_nor(dest, zero_ir2_opnd, src0);

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
    la_addi_d(tmp, zero_ir2_opnd, -1);
    la_amxor_d(src0, tmp, mem_opnd);

    /*
     * exit
     */
    la_label(label_exit);

    /* free */
    ra_free_temp(src0);
    ra_free_temp(dest);
    ra_free_temp(tmp);
    return true;
}

/**
* @brief translate_lock_xor - use ll-sc/am* to translate lock xor
*
* @param pir1
*
* @return
*/
bool translate_lock_xor(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND src0, src1, dest, tmp;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* label */
    IR2_OPND label_interpret = ra_alloc_label();
    IR2_OPND label_aligned = ra_alloc_label();
    IR2_OPND label_not_aligned = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    IR2_OPND label_ll = ra_alloc_label();

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();
    tmp = ra_alloc_itemp();

    /* get src1 */
    src1 = load_ireg_from_ir1(opnd1, SIGN_EXTENSION, false);

    /* get mem_opnd */
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
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

#ifdef TARGET_X86_64
    if (CODEIS64 && opnd0_size == 64) {
        la_amxor_db_d(src0, src1, mem_opnd);
        generate_eflag_calculation(dest, src0, src1, pir1, true);
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
        la_amxor_db_w(src0, src1, mem_opnd);
        la_b(label_exit);
    } else if (opnd0_size == 16) {
        li_wu(dest, 7);
        la_beq(tmp, dest, label_interpret);
    }

    /*
     * The following code is used when memory is not aligned
     */
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
    la_xor(dest, src0, src1);

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
    la_andi(zero_ir2_opnd, zero_ir2_opnd, opnd0_size);
    la_label(label_interpret);
    la_amxor_d(src0, src1, mem_opnd);

    /*
     * exit
     */
    la_label(label_exit);
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    /* free */
    ra_free_temp(src0);
    ra_free_temp(dest);
    ra_free_temp(tmp);
    return true;
}

/**
* @brief translate_lock_xadd - use ll-sc/am* to translate lock xadd
*
* @param pir1
*
* @return
*/
bool translate_lock_xadd(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND src0, src1, dest, tmp;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* label */
    IR2_OPND label_interpret = ra_alloc_label();
    IR2_OPND label_aligned = ra_alloc_label();
    IR2_OPND label_not_aligned = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();

    IR2_OPND label_ll = ra_alloc_label();

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();
    tmp = ra_alloc_itemp();

    /* get src1 */
    src1 = ra_alloc_itemp();
    load_ireg_from_ir1_2(src1, opnd1, SIGN_EXTENSION, false);

    /* get mem_opnd */
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
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

#ifdef TARGET_X86_64
    if (CODEIS64 && opnd0_size == 64) {
        la_amadd_db_d(src0, src1, mem_opnd);
        store_ireg_to_ir1(src0, opnd1, false);
        generate_eflag_calculation(dest, src0, src1, pir1, true);
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
        la_amadd_db_w(src0, src1, mem_opnd);
        store_ireg_to_ir1(src0, opnd1, false);
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
    store_ireg_to_ir1(src0, opnd1, false);
    la_add_w(dest, src0, src1);

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
    /* 请不要删除这两条指令，interpret_add 会从 inst[-2] 读取 opnd0_size*/
    la_andi(zero_ir2_opnd, zero_ir2_opnd, opnd0_size);
    la_andi(zero_ir2_opnd, zero_ir2_opnd, opnd0_size);
    la_label(label_interpret);
    la_amadd_d(src0, src1, mem_opnd);
    store_ireg_to_ir1(src0, opnd1, false);

    /*
     * exit
     */
    la_label(label_exit);
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    /* free */
    ra_free_temp(src0);
    ra_free_temp(dest);
    ra_free_temp(tmp);
    return true;
}

/**
* @brief translate_lock_cmpxchg - use ll-sc/am* to translate lock cmpxchg
*
* @param pir1
*
* @return
*/
bool translate_lock_cmpxchg(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND src0, src1, dest, tmp, eax_opnd;
    IR2_OPND mem_opnd;
    IR1_OPND *reg_ir1 = NULL;
    int opnd0_size = ir1_opnd_size(opnd0);

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
    eax_opnd = load_ireg_from_ir1(reg_ir1, SIGN_EXTENSION, false);

    /* label */
    IR2_OPND label_interpret = ra_alloc_label();
    IR2_OPND label_aligned = ra_alloc_label();
    IR2_OPND label_not_aligned = ra_alloc_label();
    IR2_OPND label_exit = ra_alloc_label();
    IR2_OPND label_flag = ra_alloc_label();

    IR2_OPND label_ll = ra_alloc_label();
    IR2_OPND label_unequal = ra_alloc_label();

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();
    tmp = ra_alloc_itemp();

    /* get src1 */
    src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);

    /* get mem_opnd */
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
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

#ifdef TARGET_X86_64
    IR2_OPND label_ll_d = ra_alloc_label();
    if (CODEIS64 && opnd0_size == 64) {
        la_label(label_ll_d);
        la_or(dest, zero_ir2_opnd, src1);
        la_ll_d(src0, mem_opnd, 0);
        la_bne(src0, eax_opnd, label_unequal);
        /* equal */
        la_sc_d(dest, mem_opnd, 0);
        la_beq(dest, zero_ir2_opnd, label_ll_d);
        la_b(label_flag);
        goto tr_exit;
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
        la_ll_w(src0, mem_opnd, 0);
        la_bne(src0, eax_opnd, label_unequal);
        /* equal */
        la_or(dest, zero_ir2_opnd, src1);
        la_sc_w(dest, mem_opnd, 0);
        la_beq(dest, zero_ir2_opnd, label_aligned);
        la_b(label_flag);
    } else if (opnd0_size == 16) {
        li_wu(dest, 7);
        la_beq(tmp, dest, label_interpret);
    }

    /* The following code is used when memory is not aligned*/
    la_label(label_not_aligned);
    la_bstrins_d(mem_opnd, zero_ir2_opnd, 2, 0);

    if (opnd0_size == 32) {
        IR2_OPND label_1 = ra_alloc_label();
        IR2_OPND label_2 = ra_alloc_label();
        IR2_OPND label_3 = ra_alloc_label();

        la_ori(dest, zero_ir2_opnd, 2);
        la_beq(tmp, dest, label_2);
        la_blt(tmp, dest, label_1);
        /* address 3 */
        la_label(label_3);
        la_ll_d(src0, mem_opnd, 0);
        la_or(dest, src0, zero_ir2_opnd);
        la_bstrpick_w(src0, src0, 55, 24);
        la_bne(src0, eax_opnd, label_unequal);
        la_bstrins_d(dest, src1, 55, 24);
        la_sc_d(dest, mem_opnd, 0);
        la_beq(dest, zero_ir2_opnd, label_3);
        la_b(label_flag);

        /* address 2 */
        la_label(label_2);
        la_ll_d(src0, mem_opnd, 0);
        la_or(dest, src0, zero_ir2_opnd);
        la_bstrpick_w(src0, src0, 47, 16);
        la_bne(src0, eax_opnd, label_unequal);
        la_bstrins_d(dest, src1, 47, 16);
        la_sc_d(dest, mem_opnd, 0);
        la_beq(dest, zero_ir2_opnd, label_2);
        la_b(label_flag);

        /* address 1 */
        la_label(label_1);
        la_ll_d(src0, mem_opnd, 0);
        la_or(dest, src0, zero_ir2_opnd);
        la_bstrpick_w(src0, src0, 39, 8);
        la_bne(src0, eax_opnd, label_unequal);
        la_bstrins_d(dest, src1, 39, 8);
        la_sc_d(dest, mem_opnd, 0);
        la_beq(dest, zero_ir2_opnd, label_1);
        la_b(label_flag);
    } else {
        uint32 mask;

        IR2_OPND mask_opnd = a1_ir2_opnd;
        la_slli_d(tmp, tmp, 3);

        la_label(label_ll);
        la_ll_d(src0, mem_opnd, 0);
        la_or(dest, src0, zero_ir2_opnd);
        la_srl_d(src0, src0, tmp);
        if (opnd0_size == 16) {
            mask = 0xFFFF;
            la_ext_w_h(src0, src0);
        } else {
            mask = 0xFF;
            la_ext_w_b(src0, src0);
        }
        la_bne(src0, eax_opnd, label_unequal);
        /* equal :use dest as mask_opnd*/
        li_wu(mask_opnd, mask);
        la_sll_d(mask_opnd, mask_opnd, tmp);
        la_nor(mask_opnd, zero_ir2_opnd, mask_opnd);
        la_and(dest, dest, mask_opnd);
        /* rebuild dest */
        la_bstrpick_d(mask_opnd, src1, opnd0_size - 1, 0);
        la_sll_d(mask_opnd, mask_opnd, tmp);
        la_or(dest, dest, mask_opnd);
        /* write back */
        la_sc_d(dest, mem_opnd, 0);
        la_beq(dest, zero_ir2_opnd, label_ll);
        la_b(label_flag);
    }

    /*
     * interpret path
     */
    la_andi(zero_ir2_opnd, zero_ir2_opnd, opnd0_size);
    la_label(label_interpret);
    la_or(dest, zero_ir2_opnd, src1);
    la_ll_d(src0, mem_opnd, 0);
    la_bne(src0, eax_opnd, label_unequal);
    /* equal */
    la_sc_d(dest, mem_opnd, 0);
    la_beq(dest, zero_ir2_opnd, label_interpret);
    la_b(label_flag);

#ifdef TARGET_X86_64
tr_exit:
#endif
    /* unequal */
    la_label(label_unequal);
    la_dbar(0);
    generate_eflag_calculation(src0, eax_opnd, src0, pir1, true);
    store_ireg_to_ir1(src0, reg_ir1, false);
    la_b(label_exit);

    /*
     * exit
     */
    la_label(label_flag);
    generate_eflag_calculation(src0, eax_opnd, src0, pir1, true);
    la_label(label_exit);
    return true;
}
