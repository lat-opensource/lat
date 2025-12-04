#ifndef __LISA_ASSEMBLE_H_
#define __LISA_ASSEMBLE_H_

#include "la-ir2.h"
typedef enum {
    OPD_INVALID = 0,
    FCC_CA,
    FCC_CD,
    FCC_CJ,
    IMM_CODE,
    IMM_CONDF,
    IMM_CONDH,
    IMM_CONDL,
    OPD_CSR,
    FPR_FA,
    OPD_FCSRH,
    OPD_FCSRL,
    FPR_FD,
    FPR_FJ,
    FPR_FK,
    IMM_HINTL,
    IMM_HINTS,
    IMM_I13,
    IMM_IDXS,
    IMM_IDXM,
    IMM_IDXL,
    IMM_IDXLL,
    IMM_LEVEL,
    IMM_LSBD,
    IMM_LSBW,
    IMM_MODE,
    IMM_MSBD,
    IMM_MSBW,
    IMM_OFFS,
    IMM_OFFL,
    IMM_OFFLL,
    OPD_OPCACHE,
    IMM_OPX86,
    IMM_PTR,
    GPR_RD,
    GPR_RJ,
    GPR_RK,
    IMM_SA2,
    IMM_SA3,
    SCR_SD,
    IMM_SEQ,
    IMM_SI10,
    IMM_SI11,
    IMM_SI12,
    IMM_SI14,
    IMM_SI16,
    IMM_SI20,
    IMM_SI5,
    IMM_SI8,
    IMM_SI9,
    SCR_SJ,
    IMM_UI1,
    IMM_UI12,
    IMM_UI2,
    IMM_UI3,
    IMM_UI4,
    IMM_UI5H,
    IMM_UI5L,
    IMM_UI6,
    IMM_UI7,
    IMM_UI8,
    FPR_VA,
    FPR_VD,
    FPR_VJ,
    FPR_VK,
    FPR_XA,
    FPR_XD,
    FPR_XJ,
    FPR_XK,
} GM_OPERAND_TYPE;

typedef struct pair {
    int start;
    int end;
} pair;

typedef struct {
    GM_OPERAND_TYPE type;
    pair bit_range_0;
    pair bit_range_1; /* some branch offset is splited into 2 parts */
} GM_OPERAND_PLACE_RELATION;

typedef struct {
    IR2_OPCODE type;
    uint32_t opcode;
    GM_OPERAND_TYPE opnd[4];
} GM_LA_OPCODE_FORMAT;

typedef struct {
    GM_OPERAND_TYPE place;
    IR2_OPND_TYPE type;
} GM_LA_OPERAND_PLACE_TYPE;

extern GM_OPERAND_PLACE_RELATION bit_field_table[];
extern GM_LA_OPCODE_FORMAT lisa_format_table[];

#endif

