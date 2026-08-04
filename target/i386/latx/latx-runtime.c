/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"
#include "latx-runtime.h"

static LatxRuntimeSource option_source = LATX_RUNTIME_SOURCE_DEFAULT;
static LatxRuntimeSource prefix_source = LATX_RUNTIME_SOURCE_DEFAULT;

void latx_runtime_reset(void)
{
    option_source = LATX_RUNTIME_SOURCE_DEFAULT;
    prefix_source = LATX_RUNTIME_SOURCE_DEFAULT;
}

void latx_runtime_option_source_set(LatxRuntimeSource source)
{
    option_source = source;
}

void latx_runtime_prefix_selected(void)
{
    prefix_source = option_source;
}

const char *latx_runtime_prefix_source_name(void)
{
    switch (prefix_source) {
    case LATX_RUNTIME_SOURCE_SYSTEM_CONFIG:
        return "system_config";
    case LATX_RUNTIME_SOURCE_USER_CONFIG:
        return "user_config";
    case LATX_RUNTIME_SOURCE_ENVIRONMENT:
        return "environment";
    case LATX_RUNTIME_SOURCE_COMMAND_LINE:
        return "command_line";
    case LATX_RUNTIME_SOURCE_DEFAULT:
        return "default";
    }

    g_assert_not_reached();
}

const char *latx_runtime_guest_abi(void)
{
#ifdef TARGET_X86_64
    return "x86_64";
#else
    return "i386";
#endif
}
