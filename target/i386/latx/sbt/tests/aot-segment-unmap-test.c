/* SPDX-License-Identifier: GPL-2.0-only */
#include "qemu/osdep.h"

#include <sys/shm.h>

#include "aot.h"
#include "aot_lib.h"
#include "segment.h"
#include "aot-shm-syscall-test.h"

/* Exercise the actual guest mmap/munmap paths with real host mappings. */
#include "../../../../../linux-user/mmap.c"

int qemu_loglevel;
int trace_events_enabled_count;
uint16_t _TRACE_TARGET_MUNMAP_DSTATE;
uint16_t _TRACE_TARGET_MMAP_DSTATE;
int option_aot_wine;
int option_debug_aot;
int latx_wine;
__thread CPUState *thread_cpu;
static GHashTable *page_flags;

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

/* These unrelated translation/shadow paths must not run in this seam. */
int option_private_mmap_shadow;
int option_shadow_file;
uintptr_t qemu_real_host_page_size;
uint16_t _TRACE_TARGET_MPROTECT_DSTATE;
uint16_t _TRACE_TARGET_MMAP_COMPLETE_DSTATE;
QemuLogFile *qemu_logfile;
unsigned long rcu_gp_ctr;
QemuEvent rcu_gp_event;
__thread struct rcu_reader_data rcu_reader;

void qemu_event_set(QemuEvent *ev)
{
    g_assert_not_reached();
}

void page_dump(FILE *f)
{
    g_assert_not_reached();
}

int hostpage_exist_shadow_page(uint64_t addr)
{
    return 0;
}

void shadow_page_munmap(abi_ulong start, abi_ulong end)
{
    g_assert_not_reached();
}

void create_shadow_page_chunk(abi_ulong start, abi_ulong end, int prot,
                              int flags, int fd, abi_ulong offset)
{
    g_assert_not_reached();
}

void update_shadow_page_chunk(abi_ulong start, abi_ulong end, int prot,
                              int flags, int fd, abi_ulong offset)
{
    g_assert_not_reached();
}

int mprotect_one_shadow_page(abi_ulong addr, int prot)
{
    g_assert_not_reached();
}

int mprotect_shadow_page_range_if_exist(abi_ulong start, abi_ulong end,
                                        int prot)
{
    g_assert_not_reached();
}

bool pageflags_set_clear(target_ulong start, target_ulong last,
                         int set_flags, int clear_flags)
{
    g_assert_not_reached();
}

bool page_check_range(target_ulong start, target_ulong len, int flags)
{
    g_assert_not_reached();
}

void page_set_flags_tb_reload(target_ulong start, target_ulong end,
                              int flags, bool tb_reload)
{
    g_assert_not_reached();
}

void page_set_page_state_range(target_ulong start, target_ulong end, int state)
{
    g_assert_not_reached();
}

void recover_aot_tb(char *path, uint64_t offset, abi_long start, abi_long len)
{
    g_assert_not_reached();
}

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
    return GPOINTER_TO_INT(g_hash_table_lookup(page_flags,
               (void *)(uintptr_t)(address & TARGET_PAGE_MASK)));
}

void page_set_flags(target_ulong start, target_ulong end, int flags)
{
    for (target_ulong addr = start & TARGET_PAGE_MASK;
         addr < TARGET_PAGE_ALIGN(end); addr += TARGET_PAGE_SIZE) {
        g_hash_table_insert(page_flags, (void *)(uintptr_t)addr,
                            GINT_TO_POINTER(flags));
    }
}

target_ulong page_find_range_empty(target_ulong min, target_ulong max,
                                   target_ulong len, target_ulong align)
{
    target_ulong start = ROUND_UP(min, align);

    while (start <= max && len - 1 <= max - start) {
        target_ulong addr;

        for (addr = start; addr < start + len; addr += TARGET_PAGE_SIZE) {
            if (page_get_flags(addr) & PAGE_VALID) {
                break;
            }
        }
        if (addr == start + len) {
            return start;
        }
        start = ROUND_UP(addr + TARGET_PAGE_SIZE, align);
    }
    return -1;
}

void tb_flush(CPUState *cpu)
{
    g_assert_not_reached();
}

