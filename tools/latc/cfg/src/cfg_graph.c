#define _GNU_SOURCE

#include "cfg_graph.h"

#include "common.h"
#include "cfg_decoder.h"

/*
 * CFG construction and presentation for one function at a time.
 *
 * The module is deliberately output-oriented: it builds enough temporary graph
 * facts to print blocks/edges and to feed cfg_check, then frees them before
 * moving to the next function.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

typedef enum {
    XFER_NONE,
    XFER_DIRECT_CALL,
    XFER_GOT_CALL,
    XFER_INDIRECT_CALL,
    XFER_INDIRECT_JMP,
} XferKind;

typedef enum {
    XFER_TARGET_UNKNOWN,
    XFER_TARGET_REG,
    XFER_TARGET_RIP_MEM,
    XFER_TARGET_STACK_MEM,
    XFER_TARGET_OBJECT_FIELD,
    XFER_TARGET_INDEXED_MEM,
    XFER_TARGET_ABS_MEM,
} XferTargetKind;

typedef struct {
    /* Parsed shape of call/jmp operands that could not be fully resolved. */
    XferKind kind;
    XferTargetKind target_kind;
    int reg;
    int base;
    int index;
    int scale;
    int32_t disp;
    uint64_t rip_target;
} XferInfo;

typedef struct {
    Insn *v;
    size_t n;
    size_t cap;
} InsnVec;

typedef struct {
    uint64_t *v;
    size_t n;
    size_t cap;
} AddrVec;

typedef struct {
    CfgCheckBlock *v;
    size_t n;
    size_t cap;
} CheckBlockVec;

typedef struct {
    CfgCheckEdge *v;
    size_t n;
    size_t cap;
} CheckEdgeVec;

static void insn_push(InsnVec *vec, Insn insn)
{
    if (vec->n == vec->cap) {
        size_t cap = vec->cap ? vec->cap * 2 : 256;
        Insn *v = realloc(vec->v, cap * sizeof(*v));
        if (!v) {
            die_errno("realloc insns");
        }
        vec->v = v;
        vec->cap = cap;
    }
    vec->v[vec->n++] = insn;
}

static void addr_push(AddrVec *vec, uint64_t addr)
{
    if (vec->n == vec->cap) {
        size_t cap = vec->cap ? vec->cap * 2 : 64;
        uint64_t *v = realloc(vec->v, cap * sizeof(*v));
        if (!v) {
            die_errno("realloc addrs");
        }
        vec->v = v;
        vec->cap = cap;
    }
    vec->v[vec->n++] = addr;
}

static void check_block_push(CheckBlockVec *vec, CfgCheckBlock block)
{
    if (vec->n == vec->cap) {
        size_t cap = vec->cap ? vec->cap * 2 : 64;
        CfgCheckBlock *v = realloc(vec->v, cap * sizeof(*v));
        if (!v) {
            die_errno("realloc cfg check blocks");
        }
        vec->v = v;
        vec->cap = cap;
    }
    vec->v[vec->n++] = block;
}

static void check_edge_push(CheckEdgeVec *vec, CfgCheckEdge edge)
{
    if (vec->n == vec->cap) {
        size_t cap = vec->cap ? vec->cap * 2 : 128;
        CfgCheckEdge *v = realloc(vec->v, cap * sizeof(*v));
        if (!v) {
            die_errno("realloc cfg check edges");
        }
        vec->v = v;
        vec->cap = cap;
    }
    vec->v[vec->n++] = edge;
}

static const char *xfer_target_name(XferTargetKind kind)
{
    switch (kind) {
    case XFER_TARGET_REG:
        return "register";
    case XFER_TARGET_RIP_MEM:
        return "rip-memory";
    case XFER_TARGET_STACK_MEM:
        return "stack-memory";
    case XFER_TARGET_OBJECT_FIELD:
        return "object-field";
    case XFER_TARGET_INDEXED_MEM:
        return "indexed-memory";
    case XFER_TARGET_ABS_MEM:
        return "absolute-memory";
    default:
        return "unknown";
    }
}

