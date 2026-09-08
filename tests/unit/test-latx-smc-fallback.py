#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise allocation failure through production page_unprotect and signal dispatch.

Page metadata, the TB index and CPU state are modelled; allocation, protection,
shadow creation, SMC profiling, invalidation/unwind and store interpretation use
production bodies and real Linux mappings. This seam does not replace guest
execution on a LoongArch translator.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2] + '\n'


PRELUDE = r'''
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)
#ifndef TARGET_PAGE_SIZE
#define TARGET_PAGE_SIZE 4096
#endif
#define TARGET_PAGE_MASK (~(uintptr_t)(TARGET_PAGE_SIZE - 1))
#define PAGE_READ PROT_READ
#define PAGE_WRITE PROT_WRITE
#define PAGE_EXEC PROT_EXEC
#define PAGE_BITS (PAGE_READ | PAGE_WRITE | PAGE_EXEC)
#define PAGE_VALID 8
#define PAGE_WRITE_ORG 16
#define PAGE_ANON 128
TB_FLAGS
#define TARGET_HAS_PRECISE_SMC 1
#define CONFIG_LATX_SMC_OPT 1
#define CONFIG_LOONGARCH_NEW_WORLD 1
#define HOST_PAGE_ALIGN(x) (((x) + qemu_host_page_size - 1) & qemu_host_page_mask)
#define g_assert_not_reached() CHECK(0)
#define qemu_log_mask(...) ((void)0)
typedef uintptr_t target_ulong;
typedef uintptr_t abi_ulong;
typedef uintptr_t tb_page_addr_t;
typedef struct { int flags; } PageFlagsNode;
typedef struct { void *p_addr; int64_t access_off; int is_shmm; } ShadowPageDesc;
typedef struct {
    int flags, size, slot;
    uintptr_t pc;
    unsigned smc_data;
    bool linked;
} TranslationBlock;
typedef TranslationBlock *PageForEachNext;
typedef struct { unsigned cflags_next_tb; } CPUState;
static CPUState cpu_state, *current_cpu = &cpu_state;
static size_t qemu_host_page_size;
static uintptr_t qemu_host_page_mask, guest_base;
static PageFlagsNode pages[32];
static ShadowPageDesc shadows[32];
static TranslationBlock tbs[32];
static int locks, fail_kind, fail_host, shared_mode;
static int attempts, full_invalidations, selective_invalidations, private_shadows;
static int restore_count, retrans_inserts;
#ifdef CONFIG_LATX_AOT
static int option_aot = 1;
static unsigned smc_pages;
#define PAGE_SMC 1
static void page_set_page_state_range(uintptr_t start, uintptr_t end, int state) {
    CHECK(end == start + qemu_host_page_size && state == PAGE_SMC);
    CHECK(start >= guest_base && end <= guest_base + 2 * qemu_host_page_size);
    smc_pages |= 1u << ((start - guest_base) / qemu_host_page_size);
}
static void *segment_tree_winepe_lookup(uintptr_t address) {
    (void)address;
    return NULL;
}
#endif
static TranslationBlock active_tb;
static jmp_buf exit_env;
static void mmap_lock(void) { locks++; }
static void mmap_unlock(void) { CHECK(locks > 0); locks--; }
static void *g2h_untagged(uintptr_t address) { return (void *)address; }
static uintptr_t h2g(uintptr_t address) { return address; }
static unsigned index_of(uintptr_t address) {
    CHECK(address >= guest_base && address < guest_base + 2 * qemu_host_page_size);
    return (address - guest_base) / TARGET_PAGE_SIZE;
}
static PageFlagsNode *pageflags_find(uintptr_t address, uintptr_t last) {
    CHECK(address == last);
    return &pages[index_of(address)];
}
static int page_get_flags(uintptr_t address) { return pageflags_find(address, address)->flags; }
static ShadowPageDesc *page_get_target_data(uintptr_t address) {
    ShadowPageDesc *spd = &shadows[index_of(address)];
    return spd->p_addr ? spd : NULL;
}
static ShadowPageDesc *set_shadow_page(uintptr_t address, void *ptr, int64_t off) {
    ShadowPageDesc *spd = &shadows[index_of(address)];
    CHECK(!spd->p_addr);
    spd->p_addr = ptr; spd->access_off = off;
    return spd;
}
static void pageflags_set_clear(uintptr_t start, uintptr_t last, int set, int clear) {
    for (uintptr_t addr = start; addr <= last; addr += TARGET_PAGE_SIZE) {
        PageFlagsNode *p = pageflags_find(addr, addr);
        p->flags = (p->flags | set) & ~clear;
    }
}
static TranslationBlock *tcg_tb_lookup(uintptr_t pc) { (void)pc; return &active_tb; }
static int tb_cflags(TranslationBlock *tb) { return tb->flags; }
static unsigned curr_cflags(CPUState *cpu) { (void)cpu; return 0; }
#define assert_memory_lock() CHECK(locks > 0)
static void smc_retrans_tree_init(void) {}
static bool smc_retrans_insert(TranslationBlock *tb) {
    CHECK(tb == &active_tb);
    return retrans_inserts++ == 0;
}
static void tb_phys_invalidate__locked(TranslationBlock *tb) {
    assert_memory_lock();
    tb->flags |= CF_INVALID;
    tb->linked = false;
}
static void cpu_restore_state_from_tb(CPUState *cpu, TranslationBlock *tb,
                                     uintptr_t pc, bool will_exit) {
    CHECK(cpu == current_cpu && tb == &active_tb && tb->linked);
    CHECK(pc == 0x1000 && will_exit);
    restore_count++;
}
static bool jrra_preserve_current_tb_head(TranslationBlock *tb,
        TranslationBlock *current, bool modified, uint32_t *inst) {
    (void)tb; (void)current; (void)modified; (void)inst;
    return false;
}
static void jrra_restore_current_tb_head(TranslationBlock *tb,
        TranslationBlock *current, bool modified, uint32_t inst) {
    (void)tb; (void)current; (void)modified; (void)inst;
    CHECK(0);
}
static TranslationBlock *next_tb(int slot, uintptr_t start, uintptr_t end) {
    for (; slot <= 32; slot++) {
        TranslationBlock *tb = slot == 32 ? &active_tb : &tbs[slot];
        if (tb->linked && tb->pc < end && tb->pc + tb->size > start)
            return tb;
    }
    return NULL;
}
/* Compute the successor before invalidation removes the current index entry. */
#define PAGE_FOR_EACH_TB(start, end, unused, tb, next) \
    for (tb = next_tb(0, start, end), \
         next = tb ? next_tb(tb->slot + 1, start, end) : NULL; \
         tb; tb = next, \
         next = tb ? next_tb(tb->slot + 1, start, end) : NULL)
bool production_tb_invalidate_phys_page_unwind(uintptr_t, uintptr_t, int *);
static bool tb_invalidate_phys_page_unwind(uintptr_t addr, uintptr_t pc, int *count) {
    if (count) selective_invalidations++;
    else full_invalidations++;
    return production_tb_invalidate_phys_page_unwind(addr, pc, count);
}
static int latx_smc_shmm(void) { return shared_mode & 2; }
static void latx_smc_shmm_disable(void) { shared_mode &= ~2; }
static int latx_smc_use_store_helper(void) { return shared_mode & 4; }
static int allocation_fails(void) {
    return attempts == fail_host + 1;
}
static int test_memfd_create(const char *name, unsigned flags) {
    attempts++;
    if (fail_kind == 1 && allocation_fails()) { errno = EMFILE; return -1; }
    return memfd_create(name, flags);
}
static int test_ftruncate(int fd, off_t size) {
    if (fail_kind == 2 && allocation_fails()) { errno = ENOSPC; return -1; }
    return ftruncate(fd, size);
}
static void *test_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off) {
    if (!addr && (flags & MAP_SHARED) && fail_kind == 3 && allocation_fails()) {
        errno = ENOMEM; return MAP_FAILED;
    }
    if (flags & MAP_PRIVATE) private_shadows++;
    return mmap(addr, len, prot, flags, fd, off);
}
/* Only integer stores are exercised; FP context access must never be reached. */
typedef struct { uintptr_t pc; uint64_t regs[32]; } TestContext;
#define ucontext_t TestContext
#define UC_PC(uc) ((uc)->pc)
#define UC_GR(uc) ((uc)->regs)
struct extctx_layout { int unused; };
static void parse_extcontext(TestContext *uc, struct extctx_layout *ext) { (void)uc; (void)ext; }
#define UC_GET_FTOP(ext, type) 0
#define UC_GET_FPR(ext, fd, type) (abort(), (type)0)
#define UC_GET_LSX(ext, fd, lane, type) (abort(), (type)0)
static void clear_helper_retaddr(void) {}
static _Noreturn void cpu_exit_tb_from_sighandler(void *cpu, void *old_set) {
    (void)cpu; (void)old_set; CHECK(!locks); longjmp(exit_env, 1);
}
#define memfd_create test_memfd_create
#define ftruncate test_ftruncate
#define mmap test_mmap
'''

