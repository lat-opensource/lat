#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "tbset.h"
#include "cfg_decoder.h"
#include "elf_image.h"
#include "lat-tb-key-set.h"
#include "lat-aot-v2.h"

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef STT_GNU_IFUNC
#define STT_GNU_IFUNC 10
#endif

typedef struct TbTemplateIndex {
    uint64_t pc;
    uint32_t flags;
    size_t index;
} TbTemplateIndex;

static int compare_template_index(const void *left, const void *right)
{
    const TbTemplateIndex *a = left;
    const TbTemplateIndex *b = right;

    if (a->pc != b->pc) {
        return a->pc < b->pc ? -1 : 1;
    }
    return (a->index > b->index) - (a->index < b->index);
}

static void sort_template_indices(TbTemplateIndex *templates, size_t count)
{
    if (count < 4096) {
        qsort(templates, count, sizeof(*templates), compare_template_index);
        return;
    }

    TbTemplateIndex *scratch = malloc(count * sizeof(*scratch));
    size_t *offsets = calloc(1U << 16, sizeof(*offsets));
    if (!scratch || !offsets) {
        free(offsets);
        free(scratch);
        qsort(templates, count, sizeof(*templates), compare_template_index);
        return;
    }

    TbTemplateIndex *source = templates;
    TbTemplateIndex *destination = scratch;
    for (unsigned int shift = 0; shift < 64; shift += 16) {
        size_t next = 0;

        memset(offsets, 0, (1U << 16) * sizeof(*offsets));
        for (size_t i = 0; i < count; i++) {
            offsets[(source[i].pc >> shift) & 0xffff]++;
        }
        for (size_t bucket = 0; bucket < (1U << 16); bucket++) {
            size_t bucket_count = offsets[bucket];
            offsets[bucket] = next;
            next += bucket_count;
        }
        for (size_t i = 0; i < count; i++) {
            size_t bucket = (source[i].pc >> shift) & 0xffff;
            destination[offsets[bucket]++] = source[i];
        }
        TbTemplateIndex *swap = source;
        source = destination;
        destination = swap;
    }

    free(offsets);
    free(scratch);
}

static size_t template_lower_bound(const TbTemplateIndex *templates,
                                   size_t count, uint64_t pc)
{
    size_t low = 0;
    size_t high = count;

    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (templates[middle].pc < pc) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low;
}

static void template_lookup(const TbTemplateIndex *templates, size_t count,
                            uint64_t pc, uint32_t flags,
                            size_t *same_pc, size_t *exact)
{
    size_t position = template_lower_bound(templates, count, pc);

    *same_pc = SIZE_MAX;
    *exact = SIZE_MAX;
    for (; position < count && templates[position].pc == pc; position++) {
        if (*same_pc == SIZE_MAX) {
            *same_pc = templates[position].index;
        }
        if (templates[position].flags == flags) {
            *exact = templates[position].index;
        }
    }
}

static int fail(char *error, size_t size, const char *message)
{
    if (error && size) snprintf(error, size, "%s", message);
    return -1;
}

