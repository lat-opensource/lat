/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
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
int flock_set(int fd, int type, bool wait);
uint64_t aot_file_rmgroup(char *aotFile);
int file_lock(char *file_name, int *fd, int type, bool wait);
int send_file_message(char *file_d, char *message);
#endif

