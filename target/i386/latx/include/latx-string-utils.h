/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef _LATX_STRING_UTILS_H_
#define _LATX_STRING_UTILS_H_

#include <stddef.h>

char *latx_trim(char *s);
int latx_option_line_init(char *line, char **name, char **value);
int latx_user_config_home_is_safe(const char *home, uid_t real_uid,
                                  uid_t effective_uid, gid_t real_gid,
                                  gid_t effective_gid);
int latx_user_config_path(char *buffer, size_t buffer_size,
                          const char *home, const char *filename);
/* filename and buffer must be non-NULL, and buffer_size must be positive. */
void latx_extract_filename(const char *filename, char *buffer,
                           size_t buffer_size);

#endif /* _LATX_STRING_UTILS_H_ */
