#include "capstone_shadow.h"

/*
 * Capstone shadow decoder.
 *
 * This module compares lengths only. It is deliberately not used to classify
 * branches or compute targets, so enabling --shadow-capstone cannot hide bugs
 * in the local CFG decoder.
 */

#include <capstone/capstone.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#define CAPSTONE_SHADOW_DETAIL_LIMIT 32

static csh cs_handle;
static bool cs_ready;

void capstone_shadow_summary_init(CapstoneShadowSummary *summary)
{
    memset(summary, 0, sizeof(*summary));
}

int capstone_shadow_init(void)
{
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &cs_handle) != CS_ERR_OK) {
        cs_ready = false;
        return -1;
    }
    cs_option(cs_handle, CS_OPT_DETAIL, CS_OPT_OFF);
    cs_ready = true;
    return 0;
}

void capstone_shadow_shutdown(void)
{
    if (cs_ready) {
        cs_close(&cs_handle);
        cs_ready = false;
    }
}

void capstone_shadow_check_function(const char *name, const uint8_t *buf,
                                    size_t size, uint64_t base,
                                    const Insn *insns, size_t insn_count,
                                    FILE *out,
                                    CapstoneShadowSummary *summary)
{
    if (!cs_ready) {
        return;
    }
    (void)base;

    size_t func_failures = 0;
    size_t func_mismatches = 0;

    /* Decode exactly one Capstone instruction at each local decoder boundary. */
    for (size_t i = 0; i < insn_count; i++) {
        const Insn *in = &insns[i];
        cs_insn *cs_insn = NULL;
        size_t n = cs_disasm(cs_handle, buf + in->off, size - in->off,
                             in->addr, 1, &cs_insn);
        summary->instructions_checked++;
        if (n == 0) {
            summary->decode_failures++;
            func_failures++;
            if (summary->reported < CAPSTONE_SHADOW_DETAIL_LIMIT) {
                fprintf(out,
                        "  capstone_shadow decode-fail function=%s"
                        " at=0x%016" PRIx64 " len=%zu\n",
                        name, in->addr, in->len);
                summary->reported++;
            }
            continue;
        }

        if (cs_insn[0].size != in->len) {
            summary->length_mismatches++;
            func_mismatches++;
            if (summary->reported < CAPSTONE_SHADOW_DETAIL_LIMIT) {
                fprintf(out,
                        "  capstone_shadow len-mismatch function=%s"
                        " at=0x%016" PRIx64
                        " cfg_len=%zu capstone_len=%u mnemonic=%s op=%s\n",
                        name, in->addr, in->len, cs_insn[0].size,
                        cs_insn[0].mnemonic, cs_insn[0].op_str);
                summary->reported++;
            }
        }
        cs_free(cs_insn, n);
    }

    summary->functions_checked++;
    if (func_failures || func_mismatches) {
        fprintf(out,
                "  capstone_shadow status=error failures=%zu"
                " mismatches=%zu instructions=%zu\n",
                func_failures, func_mismatches, insn_count);
    }
}

void capstone_shadow_print_summary(FILE *out,
                                   const CapstoneShadowSummary *summary)
{
    const char *status =
        (summary->decode_failures || summary->length_mismatches) ?
        "error" : "ok";

    fprintf(out, "capstone_shadow_summary\n");
    fprintf(out, "  status=%s\n", status);
    fprintf(out, "  functions_checked=%zu\n", summary->functions_checked);
    fprintf(out, "  instructions_checked=%zu\n",
            summary->instructions_checked);
    fprintf(out, "  decode_failures=%zu\n", summary->decode_failures);
    fprintf(out, "  length_mismatches=%zu\n", summary->length_mismatches);
}