static int source_identity(const char *path, uint8_t digest[32],
                           uint64_t *base, char *error, size_t error_size)
{
    const char *trusted_digest = getenv("LATC_TRUSTED_SOURCE_SHA256");
    bool digest_supplied = trusted_digest && strlen(trusted_digest) == 64;
    for (size_t i = 0; digest_supplied && i < 32; i++) {
        int high = g_ascii_xdigit_value(trusted_digest[i * 2]);
        int low = g_ascii_xdigit_value(trusted_digest[i * 2 + 1]);
        if (high < 0 || low < 0) {
            digest_supplied = false;
        } else {
            digest[i] = (uint8_t)((high << 4) | low);
        }
    }
    if (digest_supplied) {
        int fd = open(path, O_RDONLY | O_CLOEXEC);
        Elf64_Ehdr header;
        struct stat status;
        if (fd < 0 || fstat(fd, &status) || status.st_size < 0 ||
            pread(fd, &header, sizeof(header), 0) != sizeof(header) ||
            memcmp(header.e_ident, ELFMAG, SELFMAG) ||
            header.e_ident[EI_CLASS] != ELFCLASS64 ||
            header.e_ident[EI_DATA] != ELFDATA2LSB ||
            header.e_phentsize != sizeof(Elf64_Phdr) || !header.e_phnum ||
            header.e_phoff > (uint64_t)status.st_size ||
            header.e_phnum > ((uint64_t)status.st_size - header.e_phoff) /
                             sizeof(Elf64_Phdr)) {
            if (fd >= 0) close(fd);
            goto malformed_trusted;
        }
        uint64_t result = UINT64_MAX;
        for (uint16_t i = 0; i < header.e_phnum; i++) {
            Elf64_Phdr phdr;
            off_t offset = (off_t)(header.e_phoff + i * sizeof(phdr));
            if (pread(fd, &phdr, sizeof(phdr), offset) != sizeof(phdr)) {
                close(fd);
                goto malformed_trusted;
            }
            if (phdr.p_type == PT_LOAD && phdr.p_memsz &&
                phdr.p_vaddr < result) {
                result = phdr.p_vaddr;
            }
        }
        close(fd);
        if (result == UINT64_MAX) goto malformed_trusted;
        *base = result;
        return 0;
    }

    gchar *contents = NULL;
    gsize size = 0;
    GError *gerror = NULL;
    if (!g_file_get_contents(path, &contents, &size, &gerror)) {
        if (error && error_size) {
            snprintf(error, error_size, "%s: %s", path,
                     gerror ? gerror->message : "cannot read source");
        }
        g_clear_error(&gerror);
        return -1;
    }
    GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(checksum, (const guchar *)contents, size);
    gsize digest_size = 32;
    g_checksum_get_digest(checksum, digest, &digest_size);
    g_checksum_free(checksum);
    if (digest_size != 32) goto malformed;
    Elf64_Ehdr header;
    if (size < sizeof(header)) goto malformed;
    memcpy(&header, contents, sizeof(header));
    if (memcmp(header.e_ident, ELFMAG, SELFMAG) ||
        header.e_ident[EI_CLASS] != ELFCLASS64 ||
        header.e_ident[EI_DATA] != ELFDATA2LSB ||
        header.e_phentsize != sizeof(Elf64_Phdr) || !header.e_phnum ||
        header.e_phoff > size ||
        header.e_phnum > (size - header.e_phoff) / sizeof(Elf64_Phdr)) {
        goto malformed;
    }
    uint64_t result = UINT64_MAX;
    for (uint16_t i = 0; i < header.e_phnum; i++) {
        Elf64_Phdr phdr;
        memcpy(&phdr, contents + header.e_phoff + i * sizeof(phdr),
               sizeof(phdr));
        if (phdr.p_type == PT_LOAD && phdr.p_memsz && phdr.p_vaddr < result) {
            result = phdr.p_vaddr;
        }
    }
    if (result == UINT64_MAX) goto malformed;
    g_free(contents);
    *base = result;
    return 0;

malformed:
    g_free(contents);
malformed_trusted:
    if (error && error_size) {
        snprintf(error, error_size,
                 "%s: cannot determine preferred guest base", path);
    }
    return -1;
}

