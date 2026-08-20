/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "kzt_public_loader_observer.h"

_Static_assert(sizeof(uintptr_t) == sizeof(uint64_t),
               "KZT public loader observer requires a 64-bit host");
_Static_assert(sizeof(kzt_x86_64_dynamic_entry_t) == 16,
               "unexpected x86_64 Elf64_Dyn layout");
_Static_assert(offsetof(kzt_x86_64_r_debug_t, map) == 8,
               "unexpected x86_64 r_debug.r_map offset");
_Static_assert(offsetof(kzt_x86_64_r_debug_t, brk) == 16,
               "unexpected x86_64 r_debug.r_brk offset");
_Static_assert(offsetof(kzt_x86_64_r_debug_t, state) == 24,
               "unexpected x86_64 r_debug.r_state offset");
_Static_assert(sizeof(kzt_x86_64_r_debug_t) == 40,
               "unexpected x86_64 r_debug layout");
_Static_assert(sizeof(kzt_x86_64_link_map_prefix_t) == 40,
               "unexpected public x86_64 link_map prefix layout");

#define KZT_X86_64_ELFCLASS64 2
#define KZT_X86_64_ELFDATA2LSB 1
#define KZT_X86_64_EV_CURRENT 1
#define KZT_X86_64_PT_GNU_RELRO UINT32_C(0x6474e552)
#define KZT_X86_64_DT_HASH 4
#define KZT_X86_64_DT_STRTAB 5
#define KZT_X86_64_DT_SYMTAB 6
#define KZT_X86_64_DT_STRSZ 10
#define KZT_X86_64_DT_SYMENT 11
#define KZT_X86_64_DT_GNU_HASH INT64_C(0x6ffffef5)
#define KZT_PUBLIC_LOADER_MAX_DYNAMIC_ENTRIES 4096
#define KZT_PUBLIC_LOADER_MAX_SYMBOLS (1024 * 1024)
#define KZT_PUBLIC_LOADER_MAX_SYMBOL_NAME 512

typedef struct kzt_x86_64_elf_header {
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
} kzt_x86_64_elf_header_t;

typedef struct kzt_x86_64_program_header {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} kzt_x86_64_program_header_t;

typedef struct kzt_x86_64_symbol {
    uint32_t name;
    uint8_t info;
    uint8_t other;
    uint16_t section_index;
    uint64_t value;
    uint64_t size;
} kzt_x86_64_symbol_t;

_Static_assert(sizeof(kzt_x86_64_elf_header_t) == 64,
               "unexpected x86_64 ELF header layout");
_Static_assert(sizeof(kzt_x86_64_program_header_t) == 56,
               "unexpected x86_64 program header layout");
_Static_assert(sizeof(kzt_x86_64_symbol_t) == 24,
               "unexpected x86_64 symbol layout");

static int kzt_public_loader_read(
    const kzt_public_loader_reader_t *reader,
    uintptr_t guest_addr,
    void *dst,
    size_t size)
{
    if (!reader || !reader->read_memory || !guest_addr || !dst || !size) {
        return -1;
    }
    return reader->read_memory(guest_addr, dst, size, reader->opaque) == 0
               ? 0
               : -1;
}

static int kzt_public_loader_add_offset(uintptr_t base,
                                        size_t index,
                                        size_t stride,
                                        uintptr_t *result)
{
    size_t offset;

    if (!result || (index && stride > SIZE_MAX / index)) {
        return -1;
    }
    offset = index * stride;
    if (base > UINTPTR_MAX - offset) {
        return -1;
    }
    *result = base + offset;
    return 0;
}

static int kzt_public_loader_dynamic_pointer(
    const kzt_public_loader_object_t *object,
    uint64_t value,
    uintptr_t *result)
{
    if (!object || !value || !result) {
        return -1;
    }
    if (value >= object->load_bias) {
        *result = (uintptr_t)value;
        return 0;
    }
    if (value > UINTPTR_MAX - object->load_bias) {
        return -1;
    }
    *result = object->load_bias + (uintptr_t)value;
    return 0;
}

