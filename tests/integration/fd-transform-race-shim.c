#define _GNU_SOURCE

#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

typedef void *(*GReallocNFunc)(void *, size_t, size_t);
typedef int (*EventfdFunc)(unsigned int, int);

static GReallocNFunc real_g_realloc_n;
static EventfdFunc real_eventfd;
static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static void *fd_table;
static size_t fd_table_blocks;
static int injected;
static int enabled;
static const char *marker;
static __thread int expect_fd_table;

static void init_symbols(void)
{
    real_g_realloc_n = dlsym(RTLD_NEXT, "g_realloc_n");
    real_eventfd = dlsym(RTLD_NEXT, "eventfd");
    marker = getenv("LATX_TEST_FD_TRANS_RACE_MARKER");
    enabled = getenv("LATX_TEST_FD_TRANS_RACE") != NULL && marker;
    if (!real_g_realloc_n || !real_eventfd) {
        abort();
    }
}

static void *allocate_table(size_t n_blocks, size_t n_block_bytes)
{
    size_t size;
    void *table;

    if (!n_blocks || n_blocks > SIZE_MAX / n_block_bytes) {
        return NULL;
    }
    size = n_blocks * n_block_bytes;
    table = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return table == MAP_FAILED ? NULL : table;
}

static void mark_injected(void)
{
    int fd;

    fd = open(marker, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd >= 0) {
        write(fd, "1", 1);
        close(fd);
    }
}

int eventfd(unsigned int initval, int flags)
{
    int ret;

    pthread_once(&init_once, init_symbols);
    ret = real_eventfd(initval, flags);
    if (enabled && initval == 0x7fffffff && flags == EFD_SEMAPHORE &&
        ret >= 0) {
        expect_fd_table = 1;
    }
    return ret;
}

void *g_realloc_n(void *mem, size_t n_blocks, size_t n_block_bytes)
{
    struct timespec delay = { .tv_sec = 0, .tv_nsec = 250000000 };
    size_t old_blocks;
    size_t old_size;
    void *old_table;
    void *new_mem;

    pthread_once(&init_once, init_symbols);

    if (!enabled || n_block_bytes != sizeof(void *)) {
        return real_g_realloc_n(mem, n_blocks, n_block_bytes);
    }

    if (expect_fd_table) {
        expect_fd_table = 0;
        if (!mem) {
            new_mem = allocate_table(n_blocks, n_block_bytes);
            if (!new_mem) {
                abort();
            }
            __atomic_store_n(&fd_table_blocks, n_blocks, __ATOMIC_RELAXED);
            __atomic_store_n(&fd_table, new_mem, __ATOMIC_RELEASE);
            return new_mem;
        }
    }

    old_table = __atomic_load_n(&fd_table, __ATOMIC_ACQUIRE);
    old_blocks = __atomic_load_n(&fd_table_blocks, __ATOMIC_RELAXED);
    if (mem != old_table || old_blocks > SIZE_MAX - 64 ||
        n_blocks != old_blocks + 64 || !old_blocks ||
        !__sync_bool_compare_and_swap(&injected, 0, 1)) {
        return real_g_realloc_n(mem, n_blocks, n_block_bytes);
    }

    new_mem = allocate_table(n_blocks, n_block_bytes);
    if (!new_mem) {
        abort();
    }
    old_size = old_blocks * n_block_bytes;
    memcpy(new_mem, mem, old_size);
    munmap(mem, old_size);
    mark_injected();
    nanosleep(&delay, NULL);
    return new_mem;
}
