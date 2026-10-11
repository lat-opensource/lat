#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "elf-validate.h"

#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef ELF64_ST_VISIBILITY
#define ELF64_ST_VISIBILITY(value) ((value) & 0x3)
#endif
#ifndef STV_INTERNAL
#define STV_INTERNAL 1
#endif
#ifndef STV_HIDDEN
#define STV_HIDDEN 2
#endif
#ifndef R_LARCH_RELATIVE
#define R_LARCH_RELATIVE 3
#endif
#ifndef R_LARCH_JUMP_SLOT
#define R_LARCH_JUMP_SLOT 5
#endif

static int reject(char *error, size_t error_size, const char *format, ...)
{
    if (error && error_size) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(error, error_size, format, arguments);
        va_end(arguments);
    }
    errno = ENOEXEC;
    return -1;
}

static int runtime_import_allowed(const char *name)
{
    static const char *const imports[] = {
        LAT_AOT_V2_RUNTIME_ABI_SYMBOL,
        LAT_AOT_V2_RUNTIME_SYSCALL_SYMBOL,
        "lat_aot_runtime_epilogue_ret_id_1",
        "lat_aot_runtime_epilogue_ret_id_0",
        "lat_aot_runtime_jirl_epilogue_ret_id_1",
        "lat_aot_runtime_jirl_epilogue_ret_id_0",
        "lat_aot_runtime_epilogue_ret_0",
        "lat_aot_runtime_update_mxcsr_status",
        "lat_aot_runtime_fxsave",
        "lat_aot_runtime_fxrstor",
        "lat_aot_runtime_fpregs_x80_to_64",
        "lat_aot_runtime_fpregs_64_to_x80",
        "lat_aot_runtime_update_fp_status",
        "lat_aot_runtime_cpuid",
        "lat_aot_runtime_raise_illop",
        "lat_aot_runtime_raise_gpf",
        "lat_aot_runtime_pcmpistri_xmm",
        "lat_aot_runtime_pcmpistrm_xmm",
        "lat_aot_runtime_eflagtf",
        "lat_aot_runtime_log2",
        "lat_aot_runtime_pow",
        "lat_aot_runtime_sin",
        "lat_aot_runtime_cos",
        "lat_aot_runtime_atan2",
        "lat_aot_runtime_logb",
        "lat_aot_runtime_sincos",
        "lat_aot_runtime_fpatan",
        "lat_aot_runtime_fptan",
        "lat_aot_runtime_fprem",
        "lat_aot_runtime_fprem1",
        "lat_aot_runtime_frndint",
        "lat_aot_runtime_f2xm1",
        "lat_aot_runtime_fxtract",
        "lat_aot_runtime_fyl2x",
        "lat_aot_runtime_fyl2xp1",
        "lat_aot_runtime_fsincos",
        "lat_aot_runtime_fsin",
        "lat_aot_runtime_fcos",
        "lat_aot_runtime_fbld_st0",
        "lat_aot_runtime_fbst_st0",
        "lat_aot_runtime_aesimc_xmm",
        "lat_aot_runtime_aeskeygenassist_xmm",
        "lat_aot_runtime_aesdec_xmm",
        "lat_aot_runtime_aesdeclast_xmm",
        "lat_aot_runtime_aesenc_xmm",
        "lat_aot_runtime_aesenclast_xmm",
        "lat_aot_runtime_sha1nexte",
        "lat_aot_runtime_sha1msg1",
        "lat_aot_runtime_sha1msg2",
        "lat_aot_runtime_sha256msg1",
        "lat_aot_runtime_sha256msg2",
        "lat_aot_runtime_sha1rnds4_f0",
        "lat_aot_runtime_sha1rnds4_f1",
        "lat_aot_runtime_sha1rnds4_f2",
        "lat_aot_runtime_sha1rnds4_f3",
        "lat_aot_runtime_sha256rnds2_xmm0",
        "lat_aot_runtime_raise_int",
        "lat_aot_runtime_raise_trapop",
        "lat_aot_runtime_raise_into",
        "lat_aot_runtime_raise_bound",
        "lat_aot_runtime_xgetbv",
        "lat_aot_runtime_xsave",
        "lat_aot_runtime_xsaveopt",
        "lat_aot_runtime_xrstor",
        "lat_aot_runtime_xsetbv",
        "lat_aot_runtime_kzt_get_alternate",
    };
    for (size_t i = 0; i < sizeof(imports) / sizeof(imports[0]); i++) {
        if (!strcmp(name, imports[i])) {
            return 1;
        }
    }
    return 0;
}

