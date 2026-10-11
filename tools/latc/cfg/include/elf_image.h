#ifndef ELF_IMAGE_H
#define ELF_IMAGE_H

/*
 * ELF image ownership and address mapping.
 *
 * The rest of the tool works in virtual addresses. This module owns the file
 * bytes and provides the conversions needed to locate function bodies and
 * allocated sections inside the on-disk ELF image.
 */

#include <elf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "got_plt.h"
#include "ijmp_resolve.h"

typedef struct {
    /* mmap-like owned file buffer loaded by elf_load(). */
    uint8_t *data;
    size_t size;

    /* Pointers into data; valid until elf_free(). */
    Elf64_Ehdr *eh;
    Elf64_Shdr *sh;
    const char *shstr;
} ElfFile;

void elf_load(const char *path, ElfFile *elf);
void elf_free(ElfFile *elf);

bool elf_executable_section(const ElfFile *elf, unsigned idx);
bool elf_has_section_type(const ElfFile *elf, uint32_t type);
const Elf64_Shdr *elf_find_section_by_name(const ElfFile *elf,
                                           const char *name,
                                           unsigned *idx_out);
bool elf_find_exec_section_for_range(const ElfFile *elf, uint64_t addr,
                                     uint64_t size, unsigned *idx_out);

/* Translate a function's virtual range to the corresponding file offset. */
bool elf_function_file_offset(const ElfFile *elf, uint64_t addr,
                              uint64_t size, unsigned shndx,
                              uint64_t *off_out);

/* Build allocated section metadata for indirect-jump table recovery. */
IjmpSection *elf_build_ijmp_sections(const ElfFile *elf, size_t *count_out);

/* Adapter view consumed by the GOT/PLT parser. */
GotPltElf elf_gotplt_view(const ElfFile *elf);

const char *elf_type_name(uint16_t type);
const char *elf_linking_kind(const ElfFile *elf);

#endif
