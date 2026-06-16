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

#include "box64context.h"
#include "debug.h"

char* box_strdup(const char* s) {
#if 1
    if (!s) {
        char* ret = box_calloc(1, 1);
        ret[0] = '\0';
        return ret;
    }
    #endif
    char* ret = box_calloc(1, strlen(s)+1);
    memcpy(ret, s, strlen(s));
    return ret;
}
