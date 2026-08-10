/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kzt_public_loader_observer.h"

#define FIXTURE_BASE UINT64_C(0x10000000)
#define FIXTURE_SIZE 0x10000

#define DYNAMIC_ADDR (FIXTURE_BASE + 0x1000)
#define R_DEBUG_ADDR (FIXTURE_BASE + 0x2000)
#define R_BRK_ADDR (FIXTURE_BASE + 0x2800)
#define MAP1_ADDR (FIXTURE_BASE + 0x3000)
#define MAP2_ADDR (FIXTURE_BASE + 0x3100)
#define MAP3_ADDR (FIXTURE_BASE + 0x3200)
#define NAME1_ADDR (FIXTURE_BASE + 0x4000)
#define NAME2_ADDR (FIXTURE_BASE + 0x4100)
#define NAME3_ADDR (FIXTURE_BASE + 0x4200)
#define ELF1_BASE (FIXTURE_BASE + 0x6000)
#define ELF2_BASE (FIXTURE_BASE + 0xb000)
#define TEST_PAGE_SIZE 0x1000
#define KZT_TEST_PT_GNU_RELRO UINT32_C(0x6474e552)

typedef struct test_x86_64_elf_header {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} test_x86_64_elf_header_t;

typedef struct test_x86_64_program_header {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} test_x86_64_program_header_t;

#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__,       \
                    __LINE__, #condition);                                   \
            exit(1);                                                         \
        }                                                                    \
    } while (0)

typedef struct fixture {
    uint8_t bytes[FIXTURE_SIZE];
    int reject_reads;
} fixture_t;

typedef struct visit_log {
    kzt_public_loader_object_t objects[8];
    size_t count;
    int fail;
} visit_log_t;

static void fixture_write(fixture_t *fixture,
                          uintptr_t guest_addr,
                          const void *src,
                          size_t size)
{
    size_t offset;

    CHECK(guest_addr >= FIXTURE_BASE);
    offset = (size_t)(guest_addr - FIXTURE_BASE);
    CHECK(offset <= FIXTURE_SIZE);
    CHECK(size <= FIXTURE_SIZE - offset);
    memcpy(fixture->bytes + offset, src, size);
}

static int fixture_read(uintptr_t guest_addr,
                        void *dst,
                        size_t size,
                        void *opaque)
{
    fixture_t *fixture = opaque;
    size_t offset;

    if (fixture->reject_reads || guest_addr < FIXTURE_BASE) {
        return -1;
    }
    offset = (size_t)(guest_addr - FIXTURE_BASE);
    if (offset > FIXTURE_SIZE || size > FIXTURE_SIZE - offset) {
        return -1;
    }
    memcpy(dst, fixture->bytes + offset, size);
    return 0;
}

static int record_visit(const kzt_public_loader_object_t *object,
                        void *opaque)
{
    visit_log_t *log = opaque;

    if (log->fail) {
        return -1;
    }
    CHECK(log->count < sizeof(log->objects) / sizeof(log->objects[0]));
    log->objects[log->count++] = *object;
    return 0;
}

static void write_dynamic(fixture_t *fixture, uintptr_t r_debug_addr)
{
    const kzt_x86_64_dynamic_entry_t dynamic[] = {
        { .tag = 1, .value = UINT64_C(0x55) },
        { .tag = KZT_X86_64_DT_DEBUG, .value = r_debug_addr },
        { .tag = KZT_X86_64_DT_NULL, .value = 0 },
    };

    fixture_write(fixture, DYNAMIC_ADDR, dynamic, sizeof(dynamic));
}

static void write_debug(fixture_t *fixture,
                        int32_t state,
                        uintptr_t map_addr)
{
    const kzt_x86_64_r_debug_t debug = {
        .version = 1,
        .map = map_addr,
        .brk = R_BRK_ADDR,
        .state = state,
        .loader_base = FIXTURE_BASE + 0x8000,
    };

    fixture_write(fixture, R_DEBUG_ADDR, &debug, sizeof(debug));
}

static void write_map(fixture_t *fixture,
                      uintptr_t map_addr,
                      uintptr_t load_bias,
                      uintptr_t name_addr,
                      uintptr_t dynamic_addr,
                      uintptr_t next,
                      uintptr_t previous)
{
    const kzt_x86_64_link_map_prefix_t map = {
        .load_bias = load_bias,
        .name = name_addr,
        .dynamic_addr = dynamic_addr,
        .next = next,
        .previous = previous,
    };

    fixture_write(fixture, map_addr, &map, sizeof(map));
}

