/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LATX_WRAPPEDFONT_INTEROP_H
#define LATX_WRAPPEDFONT_INTEROP_H

#include <stdbool.h>

bool latx_freetype_face_reference(void *face);
void latx_freetype_face_release(void *face);
bool latx_freetype_face_borrow(void *face);
void latx_freetype_face_return(void *face);

void *latx_fontconfig_pattern_ft_face(void *pattern);

#endif
