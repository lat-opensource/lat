#ifndef LATC_NATIVE_DISPATCH_H
#define LATC_NATIVE_DISPATCH_H

#define LAT_NATIVE_X86_JMP_CACHE_BITS 16
#define LAT_NATIVE_X86_JMP_CACHE_SIZE \
    (1u << LAT_NATIVE_X86_JMP_CACHE_BITS)

#ifndef __ASSEMBLER__
#include "lat-native-image.h"

#include <stddef.h>
#include <stdint.h>

typedef struct LatNativeX86FastTb {
    uint64_t pc;
    const void *ptr;
} LatNativeX86FastTb;

const LatNativeTbV1 *lat_native_tb_find(const LatNativeImageHeaderV2 *header,
                                        const unsigned char *image,
                                        size_t image_size,
                                        uint64_t guest_pc, uint32_t flags);
const LatNativeTbV1 *lat_native_tb_find_unique_pc(
    const LatNativeImageHeaderV2 *header, const unsigned char *image,
    size_t image_size, uint64_t guest_pc);

void lat_native_x86_dispatch_configure(const LatNativeImageHeaderV2 *header,
                                       const unsigned char *image,
                                       size_t image_size,
                                       const void *code_address,
                                       LatNativeX86FastTb *jump_cache,
                                       size_t jump_cache_count);
void *lat_native_x86_dispatch_lookup(uint64_t guest_pc);
#endif

#endif
