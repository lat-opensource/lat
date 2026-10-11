#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "function_symbols.h"

#include "cfg_decoder.h"
#include "common.h"

/*
 * Function discovery from symbols and .eh_frame.
 *
 * Symbol tables are precise when present, but stripped system binaries often
 * retain only unwind metadata. The .eh_frame parser extracts only the CIE/FDE
 * fields needed to recover function start and size.
 */

#include <elf.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    /* File-relative offset of the CIE record and its FDE pointer encoding. */
    uint64_t cie_off;
    uint8_t fde_encoding;
} CieInfo;

typedef struct {
    CieInfo *v;
    size_t n;
    size_t cap;
} CieVec;

static void func_push(FuncVec *vec, FuncSym fn)
{
    if (vec->n == vec->cap) {
        size_t cap = vec->cap ? vec->cap * 2 : 512;
        FuncSym *v = realloc(vec->v, cap * sizeof(*v));
        if (!v) {
            die_errno("realloc funcs");
        }
        vec->v = v;
        vec->cap = cap;
    }
    vec->v[vec->n++] = fn;
}

static void cie_push(CieVec *vec, CieInfo cie)
{
    if (vec->n == vec->cap) {
        size_t cap = vec->cap ? vec->cap * 2 : 16;
        CieInfo *v = realloc(vec->v, cap * sizeof(*v));
        if (!v) {
            die_errno("realloc cies");
        }
        vec->v = v;
        vec->cap = cap;
    }
    vec->v[vec->n++] = cie;
}

static int cmp_func(const void *a, const void *b)
{
    const FuncSym *fa = a;
    const FuncSym *fb = b;
    if (fa->addr != fb->addr) {
        return (fa->addr > fb->addr) - (fa->addr < fb->addr);
    }
    return strcmp(fa->name, fb->name);
}

/* DW_EH_PE constants used by .eh_frame pointer encodings. */
enum {
    DW_EH_PE_absptr = 0x00,
    DW_EH_PE_uleb128 = 0x01,
    DW_EH_PE_udata2 = 0x02,
    DW_EH_PE_udata4 = 0x03,
    DW_EH_PE_udata8 = 0x04,
    DW_EH_PE_sleb128 = 0x09,
    DW_EH_PE_sdata2 = 0x0a,
    DW_EH_PE_sdata4 = 0x0b,
    DW_EH_PE_sdata8 = 0x0c,
    DW_EH_PE_pcrel = 0x10,
    DW_EH_PE_omit = 0xff,
};

static bool read_eh_value(const uint8_t *buf, size_t size, size_t *off,
                          uint64_t field_addr, uint8_t enc,
                          bool apply_relative, uint64_t *value_out)
{
    if (enc == DW_EH_PE_omit) {
        return false;
    }

    uint8_t fmt = enc & 0x0f;
    uint64_t value = 0;
    size_t p = *off;

    switch (fmt) {
    case DW_EH_PE_absptr:
        if (p + 8 > size) {
            return false;
        }
        value = rd64(buf + p);
        p += 8;
        break;
    case DW_EH_PE_uleb128:
        value = read_uleb(buf, size, &p);
        break;
    case DW_EH_PE_udata2:
        if (p + 2 > size) {
            return false;
        }
        value = (uint64_t)buf[p] | ((uint64_t)buf[p + 1] << 8);
        p += 2;
        break;
    case DW_EH_PE_udata4:
        if (p + 4 > size) {
            return false;
        }
        value = rd32(buf + p);
        p += 4;
        break;
    case DW_EH_PE_udata8:
        if (p + 8 > size) {
            return false;
        }
        value = rd64(buf + p);
        p += 8;
        break;
    case DW_EH_PE_sleb128:
        value = (uint64_t)read_sleb(buf, size, &p);
        break;
    case DW_EH_PE_sdata2:
        if (p + 2 > size) {
            return false;
        }
        value = (uint64_t)(int16_t)((uint16_t)buf[p] |
                                    ((uint16_t)buf[p + 1] << 8));
        p += 2;
        break;
    case DW_EH_PE_sdata4:
        if (p + 4 > size) {
            return false;
        }
        value = (uint64_t)rd_i32(buf + p);
        p += 4;
        break;
    case DW_EH_PE_sdata8:
        if (p + 8 > size) {
            return false;
        }
        value = rd64(buf + p);
        p += 8;
        break;
    default:
        return false;
    }

    if (apply_relative && (enc & 0x70) == DW_EH_PE_pcrel) {
        value += field_addr;
    }

    *off = p;
    *value_out = value;
    return true;
}