static int range_valid(size_t file_size, uint64_t offset, uint64_t size)
{
    return offset <= file_size && size <= file_size - offset;
}

static int align4(uint64_t value, uint64_t *aligned)
{
    if (value > UINT64_MAX - 3) {
        return -1;
    }
    *aligned = (value + 3) & ~(uint64_t)3;
    return 0;
}

static const void *vaddr_to_file(const unsigned char *file, size_t file_size,
                                 const Elf64_Phdr *phdrs, size_t phnum,
                                 uint64_t address, uint64_t size)
{
    for (size_t i = 0; i < phnum; i++) {
        const Elf64_Phdr *phdr = &phdrs[i];
        if (phdr->p_type != PT_LOAD || address < phdr->p_vaddr) {
            continue;
        }
        uint64_t delta = address - phdr->p_vaddr;
        if (delta <= phdr->p_filesz && size <= phdr->p_filesz - delta &&
            phdr->p_offset <= file_size && delta <= file_size - phdr->p_offset &&
            range_valid(file_size, phdr->p_offset + delta, size)) {
            return file + phdr->p_offset + delta;
        }
    }
    return NULL;
}

static int validate_note(const unsigned char *file, size_t file_size,
                         const Elf64_Phdr *phdrs, size_t phnum,
                         const LatAotExpectedV2 *expected,
                         LatAotNoteV2 *result, char *error,
                         size_t error_size)
{
    int found = 0;
    LatAotNoteV2 selected = {0};

    for (size_t i = 0; i < phnum; i++) {
        const Elf64_Phdr *phdr = &phdrs[i];
        if (phdr->p_type != PT_NOTE) {
            continue;
        }
        if (phdr->p_offset % _Alignof(Elf64_Nhdr) ||
            !range_valid(file_size, phdr->p_offset, phdr->p_filesz)) {
            return reject(error, error_size, "AOT note segment is truncated");
        }
        uint64_t cursor = phdr->p_offset;
        uint64_t end = cursor + phdr->p_filesz;
        while (cursor < end) {
            if (end - cursor < sizeof(Elf64_Nhdr)) {
                return reject(error, error_size, "AOT note header is truncated");
            }
            const Elf64_Nhdr *header = (const void *)(file + cursor);
            cursor += sizeof(*header);
            uint64_t name_size;
            uint64_t desc_size;
            if (align4(header->n_namesz, &name_size) ||
                align4(header->n_descsz, &desc_size) ||
                name_size > end - cursor ||
                desc_size > end - cursor - name_size) {
                return reject(error, error_size, "AOT note payload is truncated");
            }
            const char *name = (const void *)(file + cursor);
            const unsigned char *description = file + cursor + name_size;
            if (header->n_type == LAT_AOT_V2_NOTE_TYPE &&
                header->n_namesz == sizeof(LAT_AOT_V2_NOTE_NAME) &&
                !memcmp(name, LAT_AOT_V2_NOTE_NAME,
                        sizeof(LAT_AOT_V2_NOTE_NAME))) {
                if (found || header->n_descsz != sizeof(selected)) {
                    return reject(error, error_size,
                                  "AOT v2 note count or size is invalid");
                }
                memcpy(&selected, description, sizeof(selected));
                found = 1;
            }
            cursor += name_size + desc_size;
        }
    }
    if (!found) {
        return reject(error, error_size, "AOT v2 note is missing");
    }
    if (!lat_aot_v2_magic_valid(selected.magic) ||
        selected.abi_version != LAT_AOT_V2_ABI_VERSION ||
        selected.struct_size != sizeof(selected)) {
        return reject(error, error_size, "AOT v2 note ABI is invalid");
    }
    if ((selected.module_flags & LAT_AOT_MODULE_READONLY_TEXT) == 0) {
        return reject(error, error_size, "AOT module does not require read-only text");
    }
    if ((selected.module_flags & (LAT_AOT_MODULE_SYNTHETIC_FIXTURE |
                                  LAT_AOT_MODULE_M1_TEST_ONLY)) == 0 &&
        (selected.module_flags & LAT_AOT_MODULE_PRECISE_PC_MAP) == 0) {
        return reject(error, error_size, "AOT module lacks a precise PC map");
    }
    if ((selected.required_features & LAT_AOT_V2_REQUIRED_BASE_FEATURES) !=
        LAT_AOT_V2_REQUIRED_BASE_FEATURES) {
        return reject(error, error_size, "AOT module lacks mandatory LBT or LSX");
    }
    if (expected) {
        if (memcmp(selected.source_sha256, expected->source_sha256,
                   sizeof(selected.source_sha256))) {
            return reject(error, error_size, "AOT source SHA-256 mismatch");
        }
        if (memcmp(selected.codegen_id, expected->codegen_id,
                   sizeof(selected.codegen_id))) {
            return reject(error, error_size, "AOT codegen ID mismatch");
        }
        if (selected.required_features & ~expected->available_features) {
            return reject(error, error_size, "AOT CPU features are unavailable");
        }
    }
    if (result) {
        *result = selected;
    }
    return 0;
}

