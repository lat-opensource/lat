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
#define SYMBOL_ELF_BASE (FIXTURE_BASE + 0x8000)
#define SYMBOL_DYNAMIC_ADDR (SYMBOL_ELF_BASE + 0x100)
#define SYMBOL_HASH_ADDR (SYMBOL_ELF_BASE + 0x200)
#define SYMBOL_TABLE_ADDR (SYMBOL_ELF_BASE + 0x300)
#define SYMBOL_STRING_ADDR (SYMBOL_ELF_BASE + 0x400)
#define TLS_ELF_BASE (FIXTURE_BASE + 0x9000)
#define TLS_DYNAMIC_ADDR (TLS_ELF_BASE + 0x300)
#define TLS_RELA_ADDR (TLS_ELF_BASE + 0x500)
#define TLS_IMAGE_ADDR (TLS_ELF_BASE + 0x600)
#define TLS_MODULE_RELOCATION_ADDR (TLS_ELF_BASE + 0x700)
#define TLS_EXTERNAL_RELOCATION_ADDR (TLS_ELF_BASE + 0x708)
#define TLS_STATIC_RELOCATION_ADDR (TLS_ELF_BASE + 0x710)
#define TLS_SYMBOL_TABLE_ADDR (TLS_ELF_BASE + 0x800)
#define TLS_STRING_TABLE_ADDR (TLS_ELF_BASE + 0x880)
#define TLS_HASH_ADDR (TLS_ELF_BASE + 0x8c0)
#define DUP_TLS_DYNAMIC_ADDR (FIXTURE_BASE + 0x5800)
#define DUP_TLS_HASH_ADDR (FIXTURE_BASE + 0x5900)
#define DUP_TLS_SYMBOL_ADDR (FIXTURE_BASE + 0x5a00)
#define DUP_TLS_STRING_ADDR (FIXTURE_BASE + 0x5b00)
#define TEST_PAGE_SIZE 0x1000
#define KZT_TEST_ET_DYN 3
#define KZT_TEST_EM_X86_64 62
#define KZT_TEST_PT_LOAD 1
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

typedef struct test_x86_64_symbol {
    uint32_t name;
    uint8_t info;
    uint8_t other;
    uint16_t section_index;
    uint64_t value;
    uint64_t size;
} test_x86_64_symbol_t;

typedef struct test_x86_64_relocation {
    uint64_t offset;
    uint64_t info;
    int64_t addend;
} test_x86_64_relocation_t;

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

static void write_elf_loads(
    fixture_t *fixture,
    uintptr_t load_bias,
    const test_x86_64_program_header_t *phdrs,
    size_t phnum)
{
    test_x86_64_elf_header_t header = { 0 };

    CHECK(phnum > 0);
    CHECK(phnum <= UINT16_MAX);
    header.ident[0] = 0x7f;
    header.ident[1] = 'E';
    header.ident[2] = 'L';
    header.ident[3] = 'F';
    header.ident[4] = 2;
    header.ident[5] = 1;
    header.ident[6] = 1;
    header.type = KZT_TEST_ET_DYN;
    header.machine = KZT_TEST_EM_X86_64;
    header.version = 1;
    header.phoff = sizeof(header);
    header.ehsize = sizeof(header);
    header.phentsize = sizeof(*phdrs);
    header.phnum = phnum;
    fixture_write(fixture, load_bias, &header, sizeof(header));
    fixture_write(fixture, load_bias + header.phoff,
                  phdrs, phnum * sizeof(*phdrs));
}

static void write_symbol_object(fixture_t *fixture)
{
    const kzt_x86_64_dynamic_entry_t dynamic[] = {
        { .tag = 4, .value = SYMBOL_HASH_ADDR - SYMBOL_ELF_BASE },
        { .tag = 5, .value = SYMBOL_STRING_ADDR - SYMBOL_ELF_BASE },
        { .tag = 6, .value = SYMBOL_TABLE_ADDR - SYMBOL_ELF_BASE },
        { .tag = 10, .value = sizeof("\0_dl_allocate_tls") },
        { .tag = 11, .value = sizeof(test_x86_64_symbol_t) },
        { .tag = KZT_X86_64_DT_NULL, .value = 0 },
    };
    const uint32_t hash_header[] = { 1, 2 };
    const test_x86_64_symbol_t symbols[] = {
        { 0 },
        {
            .name = 1,
            .info = 0x12,
            .section_index = 1,
            .value = 0x1234,
        },
    };
    static const char strings[] = "\0_dl_allocate_tls";

    fixture_write(fixture, SYMBOL_DYNAMIC_ADDR,
                  dynamic, sizeof(dynamic));
    fixture_write(fixture, SYMBOL_HASH_ADDR,
                  hash_header, sizeof(hash_header));
    fixture_write(fixture, SYMBOL_TABLE_ADDR,
                  symbols, sizeof(symbols));
    fixture_write(fixture, SYMBOL_STRING_ADDR,
                  strings, sizeof(strings));
}

static void write_gnu_hash_symbol_object(fixture_t *fixture)
{
    const kzt_x86_64_dynamic_entry_t dynamic[] = {
        { .tag = INT64_C(0x6ffffef5),
          .value = SYMBOL_HASH_ADDR - SYMBOL_ELF_BASE },
        { .tag = 5, .value = SYMBOL_STRING_ADDR - SYMBOL_ELF_BASE },
        { .tag = 6, .value = SYMBOL_TABLE_ADDR - SYMBOL_ELF_BASE },
        { .tag = 10, .value = sizeof("\0_dl_allocate_tls") },
        { .tag = 11, .value = sizeof(test_x86_64_symbol_t) },
        { .tag = KZT_X86_64_DT_NULL, .value = 0 },
    };
    const struct {
        uint32_t bucket_count;
        uint32_t symbol_offset;
        uint32_t bloom_size;
        uint32_t bloom_shift;
        uint64_t bloom;
        uint32_t bucket;
        uint32_t chain;
    } hash = {
        .bucket_count = 1,
        .symbol_offset = 1,
        .bloom_size = 1,
        .bucket = 1,
        .chain = 1,
    };
    const test_x86_64_symbol_t symbols[] = {
        { 0 },
        {
            .name = 1,
            .info = 0x12,
            .section_index = 1,
            .value = 0x1234,
        },
    };
    static const char strings[] = "\0_dl_allocate_tls";

    fixture_write(fixture, SYMBOL_DYNAMIC_ADDR,
                  dynamic, sizeof(dynamic));
    fixture_write(fixture, SYMBOL_HASH_ADDR, &hash, sizeof(hash));
    fixture_write(fixture, SYMBOL_TABLE_ADDR,
                  symbols, sizeof(symbols));
    fixture_write(fixture, SYMBOL_STRING_ADDR,
                  strings, sizeof(strings));
}

