#include "cfg_decoder.h"

#include "common.h"

/*
 * Length-oriented x86-64 decoder.
 *
 * This is not intended to replace Capstone. It recognizes CFG terminators and
 * computes instruction lengths for the subset of encodings seen in SPEC and
 * common system binaries. The optional Capstone shadow pass validates that the
 * resulting boundaries stay aligned.
 */

typedef struct {
    const uint8_t *buf;
    size_t size;

    /* Prefix state that changes immediate/address sizes. */
    bool op66;
    bool addr67;
    bool rex_w;
} DecodeCtx;

bool cfg_is_legacy_prefix(uint8_t b)
{
    switch (b) {
    case 0xf0:
    case 0xf2:
    case 0xf3:
    case 0x2e:
    case 0x36:
    case 0x3e:
    case 0x26:
    case 0x64:
    case 0x65:
    case 0x66:
    case 0x67:
        return true;
    default:
        return false;
    }
}

static size_t imm_osz(const DecodeCtx *ctx)
{
    return ctx->op66 ? 2 : 4;
}

static size_t moffs_size(const DecodeCtx *ctx)
{
    return ctx->addr67 ? 4 : 8;
}

static size_t modrm_tail_len(const DecodeCtx *ctx, size_t off)
{
    if (off >= ctx->size) {
        return 0;
    }

    uint8_t modrm = ctx->buf[off];
    uint8_t mod = modrm >> 6;
    uint8_t rm = modrm & 7;
    size_t len = 1;

    /*
     * Account for SIB and displacement bytes. This is the core piece needed to
     * avoid drifting into the middle of the next instruction after FF /4, FF /5,
     * vector ops, and ordinary memory operands.
     */
    if (!ctx->addr67 && mod != 3 && rm == 4) {
        if (off + len >= ctx->size) {
            return len;
        }
        uint8_t sib = ctx->buf[off + len];
        uint8_t base = sib & 7;
        len++;
        if (mod == 0 && base == 5) {
            len += 4;
        }
    } else if (ctx->addr67 && mod != 3 && rm == 4) {
        if (off + len >= ctx->size) {
            return len;
        }
        uint8_t sib = ctx->buf[off + len];
        uint8_t base = sib & 7;
        len++;
        if (mod == 0 && base == 5) {
            len += 4;
        }
    }

    if (mod == 1) {
        len += 1;
    } else if (mod == 2) {
        len += 4;
    } else if (mod == 0 && rm == 5) {
        len += 4;
    }

    return len;
}

static uint8_t modrm_reg(const DecodeCtx *ctx, size_t off)
{
    if (off >= ctx->size) {
        return 0;
    }
    return (ctx->buf[off] >> 3) & 7;
}

static bool one_byte_has_modrm(uint8_t op)
{
    /*
     * Compact opcode table for the instructions this tool must step over.
     * Unknown one-byte opcodes are treated as no-ModRM with zero immediate so
     * the decoder advances conservatively by one byte.
     */
    if (op <= 0x3f) {
        return (op & 7) <= 3;
    }
    if (op >= 0x84 && op <= 0x8f) {
        return true;
    }
    if (op >= 0xd0 && op <= 0xdf) {
        return true;
    }
    switch (op) {
    case 0x62:
    case 0x63:
    case 0x69:
    case 0x6b:
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
    case 0xc0:
    case 0xc1:
    case 0xc4:
    case 0xc5:
    case 0xc6:
    case 0xc7:
    case 0xf6:
    case 0xf7:
    case 0xfe:
    case 0xff:
        return true;
    default:
        return false;
    }
}

static bool op0f_no_modrm(uint8_t op)
{
    switch (op) {
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x08:
    case 0x09:
    case 0x0b:
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x37:
    case 0x77:
    case 0xa0:
    case 0xa1:
    case 0xa2:
    case 0xa8:
    case 0xa9:
        return true;
    default:
        return op >= 0xc8 && op <= 0xcf;
    }
}

static size_t decode_0f_map(const DecodeCtx *ctx, size_t op_off, bool map_0f38,
                            bool map_0f3a)
{
    if (op_off >= ctx->size) {
        return ctx->size ? 1 : 0;
    }

    uint8_t op = ctx->buf[op_off];
    size_t len = op_off + 1;
    bool has_modrm = true;
    size_t imm = 0;
    /*
     * 0F 38 and 0F 3A are separate opcode maps. Most entries in both maps have
     * ModRM; 0F 3A also carries an imm8. Do not apply the normal 0F no-ModRM
     * table here: opcodes like VEX.0F38.A9 are FMA instructions with ModRM,
     * while legacy 0F A9 is POP GS without ModRM.
     */
    if (map_0f38) {
        has_modrm = true;
    } else if (map_0f3a) {
        has_modrm = true;
        imm = 1;
    } else {
        if (op0f_no_modrm(op) || (op >= 0x80 && op <= 0x8f)) {
            has_modrm = false;
        }
        switch (op) {
        case 0x0f:
        case 0x70:
        case 0x71:
        case 0x72:
        case 0x73:
        case 0xa4:
        case 0xac:
        case 0xba:
        case 0xc2:
        case 0xc4:
        case 0xc5:
        case 0xc6:
            imm = 1;
            break;
        default:
            break;
        }
    }

    if (has_modrm) {
        len += modrm_tail_len(ctx, len);
    }
    len += imm;
    return len;
}

