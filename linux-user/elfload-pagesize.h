#ifndef LINUX_USER_ELFLOAD_PAGESIZE_H
#define LINUX_USER_ELFLOAD_PAGESIZE_H

#include <stdint.h>

typedef struct ElfLoadMapRange {
    uint32_t p_type;
    uint64_t vaddr;
    uint64_t memsz;
    uint64_t align;
} ElfLoadMapRange;

uint64_t elf_load_exec_pagesize(const ElfLoadMapRange *segments,
                                int segment_count, int current,
                                uint64_t load_bias,
                                uint64_t host_page_size,
                                uint64_t target_page_size);
uint64_t elf_load_page_start(uint64_t vaddr, uint64_t pagesize);
uint64_t elf_load_page_offset(uint64_t vaddr, uint64_t pagesize);
uint64_t elf_load_page_length(uint64_t len, uint64_t pagesize);

#endif
