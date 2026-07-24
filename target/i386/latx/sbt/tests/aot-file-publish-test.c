#include "qemu/osdep.h"

#include "file_ctx.h"
#include "aot-file-publish-test.h"

static bool fail_rename;
static bool fail_fsync;
char aot_file_path_buffer[PATH_MAX];
char aot_file_lock_buffer[PATH_MAX];
char *aot_file_path = aot_file_path_buffer;
char *aot_file_lock = aot_file_lock_buffer;
int qemu_loglevel;

int qemu_log(const char *fmt, ...)
{
    return 0;
}

void pstrcpy(char *buf, int buf_size, const char *str)
{
    g_strlcpy(buf, str, buf_size);
}

int aot_get_file_name(char *aot_file, char *buf, int index)
{
    int len;

    if (index == 0) {
        pstrcpy(buf, PATH_MAX, aot_file);
        return access(buf, F_OK) < 0 ? -1 : 0;
    }
    len = snprintf(buf, PATH_MAX, "%s%c", aot_file, 'A' + index - 1);
    if (len < 0 || len >= PATH_MAX || access(buf, F_OK) < 0) {
        return -1;
    }
    return 0;
}

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
    g_autofree char *legacy_a_path = NULL;
    g_autofree char *legacy_b_path = NULL;
    g_autofree char *cache_dir = NULL;
    g_autofree char *cache_parent = NULL;
    g_autofree char *stale_lock_path = NULL;
    g_autofree char *stale_tmp_path = NULL;
    g_autofree char *stale_contents = NULL;
    g_autofree char *old_home = NULL;
    char lock_path[PATH_MAX];
    FILE *file;
    int old_fd;
    int current_fd;

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

    file = fopen(tmp_path, "w");
    g_assert(file != NULL);
    g_assert(fputs("new", file) >= 0);
    g_assert(aot_file_complete_write(file, tmp_path) == 0);
    fail_fsync = true;
    g_assert(aot_file_publish(tmp_path, aot_path) == EIO);
    fail_fsync = false;
    assert_file_contents(aot_path, "new");
    g_assert(!g_file_test(tmp_path, G_FILE_TEST_EXISTS));

    legacy_a_path = g_strconcat(aot_path, "A", NULL);
    legacy_b_path = g_strconcat(aot_path, "B", NULL);
    g_assert(g_file_set_contents(legacy_a_path, "legacy-a", -1, NULL));
    g_assert(g_file_set_contents(legacy_b_path, "legacy-b", -1, NULL));
    g_assert(aot_file_remove_legacy_fragments(aot_path) == 0);
    g_assert(!g_file_test(legacy_a_path, G_FILE_TEST_EXISTS));
    g_assert(!g_file_test(legacy_b_path, G_FILE_TEST_EXISTS));
    assert_file_contents(aot_path, "new");

    g_assert(aot_file_get_lock_path(aot_path, lock_path,
                                    sizeof(lock_path)) == 0);
    old_fd = open(aot_path, O_RDONLY);
    g_assert(old_fd >= 0);
    g_assert(g_file_set_contents(tmp_path, "new", -1, NULL));
    g_assert(rename(tmp_path, aot_path) == 0);
    g_assert(aot_file_unlink_if_same(aot_path, old_fd, lock_path) == 0);
    assert_file_contents(aot_path, "new");
    g_assert(close(old_fd) == 0);

    current_fd = open(aot_path, O_RDONLY);
    g_assert(current_fd >= 0);
    g_assert(aot_file_unlink_if_same(aot_path, current_fd, lock_path) == 0);
    g_assert(!g_file_test(aot_path, G_FILE_TEST_EXISTS));
    g_assert(close(current_fd) == 0);
    g_assert(unlink(lock_path) == 0);

    old_home = g_strdup(g_getenv("HOME"));
    cache_dir = g_build_filename(test_dir, ".cache", "latx", NULL);
    g_assert(g_mkdir_with_parents(cache_dir, 0700) == 0);
    stale_tmp_path = g_build_filename(cache_dir, "stale.aot2.tmp", NULL);
    stale_contents = g_malloc0(2 * MiB);
    g_assert(g_file_set_contents(stale_tmp_path, stale_contents, 2 * MiB,
                                 NULL));
    g_assert(g_setenv("HOME", test_dir, true));
    g_assert(aot_file_ctx(2, 1) == 0);
    g_assert(!g_file_test(stale_tmp_path, G_FILE_TEST_EXISTS));
    stale_lock_path = g_build_filename(cache_dir, "stale.aot2.lock", NULL);
    g_assert(unlink(stale_lock_path) == 0);
    if (old_home) {
        g_assert(g_setenv("HOME", old_home, true));
    } else {
        g_unsetenv("HOME");
    }
    g_assert(g_rmdir(cache_dir) == 0);
    cache_parent = g_build_filename(test_dir, ".cache", NULL);
    g_assert(g_rmdir(cache_parent) == 0);
    g_assert(rmdir(test_dir) == 0);
    return 0;
}
