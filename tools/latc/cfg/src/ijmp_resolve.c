#include "ijmp_resolve.h"

#include "common.h"

/*
 * Pattern-based indirect jump resolver.
 *
 * Supported table forms:
 *   - qword table with a register base or absolute disp32, direct or loaded
 *   - GCC PIC offset table: movsxd table[index*4], add/lea offset base, jmp reg
 *
 * Candidate targets must match decoded instruction boundaries or known
 * cross-function targets.
 */

#include <string.h>

#define IJMP_LOOKBACK_BYTES 4096

typedef struct {
    /* Decoded legacy/REX prefix state and offset of the real opcode. */
    int rex_w;
    int rex_r;
    int rex_x;
    int rex_b;
    bool nonflat_address;
    size_t op;
} Prefix;

typedef struct {
    /* add r/m64, r64 with register destination/source. */
    int dst;
    int src;
    size_t len;
} AddReg;

typedef struct {
    /* movsxd r64, dword ptr [base + index * 4] */
    int dst;
    int base;
    int index;
    int scale;
    size_t len;
} MovsxdMem;

typedef struct {
    /* lea reg, [rip + disp32] */
    int dst;
    uint64_t target;
    size_t len;
} LeaRip;

typedef struct {
    /* lea reg, [base + index * scale] */
    int dst;
    int base;
    int index;
    int scale;
    size_t len;
} LeaReg;

typedef struct {
    /* mov r64, r64 */
    int dst;
    int src;
    size_t len;
} MovReg;

typedef struct {
    /* mov r64, qword ptr [rip + disp32] */
    int dst;
    uint64_t address;
    size_t len;
} MovRip;

typedef struct {
    /* mov r64, qword ptr [base + index * scale + disp] */
    int dst;
    int base;
    int index;
    int scale;
    int32_t disp;
    size_t len;
} MovMem;

typedef struct {
    /* jmp qword ptr [base + index * scale], or [disp32 + index * scale]. */
    int base;
    int index;
    int scale;
    uint64_t table_addr;
    size_t len;
} JmpMem;

typedef struct {
    int dst;
    uint32_t value;
    size_t len;
} RegImm;

static bool in_func(const IjmpContext *ctx, uint64_t addr)
{
    return addr >= ctx->func_addr && addr < ctx->func_addr + ctx->func_size;
}

static bool is_insn_addr(const IjmpContext *ctx, uint64_t addr)
{
    if (ctx->insn_bitmap && addr >= ctx->func_addr) {
        uint64_t offset = addr - ctx->func_addr;
        return offset < ctx->insn_bitmap_size && ctx->insn_bitmap[offset];
    }
    size_t lo = 0;
    size_t hi = ctx->insn_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (ctx->insn_addrs[mid] < addr) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo < ctx->insn_count && ctx->insn_addrs[lo] == addr;
}

