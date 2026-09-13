/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "qemu/osdep.h"
#include "qemu/rcu.h"
#include "qemu/thread.h"
#include <signal.h>
#include <sys/wait.h>

/* Keep the internal deadlines below the Meson timeout (45 seconds). */
#define CHILD_TIMEOUT_SECONDS 10
#define MAIN_TIMEOUT_SECONDS 30

typedef enum DeadlineStage {
    STAGE_NOT_STARTED,
    STAGE_HELD_READER_START,
    STAGE_HELD_GRACE_PERIOD,
    STAGE_HELD_CHILD_LAZY_DRAIN,
    STAGE_HELD_CHILD_DEFERRED_WORKER_START,
    STAGE_HELD_CHILD_CALLBACK_WAIT,
    STAGE_HELD_PARENT_READER_RELEASE,
    STAGE_HELD_PARENT_DRAIN,
    STAGE_HELD_PARENT_CHILD_WAIT,
    STAGE_EMPTY_CHILD_WORKER_START,
    STAGE_EMPTY_CHILD_CALLBACK_WAIT,
    STAGE_EMPTY_PARENT_CHILD_WAIT,
    STAGE_REENTRANT_CALLBACK_WAIT,
    STAGE_REENTRANT_DRAIN,
    STAGE_CONCURRENT_READER_START,
    STAGE_CONCURRENT_CONSUMER_START,
    STAGE_CONCURRENT_GRACE_PERIOD,
    STAGE_CONCURRENT_PUBLISH_WAIT,
    STAGE_CONCURRENT_CHILD_DRAIN_WORKER_START,
    STAGE_CONCURRENT_CHILD_DRAIN_CALLBACK_WAIT,
    STAGE_CONCURRENT_CHILD_EXACT_ONCE,
    STAGE_CONCURRENT_PARENT_PRODUCER_JOIN,
    STAGE_CONCURRENT_PARENT_READER_RELEASE,
    STAGE_CONCURRENT_PARENT_DRAIN,
    STAGE_CONCURRENT_PARENT_EXACT_ONCE,
    STAGE_CONCURRENT_PARENT_CHILD_WAIT,
} DeadlineStage;

static volatile sig_atomic_t deadline_stage = STAGE_NOT_STARTED;

#define STAGE_DESCRIPTION_SIZE 64
static const char stage_descriptions[][STAGE_DESCRIPTION_SIZE] = {
    [STAGE_NOT_STARTED] = "not started",
    [STAGE_HELD_READER_START] = "held-fork reader startup",
    [STAGE_HELD_GRACE_PERIOD] = "held-fork grace-period wait",
    [STAGE_HELD_CHILD_LAZY_DRAIN] = "held-fork child lazy drain",
    [STAGE_HELD_CHILD_DEFERRED_WORKER_START] =
        "held-fork child deferred worker startup",
    [STAGE_HELD_CHILD_CALLBACK_WAIT] = "held-fork child callback wait",
    [STAGE_HELD_PARENT_READER_RELEASE] = "held-fork parent reader release",
    [STAGE_HELD_PARENT_DRAIN] = "held-fork parent drain",
    [STAGE_HELD_PARENT_CHILD_WAIT] = "held-fork parent child wait",
    [STAGE_EMPTY_CHILD_WORKER_START] = "empty-fork child worker startup",
    [STAGE_EMPTY_CHILD_CALLBACK_WAIT] = "empty-fork child callback wait",
    [STAGE_EMPTY_PARENT_CHILD_WAIT] = "empty-fork parent child wait",
    [STAGE_REENTRANT_CALLBACK_WAIT] = "reentrant callback wait",
    [STAGE_REENTRANT_DRAIN] = "reentrant callback drain",
    [STAGE_CONCURRENT_READER_START] = "concurrent-fork reader startup",
    [STAGE_CONCURRENT_CONSUMER_START] =
        "concurrent-fork blocker callback wait",
    [STAGE_CONCURRENT_GRACE_PERIOD] = "concurrent-fork grace-period wait",
    [STAGE_CONCURRENT_PUBLISH_WAIT] =
        "concurrent-fork first publication wait",
    [STAGE_CONCURRENT_CHILD_DRAIN_WORKER_START] =
        "concurrent-fork child drain worker startup",
    [STAGE_CONCURRENT_CHILD_DRAIN_CALLBACK_WAIT] =
        "concurrent-fork child drain callback wait",
    [STAGE_CONCURRENT_CHILD_EXACT_ONCE] =
        "concurrent-fork child exact-once check",
    [STAGE_CONCURRENT_PARENT_PRODUCER_JOIN] =
        "concurrent-fork parent producer join",
    [STAGE_CONCURRENT_PARENT_READER_RELEASE] =
        "concurrent-fork parent reader release",
    [STAGE_CONCURRENT_PARENT_DRAIN] = "concurrent-fork parent drain",
    [STAGE_CONCURRENT_PARENT_EXACT_ONCE] =
        "concurrent-fork parent exact-once check",
    [STAGE_CONCURRENT_PARENT_CHILD_WAIT] =
        "concurrent-fork parent child wait",
};

