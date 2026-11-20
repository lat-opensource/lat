/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"
#include "host-features.h"

static LatxHostFeatures parse(const char *text)
{
    FILE *stream = tmpfile();
    LatxHostFeatures result;

    g_assert_nonnull(stream);
    g_assert_cmpint(fputs(text, stream), >=, 0);
    rewind(stream);
    result = latx_cpuinfo_features(stream);
    fclose(stream);
    return result;
}

static void test_cpuinfo(void)
{
    LatxHostFeatures f = parse("processor: 0\nFeatures : cpucfg lsx lasx lbt_x86\n");
    g_assert_true(f.known && f.lbt && f.lsx && f.lasx);
    f = parse("flags: lsx lbt_arm lbt_x86_extra fake_lasx\n");
    g_assert_true(f.known && f.lsx);
    g_assert_false(f.lbt || f.lasx);
    f = parse("Features: lsx lasx lbt_x86\nFeatures: lsx\n");
    g_assert_true(f.known && f.lsx);
    g_assert_false(f.lbt || f.lasx);
    f = parse("Features: lsx\nFeatures: lsx lasx lbt_x86\n");
    g_assert_false(f.lbt || f.lasx);
    f = parse("Features:\n");
    g_assert_true(f.known);
    g_assert_false(f.lbt || f.lsx || f.lasx);
    f = parse("model name: lbt_x86 lasx lsx\n");
    g_assert_false(f.known);
    f = latx_cpuinfo_features(NULL);
    g_assert_false(f.known);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/latx/host-features/cpuinfo", test_cpuinfo);
    return g_test_run();
}
