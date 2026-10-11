#ifndef FUNCTION_SYMBOLS_H
#define FUNCTION_SYMBOLS_H

/*
 * Function discovery.
 *
 * Prefer real ELF function symbols when available, then merge or fall back to
 * .eh_frame FDE ranges for stripped binaries. Entries are deduplicated by
 * address so symbol aliases do not cause duplicate CFG output.
 */

#include <stddef.h>
#include <stdint.h>

#include "elf_image.h"

typedef struct {
    char *name;
    uint64_t addr;
    uint64_t size;
    /* Section index used later to map the virtual function range to bytes. */
    unsigned shndx;
} FuncSym;

typedef struct {
    FuncSym *v;
    size_t n;
    size_t cap;
} FuncVec;

void funcs_load_all(const ElfFile *elf, FuncVec *funcs,
                    const char **source_out);
void funcs_free(FuncVec *funcs);

const FuncSym *funcs_find_by_entry(const FuncVec *funcs, uint64_t addr);
const FuncSym *funcs_find_containing(const FuncVec *funcs, uint64_t addr);

/* Name-based hot/cold relation; unavailable for synthetic stripped names. */
bool funcs_is_cold_fragment(const FuncSym *src, const FuncSym *dst);

#endif
