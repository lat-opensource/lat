#include "cfg_check.h"

/*
 * Integrity checks for the facts emitted by cfg_graph.
 *
 * Errors mean the graph is internally inconsistent. Warnings mean the graph is
 * structurally usable but intentionally open, usually because an edge leaves
 * the current function or an indirect jump could not be resolved statically.
 */

#include <inttypes.h>
#include <stdarg.h>
#include <string.h>

#define CFG_CHECK_DETAIL_LIMIT 8

static const char *term_name(CfgCheckTerm term)
{
    switch (term) {
    case CFG_CHECK_TERM_JCC:
        return "jcc";
    case CFG_CHECK_TERM_JMP:
        return "jmp";
    case CFG_CHECK_TERM_IJMP:
        return "ijmp";
    case CFG_CHECK_TERM_RET:
        return "ret";
    case CFG_CHECK_TERM_STOP:
        return "stop";
    default:
        return "normal";
    }
}

static const char *edge_name(CfgCheckEdgeKind kind)
{
    switch (kind) {
    case CFG_CHECK_EDGE_FALL:
        return "fall";
    case CFG_CHECK_EDGE_TRUE:
        return "true";
    case CFG_CHECK_EDGE_FALSE:
        return "false";
    case CFG_CHECK_EDGE_JMP:
        return "jmp";
    case CFG_CHECK_EDGE_CASE:
        return "case";
    case CFG_CHECK_EDGE_EXTERNAL:
        return "external";
    default:
        return "?";
    }
}

static bool legal_cross_function_xfer(CfgCheckXferKind kind)
{
    return kind == CFG_CHECK_XFER_TAILCALL ||
           kind == CFG_CHECK_XFER_PLT_TAILCALL ||
           kind == CFG_CHECK_XFER_INTERPROCEDURAL ||
           kind == CFG_CHECK_XFER_COLD_FRAGMENT ||
           kind == CFG_CHECK_XFER_EXTERNAL;
}

static void count_xfer(CfgCheckResult *result, CfgCheckXferKind kind)
{
    switch (kind) {
    case CFG_CHECK_XFER_TAILCALL:
        result->tailcall_edges++;
        break;
    case CFG_CHECK_XFER_PLT_TAILCALL:
        result->plt_tailcall_edges++;
        break;
    case CFG_CHECK_XFER_INTERPROCEDURAL:
        result->interprocedural_edges++;
        break;
    case CFG_CHECK_XFER_COLD_FRAGMENT:
        result->cold_fragment_edges++;
        break;
    case CFG_CHECK_XFER_EXTERNAL:
        result->external_exit_edges++;
        break;
    default:
        break;
    }
}

static bool in_func(uint64_t addr, uint64_t start, uint64_t size)
{
    return addr >= start && addr < start + size;
}

static bool has_block(const CfgCheckBlock *blocks, size_t block_count,
                      uint64_t start)
{
    size_t lo = 0;
    size_t hi = block_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (blocks[mid].start < start) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo < block_count && blocks[lo].start == start) {
        return true;
    }
    return false;
}

static bool edge_exists(const CfgCheckEdge *edges, size_t edge_count,
                        uint64_t from, CfgCheckEdgeKind kind)
{
    for (size_t i = 0; i < edge_count; i++) {
        if (edges[i].from == from && edges[i].kind == kind) {
            return true;
        }
    }
    return false;
}

static bool block_has_edge(const CfgCheckEdge *edges, size_t edge_count,
                           uint64_t from)
{
    for (size_t i = 0; i < edge_count; i++) {
        if (edges[i].from == from) {
            return true;
        }
    }
    return false;
}

static void emit_issue(FILE *out, size_t *emitted, size_t *omitted,
                       const char *level, const char *fmt, ...)
{
    /* Keep per-function output readable while preserving aggregate counts. */
    if (*emitted >= CFG_CHECK_DETAIL_LIMIT) {
        (*omitted)++;
        return;
    }

    fprintf(out, "    check %s ", level);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    putc('\n', out);
    (*emitted)++;
}

