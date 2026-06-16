/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "wrappedlibs.h"

#include "wrapper.h"
#include "bridge.h"
#include "library_private.h"

#ifdef ANDROID
	const char* libxcbxkbName = "libxcb-xkb.so";
#else
	const char* libxcbxkbName = "libxcb-xkb.so.1";
#endif

#define LIBNAME libxcbxkb

#include "wrappedlib_init.h"

