#ifndef CFG_DECODER_H
#define CFG_DECODER_H

/*
 * Lightweight x86-64 instruction decoder for CFG construction.
 *
 * The decoder is intentionally length-oriented. It recognizes control-flow
 * terminators and direct branch targets, and otherwise returns enough
 * instruction length information to keep block boundaries aligned.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    INSN_NORMAL,
    INSN_CALL,
    INSN_ICALL,
    INSN_JCC,
    INSN_JMP,
    INSN_IJMP,
    INSN_RET,
    INSN_SYSCALL,
    INSN_STOP,
} InsnKind;

typedef struct {
    /* Virtual address and offset within the function byte buffer. */
    uint64_t addr;
    size_t off;

    /* Decoded byte length. A caller may clamp zero-length malformed input. */
    size_t len;
    InsnKind kind;

    /* Present for direct jcc/jmp targets only. */
    bool has_target;
    uint64_t target;
} Insn;

Insn cfg_decode_insn(const uint8_t *buf, size_t size, uint64_t base,
                     size_t off);
const char *cfg_insn_kind_name(InsnKind kind);
bool cfg_is_legacy_prefix(uint8_t b);

#endif
