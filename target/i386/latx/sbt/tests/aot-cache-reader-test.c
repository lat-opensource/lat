#include "qemu/osdep.h"

#include <fcntl.h>

#include "aot.h"
#include "aot_reader.h"
#include "aot_lib.h"
#include "file_ctx.h"
#include "latx-options.h"

char aot_file_path_buffer[PATH_MAX];
char aot_file_lock_buffer[PATH_MAX];
char *aot_file_path = aot_file_path_buffer;
char *aot_file_lock = aot_file_lock_buffer;
int qemu_loglevel;
int option_imm_reg;
int option_imm_rip;
int option_imm_complex;
static bool fail_fdopen;
static int tracked_fd;
static int sentinel_fd;
static unsigned int fdopen_count;
static unsigned int fclose_count;

FILE *__real_fdopen(int fd, const char *mode);
int __real_fclose(FILE *stream);
FILE *__wrap_fdopen(int fd, const char *mode);
int __wrap_fclose(FILE *stream);

FILE *__wrap_fdopen(int fd, const char *mode)
{
    fdopen_count++;
    tracked_fd = fd;
    if (fail_fdopen) {
        errno = EMFILE;
        return NULL;
    }
    return __real_fdopen(fd, mode);
}

int __wrap_fclose(FILE *stream)
{
    int ret;

    ret = __real_fclose(stream);
    fclose_count++;
    sentinel_fd = open("/dev/null", O_RDONLY);
    g_assert_cmpint(sentinel_fd, ==, tracked_fd);
    return ret;
}

int qemu_log(const char *fmt G_GNUC_UNUSED, ...)
{
    return 0;
}

void print_stack_trace(void)
{
}

void pstrcpy(char *buf, int buf_size, const char *str)
{
    g_strlcpy(buf, str, buf_size);
}

static void reset_stream_counts(void)
{
    fail_fdopen = false;
    tracked_fd = -1;
    sentinel_fd = -1;
    fdopen_count = 0;
    fclose_count = 0;
}

static void assert_tracked_fd_is_closed(void)
{
    g_assert_cmpint(tracked_fd, >=, 0);
    errno = 0;
    g_assert_cmpint(fcntl(tracked_fd, F_GETFD), ==, -1);
    g_assert_cmpint(errno, ==, EBADF);
}

static void assert_stream_closed(void)
{
    g_assert_cmpuint(fdopen_count, ==, 1);
    g_assert_cmpuint(fclose_count, ==, 1);
    g_assert_cmpint(sentinel_fd, ==, tracked_fd);
    g_assert_cmpint(fcntl(sentinel_fd, F_GETFD), !=, -1);
    g_assert_cmpint(close(sentinel_fd), ==, 0);
    sentinel_fd = -1;
}

static void write_cache(const char *name, bool has_header, bool has_footer,
                        char *path)
{
    g_autofree char *dir = NULL;
    g_autofree uint8_t *contents = NULL;
    size_t footer_size = strlen(AOT_VERSION);
    size_t size = (has_header ? sizeof(aot_header) : 0) +
                  (has_footer ? footer_size : 0);

    g_assert(get_aot_path(name, path, PATH_MAX) == 0);
    dir = g_path_get_dirname(path);
    g_assert(g_mkdir_with_parents(dir, 0700) == 0);

    contents = g_malloc0(size);
    if (has_header) {
        ((aot_header *)contents)->aot_file_type = CACHE_AOT_FILE;
        ((aot_header *)contents)->imm_rip =
            !!(option_imm_reg && option_imm_rip);
        ((aot_header *)contents)->imm_complex =
            option_imm_reg ? option_imm_complex : 0;
    }
    if (has_footer) {
        memcpy(contents + size - footer_size, AOT_VERSION, footer_size);
    }
    g_assert(g_file_set_contents(path, (char *)contents, size, NULL));
}

static void remove_cache(const char *name)
{
    char path[PATH_MAX];
    char lock_path[PATH_MAX];

    g_assert(get_aot_path(name, path, sizeof(path)) == 0);
    if (g_file_test(path, G_FILE_TEST_EXISTS)) {
        g_assert(g_remove(path) == 0);
    }
    g_assert(aot_file_get_lock_path(path, lock_path, sizeof(lock_path)) == 0);
    if (g_file_test(lock_path, G_FILE_TEST_EXISTS)) {
        g_assert(g_remove(lock_path) == 0);
    }
}