static void write_tls_object(fixture_t *fixture)
{
    test_x86_64_elf_header_t header = { 0 };
    const test_x86_64_program_header_t phdr = {
        .type = 7,
        .vaddr = TLS_IMAGE_ADDR - TLS_ELF_BASE + 8,
        .filesz = 24,
        .memsz = 32,
        .align = 64,
    };
    const kzt_x86_64_dynamic_entry_t dynamic[] = {
        { .tag = 7, .value = TLS_RELA_ADDR - TLS_ELF_BASE },
        { .tag = 8, .value = 6 * sizeof(test_x86_64_relocation_t) },
        { .tag = 9, .value = sizeof(test_x86_64_relocation_t) },
        { .tag = 6, .value = TLS_SYMBOL_TABLE_ADDR - TLS_ELF_BASE },
        { .tag = 11, .value = sizeof(test_x86_64_symbol_t) },
        { .tag = 5, .value = TLS_STRING_TABLE_ADDR - TLS_ELF_BASE },
        { .tag = 10, .value = sizeof("\0tls_symbol") },
        { .tag = 4, .value = TLS_HASH_ADDR - TLS_ELF_BASE },
        { .tag = KZT_X86_64_DT_NULL, .value = 0 },
    };
    const test_x86_64_relocation_t relocations[] = {
        {
            .offset = TLS_EXTERNAL_RELOCATION_ADDR - TLS_ELF_BASE,
            .info = (UINT64_C(1) << 32) | 16,
        },
        {
            .offset = TLS_MODULE_RELOCATION_ADDR - TLS_ELF_BASE,
            .info = 16,
        },
        {
            .offset = TLS_STATIC_RELOCATION_ADDR - TLS_ELF_BASE,
            .info = (UINT64_C(1) << 32) | 18,
            .addend = 4,
        },
        {
            .offset = TLS_IMAGE_ADDR + 8 - TLS_ELF_BASE,
            .info = 8,
            .addend = 0x1234,
        },
        {
            .offset = TLS_IMAGE_ADDR + 16 - TLS_ELF_BASE,
            .info = (UINT64_C(1) << 32) | 1,
        },
        {
            .offset = TLS_IMAGE_ADDR + 24 - TLS_ELF_BASE,
            .info = (UINT64_C(2) << 32) | 1,
        },
    };
    const test_x86_64_symbol_t symbols[] = {
        { 0 },
        {
            .name = 1,
            .info = 0x16,
            /* A default-visible definition can be preempted. */
            .section_index = 1,
        },
        {
            .info = 0x0a,
            .section_index = 1,
            .value = 0x555,
        },
    };
    const uint64_t module_id = 7;
    const uint64_t external_module_id = 3;
    const int64_t static_relocation = -0x11c;
    const uint64_t image_values[] = {
        UINT64_C(0x123456789abcdef0),
        UINT64_C(0xabcdef0123456789),
        UINT64_C(0xfeedfacecafebeef),
    };
    static const char string_table[] = "\0tls_symbol";
    const uint32_t hash_header[] = { 1, 3 };

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
    fixture_write(fixture, TLS_ELF_BASE, &header, sizeof(header));
    fixture_write(fixture, TLS_ELF_BASE + header.phoff,
                  &phdr, sizeof(phdr));
    fixture_write(fixture, TLS_DYNAMIC_ADDR, dynamic, sizeof(dynamic));
    fixture_write(fixture, TLS_RELA_ADDR,
                  relocations, sizeof(relocations));
    fixture_write(fixture, TLS_SYMBOL_TABLE_ADDR,
                  symbols, sizeof(symbols));
    fixture_write(fixture, TLS_STRING_TABLE_ADDR,
                  string_table, sizeof(string_table));
    fixture_write(fixture, TLS_HASH_ADDR,
                  hash_header, sizeof(hash_header));
    fixture_write(fixture, TLS_MODULE_RELOCATION_ADDR,
                  &module_id, sizeof(module_id));
    fixture_write(fixture, TLS_EXTERNAL_RELOCATION_ADDR,
                  &external_module_id, sizeof(external_module_id));
    fixture_write(fixture, TLS_STATIC_RELOCATION_ADDR,
                  &static_relocation, sizeof(static_relocation));
    fixture_write(fixture, TLS_IMAGE_ADDR + 8,
                  image_values, sizeof(image_values));
}