static kzt_public_loader_result_t kzt_public_loader_gnu_symbol_count(
    const kzt_public_loader_reader_t *reader,
    uintptr_t hash_addr,
    size_t *symbol_count)
{
    uint32_t header[4];
    uintptr_t buckets_addr;
    uintptr_t chains_addr;
    size_t maximum = 0;
    size_t bucket_index;

    if (kzt_public_loader_read(reader, hash_addr,
                               header, sizeof(header)) != 0) {
        return KZT_PUBLIC_LOADER_READ_ERROR;
    }
    if (!header[0] || !header[2] ||
        header[0] > KZT_PUBLIC_LOADER_MAX_SYMBOLS ||
        header[1] > KZT_PUBLIC_LOADER_MAX_SYMBOLS ||
        header[2] > KZT_PUBLIC_LOADER_MAX_SYMBOLS) {
        return KZT_PUBLIC_LOADER_LIMIT;
    }
    if (hash_addr > UINTPTR_MAX - sizeof(header) ||
        header[2] > (UINTPTR_MAX - hash_addr - sizeof(header)) /
                        sizeof(uint64_t)) {
        return KZT_PUBLIC_LOADER_OVERFLOW;
    }
    buckets_addr = hash_addr + sizeof(header) +
                   (uintptr_t)header[2] * sizeof(uint64_t);
    if (header[0] > (UINTPTR_MAX - buckets_addr) / sizeof(uint32_t)) {
        return KZT_PUBLIC_LOADER_OVERFLOW;
    }
    chains_addr = buckets_addr +
                  (uintptr_t)header[0] * sizeof(uint32_t);

    for (bucket_index = 0; bucket_index < header[0]; ++bucket_index) {
        uintptr_t entry_addr;
        uint32_t symbol_index;
        size_t chain_steps = 0;

        if (kzt_public_loader_add_offset(
                buckets_addr, bucket_index, sizeof(symbol_index),
                &entry_addr) != 0) {
            return KZT_PUBLIC_LOADER_OVERFLOW;
        }
        if (kzt_public_loader_read(reader, entry_addr,
                                   &symbol_index,
                                   sizeof(symbol_index)) != 0) {
            return KZT_PUBLIC_LOADER_READ_ERROR;
        }
        if (!symbol_index) {
            continue;
        }
        if (symbol_index < header[1] ||
            symbol_index > KZT_PUBLIC_LOADER_MAX_SYMBOLS) {
            return KZT_PUBLIC_LOADER_INVALID_STATE;
        }
        while (1) {
            uint32_t chain;

            if (kzt_public_loader_add_offset(
                    chains_addr, symbol_index - header[1],
                    sizeof(chain), &entry_addr) != 0) {
                return KZT_PUBLIC_LOADER_OVERFLOW;
            }
            if (kzt_public_loader_read(reader, entry_addr,
                                       &chain, sizeof(chain)) != 0) {
                return KZT_PUBLIC_LOADER_READ_ERROR;
            }
            if (symbol_index > maximum) {
                maximum = symbol_index;
            }
            if (chain & 1) {
                break;
            }
            if (++chain_steps == KZT_PUBLIC_LOADER_MAX_SYMBOLS ||
                ++symbol_index > KZT_PUBLIC_LOADER_MAX_SYMBOLS) {
                return KZT_PUBLIC_LOADER_LIMIT;
            }
        }
    }
    *symbol_count = maximum ? maximum + 1 : header[1];
    return KZT_PUBLIC_LOADER_OK;
}

static int kzt_public_loader_symbol_name_matches(
    const kzt_public_loader_reader_t *reader,
    uintptr_t string_addr,
    size_t available,
    const char *symbol_name,
    size_t symbol_name_size)
{
    size_t index;

    if (symbol_name_size + 1 > available) {
        return 0;
    }
    for (index = 0; index <= symbol_name_size; ++index) {
        char byte;

        if (string_addr > UINTPTR_MAX - index ||
            kzt_public_loader_read(reader, string_addr + index,
                                   &byte, sizeof(byte)) != 0) {
            return -1;
        }
        if (byte != symbol_name[index]) {
            return 0;
        }
    }
    return 1;
}

