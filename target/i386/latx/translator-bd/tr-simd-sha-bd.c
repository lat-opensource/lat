#include "common.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate-bd.h"

bool translate_sha1nexte_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    int d = ir1_opnd_base_reg_num_bd(opnd0);
    if (!ir1_opnd_is_mem_bd(opnd1)) {
        int s1 = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_sha1nexte, d, d, s1);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        la_xvor_v(temp, src, src);
        assert(ir1_opnd_size_bd(opnd1) == 128);
        load_freg128_from_ir1_mem_bd(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_sha1nexte, d, d, s1);
        la_xvor_v(src, temp, temp);
    }
    /* TODO: need to check */
    return true;
}

bool translate_sha1msg1_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    int d = ir1_opnd_base_reg_num_bd(opnd0);
    if (!ir1_opnd_is_mem_bd(opnd1)) {
        int s1 = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_sha1msg1, d, d, s1);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        la_xvor_v(temp, src, src);
        assert(ir1_opnd_size_bd(opnd1) == 128);
        load_freg128_from_ir1_mem_bd(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_sha1msg1, d, d, s1);
        la_xvor_v(src, temp, temp);
    }
    /* TODO: need to check */
    return true;
}

bool translate_sha1msg2_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    int d = ir1_opnd_base_reg_num_bd(opnd0);
    if (!ir1_opnd_is_mem_bd(opnd1)) {
        int s1 = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_sha1msg2, d, d, s1);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        la_xvor_v(temp, src, src);
        assert(ir1_opnd_size_bd(opnd1) == 128);
        load_freg128_from_ir1_mem_bd(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_sha1msg2, d, d, s1);
        la_xvor_v(src, temp, temp);
    }
    /* TODO: need to check */
    return true;
}

bool translate_sha1rnds4_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    IR1_OPND_BD *opnd2 = ir1_get_opnd_bd(pir1, 2);
    int imm = ir1_opnd_uimm_bd(opnd2);
    int d = ir1_opnd_base_reg_num_bd(opnd0);
	ADDR helper_func;
	switch (imm) {
		case 0:
			helper_func = (ADDR)helper_sha1rnds4_f0;
			break;
		case 1:
			helper_func = (ADDR)helper_sha1rnds4_f1;
			break;
		case 2:
			helper_func = (ADDR)helper_sha1rnds4_f2;
			break;
		case 3:
			helper_func = (ADDR)helper_sha1rnds4_f3;
			break;
        default:
            helper_func = 0xdeadbeaf;
            lsassert(0);
            break;
	}
    if (!ir1_opnd_is_mem_bd(opnd1)) {
        int s1 = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_func, d, d, s1);
    } else {
        int s1 = (d + 1) & 7;
		/* DO NOT use XMM0 because this insns use it implicitly */
        if (s1 == 0)
            s1++;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        la_xvor_v(temp, src, src);
        assert(ir1_opnd_size_bd(opnd1) == 128);
        load_freg128_from_ir1_mem_bd(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_func, d, d, s1);
        la_xvor_v(src, temp, temp);
    }
    /* TODO: need to check */
    return true;
}

bool translate_sha256rnds2_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    int d = ir1_opnd_base_reg_num_bd(opnd0);
    if (!ir1_opnd_is_mem_bd(opnd1)) {
        int s1 = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_sha256rnds2_xmm0, d, d, s1);
    } else {
        int s1 = (d + 1) & 7;
		/* DO NOT use XMM0 because this insns use it implicitly */
        if (s1 == 0)
            s1++;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        la_xvor_v(temp, src, src);
        assert(ir1_opnd_size_bd(opnd1) == 128);
        load_freg128_from_ir1_mem_bd(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_sha256rnds2_xmm0, d, d, s1);
        la_xvor_v(src, temp, temp);
    }
    /* TODO: need to check */
    return true;
}

bool translate_sha256msg1_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    int d = ir1_opnd_base_reg_num_bd(opnd0);
    if (!ir1_opnd_is_mem_bd(opnd1)) {
        int s1 = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_sha256msg1, d, d, s1);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        la_xvor_v(temp, src, src);
        assert(ir1_opnd_size_bd(opnd1) == 128);
        load_freg128_from_ir1_mem_bd(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_sha256msg1, d, d, s1);
        la_xvor_v(src, temp, temp);
    }
    /* TODO: need to check */
    return true;
}

bool translate_sha256msg2_bd(IR1_INST *pir1)
{
    IR1_OPND_BD *opnd0 = ir1_get_opnd_bd(pir1, 0);
    IR1_OPND_BD *opnd1 = ir1_get_opnd_bd(pir1, 1);
    int d = ir1_opnd_base_reg_num_bd(opnd0);
    if (!ir1_opnd_is_mem_bd(opnd1)) {
        int s1 = ir1_opnd_base_reg_num_bd(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_sha256msg2, d, d, s1);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        la_xvor_v(temp, src, src);
        assert(ir1_opnd_size_bd(opnd1) == 128);
        load_freg128_from_ir1_mem_bd(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_sha256msg2, d, d, s1);
        la_xvor_v(src, temp, temp);
    }
    /* TODO: need to check */
    return true;
}