static void write_duplicate_tls_symbol(fixture_t *fixture)
{
    const kzt_x86_64_dynamic_entry_t dynamic[] = {
        { .tag = 4, .value = DUP_TLS_HASH_ADDR },
        { .tag = 5, .value = DUP_TLS_STRING_ADDR },
        { .tag = 6, .value = DUP_TLS_SYMBOL_ADDR },
        { .tag = 10, .value = sizeof("\0tls_symbol") },
        { .tag = 11, .value = sizeof(test_x86_64_symbol_t) },
        { .tag = KZT_X86_64_DT_NULL, .value = 0 },
    };
    const uint32_t hash_header[] = { 1, 2 };
    const test_x86_64_symbol_t symbols[] = {
        { 0 },
        {
            .name = 1,
            .info = 0x16,
            .section_index = 1,
            .value = 8,
        },
    };
    static const char strings[] = "\0tls_symbol";

    fixture_write(fixture, DUP_TLS_DYNAMIC_ADDR,
                  dynamic, sizeof(dynamic));
    fixture_write(fixture, DUP_TLS_HASH_ADDR,
                  hash_header, sizeof(hash_header));
    fixture_write(fixture, DUP_TLS_SYMBOL_ADDR,
                  symbols, sizeof(symbols));
    fixture_write(fixture, DUP_TLS_STRING_ADDR,
                  strings, sizeof(strings));
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
    uint64_t first_generation;

    setup_two_maps(&fixture);
    kzt_public_loader_observer_reset(&observer);
    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_OK);
    first_generation = observer.live_map_generations[1];
    CHECK(first_generation != 0);

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
    CHECK(observer.live_map_generations[1] != first_generation);
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
    CHECK(kzt_public_loader_observer_mark_processed(&observer, MAP1_ADDR) ==
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

static void test_relro_lookup_matches_one_object_before_protection(void)
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
              TEST_PAGE_SIZE, &reader, &object) == KZT_PUBLIC_LOADER_OK);
    CHECK(object.link_map_addr == MAP2_ADDR);

    write_debug(&fixture, KZT_LOADER_DEBUG_DELETE, MAP1_ADDR);
    CHECK(kzt_public_loader_find_relro_object(
              DYNAMIC_ADDR, 16, ELF2_BASE + 0x1000, 0x2000,
              TEST_PAGE_SIZE, &reader, &object) == KZT_PUBLIC_LOADER_BUSY);

    kzt_public_loader_observer_reset(&observer);
    CHECK(!kzt_public_loader_observer_has_map(&observer, MAP2_ADDR));
    CHECK(kzt_public_loader_observer_remember(&observer, MAP2_ADDR) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(kzt_public_loader_observer_has_map(&observer, MAP2_ADDR));
}