static kzt_public_loader_result_t kzt_public_loader_find_object_symbol(
    const kzt_public_loader_object_t *object,
    const kzt_public_loader_reader_t *reader,
    const char *symbol_name,
    size_t symbol_name_size,
    uintptr_t *symbol_addr)
{
    uintptr_t hash_addr = 0;
    uintptr_t string_table_addr = 0;
    uintptr_t symbol_table_addr = 0;
    uint64_t string_table_value = 0;
    uint64_t symbol_table_value = 0;
    uint64_t hash_value = 0;
    uint64_t gnu_hash_value = 0;
    uint64_t string_table_size = 0;
    uint64_t symbol_entry_size = 0;
    uint32_t hash_header[2];
    size_t symbol_count;
    size_t index;
    int terminated = 0;

    if (!object->dynamic_addr) {
        return KZT_PUBLIC_LOADER_NOT_FOUND;
    }
    for (index = 0; index < KZT_PUBLIC_LOADER_MAX_DYNAMIC_ENTRIES;
         ++index) {
        kzt_x86_64_dynamic_entry_t entry;
        uintptr_t entry_addr;

        if (kzt_public_loader_add_offset(
                object->dynamic_addr, index, sizeof(entry),
                &entry_addr) != 0) {
            return KZT_PUBLIC_LOADER_OVERFLOW;
        }
        if (kzt_public_loader_read(reader, entry_addr,
                                   &entry, sizeof(entry)) != 0) {
            return KZT_PUBLIC_LOADER_READ_ERROR;
        }
        if (entry.tag == KZT_X86_64_DT_NULL) {
            terminated = 1;
            break;
        }
        switch (entry.tag) {
        case KZT_X86_64_DT_HASH:
            hash_value = entry.value;
            break;
        case KZT_X86_64_DT_GNU_HASH:
            gnu_hash_value = entry.value;
            break;
        case KZT_X86_64_DT_STRTAB:
            string_table_value = entry.value;
            break;
        case KZT_X86_64_DT_SYMTAB:
            symbol_table_value = entry.value;
            break;
        case KZT_X86_64_DT_STRSZ:
            string_table_size = entry.value;
            break;
        case KZT_X86_64_DT_SYMENT:
            symbol_entry_size = entry.value;
            break;
        default:
            break;
        }
    }
    if (!terminated) {
        return KZT_PUBLIC_LOADER_LIMIT;
    }
    if ((!hash_value && !gnu_hash_value) ||
        !string_table_value || !symbol_table_value ||
        !string_table_size ||
        symbol_entry_size != sizeof(kzt_x86_64_symbol_t)) {
        return KZT_PUBLIC_LOADER_NOT_FOUND;
    }
    if (kzt_public_loader_dynamic_pointer(
            object, hash_value ? hash_value : gnu_hash_value,
            &hash_addr) != 0 ||
        kzt_public_loader_dynamic_pointer(
            object, string_table_value, &string_table_addr) != 0 ||
        kzt_public_loader_dynamic_pointer(
            object, symbol_table_value, &symbol_table_addr) != 0) {
        return KZT_PUBLIC_LOADER_OVERFLOW;
    }
    if (hash_value) {
        if (kzt_public_loader_read(reader, hash_addr,
                                   hash_header, sizeof(hash_header)) != 0) {
            return KZT_PUBLIC_LOADER_READ_ERROR;
        }
        if (!hash_header[1]) {
            return KZT_PUBLIC_LOADER_NOT_FOUND;
        }
        if (hash_header[1] > KZT_PUBLIC_LOADER_MAX_SYMBOLS) {
            return KZT_PUBLIC_LOADER_LIMIT;
        }
        symbol_count = hash_header[1];
    } else {
        kzt_public_loader_result_t result =
            kzt_public_loader_gnu_symbol_count(
                reader, hash_addr, &symbol_count);

        if (result != KZT_PUBLIC_LOADER_OK) {
            return result;
        }
    }

    for (index = 0; index < symbol_count; ++index) {
        kzt_x86_64_symbol_t symbol;
        uintptr_t entry_addr;
        uintptr_t name_addr;
        int matches;

        if (kzt_public_loader_add_offset(
                symbol_table_addr, index, sizeof(symbol),
                &entry_addr) != 0) {
            return KZT_PUBLIC_LOADER_OVERFLOW;
        }
        if (kzt_public_loader_read(reader, entry_addr,
                                   &symbol, sizeof(symbol)) != 0) {
            return KZT_PUBLIC_LOADER_READ_ERROR;
        }
        if (!symbol.name || symbol.name >= string_table_size ||
            !symbol.section_index || !symbol.value) {
            continue;
        }
        if (string_table_addr > UINTPTR_MAX - symbol.name) {
            return KZT_PUBLIC_LOADER_OVERFLOW;
        }
        name_addr = string_table_addr + symbol.name;
        matches = kzt_public_loader_symbol_name_matches(
            reader, name_addr, string_table_size - symbol.name,
            symbol_name, symbol_name_size);
        if (matches < 0) {
            return KZT_PUBLIC_LOADER_READ_ERROR;
        }
        if (!matches) {
            continue;
        }
        if (symbol.value > UINTPTR_MAX - object->load_bias) {
            return KZT_PUBLIC_LOADER_OVERFLOW;
        }
        *symbol_addr = object->load_bias + (uintptr_t)symbol.value;
        return KZT_PUBLIC_LOADER_OK;
    }
    return KZT_PUBLIC_LOADER_NOT_FOUND;
}

