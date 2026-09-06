/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <time.h>
#include <X11/Xlibint.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_t worker;
static Display *target;
static unsigned requested, completed;
static int started, stop, result;

static void *run(void *unused)
{
    (void)unused;
    pthread_mutex_lock(&lock);
    while (!stop) {
        unsigned task;
        xReply reply = { 0 };
        while (!stop && requested == completed) {
            pthread_cond_wait(&cond, &lock);
        }
        if (stop) break;
        task = requested;
        pthread_mutex_unlock(&lock);
        result = target->async_handlers->handler(target, &reply,
            (char *)&reply, sizeof(reply), target->async_handlers->data);
        pthread_mutex_lock(&lock);
        completed = task;
        pthread_cond_broadcast(&cond);
    }
    pthread_mutex_unlock(&lock);
    return NULL;
}

int XEventsQueued(Display *display, int mode)
{
    pthread_mutex_lock(&lock);
    if (!started) {
        target = display;
        if (pthread_create(&worker, NULL, run, NULL)) {
            pthread_mutex_unlock(&lock);
            return -1;
        }
        started = 1;
    }
    requested++;
    pthread_cond_broadcast(&cond);
    if (!mode) {
        while (completed != requested) pthread_cond_wait(&cond, &lock);
    }
    pthread_mutex_unlock(&lock);
    return mode ? 0 : result;
}

int XFlush(Display *display)
{
    (void)display;
    pthread_mutex_lock(&lock);
    while (completed != requested) pthread_cond_wait(&cond, &lock);
    stop = 1;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);
    if (pthread_join(worker, NULL)) return -1;
    return result;
}

int XNoOp(Display *display)
{
    if (display->fd) {
        const uintptr_t *control = (const uintptr_t *)display->async_handlers->data;
        char path[128];
        const struct timespec delay = { .tv_nsec = 1000000 };

        snprintf(path, sizeof(path), "/proc/self/task/%lu/syscall",
                 (unsigned long)control[0]);
        for (int attempt = 0; attempt < 2000; attempt++) {
            FILE *file = fopen(path, "r");
            long nr = -1;
            unsigned long argument = 0;

            if (file) {
                int fields = fscanf(file, "%ld %lx", &nr, &argument);
                fclose(file);
                if (fields == 2 && argument == control[1] &&
                    nr == (display->fd == 1 ? SYS_read : SYS_futex)) {
                    fprintf(stderr, "ATTACHED_BLOCKED tid=%lu syscall=%ld argument=%#lx\n",
                            (unsigned long)control[0], nr, argument);
                    return 0;
                }
            }
            nanosleep(&delay, NULL);
        }
        return -1;
    }
    void **cpu = dlsym(RTLD_DEFAULT, "thread_cpu");
    void (*flush)(void *) = (void (*)(void *))dlsym(RTLD_DEFAULT, "tb_flush");
    (void)display;
    if (!cpu || !*cpu || !flush) return -1;
    fprintf(stderr, "REQUEST_FULL_TB_FLUSH\n");
    flush(*cpu);
    return 0;
}