static const char *reg_name(int reg)
{
    static const char *names[] = {
        "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    };
    if (reg >= 0 && reg < 16) {
        return names[reg];
    }
    return "?";
}

static XferTargetKind classify_mem_target(int base, int index, int32_t disp)
{
    if (base == 4 || base == 5 || base == 12 || base == 13) {
        return XFER_TARGET_STACK_MEM;
    }
    if (index >= 0) {
        return XFER_TARGET_INDEXED_MEM;
    }
    if (base >= 0) {
        (void)disp;
        return XFER_TARGET_OBJECT_FIELD;
    }
    return XFER_TARGET_ABS_MEM;
}

/*
 * Decode only call/jmp operand addressing. This is separate from cfg_decoder:
 * cfg_decoder decides instruction length and terminator kind; this helper
 * classifies unresolved transfer operands for readable diagnostics.
 */
static bool parse_xfer_info(const uint8_t *buf, size_t size, uint64_t base_addr,
                            size_t off, XferInfo *out)
{
    memset(out, 0, sizeof(*out));
    out->reg = -1;
    out->base = -1;
    out->index = -1;
    out->scale = 1;

    size_t p = off;
    int rex_r = 0;
    int rex_x = 0;
    int rex_b = 0;
    while (p < size) {
        uint8_t b = buf[p];
        if (b == 0x3e || b == 0x66 || b == 0x67 || b == 0xf2 ||
            b == 0xf3 || b == 0xf0 || b == 0x2e || b == 0x36 ||
            b == 0x26 || b == 0x64 || b == 0x65) {
            p++;
            continue;
        }
        if (b >= 0x40 && b <= 0x4f) {
            rex_r = (b >> 2) & 1;
            rex_x = (b >> 1) & 1;
            rex_b = b & 1;
            p++;
            continue;
        }
        break;
    }

    if (p >= size) {
        return false;
    }

    if (buf[p] == 0xe8 && p + 4 < size) {
        out->kind = XFER_DIRECT_CALL;
        return true;
    }

    if (buf[p] != 0xff || p + 1 >= size) {
        return false;
    }

    uint8_t modrm = buf[p + 1];
    uint8_t mod = modrm >> 6;
    uint8_t reg = ((modrm >> 3) & 7) + rex_r * 8;
    uint8_t rm = modrm & 7;
    if (reg == 2) {
        out->kind = XFER_INDIRECT_CALL;
    } else if (reg == 4 || reg == 5) {
        out->kind = XFER_INDIRECT_JMP;
    } else {
        return false;
    }

    if (mod == 3) {
        out->target_kind = XFER_TARGET_REG;
        out->reg = rm + rex_b * 8;
        return true;
    }

    size_t q = p + 2;
    int base_reg = rm + rex_b * 8;
    int index_reg = -1;
    int scale = 1;
    if (rm == 4) {
        if (q >= size) {
            return false;
        }
        uint8_t sib = buf[q++];
        scale = 1 << (sib >> 6);
        uint8_t idx = (sib >> 3) & 7;
        uint8_t sib_base = sib & 7;
        if (idx != 4) {
            index_reg = idx + rex_x * 8;
        }
        base_reg = sib_base + rex_b * 8;
        rm = sib_base;
    }

    int32_t disp = 0;
    bool rip_relative = false;
    bool abs_mem = false;
    if (mod == 0 && rm == 5) {
        if (q + 4 > size) {
            return false;
        }
        disp = rd_i32(buf + q);
        q += 4;
        if (base_reg == 5 || base_reg == 13) {
            rip_relative = true;
            base_reg = -1;
        } else {
            abs_mem = true;
            base_reg = -1;
        }
    } else if (mod == 1) {
        if (q >= size) {
            return false;
        }
        disp = (int8_t)buf[q++];
    } else if (mod == 2) {
        if (q + 4 > size) {
            return false;
        }
        disp = rd_i32(buf + q);
        q += 4;
    }

    out->base = base_reg;
    out->index = index_reg;
    out->scale = scale;
    out->disp = disp;
    if (rip_relative) {
        out->target_kind = XFER_TARGET_RIP_MEM;
        out->rip_target = base_addr + (q - off) + disp;
    } else if (abs_mem) {
        out->target_kind = XFER_TARGET_ABS_MEM;
    } else {
        out->target_kind = classify_mem_target(base_reg, index_reg, disp);
    }
    return true;
}

static void print_unresolved_xfer(uint64_t at, const char *what,
                                  const XferInfo *info)
{
    printf("    %s unresolved 0x%016" PRIx64 " kind=%s",
           what, at, xfer_target_name(info->target_kind));
    if (info->target_kind == XFER_TARGET_REG) {
        printf(" reg=%s", reg_name(info->reg));
    } else if (info->target_kind == XFER_TARGET_RIP_MEM) {
        printf(" mem=0x%016" PRIx64, info->rip_target);
    } else if (info->target_kind == XFER_TARGET_OBJECT_FIELD ||
               info->target_kind == XFER_TARGET_STACK_MEM) {
        printf(" base=%s disp=%" PRId32, reg_name(info->base), info->disp);
    } else if (info->target_kind == XFER_TARGET_INDEXED_MEM) {
        printf(" base=%s index=%s scale=%d disp=%" PRId32,
               reg_name(info->base), reg_name(info->index),
               info->scale, info->disp);
    }
    putchar('\n');
}

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static void sort_unique_addrs(AddrVec *vec)
{
    if (!vec->n) {
        return;
    }
    qsort(vec->v, vec->n, sizeof(vec->v[0]), cmp_u64);
    size_t out = 1;
    for (size_t i = 1; i < vec->n; i++) {
        if (vec->v[i] != vec->v[out - 1]) {
            vec->v[out++] = vec->v[i];
        }
    }
    vec->n = out;
}

static ssize_t find_insn(const InsnVec *insns, uint64_t addr)
{
    size_t lo = 0;
    size_t hi = insns->n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (insns->v[mid].addr < addr) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo < insns->n && insns->v[lo].addr == addr) {
        return (ssize_t)lo;
    }
    return -1;
}