static size_t insn_lower_bound_off(const IjmpContext *ctx, size_t off)
{
    uint64_t addr = ctx->func_addr + off;
    size_t lo = 0, hi = ctx->insn_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (ctx->insn_addrs[mid] < addr) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static bool is_func_entry(const IjmpContext *ctx, uint64_t addr)
{
    return ctx->is_func_entry &&
           ctx->is_func_entry(ctx->func_entry_data, addr);
}

static bool is_program_insn(const IjmpContext *ctx, uint64_t addr)
{
    size_t lo = 0;
    size_t hi = ctx->program_insn_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (ctx->program_insn_addrs[mid] < addr) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo < ctx->program_insn_count &&
           ctx->program_insn_addrs[lo] == addr;
}

static bool is_direct_target(const IjmpContext *ctx, uint64_t addr)
{
    size_t lo = 0;
    size_t hi = ctx->direct_target_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (ctx->direct_targets[mid] < addr) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo < ctx->direct_target_count &&
           ctx->direct_targets[lo] == addr;
}

static bool is_valid_target(const IjmpContext *ctx, uint64_t target)
{
    if (in_func(ctx, target)) {
        return is_insn_addr(ctx, target);
    }
    return is_func_entry(ctx, target) || is_program_insn(ctx, target) ||
           is_direct_target(ctx, target);
}

static const uint8_t *va_ptr(const IjmpContext *ctx, uint64_t addr, size_t len)
{
    /* Translate a virtual table address back to a file pointer. */
    for (size_t i = 0; i < ctx->section_count; i++) {
        const IjmpSection *s = &ctx->sections[i];
        if (addr >= s->addr && len <= s->size &&
            addr - s->addr <= s->size - len) {
            uint64_t off = s->off + (addr - s->addr);
            if (off <= ctx->file_size && len <= ctx->file_size - off) {
                return ctx->file + off;
            }
        }
    }
    return NULL;
}

static Prefix parse_prefix(const uint8_t *buf, size_t size, size_t off)
{
    Prefix p = {0};
    p.op = off;

    while (p.op < size) {
        uint8_t b = buf[p.op];
        if (b == 0x3e || b == 0x66 || b == 0x67 || b == 0xf2 ||
            b == 0xf3 || b == 0xf0 || b == 0x2e || b == 0x36 ||
            b == 0x26 || b == 0x64 || b == 0x65) {
            p.nonflat_address |= b == 0x67 || b == 0x64 || b == 0x65;
            p.op++;
            continue;
        }
        if (b >= 0x40 && b <= 0x4f) {
            p.rex_w = (b >> 3) & 1;
            p.rex_r = (b >> 2) & 1;
            p.rex_x = (b >> 1) & 1;
            p.rex_b = b & 1;
            p.op++;
            continue;
        }
        break;
    }
    return p;
}

static bool parse_indirect_reg(const uint8_t *buf, size_t size, size_t off,
                               int *reg_out)
{
    Prefix p = parse_prefix(buf, size, off);
    if (p.op + 1 >= size || buf[p.op] != 0xff) {
        return false;
    }

    uint8_t modrm = buf[p.op + 1];
    uint8_t mod = modrm >> 6;
    uint8_t reg = (modrm >> 3) & 7;
    uint8_t rm = modrm & 7;
    if (mod != 3 || (reg != 2 && reg != 4)) {
        return false;
    }

    *reg_out = rm + p.rex_b * 8;
    return true;
}

static bool parse_mov_rip(const uint8_t *buf, size_t size,
                          uint64_t func_addr, size_t off, MovRip *out)
{
    Prefix p = parse_prefix(buf, size, off);
    if (p.op + 5 >= size || buf[p.op] != 0x8b) {
        return false;
    }
    uint8_t modrm = buf[p.op + 1];
    if ((modrm >> 6) != 0 || (modrm & 7) != 5) {
        return false;
    }
    out->dst = ((modrm >> 3) & 7) + p.rex_r * 8;
    out->len = p.op + 6 - off;
    out->address = func_addr + off + out->len + rd_i32(buf + p.op + 2);
    return true;
}

static bool parse_jmp_mem_scaled(const uint8_t *buf, size_t size, size_t off,
                                 JmpMem *out)
{
    Prefix p = parse_prefix(buf, size, off);
    if (p.op + 2 >= size || buf[p.op] != 0xff) {
        return false;
    }

    uint8_t modrm = buf[p.op + 1];
    uint8_t mod = modrm >> 6;
    uint8_t reg = (modrm >> 3) & 7;
    uint8_t rm = modrm & 7;
    if (mod != 0 || reg != 4 || rm != 4) {
        return false;
    }

    uint8_t sib = buf[p.op + 2];
    uint8_t scale_bits = sib >> 6;
    uint8_t index = (sib >> 3) & 7;
    uint8_t base = sib & 7;
    if (index == 4 && !p.rex_x) {
        return false;
    }

    out->table_addr = 0;
    if (base == 5) {
        if (size - p.op < 7 || p.nonflat_address) {
            return false;
        }
        /* A no-base SIB uses a sign-extended disp32, even with REX.B. */
        out->table_addr = (uint64_t)(int64_t)rd_i32(buf + p.op + 3);
    }
    out->base = base == 5 ? -1 : base + p.rex_b * 8;
    out->index = index + p.rex_x * 8;
    out->scale = 1 << scale_bits;
    out->len = p.op + 3 + (base == 5 ? 4 : 0) - off;
    return true;
}

static bool parse_add_reg(const uint8_t *buf, size_t size, size_t off,
                          AddReg *out)
{
    Prefix p = parse_prefix(buf, size, off);
    if (p.op + 1 >= size || buf[p.op] != 0x01) {
        return false;
    }

    uint8_t modrm = buf[p.op + 1];
    if ((modrm >> 6) != 3) {
        return false;
    }

    out->src = ((modrm >> 3) & 7) + p.rex_r * 8;
    out->dst = (modrm & 7) + p.rex_b * 8;
    out->len = p.op + 2 - off;
    return true;
}

static bool parse_and_imm(const uint8_t *buf, size_t size, size_t off,
                          RegImm *out)
{
    Prefix p = parse_prefix(buf, size, off);
    if (p.op + 2 >= size || (buf[p.op] != 0x83 && buf[p.op] != 0x81)) {
        return false;
    }
    uint8_t modrm = buf[p.op + 1];
    if ((modrm >> 6) != 3 || ((modrm >> 3) & 7) != 4) {
        return false;
    }
    out->dst = (modrm & 7) + p.rex_b * 8;
    if (buf[p.op] == 0x83) {
        out->value = (uint8_t)buf[p.op + 2];
        out->len = p.op + 3 - off;
    } else {
        if (p.op + 5 >= size) return false;
        out->value = (uint32_t)rd_i32(buf + p.op + 2);
        out->len = p.op + 6 - off;
    }
    return true;
}

static bool parse_shl_imm(const uint8_t *buf, size_t size, size_t off,
                          RegImm *out)
{
    Prefix p = parse_prefix(buf, size, off);
    if (p.op + 2 >= size || buf[p.op] != 0xc1) return false;
    uint8_t modrm = buf[p.op + 1];
    if ((modrm >> 6) != 3 || ((modrm >> 3) & 7) != 4) return false;
    out->dst = (modrm & 7) + p.rex_b * 8;
    out->value = buf[p.op + 2];
    out->len = p.op + 3 - off;
    return true;
}

static bool parse_movsxd_mem(const uint8_t *buf, size_t size, size_t off,
                             MovsxdMem *out)
{
    Prefix p = parse_prefix(buf, size, off);
    if (p.op + 2 >= size || buf[p.op] != 0x63) {
        return false;
    }

    uint8_t modrm = buf[p.op + 1];
    uint8_t mod = modrm >> 6;
    uint8_t rm = modrm & 7;
    if (mod == 3 || rm != 4) {
        return false;
    }

    uint8_t sib = buf[p.op + 2];
    uint8_t scale_bits = sib >> 6;
    uint8_t index = (sib >> 3) & 7;
    uint8_t base = sib & 7;
    if ((index == 4 && !p.rex_x) || (mod == 0 && base == 5) ||
        scale_bits != 2) {
        return false;
    }

    out->dst = ((modrm >> 3) & 7) + p.rex_r * 8;
    out->base = base + p.rex_b * 8;
    out->index = index + p.rex_x * 8;
    out->scale = 1 << scale_bits;
    out->len = p.op + 3 - off;
    if (mod == 1) {
        out->len += 1;
    } else if (mod == 2) {
        out->len += 4;
    }
    return true;
}

static bool parse_lea_rip(const uint8_t *buf, size_t size, uint64_t func_addr,
                          size_t off, LeaRip *out)
{
    Prefix p = parse_prefix(buf, size, off);
    if (p.op + 5 >= size || buf[p.op] != 0x8d) {
        return false;
    }

    uint8_t modrm = buf[p.op + 1];
    uint8_t mod = modrm >> 6;
    uint8_t rm = modrm & 7;
    if (mod != 0 || rm != 5) {
        return false;
    }

    out->dst = ((modrm >> 3) & 7) + p.rex_r * 8;
    out->len = p.op + 6 - off;
    out->target = func_addr + off + out->len + rd_i32(buf + p.op + 2);
    return true;
}

static bool parse_lea_reg(const uint8_t *buf, size_t size, size_t off,
                          LeaReg *out)
{
    Prefix p = parse_prefix(buf, size, off);
    if (p.op + 2 >= size || buf[p.op] != 0x8d) {
        return false;
    }

    uint8_t modrm = buf[p.op + 1];
    uint8_t mod = modrm >> 6;
    uint8_t rm = modrm & 7;
    if (mod != 0 || rm != 4) {
        return false;
    }

    uint8_t sib = buf[p.op + 2];
    uint8_t scale_bits = sib >> 6;
    uint8_t index = (sib >> 3) & 7;
    uint8_t base = sib & 7;
    if ((index == 4 && !p.rex_x) || base == 5) {
        return false;
    }

    out->dst = ((modrm >> 3) & 7) + p.rex_r * 8;
    out->base = base + p.rex_b * 8;
    out->index = index + p.rex_x * 8;
    out->scale = 1 << scale_bits;
    out->len = p.op + 3 - off;
    return true;
}

static bool parse_mov_reg(const uint8_t *buf, size_t size, size_t off,
                          MovReg *out)
{
    Prefix p = parse_prefix(buf, size, off);
    if (p.op + 1 >= size || (buf[p.op] != 0x89 && buf[p.op] != 0x8b)) {
        return false;
    }

    uint8_t modrm = buf[p.op + 1];
    if ((modrm >> 6) != 3) {
        return false;
    }

    int reg = ((modrm >> 3) & 7) + p.rex_r * 8;
    int rm = (modrm & 7) + p.rex_b * 8;
    if (buf[p.op] == 0x89) {
        out->src = reg;
        out->dst = rm;
    } else {
        out->src = rm;
        out->dst = reg;
    }
    out->len = p.op + 2 - off;
    return true;
}

static bool parse_mov_mem_scaled(const uint8_t *buf, size_t size, size_t off,
                                 MovMem *out)
{
    Prefix p = parse_prefix(buf, size, off);
    if (p.op + 2 >= size || buf[p.op] != 0x8b) {
        return false;
    }

    uint8_t modrm = buf[p.op + 1];
    uint8_t mod = modrm >> 6;
    uint8_t rm = modrm & 7;
    if (mod == 3 || rm != 4) {
        return false;
    }

    uint8_t sib = buf[p.op + 2];
    uint8_t scale_bits = sib >> 6;
    uint8_t index = (sib >> 3) & 7;
    uint8_t base = sib & 7;
    if (index == 4 && !p.rex_x) {
        return false;
    }
    bool absolute = mod == 0 && base == 5;
    if (absolute && (!p.rex_w || p.nonflat_address)) {
        return false;
    }

    size_t len = p.op + 3 - off;
    int32_t disp = 0;
    if (mod == 1) {
        if (p.op + 3 >= size) {
            return false;
        }
        disp = (int8_t)buf[p.op + 3];
        len += 1;
    } else if (mod == 2 || absolute) {
        if (p.op + 6 >= size) {
            return false;
        }
        disp = rd_i32(buf + p.op + 3);
        len += 4;
    }

    out->dst = ((modrm >> 3) & 7) + p.rex_r * 8;
    out->base = absolute ? -1 : base + p.rex_b * 8;
    out->index = index + p.rex_x * 8;
    out->scale = 1 << scale_bits;
    out->disp = disp;
    out->len = len;
    return true;
}

static bool add_target(IjmpResult *out, uint64_t target)
{
    /* Tables may contain duplicate case targets; keep each edge once. */
    for (size_t i = 0; i < out->count; i++) {
        if (out->targets[i] == target) {
            return true;
        }
    }
    if (out->count == IJMP_MAX_TARGETS) {
        return false;
    }
    out->targets[out->count++] = target;
    return true;
}

static bool resolve_qword_table(const IjmpContext *ctx, const uint8_t *func,
                                size_t func_size, size_t ijmp_off,
                                IjmpResult *out)
{
    /*
     * Absolute qword table pattern:
     *   lea base, [rip + table]
     *   jmp qword ptr [base + index * 8]
     * or jmp qword ptr [disp32 + index * 8].
     */
    JmpMem jmp;
    if (!parse_jmp_mem_scaled(func, func_size, ijmp_off, &jmp) ||
        jmp.scale != 8) {
        return false;
    }

    size_t start = ijmp_off > IJMP_LOOKBACK_BYTES ?
                   ijmp_off - IJMP_LOOKBACK_BYTES : 0;
    LeaRip lea = { .target = jmp.table_addr };
    bool have_lea = jmp.base < 0;
    for (size_t ii = insn_lower_bound_off(ctx, start);
         jmp.base >= 0 && ii < ctx->insn_count &&
         ctx->insn_addrs[ii] < ctx->func_addr + ijmp_off; ii++) {
        size_t off = (size_t)(ctx->insn_addrs[ii] - ctx->func_addr);
        LeaRip cur;
        if (parse_lea_rip(func, ijmp_off, ctx->func_addr, off, &cur) &&
            cur.dst == jmp.base) {
            lea = cur;
            have_lea = true;
        }
    }
    if (!have_lea) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->table_addr = lea.target;
    size_t entries = 0;
    bool has_cross_target = false;
    for (size_t i = 0; i < IJMP_MAX_TARGETS; i++) {
        if (i > (UINT64_MAX - lea.target) / 8) {
            break;
        }
        const uint8_t *p = va_ptr(ctx, lea.target + i * 8, 8);
        if (!p) {
            break;
        }

        uint64_t target = (uint64_t)p[0] | ((uint64_t)p[1] << 8) |
                          ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24) |
                          ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) |
                          ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
        if (!is_valid_target(ctx, target)) {
            break;
        }
        entries++;
        if (!in_func(ctx, target)) {
            has_cross_target = true;
        }
        if (!add_target(out, target)) {
            break;
        }
    }

    return out->count > 0 && (!has_cross_target || entries >= 2);
}

