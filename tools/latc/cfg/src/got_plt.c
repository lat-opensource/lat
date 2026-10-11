#define _GNU_SOURCE

#include "got_plt.h"

#include "common.h"

/*
 * GOT/PLT metadata recovery and tiny instruction decoders.
 *
 * Relocations tell us which GOT slots represent external symbols or static
 * IFUNC resolvers. The CFG builder then uses this metadata to annotate direct
 * calls to PLT stubs and indirect calls/jumps through RIP-relative GOT slots.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void got_push(GotPltSymVec *vec, GotPltSym sym)
{
    if (vec->n == vec->cap) {
        size_t cap = vec->cap ? vec->cap * 2 : 128;
        GotPltSym *v = realloc(vec->v, cap * sizeof(*v));
        if (!v) {
            die_errno("realloc got symbols");
        }
        vec->v = v;
        vec->cap = cap;
    }
    vec->v[vec->n++] = sym;
}

static void plt_push(GotPltEntryVec *vec, GotPltEntry ent)
{
    if (vec->n == vec->cap) {
        size_t cap = vec->cap ? vec->cap * 2 : 64;
        GotPltEntry *v = realloc(vec->v, cap * sizeof(*v));
        if (!v) {
            die_errno("realloc plt entries");
        }
        vec->v = v;
        vec->cap = cap;
    }
    vec->v[vec->n++] = ent;
}

static int cmp_got(const void *a, const void *b)
{
    const GotPltSym *ga = a;
    const GotPltSym *gb = b;
    return (ga->offset > gb->offset) - (ga->offset < gb->offset);
}

static int cmp_plt(const void *a, const void *b)
{
    const GotPltEntry *pa = a;
    const GotPltEntry *pb = b;
    return (pa->addr > pb->addr) - (pa->addr < pb->addr);
}

static const Elf64_Shdr *find_section_by_name(const GotPltElf *elf,
                                              const char *name)
{
    for (unsigned i = 0; i < elf->eh->e_shnum; i++) {
        const char *secname = elf->shstr + elf->sh[i].sh_name;
        if (strcmp(secname, name) == 0) {
            return &elf->sh[i];
        }
    }
    return NULL;
}

static char *find_symbol_name_at(const GotPltElf *elf, uint64_t addr)
{
    const char *fallback = NULL;
    const char *best_ifunc = NULL;

    for (unsigned si = 0; si < elf->eh->e_shnum; si++) {
        Elf64_Shdr *symsec = &elf->sh[si];
        if ((symsec->sh_type != SHT_SYMTAB && symsec->sh_type != SHT_DYNSYM) ||
            symsec->sh_entsize != sizeof(Elf64_Sym) ||
            symsec->sh_link >= elf->eh->e_shnum ||
            !range_ok(elf->size, symsec->sh_offset, symsec->sh_size)) {
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
            if (sym->st_value != addr || sym->st_name >= strsec->sh_size) {
                continue;
            }
            const char *name = strtab + sym->st_name;
            if (!*name) {
                continue;
            }

            unsigned type = ELF64_ST_TYPE(sym->st_info);
            unsigned bind = ELF64_ST_BIND(sym->st_info);
            /*
             * Static IFUNC resolvers often have local aliases. Prefer the
             * shortest global/weak IFUNC name because it is usually the public
             * resolver name users expect to see.
             */
            if (type == STT_GNU_IFUNC &&
                (bind == STB_GLOBAL || bind == STB_WEAK)) {
                if (!best_ifunc || strlen(name) < strlen(best_ifunc)) {
                    best_ifunc = name;
                }
            } else if (!fallback &&
                       (type == STT_FUNC || type == STT_GNU_IFUNC)) {
                fallback = name;
            }
        }
    }

    if (best_ifunc) {
        return xstrdup(best_ifunc);
    }
    if (fallback) {
        return xstrdup(fallback);
    }

    char buf[48];
    snprintf(buf, sizeof(buf), "ifunc_resolver_%016" PRIx64, addr);
    return xstrdup(buf);
}