static ssize_t find_containing_insn(const InsnVec *insns, uint64_t addr)
{
    size_t lo = 0;
    size_t hi = insns->n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (insns->v[mid].addr <= addr) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo == 0) {
        return -1;
    }
    size_t idx = lo - 1;
    if (addr >= insns->v[idx].addr &&
        addr < insns->v[idx].addr + insns->v[idx].len) {
        return (ssize_t)idx;
    }
    return -1;
}

static bool prefix_entry_target(const InsnVec *insns, const uint8_t *buf,
                                uint64_t target)
{
    /*
     * Some x86 code deliberately branches past legacy prefixes, for example
     * into the cmpxchg after a lock prefix. Treat those as explainable edges
     * rather than "no instruction boundary" errors.
     */
    if (find_insn(insns, target) >= 0) {
        return false;
    }

    ssize_t ci = find_containing_insn(insns, target);
    if (ci < 0) {
        return false;
    }

    const Insn *in = &insns->v[ci];
    size_t prefix_len = (size_t)(target - in->addr);
    if (prefix_len == 0 || prefix_len >= in->len) {
        return false;
    }
    for (size_t i = 0; i < prefix_len; i++) {
        if (!cfg_is_legacy_prefix(buf[in->off + i])) {
            return false;
        }
    }
    return true;
}

static bool addr_in_func(uint64_t addr, const FuncSym *fn)
{
    return addr >= fn->addr && addr < fn->addr + fn->size;
}

static bool ijmp_func_entry_lookup(const void *data, uint64_t addr)
{
    const FuncVec *funcs = data;
    return funcs_find_by_entry(funcs, addr) != NULL;
}

