#ifndef LATC_LAT_NATIVE_IMAGE_H
#define LATC_LAT_NATIVE_IMAGE_H

#include <stdint.h>
#include <string.h>

#define LAT_NATIVE_IMAGE_MAGIC "LATNAT2"
#define LAT_NATIVE_IMAGE_VERSION 8u
#define LAT_NATIVE_BUILD_ID_SIZE 65u

#define LAT_NATIVE_INDIRECT_EXIT_WORDS 38u
#define LAT_NATIVE_AOT_V2_INDIRECT_EXIT_WORDS 27u

static inline int lat_native_decode_local_branch_target(
    const uint32_t *words, uint32_t slots, uint64_t code_offset,
    uint64_t code_size, uint64_t *target)
{
    if (!words || !target || (slots != 1 && slots != 2)) {
        return 0;
    }

    int64_t offset;
    if (slots == 2) {
        if ((words[0] & 0xfe000000u) != 0x1e000000u ||
            (words[1] & 0xfc000000u) != 0x4c000000u) {
            return 0;
        }
        int64_t upper = (int32_t)(words[0] >> 5) & 0xfffff;
        if (upper & (1 << 19)) {
            upper -= 1 << 20;
        }
        int64_t lower = (int16_t)(words[1] >> 10);
        offset = (upper << 18) + lower * 4;
    } else {
        uint32_t word = words[0];
        uint32_t opcode = word & 0xfc000000u;
        uint32_t opcode20 = word & 0xfc000100u;
        if (opcode == 0x50000000u || opcode == 0x54000000u) {
            uint32_t raw = ((word >> 10) & 0xffffu) |
                           (((word >> 16) & 0x3ffu) << 16);
            offset = (int64_t)((int32_t)(raw << 6) >> 6) * 4;
        } else if (opcode == 0x40000000u || opcode == 0x44000000u ||
                   opcode20 == 0x48000000u || opcode20 == 0x48000100u) {
            uint32_t raw = ((word >> 10) & 0xffffu) |
                           (((word >> 16) & 0x1fu) << 16);
            offset = (int64_t)((int32_t)(raw << 12) >> 12) * 4;
        } else if (opcode >= 0x58000000u && opcode <= 0x6c000000u) {
            offset = (int64_t)(int16_t)(word >> 10) * 4;
        } else {
            return 0;
        }
    }

    if (offset < 0) {
        uint64_t magnitude = (uint64_t)(-offset);
        if (magnitude > code_offset) {
            return 0;
        }
        *target = code_offset - magnitude;
    } else {
        uint64_t magnitude = (uint64_t)offset;
        if (magnitude > code_size || code_offset > code_size - magnitude) {
            return 0;
        }
        *target = code_offset + magnitude;
    }
    return 1;
}

/* The ordinary FAST_JMPCACHE dispatch window, excluding its runtime exit. */
static inline int lat_native_indirect_exit_valid(const void *code)
{
    static const uint32_t expected[LAT_NATIVE_INDIRECT_EXIT_WORDS] = {
        0x004542abu, 0x0015aeabu, 0x00cf016bu, 0x002dd96bu,
        0x28c0016cu, 0x5c000d95u, 0x28c0216bu, 0x4c000160u,
        0x28c003edu, 0x580075a0u, 0x004542aeu, 0x0015baaeu,
        0x00cf01ceu, 0x004119ceu, 0x0010b5ceu, 0x28c001ccu,
        0x5c005995u, 0x28c061ccu, 0x58005180u, 0x28c0018bu,
        0x28c081ccu, 0x5c00458bu, 0x28c041ccu, 0x28c003ebu,
        0x5800318bu, 0x28c0a1cdu, 0x28c0c1cbu, 0x58002160u,
        0x001502cfu, 0x02ffe1adu, 0x02ffe1efu, 0x28c001b0u,
        0x29c001f0u, 0x02fffd6bu, 0x5fffed60u, 0x29c003ecu,
        0x28c021cbu, 0x4c000160u,
    };
    static const uint8_t register_fields[LAT_NATIVE_INDIRECT_EXIT_WORDS] = {
        2, 3, 2, 3, 2, 2, 2, 2, 2, 2, 2, 3, 2, 2, 3, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    };
    int8_t scratch_map[5] = {-1, -1, -1, -1, -1};
    uint32_t mapped_registers = 0;

    for (unsigned int i = 0; i < LAT_NATIVE_INDIRECT_EXIT_WORDS; i++) {
        uint32_t word;
        memcpy(&word, (const uint8_t *)code + i * 4, 4);
        uint32_t mask = i == 8 || i == 23 || i == 35 ?
            0xffc003ffu : UINT32_MAX;
        uint32_t register_mask = register_fields[i] == 3 ? 0x7fffu : 0x3ffu;
        if ((word & mask & ~register_mask) !=
            (expected[i] & mask & ~register_mask)) {
            return 0;
        }
        for (unsigned int field = 0; field < register_fields[i]; field++) {
            unsigned int shift = field * 5;
            unsigned int expected_register = (expected[i] >> shift) & 31;
            unsigned int actual_register = (word >> shift) & 31;
            if (expected_register >= 12 && expected_register <= 16) {
                unsigned int index = expected_register - 12;
                if (scratch_map[index] < 0) {
                    uint32_t fixed = (1u << 0) | (1u << 11) | (1u << 21) |
                                     (1u << 22) | (1u << 27);
                    if ((fixed | mapped_registers) & (1u << actual_register)) {
                        return 0;
                    }
                    scratch_map[index] = actual_register;
                    mapped_registers |= 1u << actual_register;
                } else if ((unsigned int)scratch_map[index] != actual_register) {
                    return 0;
                }
            } else if (expected_register != actual_register) {
                return 0;
            }
        }
    }
    return 1;
}