static int validate_dynamic(const unsigned char *file, size_t file_size,
                            const Elf64_Phdr *phdrs, size_t phnum,
                            char *error, size_t error_size)
{
    const Elf64_Phdr *dynamic_phdr = NULL;
    uint64_t string_address = 0;
    uint64_t string_size = 0;
    uint64_t needed_offset = 0;
    size_t needed_count = 0;

    for (size_t i = 0; i < phnum; i++) {
        if (phdrs[i].p_type == PT_DYNAMIC) {
            if (dynamic_phdr) {
                return reject(error, error_size,
                              "multiple dynamic segments are not allowed");
            }
            dynamic_phdr = &phdrs[i];
        }
    }
    if (!dynamic_phdr ||
        dynamic_phdr->p_offset % _Alignof(Elf64_Dyn) ||
        !range_valid(file_size, dynamic_phdr->p_offset,
                     dynamic_phdr->p_filesz) ||
        dynamic_phdr->p_filesz % sizeof(Elf64_Dyn)) {
        return reject(error, error_size, "AOT dynamic segment is invalid");
    }
    const Elf64_Dyn *entries =
        (const void *)(file + dynamic_phdr->p_offset);
    size_t count = dynamic_phdr->p_filesz / sizeof(*entries);
    int terminated = 0;
    for (size_t i = 0; i < count; i++) {
        const Elf64_Dyn *entry = &entries[i];
        if (entry->d_tag == DT_NULL) {
            terminated = 1;
            break;
        }
        switch (entry->d_tag) {
        case DT_STRTAB:
            string_address = entry->d_un.d_ptr;
            break;
        case DT_STRSZ:
            string_size = entry->d_un.d_val;
            break;
        case DT_NEEDED:
            needed_offset = entry->d_un.d_val;
            needed_count++;
            break;
        case DT_INIT:
        case DT_FINI:
        case DT_INIT_ARRAY:
        case DT_FINI_ARRAY:
#ifdef DT_PREINIT_ARRAY
        case DT_PREINIT_ARRAY:
#endif
        case DT_TEXTREL:
            return reject(error, error_size,
                          "constructors, destructors, or text relocations are not allowed");
#ifdef DT_RPATH
        case DT_RPATH:
#endif
#ifdef DT_RUNPATH
        case DT_RUNPATH:
#endif
            return reject(error, error_size,
                          "AOT module search paths are not allowed");
        case DT_INIT_ARRAYSZ:
        case DT_FINI_ARRAYSZ:
#ifdef DT_PREINIT_ARRAYSZ
        case DT_PREINIT_ARRAYSZ:
#endif
            if (entry->d_un.d_val) {
                return reject(error, error_size,
                              "constructor or destructor arrays are not allowed");
            }
            break;
        default:
            break;
        }
    }
    if (!terminated || needed_count != 1 || !string_address || !string_size ||
        needed_offset >= string_size) {
        return reject(error, error_size, "AOT runtime dependency is invalid");
    }
    const char *strings = vaddr_to_file(file, file_size, phdrs, phnum,
                                        string_address, string_size);
    if (!strings || !memchr(strings + needed_offset, 0,
                            string_size - needed_offset) ||
        strcmp(strings + needed_offset, LAT_AOT_V2_RUNTIME_SONAME)) {
        return reject(error, error_size,
                      "AOT module has an unexpected runtime dependency");
    }
    return 0;
}