static CfgCheckXferKind classify_cross_xfer(const FuncSym *fn,
                                            const FuncVec *funcs,
                                            const GotPltEntryVec *plt,
                                            CfgCheckEdgeKind edge_kind,
                                            uint64_t to)
{
    if (addr_in_func(to, fn)) {
        return CFG_CHECK_XFER_NORMAL;
    }

    if (edge_kind == CFG_CHECK_EDGE_JMP && gotplt_lookup_plt(plt, to)) {
        return CFG_CHECK_XFER_PLT_TAILCALL;
    }

    const FuncSym *dst = funcs_find_by_entry(funcs, to);
    if (!dst) {
        dst = funcs_find_containing(funcs, to);
    }
    if (!dst) {
        return CFG_CHECK_XFER_NORMAL;
    }
    if (funcs_is_cold_fragment(fn, dst)) {
        return CFG_CHECK_XFER_COLD_FRAGMENT;
    }
    if (edge_kind == CFG_CHECK_EDGE_JMP) {
        return CFG_CHECK_XFER_TAILCALL;
    }
    return CFG_CHECK_XFER_INTERPROCEDURAL;
}

static const char *xfer_label(CfgCheckXferKind kind)
{
    switch (kind) {
    case CFG_CHECK_XFER_TAILCALL:
        return "tailcall";
    case CFG_CHECK_XFER_PLT_TAILCALL:
        return "plt-tailcall";
    case CFG_CHECK_XFER_INTERPROCEDURAL:
        return "interprocedural";
    case CFG_CHECK_XFER_COLD_FRAGMENT:
        return "cold-fragment";
    case CFG_CHECK_XFER_EXTERNAL:
        return "external";
    default:
        return "unknown-external";
    }
}

static void print_target_annotation(uint64_t to, const FuncVec *funcs,
                                    const GotPltEntryVec *plt,
                                    const GotPltSymVec *got)
{
    const GotPltEntry *plt_ent = gotplt_lookup_plt(plt, to);
    if (plt_ent) {
        const GotPltSym *sym = gotplt_lookup_symbol(got, plt_ent->got_offset);
        if (sym) {
            printf(" ; target=%s@PLT GOT[0x%016" PRIx64 "]",
                   sym->name, plt_ent->got_offset);
        } else {
            printf(" ; target=PLT GOT[0x%016" PRIx64 "]",
                   plt_ent->got_offset);
        }
        return;
    }

    const FuncSym *entry = funcs_find_by_entry(funcs, to);
    if (entry) {
        printf(" ; target=%s", entry->name);
        return;
    }

    const FuncSym *owner = funcs_find_containing(funcs, to);
    if (owner) {
        printf(" ; inside=%s+0x%" PRIx64, owner->name, to - owner->addr);
    }
}

static void print_direct_call(const FuncSym *fn, const FuncVec *funcs,
                              const GotPltEntryVec *plt,
                              const GotPltSymVec *got,
                              uint64_t at, uint64_t target)
{
    const FuncSym *dst = funcs_find_by_entry(funcs, target);
    CfgCheckXferKind xfer = CFG_CHECK_XFER_NORMAL;
    const char *kind = "direct";
    if (!addr_in_func(target, fn) && dst) {
        xfer = funcs_is_cold_fragment(fn, dst) ?
               CFG_CHECK_XFER_COLD_FRAGMENT :
               CFG_CHECK_XFER_INTERPROCEDURAL;
        kind = xfer_label(xfer);
    }

    printf("    call %-16s 0x%016" PRIx64 " -> 0x%016" PRIx64,
           kind, at, target);
    if (!addr_in_func(target, fn)) {
        if (dst) {
            printf(" ; target=%s", dst->name);
        } else {
            printf(" ; unknown-external");
            print_target_annotation(target, funcs, plt, got);
        }
    }
    putchar('\n');
}