static int add_parallel_dynamic_entries(const char *path, CfgProgram *program,
                                        char *error, size_t error_size)
{
    gchar *contents = NULL;
    gsize size = 0;
    if (!g_file_get_contents(path, &contents, &size, NULL) ||
        size < sizeof(Elf64_Ehdr)) {
        g_free(contents);
        return fail(error, error_size, "cannot read ELF dynamic symbols");
    }
    const unsigned char *file = (const void *)contents;
    const Elf64_Ehdr *header = (const void *)file;
    if (header->e_shentsize != sizeof(Elf64_Shdr) ||
        header->e_shoff > size ||
        header->e_shnum > (size - header->e_shoff) / sizeof(Elf64_Shdr)) {
        g_free(contents);
        return fail(error, error_size, "invalid ELF section table");
    }
    const Elf64_Shdr *sections = (const void *)(file + header->e_shoff);
    const char *section_names = NULL;
    size_t section_names_size = 0;
    if (header->e_shstrndx < header->e_shnum) {
        const Elf64_Shdr *names = &sections[header->e_shstrndx];
        if (names->sh_offset <= size && names->sh_size <= size - names->sh_offset) {
            section_names = (const void *)(file + names->sh_offset);
            section_names_size = names->sh_size;
        }
    }
    GHashTable *parallel_pcs = g_hash_table_new_full(
        g_int64_hash, g_int64_equal, g_free, NULL);
    for (size_t i = 0; i < program->tb_count; i++) {
        if (program->tbs[i].semantic_flags !=
            (CFG_TB_CODE64 | CFG_TB_PARALLEL)) {
            continue;
        }
        uint64_t *key = g_new(uint64_t, 1);
        *key = program->tbs[i].start;
        g_hash_table_add(parallel_pcs, key);
    }
    for (uint16_t section = 0; section < header->e_shnum; section++) {
        const Elf64_Shdr *symbols = &sections[section];
        if (symbols->sh_type != SHT_DYNSYM ||
            symbols->sh_entsize != sizeof(Elf64_Sym) ||
            symbols->sh_offset > size ||
            symbols->sh_size > size - symbols->sh_offset) {
            continue;
        }
        size_t count = symbols->sh_size / sizeof(Elf64_Sym);
        const Elf64_Sym *entries = (const void *)(file + symbols->sh_offset);
        if (count > (SIZE_MAX / sizeof(*program->tbs)) - program->tb_count) {
            g_hash_table_destroy(parallel_pcs);
            g_free(contents);
            return fail(error, error_size,
                        "too many dynamic function entries");
        }
        CfgTb *next = realloc(program->tbs,
                              (program->tb_count + count) * sizeof(*next));
        if (!next && count) {
            g_hash_table_destroy(parallel_pcs);
            g_free(contents);
            return fail(error, error_size,
                        "out of memory adding dynamic function entry");
        }
        program->tbs = next;
        for (size_t i = 0; i < count; i++) {
            unsigned int type = ELF64_ST_TYPE(entries[i].st_info);
            uint64_t pc = entries[i].st_value;
            if ((type != STT_FUNC && type != STT_GNU_IFUNC) || !pc ||
                !cfg_program_address_is_executable(program, pc)) {
                continue;
            }
            if (g_hash_table_contains(parallel_pcs, &pc)) {
                continue;
            }
            program->tbs[program->tb_count++] = (CfgTb) {
                .start = pc,
                .end = pc + 1,
                .terminator_pc = pc,
                .selected = true,
                .semantic_flags = CFG_TB_CODE64 | CFG_TB_PARALLEL |
                                  CFG_TB_BOUNDED,
                .terminator = CFG_TB_FALLTHROUGH,
            };
            uint64_t *key = g_new(uint64_t, 1);
            *key = pc;
            g_hash_table_add(parallel_pcs, key);
        }
    }

    /* A classic x86-64 PLT entry has a second entry point at byte 6.  The
     * dynamic linker returns there on the first call, so it is a real runtime
     * TB even though no direct branch in the ELF points to it.  Strict AOT can
     * be the first user of a symbol itself; do not require a profiling run to
     * have happened to exercise every lazy binding. */
    if (header->e_machine == EM_X86_64 && section_names) {
        size_t candidate_count = 0;
        for (uint16_t section = 0; section < header->e_shnum; section++) {
            const Elf64_Shdr *plt = &sections[section];
            if (plt->sh_name >= section_names_size ||
                strcmp(section_names + plt->sh_name, ".plt") ||
                plt->sh_offset > size || plt->sh_size > size - plt->sh_offset) {
                continue;
            }
            size_t entry_size = plt->sh_entsize ? plt->sh_entsize : 16;
            if (entry_size >= 16) candidate_count += plt->sh_size / entry_size;
        }
        if (candidate_count > (SIZE_MAX / sizeof(*program->tbs)) -
                              program->tb_count) {
            g_hash_table_destroy(parallel_pcs);
            g_free(contents);
            return fail(error, error_size, "too many PLT runtime entries");
        }
        CfgTb *next = realloc(
            program->tbs,
            (program->tb_count + candidate_count) * sizeof(*next));
        if (!next && candidate_count) {
            g_hash_table_destroy(parallel_pcs);
            g_free(contents);
            return fail(error, error_size,
                        "out of memory adding PLT runtime entries");
        }
        program->tbs = next;
        for (uint16_t section = 0; section < header->e_shnum; section++) {
            const Elf64_Shdr *plt = &sections[section];
            if (plt->sh_name >= section_names_size ||
                strcmp(section_names + plt->sh_name, ".plt") ||
                plt->sh_offset > size || plt->sh_size > size - plt->sh_offset) {
                continue;
            }
            size_t entry_size = plt->sh_entsize ? plt->sh_entsize : 16;
            if (entry_size < 16) continue;
            const uint8_t *bytes = file + plt->sh_offset;
            for (size_t offset = 0; offset + 16 <= plt->sh_size;
                 offset += entry_size) {
                /* push imm32; jmp rel32 is the lazy-binding continuation. */
                if (bytes[offset + 6] != 0x68 || bytes[offset + 11] != 0xe9) {
                    continue;
                }
                uint64_t pc = plt->sh_addr + offset + 6;
                if (g_hash_table_contains(parallel_pcs, &pc)) continue;
                program->tbs[program->tb_count++] = (CfgTb) {
                    .start = pc,
                    .end = pc + 1,
                    .terminator_pc = pc,
                    .selected = true,
                    .semantic_flags = CFG_TB_CODE64 | CFG_TB_PARALLEL |
                                      CFG_TB_BOUNDED,
                    .terminator = CFG_TB_FALLTHROUGH,
                };
                uint64_t *key = g_new(uint64_t, 1);
                *key = pc;
                g_hash_table_add(parallel_pcs, key);
            }
        }
    }
    g_hash_table_destroy(parallel_pcs);
    g_free(contents);
    return 0;
}

