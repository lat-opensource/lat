#ifndef APP_OWNERSHIP_H
#define APP_OWNERSHIP_H

/*
 * Heuristic application-code ownership.
 *
 * Static binaries do not preserve a formal "application vs libc" boundary.
 * This module provides a deliberately simple classifier for SPEC-style
 * unstripped binaries: start at main, skip known runtime/library helpers, and
 * stop when library/runtime code begins after the first application cluster.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "cfg_check.h"
#include "cfg_graph.h"
#include "function_symbols.h"

typedef struct {
    bool available;
    uint64_t start;
    uint64_t end;
    size_t functions;
    const char *method;
    const char *reason;
} AppOwnership;

void app_ownership_detect(const FuncVec *funcs, AppOwnership *owner);
bool app_ownership_contains(const AppOwnership *owner, const FuncSym *fn);
void app_ownership_print_filter_summary(FILE *out,
                                        const AppOwnership *owner);
void app_ownership_print_result_summary(FILE *out,
                                        const AppOwnership *owner,
                                        const CfgCheckSummary *check,
                                        const CfgGraphSummary *graph);

#endif