static bool resolve_reg_qword_table(const IjmpContext *ctx,
                                    const uint8_t *func,
                                    size_t ijmp_off,
                                    int jmp_reg, IjmpResult *out)
{
    /*
     * Function-pointer table loaded through a register:
     *   lea base, [rip + table]
     *   mov target, qword ptr [base + index * 8 + disp]
     *   jmp target
     */
    size_t start = ijmp_off > IJMP_LOOKBACK_BYTES ?
                   ijmp_off - IJMP_LOOKBACK_BYTES : 0;
    MovMem mov = {0};
    size_t mov_off = 0;
    bool have_mov = false;

    for (size_t ii = insn_lower_bound_off(ctx, start);
         ii < ctx->insn_count &&
         ctx->insn_addrs[ii] < ctx->func_addr + ijmp_off; ii++) {
        size_t off = (size_t)(ctx->insn_addrs[ii] - ctx->func_addr);
        MovMem cur;
        if (parse_mov_mem_scaled(func, ijmp_off, off, &cur) &&
            cur.dst == jmp_reg && cur.scale == 8) {
            if (cur.base < 0 && cur.len != ijmp_off - off) {
                continue;
            }
            mov = cur;
            mov_off = off;
            have_mov = true;
        }
    }
    if (!have_mov) {
        return false;
    }

    LeaRip lea = {0};
    bool have_lea = mov.base < 0;
    for (size_t ii = insn_lower_bound_off(ctx, start);
         mov.base >= 0 && ii < ctx->insn_count &&
         ctx->insn_addrs[ii] < ctx->func_addr + mov_off; ii++) {
        size_t off = (size_t)(ctx->insn_addrs[ii] - ctx->func_addr);
        LeaRip cur;
        if (parse_lea_rip(func, mov_off, ctx->func_addr, off, &cur) &&
            cur.dst == mov.base) {
            lea = cur;
            have_lea = true;
        }
    }
    if (!have_lea) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->table_addr = lea.target + (uint64_t)(int64_t)mov.disp;
    size_t entries = 0;
    bool has_cross_target = false;
    for (size_t i = 0; i < IJMP_MAX_TARGETS; i++) {
        if (i > (UINT64_MAX - out->table_addr) / 8) {
            break;
        }
        const uint8_t *p = va_ptr(ctx, out->table_addr + i * 8, 8);
        if (!p) {
            break;
        }

        uint64_t target = (uint64_t)p[0] | ((uint64_t)p[1] << 8) |
                          ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24) |
                          ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) |
                          ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
        if (!is_valid_target(ctx, target)) {
            break;
        }
        entries++;
        if (!in_func(ctx, target)) {
            has_cross_target = true;
        }
        if (!add_target(out, target)) {
            break;
        }
    }

    return out->count > 0 && (!has_cross_target || entries >= 2);
}

