/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_WRAPPEDFONT_VAARGS_H
#define LATX_WRAPPEDFONT_VAARGS_H

#include "x64-vaargs.h"

void *latx_fontconfig_pattern_build(LatxX64VaReader *reader,
                                    void *pattern);
void *latx_fontconfig_object_set_build(LatxX64VaReader *reader,
                                       const char *first);

#endif /* LATX_WRAPPEDFONT_VAARGS_H */
