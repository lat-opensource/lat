/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef LATX_WOW64_X86_STATE_H
#define LATX_WOW64_X86_STATE_H

#include "latx-wow64.h"
void lat_wow64_context_to_cpu(CPUX86State *env,
                             const LatWow64Context *context);
void lat_wow64_context_from_cpu(LatWow64Context *context, CPUX86State *env,
                               int trap, uint32_t float_status);

int lat_wow64_float_trap(CPUX86State *env, int trap);

#endif