typedef struct PcSet {
    uint64_t *keys;
    unsigned char *used;
    size_t mask;
} PcSet;

static size_t pc_set_slot(uint64_t pc, size_t mask)
{
    pc ^= pc >> 30;
    pc *= UINT64_C(0xbf58476d1ce4e5b9);
    pc ^= pc >> 27;
    pc *= UINT64_C(0x94d049bb133111eb);
    pc ^= pc >> 31;
    return pc & mask;
}

static int pc_set_init(PcSet *set, size_t expected)
{
    size_t capacity = 1;
    while (capacity < expected * 2) {
        capacity <<= 1;
    }
    set->keys = malloc(capacity * sizeof(*set->keys));
    set->used = calloc(capacity, 1);
    set->mask = capacity - 1;
    return set->keys && set->used ? 0 : -1;
}

static void pc_set_destroy(PcSet *set)
{
    free(set->used);
    free(set->keys);
}

static int pc_set_contains(const PcSet *set, uint64_t pc)
{
    size_t slot = pc_set_slot(pc, set->mask);
    while (set->used[slot]) {
        if (set->keys[slot] == pc) {
            return 1;
        }
        slot = (slot + 1) & set->mask;
    }
    return 0;
}

static void pc_set_add(PcSet *set, uint64_t pc)
{
    size_t slot = pc_set_slot(pc, set->mask);
    while (set->used[slot] && set->keys[slot] != pc) {
        slot = (slot + 1) & set->mask;
    }
    set->keys[slot] = pc;
    set->used[slot] = 1;
}

static size_t first_containing_function(const CfgProgram *program,
                                        const uint64_t *prefix_end,
                                        uint64_t address)
{
    size_t low = 0;
    size_t high = program->function_count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (program->functions[middle].start <= address) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    size_t limit = low;
    low = 0;
    high = limit;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (prefix_end[middle] <= address) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low < limit ? low : SIZE_MAX;
}

