#ifndef LA_BDDISASM_H
#define LA_BDDISASM_H

//#include "latx-disassemble.h"
#include "bddisasm/bddisasm.h"
#include "latx-disasm.h"
#include <stdint.h>
#include <stdio.h>

//#define IR1_INST_SIZE sizeof(INSTRUX)

int labddisasm_get_64(const uint8_t *code, size_t code_size,
        struct _INSTRUX **insn,
        int ir1_num, void *pir1_base);
int labddisasm_get_32(const uint8_t *code, size_t code_size,
        struct _INSTRUX **insn,
        int ir1_num, void *pir1_base);
int labddisasm_get_16(const uint8_t *code, size_t code_size,
        struct _INSTRUX **insn,
        int ir1_num, void *pir1_base);

#endif