void cfg_check_summary_init(CfgCheckSummary *summary)
{
    memset(summary, 0, sizeof(*summary));
}

void cfg_check_summary_add_result(CfgCheckSummary *summary,
                                  const CfgCheckResult *result,
                                  size_t block_count,
                                  size_t edge_count)
{
    if (!summary) {
        return;
    }

    summary->functions_checked++;
    summary->blocks += block_count;
    summary->edges += edge_count;
    summary->errors += result->errors;
    summary->warnings += result->warnings;
    summary->block_gaps += result->block_gaps;
    summary->block_overlaps += result->block_overlaps;
    summary->bad_blocks += result->bad_blocks;
    summary->unclosed_internal_edges += result->unclosed_internal_edges;
    summary->cross_function_edges += result->cross_function_edges;
    summary->unresolved_cross_function_edges +=
        result->unresolved_cross_function_edges;
    summary->legal_cross_function_edges +=
        result->legal_cross_function_edges;
    summary->tailcall_edges += result->tailcall_edges;
    summary->plt_tailcall_edges += result->plt_tailcall_edges;
    summary->interprocedural_edges += result->interprocedural_edges;
    summary->cold_fragment_edges += result->cold_fragment_edges;
    summary->external_exit_edges += result->external_exit_edges;
    summary->prefix_entry_edges += result->prefix_entry_edges;
    summary->open_indirect_jumps += result->open_indirect_jumps;
    summary->missing_fallthrough_edges += result->missing_fallthrough_edges;
    if (result->errors) {
        summary->error_functions++;
    } else if (result->warnings) {
        summary->open_functions++;
    } else {
        summary->ok_functions++;
    }
}