static kzt_public_loader_result_t kzt_public_loader_find_r_debug(
    uintptr_t dynamic_addr,
    size_t max_dynamic_entries,
    const kzt_public_loader_reader_t *reader,
    uintptr_t *r_debug_addr)
{
    size_t index;

    if (!dynamic_addr || !max_dynamic_entries || !reader ||
        !reader->read_memory || !r_debug_addr) {
        return KZT_PUBLIC_LOADER_INVALID_INPUT;
    }
    *r_debug_addr = 0;

    for (index = 0; index < max_dynamic_entries; ++index) {
        kzt_x86_64_dynamic_entry_t entry;
        uintptr_t entry_addr;

        if (kzt_public_loader_add_offset(
                dynamic_addr, index, sizeof(entry), &entry_addr) != 0) {
            return KZT_PUBLIC_LOADER_OVERFLOW;
        }
        if (kzt_public_loader_read(
                reader, entry_addr, &entry, sizeof(entry)) != 0) {
            return KZT_PUBLIC_LOADER_READ_ERROR;
        }
        if (entry.tag == KZT_X86_64_DT_DEBUG && entry.value) {
            *r_debug_addr = (uintptr_t)entry.value;
            return KZT_PUBLIC_LOADER_OK;
        }
        if (entry.tag == KZT_X86_64_DT_NULL) {
            return KZT_PUBLIC_LOADER_NOT_FOUND;
        }
    }
    return KZT_PUBLIC_LOADER_NOT_FOUND;
}

static int kzt_public_loader_contains(const uintptr_t *maps,
                                      size_t map_count,
                                      uintptr_t map_addr)
{
    size_t index;

    for (index = 0; index < map_count; ++index) {
        if (maps[index] == map_addr) {
            return 1;
        }
    }
    return 0;
}

static int kzt_public_loader_objects_contain(
    const kzt_public_loader_object_t *objects,
    size_t object_count,
    uintptr_t map_addr)
{
    size_t index;

    for (index = 0; index < object_count; ++index) {
        if (objects[index].link_map_addr == map_addr) {
            return 1;
        }
    }
    return 0;
}

static void kzt_public_loader_retain_live_state(
    uintptr_t *maps,
    size_t *map_count,
    const kzt_public_loader_object_t *objects,
    size_t object_count)
{
    size_t read_index;
    size_t write_index = 0;

    for (read_index = 0; read_index < *map_count; ++read_index) {
        if (kzt_public_loader_objects_contain(
                objects, object_count, maps[read_index])) {
            maps[write_index++] = maps[read_index];
        }
    }
    *map_count = write_index;
}

static int kzt_public_loader_relro_matches(
    const kzt_public_loader_reader_t *reader,
    uintptr_t load_bias,
    uintptr_t protect_start,
    uintptr_t protect_end,
    size_t page_size)
{
    kzt_x86_64_elf_header_t header;
    uintptr_t page_mask = page_size - 1;
    uintptr_t phdr_base;
    size_t index;

    if (!load_bias) {
        return 0;
    }
    if (kzt_public_loader_read(reader, load_bias,
                               &header, sizeof(header)) != 0) {
        return -1;
    }
    if (header.ident[0] != 0x7f || header.ident[1] != 'E' ||
        header.ident[2] != 'L' || header.ident[3] != 'F' ||
        header.ident[4] != KZT_X86_64_ELFCLASS64 ||
        header.ident[5] != KZT_X86_64_ELFDATA2LSB ||
        header.ident[6] != KZT_X86_64_EV_CURRENT ||
        header.phentsize != sizeof(kzt_x86_64_program_header_t) ||
        !header.phnum || header.phnum > KZT_PUBLIC_LOADER_MAX_OBJECTS ||
        header.phoff > UINTPTR_MAX - load_bias) {
        return 0;
    }
    phdr_base = load_bias + (uintptr_t)header.phoff;

    for (index = 0; index < header.phnum; ++index) {
        kzt_x86_64_program_header_t phdr;
        uintptr_t phdr_addr;
        uintptr_t relro_start;
        uintptr_t relro_end;

        if (kzt_public_loader_add_offset(
                phdr_base, index, sizeof(phdr), &phdr_addr) != 0) {
            return -1;
        }
        if (kzt_public_loader_read(reader, phdr_addr,
                                   &phdr, sizeof(phdr)) != 0) {
            return -1;
        }
        if (phdr.type != KZT_X86_64_PT_GNU_RELRO || !phdr.memsz) {
            continue;
        }
        if (phdr.vaddr > UINTPTR_MAX - load_bias) {
            return -1;
        }
        relro_start = load_bias + (uintptr_t)phdr.vaddr;
        if (phdr.memsz > UINTPTR_MAX - relro_start ||
            relro_start + (uintptr_t)phdr.memsz >
                UINTPTR_MAX - page_mask) {
            return -1;
        }
        relro_end = (relro_start + (uintptr_t)phdr.memsz + page_mask) &
                    ~page_mask;
        relro_start &= ~page_mask;
        /*
         * Accept a loader that protects one RELRO range in several calls,
         * or one protection call that fully contains this RELRO range.  A
         * mere overlap is ambiguous and must stay on the guest path.
         */
        return (protect_start >= relro_start && protect_end <= relro_end) ||
               (relro_start >= protect_start && relro_end <= protect_end);
    }
    return 0;
}