static const CieInfo *find_cie(const CieVec *cies, uint64_t cie_off)
{
    for (size_t i = 0; i < cies->n; i++) {
        if (cies->v[i].cie_off == cie_off) {
            return &cies->v[i];
        }
    }
    return NULL;
}

/*
 * Parse the CIE augmentation string only far enough to learn the FDE pointer
 * encoding ('R'). Other augmentation fields are skipped because call-frame
 * instructions are not needed for CFG function range recovery.
 */
static void parse_cie_record(const uint8_t *buf, size_t size, uint64_t rec_off,
                             uint64_t content_off, uint64_t end,
                             CieVec *cies)
{
    size_t p = (size_t)content_off + 4;
    if (p >= end) {
        return;
    }

    uint8_t version = buf[p++];
    const char *aug = (const char *)buf + p;
    size_t aug_len = strnlen(aug, (size_t)end - p);
    if (p + aug_len >= end) {
        return;
    }
    p += aug_len + 1;

    (void)read_uleb(buf, size, &p);
    (void)read_sleb(buf, size, &p);
    if (version == 1) {
        (void)read_uleb(buf, size, &p);
    } else {
        (void)read_uleb(buf, size, &p);
    }

    uint8_t fde_encoding = DW_EH_PE_absptr;
    if (aug[0] == 'z') {
        uint64_t aug_data_len = read_uleb(buf, size, &p);
        size_t aug_data_end = p + (size_t)aug_data_len;
        for (const char *c = aug + 1; *c && p < aug_data_end; c++) {
            if (*c == 'R') {
                fde_encoding = buf[p++];
            } else if (*c == 'L') {
                p++;
            } else if (*c == 'P') {
                uint8_t enc = buf[p++];
                uint64_t ignored;
                (void)read_eh_value(buf, size, &p, 0, enc, false, &ignored);
            }
        }
    }

    cie_push(cies, (CieInfo){
        .cie_off = rec_off,
        .fde_encoding = fde_encoding,
    });
}

static void synth_name(char *buf, size_t size, uint64_t addr)
{
    snprintf(buf, size, "sub_%016" PRIx64, addr);
}

static void dedup_functions(FuncVec *funcs)
{
    qsort(funcs->v, funcs->n, sizeof(funcs->v[0]), cmp_func);

    /* Prefer the largest range when multiple names start at the same address. */
    size_t out = 0;
    for (size_t i = 0; i < funcs->n; i++) {
        if (out > 0 && funcs->v[i].addr == funcs->v[out - 1].addr) {
            if (funcs->v[i].size > funcs->v[out - 1].size) {
                funcs->v[out - 1].size = funcs->v[i].size;
                funcs->v[out - 1].shndx = funcs->v[i].shndx;
            }
            free(funcs->v[i].name);
            continue;
        }
        funcs->v[out++] = funcs->v[i];
    }
    funcs->n = out;
}

static void infer_zero_function_sizes(const ElfFile *elf, FuncVec *funcs)
{
    for (size_t i = 0; i < funcs->n; i++) {
        FuncSym *fn = &funcs->v[i];
        if (fn->size || fn->shndx >= elf->eh->e_shnum) {
            continue;
        }
        const Elf64_Shdr *section = &elf->sh[fn->shndx];
        uint64_t end = section->sh_addr + section->sh_size;
        for (size_t j = i + 1; j < funcs->n; j++) {
            if (funcs->v[j].shndx == fn->shndx &&
                funcs->v[j].addr > fn->addr) {
                end = funcs->v[j].addr;
                break;
            }
        }
        if (fn->addr >= section->sh_addr && fn->addr < end) {
            fn->size = end - fn->addr;
        }
    }
}

static bool special_section_func(const ElfFile *elf, const char *section_name,
                                 const char *func_name, FuncSym *out)
{
    unsigned shndx = 0;
    const Elf64_Shdr *sec = elf_find_section_by_name(elf, section_name, &shndx);
    if (!sec || sec->sh_size == 0 || !range_ok(elf->size, sec->sh_offset,
                                               sec->sh_size)) {
        return false;
    }

    *out = (FuncSym){
        .name = xstrdup(func_name),
        .addr = sec->sh_addr,
        .size = sec->sh_size,
        .shndx = shndx,
    };
    return true;
}

