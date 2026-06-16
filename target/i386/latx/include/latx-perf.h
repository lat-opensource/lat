/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file latx-perf.h
 * @author Hanlu Li <heuleehanlu@gmail.com>
 * @brief a timer module to aid in performance analysis.
 */
#ifndef _LATX_PERF_H_
#define _LATX_PERF_H_
#include <stdio.h>
#include <time.h>

#define GENERATE_ENUM(ENUM)         ENUM,
#define GENERATE_STRING(STRING)     #STRING,
#define FOREACH_TIMER(TIMER)    \
        TIMER(TIMER_PROCESS)    \
        TIMER(TIMER_MMAP)       \
        TIMER(TIMER_MMAP_LOCK)  \
        TIMER(TIMER_PAGE_FLAGS) \
        TIMER(TIMER_TS)         \


typedef enum {
    FOREACH_TIMER(GENERATE_ENUM)
    TIMER_COUNT
} TimerCategory;

typedef struct {
    double total_time[TIMER_COUNT];
    struct timespec last_start[TIMER_COUNT];
    unsigned long call_times[TIMER_COUNT];
} Timer;

void latx_timer_start(TimerCategory category);
void latx_timer_stop(TimerCategory category);
void latx_print_timer(TimerCategory category);
void latx_print_all_timers(void);
#endif