static void write_elf_relro(fixture_t *fixture,
                            uintptr_t load_bias,
                            uintptr_t relro_vaddr,
                            size_t relro_size)
{
    test_x86_64_elf_header_t header = { 0 };
    const test_x86_64_program_header_t phdr = {
        .type = KZT_TEST_PT_GNU_RELRO,
        .vaddr = relro_vaddr,
        .memsz = relro_size,
    };

    header.ident[0] = 0x7f;
    header.ident[1] = 'E';
    header.ident[2] = 'L';
    header.ident[3] = 'F';
    header.ident[4] = 2;
    header.ident[5] = 1;
    header.ident[6] = 1;
    header.phoff = sizeof(header);
    header.ehsize = sizeof(header);
    header.phentsize = sizeof(phdr);
    header.phnum = 1;
    fixture_write(fixture, load_bias, &header, sizeof(header));
    fixture_write(fixture, load_bias + header.phoff, &phdr, sizeof(phdr));
}

static void write_elf_without_relro(fixture_t *fixture,
                                    uintptr_t load_bias)
{
    test_x86_64_elf_header_t header = { 0 };
    const test_x86_64_program_header_t phdr = {
        .type = 1,
        .vaddr = 0,
        .memsz = TEST_PAGE_SIZE,
    };

    header.ident[0] = 0x7f;
    header.ident[1] = 'E';
    header.ident[2] = 'L';
    header.ident[3] = 'F';
    header.ident[4] = 2;
    header.ident[5] = 1;
    header.ident[6] = 1;
    header.phoff = sizeof(header);
    header.ehsize = sizeof(header);
    header.phentsize = sizeof(phdr);
    header.phnum = 1;
    fixture_write(fixture, load_bias, &header, sizeof(header));
    fixture_write(fixture, load_bias + header.phoff, &phdr, sizeof(phdr));
}

static void setup_two_maps(fixture_t *fixture)
{
    memset(fixture, 0, sizeof(*fixture));
    write_dynamic(fixture, R_DEBUG_ADDR);
    write_debug(fixture, KZT_LOADER_DEBUG_CONSISTENT, MAP1_ADDR);
    write_map(fixture, MAP1_ADDR, UINT64_C(0x400000), NAME1_ADDR,
              FIXTURE_BASE + 0x5000, MAP2_ADDR, 0);
    write_map(fixture, MAP2_ADDR, UINT64_C(0x700000), NAME2_ADDR,
              FIXTURE_BASE + 0x5100, 0, MAP1_ADDR);
}

static void test_activate_reads_public_loader_state(void)
{
    fixture_t fixture;
    visit_log_t log = { 0 };
    kzt_public_loader_observer_t observer;
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };

    setup_two_maps(&fixture);
    kzt_public_loader_observer_reset(&observer);

    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_OK);
    CHECK(observer.active == 1);
    CHECK(observer.r_debug_addr == R_DEBUG_ADDR);
    CHECK(observer.r_brk_addr == R_BRK_ADDR);
    CHECK(observer.live_map_count == 2);
    CHECK(log.count == 2);
    CHECK(log.objects[0].link_map_addr == MAP1_ADDR);
    CHECK(log.objects[0].load_bias == UINT64_C(0x400000));
    CHECK(log.objects[0].name_addr == NAME1_ADDR);
    CHECK(log.objects[1].link_map_addr == MAP2_ADDR);
    CHECK(log.objects[1].previous_addr == MAP1_ADDR);
}

