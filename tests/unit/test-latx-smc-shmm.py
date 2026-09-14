#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Production SMC backing ownership with real memfd, mmap, unmap and fork.

Only guest page metadata is simplified. Syscall wrappers inject failures before
side effects, except close(EINTR), which models Linux's already-closed fd.
The guest MAP_FIXED failure remains fatal and is intercepted for cleanup checks.
The standalone guest SMC probe separately verifies the translator entry path.
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
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s (errno=%d)\n", \
    __LINE__, #x, errno); exit(1); } } while (0)
#define TARGET_PAGE_SIZE 4096
#define PAGE_READ PROT_READ
#define PAGE_WRITE PROT_WRITE
static size_t qemu_host_page_size;
static uintptr_t qemu_host_page_mask;
typedef struct { void *p_addr; int64_t access_off; int is_shmm; } ShadowPageDesc;
static ShadowPageDesc descriptors[16];
static uintptr_t guest_base;
static int fail_kind, shadow_mappings, last_memfd = -1, expected_fatal;
static int reject_nonanon;
static jmp_buf fatal_env;
static void fatal(void) {
    CHECK(expected_fatal);
    longjmp(fatal_env, 1);
}
#define g_assert_not_reached() fatal()
static void *g2h_untagged(uintptr_t address) { return (void *)address; }
static int smc_shmm_check_page_anon(uint64_t start, int *prot) {
    CHECK(start == guest_base);
    *prot = PROT_READ | PROT_WRITE;
    return reject_nonanon;
}
static ShadowPageDesc *set_shadow_page(uint64_t address, void *ptr, int64_t off) {
    size_t index = (address - guest_base) / TARGET_PAGE_SIZE;
    CHECK(index < 16 && !descriptors[index].p_addr);
    descriptors[index].p_addr = ptr;
    descriptors[index].access_off = off;
    return &descriptors[index];
}
static void latx_smc_shmm_disable(void) {}
static int test_memfd_create(const char *name, unsigned flags) {
    if (fail_kind == 1) { errno = EMFILE; return -1; }
    last_memfd = memfd_create(name, flags);
    return last_memfd;
}
static int test_ftruncate(int fd, off_t size) {
    if (fail_kind == 2) { errno = ENOSPC; return -1; }
    return ftruncate(fd, size);
}
static void *test_mmap(void *addr, size_t len, int prot, int flags,
                       int fd, off_t offset) {
    if ((fail_kind == 3 && !addr) || (fail_kind == 4 && addr)) {
        errno = ENOMEM; return MAP_FAILED;
    }
    void *result = mmap(addr, len, prot, flags, fd, offset);
    if (result != MAP_FAILED && !addr) shadow_mappings++;
    return result;
}
static int test_munmap(void *addr, size_t len) {
    int result = munmap(addr, len);
    if (!result && addr != (void *)guest_base) shadow_mappings--;
    return result;
}
static int test_close(int fd) {
    int result = close(fd);
    if (fail_kind == 5 && !result) { errno = EINTR; return -1; }
    return result;
}
static int count_smc_fds(void) {
    DIR *dir = opendir("/proc/self/fd");
    struct dirent *entry;
    int count = 0;
    CHECK(dir);
    while ((entry = readdir(dir))) {
        char path[512], target[512];
        snprintf(path, sizeof(path), "/proc/self/fd/%s", entry->d_name);
        ssize_t n = readlink(path, target, sizeof(target) - 1);
        if (n < 0) continue;
        target[n] = 0;
        if (strstr(target, "smc_shmm")) count++;
    }
    CHECK(!closedir(dir));
    return count;
}
static void check_mapping_permissions(uintptr_t address, int writable) {
    FILE *maps = fopen("/proc/self/maps", "r");
    char line[512], permissions[5];
    unsigned long start, end;
    int found = 0;
    CHECK(maps);
    while (fgets(line, sizeof(line), maps)) {
        if (sscanf(line, "%lx-%lx %4s", &start, &end, permissions) == 3 &&
            start <= address && address < end) {
            CHECK(permissions[0] == 'r');
            CHECK((permissions[1] == 'w') == writable);
            CHECK(permissions[2] != 'x' && permissions[3] == 's');
            found = 1;
            break;
        }
    }
    CHECK(!fclose(maps) && found);
}
#define memfd_create test_memfd_create
#define ftruncate test_ftruncate
#define mmap test_mmap
#define munmap test_munmap
#define close test_close
'''

TEST = r'''
#undef memfd_create
#undef ftruncate
#undef mmap
#undef munmap
#undef close
static void new_guest(void) {
    void *p = mmap(NULL, qemu_host_page_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(p != MAP_FAILED);
    guest_base = (uintptr_t)p;
    memset(p, 0x31, qemu_host_page_size);
    memset(descriptors, 0, sizeof(descriptors));
}
static void release_guest(void) {
    CHECK(!munmap((void *)guest_base, qemu_host_page_size));
    if (descriptors[0].p_addr) {
        CHECK(!test_munmap(descriptors[0].p_addr, qemu_host_page_size));
    }
    memset(descriptors, 0, sizeof(descriptors));
    CHECK(shadow_mappings == 0 && count_smc_fds() == 0);
}
static void check_aliases(void) {
    CHECK(count_smc_fds() == 0 && shadow_mappings == 1);
    check_mapping_permissions(guest_base, 0);
    check_mapping_permissions((uintptr_t)descriptors[0].p_addr, 1);
    for (unsigned i = 0; i < qemu_host_page_size / TARGET_PAGE_SIZE; i++) {
        CHECK(descriptors[i].is_shmm);
        CHECK(descriptors[i].p_addr == descriptors[0].p_addr);
    }
    CHECK(*(unsigned char *)guest_base == 0x31);
    *(unsigned char *)descriptors[0].p_addr = 0x42;
    CHECK(*(unsigned char *)guest_base == 0x42);
}
static void fork_lifetime(void) {
    int sync[2], status;
    new_guest();
    CHECK(!smc_create_shadow_page_shmm(guest_base));
    check_aliases();
    CHECK(!pipe(sync));
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {
        char token;
        CHECK(!close(sync[1]));
        CHECK(read(sync[0], &token, 1) == 1);
        CHECK(!close(sync[0]));
        /* Parent has unmapped both aliases; the child's mappings own data. */
        CHECK(*(unsigned char *)guest_base == 0x42);
        *(unsigned char *)descriptors[0].p_addr = 0x53;
        CHECK(*(unsigned char *)guest_base == 0x53);
        release_guest();
        new_guest();
        CHECK(!smc_create_shadow_page_shmm(guest_base));
        check_aliases(); release_guest();
        _exit(0);
    }
    CHECK(!close(sync[0]));
    release_guest();
    CHECK(write(sync[1], "x", 1) == 1);
    CHECK(!close(sync[1]));
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
int main(void) {
    qemu_host_page_size = sysconf(_SC_PAGESIZE);
    qemu_host_page_mask = ~(qemu_host_page_size - 1);
    CHECK(qemu_host_page_size >= TARGET_PAGE_SIZE);
    CHECK(qemu_host_page_size / TARGET_PAGE_SIZE <= 16);
    for (int i = 0; i < 1024; i++) {
        new_guest();
        CHECK(!smc_create_shadow_page_shmm(guest_base));
        check_aliases(); release_guest();
    }
    fork_lifetime();
    reject_nonanon = 1; new_guest();
    CHECK(smc_create_shadow_page_shmm(guest_base) == -1);
    CHECK(!descriptors[0].p_addr && !shadow_mappings);
    CHECK(*(unsigned char *)guest_base == 0x31);
    *(unsigned char *)guest_base = 0x62;
    release_guest(); reject_nonanon = 0;
    for (fail_kind = 1; fail_kind <= 3; fail_kind++) {
        new_guest();
        CHECK(smc_create_shadow_page_shmm(guest_base) == -1);
        CHECK(!descriptors[0].p_addr && !shadow_mappings);
        CHECK(count_smc_fds() == 0);
        /* Preparatory failures must leave the original mapping usable. */
        CHECK(*(unsigned char *)guest_base == 0x31);
        *(unsigned char *)guest_base = 0x62;
        release_guest();
    }
    fail_kind = 4; expected_fatal = 1; new_guest();
    if (!setjmp(fatal_env)) {
        smc_create_shadow_page_shmm(guest_base);
        CHECK(0);
    }
    CHECK(count_smc_fds() == 0 && shadow_mappings == 0);
    /* MAP_FIXED failure is fatal: no assumption about the guest VMA. */
    CHECK(!munmap((void *)guest_base, qemu_host_page_size));
    expected_fatal = 0;
    fail_kind = 5; new_guest();
    CHECK(!smc_create_shadow_page_shmm(guest_base));
    check_aliases(); release_guest();
    fail_kind = 0;
    int saved_stdin = dup(STDIN_FILENO);
    CHECK(!close(STDIN_FILENO) || errno == EBADF);
    new_guest();
    CHECK(!smc_create_shadow_page_shmm(guest_base));
    CHECK(last_memfd == 0);
    CHECK(fcntl(STDIN_FILENO, F_GETFD) == -1 && errno == EBADF);
    check_aliases(); release_guest();
    if (saved_stdin != -1) {
        CHECK(dup2(saved_stdin, STDIN_FILENO) == STDIN_FILENO);
        CHECK(!close(saved_stdin));
    }
    puts("SMC backing: 1024 lifecycles, alias permissions, fork, fd=0 and failure cleanup passed");
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('repo', type=Path)
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--source', type=Path,
                        help='optional saved translate-all.c for RED comparison')
    args = parser.parse_args()
    source_path = args.source or args.repo / 'accel/tcg/translate-all.c'
    source = source_path.read_text()
    # Also compile the old append-only allocator when testing a baseline file.
    allocator = ''
    if 'static int smc_shmm_fd;' in source:
        start = source.index('static int smc_shmm_fd;')
        end = source.index('/*\n * Check if the guest pages', start)
        allocator = source[start:end]
    unit = PRELUDE + allocator + function(
        source, 'static int smc_create_shadow_page_shmm(') + TEST
    with tempfile.TemporaryDirectory(prefix='latx-smc-shmm-') as directory:
        path = Path(directory)
        (path / 'test.c').write_text(unit)
        for flags in ([], ['-DNDEBUG']):
            subprocess.run(shlex.split(args.cc) + [
                '-std=gnu11', '-O1', '-g', '-Wall', '-Wextra',
                '-Wno-unused-function', *flags, str(path / 'test.c'),
                '-o', str(path / 'test')], check=True)
            subprocess.run([str(path / 'test')], check=True, timeout=30)


if __name__ == '__main__':
    main()