uint8_t is_pe_file(const char *path)
{
    char magic[2];
    int fd = open(path, O_RDONLY);
    ssize_t n;

    g_assert(fd >= 0);
    n = read(fd, magic, sizeof(magic));
    close(fd);
    return n == sizeof(magic) && magic[0] == 'M' && magic[1] == 'Z';
}

uint8_t get_file_type(const char *name)
{
    return PE_AOT_FILE;
}

uint8_t is_elf_file(const char *name)
{
    return false;
}

static void attach_cache(seg_info *seg, void **cache)
{
    char name[PATH_MAX];
    int ret;
    lib_info *lib;

    g_assert(seg != NULL);
    *cache = mmap(NULL, qemu_host_page_size, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    g_assert(*cache != MAP_FAILED);
    seg->buffer = *cache;
    seg->seg_flag |= SEG_AOT_LOADED;
    ret = segment_get_aot_file_name(seg, name, sizeof(name));
    g_assert(ret == 0);
    lib = lib_tree_insert(name, *cache, qemu_host_page_size);
    g_assert(lib != NULL);
}

static void add_segment(char *path, uintptr_t address, size_t length,
                        void **cache)
{
    segment_tree_insert(path, 0, address, address + length);
    attach_cache(segment_tree_lookup(address), cache);
}

static void assert_unmapped(void *address)
{
    unsigned char resident;
    int ret;

    errno = 0;
    ret = mincore(address, qemu_host_page_size, &resident);
    g_assert(ret == -1 && errno == ENOMEM);
}

static void test_mmap_replacement(char *path)
{
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    abi_long addr, ret;
    void *cache;

    addr = target_mmap(0, qemu_host_page_size, PROT_READ | PROT_WRITE,
                       flags, -1, 0, 0);
    g_assert(addr != -1);
    add_segment(path, addr, qemu_host_page_size, &cache);
#if MAP_FIXED_NOREPLACE != 0
    errno = 0;
    ret = target_mmap(addr, qemu_host_page_size, PROT_READ,
                      flags | MAP_FIXED_NOREPLACE, -1, 0, 0);
    g_assert(ret == -1 && errno == EEXIST);
    g_assert(get_segment_num() == 1 && get_lib_num() == 1);
#endif
    ret = target_mmap(addr, qemu_host_page_size, PROT_READ,
                      flags | MAP_FIXED, -1, 0, 0);
    g_assert(ret == addr);
    g_assert(get_segment_num() == 0 && get_lib_num() == 0);
    assert_unmapped(cache);

    add_segment(path, addr, qemu_host_page_size, &cache);
    option_mmap_fixed = 1;
    ret = target_mmap(addr, qemu_host_page_size, PROT_READ,
                      flags, -1, 0, 0);
    option_mmap_fixed = 0;
    g_assert(ret == addr);
    g_assert(get_segment_num() == 0 && get_lib_num() == 0);
    assert_unmapped(cache);
    ret = target_munmap(addr, qemu_host_page_size, 0);
    g_assert(ret == 0);
}

typedef struct LateSegment {
    abi_ulong addr;
    int fd;
    void *cache;
    pthread_barrier_t ready;
    pthread_barrier_t unmapped;
} LateSegment;

static void wait_barrier(pthread_barrier_t *barrier)
{
    int ret = pthread_barrier_wait(barrier);

    g_assert(ret == 0 || ret == PTHREAD_BARRIER_SERIAL_THREAD);
}

static void *register_late_segment(void *opaque)
{
    LateSegment *late = opaque;
    char buf[PATH_MAX];
    int prot;
    ssize_t ret;
    seg_info *seg;

    ret = pread(late->fd, g2h_untagged(late->addr), TARGET_PAGE_SIZE, 0);
    g_assert(ret == 2);
    prot = page_get_flags(late->addr) & (PAGE_READ | PAGE_WRITE | PAGE_EXEC);
    g_assert(prot & PAGE_EXEC);

    /* Model preemption after the pread AOT hook's permission check. */
    wait_barrier(&late->ready);
    wait_barrier(&late->unmapped);
    g_assert(!(page_get_flags(late->addr) & PAGE_VALID));
    deal_seg(NULL, true, 0, buf, late->fd, prot, TARGET_PAGE_SIZE, late->addr);
    seg = segment_tree_lookup(late->addr);
    g_assert(seg != NULL);
    attach_cache(seg, &late->cache);
    g_assert(late->cache != g2h_untagged(late->addr));
    return NULL;
}

static void test_mmap_late_segment(char *path, int mmap_flags)
{
    LateSegment late = { 0 };
    pthread_t writer;
    abi_long addr, mapped;
    int ret;
    ssize_t written;

    late.fd = open(path, O_RDWR);
    g_assert(late.fd > 2);
    written = pwrite(late.fd, "MZ", 2, 0);
    g_assert(written == 2);
    addr = target_mmap(0, qemu_host_page_size,
                       PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0, 0);
    g_assert(addr != -1);
    late.addr = addr;
    ret = pthread_barrier_init(&late.ready, NULL, 2);
    g_assert(ret == 0);
    ret = pthread_barrier_init(&late.unmapped, NULL, 2);
    g_assert(ret == 0);
    ret = pthread_create(&writer, NULL, register_late_segment, &late);
    g_assert(ret == 0);
    wait_barrier(&late.ready);
    ret = target_munmap(addr, qemu_host_page_size, 0);
    g_assert(ret == 0);
    g_assert(get_segment_num() == 0 && get_lib_num() == 0);
    wait_barrier(&late.unmapped);
    ret = pthread_join(writer, NULL);
    g_assert(ret == 0);
    ret = pthread_barrier_destroy(&late.ready);
    g_assert(ret == 0);
    ret = pthread_barrier_destroy(&late.unmapped);
    g_assert(ret == 0);
    close(late.fd);
    g_assert(get_segment_num() == 1 && get_lib_num() == 1);
    g_assert(!(page_get_flags(addr) & PAGE_VALID));

    /* A free guest range can still contain a published old identity. */
    mapped = target_mmap(addr, qemu_host_page_size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | mmap_flags, -1, 0, 0);
    g_assert(mapped == addr);
    g_assert(get_segment_num() == 0 && get_lib_num() == 0);
    assert_unmapped(late.cache);
    ret = target_munmap(addr, qemu_host_page_size, 0);
    g_assert(ret == 0);
}

/*
 * Real IPC/mapping and AOT ownership; guest page metadata is modelled above.
 * The pread hook is represented by pread + deal_seg, not syscall dispatch.
 * Cache mappings model ownership, not a translated AOT artifact load.
 */
static void test_shm_lifecycle(char *path, int mmap_flags, bool tail)
{
    size_t size = qemu_host_page_size * 3 - (tail ? 1 : 0);
    abi_ulong addr;
    abi_long ret;
    int shmid, fd;
    char buf[PATH_MAX];
    void *cache[3];
    void *alias, *restored;
    seg_info *seg;
    ssize_t n;

    shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0700);
    g_assert(shmid >= 0);
    addr = test_aot_shmat(shmid);
    alias = shmat(shmid, NULL, SHM_EXEC);
    g_assert(alias != (void *)-1);
    ret = shmctl(shmid, IPC_RMID, NULL);
    g_assert(ret == 0);
    g_assert((abi_long)addr > 0);
    g_assert(page_get_flags(addr) & PAGE_EXEC);
    fd = open(path, O_RDWR);
    g_assert(fd > 2);
    n = pwrite(fd, "MZ", 2, 0);
    g_assert(n == 2);
    n = pread(fd, g2h_untagged(addr), TARGET_PAGE_SIZE, 0);
    g_assert(n == 2);
    deal_seg(NULL, true, 0, buf, fd, PAGE_READ | PAGE_WRITE | PAGE_EXEC,
             TARGET_PAGE_SIZE, addr);
    close(fd);
    seg = segment_tree_lookup(addr);
    g_assert(seg != NULL && seg->seg_begin == addr);
    /* Attach real owned cache mappings to three distinct segment identities. */
    attach_cache(seg, &cache[0]);
    for (unsigned int i = 1; i < G_N_ELEMENTS(cache); i++) {
        abi_ulong offset = tail && i == 2 ? size : i * qemu_host_page_size;

        add_segment(path, addr + offset, tail && i == 2 ? 1 : TARGET_PAGE_SIZE,
                    &cache[i]);
    }
    g_assert(get_segment_num() == 3 && get_lib_num() == 3);
    ret = test_aot_shmdt(addr + 1);
    g_assert(ret == -TARGET_EINVAL);
    g_assert(get_segment_num() == 3 && get_lib_num() == 3);
    g_assert(page_get_flags(addr) & PAGE_VALID);
    for (unsigned int i = 0; i < G_N_ELEMENTS(cache); i++) {
        unsigned char resident;

        ret = mincore(cache[i], qemu_host_page_size, &resident);
        g_assert(ret == 0);
    }
    /*
     * Force a host failure at the tracked start, retaining the segment via
     * an alias so that we can restore the host mapping afterwards.
     */
    ret = shmdt(g2h_untagged(addr));
    g_assert(ret == 0);
    ret = test_aot_shmdt(addr);
    g_assert(ret == -TARGET_EINVAL);
    g_assert(get_segment_num() == 3 && get_lib_num() == 3);
    g_assert(page_get_flags(addr) & PAGE_VALID);
    restored = shmat(shmid, g2h_untagged(addr), SHM_EXEC | SHM_REMAP);
    g_assert(restored == g2h_untagged(addr));
    ret = shmdt(alias);
    g_assert(ret == 0);
    ret = test_aot_shmdt(addr);
    g_assert(ret == 0);
    g_assert(!(page_get_flags(addr) & PAGE_VALID));
    g_assert(get_segment_num() == 0 && get_lib_num() == 0);
    for (unsigned int i = 0; i < G_N_ELEMENTS(cache); i++) {
        assert_unmapped(cache[i]);
    }
    ret = target_mmap(addr, HOST_PAGE_ALIGN(size), PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | mmap_flags, -1, 0, 0);
    g_assert(ret == addr);
    g_assert(get_segment_num() == 0 && get_lib_num() == 0);
    ret = target_munmap(addr, HOST_PAGE_ALIGN(size), 0);
    g_assert(ret == 0);
}