static void test_observed_processed_and_reported_states_are_distinct(void)
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

    CHECK(kzt_public_loader_observer_has_map(&observer, MAP2_ADDR));
    CHECK(!kzt_public_loader_observer_is_processed(&observer, MAP2_ADDR));
    CHECK(kzt_public_loader_observer_mark_processed(
              &observer, MAP2_ADDR) == KZT_PUBLIC_LOADER_OK);
    CHECK(kzt_public_loader_observer_is_processed(&observer, MAP2_ADDR));

    CHECK(kzt_public_loader_observer_mark_fallback_reported(
              &observer, MAP2_ADDR));
    CHECK(kzt_public_loader_observer_fallback_was_reported(
              &observer, MAP2_ADDR));
    CHECK(!kzt_public_loader_observer_mark_fallback_reported(
              &observer, MAP2_ADDR));

    write_map(&fixture, MAP1_ADDR, UINT64_C(0x400000), NAME1_ADDR,
              FIXTURE_BASE + 0x5000, 0, 0);
    CHECK(kzt_public_loader_observer_refresh(
              &observer, &reader, record_visit, &log) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(!kzt_public_loader_observer_has_map(&observer, MAP2_ADDR));
    CHECK(!kzt_public_loader_observer_is_processed(&observer, MAP2_ADDR));
    CHECK(!kzt_public_loader_observer_fallback_was_reported(
              &observer, MAP2_ADDR));

    write_map(&fixture, MAP1_ADDR, UINT64_C(0x400000), NAME1_ADDR,
              FIXTURE_BASE + 0x5000, MAP2_ADDR, 0);
    write_map(&fixture, MAP2_ADDR, UINT64_C(0xa00000), NAME3_ADDR,
              FIXTURE_BASE + 0x5300, 0, MAP1_ADDR);
    CHECK(kzt_public_loader_observer_refresh(
              &observer, &reader, record_visit, &log) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(kzt_public_loader_observer_mark_fallback_reported(
              &observer, MAP2_ADDR));
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

static void test_symbol_lookup_uses_live_relocated_elf_state(void)
{
    fixture_t fixture;
    visit_log_t log = { 0 };
    kzt_public_loader_observer_t observer;
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };
    uintptr_t symbol_addr = 0;

    memset(&fixture, 0, sizeof(fixture));
    write_dynamic(&fixture, R_DEBUG_ADDR);
    write_debug(&fixture, KZT_LOADER_DEBUG_CONSISTENT, MAP1_ADDR);
    write_map(&fixture, MAP1_ADDR, SYMBOL_ELF_BASE, NAME1_ADDR,
              SYMBOL_DYNAMIC_ADDR, 0, 0);
    write_symbol_object(&fixture);
    kzt_public_loader_observer_reset(&observer);
    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_OK);

    CHECK(kzt_public_loader_find_symbol(
              &observer, &reader, "_dl_allocate_tls", &symbol_addr) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(symbol_addr == SYMBOL_ELF_BASE + 0x1234);
    CHECK(log.count == 1);
    symbol_addr = 0;
    CHECK(kzt_public_loader_find_symbol_in_object(
              &log.objects[0], &reader,
              "_dl_allocate_tls", &symbol_addr) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(symbol_addr == SYMBOL_ELF_BASE + 0x1234);
    CHECK(kzt_public_loader_find_symbol(
              &observer, &reader, "missing", &symbol_addr) ==
          KZT_PUBLIC_LOADER_NOT_FOUND);

    memset(&fixture, 0, sizeof(fixture));
    write_dynamic(&fixture, R_DEBUG_ADDR);
    write_debug(&fixture, KZT_LOADER_DEBUG_CONSISTENT, MAP1_ADDR);
    write_map(&fixture, MAP1_ADDR, SYMBOL_ELF_BASE, NAME1_ADDR,
              SYMBOL_DYNAMIC_ADDR, 0, 0);
    write_gnu_hash_symbol_object(&fixture);
    kzt_public_loader_observer_reset(&observer);
    log.count = 0;
    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_OK);
    CHECK(kzt_public_loader_find_symbol(
              &observer, &reader, "_dl_allocate_tls", &symbol_addr) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(symbol_addr == SYMBOL_ELF_BASE + 0x1234);
}

static void test_address_lookup_uses_exact_load_segments(void)
{
    fixture_t fixture;
    visit_log_t log = { 0 };
    kzt_public_loader_observer_t observer;
    kzt_public_loader_object_t object = { 0 };
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };
    const test_x86_64_program_header_t elf1_phdrs[] = {
        {
            .type = KZT_TEST_PT_LOAD,
            .offset = 0x1000,
            .vaddr = 0x1000,
            .filesz = 0x400,
            .memsz = 0x800,
            .align = 0x1000,
        },
        {
            .type = KZT_TEST_PT_LOAD,
            .offset = 0x3000,
            .vaddr = 0x3000,
            .filesz = 0x500,
            .memsz = 0x500,
            .align = 0x1000,
        },
        {
            .type = KZT_TEST_PT_LOAD,
            .offset = 0x3200,
            .vaddr = 0x3200,
            .filesz = 0x200,
            .memsz = 0x200,
            .align = 0x100,
        },
    };
    const test_x86_64_program_header_t elf2_phdr = {
        .type = KZT_TEST_PT_LOAD,
        .offset = 0x5000,
        .vaddr = 0x5000,
        .filesz = 0x100,
        .memsz = 0x100,
        .align = 0x1000,
    };

    memset(&fixture, 0, sizeof(fixture));
    write_dynamic(&fixture, R_DEBUG_ADDR);
    write_debug(&fixture, KZT_LOADER_DEBUG_CONSISTENT, MAP1_ADDR);
    write_map(&fixture, MAP1_ADDR, ELF1_BASE, NAME1_ADDR,
              FIXTURE_BASE + 0x5000, MAP2_ADDR, 0);
    write_map(&fixture, MAP2_ADDR, ELF2_BASE, NAME2_ADDR,
              FIXTURE_BASE + 0x5100, 0, MAP1_ADDR);
    write_elf_loads(&fixture, ELF1_BASE, elf1_phdrs,
                    sizeof(elf1_phdrs) / sizeof(elf1_phdrs[0]));
    write_elf_loads(&fixture, ELF2_BASE, &elf2_phdr, 1);
    kzt_public_loader_observer_reset(&observer);
    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_OK);

    CHECK(kzt_public_loader_find_object_by_address(
              &observer, &reader, ELF1_BASE + 0x1100, &object) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(object.link_map_addr == MAP1_ADDR);
    CHECK(object.load_bias == ELF1_BASE);
    CHECK(kzt_public_loader_find_object_by_address(
              &observer, &reader, ELF1_BASE + 0x1600, &object) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(object.link_map_addr == MAP1_ADDR);
    CHECK(kzt_public_loader_find_object_by_address(
              &observer, &reader, ELF1_BASE + 0x3300, &object) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(object.link_map_addr == MAP1_ADDR);
    CHECK(kzt_public_loader_find_object_by_address(
              &observer, &reader, ELF1_BASE + 0x2000, &object) ==
          KZT_PUBLIC_LOADER_NOT_FOUND);
    CHECK(kzt_public_loader_find_object_by_address(
              &observer, &reader, ELF1_BASE + 0x1800, &object) ==
          KZT_PUBLIC_LOADER_NOT_FOUND);
}

static void test_address_lookup_rejects_ambiguity_and_malformed_elf(void)
{
    fixture_t fixture;
    visit_log_t log = { 0 };
    kzt_public_loader_observer_t observer;
    kzt_public_loader_object_t object = { 0 };
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };
    test_x86_64_program_header_t phdr1 = {
        .type = KZT_TEST_PT_LOAD,
        .offset = 0x7000,
        .vaddr = 0x7000,
        .filesz = 0x200,
        .memsz = 0x200,
        .align = 0x1000,
    };
    const test_x86_64_program_header_t phdr2 = {
        .type = KZT_TEST_PT_LOAD,
        .offset = 0x2000,
        .vaddr = 0x2000,
        .filesz = 0x200,
        .memsz = 0x200,
        .align = 0x1000,
    };
    test_x86_64_elf_header_t invalid_header = { 0 };

    memset(&fixture, 0, sizeof(fixture));
    write_dynamic(&fixture, R_DEBUG_ADDR);
    write_debug(&fixture, KZT_LOADER_DEBUG_CONSISTENT, MAP1_ADDR);
    write_map(&fixture, MAP1_ADDR, ELF1_BASE, NAME1_ADDR,
              FIXTURE_BASE + 0x5000, MAP2_ADDR, 0);
    write_map(&fixture, MAP2_ADDR, ELF2_BASE, NAME2_ADDR,
              FIXTURE_BASE + 0x5100, 0, MAP1_ADDR);
    write_elf_loads(&fixture, ELF1_BASE, &phdr1, 1);
    write_elf_loads(&fixture, ELF2_BASE, &phdr2, 1);
    kzt_public_loader_observer_reset(&observer);
    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_OK);
    CHECK(kzt_public_loader_find_object_by_address(
              &observer, &reader, ELF1_BASE + 0x7100, &object) ==
          KZT_PUBLIC_LOADER_INVALID_STATE);

    write_map(&fixture, MAP1_ADDR, ELF1_BASE, NAME1_ADDR,
              FIXTURE_BASE + 0x5000, 0, 0);
    CHECK(kzt_public_loader_observer_refresh(
              &observer, &reader, record_visit, &log) ==
          KZT_PUBLIC_LOADER_OK);
    fixture_write(&fixture, ELF1_BASE, &invalid_header,
                  sizeof(invalid_header));
    CHECK(kzt_public_loader_find_object_by_address(
              &observer, &reader, ELF1_BASE + 0x7100, &object) ==
          KZT_PUBLIC_LOADER_INVALID_STATE);

    write_elf_loads(&fixture, ELF1_BASE, &phdr1, 1);
    phdr1.vaddr = UINT64_MAX;
    phdr1.align = 1;
    fixture_write(&fixture, ELF1_BASE + sizeof(invalid_header),
                  &phdr1, sizeof(phdr1));
    CHECK(kzt_public_loader_find_object_by_address(
              &observer, &reader, ELF1_BASE + 0x7100, &object) ==
          KZT_PUBLIC_LOADER_OVERFLOW);
}

static void test_address_lookup_requires_active_consistent_observer(void)
{
    fixture_t fixture;
    visit_log_t log = { 0 };
    kzt_public_loader_observer_t observer;
    kzt_public_loader_object_t object = { 0 };
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };

    memset(&fixture, 0, sizeof(fixture));
    kzt_public_loader_observer_reset(&observer);
    CHECK(kzt_public_loader_find_object_by_address(
              &observer, &reader, ELF1_BASE + 0x1000, &object) ==
          KZT_PUBLIC_LOADER_INVALID_INPUT);

    write_dynamic(&fixture, R_DEBUG_ADDR);
    write_debug(&fixture, KZT_LOADER_DEBUG_CONSISTENT, MAP1_ADDR);
    write_map(&fixture, MAP1_ADDR, 0, NAME1_ADDR,
              FIXTURE_BASE + 0x5000, 0, 0);
    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_OK);
    CHECK(kzt_public_loader_find_object_by_address(
              &observer, &reader, ELF1_BASE + 0x1000, &object) ==
          KZT_PUBLIC_LOADER_NOT_FOUND);

    write_debug(&fixture, KZT_LOADER_DEBUG_ADD, MAP1_ADDR);
    CHECK(kzt_public_loader_find_object_by_address(
              &observer, &reader, ELF1_BASE + 0x1000, &object) ==
          KZT_PUBLIC_LOADER_BUSY);
}

