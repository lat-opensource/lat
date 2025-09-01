#ifndef _FLAG_LBT_BD_H_
#define _FLAG_LBT_BD_H_

#include "ir1-bd.h"
#include "ir2.h"

bool generate_eflag_by_lbt_bd(IR2_OPND dest, IR2_OPND src0, IR2_OPND src1,
                           IR1_INST *pir1, bool is_imm);

void get_eflag_condition_bd(IR2_OPND *value, IR1_INST *pir1);

#define GENERATE_EFLAG_IR2_1(ir2_opcode) do{ \
    if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 8) { \
        ir2_opcode##_b(src0); \
    } \
    else if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 16) { \
        ir2_opcode##_h(src0); \
    } \
    else if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 32) { \
        ir2_opcode##_w(src0); \
    } \
    else if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 64) { \
        ir2_opcode##_d(src0); \
    } \
} while(0)

#define GENERATE_EFLAG_IR2_2(ir2_opcode) do{ \
    if (ir2_opnd_is_imm(&src1)) { \
        src1 = \
            load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 0) + 1, SIGN_EXTENSION, false); \
    } \
    if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 8) { \
        ir2_opcode##_b(src0, src1); \
    } \
    else if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 16) { \
        ir2_opcode##_h(src0, src1); \
    } \
    else if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 32) { \
        ir2_opcode##_w(src0, src1); \
    } \
    else if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 64) { \
        ir2_opcode##_d(src0, src1); \
    } \
    if (ir2_opnd_is_imm(&src1)) { \
        ra_free_temp(src1); \
    } \
} while(0)

#define GENERATE_EFLAG_IR2_2_U(ir2_opcode) do{ \
    if (ir2_opnd_is_imm(&src1)) { \
        src1 = \
            load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 0) + 1, SIGN_EXTENSION, false); \
    } \
    if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 8) { \
        ir2_opcode##_bu(src0, src1); \
    } \
    else if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 16) { \
        ir2_opcode##_hu(src0, src1); \
    } \
    else if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 32) { \
        ir2_opcode##_wu(src0, src1); \
    } \
    else if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 64) { \
        ir2_opcode##_du(src0, src1); \
    } \
    if (ir2_opnd_is_imm(&src1)) { \
        ra_free_temp(src1); \
    } \
} while(0)

#define GENERATE_EFLAG_IR2_2_R(ir2_opcode) do{ \
    if (ir2_opnd_is_imm(&src1)) { \
        src1 = \
            load_ireg_from_ir1_bd(ir1_get_opnd_bd(pir1, 0) + 1, SIGN_EXTENSION, false); \
    } \
    if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 8) { \
        la_append_ir2_opnd2(ir2_opcode, src0, src1); \
    } \
    else if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 16) { \
        la_append_ir2_opnd2(ir2_opcode + 1, src0, src1); \
    } \
    else if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 32) { \
        la_append_ir2_opnd2(ir2_opcode + 2, src0, src1); \
    } \
    else if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 64) { \
        la_append_ir2_opnd2(ir2_opcode + 3, src0, src1); \
    } \
    if (ir2_opnd_is_imm(&src1)) { \
        ra_free_temp(src1); \
    } \
} while(0)

#define GENERATE_EFLAG_IR2_2_I(ir2_opcode) do { \
    lsassert(ir2_opnd_is_imm(&src1)); \
    uint32 imm = ir2_opnd_imm(&src1); \
    if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 8) { \
        ir2_opcode##_b(src0, imm); \
    } \
    else if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 16) { \
        ir2_opcode##_h(src0, imm); \
    } \
    else if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 32) { \
        ir2_opcode##_w(src0, imm); \
    } \
    else if (ir1_opnd_size_bd(ir1_get_opnd_bd(pir1, 0)) == 64) { \
        ir2_opcode##_d(src0, imm); \
    } \
} while (0)

#endif
