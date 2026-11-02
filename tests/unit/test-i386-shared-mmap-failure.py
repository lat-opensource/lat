#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the production shadow allocator with real VMAs and injected failure."""
import argparse
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

PRELUDE = r'''
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)
typedef uintptr_t abi_ulong;
typedef struct { void *p_addr; int64_t access_off; int is_shmm; } ShadowPageDesc;
static size_t qemu_host_page_size;
static uintptr_t qemu_host_page_mask, guest;
static unsigned long reserved_va;
#define TARGET_PAGE_SIZE (qemu_host_page_size / 4)
#define PAGE_BITS 7
#define PAGE_VALID 8
#define PAGE_WRITE_ORG 16
#define PAGE_ANON 32
#define PAGE_OVERFLOW 64
#define PAGE_MEMSHARE 128
#define HOST_PAGE_ALIGN(x) (((x) + qemu_host_page_size - 1) & qemu_host_page_mask)
static int flags[8], fail_protect, active, fail_mapping, mmap_calls;
static ShadowPageDesc desc[8];
static void *staged[32];
static unsigned count;
static void auto_free(void *p) { free(*(void **)p); }
#define g_autofree __attribute__((cleanup(auto_free)))
#define g_try_new0(type, count) ((type *)calloc((count), sizeof(type)))
static bool have_mmap_lock(void) { return true; }
static unsigned index_of(uintptr_t p) {
    CHECK(p >= guest && p < guest + 2 * qemu_host_page_size);
    return (p - guest) / TARGET_PAGE_SIZE;
}
static int page_get_flags(uintptr_t p) { return flags[index_of(p)]; }
static ShadowPageDesc *page_get_target_data(uintptr_t p) {
    ShadowPageDesc *d = &desc[index_of(p)];
    return d->p_addr ? d : NULL;
}
static void *g2h_untagged(uintptr_t p) { return (void *)p; }
static int hostpage_exist_shadow_page(uintptr_t p) {
    for (unsigned i = index_of(p); i < index_of(p) + 4; i++)
        if (desc[i].p_addr) return 1;
    return 0;
}
static ShadowPageDesc *set_shadow_page(uintptr_t p, void *m, int64_t off) {
    ShadowPageDesc *d = &desc[index_of(p)];
    if (d->p_addr) munmap(d->p_addr, qemu_host_page_size);
    d->p_addr = m; d->access_off = off;
    return d;
}
static void pageflags_set_clear(uintptr_t start, uintptr_t last, int set, int clear) {
    for (uintptr_t p = start; p <= last; p += TARGET_PAGE_SIZE)
        flags[index_of(p)] = (flags[index_of(p)] | set) & ~clear;
}
static void *test_mmap(void *p, size_t len, int prot, int flags, int fd, off_t off) {
    if (!p && ++mmap_calls == fail_mapping) {
        errno = ENOMEM;
        return MAP_FAILED;
    }
    void *result = mmap(p, len, prot, flags, fd, off);
    if (!p && result != MAP_FAILED) {
        CHECK(count < 32); staged[count++] = result; active++;
    }
    return result;
}
static int test_munmap(void *p, size_t len) {
    int ret = munmap(p, len);
    if (!ret) for (unsigned i = 0; i < count; i++) {
        if (staged[i] == p) { staged[i] = NULL; active--; break; }
    }
    return ret;
}
static int test_mprotect(void *p, size_t len, int prot) {
    if (fail_protect && p == (void *)guest &&
        len == 2 * qemu_host_page_size && prot == PROT_NONE) {
        CHECK(mprotect(p, qemu_host_page_size, PROT_NONE) == 0);
        errno = ENOMEM;
        return -1;
    }
    return mprotect(p, len, prot);
}
#define mmap test_mmap
#define munmap test_munmap
#define mprotect test_mprotect
'''