CfgCheckResult cfg_check_function(const char *name, uint64_t func_addr,
                                  uint64_t func_size,
                                  const CfgCheckBlock *blocks,
                                  size_t block_count,
                                  const CfgCheckEdge *edges,
                                  size_t edge_count,
                                  FILE *out,
                                  CfgCheckSummary *summary)
{
    CfgCheckResult result;
    memset(&result, 0, sizeof(result));

    size_t emitted = 0;
    size_t omitted = 0;
    uint64_t func_end = func_addr + func_size;

    /* Block coverage must be ordered, non-overlapping, and inside function. */
    if (block_count == 0) {
        result.errors++;
        result.bad_blocks++;
        emit_issue(out, &emitted, &omitted, "error",
                   "function %s has no basic blocks", name);
    }

    uint64_t prev_end = func_addr;
    for (size_t i = 0; i < block_count; i++) {
        const CfgCheckBlock *bb = &blocks[i];
        if (bb->start < func_addr || bb->end > func_end ||
            bb->start >= bb->end ||
            !in_func(bb->term_addr, func_addr, func_size)) {
            result.errors++;
            result.bad_blocks++;
            emit_issue(out, &emitted, &omitted, "error",
                       "bad block 0x%016" PRIx64 "-0x%016" PRIx64
                       " term=%s @0x%016" PRIx64,
                       bb->start, bb->end, term_name(bb->term), bb->term_addr);
        }
        if (bb->start < prev_end) {
            result.errors++;
            result.block_overlaps++;
            emit_issue(out, &emitted, &omitted, "error",
                       "overlap before block 0x%016" PRIx64
                       " previous_end=0x%016" PRIx64,
                       bb->start, prev_end);
        } else if (bb->start > prev_end) {
            result.warnings++;
            result.block_gaps++;
            emit_issue(out, &emitted, &omitted, "warn",
                       "coverage gap 0x%016" PRIx64 "-0x%016" PRIx64,
                       prev_end, bb->start);
        }
        if (bb->end > prev_end) {
            prev_end = bb->end;
        }
    }

    if (prev_end < func_end) {
        result.warnings++;
        result.block_gaps++;
        emit_issue(out, &emitted, &omitted, "warn",
                   "coverage gap 0x%016" PRIx64 "-0x%016" PRIx64,
                   prev_end, func_end);
    }

    for (size_t i = 0; i < edge_count; i++) {
        const CfgCheckEdge *edge = &edges[i];
        if (!has_block(blocks, block_count, edge->from)) {
            result.errors++;
            result.unclosed_internal_edges++;
            emit_issue(out, &emitted, &omitted, "error",
                       "%s edge has non-block source 0x%016" PRIx64,
                       edge_name(edge->kind), edge->from);
        }

        if (!in_func(edge->to, func_addr, func_size)) {
            result.cross_function_edges++;
            if (legal_cross_function_xfer(edge->xfer_kind)) {
                result.legal_cross_function_edges++;
                count_xfer(&result, edge->xfer_kind);
            } else {
                result.warnings++;
                result.unresolved_cross_function_edges++;
                emit_issue(out, &emitted, &omitted, "warn",
                           "%s edge crosses function boundary 0x%016" PRIx64
                           " -> 0x%016" PRIx64,
                           edge_name(edge->kind), edge->from, edge->to);
            }
            continue;
        }

        if (!has_block(blocks, block_count, edge->to)) {
            if (edge->prefix_entry) {
                /* lock/rep-style prefix entries are tracked but accepted. */
                result.prefix_entry_edges++;
                continue;
            }
            result.errors++;
            result.unclosed_internal_edges++;
            emit_issue(out, &emitted, &omitted, "error",
                       "%s edge target is not a block start 0x%016" PRIx64
                       " -> 0x%016" PRIx64,
                       edge_name(edge->kind), edge->from, edge->to);
        }
    }

    for (size_t i = 0; i < block_count; i++) {
        const CfgCheckBlock *bb = &blocks[i];
        /* Verify that each terminator kind has the expected edge shape. */
        switch (bb->term) {
        case CFG_CHECK_TERM_NORMAL:
            if (bb->end < func_end &&
                !edge_exists(edges, edge_count, bb->start,
                             CFG_CHECK_EDGE_FALL)) {
                result.errors++;
                result.missing_fallthrough_edges++;
                emit_issue(out, &emitted, &omitted, "error",
                           "normal block lacks fall edge 0x%016" PRIx64,
                           bb->start);
            }
            break;
        case CFG_CHECK_TERM_JCC:
            if (!edge_exists(edges, edge_count, bb->start,
                             CFG_CHECK_EDGE_TRUE)) {
                result.errors++;
                result.missing_fallthrough_edges++;
                emit_issue(out, &emitted, &omitted, "error",
                           "jcc block lacks true edge 0x%016" PRIx64,
                           bb->start);
            }
            if (bb->end < func_end &&
                !edge_exists(edges, edge_count, bb->start,
                             CFG_CHECK_EDGE_FALSE)) {
                result.errors++;
                result.missing_fallthrough_edges++;
                emit_issue(out, &emitted, &omitted, "error",
                           "jcc block lacks false edge 0x%016" PRIx64,
                           bb->start);
            }
            break;
        case CFG_CHECK_TERM_JMP:
            if (!block_has_edge(edges, edge_count, bb->start)) {
                result.errors++;
                result.unclosed_internal_edges++;
                emit_issue(out, &emitted, &omitted, "error",
                           "jmp block lacks outgoing edge 0x%016" PRIx64,
                           bb->start);
            }
            break;
        case CFG_CHECK_TERM_IJMP:
            if (!block_has_edge(edges, edge_count, bb->start)) {
                result.warnings++;
                result.open_indirect_jumps++;
                emit_issue(out, &emitted, &omitted, "warn",
                           "ijmp block remains open 0x%016" PRIx64,
                           bb->start);
            }
            break;
        case CFG_CHECK_TERM_RET:
        case CFG_CHECK_TERM_STOP:
            if (block_has_edge(edges, edge_count, bb->start)) {
                result.errors++;
                result.unclosed_internal_edges++;
                emit_issue(out, &emitted, &omitted, "error",
                           "%s block has outgoing edge 0x%016" PRIx64,
                           term_name(bb->term), bb->start);
            }
            break;
        }
    }

    const char *status = "ok";
    if (result.errors) {
        status = "error";
    } else if (result.warnings) {
        status = "open";
    }
    fprintf(out,
            "  cfg_check status=%s errors=%zu warnings=%zu"
            " blocks=%zu edges=%zu",
            status, result.errors, result.warnings, block_count, edge_count);
    if (result.prefix_entry_edges) {
        fprintf(out, " prefix_entry_edges=%zu",
                result.prefix_entry_edges);
    }
    if (result.legal_cross_function_edges) {
        fprintf(out, " legal_cross_function_edges=%zu",
                result.legal_cross_function_edges);
    }
    if (result.tailcall_edges) {
        fprintf(out, " tailcall_edges=%zu", result.tailcall_edges);
    }
    if (result.plt_tailcall_edges) {
        fprintf(out, " plt_tailcall_edges=%zu", result.plt_tailcall_edges);
    }
    if (result.interprocedural_edges) {
        fprintf(out, " interprocedural_edges=%zu",
                result.interprocedural_edges);
    }
    if (result.cold_fragment_edges) {
        fprintf(out, " cold_fragment_edges=%zu",
                result.cold_fragment_edges);
    }
    if (result.external_exit_edges) {
        fprintf(out, " external_exit_edges=%zu",
                result.external_exit_edges);
    }
    putc('\n', out);
    if (omitted) {
        fprintf(out, "    check info omitted_issues=%zu\n", omitted);
    }

    cfg_check_summary_add_result(summary, &result, block_count, edge_count);

    return result;
}

