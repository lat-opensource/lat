#ifndef IJMP_RESOLVE_H
#define IJMP_RESOLVE_H

/*
 * Indirect jump recovery for compiler-generated jump tables.
 *
 * The resolver is intentionally pattern-based. It recognizes common GCC
 * sequences around FF /4 and FF /5 exits, validates candidate targets against
 * decoded instruction boundaries, and leaves non-table dispatch unresolved.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IJMP_MAX_TARGETS 512

typedef struct {
    /* Allocated section range in virtual address space and file offset space. */
    uint64_t addr;
    uint64_t size;
    uint64_t off;
} IjmpSection;

typedef struct {
    /* Whole-file view used to read candidate table entries. */
    const uint8_t *file;
    size_t file_size;

    const IjmpSection *sections;
    size_t section_count;

    /* Valid branch targets for the current function. */
    const uint64_t *insn_addrs;
    size_t insn_count;
    /* One byte per function byte for constant-time local target checks. */
    const uint8_t *insn_bitmap;
    size_t insn_bitmap_size;

    /*
     * Whole-program instruction boundaries. GCC may place cold partitions in
     * a separate function symbol while still using their internal labels as
     * switch-table case targets.
     */
    const uint64_t *program_insn_addrs;
    size_t program_insn_count;

    const uint64_t *direct_targets;
    size_t direct_target_count;

    /*
     * Optional whole-program function-entry predicate. This lets qword
     * function-pointer tables resolve interprocedural tail-dispatch targets
     * without accepting arbitrary cross-function instruction addresses.
     */
    bool (*is_func_entry)(const void *data, uint64_t addr);
    const void *func_entry_data;

    uint64_t func_addr;
    uint64_t func_size;
} IjmpContext;

typedef struct {
    uint64_t table_addr;
    uint64_t targets[IJMP_MAX_TARGETS];
    size_t count;
} IjmpResult;

bool ijmp_resolve_jump_table(const IjmpContext *ctx, const uint8_t *func,
                             size_t func_size, size_t ijmp_off,
                             IjmpResult *out);

#endif
