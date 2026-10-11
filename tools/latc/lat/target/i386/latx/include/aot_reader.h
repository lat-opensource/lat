/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_AOT_READER_H
#define LATX_AOT_READER_H

#include "aot.h"

int aot_get_tb_num(char *lib_name, char *aot_file_name, CPUState *cpu);

#endif