static size_t decode_vex_len(const DecodeCtx *ctx, size_t off, uint8_t op)
{
    size_t prefix_len = op == 0xc5 ? 2 : 3;
    if (off + prefix_len >= ctx->size) {
        return ctx->size - off;
    }

    uint8_t map = 1;
    if (op == 0xc4) {
        map = ctx->buf[off + 1] & 0x1f;
    }

    size_t op_off = off + prefix_len;
    if (map == 2) {
        return decode_0f_map(ctx, op_off, true, false) - op_off + prefix_len;
    }
    if (map == 3) {
        return decode_0f_map(ctx, op_off, false, true) - op_off + prefix_len;
    }
    return decode_0f_map(ctx, op_off, false, false) - op_off + prefix_len;
}

static size_t decode_evex_len(const DecodeCtx *ctx, size_t off)
{
    if (off + 4 >= ctx->size) {
        return ctx->size - off;
    }

    uint8_t map = ctx->buf[off + 1] & 3;
    size_t op_off = off + 4;
    if (map == 2) {
        return decode_0f_map(ctx, op_off, true, false) - op_off + 4;
    }
    if (map == 3) {
        return decode_0f_map(ctx, op_off, false, true) - op_off + 4;
    }
    return decode_0f_map(ctx, op_off, false, false) - op_off + 4;
}