static void test_busy_state_defers_new_object(void)
{
    fixture_t fixture;
    visit_log_t log = { 0 };
    kzt_public_loader_observer_t observer;
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };

    setup_two_maps(&fixture);
    kzt_public_loader_observer_reset(&observer);
    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_OK);
    log.count = 0;

    write_map(&fixture, MAP2_ADDR, UINT64_C(0x700000), NAME2_ADDR,
              FIXTURE_BASE + 0x5100, MAP3_ADDR, MAP1_ADDR);
    write_map(&fixture, MAP3_ADDR, UINT64_C(0x900000), NAME3_ADDR,
              FIXTURE_BASE + 0x5200, 0, MAP2_ADDR);
    write_debug(&fixture, KZT_LOADER_DEBUG_ADD, MAP1_ADDR);

    CHECK(kzt_public_loader_observer_refresh(
              &observer, &reader, record_visit, &log) ==
          KZT_PUBLIC_LOADER_BUSY);
    CHECK(log.count == 0);
    CHECK(observer.live_map_count == 2);

    write_debug(&fixture, KZT_LOADER_DEBUG_CONSISTENT, MAP1_ADDR);
    CHECK(kzt_public_loader_observer_refresh(
              &observer, &reader, record_visit, &log) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(log.count == 1);
    CHECK(log.objects[0].link_map_addr == MAP3_ADDR);
    CHECK(observer.live_map_count == 3);
}

static void test_deleted_address_can_be_observed_again(void)
{
    fixture_t fixture;
    visit_log_t log = { 0 };
    kzt_public_loader_observer_t observer;
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };

    setup_two_maps(&fixture);
    kzt_public_loader_observer_reset(&observer);
    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_OK);

    write_map(&fixture, MAP1_ADDR, UINT64_C(0x400000), NAME1_ADDR,
              FIXTURE_BASE + 0x5000, 0, 0);
    log.count = 0;
    CHECK(kzt_public_loader_observer_refresh(
              &observer, &reader, record_visit, &log) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(log.count == 0);
    CHECK(observer.live_map_count == 1);

    write_map(&fixture, MAP1_ADDR, UINT64_C(0x400000), NAME1_ADDR,
              FIXTURE_BASE + 0x5000, MAP2_ADDR, 0);
    write_map(&fixture, MAP2_ADDR, UINT64_C(0xa00000), NAME3_ADDR,
              FIXTURE_BASE + 0x5300, 0, MAP1_ADDR);
    CHECK(kzt_public_loader_observer_refresh(
              &observer, &reader, record_visit, &log) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(log.count == 1);
    CHECK(log.objects[0].link_map_addr == MAP2_ADDR);
    CHECK(log.objects[0].load_bias == UINT64_C(0xa00000));
}

static void test_cycle_does_not_replace_last_complete_snapshot(void)
{
    fixture_t fixture;
    visit_log_t log = { 0 };
    kzt_public_loader_observer_t observer;
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };

    setup_two_maps(&fixture);
    kzt_public_loader_observer_reset(&observer);
    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_OK);

    write_map(&fixture, MAP2_ADDR, UINT64_C(0x700000), NAME2_ADDR,
              FIXTURE_BASE + 0x5100, MAP1_ADDR, MAP1_ADDR);
    log.count = 0;
    CHECK(kzt_public_loader_observer_refresh(
              &observer, &reader, record_visit, &log) ==
          KZT_PUBLIC_LOADER_CYCLE);
    CHECK(log.count == 0);
    CHECK(observer.live_map_count == 2);
    CHECK(observer.live_maps[0] == MAP1_ADDR);
    CHECK(observer.live_maps[1] == MAP2_ADDR);
}

static void test_preprotected_object_is_not_replayed(void)
{
    fixture_t fixture;
    visit_log_t log = { 0 };
    kzt_public_loader_observer_t observer;
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };

    setup_two_maps(&fixture);
    kzt_public_loader_observer_reset(&observer);
    CHECK(kzt_public_loader_observer_remember(&observer, MAP1_ADDR) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_OK);
    CHECK(log.count == 1);
    CHECK(log.objects[0].link_map_addr == MAP2_ADDR);
    CHECK(observer.live_map_count == 2);
}

static void test_invalid_sources_fail_closed(void)
{
    fixture_t fixture;
    visit_log_t log = { 0 };
    kzt_public_loader_observer_t observer;
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };

    setup_two_maps(&fixture);
    write_dynamic(&fixture, 0);
    kzt_public_loader_observer_reset(&observer);
    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_NOT_FOUND);
    CHECK(observer.active == 0);

    setup_two_maps(&fixture);
    fixture.reject_reads = 1;
    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_READ_ERROR);
    CHECK(observer.active == 0);
}

