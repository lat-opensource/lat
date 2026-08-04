/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_RUNTIME_H
#define LATX_RUNTIME_H

typedef enum LatxRuntimeSource {
    LATX_RUNTIME_SOURCE_DEFAULT,
    LATX_RUNTIME_SOURCE_SYSTEM_CONFIG,
    LATX_RUNTIME_SOURCE_USER_CONFIG,
    LATX_RUNTIME_SOURCE_ENVIRONMENT,
    LATX_RUNTIME_SOURCE_COMMAND_LINE,
} LatxRuntimeSource;

void latx_runtime_reset(void);
void latx_runtime_option_source_set(LatxRuntimeSource source);
void latx_runtime_prefix_selected(void);
const char *latx_runtime_prefix_source_name(void);
const char *latx_runtime_guest_abi(void);

#endif