void cfg_check_print_summary_named(FILE *out, const char *title,
                                   const CfgCheckSummary *summary)
{
    const char *status = "ok";
    if (summary->errors) {
        status = "error";
    } else if (summary->warnings) {
        status = "open";
    }

    fprintf(out, "%s\n", title);
    fprintf(out, "  status=%s\n", status);
    fprintf(out, "  functions_checked=%zu\n", summary->functions_checked);
    fprintf(out, "  ok_functions=%zu\n", summary->ok_functions);
    fprintf(out, "  open_functions=%zu\n", summary->open_functions);
    fprintf(out, "  error_functions=%zu\n", summary->error_functions);
    fprintf(out, "  blocks=%zu\n", summary->blocks);
    fprintf(out, "  edges=%zu\n", summary->edges);
    fprintf(out, "  errors=%zu\n", summary->errors);
    fprintf(out, "  warnings=%zu\n", summary->warnings);
    fprintf(out, "  block_gaps=%zu\n", summary->block_gaps);
    fprintf(out, "  block_overlaps=%zu\n", summary->block_overlaps);
    fprintf(out, "  bad_blocks=%zu\n", summary->bad_blocks);
    fprintf(out, "  unclosed_internal_edges=%zu\n",
            summary->unclosed_internal_edges);
    fprintf(out, "  cross_function_edges=%zu\n",
            summary->cross_function_edges);
    fprintf(out, "  unresolved_cross_function_edges=%zu\n",
            summary->unresolved_cross_function_edges);
    fprintf(out, "  legal_cross_function_edges=%zu\n",
            summary->legal_cross_function_edges);
    fprintf(out, "  tailcall_edges=%zu\n", summary->tailcall_edges);
    fprintf(out, "  plt_tailcall_edges=%zu\n",
            summary->plt_tailcall_edges);
    fprintf(out, "  interprocedural_edges=%zu\n",
            summary->interprocedural_edges);
    fprintf(out, "  cold_fragment_edges=%zu\n",
            summary->cold_fragment_edges);
    fprintf(out, "  external_exit_edges=%zu\n",
            summary->external_exit_edges);
    fprintf(out, "  prefix_entry_edges=%zu\n",
            summary->prefix_entry_edges);
    fprintf(out, "  open_indirect_jumps=%zu\n",
            summary->open_indirect_jumps);
    fprintf(out, "  missing_fallthrough_edges=%zu\n",
            summary->missing_fallthrough_edges);
}

void cfg_check_print_summary(FILE *out, const CfgCheckSummary *summary)
{
    cfg_check_print_summary_named(out, "cfg_check_summary", summary);
}