static int validate_dynamic_symbols(const unsigned char *file,
                                    size_t file_size,
                                    const Elf64_Ehdr *header,
                                    char *error, size_t error_size)
{
    if (!header->e_shoff || header->e_shentsize != sizeof(Elf64_Shdr) ||
        !header->e_shnum ||
        header->e_shoff % _Alignof(Elf64_Shdr) ||
        !range_valid(file_size, header->e_shoff,
                     (uint64_t)header->e_shnum * sizeof(Elf64_Shdr))) {
        return reject(error, error_size, "AOT section table is missing or invalid");
    }
    const Elf64_Shdr *sections = (const void *)(file + header->e_shoff);
    const Elf64_Shdr *symbols = NULL;
    for (size_t i = 0; i < header->e_shnum; i++) {
        if (sections[i].sh_type == SHT_DYNSYM) {
            if (symbols) {
                return reject(error, error_size,
                              "multiple dynamic symbol tables are not allowed");
            }
            symbols = &sections[i];
        }
    }
    if (!symbols || symbols->sh_link >= header->e_shnum ||
        symbols->sh_entsize != sizeof(Elf64_Sym) ||
        symbols->sh_size % sizeof(Elf64_Sym) ||
        symbols->sh_offset % _Alignof(Elf64_Sym) ||
        !range_valid(file_size, symbols->sh_offset, symbols->sh_size)) {
        return reject(error, error_size, "AOT dynamic symbol table is invalid");
    }
    const Elf64_Shdr *string_section = &sections[symbols->sh_link];
    if (string_section->sh_type != SHT_STRTAB ||
        !range_valid(file_size, string_section->sh_offset,
                     string_section->sh_size)) {
        return reject(error, error_size, "AOT dynamic string table is invalid");
    }
    const char *strings = (const void *)(file + string_section->sh_offset);
    const Elf64_Sym *table = (const void *)(file + symbols->sh_offset);
    size_t count = symbols->sh_size / sizeof(*table);
    int descriptor_exports = 0;
    int runtime_abi_imports = 0;
    for (size_t i = 1; i < count; i++) {
        unsigned bind = ELF64_ST_BIND(table[i].st_info);
        unsigned visibility = ELF64_ST_VISIBILITY(table[i].st_other);
        if ((bind != STB_GLOBAL && bind != STB_WEAK) ||
            visibility == STV_HIDDEN || visibility == STV_INTERNAL) {
            continue;
        }
        if (table[i].st_name >= string_section->sh_size ||
            !memchr(strings + table[i].st_name, 0,
                    string_section->sh_size - table[i].st_name)) {
            return reject(error, error_size, "AOT dynamic symbol name is invalid");
        }
        const char *name = strings + table[i].st_name;
        if (table[i].st_shndx == SHN_UNDEF &&
            !strcmp(name, LAT_AOT_V2_RUNTIME_ABI_SYMBOL)) {
            runtime_abi_imports++;
        } else if (table[i].st_shndx == SHN_UNDEF &&
                   runtime_import_allowed(name)) {
            continue;
        } else if (table[i].st_shndx != SHN_UNDEF &&
                   !strcmp(name, LAT_AOT_V2_DESCRIPTOR_SYMBOL)) {
            descriptor_exports++;
        } else if (table[i].st_shndx == SHN_ABS &&
                   !strcmp(name, LAT_AOT_V2_MODULE_VERSION)) {
            continue;
        } else if (*name) {
            return reject(error, error_size,
                          "unexpected AOT dynamic symbol: %s", name);
        }
    }
    if (descriptor_exports != 1 || runtime_abi_imports != 1) {
        return reject(error, error_size,
                      "AOT descriptor export or runtime import is missing");
    }
    return 0;
}

