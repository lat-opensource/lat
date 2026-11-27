/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"
#include "host-features.h"

LatxHostFeatures latx_cpuinfo_features(FILE *stream)
{
    LatxHostFeatures result = { 0 };
    char *line = NULL;
    size_t capacity = 0;

    if (!stream) {
        return result;
    }
    while (getline(&line, &capacity, stream) >= 0) {
        char *colon = strchr(line, ':');
        char *token, *save;
        LatxHostFeatures record = { .known = true };

        if (!colon) {
            continue;
        }
        *colon = 0;
        if (g_ascii_strcasecmp(g_strstrip(line), "Features") &&
            g_ascii_strcasecmp(g_strstrip(line), "flags")) {
            continue;
        }
        for (token = strtok_r(colon + 1, " \t\r\n", &save); token;
             token = strtok_r(NULL, " \t\r\n", &save)) {
            record.lbt |= !strcmp(token, "lbt_x86");
            record.lsx |= !strcmp(token, "lsx");
            record.lasx |= !strcmp(token, "lasx");
        }
        if (!result.known) {
            result = record;
        } else {
            result.lbt &= record.lbt;
            result.lsx &= record.lsx;
            result.lasx &= record.lasx;
        }
    }
    free(line);
    /* A truncated read must not enable instructions based on a partial list. */
    if (ferror(stream)) {
        return (LatxHostFeatures) { .known = true };
    }
    return result;
}
