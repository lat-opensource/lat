/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef _PROFILE_H_
#define _PROFILE_H_

#include "common.h"
#include "env.h"
#include "reg-alloc.h"
#include "translate.h"

/**
 * @brief Add a per tb counter stub
 * @param area Record area in memory
 * @param inc Record add number (can be negative)
 */
void per_tb_count(void *area, int inc);

#ifdef CONFIG_LATX_PROFILER
#define PER_TB_COUNT(area, inc) \
    per_tb_count((area), (inc))
#else
#define PER_TB_COUNT(area, inc) ((void)0)
#endif

#endif