int main(void)
{
    g_autofree char *cache_dir = NULL;
    g_autofree char *cache_parent = NULL;
    g_autofree char *test_dir = NULL;
    g_autofree char *old_home = NULL;
    char lib_name[] = "test-library";
    char bad_footer_name[] = "bad-footer";
    char truncated_name[] = "truncated";
    char complete_name[] = "complete";
    char mode_mismatch_name[] = "mode-mismatch";
    char fdopen_failure_name[] = "fdopen-failure";
    char cache_path[PATH_MAX];
    void *buffer;
    lib_info *lib;

    test_dir = g_dir_make_tmp("latx-aot-reader-XXXXXX", NULL);
    g_assert(test_dir != NULL);
    old_home = g_strdup(g_getenv("HOME"));
    g_assert(g_setenv("HOME", test_dir, true));

    lib_tree_init();
    for (int i = 0; i < 16; i++) {
        reset_stream_counts();
        write_cache(bad_footer_name, true, false, cache_path);
        g_assert_cmpint(aot_get_tb_num(lib_name, bad_footer_name, NULL),
                        ==, 0);
        g_assert(!g_file_test(cache_path, G_FILE_TEST_EXISTS));
        assert_stream_closed();

        reset_stream_counts();
        buffer = NULL;
        write_cache(truncated_name, false, true, cache_path);
        g_assert(aot_load(lib_name, truncated_name, &buffer) == NULL);
        g_assert(buffer == NULL);
        g_assert(!g_file_test(cache_path, G_FILE_TEST_EXISTS));
        assert_stream_closed();
    }

    reset_stream_counts();
    buffer = NULL;
    write_cache(complete_name, true, true, cache_path);
    lib = aot_load(lib_name, complete_name, &buffer);
    g_assert(lib != NULL);
    g_assert(buffer != NULL);
    assert_stream_closed();
    g_assert(lib_tree_remove(complete_name));

    option_imm_reg = 0;
    option_imm_rip = 0;
    write_cache(mode_mismatch_name, true, true, cache_path);
    option_imm_reg = 1;
    option_imm_rip = 1;
    g_assert(aot_get_tb_num(lib_name, mode_mismatch_name, NULL) == 0);
    g_assert(!g_file_test(cache_path, G_FILE_TEST_EXISTS));

    buffer = NULL;
    write_cache(mode_mismatch_name, true, true, cache_path);
    option_imm_rip = 0;
    g_assert(aot_load(lib_name, mode_mismatch_name, &buffer) == NULL);
    g_assert(buffer == NULL);
    g_assert(!g_file_test(cache_path, G_FILE_TEST_EXISTS));
    option_imm_reg = 0;

    option_imm_reg = 1;
    option_imm_complex = LATX_IMM_COMPLEX_BASE_DISP;
    write_cache(mode_mismatch_name, true, true, cache_path);
    option_imm_complex = LATX_IMM_COMPLEX_BASE_INDEX_DISP;
    g_assert(aot_get_tb_num(lib_name, mode_mismatch_name, NULL) == 0);
    g_assert(!g_file_test(cache_path, G_FILE_TEST_EXISTS));

    buffer = NULL;
    write_cache(mode_mismatch_name, true, true, cache_path);
    option_imm_complex = 0;
    g_assert(aot_load(lib_name, mode_mismatch_name, &buffer) == NULL);
    g_assert(buffer == NULL);
    g_assert(!g_file_test(cache_path, G_FILE_TEST_EXISTS));
    option_imm_reg = 0;

    reset_stream_counts();
    fail_fdopen = true;
    buffer = NULL;
    write_cache(fdopen_failure_name, true, true, cache_path);
    g_assert(aot_load(lib_name, fdopen_failure_name, &buffer) == NULL);
    g_assert(buffer == NULL);
    g_assert_cmpuint(fdopen_count, ==, 1);
    g_assert_cmpuint(fclose_count, ==, 0);
    assert_tracked_fd_is_closed();

    remove_cache(bad_footer_name);
    remove_cache(truncated_name);
    remove_cache(complete_name);
    remove_cache(mode_mismatch_name);
    remove_cache(fdopen_failure_name);
    cache_dir = g_build_filename(test_dir, ".cache", "latx", NULL);
    g_assert(g_rmdir(cache_dir) == 0);
    cache_parent = g_build_filename(test_dir, ".cache", NULL);
    g_assert(g_rmdir(cache_parent) == 0);

    if (old_home) {
        g_assert(g_setenv("HOME", old_home, true));
    } else {
        g_unsetenv("HOME");
    }
    g_assert(g_rmdir(test_dir) == 0);
    return 0;
}
