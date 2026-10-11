#ifndef CFG_CHECK_H
#define CFG_CHECK_H

/*
 * CFG integrity checker.
 *
 * The builder prints a best-effort graph even for stripped or unresolved code.
 * This checker records whether that graph is internally closed, crosses known
 * function boundaries, or still depends on unresolved indirect exits.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    CFG_CHECK_TERM_NORMAL,
    CFG_CHECK_TERM_JCC,
    CFG_CHECK_TERM_JMP,
    CFG_CHECK_TERM_IJMP,
    CFG_CHECK_TERM_RET,
    CFG_CHECK_TERM_STOP,
} CfgCheckTerm;

typedef enum {
    CFG_CHECK_EDGE_FALL,
    CFG_CHECK_EDGE_TRUE,
    CFG_CHECK_EDGE_FALSE,
    CFG_CHECK_EDGE_JMP,
    CFG_CHECK_EDGE_CASE,
    CFG_CHECK_EDGE_EXTERNAL,
} CfgCheckEdgeKind;

typedef enum {
    CFG_CHECK_XFER_NORMAL,
    CFG_CHECK_XFER_TAILCALL,
    CFG_CHECK_XFER_PLT_TAILCALL,
    CFG_CHECK_XFER_INTERPROCEDURAL,
    CFG_CHECK_XFER_COLD_FRAGMENT,
    CFG_CHECK_XFER_EXTERNAL,
} CfgCheckXferKind;

typedef struct {
    uint64_t start;
    uint64_t end;
    uint64_t term_addr;
    CfgCheckTerm term;
} CfgCheckBlock;

typedef struct {
    uint64_t from;
    uint64_t to;
    CfgCheckEdgeKind kind;
    CfgCheckXferKind xfer_kind;

    /* Branches into legacy prefixes are allowed for patterns such as lock cmpxchg. */
    bool prefix_entry;
} CfgCheckEdge;

typedef struct {
    size_t functions_checked;
    size_t ok_functions;
    size_t open_functions;
    size_t error_functions;
    size_t blocks;
    size_t edges;
    size_t errors;
    size_t warnings;
    size_t block_gaps;
    size_t block_overlaps;
    size_t bad_blocks;
    size_t unclosed_internal_edges;
    size_t cross_function_edges;
    size_t unresolved_cross_function_edges;
    size_t legal_cross_function_edges;
    size_t tailcall_edges;
    size_t plt_tailcall_edges;
    size_t interprocedural_edges;
    size_t cold_fragment_edges;
    size_t external_exit_edges;
    size_t prefix_entry_edges;
    size_t open_indirect_jumps;
    size_t missing_fallthrough_edges;
} CfgCheckSummary;

typedef struct {
    size_t errors;
    size_t warnings;
    size_t block_gaps;
    size_t block_overlaps;
    size_t bad_blocks;
    size_t unclosed_internal_edges;
    size_t cross_function_edges;
    size_t unresolved_cross_function_edges;
    size_t legal_cross_function_edges;
    size_t tailcall_edges;
    size_t plt_tailcall_edges;
    size_t interprocedural_edges;
    size_t cold_fragment_edges;
    size_t external_exit_edges;
    size_t prefix_entry_edges;
    size_t open_indirect_jumps;
    size_t missing_fallthrough_edges;
} CfgCheckResult;

void cfg_check_summary_init(CfgCheckSummary *summary);
void cfg_check_summary_add_result(CfgCheckSummary *summary,
                                  const CfgCheckResult *result,
                                  size_t block_count,
                                  size_t edge_count);

CfgCheckResult cfg_check_function(const char *name, uint64_t func_addr,
                                  uint64_t func_size,
                                  const CfgCheckBlock *blocks,
                                  size_t block_count,
                                  const CfgCheckEdge *edges,
                                  size_t edge_count,
                                  FILE *out,
                                  CfgCheckSummary *summary);

void cfg_check_print_summary_named(FILE *out, const char *title,
                                   const CfgCheckSummary *summary);
void cfg_check_print_summary(FILE *out, const CfgCheckSummary *summary);

#endif
