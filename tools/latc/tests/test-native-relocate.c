#include "lat-native-image.h"
#include "relocate.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    unsigned char image[sizeof(LatNativeImageHeaderV2) + 96 +
                        sizeof(LatNativeTbV1) +
                        7 * sizeof(LatNativeRelocationV1)] = {0};
    LatNativeImageHeaderV2 *header = (void *)image;
    uint32_t *instructions;
    LatNativeRelocationV1 *relocations;
    LatNativeCode code = {0};
    char error[128] = {0};

    header->code_offset = sizeof(*header);
    header->code_size = 96;
    header->tb_table_offset = header->code_offset + header->code_size;
    header->tb_count = 1;
    header->relocation_offset = header->tb_table_offset +
                                sizeof(LatNativeTbV1);
    header->relocation_count = 7;
    header->pc_map_offset = header->relocation_offset +
                            header->relocation_count *
                            sizeof(LatNativeRelocationV1);
    instructions = (void *)(image + header->code_offset);
    instructions[0] = 0x1400000c;
    instructions[1] = 0x0380018c;
    instructions[2] = 0x1600000c;
    instructions[3] = 0x1400000c;
    instructions[4] = 0x0380018c;
    instructions[5] = 0x1e00000c;
    instructions[6] = 0x4c000184;
    instructions[7] = 0x1e00000c;
    instructions[8] = 0x4c000180;
    instructions[9] = 0x50000000;
    instructions[10] = 0x1a00000c;
    instructions[11] = 0x0380018c;
    instructions[12] = 0x00000981;
    instructions[13] = 0x000009a0;
    instructions[14] = 0x1400000d;
    instructions[15] = 0x038001ad;
    instructions[16] = 0x1600000d;
    memset((unsigned char *)instructions + 80, 0xa5, 16);
    LatNativeTbV1 *tb = (void *)(image + header->tb_table_offset);
    tb->guest_pc = 0x402000;
    tb->code_offset = 0;
    relocations = (void *)(image + header->relocation_offset);
    relocations[0].code_offset = 0;
    relocations[0].kind = LAT_NATIVE_RELOC_RUNTIME_SYMBOL;
    relocations[0].target = LAT_NATIVE_SYMBOL_RAISE_SYSCALL;
    relocations[0].slots = 3;
    relocations[1].code_offset = 12;
    relocations[1].kind = LAT_NATIVE_RELOC_GUEST_ADDRESS;
    relocations[1].addend = 0x401000;
    relocations[1].slots = 2;
    relocations[2].code_offset = 20;
    relocations[2].kind = LAT_NATIVE_RELOC_TB_TARGET;
    relocations[2].addend = 0x402000;
    relocations[2].slots = 2;
    relocations[2].reserved = LAT_NATIVE_SYMBOL_EPILOGUE_RET_ID_0;
    relocations[3].code_offset = 28;
    relocations[3].kind = LAT_NATIVE_RELOC_TB_TARGET;
    relocations[3].addend = 0x403000;
    relocations[3].slots = 2;
    relocations[3].reserved = LAT_NATIVE_SYMBOL_EPILOGUE_RET_ID_0;
    relocations[4].code_offset = 36;
    relocations[4].kind = LAT_NATIVE_RELOC_TB_TARGET;
    relocations[4].addend = 0x402000;
    relocations[4].slots = 1;
    relocations[5].code_offset = 40;
    relocations[5].kind = LAT_NATIVE_RELOC_JRRA_TARGET;
    relocations[5].addend = 0x402000;
    relocations[5].slots = 4;
    relocations[6] = (LatNativeRelocationV1){
        .code_offset = 56, .kind = LAT_NATIVE_RELOC_IMAGE_DATA,
        .slots = 3, .addend = 80, .reserved = 16,
    };

    size_t image_size = header->pc_map_offset;
    if (lat_native_code_load(header, image, image_size, &code,
                             error, sizeof(error))) {
        fprintf(stderr, "native relocation failed: %s\n", error);
        return 1;
    }
    const uint32_t *loaded = code.address;
    int64_t data_pages = (int32_t)((loaded[14] >> 5) << 12) >> 12;
    uintptr_t data_target = ((uintptr_t)&loaded[14] & ~(uintptr_t)0xfff) +
                            data_pages * 4096;
    data_target |= (loaded[15] >> 10) & 0xfff;
    if (data_target != (uintptr_t)code.address + 80 ||
        loaded[16] != 0x03400000u ||
        memcmp((unsigned char *)code.address + 80,
               (unsigned char *)instructions + 80, 16)) {
        fprintf(stderr, "native data address or contents changed\n");
        lat_native_code_unload(&code);
        return 1;
    }
    uint32_t jrra_high = (loaded[10] >> 5) & 0xfffff;
    int64_t jrra_page_delta = (int32_t)(jrra_high << 12) >> 12;
    uintptr_t jrra_target = (((uintptr_t)&loaded[10] >> 12) +
                             jrra_page_delta) << 12;
    jrra_target |= (loaded[11] >> 10) & 0xfff;
    if (loaded[5] != 0x18000044u ||
        (loaded[6] & 0xfc000000u) != 0x50000000u ||
        (loaded[7] == instructions[7] && loaded[8] == instructions[8]) ||
        loaded[9] == instructions[9] ||
        jrra_target != (uintptr_t)code.address ||
        loaded[12] != instructions[12] || loaded[13] != instructions[13]) {
        fprintf(stderr, "native TB target was not relocated or preserved\n");
        lat_native_code_unload(&code);
        return 1;
    }
    lat_native_code_unload(&code);
    if (code.address || code.size) {
        return 1;
    }
    header->flags = LAT_NATIVE_IMAGE_X86_STATIC_EXEC;
    error[0] = '\0';
    if (lat_native_code_load(header, image, image_size, &code,
                             error, sizeof(error)) == 0 ||
        !strstr(error, "relocation")) {
        fprintf(stderr, "missing static TB target accepted: %s\n", error);
        lat_native_code_unload(&code);
        return 1;
    }
    puts("test-native-relocate: PASS");
    return 0;
}
