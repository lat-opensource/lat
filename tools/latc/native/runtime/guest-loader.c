#define _GNU_SOURCE

#include "guest-loader.h"

#include <elf.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

static int fail(char *error, size_t error_size, const char *message)
{
    if (error && error_size) {
        snprintf(error, error_size, "%s: %s", message, strerror(errno));
    }
    return -1;
}

static uint64_t round_down(uint64_t value, uint64_t alignment)
{
    return value & ~(alignment - 1);
}

static int round_up(uint64_t value, uint64_t alignment, uint64_t *result)
{
    if (value > UINT64_MAX - (alignment - 1)) {
        errno = EOVERFLOW;
        return -1;
    }
    *result = (value + alignment - 1) & ~(alignment - 1);
    return 0;
}

static int elf_protection(uint32_t flags)
{
    int protection = 0;
    if (flags & PF_R) protection |= PROT_READ;
    if (flags & PF_W) protection |= PROT_WRITE;
    if (flags & PF_X) protection |= PROT_EXEC;
    return protection;
}

int lat_guest_map(const LatNativeImageHeaderV2 *header,
                  const unsigned char *image, size_t image_size,
                  LatGuestMapping *mapping, char *error,
                  size_t error_size)
{
    long page_size_long = sysconf(_SC_PAGESIZE);
    if (!header || !image || !mapping || page_size_long <= 0) {
        errno = EINVAL;
        return fail(error, error_size, "invalid guest mapping arguments");
    }
    uint64_t page_size = (uint64_t)page_size_long;
    if ((page_size & (page_size - 1)) != 0 ||
        header->guest_image_offset > image_size ||
        header->guest_image_size > image_size - header->guest_image_offset ||
        header->guest_image_size < sizeof(Elf64_Ehdr)) {
        errno = EINVAL;
        return fail(error, error_size, "invalid embedded guest image");
    }
    const unsigned char *guest = image + header->guest_image_offset;
    size_t guest_size = header->guest_image_size;
    const Elf64_Ehdr *elf = (const void *)guest;
    if (memcmp(elf->e_ident, ELFMAG, SELFMAG) ||
        elf->e_ident[EI_CLASS] != ELFCLASS64 ||
        elf->e_ident[EI_DATA] != ELFDATA2LSB ||
        elf->e_machine != EM_X86_64 || elf->e_type != ET_EXEC ||
        elf->e_phentsize != sizeof(Elf64_Phdr) ||
        elf->e_phoff > guest_size ||
        (uint64_t)elf->e_phnum * sizeof(Elf64_Phdr) >
            guest_size - elf->e_phoff) {
        errno = ENOEXEC;
        return fail(error, error_size, "unsupported guest ELF");
    }
    const Elf64_Phdr *program_headers =
        (const void *)(guest + elf->e_phoff);
    uint64_t base = UINT64_MAX;
    uint64_t end = 0;
    uint64_t phdr_address = 0;
    for (uint16_t i = 0; i < elf->e_phnum; i++) {
        const Elf64_Phdr *segment = &program_headers[i];
        if (segment->p_type == PT_PHDR) phdr_address = segment->p_vaddr;
        if (segment->p_type != PT_LOAD) continue;
        if (!segment->p_memsz) continue;
        if (segment->p_filesz > segment->p_memsz ||
            segment->p_offset > guest_size ||
            segment->p_filesz > guest_size - segment->p_offset ||
            segment->p_vaddr > UINT64_MAX - segment->p_memsz) {
            errno = ENOEXEC;
            return fail(error, error_size, "invalid guest load segment");
        }
        uint64_t segment_base = round_down(segment->p_vaddr, page_size);
        uint64_t segment_end;
        if (round_up(segment->p_vaddr + segment->p_memsz, page_size,
                     &segment_end)) {
            return fail(error, error_size, "guest address overflow");
        }
        if (segment_base < base) base = segment_base;
        if (segment_end > end) end = segment_end;
    }
    if (!phdr_address) {
        uint64_t phdr_size = (uint64_t)elf->e_phnum * elf->e_phentsize;
        for (uint16_t i = 0; i < elf->e_phnum; i++) {
            const Elf64_Phdr *segment = &program_headers[i];
            if (segment->p_type != PT_LOAD || elf->e_phoff < segment->p_offset ||
                phdr_size > segment->p_filesz ||
                elf->e_phoff - segment->p_offset >
                    segment->p_filesz - phdr_size) {
                continue;
            }
            phdr_address = segment->p_vaddr +
                           (elf->e_phoff - segment->p_offset);
            break;
        }
    }
    if (base == UINT64_MAX || end <= base || base != header->preferred_guest_base ||
        elf->e_entry != header->guest_entry || end - base > SIZE_MAX ||
        !phdr_address) {
        errno = ENOEXEC;
        return fail(error, error_size, "guest layout does not match native image");
    }
    size_t page_count = (size_t)((end - base) / page_size);
    unsigned char *page_protection = calloc(page_count, 1);
    unsigned char *page_loaded = calloc(page_count, 1);
    if (!page_protection || !page_loaded) {
        free(page_protection);
        free(page_loaded);
        return fail(error, error_size, "cannot allocate guest page table");
    }
    void *address = mmap((void *)(uintptr_t)base, (size_t)(end - base),
                         PROT_NONE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                         -1, 0);
    if (address == MAP_FAILED) {
        free(page_protection);
        free(page_loaded);
        return fail(error, error_size, "cannot reserve guest address range");
    }

    int result = -1;
    for (uint16_t i = 0; i < elf->e_phnum; i++) {
        const Elf64_Phdr *segment = &program_headers[i];
        if (segment->p_type != PT_LOAD) continue;
        if (!segment->p_memsz) continue;
        uint64_t segment_base = round_down(segment->p_vaddr, page_size);
        uint64_t segment_end;
        if (round_up(segment->p_vaddr + segment->p_memsz, page_size,
                     &segment_end) ||
            mprotect((void *)(uintptr_t)segment_base,
                     (size_t)(segment_end - segment_base),
                     PROT_READ | PROT_WRITE)) {
            fail(error, error_size, "cannot prepare guest load segment");
            goto out;
        }
        memcpy((void *)(uintptr_t)segment->p_vaddr,
               guest + segment->p_offset, segment->p_filesz);
        int protection = elf_protection(segment->p_flags);
        size_t first_page = (size_t)((segment_base - base) / page_size);
        size_t last_page = (size_t)((segment_end - base) / page_size);
        for (size_t page = first_page; page < last_page; page++) {
            page_loaded[page] = 1;
            page_protection[page] |= protection;
        }
    }
    for (size_t first = 0; first < page_count;) {
        int protection = page_loaded[first] ? page_protection[first] : PROT_NONE;
        size_t last = first + 1;
        while (last < page_count &&
               (page_loaded[last] ? page_protection[last] : PROT_NONE) ==
                   protection) {
            last++;
        }
        if (mprotect((void *)(uintptr_t)(base + first * page_size),
                     (last - first) * page_size, protection)) {
            fail(error, error_size, "cannot protect guest load segment");
            goto out;
        }
        first = last;
    }
    mapping->base = base;
    mapping->end = end;
    mapping->entry = elf->e_entry;
    mapping->phdr = phdr_address;
    mapping->phent = elf->e_phentsize;
    mapping->phnum = elf->e_phnum;
    result = 0;
out:
    free(page_protection);
    free(page_loaded);
    if (result) {
        munmap((void *)(uintptr_t)base, (size_t)(end - base));
    }
    return result;
}

void lat_guest_unmap(LatGuestMapping *mapping)
{
    if (!mapping || mapping->end <= mapping->base) return;
    munmap((void *)(uintptr_t)mapping->base,
           (size_t)(mapping->end - mapping->base));
    memset(mapping, 0, sizeof(*mapping));
}
