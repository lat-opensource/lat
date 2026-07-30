/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"
#include "latx-string-utils.h"

char *latx_trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

int latx_option_line_init(char *line, char **name, char **value)
{
    char *eq;

    while (isspace((unsigned char)*line)) {
        line++;
    }
    if (*line == '#' || *line == '\0') {
        return false;
    }

    eq = strchr(line, '=');
    if (eq == NULL) {
        return false;
    }

    *eq = '\0';
    *name = latx_trim(line);
    *value = latx_trim(eq + 1);
    return true;
}

void latx_extract_filename(const char *filename, char *buffer,
                           size_t buffer_size)
{
    const char *filename_only;
    const char *last_slash;
    char *extension;

    last_slash = strrchr(filename, '/');
    if (last_slash != NULL) {
        filename_only = last_slash + 1;
    } else {
        filename_only = filename;
    }

    strncpy(buffer, filename_only, buffer_size - 1);

    extension = strchr(buffer, '.');
    if (extension != NULL) {
        *extension = '\0';
    }
}
