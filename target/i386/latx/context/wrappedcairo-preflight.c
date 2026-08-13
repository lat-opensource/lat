/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wrappedcairo-preflight.h"

#define GO(name, signature) #name,
#define GOM(name, signature) #name,
#define GOW(name, signature) #name,
#define GOWM(name, signature) #name,
#define GO2(name, signature, alias) #name,
#define GOS(name, signature) #name,
#define DATA(name, size)
#define DATAV(name, size)
#define DATAB(name, size)
#define DATAM(name, size)
static const char *const cairo_supported_symbols[] = {
#include "wrappedcairo_private.h"
};
#undef GO
#undef GOM
#undef GOW
#undef GOWM
#undef GO2
#undef GOS
#undef DATA
#undef DATAV
#undef DATAB
#undef DATAM

static bool cairo_symbol_is_supported(const char *name)
{
    size_t count = sizeof(cairo_supported_symbols) /
                   sizeof(cairo_supported_symbols[0]);

    for (size_t i = 0; i < count; i++) {
        if (!strcmp(name, cairo_supported_symbols[i])) {
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

static bool reject(char *reason, size_t reason_size, const char *format,
                   const char *detail)
{
    snprintf(reason, reason_size, format, detail ? detail : "");
    return false;
}

bool latx_cairo_preflight_guest(const char *guest_path,
                                const char *host_soname,
                                char *reason, size_t reason_size)
{
    Elf64_Ehdr ehdr;
    Elf64_Shdr *sections = NULL;
    FILE *file = NULL;
    void *host = NULL;
    bool found_dynsym = false;
    bool found_cairo_symbol = false;
    bool ok = false;
    long file_size;

    if (!reason || !reason_size) {
        return false;
    }
    reason[0] = '\0';
    if (!guest_path || !host_soname) {
        return reject(reason, reason_size,
                      "guest or host Cairo path is unavailable%s", NULL);
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
        reject(reason, reason_size,
               "cannot determine guest Cairo ELF size%s", NULL);
        goto out;
    }
    if (fread(&ehdr, 1, sizeof(ehdr), file) != sizeof(ehdr) ||
        memcmp(ehdr.e_ident, ELFMAG, SELFMAG) ||
        ehdr.e_ident[EI_CLASS] != ELFCLASS64 ||
        ehdr.e_ident[EI_DATA] != ELFDATA2LSB ||
        ehdr.e_type != ET_DYN ||
        ehdr.e_machine != EM_X86_64 ||
        ehdr.e_shentsize != sizeof(Elf64_Shdr) ||
        !ehdr.e_shoff || !ehdr.e_shnum ||
        !range_is_valid(ehdr.e_shoff,
                        (uint64_t)ehdr.e_shnum * sizeof(Elf64_Shdr),
                        file_size)) {
        reject(reason, reason_size,
               "guest Cairo is not a supported ELF64 little-endian shared object%s",
               NULL);
        goto out;
    }
    sections = calloc(ehdr.e_shnum, sizeof(*sections));
    if (!sections ||
        !read_exact(file, sections, ehdr.e_shnum * sizeof(*sections),
                    (long)ehdr.e_shoff)) {
        reject(reason, reason_size,
               "cannot read guest Cairo section table%s", NULL);
        goto out;
    }

    host = dlopen(host_soname, RTLD_LAZY | RTLD_LOCAL);
    if (!host) {
        reject(reason, reason_size, "cannot load host Cairo: %s", dlerror());
        goto out;
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
            reject(reason, reason_size,
                   "guest Cairo has an invalid dynamic symbol table%s", NULL);
            goto out;
        }
        strtab = &sections[dynsym->sh_link];
        if (strtab->sh_type != SHT_STRTAB || !strtab->sh_size ||
            !range_is_valid(strtab->sh_offset, strtab->sh_size,
                            file_size)) {
            reject(reason, reason_size,
                   "guest Cairo has an invalid dynamic string table%s",
                   NULL);
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
            reject(reason, reason_size,
                   "cannot read guest Cairo dynamic symbols%s", NULL);
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
                reject(reason, reason_size,
                       "guest Cairo has an unterminated dynamic symbol%s",
                       NULL);
                goto out;
            }
            if (strncmp(name, "cairo_", 6)) {
                continue;
            }
            found_cairo_symbol = true;
            if (!cairo_symbol_is_supported(name)) {
                snprintf(reason, reason_size,
                         "guest symbol '%s' has no safe wrapper", name);
                free(symbols);
                free(strings);
                goto out;
            }
            if (!dlsym(host, name)) {
                snprintf(reason, reason_size,
                         "host Cairo is missing guest symbol '%s'", name);
                free(symbols);
                free(strings);
                goto out;
            }
        }
        free(symbols);
        free(strings);
    }

    if (!found_dynsym) {
        reject(reason, reason_size,
               "guest Cairo has no inspectable dynamic symbol table%s", NULL);
        goto out;
    }
    if (!found_cairo_symbol) {
        reject(reason, reason_size,
               "guest Cairo exports no Cairo functions%s", NULL);
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