static void write_stage(int fd, DeadlineStage stage, bool timeout)
{
    static const char progress_prefix[] = "RCU fork queue stage: ";
    static const char timeout_prefix[] = "RCU fork queue test timed out at: ";
    const char *prefix = timeout ? timeout_prefix : progress_prefix;
    const char *description;
    char message[sizeof(timeout_prefix) + STAGE_DESCRIPTION_SIZE];
    size_t length = 0;

    if ((unsigned)stage >= ARRAY_SIZE(stage_descriptions)) {
        stage = STAGE_NOT_STARTED;
    }
    description = stage_descriptions[stage];
    while (*prefix) {
        message[length++] = *prefix++;
    }
    while (*description) {
        message[length++] = *description++;
    }
    message[length++] = '\n';
    (void)!write(fd, message, length);
}

static void set_stage(DeadlineStage stage)
{
    deadline_stage = stage;
    write_stage(STDERR_FILENO, stage, false);
}

typedef struct TestCallback {
    struct rcu_head rcu;
    int calls;
    bool published;
    QemuEvent done;
} TestCallback;

#ifdef CONFIG_LATX_KZT
static QemuEvent reader_entered, reader_release;
static struct rcu_reader_data *held_reader_state;
#endif

static void deadline(int sig)
{
    write_stage(STDERR_FILENO, deadline_stage, true);
    _exit(124);
}

static void mark_callback(struct rcu_head *head)
{
    TestCallback *callback = container_of(head, TestCallback, rcu);
    int previous_calls = qatomic_fetch_inc(&callback->calls);

    g_assert_cmpint(previous_calls, ==, 0);
    qemu_event_set(&callback->done);
}

#ifdef CONFIG_LATX_KZT
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

static void begin_reader(QemuThread *thread, DeadlineStage stage)
{
    qemu_event_init(&reader_entered, false);
    qemu_event_init(&reader_release, false);
    qemu_thread_create(thread, "held-reader", held_reader, NULL,
                       QEMU_THREAD_JOINABLE);
    set_stage(stage);
    qemu_event_wait(&reader_entered);
}

static void wait_for_grace_period(DeadlineStage stage)
{
    set_stage(stage);
    for (int i = 0; i < 2000; i++) {
        if (qatomic_read(&held_reader_state->waiting)) {
            return;
        }
        g_usleep(1000);
    }
    g_error("RCU callback consumer did not enter the held grace period");
}

static void end_reader(QemuThread *thread, DeadlineStage stage)
{
    set_stage(stage);
    qemu_event_set(&reader_release);
    qemu_thread_join(thread);
    qemu_event_destroy(&reader_entered);
    qemu_event_destroy(&reader_release);
}
#endif

static void check_child(pid_t child)
{
    int status;
    pid_t result = waitpid(child, &status, 0);

    g_assert_cmpint(result, ==, child);
    g_assert_true(WIFEXITED(status));
    g_assert_cmpint(WEXITSTATUS(status), ==, 0);
}