static int expand_bounded_parallel_functions(CfgProgram *program,
                                             size_t template_count,
                                             char *error, size_t error_size)
{
    if (template_count > (SIZE_MAX / sizeof(*program->tbs)) -
                         program->tb_count ||
        program->tb_count > SIZE_MAX - template_count) {
        return fail(error, error_size,
                    "too many bounded parallel CFG blocks");
    }
    PcSet parallel_pcs = {0};
    if (pc_set_init(&parallel_pcs, program->tb_count + template_count)) {
        pc_set_destroy(&parallel_pcs);
        return fail(error, error_size,
                    "out of memory indexing parallel CFG blocks");
    }
    uint64_t *function_prefix_end = malloc(
        program->function_count * sizeof(*function_prefix_end));
    if (!function_prefix_end && program->function_count) {
        pc_set_destroy(&parallel_pcs);
        return fail(error, error_size,
                    "out of memory indexing CFG functions");
    }
    uint64_t maximum_end = 0;
    for (size_t i = 0; i < program->function_count; i++) {
        const CfgProgramFunction *fn = &program->functions[i];
        uint64_t end = !fn->tb_count ? 0 :
            (fn->size > UINT64_MAX - fn->start ?
             UINT64_MAX : fn->start + fn->size);
        if (end > maximum_end) {
            maximum_end = end;
        }
        function_prefix_end[i] = maximum_end;
    }
    for (size_t i = 0; i < program->tb_count; i++) {
        if (!program->tbs[i].selected ||
            !(program->tbs[i].semantic_flags & CFG_TB_PARALLEL)) continue;
        pc_set_add(&parallel_pcs, program->tbs[i].start);
    }
    CfgTb *expanded = realloc(
        program->tbs,
        (program->tb_count + template_count) * sizeof(*expanded));
    if (!expanded && template_count) {
        free(function_prefix_end);
        pc_set_destroy(&parallel_pcs);
        return fail(error, error_size,
                    "out of memory adding bounded parallel CFG blocks");
    }
    program->tbs = expanded;
    size_t bounded_entry_count = program->tb_count;
    for (size_t i = 0; i < bounded_entry_count; i++) {
        CfgTb *entry = &program->tbs[i];
        if (!entry->selected ||
            !(entry->semantic_flags & CFG_TB_PARALLEL)) continue;
        size_t f = first_containing_function(
            program, function_prefix_end, entry->start);
        if (f != SIZE_MAX) {
            const CfgProgramFunction *fn = &program->functions[f];
            /* Preserve the real CFG block at the function entry instead of
             * a one-byte observed or synthetic placeholder. */
            CfgTb real_entry = program->tbs[fn->first_tb];
            real_entry.selected = true;
            real_entry.observed = entry->observed;
            real_entry.semantic_flags = CFG_TB_CODE64 | CFG_TB_PARALLEL |
                                        CFG_TB_BOUNDED;
            real_entry.first_edge = 0;
            real_entry.edge_count = 0;
            if (entry->start == fn->start) {
                *entry = real_entry;
            } else if (!pc_set_contains(&parallel_pcs, real_entry.start)) {
                real_entry.observed = false;
                program->tbs[program->tb_count++] = real_entry;
                pc_set_add(&parallel_pcs, real_entry.start);
            }
            for (size_t j = 0; j < fn->tb_count; j++) {
                const CfgTb *source = &program->tbs[fn->first_tb + j];
                if (pc_set_contains(&parallel_pcs, source->start)) {
                    continue;
                }
                CfgTb added = *source;
                added.selected = true;
                added.observed = false;
                added.semantic_flags = CFG_TB_CODE64 | CFG_TB_PARALLEL |
                                       CFG_TB_BOUNDED;
                added.first_edge = 0;
                added.edge_count = 0;
                program->tbs[program->tb_count++] = added;
                pc_set_add(&parallel_pcs, source->start);
            }
        }
    }
    free(function_prefix_end);
    pc_set_destroy(&parallel_pcs);
    return 0;
}

static int select_all_parallel_cfg(CfgProgram *program, size_t template_count,
                                   char *error, size_t error_size)
{
    if (template_count > (SIZE_MAX / sizeof(*program->tbs)) -
                         program->tb_count) {
        return fail(error, error_size, "too many parallel CFG blocks");
    }
    PcSet parallel_pcs = {0};
    if (pc_set_init(&parallel_pcs, program->tb_count + template_count)) {
        return fail(error, error_size,
                    "out of memory indexing parallel CFG blocks");
    }
    for (size_t i = 0; i < program->tb_count; i++) {
        if (program->tbs[i].selected &&
            (program->tbs[i].semantic_flags & CFG_TB_PARALLEL)) {
            pc_set_add(&parallel_pcs, program->tbs[i].start);
        }
    }
    CfgTb *expanded = realloc(
        program->tbs,
        (program->tb_count + template_count) * sizeof(*expanded));
    if (!expanded && template_count) {
        pc_set_destroy(&parallel_pcs);
        return fail(error, error_size,
                    "out of memory adding parallel CFG blocks");
    }
    program->tbs = expanded;
    for (size_t i = 0; i < template_count; i++) {
        if (pc_set_contains(&parallel_pcs, program->tbs[i].start)) {
            continue;
        }
        CfgTb added = program->tbs[i];
        added.selected = true;
        added.observed = false;
        added.semantic_flags = CFG_TB_CODE64 | CFG_TB_PARALLEL |
                               CFG_TB_BOUNDED;
        added.first_edge = 0;
        added.edge_count = 0;
        program->tbs[program->tb_count++] = added;
        pc_set_add(&parallel_pcs, added.start);
    }
    pc_set_destroy(&parallel_pcs);
    return 0;
}

