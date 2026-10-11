#ifndef LATC_BUNDLE_FORMAT_H
#define LATC_BUNDLE_FORMAT_H

#include <stdint.h>

#define LATC_BUNDLE_MAGIC "LATCBND1"
#define LATC_CFG_MAGIC "LATCCFG1"
#define LATC_BUNDLE_VERSION 4u
#define LATC_CFG_VERSION 5u

/* Compiler-only provenance carried from CFG selection into translation. */
#define LATC_CFG_TB_BOUNDED (1u << 2)

typedef struct __attribute__((packed)) LatcDiskCfgHeader {
    char magic[8];
    uint32_t version;
    uint32_t record_size;
    uint64_t function_count;
    uint64_t tb_count;
    uint64_t edge_count;
    uint64_t exec_range_count;
    uint64_t string_size;
} LatcDiskCfgHeader;

typedef struct __attribute__((packed)) LatcDiskFunction {
    uint64_t start, size, first_tb, tb_count, name_offset;
    uint32_t status, reserved;
} LatcDiskFunction;

typedef struct __attribute__((packed)) LatcDiskTb {
    uint64_t start, end, terminator_pc, first_edge, edge_count;
    uint64_t selected;
    uint32_t terminator, semantic_flags;
} LatcDiskTb;

typedef struct __attribute__((packed)) LatcDiskExecRange {
    uint64_t start, size;
} LatcDiskExecRange;

typedef struct __attribute__((packed)) LatcDiskEdge {
    uint64_t from, to;
    uint32_t kind, resolution;
} LatcDiskEdge;

typedef struct __attribute__((packed)) LatcDiskFooter {
    char magic[8];
    uint32_t version;
    uint32_t footer_size;
    uint64_t runner_size;
    uint64_t guest_offset;
    uint64_t guest_size;
    uint64_t cfg_offset;
    uint64_t cfg_size;
    uint64_t aot_offset;
    uint64_t aot_size;
    uint64_t function_count;
    uint64_t tb_count;
    uint64_t edge_count;
    uint64_t selected_tb_count;
    char guest_sha256[64];
    char aot_sha256[64];
    char aot_name[160];
} LatcDiskFooter;

#endif
