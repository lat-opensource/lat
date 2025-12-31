#include "env.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "flag-lbt.h"
#include "translate.h"
#include "hbr.h"

#define ATO_AMADD(opsz)   (opsz == 64 ? la_amadd_db_d : \
                           opsz == 32 ? la_amadd_db_w : \
                           opsz == 16 ? la_amadd_db_h : la_amadd_db_b)
#define ATO_AMSWAP(opsz)  (opsz == 64 ? la_amswap_db_d : \
                           opsz == 32 ? la_amswap_db_w : \
                           opsz == 16 ? la_amswap_db_h : la_amswap_db_b)

#define ATO_AMCAS(opsz)   (opsz == 64 ? la_amcas_db_d : \
                           opsz == 32 ? la_amcas_db_w : \
                           opsz == 16 ? la_amcas_db_h : la_amcas_db_b)

//bool translate_lock_btx_fast_atomic(IR1_INST *pir1)
//bool translate_lock_sbb_fast_atomic(IR1_INST *pir1)
//bool translate_lock_adc_fast_atomic(IR1_INST *pir1)
//bool translate_lock_neg_fast_atomic(IR1_INST *pir1)

bool translate_lock_add_fast_atomic(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND src0, src1, dest;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();

    mem_opnd = convert_mem_no_offset(opnd0);
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

    IR2_INST *(*amadd_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
    amadd_inst = ATO_AMADD(opnd0_size);

    src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);

    amadd_inst(src0, src1, mem_opnd);
    /* restore src1 to old valule */
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    return true;
}

bool translate_lock_and_fast_atomic(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND src0, src1, dest;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();

    mem_opnd = convert_mem_no_offset(opnd0);
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

    src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);

    if (opnd0_size == 64) {
        la_amand_db_d(src0, src1, mem_opnd);
    } else if (opnd0_size == 32) {
        la_amand_db_w(src0, src1, mem_opnd);
    } else if (opnd0_size == 16 || opnd0_size == 8) {
        IR2_INST *(*amcas_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
        amcas_inst = ATO_AMCAS(opnd0_size);
        IR2_OPND tmp = ra_alloc_itemp();
        IR2_OPND label_atomic_retry = ra_alloc_label();
        la_label(label_atomic_retry);
        /* 1. read source operand */
        load_ireg_from_ir2_mem_2(src0, mem_opnd, 0, opnd0_size, SIGN_EXTENSION, false);
        la_or(tmp, src0, zero_ir2_opnd);
        /* 2. do operation (and) */
        la_and(dest, src0, src1);
        /* 3. ret = AMCAS */
        amcas_inst(tmp, dest, mem_opnd);
        /* 4. compare source with ret. goto 1 if not equal */
        la_bne(tmp, src0, label_atomic_retry);
    } else {
        lsassert(0);
    }

    generate_eflag_calculation(dest, src0, src1, pir1, true);

    return true;
}

bool translate_lock_inc_fast_atomic(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);

    IR2_OPND src0, src1, dest;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();
    src1 = ra_alloc_itemp();

    mem_opnd = convert_mem_no_offset(opnd0);
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

    IR2_INST *(*amadd_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
    amadd_inst = ATO_AMADD(opnd0_size);

    la_addi_d(src1, zero_ir2_opnd, 1);

    amadd_inst(src0, src1, mem_opnd);
    /* restore src1 to old valule */
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    return true;
}

bool translate_lock_dec_fast_atomic(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);

    IR2_OPND src0, src1, dest;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();
    src1 = ra_alloc_itemp();

    mem_opnd = convert_mem_no_offset(opnd0);
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

    IR2_INST *(*amadd_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
    amadd_inst = ATO_AMADD(opnd0_size);

    la_addi_d(src1, zero_ir2_opnd, -1);

    amadd_inst(src0, src1, mem_opnd);
    /* restore src1 to old valule */
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    return true;
}