static bool add_root_function(const ElfFile *elf, FuncVec *funcs,
                              uint64_t addr, const char *root_name)
{
    unsigned shndx = 0;
    if (!addr || !elf_find_exec_section_for_range(elf, addr, 1, &shndx)) {
        return false;
    }
    for (size_t i = 0; i < funcs->n; i++) {
        uint64_t end = funcs->v[i].addr + funcs->v[i].size;
        if (end >= funcs->v[i].addr && addr >= funcs->v[i].addr &&
            addr < end) {
            return false;
        }
    }

    char name[64];
    snprintf(name, sizeof(name), "%s_%016" PRIx64, root_name, addr);
    func_push(funcs, (FuncSym){
        .name = xstrdup(name),
        .addr = addr,
        .size = 0,
        .shndx = shndx,
    });
    return true;
}

static void load_array_relocations(const ElfFile *elf,
                                   const Elf64_Shdr *array,
                                   const char *array_name, FuncVec *funcs)
{
    uint64_t array_end = array->sh_addr + array->sh_size;
    if (array_end < array->sh_addr) {
        return;
    }

    for (unsigned si = 0; si < elf->eh->e_shnum; si++) {
        const Elf64_Shdr *relsec = &elf->sh[si];
        if (relsec->sh_type != SHT_RELA ||
            relsec->sh_entsize != sizeof(Elf64_Rela) ||
            !range_ok(elf->size, relsec->sh_offset, relsec->sh_size)) {
            continue;
        }

        const Elf64_Sym *symbols = NULL;
        size_t symbol_count = 0;
        if (relsec->sh_link < elf->eh->e_shnum) {
            const Elf64_Shdr *symsec = &elf->sh[relsec->sh_link];
            if ((symsec->sh_type == SHT_SYMTAB ||
                 symsec->sh_type == SHT_DYNSYM) &&
                symsec->sh_entsize == sizeof(Elf64_Sym) &&
                range_ok(elf->size, symsec->sh_offset, symsec->sh_size)) {
                symbols = (const void *)(elf->data + symsec->sh_offset);
                symbol_count = symsec->sh_size / sizeof(*symbols);
            }
        }

        const Elf64_Rela *relas =
            (const void *)(elf->data + relsec->sh_offset);
        size_t count = relsec->sh_size / sizeof(*relas);
        for (size_t i = 0; i < count; i++) {
            if (relas[i].r_offset < array->sh_addr ||
                relas[i].r_offset >= array_end) {
                continue;
            }

            unsigned type = ELF64_R_TYPE(relas[i].r_info);
            uint64_t addr = 0;
            if (type == R_X86_64_RELATIVE || type == R_X86_64_IRELATIVE) {
                addr = (uint64_t)relas[i].r_addend;
            } else if (type == R_X86_64_64 && symbols) {
                size_t sym = ELF64_R_SYM(relas[i].r_info);
                if (sym < symbol_count && symbols[sym].st_shndx != SHN_UNDEF) {
                    addr = symbols[sym].st_value + relas[i].r_addend;
                }
            }
            add_root_function(elf, funcs, addr, array_name);
        }
    }
}

static void load_function_array(const ElfFile *elf, const char *section_name,
                                const char *array_name, FuncVec *funcs)
{
    const Elf64_Shdr *array =
        elf_find_section_by_name(elf, section_name, NULL);
    if (!array || array->sh_size < sizeof(uint64_t) ||
        !range_ok(elf->size, array->sh_offset, array->sh_size)) {
        return;
    }

    const uint8_t *entries = elf->data + array->sh_offset;
    for (uint64_t off = 0; off + sizeof(uint64_t) <= array->sh_size;
         off += sizeof(uint64_t)) {
        add_root_function(elf, funcs, rd64(entries + off), array_name);
    }
    load_array_relocations(elf, array, array_name, funcs);
}

static void load_direct_target_closure(const ElfFile *elf, FuncVec *funcs)
{
    size_t work_count = 0;
    size_t work_cap = funcs->n ? funcs->n : 1;
    uint64_t *work = xmalloc(work_cap * sizeof(*work));
    for (size_t i = 0; i < funcs->n; i++) {
        if (!strncmp(funcs->v[i].name, "preinit_array_", 14) ||
            !strncmp(funcs->v[i].name, "init_array_", 11) ||
            !strncmp(funcs->v[i].name, "fini_array_", 11)) {
            work[work_count++] = funcs->v[i].addr;
        }
    }

    for (size_t next = 0; next < work_count; next++) {
        const FuncSym *fn = funcs_find_by_entry(funcs, work[next]);
        uint64_t file_off;
        if (!fn || !fn->size || !elf_function_file_offset(
                elf, fn->addr, fn->size, fn->shndx, &file_off)) {
            continue;
        }
        uint64_t fn_addr = fn->addr;
        size_t fn_size = (size_t)fn->size;
        const uint8_t *buf = elf->data + file_off;
        for (size_t off = 0; off < fn_size;) {
            Insn in = cfg_decode_insn(buf, fn_size, fn_addr, off);
            if (!in.len) in.len = 1;
            off += in.len;
            if (!in.has_target ||
                (in.kind != INSN_CALL && in.kind != INSN_JCC &&
                 in.kind != INSN_JMP) ||
                funcs_find_containing(funcs, in.target)) {
                continue;
            }
            if (!add_root_function(elf, funcs, in.target, "direct_target")) {
                continue;
            }
            dedup_functions(funcs);
            infer_zero_function_sizes(elf, funcs);
            if (work_count == work_cap) {
                work_cap *= 2;
                uint64_t *next_work =
                    realloc(work, work_cap * sizeof(*work));
                if (!next_work) die_errno("realloc direct target worklist");
                work = next_work;
            }
            work[work_count++] = in.target;
        }
    }
    free(work);
}

