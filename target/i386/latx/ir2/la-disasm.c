#include "ir2.h"
#include "ir2-la-assemble.h"

#define IR2_OPND_IMMH IR2_OPND_IMM
#define IR2_OPND_IMMD IR2_OPND_IMM

static IR2_OPND_TYPE la_to_ir2_type[] = {
    IR2_OPND_NONE,//OPD_INVALID
    IR2_OPND_CC,//OPD_CA
    IR2_OPND_CC,//OPD_CD
    IR2_OPND_CC,//OPD_CJ
    IR2_OPND_IMMH,//OPD_CODE
    IR2_OPND_IMMH,//OPD_CONDF
    IR2_OPND_IMMH,//OPD_CONDH
    IR2_OPND_IMMH,//OPD_CONDL
    IR2_OPND_IMMH,//OPD_CSR
    IR2_OPND_FPR,//OPD_FA
    IR2_OPND_FCSR,//OPD_FCSRH
    IR2_OPND_FCSR,//OPD_FCSRL
    IR2_OPND_FPR,//OPD_FD
    IR2_OPND_FPR,//OPD_FJ
    IR2_OPND_FPR,//OPD_FK
    IR2_OPND_IMMH,//OPD_HINTL
    IR2_OPND_IMMH,//OPD_HINTS
    IR2_OPND_IMMH,//OPD_I13
    IR2_OPND_FPR,//OPD_IDXS
    IR2_OPND_FPR,//OPD_IDXM
    IR2_OPND_FPR,//OPD_IDXL
    IR2_OPND_FPR,//OPD_IDXLL
    IR2_OPND_IMMD,//OPD_LEVEL
    IR2_OPND_IMMD,//OPD_LSBD
    IR2_OPND_IMMD,//OPD_LSBW
    IR2_OPND_IMMH,//OPD_MODE
    IR2_OPND_IMMD,//OPD_MSBD
    IR2_OPND_IMMD,//OPD_MSBW
    IR2_OPND_IMMD,//OPD_OFFS
    IR2_OPND_IMMD,//OPD_OFFL
    IR2_OPND_IMMD,//OPD_OFFLL
    IR2_OPND_IMMH,//OPD_OPCACHE
    IR2_OPND_IMMH,//OPD_OPX86
    IR2_OPND_IMMH,//OPD_PTR
    IR2_OPND_GPR,//OPD_RD
    IR2_OPND_GPR,//OPD_RJ
    IR2_OPND_GPR,//OPD_RK
    IR2_OPND_IMMD,//OPD_SA2
    IR2_OPND_IMMD,//OPD_SA3
    IR2_OPND_SCR,//OPD_SD
    IR2_OPND_IMMD,//OPD_SEQ
    IR2_OPND_IMMD,//OPD_SI10
    IR2_OPND_IMMD,//OPD_SI11
    IR2_OPND_IMMD,//OPD_SI12
    IR2_OPND_IMMD,//OPD_SI14
    IR2_OPND_IMMD,//OPD_SI16
    IR2_OPND_IMMD,//OPD_SI20
    IR2_OPND_IMMD,//OPD_SI5
    IR2_OPND_IMMD,//OPD_SI8
    IR2_OPND_IMMD,//OPD_SI9
    IR2_OPND_SCR,//OPD_SJ
    IR2_OPND_IMMH,//OPD_UI1
    IR2_OPND_IMMH,//OPD_UI12
    IR2_OPND_IMMH,//OPD_UI2
    IR2_OPND_IMMH,//OPD_UI3
    IR2_OPND_IMMH,//OPD_UI4
    IR2_OPND_IMMH,//OPD_UI5H
    IR2_OPND_IMMH,//OPD_UI5L
    IR2_OPND_IMMH,//OPD_UI6
    IR2_OPND_IMMH,//OPD_UI7
    IR2_OPND_IMMH,//OPD_UI8
    IR2_OPND_FPR,//OPD_VA
    IR2_OPND_FPR,//OPD_VD
    IR2_OPND_FPR,//OPD_VJ
    IR2_OPND_FPR,//OPD_VK
    IR2_OPND_FPR,//OPD_XA
    IR2_OPND_FPR,//OPD_XD
    IR2_OPND_FPR,//OPD_XJ
    IR2_OPND_FPR,//OPD_XK
};

