/*
 * Translation block jump reset.
 *
 * Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include "qemu/osdep.h"

#include "exec/exec-all.h"
#include "tcg/tcg.h"

/*
 * Reset the jump entry 'n' of a TB so that it is not chained to another TB.
 */
void tb_reset_jump(TranslationBlock *tb, int n)
{
    uintptr_t addr;

    assert(!use_tu_jmp(tb));
    addr = (uintptr_t)(tb->tc.ptr + tb->jmp_reset_offset[n]);
    tb_set_jmp_target(tb, n, addr);
#ifdef CONFIG_LATX_INSTS_PATTERN
    if (tb->eflags_target_arg[n] != TB_JMP_RESET_OFFSET_INVALID) {
        tb_eflag_recover(tb, n);
    }
#endif
#ifdef CONFIG_LATX_XCOMISX_OPT
    if (tb->jmp_stub_reset_offset[n] != TB_JMP_RESET_OFFSET_INVALID) {
        uintptr_t offset = tb->jmp_stub_target_arg[n];
        uintptr_t tc_ptr = (uintptr_t)tb->tc.ptr;
        uintptr_t jmp_rx = tc_ptr + offset;
        uintptr_t jmp_rw = jmp_rx - tcg_splitwx_diff;
        tb_target_set_nop(tc_ptr, jmp_rx, jmp_rw, addr);
    }
#endif
}
