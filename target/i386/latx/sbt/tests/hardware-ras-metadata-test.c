/* SPDX-License-Identifier: GPL-2.0-only */
#include "qemu/osdep.h"
#include "qemu/typedefs.h"
#include "exec/exec-all.h"
#include "hardware-ras.h"

void print_stack_trace(void)
{
}

int main(void)
{
    const target_ulong pc = 0x1000;
    target_ulong next_pc;
    int64_t metadata;

    g_assert(hras_pack_call_continuation(pc, 0x1010, 2) == 0);

    metadata = hras_pack_call_continuation(pc, 0x1020, 4);
    g_assert(hras_unpack_call_offset(metadata, pc, &next_pc));
    g_assert(next_pc == 0x1020);

    puts("PASS: hardware RAS CALL continuation metadata");
    return 0;
}