static void test_shm_disabled(void)
{
    int shmid = shmget(IPC_PRIVATE, qemu_host_page_size, IPC_CREAT | 0700);
    abi_ulong addr;
    abi_long ret;

    g_assert(shmid >= 0);
    option_aot = 0;
    addr = test_aot_shmat(shmid);
    ret = shmctl(shmid, IPC_RMID, NULL);
    g_assert(ret == 0);
    g_assert((abi_long)addr > 0);
    ret = test_aot_shmdt(addr);
    g_assert(ret == 0);
    g_assert(!(page_get_flags(addr) & PAGE_VALID));
    g_assert(get_segment_num() == 0 && get_lib_num() == 0);
    option_aot = 1;
}

int main(int argc, char **argv)
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
    qemu_real_host_page_size = page_size;
    qemu_host_page_mask = ~(page_size - 1);

    page_flags = g_hash_table_new(g_direct_hash, g_direct_equal);
    segment_tree_init();
    lib_tree_init();
    fd = mkstemp(path);
    g_assert(fd >= 0);
    close(fd);
    if (argc == 2) {
        if (!strcmp(argv[1], "--mmap-late-hint")) {
            test_mmap_late_segment(path, 0);
        } else if (!strcmp(argv[1], "--mmap-late-noreplace")) {
#if MAP_FIXED_NOREPLACE != 0
            test_mmap_late_segment(path, MAP_FIXED_NOREPLACE);
#else
            return 77;
#endif
        } else if (!strcmp(argv[1], "--shm-hint")) {
            test_shm_lifecycle(path, 0, false);
        } else if (!strcmp(argv[1], "--shm-noreplace")) {
#if MAP_FIXED_NOREPLACE != 0
            test_shm_lifecycle(path, MAP_FIXED_NOREPLACE, false);
#else
            return 77;
#endif
        } else if (!strcmp(argv[1], "--shm-disabled")) {
            test_shm_disabled();
        } else {
            g_assert(!strcmp(argv[1], "--shm-tail"));
            test_shm_lifecycle(path, 0, true);
        }
        unlink(path);
        g_hash_table_destroy(page_flags);
        return 0;
    }
    test_mmap_replacement(path);
    test_mmap_late_segment(path, 0);
#if MAP_FIXED_NOREPLACE != 0
    test_mmap_late_segment(path, MAP_FIXED_NOREPLACE);
#endif
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
    g_hash_table_destroy(page_flags);
    return 0;
}
