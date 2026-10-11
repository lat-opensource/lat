#include "qemu/osdep.h"
#include "elf.h"
#include "qemu/range.h"
#include "linux-user/elfload-pagesize.h"

uint64_t elf_load_page_start(uint64_t vaddr, uint64_t pagesize)
{
    return vaddr & ~(pagesize - 1);
}

uint64_t elf_load_page_offset(uint64_t vaddr, uint64_t pagesize)
{
    return vaddr & (pagesize - 1);
}

uint64_t elf_load_page_length(uint64_t len, uint64_t pagesize)
{
    return ROUND_UP(len, pagesize);
}

uint64_t elf_load_exec_pagesize(const ElfLoadMapRange *segments,
                                int segment_count, int current,
                                uint64_t load_bias,
                                uint64_t host_page_size,
                                uint64_t target_page_size)
{
    uint64_t elf_pagesize;
    uint64_t host_page_start, guest_page_start;
    int i;

    g_assert(segments != NULL);
    g_assert(current >= 0 && current < segment_count);
    g_assert(host_page_size != 0);
    g_assert(target_page_size != 0);

    elf_pagesize = ((segments[current].align & (host_page_size - 1)) != 0) ?
        target_page_size : MAX(host_page_size, target_page_size);

    if (elf_pagesize <= target_page_size) {
        return elf_pagesize;
    }

    host_page_start = elf_load_page_start(load_bias + segments[current].vaddr,
                                          elf_pagesize);
    guest_page_start = elf_load_page_start(load_bias + segments[current].vaddr,
                                           target_page_size);
    if (host_page_start >= guest_page_start) {
        return elf_pagesize;
    }

    for (i = 0; i < segment_count; ++i) {
        uint64_t other_start;

        if (i == current || segments[i].p_type != PT_LOAD ||
            segments[i].memsz == 0) {
            continue;
        }

        other_start = load_bias + segments[i].vaddr;
        if (ranges_overlap(host_page_start, guest_page_start - host_page_start,
                           other_start, segments[i].memsz)) {
            return target_page_size;
        }
    }

    return elf_pagesize;
}