#ifdef CONFIG_LATX_KZT
static void test_held_fork(bool deferred)
{
    QemuThread reader;
    TestCallback callback = {0};
    bool previous_defer = false;
    pid_t child;

    qemu_event_init(&callback.done, false);
    begin_reader(&reader, STAGE_HELD_READER_START);
    call_rcu1(&callback.rcu, mark_callback);
    wait_for_grace_period(STAGE_HELD_GRACE_PERIOD);
    g_assert_cmpint(qatomic_read(&callback.calls), ==, 0);
    if (deferred) {
        previous_defer = rcu_defer_atfork_child();
    }
    child = fork();
    g_assert_cmpint(child, >=, 0);
    if (child == 0) {
        alarm(CHILD_TIMEOUT_SECONDS);
        g_assert_false(rcu_call_thread_is_running());
        g_usleep(10000);
        g_assert_false(rcu_call_thread_is_running());
        if (deferred) {
            /* This is the existing guest namespace/thread safe-point API. */
            set_stage(STAGE_HELD_CHILD_DEFERRED_WORKER_START);
            rcu_start_deferred_thread();
            set_stage(STAGE_HELD_CHILD_CALLBACK_WAIT);
            qemu_event_wait(&callback.done);
        } else {
            /* The first new queued call must also drain inherited work. */
            set_stage(STAGE_HELD_CHILD_LAZY_DRAIN);
            drain_call_rcu();
        }
        g_assert_cmpint(qatomic_read(&callback.calls), ==, 1);
        _exit(0);
    }
    if (deferred) {
        rcu_restore_atfork_child_defer(previous_defer);
    }
    end_reader(&reader, STAGE_HELD_PARENT_READER_RELEASE);
    set_stage(STAGE_HELD_PARENT_DRAIN);
    drain_call_rcu();
    set_stage(STAGE_HELD_PARENT_CHILD_WAIT);
    check_child(child);
    g_assert_cmpint(qatomic_read(&callback.calls), ==, 1);
    qemu_event_destroy(&callback.done);
}
#endif

static void test_empty_fork(void)
{
    pid_t child;

    drain_call_rcu();
    child = fork();
    g_assert_cmpint(child, >=, 0);
    if (child == 0) {
        TestCallback callback = {0};

        alarm(CHILD_TIMEOUT_SECONDS);
#ifdef CONFIG_LATX
        g_assert_false(rcu_call_thread_is_running());
#endif
        qemu_event_init(&callback.done, false);
        set_stage(STAGE_EMPTY_CHILD_WORKER_START);
        call_rcu1(&callback.rcu, mark_callback);
        set_stage(STAGE_EMPTY_CHILD_CALLBACK_WAIT);
        qemu_event_wait(&callback.done);
        g_assert_cmpint(qatomic_read(&callback.calls), ==, 1);
        _exit(0);
    }
    set_stage(STAGE_EMPTY_PARENT_CHILD_WAIT);
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
    set_stage(STAGE_REENTRANT_CALLBACK_WAIT);
    qemu_event_wait(&nested.done);
    set_stage(STAGE_REENTRANT_DRAIN);
    drain_call_rcu();
    g_assert_cmpint(qatomic_read(&nested.calls), ==, 1);
    qemu_event_destroy(&nested.done);
}

/* One publisher still races call_rcu1() against fork; keep 512 callbacks. */
#define PRODUCERS 1
#define CALLBACKS_PER_PRODUCER 512
static TestCallback concurrent[PRODUCERS * CALLBACKS_PER_PRODUCER];
static int published, completed;
static QemuEvent consumer_entered, consumer_release;

static void hold_consumer(struct rcu_head *head)
{
    /* Outside synchronize_rcu(): ordinary atfork can take rcu_sync_lock. */
    qemu_event_set(&consumer_entered);
    qemu_event_wait(&consumer_release);
}

static void check_published_callbacks(void)
{
    int checked = 0;

    for (size_t i = 0; i < ARRAY_SIZE(concurrent); i++) {
        if (qatomic_read(&concurrent[i].published)) {
            g_assert_cmpint(qatomic_read(&concurrent[i].calls), ==, 1);
            checked++;
        }
    }
    /* Every fork must exercise a nonempty published snapshot. */
    g_assert_cmpint(checked, >, 0);
}

static void concurrent_callback(struct rcu_head *head)
{
    TestCallback *callback = container_of(head, TestCallback, rcu);
    int previous_calls = qatomic_fetch_inc(&callback->calls);

    g_assert_cmpint(previous_calls, ==, 0);
    qatomic_inc(&completed);
}

static void drain_concurrent_child(void)
{
    TestCallback barrier = {0};

    qemu_event_init(&barrier.done, false);
    set_stage(STAGE_CONCURRENT_CHILD_DRAIN_WORKER_START);
    call_rcu1(&barrier.rcu, mark_callback);
    g_assert_true(rcu_call_thread_is_running());
    set_stage(STAGE_CONCURRENT_CHILD_DRAIN_CALLBACK_WAIT);
    qemu_event_wait(&barrier.done);
    g_assert_cmpint(qatomic_read(&barrier.calls), ==, 1);
    qemu_event_destroy(&barrier.done);
}

