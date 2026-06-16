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
	const char* libxcbutilName = "libxcb-util.so";
#else
	const char* libxcbutilName = "libxcb-util.so.1";
#endif

#define LIBNAME libxcbutil

#include "wrappedlib_init.h"
