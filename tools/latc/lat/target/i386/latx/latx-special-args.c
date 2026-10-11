/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "latx-config.h"
#include "latx-options.h"
#include "latx-string-utils.h"

typedef struct {
    const char* filename;
    void (*func)(void);
} FileFunMap;

static void lative_func(void)
{
    option_lative = 1;
    return;
}

FileFunMap filefunmap[] = {
   {"program", lative_func},                        // lative
};

void latx_handle_args(char *filename)
{
    int i;
    char buffer[256];
    latx_extract_filename(filename, buffer, sizeof(buffer));

    for (i = 0; i < sizeof(filefunmap) / sizeof(FileFunMap); i++) {
        if (strcmp(buffer, filefunmap[i].filename) == 0) {
            filefunmap[i].func();
            return;
        }
    }
}
