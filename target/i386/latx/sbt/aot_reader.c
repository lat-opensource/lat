/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu-def.h"
#include "aot.h"
#include "aot_reader.h"
#include "file_ctx.h"
#include "latx-options.h"
#include "qemu.h"

#ifdef CONFIG_LATX_AOT

int aot_get_tb_num(char *lib_name, char *aot_file_name, CPUState *cpu)
{
    void *buffer = MAP_FAILED;
    FILE *pf = NULL;
    long file_size;
    size_t file_sz = 0;
    int aim_tb_num = 0;
    int fd;

    if (get_aot_path(aot_file_name, aot_file_path, PATH_MAX) < 0) {
        return 0;
    }
    if (access(aot_file_path, 0) < 0) {
        return 0;
    }
    fd = open(aot_file_path, O_RDONLY);
    if (fd < 0) {
        return 0;
    }
    pf = fdopen(fd, "r");
    if (!pf) {
        close(fd);
        return 0;
    }

    /* Get file size */
    if (fseek(pf, 0, SEEK_END) || (file_size = ftell(pf)) < 0) {
        goto out;
    }
    file_sz = file_size;
    if (file_sz < sizeof(aot_header)) {
        qemu_log_mask(LAT_LOG_AOT, "aot file is too short %s\n", lib_name);
        remove(aot_file_path);
        goto out;
    }

    /* Check that the AOT file was completely written. */
    if (!aot_file_has_footer(pf, AOT_VERSION)) {
        qemu_log_mask(LAT_LOG_AOT, "aot file is not complete %s\n", lib_name);
        remove(aot_file_path);
        goto out;
    }
    fseek(pf, 0, SEEK_SET);

    buffer = mmap(NULL, file_sz, PROT_READ, MAP_SHARED, fd, 0);
    if (buffer == MAP_FAILED) {
        qemu_log_mask(LAT_LOG_AOT, "aot file mmap error\n");
        goto out;
    }
    assert(buffer);
    aot_header *p_header = (aot_header *)buffer;
    struct stat statbuf;

    if (p_header->imm_rip != !!(option_imm_reg && option_imm_rip)) {
        qemu_log_mask(LAT_LOG_AOT,
                      "RIP immediate cache mode changed, remove aot %s\n",
                      aot_file_path);
        remove(aot_file_path);
        goto out;
    }
    if (p_header->imm_complex !=
        (option_imm_reg ? option_imm_complex : 0)) {
        qemu_log_mask(LAT_LOG_AOT,
                      "complex immediate cache mode changed, remove aot %s\n",
                      aot_file_path);
        remove(aot_file_path);
        goto out;
    }

    if ((p_header->aot_file_type & (ELF_AOT_FILE | PE_AOT_FILE))
            && (stat(lib_name, &statbuf)
            || p_header->lib_size != statbuf.st_size
            || p_header->last_modify_time.tv_sec != statbuf.st_mtim.tv_sec
            || p_header->last_modify_time.tv_nsec != statbuf.st_mtim.tv_nsec)) {
        qemu_log_mask(LAT_LOG_AOT,
                "need remove old aot file. %s lib_size %d %ld\n",
                aot_file_path, p_header->lib_size, statbuf.st_size);
        remove(aot_file_path);
        qemu_log_mask(LAT_LOG_AOT, "remove end\n");
        goto out;
    }

    if (cpu->tcg_cflags & CF_PARALLEL) {
        aim_tb_num = p_header->parallel_tb_num;
    } else {
        aim_tb_num = p_header->unparallel_tb_num;
    }

out:
    if (buffer != MAP_FAILED) {
        munmap(buffer, file_sz);
    }
    fclose(pf);
    return aim_tb_num;
}

static void remove_curr_aot_file(int fd)
{
    char lock_path[PATH_MAX];

    if (aot_file_get_lock_path(aot_file_path, lock_path,
                               sizeof(lock_path)) < 0) {
        return;
    }
    aot_file_unlink_if_same(aot_file_path, fd, lock_path);
}

lib_info *aot_load(char *lib_name, char *aot_file_name,
                   void **curr_aot_buffer)
{
    void *buffer = MAP_FAILED;
    struct stat statbuf;
    lib_info *curr_lib_info = NULL;
    size_t file_sz = 0;
    long file_size;
    int fd;
    FILE *pf = NULL;

    assert(lib_name);
    if (get_aot_path(aot_file_name, aot_file_path, PATH_MAX) < 0) {
        return NULL;
    }
    if (access(aot_file_path, 0) < 0) {
        return NULL;
    }

    fd = open(aot_file_path, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }
    pf = fdopen(fd, "r");
    if (!pf) {
        close(fd);
        return NULL;
    }

    if (fseek(pf, 0, SEEK_END) || (file_size = ftell(pf)) < 0) {
        goto out;
    }
    if ((size_t)file_size < sizeof(aot_header)) {
        qemu_log_mask(LAT_LOG_AOT, "aot file is too short %s\n", lib_name);
        remove_curr_aot_file(fd);
        goto out;
    }
    file_sz = file_size;

    if (!aot_file_has_footer(pf, AOT_VERSION)) {
        qemu_log_mask(LAT_LOG_AOT, "aot file is not complete %s\n", lib_name);
        remove_curr_aot_file(fd);
        goto out;
    }

    fseek(pf, 0, SEEK_SET);
    buffer = mmap(NULL, file_sz, PROT_READ, MAP_SHARED, fd, 0);
    if (buffer == MAP_FAILED) {
        qemu_log_mask(LAT_LOG_AOT, "aot file mmap error\n");
        goto out;
    }
    assert(buffer);
    aot_header *p_header = (aot_header *)buffer;

    if (p_header->imm_rip != !!(option_imm_reg && option_imm_rip)) {
        qemu_log_mask(LAT_LOG_AOT,
                      "RIP immediate cache mode changed, remove aot %s\n",
                      aot_file_path);
        remove_curr_aot_file(fd);
        goto out;
    }
    if (p_header->imm_complex !=
        (option_imm_reg ? option_imm_complex : 0)) {
        qemu_log_mask(LAT_LOG_AOT,
                      "complex immediate cache mode changed, remove aot %s\n",
                      aot_file_path);
        remove_curr_aot_file(fd);
        goto out;
    }

    if (p_header->aot_file_type & (ELF_AOT_FILE | PE_AOT_FILE)) {
        if (stat(lib_name, &statbuf)
                || p_header->lib_size != statbuf.st_size
                || p_header->last_modify_time.tv_sec != statbuf.st_mtim.tv_sec
                || p_header->last_modify_time.tv_nsec != statbuf.st_mtim.tv_nsec) {
            qemu_log_mask(LAT_LOG_AOT,
                    "need remove old aot file. %s lib_size %d %ld\n",
                    aot_file_path, p_header->lib_size, statbuf.st_size);
            remove_curr_aot_file(fd);
            goto out;
        }
    }

    *curr_aot_buffer = buffer;
    curr_lib_info = lib_tree_insert(aot_file_name, buffer, file_sz);

out:
    if (!curr_lib_info && buffer != MAP_FAILED) {
        munmap(buffer, file_sz);
    }
    fclose(pf);
    return curr_lib_info;
}

#endif
