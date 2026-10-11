#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

static int worker_stop[2];

__attribute__((noinline, noclone)) static uint64_t fork_hot_value(uint64_t value)
{
    value ^= UINT64_C(0x9e3779b97f4a7c15);
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    return value ^ (value >> 29);
}

static void *waiting_worker(void *opaque)
{
    char byte;
    (void)opaque;
    return read(worker_stop[0], &byte, 1) == 1 ? NULL : (void *)1;
}

int main(void)
{
    pthread_t worker;
    uint64_t expected = fork_hot_value(42);

    if (pipe(worker_stop) || pthread_create(&worker, NULL, waiting_worker, NULL)) {
        return 1;
    }
    if (fork_hot_value(42) != expected) {
        return 2;
    }

    pid_t child = fork();
    if (child < 0) {
        return 3;
    }
    if (child == 0) {
        if (fork_hot_value(42) != expected) {
            return 4;
        }
        puts("FORK_CHILD_AOT_OK");
        return 0;
    }

    int status;
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0 || fork_hot_value(42) != expected) {
        return 5;
    }
    if (write(worker_stop[1], "x", 1) != 1 ||
        pthread_join(worker, NULL)) {
        return 6;
    }
    close(worker_stop[0]);
    close(worker_stop[1]);
    printf("FORK_OK parent_aot_child_aot=1 value=%llu\n",
           (unsigned long long)expected);
    return 0;
}