kzt_public_loader_result_t kzt_public_loader_object_has_relro(
    const kzt_public_loader_object_t *object,
    const kzt_public_loader_reader_t *reader,
    int *has_relro)
{
    kzt_x86_64_elf_header_t header;
    uintptr_t phdr_base;
    size_t index;

    if (!object || !object->load_bias || !reader ||
        !reader->read_memory || !has_relro) {
        return KZT_PUBLIC_LOADER_INVALID_INPUT;
    }
    *has_relro = 0;
    if (kzt_public_loader_read(reader, object->load_bias,
                               &header, sizeof(header)) != 0) {
        return KZT_PUBLIC_LOADER_READ_ERROR;
    }
    if (header.ident[0] != 0x7f || header.ident[1] != 'E' ||
        header.ident[2] != 'L' || header.ident[3] != 'F' ||
        header.ident[4] != KZT_X86_64_ELFCLASS64 ||
        header.ident[5] != KZT_X86_64_ELFDATA2LSB ||
        header.ident[6] != KZT_X86_64_EV_CURRENT ||
        header.phentsize != sizeof(kzt_x86_64_program_header_t) ||
        !header.phnum || header.phoff > UINTPTR_MAX - object->load_bias) {
        return KZT_PUBLIC_LOADER_INVALID_STATE;
    }
    if (header.phnum > KZT_PUBLIC_LOADER_MAX_OBJECTS) {
        return KZT_PUBLIC_LOADER_LIMIT;
    }
    phdr_base = object->load_bias + (uintptr_t)header.phoff;
    for (index = 0; index < header.phnum; ++index) {
        kzt_x86_64_program_header_t phdr;
        uintptr_t phdr_addr;

        if (kzt_public_loader_add_offset(
                phdr_base, index, sizeof(phdr), &phdr_addr) != 0) {
            return KZT_PUBLIC_LOADER_OVERFLOW;
        }
        if (kzt_public_loader_read(reader, phdr_addr,
                                   &phdr, sizeof(phdr)) != 0) {
            return KZT_PUBLIC_LOADER_READ_ERROR;
        }
        if (phdr.type == KZT_X86_64_PT_GNU_RELRO && phdr.memsz) {
            *has_relro = 1;
            return KZT_PUBLIC_LOADER_OK;
        }
    }
    return KZT_PUBLIC_LOADER_OK;
}

static kzt_public_loader_result_t kzt_public_loader_capture(
    kzt_public_loader_observer_t *observer,
    const kzt_public_loader_reader_t *reader,
    kzt_public_loader_visit_fn visit,
    void *visit_opaque,
    uintptr_t *observed_brk)
{
    kzt_x86_64_r_debug_t debug;
    kzt_public_loader_object_t objects[KZT_PUBLIC_LOADER_MAX_OBJECTS];
    uintptr_t current;
    size_t count = 0;
    size_t index;

    if (!observer || !observer->r_debug_addr || !reader ||
        !reader->read_memory || !visit) {
        return KZT_PUBLIC_LOADER_INVALID_INPUT;
    }
    if (kzt_public_loader_read(reader, observer->r_debug_addr,
                               &debug, sizeof(debug)) != 0) {
        return KZT_PUBLIC_LOADER_READ_ERROR;
    }
    if (debug.version < 1 ||
        debug.state < KZT_LOADER_DEBUG_CONSISTENT ||
        debug.state > KZT_LOADER_DEBUG_DELETE) {
        return KZT_PUBLIC_LOADER_INVALID_STATE;
    }
    if (debug.state != KZT_LOADER_DEBUG_CONSISTENT) {
        return KZT_PUBLIC_LOADER_BUSY;
    }
    if (!debug.map || !debug.brk) {
        return KZT_PUBLIC_LOADER_NOT_FOUND;
    }

    current = (uintptr_t)debug.map;
    while (current) {
        kzt_x86_64_link_map_prefix_t map;
        kzt_public_loader_object_t *object;

        if (count == KZT_PUBLIC_LOADER_MAX_OBJECTS) {
            return KZT_PUBLIC_LOADER_LIMIT;
        }
        for (index = 0; index < count; ++index) {
            if (objects[index].link_map_addr == current) {
                return KZT_PUBLIC_LOADER_CYCLE;
            }
        }
        if (kzt_public_loader_read(reader, current,
                                   &map, sizeof(map)) != 0) {
            return KZT_PUBLIC_LOADER_READ_ERROR;
        }

        object = &objects[count++];
        object->link_map_addr = current;
        object->load_bias = (uintptr_t)map.load_bias;
        object->name_addr = (uintptr_t)map.name;
        object->dynamic_addr = (uintptr_t)map.dynamic_addr;
        object->next_addr = (uintptr_t)map.next;
        object->previous_addr = (uintptr_t)map.previous;
        current = object->next_addr;
    }

    for (index = 0; index < count; ++index) {
        if (!kzt_public_loader_contains(observer->live_maps,
                                        observer->live_map_count,
                                        objects[index].link_map_addr) &&
            visit(&objects[index], visit_opaque) != 0) {
            return KZT_PUBLIC_LOADER_VISITOR_ERROR;
        }
    }

    kzt_public_loader_retain_live_state(
        observer->processed_maps, &observer->processed_map_count,
        objects, count);
    kzt_public_loader_retain_live_state(
        observer->fallback_reported_maps,
        &observer->fallback_reported_map_count, objects, count);
    for (index = 0; index < count; ++index) {
        observer->live_maps[index] = objects[index].link_map_addr;
    }
    observer->live_map_count = count;
    if (observed_brk) {
        *observed_brk = (uintptr_t)debug.brk;
    }
    return KZT_PUBLIC_LOADER_OK;
}