static void test_loader_state_probe_requires_same_consistent_instance(void)
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
    CHECK(kzt_public_loader_state_is_consistent(
              &observer, &reader) == KZT_PUBLIC_LOADER_OK);

    write_debug(&fixture, KZT_LOADER_DEBUG_ADD, MAP1_ADDR);
    CHECK(kzt_public_loader_state_is_consistent(
              &observer, &reader) == KZT_PUBLIC_LOADER_BUSY);

    write_debug(&fixture, KZT_LOADER_DEBUG_CONSISTENT, MAP1_ADDR);
    observer.r_brk_addr += 8;
    CHECK(kzt_public_loader_state_is_consistent(
              &observer, &reader) == KZT_PUBLIC_LOADER_INVALID_STATE);
}

static void test_address_lookup_accepts_remembered_object_during_add(void)
{
    fixture_t fixture;
    visit_log_t log = { 0 };
    kzt_public_loader_observer_t observer;
    kzt_public_loader_object_t object = { 0 };
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };
    const test_x86_64_program_header_t phdr = {
        .type = KZT_TEST_PT_LOAD,
        .offset = 0x1000,
        .vaddr = 0x1000,
        .filesz = 0x400,
        .memsz = 0x800,
        .align = 0x1000,
    };

    memset(&fixture, 0, sizeof(fixture));
    write_dynamic(&fixture, R_DEBUG_ADDR);
    write_debug(&fixture, KZT_LOADER_DEBUG_CONSISTENT, MAP1_ADDR);
    write_map(&fixture, MAP1_ADDR, ELF1_BASE, NAME1_ADDR,
              FIXTURE_BASE + 0x5000, 0, 0);
    write_elf_loads(&fixture, ELF1_BASE, &phdr, 1);
    write_elf_loads(&fixture, ELF2_BASE, &phdr, 1);
    kzt_public_loader_observer_reset(&observer);
    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_OK);

    /*
     * The pre-RELRO path can observe and remember a new object while ld.so
     * still reports RT_ADD.  Address ownership for that already-mapped object
     * must not wait for the later RT_CONSISTENT notification.
     */
    write_map(&fixture, MAP1_ADDR, ELF1_BASE, NAME1_ADDR,
              FIXTURE_BASE + 0x5000, MAP2_ADDR, 0);
    write_map(&fixture, MAP2_ADDR, ELF2_BASE, NAME2_ADDR,
              FIXTURE_BASE + 0x5100, 0, MAP1_ADDR);
    CHECK(kzt_public_loader_observer_remember(&observer, MAP2_ADDR) ==
          KZT_PUBLIC_LOADER_OK);
    write_debug(&fixture, KZT_LOADER_DEBUG_ADD, MAP1_ADDR);
    log.count = 0;
    CHECK(kzt_public_loader_observer_refresh(
              &observer, &reader, record_visit, &log) ==
          KZT_PUBLIC_LOADER_BUSY);
    CHECK(log.count == 0);

    CHECK(kzt_public_loader_find_object_by_address(
              &observer, &reader, ELF2_BASE + 0x1100, &object) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(object.link_map_addr == MAP2_ADDR);
    CHECK(object.load_bias == ELF2_BASE);
}