/*
 * Follow direct exits even when the destination has no function symbol.
 * Bounded translation does not discover these successors on our behalf.
 */
static int close_parallel_targets(const char *path, CfgProgram *program,
                                 char *error, size_t error_size)
{
    ElfFile elf;
    elf_load(path, &elf);
    GHashTable *seen = g_hash_table_new_full(g_int64_hash, g_int64_equal,
                                            g_free, NULL);
    GArray *pending = g_array_new(FALSE, FALSE, sizeof(uint64_t));
    GArray *added = g_array_new(FALSE, FALSE, sizeof(CfgTb));
    int result = 0;

    for (size_t i = 0; i < program->tb_count; i++) {
        const CfgTb *tb = &program->tbs[i];
        if (tb->selected && (tb->semantic_flags & CFG_TB_PARALLEL) &&
            !g_hash_table_contains(seen, &tb->start)) {
            uint64_t *key = g_new(uint64_t, 1);
            *key = tb->start;
            g_hash_table_add(seen, key);
            g_array_append_val(pending, *key);
        }
    }
    for (guint i = 0; i < pending->len; i++) {
        uint64_t pc = g_array_index(pending, uint64_t, i);
        unsigned section;
        uint64_t offset;
        if (!elf_find_exec_section_for_range(&elf, pc, 1, &section) ||
            !elf_function_file_offset(&elf, pc, 1, section, &offset)) {
            continue;
        }
        const Elf64_Shdr *sh = &elf.sh[section];
        size_t available = sh->sh_size - (pc - sh->sh_addr);
        if (offset > elf.size || available > elf.size - offset) {
            continue;
        }
        size_t cursor = 0;
        Insn insn = {0};
        for (unsigned count = 0; count < 254 && cursor < available; count++) {
            insn = cfg_decode_insn(elf.data + offset, available, pc, cursor);
            if (!insn.len || insn.len > available - cursor) {
                break;
            }
            cursor += insn.len;
            if (insn.kind != INSN_NORMAL) {
                break;
            }
        }
        if (!cursor) {
            continue;
        }
        uint64_t targets[2];
        unsigned count = 0;
        if (insn.has_target) {
            targets[count++] = insn.target;
        }
        if (insn.kind == INSN_NORMAL || insn.kind == INSN_CALL ||
            insn.kind == INSN_ICALL || insn.kind == INSN_JCC ||
            insn.kind == INSN_SYSCALL) {
            targets[count++] = pc + cursor;
        }
        for (unsigned j = 0; j < count; j++) {
            uint64_t target = targets[j];
            if (!cfg_program_address_is_executable(program, target) ||
                g_hash_table_contains(seen, &target)) {
                continue;
            }
            if (pending->len >= 1048576) {
                result = fail(error, error_size, "direct TB closure too large");
                goto out;
            }
            uint64_t *key = g_new(uint64_t, 1);
            *key = target;
            g_hash_table_add(seen, key);
            g_array_append_val(pending, target);
            CfgTb tb = {
                .start = target,
                .end = target + 1,
                .terminator_pc = target,
                .selected = true,
                .semantic_flags = CFG_TB_CODE64 | CFG_TB_PARALLEL |
                                  CFG_TB_BOUNDED,
                .terminator = CFG_TB_FALLTHROUGH,
            };
            g_array_append_val(added, tb);
        }
    }
    if (added->len) {
        CfgTb *next = realloc(program->tbs,
                              (program->tb_count + added->len) * sizeof(*next));
        if (!next) {
            result = fail(error, error_size,
                          "out of memory closing TB targets");
            goto out;
        }
        program->tbs = next;
        memcpy(next + program->tb_count, added->data,
               added->len * sizeof(*next));
        program->tb_count += added->len;
    }
out:
    g_array_free(added, TRUE);
    g_array_free(pending, TRUE);
    g_hash_table_destroy(seen);
    elf_free(&elf);
    return result;
}