static void print_edge(const FuncSym *fn, const FuncVec *funcs,
                       const GotPltEntryVec *plt, const GotPltSymVec *got,
                       uint64_t from, const char *kind, uint64_t to,
                       CfgCheckXferKind xfer_kind)
{
    printf("    edge %-5s 0x%016" PRIx64 " -> 0x%016" PRIx64,
           kind, from, to);
    if (!addr_in_func(to, fn)) {
        printf("  ; %s", xfer_label(xfer_kind));
        print_target_annotation(to, funcs, plt, got);
    }
    putchar('\n');
}

static void record_edge(CheckEdgeVec *edges, const InsnVec *insns,
                        const uint8_t *buf, uint64_t from, uint64_t to,
                        CfgCheckEdgeKind kind, CfgCheckXferKind xfer_kind)
{
    check_edge_push(edges, (CfgCheckEdge){
        .from = from,
        .to = to,
        .kind = kind,
        .xfer_kind = xfer_kind,
        .prefix_entry = prefix_entry_target(insns, buf, to),
    });
}

static CfgCheckTerm check_term(InsnKind kind)
{
    switch (kind) {
    case INSN_JCC:
        return CFG_CHECK_TERM_JCC;
    case INSN_JMP:
        return CFG_CHECK_TERM_JMP;
    case INSN_IJMP:
        return CFG_CHECK_TERM_IJMP;
    case INSN_RET:
        return CFG_CHECK_TERM_RET;
    case INSN_STOP:
        return CFG_CHECK_TERM_STOP;
    default:
        return CFG_CHECK_TERM_NORMAL;
    }
}

void cfg_graph_summary_init(CfgGraphSummary *summary)
{
    memset(summary, 0, sizeof(*summary));
}

void cfg_graph_summary_add(CfgGraphSummary *summary,
                           const CfgGraphSummary *delta)
{
    if (!summary || !delta) {
        return;
    }
    summary->functions_printed += delta->functions_printed;
    summary->unresolved_indirect_calls += delta->unresolved_indirect_calls;
    summary->unresolved_indirect_jumps += delta->unresolved_indirect_jumps;
    summary->resolved_indirect_jump_tables +=
        delta->resolved_indirect_jump_tables;
    summary->resolved_indirect_jump_table_targets +=
        delta->resolved_indirect_jump_table_targets;
}

void cfg_graph_print_summary(FILE *out, const char *title,
                             const CfgGraphSummary *summary)
{
    fprintf(out, "%s\n", title);
    fprintf(out, "  functions_printed=%zu\n", summary->functions_printed);
    fprintf(out, "  unresolved_indirect_calls=%zu\n",
            summary->unresolved_indirect_calls);
    fprintf(out, "  unresolved_indirect_jumps=%zu\n",
            summary->unresolved_indirect_jumps);
    fprintf(out, "  resolved_indirect_jump_tables=%zu\n",
            summary->resolved_indirect_jump_tables);
    fprintf(out, "  resolved_indirect_jump_table_targets=%zu\n",
            summary->resolved_indirect_jump_table_targets);
}

