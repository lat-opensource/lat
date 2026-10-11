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
        0x2600008c, /* ldptr.d $t0, $a0, 0 */
        0x1400002d, /* lu12i.w $t1, 1 */
        0x0388d1ad, /* ori $t1, $t1, 0x234 */
        0x02c01d8c, /* addi.d $t0, $t0, 7 */
        0x29c2008d, /* st.d $t1, $a0, 128 */
        0x2700008c, /* stptr.d $t0, $a0, 0 */
        0x00408184, /* slli.w $a0, $t0, 0 */
        0x4c000020, /* ret */
    };
    unsigned char image[512] = {0};
    LatNativeImageHeaderV2 *header = (void *)image;
    memcpy(header->magic, LAT_NATIVE_IMAGE_MAGIC, 8);
    header->version = LAT_NATIVE_IMAGE_VERSION;
    header->header_size = sizeof(*header);
    header->flags = LAT_NATIVE_IMAGE_PIE |
                    LAT_NATIVE_IMAGE_C_ABI_STATE_SMOKE |
                    LAT_NATIVE_IMAGE_NO_PRECISE_SIGNAL_MAP;
    header->guest_entry = 0x2000;
    header->preferred_guest_base = 0x2000;
    header->guest_image_offset = sizeof(*header);
    header->guest_image_size = 1;
    header->code_offset = header->guest_image_offset + 8;
    header->code_size = sizeof(code);
    header->tb_table_offset = header->code_offset + sizeof(code);
    header->tb_count = 1;
    header->relocation_offset = header->tb_table_offset +
                                sizeof(LatNativeTbV1);
    header->pc_map_offset = header->relocation_offset;
    strcpy(header->lat_build_id, "state-smoke-v1");
    memcpy(image + header->code_offset, code, sizeof(code));
    LatNativeTbV1 *tb = (void *)(image + header->tb_table_offset);
    tb->guest_pc = header->guest_entry;
    tb->code_offset = 0;
    tb->code_size = sizeof(code);
    FILE *output = fopen(argv[1], "wb");
    if (!output || fwrite(image, header->pc_map_offset, 1, output) != 1 ||
        fclose(output)) {
        fprintf(stderr, "cannot write state smoke image\n");
        return 1;
    }
    return 0;
}