static void *producer(void *opaque)
{
    size_t id = (size_t)opaque;

    for (size_t i = 0; i < CALLBACKS_PER_PRODUCER; i++) {
        TestCallback *callback = &concurrent[id * CALLBACKS_PER_PRODUCER + i];

        call_rcu1(&callback->rcu, concurrent_callback);
        /* Only callbacks whose enqueue has returned are fully published. */
        qatomic_set(&callback->published, true);
        qatomic_inc(&published);
        g_usleep(250);
    }
    return NULL;
}

static void test_concurrent_fork(bool held)
{
#ifdef CONFIG_LATX_KZT
    QemuThread reader;
#endif
    QemuThread producers[PRODUCERS];
    struct rcu_head blocker = {0};
    pid_t child;

#ifdef CONFIG_LATX_KZT
    if (held) {
        begin_reader(&reader, STAGE_CONCURRENT_READER_START);
    } else
#endif
    {
        /*
         * This callback has already been dequeued and is not replayed in the
         * child. All producer callbacks remain queued until the parent releases
         * it, so no callback under test can be in flight at the fork snapshot.
         */
        qemu_event_init(&consumer_entered, false);
        qemu_event_init(&consumer_release, false);
        call_rcu1(&blocker, hold_consumer);
        set_stage(STAGE_CONCURRENT_CONSUMER_START);
        qemu_event_wait(&consumer_entered);
    }
    for (size_t i = 0; i < PRODUCERS; i++) {
        qemu_thread_create(&producers[i], "rcu-producer", producer, (void *)i,
                           QEMU_THREAD_JOINABLE);
    }
#ifdef CONFIG_LATX_KZT
    if (held) {
        wait_for_grace_period(STAGE_CONCURRENT_GRACE_PERIOD);
    }
#endif
    set_stage(STAGE_CONCURRENT_PUBLISH_WAIT);
    while (qatomic_read(&published) == 0) {
        g_usleep(1000);
    }
    for (int i = 0; i < 4; i++) {
        child = fork();
        g_assert_cmpint(child, >=, 0);
        if (child == 0) {
            alarm(CHILD_TIMEOUT_SECONDS);
#ifdef CONFIG_LATX
            g_assert_false(rcu_call_thread_is_running());
            g_usleep(10000);
            g_assert_false(rcu_call_thread_is_running());
#else
            g_assert_true(rcu_call_thread_is_running());
#endif
            drain_concurrent_child();
            set_stage(STAGE_CONCURRENT_CHILD_EXACT_ONCE);
            check_published_callbacks();
            _exit(0);
        }
        /*
         * Keep post-fork worker startups independent.  QEMU user-mode can
         * strand one when several emulated children create pthreads at once.
         * Every child still checks its full published snapshot exactly once.
         */
        set_stage(STAGE_CONCURRENT_PARENT_CHILD_WAIT);
        check_child(child);
    }
    set_stage(STAGE_CONCURRENT_PARENT_PRODUCER_JOIN);
    for (size_t i = 0; i < PRODUCERS; i++) {
        qemu_thread_join(&producers[i]);
    }
#ifdef CONFIG_LATX_KZT
    if (held) {
        end_reader(&reader, STAGE_CONCURRENT_PARENT_READER_RELEASE);
    } else
#endif
    {
        qemu_event_set(&consumer_release);
    }
    set_stage(STAGE_CONCURRENT_PARENT_DRAIN);
    drain_call_rcu();
    set_stage(STAGE_CONCURRENT_PARENT_EXACT_ONCE);
    check_published_callbacks();
    if (!held) {
        qemu_event_destroy(&consumer_entered);
        qemu_event_destroy(&consumer_release);
    }
    g_assert_cmpint(qatomic_read(&completed), ==,
                    PRODUCERS * CALLBACKS_PER_PRODUCER);
}

int main(int argc, char **argv)
{
    signal(SIGALRM, deadline);
    alarm(MAIN_TIMEOUT_SECONDS);
#ifdef CONFIG_LATX_KZT
    if (argc == 2 && !strcmp(argv[1], "--held-reader")) {
        test_held_fork(false);
        test_held_fork(true);
        test_concurrent_fork(true);
        puts("RCU fork queue: held grace, lazy/deferred and concurrent "
             "publication passed");
    } else
#endif
    {
        g_assert_cmpint(argc, ==, 1);
        test_empty_fork();
        test_reentrant_callback();
        test_concurrent_fork(false);
        puts("RCU fork queue: empty, reentrant and concurrent queued callbacks passed");
    }
    alarm(0);
    return 0;
}
