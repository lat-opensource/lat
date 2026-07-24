#include "qemu/osdep.h"

#include "file_ctx.h"
#include "aot-file-publish-test.h"

static bool fail_rename;
static bool fail_fsync;

int __wrap_fsync(int fd)
{
    if (fail_fsync) {
        errno = EIO;
        return -1;
    }
    return __real_fsync(fd);
}

int __wrap_rename(const char *old_path, const char *new_path)
{
    if (fail_rename) {
        errno = EIO;
        return -1;
    }
    return __real_rename(old_path, new_path);
}

static void assert_file_contents(const char *path, const char *expected)
{
    g_autofree char *contents = NULL;

    g_assert(g_file_get_contents(path, &contents, NULL, NULL));
    g_assert(strcmp(contents, expected) == 0);
}

int main(void)
{
    char tmp_path[PATH_MAX];
    g_autofree char *test_dir = NULL;
    g_autofree char *aot_path = NULL;
    FILE *file;

    g_assert(aot_file_get_tmp_path("/tmp/cache.aot2", tmp_path,
                                   sizeof(tmp_path)) == 0);
    g_assert(strcmp(tmp_path, "/tmp/cache.aot2.tmp") == 0);

    test_dir = g_dir_make_tmp("latx-aot-publish-XXXXXX", NULL);
    g_assert(test_dir != NULL);
    aot_path = g_build_filename(test_dir, "cache.aot2", NULL);
    g_assert(aot_file_get_tmp_path(aot_path, tmp_path,
                                   sizeof(tmp_path)) == 0);

    g_assert(g_file_set_contents(aot_path, "old", -1, NULL));
    file = fopen(tmp_path, "w");
    g_assert(file != NULL);
    g_assert(fputs("new", file) >= 0);
    g_assert(aot_file_complete_write(file, tmp_path) == 0);
    g_assert(aot_file_publish(tmp_path, aot_path) == 0);
    assert_file_contents(aot_path, "new");
    g_assert(!g_file_test(tmp_path, G_FILE_TEST_EXISTS));

    g_assert(g_file_set_contents(aot_path, "old", -1, NULL));
    file = fopen(tmp_path, "w");
    g_assert(file != NULL);
    g_assert(fputs("new", file) >= 0);
    g_assert(aot_file_complete_write(file, tmp_path) == 0);
    fail_rename = true;
    g_assert(aot_file_publish(tmp_path, aot_path) == -EIO);
    fail_rename = false;
    assert_file_contents(aot_path, "old");
    g_assert(!g_file_test(tmp_path, G_FILE_TEST_EXISTS));

    file = fopen(tmp_path, "w");
    g_assert(file != NULL);
    g_assert(fputs("new", file) >= 0);
    fail_fsync = true;
    g_assert(aot_file_complete_write(file, tmp_path) == -EIO);
    fail_fsync = false;
    assert_file_contents(aot_path, "old");
    g_assert(!g_file_test(tmp_path, G_FILE_TEST_EXISTS));

    g_assert(unlink(aot_path) == 0);
    g_assert(rmdir(test_dir) == 0);
    return 0;
}
