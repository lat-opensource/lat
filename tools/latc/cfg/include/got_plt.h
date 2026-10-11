#ifndef GOT_PLT_H
#define GOT_PLT_H

/*
 * GOT/PLT metadata and small call-site decoders.
 *
 * The CFG builder uses this module for two separate questions:
 *   1. Which GOT slots correspond to dynamic symbols or static IFUNC
 *      R_X86_64_IRELATIVE entries?
 *   2. Does an instruction encode a direct call or RIP-relative GOT access
 *      that can be annotated at the call/jump site?
 */

#include <elf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    GOTPLT_SYM_EXTERNAL,
    GOTPLT_SYM_IRELATIVE,
} GotPltSymKind;

typedef struct {
    uint64_t offset;
    char *name;
    GotPltSymKind kind;
} GotPltSym;

typedef struct {
    GotPltSym *v;
    size_t n;
    size_t cap;
} GotPltSymVec;

typedef struct {
    /* PLT stub virtual address and the GOT slot it ultimately dispatches via. */
    uint64_t addr;
    uint64_t got_offset;
} GotPltEntry;

typedef struct {
    GotPltEntry *v;
    size_t n;
    size_t cap;
} GotPltEntryVec;

typedef struct {
    /* Borrowed ELF view; ownership stays with ElfFile. */
    uint8_t *data;
    size_t size;
    Elf64_Ehdr *eh;
    Elf64_Shdr *sh;
    const char *shstr;
} GotPltElf;

void gotplt_load_symbols(const GotPltElf *elf, GotPltSymVec *got);
void gotplt_load_plt_entries(const GotPltElf *elf, GotPltEntryVec *plt);
void gotplt_free_symbols(GotPltSymVec *got);

const GotPltSym *gotplt_lookup_symbol(const GotPltSymVec *got,
                                      uint64_t offset);
const GotPltEntry *gotplt_lookup_plt(const GotPltEntryVec *plt,
                                     uint64_t addr);

size_t gotplt_count_kind(const GotPltSymVec *got, GotPltSymKind kind);

bool gotplt_decode_direct_call(const uint8_t *buf, size_t size, size_t off,
                               uint64_t addr, uint64_t *target);
bool gotplt_decode_rip_mem_ref(const uint8_t *buf, size_t size, size_t off,
                               uint64_t addr, uint8_t op, uint8_t group_reg,
                               uint64_t *target);

#endif
