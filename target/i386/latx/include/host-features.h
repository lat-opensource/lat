/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_HOST_FEATURES_H
#define LATX_HOST_FEATURES_H

#include <stdbool.h>
#include <stdio.h>

typedef struct LatxHostFeatures {
    bool known;
    bool lbt;
    bool lsx;
    bool lasx;
} LatxHostFeatures;

/* Intersect Features/flags records; match complete tokens only. */
LatxHostFeatures latx_cpuinfo_features(FILE *stream);

#endif
