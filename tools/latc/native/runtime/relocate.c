#define _GNU_SOURCE

#include "relocate.h"

#include "runtime-symbols.h"
#include "dispatch.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static int fail(char *error, size_t error_size, const char *message)
{
    if (error && error_size) {
        snprintf(error, error_size, "%s: %s", message, strerror(errno));
    }
    return -1;
}

static int patch_pc_relative(uint32_t *instructions, uintptr_t patch_address,
                             uintptr_t target)
{
    int64_t difference = (int64_t)target - (int64_t)patch_address;
    if ((difference & 3) != 0) {
        errno = ERANGE;
        return -1;
    }
    int64_t offset = difference >> 2;
    int64_t upper = (offset + (1 << 15)) >> 16;
    if (upper < -(1 << 19) || upper >= (1 << 19)) {
        errno = ERANGE;
        return -1;
    }
    uint32_t destination = instructions[1] & 0x1f;
    instructions[0] = 0x1e000000u |
        ((uint32_t)upper & 0xfffff) << 5 | 12u;
    instructions[1] = 0x4c000000u |
        ((uint32_t)offset & 0xffff) << 10 | 12u << 5 | destination;
    return 0;
}

static int patch_tb_target_pair(uint32_t *instructions,
                                uintptr_t patch_address, uintptr_t target)
{
    if ((instructions[0] & 0xfe00001fu) == 0x1e00000cu &&
        (instructions[1] & 0xfc0003e0u) == 0x4c000180u) {
        uintptr_t branch_address = patch_address + sizeof(*instructions);
        int64_t difference = (int64_t)target - (int64_t)branch_address;
        int64_t offset = difference >> 2;
        if (!(difference & 3) && offset >= -(1 << 25) &&
            offset < (1 << 25)) {
            uint32_t destination = instructions[1] & 0x1fu;
            instructions[0] = destination ?
                0x18000040u | destination : 0x03400000u;
            instructions[1] = 0x50000000u |
                ((uint32_t)offset & 0xffffu) << 10 |
                (((uint32_t)offset >> 16) & 0x3ffu);
            return 0;
        }
    }
    return patch_pc_relative(instructions, patch_address, target);
}

static int patch_branch(uint32_t *instruction, uintptr_t patch_address,
                        uintptr_t target)
{
    int64_t difference = (int64_t)target - (int64_t)patch_address;
    if ((difference & 3) != 0) {
        errno = ERANGE;
        return -1;
    }
    int64_t offset = difference >> 2;
    uint32_t opcode = *instruction & 0xfc000000u;
    if (opcode == 0x50000000u || opcode == 0x54000000u) {
        if (offset < -(1 << 25) || offset >= (1 << 25)) {
            errno = ERANGE;
            return -1;
        }
        *instruction = opcode | ((uint32_t)offset & 0xffffu) << 10 |
                       (((uint32_t)offset >> 16) & 0x3ffu);
        return 0;
    }
    if (opcode >= 0x58000000u && opcode <= 0x6c000000u) {
        if (offset < -(1 << 15) || offset >= (1 << 15)) {
            errno = ERANGE;
            return -1;
        }
        *instruction = (*instruction & 0xfc0003ffu) |
                       ((uint32_t)offset & 0xffffu) << 10;
        return 0;
    }
    uint32_t opcode20 = *instruction & 0xfc000100u;
    if (opcode == 0x40000000u || opcode == 0x44000000u ||
        opcode20 == 0x48000000u || opcode20 == 0x48000100u) {
        if (offset < -(1 << 19) || offset >= (1 << 19)) {
            errno = ERANGE;
            return -1;
        }
        *instruction = (*instruction & 0xfc0003e0u) |
                       ((uint32_t)offset & 0xffffu) << 10 |
                       (((uint32_t)offset >> 16) & 0x1fu);
        return 0;
    }
    errno = ENOEXEC;
    return -1;
}