int latc_tbset_apply(const char *path, const char *source_path,
                     CfgProgram *program,
                     bool ignore_outside_exec, size_t *matched,
                     size_t *unmatched, size_t *ignored, uint8_t digest[32],
                     char *error, size_t error_size)
{
    uint64_t load_base;
    LatTbKeySet set;
    if (source_identity(source_path, digest, &load_base,
                        error, error_size) ||
        lat_tb_key_set_read_file(path, digest, &set, error, error_size)) {
        return -1;
    }
    size_t hit = 0, miss = 0, skip = 0;
    bool has_parallel = false;
    size_t template_count = program->tb_count;
    if (set.count > (SIZE_MAX / sizeof(*program->tbs)) -
                    program->tb_count) {
        lat_tb_key_set_destroy(&set);
        return fail(error, error_size, "too many TB key entries");
    }
    CfgTb *reserved = realloc(
        program->tbs,
        (program->tb_count + set.count) * sizeof(*reserved));
    if (!reserved && set.count) {
        lat_tb_key_set_destroy(&set);
        return fail(error, error_size, "out of memory adding TB key entries");
    }
    program->tbs = reserved;
    TbTemplateIndex *templates = malloc(template_count * sizeof(*templates));
    if (!templates && template_count) {
        lat_tb_key_set_destroy(&set);
        return fail(error, error_size, "out of memory indexing CFG blocks");
    }
    bool templates_sorted = true;
    for (size_t i = 0; i < template_count; i++) {
        templates[i] = (TbTemplateIndex) {
            .pc = program->tbs[i].start,
            .flags = program->tbs[i].semantic_flags,
            .index = i,
        };
        if (i && templates[i - 1].pc > templates[i].pc) {
            templates_sorted = false;
        }
    }
    if (!templates_sorted) {
        sort_template_indices(templates, template_count);
    }
    for (size_t record = 0; record < set.count; record++) {
        const LatTbKey *key = &set.keys[record];
        has_parallel |= !!(key->flags & CFG_TB_PARALLEL);
        if (key->guest_rva > UINT64_MAX - load_base) {
            free(templates);
            lat_tb_key_set_destroy(&set);
            return fail(error, error_size, "TB key address overflows");
        }
        uint64_t pc = key->guest_rva + load_base;
        size_t template_index;
        size_t exact_index;
        template_lookup(templates, template_count, pc, key->flags,
                        &template_index, &exact_index);
        bool found = exact_index != SIZE_MAX;
        /* The on-disk key set is already unique.  Only pre-existing CFG
         * templates can match; entries appended by this loop cannot. */
        if (found) {
            program->tbs[exact_index].selected = true;
            program->tbs[exact_index].observed = true;
        }
        if (found) {
            hit++;
        } else if (cfg_program_address_is_executable(program, pc)) {
                CfgTb added = template_index != SIZE_MAX ?
                    program->tbs[template_index] : (CfgTb) {
                    .start = pc,
                    .end = pc + 1,
                    .terminator_pc = pc,
                    .terminator = CFG_TB_FALLTHROUGH,
                };
            added.selected = true;
            added.observed = true;
            added.semantic_flags = key->flags;
            added.first_edge = 0;
            added.edge_count = 0;
            program->tbs[program->tb_count++] = added;
            miss++;
        } else if (ignore_outside_exec) {
            skip++;
        } else {
            if (error && error_size) {
                snprintf(error, error_size,
                         "%s: key %zu address 0x%" PRIx64
                         " is outside executable ELF sections",
                         path, record, pc);
            }
            free(templates);
            lat_tb_key_set_destroy(&set);
            return -1;
        }
    }
    if (has_parallel && add_parallel_dynamic_entries(
            source_path, program, error, error_size)) {
        free(templates);
        lat_tb_key_set_destroy(&set);
        return -1;
    }
    bool full_parallel_cfg = template_count <= 49152 ||
        (template_count <= 81920 &&
         program->resolved_jump_table_targets >= 1024);
    if (has_parallel && full_parallel_cfg && select_all_parallel_cfg(
            program, template_count, error, error_size)) {
        free(templates);
        lat_tb_key_set_destroy(&set);
        return -1;
    }
    GQueue reachable = G_QUEUE_INIT;
    for (size_t i = 0; i < template_count; i++) {
        if (program->tbs[i].selected) {
            g_queue_push_tail(&reachable, GSIZE_TO_POINTER(i + 1));
        }
    }
    while (!g_queue_is_empty(&reachable)) {
        size_t index = GPOINTER_TO_SIZE(g_queue_pop_head(&reachable)) - 1;
        const CfgTb *tb = &program->tbs[index];
        for (size_t edge_index = tb->first_edge;
             edge_index < tb->first_edge + tb->edge_count; edge_index++) {
            const CfgProgramEdge *edge = &program->edges[edge_index];
            if (edge->resolution != CFG_EDGE_STATIC) {
                continue;
            }
            size_t target_index;
            size_t exact_index;
            template_lookup(templates, template_count, edge->to,
                            program->tbs[index].semantic_flags,
                            &target_index, &exact_index);
            if (target_index == SIZE_MAX ||
                program->tbs[target_index].selected) {
                continue;
            }
            program->tbs[target_index].selected = true;
            program->tbs[target_index].semantic_flags |= CFG_TB_BOUNDED;
            g_queue_push_tail(&reachable,
                              GSIZE_TO_POINTER(target_index + 1));
        }
    }
    if (has_parallel && expand_bounded_parallel_functions(
            program, template_count, error, error_size)) {
        free(templates);
        lat_tb_key_set_destroy(&set);
        return -1;
    }
    if (has_parallel && close_parallel_targets(source_path, program,
                                                error, error_size)) {
        free(templates);
        lat_tb_key_set_destroy(&set);
        return -1;
    }
    for (size_t i = 0; i < program->tb_count; i++) {
        if (program->tbs[i].selected) {
            program->tbs[i].semantic_flags |= CFG_TB_BOUNDED;
        }
    }
    free(templates);
    lat_tb_key_set_destroy(&set);
    if (matched) *matched = hit;
    if (unmatched) *unmatched = miss;
    if (ignored) *ignored = skip;
    return 0;
}

