/*
 * SPDX-FileCopyrightText: 2021-2026 LATX Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file latx-perf.c
 * @author Hanlu Li <heuleehanlu@gmail.com>
 * @brief a timer module to aid in performance analysis.
 */
#include <stdbool.h>
#include "latx-perf.h"

Timer latx_timer;

const char* timer_names[] = {
    FOREACH_TIMER(GENERATE_STRING)
};

/* All timers are enabled by default */
#if 0
bool timer_switch[TIMER_COUNT] = {
    [0 ... TIMER_COUNT-1] = true
};
#else
bool timer_switch[TIMER_COUNT] = {
    [0 ... TIMER_COUNT-1] = false,
    [TIMER_PROCESS] = true,
    [TIMER_MMAP_LOCK] = true,
    [TIMER_PAGE_FLAGS] = true,
};
#endif

void latx_timer_start(TimerCategory category)
{
    if (!timer_switch[category]) {
        return;
    }
    clock_gettime(CLOCK_MONOTONIC, &latx_timer.last_start[category]);
}

void latx_timer_stop(TimerCategory category)
{
    if (!timer_switch[category]) {
        return;
    }
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    double elapsed_time =
        (end_time.tv_sec - latx_timer.last_start[category].tv_sec) * 1e9;
    elapsed_time +=
        (end_time.tv_nsec - latx_timer.last_start[category].tv_nsec);
    elapsed_time *= 1e-9; // Convert nanoseconds to seconds

    latx_timer.total_time[category] += elapsed_time;
    latx_timer.call_times[category]++;
}

void latx_print_timer(TimerCategory category)
{
    fprintf(stderr, "%-15s - Total time: %.9f seconds, Calls: %lu\n",
        timer_names[category], latx_timer.total_time[category],
        latx_timer.call_times[category]);
}

void latx_print_all_timers(void)
{
    for (int i = 0; i < TIMER_COUNT; ++i) {
        if (timer_switch[i]) {
            latx_print_timer(i);
        }
    }
}