TEST = r'''
#undef memfd_create
#undef ftruncate
#undef mmap
static int dispatch(uintptr_t address, int size, TestContext *uc, uint32_t inst) {
    siginfo_t *info = NULL;
    void *cpu = NULL, *old_set = NULL;
    uintptr_t pc = uc->pc, guest_store_address = address;
    int emu_store = size, *emu = &emu_store;
    if (!size) emu = NULL;
SIGNAL_DISPATCH
    CHECK(0);
    return 0;
}
static void run_case(int kind, int failed_host, int cross, int current, int mode,
                     unsigned initial_count) {
    guest_base = (uintptr_t)mmap(NULL, 2 * qemu_host_page_size, PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK((void *)guest_base != MAP_FAILED);
    memset((void *)guest_base, 0x31, 2 * qemu_host_page_size);
    memset(shadows, 0, sizeof(shadows));
    memset(pages, 0, sizeof(pages));
    memset(tbs, 0, sizeof(tbs));
    unsigned n = 2 * qemu_host_page_size / TARGET_PAGE_SIZE;
    for (unsigned i = 0; i < n; i++) {
        pages[i].flags = PAGE_READ | PAGE_WRITE_ORG | PAGE_VALID | PAGE_ANON;
        tbs[i] = (TranslationBlock) {
            .pc = guest_base + i * TARGET_PAGE_SIZE + 32,
            .size = 4, .slot = i, .linked = true,
        };
    }
    CHECK(!mprotect((void *)guest_base, 2 * qemu_host_page_size, PROT_READ));
    fail_kind = kind; fail_host = failed_host; shared_mode = mode;
    attempts = full_invalidations = selective_invalidations = private_shadows = 0;
    restore_count = retrans_inserts = 0;
    cpu_state.cflags_next_tb = 0;
#ifdef CONFIG_LATX_AOT
    smc_pages = 0;
#endif
    int width = cross ? 8 : 1;
    uintptr_t address = guest_base;
    if (cross) address += (cross == 1 ? qemu_host_page_size : TARGET_PAGE_SIZE) - 4;
    active_tb = (TranslationBlock) {
        .pc = current < 0 ? guest_base + 2 * qemu_host_page_size :
              (current == 1 ? guest_base + qemu_host_page_size : address),
        .size = current == 2 ? 8 : 4,
        .slot = 32, .linked = true, .smc_data = initial_count,
    };
    uint32_t inst = ((cross ? 0xa7 : 0xa4) << 22) | 5;
    TestContext uc = { .pc = 0x1000, .regs = { [5] = 0x123456789abcdef0ULL } };
    int exited = setjmp(exit_env);
    if (!exited) CHECK(dispatch(address, kind == 4 ? -1 : kind == 5 ? 0 : width,
                               &uc, inst) == 1);
    CHECK(locks == 0);
#ifdef CONFIG_LATX_AOT
    CHECK(smc_pages == (cross == 1 ? 3u : 1u));
#endif
    if (exited != (current >= 0) || restore_count != (current >= 0)) {
        fprintf(stderr, "mode=%d count=%u cross=%d current=%d exit=%d restore=%d next=%u retrans=%d\n",
                mode, initial_count, cross, current, exited, restore_count,
                cpu_state.cflags_next_tb, retrans_inserts);
    }
    CHECK(exited == (current >= 0));
    CHECK(restore_count == (current >= 0));
    CHECK(cpu_state.cflags_next_tb == (current >= 0 ? 1 : 0));
    CHECK(retrans_inserts == ((mode & 4) && initial_count == TBSMC_OPT_THRESHOLD));
    CHECK(active_tb.smc_data == initial_count +
          (((mode & 4) && initial_count < TBSMC_OPT_THRESHOLD) ? 1 : 0));
    if (kind) {
        CHECK(!private_shadows && !selective_invalidations);
        CHECK(full_invalidations == (int)(cross == 1 ? n : n / 2));
        CHECK(exited == (current >= 0));
        CHECK(uc.pc == 0x1000);
        for (unsigned i = 0; i < (cross == 1 ? n : n / 2); i++) {
            CHECK(!tbs[i].linked && (pages[i].flags & PAGE_WRITE));
        }
        /* Same host store is retried, or regenerated after current TB exit. */
        memcpy((void *)address, &uc.regs[5], width);
    } else {
        CHECK(!exited && selective_invalidations == 1 && !full_invalidations);
        CHECK(uc.pc == 0x1004);
        CHECK(!private_shadows && tbs[0].linked);
    }
    CHECK(!memcmp((void *)address, &uc.regs[5], width));
    for (unsigned h = 0; h < 2; h++) {
        ShadowPageDesc *spd = &shadows[h * qemu_host_page_size / TARGET_PAGE_SIZE];
        if (spd->p_addr) CHECK(!munmap(spd->p_addr, qemu_host_page_size));
    }
    CHECK(!munmap((void *)guest_base, 2 * qemu_host_page_size));
}
int main(void) {
    qemu_host_page_size = sysconf(_SC_PAGESIZE);
    qemu_host_page_mask = ~(qemu_host_page_size - 1);
    CHECK(qemu_host_page_size >= TARGET_PAGE_SIZE);
    CHECK(2 * qemu_host_page_size / TARGET_PAGE_SIZE <= 32);
    /* Controls for the shared helper's normal and forced invalidation users. */
    for (int current = -1; current <= 0; current++) {
        run_case(4, 0, 0, current, 0, 0);
        run_case(5, 0, 0, current, 0, 0);
    }
    puts("SMC whole-page controls: ordinary and forced invalidation passed");
    /* Faulting PC is inside a TB that writes its own translated code. */
    run_case(1, 0, 0, 0, 6, 254);
    const int modes[] = { 2, 3, 6, 7 };
    for (unsigned m = 0; m < sizeof(modes) / sizeof(modes[0]); m++) {
        int mode = modes[m];
        for (unsigned count = 253; count <= 255; count++) {
            run_case(0, 0, 0, -1, mode, count);
            for (int kind = 1; kind <= 3; kind++) {
                for (int current = -1; current <= 0; current++) {
                    run_case(kind, 0, 0, current, mode, count);
                    if (qemu_host_page_size > TARGET_PAGE_SIZE) {
                        run_case(kind, 0, 2, current, mode, count);
                    }
                }
                for (int failed_host = 0; failed_host < 2; failed_host++) {
                    for (int current = -1; current <= 2; current++) {
                        run_case(kind, failed_host, 1, current, mode, count);
                    }
                }
            }
        }
    }
    puts("SMC fallback: allocation failures, cross-host retry, store-helper thresholds and precise unwind passed");
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('repo', type=Path)
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--source', type=Path,
                        help='saved translate-all.c for RED comparison')
    args = parser.parse_args()
    source = (args.source or args.repo / 'accel/tcg/translate-all.c').read_text()
    user = (args.repo / 'accel/tcg/user-exec.c').read_text()
    header = (args.repo / 'include/exec/exec-all.h').read_text()
    flags = '\n'.join(line for line in header.splitlines() if any(
        line.startswith('#define ' + name + ' ') for name in
        ('CF_INVALID', 'CF_COUNT_MASK', 'TBSMC_COUNT_MASK',
         'TBSMC_OPT_THRESHOLD', 'TBSMC_OPTED_MASK')))
    macros_start = source.index('#define SMC_CREATE_SHADOW_PAGE_SHMM(')
    macros_end = source.index('/* TODO: handle cross page */', macros_start)
    dispatch_start = user.index('        switch (page_unprotect(guest_store_address, pc, emu)) {')
    dispatch_end = user.index('\n    }\n', dispatch_start)
    bodies = ''.join(function(source, signature) for signature in (
        'static bool is_shadow_page(', 'static bool is_shadow_page_shmm(',
        'bool page_is_shadow_not_shmm(', 'static inline int smc_shmm_check_page_anon(',
        'static int smc_create_shadow_page_shmm(', 'void create_shadow_page_chunk('))
    bodies += function(source, 'static inline bool tb_is_covered(')
    bodies += '#define tb_invalidate_phys_page_unwind production_tb_invalidate_phys_page_unwind\n'
    bodies += function(source, 'bool tb_invalidate_phys_page_unwind(')
    bodies += '#undef tb_invalidate_phys_page_unwind\n'
    bodies += function(source, 'static void smc_retrans_triger(')
    if 'static bool page_unprotect_host_page(' in source:
        bodies += function(source, 'static bool page_unprotect_host_page(')
    bodies += source[macros_start:macros_end] + function(source, 'int page_unprotect(')
    bodies += ''.join(function(user, signature) for signature in (
        'static void write_by_byte(', 'static void smc_store_shadow_page(',
        'static int smc_store_interpret('))
    unit = PRELUDE.replace('TB_FLAGS', flags) + bodies + TEST.replace('SIGNAL_DISPATCH', '#ifdef CONFIG_LATX_SMC_OPT\n' + user[dispatch_start:dispatch_end])
    with tempfile.TemporaryDirectory(prefix='latx-smc-fallback-') as directory:
        path = Path(directory)
        (path / 'test.c').write_text(unit)
        page_sizes = sorted({4096, os.sysconf('SC_PAGESIZE')})
        for flags in ([f'-DTARGET_PAGE_SIZE={size}', *release, *aot]
                      for size in page_sizes
                      for release in ([], ['-DNDEBUG'])
                      for aot in ([], ['-DCONFIG_LATX_AOT'])):
            subprocess.run(shlex.split(args.cc) + ['-std=gnu11', '-O1', '-g',
                '-Wall', '-Wextra', '-Wno-unused-function', '-Wno-unused-variable',
                '-Wno-unused-parameter', '-Wno-sign-compare', *flags, str(path / 'test.c'),
                '-o', str(path / 'test')], check=True)
            subprocess.run([str(path / 'test')], check=True, timeout=30)


if __name__ == '__main__':
    main()