void kzt_public_loader_observer_reset(
    kzt_public_loader_observer_t *observer)
{
    if (observer) {
        memset(observer, 0, sizeof(*observer));
    }
}

kzt_public_loader_result_t kzt_public_loader_observer_remember(
    kzt_public_loader_observer_t *observer,
    uintptr_t link_map_addr)
{
    if (!observer || !link_map_addr) {
        return KZT_PUBLIC_LOADER_INVALID_INPUT;
    }
    if (kzt_public_loader_contains(observer->live_maps,
                                   observer->live_map_count,
                                   link_map_addr)) {
        return KZT_PUBLIC_LOADER_OK;
    }
    if (observer->live_map_count == KZT_PUBLIC_LOADER_MAX_OBJECTS) {
        return KZT_PUBLIC_LOADER_LIMIT;
    }
    observer->live_maps[observer->live_map_count++] = link_map_addr;
    return KZT_PUBLIC_LOADER_OK;
}

int kzt_public_loader_observer_has_map(
    const kzt_public_loader_observer_t *observer,
    uintptr_t link_map_addr)
{
    return observer && link_map_addr &&
           kzt_public_loader_contains(observer->live_maps,
                                      observer->live_map_count,
                                      link_map_addr);
}

kzt_public_loader_result_t kzt_public_loader_observer_mark_processed(
    kzt_public_loader_observer_t *observer,
    uintptr_t link_map_addr)
{
    if (!observer || !link_map_addr) {
        return KZT_PUBLIC_LOADER_INVALID_INPUT;
    }
    if (kzt_public_loader_contains(observer->processed_maps,
                                   observer->processed_map_count,
                                   link_map_addr)) {
        return KZT_PUBLIC_LOADER_OK;
    }
    if (observer->processed_map_count == KZT_PUBLIC_LOADER_MAX_OBJECTS) {
        return KZT_PUBLIC_LOADER_LIMIT;
    }
    observer->processed_maps[observer->processed_map_count++] = link_map_addr;
    return KZT_PUBLIC_LOADER_OK;
}

int kzt_public_loader_observer_is_processed(
    const kzt_public_loader_observer_t *observer,
    uintptr_t link_map_addr)
{
    return observer && link_map_addr &&
           kzt_public_loader_contains(observer->processed_maps,
                                      observer->processed_map_count,
                                      link_map_addr);
}

int kzt_public_loader_observer_mark_fallback_reported(
    kzt_public_loader_observer_t *observer,
    uintptr_t link_map_addr)
{
    if (!observer || !link_map_addr ||
        kzt_public_loader_contains(observer->fallback_reported_maps,
                                   observer->fallback_reported_map_count,
                                   link_map_addr) ||
        observer->fallback_reported_map_count ==
            KZT_PUBLIC_LOADER_MAX_OBJECTS) {
        return 0;
    }
    observer->fallback_reported_maps[
        observer->fallback_reported_map_count++] = link_map_addr;
    return 1;
}

int kzt_public_loader_observer_fallback_was_reported(
    const kzt_public_loader_observer_t *observer,
    uintptr_t link_map_addr)
{
    return observer && link_map_addr &&
           kzt_public_loader_contains(observer->fallback_reported_maps,
                                      observer->fallback_reported_map_count,
                                      link_map_addr);
}