int latc_tbset_write_static(const char *path, const char *source_path,
                            const CfgProgram *program,
                            size_t *written, char *error, size_t error_size)
{
    uint64_t load_base;
    LatTbKeySet set = {0};
    if (!path || !source_path || !program ||
        source_identity(source_path, set.source_sha256, &load_base,
                        error, error_size)) {
        return -1;
    }
    if (program->tb_count > LAT_AOT_V2_TBSET_RECORD_LIMIT) {
        return fail(error, error_size, "too many static CFG blocks");
    }
    set.count = program->tb_count;
    set.keys = set.count ? calloc(set.count, sizeof(*set.keys)) : NULL;
    if (set.count && !set.keys) {
        return fail(error, error_size,
                    "out of memory creating static TB key set");
    }
    size_t output = 0;
    for (size_t i = 0; i < program->tb_count; i++) {
        if (program->tbs[i].start < load_base) {
            lat_tb_key_set_destroy(&set);
            return fail(error, error_size,
                        "static CFG block precedes preferred guest base");
        }
        uint64_t rva = program->tbs[i].start - load_base;
        set.keys[output++] = (LatTbKey) {
            .guest_rva = rva,
            .flags = CFG_TB_CODE64 | CFG_TB_PARALLEL,
        };
    }
    if (lat_tb_key_set_sort_unique(&set, error, error_size)) {
        lat_tb_key_set_destroy(&set);
        return -1;
    }
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW,
                  0600);
    if (fd < 0) {
        lat_tb_key_set_destroy(&set);
        if (error && error_size) {
            snprintf(error, error_size, "cannot open static TB key set: %s",
                     strerror(errno));
        }
        return -1;
    }
    int result = lat_tb_key_set_write_fd(fd, &set, error, error_size);
    if (!result && fsync(fd)) {
        result = fail(error, error_size,
                      "cannot synchronize static TB key set");
    }
    if (close(fd) && !result) {
        result = fail(error, error_size, "cannot close static TB key set");
    }
    if (!result && written) {
        *written = set.count;
    }
    lat_tb_key_set_destroy(&set);
    return result;
}
