/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "qemu/osdep.h"
#include "capstone_git.h"
#include "target/i386/latx/ir1/ir1-decode.h"

csh handle[2];

static unsigned int checked;
static unsigned int failures;

static void mismatch(const uint8_t *code, const char *field,
                     int64_t actual, int64_t expected)
{
    static unsigned int shown[256];
    unsigned int opcode =
        code[0] >= 0x40 && code[0] <= 0x4f ? code[1] : code[0];
    failures++;
    if (shown[opcode]++ < 3) {
        fprintf(stderr, "%s: got 0x%" PRIx64 " expected 0x%" PRIx64 " bytes:",
                field, (uint64_t)actual, (uint64_t)expected);
        for (int i = 0; i < 15; i++) {
            fprintf(stderr, " %02x", code[i]);
        }
        fprintf(stderr, "\n");
    }
}

#define SAME(field) do { \
    if (fast.field != full->field) { \
        mismatch(code, #field, fast.field, full->field); \
    } \
} while (0)

static void compare(const uint8_t *code, uint64_t address)
{
    struct la_dt_insn fast = { 0 }, decoded = { 0 }, *full = NULL;
    if (!ir1_hot_decode(code, address, 0, &fast, 1)) {
        return;
    }
    checked++;
    int count = gitcapstone_get(code, 15, address, 1, &full, 0, &decoded, 1);
    if (count != 1 || !full) {
        mismatch(code, "valid instruction", 1, 0);
        return;
    }
    SAME(id);
    SAME(size);
    SAME(address);
    SAME(x86.addr_size);
    SAME(x86.rex);
    SAME(x86.modrm);
    SAME(x86.op_count);
    SAME(x86.avx_cc);
    for (int i = 0; i < 4; i++) {
        SAME(x86.prefix[i]);
        SAME(x86.opcode[i]);
    }
    for (int i = 0; i < fast.size; i++) {
        SAME(bytes[i]);
    }
    for (int i = 0; i < fast.x86.op_count; i++) {
        SAME(x86.operands[i].type);
        SAME(x86.operands[i].size);
        SAME(x86.operands[i].access);
        SAME(x86.operands[i].avx_bcast);
        SAME(x86.operands[i].avx_zero_opmask);
        switch (fast.x86.operands[i].type) {
        case dt_X86_OP_REG:
            SAME(x86.operands[i].reg);
            break;
        case dt_X86_OP_IMM:
            SAME(x86.operands[i].imm);
            break;
        case dt_X86_OP_MEM:
            SAME(x86.operands[i].mem.segment);
            SAME(x86.operands[i].mem.default_segment);
            SAME(x86.operands[i].mem.base);
            SAME(x86.operands[i].mem.index);
            SAME(x86.operands[i].mem.scale);
            SAME(x86.operands[i].mem.disp);
            break;
        default:
            g_assert_not_reached();
        }
    }
}

static void test_cache(void)
{
    uint8_t code[15] = { 0x0f, 0xaf, 0xc1 }; /* imul eax, ecx */
    struct la_dt_insn decoded = { 0 }, output[2] = { 0 }, *full;
    g_assert_cmpint(gitcapstone_get(code, 15, 0x1000, 1, &full,
                                   0, &decoded, 1), ==, 1);
    ir1_decode_template_insert(code, 15, 1, full);
    g_assert_true(ir1_decode_template_lookup(code, 15, 0x2000, 1,
                                             output, 1) == &output[1]);
    decoded.address = 0x2000;
    g_assert_cmpmem(&decoded, sizeof(decoded), &output[1], sizeof(output[1]));
    code[3] = 0xff; /* bytes beyond the instruction must not affect the key */
    g_assert_nonnull(ir1_decode_template_lookup(
        code, 15, 0x3000, 0, output, 1));
    g_assert_null(ir1_decode_template_lookup(code, 2, 0x3000, 0, output, 1));
    g_assert_null(ir1_decode_template_lookup(code, 15, 0x3000, 0, output, 0));
    g_assert_null(ir1_decode_template_lookup(code, 15, 0x3000, 0, NULL, 1));
    code[2] ^= 1;
    g_assert_null(ir1_decode_template_lookup(code, 15, 0x3000, 0, output, 1));
    decoded.x86.operands[0].type = dt_X86_OP_IMM;
    g_assert_false(ir1_decode_template_is_address_independent(&decoded));
    decoded.x86.operands[0].type = dt_X86_OP_MEM;
    decoded.x86.operands[0].mem.base = dt_X86_REG_RIP;
    g_assert_false(ir1_decode_template_is_address_independent(&decoded));
}

