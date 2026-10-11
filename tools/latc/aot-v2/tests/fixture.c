#include "lat-aot-v2.h"

#include <elf.h>
#include <stdint.h>

#define BYTE_32(value) { \
    value, value, value, value, value, value, value, value, \
    value, value, value, value, value, value, value, value, \
    value, value, value, value, value, value, value, value, \
    value, value, value, value, value, value, value, value \
}

#define LAT_AOT_MAGIC_BYTES { 'L', 'A', 'T', 'A', 'O', 'T', '2', 0 }

#ifndef LAT_AOT_FIXTURE_DESCRIPTOR_SOURCE_BYTE
#define LAT_AOT_FIXTURE_DESCRIPTOR_SOURCE_BYTE 0x11
#endif

#ifndef LAT_AOT_FIXTURE_BAD_PC_MAP
#define LAT_AOT_FIXTURE_BAD_PC_MAP 0
#endif

#ifndef LAT_AOT_FIXTURE_STACK_DELTA
#define LAT_AOT_FIXTURE_STACK_DELTA 0
#endif

extern uint64_t lat_aot_fixture_entry(void);
extern const uint8_t lat_aot_fixture_text_end[];

typedef struct LatAotElfNoteFixtureV2 {
    Elf64_Nhdr header;
    char name[4];
    LatAotNoteV2 description;
} LatAotElfNoteFixtureV2;

__attribute__((section(".note.lat.aot"), aligned(4), used))
static const LatAotElfNoteFixtureV2 fixture_note = {
    .header = {
        .n_namesz = sizeof(LAT_AOT_V2_NOTE_NAME),
        .n_descsz = sizeof(LatAotNoteV2),
        .n_type = LAT_AOT_V2_NOTE_TYPE,
    },
    .name = LAT_AOT_V2_NOTE_NAME,
    .description = {
        .magic = LAT_AOT_MAGIC_BYTES,
        .abi_version = LAT_AOT_V2_ABI_VERSION,
        .struct_size = sizeof(LatAotNoteV2),
        .module_flags = LAT_AOT_MODULE_PARTIAL |
                        LAT_AOT_MODULE_READONLY_TEXT |
                        LAT_AOT_MODULE_PRECISE_PC_MAP |
                        LAT_AOT_MODULE_SYNTHETIC_FIXTURE,
        .required_features = LAT_AOT_V2_REQUIRED_BASE_FEATURES,
        .source_sha256 = BYTE_32(0x11),
        .codegen_id = BYTE_32(0x22),
        .tbset_digest = BYTE_32(0x33),
    },
};

__attribute__((section(".rodata.lat.map"), used))
static const LatAotPcMapV2 fixture_pc_maps[] = {
    {
        .guest_rva = 0x1000,
        .host_offset_begin = 0,
        .host_offset_end = LAT_AOT_FIXTURE_BAD_PC_MAP ? 0 : 28,
        .state_record_offset = LAT_AOT_FIXTURE_STACK_DELTA ?
            (uint32_t)(int32_t)-8 : 0,
        .flags = LAT_AOT_PC_MAP_DYNAMIC_STATE |
            (LAT_AOT_FIXTURE_STACK_DELTA ?
             LAT_AOT_PC_MAP_STACK_POINTER_DELTA : 0),
    },
};

__attribute__((section(".rodata.lat.tb"), used))
static const LatAotTbV2 fixture_tbs[] = {
    {
        .guest_rva = 0x1000,
        .host_offset = 0,
        .host_size = 28,
        .flags = LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL,
    },
};

__attribute__((visibility("default"), section(".data.rel.ro.lat.module"), used))
const LatAotModuleV2 lat_aot_module_v2 = {
    .magic = LAT_AOT_MAGIC_BYTES,
    .abi_version = LAT_AOT_V2_ABI_VERSION,
    .struct_size = sizeof(LatAotModuleV2),
    .module_flags = LAT_AOT_MODULE_PARTIAL |
                    LAT_AOT_MODULE_READONLY_TEXT |
                    LAT_AOT_MODULE_PRECISE_PC_MAP |
                    LAT_AOT_MODULE_SYNTHETIC_FIXTURE,
    .required_features = LAT_AOT_V2_REQUIRED_BASE_FEATURES,
    .source_sha256 = BYTE_32(LAT_AOT_FIXTURE_DESCRIPTOR_SOURCE_BYTE),
    .codegen_id = BYTE_32(0x22),
    .tbset_digest = BYTE_32(0x33),
    .text_begin = (const uint8_t *)lat_aot_fixture_entry,
    .text_end = lat_aot_fixture_text_end,
    .tb_begin = fixture_tbs,
    .tb_end = fixture_tbs + 1,
    .pc_map_begin = fixture_pc_maps,
    .pc_map_end = fixture_pc_maps + 1,
};