static void load_special_section_functions(const ElfFile *elf, FuncVec *funcs)
{
    FuncSym fn;
    if (special_section_func(elf, ".init", "_init", &fn)) {
        func_push(funcs, fn);
    }
    if (special_section_func(elf, ".fini", "_fini", &fn)) {
        func_push(funcs, fn);
    }
    if (special_section_func(elf, ".plt", "_plt", &fn)) {
        func_push(funcs, fn);
    }
    if (special_section_func(elf, ".plt.got", "_plt_got", &fn)) {
        func_push(funcs, fn);
    }
    if (special_section_func(elf, ".plt.sec", "_plt_sec", &fn)) {
        func_push(funcs, fn);
    }
    load_function_array(elf, ".preinit_array", "preinit_array", funcs);
    load_function_array(elf, ".init_array", "init_array", funcs);
    load_function_array(elf, ".fini_array", "fini_array", funcs);
    dedup_functions(funcs);
    infer_zero_function_sizes(elf, funcs);
    load_direct_target_closure(elf, funcs);
}

/*
 * Recover function ranges from FDE initial_location/address_range pairs.
 * Ranges are accepted only when they map into executable section bytes.
 */
static void load_functions_from_eh_frame(const ElfFile *elf, FuncVec *funcs)
{
    unsigned eh_idx = 0;
    const Elf64_Shdr *eh = elf_find_section_by_name(elf, ".eh_frame", &eh_idx);
    (void)eh_idx;
    if (!eh || !range_ok(elf->size, eh->sh_offset, eh->sh_size)) {
        return;
    }

    const uint8_t *buf = elf->data + eh->sh_offset;
    size_t size = eh->sh_size;
    uint64_t sec_addr = eh->sh_addr;
    CieVec cies = {0};

    for (size_t off = 0; off + 4 <= size;) {
        uint64_t rec_off = off;
        uint64_t len = rd32(buf + off);
        off += 4;
        if (len == 0) {
            break;
        }
        if (len == UINT32_MAX) {
            if (off + 8 > size) {
                break;
            }
            len = rd64(buf + off);
            off += 8;
        }
        if (len > size - off) {
            break;
        }

        uint64_t content_off = off;
        uint64_t end = off + len;
        if (end - content_off < 4) {
            off = (size_t)end;
            continue;
        }

        uint32_t id = rd32(buf + content_off);
        if (id == 0) {
            parse_cie_record(buf, size, rec_off, content_off, end, &cies);
        } else {
            uint64_t cie_off = content_off - id;
            const CieInfo *cie = find_cie(&cies, cie_off);
            if (cie) {
                size_t p = (size_t)content_off + 4;
                uint64_t start = 0;
                uint64_t range = 0;
                uint64_t field_addr = sec_addr + p;
                if (read_eh_value(buf, size, &p, field_addr, cie->fde_encoding,
                                  true, &start) &&
                    read_eh_value(buf, size, &p, 0, cie->fde_encoding,
                                  false, &range) &&
                    range > 0) {
                    unsigned shndx = 0;
                    if (elf_find_exec_section_for_range(elf, start, range,
                                                        &shndx)) {
                        char name[40];
                        synth_name(name, sizeof(name), start);
                        func_push(funcs, (FuncSym){
                            .name = xstrdup(name),
                            .addr = start,
                            .size = range,
                            .shndx = shndx,
                        });
                    }
                }
            }
        }
        off = (size_t)end;
    }

    free(cies.v);
    dedup_functions(funcs);
}