static void test_address_lookup_walks_beyond_snapshot_capacity(void)
{
    enum {
        object_count = KZT_PUBLIC_LOADER_MAX_OBJECTS + 1,
        elf_stride = 0x80,
    };
    const uintptr_t map_base = FIXTURE_BASE + 0x3000;
    const uintptr_t elf_base = FIXTURE_BASE + 0x6000;
    const test_x86_64_program_header_t phdr = {
        .type = KZT_TEST_PT_LOAD,
        .offset = 0,
        .vaddr = 0,
        .filesz = elf_stride,
        .memsz = elf_stride,
        .align = 1,
    };
    fixture_t fixture;
    kzt_public_loader_observer_t observer;
    kzt_public_loader_object_t object = { 0 };
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };

    memset(&fixture, 0, sizeof(fixture));
    write_debug(&fixture, KZT_LOADER_DEBUG_CONSISTENT, map_base);
    kzt_public_loader_observer_reset(&observer);
    observer.active = 1;
    observer.r_debug_addr = R_DEBUG_ADDR;
    observer.r_brk_addr = R_BRK_ADDR;
    observer.live_map_count = KZT_PUBLIC_LOADER_MAX_OBJECTS;

    for (size_t index = 0; index < object_count; ++index) {
        uintptr_t map_addr =
            map_base + index * sizeof(kzt_x86_64_link_map_prefix_t);
        uintptr_t object_base = elf_base + index * elf_stride;
        uintptr_t next = index + 1 < object_count
            ? map_addr + sizeof(kzt_x86_64_link_map_prefix_t) : 0;
        uintptr_t previous = index
            ? map_addr - sizeof(kzt_x86_64_link_map_prefix_t) : 0;

        write_map(&fixture, map_addr, object_base, 0, 0,
                  next, previous);
        write_elf_loads(&fixture, object_base, &phdr, 1);
        if (index < KZT_PUBLIC_LOADER_MAX_OBJECTS) {
            observer.live_maps[index] = map_addr;
        }
    }

    CHECK(kzt_public_loader_find_object_by_address(
              &observer, &reader,
              elf_base + (object_count - 1) * elf_stride + 0x70,
              &object) == KZT_PUBLIC_LOADER_OK);
    CHECK(object.link_map_addr ==
          map_base + (object_count - 1) *
              sizeof(kzt_x86_64_link_map_prefix_t));
    CHECK(object.load_bias ==
          elf_base + (object_count - 1) * elf_stride);
}

static void test_tls_collection_uses_live_image_and_module_relocation(void)
{
    fixture_t fixture;
    visit_log_t log = { 0 };
    kzt_public_loader_observer_t observer;
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };
    kzt_public_loader_tls_object_t tls_objects[2];
    size_t tls_count = 0;
    uint64_t materialized_image[3];

    memset(&fixture, 0, sizeof(fixture));
    write_dynamic(&fixture, R_DEBUG_ADDR);
    write_debug(&fixture, KZT_LOADER_DEBUG_CONSISTENT, MAP1_ADDR);
    write_map(&fixture, MAP1_ADDR, TLS_ELF_BASE, NAME1_ADDR,
              TLS_DYNAMIC_ADDR, 0, 0);
    write_tls_object(&fixture);
    kzt_public_loader_observer_reset(&observer);
    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_OK);

    CHECK(kzt_public_loader_collect_tls(
              &observer, &reader, tls_objects, 2, &tls_count) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(tls_count == 1);
    CHECK(tls_objects[0].image_addr == TLS_IMAGE_ADDR + 8);
    CHECK(tls_objects[0].file_size == 24);
    CHECK(tls_objects[0].memory_size == 32);
    CHECK(tls_objects[0].alignment == 64);
    CHECK(tls_objects[0].first_byte_offset == 8);
    CHECK(tls_objects[0].static_tls_offset == -0x120);
    CHECK(tls_objects[0].static_tls_offset_valid == 1);
    CHECK(tls_objects[0].static_tls_offset_needs_validation == 0);
    CHECK(tls_objects[0].static_tls_symbol_name_addr == 0);
    CHECK(tls_objects[0].module_id == 7);
    CHECK(tls_objects[0].link_map_addr == MAP1_ADDR);
    CHECK(tls_objects[0].load_generation != 0);
    CHECK(kzt_public_loader_materialize_tls_image(
              &tls_objects[0], &reader, &materialized_image,
              sizeof(materialized_image)) == KZT_PUBLIC_LOADER_OK);
    CHECK(materialized_image[0] == TLS_ELF_BASE + 0x1234);
    CHECK(materialized_image[1] == UINT64_C(0xabcdef0123456789));
    CHECK(materialized_image[2] == UINT64_C(0xfeedfacecafebeef));
    CHECK(kzt_public_loader_materialize_tls_image(
              &tls_objects[0], &reader, materialized_image,
              sizeof(materialized_image) - 1) ==
          KZT_PUBLIC_LOADER_INVALID_INPUT);
    fixture.reject_reads = 1;
    CHECK(kzt_public_loader_materialize_tls_image(
              &tls_objects[0], &reader, materialized_image,
              sizeof(materialized_image)) ==
          KZT_PUBLIC_LOADER_READ_ERROR);
    fixture.reject_reads = 0;

    {
        test_x86_64_relocation_t original;
        test_x86_64_relocation_t unsupported;
        uintptr_t relocation_addr =
            TLS_RELA_ADDR + 5 * sizeof(test_x86_64_relocation_t);

        CHECK(fixture_read(relocation_addr, &original,
                           sizeof(original), &fixture) == 0);
        unsupported = original;
        unsupported.info = 99;
        fixture_write(&fixture, relocation_addr,
                      &unsupported, sizeof(unsupported));
        CHECK(kzt_public_loader_materialize_tls_image(
                  &tls_objects[0], &reader, materialized_image,
                  sizeof(materialized_image)) ==
              KZT_PUBLIC_LOADER_INVALID_STATE);
        fixture_write(&fixture, relocation_addr,
                      &original, sizeof(original));
    }

    {
        const uint64_t unresolved_module_id = 0;
        const uint64_t resolved_module_id = 7;

        fixture_write(&fixture, TLS_MODULE_RELOCATION_ADDR,
                      &unresolved_module_id,
                      sizeof(unresolved_module_id));
        tls_count = 0;
        CHECK(kzt_public_loader_collect_tls(
                  &observer, &reader, tls_objects, 2, &tls_count) ==
              KZT_PUBLIC_LOADER_OK);
        CHECK(tls_count == 1);
        CHECK(tls_objects[0].module_id == 0);
        fixture_write(&fixture, TLS_MODULE_RELOCATION_ADDR,
                      &resolved_module_id,
                      sizeof(resolved_module_id));
    }
    {
        const int64_t pending_static_offset = 0;
        const int64_t resolved_static_offset = -0x11c;

        fixture_write(&fixture, TLS_STATIC_RELOCATION_ADDR,
                      &pending_static_offset,
                      sizeof(pending_static_offset));
        CHECK(kzt_public_loader_collect_tls(
                  &observer, &reader, tls_objects, 2, &tls_count) ==
              KZT_PUBLIC_LOADER_BUSY);
        fixture_write(&fixture, TLS_STATIC_RELOCATION_ADDR,
                      &resolved_static_offset,
                      sizeof(resolved_static_offset));
    }

    write_debug(&fixture, KZT_LOADER_DEBUG_ADD, MAP1_ADDR);
    tls_count = 0;
    CHECK(kzt_public_loader_snapshot_tls(
              &observer, 0, 0, &reader, 0,
              tls_objects, 2, &tls_count) == KZT_PUBLIC_LOADER_OK);
    CHECK(tls_count == 1);
    CHECK(tls_objects[0].module_id == 7);
    CHECK(tls_objects[0].static_tls_offset == -0x120);
}