void cfg_print_function(const uint8_t *code, size_t code_size,
                        uint64_t file_off, const FuncSym *fn,
                        const FuncVec *funcs,
                        const IjmpSection *sections,
                        size_t section_count, const GotPltSymVec *got,
                        const GotPltEntryVec *plt,
                        const char *filter,
                        const CfgSummarySink *sinks,
                        size_t sink_count,
                        bool shadow_capstone,
                        CapstoneShadowSummary *shadow_summary)
{
    if (filter && !strstr(fn->name, filter)) {
        return;
    }

    const uint8_t *buf = code + file_off;
    size_t size = (size_t)fn->size;
    InsnVec insns = {0};
    AddrVec leaders = {0};
    AddrVec insn_addrs = {0};
    CheckBlockVec check_blocks = {0};
    CheckEdgeVec check_edges = {0};
    CfgGraphSummary graph_delta = {0};
    graph_delta.functions_printed = 1;

    /* First pass: establish instruction boundaries for the whole function. */
    for (size_t off = 0; off < size;) {
        Insn insn = cfg_decode_insn(buf, size, fn->addr, off);
        if (insn.len == 0) {
            insn.len = 1;
        }
        insn_push(&insns, insn);
        addr_push(&insn_addrs, insn.addr);
        off += insn.len;
    }

    /* Optional validation of the local decoder without changing CFG output. */
    if (shadow_capstone) {
        capstone_shadow_check_function(fn->name, buf, size, fn->addr,
                                       insns.v, insns.n, stdout,
                                       shadow_summary);
    }

    IjmpContext ijmp_ctx = {
        .file = code,
        .file_size = code_size,
        .sections = sections,
        .section_count = section_count,
        .insn_addrs = insn_addrs.v,
        .insn_count = insn_addrs.n,
        .is_func_entry = ijmp_func_entry_lookup,
        .func_entry_data = funcs,
        .func_addr = fn->addr,
        .func_size = fn->size,
    };

    /*
     * Leaders are function entry, direct branch targets, fallthrough after
     * terminators, and recovered jump-table case targets.
     */
    addr_push(&leaders, fn->addr);
    for (size_t i = 0; i < insns.n; i++) {
        Insn *in = &insns.v[i];
        uint64_t next = in->addr + in->len;
        if ((in->kind == INSN_JCC || in->kind == INSN_JMP) &&
            in->has_target && addr_in_func(in->target, fn)) {
            addr_push(&leaders, in->target);
        }
        if ((in->kind == INSN_JCC || in->kind == INSN_JMP ||
             in->kind == INSN_IJMP || in->kind == INSN_RET ||
             in->kind == INSN_STOP) &&
            next < fn->addr + fn->size) {
            addr_push(&leaders, next);
        }
        if (in->kind == INSN_JCC && next < fn->addr + fn->size) {
            addr_push(&leaders, next);
        }
        if (in->kind == INSN_IJMP) {
            IjmpResult jt;
            if (ijmp_resolve_jump_table(&ijmp_ctx, buf, size, in->off, &jt)) {
                for (size_t ti = 0; ti < jt.count; ti++) {
                    if (addr_in_func(jt.targets[ti], fn)) {
                        addr_push(&leaders, jt.targets[ti]);
                    }
                }
            }
        }
    }
    sort_unique_addrs(&leaders);

    printf("function 0x%016" PRIx64 " size=%" PRIu64 " name=%s blocks=%zu\n",
           fn->addr, fn->size, fn->name, leaders.n);

    /* Second pass: emit basic blocks, annotations, and CFG/check edges. */
    for (size_t bi = 0; bi < leaders.n; bi++) {
        uint64_t start = leaders.v[bi];
        ssize_t si = find_insn(&insns, start);
        if (si < 0) {
            if (prefix_entry_target(&insns, buf, start)) {
                printf("  bb 0x%016" PRIx64
                       " ; prefix entry into decoded instruction\n", start);
            } else {
                printf("  bb 0x%016" PRIx64
                       " ; no instruction boundary\n", start);
            }
            continue;
        }

        size_t ei = (size_t)si;
        uint64_t end = start;
        while (ei < insns.n) {
            Insn *in = &insns.v[ei];
            if (ei != (size_t)si) {
                ssize_t maybe_leader = -1;
                for (size_t li = 0; li < leaders.n; li++) {
                    if (leaders.v[li] == in->addr) {
                        maybe_leader = (ssize_t)li;
                        break;
                    }
                }
                if (maybe_leader >= 0) {
                    break;
                }
            }
            end = in->addr + in->len;
            if (in->kind != INSN_NORMAL) {
                ei++;
                break;
            }
            ei++;
        }

        if (ei == (size_t)si) {
            ei++;
        }

        Insn *last = &insns.v[ei - 1];
        printf("  bb 0x%016" PRIx64 "-0x%016" PRIx64
               " term=%s @0x%016" PRIx64 "\n",
               start, end, cfg_insn_kind_name(last->kind), last->addr);
        check_block_push(&check_blocks, (CfgCheckBlock){
            .start = start,
            .end = end,
            .term_addr = last->addr,
            .term = check_term(last->kind),
        });

        for (size_t ci = (size_t)si; ci < ei; ci++) {
            Insn *call = &insns.v[ci];
            XferInfo xfer;
            uint64_t slot = 0;

            /* Annotate indirect calls through known dynamic GOT slots. */
            if (gotplt_decode_rip_mem_ref(buf, size, call->off, call->addr,
                                          0xff, 2, &slot)) {
                const GotPltSym *sym = gotplt_lookup_symbol(got, slot);
                if (sym) {
                    printf("    call extern 0x%016" PRIx64
                           " -> %s@GOT[0x%016" PRIx64 "]\n",
                           call->addr, sym->name, slot);
                }
            }

            uint64_t direct_target = 0;
            bool direct_call = false;
            bool direct_call_is_plt = false;

            /* Direct calls may target normal code or a local IFUNC PLT stub. */
            if (gotplt_decode_direct_call(buf, size, call->off, call->addr,
                                          &direct_target)) {
                direct_call = true;
                const GotPltEntry *ent = gotplt_lookup_plt(plt, direct_target);
                if (ent) {
                    direct_call_is_plt = true;
                    const GotPltSym *sym = gotplt_lookup_symbol(got, ent->got_offset);
                    if (sym) {
                        printf("    call ifunc  0x%016" PRIx64
                               " -> %s@PLT[0x%016" PRIx64
                               "] GOT[0x%016" PRIx64 "]\n",
                               call->addr, sym->name, direct_target,
                               ent->got_offset);
                    }
                }
            }

            /*
             * Unresolved indirect calls are not CFG terminators, but printing
             * their operand shape makes the remaining dynamic dispatch visible.
            */
            if (direct_call && !direct_call_is_plt) {
                print_direct_call(fn, funcs, plt, got, call->addr,
                                  direct_target);
            } else if (parse_xfer_info(buf, size, call->addr,
                                       call->off, &xfer) &&
                       xfer.kind == XFER_INDIRECT_CALL &&
                       xfer.target_kind != XFER_TARGET_RIP_MEM) {
                print_unresolved_xfer(call->addr, "call", &xfer);
                graph_delta.unresolved_indirect_calls++;
            } else if (parse_xfer_info(buf, size, call->addr,
                                       call->off, &xfer) &&
                       xfer.kind == XFER_INDIRECT_CALL &&
                       xfer.target_kind == XFER_TARGET_RIP_MEM &&
                       !gotplt_lookup_symbol(got, xfer.rip_target)) {
                print_unresolved_xfer(call->addr, "call", &xfer);
                graph_delta.unresolved_indirect_calls++;
            }
        }

        uint64_t next = last->addr + last->len;
        if (last->kind == INSN_JCC) {
            if (last->has_target) {
                CfgCheckXferKind xfer_kind =
                    classify_cross_xfer(fn, funcs, plt, CFG_CHECK_EDGE_TRUE,
                                        last->target);
                record_edge(&check_edges, &insns, buf, start, last->target,
                            CFG_CHECK_EDGE_TRUE, xfer_kind);
                print_edge(fn, funcs, plt, got, start, "true", last->target,
                           xfer_kind);
            }
            if (next < fn->addr + fn->size) {
                record_edge(&check_edges, &insns, buf, start, next,
                            CFG_CHECK_EDGE_FALSE, CFG_CHECK_XFER_NORMAL);
                print_edge(fn, funcs, plt, got, start, "false", next,
                           CFG_CHECK_XFER_NORMAL);
            }
        } else if (last->kind == INSN_JMP) {
            if (last->has_target) {
                CfgCheckXferKind xfer_kind =
                    classify_cross_xfer(fn, funcs, plt, CFG_CHECK_EDGE_JMP,
                                        last->target);
                record_edge(&check_edges, &insns, buf, start, last->target,
                            CFG_CHECK_EDGE_JMP, xfer_kind);
                print_edge(fn, funcs, plt, got, start, "jmp", last->target,
                           xfer_kind);
            }
        } else if (last->kind == INSN_IJMP) {
            IjmpResult jt;
            if (ijmp_resolve_jump_table(&ijmp_ctx, buf, size, last->off, &jt)) {
                if (jt.table_addr) {
                    printf("    ijmp table 0x%016" PRIx64 " targets=%zu\n",
                           jt.table_addr, jt.count);
                } else {
                    printf("    ijmp fixed-target targets=%zu\n", jt.count);
                }
                graph_delta.resolved_indirect_jump_tables++;
                graph_delta.resolved_indirect_jump_table_targets += jt.count;
                for (size_t ti = 0; ti < jt.count; ti++) {
                    CfgCheckXferKind xfer_kind =
                        classify_cross_xfer(fn, funcs, plt, CFG_CHECK_EDGE_CASE,
                                            jt.targets[ti]);
                    record_edge(&check_edges, &insns, buf, start, jt.targets[ti],
                                CFG_CHECK_EDGE_CASE, xfer_kind);
                    print_edge(fn, funcs, plt, got, start, "case", jt.targets[ti],
                               xfer_kind);
                }
            } else {
                uint64_t slot = 0;
                /* Tail jumps through GOT/PLT are external exits, not cases. */
                if (gotplt_decode_rip_mem_ref(buf, size, last->off, last->addr,
                                              0xff, 4, &slot)) {
                    const GotPltSym *sym = gotplt_lookup_symbol(got, slot);
                    if (sym) {
                        record_edge(&check_edges, &insns, buf, start, slot,
                                    CFG_CHECK_EDGE_EXTERNAL,
                                    CFG_CHECK_XFER_EXTERNAL);
                        printf("    edge ijmp  0x%016" PRIx64
                               " -> %s@GOT[0x%016" PRIx64 "] ; external\n",
                               start, sym->name, slot);
                    } else {
                        record_edge(&check_edges, &insns, buf, start, slot,
                                    CFG_CHECK_EDGE_EXTERNAL,
                                    CFG_CHECK_XFER_EXTERNAL);
                        printf("    edge ijmp  0x%016" PRIx64
                               " -> <got 0x%016" PRIx64 ">\n", start, slot);
                    }
                } else {
                    XferInfo xfer;
                    if (parse_xfer_info(buf, size, last->addr,
                                        last->off, &xfer)) {
                        print_unresolved_xfer(last->addr, "ijmp", &xfer);
                    } else {
                        printf("    ijmp unresolved 0x%016" PRIx64
                               " kind=unknown\n", last->addr);
                    }
                    graph_delta.unresolved_indirect_jumps++;
                }
            }
        } else if (last->kind == INSN_NORMAL &&
                   next < fn->addr + fn->size) {
            record_edge(&check_edges, &insns, buf, start, next,
                        CFG_CHECK_EDGE_FALL, CFG_CHECK_XFER_NORMAL);
            print_edge(fn, funcs, plt, got, start, "fall", next,
                       CFG_CHECK_XFER_NORMAL);
        }
    }

    CfgCheckResult check_result =
        cfg_check_function(fn->name, fn->addr, fn->size,
                           check_blocks.v, check_blocks.n,
                           check_edges.v, check_edges.n,
                           stdout, NULL);
    for (size_t i = 0; i < sink_count; i++) {
        cfg_check_summary_add_result(sinks[i].check, &check_result,
                                     check_blocks.n, check_edges.n);
        cfg_graph_summary_add(sinks[i].graph, &graph_delta);
    }
    putchar('\n');
    free(insns.v);
    free(leaders.v);
    free(insn_addrs.v);
    free(check_blocks.v);
    free(check_edges.v);
}
