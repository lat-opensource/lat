#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "module-inspect.h"

#include "elf-validate.h"
#include "lat-tb-key-set.h"

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int fail(char *error, size_t error_size, const char *format, ...)
{
    if (error && error_size) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(error, error_size, format, arguments);
        va_end(arguments);
    }
    return -1;
}

static const Elf64_Shdr *find_section(const unsigned char *file,
                                      const Elf64_Ehdr *header,
                                      const char *names, size_t names_size,
                                      const char *wanted)
{
    const Elf64_Shdr *sections = (const void *)(file + header->e_shoff);
    for (uint16_t i = 0; i < header->e_shnum; i++) {
        if (sections[i].sh_name >= names_size) {
            return NULL;
        }
        const char *name = names + sections[i].sh_name;
        if (!memchr(name, '\0', names_size - sections[i].sh_name)) {
            return NULL;
        }
        if (!strcmp(name, wanted)) {
            return &sections[i];
        }
    }
    return NULL;
}

int lat_aot_v2_module_inspect_file(const char *path,
                                   LatAotModuleInfoV2 *info,
                                   char *error, size_t error_size)
{
    if (!path || !info) {
        return fail(error, error_size, "invalid module inspection arguments");
    }
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        return fail(error, error_size, "cannot open AOT module: %s",
                    strerror(errno));
    }
    memset(info, 0, sizeof(*info));
    if (lat_aot_v2_elf_validate_fd(fd, NULL, &info->note,
                                   error, error_size)) {
        close(fd);
        return -1;
    }
    close(fd);

    gchar *contents = NULL;
    gsize size = 0;
    if (!g_file_get_contents(path, &contents, &size, NULL) ||
        size < sizeof(Elf64_Ehdr)) {
        g_free(contents);
        return fail(error, error_size, "cannot read AOT module");
    }
    const unsigned char *file = (const void *)contents;
    const Elf64_Ehdr *header = (const void *)file;
    if (header->e_shentsize != sizeof(Elf64_Shdr) || !header->e_shnum ||
        header->e_shstrndx >= header->e_shnum ||
        header->e_shoff > size ||
        header->e_shnum > (size - header->e_shoff) / sizeof(Elf64_Shdr)) {
        g_free(contents);
        return fail(error, error_size, "AOT section table is invalid");
    }
    const Elf64_Shdr *sections = (const void *)(file + header->e_shoff);
    const Elf64_Shdr *names_section = &sections[header->e_shstrndx];
    if (names_section->sh_offset > size ||
        names_section->sh_size > size - names_section->sh_offset) {
        g_free(contents);
        return fail(error, error_size, "AOT section names are invalid");
    }
    const char *names = (const void *)(file + names_section->sh_offset);
    const Elf64_Shdr *text = find_section(file, header, names,
        names_section->sh_size, ".text.lat.tu");
    const Elf64_Shdr *tbs = find_section(file, header, names,
        names_section->sh_size, ".rodata.lat.tb");
    const Elf64_Shdr *maps = find_section(file, header, names,
        names_section->sh_size, ".rodata.lat.map");
    const Elf64_Shdr *slots = find_section(file, header, names,
        names_section->sh_size, ".rodata.lat.guest");
    if (!text || !text->sh_size || !tbs || !tbs->sh_size ||
        tbs->sh_size % sizeof(LatAotTbV2) || !maps ||
        maps->sh_size % sizeof(LatAotPcMapV2) || !slots ||
        slots->sh_size % sizeof(LatAotGuestSlotV2) ||
        text->sh_offset > size || text->sh_size > size - text->sh_offset ||
        tbs->sh_offset > size || tbs->sh_size > size - tbs->sh_offset ||
        maps->sh_offset > size || maps->sh_size > size - maps->sh_offset ||
        slots->sh_offset > size || slots->sh_size > size - slots->sh_offset ||
        ((info->note.module_flags & LAT_AOT_MODULE_PRECISE_PC_MAP) &&
         !maps->sh_size)) {
        g_free(contents);
        return fail(error, error_size, "AOT module sections are incomplete");
    }
    info->text_size = text->sh_size;
    info->tb_count = tbs->sh_size / sizeof(LatAotTbV2);
    info->pc_map_count = maps->sh_size / sizeof(LatAotPcMapV2);
    info->guest_slot_records = slots->sh_size / sizeof(LatAotGuestSlotV2);
    g_free(contents);
    return 0;
}