static void test_tls_snapshot_walks_beyond_observer_capacity(void)
{
    enum { object_count = KZT_PUBLIC_LOADER_MAX_OBJECTS + 1 };
    const uintptr_t map_base = FIXTURE_BASE + 0x3000;
    const uintptr_t tls_map_addr =
        map_base + (object_count - 1) *
            sizeof(kzt_x86_64_link_map_prefix_t);
    fixture_t fixture;
    kzt_public_loader_observer_t observer;
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };
    kzt_public_loader_tls_object_t tls_objects[2];
    size_t tls_count = 0;
    uint64_t first_generation;

    memset(&fixture, 0, sizeof(fixture));
    write_debug(&fixture, KZT_LOADER_DEBUG_CONSISTENT, map_base);
    kzt_public_loader_observer_reset(&observer);
    observer.active = 1;
    observer.r_debug_addr = R_DEBUG_ADDR;
    observer.r_brk_addr = R_BRK_ADDR;
    observer.live_map_count = KZT_PUBLIC_LOADER_MAX_OBJECTS;
    observer.next_load_generation = KZT_PUBLIC_LOADER_MAX_OBJECTS;

    for (size_t index = 0; index < object_count; ++index) {
        uintptr_t map_addr =
            map_base + index * sizeof(kzt_x86_64_link_map_prefix_t);
        uintptr_t next = index + 1 < object_count
            ? map_addr + sizeof(kzt_x86_64_link_map_prefix_t) : 0;
        uintptr_t previous = index
            ? map_addr - sizeof(kzt_x86_64_link_map_prefix_t) : 0;

        write_map(&fixture, map_addr,
                  index + 1 == object_count ? TLS_ELF_BASE : 0,
                  0,
                  index + 1 == object_count ? TLS_DYNAMIC_ADDR : 0,
                  next, previous);
        if (index < KZT_PUBLIC_LOADER_MAX_OBJECTS) {
            observer.live_maps[index] = map_addr;
            observer.live_map_generations[index] = index + 1;
        }
    }
    write_tls_object(&fixture);

    CHECK(kzt_public_loader_snapshot_tls(
              &observer, 0, 0, &reader, 0,
              tls_objects, 2, &tls_count) == KZT_PUBLIC_LOADER_OK);
    CHECK(tls_count == 1);
    CHECK(tls_objects[0].link_map_addr == tls_map_addr);
    CHECK(tls_objects[0].module_id == 7);
    CHECK(tls_objects[0].load_generation != 0);
    first_generation = tls_objects[0].load_generation;

    tls_count = 0;
    CHECK(kzt_public_loader_snapshot_tls(
              &observer, 0, 0, &reader, 0,
              tls_objects, 2, &tls_count) == KZT_PUBLIC_LOADER_OK);
    CHECK(tls_count == 1);
    CHECK(tls_objects[0].link_map_addr == tls_map_addr);
    CHECK(tls_objects[0].load_generation == first_generation);
}

static void test_tls_snapshot_does_not_consume_binding_visit(void)
{
    fixture_t fixture;
    visit_log_t log = { 0 };
    kzt_public_loader_observer_t observer;
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };
    kzt_public_loader_tls_object_t tls_objects[2];
    size_t tls_count = 0;

    memset(&fixture, 0, sizeof(fixture));
    write_dynamic(&fixture, R_DEBUG_ADDR);
    write_debug(&fixture, KZT_LOADER_DEBUG_CONSISTENT, MAP1_ADDR);
    write_map(&fixture, MAP1_ADDR, SYMBOL_ELF_BASE, NAME1_ADDR,
              SYMBOL_DYNAMIC_ADDR, 0, 0);
    write_symbol_object(&fixture);
    kzt_public_loader_observer_reset(&observer);
    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_OK);
    CHECK(observer.live_map_count == 1);

    write_map(&fixture, MAP1_ADDR, 0, NAME1_ADDR,
              0, MAP2_ADDR, 0);
    write_map(&fixture, MAP2_ADDR, TLS_ELF_BASE, NAME2_ADDR,
              TLS_DYNAMIC_ADDR, 0, MAP1_ADDR);
    write_tls_object(&fixture);
    CHECK(kzt_public_loader_snapshot_tls(
              &observer, DYNAMIC_ADDR, 16, &reader, 0,
              tls_objects, 2, &tls_count) == KZT_PUBLIC_LOADER_OK);
    CHECK(tls_count == 1);
    CHECK(tls_objects[0].module_id == 7);
    CHECK(tls_objects[0].link_map_addr == MAP2_ADDR);
    CHECK(tls_objects[0].load_generation != 0);
    CHECK(observer.live_map_count == 1);

    log.count = 0;
    CHECK(kzt_public_loader_observer_refresh(
              &observer, &reader, record_visit, &log) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(log.count == 1);
    CHECK(log.objects[0].link_map_addr == MAP2_ADDR);
}

