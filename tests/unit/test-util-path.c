#include "qemu/osdep.h"
#include <glib/gstdio.h>

#include "qemu/path.h"

static char *prefix;

static char *create_prefixed_file(const char *name)
{
    char *full = g_build_filename(prefix, name, NULL);
    char *parent = g_path_get_dirname(full);
    GError *error = NULL;

    g_assert_cmpint(g_mkdir_with_parents(parent, 0700), ==, 0);
    g_assert_true(g_file_set_contents(full, "", 0, &error));
    g_assert_no_error(error);
    g_free(parent);
    return full;
}

static void remove_tree(const char *name)
{
    GDir *directory = g_dir_open(name, 0, NULL);
    const char *entry;

    g_assert_nonnull(directory);
    while ((entry = g_dir_read_name(directory))) {
        char *child = g_build_filename(name, entry, NULL);

        if (g_file_test(child, G_FILE_TEST_IS_DIR)) {
            remove_tree(child);
        } else {
            g_assert_cmpint(g_remove(child), ==, 0);
        }
        g_free(child);
    }
    g_dir_close(directory);
    g_assert_cmpint(g_rmdir(name), ==, 0);
}

static void test_regular_path_uses_prefix(void)
{
    const char *names[] = {
        "/dev/null",
        "/home/guest",
        "/lib/guest.so",
        "/run/user/1000/bus",
        "/sys/devices/system/cpu/online",
        "/tmp/guest",
        "/var/lock/guest.lock",
        "/var/run/guest.sock",
    };
    size_t i;

    for (i = 0; i < ARRAY_SIZE(names); i++) {
        char *expected = g_build_filename(prefix, names[i], NULL);

        g_assert_cmpstr(path(names[i]), ==, expected);
        g_free(expected);
    }
}

static void test_proc_namespace_ignores_prefix(void)
{
    const char *names[] = {
        "/proc",
        "/proc/self/status",
    };
    size_t i;

    for (i = 0; i < ARRAY_SIZE(names); i++) {
        g_assert_cmpstr(path(names[i]), ==, names[i]);
    }
}

static void test_proc_namespace_boundaries(void)
{
    const char *similar_names[] = {
        "/procedure",
    };
    char *escaped = g_build_filename(prefix, "proc", "..", "lib",
                                     "guest.so", NULL);
    char *indirect_proc = g_build_filename(prefix, "lib", "..", "proc",
                                           "self", "status", NULL);
    char *indirect_run = g_build_filename(prefix, "var", "tmp", "..", "run",
                                          "guest.sock", NULL);
    char *var_tmp = g_build_filename(prefix, "var", "run", "..", "tmp",
                                     "guest", NULL);
    size_t i;

    g_assert_cmpstr(path("/proc/"), ==, "/proc/");
    g_assert_cmpstr(path("/proc///./self/status"), ==,
                    "/proc///./self/status");
    g_assert_cmpstr(path("/lib/../proc/self/status"), ==, indirect_proc);
    g_assert_cmpstr(path("/var/tmp/../run/guest.sock"), ==, indirect_run);
    g_assert_cmpstr(path("/proc/../lib/guest.so"), ==, escaped);
    g_assert_cmpstr(path("/var/run/../tmp/guest"), ==, var_tmp);

    for (i = 0; i < ARRAY_SIZE(similar_names); i++) {
        char *expected = g_build_filename(prefix, similar_names[i], NULL);

        g_assert_cmpstr(path(similar_names[i]), ==, expected);
        g_free(expected);
    }

    g_free(var_tmp);
    g_free(indirect_run);
    g_free(indirect_proc);
    g_free(escaped);
}

int main(int argc, char **argv)
{
    GError *error = NULL;
    const char *files[] = {
        "dev/null",
        "home/guest",
        "lib/guest.so",
        "proc/self/status",
        "procedure",
        "run/guest.sock",
        "run/user/1000/bus",
        "sys/devices/system/cpu/online",
        "tmp/guest",
        "var/lock/guest.lock",
        "var/run/guest.sock",
        "var/tmp/guest",
    };
    size_t i;
    int ret;

    g_test_init(&argc, &argv, NULL);

    prefix = g_dir_make_tmp("test-util-path-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(prefix);

    for (i = 0; i < ARRAY_SIZE(files); i++) {
        g_free(create_prefixed_file(files[i]));
    }
    init_paths(prefix);

    g_test_add_func("/util/path/regular-uses-prefix",
                    test_regular_path_uses_prefix);
    g_test_add_func("/util/path/proc-namespace-ignores-prefix",
                    test_proc_namespace_ignores_prefix);
    g_test_add_func("/util/path/proc-namespace-boundaries",
                    test_proc_namespace_boundaries);

    ret = g_test_run();

    remove_tree(prefix);
    g_free(prefix);

    return ret;
}
