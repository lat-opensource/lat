#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "tcg/tcg.h"

int main(void)
{
    printf("{\"pc\":%zu,\"next\":%zu,"
           "\"flags\":%zu,\"ptr\":%zu,\"size\":%zu,"
           "\"jump\":%zu,\"patch\":%zu,\"tu_bit\":%u,\"jmp_bit\":%u,",
           offsetof(TranslationBlock, pc),
           offsetof(TranslationBlock, tu_next),
           offsetof(TranslationBlock, bool_flags),
           offsetof(TranslationBlock, tc.ptr),
           offsetof(TranslationBlock, tc.size),
           offsetof(TranslationBlock, tu_jmp),
           offsetof(TranslationBlock, jmp_target_arg), IS_TU_TB, IS_TU_JMP);
    printf("\"dest\":%zu,\"cflags\":%zu,\"invalid_bit\":%u,"
           "\"guest_size\":%zu,\"pc_size\":%zu,",
           offsetof(TranslationBlock, jmp_dest),
           offsetof(TranslationBlock, cflags), CF_INVALID,
           offsetof(TranslationBlock, size), sizeof(target_ulong));
    printf("\"code_ptr\":%zu,\"tb_ptr\":%zu,\"data_ptr\":%zu,"
           "\"code_limit\":%zu,\"tb_limit\":%zu}\n",
           offsetof(TCGContext, code_gen_ptr),
           offsetof(TCGContext, tb_gen_ptr),
           offsetof(TCGContext, data_gen_ptr),
           offsetof(TCGContext, code_gen_highwater),
           offsetof(TCGContext, tb_gen_highwater));
    return 0;
}