/* Match the current AOT v2 fast-cache window without reading its runtime exit. */
static inline int lat_native_aot_v2_indirect_exit_valid(const void *code)
{
    static const uint32_t expected[2][LAT_NATIVE_AOT_V2_INDIRECT_EXIT_WORDS] = {
        {
            0x004542ab, 0x0015aeab, 0x00cf016b, 0x002dd96b,
            0x28c0016c, 0x5c000d95, 0x28c0216b, 0x4c000160,
            0x28c543ed, 0x580049a0, 0x004542ae, 0x0015baae,
            0x00cf01ce, 0x004119ce, 0x0010b5ce, 0x28c001cc,
            0x5c002d95, 0x28c061cc, 0x58002580, 0x28c0018b,
            0x28c081cc, 0x5c00198b, 0x28c041cc, 0x28c563eb,
            0x5c000d8b, 0x28c021cb, 0x4c000160,
        },
        {
            0x004542ab, 0x0015aeab, 0x00cf016b, 0x002dd96b,
            0x28c0016d, 0x5c000db5, 0x28c0216b, 0x4c000160,
            0x28c543ee, 0x580049c0, 0x004542af, 0x0015beaf,
            0x00cf01ef, 0x004119ef, 0x0010b9ef, 0x28c001ed,
            0x5c002db5, 0x28c061ed, 0x580025a0, 0x28c001ab,
            0x28c081ed, 0x5c0019ab, 0x28c041ed, 0x28c563eb,
            0x5c000dab, 0x28c021eb, 0x4c000160,
        },
    };
    for (unsigned int variant = 0; variant < 2; variant++) {
        if (!memcmp(code, expected[variant], sizeof(expected[variant]))) {
            return 1;
        }
    }
    return 0;
}

static inline unsigned int lat_native_indirect_exit_words(const void *code,
                                                            size_t available)
{
    if (available >= LAT_NATIVE_INDIRECT_EXIT_WORDS * 4 &&
        lat_native_indirect_exit_valid(code)) {
        return LAT_NATIVE_INDIRECT_EXIT_WORDS;
    }
    if (available >= LAT_NATIVE_AOT_V2_INDIRECT_EXIT_WORDS * 4 &&
        lat_native_aot_v2_indirect_exit_valid(code)) {
        return LAT_NATIVE_AOT_V2_INDIRECT_EXIT_WORDS;
    }
    return 0;
}

enum LatNativeImageFlags {
    LAT_NATIVE_IMAGE_PIE = 1u << 0,
    LAT_NATIVE_IMAGE_NEEDS_FALLBACK = 1u << 1,
    LAT_NATIVE_IMAGE_LBT = 1u << 2,
    LAT_NATIVE_IMAGE_LSX = 1u << 3,
    LAT_NATIVE_IMAGE_NO_PRECISE_SIGNAL_MAP = 1u << 4,
    LAT_NATIVE_IMAGE_CROSS_MODULE_TARGETS = 1u << 5,
    LAT_NATIVE_IMAGE_X86_STATIC_EXEC = 1u << 28,
    LAT_NATIVE_IMAGE_X86_EXIT_SMOKE = LAT_NATIVE_IMAGE_X86_STATIC_EXEC,
    LAT_NATIVE_IMAGE_C_ABI_DISPATCH_SMOKE = 1u << 29,
    LAT_NATIVE_IMAGE_C_ABI_STATE_SMOKE = 1u << 30,
    LAT_NATIVE_IMAGE_C_ABI_SMOKE = 1u << 31,
};

