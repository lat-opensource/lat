#ifndef LATC_CFG_PROGRAM_H
#define LATC_CFG_PROGRAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    CFG_TB_FALLTHROUGH,
    CFG_TB_CALL,
    CFG_TB_INDIRECT_CALL,
    CFG_TB_CONDITIONAL,
    CFG_TB_JUMP,
    CFG_TB_INDIRECT_JUMP,
    CFG_TB_RETURN,
    CFG_TB_SYSCALL,
    CFG_TB_STOP,
} CfgTbTerm;

typedef enum {
    CFG_EDGE_FALLTHROUGH,
    CFG_EDGE_TRUE,
    CFG_EDGE_FALSE,
    CFG_EDGE_JUMP,
    CFG_EDGE_CALL,
    CFG_EDGE_CALL_RETURN,
    CFG_EDGE_CASE,
    CFG_EDGE_RUNTIME,
} CfgProgramEdgeKind;

typedef enum {
    CFG_EDGE_STATIC,
    CFG_EDGE_RUNTIME_RESOLVED,
} CfgEdgeResolution;

typedef enum {
    CFG_FUNCTION_OK,
    CFG_FUNCTION_OPEN,
    CFG_FUNCTION_ERROR,
} CfgFunctionStatus;

enum CfgTbSemanticFlag {
    CFG_TB_CODE64 = 1u << 0,
    CFG_TB_PARALLEL = 1u << 1,
    /* Internal compiler provenance.  This block was added to cover a whole
     * indirect-entry function, not observed by JIT. */
    CFG_TB_BOUNDED = 1u << 2,
};

typedef struct {
    uint64_t from;
    uint64_t to;
    CfgProgramEdgeKind kind;
    CfgEdgeResolution resolution;
} CfgProgramEdge;

typedef struct {
    uint64_t start;
    uint64_t end;
    uint64_t terminator_pc;
    bool selected;
    bool observed;
    uint32_t semantic_flags;
    CfgTbTerm terminator;
    size_t first_edge;
    size_t edge_count;
} CfgTb;

typedef struct {
    char *name;
    uint64_t start;
    uint64_t size;
    CfgFunctionStatus status;
    size_t first_tb;
    size_t tb_count;
} CfgProgramFunction;

typedef struct {
    uint64_t start;
    uint64_t size;
} CfgExecRange;

typedef struct {
    CfgProgramFunction *functions;
    size_t function_count;
    CfgTb *tbs;
    size_t tb_count;
    CfgProgramEdge *edges;
    size_t edge_count;
    CfgExecRange *exec_ranges;
    size_t exec_range_count;
    size_t open_function_count;
    size_t error_function_count;
    size_t resolved_jump_tables;
    size_t resolved_jump_table_targets;
} CfgProgram;

typedef struct {
    bool resolve_jump_tables;
} CfgAnalyzeOptions;

int cfg_analyze_elf(const char *path, const CfgAnalyzeOptions *options,
                    CfgProgram *program, char *error, size_t error_size);
void cfg_program_destroy(CfgProgram *program);
bool cfg_program_address_is_executable(const CfgProgram *program,
                                       uint64_t address);

const char *cfg_tb_term_name(CfgTbTerm term);
const char *cfg_program_edge_kind_name(CfgProgramEdgeKind kind);
const char *cfg_function_status_name(CfgFunctionStatus status);

#endif
