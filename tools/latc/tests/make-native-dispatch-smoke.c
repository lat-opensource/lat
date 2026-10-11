#include "lat-native-image.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s OUTPUT\n", argv[0]);
        return 2;
    }
    static const uint32_t code[] = {
        0x0015008c, 0x2600018d, 0x1400006e, 0x038041ce,
        0x02c01dad, 0x00150004, 0x29c2018e, 0x2700018d,
        0x4c000020,
        0x0015008c, 0x2600018d, 0x00150004, 0x29c20180,
        0x004105ad, 0x2700018d, 0x4c000020,
    };
    unsigned char image[512] = {0};
    LatNativeImageHeaderV2 *header = (void *)image;
    memcpy(header->magic, LAT_NATIVE_IMAGE_MAGIC, 8);
    header->version = LAT_NATIVE_IMAGE_VERSION;
    header->header_size = sizeof(*header);
    header->flags = LAT_NATIVE_IMAGE_PIE |
                    LAT_NATIVE_IMAGE_C_ABI_DISPATCH_SMOKE |
                    LAT_NATIVE_IMAGE_NO_PRECISE_SIGNAL_MAP;
    header->guest_entry = 0x3000;
    header->preferred_guest_base = 0x3000;
    header->guest_image_offset = sizeof(*header);
    header->guest_image_size = 1;
    header->code_offset = header->guest_image_offset + 8;
    header->code_size = sizeof(code);
    header->tb_table_offset = header->code_offset + sizeof(code);
    header->tb_count = 2;
    header->relocation_offset = header->tb_table_offset +
                                2 * sizeof(LatNativeTbV1);
    header->pc_map_offset = header->relocation_offset;
    strcpy(header->lat_build_id, "dispatch-smoke-v1");
    memcpy(image + header->code_offset, code, sizeof(code));
    LatNativeTbV1 *tbs = (void *)(image + header->tb_table_offset);
    tbs[0] = (LatNativeTbV1){
        .guest_pc = 0x3000, .code_offset = 0, .code_size = 9 * 4,
    };
    tbs[1] = (LatNativeTbV1){
        .guest_pc = 0x3010, .code_offset = 9 * 4, .code_size = 7 * 4,
    };
    FILE *output = fopen(argv[1], "wb");
    if (!output || fwrite(image, header->pc_map_offset, 1, output) != 1 ||
        fclose(output)) {
        fprintf(stderr, "cannot write dispatch smoke image\n");
        return 1;
    }
    return 0;
}
