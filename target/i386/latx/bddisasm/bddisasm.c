#include "labddisasm.h"
#include <assert.h>

#include "capstone_git.h"

int labddisasm_get_64(const uint8_t *code, size_t code_size,
        struct _INSTRUX **insn,
        int ir1_num, void *pir1_base)
{
    INSTRUX *ix;

    if (pir1_base) {
        uint64_t current_address =  (uint64_t)pir1_base +
            // (ir1_num * IR1_INST_SIZE);
            (ir1_num * sizeof(struct la_dt_insn));
        ix = (void *)current_address;
    } else {
        // ix = malloc(IR1_INST_SIZE);
        ix = malloc(sizeof(struct la_dt_insn));
    }

    NDSTATUS status = NdDecodeEx(ix, code, code_size, ND_CODE_64, ND_DATA_64);

    if (!ND_SUCCESS(status))
    {
#ifdef CONFIG_LATX_DEBUG
        fprintf(stderr, "%s: can't disasm code 0x%x at addr %p\n",
            __func__, *(uint32_t *)code, code);
#endif
        *insn = NULL;
        return -1;
    } 

    *insn = ix;

    return 1;
}

int labddisasm_get_32(const uint8_t *code, size_t code_size,
        struct _INSTRUX **insn,
        int ir1_num, void *pir1_base)
{
    INSTRUX *ix;

    if (pir1_base) {
        uint64_t current_address =  (uint64_t)pir1_base +
            // (ir1_num * IR1_INST_SIZE);
            (ir1_num * sizeof(struct la_dt_insn));
        ix = (void *)current_address;
    } else {
        // ix = malloc(IR1_INST_SIZE);
        ix = malloc(sizeof(struct la_dt_insn));
    }
    
    NDSTATUS status = NdDecodeEx(ix, code, code_size, ND_CODE_32, ND_DATA_32);

    if (!ND_SUCCESS(status))
    {
#ifdef CONFIG_LATX_DEBUG
        fprintf(stderr, "%s: can't disasm code 0x%x at addr %p\n",
            __func__, *(uint32_t *)code, code);
#endif
        *insn = NULL;
        return -1;
    } 

    *insn = ix;

    return 1;
}

int labddisasm_get_16(const uint8_t *code, size_t code_size,
        struct _INSTRUX **insn,
        int ir1_num, void *pir1_base)
{
    INSTRUX *ix;

    if (pir1_base) {
        uint64_t current_address =  (uint64_t)pir1_base +
            // (ir1_num * IR1_INST_SIZE);
            (ir1_num * sizeof(struct la_dt_insn));
        ix = (void *)current_address;
    } else {
        // ix = malloc(IR1_INST_SIZE);
        ix = malloc(sizeof(struct la_dt_insn));
    }
    
    NDSTATUS status = NdDecodeEx(ix, code, code_size, ND_CODE_16, ND_DATA_16);

    if (!ND_SUCCESS(status))
    {
#ifdef CONFIG_LATX_DEBUG
        fprintf(stderr, "%s: can't disasm code 0x%x at addr %p\n",
            __func__, *(uint32_t *)code, code);
        *insn = NULL;
#endif
        return -1;
    } 

    *insn = ix;

    return 1;
}

