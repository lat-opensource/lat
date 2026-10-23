#!/usr/bin/env python3
"""Check the JECXZ preceding-write optimisation decision."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    match = re.search(r"(?:static )?(?:void|bool|int) " + name +
                      r"\([^)]*\)\s*\{", source)
    if not match:
        raise RuntimeError("missing function: " + name)
    depth, end = 1, match.end()
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


HARNESS = r"""
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define CONFIG_LATX_INSTS_PATTERN 1
#define INSTPTN_OPC_NONE 0

enum {
    dt_X86_INS_MOV,
    dt_X86_INS_MOVZX,
    dt_X86_INS_MOVSX,
    dt_X86_INS_LEA,
    dt_X86_INS_ADC,
    dt_X86_INS_SBB,
    dt_X86_INS_ADD,
    dt_X86_INS_SUB,
    dt_X86_INS_AND,
    dt_X86_INS_OR,
    dt_X86_INS_XOR,
    dt_X86_INS_INC,
    dt_X86_INS_DEC,
    dt_X86_INS_NEG,
    dt_X86_INS_NOT,
    dt_X86_INS_CMP,
    dt_X86_REG_RIP = 100,
};

typedef struct {
    int opc;
    void *next;
} INSTPTN;

typedef struct {
    int type;
    int size;
    int reg;
} IR1_OPND;

typedef struct IR1_INST {
    INSTPTN instptn;
    IR1_OPND opnd[2];
    int op_count;
    int opcode;
} IR1_INST;

typedef struct {
    IR1_INST *ir1;
} TB_S_DATA;

typedef struct {
    TB_S_DATA *s_data;
    int icount;
} TranslationBlock;

typedef struct {
    TranslationBlock *curr_tb;
} TRANSLATION_DATA;

typedef struct {
    TRANSLATION_DATA *tr_data;
} ENV;

static int ecx_index = 1;
static int codeis64 = 1;
#define CODEIS64 codeis64

static IR1_INST *tb_ir1_inst(TranslationBlock *tb, int i)
{
    return tb->s_data->ir1 + i;
}
static int tb_ir1_num(TranslationBlock *tb)
{
    return tb->icount;
}
static int ir1_get_opnd_num(IR1_INST *ir1)
{
    return ir1->op_count;
}
static IR1_OPND *ir1_get_opnd(IR1_INST *ir1, int i)
{
    return &ir1->opnd[i];
}
static int ir1_opcode(IR1_INST *ir1)
{
    return ir1->opcode;
}
static int ir1_opnd_is_gpr(IR1_OPND *opnd)
{
    return opnd->type == 1;
}
static int ir1_opnd_is_mem(IR1_OPND *opnd)
{
    return opnd->type == 2;
}
static int ir1_opnd_base_reg_num(IR1_OPND *opnd)
{
    return opnd->reg;
}
static int ir1_opnd_size(IR1_OPND *opnd)
{
    return opnd->size;
}
static int ir1_opnd_base_reg(IR1_OPND *opnd)
{
    return opnd->reg;
}

static IR1_INST ir1[4];
static TB_S_DATA tb_data;
static TranslationBlock tb;
static TRANSLATION_DATA tr_data;
static ENV env = { &tr_data };
static ENV *lsenv = &env;

static void reset(void)
{
    int i;

    for (i = 0; i < 4; i++) {
        ir1[i] = (IR1_INST){ 0 };
        ir1[i].instptn.opc = INSTPTN_OPC_NONE;
    }
    tb_data.ir1 = ir1;
    tb.s_data = &tb_data;
    tb.icount = 4;
    tr_data.curr_tb = &tb;
    lsenv = &env;
    codeis64 = 1;

    /* JECXZ at index 1, MOV ECX, ... immediately before it. */
    ir1[1].op_count = 0;
    ir1[1].opcode = 999;
    ir1[0].op_count = 2;
    ir1[0].opcode = dt_X86_INS_MOV;
    ir1[0].opnd[0].type = 1;
    ir1[0].opnd[0].size = 32;
    ir1[0].opnd[0].reg = ecx_index;
}
"""

CHECK = r"""
#define EXPECT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #cond); \
        return 1; \
    } \
} while (0)

