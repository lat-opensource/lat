#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "elf_image.h"

#include "common.h"

/*
 * ELF loading and address translation.
 *
 * All later modules operate on virtual addresses. This module is the only
 * place that knows how those virtual ranges map back to bytes in the file.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static uint8_t *read_file(const char *path, size_t *size_out)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        die_errno(path);
    }

    struct stat st;
    if (fstat(fileno(fp), &st) != 0) {
        die_errno("fstat");
    }
    if (st.st_size <= 0) {
        die("empty input file");
    }

    uint8_t *data = xmalloc((size_t)st.st_size);
    if (fread(data, 1, (size_t)st.st_size, fp) != (size_t)st.st_size) {
        die_errno("fread");
    }
    fclose(fp);
    *size_out = (size_t)st.st_size;
    return data;
}

/* Load and minimally validate the only ELF flavour this tool supports. */
void elf_load(const char *path, ElfFile *elf)
{
    elf->data = read_file(path, &elf->size);
    if (elf->size < sizeof(Elf64_Ehdr)) {
        die("file too small for ELF64 header");
    }

    elf->eh = (Elf64_Ehdr *)elf->data;
    if (memcmp(elf->eh->e_ident, ELFMAG, SELFMAG) != 0) {
        die("not an ELF file");
    }
    if (elf->eh->e_ident[EI_CLASS] != ELFCLASS64 ||
        elf->eh->e_ident[EI_DATA] != ELFDATA2LSB ||
        elf->eh->e_machine != EM_X86_64) {
        die("expected little-endian ELF64 x86-64 executable");
    }
    if (elf->eh->e_shentsize != sizeof(Elf64_Shdr)) {
        die("unexpected section header size");
    }
    if (!range_ok(elf->size, elf->eh->e_shoff,
                  (uint64_t)elf->eh->e_shnum * sizeof(Elf64_Shdr))) {
        die("section header table outside file");
    }

    elf->sh = (Elf64_Shdr *)(elf->data + elf->eh->e_shoff);
    if (elf->eh->e_shstrndx >= elf->eh->e_shnum) {
        die("bad shstrndx");
    }
    Elf64_Shdr *shstr = &elf->sh[elf->eh->e_shstrndx];
    if (!range_ok(elf->size, shstr->sh_offset, shstr->sh_size)) {
        die("section string table outside file");
    }
    elf->shstr = (const char *)elf->data + shstr->sh_offset;
}

/* A function symbol is usable only if it lives in executable bytes. */
bool elf_executable_section(const ElfFile *elf, unsigned idx)
{
    if (idx >= elf->eh->e_shnum) {
        return false;
    }
    return (elf->sh[idx].sh_flags & SHF_EXECINSTR) != 0;
}

/* Used to decide whether symbol discovery needs the .eh_frame fallback. */
bool elf_has_section_type(const ElfFile *elf, uint32_t type)
{
    for (unsigned i = 0; i < elf->eh->e_shnum; i++) {
        if (elf->sh[i].sh_type == type) {
            return true;
        }
    }
    return false;
}

/* Section-name lookup is section-header based; stripped files still keep it. */
const Elf64_Shdr *elf_find_section_by_name(const ElfFile *elf,
                                              const char *name,
                                              unsigned *idx_out)
{
    for (unsigned i = 0; i < elf->eh->e_shnum; i++) {
        const char *secname = elf->shstr + elf->sh[i].sh_name;
        if (strcmp(secname, name) == 0) {
            if (idx_out) {
                *idx_out = i;
            }
            return &elf->sh[i];
        }
    }
    return NULL;
}

/* Validate that an FDE or symbol range is fully inside one executable section. */
bool elf_find_exec_section_for_range(const ElfFile *elf, uint64_t addr,
                                        uint64_t size, unsigned *idx_out)
{
    for (unsigned i = 0; i < elf->eh->e_shnum; i++) {
        Elf64_Shdr *sec = &elf->sh[i];
        if (!elf_executable_section(elf, i) || sec->sh_type == SHT_NOBITS) {
            continue;
        }
        if (addr >= sec->sh_addr && size <= sec->sh_size &&
            addr - sec->sh_addr <= sec->sh_size - size) {
            *idx_out = i;
            return true;
        }
    }
    return false;
}


void elf_free(ElfFile *elf)
{
    free(elf->data);
    memset(elf, 0, sizeof(*elf));
}

bool elf_function_file_offset(const ElfFile *elf, uint64_t addr,
                              uint64_t size, unsigned shndx,
                              uint64_t *off_out)
{
    if (shndx >= elf->eh->e_shnum) {
        return false;
    }
    Elf64_Shdr *sec = &elf->sh[shndx];
    if (addr < sec->sh_addr ||
        size > sec->sh_size ||
        addr - sec->sh_addr > sec->sh_size - size) {
        return false;
    }
    uint64_t off = sec->sh_offset + (addr - sec->sh_addr);
    if (!range_ok(elf->size, off, size)) {
        return false;
    }
    *off_out = off;
    return true;
}

/* Jump-table recovery needs every allocated byte range, not just .text. */
IjmpSection *elf_build_ijmp_sections(const ElfFile *elf, size_t *count_out)
{
    IjmpSection *sections = xmalloc((size_t)elf->eh->e_shnum * sizeof(*sections));
    size_t n = 0;

    for (unsigned i = 0; i < elf->eh->e_shnum; i++) {
        Elf64_Shdr *sec = &elf->sh[i];
        if ((sec->sh_flags & SHF_ALLOC) == 0 || sec->sh_type == SHT_NOBITS ||
            sec->sh_size == 0) {
            continue;
        }
        if (!range_ok(elf->size, sec->sh_offset, sec->sh_size)) {
            continue;
        }
        sections[n++] = (IjmpSection){
            .addr = sec->sh_addr,
            .size = sec->sh_size,
            .off = sec->sh_offset,
        };
    }

    *count_out = n;
    return sections;
}


/* Keep got_plt independent from the concrete ElfFile owner type. */
GotPltElf elf_gotplt_view(const ElfFile *elf)
{
    return (GotPltElf){
        .data = elf->data,
        .size = elf->size,
        .eh = elf->eh,
        .sh = elf->sh,
        .shstr = elf->shstr,
    };
}

const char *elf_type_name(uint16_t type)
{
    switch (type) {
    case ET_EXEC:
        return "EXEC";
    case ET_DYN:
        return "DYN";
    case ET_REL:
        return "REL";
    default:
        return "OTHER";
    }
}

static bool has_program_header_type(const ElfFile *elf, uint32_t type)
{
    if (elf->eh->e_phoff == 0 || elf->eh->e_phentsize != sizeof(Elf64_Phdr) ||
        !range_ok(elf->size, elf->eh->e_phoff,
                  (uint64_t)elf->eh->e_phnum * sizeof(Elf64_Phdr))) {
        return false;
    }

    Elf64_Phdr *ph = (Elf64_Phdr *)(elf->data + elf->eh->e_phoff);
    for (unsigned i = 0; i < elf->eh->e_phnum; i++) {
        if (ph[i].p_type == type) {
            return true;
        }
    }
    return false;
}

/* PT_INTERP is the practical dynamic-linking marker for executables here. */
const char *elf_linking_kind(const ElfFile *elf)
{
    return has_program_header_type(elf, PT_INTERP) ? "dynamic" : "static";
}
