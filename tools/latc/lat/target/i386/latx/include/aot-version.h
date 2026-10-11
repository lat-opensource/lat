/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef LATX_AOT_VERSION_H
#define LATX_AOT_VERSION_H

#include "latx-version.h"
#include "latx-aot-build-id.h"

#ifdef CONFIG_LATX_DEBUG
#define AOT_BUILD_FLAVOR "debug"
#else
#define AOT_BUILD_FLAVOR "release"
#endif

#define AOT_VERSION \
    "Version: " LATX_VERSION "-" AOT_BUILD_FLAVOR "-" LATX_AOT_BUILD_ID

#endif
