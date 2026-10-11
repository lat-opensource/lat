#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "cfg_program.h"

#include "cfg_decoder.h"
#include "elf_image.h"
#include "function_symbols.h"
#include "ijmp_resolve.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

typedef struct { Insn *v; size_t n; size_t cap; } InsnVec;
typedef struct { uint64_t *v; size_t n; size_t cap; } AddrVec;
typedef struct {
    InsnVec insns;
    AddrVec insn_addrs;
    uint8_t *insn_bitmap;
} DecodedFunction;
typedef struct {
    bool found;
    IjmpResult result;
} IjmpCache;

static bool grow(void **ptr, size_t *cap, size_t count, size_t elem)
{
    if (count < *cap) {
        return true;
    }
    size_t next = *cap ? *cap * 2 : 64;
    void *p = realloc(*ptr, next * elem);
    if (!p) {
        return false;
    }
    *ptr = p;
    *cap = next;
    return true;
}

static bool insn_push(InsnVec *v, Insn value)
{
    if (!grow((void **)&v->v, &v->cap, v->n, sizeof(*v->v))) {
        return false;
    }
    v->v[v->n++] = value;
    return true;
}

static bool addr_push(AddrVec *v, uint64_t value)
{
    if (!grow((void **)&v->v, &v->cap, v->n, sizeof(*v->v))) {
        return false;
    }
    v->v[v->n++] = value;
    return true;
}

