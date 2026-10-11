/*
 * Local latc adapter over the copied LAT IR1 decoder.
 * This file is not part of the parent LAT build.
 */

#include "latc-verify.h"

#include "ir1.h"
#include "latx-disassemble-trace.h"

#include <stdbool.h>
#include <string.h>

#define LATC_MAX_TB_INSNS 256

void latc_lat_decoder_init(void)
{
#ifdef CONFIG_LATX_CAPSTONE_GIT
    gitcapstone_init(64);
#else
    lacapstone_init(64);
#endif
}

LatcVerifyStatus latc_lat_verify_tb(const uint8_t *code, size_t code_size,
                                    uint64_t guest_pc,
                                    uint64_t expected_end,
                                    LatcVerifyResult *result)
{
    LatcVerifyResult local = {
        .status = LATC_VERIFY_INVALID_INSTRUCTION,
        .start = guest_pc,
        .expected_end = expected_end,
        .actual_end = guest_pc,
    };
    struct la_dt_insn decoded[LATC_MAX_TB_INSNS];
    memset(decoded, 0, sizeof(decoded));

    if (!code || expected_end <= guest_pc) {
        if (result) *result = local;
        return local.status;
    }

    size_t offset = 0;
    for (unsigned i = 0; i < LATC_MAX_TB_INSNS; i++) {
        if (offset >= code_size) {
            local.status = LATC_VERIFY_BUFFER_TOO_SHORT;
            break;
        }
        struct la_dt_insn *info = NULL;
        int count = la_disa_v1(code + offset, code_size - offset,
                               guest_pc + offset, 1, &info, i, decoded, 1);
        if (count != 1 || !info || !info->size || info->size > code_size - offset) {
            local.status = LATC_VERIFY_INVALID_INSTRUCTION;
            break;
        }
        IR1_INST ir1 = { .info = info };
        offset += info->size;
        local.actual_end = guest_pc + offset;
        local.instruction_count = i + 1;
        local.terminator_id = info->id;
        if (ir1_is_tb_ending(&ir1)) {
            local.status = local.actual_end == expected_end ?
                LATC_VERIFY_MATCH : LATC_VERIFY_BOUNDARY_MISMATCH;
            break;
        }
        if (local.actual_end >= expected_end) {
            local.status = LATC_VERIFY_BOUNDARY_MISMATCH;
            break;
        }
        if (i + 1 == LATC_MAX_TB_INSNS) {
            local.status = LATC_VERIFY_TOO_MANY_INSTRUCTIONS;
        }
    }
    if (result) *result = local;
    return local.status;
}
