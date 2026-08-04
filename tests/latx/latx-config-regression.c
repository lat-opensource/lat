/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"
#include <glib.h>

#include "latx-string-utils.h"

static void test_empty_config_value(void)
{
    char line[] = "LATX_CLOSE_PARALLEL=\n";
    char *name = NULL;
    char *value = NULL;

    g_assert_true(latx_option_line_init(line, &name, &value));
    g_assert_cmpstr(name, ==, "LATX_CLOSE_PARALLEL");
    g_assert_cmpstr(value, ==, "");
}

static void test_trim_empty_and_whitespace(void)
{
    char empty[] = "";
    char whitespace[] = " \t \n";

    g_assert_cmpstr(latx_trim(empty), ==, "");
    g_assert_cmpstr(latx_trim(whitespace), ==, "");
}

static void test_long_filename_is_terminated(void)
{
    char filename[512];
    char buffer[8];

    filename[0] = '/';
    filename[1] = 't';
    filename[2] = 'm';
    filename[3] = 'p';
    filename[4] = '/';
    memset(filename + 5, 'b', sizeof(filename) - 6);
    filename[sizeof(filename) - 1] = '\0';

    memset(buffer, 'x', sizeof(buffer));
    latx_extract_filename(filename, buffer, sizeof(buffer));

    g_assert_cmpint(buffer[sizeof(buffer) - 1], ==, '\0');
    g_assert_cmpstr(buffer, ==, "bbbbbbb");
}

static void test_single_byte_filename_buffer(void)
{
    char buffer[1] = { 'x' };

    latx_extract_filename("program", buffer, sizeof(buffer));

    g_assert_cmpint(buffer[0], ==, '\0');
}

static void test_filename_path_and_extension(void)
{
    char buffer[32];

    latx_extract_filename("/usr/bin/program.exe", buffer, sizeof(buffer));

    g_assert_cmpstr(buffer, ==, "program");
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/latx/config/empty-value", test_empty_config_value);
    g_test_add_func("/latx/config/trim-empty-and-whitespace",
                    test_trim_empty_and_whitespace);
    g_test_add_func("/latx/config/long-filename-terminated",
                    test_long_filename_is_terminated);
    g_test_add_func("/latx/config/single-byte-filename-buffer",
                    test_single_byte_filename_buffer);
    g_test_add_func("/latx/config/filename-path-and-extension",
                    test_filename_path_and_extension);

    return g_test_run();
}