Insn cfg_decode_insn(const uint8_t *buf, size_t size, uint64_t base,
                     size_t off)
{
    DecodeCtx ctx = {
        .buf = buf,
        .size = size,
    };
    Insn insn = {
        .addr = base + off,
        .off = off,
        .len = 1,
        .kind = INSN_NORMAL,
    };

    /* Consume legacy and REX prefixes before classifying the opcode. */
    size_t p = off;
    while (p < size) {
        uint8_t b = buf[p];
        if (cfg_is_legacy_prefix(b)) {
            if (b == 0x66) {
                ctx.op66 = true;
            } else if (b == 0x67) {
                ctx.addr67 = true;
            }
            p++;
            continue;
        }
        if (b >= 0x40 && b <= 0x4f) {
            ctx.rex_w = (b & 8) != 0;
            p++;
            continue;
        }
        break;
    }

    if (p >= size) {
        insn.len = size - off;
        return insn;
    }

    uint8_t op = buf[p];
    size_t len = p + 1;

    /* First handle the control-flow opcodes that define CFG boundaries. */
    if (op >= 0x70 && op <= 0x7f) {
        if (p + 1 < size) {
            insn.kind = INSN_JCC;
            insn.has_target = true;
            insn.target = insn.addr + (len - off) + 1 + (int8_t)buf[p + 1];
        }
        len += 1;
    } else if (op == 0x0f) {
        if (p + 1 >= size) {
            len = size;
        } else {
            uint8_t op2 = buf[p + 1];
            if (op2 >= 0x80 && op2 <= 0x8f) {
                if (p + 5 < size) {
                    insn.kind = INSN_JCC;
                    insn.has_target = true;
                    insn.target = insn.addr + (p + 6 - off) +
                                  rd_i32(buf + p + 2);
                }
                len = p + 6;
            } else if (op2 == 0x05 || op2 == 0x34) {
                /* syscall/sysenter return to the following guest address. */
                insn.kind = INSN_SYSCALL;
                len = p + 2;
            } else {
                if (op2 == 0x38 && p + 2 < size) {
                    len = decode_0f_map(&ctx, p + 2, true, false);
                } else if (op2 == 0x3a && p + 2 < size) {
                    len = decode_0f_map(&ctx, p + 2, false, true);
                } else {
                    len = decode_0f_map(&ctx, p + 1, false, false);
                }
            }
        }
    } else if (op == 0xe8) {
        if (p + 4 < size) {
            insn.kind = INSN_CALL;
            insn.has_target = true;
            insn.target = insn.addr + (p + 5 - off) + rd_i32(buf + p + 1);
        }
        len += 4;
    } else if (op >= 0xe0 && op <= 0xe3) {
        /* loopne/loope/loop/jrcxz are conditional control transfers. */
        if (p + 1 < size) {
            insn.kind = INSN_JCC;
            insn.has_target = true;
            insn.target = insn.addr + (len - off) + 1 + (int8_t)buf[p + 1];
        }
        len += 1;
    } else if (op == 0xeb) {
        if (p + 1 < size) {
            insn.kind = INSN_JMP;
            insn.has_target = true;
            insn.target = insn.addr + (len - off) + 1 + (int8_t)buf[p + 1];
        }
        len += 1;
    } else if (op == 0xe9) {
        if (p + 4 < size) {
            insn.kind = INSN_JMP;
            insn.has_target = true;
            insn.target = insn.addr + (p + 5 - off) + rd_i32(buf + p + 1);
        }
        len += 4;
    } else if (op == 0xc2 || op == 0xca) {
        insn.kind = INSN_RET;
        len += 2;
    } else if (op == 0xc3 || op == 0xcb) {
        insn.kind = INSN_RET;
    } else if (op == 0xcc || op == 0xce || op == 0xcf || op == 0xf4) {
        insn.kind = INSN_STOP;
    } else if (op == 0xcd) {
        insn.kind = INSN_STOP;
        len += 1;
    } else if (op == 0xc5 && p + 1 < size) {
        /* In 64-bit mode C5/C4 are VEX prefixes, not LDS/LES. */
        len = p + decode_vex_len(&ctx, p, op);
    } else if (op == 0xc4 && p + 2 < size) {
        len = p + decode_vex_len(&ctx, p, op);
    } else if (op == 0x62 && p + 3 < size) {
        /* EVEX prefix; required for AVX-512-heavy libc routines. */
        len = p + decode_evex_len(&ctx, p);
    } else {
        bool has_modrm = one_byte_has_modrm(op);
        size_t imm = 0;

        /* Generic length path for non-control-flow instructions. */
        if ((op & 0xf8) == 0xb8) {
            imm = ctx.rex_w ? 8 : imm_osz(&ctx);
        } else if ((op & 0xf8) == 0xb0) {
            imm = 1;
        } else if ((op & 0xc7) == 0x04 || (op >= 0x6a && op <= 0x6b) ||
                   op == 0xa8 || op == 0xc0 || op == 0xc1) {
            imm = 1;
        } else if ((op & 0xc7) == 0x05 || op == 0x68 || op == 0xa9) {
            imm = imm_osz(&ctx);
        } else if (op == 0xe8) {
            imm = 4;
        } else if (op == 0xc8) {
            imm = 3;
        } else if (op >= 0xa0 && op <= 0xa3) {
            imm = moffs_size(&ctx);
        } else if (op >= 0xe0 && op <= 0xe7) {
            imm = 1;
        } else if (op == 0x9a || op == 0xea) {
            imm = ctx.op66 ? 6 : 10;
        }

        if (has_modrm) {
            size_t modrm_off = len;
            uint8_t reg = modrm_reg(&ctx, modrm_off);
            len += modrm_tail_len(&ctx, modrm_off);

            switch (op) {
            case 0x69:
            case 0x81:
            case 0xc7:
                imm = imm_osz(&ctx);
                break;
            case 0x6b:
            case 0x80:
            case 0x82:
            case 0x83:
            case 0xc0:
            case 0xc1:
            case 0xc6:
                imm = 1;
                break;
            case 0xf6:
                imm = (reg == 0 || reg == 1) ? 1 : 0;
                break;
            case 0xf7:
                imm = (reg == 0 || reg == 1) ? imm_osz(&ctx) : 0;
                break;
            case 0xff:
                if (reg == 2 || reg == 3) {
                    /* FF /2 and FF /3 are indirect call exits. */
                    insn.kind = INSN_ICALL;
                } else if (reg == 4 || reg == 5) {
                    /* FF /4 and FF /5 are indirect jump exits. */
                    insn.kind = INSN_IJMP;
                }
                break;
            default:
                break;
            }
        }

        len += imm;
    }

    if (len <= off) {
        len = off + 1;
    }
    if (len > size) {
        len = size;
    }
    insn.len = len - off;
    return insn;
}

const char *cfg_insn_kind_name(InsnKind kind)
{
    switch (kind) {
    case INSN_CALL:
        return "call";
    case INSN_ICALL:
        return "icall";
    case INSN_JCC:
        return "jcc";
    case INSN_JMP:
        return "jmp";
    case INSN_IJMP:
        return "ijmp";
    case INSN_RET:
        return "ret";
    case INSN_SYSCALL:
        return "syscall";
    case INSN_STOP:
        return "stop";
    default:
        return "normal";
    }
}