static int cmp_addr(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static void sort_unique(AddrVec *v)
{
    if (!v->n) {
        return;
    }
    bool sorted = true;
    for (size_t i = 1; i < v->n; i++) {
        if (v->v[i] < v->v[i - 1]) {
            sorted = false;
            break;
        }
    }
    if (sorted) {
        /* Function symbols and their decoded instructions are normally already
         * in address order.  Deduplicate that common case in one linear pass. */
    } else if (v->n < 4096) {
        qsort(v->v, v->n, sizeof(*v->v), cmp_addr);
    } else {
        uint64_t *scratch = malloc(v->n * sizeof(*scratch));
        size_t *offsets = calloc(1U << 16, sizeof(*offsets));

        if (!scratch || !offsets) {
            free(offsets);
            free(scratch);
            qsort(v->v, v->n, sizeof(*v->v), cmp_addr);
        } else {
            uint64_t *source = v->v;
            uint64_t *destination = scratch;

            for (unsigned int shift = 0; shift < 64; shift += 16) {
                size_t next = 0;

                memset(offsets, 0, (1U << 16) * sizeof(*offsets));
                for (size_t i = 0; i < v->n; i++) {
                    offsets[(source[i] >> shift) & 0xffff]++;
                }
                for (size_t bucket = 0; bucket < (1U << 16); bucket++) {
                    size_t count = offsets[bucket];
                    offsets[bucket] = next;
                    next += count;
                }
                for (size_t i = 0; i < v->n; i++) {
                    size_t bucket = (source[i] >> shift) & 0xffff;
                    destination[offsets[bucket]++] = source[i];
                }
                uint64_t *swap = source;
                source = destination;
                destination = swap;
            }
            free(offsets);
            free(scratch);
        }
    }
    size_t out = 1;
    for (size_t i = 1; i < v->n; i++) {
        if (v->v[i] != v->v[out - 1]) {
            v->v[out++] = v->v[i];
        }
    }
    v->n = out;
}

static size_t addr_lower_bound(const AddrVec *v, uint64_t value)
{
    size_t lo = 0;
    size_t hi = v->n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (v->v[mid] < value) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

static bool prefix_entry(const InsnVec *v, const uint8_t *buf, uint64_t addr)
{
    for (size_t i = 0; i < v->n; i++) {
        const Insn *in = &v->v[i];
        if (addr <= in->addr || addr >= in->addr + in->len) continue;
        size_t skipped = (size_t)(addr - in->addr);
        for (size_t j = 0; j < skipped; j++) {
            if (!cfg_is_legacy_prefix(buf[in->off + j])) return false;
        }
        return true;
    }
    return false;
}

static bool addr_in_function(uint64_t addr, const FuncSym *fn)
{
    return addr >= fn->addr && addr < fn->addr + fn->size;
}

static bool is_function_entry(const void *opaque, uint64_t addr)
{
    return funcs_find_by_entry((const FuncVec *)opaque, addr) != NULL;
}

static bool push_function(CfgProgram *p, size_t *cap, CfgProgramFunction value)
{
    if (!grow((void **)&p->functions, cap, p->function_count,
              sizeof(*p->functions))) return false;
    p->functions[p->function_count++] = value;
    return true;
}

static bool push_tb(CfgProgram *p, size_t *cap, CfgTb value)
{
    if (!grow((void **)&p->tbs, cap, p->tb_count, sizeof(*p->tbs))) return false;
    p->tbs[p->tb_count++] = value;
    return true;
}

static bool push_edge(CfgProgram *p, size_t *cap, CfgProgramEdge value)
{
    if (!grow((void **)&p->edges, cap, p->edge_count, sizeof(*p->edges))) return false;
    p->edges[p->edge_count++] = value;
    return true;
}

static CfgTbTerm term_from_insn(InsnKind kind)
{
    switch (kind) {
    case INSN_CALL: return CFG_TB_CALL;
    case INSN_ICALL: return CFG_TB_INDIRECT_CALL;
    case INSN_JCC: return CFG_TB_CONDITIONAL;
    case INSN_JMP: return CFG_TB_JUMP;
    case INSN_IJMP: return CFG_TB_INDIRECT_JUMP;
    case INSN_RET: return CFG_TB_RETURN;
    case INSN_SYSCALL: return CFG_TB_SYSCALL;
    case INSN_STOP: return CFG_TB_STOP;
    default: return CFG_TB_FALLTHROUGH;
    }
}

static bool insn_can_fall_through(InsnKind kind)
{
    return kind == INSN_CALL || kind == INSN_ICALL || kind == INSN_JCC ||
           kind == INSN_SYSCALL;
}

static int collect_program_insns_and_direct_targets(const ElfFile *elf,
                                                    FuncVec *funcs,
                                                    DecodedFunction *decoded,
                                                    AddrVec *insn_addrs,
                                                    AddrVec *targets)
{
    for (size_t i = 0; i < funcs->n; i++) {
        FuncSym *fn = &funcs->v[i];
        uint64_t file_off;
        if (!fn->size || !elf_function_file_offset(
                elf, fn->addr, fn->size, fn->shndx, &file_off)) {
            continue;
        }
        const uint8_t *buf = elf->data + file_off;
        size_t size = (size_t)fn->size;
        DecodedFunction *function = &decoded[i];
        function->insn_bitmap = calloc(size, 1);
        if (!function->insn_bitmap) {
            return -1;
        }
        for (size_t off = 0; off < size;) {
            Insn in = cfg_decode_insn(buf, size, fn->addr, off);
            if (!in.len) in.len = 1;
            if (!insn_push(&function->insns, in) ||
                !addr_push(&function->insn_addrs, in.addr) ||
                !addr_push(insn_addrs, in.addr)) {
                return -1;
            }
            function->insn_bitmap[off] = 1;
            if (in.has_target &&
                (in.kind == INSN_CALL || in.kind == INSN_JCC ||
                 in.kind == INSN_JMP) &&
                !addr_push(targets, in.target)) {
                return -1;
            }
            off += in.len;
        }

        /* Some glibc assembly functions end in a conditional branch and
         * deliberately fall through alignment NOPs into the next symbol.
         * Reuse the decode above to identify them, then decode only the gap. */
        if (i + 1 < funcs->n && function->insns.n) {
            const FuncSym *next = &funcs->v[i + 1];
            const Insn *last = &function->insns.v[function->insns.n - 1];
            if (fn->shndx == next->shndx && fn->addr + fn->size < next->addr &&
                last->addr + last->len == fn->addr + fn->size &&
                insn_can_fall_through(last->kind)) {
                size_t extended_size = (size_t)(next->addr - fn->addr);
                uint8_t *extended_bitmap = realloc(function->insn_bitmap,
                                                   extended_size);
                if (!extended_bitmap) {
                    return -1;
                }
                function->insn_bitmap = extended_bitmap;
                memset(extended_bitmap + size, 0, extended_size - size);
                fn->size = extended_size;
                buf = elf->data + file_off;
                for (size_t off = size; off < extended_size;) {
                    Insn in = cfg_decode_insn(buf, extended_size, fn->addr, off);
                    if (!in.len) in.len = 1;
                    if (!insn_push(&function->insns, in) ||
                        !addr_push(&function->insn_addrs, in.addr) ||
                        !addr_push(insn_addrs, in.addr)) {
                        return -1;
                    }
                    extended_bitmap[off] = 1;
                    if (in.has_target &&
                        (in.kind == INSN_CALL || in.kind == INSN_JCC ||
                         in.kind == INSN_JMP) &&
                        !addr_push(targets, in.target)) {
                        return -1;
                    }
                    off += in.len;
                }
            }
        }
    }
    sort_unique(insn_addrs);
    sort_unique(targets);
    return 0;
}

static int analyze_function(const ElfFile *elf, const FuncVec *funcs,
                            const IjmpSection *sections, size_t section_count,
                            const AddrVec *program_insns,
                            const AddrVec *direct_targets, const FuncSym *fn,
                            const DecodedFunction *decoded,
                            bool resolve_jt, CfgProgram *out,
                            size_t *tb_cap, size_t *edge_cap,
                            CfgProgramFunction *result)
{
    uint64_t file_off;
    if (!fn->size || !elf_function_file_offset(elf, fn->addr, fn->size,
                                                fn->shndx, &file_off)) {
        return 1;
    }
    const uint8_t *buf = elf->data + file_off;
    size_t size = (size_t)fn->size;
    InsnVec insns = decoded->insns;
    AddrVec leaders = {0};
    AddrVec insn_addrs = decoded->insn_addrs;
    uint8_t *insn_bitmap = decoded->insn_bitmap;
    IjmpCache **ijmp_cache = resolve_jt ?
        calloc(insns.n, sizeof(*ijmp_cache)) : NULL;
    uint8_t *leader_bitmap = NULL;
    int rc = -1;

    if (!insn_bitmap || (resolve_jt && !ijmp_cache && insns.n)) goto done;

    IjmpContext ijmp = {
        .file = elf->data, .file_size = elf->size,
        .sections = sections, .section_count = section_count,
        .insn_addrs = insn_addrs.v, .insn_count = insn_addrs.n,
        .insn_bitmap = insn_bitmap, .insn_bitmap_size = size,
        .program_insn_addrs = program_insns->v,
        .program_insn_count = program_insns->n,
        .direct_targets = direct_targets->v,
        .direct_target_count = direct_targets->n,
        .is_func_entry = is_function_entry, .func_entry_data = funcs,
        .func_addr = fn->addr, .func_size = fn->size,
    };

    if (!addr_push(&leaders, fn->addr)) goto done;
    uint64_t function_end = fn->addr + fn->size;
    for (size_t i = addr_lower_bound(direct_targets, fn->addr);
         i < direct_targets->n && direct_targets->v[i] < function_end; i++) {
        if (!addr_push(&leaders, direct_targets->v[i])) {
            goto done;
        }
    }
    for (size_t i = 0; i < insns.n; i++) {
        Insn *in = &insns.v[i];
        uint64_t next = in->addr + in->len;
        if (in->has_target && addr_in_function(in->target, fn) &&
            !addr_push(&leaders, in->target)) goto done;
        if (in->kind != INSN_NORMAL && next < fn->addr + fn->size &&
            !addr_push(&leaders, next)) goto done;
        if (resolve_jt && (in->kind == INSN_IJMP ||
                           in->kind == INSN_ICALL)) {
            IjmpCache *cached = calloc(1, sizeof(*cached));
            if (!cached) goto done;
            cached->found = ijmp_resolve_jump_table(
                &ijmp, buf, size, in->off, &cached->result);
            ijmp_cache[i] = cached;
            if (cached->found) {
                for (size_t j = 0; j < cached->result.count; j++) {
                    if (addr_in_function(cached->result.targets[j], fn) &&
                        !addr_push(&leaders, cached->result.targets[j]))
                        goto done;
                }
            }
        }
    }
    sort_unique(&leaders);
    leader_bitmap = calloc(size, 1);
    if (!leader_bitmap && size) goto done;
    for (size_t i = 0; i < leaders.n; i++) {
        if (leaders.v[i] >= fn->addr && leaders.v[i] - fn->addr < size) {
            leader_bitmap[leaders.v[i] - fn->addr] = 1;
        }
    }

    result->first_tb = out->tb_count;
    result->status = CFG_FUNCTION_OK;
    size_t next_insn_index = 0;
    for (size_t li = 0; li < leaders.n; li++) {
        uint64_t start = leaders.v[li];
        while (next_insn_index < insns.n &&
               insns.v[next_insn_index].addr < start) {
            next_insn_index++;
        }
        if (next_insn_index == insns.n ||
            insns.v[next_insn_index].addr != start) {
            result->status = prefix_entry(&insns, buf, start) ?
                CFG_FUNCTION_OPEN : CFG_FUNCTION_ERROR;
            continue;
        }
        size_t si = next_insn_index;
        size_t ei = si;
        while (ei < insns.n) {
            if (ei != si && insns.v[ei].addr >= fn->addr &&
                insns.v[ei].addr - fn->addr < size &&
                leader_bitmap[insns.v[ei].addr - fn->addr]) break;
            InsnKind kind = insns.v[ei].kind;
            ei++;
            if (kind != INSN_NORMAL) break;
        }
        if (ei == si) continue;
        for (size_t chunk_si = si; chunk_si < ei; ) {
            /* LATX turns instruction 255 into the synthetic TB-exit jump, so
             * that instruction itself is the next TB start. */
            size_t chunk_ei = chunk_si + 254;
            if (chunk_ei > ei) chunk_ei = ei;
            Insn *last = &insns.v[chunk_ei - 1];
            uint64_t chunk_start = insns.v[chunk_si].addr;
            CfgTb tb = {
                .start = chunk_start,
                .end = last->addr + last->len,
                .terminator_pc = last->addr,
                .semantic_flags = CFG_TB_CODE64,
                .terminator = term_from_insn(last->kind),
                .first_edge = out->edge_count,
            };
            uint64_t next = last->addr + last->len;
#define EDGE(K, T, R) do { \
            if (!push_edge(out, edge_cap, (CfgProgramEdge){ \
                    .from = chunk_start, .to = (T), .kind = (K), \
                    .resolution = (R) })) \
                goto done; \
        } while (0)
            switch (last->kind) {
        case INSN_CALL:
            if (last->has_target) EDGE(CFG_EDGE_CALL, last->target, CFG_EDGE_STATIC);
            if (next < fn->addr + fn->size)
                EDGE(CFG_EDGE_CALL_RETURN, next, CFG_EDGE_STATIC);
            break;
        case INSN_ICALL:
        {
            IjmpCache *cached = ijmp_cache ? ijmp_cache[chunk_ei - 1] : NULL;
            bool found = cached && cached->found;
            if (found) {
                for (size_t j = 0; j < cached->result.count; j++) {
                    EDGE(CFG_EDGE_CALL, cached->result.targets[j],
                         CFG_EDGE_STATIC);
                }
            } else {
                EDGE(CFG_EDGE_RUNTIME, 0, CFG_EDGE_RUNTIME_RESOLVED);
                if (result->status != CFG_FUNCTION_ERROR)
                    result->status = CFG_FUNCTION_OPEN;
            }
            if (next < fn->addr + fn->size)
                EDGE(CFG_EDGE_CALL_RETURN, next, CFG_EDGE_STATIC);
            break;
        }
        case INSN_JCC:
            if (last->has_target) EDGE(CFG_EDGE_TRUE, last->target, CFG_EDGE_STATIC);
            if (next < fn->addr + fn->size) EDGE(CFG_EDGE_FALSE, next, CFG_EDGE_STATIC);
            break;
        case INSN_JMP:
            if (last->has_target) EDGE(CFG_EDGE_JUMP, last->target, CFG_EDGE_STATIC);
            break;
        case INSN_IJMP: {
            IjmpCache *cached = ijmp_cache ? ijmp_cache[chunk_ei - 1] : NULL;
            bool found = cached && cached->found;
            if (found) {
                out->resolved_jump_tables++;
                out->resolved_jump_table_targets += cached->result.count;
                for (size_t j = 0; j < cached->result.count; j++)
                    EDGE(CFG_EDGE_CASE, cached->result.targets[j],
                         CFG_EDGE_STATIC);
            } else {
                EDGE(CFG_EDGE_RUNTIME, 0, CFG_EDGE_RUNTIME_RESOLVED);
                if (result->status != CFG_FUNCTION_ERROR)
                    result->status = CFG_FUNCTION_OPEN;
            }
            break;
        }
        case INSN_RET:
            EDGE(CFG_EDGE_RUNTIME, 0, CFG_EDGE_RUNTIME_RESOLVED);
            break;
        case INSN_SYSCALL:
            EDGE(CFG_EDGE_RUNTIME, 0, CFG_EDGE_RUNTIME_RESOLVED);
            if (next < fn->addr + fn->size) EDGE(CFG_EDGE_FALLTHROUGH, next, CFG_EDGE_STATIC);
            break;
        case INSN_NORMAL:
            if (next < fn->addr + fn->size) EDGE(CFG_EDGE_FALLTHROUGH, next, CFG_EDGE_STATIC);
            break;
        case INSN_STOP:
            break;
            }
#undef EDGE
            tb.edge_count = out->edge_count - tb.first_edge;
            if (!push_tb(out, tb_cap, tb)) goto done;
            chunk_si = chunk_ei;
        }
    }
    result->tb_count = out->tb_count - result->first_tb;
    rc = 0;
done:
    if (ijmp_cache) {
        for (size_t i = 0; i < insns.n; i++) free(ijmp_cache[i]);
    }
    free(ijmp_cache);
    free(leader_bitmap);
    free(leaders.v);
    return rc;
}

int cfg_analyze_elf(const char *path, const CfgAnalyzeOptions *options,
                    CfgProgram *program, char *error, size_t error_size)
{
    if (!path || !program) return -1;
    memset(program, 0, sizeof(*program));
    ElfFile elf = {0};
    FuncVec funcs = {0};
    AddrVec program_insns = {0}, direct_targets = {0};
    DecodedFunction *decoded = NULL;
    IjmpSection *sections = NULL;
    const char *source = NULL;
    size_t function_cap = 0, tb_cap = 0, edge_cap = 0;
    int rc = -1;

    /* The imported ELF reader reports malformed input fatally. Validated callers
     * use this API today; converting parser failures to diagnostics is tracked
     * before exposing latc to untrusted files. */
    elf_load(path, &elf);
    funcs_load_all(&elf, &funcs, &source);
    (void)source;
    decoded = calloc(funcs.n, sizeof(*decoded));
    if (!decoded && funcs.n) goto out;
    if (collect_program_insns_and_direct_targets(
            &elf, &funcs, decoded, &program_insns, &direct_targets)) goto out;
    size_t section_count = 0;
    sections = elf_build_ijmp_sections(&elf, &section_count);
    program->exec_ranges = calloc(elf.eh->e_shnum,
                                  sizeof(*program->exec_ranges));
    if (!program->exec_ranges) goto out;
    for (unsigned i = 0; i < elf.eh->e_shnum; i++) {
        const Elf64_Shdr *section = &elf.sh[i];
        if (!elf_executable_section(&elf, i) || !section->sh_size ||
            section->sh_type == SHT_NOBITS) continue;
        program->exec_ranges[program->exec_range_count++] = (CfgExecRange){
            .start = section->sh_addr,
            .size = section->sh_size,
        };
    }

    for (size_t i = 0; i < funcs.n; i++) {
        CfgProgramFunction fn = {
            .name = strdup(funcs.v[i].name),
            .start = funcs.v[i].addr,
            .size = funcs.v[i].size,
        };
        if (!fn.name) goto out;
        int ar = analyze_function(&elf, &funcs, sections, section_count,
                                  &program_insns, &direct_targets, &funcs.v[i],
                                  &decoded[i],
                                  !options || options->resolve_jump_tables,
                                  program, &tb_cap, &edge_cap, &fn);
        if (ar < 0) { free(fn.name); goto out; }
        if (ar > 0) { free(fn.name); continue; }
        if (fn.status == CFG_FUNCTION_OPEN) program->open_function_count++;
        if (fn.status == CFG_FUNCTION_ERROR) program->error_function_count++;
        if (!push_function(program, &function_cap, fn)) { free(fn.name); goto out; }
    }
    rc = 0;
out:
    if (decoded) {
        for (size_t i = 0; i < funcs.n; i++) {
            free(decoded[i].insns.v);
            free(decoded[i].insn_addrs.v);
            free(decoded[i].insn_bitmap);
        }
    }
    free(decoded);
    free(program_insns.v);
    free(direct_targets.v);
    free(sections);
    funcs_free(&funcs);
    elf_free(&elf);
    if (rc && error && error_size) {
        snprintf(error, error_size, "CFG analysis failed: %s", strerror(errno));
        cfg_program_destroy(program);
    }
    return rc;
}

void cfg_program_destroy(CfgProgram *p)
{
    if (!p) return;
    for (size_t i = 0; i < p->function_count; i++) free(p->functions[i].name);
    free(p->functions); free(p->tbs); free(p->edges); free(p->exec_ranges);
    memset(p, 0, sizeof(*p));
}

bool cfg_program_address_is_executable(const CfgProgram *p, uint64_t address)
{
    for (size_t i = 0; i < p->exec_range_count; i++) {
        const CfgExecRange *range = &p->exec_ranges[i];
        if (address >= range->start && address - range->start < range->size)
            return true;
    }
    return false;
}

const char *cfg_tb_term_name(CfgTbTerm t)
{
    static const char *names[] = {"fallthrough", "call", "icall", "jcc", "jmp",
        "ijmp", "ret", "syscall", "stop"};
    return (unsigned)t < sizeof(names) / sizeof(names[0]) ? names[t] : "unknown";
}

const char *cfg_program_edge_kind_name(CfgProgramEdgeKind k)
{
    static const char *names[] = {"fall", "true", "false", "jmp", "call",
        "return", "case", "runtime"};
    return (unsigned)k < sizeof(names) / sizeof(names[0]) ? names[k] : "unknown";
}

const char *cfg_function_status_name(CfgFunctionStatus s)
{
    static const char *names[] = {"ok", "open", "error"};
    return (unsigned)s < sizeof(names) / sizeof(names[0]) ? names[s] : "unknown";
}
