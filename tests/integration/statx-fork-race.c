#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

static atomic_int stop;
static atomic_int failed;
static atomic_ulong calls;
static void *unmapped;

static void *stat_worker(void *unused)
{
    (void)unused;

    while (!atomic_load(&stop)) {
        if (syscall(SYS_statx, AT_FDCWD, "/", 0, STATX_BASIC_STATS,
                    unmapped) != -1 || errno != EFAULT) {
            atomic_store(&failed, 1);
            break;
        }
        atomic_fetch_add(&calls, 1);
    }
    return NULL;
}

int main(void)
{
    pthread_t thread;
    unsigned long page_size = getauxval(AT_PAGESZ);
    int i;

    if (!page_size) {
        fprintf(stderr, "missing guest page size\n");
        return 1;
    }
    /* Keep the address inside the guest VA range, but without a page entry. */
    unmapped = mmap(NULL, 3 * page_size, PROT_NONE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (unmapped == MAP_FAILED) {
        perror("guard mapping");
        return 1;
    }
    unmapped = (char *)unmapped + page_size;
    if (munmap(unmapped, page_size)) {
        perror("unmapped output address");
        return 1;
    }
    if (pthread_create(&thread, NULL, stat_worker, NULL)) {
        return 1;
    }
    while (!atomic_load(&calls) && !atomic_load(&failed)) {
        sched_yield();
    }
    for (i = 0; i < 1000 && !atomic_load(&failed); i++) {
        int status;
        pid_t child = fork();

        if (child == 0) {
            _exit(0);
        }
        if (child < 0 || waitpid(child, &status, 0) != child ||
            !WIFEXITED(status) || WEXITSTATUS(status)) {
            perror("fork/wait");
            atomic_store(&failed, 1);
            break;
        }
    }
    atomic_store(&stop, 1);
    pthread_join(thread, NULL);
    printf("statx-fork: forks=%d EFAULT-calls=%lu failed=%d\n", i,
           atomic_load(&calls), atomic_load(&failed));
    return atomic_load(&failed) || i != 1000;
}
