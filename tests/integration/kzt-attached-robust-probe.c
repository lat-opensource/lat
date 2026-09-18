/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <pthread.h>
#include <string.h>

#include <X11/Xlibint.h>

typedef struct owner_call {
    Display *display;
    int result;
} owner_call;

static pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t state_cond = PTHREAD_COND_INITIALIZER;
static pthread_t owner_thread;
static owner_call call;
static int owner_started;
static int owner_ready;
static int owner_finished;
static int release_owner;

static void *run_owner(void *opaque)
{
    owner_call *owner = opaque;
    xReply reply;

    memset(&reply, 0, sizeof(reply));
    owner->result = owner->display->async_handlers->handler(
        owner->display, &reply, (char *)&reply,
        sizeof(reply), owner->display->async_handlers->data);
    pthread_mutex_lock(&state_lock);
    owner_finished = 1;
    pthread_cond_broadcast(&state_cond);
    pthread_mutex_unlock(&state_lock);
    return NULL;
}

int XNoOp(Display *display)
{
    (void)display;
    pthread_mutex_lock(&state_lock);
    owner_ready = 1;
    pthread_cond_broadcast(&state_cond);
    while (!release_owner) {
        pthread_cond_wait(&state_cond, &state_lock);
    }
    pthread_mutex_unlock(&state_lock);
    return 0;
}

int XEventsQueued(Display *display, int mode)
{
    (void)mode;
    if (!display || !display->async_handlers) {
        return -1;
    }
    pthread_mutex_lock(&state_lock);
    call.display = display;
    call.result = -1;
    owner_ready = 0;
    owner_finished = 0;
    release_owner = 0;
    if (pthread_create(&owner_thread, NULL, run_owner, &call) != 0) {
        pthread_mutex_unlock(&state_lock);
        return -1;
    }
    owner_started = 1;
    while (!owner_ready && !owner_finished) {
        pthread_cond_wait(&state_cond, &state_lock);
    }
    if (owner_finished) {
        pthread_mutex_unlock(&state_lock);
        pthread_join(owner_thread, NULL);
        owner_started = 0;
        return -1;
    }
    pthread_mutex_unlock(&state_lock);
    return 77;
}

int XFlush(Display *display)
{
    (void)display;
    pthread_mutex_lock(&state_lock);
    if (!owner_started) {
        pthread_mutex_unlock(&state_lock);
        return -1;
    }
    release_owner = 1;
    pthread_cond_broadcast(&state_cond);
    pthread_mutex_unlock(&state_lock);
    if (pthread_join(owner_thread, NULL) != 0 || call.result != 0) {
        return -1;
    }
    owner_started = 0;
    return 78;
}
