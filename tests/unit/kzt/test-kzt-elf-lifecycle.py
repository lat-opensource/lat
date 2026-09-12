#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Test the production public-ELF reclamation bodies at an ownership seam.

The mock RCU queue deliberately retains callbacks until the test drains it.
Guest dlopen/close tests separately exercise the real loader and RCU runtime.
"""
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2] + '\n'


PRELUDE = r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <elf.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", \
    __LINE__, #x); exit(1); } } while (0)
#define TARGET_PAGE_SIZE UINT64_C(4096)
#define TARGET_PAGE_MASK (~(TARGET_PAGE_SIZE - 1))
#define PAGE_VALID 8
#define LOG_INFO 0
#define printf_log(...) ((void)0)
#define g_new(t, n) ((t *)calloc(n, sizeof(t)))
#define g_free free
#define box_realloc realloc
#define qatomic_xchg(p, v) __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST)
#define qatomic_read(p) __atomic_load_n(p, __ATOMIC_SEQ_CST)
#define qatomic_set(p, v) __atomic_store_n(p, v, __ATOMIC_SEQ_CST)
struct rcu_head { void *next; };
typedef struct elfheader_s {
    size_t numPHEntries;
    Elf64_Phdr *PHEntries;
    uintptr_t public_link_map, public_load_bias;
    void *lib;
} elfheader_t;
struct malloc_map { elfheader_t *h; };
typedef struct {
    elfheader_t **elfs;
    int elfcap, elfsize, mallocmapsize;
    struct malloc_map **mallocmaps;
} box64context_t;
typedef struct { int running; } CPUX86State;
typedef struct { uintptr_t live_maps[8]; unsigned live_map_count; }
    kzt_public_loader_observer_t;
typedef int kzt_public_loader_result_t;
#define KZT_PUBLIC_LOADER_OK 0
#define KZT_PUBLIC_LOADER_BUSY 1
static kzt_public_loader_observer_t kzt_public_loader_observer;
static int kzt_public_loader_reader;
static int snapshot_result, allow_exclusive = 1, exclusive, mmap_locked;
static int kicked, freed, delayed_count;
static uintptr_t mapped_page;
static box64context_t context, *my_context = &context;
static elfheader_t *elf_header;
static bool kzt_header_cleanup_pending;
static void *delayed[32];
static void (*delayed_fn[32])(void *);
static bool kzt_public_loader_observer_has_map(
    const kzt_public_loader_observer_t *o, uintptr_t key) {
    for (unsigned i = 0; i < o->live_map_count; i++) {
        if (o->live_maps[i] == key) return true;
    }
    return false;
}
static int page_get_flags(uintptr_t address) {
    CHECK(mmap_locked); return address == mapped_page ? PAGE_VALID : 0;
}
static bool start_exclusive_timeout(int timeout) {
    CHECK(!mmap_locked && !exclusive && timeout > 0);
    if (!allow_exclusive) return false;
    exclusive = 1; return true;
}
static void end_exclusive(void) {
    CHECK(exclusive && !mmap_locked); exclusive = 0;
}
static void mmap_lock(void) { CHECK(!mmap_locked); mmap_locked = 1; }
static void mmap_unlock(void) { CHECK(mmap_locked); mmap_locked = 0; }
static int kzt_loader_snapshot_visit(void *object, void *opaque) { return 0; }
static int kzt_public_loader_observer_refresh(
    kzt_public_loader_observer_t *o, void *reader, void *visit, void *opaque) {
    CHECK(exclusive && mmap_locked); return snapshot_result;
}
static CPUX86State *env_cpu(CPUX86State *env) { return env; }
static void cpu_exit(CPUX86State *env) { CHECK(mmap_locked); kicked++; }
static void FreeElfHeader(elfheader_t **head) {
    CHECK(*head); free((*head)->PHEntries); free(*head); *head = NULL; freed++;
}
static void defer(void *item, void (*fn)(void *)) {
    CHECK(exclusive && mmap_locked && delayed_count < 32);
    delayed[delayed_count] = item; delayed_fn[delayed_count++] = fn;
}
#define call_rcu(p, fn, member) defer(p, (void (*)(void *))fn)
static void drain(void) {
    for (int i = 0; i < delayed_count; i++) delayed_fn[i](delayed[i]);
    delayed_count = 0;
}
'''

TEST = r'''
static elfheader_t *new_header(uintptr_t key, uintptr_t page) {
    elfheader_t *h = calloc(1, sizeof(*h));
    CHECK(h);
    h->public_link_map = key; h->public_load_bias = page;
    h->numPHEntries = 1; h->PHEntries = calloc(1, sizeof(Elf64_Phdr));
    CHECK(h->PHEntries);
    h->PHEntries[0].p_type = PT_LOAD;
    h->PHEntries[0].p_memsz = TARGET_PAGE_SIZE * 2;
    AddElfHeader(my_context, h);
    return h;
}
static void request(CPUX86State *env) {
    mmap_lock(); kzt_request_header_cleanup(env); mmap_unlock();
}
int main(void) {
    CPUX86State cpu = {0};
    elfheader_t *main_h = new_header(0, 0x10000);
    elfheader_t *live_h = new_header(1, 0x20000);
    elfheader_t *dead_h = new_header(2, 0x30000);
    elfheader_t *lib_h = new_header(3, 0x40000);
    elfheader_t *malloc_h = new_header(4, 0x50000);
    struct malloc_map malloc_owner = { .h = malloc_h };
    struct malloc_map *owners[] = { &malloc_owner };
    elf_header = main_h; lib_h->lib = (void *)1;
    context.mallocmaps = owners; context.mallocmapsize = 1;
    kzt_public_loader_observer.live_maps[0] = 1;
    kzt_public_loader_observer.live_map_count = 1;

    /* A partly mapped object must retain its PLT metadata. */
    mapped_page = 0x31000;
    request(&cpu); CHECK(!kzt_header_cleanup_pending);
    mapped_page = 0;
    request(&cpu); CHECK(kzt_header_cleanup_pending && kicked == 1);
    allow_exclusive = 0;
    kzt_reclaim_unloaded_headers();
    CHECK(!kzt_header_cleanup_pending && !freed && !delayed_count);
    /* Another loader event retries existing retired candidates. */
    request(&cpu); allow_exclusive = 1;
    snapshot_result = KZT_PUBLIC_LOADER_BUSY;
    kzt_reclaim_unloaded_headers();
    CHECK(!freed && !delayed_count && context.elfs[2] == dead_h);
    request(&cpu); snapshot_result = KZT_PUBLIC_LOADER_OK;
    /* The fresh safe-point snapshot may show link_map address reuse. */
    kzt_public_loader_observer.live_maps[1] = 2;
    kzt_public_loader_observer.live_map_count = 2;
    kzt_reclaim_unloaded_headers();
    CHECK(!freed && !delayed_count && context.elfs[2] == dead_h);
    kzt_public_loader_observer.live_map_count = 1;
    request(&cpu); kzt_reclaim_unloaded_headers();
    CHECK(context.elfs[2] == NULL && delayed_count == 1 && freed == 0);
    CHECK(dead_h->PHEntries[0].p_type == PT_LOAD); /* Borrow valid until RCU. */
    CHECK(context.elfs[0] == main_h && context.elfs[1] == live_h);
    CHECK(context.elfs[3] == lib_h && context.elfs[4] == malloc_h);
    drain(); CHECK(freed == 1);
    for (unsigned i = 0; i < 2000; i++) {
        elfheader_t *h = new_header(100 + i, 0x60000);
        CHECK(context.elfsize == 5 && context.elfs[2] == h);
        request(&cpu); kzt_reclaim_unloaded_headers(); drain();
        CHECK(context.elfs[2] == NULL && context.elfsize == 5);
    }
    CHECK(context.elfcap == 16 && freed == 2001);
    for (int i = 0; i < context.elfsize; i++) {
        if (context.elfs[i]) FreeElfHeader(&context.elfs[i]);
    }
    free(context.elfs);
    puts("KZT ELF retirement: live/borrowed guards, safe-point retry, RCU and slot reuse passed");
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('repo', type=Path)
    parser.add_argument('--cc', default='cc')
    args = parser.parse_args()
    source = (args.repo / 'target/i386/latx/context/myalign.c').read_text()
    context = (args.repo / 'target/i386/latx/context/box64context.c').read_text()
    fields = source[source.index('typedef struct KZTRetiredElfHeader'):]
    fields = fields[:fields.index('} KZTRetiredElfHeader;') +
                    len('} KZTRetiredElfHeader;')] + '\n'
    body = PRELUDE + fields
    for signature in ('static bool kzt_public_header_is_unmapped(',
                      'static bool kzt_public_header_can_retire(',
                      'static void kzt_free_retired_header(',
                      'void kzt_reclaim_unloaded_headers(',
                      'static void kzt_request_header_cleanup('):
        body += function(source, signature)
    body += function(context, 'int AddElfHeader(') + TEST
    with tempfile.TemporaryDirectory(prefix='latx-elf-lifecycle-') as temp:
        fixture = Path(temp) / 'fixture.c'
        fixture.write_text(body)
        for release in (False, True):
            output = Path(temp) / ('release' if release else 'debug')
            command = shlex.split(args.cc) + ['-std=gnu11', '-Wall', '-O2']
            if release:
                command += ['-DNDEBUG']
            subprocess.run(command + [str(fixture), '-o', str(output)], check=True)
            subprocess.run([str(output)], check=True)


if __name__ == '__main__':
    main()
