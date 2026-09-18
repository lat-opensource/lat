/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <dlfcn.h>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

#include "kzt-tls-dlopen-stress-shared.h"

static int anchor;
static int constructor_error;
static pid_t constructor_child_pid = -1;
static int constructor_is_fork_child;

__thread int stress_ie_value
    __attribute__((tls_model("initial-exec"))) = 0x2468;
__thread void *stress_ie_pointer
    __attribute__((tls_model("initial-exec"))) = &anchor;

static void *constructor_loader_query(void *opaque)
{
    (void)opaque;
    (void)dlerror();
    return NULL;
}

static void __attribute__((constructor)) stress_ie_constructor(void)
{
    pthread_t thread;
    pid_t child;

    if (pthread_create(
            &thread, NULL, constructor_loader_query, NULL) != 0 ||
        pthread_join(thread, NULL) != 0) {
        constructor_error = 1;
    }
    child = fork();
    if (child == 0) {
        constructor_is_fork_child = 1;
        (void)dlerror();
        kzt_tls_stress_mark_fork_child();
        return;
    }
    if (child < 0) {
        constructor_error = 1;
    } else {
        constructor_child_pid = child;
    }
}

int kzt_host_thread_tls_plugin_check(void)
{
    int status;

    if (constructor_is_fork_child) {
        _exit(0);
    }
    if (constructor_child_pid > 0) {
        if (waitpid(constructor_child_pid, &status, 0) !=
                constructor_child_pid || !WIFEXITED(status) ||
            WEXITSTATUS(status) != 0) {
            constructor_error = 1;
        }
        constructor_child_pid = -1;
    }
    return !constructor_error && stress_ie_value == 0x2468 &&
           stress_ie_pointer == &anchor ? 0 : 1;
}