TEST = r'''
#undef mmap
#undef munmap
#undef mprotect
static void permissions(uintptr_t addr, int writable) {
    FILE *f = fopen("/proc/self/maps", "r");
    char line[512], prot[5]; unsigned long start, end; bool found = false;
    CHECK(f);
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%lx-%lx %4s", &start, &end, prot) == 3 &&
            start <= addr && addr < end) {
            CHECK(prot[0] == 'r' && (prot[1] == 'w') == writable);
            found = true; break;
        }
    }
    fclose(f); CHECK(found);
}
static void run_failure(int smc, int allocation) {
    size_t len = 2 * qemu_host_page_size;
    void *old_alias = NULL;
    int fd = memfd_create("lat-shadow-rollback", 0);
    CHECK(fd >= 0 && ftruncate(fd, len + qemu_host_page_size) == 0);
    void *p = mmap(NULL, len, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(p != MAP_FAILED); guest = (uintptr_t)p;
    memset(flags, 0, sizeof(flags)); memset(desc, 0, sizeof(desc));
    memset(p, 0x5a, len); count = 0; active = 0;
    for (unsigned i = 0; i < 8; i++)
        flags[i] = PAGE_VALID | PAGE_ANON | PAGE_WRITE_ORG | PROT_READ | PROT_WRITE;
    if (smc) {
        old_alias = mmap(NULL, qemu_host_page_size, PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        CHECK(old_alias != MAP_FAILED);
        memset(old_alias, 0x5a, qemu_host_page_size);
        for (unsigned i = 0; i < 4; i++) {
            desc[i].p_addr = old_alias; desc[i].is_shmm = 1;
            desc[i].access_off = (uintptr_t)old_alias - guest;
            flags[i] &= ~PROT_WRITE;
        }
        CHECK(mprotect(p, qemu_host_page_size, PROT_READ) == 0);
    }
    fail_protect = !allocation;
    fail_mapping = allocation;
    mmap_calls = 0;
    CHECK(mmap_i386_shared_shadow(guest, guest + len,
          PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd,
          TARGET_PAGE_SIZE) == -1);
    CHECK(errno == ENOMEM && active == 0);
    permissions(guest, !smc); permissions(guest + qemu_host_page_size, 1);
    CHECK(*(volatile unsigned char *)p == 0x5a);
    CHECK(*(volatile unsigned char *)(guest + qemu_host_page_size) == 0x5a);
    for (unsigned i = 0; i < 8; i++) {
        CHECK(flags[i] & PAGE_ANON);
        CHECK(desc[i].p_addr == (smc && i < 4 ? old_alias : NULL));
    }
    if (!smc) {
        fail_protect = 0; fail_mapping = 0;
        CHECK(mmap_i386_shared_shadow(guest, guest + len, PROT_READ | PROT_WRITE,
              MAP_SHARED | MAP_FIXED, fd, TARGET_PAGE_SIZE) == 0);
        CHECK(!(flags[0] & PAGE_ANON));
        unsigned value = 0;
        *(unsigned *)((uintptr_t)desc[0].p_addr + TARGET_PAGE_SIZE) = 0x12345678;
        CHECK(pread(fd, &value, 4, TARGET_PAGE_SIZE) == 4 && value == 0x12345678);
        for (unsigned i = 0; i < 8; i++) {
            CHECK(test_munmap(desc[i].p_addr, qemu_host_page_size) == 0);
            desc[i].p_addr = NULL;
        }
        CHECK(active == 0);
    }
    if (smc) CHECK(munmap(old_alias, qemu_host_page_size) == 0);
    CHECK(munmap(p, len) == 0); CHECK(close(fd) == 0);
}
int main(void) {
    qemu_host_page_size = sysconf(_SC_PAGESIZE);
    qemu_host_page_mask = ~(qemu_host_page_size - 1);
    for (int smc = 0; smc < 2; smc++) {
        for (int allocation = 0; allocation <= 8; allocation++) {
            run_failure(smc, allocation);
        }
    }
    puts("PASS: partial protection failure restores VMAs, metadata and aliases");
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('repo', type=Path)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--cc', default='cc')
    args = parser.parse_args()
    if not sys.platform.startswith('linux'):
        return 77
    source = (args.source or args.repo / 'linux-user/mmap.c').read_text()
    start = source.index('static int\nmmap_i386_shared_shadow(')
    source = source[start:source.index('\n}', start) + 2]
    with tempfile.TemporaryDirectory(prefix='lat-shared-mmap-') as directory:
        root = Path(directory)
        (root / 'test.c').write_text(PRELUDE + source + TEST)
        for flags in ([], ['-DNDEBUG']):
            subprocess.run(shlex.split(args.cc) + [
                '-std=gnu11', '-O1', '-Wall', '-Wextra',
                '-Wno-unused-function', '-Wno-unused-parameter', *flags,
                str(root / 'test.c'), '-o', str(root / 'test')], check=True)
            subprocess.run([str(root / 'test')], check=True, timeout=15)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