static int relocation_target_is_writable(const Elf64_Phdr *phdrs,
                                         size_t phnum, uint64_t address)
{
    for (size_t i = 0; i < phnum; i++) {
        const Elf64_Phdr *phdr = &phdrs[i];
        if (phdr->p_type == PT_LOAD && address >= phdr->p_vaddr &&
            address - phdr->p_vaddr <= phdr->p_memsz &&
            sizeof(uint64_t) <= phdr->p_memsz - (address - phdr->p_vaddr)) {
            return (phdr->p_flags & (PF_W | PF_X)) == PF_W;
        }
    }
    return 0;
}

static int address_is_in_load(const Elf64_Phdr *phdrs, size_t phnum,
                              uint64_t address)
{
    for (size_t i = 0; i < phnum; i++) {
        const Elf64_Phdr *phdr = &phdrs[i];
        /* A relative relocation may encode a valid one-past module pointer. */
        if (phdr->p_type == PT_LOAD && address >= phdr->p_vaddr &&
            address - phdr->p_vaddr <= phdr->p_memsz) {
            return 1;
        }
    }
    return 0;
}

static int validate_relocations(const unsigned char *file, size_t file_size,
                                const Elf64_Ehdr *header,
                                const Elf64_Phdr *phdrs, size_t phnum,
                                char *error, size_t error_size)
{
    const Elf64_Shdr *sections = (const void *)(file + header->e_shoff);
    for (size_t i = 0; i < header->e_shnum; i++) {
        const Elf64_Shdr *section = &sections[i];
        if (section->sh_type == SHT_REL) {
            return reject(error, error_size,
                          "AOT module contains unsupported REL relocations");
        }
        if (section->sh_type != SHT_RELA) {
            continue;
        }
        if (section->sh_entsize != sizeof(Elf64_Rela) ||
            section->sh_size % sizeof(Elf64_Rela) ||
            section->sh_offset % _Alignof(Elf64_Rela) ||
            !range_valid(file_size, section->sh_offset, section->sh_size) ||
            section->sh_link >= header->e_shnum) {
            return reject(error, error_size,
                          "AOT RELA section is invalid");
        }
        const Elf64_Shdr *symbol_section = &sections[section->sh_link];
        if (symbol_section->sh_type != SHT_DYNSYM ||
            symbol_section->sh_entsize != sizeof(Elf64_Sym) ||
            symbol_section->sh_size % sizeof(Elf64_Sym) ||
            symbol_section->sh_offset % _Alignof(Elf64_Sym) ||
            !range_valid(file_size, symbol_section->sh_offset,
                         symbol_section->sh_size) ||
            symbol_section->sh_link >= header->e_shnum) {
            return reject(error, error_size,
                          "AOT relocation symbol table is invalid");
        }
        const Elf64_Shdr *string_section = &sections[symbol_section->sh_link];
        if (string_section->sh_type != SHT_STRTAB ||
            !range_valid(file_size, string_section->sh_offset,
                         string_section->sh_size)) {
            return reject(error, error_size,
                          "AOT relocation string table is invalid");
        }
        const Elf64_Sym *symbols =
            (const void *)(file + symbol_section->sh_offset);
        size_t symbol_count = symbol_section->sh_size / sizeof(*symbols);
        const char *strings = (const void *)(file + string_section->sh_offset);
        const Elf64_Rela *relocations =
            (const void *)(file + section->sh_offset);
        size_t count = section->sh_size / sizeof(*relocations);
        for (size_t j = 0; j < count; j++) {
            uint32_t type = (uint32_t)relocations[j].r_info;
            uint32_t symbol_index = relocations[j].r_info >> 32;
            if (!relocation_target_is_writable(phdrs, phnum,
                                               relocations[j].r_offset)) {
                return reject(error, error_size,
                              "AOT relocation targets a non-writable or executable segment");
            }
            if (type == R_LARCH_RELATIVE) {
                if (symbol_index || relocations[j].r_addend < 0 ||
                    !address_is_in_load(phdrs, phnum,
                                        relocations[j].r_addend)) {
                    return reject(error, error_size,
                                  "AOT relative relocation is not module-local");
                }
                continue;
            }
            if (type != R_LARCH_JUMP_SLOT && type != R_LARCH_64) {
                return reject(error, error_size,
                              "AOT relocation type %u is not allowed", type);
            }
            if (!symbol_index || symbol_index >= symbol_count ||
                symbols[symbol_index].st_name >= string_section->sh_size ||
                !memchr(strings + symbols[symbol_index].st_name, 0,
                        string_section->sh_size -
                        symbols[symbol_index].st_name) ||
                !runtime_import_allowed(
                    strings + symbols[symbol_index].st_name)) {
                return reject(error, error_size,
                              "AOT relocation references an unexpected symbol");
            }
        }
    }
    return 0;
}

