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
    char line[] = "LATX_CLOSE_PARALLEL=   \n";
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

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/latx/config/empty-value", test_empty_config_value);
    g_test_add_func("/latx/config/trim-empty-and-whitespace",
                    test_trim_empty_and_whitespace);

    return g_test_run();
}