static void test_modes(void)
{
    /* The same bytes are DEC EAX in 32-bit mode and IMUL RAX, RCX in 64-bit. */
    uint8_t code[15] = { 0x48, 0x0f, 0xaf, 0xc1 };
    struct la_dt_insn decoded[2] = { 0 }, output = { 0 }, *full;
    for (int mode = 0; mode < 2; mode++) {
        g_assert_cmpint(gitcapstone_get(code, 15, 0x1000, 1, &full,
            0, &decoded[mode], mode), ==, 1);
        ir1_decode_template_insert(code, 15, mode, full);
    }
    g_assert_cmpuint(decoded[0].size, ==, 1);
    g_assert_cmpuint(decoded[1].size, ==, 4);
    for (int mode = 0; mode < 2; mode++) {
        g_assert_nonnull(ir1_decode_template_lookup(code, 15, 0x2000,
            0, &output, mode));
        decoded[mode].address = 0x2000;
        g_assert_cmpmem(&decoded[mode], sizeof(output), &output, sizeof(output));
    }
}

static void test_collision(void)
{
    uint8_t a[15] = { 0x0f, 0x1f, 0x84, 0, 0x11, 0x22, 0x33, 0x44 };
    uint8_t b[15];
    struct la_dt_insn decoded = { 0 }, output = { 0 }, *full;
    memcpy(b, a, sizeof(b));
    b[7] ^= 1;
    g_assert_cmpuint(ir1_decode_template_hash(a, 4, 1), ==,
                     ir1_decode_template_hash(b, 4, 1));
    g_assert_cmpint(gitcapstone_get(a, 15, 0, 1, &full,
                                   0, &decoded, 1), ==, 1);
    ir1_decode_template_insert(a, 15, 1, full);
    g_assert_cmpint(gitcapstone_get(b, 15, 0, 1, &full,
                                   0, &decoded, 1), ==, 1);
    ir1_decode_template_insert(b, 15, 1, full);
    g_assert_null(ir1_decode_template_lookup(a, 15, 0, 0, &output, 1));
    g_assert_nonnull(ir1_decode_template_lookup(b, 15, 0, 0, &output, 1));
    g_assert_cmpmem(&decoded, sizeof(decoded), &output, sizeof(output));
}

static gpointer cache_worker(gpointer opaque)
{
    const struct la_dt_insn *decoded = opaque;
    struct la_dt_insn output = { 0 };
    g_assert_null(ir1_decode_templates);
    g_assert_null(ir1_decode_template_valid);
    ir1_decode_template_insert(decoded->bytes, 15, 1, decoded);
    g_assert_nonnull(ir1_decode_template_lookup(decoded->bytes, 15,
        decoded->address, 0, &output, 1));
    g_assert_cmpmem(decoded, sizeof(*decoded), &output, sizeof(output));
    /* Both GPrivate owners release their storage when the thread exits. */
    return NULL;
}

static void test_threads(void)
{
    uint8_t code[15] = { 0x0f, 0xaf, 0xc1 };
    struct la_dt_insn decoded = { 0 }, *full;
    g_assert_cmpint(gitcapstone_get(code, 15, 0x1000, 1, &full,
                                   0, &decoded, 1), ==, 1);
    for (int i = 0; i < 32; i++) {
        GThread *worker = g_thread_new("decode-cache", cache_worker, &decoded);
        g_thread_join(worker);
    }
}

int main(void)
{
    gitcapstone_init(64);
    uint8_t code[15];
    /* Every opcode/ModRM/REX combination, with both displacement signs. */
    for (int r = -1; r < 16; r++) {
        for (int op = 0; op < 256; op++) {
            for (int modrm = 0; modrm < 256; modrm++) {
                memset(code, modrm & 1 ? 0x81 : 0x7e, sizeof(code));
                int n = 0;
                if (r >= 0) {
                    code[n++] = 0x40 + r;
                }
                code[n++] = op;
                code[n] = modrm;
                compare(code, UINT64_C(0x7fff12345678));
            }
        }
    }
    /* Exercise each SIB byte and 32/64-bit register extension combination. */
    for (int r = 0; r < 16; r++) {
        for (int mod = 0; mod < 3; mod++) {
            for (int sib = 0; sib < 256; sib++) {
                memset(code, 0x80, sizeof(code));
                code[0] = 0x40 + r;
                code[1] = 0x8b;
                code[2] = (mod << 6) | 4;
                code[3] = sib;
                compare(code, UINT64_MAX - 4);
            }
        }
    }
    /* Deterministic mixed prefixes, immediates and displacements. */
    uint32_t random = 0x12345678;
    for (int sample = 0; sample < 500000; sample++) {
        for (int i = 0; i < 15; i++) {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            code[i] = random;
        }
        compare(code, sample & 1 ? 0 : UINT64_MAX - 4);
    }
    memset(code, 0x90, sizeof(code));
    struct la_dt_insn output;
    g_assert_null(ir1_hot_decode(code, 0, 0, &output, 0));
    g_assert_null(ir1_hot_decode(code, 0, 0, NULL, 1));
    test_cache();
    test_collision();
    test_modes();
    test_threads();
    g_assert_cmpuint(checked, >, 250000);
    printf("fast decoder checked=%u field mismatches=%u\n", checked, failures);
    return failures ? 1 : 0;
}
