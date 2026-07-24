/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file file_ctx.h
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT header
 */
#ifndef AOT_FILE_CTX_H
#define AOT_FILE_CTX_H
#include "aot.h"
int aot_file_ctx(uint64_t maxSize, uint64_t leftMinSize);
int aot_file_get_tmp_path(const char *aot_file, char *tmp_path,
                          size_t tmp_path_size);
int aot_file_get_lock_path(const char *aot_file, char *lock_path,
                           size_t lock_path_size);
int aot_file_complete_write(FILE *file, const char *tmp_path);
/*
 * Negative: rename failed. Zero: rename and directory sync succeeded.
 * Positive errno: rename succeeded, but directory durability is uncertain.
 */
int aot_file_publish(const char *tmp_path, const char *aot_file);
int aot_file_remove_legacy_fragments(const char *aot_file);
int aot_file_unlink_if_same(const char *aot_file, int file_fd,
                            const char *lock_path);
int flock_set(int fd, int type, bool wait);
uint64_t aot_file_rmgroup(char *aotFile);
int file_lock(const char *file_name, int *fd, int type, bool wait);
int send_file_message(char *file_d, char *message);
#endif