enum LatNativePcMapFlagV2 {
    LAT_NATIVE_PC_MAP_DYNAMIC_STATE = 1u << 0,
    /* state_record_offset contains a signed 32-bit stack-pointer delta. */
    LAT_NATIVE_PC_MAP_STACK_POINTER_DELTA = 1u << 1,
};

enum LatNativeRelocationKind {
    LAT_NATIVE_RELOC_RUNTIME_SYMBOL = 1,
    LAT_NATIVE_RELOC_TB_TARGET = 2,
    LAT_NATIVE_RELOC_GUEST_ADDRESS = 3,
    LAT_NATIVE_RELOC_JRRA_TARGET = 4,
    /* addend is a code-section data offset; reserved is its byte size. */
    LAT_NATIVE_RELOC_IMAGE_DATA = 5,
};

enum LatNativeRuntimeSymbolV1 {
    LAT_NATIVE_SYMBOL_INVALID = 0,
    LAT_NATIVE_SYMBOL_EPILOGUE_RET_ID_1,
    LAT_NATIVE_SYMBOL_EPILOGUE_RET_ID_0,
    LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_1,
    LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_0,
    LAT_NATIVE_SYMBOL_EPILOGUE_RET_0,
    LAT_NATIVE_SYMBOL_UPDATE_MXCSR_STATUS,
    LAT_NATIVE_SYMBOL_FXSAVE,
    LAT_NATIVE_SYMBOL_FXRSTOR,
    LAT_NATIVE_SYMBOL_FPREGS_X80_TO_64,
    LAT_NATIVE_SYMBOL_FPREGS_64_TO_X80,
    LAT_NATIVE_SYMBOL_UPDATE_FP_STATUS,
    LAT_NATIVE_SYMBOL_CPUID,
    LAT_NATIVE_SYMBOL_RAISE_ILLOP,
    LAT_NATIVE_SYMBOL_RAISE_GPF,
    LAT_NATIVE_SYMBOL_RAISE_SYSCALL,
    LAT_NATIVE_SYMBOL_PFTABLE,
    LAT_NATIVE_SYMBOL_PCMPISTRI_XMM,
    LAT_NATIVE_SYMBOL_PCMPISTRM_XMM,
    LAT_NATIVE_SYMBOL_EFLAGTF,
    LAT_NATIVE_SYMBOL_LOG2,
    LAT_NATIVE_SYMBOL_POW,
    LAT_NATIVE_SYMBOL_SIN,
    LAT_NATIVE_SYMBOL_COS,
    LAT_NATIVE_SYMBOL_ATAN2,
    LAT_NATIVE_SYMBOL_LOGB,
    LAT_NATIVE_SYMBOL_SINCOS,
    LAT_NATIVE_SYMBOL_FPATAN,
    LAT_NATIVE_SYMBOL_FPTAN,
    LAT_NATIVE_SYMBOL_FPREM,
    LAT_NATIVE_SYMBOL_FPREM1,
    LAT_NATIVE_SYMBOL_FRNDINT,
    LAT_NATIVE_SYMBOL_F2XM1,
    LAT_NATIVE_SYMBOL_FXTRACT,
    LAT_NATIVE_SYMBOL_FYL2X,
    LAT_NATIVE_SYMBOL_FYL2XP1,
    LAT_NATIVE_SYMBOL_FSINCOS,
    LAT_NATIVE_SYMBOL_FSIN,
    LAT_NATIVE_SYMBOL_FCOS,
    LAT_NATIVE_SYMBOL_FBLD_ST0,
    LAT_NATIVE_SYMBOL_FBST_ST0,
    LAT_NATIVE_SYMBOL_AESIMC_XMM,
    LAT_NATIVE_SYMBOL_AESKEYGENASSIST_XMM,
    LAT_NATIVE_SYMBOL_AESDEC_XMM,
    LAT_NATIVE_SYMBOL_AESDECLAST_XMM,
    LAT_NATIVE_SYMBOL_AESENC_XMM,
    LAT_NATIVE_SYMBOL_AESENCLAST_XMM,
    LAT_NATIVE_SYMBOL_SHA1NEXTE,
    LAT_NATIVE_SYMBOL_SHA1MSG1,
    LAT_NATIVE_SYMBOL_SHA1MSG2,
    LAT_NATIVE_SYMBOL_SHA256MSG1,
    LAT_NATIVE_SYMBOL_SHA256MSG2,
    LAT_NATIVE_SYMBOL_SHA1RNDS4_F0,
    LAT_NATIVE_SYMBOL_SHA1RNDS4_F1,
    LAT_NATIVE_SYMBOL_SHA1RNDS4_F2,
    LAT_NATIVE_SYMBOL_SHA1RNDS4_F3,
    LAT_NATIVE_SYMBOL_SHA256RNDS2_XMM0,
    LAT_NATIVE_SYMBOL_RAISE_INT,
    LAT_NATIVE_SYMBOL_RAISE_TRAPOP,
    LAT_NATIVE_SYMBOL_RAISE_INTO,
    LAT_NATIVE_SYMBOL_RAISE_BOUND,
    LAT_NATIVE_SYMBOL_XGETBV,
    LAT_NATIVE_SYMBOL_KZT_GET_ALTERNATE,
    LAT_NATIVE_SYMBOL_XSAVE,
    LAT_NATIVE_SYMBOL_XSAVEOPT,
    LAT_NATIVE_SYMBOL_XRSTOR,
    LAT_NATIVE_SYMBOL_XSETBV,
    LAT_NATIVE_SYMBOL_COUNT,
};

