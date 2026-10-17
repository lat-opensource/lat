#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Compile production filter ownership paths with isolated CPU/guest-memory seams.

This tests allocation lifetime, not BPF interpretation or guest syscall delivery.
The unmodified implementation deliberately fails the final allocation assertions.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


def function(source, signature):
    start = source.index(signature)
    while source.find(";", start, source.index("{", start)) >= 0:
        start = source.index(signature, start + len(signature))
    return source[start:source.index("\n}", start) + 2] + "\n"


PRELUDE = r'''
#include <assert.h>
#include <glib.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); abort(); } } while (0)
typedef unsigned long abi_ulong;
typedef long abi_long;
typedef struct GuestSeccompFilter GuestSeccompFilter;
typedef struct { GuestSeccompFilter *seccomp_filter; int ts_tid; bool seccomp_exiting; } TaskState;
typedef struct CPUState { TaskState *opaque; struct CPUState *next; } CPUState;
typedef CPUState CPUArchState;
static CPUState *cpus, *thread_cpu;
static unsigned allocations;
static unsigned root_publications;
static bool list_locked;
static GMutex list_mutex;
static CPUState *publication_watch;
static TaskState *publication_expected;
#define qatomic_store_release(p, v) do { \
    root_publications++; __atomic_store_n(p, v, __ATOMIC_RELEASE); \
} while (0)
static bool valid = true;
static void *tracked_malloc(size_t size) {
    void *p = malloc(size); CHECK(p); allocations++; return p;
}
static void tracked_free(void *p) { if (p) { CHECK(allocations); allocations--; free(p); } }
#undef g_malloc
#undef g_free
#define g_malloc tracked_malloc
#define g_free tracked_free
#define TARGET_EINVAL 22
#define TARGET_EFAULT 14
#define TARGET_EACCES 13
#define TARGET_EAGAIN 11
#define VERIFY_READ 0
#define target_sock_filter sock_filter
#define lock_user(mode, addr, size, copy) ((void *)(addr))
#define unlock_user(ptr, addr, copy) ((void)0)
#define tswap16(x) (x)
#define tswap32(x) (x)
#define env_cpu(env) (env)
#define CPU_FOREACH(cpu) for ((cpu) = cpus; (cpu); (cpu) = (cpu)->next)
#define CPU_FOREACH_SAFE(cpu, next_cpu) for ((cpu) = cpus; (cpu) && (((next_cpu) = (cpu)->next), 1); (cpu) = (next_cpu))
static void remove_cpu(CPUState **head, CPUState *cpu) {
    while (*head != cpu) { CHECK(*head); head = &(*head)->next; }
    *head = cpu->next;
}
#define QTAILQ_REMOVE_RCU(head, cpu, node) remove_cpu(head, cpu)
#define prctl(...) 1
#define start_exclusive() ((void)0)
#define end_exclusive() ((void)0)
static void cpu_list_lock(void) {
    g_mutex_lock(&list_mutex); CHECK(!list_locked); list_locked = true;
}
static void cpu_list_unlock(void) {
    CHECK(list_locked);
    if (publication_watch) {
        CHECK(publication_watch->opaque == publication_expected);
    }
    list_locked = false;
    g_mutex_unlock(&list_mutex);
}
#define mmap_fork_end(child) ((void)0)
#define sigact_fork_end(child) ((void)0)
#define path_fork_end(child) ((void)0)
#define fd_trans_fork_end() ((void)0)
#define qemu_init_cpu_list() ((void)0)
#define gdbserver_fork(cpu) ((void)0)
static bool seccomp_filter_valid(const struct sock_filter *insns, unsigned len) { return valid; }
static abi_long seccomp_load_program(abi_ulong program, unsigned *len, abi_ulong *filter) {
    *len = 1; *filter = program; return 0;
}
'''

