/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef _TYPES_H_
#define _TYPES_H_

#include <inttypes.h>
#include "qemu/osdep.h"

typedef char int8;
typedef short int16;
typedef int int32;
#if __WORDSIZE == 64
typedef long int64;
#else
typedef long long int64;
#endif

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
#if __WORDSIZE == 64
typedef unsigned long uint64;
#else
typedef unsigned long long uint64;
#endif
typedef unsigned long ADDR;
#define PRIADDR "lx"

#ifndef N64
#define N64
#endif

#if defined N64
#define X86_MEMORY_LIMIT (0xc0000000)
#elif defined N32 || defined O32
#define X86_MEMORY_LIMIT (0x30000000)
#else
#error Must define N32, O32, or N64 in Makefile
#endif

#ifndef TARGET_X86_64
typedef int32 longx;
typedef uint32 ulongx;
typedef uint32 ADDRX;
#define PRIADDRX PRIx32
#define PRILONGX PRIx32
#else
typedef int64 longx;
typedef uint64 ulongx;
typedef uint64 ADDRX;
#define PRIADDRX PRIx64
#define PRILONGX PRIx64
#endif

#endif

