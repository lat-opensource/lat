#include "ir2.h"
#include "translate.h"
#include "ir2-la-assemble.h"

void set_operand_into_instruction(GM_OPERAND_TYPE operand_type,
                                  IR2_OPND *p_opnd, uint32 *result);

GM_OPERAND_PLACE_RELATION bit_field_table[] = {
    {OPD_INVALID, {-1, -1}, {-1, -1} },
    {FCC_CA, {15, 17}, {-1, -1} },
    {FCC_CD, {0, 2}, {-1, -1} },
    {FCC_CJ, {5, 7}, {-1, -1} },
    {IMM_CODE, {0, 14}, {-1, -1} },
    {IMM_CONDF, {15, 19}, {-1, -1} },
    {IMM_CONDH, {10, 13}, {-1, -1} },
    {IMM_CONDL, {0, 3}, {-1, -1} },
    {OPD_CSR, {10, 23}, {-1, -1} },
    {FPR_FA, {15, 19}, {-1, -1} },
    {OPD_FCSRH, {5, 9}, {-1, -1} },
    {OPD_FCSRL, {0, 4}, {-1, -1} },
    {FPR_FD, {0, 4}, {-1, -1} },
    {FPR_FJ, {5, 9}, {-1, -1} },
    {FPR_FK, {10, 14}, {-1, -1} },
    {IMM_HINTL, {0, 14}, {-1, -1} },
    {IMM_HINTS, {0, 4}, {-1, -1} },
    {IMM_I13, {5, 17}, {-1, -1} },
    {IMM_IDXS, {18, 18}, {-1, -1} },
    {IMM_IDXM, {18, 19}, {-1, -1} },
    {IMM_IDXL, {18, 20}, {-1, -1} },
    {IMM_IDXLL, {18, 21}, {-1, -1} },
    {IMM_LEVEL, {10, 17}, {-1, -1} },
    {IMM_LSBD, {10, 15}, {-1, -1} },
    {IMM_LSBW, {10, 14}, {-1, -1} },
    {IMM_MODE, {5, 9}, {-1, -1} },
    {IMM_MSBD, {16, 21}, {-1, -1} },
    {IMM_MSBW, {16, 20}, {-1, -1} },
    {IMM_OFFS, {10, 25}, {-1, -1} },
    {IMM_OFFL, {10, 25}, {0, 4} },
    {IMM_OFFLL, {10, 25}, {0, 9} },
    {OPD_OPCACHE, {0, 4}, {-1, -1} },
    {IMM_OPX86, {5, 9}, {-1, -1} },
    {IMM_PTR, {5, 7}, {-1, -1} },
    {GPR_RD, {0, 4}, {-1, -1} },
    {GPR_RJ, {5, 9}, {-1, -1} },
    {GPR_RK, {10, 14}, {-1, -1} },
    {IMM_SA2, {15, 16}, {-1, -1} },
    {IMM_SA3, {15, 17}, {-1, -1} },
    {SCR_SD, {0, 1}, {-1, -1} },
    {IMM_SEQ, {10, 17}, {-1, -1} },
    {IMM_SI10, {10, 19}, {-1, -1} },
    {IMM_SI11, {10, 20}, {-1, -1} },
    {IMM_SI12, {10, 21}, {-1, -1} },
    {IMM_SI14, {10, 23}, {-1, -1} },
    {IMM_SI16, {10, 25}, {-1, -1} },
    {IMM_SI20, {5, 24}, {-1, -1} },
    {IMM_SI5, {10, 14}, {-1, -1} },
    {IMM_SI8, {10, 17}, {-1, -1} },
    {IMM_SI9, {10, 18}, {-1, -1} },
    {SCR_SJ, {5, 6}, {-1, -1} },
    {IMM_UI1, {10, 10}, {-1, -1} },
    {IMM_UI12, {10, 21}, {-1, -1} },
    {IMM_UI2, {10, 11}, {-1, -1} },
    {IMM_UI3, {10, 12}, {-1, -1} },
    {IMM_UI4, {10, 13}, {-1, -1} },
    {IMM_UI5H, {15, 19}, {-1, -1} },
    {IMM_UI5L, {10, 14}, {-1, -1} },
    {IMM_UI6, {10, 15}, {-1, -1} },
    {IMM_UI7, {10, 16}, {-1, -1} },
    {IMM_UI8, {10, 17}, {-1, -1} },
    {FPR_VA, {15, 19}, {-1, -1} },
    {FPR_VD, {0, 4}, {-1, -1} },
    {FPR_VJ, {5, 9}, {-1, -1} },
    {FPR_VK, {10, 14}, {-1, -1} },
    {FPR_XA, {15, 19}, {-1, -1} },
    {FPR_XD, {0, 4}, {-1, -1} },
    {FPR_XJ, {5, 9}, {-1, -1} },
    {FPR_XK, {10, 14}, {-1, -1} },
};

/* there are some errors in vector instruction */
GM_LA_OPCODE_FORMAT lisa_format_table[] = {
#include "format-table.h"
};

/*
 * FIXME: This is a intial port code, there is no any verification!!!
 */
uint32 ir2_assemble(const IR2_INST *ir2)
{
    if (ir2_opcode(ir2) == LISA_CODE) {
        lsassert(ir2_opnd_is_pseudo(&ir2->_opnd[0]));
        return ir2->_opnd[0]._val;
    }
    lsassert(ir2_opcode(ir2) > LISA_PSEUDO_END);
    GM_LA_OPCODE_FORMAT format = lisa_format_table[ir2_opcode(ir2) - LISA_INVALID];
    lsassert(format.type == ir2_opcode(ir2));
    lsassert(format.opcode != 0);

    uint32_t ins = format.opcode;
    lsassertm(ins, "Cannot use a pseudo opcode!");
    for (int i = 0; i < 4; i++) {
        GM_OPERAND_TYPE opnd_type = format.opnd[i];
        if (opnd_type == OPD_INVALID)
            break;

        GM_OPERAND_PLACE_RELATION bit_field = bit_field_table[opnd_type];
        lsassert(opnd_type == bit_field.type);

        int start = bit_field.bit_range_0.start;
        int end = bit_field.bit_range_0.end;
        int bit_len = end - start + 1;
        // FIXME: this is a unoin here.
        int val = ir2->_opnd[i]._val;
        int mask = (1 << bit_len) - 1;

        lsassert(!(ir2_opnd_is_pseudo(&ir2->_opnd[i]) ||
                   ir2_opnd_is_data(&ir2->_opnd[i])));

        ins |= (val & mask) << start;

        if (bit_field.bit_range_1.start >= 0) {
            val = val >> bit_len;
            start = bit_field.bit_range_1.start;
            end = bit_field.bit_range_1.end;
            bit_len = end - start + 1;
            mask = (1 << bit_len) - 1;
            ins |= (val & mask) << start;
        }
    }

    return ins;
}