static int patch_absolute(uint32_t *instructions, uint32_t slots,
                          uint64_t target)
{
    if (slots < 2 || slots > 3 || (slots == 2 && target >> 31)) {
        errno = ERANGE;
        return -1;
    }
    if ((instructions[0] & 0xfe000000u) != 0x14000000u ||
        (instructions[1] & 0xffc00000u) != 0x03800000u ||
        (slots == 3 &&
         (instructions[2] & 0xfe000000u) != 0x16000000u)) {
        errno = ENOEXEC;
        return -1;
    }
    instructions[0] = (instructions[0] & 0xfe00001fu) |
        (((target >> 12) & 0xfffff) << 5);
    instructions[1] = (instructions[1] & 0xffc003ffu) |
        ((target & 0xfff) << 10);
    if (slots == 3) {
        instructions[2] = (instructions[2] & 0xfe00001fu) |
            (((target >> 32) & 0xfffff) << 5);
    }
    return 0;
}

static int patch_jrra(uint32_t *instructions, uintptr_t patch_address,
                      uintptr_t target)
{
    if ((instructions[0] & 0xfe00001fu) != 0x1a00000cu ||
        (instructions[1] & 0xffc003ffu) != 0x0380018cu ||
        instructions[2] != (0x00000801u | 12u << 5) ||
        instructions[3] != (0x00000800u | 13u << 5)) {
        errno = ENOEXEC;
        return -1;
    }
    uint32_t high = ((target >> 12) - (patch_address >> 12)) & 0xfffff;
    instructions[0] = 0x1a000000u | high << 5 | 12u;
    instructions[1] = 0x03800000u | (target & 0xfff) << 10 |
        12u << 5 | 12u;
    instructions[2] = 0x00000801u | 12u << 5;
    instructions[3] = 0x00000800u | 13u << 5;
    return 0;
}