int lat_aot_v2_module_validate_tbset_file(const char *path,
                                          const char *tbset_path,
                                          char *error,
                                          size_t error_size)
{
    LatAotModuleInfoV2 info;
    return lat_aot_v2_module_inspect_and_validate_tbset_file(
        path, tbset_path, &info, error, error_size);
}

static int module_missing_tbset_file(
    const char *path, const char *tbset_path, LatAotModuleInfoV2 *info,
    LatTbKeySet *missing, size_t *requested_count,
    char *error, size_t error_size)
{
    memset(missing, 0, sizeof(*missing));
    if (lat_aot_v2_module_inspect_file(path, info, error, error_size)) {
        return -1;
    }
    gchar *contents = NULL;
    gsize size = 0;
    if (!g_file_get_contents(path, &contents, &size, NULL)) {
        return fail(error, error_size, "cannot read AOT module");
    }
    const unsigned char *file = (const void *)contents;
    const Elf64_Ehdr *header = (const void *)file;
    const Elf64_Shdr *sections = (const void *)(file + header->e_shoff);
    const Elf64_Shdr *names_section = &sections[header->e_shstrndx];
    const char *names = (const void *)(file + names_section->sh_offset);
    const Elf64_Shdr *section = find_section(file, header, names,
        names_section->sh_size, ".rodata.lat.tb");
    if (!section || section->sh_offset > size ||
        section->sh_size > size - section->sh_offset ||
        section->sh_size % sizeof(LatAotTbV2)) {
        g_free(contents);
        return fail(error, error_size, "AOT TB section is invalid");
    }
    const LatAotTbV2 *tbs = (const void *)(file + section->sh_offset);
    size_t tb_count = section->sh_size / sizeof(*tbs);
    LatTbKeySet tbset;
    if (lat_tb_key_set_read_file(tbset_path, info->note.source_sha256,
                                 &tbset, error, error_size)) {
        g_free(contents);
        return -1;
    }
    if (requested_count) *requested_count = tbset.count;
    memcpy(missing->source_sha256, tbset.source_sha256,
           sizeof(missing->source_sha256));
    missing->sequence = tbset.sequence;
    missing->keys = g_try_new(LatTbKey, tbset.count);
    if (tbset.count && !missing->keys) {
        lat_tb_key_set_destroy(&tbset);
        g_free(contents);
        return fail(error, error_size,
                    "cannot allocate missing module TB set");
    }
    size_t module_record = 0;
    for (size_t record = 0; record < tbset.count; record++) {
        uint64_t rva = tbset.keys[record].guest_rva;
        uint32_t flags = tbset.keys[record].flags;
        while (module_record < tb_count &&
               (tbs[module_record].guest_rva < rva ||
                (tbs[module_record].guest_rva == rva &&
                 tbs[module_record].flags < flags))) {
            module_record++;
        }
        if (module_record == tb_count ||
            tbs[module_record].guest_rva != rva ||
            tbs[module_record].flags != flags) {
            missing->keys[missing->count++] = tbset.keys[record];
        }
    }
    lat_tb_key_set_destroy(&tbset);
    g_free(contents);
    return 0;
}

int lat_aot_v2_module_missing_tbset_file(
    const char *path, const char *tbset_path, LatAotModuleInfoV2 *info,
    LatTbKeySet *missing, char *error, size_t error_size)
{
    if (!path || !tbset_path || !info || !missing) {
        return fail(error, error_size,
                    "invalid missing module TB set arguments");
    }
    return module_missing_tbset_file(path, tbset_path, info, missing, NULL,
                                     error, error_size);
}

int lat_aot_v2_module_inspect_and_validate_tbset_file(
    const char *path, const char *tbset_path, LatAotModuleInfoV2 *info,
    char *error, size_t error_size)
{
    LatTbKeySet missing = {0};
    size_t requested = 0;
    if (module_missing_tbset_file(path, tbset_path, info, &missing,
                                  &requested, error, error_size)) {
        return -1;
    }
    int result = 0;
    if (missing.count) {
        result = fail(error, error_size,
                      "TB set is not fully covered: covered=%zu total=%zu "
                      "first_missing_rva=0x%" PRIx64 " flags=0x%x",
                      requested - missing.count, requested,
                      missing.keys[0].guest_rva, missing.keys[0].flags);
    }
    lat_tb_key_set_destroy(&missing);
    return result;
}