void gotplt_load_symbols(const GotPltElf *elf, GotPltSymVec *got)
{
    /*
     * Walk every RELA section. Dynamic binaries contribute external GOT/PLT
     * symbols; static binaries may still contain R_X86_64_IRELATIVE records
     * for IFUNC dispatch.
     */
    for (unsigned si = 0; si < elf->eh->e_shnum; si++) {
        Elf64_Shdr *relsec = &elf->sh[si];
        if (relsec->sh_type != SHT_RELA ||
            relsec->sh_entsize != sizeof(Elf64_Rela) ||
            !range_ok(elf->size, relsec->sh_offset, relsec->sh_size)) {
            continue;
        }

        Elf64_Rela *relas = (Elf64_Rela *)(elf->data + relsec->sh_offset);
        size_t nrelas = relsec->sh_size / sizeof(Elf64_Rela);

        Elf64_Shdr *symsec = NULL;
        Elf64_Shdr *strsec = NULL;
        const char *strtab = NULL;
        Elf64_Sym *syms = NULL;
        size_t nsyms = 0;
        if (relsec->sh_link < elf->eh->e_shnum) {
            symsec = &elf->sh[relsec->sh_link];
            if ((symsec->sh_type == SHT_SYMTAB || symsec->sh_type == SHT_DYNSYM) &&
                symsec->sh_entsize == sizeof(Elf64_Sym) &&
                symsec->sh_link < elf->eh->e_shnum &&
                range_ok(elf->size, symsec->sh_offset, symsec->sh_size)) {
                strsec = &elf->sh[symsec->sh_link];
                if (range_ok(elf->size, strsec->sh_offset, strsec->sh_size)) {
                    strtab = (const char *)elf->data + strsec->sh_offset;
                    syms = (Elf64_Sym *)(elf->data + symsec->sh_offset);
                    nsyms = symsec->sh_size / sizeof(Elf64_Sym);
                }
            }
        }

        for (size_t i = 0; i < nrelas; i++) {
            unsigned type = ELF64_R_TYPE(relas[i].r_info);
            unsigned symidx = ELF64_R_SYM(relas[i].r_info);
            if (type == R_X86_64_IRELATIVE) {
                char *name = find_symbol_name_at(elf, (uint64_t)relas[i].r_addend);
                got_push(got, (GotPltSym){
                    .offset = relas[i].r_offset,
                    .name = name,
                    .kind = GOTPLT_SYM_IRELATIVE,
                });
                continue;
            }

            if (type != R_X86_64_GLOB_DAT &&
                type != R_X86_64_JUMP_SLOT &&
                type != R_X86_64_64) {
                continue;
            }
            if (!syms || !strtab || !strsec || symidx >= nsyms) {
                continue;
            }

            Elf64_Sym *sym = &syms[symidx];
            if (sym->st_name >= strsec->sh_size) {
                continue;
            }
            const char *name = strtab + sym->st_name;
            if (!*name) {
                continue;
            }

            got_push(got, (GotPltSym){
                .offset = relas[i].r_offset,
                .name = xstrdup(name),
                .kind = GOTPLT_SYM_EXTERNAL,
            });
        }
    }

    qsort(got->v, got->n, sizeof(got->v[0]), cmp_got);
    size_t out = 0;
    for (size_t i = 0; i < got->n; i++) {
        if (out > 0 && got->v[i].offset == got->v[out - 1].offset) {
            free(got->v[i].name);
            continue;
        }
        got->v[out++] = got->v[i];
    }
    got->n = out;
}

