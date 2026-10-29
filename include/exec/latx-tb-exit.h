/*
 * LATX translated block exit state helpers.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef EXEC_LATX_TB_EXIT_H
#define EXEC_LATX_TB_EXIT_H

#define LATX_CANLINK_EIP_STORED (-1)

static inline bool latx_tb_exit_resolve_pc(int8_t canlink, uint64_t tb_pc,
                                           int64_t lazypc,
                                           uint64_t *next_pc)
{
    if (canlink == LATX_CANLINK_EIP_STORED) {
        return false;
    }

    *next_pc = tb_pc + lazypc;
    return true;
}

#endif /* EXEC_LATX_TB_EXIT_H */