typedef struct LatNativeImageHeaderV2 {
    uint8_t magic[8];
    uint32_t version;
    uint32_t header_size;
    uint32_t flags;
    uint32_t reserved;
    uint64_t guest_entry;
    uint64_t preferred_guest_base;
    uint64_t guest_image_offset;
    uint64_t guest_image_size;
    uint64_t code_offset;
    uint64_t code_size;
    uint64_t tb_table_offset;
    uint64_t tb_count;
    uint64_t relocation_offset;
    uint64_t relocation_count;
    uint64_t pc_map_offset;
    uint64_t pc_map_count;
    uint64_t tu_table_offset;
    uint64_t tu_count;
    uint8_t guest_sha256[32];
    char lat_build_id[LAT_NATIVE_BUILD_ID_SIZE];
    uint8_t reserved_tail[7];
} LatNativeImageHeaderV2;

typedef struct LatNativeTbV1 {
    uint64_t guest_pc;
    uint64_t code_offset;
    uint32_t code_size;
    uint32_t flags;
    uint32_t optimization_flags;
    /* TB-relative instruction offset plus one; zero means unavailable. */
    uint16_t eflags_offset[2];
    uint32_t eflags_instruction;
    uint16_t eflags_stub_offset[2];
    /* TB-relative BEQ/BNE/BLT/BGE/BLTU/BGEU offset plus one, or zero. */
    uint32_t conditional_exit_offset;
    /* TB-relative ordinary indirect dispatch offset plus one, or zero. */
    uint32_t indirect_exit_offset;
    /* Index into the physical translation-unit table. */
    uint32_t tu_index;
} LatNativeTbV1;

enum LatNativeTbOptimizationFlags {
    LAT_NATIVE_TB_ENTRY_FLAGS_DEAD = 1u << 0,
};

typedef struct LatNativeRelocationV1 {
    uint64_t code_offset;
    int64_t addend;
    uint32_t kind;
    uint32_t target;
    uint32_t slots;
    uint32_t reserved;
    /* Physical TB index plus one for a local TB/JRRA edge; zero is none. */
    uint32_t physical_target_plus_one;
} LatNativeRelocationV1;

typedef struct LatNativePcMapV2 {
    uint64_t guest_pc;
    uint64_t host_offset_begin;
    uint64_t host_offset_end;
    uint32_t state_record_offset;
    uint32_t flags;
} LatNativePcMapV2;

typedef struct LatNativeTuV1 {
    uint64_t code_offset;
    uint64_t code_size;
    uint64_t search_offset;
    uint32_t flags;
    uint32_t reserved;
} LatNativeTuV1;

#endif