int lat_aot_v2_elf_validate_memory(const void *data, size_t file_size,
                                   const LatAotExpectedV2 *expected,
                                   LatAotNoteV2 *note, char *error,
                                   size_t error_size)
{
    const unsigned char *file = data;
    int result = -1;

    if (!file || file_size < sizeof(Elf64_Ehdr) ||
        (uintptr_t)file % _Alignof(Elf64_Ehdr)) {
        return reject(error, error_size, "AOT ELF memory is missing or unaligned");
    }
    const Elf64_Ehdr *header = (const void *)file;
    if (memcmp(header->e_ident, ELFMAG, SELFMAG) ||
        header->e_ident[EI_CLASS] != ELFCLASS64 ||
        header->e_ident[EI_DATA] != ELFDATA2LSB ||
        header->e_ident[EI_VERSION] != EV_CURRENT ||
        header->e_type != ET_DYN || header->e_machine != EM_LOONGARCH ||
        header->e_version != EV_CURRENT ||
        header->e_ehsize != sizeof(*header) ||
        header->e_phentsize != sizeof(Elf64_Phdr) || !header->e_phnum ||
        header->e_phoff % _Alignof(Elf64_Phdr) ||
        !range_valid(file_size, header->e_phoff,
                     (uint64_t)header->e_phnum * sizeof(Elf64_Phdr))) {
        return reject(error, error_size,
                      "file is not a supported LoongArch AOT ELF");
    }
    const Elf64_Phdr *phdrs = (const void *)(file + header->e_phoff);
    for (size_t i = 0; i < header->e_phnum; i++) {
        const Elf64_Phdr *phdr = &phdrs[i];
        if (phdr->p_type == PT_INTERP) {
            return reject(error, error_size,
                          "AOT module must not have PT_INTERP");
        }
        if (phdr->p_type == PT_LOAD) {
            if ((phdr->p_flags & (PF_W | PF_X)) == (PF_W | PF_X)) {
                return reject(error, error_size,
                              "AOT module contains a W+X segment");
            }
            if (phdr->p_filesz > phdr->p_memsz ||
                !range_valid(file_size, phdr->p_offset, phdr->p_filesz)) {
                return reject(error, error_size,
                              "AOT load segment is invalid");
            }
        }
    }
    if (validate_note(file, file_size, phdrs, header->e_phnum, expected,
                      note, error, error_size) ||
        validate_dynamic(file, file_size, phdrs, header->e_phnum,
                         error, error_size) ||
        validate_dynamic_symbols(file, file_size, header,
                                 error, error_size) ||
        validate_relocations(file, file_size, header, phdrs,
                             header->e_phnum, error, error_size)) {
        return -1;
    }
    result = 0;
    return result;
}

int lat_aot_v2_elf_validate_fd(int fd, const LatAotExpectedV2 *expected,
                               LatAotNoteV2 *note, char *error,
                               size_t error_size)
{
    struct stat status;
    unsigned char *file;

    if (fd < 0 || fstat(fd, &status) ||
        status.st_size < (off_t)sizeof(Elf64_Ehdr)) {
        return reject(error, error_size, "cannot stat AOT ELF");
    }
    if (!S_ISREG(status.st_mode) || (status.st_mode & (S_IWGRP | S_IWOTH))) {
        return reject(error, error_size, "AOT ELF permissions are unsafe");
    }
    size_t file_size = (size_t)status.st_size;
    if ((off_t)file_size != status.st_size) {
        return reject(error, error_size, "AOT ELF is too large");
    }
    file = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file == MAP_FAILED) {
        return reject(error, error_size, "cannot map AOT ELF");
    }
    int result = lat_aot_v2_elf_validate_memory(file, file_size, expected,
                                                note, error, error_size);
    munmap(file, file_size);
    return result;
}
