#ifndef CAPSTONE_SHADOW_H
#define CAPSTONE_SHADOW_H

/*
 * Optional decoder validation against Capstone.
 *
 * Shadow comparison never drives CFG construction. It only checks whether the
 * local decoder's instruction boundaries agree with Capstone's decoded length
 * at the same address.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "cfg_decoder.h"

typedef struct {
    size_t functions_checked;
    size_t instructions_checked;
    size_t decode_failures;
    size_t length_mismatches;

    /* Limits per-run detail spam while preserving aggregate counters. */
    size_t reported;
} CapstoneShadowSummary;

void capstone_shadow_summary_init(CapstoneShadowSummary *summary);
int capstone_shadow_init(void);
void capstone_shadow_shutdown(void);

void capstone_shadow_check_function(const char *name, const uint8_t *buf,
                                    size_t size, uint64_t base,
                                    const Insn *insns, size_t insn_count,
                                    FILE *out,
                                    CapstoneShadowSummary *summary);

void capstone_shadow_print_summary(FILE *out,
                                   const CapstoneShadowSummary *summary);

#endif
