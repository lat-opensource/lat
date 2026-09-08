/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include "qemu/rcu.h"
#include "qemu/thread.h"
#include <signal.h>
#include <sys/wait.h>

typedef struct TestCallback {
    struct rcu_head rcu;
    int calls;
    QemuEvent done;
} TestCallback;

static QemuEvent reader_entered, reader_release;
static struct rcu_reader_data *held_reader_state;

static void deadline(int sig)
{
    static const char message[] = "RCU fork queue test timed out\n";

    (void)!write(STDERR_FILENO, message, sizeof(message) - 1);
    _exit(124);
}

static void mark_callback(struct rcu_head *head)
{
    TestCallback *callback = container_of(head, TestCallback, rcu);
    int previous_calls = qatomic_fetch_inc(&callback->calls);

    g_assert_cmpint(previous_calls, ==, 0);
    qemu_event_set(&callback->done);
}

static void *held_reader(void *opaque)
{
    rcu_register_thread();
    rcu_read_lock();
    held_reader_state = &rcu_reader;
    qemu_event_set(&reader_entered);
    qemu_event_wait(&reader_release);
    rcu_read_unlock();
    rcu_unregister_thread();
    return NULL;
}

static void begin_reader(QemuThread *thread)
{
    qemu_event_init(&reader_entered, false);
    qemu_event_init(&reader_release, false);
    qemu_thread_create(thread, "held-reader", held_reader, NULL,
                       QEMU_THREAD_JOINABLE);
    qemu_event_wait(&reader_entered);
}

static void wait_for_grace_period(void)
{
    for (int i = 0; i < 2000; i++) {
        if (qatomic_read(&held_reader_state->waiting)) {
            return;
        }
        g_usleep(1000);
    }
    g_error("RCU callback consumer did not enter the held grace period");
}

static void end_reader(QemuThread *thread)
{
    qemu_event_set(&reader_release);
    qemu_thread_join(thread);
    qemu_event_destroy(&reader_entered);
    qemu_event_destroy(&reader_release);
}

static void check_child(pid_t child)
{
    int status;
    pid_t result = waitpid(child, &status, 0);

    g_assert_cmpint(result, ==, child);
    g_assert_true(WIFEXITED(status));
    g_assert_cmpint(WEXITSTATUS(status), ==, 0);
}

static void test_held_fork(bool deferred)
{
    QemuThread reader;
    TestCallback callback = {0};
    bool previous_defer = false;
    pid_t child;

    qemu_event_init(&callback.done, false);
    begin_reader(&reader);
    call_rcu1(&callback.rcu, mark_callback);
    wait_for_grace_period();
    g_assert_cmpint(qatomic_read(&callback.calls), ==, 0);
    if (deferred) {
        previous_defer = rcu_defer_atfork_child();
    }
    child = fork();
    g_assert_cmpint(child, >=, 0);
    if (child == 0) {
        alarm(3);
        g_assert_false(rcu_call_thread_is_running());
        g_usleep(10000);
        g_assert_false(rcu_call_thread_is_running());
        if (deferred) {
            /* This is the existing guest namespace/thread safe-point API. */
            rcu_start_deferred_thread();
            qemu_event_wait(&callback.done);
        } else {
            /* The first new queued call must also drain inherited work. */
            drain_call_rcu();
        }
        g_assert_cmpint(qatomic_read(&callback.calls), ==, 1);
        _exit(0);
    }
    if (deferred) {
        rcu_restore_atfork_child_defer(previous_defer);
    }
    end_reader(&reader);
    drain_call_rcu();
    check_child(child);
    g_assert_cmpint(qatomic_read(&callback.calls), ==, 1);
    qemu_event_destroy(&callback.done);
}

static void test_empty_fork(void)
{
    pid_t child;

    drain_call_rcu();
    child = fork();
    g_assert_cmpint(child, >=, 0);
    if (child == 0) {
        TestCallback callback = {0};

        alarm(3);
        g_assert_false(rcu_call_thread_is_running());
        qemu_event_init(&callback.done, false);
        call_rcu1(&callback.rcu, mark_callback);
        qemu_event_wait(&callback.done);
        g_assert_cmpint(qatomic_read(&callback.calls), ==, 1);
        _exit(0);
    }
    check_child(child);
}

static TestCallback nested;

static void reentrant_callback(struct rcu_head *head)
{
    call_rcu1(&nested.rcu, mark_callback);
}

static void test_reentrant_callback(void)
{
    struct rcu_head first = {0};

    qemu_event_init(&nested.done, false);
    call_rcu1(&first, reentrant_callback);
    qemu_event_wait(&nested.done);
    drain_call_rcu();
    g_assert_cmpint(qatomic_read(&nested.calls), ==, 1);
    qemu_event_destroy(&nested.done);
}

#define PRODUCERS 4
#define CALLBACKS_PER_PRODUCER 128
static TestCallback concurrent[PRODUCERS * CALLBACKS_PER_PRODUCER];
static int published, completed;

static void concurrent_callback(struct rcu_head *head)
{
    TestCallback *callback = container_of(head, TestCallback, rcu);
    int previous_calls = qatomic_fetch_inc(&callback->calls);

    g_assert_cmpint(previous_calls, ==, 0);
    qatomic_inc(&completed);
}

static void *producer(void *opaque)
{
    size_t id = (size_t)opaque;

    for (size_t i = 0; i < CALLBACKS_PER_PRODUCER; i++) {
        TestCallback *callback = &concurrent[id * CALLBACKS_PER_PRODUCER + i];

        call_rcu1(&callback->rcu, concurrent_callback);
        qatomic_inc(&published);
        g_usleep(250);
    }
    return NULL;
}

static void test_concurrent_fork(void)
{
    QemuThread reader, producers[PRODUCERS];
    pid_t children[4];

    begin_reader(&reader);
    for (size_t i = 0; i < PRODUCERS; i++) {
        qemu_thread_create(&producers[i], "rcu-producer", producer, (void *)i,
                           QEMU_THREAD_JOINABLE);
    }
    wait_for_grace_period();
    for (int i = 0; i < ARRAY_SIZE(children); i++) {
        int before_fork = qatomic_read(&published);

        children[i] = fork();
        g_assert_cmpint(children[i], >=, 0);
        if (children[i] == 0) {
            alarm(3);
            drain_call_rcu();
            g_assert_cmpint(qatomic_read(&completed), >=, before_fork);
            _exit(0);
        }
    }
    for (size_t i = 0; i < PRODUCERS; i++) {
        qemu_thread_join(&producers[i]);
    }
    end_reader(&reader);
    drain_call_rcu();
    g_assert_cmpint(qatomic_read(&completed), ==,
                    PRODUCERS * CALLBACKS_PER_PRODUCER);
    for (int i = 0; i < ARRAY_SIZE(children); i++) {
        check_child(children[i]);
    }
}

int main(void)
{
    signal(SIGALRM, deadline);
    alarm(20);
    test_held_fork(false);
    test_held_fork(true);
    test_empty_fork();
    test_reentrant_callback();
    test_concurrent_fork();
    alarm(0);
    puts("RCU fork queue: held grace, lazy/deferred, reentrant and concurrent producers passed");
    return 0;
}
