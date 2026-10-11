#define _GNU_SOURCE

#include "dispatch.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static const LatNativeImageHeaderV2 *dispatch_header;
static const unsigned char *dispatch_image;
static size_t dispatch_image_size;
static const unsigned char *dispatch_code;
static LatNativeX86FastTb *dispatch_jump_cache;
static size_t dispatch_jump_cache_count;
static LatNativeX86FastTb dispatch_lookup_cache[LAT_NATIVE_X86_JMP_CACHE_SIZE];

const LatNativeTbV1 *lat_native_tb_find(const LatNativeImageHeaderV2 *header,
                                        const unsigned char *image,
                                        size_t image_size,
                                        uint64_t guest_pc, uint32_t flags)
{
    if (!header || !image || header->tb_table_offset > image_size ||
        header->tb_count > (image_size - header->tb_table_offset) /
            sizeof(LatNativeTbV1)) {
        return NULL;
    }
    const LatNativeTbV1 *tbs =
        (const void *)(image + header->tb_table_offset);
    uint64_t left = 0;
    uint64_t right = header->tb_count;
    while (left < right) {
        uint64_t middle = left + (right - left) / 2;
        const LatNativeTbV1 *tb = &tbs[middle];
        if (tb->guest_pc < guest_pc ||
            (tb->guest_pc == guest_pc && tb->flags < flags)) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    if (left < header->tb_count && tbs[left].guest_pc == guest_pc &&
        tbs[left].flags == flags) {
        return &tbs[left];
    }
    return NULL;
}

const LatNativeTbV1 *lat_native_tb_find_unique_pc(
    const LatNativeImageHeaderV2 *header, const unsigned char *image,
    size_t image_size, uint64_t guest_pc)
{
    if (!header || !image || header->tb_table_offset > image_size ||
        header->tb_count > (image_size - header->tb_table_offset) /
            sizeof(LatNativeTbV1)) {
        return NULL;
    }
    const LatNativeTbV1 *tbs =
        (const void *)(image + header->tb_table_offset);
    uint64_t left = 0;
    uint64_t right = header->tb_count;
    while (left < right) {
        uint64_t middle = left + (right - left) / 2;
        if (tbs[middle].guest_pc < guest_pc) {
            left = middle + 1;
        } else {
            right = middle;
        }
    }
    if (left >= header->tb_count || tbs[left].guest_pc != guest_pc) {
        return NULL;
    }
    if (left + 1 < header->tb_count && tbs[left + 1].guest_pc == guest_pc) {
        return NULL;
    }
    return &tbs[left];
}

void lat_native_x86_dispatch_configure(const LatNativeImageHeaderV2 *header,
                                       const unsigned char *image,
                                       size_t image_size,
                                       const void *code_address,
                                       LatNativeX86FastTb *jump_cache,
                                       size_t jump_cache_count)
{
    dispatch_header = header;
    dispatch_image = image;
    dispatch_image_size = image_size;
    dispatch_code = code_address;
    dispatch_jump_cache = jump_cache;
    dispatch_jump_cache_count = jump_cache_count;
    for (size_t i = 0; i < jump_cache_count; i++) {
        jump_cache[i].pc = UINT64_MAX;
        jump_cache[i].ptr = NULL;
    }
    for (size_t i = 0; i < LAT_NATIVE_X86_JMP_CACHE_SIZE; i++) {
        dispatch_lookup_cache[i].pc = UINT64_MAX;
        dispatch_lookup_cache[i].ptr = NULL;
    }
}

void *lat_native_x86_dispatch_lookup(uint64_t guest_pc)
{
    size_t hash = (guest_pc ^
        (guest_pc >> LAT_NATIVE_X86_JMP_CACHE_BITS)) &
        (LAT_NATIVE_X86_JMP_CACHE_SIZE - 1);
    if (dispatch_lookup_cache[hash].pc == guest_pc) {
        return (void *)dispatch_lookup_cache[hash].ptr;
    }
    const LatNativeTbV1 *tb = lat_native_tb_find(
        dispatch_header, dispatch_image, dispatch_image_size, guest_pc, 0);
    if (!tb || !dispatch_code) {
        dprintf(STDERR_FILENO,
                "latc: static native image is missing TB pc=0x%llx\n",
                (unsigned long long)guest_pc);
        _exit(127);
    }
    void *target = (void *)(dispatch_code + tb->code_offset);
    dispatch_lookup_cache[hash].ptr = target;
    dispatch_lookup_cache[hash].pc = guest_pc;
    if (hash < dispatch_jump_cache_count) {
        dispatch_jump_cache[hash].ptr = target;
        dispatch_jump_cache[hash].pc = guest_pc;
    }
    return target;
}