kzt_public_loader_result_t kzt_public_loader_find_relro_object(
    uintptr_t dynamic_addr,
    size_t max_dynamic_entries,
    uintptr_t protect_start,
    size_t protect_size,
    size_t page_size,
    const kzt_public_loader_reader_t *reader,
    kzt_public_loader_object_t *object)
{
    kzt_x86_64_r_debug_t debug;
    uintptr_t visited[KZT_PUBLIC_LOADER_MAX_OBJECTS];
    uintptr_t r_debug_addr;
    uintptr_t protect_end;
    uintptr_t current;
    size_t count = 0;
    size_t index;
    int found = 0;
    kzt_public_loader_result_t result;

    if (!dynamic_addr || !max_dynamic_entries || !protect_start ||
        !protect_size || !page_size || (page_size & (page_size - 1)) ||
        (protect_start & (page_size - 1)) ||
        (protect_size & (page_size - 1)) ||
        protect_size > UINTPTR_MAX - protect_start ||
        !reader || !reader->read_memory || !object) {
        return KZT_PUBLIC_LOADER_INVALID_INPUT;
    }
    memset(object, 0, sizeof(*object));
    protect_end = protect_start + protect_size;

    result = kzt_public_loader_find_r_debug(
        dynamic_addr, max_dynamic_entries, reader, &r_debug_addr);
    if (result != KZT_PUBLIC_LOADER_OK) {
        return result;
    }
    if (kzt_public_loader_read(reader, r_debug_addr,
                               &debug, sizeof(debug)) != 0) {
        return KZT_PUBLIC_LOADER_READ_ERROR;
    }
    if (debug.version < 1 ||
        debug.state < KZT_LOADER_DEBUG_CONSISTENT ||
        debug.state > KZT_LOADER_DEBUG_DELETE) {
        return KZT_PUBLIC_LOADER_INVALID_STATE;
    }
    if (debug.state != KZT_LOADER_DEBUG_ADD &&
        debug.state != KZT_LOADER_DEBUG_CONSISTENT) {
        return KZT_PUBLIC_LOADER_BUSY;
    }
    if (!debug.map) {
        return KZT_PUBLIC_LOADER_NOT_FOUND;
    }

    current = (uintptr_t)debug.map;
    while (current) {
        kzt_x86_64_link_map_prefix_t map;
        int matches;

        if (count == KZT_PUBLIC_LOADER_MAX_OBJECTS) {
            return KZT_PUBLIC_LOADER_LIMIT;
        }
        for (index = 0; index < count; ++index) {
            if (visited[index] == current) {
                return KZT_PUBLIC_LOADER_CYCLE;
            }
        }
        visited[count++] = current;
        if (kzt_public_loader_read(reader, current,
                                   &map, sizeof(map)) != 0) {
            return KZT_PUBLIC_LOADER_READ_ERROR;
        }
        matches = kzt_public_loader_relro_matches(
            reader, (uintptr_t)map.load_bias,
            protect_start, protect_end, page_size);
        if (matches < 0) {
            return KZT_PUBLIC_LOADER_READ_ERROR;
        }
        if (matches) {
            if (found) {
                memset(object, 0, sizeof(*object));
                return KZT_PUBLIC_LOADER_INVALID_STATE;
            }
            object->link_map_addr = current;
            object->load_bias = (uintptr_t)map.load_bias;
            object->name_addr = (uintptr_t)map.name;
            object->dynamic_addr = (uintptr_t)map.dynamic_addr;
            object->next_addr = (uintptr_t)map.next;
            object->previous_addr = (uintptr_t)map.previous;
            found = 1;
        }
        current = (uintptr_t)map.next;
    }
    return found ? KZT_PUBLIC_LOADER_OK : KZT_PUBLIC_LOADER_NOT_FOUND;
}

kzt_public_loader_result_t kzt_public_loader_observer_activate(
    kzt_public_loader_observer_t *observer,
    uintptr_t dynamic_addr,
    size_t max_dynamic_entries,
    const kzt_public_loader_reader_t *reader,
    kzt_public_loader_visit_fn visit,
    void *visit_opaque)
{
    kzt_public_loader_result_t result;
    uintptr_t r_debug_addr = 0;
    uintptr_t r_brk_addr = 0;

    if (!observer) {
        return KZT_PUBLIC_LOADER_INVALID_INPUT;
    }
    observer->active = 0;
    observer->r_debug_addr = 0;
    observer->r_brk_addr = 0;

    result = kzt_public_loader_find_r_debug(
        dynamic_addr, max_dynamic_entries, reader, &r_debug_addr);
    if (result != KZT_PUBLIC_LOADER_OK) {
        return result;
    }

    observer->r_debug_addr = r_debug_addr;
    result = kzt_public_loader_capture(observer, reader, visit,
                                       visit_opaque, &r_brk_addr);
    if (result != KZT_PUBLIC_LOADER_OK) {
        observer->r_debug_addr = 0;
        return result;
    }
    observer->r_brk_addr = r_brk_addr;
    observer->active = 1;
    return KZT_PUBLIC_LOADER_OK;
}

