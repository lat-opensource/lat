#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the production masked CAS without a guest runtime."""
import pathlib
import os
import shlex
import subprocess
import sys
import tempfile

source = (pathlib.Path(sys.argv[1]) / 'accel/tcg/translate-all.c').read_text()
start = source.index('static bool shadow_small_atomic(')
common_start = source.index('static bool shadow_small_atomic(')
end = source.index('\n}\n', common_start) + 3
prelude = r'''
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#define qatomic_read(p) __atomic_load_n(p, __ATOMIC_RELAXED)
static uint64_t cas(uint64_t *p, uint64_t old, uint64_t value) {
    __atomic_compare_exchange_n(p, &old, value, false,
                               __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return old;
}
#define qatomic_cmpxchg cas
#define CHECK(x) do { if (!(x)) abort(); } while (0)
'''
test = r'''
static uint64_t words[2] __attribute__((aligned(16)));
#define word words[0]
static int split_offset;
static void *worker(void *unused) {
    uint64_t old;
    for (int i = 0; i < 5000; i++) {
        CHECK(shadow_small_atomic((intptr_t)&word + split_offset,
                                   2, 1, false, &old));
    }
    return NULL;
}
int main(void) {
    for (int size = 1; size <= 2; size++) {
        for (int off = 0; off < 16; off++) {
            for (int exchange = 0; exchange <= 1; exchange++) {
                words[0] = UINT64_C(0x8877665544332211);
                words[1] = UINT64_C(0x123456789abcdef0);
                unsigned char before[16], *bytes = (void *)words;
                for (int i = 0; i < 16; i++) before[i] = bytes[i];
                uint64_t old = UINT64_MAX;
                uint64_t mask = size == 1 ? 255 : 65535;
                bool ok = shadow_small_atomic((intptr_t)&word + off, size,
                                               0xffff, exchange, &old);
                if (size == 2 && (off & 7) == 7) {
                    CHECK(!ok && old == UINT64_MAX);
                    for (int i = 0; i < 16; i++) CHECK(bytes[i] == before[i]);
                    continue;
                }
                uint64_t expected = before[off];
                if (size == 2) expected |= (uint64_t)before[off + 1] << 8;
                CHECK(ok && old == expected);
                uint64_t value = (exchange ? 0xffff : old + 0xffff) & mask;
                before[off] = value;
                if (size == 2) before[off + 1] = value >> 8;
                for (int i = 0; i < 16; i++) CHECK(bytes[i] == before[i]);
            }
        }
    }
    pthread_t threads[4];
    for (int pass = 0; pass < 1; pass++) {
        unsigned char *bytes = (void *)words;
        split_offset = 1;
        for (int i = 0; i < 16; i++) bytes[i] = 0x5a;
        bytes[split_offset] = 0;
        bytes[split_offset + 1] = 0;
        for (int i = 0; i < 4; i++) {
            CHECK(!pthread_create(&threads[i], 0, worker, 0));
        }
        for (int i = 0; i < 4; i++) CHECK(!pthread_join(threads[i], 0));
        CHECK(bytes[split_offset] == (20000 & 255));
        CHECK(bytes[split_offset + 1] == (20000 >> 8));
        for (int i = 0; i < 16; i++) {
            if (i != split_offset && i != split_offset + 1) {
                CHECK(bytes[i] == 0x5a);
            }
        }
    }
    puts("PASS: shadow CAS results, neighboring bytes and concurrent updates");
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = pathlib.Path(directory)
    production = source[start:end]
    (path / 'test.c').write_text(prelude + production + test)
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) +
                   ['-std=gnu99', '-O2', '-pthread', str(path / 'test.c'),
                    '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True, timeout=15)