bool translate_lock_sub_fast_atomic(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND src0, src1, dest, temp;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();
    temp = ra_alloc_itemp();

    mem_opnd = convert_mem_no_offset(opnd0);
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

    IR2_INST *(*amadd_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
    amadd_inst = ATO_AMADD(opnd0_size);

    src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
    la_sub_d(temp, zero_ir2_opnd, src1);

    amadd_inst(src0, temp, mem_opnd);
    /* restore src1 to old valule */
    generate_eflag_calculation(dest, src0, src1, pir1, true);

    return true;
}

bool translate_lock_or_fast_atomic(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND src0, src1, dest;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();

    mem_opnd = convert_mem_no_offset(opnd0);
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

    src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);

    if (opnd0_size == 64) {
        la_amor_db_d(src0, src1, mem_opnd);
    } else if (opnd0_size == 32) {
        la_amor_db_w(src0, src1, mem_opnd);
    } else if (opnd0_size == 16 || opnd0_size == 8) {
        IR2_INST *(*amcas_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
        amcas_inst = ATO_AMCAS(opnd0_size);
        IR2_OPND tmp = ra_alloc_itemp();
        IR2_OPND label_atomic_retry = ra_alloc_label();
        la_label(label_atomic_retry);
        /* 1. read source operand */
        load_ireg_from_ir2_mem_2(src0, mem_opnd, 0, opnd0_size, SIGN_EXTENSION, false);
        la_or(tmp, src0, zero_ir2_opnd);
        /* 2. do operation (xor) */
        la_or(dest, src0, src1);
        /* 3. ret = AMCAS */
        amcas_inst(tmp, dest, mem_opnd);
        /* 4. compare source with ret. goto 1 if not equal */
        la_bne(tmp, src0, label_atomic_retry);
    } else {
        lsassert(0);
    }

    generate_eflag_calculation(dest, src0, src1, pir1, true);

    return true;
}

bool translate_lock_not_fast_atomic(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);

    IR2_OPND src0, src1, dest;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();
    src1 = ra_alloc_itemp();

    mem_opnd = convert_mem_no_offset(opnd0);
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

    la_addi_d(src1, zero_ir2_opnd, -1);

    if (opnd0_size == 64) {
        la_amxor_db_d(src0, src1, mem_opnd);
    } else if (opnd0_size == 32) {
        la_amxor_db_w(src0, src1, mem_opnd);
    } else if (opnd0_size == 16 || opnd0_size == 8) {
        IR2_INST *(*amcas_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
        amcas_inst = ATO_AMCAS(opnd0_size);
        IR2_OPND tmp = ra_alloc_itemp();
        IR2_OPND label_atomic_retry = ra_alloc_label();
        la_label(label_atomic_retry);
        /* 1. read source operand */
        load_ireg_from_ir2_mem_2(src0, mem_opnd, 0, opnd0_size, SIGN_EXTENSION, false);
        la_or(tmp, src0, zero_ir2_opnd);
        /* 2. do operation (xor) */
        la_xor(dest, src0, src1);
        /* 3. ret = AMCAS */
        amcas_inst(tmp, dest, mem_opnd);
        /* 4. compare source with ret. goto 1 if not equal */
        la_bne(tmp, src0, label_atomic_retry);
    } else {
        lsassert(0);
    }

    return true;
}

bool translate_lock_xor_fast_atomic(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND src0, src1, dest;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();

    mem_opnd = convert_mem_no_offset(opnd0);
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

    src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);

    if (opnd0_size == 64) {
        la_amxor_db_d(src0, src1, mem_opnd);
    } else if (opnd0_size == 32) {
        la_amxor_db_w(src0, src1, mem_opnd);
    } else if (opnd0_size == 16 || opnd0_size == 8) {
        IR2_INST *(*amcas_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
        amcas_inst = ATO_AMCAS(opnd0_size);
        IR2_OPND tmp = ra_alloc_itemp();
        IR2_OPND label_atomic_retry = ra_alloc_label();
        la_label(label_atomic_retry);
        /* 1. read source operand */
        load_ireg_from_ir2_mem_2(src0, mem_opnd, 0, opnd0_size, SIGN_EXTENSION, false);
        la_or(tmp, src0, zero_ir2_opnd);
        /* 2. do operation (xor) */
        la_xor(dest, src0, src1);
        /* 3. ret = AMCAS */
        amcas_inst(tmp, dest, mem_opnd);
        /* 4. compare source with ret. goto 1 if not equal */
        la_bne(tmp, src0, label_atomic_retry);
    } else {
        lsassert(0);
    }

    generate_eflag_calculation(dest, src0, src1, pir1, true);

    return true;
}

bool translate_lock_xadd_fast_atomic(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND src0, src1, dest;
    IR2_OPND mem_opnd;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* alloc itemp */
    dest = ra_alloc_itemp();
    src0 = ra_alloc_itemp();

    mem_opnd = convert_mem_no_offset(opnd0);
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

    IR2_INST *(*amadd_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
    amadd_inst = ATO_AMADD(opnd0_size);

    src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);

    amadd_inst(src0, src1, mem_opnd);
    generate_eflag_calculation(dest, src0, src1, pir1, true);
    store_ireg_to_ir1(src0, opnd1, false);

    return true;
}

bool translate_lock_xchg_fast_atomic(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *reg_opnd;

    IR2_OPND mem_opnd;
    IR2_OPND src0 = ra_alloc_itemp();
    IR2_OPND src1;

    int opnd0_size = ir1_opnd_size(opnd0);
    /* get src1 and mem_opnd */
    if (ir1_opnd_is_mem(opnd0)) {
        mem_opnd = convert_mem_no_offset(opnd0);
        src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);
        reg_opnd = opnd1;
    } else {
        mem_opnd = convert_mem_no_offset(opnd1);
        src1 = load_ireg_from_ir1(opnd0, UNKNOWN_EXTENSION, false);
        reg_opnd = opnd0;
    }

    IR2_INST *(*amswap_inst)(IR2_OPND, IR2_OPND, IR2_OPND);
    amswap_inst = ATO_AMSWAP(opnd0_size);

    amswap_inst(src0, src1, mem_opnd);
    store_ireg_to_ir1(src0, reg_opnd, false);
    return true;

}

