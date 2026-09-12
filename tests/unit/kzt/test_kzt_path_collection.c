/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"
#include "debug.h"
#include "pathcoll.h"

char *box_strdup(const char *s)
{
    return strdup(s);
}

static void test_repeated_runpath(void)
{
    path_collection_t paths = { 0 };

    ParseList("/system:/fallback", &paths, 1);
    for (int i = 0; i < 2000; i++) {
        /* The same normalized RUNPATH is processed on each library reload. */
        PrependList(&paths, "/application:/system", 1);
    }
    g_test_message("paths after 2000 repeated RUNPATH registrations: %d",
                   paths.size);
    g_assert_cmpint(paths.size, ==, 3);
    g_assert_cmpstr(paths.paths[0], ==, "/application/");
    g_assert_cmpstr(paths.paths[1], ==, "/system/");
    g_assert_cmpstr(paths.paths[2], ==, "/fallback/");
    FreeCollection(&paths);
    g_assert_cmpint(paths.size, ==, 0);
    g_assert_cmpint(paths.cap, ==, 0);
    g_assert(paths.paths == NULL);
}

static void test_precedence_and_pointer_ownership(void)
{
    path_collection_t paths = { 0 };
    char *system;
    char *fallback;
    char *application;
    int capacity;

    ParseList("/system:/fallback", &paths, 1);
    system = paths.paths[0];
    fallback = paths.paths[1];
    PrependList(&paths, "/application:/system/", 1);
    g_assert_cmpint(paths.size, ==, 3);
    application = paths.paths[0];
    capacity = paths.cap;
    g_assert(paths.paths[1] == system);
    g_assert(paths.paths[2] == fallback);

    /* Existing paths move to the new priority rather than being ignored. */
    PrependList(&paths, "/fallback:/application", 1);
    g_assert_cmpint(paths.size, ==, 3);
    g_assert_cmpint(paths.cap, ==, capacity);
    g_assert(paths.paths[0] == fallback);
    g_assert(paths.paths[1] == application);
    g_assert(paths.paths[2] == system);

    PrependPath(paths.paths[0], &paths, 1);
    PrependList(&paths, "::", 1);
    PrependList(&paths, NULL, 1);
    g_assert_cmpint(paths.size, ==, 3);
    g_assert(paths.paths[0] == fallback);
    FreeCollection(&paths);
}

static void test_literal_names_and_existing_duplicates(void)
{
    path_collection_t paths = { 0 };
    char *first;
    char *second;

    /* With folder=false, a trailing slash remains part of a literal name. */
    ParseList("name:name/:other", &paths, 0);
    first = paths.paths[0];
    second = paths.paths[1];
    PrependPath("name/", &paths, 0);
    g_assert_cmpint(paths.size, ==, 3);
    g_assert(paths.paths[0] == second);
    g_assert(paths.paths[1] == first);
    FreeCollection(&paths);

    /* Do not invalidate strings already owned by the initial collection. */
    ParseList("/same:/same:/other", &paths, 1);
    first = paths.paths[0];
    second = paths.paths[1];
    PrependPath("/same", &paths, 1);
    g_assert_cmpint(paths.size, ==, 3);
    g_assert(paths.paths[0] == first);
    g_assert(paths.paths[1] == second);
    FreeCollection(&paths);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/kzt-path/repeated-runpath", test_repeated_runpath);
    g_test_add_func("/kzt-path/precedence-and-ownership",
                    test_precedence_and_pointer_ownership);
    g_test_add_func("/kzt-path/literal-names-and-existing-duplicates",
                    test_literal_names_and_existing_duplicates);
    return g_test_run();
}
