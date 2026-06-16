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

#include "debug.h"
#include "wrapper.h"
#include "bridge.h"
#include "library_private.h"
#include "callback.h"
#include "librarian.h"
#include "box64context.h"

#ifdef ANDROID
	const char* libxcbsyncName = "libxcb-sync.so";
#else
	const char* libxcbsyncName = "libxcb-sync.so.1";
#endif

#define LIBNAME libxcbsync

#include "wrappedlib_init.h"