static bool is_la_sign_opnd[] = {
    0,//OPD_INVALID
    0,//OPD_CA
    0,//OPD_CD
    0,//OPD_CJ
    0,//OPD_CODE
    0,//OPD_CONDF
    0,//OPD_CONDH
    0,//OPD_CONDL
    0,//OPD_CSR
    0,//OPD_FA
    0,//OPD_FCSRH
    0,//OPD_FCSRL
    0,//OPD_FD
    0,//OPD_FJ
    0,//OPD_FK
    0,//OPD_HINTL
    0,//OPD_HINTS
    1,//OPD_I13
    0,//OPD_IDXS
    0,//OPD_IDXM
    0,//OPD_IDXL
    0,//OPD_IDXLL
    0,//OPD_LEVEL
    0,//OPD_LSBD
    0,//OPD_LSBW
    0,//OPD_MODE
    0,//OPD_MSBD
    0,//OPD_MSBW
    1,//OPD_OFFS
    1,//OPD_OFFL
    1,//OPD_OFFLL
    0,//OPD_OPCACHE
    0,//OPD_OPX86
    0,//OPD_PTR
    0,//OPD_RD
    0,//OPD_RJ
    0,//OPD_RK
    0,//OPD_SA2
    0,//OPD_SA3
    0,//OPD_SD
    0,//OPD_SEQ
    1,//OPD_SI10
    1,//OPD_SI11
    1,//OPD_SI12
    1,//OPD_SI14
    1,//OPD_SI16
    1,//OPD_SI20
    1,//OPD_SI5
    1,//OPD_SI8
    1,//OPD_SI9
    0,//OPD_SJ
    0,//OPD_UI1
    0,//OPD_UI12
    0,//OPD_UI2
    0,//OPD_UI3
    0,//OPD_UI4
    0,//OPD_UI5H
    0,//OPD_UI5L
    0,//OPD_UI6
    0,//OPD_UI7
    0,//OPD_UI8
    0,//OPD_VA
    0,//OPD_VD
    0,//OPD_VJ
    0,//OPD_VK
    0,//OPD_XA
    0,//OPD_XD
    0,//OPD_XJ
    0,//OPD_XK
};


static int la_get_opnd_val(GM_OPERAND_TYPE type, int insn) 
{
    GM_OPERAND_PLACE_RELATION *bit_field = &bit_field_table[type];

    int bit_start = bit_field->bit_range_0.start;
    int bit_end = bit_field->bit_range_0.end;
    int bit_len = bit_end - bit_start + 1;
    int val = (insn >> bit_start) & ((1 << bit_len) - 1);

    bit_start = bit_field->bit_range_1.start;
    bit_end = bit_field->bit_range_1.end;
    if (bit_start >= 0 &&  bit_end >= 0) {
        int field1_val = insn << (31 - bit_end) >> (31 - bit_end + bit_start);
        val |= field1_val << bit_len;
    }

    if (is_la_sign_opnd[type]) {
        if (bit_end >= 0)
            bit_len += bit_field->bit_range_1.end - bit_field->bit_range_1.start + 1;
        val = val << (32 - bit_len) >> (32 - bit_len);
    }

    return val;
}

void la_disasm(uint32_t insn) {
    IR2_INST ir2;
    IR2_OPCODE opcode = la_decode(insn);
    ir2._opcode = opcode;
    ir2.op_count = 0;
    GM_LA_OPCODE_FORMAT format = lisa_format_table[opcode - LISA_INVALID];
    /* lsassert(format.type == opcode); */
    /* lsassert(format.opcode != 0); */

    for (int i = 0; i < 4; i++) {
        GM_OPERAND_TYPE opnd_type = format.opnd[i];
        if (opnd_type == OPD_INVALID) {
            break;
        }
        IR2_OPND_TYPE ir2_opnd_type = la_to_ir2_type[opnd_type];
        int ir2_opnd_val = la_get_opnd_val(opnd_type, insn);
        ir2._opnd[i] = create_ir2_opnd(ir2_opnd_type, ir2_opnd_val);
        ir2.op_count++;
    }
    ir2_dump(&ir2, true);
}

