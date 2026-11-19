/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef LATX_WOW64_X86_STATE_H
#define LATX_WOW64_X86_STATE_H

#include "latx-wow64.h"
#ifdef CONFIG_LATX_WOW64
#include "qemu.h"
#endif

/* While native x87 code is running, its FCSR is in the host register. */
#define LAT_WOW64_X87_FCSR_LIVE (1u << 31)

static inline bool lat_wow64_split_native_fcsr(void)
{
#ifdef CONFIG_LATX_WOW64_PE
    return !option_softfpu;
#elif defined(CONFIG_LATX_WOW64)
    return lat_wow64_provider_plugin_active && !option_softfpu;
#else
    return false;
#endif
}

void gen_wow64_x87_fcsr_enter(void);
void gen_wow64_x87_fcsr_exit(void);
void gen_wow64_x87_fcsr_restore(void);

void lat_wow64_context_to_cpu(CPUX86State *env,
                             const LatWow64Context *context);
void lat_wow64_context_from_cpu(LatWow64Context *context, CPUX86State *env,
                               int trap, uint32_t float_status);

int lat_wow64_float_trap(CPUX86State *env, int trap);

#endif
