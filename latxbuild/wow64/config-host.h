/* SPDX-License-Identifier: GPL-2.0-or-later */
#if !defined(_WIN64) || !defined(__loongarch64) || defined(__loong64ec__) || defined(_M_LOONG64EC)
#error The WoW64 PE build requires an ordinary Loong64 MinGW compiler
#endif

#pragma once
#define CONFIG_WIN32 1
#define CONFIG_ATOMIC64 1
#define CONFIG_INT128 1
#define CONFIG_LATX 1
#define CONFIG_LATX_CAPSTONE_GIT 1
#define CONFIG_LATX_CAPSTONE_OP_ACCESS 1
#define CONFIG_CAPSTONE_DIET 1
#define CONFIG_LATX_LSFPU 1
#define CONFIG_LATX_O0 1
#define CONFIG_LATX_WOW64_PE 1
#define CONFIG_LOONGARCH_NEW_WORLD 1
#define CONFIG_HOST_DSOSUF ".dll"
#define CONFIG_PREFIX ""
#define CONFIG_BINDIR ""
#define USE_FSINCOS_HELPER 1
#define CONFIG_STATIC_ASSERT 1
#define CONFIG_TCG 1
