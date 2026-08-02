/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"
#include <glib.h>

#include "latx-string-utils.h"
#include "latx-options.h"

static bool config_option_registered(const char *expected)
{
#define ENVFUN(NAME, handler) \
    if (g_str_equal(expected, #NAME)) { \
        return true; \
    }
    ENVS
#undef ENVFUN
    return false;
}

static void test_target_config_files(void)
{
#ifdef TARGET_X86_64
    g_assert_cmpstr(LATX_SYSTEM_CONFIG_FILE, ==,
                    "/etc/latx-x86_64.conf");
    g_assert_cmpstr(LATX_USER_CONFIG_FILE, ==, "latx-x86_64.conf");
#else
    g_assert_cmpstr(LATX_SYSTEM_CONFIG_FILE, ==, "/etc/latx-i386.conf");
    g_assert_cmpstr(LATX_USER_CONFIG_FILE, ==, "latx-i386.conf");
#endif
}

static void test_release_loader_prefix_config(void)
{
    g_assert_true(config_option_registered("LAT_LD_PREFIX"));
}

static void test_user_config_path(void)
{
    char expected[PATH_MAX];
    char path[PATH_MAX] = "must be cleared";
    char truncated[8] = "unclear";

    g_assert_true(latx_user_config_home_is_safe("/home/test", 1000, 1000,
                                                1000, 1000));
    g_assert_false(latx_user_config_home_is_safe("/home/test", 1000, 0,
                                                 1000, 1000));
    g_assert_false(latx_user_config_home_is_safe("/home/test", 1000, 1000,
                                                 1000, 0));
    g_assert_false(latx_user_config_home_is_safe("relative/home", 1000, 1000,
                                                 1000, 1000));

    g_assert_cmpint(snprintf(expected, sizeof(expected),
                             "/home/test/.config/%s",
                             LATX_USER_CONFIG_FILE), >, 0);
    g_assert_true(latx_user_config_path(path, sizeof(path), "/home/test",
                                       LATX_USER_CONFIG_FILE));
    g_assert_cmpstr(path, ==, expected);

    g_assert_false(latx_user_config_path(path, sizeof(path), NULL,
                                        LATX_USER_CONFIG_FILE));
    g_assert_cmpstr(path, ==, "");
    g_assert_false(latx_user_config_path(path, sizeof(path), "relative/home",
                                        LATX_USER_CONFIG_FILE));
    g_assert_cmpstr(path, ==, "");
    g_assert_false(latx_user_config_path(truncated, sizeof(truncated),
                                        "/home/test",
                                        LATX_USER_CONFIG_FILE));
    g_assert_cmpstr(truncated, ==, "");
    g_assert_false(latx_user_config_path(path, sizeof(path), "/home/test",
                                        NULL));
    g_assert_cmpstr(path, ==, "");
    g_assert_false(latx_user_config_path(NULL, 0, "/home/test",
                                        LATX_USER_CONFIG_FILE));
}

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
    g_test_add_func("/latx/config/target-config-files",
                    test_target_config_files);
    g_test_add_func("/latx/config/release-loader-prefix",
                    test_release_loader_prefix_config);
    g_test_add_func("/latx/config/user-config-path",
                    test_user_config_path);

    return g_test_run();
}