bool translate_lock_cmpxchg_fast_atomic(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);

    IR2_OPND src0, src1;
    IR1_OPND *reg_ir1 = NULL;
    int opnd0_size = ir1_opnd_size(opnd0);

    /* get eax */
    if (opnd0_size == 64) {
        reg_ir1 = &rax_ir1_opnd;
    } else if (opnd0_size == 32) {
        reg_ir1 = &eax_ir1_opnd;
    } else if (opnd0_size == 16) {
        reg_ir1 = &ax_ir1_opnd;
    } else if (opnd0_size == 8) {
        reg_ir1 = &al_ir1_opnd;
    }
    /* get src1 */
    src1 = load_ireg_from_ir1(opnd1, UNKNOWN_EXTENSION, false);

    IR2_OPND gpr_eax_opnd = ra_alloc_gpr(eax_index);

    IR2_OPND mem_opnd = convert_mem_no_offset(opnd0);
    gen_test_page_flag(mem_opnd, 0, PAGE_WRITE | PAGE_WRITE_ORG);

#ifdef TARGET_X86_64
    if (CODEIS64 && opnd0_size == 64) {
	    /* amcas always change rd to rj (change gpr_eax_opnd to *mem_addr),
         * so we need to generate EFLAGS.ZF by comparing old eax and new eax(src0_mem_addr) */
        IR2_OPND mem_val = ra_alloc_itemp();
        load_ireg_from_ir1_2(mem_val, reg_ir1, SIGN_EXTENSION, false);

        la_amcas_db_d(mem_val, src1, mem_opnd);
        if (ir1_need_calculate_any_flag(pir1)) {
            generate_eflag_calculation(src0, gpr_eax_opnd, mem_val, pir1, true);
        }
        IR2_OPND temp1 = ra_alloc_itemp();
        IR2_OPND temp2 = ra_alloc_itemp();
        la_xor(temp1, mem_val, gpr_eax_opnd);
        la_masknez(temp2, gpr_eax_opnd, temp1);
        la_maskeqz(temp1, mem_val, temp1);
        la_or(gpr_eax_opnd, temp2, temp1);
        return true;
    }

    /* In x64 mode, RAX should be carefully handled in 32 bit cmpxchg.
     * If equal, the entire 64 bit of RAX cannot be changed.
     * If uneuqal, eax = m32 and high 32 bit should be cleared to zero.
     */
    if (opnd0_size == 32) {
        IR2_OPND temp1 = ra_alloc_itemp();
        IR2_OPND old_eax_opnd = ra_alloc_itemp();
        IR2_OPND zx_opnd = ra_alloc_itemp();
        load_ireg_from_ir1_2(temp1, reg_ir1, SIGN_EXTENSION, false);
        load_ireg_from_ir1_2(old_eax_opnd, reg_ir1, SIGN_EXTENSION, false);
	    /* amcas always change rd to rj (change eax_opnd to *src0_mem_addr),
         * so we need to generate EFLAGS.ZF by comparing old eax and new eax(src0_mem_addr) */
        la_amcas_db_w(temp1, src1, mem_opnd);
        generate_eflag_calculation(src0, old_eax_opnd, temp1, pir1, true);

        la_mov32_zx(zx_opnd, temp1);

        // temp1 == old_eax_opnd ? gpr_eax_opnd : zx_opnd
        la_xor(temp1, temp1, old_eax_opnd);
        la_masknez(old_eax_opnd, gpr_eax_opnd, temp1); //equal, use gpr_eax
        la_maskeqz(zx_opnd, zx_opnd, temp1); //not equal, use zx_opnd
        la_or(gpr_eax_opnd, old_eax_opnd, zx_opnd);

        return true;
    }
