#ifndef LAT_AOT_V2_H
#define LAT_AOT_V2_H

#include <stddef.h>
#include <stdint.h>

#define LAT_AOT_V2_ABI_VERSION 2u
#define LAT_AOT_V2_NOTE_TYPE 0x4c415432u
#define LAT_AOT_V2_NOTE_NAME "LAT"
#define LAT_AOT_V2_DESCRIPTOR_SYMBOL "lat_aot_module_v2"
#define LAT_AOT_V2_MODULE_VERSION "LAT_AOT_MODULE_2.0"
#define LAT_AOT_V2_RUNTIME_SONAME "liblat-aot-runtime.so.2"
#define LAT_AOT_V2_RUNTIME_ABI_SYMBOL "lat_aot_runtime_abi_version"
#define LAT_AOT_V2_RUNTIME_SYSCALL_SYMBOL "lat_aot_runtime_raise_syscall"
#define LAT_AOT_V2_CONTEXT_GUEST_SLOT_LIMIT 256u
#define LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT 256u
#define LAT_AOT_V2_TWO_LEVEL_GUEST_ADDRESS_LIMIT \
    (LAT_AOT_V2_CONTEXT_GUEST_SLOT_LIMIT * LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT)
#define LAT_AOT_V2_GUEST_ADDRESS_LIMIT \
    (LAT_AOT_V2_TWO_LEVEL_GUEST_ADDRESS_LIMIT * \
     LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT)
#define LAT_AOT_V2_TBSET_RECORD_LIMIT 1048576u
#define LAT_AOT_RUNTIME_TARGET_COUNT 64u

enum LatAotFeatureV2 {
    LAT_AOT_FEATURE_LBT = 1u << 0,
    LAT_AOT_FEATURE_LSX = 1u << 1,
    LAT_AOT_FEATURE_LASX = 1u << 2,
};

#define LAT_AOT_V2_REQUIRED_BASE_FEATURES \
    (LAT_AOT_FEATURE_LBT | LAT_AOT_FEATURE_LSX)

enum LatAotModuleFlagV2 {
    LAT_AOT_MODULE_PARTIAL = 1u << 0,
    LAT_AOT_MODULE_READONLY_TEXT = 1u << 1,
    LAT_AOT_MODULE_PRECISE_PC_MAP = 1u << 2,
    /* FP slots point at 256-entry pages; guest slot reserved is page offset. */
    LAT_AOT_MODULE_TWO_LEVEL_GUEST_SLOTS = 1u << 3,
    /* FP slots point at pointer pages, which point at 256-address pages. */
    LAT_AOT_MODULE_THREE_LEVEL_GUEST_SLOTS = 1u << 4,
};

#define LAT_AOT_MODULE_SYNTHETIC_FIXTURE (UINT64_C(1) << 63)
#define LAT_AOT_MODULE_M1_TEST_ONLY (UINT64_C(1) << 62)

enum LatAotTbFlagV2 {
    LAT_AOT_TB_CODE64 = 1u << 0,
    LAT_AOT_TB_PARALLEL = 1u << 1,
};

enum LatAotPcMapFlagV2 {
    LAT_AOT_PC_MAP_DYNAMIC_STATE = 1u << 0,
    /* state_record_offset contains a signed 32-bit stack-pointer delta. */
    LAT_AOT_PC_MAP_STACK_POINTER_DELTA = 1u << 1,
};

typedef struct LatAotNoteV2 {
    uint8_t magic[8];
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t module_flags;
    uint64_t required_features;
    uint8_t source_sha256[32];
    uint8_t codegen_id[32];
    uint8_t tbset_digest[32];
} LatAotNoteV2;

typedef struct LatAotTbV2 {
    uint64_t guest_rva;
    uint64_t host_offset;
    uint32_t host_size;
    uint32_t flags;
} LatAotTbV2;

typedef struct LatAotPcMapV2 {
    uint64_t guest_rva;
    uint64_t host_offset_begin;
    uint64_t host_offset_end;
    uint32_t state_record_offset;
    uint32_t flags;
} LatAotPcMapV2;

typedef struct LatAotGuestSlotV2 {
    uint64_t guest_rva;
    int32_t fp_offset;
    uint32_t reserved;
} LatAotGuestSlotV2;

typedef struct LatAotModuleV2 {
    uint8_t magic[8];
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t module_flags;
    uint64_t required_features;
    uint8_t source_sha256[32];
    uint8_t codegen_id[32];
    uint8_t tbset_digest[32];
    const uint8_t *text_begin;
    const uint8_t *text_end;
    const LatAotTbV2 *tb_begin;
    const LatAotTbV2 *tb_end;
    const LatAotPcMapV2 *pc_map_begin;
    const LatAotPcMapV2 *pc_map_end;
    const LatAotGuestSlotV2 *guest_slot_begin;
    const LatAotGuestSlotV2 *guest_slot_end;
} LatAotModuleV2;

typedef struct LatAotExpectedV2 {
    uint8_t source_sha256[32];
    uint8_t codegen_id[32];
    uint64_t available_features;
} LatAotExpectedV2;