static void test_relro_lookup_matches_one_object_during_add(void)
{
    fixture_t fixture;
    kzt_public_loader_object_t object = { 0 };
    kzt_public_loader_observer_t observer;
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };

    memset(&fixture, 0, sizeof(fixture));
    write_dynamic(&fixture, R_DEBUG_ADDR);
    write_debug(&fixture, KZT_LOADER_DEBUG_ADD, MAP1_ADDR);
    write_map(&fixture, MAP1_ADDR, ELF1_BASE, NAME1_ADDR,
              FIXTURE_BASE + 0x5000, MAP2_ADDR, 0);
    write_map(&fixture, MAP2_ADDR, ELF2_BASE, NAME2_ADDR,
              FIXTURE_BASE + 0x5100, 0, MAP1_ADDR);
    write_elf_relro(&fixture, ELF1_BASE, 0x1000, 0x900);
    write_elf_relro(&fixture, ELF2_BASE, 0x1000, 0x1800);

    CHECK(kzt_public_loader_find_relro_object(
              DYNAMIC_ADDR, 16, ELF2_BASE + 0x1000, 0x2000,
              TEST_PAGE_SIZE, &reader, &object) == KZT_PUBLIC_LOADER_OK);
    CHECK(object.link_map_addr == MAP2_ADDR);
    CHECK(object.load_bias == ELF2_BASE);
    CHECK(object.name_addr == NAME2_ADDR);

    CHECK(kzt_public_loader_find_relro_object(
              DYNAMIC_ADDR, 16, ELF2_BASE + 0x2000, 0x1000,
              TEST_PAGE_SIZE, &reader, &object) == KZT_PUBLIC_LOADER_OK);
    CHECK(object.link_map_addr == MAP2_ADDR);

    CHECK(kzt_public_loader_find_relro_object(
              DYNAMIC_ADDR, 16, ELF2_BASE + 0x3000, 0x1000,
              TEST_PAGE_SIZE, &reader, &object) ==
          KZT_PUBLIC_LOADER_NOT_FOUND);

    CHECK(kzt_public_loader_find_relro_object(
              DYNAMIC_ADDR, 16, ELF1_BASE + 0x1000,
              (ELF2_BASE + 0x3000) - (ELF1_BASE + 0x1000),
              TEST_PAGE_SIZE, &reader, &object) ==
          KZT_PUBLIC_LOADER_INVALID_STATE);

    write_debug(&fixture, KZT_LOADER_DEBUG_CONSISTENT, MAP1_ADDR);
    CHECK(kzt_public_loader_find_relro_object(
              DYNAMIC_ADDR, 16, ELF2_BASE + 0x1000, 0x2000,
              TEST_PAGE_SIZE, &reader, &object) == KZT_PUBLIC_LOADER_BUSY);

    kzt_public_loader_observer_reset(&observer);
    CHECK(!kzt_public_loader_observer_has_map(&observer, MAP2_ADDR));
    CHECK(kzt_public_loader_observer_remember(&observer, MAP2_ADDR) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(kzt_public_loader_observer_has_map(&observer, MAP2_ADDR));
}

static void test_object_relro_classification_uses_guest_memory(void)
{
    fixture_t fixture;
    kzt_public_loader_object_t object = {
        .link_map_addr = MAP1_ADDR,
        .load_bias = ELF1_BASE,
    };
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };
    int has_relro = -1;

    memset(&fixture, 0, sizeof(fixture));
    write_elf_relro(&fixture, ELF1_BASE, 0x1000, 0x900);
    CHECK(kzt_public_loader_object_has_relro(
              &object, &reader, &has_relro) == KZT_PUBLIC_LOADER_OK);
    CHECK(has_relro == 1);

    memset(&fixture, 0, sizeof(fixture));
    write_elf_without_relro(&fixture, ELF1_BASE);
    CHECK(kzt_public_loader_object_has_relro(
              &object, &reader, &has_relro) == KZT_PUBLIC_LOADER_OK);
    CHECK(has_relro == 0);
}

int main(void)
{
    test_activate_reads_public_loader_state();
    test_busy_state_defers_new_object();
    test_deleted_address_can_be_observed_again();
    test_cycle_does_not_replace_last_complete_snapshot();
    test_preprotected_object_is_not_replayed();
    test_invalid_sources_fail_closed();
    test_relro_lookup_matches_one_object_during_add();
    test_object_relro_classification_uses_guest_memory();
    puts("kzt public loader observer tests: PASS");
    return 0;
}
