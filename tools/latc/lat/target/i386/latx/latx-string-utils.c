/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"
#include "latx-string-utils.h"

char *latx_trim(char *s)
{
    char *end;

    while (isspace((unsigned char)*s)) {
        s++;
    }
    if (*s == '\0') {
        return s;
    }

    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        end--;
    }
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

int latx_user_config_home_is_safe(const char *home, uid_t real_uid,
                                  uid_t effective_uid, gid_t real_gid,
                                  gid_t effective_gid)
{
    return home != NULL && home[0] == '/' && real_uid == effective_uid &&
           real_gid == effective_gid;
}

int latx_user_config_path(char *buffer, size_t buffer_size,
                          const char *home, const char *filename)
{
    int length;

    if (buffer == NULL || buffer_size == 0) {
        return false;
    }
    buffer[0] = '\0';
    if (filename == NULL || filename[0] == '\0' ||
        !latx_user_config_home_is_safe(home, getuid(), geteuid(),
                                       getgid(), getegid())) {
        return false;
    }

    length = snprintf(buffer, buffer_size, "%s/.config/%s", home, filename);
    if (length < 0 || (size_t)length >= buffer_size) {
        buffer[0] = '\0';
        return false;
    }
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
    buffer[buffer_size - 1] = '\0';

    extension = strchr(buffer, '.');
    if (extension != NULL) {
        *extension = '\0';
    }
}