MAIN = r'''
static struct sock_filter allow = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW);
static void install(CPUState *cpu, unsigned flags) {
    unsigned before = root_publications;
    CHECK(seccomp_install_filter(cpu, flags, (abi_ulong)&allow) == 0);
    /* Syscall readers need publication ordering even outside cpu_exec. */
    CHECK(root_publications > before);
}
static void release(TaskState *task) {
    guest_seccomp_filter_unref(task->seccomp_filter); task->seccomp_filter = NULL;
}

#ifdef HAVE_SECCOMP_ATTACH
typedef struct AttachRace {
    CPUState *cpu;
    TaskState *task;
    gint *started;
    bool published;
} AttachRace;
static gpointer race_attach(gpointer opaque) {
    AttachRace *race = opaque;
    g_atomic_int_inc(race->started);
    race->published = guest_seccomp_attach_task(race->cpu, race->task);
    return NULL;
}
static gpointer race_tsync(gpointer opaque) {
    AttachRace *race = opaque;
    g_atomic_int_inc(race->started);
    install(race->cpu, SECCOMP_FILTER_FLAG_TSYNC);
    return NULL;
}

static void test_attach_tsync_race(void) {
    for (unsigned i = 0; i < 32; i++) {
        TaskState owner = { .ts_tid = 1 }, attached = { .ts_tid = 2 };
        CPUState newcomer = { 0 }, source = { .opaque = &owner, .next = &newcomer };
        gint started = 0;
        AttachRace attach = { .cpu = &newcomer, .task = &attached, .started = &started };
        AttachRace sync = { .cpu = &source, .started = &started };
        GThread *a, *b;

        cpus = &source;
        install(&source, 0);
        /* Both operations queue on the real lock before either can publish. */
        cpu_list_lock();
        a = g_thread_new("attach", race_attach, &attach);
        b = g_thread_new("tsync", race_tsync, &sync);
        while (g_atomic_int_get(&started) != 2) { g_thread_yield(); }
        cpu_list_unlock();
        g_thread_join(a); g_thread_join(b);
        CHECK(attach.published && newcomer.opaque == &attached);
        CHECK(owner.seccomp_filter == attached.seccomp_filter);
        CHECK(owner.seccomp_filter->references == 2);
        CHECK(owner.seccomp_filter->previous->references == 1);
        CHECK(allocations == 2);
        cleanup_guest_seccomp(&attached);
        cleanup_guest_seccomp(&owner);
        CHECK(allocations == 0);
    }
    puts("seccomp attach: concurrent publication and TSYNC PASS");
}
#endif

static void test_attached_tasks(void) {
#ifdef HAVE_SECCOMP_ATTACH
    TaskState owner = { .ts_tid = 1 }, attached = { .ts_tid = 2 };
    TaskState divergent = { .ts_tid = 3 };
    CPUState source = { .opaque = &owner }, newcomer = { 0 };
    CPUState other = { .opaque = &divergent };

    cpus = &newcomer;
    CHECK(!guest_seccomp_attach_task(&newcomer, &attached));
    CHECK(!newcomer.opaque && !attached.seccomp_filter && allocations == 0);

    cpus = &source; source.next = &newcomer;
    publication_watch = &newcomer; publication_expected = &attached;
    CHECK(guest_seccomp_attach_task(&newcomer, &attached));
    publication_watch = NULL;
    CHECK(!attached.seccomp_filter); /* No policy is also a shared policy. */
    cleanup_guest_seccomp(&attached);
    CHECK(attached.seccomp_exiting);

    /* TSYNC before attach skips the unpublished task; attach takes its result. */
    newcomer.opaque = NULL; attached.seccomp_exiting = false;
    install(&source, SECCOMP_FILTER_FLAG_TSYNC);
    CHECK(allocations == 1 && owner.seccomp_filter->references == 1);
    publication_watch = &newcomer; publication_expected = &attached;
    CHECK(guest_seccomp_attach_task(&newcomer, &attached));
    publication_watch = NULL;
    CHECK(attached.seccomp_filter == owner.seccomp_filter);
    CHECK(owner.seccomp_filter->references == 2);

    /* TSYNC after attach sees its owning root and preserves the previous edge. */
    install(&source, SECCOMP_FILTER_FLAG_TSYNC);
    CHECK(allocations == 2 && owner.seccomp_filter == attached.seccomp_filter);
    CHECK(owner.seccomp_filter->references == 2);
    CHECK(owner.seccomp_filter->previous->references == 1);
    cleanup_guest_seccomp(&attached);
    CHECK(attached.seccomp_exiting && !attached.seccomp_filter);
    CHECK(owner.seccomp_filter->references == 1);
    cleanup_guest_seccomp(&attached); /* A failed attempt can unwind once more. */
    CHECK(owner.seccomp_filter->references == 1);

    /* Different live roots are ambiguous, including filtered versus NULL. */
    newcomer.opaque = NULL; attached.seccomp_exiting = false;
    newcomer.next = &other;
    publication_watch = &newcomer; publication_expected = NULL;
    CHECK(!guest_seccomp_attach_task(&newcomer, &attached));
    publication_watch = NULL;
    CHECK(owner.seccomp_filter->references == 1 && allocations == 2);
    install(&other, 0);
    CHECK(!guest_seccomp_attach_task(&newcomer, &attached));
    CHECK(allocations == 3 && !attached.seccomp_filter);

    /* A departing task is not an eligible policy source. */
    cleanup_guest_seccomp(&divergent);
    CHECK(guest_seccomp_attach_task(&newcomer, &attached));
    CHECK(attached.seccomp_filter == owner.seccomp_filter);
    cleanup_guest_seccomp(&attached);
    cleanup_guest_seccomp(&owner);
    CHECK(allocations == 0);
    publication_watch = NULL;
    puts("seccomp attach: shared policy, publication, TSYNC, divergence, exit PASS");
    test_attach_tsync_race();
#endif
}

int main(void) {
    TaskState tasks[3] = {{ .ts_tid = 1 }, { .ts_tid = 2 }, { .ts_tid = 3 }};
    CPUState cpu[3] = {{ .opaque = &tasks[0] }, { .opaque = &tasks[1] }, { .opaque = &tasks[2] }};
    test_attached_tasks();
    cpus = &cpu[0]; thread_cpu = cpus;
    CHECK(guest_seccomp_filter_ref(NULL) == NULL);
    guest_seccomp_filter_unref(NULL);
    valid = false;
    CHECK(seccomp_install_filter(cpus, 0, (abi_ulong)&allow) == -TARGET_EINVAL);
    CHECK(allocations == 0); valid = true;
    /* cpu_create publishes a CPU before its TaskState is initialized. */
    cpu[0].next = &cpu[1]; cpu[1].opaque = NULL;
    install(&cpu[0], SECCOMP_FILTER_FLAG_TSYNC);
    CHECK(allocations == 1);
    /* An unpublished guest TID cannot report TSYNC failure as success (0). */
    cpu[1].opaque = &tasks[1]; tasks[1].ts_tid = 0;
    CHECK(seccomp_install_filter(&cpu[0], SECCOMP_FILTER_FLAG_TSYNC, (abi_ulong)&allow) == -TARGET_EAGAIN);
    CHECK(allocations == 1);
    release(&tasks[0]); CHECK(allocations == 0);
    tasks[1].seccomp_exiting = true;
    install(&cpu[0], SECCOMP_FILTER_FLAG_TSYNC);
    CHECK(allocations == 1 && tasks[1].seccomp_filter == NULL);
    release(&tasks[0]); CHECK(allocations == 0);
    tasks[1].seccomp_exiting = false;
    tasks[1].ts_tid = 2; cpu[0].next = NULL;
    for (unsigned cycle = 0; cycle < 64; cycle++) {
        /* Clone sharing, private extension, then child exit. */
        install(&cpu[0], 0);
        tasks[1].seccomp_filter = guest_seccomp_filter_ref(tasks[0].seccomp_filter);
        cpu[0].next = &cpu[1];
        install(&cpu[1], 0);
        CHECK(allocations == 2);
        /* A rejected TSYNC must drop only its new node. */
        CHECK(seccomp_install_filter(&cpu[0], SECCOMP_FILTER_FLAG_TSYNC, (abi_ulong)&allow) == 2);
        CHECK(allocations == 2);
        release(&tasks[1]);
        CHECK(allocations == 1);
        tasks[1].seccomp_filter = guest_seccomp_filter_ref(tasks[0].seccomp_filter);
        install(&cpu[0], SECCOMP_FILTER_FLAG_TSYNC);
        CHECK(allocations == 2 && tasks[0].seccomp_filter == tasks[1].seccomp_filter);
        release(&tasks[0]);
        CHECK(allocations == 2); /* Child still owns the full chain. */
        release(&tasks[1]);
        CHECK(allocations == 0);
        cpu[0].next = NULL;
    }
    /* A fork child loses all roots belonging to vanished parent threads. */
    install(&cpu[0], 0);
    tasks[1].seccomp_filter = guest_seccomp_filter_ref(tasks[0].seccomp_filter);
    tasks[2].seccomp_filter = guest_seccomp_filter_ref(tasks[0].seccomp_filter);
    cpu[0].next = &cpu[1]; cpu[1].next = &cpu[2];
    install(&cpu[2], 0);
    fork_end(1);
    CHECK(cpus == &cpu[0] && cpus->next == NULL);
    CHECK(allocations == 1);
    release(&tasks[0]); CHECK(allocations == 0);
    /* Destruction is iterative even with a deep filter chain. */
    for (unsigned i = 0; i < 10000; i++) { install(&cpu[0], 0); }
    CHECK(allocations == 10000);
    release(&tasks[0]); CHECK(allocations == 0);
    puts("seccomp lifetime: clone, private/TSYNC, failure, fork, deep chain PASS");
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("--revision", help="Read production sources from a git revision")
    args = parser.parse_args()

    def read_source(path):
        if args.revision:
            return subprocess.check_output(
                ["git", "-C", str(args.source), "show", f"{args.revision}:{path}"],
                text=True)
        return (args.source / path).read_text()

    source = read_source("linux-user/guest-seccomp.c")
    main_source = read_source("linux-user/main.c")
    begin = source.index("typedef struct GuestSeccompFilter {")
    end = source.index("} GuestSeccompFilter;", begin) + len("} GuestSeccompFilter;")
    text = PRELUDE + source[begin:end] + "\n"
    if "guest_seccomp_filter_ref(" in source:
        text += function(source, "GuestSeccompFilter *guest_seccomp_filter_ref(")
        text += function(source, "void guest_seccomp_filter_unref(")
    else:
        text += "GuestSeccompFilter *guest_seccomp_filter_ref(GuestSeccompFilter *p) { return p; }\n"
        text += "void guest_seccomp_filter_unref(GuestSeccompFilter *p) {}\n"
    text += function(source, "static abi_long seccomp_load_filter(")
    text += function(source, "static abi_long seccomp_install_filter(")
    if "bool guest_seccomp_attach_task(" in source:
        text += "#define HAVE_SECCOMP_ATTACH 1\n"
        text += function(source, "bool guest_seccomp_attach_task(")
        text += function(read_source("linux-user/syscall.c"),
                         "static void cleanup_guest_seccomp(")
    text += function(main_source, "void fork_end(") + MAIN
    flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "glib-2.0"], text=True))
    with tempfile.TemporaryDirectory(prefix="lat-seccomp-lifetime-") as temporary:
        src = Path(temporary) / "test.c"
        src.write_text(text)
        for mode in ([], ["-DNDEBUG"]):
            binary = Path(temporary) / "test"
            subprocess.run(shlex.split(os.environ.get("CC", "cc")) + ["-std=gnu11", "-O2"] + mode + [str(src), "-o", str(binary)] + flags, check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
