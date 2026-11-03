/* SPDX-License-Identifier: GPL-2.0-only */
#include "qemu-def.h"
#include "../../../../../accel/tcg/cpu-exec.c"

int qemu_loglevel;
static unsigned patched;

bool qemu_log_in_addr_range(uint64_t addr G_GNUC_UNUSED)
{
    return false;
}
int qemu_log(const char *fmt G_GNUC_UNUSED, ...)
{
    return 0;
}

bool use_tu_jmp(TranslationBlock *tb)
{
    return tb->bool_flags & IS_TU_JMP;
}
bool tb_need_relink(TranslationBlock *tb, int n)
{
    return tb->bool_flags & (n ? SIGNAL_RELINK1 : SIGNAL_RELINK0);
}
void clear_signal_link_flag(TranslationBlock *tb, int n)
{
    tb->bool_flags &= ~(n ? SIGNAL_RELINK1 | SIGNAL_UNLINK1 :
                           SIGNAL_RELINK0 | SIGNAL_UNLINK0);
}
void latx_tb_set_jmp_target(TranslationBlock *tb G_GNUC_UNUSED,
                            int n G_GNUC_UNUSED,
                            TranslationBlock *next G_GNUC_UNUSED)
{
    patched++;
}
int main(void)
{
    for (unsigned int n = 0; n < 2; n++) {
        TranslationBlock tb = { 0 }, next = { 0 };
        unsigned int before = patched;

        qemu_spin_init(&next.jmp_lock);
        tb.jmp_target_arg[n] = TB_JMP_RESET_OFFSET_INVALID;
        tb_add_jump(&tb, n, &next);
        g_assert(patched == before);
        g_assert(tb.jmp_dest[n] == 0 && next.jmp_list_head == 0);

        /* Offset zero is a valid jump slot, not an absent slot. */
        tb.jmp_target_arg[n] = 0;
        tb_add_jump(&tb, n, &next);
        g_assert(patched == before + 1);
        g_assert(tb.jmp_dest[n] == (uintptr_t)&next);
        g_assert(next.jmp_list_head == ((uintptr_t)&tb | n));
        tb_add_jump(&tb, n, &next);
        g_assert(patched == before + 1);

        tb.bool_flags |= n ? SIGNAL_RELINK1 : SIGNAL_RELINK0;
        tb_add_jump(&tb, n, &next);
        g_assert(patched == before + 2);
        g_assert(!tb_need_relink(&tb, n));
        qemu_spin_destroy(&next.jmp_lock);
    }
    puts("PASS: slotless AOT edges cannot patch code "
         "or claim an incoming link");
    return 0;
}
