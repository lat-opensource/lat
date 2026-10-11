#include "lat-native-image.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s OUTPUT\n", argv[0]);
        return 2;
    }
    unsigned char image[512] = {0};
    LatNativeImageHeaderV2 *header = (void *)image;
    memcpy(header->magic, LAT_NATIVE_IMAGE_MAGIC, 8);
    header->version = LAT_NATIVE_IMAGE_VERSION;
    header->header_size = sizeof(*header);
    header->flags = LAT_NATIVE_IMAGE_PIE | LAT_NATIVE_IMAGE_C_ABI_SMOKE |
                    LAT_NATIVE_IMAGE_NO_PRECISE_SIGNAL_MAP;
    header->guest_entry = 0x1000;
    header->preferred_guest_base = 0x1000;
    header->guest_image_offset = sizeof(*header);
    header->guest_image_size = 1;
    header->code_offset = header->guest_image_offset + 8;
    header->code_size = 8;
    header->tb_table_offset = header->code_offset + header->code_size;
    header->tb_count = 1;
    header->relocation_offset = header->tb_table_offset + sizeof(LatNativeTbV1);
    header->pc_map_offset = header->relocation_offset;
    strcpy(header->lat_build_id, "smoke-v1");
    uint32_t *code = (void *)(image + header->code_offset);
    code[0] = 0x0280a804; /* li.w $a0, 42 */
    code[1] = 0x4c000020; /* ret */
    LatNativeTbV1 *tb = (void *)(image + header->tb_table_offset);
    tb->guest_pc = header->guest_entry;
    tb->code_offset = 0;
    tb->code_size = 8;
    FILE *output = fopen(argv[1], "wb");
    if (!output || fwrite(image, header->pc_map_offset, 1, output) != 1 ||
        fclose(output)) {
        fprintf(stderr, "cannot write smoke image\n");
        return 1;
    }
    return 0;
}
