/*
 * Steam pressure-vessel compatibility helpers.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LINUX_USER_PRESSURE_VESSEL_H
#define LINUX_USER_PRESSURE_VESSEL_H

#include "qemu/envlist.h"

#if defined(CONFIG_LATX) && defined(TARGET_X86_64)
char **latx_pressure_vessel_prepare(const char *program, char **target_argv,
                                    envlist_t *envlist);
#endif

#endif
