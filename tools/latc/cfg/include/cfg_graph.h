#ifndef CFG_GRAPH_H
#define CFG_GRAPH_H

/*
 * Per-function CFG construction and printing.
 *
 * This module owns the high-level graph walk: decode all instruction
 * boundaries, choose basic-block leaders, annotate calls and indirect exits,
 * emit edges, and hand the final facts to cfg_check.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "capstone_shadow.h"
#include "cfg_check.h"
#include "function_symbols.h"
#include "got_plt.h"
#include "ijmp_resolve.h"

typedef struct {
    size_t functions_printed;
    size_t unresolved_indirect_calls;
    size_t unresolved_indirect_jumps;
    size_t resolved_indirect_jump_tables;
    size_t resolved_indirect_jump_table_targets;
} CfgGraphSummary;

typedef struct {
    CfgCheckSummary *check;
    CfgGraphSummary *graph;
} CfgSummarySink;

void cfg_graph_summary_init(CfgGraphSummary *summary);
void cfg_graph_summary_add(CfgGraphSummary *summary,
                           const CfgGraphSummary *delta);
void cfg_graph_print_summary(FILE *out, const char *title,
                             const CfgGraphSummary *summary);

void cfg_print_function(const uint8_t *code, size_t code_size,
                        uint64_t file_off, const FuncSym *fn,
                        const FuncVec *funcs,
                        const IjmpSection *sections, size_t section_count,
                        const GotPltSymVec *got,
                        const GotPltEntryVec *plt,
                        const char *filter,
                        const CfgSummarySink *sinks,
                        size_t sink_count,
                        bool shadow_capstone,
                        CapstoneShadowSummary *shadow_summary);

#endif