/* Load named STT_FUNC entries from all available symbol tables. */
static void load_functions(const ElfFile *elf, FuncVec *funcs)
{
    for (unsigned si = 0; si < elf->eh->e_shnum; si++) {
        Elf64_Shdr *symsec = &elf->sh[si];
        if (symsec->sh_type != SHT_SYMTAB && symsec->sh_type != SHT_DYNSYM) {
            continue;
        }
        if (symsec->sh_entsize != sizeof(Elf64_Sym) ||
            symsec->sh_link >= elf->eh->e_shnum) {
            continue;
        }
        if (!range_ok(elf->size, symsec->sh_offset, symsec->sh_size)) {
            continue;
        }

        Elf64_Shdr *strsec = &elf->sh[symsec->sh_link];
        if (!range_ok(elf->size, strsec->sh_offset, strsec->sh_size)) {
            continue;
        }

        const char *strtab = (const char *)elf->data + strsec->sh_offset;
        Elf64_Sym *syms = (Elf64_Sym *)(elf->data + symsec->sh_offset);
        size_t nsyms = symsec->sh_size / sizeof(Elf64_Sym);

        for (size_t i = 0; i < nsyms; i++) {
            Elf64_Sym *sym = &syms[i];
            unsigned type = ELF64_ST_TYPE(sym->st_info);
            if (type != STT_FUNC ||
                sym->st_shndx == SHN_UNDEF ||
                sym->st_name >= strsec->sh_size ||
                !elf_executable_section(elf, sym->st_shndx)) {
                continue;
            }
            const char *name = strtab + sym->st_name;
            if (!*name) {
                continue;
            }
            func_push(funcs, (FuncSym){
                .name = xstrdup(name),
                .addr = sym->st_value,
                .size = sym->st_size,
                .shndx = sym->st_shndx,
            });
        }
    }

    dedup_functions(funcs);
    infer_zero_function_sizes(elf, funcs);
}


/*
 * Public discovery policy: symbols first, then .eh_frame if no complete
 * .symtab is present or no usable function symbols were found.
 */
void funcs_load_all(const ElfFile *elf, FuncVec *funcs,
                    const char **source_out)
{
    load_functions(elf, funcs);
    const char *source = "symbols";
    bool has_symtab = elf_has_section_type(elf, SHT_SYMTAB);
    bool had_symbol_funcs = funcs->n != 0;
    if (!had_symbol_funcs || !has_symtab) {
        load_functions_from_eh_frame(elf, funcs);
        source = had_symbol_funcs ? "symbols+.eh_frame" : ".eh_frame";
    }
    /* The loader starts at e_entry even when no symbol or FDE names it. */
    unsigned entry_section;
    if (elf->eh->e_entry &&
        !funcs_find_by_entry(funcs, elf->eh->e_entry) &&
        elf_find_exec_section_for_range(elf, elf->eh->e_entry, 1,
                                        &entry_section)) {
        func_push(funcs, (FuncSym){
            .name = xstrdup("elf_entry"),
            .addr = elf->eh->e_entry,
            .shndx = entry_section,
        });
    }
    load_special_section_functions(elf, funcs);
    *source_out = source;
}

void funcs_free(FuncVec *funcs)
{
    for (size_t i = 0; i < funcs->n; i++) {
        free(funcs->v[i].name);
    }
    free(funcs->v);
    memset(funcs, 0, sizeof(*funcs));
}

const FuncSym *funcs_find_by_entry(const FuncVec *funcs, uint64_t addr)
{
    size_t lo = 0;
    size_t hi = funcs->n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (funcs->v[mid].addr < addr) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo < funcs->n && funcs->v[lo].addr == addr) {
        return &funcs->v[lo];
    }
    return NULL;
}

const FuncSym *funcs_find_containing(const FuncVec *funcs, uint64_t addr)
{
    size_t lo = 0;
    size_t hi = funcs->n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (funcs->v[mid].addr <= addr) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo == 0) {
        return NULL;
    }

    const FuncSym *fn = &funcs->v[lo - 1];
    if (addr >= fn->addr && addr < fn->addr + fn->size) {
        return fn;
    }
    return NULL;
}

static size_t cold_base_len(const char *name)
{
    const char *cold = strstr(name, ".cold");
    return cold ? (size_t)(cold - name) : strlen(name);
}

bool funcs_is_cold_fragment(const FuncSym *src, const FuncSym *dst)
{
    if (!src || !dst) {
        return false;
    }
    if (!strstr(src->name, ".cold") && !strstr(dst->name, ".cold")) {
        return false;
    }

    size_t src_len = cold_base_len(src->name);
    size_t dst_len = cold_base_len(dst->name);
    return src_len == dst_len && strncmp(src->name, dst->name, src_len) == 0;
}
