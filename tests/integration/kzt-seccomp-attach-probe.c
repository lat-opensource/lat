/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
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
    fprintf(stderr, "NATIVE_CALLBACK_THREAD=%ld\n", syscall(SYS_gettid));
    pthread_mutex_lock(&lock);
    while (!stop) {
        unsigned task;
        xReply reply = { 0 };

        while (!stop && requested == completed) {
            pthread_cond_wait(&cond, &lock);
        }
        if (stop) {
            break;
        }
        task = requested;
        pthread_mutex_unlock(&lock);
        result = target->async_handlers->handler(
            target, &reply, NULL, 0, target->async_handlers->data);
        pthread_mutex_lock(&lock);
        completed = task;
        pthread_cond_broadcast(&cond);
    }
    pthread_mutex_unlock(&lock);
    return NULL;
}

int XEventsQueued(Display *display, int mode)
{
    (void)mode;
    pthread_mutex_lock(&lock);
    if (!started) {
        target = display;
        stop = 0;
        if (pthread_create(&worker, NULL, run, NULL)) {
            pthread_mutex_unlock(&lock);
            return -1;
        }
        started = 1;
    }
    requested++;
    pthread_cond_broadcast(&cond);
    while (completed != requested) {
        pthread_cond_wait(&cond, &lock);
    }
    pthread_mutex_unlock(&lock);
    return result;
}

int XFlush(Display *display)
{
    (void)display;
    pthread_mutex_lock(&lock);
    stop = 1;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);
    if (pthread_join(worker, NULL)) {
        return -1;
    }
    started = 0;
    return 0;
}