static bool resolve_const_reg_target(const IjmpContext *ctx,
                                     const uint8_t *func,
                                     size_t func_size, size_t ijmp_off,
                                     int jmp_reg, IjmpResult *out)
{
    /*
     * Some optimized loops use a register as a local branch target without a
     * table, e.g.:
     *   lea rcx, [rip + label]
     *   ...
     *   jmp rcx
     * Allow a short backwards alias chain through mov reg,reg, but stop when
     * the wanted register is otherwise modified. That keeps callback/vtable
     * jumps classified as unresolved.
     */
    size_t start = ijmp_off > IJMP_LOOKBACK_BYTES ?
                   ijmp_off - IJMP_LOOKBACK_BYTES : 0;
    int wanted = jmp_reg;

    for (size_t ii = insn_lower_bound_off(ctx, ijmp_off); ii > 0;) {
        size_t off = (size_t)(ctx->insn_addrs[--ii] - ctx->func_addr);
        if (off < start) break;

        LeaRip lea;
        if (parse_lea_rip(func, func_size, ctx->func_addr, off, &lea) &&
            lea.dst == wanted) {
            if (!is_valid_target(ctx, lea.target)) {
                return false;
            }
            memset(out, 0, sizeof(*out));
            out->table_addr = 0;
            return add_target(out, lea.target);
        }

        MovRip mov_rip;
        if (parse_mov_rip(func, func_size, ctx->func_addr, off, &mov_rip) &&
            mov_rip.dst == wanted) {
            const uint8_t *p = va_ptr(ctx, mov_rip.address, 8);
            if (!p) return false;
            uint64_t target = (uint64_t)p[0] | ((uint64_t)p[1] << 8) |
                              ((uint64_t)p[2] << 16) |
                              ((uint64_t)p[3] << 24) |
                              ((uint64_t)p[4] << 32) |
                              ((uint64_t)p[5] << 40) |
                              ((uint64_t)p[6] << 48) |
                              ((uint64_t)p[7] << 56);
            if (!is_valid_target(ctx, target)) return false;
            memset(out, 0, sizeof(*out));
            out->table_addr = mov_rip.address;
            return add_target(out, target);
        }

        MovReg mov;
        if (parse_mov_reg(func, func_size, off, &mov)) {
            if (mov.dst == wanted) {
                wanted = mov.src;
            }
            continue;
        }

        AddReg add;
        if (parse_add_reg(func, func_size, off, &add) && add.dst == wanted) {
            return false;
        }

        MovsxdMem movsxd;
        if (parse_movsxd_mem(func, func_size, off, &movsxd) &&
            movsxd.dst == wanted) {
            return false;
        }

        LeaReg lea_reg;
        if (parse_lea_reg(func, func_size, off, &lea_reg) &&
            lea_reg.dst == wanted) {
            return false;
        }
    }

    return false;
}