kzt_public_loader_result_t kzt_public_loader_observer_refresh(
    kzt_public_loader_observer_t *observer,
    const kzt_public_loader_reader_t *reader,
    kzt_public_loader_visit_fn visit,
    void *visit_opaque)
{
    uintptr_t observed_brk = 0;
    kzt_public_loader_result_t result;

    if (!observer || !observer->active) {
        return KZT_PUBLIC_LOADER_INVALID_INPUT;
    }
    result = kzt_public_loader_capture(observer, reader, visit,
                                       visit_opaque, &observed_brk);
    if (result == KZT_PUBLIC_LOADER_OK &&
        observed_brk != observer->r_brk_addr) {
        return KZT_PUBLIC_LOADER_INVALID_STATE;
    }
    return result;
}

kzt_public_loader_result_t kzt_public_loader_find_symbol(
    const kzt_public_loader_observer_t *observer,
    const kzt_public_loader_reader_t *reader,
    const char *symbol_name,
    uintptr_t *symbol_addr)
{
    uintptr_t found_addr = 0;
    size_t symbol_name_size;
    size_t index;

    if (!observer || !observer->active || !reader ||
        !reader->read_memory || !symbol_name || !symbol_addr) {
        return KZT_PUBLIC_LOADER_INVALID_INPUT;
    }
    symbol_name_size = strlen(symbol_name);
    if (!symbol_name_size ||
        symbol_name_size > KZT_PUBLIC_LOADER_MAX_SYMBOL_NAME) {
        return KZT_PUBLIC_LOADER_INVALID_INPUT;
    }
    *symbol_addr = 0;

    for (index = 0; index < observer->live_map_count; ++index) {
        kzt_x86_64_link_map_prefix_t map;
        kzt_public_loader_object_t object;
        uintptr_t candidate = 0;
        kzt_public_loader_result_t result;

        if (kzt_public_loader_read(reader, observer->live_maps[index],
                                   &map, sizeof(map)) != 0) {
            return KZT_PUBLIC_LOADER_READ_ERROR;
        }
        object = (kzt_public_loader_object_t) {
            .link_map_addr = observer->live_maps[index],
            .load_bias = (uintptr_t)map.load_bias,
            .name_addr = (uintptr_t)map.name,
            .dynamic_addr = (uintptr_t)map.dynamic_addr,
            .next_addr = (uintptr_t)map.next,
            .previous_addr = (uintptr_t)map.previous,
        };
        result = kzt_public_loader_find_object_symbol(
            &object, reader, symbol_name, symbol_name_size, &candidate);
        if (result == KZT_PUBLIC_LOADER_NOT_FOUND) {
            continue;
        }
        if (result != KZT_PUBLIC_LOADER_OK) {
            return result;
        }
        if (found_addr && found_addr != candidate) {
            return KZT_PUBLIC_LOADER_INVALID_STATE;
        }
        found_addr = candidate;
    }
    if (!found_addr) {
        return KZT_PUBLIC_LOADER_NOT_FOUND;
    }
    *symbol_addr = found_addr;
    return KZT_PUBLIC_LOADER_OK;
}

const char *kzt_public_loader_result_name(
    kzt_public_loader_result_t result)
{
    switch (result) {
    case KZT_PUBLIC_LOADER_OK:
        return "OK";
    case KZT_PUBLIC_LOADER_BUSY:
        return "BUSY";
    case KZT_PUBLIC_LOADER_INVALID_INPUT:
        return "INVALID_INPUT";
    case KZT_PUBLIC_LOADER_NOT_FOUND:
        return "NOT_FOUND";
    case KZT_PUBLIC_LOADER_READ_ERROR:
        return "READ_ERROR";
    case KZT_PUBLIC_LOADER_INVALID_STATE:
        return "INVALID_STATE";
    case KZT_PUBLIC_LOADER_CYCLE:
        return "CYCLE";
    case KZT_PUBLIC_LOADER_LIMIT:
        return "LIMIT";
    case KZT_PUBLIC_LOADER_VISITOR_ERROR:
        return "VISITOR_ERROR";
    case KZT_PUBLIC_LOADER_OVERFLOW:
        return "OVERFLOW";
    }
    return "UNKNOWN";
}