int main(void)
{
    const int always[] = {
        dt_X86_INS_MOV, dt_X86_INS_MOVZX, dt_X86_INS_MOVSX,
        dt_X86_INS_ADC, dt_X86_INS_SBB,
    };
    const int mode64[] = {
        dt_X86_INS_ADD, dt_X86_INS_SUB, dt_X86_INS_AND, dt_X86_INS_OR,
        dt_X86_INS_XOR, dt_X86_INS_INC, dt_X86_INS_DEC, dt_X86_INS_NEG,
        dt_X86_INS_NOT,
    };
    unsigned i;

    reset();
    EXPECT(!jecxz_prev_defines_zero_extended_ecx(&ir1[0]));
    EXPECT(jecxz_prev_defines_zero_extended_ecx(&ir1[1]));
    EXPECT(!jecxz_prev_defines_zero_extended_ecx(&ir1[2]));
    EXPECT(!jecxz_prev_defines_zero_extended_ecx(&ir1[3]));

    for (i = 0; i < sizeof(always) / sizeof(always[0]); i++) {
        reset();
        ir1[0].opcode = always[i];
        codeis64 = 1;
        EXPECT(jecxz_prev_defines_zero_extended_ecx(&ir1[1]));
        codeis64 = 0;
        EXPECT(jecxz_prev_defines_zero_extended_ecx(&ir1[1]));
    }

    for (i = 0; i < sizeof(mode64) / sizeof(mode64[0]); i++) {
        reset();
        ir1[0].opcode = mode64[i];
        codeis64 = 1;
        EXPECT(jecxz_prev_defines_zero_extended_ecx(&ir1[1]));
        codeis64 = 0;
        EXPECT(!jecxz_prev_defines_zero_extended_ecx(&ir1[1]));
    }

    reset();
    ir1[0].opcode = dt_X86_INS_LEA;
    ir1[0].opnd[1].type = 2;
    ir1[0].opnd[1].reg = 2;
    codeis64 = 1;
    EXPECT(jecxz_prev_defines_zero_extended_ecx(&ir1[1]));

    /* translate_lea()'s RIP-relative path returns the full effective
     * address without adjust_dest(), so ECX is not zero-extended. */
    reset();
    ir1[0].opcode = dt_X86_INS_LEA;
    ir1[0].opnd[1].type = 2;
    ir1[0].opnd[1].reg = dt_X86_REG_RIP;
    codeis64 = 1;
    EXPECT(!jecxz_prev_defines_zero_extended_ecx(&ir1[1]));

    reset();
    ir1[0].opcode = dt_X86_INS_CMP;
    EXPECT(!jecxz_prev_defines_zero_extended_ecx(&ir1[1]));

    reset();
    ir1[0].opnd[0].reg = 2;
    EXPECT(!jecxz_prev_defines_zero_extended_ecx(&ir1[1]));

    reset();
    ir1[0].opnd[0].size = 64;
    EXPECT(!jecxz_prev_defines_zero_extended_ecx(&ir1[1]));

    reset();
    ir1[0].opnd[0].type = 2;
    EXPECT(!jecxz_prev_defines_zero_extended_ecx(&ir1[1]));

    reset();
    ir1[0].op_count = 0;
    EXPECT(!jecxz_prev_defines_zero_extended_ecx(&ir1[1]));

    reset();
    ir1[0].instptn.opc = 1;
    EXPECT(!jecxz_prev_defines_zero_extended_ecx(&ir1[1]));

    puts("PASS: JECXZ preceding-write decision");
    return 0;
}
"""


def main():
    source = (ROOT / "target/i386/latx/translator/tr-jcc.c").read_text()
    code = (HARNESS + function(source, "jecxz_prev_defines_zero_extended_ecx")
            + CHECK)
    with tempfile.TemporaryDirectory(prefix="latx-jecxz-test-") as directory:
        path = Path(directory)
        (path / "test.c").write_text(code)
        subprocess.run(shlex.split(os.environ.get("CC", "cc")) + [
            "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
            "-DTARGET_X86_64", str(path / "test.c"),
            "-o", str(path / "test")], check=True)
        return subprocess.run([str(path / "test")]).returncode


if __name__ == "__main__":
    raise SystemExit(main())
