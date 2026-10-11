#include "dispatch.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    unsigned char image[sizeof(LatNativeImageHeaderV2) +
                        3 * sizeof(LatNativeTbV1)] = {0};
    LatNativeImageHeaderV2 *header = (void *)image;
    header->tb_table_offset = sizeof(*header);
    header->tb_count = 3;
    LatNativeTbV1 *tbs = (void *)(image + header->tb_table_offset);
    tbs[0] = (LatNativeTbV1){ .guest_pc = 0x1000, .flags = 0 };
    tbs[1] = (LatNativeTbV1){ .guest_pc = 0x1000, .flags = 4 };
    tbs[2] = (LatNativeTbV1){
        .guest_pc = 0x2000, .code_offset = 8, .flags = 0,
    };
    if (lat_native_tb_find(header, image, sizeof(image), 0x1000, 4) !=
            &tbs[1] ||
        lat_native_tb_find(header, image, sizeof(image), 0x1000, 2) ||
        lat_native_tb_find(header, image, sizeof(image), 0x3000, 0) ||
        lat_native_tb_find_unique_pc(header, image, sizeof(image), 0x1000) ||
        lat_native_tb_find_unique_pc(header, image, sizeof(image), 0x2000) !=
            &tbs[2]) {
        fprintf(stderr, "native TB lookup failed\n");
        return 1;
    }
    LatNativeX86FastTb *cache = calloc(LAT_NATIVE_X86_JMP_CACHE_SIZE,
                                       sizeof(*cache));
    unsigned char code[16] = {0};
    if (!cache) return 1;
    lat_native_x86_dispatch_configure(header, image, sizeof(image), code,
                                       cache,
                                       LAT_NATIVE_X86_JMP_CACHE_SIZE);
    if (lat_native_x86_dispatch_lookup(0x2000) != code + 8) {
        fprintf(stderr, "native dispatch target failed\n");
        free(cache);
        return 1;
    }
    size_t hash = (0x2000 ^ (0x2000 >> LAT_NATIVE_X86_JMP_CACHE_BITS)) &
        (LAT_NATIVE_X86_JMP_CACHE_SIZE - 1);
    if (cache[hash].pc != 0x2000 || cache[hash].ptr != code + 8) {
        fprintf(stderr, "native translated jump cache was not populated\n");
        free(cache);
        return 1;
    }
    tbs[2].code_offset = 4;
    if (lat_native_x86_dispatch_lookup(0x2000) != code + 8) {
        fprintf(stderr, "native C dispatch cache failed\n");
        free(cache);
        return 1;
    }
    free(cache);
    puts("test-native-dispatch: PASS");
    return 0;
}