enum LatAotRuntimeTargetV2 {
    LAT_AOT_TARGET_EPILOGUE_RET_ID_1,
    LAT_AOT_TARGET_EPILOGUE_RET_ID_0,
    LAT_AOT_TARGET_JIRL_EPILOGUE_RET_ID_1,
    LAT_AOT_TARGET_JIRL_EPILOGUE_RET_ID_0,
    LAT_AOT_TARGET_EPILOGUE_RET_0,
    LAT_AOT_TARGET_UPDATE_MXCSR_STATUS,
    LAT_AOT_TARGET_FXSAVE,
    LAT_AOT_TARGET_FXRSTOR,
    LAT_AOT_TARGET_FPREGS_X80_TO_64,
    LAT_AOT_TARGET_FPREGS_64_TO_X80,
    LAT_AOT_TARGET_UPDATE_FP_STATUS,
    LAT_AOT_TARGET_CPUID,
    LAT_AOT_TARGET_RAISE_ILLOP,
    LAT_AOT_TARGET_RAISE_GPF,
    LAT_AOT_TARGET_PCMPISTRI_XMM,
    LAT_AOT_TARGET_PCMPISTRM_XMM,
    LAT_AOT_TARGET_EFLAGTF,
    LAT_AOT_TARGET_LOG2,
    LAT_AOT_TARGET_POW,
    LAT_AOT_TARGET_SIN,
    LAT_AOT_TARGET_COS,
    LAT_AOT_TARGET_ATAN2,
    LAT_AOT_TARGET_LOGB,
    LAT_AOT_TARGET_SINCOS,
    LAT_AOT_TARGET_FPATAN,
    LAT_AOT_TARGET_FPTAN,
    LAT_AOT_TARGET_FPREM,
    LAT_AOT_TARGET_FPREM1,
    LAT_AOT_TARGET_FRNDINT,
    LAT_AOT_TARGET_F2XM1,
    LAT_AOT_TARGET_FXTRACT,
    LAT_AOT_TARGET_FYL2X,
    LAT_AOT_TARGET_FYL2XP1,
    LAT_AOT_TARGET_FSINCOS,
    LAT_AOT_TARGET_FSIN,
    LAT_AOT_TARGET_FCOS,
    LAT_AOT_TARGET_FBLD_ST0,
    LAT_AOT_TARGET_FBST_ST0,
    LAT_AOT_TARGET_AESIMC_XMM,
    LAT_AOT_TARGET_AESKEYGENASSIST_XMM,
    LAT_AOT_TARGET_AESDEC_XMM,
    LAT_AOT_TARGET_AESDECLAST_XMM,
    LAT_AOT_TARGET_AESENC_XMM,
    LAT_AOT_TARGET_AESENCLAST_XMM,
    LAT_AOT_TARGET_SHA1NEXTE,
    LAT_AOT_TARGET_SHA1MSG1,
    LAT_AOT_TARGET_SHA1MSG2,
    LAT_AOT_TARGET_SHA256MSG1,
    LAT_AOT_TARGET_SHA256MSG2,
    LAT_AOT_TARGET_SHA1RNDS4_F0,
    LAT_AOT_TARGET_SHA1RNDS4_F1,
    LAT_AOT_TARGET_SHA1RNDS4_F2,
    LAT_AOT_TARGET_SHA1RNDS4_F3,
    LAT_AOT_TARGET_SHA256RNDS2_XMM0,
    LAT_AOT_TARGET_RAISE_INT,
    LAT_AOT_TARGET_RAISE_TRAPOP,
    LAT_AOT_TARGET_RAISE_INTO,
    LAT_AOT_TARGET_RAISE_BOUND,
    LAT_AOT_TARGET_XGETBV,
    LAT_AOT_TARGET_KZT_GET_ALTERNATE,
    LAT_AOT_TARGET_XSAVE,
    LAT_AOT_TARGET_XSAVEOPT,
    LAT_AOT_TARGET_XRSTOR,
    LAT_AOT_TARGET_XSETBV,
};

typedef struct LatAotRuntimeTargetsV2 {
    uint32_t struct_size;
    uint32_t reserved;
    uintptr_t target[LAT_AOT_RUNTIME_TARGET_COUNT];
} LatAotRuntimeTargetsV2;

uint32_t lat_aot_runtime_abi_version(void);
const char *lat_aot_runtime_build_id(void);
typedef void (*LatAotRuntimeSyscallCallbackV2)(void *opaque);
int lat_aot_runtime_bind_syscall(LatAotRuntimeSyscallCallbackV2 callback,
                                 void *opaque);
__attribute__((noreturn)) void lat_aot_runtime_raise_syscall(void);
int lat_aot_runtime_bind_targets(const LatAotRuntimeTargetsV2 *targets);

static inline int lat_aot_v2_magic_valid(const uint8_t magic[8])
{
    static const uint8_t expected[8] = {
        'L', 'A', 'T', 'A', 'O', 'T', '2', 0,
    };
    for (size_t i = 0; i < sizeof(expected); i++) {
        if (magic[i] != expected[i]) {
            return 0;
        }
    }
    return 1;
}

_Static_assert(sizeof(LatAotNoteV2) == 128,
               "AOT v2 note ABI size changed");
_Static_assert(sizeof(LatAotTbV2) == 24,
               "AOT v2 TB ABI size changed");
_Static_assert(sizeof(LatAotPcMapV2) == 32,
               "AOT v2 PC map ABI size changed");
_Static_assert(sizeof(LatAotGuestSlotV2) == 16,
               "AOT v2 guest slot ABI size changed");
_Static_assert(sizeof(LatAotModuleV2) == 192,
               "AOT v2 module ABI size changed");
_Static_assert(sizeof(LatAotRuntimeTargetsV2) == 520,
               "AOT v2 runtime target ABI size changed");

#endif