static bool resolve_computed_stride(const IjmpContext *ctx,
                                    const uint8_t *func, size_t func_size,
                                    size_t start, size_t combine_off,
                                    int offset_reg, int base_reg,
                                    IjmpResult *out)
{
    /* Optimized libc routines use code itself as a table:
     *   lea base, [rip + first_case]
     *   and index, case_count - 1
     *   shl index, log2(case_stride)
     *   add index, base
     *   jmp index
     * Every fixed-stride case is a valid indirect entry point. */
    LeaRip base = {0};
    RegImm mask = {0};
    RegImm shift = {0};
    bool have_base = false, have_mask = false, have_shift = false;
    for (size_t ii = insn_lower_bound_off(ctx, start);
         ii < ctx->insn_count &&
         ctx->insn_addrs[ii] < ctx->func_addr + combine_off; ii++) {
        size_t off = (size_t)(ctx->insn_addrs[ii] - ctx->func_addr);
        LeaRip cur_base;
        if (parse_lea_rip(func, func_size, ctx->func_addr, off, &cur_base) &&
            cur_base.dst == base_reg) {
            base = cur_base;
            have_base = true;
        }
        RegImm cur;
        if (parse_and_imm(func, func_size, off, &cur) &&
            cur.dst == offset_reg) {
            mask = cur;
            have_mask = true;
        }
        if (parse_shl_imm(func, func_size, off, &cur) &&
            cur.dst == offset_reg) {
            shift = cur;
            have_shift = true;
        }
    }
    uint64_t count = (uint64_t)mask.value + 1;
    if (!have_base || !have_mask || !have_shift || shift.value >= 63 ||
        !count || count > IJMP_MAX_TARGETS || (count & (count - 1))) {
        return false;
    }
    uint64_t stride = UINT64_C(1) << shift.value;
    memset(out, 0, sizeof(*out));
    out->table_addr = base.target;
    for (uint64_t i = 0; i < count; i++) {
        uint64_t target = base.target + i * stride;
        if (target < base.target || !is_valid_target(ctx, target) ||
            !add_target(out, target)) {
            memset(out, 0, sizeof(*out));
            return false;
        }
    }
    return out->count >= 2;
}