int lat_native_code_load(const LatNativeImageHeaderV2 *header,
                         const unsigned char *image, size_t image_size,
                         LatNativeCode *code, char *error,
                         size_t error_size)
{
    if (!header || !image || !code ||
        header->code_offset > image_size ||
        header->code_size > image_size - header->code_offset ||
        header->relocation_offset > image_size ||
        header->relocation_count >
            (image_size - header->relocation_offset) /
                sizeof(LatNativeRelocationV1)) {
        errno = EINVAL;
        return fail(error, error_size, "invalid native code image");
    }
    long page_size_long = sysconf(_SC_PAGESIZE);
    if (page_size_long <= 0 || header->code_size > SIZE_MAX) {
        errno = EOVERFLOW;
        return fail(error, error_size, "native code size is unsupported");
    }
    size_t page_size = (size_t)page_size_long;
    if (header->code_size > SIZE_MAX - (page_size - 1)) {
        errno = EOVERFLOW;
        return fail(error, error_size, "native code size overflows");
    }
    size_t mapped_size = ((size_t)header->code_size + page_size - 1) &
                         ~(page_size - 1);
    uintptr_t anchor = lat_runtime_symbol_address(
        LAT_NATIVE_SYMBOL_EPILOGUE_RET_ID_1);
    if (image_size > UINTPTR_MAX - anchor - 64 * 1024 * 1024 - page_size) {
        errno = EOVERFLOW;
        return fail(error, error_size, "native code mapping hint overflows");
    }
    uintptr_t hint_value =
        (anchor + image_size + 64 * 1024 * 1024 + page_size - 1) &
        ~(uintptr_t)(page_size - 1);
    void *address = mmap((void *)hint_value, mapped_size,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (address == MAP_FAILED) {
        return fail(error, error_size, "cannot allocate native code");
    }
    memcpy(address, image + header->code_offset, header->code_size);
    lat_runtime_symbols_configure(header->flags);
    const LatNativeRelocationV1 *relocations =
        (const void *)(image + header->relocation_offset);
    for (uint64_t i = 0; i < header->relocation_count; i++) {
        const LatNativeRelocationV1 *relocation = &relocations[i];
        if (relocation->kind == LAT_NATIVE_RELOC_IMAGE_DATA &&
            (relocation->slots < 2 || relocation->slots > 3 ||
             relocation->code_offset >= header->code_size ||
             relocation->slots * 4 >
                 header->code_size - relocation->code_offset ||
             relocation->addend < 0 || !relocation->reserved ||
             (uint64_t)relocation->addend >= header->code_size ||
             relocation->reserved >
                 header->code_size - (uint64_t)relocation->addend)) {
            errno = EINVAL;
            munmap(address, mapped_size);
            return fail(error, error_size, "invalid native data relocation");
        }
        uint32_t *instructions =
            (void *)((unsigned char *)address + relocation->code_offset);
        int result;
        if (relocation->kind == LAT_NATIVE_RELOC_GUEST_ADDRESS) {
            result = patch_absolute(instructions, relocation->slots,
                                    (uint64_t)relocation->addend);
        } else if (relocation->kind == LAT_NATIVE_RELOC_IMAGE_DATA) {
            uintptr_t target = (uintptr_t)address + relocation->addend;
            int64_t pages = ((int64_t)(target & ~(uintptr_t)0xfff) -
                (int64_t)((uintptr_t)instructions & ~(uintptr_t)0xfff)) >> 12;
            if (pages < -(1 << 19) || pages >= (1 << 19)) {
                errno = ERANGE;
                result = -1;
            } else {
                uint32_t reg = instructions[0] & 0x1f;
                instructions[0] = 0x1a000000u |
                    ((uint32_t)pages & 0xfffff) << 5 | reg;
                instructions[1] = 0x03800000u |
                    ((uint32_t)target & 0xfff) << 10 | reg << 5 | reg;
                if (relocation->slots == 3) {
                    instructions[2] = 0x03400000u;
                }
                result = 0;
            }
        } else if (relocation->kind == LAT_NATIVE_RELOC_TB_TARGET ||
                   relocation->kind == LAT_NATIVE_RELOC_JRRA_TARGET) {
            const LatNativeTbV1 *target_tb = lat_native_tb_find(
                header, image, image_size, (uint64_t)relocation->addend,
                relocation->target);
            if (!target_tb) {
                target_tb = lat_native_tb_find_unique_pc(
                    header, image, image_size,
                    (uint64_t)relocation->addend);
            }
            if (!target_tb) {
                if (header->flags & LAT_NATIVE_IMAGE_X86_STATIC_EXEC) {
                    errno = ENOENT;
                    result = -1;
                } else {
                    uintptr_t target = lat_runtime_symbol_address(
                        relocation->reserved);
                    if (!target) {
                        errno = ENOENT;
                        result = -1;
                    } else {
                        result = relocation->slots == 2 ?
                            patch_pc_relative(instructions,
                                              (uintptr_t)instructions,
                                              target) :
                            patch_absolute(instructions, relocation->slots,
                                           target);
                    }
                }
            } else if (relocation->kind == LAT_NATIVE_RELOC_JRRA_TARGET) {
                uintptr_t target = (uintptr_t)address + target_tb->code_offset;
                if (relocation->slots != 4) {
                    errno = ENOEXEC;
                    result = -1;
                } else {
                    result = patch_jrra(instructions,
                                        (uintptr_t)instructions, target);
                }
            } else {
                uintptr_t target = (uintptr_t)address + target_tb->code_offset;
                result = relocation->slots == 1 ?
                    patch_branch(instructions, (uintptr_t)instructions,
                                 target) : relocation->slots == 2 ?
                    patch_tb_target_pair(instructions,
                                         (uintptr_t)instructions, target) :
                    patch_absolute(instructions, relocation->slots, target);
            }
        } else if (relocation->kind == LAT_NATIVE_RELOC_RUNTIME_SYMBOL) {
            uintptr_t target = lat_runtime_symbol_address(relocation->target);
            if (!target) {
                errno = ENOENT;
                result = -1;
            } else if (relocation->slots == 2) {
                result = patch_pc_relative(instructions,
                    (uintptr_t)instructions, target);
            } else {
                result = patch_absolute(instructions, relocation->slots,
                                        target);
            }
        } else {
            errno = ENOTSUP;
            result = -1;
        }
        if (result) {
            char message[160];
            snprintf(message, sizeof(message),
                     "cannot apply native relocation %llu kind=%u slots=%u "
                     "offset=0x%llx insn=0x%08x",
                     (unsigned long long)i, relocation->kind,
                     relocation->slots,
                     (unsigned long long)relocation->code_offset,
                     instructions[0]);
            munmap(address, mapped_size);
            return fail(error, error_size, message);
        }
    }
    __builtin___clear_cache(address,
        (unsigned char *)address + header->code_size);
    if (mprotect(address, mapped_size, PROT_READ | PROT_EXEC)) {
        munmap(address, mapped_size);
        return fail(error, error_size, "cannot protect native code");
    }
    code->address = address;
    code->size = mapped_size;
    return 0;
}

void lat_native_code_unload(LatNativeCode *code)
{
    if (!code || !code->address || !code->size) return;
    munmap(code->address, code->size);
    memset(code, 0, sizeof(*code));
}