static void test_default_visible_tls_requires_unique_owner(void)
{
    fixture_t fixture;
    visit_log_t log = { 0 };
    kzt_public_loader_observer_t observer;
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };
    kzt_public_loader_tls_object_t tls_objects[2];
    size_t tls_count = 0;

    memset(&fixture, 0, sizeof(fixture));
    write_dynamic(&fixture, R_DEBUG_ADDR);
    write_debug(&fixture, KZT_LOADER_DEBUG_CONSISTENT, MAP1_ADDR);
    write_map(&fixture, MAP1_ADDR, TLS_ELF_BASE, NAME1_ADDR,
              TLS_DYNAMIC_ADDR, MAP2_ADDR, 0);
    write_map(&fixture, MAP2_ADDR, 0, NAME2_ADDR,
              DUP_TLS_DYNAMIC_ADDR, 0, MAP1_ADDR);
    write_tls_object(&fixture);
    write_duplicate_tls_symbol(&fixture);
    kzt_public_loader_observer_reset(&observer);
    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_OK);

    CHECK(kzt_public_loader_collect_tls(
              &observer, &reader, tls_objects, 2, &tls_count) ==
          KZT_PUBLIC_LOADER_INVALID_STATE);
}

static void write_gnu_hash_dlopen_object(fixture_t *fixture)
{
    const kzt_x86_64_dynamic_entry_t dynamic[] = {
        { .tag = INT64_C(0x6ffffef5),
          .value = SYMBOL_HASH_ADDR - SYMBOL_ELF_BASE },
        { .tag = 5, .value = SYMBOL_STRING_ADDR - SYMBOL_ELF_BASE },
        { .tag = 6, .value = SYMBOL_TABLE_ADDR - SYMBOL_ELF_BASE },
        { .tag = 10, .value = sizeof("\0dlopen") },
        { .tag = 11, .value = sizeof(test_x86_64_symbol_t) },
        { .tag = KZT_X86_64_DT_NULL, .value = 0 },
    };
    const struct {
        uint32_t bucket_count;
        uint32_t symbol_offset;
        uint32_t bloom_size;
        uint32_t bloom_shift;
        uint64_t bloom;
        uint32_t bucket;
        uint32_t chain;
    } hash = {
        .bucket_count = 1,
        .symbol_offset = 1,
        .bloom_size = 1,
        .bucket = 1,
        .chain = 1,
    };
    const test_x86_64_symbol_t symbols[] = {
        { 0 },
        {
            .name = 1,
            .info = 0x12,
            .section_index = 1,
            .value = 0x905d0,
        },
    };
    static const char strings[] = "\0dlopen";

    fixture_write(fixture, SYMBOL_DYNAMIC_ADDR,
                  dynamic, sizeof(dynamic));
    fixture_write(fixture, SYMBOL_HASH_ADDR, &hash, sizeof(hash));
    fixture_write(fixture, SYMBOL_TABLE_ADDR,
                  symbols, sizeof(symbols));
    fixture_write(fixture, SYMBOL_STRING_ADDR,
                  strings, sizeof(strings));
}

static void test_symbol_lookup_uses_live_gnu_hash_object(void)
{
    fixture_t fixture;
    visit_log_t log = { 0 };
    kzt_public_loader_observer_t observer;
    const kzt_public_loader_reader_t reader = {
        .read_memory = fixture_read,
        .opaque = &fixture,
    };
    uintptr_t symbol_addr = 0;

    memset(&fixture, 0, sizeof(fixture));
    write_dynamic(&fixture, R_DEBUG_ADDR);
    write_debug(&fixture, KZT_LOADER_DEBUG_CONSISTENT, MAP1_ADDR);
    write_map(&fixture, MAP1_ADDR, SYMBOL_ELF_BASE, NAME1_ADDR,
              SYMBOL_DYNAMIC_ADDR, 0, 0);
    write_gnu_hash_dlopen_object(&fixture);
    kzt_public_loader_observer_reset(&observer);
    CHECK(kzt_public_loader_observer_activate(
              &observer, DYNAMIC_ADDR, 16, &reader,
              record_visit, &log) == KZT_PUBLIC_LOADER_OK);

    CHECK(kzt_public_loader_find_symbol(
              &observer, &reader, "dlopen", &symbol_addr) ==
          KZT_PUBLIC_LOADER_OK);
    CHECK(symbol_addr == SYMBOL_ELF_BASE + 0x905d0);
    CHECK(kzt_public_loader_find_symbol(
              &observer, &reader, "missing", &symbol_addr) ==
          KZT_PUBLIC_LOADER_NOT_FOUND);
}

int main(void)
{
    test_activate_reads_public_loader_state();
    test_busy_state_defers_new_object();
    test_deleted_address_can_be_observed_again();
    test_cycle_does_not_replace_last_complete_snapshot();
    test_preprotected_object_is_not_replayed();
    test_invalid_sources_fail_closed();
    test_relro_lookup_matches_one_object_before_protection();
    test_observed_processed_and_reported_states_are_distinct();
    test_object_relro_classification_uses_guest_memory();
    test_symbol_lookup_uses_live_gnu_hash_object();
    test_symbol_lookup_uses_live_relocated_elf_state();
    test_address_lookup_uses_exact_load_segments();
    test_address_lookup_rejects_ambiguity_and_malformed_elf();
    test_address_lookup_requires_active_consistent_observer();
    test_loader_state_probe_requires_same_consistent_instance();
    test_address_lookup_accepts_remembered_object_during_add();
    test_address_lookup_walks_beyond_snapshot_capacity();
    test_tls_collection_uses_live_image_and_module_relocation();
    test_tls_snapshot_walks_beyond_observer_capacity();
    test_tls_snapshot_does_not_consume_binding_visit();
    test_default_visible_tls_requires_unique_owner();
    puts("kzt public loader observer tests: PASS");
    return 0;
}
