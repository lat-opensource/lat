/* SPDX-License-Identifier: GPL-2.0-only */
#include "qemu/osdep.h"

#include "aot.h"
#include "aot_lib.h"
#include "segment.h"

/* Exercise the actual guest munmap entry point with real host mappings. */
#include "../../../../../linux-user/mmap.c"

int qemu_loglevel;
int trace_events_enabled_count;
uint16_t _TRACE_TARGET_MUNMAP_DSTATE;
bool message_with_timestamp;
int option_aot = 1;
int option_aot_pe_profile;
int option_prlimit;
const char *aot_process_profile = "browser";
abi_ulong mmap_min_addr;
unsigned long guest_base;
unsigned long reserved_va;
unsigned long qemu_host_page_size;
intptr_t qemu_host_page_mask;
rlim_t vir_rlimit_as = RLIM_INFINITY;
rlim_t vir_rlimit_as_acc;

int qemu_log(const char *fmt G_GNUC_UNUSED, ...)
{
    return 0;
}

void print_stack_trace(void)
{
}

int qemu_get_thread_id(void)
{
    return getpid();
}

void *page_get_target_data(target_ulong address)
{
    return NULL;
}

int page_get_flags(target_ulong address)
{
    return 0;
}

void page_set_flags(target_ulong start, target_ulong end, int flags)
{
}

void guest_vma_name_reset(abi_ulong start, abi_ulong len)
{
}

uint8_t get_file_type(const char *name)
{
    return PE_AOT_FILE;
}

uint8_t is_elf_file(const char *name)
{
    return false;
}

static void add_segment(char *path, uintptr_t address, size_t length,
                        void **cache)
{
    seg_info *seg;
    char name[PATH_MAX];

    segment_tree_insert(path, 0, address, address + length);
    seg = segment_tree_lookup(address);
    g_assert(seg != NULL);
    *cache = mmap(NULL, qemu_host_page_size, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    g_assert(*cache != MAP_FAILED);
    seg->buffer = *cache;
    seg->seg_flag |= SEG_AOT_LOADED;
    g_assert(segment_get_aot_file_name(seg, name, sizeof(name)) == 0);
    g_assert(lib_tree_insert(name, *cache, qemu_host_page_size) != NULL);
}

int main(void)
{
    char path[] = "/tmp/latx-segment-unmap-XXXXXX";
    void *cache[5];
    unsigned char resident;
    size_t page_size = sysconf(_SC_PAGESIZE);
    size_t stride = page_size * 2;
    void *guest;
    int fd;
    int ret;

    qemu_host_page_size = page_size;
    qemu_host_page_mask = ~(page_size - 1);
    segment_tree_init();
    lib_tree_init();
    fd = mkstemp(path);
    g_assert(fd >= 0);
    close(fd);
    guest = mmap(NULL, stride * 5, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    g_assert(guest != MAP_FAILED);
    for (unsigned int i = 0; i < G_N_ELEMENTS(cache); i++) {
        add_segment(path, (uintptr_t)guest + stride * i, page_size,
                    &cache[i]);
    }
    g_assert(get_segment_num() == 5);
    g_assert(get_lib_num() == 5);

    /* Invalid input must not discard cache ownership. */
    ret = target_munmap((abi_ulong)guest + 1, stride, 0);
    g_assert(ret == -TARGET_EINVAL);
    g_assert(get_segment_num() == 5);

    /* One range removes three segments but preserves both neighbours. */
    ret = target_munmap((abi_ulong)guest + stride, stride * 3, 0);
    g_assert(ret == 0);
    g_assert(get_segment_num() == 2);
    g_assert(get_lib_num() == 2);
    for (unsigned int i = 1; i < 4; i++) {
        errno = 0;
        ret = mincore(cache[i], page_size, &resident);
        g_assert(ret == -1 && errno == ENOMEM);
    }
    g_assert(segment_tree_lookup((abi_ulong)guest) != NULL);
    g_assert(segment_tree_lookup((abi_ulong)guest + stride * 4) != NULL);

    /* Repeating an already-unmapped range is harmless. */
    ret = target_munmap((abi_ulong)guest + stride, stride * 3, 0);
    g_assert(ret == 0);
    g_assert(get_segment_num() == 2);
    ret = target_munmap((abi_ulong)guest, stride * 5, 0);
    g_assert(ret == 0);
    g_assert(get_segment_num() == 0);
    g_assert(get_lib_num() == 0);
    unlink(path);
    return 0;
}