void gotplt_load_plt_entries(const GotPltElf *elf, GotPltEntryVec *plt)
{
    const Elf64_Shdr *sec = find_section_by_name(elf, ".plt");
    if (!sec || !range_ok(elf->size, sec->sh_offset, sec->sh_size)) {
        return;
    }

    const uint8_t *buf = elf->data + sec->sh_offset;
    for (size_t off = 0; off + 6 <= sec->sh_size; off += 16) {
        size_t p = off;
        /* Skip CET endbr64 and bnd prefixes that may precede the stub jump. */
        if (p + 4 <= sec->sh_size &&
            buf[p] == 0xf3 && buf[p + 1] == 0x0f &&
            buf[p + 2] == 0x1e && buf[p + 3] == 0xfa) {
            p += 4;
        }
        if (p < sec->sh_size && buf[p] == 0xf2) {
            p++;
        }
        if (p + 6 > sec->sh_size || buf[p] != 0xff || buf[p + 1] != 0x25) {
            continue;
        }

        uint64_t insn_addr = sec->sh_addr + p;
        uint64_t got_addr = insn_addr + 6 + rd_i32(buf + p + 2);
        plt_push(plt, (GotPltEntry){
            .addr = sec->sh_addr + off,
            .got_offset = got_addr,
        });
    }

    qsort(plt->v, plt->n, sizeof(plt->v[0]), cmp_plt);
}

void gotplt_free_symbols(GotPltSymVec *got)
{
    for (size_t i = 0; i < got->n; i++) {
        free(got->v[i].name);
    }
    free(got->v);
    got->v = NULL;
    got->n = 0;
    got->cap = 0;
}

const GotPltSym *gotplt_lookup_symbol(const GotPltSymVec *got,
                                      uint64_t offset)
{
    size_t lo = 0;
    size_t hi = got->n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (got->v[mid].offset < offset) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo < got->n && got->v[lo].offset == offset) {
        return &got->v[lo];
    }
    return NULL;
}

const GotPltEntry *gotplt_lookup_plt(const GotPltEntryVec *plt,
                                     uint64_t addr)
{
    size_t lo = 0;
    size_t hi = plt->n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (plt->v[mid].addr < addr) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo < plt->n && plt->v[lo].addr == addr) {
        return &plt->v[lo];
    }
    return NULL;
}

size_t gotplt_count_kind(const GotPltSymVec *got, GotPltSymKind kind)
{
    size_t n = 0;
    for (size_t i = 0; i < got->n; i++) {
        if (got->v[i].kind == kind) {
            n++;
        }
    }
    return n;
}

bool gotplt_decode_direct_call(const uint8_t *buf, size_t size, size_t off,
                               uint64_t addr, uint64_t *target)
{
    if (off + 4 >= size || buf[off] != 0xe8) {
        return false;
    }
    *target = addr + 5 + rd_i32(buf + off + 1);
    return true;
}

bool gotplt_decode_rip_mem_ref(const uint8_t *buf, size_t size, size_t off,
                               uint64_t addr, uint8_t op, uint8_t group_reg,
                               uint64_t *target)
{
    /*
     * Recognize op /group_reg with ModRM mod=0 rm=5, i.e. RIP-relative memory:
     *   ff 15 disp32  call qword ptr [rip+disp32]
     *   ff 25 disp32  jmp  qword ptr [rip+disp32]
     */
    size_t p = off;
    while (p < size) {
        uint8_t b = buf[p];
        if (b == 0x3e || b == 0x66 || b == 0x67 || b == 0xf2 ||
            b == 0xf3 || b == 0xf0 || b == 0x2e || b == 0x36 ||
            b == 0x26 || b == 0x64 || b == 0x65 ||
            (b >= 0x40 && b <= 0x4f)) {
            p++;
            continue;
        }
        break;
    }

    if (p + 5 >= size || buf[p] != op) {
        return false;
    }

    uint8_t modrm = buf[p + 1];
    if ((modrm >> 6) != 0 || (modrm & 7) != 5 ||
        ((modrm >> 3) & 7) != group_reg) {
        return false;
    }

    size_t len = p + 6 - off;
    *target = addr + len + rd_i32(buf + p + 2);
    return true;
}
