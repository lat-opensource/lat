/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"

#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wrappedlib-preflight.h"

static bool wrapped_symbol_is_supported(
    const char *name, const char *const *supported_symbols,
    size_t supported_symbol_count)
{
    for (size_t i = 0; i < supported_symbol_count; i++) {
        if (!strcmp(name, supported_symbols[i])) {
            return true;
        }
    }
    return false;
}

static bool read_exact(FILE *file, void *buffer, size_t size, long offset)
{
    return fseek(file, offset, SEEK_SET) == 0 &&
           fread(buffer, 1, size, file) == size;
}

static bool range_is_valid(uint64_t offset, uint64_t size,
                           uint64_t file_size)
{
    return offset <= file_size && size <= file_size - offset;
}

bool latx_wrappedlib_preflight_guest(
    const char *guest_path, const char *host_soname,
    const char *library_label, LatxWrappedSymbolFilter symbol_filter,
    const char *const *supported_symbols, size_t supported_symbol_count,
    const char *const *required_host_symbols,
    size_t required_host_symbol_count,
    char *reason, size_t reason_size)
{
    Elf64_Ehdr ehdr;
    Elf64_Shdr *sections = NULL;
    FILE *file = NULL;
    void *host = NULL;
    bool found_dynsym = false;
    bool found_library_symbol = false;
    bool ok = false;
    long file_size;

    if (!reason || !reason_size) {
        return false;
    }
    reason[0] = '\0';
    if (!guest_path || !host_soname || !library_label || !symbol_filter ||
        !supported_symbols || !supported_symbol_count ||
        (required_host_symbol_count && !required_host_symbols)) {
        snprintf(reason, reason_size,
                 "wrapped-library preflight configuration is incomplete");
        return false;
    }

    file = fopen(guest_path, "rb");
    if (!file) {
        snprintf(reason, reason_size, "cannot open guest ELF '%s': %s",
                 guest_path, strerror(errno));
        goto out;
    }
    if (fseek(file, 0, SEEK_END) != 0 ||
        (file_size = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        snprintf(reason, reason_size,
                 "cannot determine guest %s ELF size", library_label);
        goto out;
    }
    if (fread(&ehdr, 1, sizeof(ehdr), file) != sizeof(ehdr) ||
        memcmp(ehdr.e_ident, ELFMAG, SELFMAG) ||
        ehdr.e_ident[EI_CLASS] != ELFCLASS64 ||
        ehdr.e_ident[EI_DATA] != ELFDATA2LSB ||
        ehdr.e_type != ET_DYN || ehdr.e_machine != EM_X86_64 ||
        ehdr.e_shentsize != sizeof(Elf64_Shdr) || !ehdr.e_shoff ||
        !ehdr.e_shnum ||
        !range_is_valid(ehdr.e_shoff,
                        (uint64_t)ehdr.e_shnum * sizeof(Elf64_Shdr),
                        file_size)) {
        snprintf(reason, reason_size,
                 "guest %s is not a supported ELF64 little-endian shared object",
                 library_label);
        goto out;
    }
    sections = calloc(ehdr.e_shnum, sizeof(*sections));
    if (!sections ||
        !read_exact(file, sections, ehdr.e_shnum * sizeof(*sections),
                    (long)ehdr.e_shoff)) {
        snprintf(reason, reason_size, "cannot read guest %s section table",
                 library_label);
        goto out;
    }

    host = dlopen(host_soname, RTLD_LAZY | RTLD_LOCAL);
    if (!host) {
        snprintf(reason, reason_size, "cannot load host %s: %s",
                 library_label, dlerror());
        goto out;
    }
    for (size_t i = 0; i < required_host_symbol_count; i++) {
        if (!dlsym(host, required_host_symbols[i])) {
            snprintf(reason, reason_size,
                     "host %s is missing wrapper prerequisite '%s'",
                     library_label, required_host_symbols[i]);
            goto out;
        }
    }

    for (size_t section_index = 0; section_index < ehdr.e_shnum;
         section_index++) {
        const Elf64_Shdr *dynsym = &sections[section_index];
        const Elf64_Shdr *strtab;
        Elf64_Sym *symbols = NULL;
        char *strings = NULL;
        size_t symbol_count;

        if (dynsym->sh_type != SHT_DYNSYM) {
            continue;
        }
        found_dynsym = true;
        if (dynsym->sh_link >= ehdr.e_shnum ||
            dynsym->sh_entsize != sizeof(Elf64_Sym) ||
            dynsym->sh_size % sizeof(Elf64_Sym) ||
            !range_is_valid(dynsym->sh_offset, dynsym->sh_size,
                            file_size)) {
            snprintf(reason, reason_size,
                     "guest %s has an invalid dynamic symbol table",
                     library_label);
            goto out;
        }
        strtab = &sections[dynsym->sh_link];
        if (strtab->sh_type != SHT_STRTAB || !strtab->sh_size ||
            !range_is_valid(strtab->sh_offset, strtab->sh_size,
                            file_size)) {
            snprintf(reason, reason_size,
                     "guest %s has an invalid dynamic string table",
                     library_label);
            goto out;
        }
        symbols = malloc(dynsym->sh_size);
        strings = malloc(strtab->sh_size);
        if (!symbols || !strings ||
            !read_exact(file, symbols, dynsym->sh_size,
                        (long)dynsym->sh_offset) ||
            !read_exact(file, strings, strtab->sh_size,
                        (long)strtab->sh_offset)) {
            free(symbols);
            free(strings);
            snprintf(reason, reason_size,
                     "cannot read guest %s dynamic symbols", library_label);
            goto out;
        }

        symbol_count = dynsym->sh_size / sizeof(Elf64_Sym);
        for (size_t i = 0; i < symbol_count; i++) {
            const Elf64_Sym *symbol = &symbols[i];
            const char *name;
            unsigned type = ELF64_ST_TYPE(symbol->st_info);
            unsigned binding = ELF64_ST_BIND(symbol->st_info);

            if (symbol->st_shndx == SHN_UNDEF ||
                (type != STT_FUNC && type != STT_GNU_IFUNC) ||
                (binding != STB_GLOBAL && binding != STB_WEAK) ||
                symbol->st_name >= strtab->sh_size) {
                continue;
            }
            name = strings + symbol->st_name;
            if (!memchr(name, '\0', strtab->sh_size - symbol->st_name)) {
                free(symbols);
                free(strings);
                snprintf(reason, reason_size,
                         "guest %s has an unterminated dynamic symbol",
                         library_label);
                goto out;
            }
            if (!symbol_filter(name)) {
                continue;
            }
            found_library_symbol = true;
            if (!wrapped_symbol_is_supported(name, supported_symbols,
                                             supported_symbol_count)) {
                snprintf(reason, reason_size,
                         "guest symbol '%s' has no safe wrapper", name);
                free(symbols);
                free(strings);
                goto out;
            }
            if (!dlsym(host, name)) {
                snprintf(reason, reason_size,
                         "host %s is missing guest symbol '%s'",
                         library_label, name);
                free(symbols);
                free(strings);
                goto out;
            }
        }
        free(symbols);
        free(strings);
    }

    if (!found_dynsym) {
        snprintf(reason, reason_size,
                 "guest %s has no inspectable dynamic symbol table",
                 library_label);
        goto out;
    }
    if (!found_library_symbol) {
        snprintf(reason, reason_size, "guest %s exports no wrapped functions",
                 library_label);
        goto out;
    }
    ok = true;

out:
    if (host) {
        dlclose(host);
    }
    free(sections);
    if (file) {
        fclose(file);
    }
    return ok;
}
