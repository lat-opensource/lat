/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef LATX_CLEANUP_H
#define LATX_CLEANUP_H

#include "elfloader.h"

void AddCleanup(void *function);
void AddCleanup1Arg(void *function, void *argument, elfheader_t *head);
void CallCleanup(elfheader_t *head);
void CallAllCleanup(void);
void cleanup_fork_start(void);
void cleanup_fork_end(void);

#endif
