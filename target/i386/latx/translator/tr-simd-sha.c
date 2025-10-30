#include "common.h"
#include "reg-alloc.h"
#include "latx-options.h"
#include "translate.h"

bool translate_sha1nexte(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    int d = ir1_opnd_base_reg_num(opnd0);
    if (!ir1_opnd_is_mem(opnd1)) {
        int s1 = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_sha1nexte, d, d, s1);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        if (option_enable_lasx) {
            la_xvor_v(temp, src, src);
        } else {
            la_vor_v(temp, src, src);
        }
        assert(ir1_opnd_size(opnd1) == 128);
        load_freg128_from_ir1_mem(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_sha1nexte, d, d, s1);
        if (option_enable_lasx) {
            la_xvor_v(src, temp, temp);
        } else {
            la_vor_v(src, temp, temp);
        }
    }
    /* TODO: need to check */
    return true;
}

bool translate_sha1msg1(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    int d = ir1_opnd_base_reg_num(opnd0);
    if (!ir1_opnd_is_mem(opnd1)) {
        int s1 = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_sha1msg1, d, d, s1);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        if (option_enable_lasx) {
            la_xvor_v(temp, src, src);
        } else {
            la_vor_v(temp, src, src);
        }
        assert(ir1_opnd_size(opnd1) == 128);
        load_freg128_from_ir1_mem(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_sha1msg1, d, d, s1);
        if (option_enable_lasx) {
            la_xvor_v(src, temp, temp);
        } else {
            la_vor_v(src, temp, temp);
        }
    }
    /* TODO: need to check */
    return true;
}

bool translate_sha1msg2(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    int d = ir1_opnd_base_reg_num(opnd0);
    if (!ir1_opnd_is_mem(opnd1)) {
        int s1 = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_sha1msg2, d, d, s1);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        if (option_enable_lasx) {
            la_xvor_v(temp, src, src);
        } else {
            la_vor_v(temp, src, src);
        }
        assert(ir1_opnd_size(opnd1) == 128);
        load_freg128_from_ir1_mem(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_sha1msg2, d, d, s1);
        if (option_enable_lasx) {
            la_xvor_v(src, temp, temp);
        } else {
            la_vor_v(src, temp, temp);
        }
    }
    /* TODO: need to check */
    return true;
}

bool translate_sha1rnds4(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    IR1_OPND *opnd2 = ir1_get_opnd(pir1, 2);
    int imm = ir1_opnd_uimm(opnd2);
    int d = ir1_opnd_base_reg_num(opnd0);
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
    if (!ir1_opnd_is_mem(opnd1)) {
        int s1 = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_func, d, d, s1);
    } else {
        int s1 = (d + 1) & 7;
		/* DO NOT use XMM0 because this insns use it implicitly */
        if (s1 == 0)
            s1++;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        if (option_enable_lasx) {
            la_xvor_v(temp, src, src);
        } else {
            la_vor_v(temp, src, src);
        }
        assert(ir1_opnd_size(opnd1) == 128);
        load_freg128_from_ir1_mem(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_func, d, d, s1);
        if (option_enable_lasx) {
            la_xvor_v(src, temp, temp);
        } else {
            la_vor_v(src, temp, temp);
        }
    }
    /* TODO: need to check */
    return true;
}

bool translate_sha256rnds2(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    int d = ir1_opnd_base_reg_num(opnd0);
    if (!ir1_opnd_is_mem(opnd1)) {
        int s1 = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_sha256rnds2_xmm0, d, d, s1);
    } else {
        int s1 = (d + 1) & 7;
		/* DO NOT use XMM0 because this insns use it implicitly */
        if (s1 == 0)
            s1++;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        if (option_enable_lasx) {
            la_xvor_v(temp, src, src);
        } else {
            la_vor_v(temp, src, src);
        }
        assert(ir1_opnd_size(opnd1) == 128);
        load_freg128_from_ir1_mem(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_sha256rnds2_xmm0, d, d, s1);
        if (option_enable_lasx) {
            la_xvor_v(src, temp, temp);
        } else {
            la_vor_v(src, temp, temp);
        }
    }
    /* TODO: need to check */
    return true;
}

bool translate_sha256msg1(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    int d = ir1_opnd_base_reg_num(opnd0);
    if (!ir1_opnd_is_mem(opnd1)) {
        int s1 = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_sha256msg1, d, d, s1);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        if (option_enable_lasx) {
            la_xvor_v(temp, src, src);
        } else {
            la_vor_v(temp, src, src);
        }
        assert(ir1_opnd_size(opnd1) == 128);
        load_freg128_from_ir1_mem(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_sha256msg1, d, d, s1);
        if (option_enable_lasx) {
            la_xvor_v(src, temp, temp);
        } else {
            la_vor_v(src, temp, temp);
        }
    }
    /* TODO: need to check */
    return true;
}

bool translate_sha256msg2(IR1_INST *pir1)
{
    IR1_OPND *opnd0 = ir1_get_opnd(pir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(pir1, 1);
    int d = ir1_opnd_base_reg_num(opnd0);
    if (!ir1_opnd_is_mem(opnd1)) {
        int s1 = ir1_opnd_base_reg_num(opnd1);
        tr_gen_call_to_helper_aes((ADDR)helper_sha256msg2, d, d, s1);
    } else {
        int s1 = (d + 1) & 7;
        IR2_OPND temp = ra_alloc_ftemp();
        IR2_OPND src = ra_alloc_xmm(s1);
        if (option_enable_lasx) {
            la_xvor_v(temp, src, src);
        } else {
            la_vor_v(temp, src, src);
        }
        assert(ir1_opnd_size(opnd1) == 128);
        load_freg128_from_ir1_mem(src, opnd1);

        tr_gen_call_to_helper_aes((ADDR)helper_sha256msg2, d, d, s1);
        if (option_enable_lasx) {
            la_xvor_v(src, temp, temp);
        } else {
            la_vor_v(src, temp, temp);
        }
    }
    /* TODO: need to check */
    return true;
}