bool ijmp_resolve_jump_table(const IjmpContext *ctx, const uint8_t *func,
                             size_t func_size, size_t ijmp_off,
                             IjmpResult *out)
{
    memset(out, 0, sizeof(*out));

    int jmp_reg = -1;
    if (!parse_indirect_reg(func, func_size, ijmp_off, &jmp_reg)) {
        return resolve_qword_table(ctx, func, func_size, ijmp_off, out);
    }

    /* An adjacent absolute table load defines the jump register directly. */
    size_t jump_index = insn_lower_bound_off(ctx, ijmp_off);
    if (jump_index) {
        size_t off = ctx->insn_addrs[jump_index - 1] - ctx->func_addr;
        MovMem mov;
        if (parse_mov_mem_scaled(func, ijmp_off, off, &mov) &&
            mov.base < 0 && mov.dst == jmp_reg && mov.scale == 8 &&
            mov.len == ijmp_off - off) {
            return resolve_reg_qword_table(ctx, func, ijmp_off, jmp_reg, out);
        }
    }

    /*
     * GCC PIC table pattern:
     *   lea table_base, [rip + table]
     *   lea base, [rip + offset_base]  (may be the same as table_base)
     *   movsxd off, dword ptr [table_base + index * 4]
     *   add/lea jmp_reg, base + off
     *   jmp jmp_reg
     *
     * The scan is deliberately local: look back only a small window and only at
     * known instruction starts to avoid matching immediates or data bytes.
     */
    size_t start = ijmp_off > IJMP_LOOKBACK_BYTES ?
                   ijmp_off - IJMP_LOOKBACK_BYTES : 0;
    int offset_reg = -1;
    int base_reg = -1;
    size_t combine_off = 0;
    bool have_combine = false;

    for (size_t ii = insn_lower_bound_off(ctx, start);
         ii < ctx->insn_count &&
         ctx->insn_addrs[ii] < ctx->func_addr + ijmp_off; ii++) {
        size_t off = (size_t)(ctx->insn_addrs[ii] - ctx->func_addr);
        AddReg cur;
        if (parse_add_reg(func, ijmp_off, off, &cur) && cur.dst == jmp_reg) {
            offset_reg = jmp_reg;
            base_reg = cur.src;
            combine_off = off;
            have_combine = true;
        }

        LeaReg lea_reg;
        if (parse_lea_reg(func, ijmp_off, off, &lea_reg) &&
            lea_reg.dst == jmp_reg && lea_reg.scale == 1) {
            offset_reg = lea_reg.index;
            base_reg = lea_reg.base;
            combine_off = off;
            have_combine = true;
        }
    }
    if (!have_combine) {
        if (resolve_reg_qword_table(ctx, func, ijmp_off, jmp_reg, out)) {
            return true;
        }
        return resolve_const_reg_target(ctx, func, func_size, ijmp_off,
                                        jmp_reg, out);
    }


    if (resolve_computed_stride(ctx, func, func_size, start, combine_off,
                                offset_reg, base_reg, out)) {
        return true;
    }

    MovsxdMem mov = {0};
    size_t mov_off = 0;
    bool have_mov = false;
    for (size_t ii = insn_lower_bound_off(ctx, start);
         ii < ctx->insn_count &&
         ctx->insn_addrs[ii] < ctx->func_addr + combine_off; ii++) {
        size_t off = (size_t)(ctx->insn_addrs[ii] - ctx->func_addr);
        MovsxdMem cur;
        if (parse_movsxd_mem(func, combine_off, off, &cur) &&
            cur.dst == offset_reg) {
            mov = cur;
            mov_off = off;
            have_mov = true;
        }
    }
    if (!have_mov) {
        return false;
    }

    LeaRip lea = {0};
    LeaRip target_base = {0};
    bool have_lea = false;
    bool have_target_base = false;
    for (size_t ii = insn_lower_bound_off(ctx, start);
         ii < ctx->insn_count &&
         ctx->insn_addrs[ii] < ctx->func_addr + mov_off; ii++) {
        size_t off = (size_t)(ctx->insn_addrs[ii] - ctx->func_addr);
        LeaRip cur;
        if (parse_lea_rip(func, combine_off, ctx->func_addr, off, &cur)) {
            if (cur.dst == mov.base) {
                lea = cur;
                have_lea = true;
            }
            if (cur.dst == base_reg) {
                target_base = cur;
                have_target_base = true;
            }
        }
    }
    if (!have_lea || !have_target_base) {
        return false;
    }

    out->table_addr = lea.target;
    for (size_t i = 0; i < IJMP_MAX_TARGETS; i++) {
        const uint8_t *p = va_ptr(ctx, lea.target + i * 4, 4);
        if (!p) {
            break;
        }

        uint64_t target = target_base.target + (uint64_t)rd_i32(p);
        if (!is_valid_target(ctx, target)) {
            break;
        }
        if (!add_target(out, target)) {
            break;
        }
    }

    return out->count > 0;
}