#else
    if (opnd0_size == 32) {
	    /* amcas always change rd to rj (change gpr_eax_opnd to *mem_addr),
         * so we need to generate EFLAGS.ZF by comparing old eax and new eax(src0_mem_addr) */
        IR2_OPND eax_opnd;
        if (ir1_need_calculate_any_flag(pir1)) {
            eax_opnd = ra_alloc_itemp();
            load_ireg_from_ir1_2(eax_opnd, reg_ir1, SIGN_EXTENSION, false);
        }
        la_amcas_db_w(gpr_eax_opnd, src1, mem_opnd);
        if (ir1_need_calculate_any_flag(pir1)) {
            generate_eflag_calculation(src0, eax_opnd, gpr_eax_opnd, pir1, true);
        }

        return true;
    }
#endif


    IR2_OPND temp1 = ra_alloc_itemp();
    IR2_OPND old_eax_opnd = ra_alloc_itemp();
    load_ireg_from_ir1_2(temp1, reg_ir1, SIGN_EXTENSION, false);
    load_ireg_from_ir1_2(old_eax_opnd, reg_ir1, SIGN_EXTENSION, false);

    if (opnd0_size == 8) {
        la_amcas_db_b(temp1, src1, mem_opnd);
    } else if (opnd0_size == 16) {
        la_amcas_db_h(temp1, src1, mem_opnd);
    } else {
        lsassert(0);
    }

    generate_eflag_calculation(src0, old_eax_opnd, temp1, pir1, true);

    IR2_OPND temp2 = ra_alloc_itemp();
    la_xor(temp2, temp1, old_eax_opnd);
    la_masknez(old_eax_opnd, old_eax_opnd, temp2);
    la_maskeqz(temp1, temp1, temp2);
    la_or(temp1, old_eax_opnd, temp1);

    store_ireg_to_ir1(temp1, reg_ir1, false);

    return true;
}
