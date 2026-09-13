#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Exercise production thread teardown and netlink receive rollback bodies.

This isolated ownership seam checks allocations and teardown ordering; target
thread-churn and seccomp tests additionally validate the full emulator path.
"""

import argparse
import os
import pathlib
import shlex
import subprocess
import tempfile


def function(source, signature, optional=False):
    start = source.find(signature)
    if start < 0 and optional:
        return ""
    if start < 0:
        raise ValueError(f"Missing production function: {signature}")
    return source[start:source.index("\n}", start) + 2] + "\n"


PRELUDE = r'''
#include <assert.h>
#include <errno.h>
#include <glib.h>
#include <limits.h>
#include <linux/netlink.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/uio.h>
#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); \
} } while (0)
#define CONFIG_LATX 1
#define CONFIG_LATX_FAST_JMPCACHE 1
#define CONFIG_LATX_SMC_OPT 1
#define QEMU_NORETURN __attribute__((noreturn))
#define OBJECT(x) (x)
#define FUTEX_WAKE 1
#define BUFF_4K 4096
#define BUFF_16K 16384
#define TARGET_GDT_ENTRIES 9
typedef long abi_long;
typedef struct { void *bucket; } IMM_CACHE;
typedef struct { void *ir2_inst_array; IMM_CACHE *imm_cache; } TRANSLATION_DATA;
typedef struct { unsigned long pc; unsigned marker; } TranslationBlock;
typedef struct { GTree *tree; unsigned tb_num, ir1_num_in_tu; } TUControl;
typedef struct { void *cpu_state; TRANSLATION_DATA *tr_data; } ENV;
typedef struct {
    unsigned child_tidptr;
    void *seccomp_filter;
    bool seccomp_exiting;
} TaskState;
typedef struct { TaskState *opaque; } CPUState;
typedef struct {
    CPUState *cpu;
    void *tb_jmp_cache_ptr;
    struct { uintptr_t base; } gdt;
} CPUArchState;
typedef CPUArchState CPUX86State;
static __thread TRANSLATION_DATA tr_data_real;
static __thread TUControl tu_data_rel, *tu_data;
static __thread ENV lsenv_real, *lsenv;
static __thread GTree *smc_retrans_tree;
static __thread struct nlmsghdr *pre_nlmh;
static __thread void *buf;
static __thread abi_long all_len;
static CPUState *thread_cpu;
static int clone_lock, allocations, trees, retired_caches, rcu_unregistered;
static int filter_unref_count, release_cpu, cpu_unrealized, gdt_unmapped;
static bool cpu_list_locked;
static jmp_buf exit_buffer;
static void *tracked_malloc(size_t n) {
    void *p = malloc(n); CHECK(p); allocations++; return p;
}
static void tracked_free(void *p) { if (p) { allocations--; free(p); } }
static GTree *tracked_tree_new(GCompareFunc cmp) {
    trees++; return g_tree_new(cmp);
}
static void tracked_tree_destroy(GTree *tree) {
    CHECK(tree); trees--; g_tree_destroy(tree);
}
static CPUState *env_cpu(CPUArchState *env) { return env->cpu; }
static void object_property_set_bool(void *o, const char *p, bool b, void *e) {
    TaskState *task = ((CPUState *)o)->opaque;
    CHECK(task->seccomp_exiting && !task->seccomp_filter && !cpu_list_locked);
    cpu_unrealized++;
}
static void object_unparent(void *o) {}
static void object_unref(void *o) { if (release_cpu) tracked_free(o); }
static void fake_mutex_unlock(void *m) {}
static void put_user_u32(unsigned n, unsigned p) {}
static void *g2h(CPUState *cpu, unsigned p) { return NULL; }
static void do_sys_futex(void *p, int op, int val, void *t, void *p2, int val2) {}
static void latx_fast_jmp_cache_free_rcu(void *p) {
    if (!p) { return; }
    CHECK(p == (void *)0x1234); retired_caches++;
}
static void target_munmap(uintptr_t address, size_t length, int flags) {
    CHECK(length == sizeof(uint64_t) * TARGET_GDT_ENTRIES);
    tracked_free((void *)address); gdt_unmapped++;
}
static void rcu_unregister_thread(void) { rcu_unregistered++; }
static void cpu_list_lock(void) { CHECK(!cpu_list_locked); cpu_list_locked = true; }
static void cpu_list_unlock(void) { CHECK(cpu_list_locked); cpu_list_locked = false; }
static void guest_seccomp_filter_unref(void *p) {
    CHECK(cpu_list_locked && p == (void *)0x5678); filter_unref_count++;
}
static void QEMU_NORETURN fake_pthread_exit(void *p) {
    CHECK(rcu_unregistered); longjmp(exit_buffer, 1);
}
#define malloc tracked_malloc
#define free tracked_free
#define g_free tracked_free
#define g_tree_new tracked_tree_new
#define g_tree_destroy tracked_tree_destroy
#define pthread_mutex_unlock fake_mutex_unlock
#define pthread_exit fake_pthread_exit
'''

MAIN = r'''
static void check_empty(void) {
    CHECK(allocations == 0);
    CHECK(!buf && !pre_nlmh && all_len == 0);
    CHECK(!lsenv && !tr_data_real.ir2_inst_array && !tr_data_real.imm_cache);
#ifdef CONFIG_LATX_TU
    CHECK(!tu_data && trees == 0);
#endif
    CHECK(!smc_retrans_tree && trees == 0);
}
static void thread_exit_case(int mode) {
    TranslationBlock shared_tb = { .pc = 42, .marker = 0xfeed };
    CPUState cpu = { .opaque = malloc(sizeof(TaskState)) };
    CPUArchState env = { .cpu = &cpu, .tb_jmp_cache_ptr = (void *)0x1234 };
    cpu.opaque->child_tidptr = 0;
    cpu.opaque->seccomp_filter = (void *)0x5678;
    thread_cpu = &cpu;
    lsenv = &lsenv_real;
    lsenv->tr_data = &tr_data_real;
#ifdef CONFIG_LATX_TU
    tu_control_init();
    g_tree_insert(tu_data->tree, &shared_tb, &shared_tb);
#endif
    smc_retrans_tree_init();
    CHECK(smc_retrans_insert(&shared_tb));
    if (mode != 0) {
        tr_data_real.ir2_inst_array = malloc(400 * 44);
        tr_data_real.imm_cache = malloc(sizeof(IMM_CACHE));
        tr_data_real.imm_cache->bucket = mode == 1 ? NULL : malloc(300 * 72);
    }
    if (mode == 3) {
        struct iovec iov = {0};
        struct msghdr msg = { .msg_iov = &iov, .msg_iovlen = 1 };
        set_16k_buf(&msg);
        all_len = 8192; /* The guest exits before draining pending messages. */
    }
    if (!setjmp(exit_buffer)) {
        exit_guest_thread_locked(&env);
    }
    CHECK(thread_cpu == NULL);
    CHECK(shared_tb.marker == 0xfeed); /* TU trees do not own shared TBs. */
    check_empty();
    latx_smc_thread_cleanup();
    latx_smc_thread_cleanup();
    check_empty();
#ifdef HAVE_LATX_DESTROY
    latx_lsenv_destroy();
    latx_lsenv_destroy();
    check_empty();
#endif
}
int main(void) {
#ifdef HAVE_LATX_DESTROY
    latx_lsenv_destroy(); /* No initialization. */
    check_empty();
#endif
    for (int i = 0; i < 32; i++) {
        const int modes[] = { 2, 0, 1, 3 };
        for (int j = 0; j < 4; j++) {
            thread_exit_case(modes[j]);
        }
    }
    CHECK(retired_caches == 128 && rcu_unregistered == 128);
    CHECK(filter_unref_count == 128);
    puts("thread teardown: lazy, partial, full, pending netlink, repeated cleanup passed");
    return 0;
}
'''

NETLINK_STUBS = r'''
static bool buf_need_fix(struct msghdr *msg, int fd) { return true; }
static int get_errno(int ret) { return ret; }
static bool is_error(int ret) { return ret < 0; }
static int recv_result;
static int safe_recvmsg(int fd, struct msghdr *msg, int flags) {
    CHECK(msg->msg_iov->iov_len == BUFF_16K);
    if (recv_result >= 0) {
        memset(msg->msg_iov->iov_base, 0, BUFF_16K);
        struct nlmsghdr *h = msg->msg_iov->iov_base;
        h->nlmsg_len = recv_result;
    }
    return recv_result;
}
static abi_long receive_fragment(struct msghdr msg) {
    abi_long ret;
    int fd = 3, flags = 0;
'''

NETLINK_MAIN = r'''
out:
    return ret;
}
int main(void) {
    char guest_buffer[BUFF_4K];
    struct iovec iov = { .iov_base = guest_buffer, .iov_len = sizeof(guest_buffer) };
    struct msghdr msg = { .msg_iov = &iov, .msg_iovlen = 1 };
    for (int i = 0; i < 32; i++) {
        recv_result = -11; /* EAGAIN must leave the original guest iov intact. */
        CHECK(receive_fragment(msg) == -11);
        CHECK(iov.iov_base == guest_buffer && iov.iov_len == BUFF_4K);
        CHECK(allocations == 0 && !buf && !pre_nlmh && all_len == 0);
        recv_result = NLMSG_LENGTH(0);
        CHECK(receive_fragment(msg) == recv_result);
        CHECK(iov.iov_base == guest_buffer && iov.iov_len == BUFF_4K);
        CHECK(allocations == 0 && !buf && !pre_nlmh && all_len == 0);
    }
    puts("netlink receive: EAGAIN rollback, successful drain, retry passed");
    return 0;
}
'''


CLONE_STUBS = r'''
typedef struct { int mutex, cond; unsigned tid; } new_thread_info;
#define sigprocmask(...) ((void)0)
#define pthread_attr_destroy(...) ((void)0)
#define pthread_cond_wait(...) ((void)0)
#define pthread_cond_destroy(...) ((void)0)
#define pthread_mutex_destroy(...) ((void)0)
static int finish_clone(int ret, CPUArchState *new_env) {
    int thread_errno = 0;
    new_thread_info info = { .tid = 42 };
'''

CLONE_MAIN = r'''
    return ret;
}
int main(void) {
    TRANSLATION_DATA *parent_data = &tr_data_real;
    ENV *parent_lsenv = &lsenv_real;
    char *parent_gdt = malloc(72);
    parent_gdt[0] = 0x42;
    lsenv = parent_lsenv;
    lsenv->tr_data = parent_data;
    release_cpu = 1;
    for (int i = 0; i < 64; i++) {
        CPUArchState child_env = {0};
        child_env.cpu = malloc(sizeof(CPUState));
        child_env.cpu->opaque = malloc(sizeof(TaskState));
        child_env.cpu->opaque->seccomp_filter = (void *)0x5678;
        child_env.gdt.base = (uintptr_t)malloc(72);
        child_env.tb_jmp_cache_ptr = (void *)0x1234;
        errno = EBUSY; /* pthread_create returns an error number, not errno. */
        CHECK(finish_clone(EAGAIN, &child_env) == -1);
        CHECK(allocations == 1 && errno == EAGAIN);
        CHECK(lsenv == parent_lsenv && lsenv->tr_data == parent_data);
        CHECK(parent_gdt[0] == 0x42 && rcu_unregistered == 0);
    }
    CHECK(cpu_unrealized == 64 && gdt_unmapped == 64);
    CHECK(retired_caches == 64 && filter_unref_count == 64);
    CHECK(finish_clone(0, NULL) == 42); /* A successful child stays owned by it. */
    CHECK(allocations == 1 && cpu_unrealized == 64);
    free(parent_gdt);
    puts("clone rollback: child CPU/GDT/cache/filter release, parent ownership, errno passed");
    return 0;
}
'''


CACHE_MAIN = r'''
    return 0;
}
int main(void) {
    CPUArchState child_env = {0};
    char *parent_gdt = malloc(72);
    parent_gdt[0] = 0x42;
    child_env.cpu = malloc(sizeof(CPUState));
    child_env.cpu->opaque = malloc(sizeof(TaskState));
    child_env.cpu->opaque->seccomp_filter = (void *)0x5678;
    child_env.gdt.base = (uintptr_t)malloc(72);
    child_env.tb_jmp_cache_ptr = (void *)0x1234; /* Borrowed from cpu_copy. */
    release_cpu = 1;
    errno = EBUSY;
    CHECK(fail_cache_init(&child_env) == -1 && errno == ENOMEM);
    CHECK(allocations == 1 && parent_gdt[0] == 0x42);
    CHECK(retired_caches == 0); /* The inherited parent cache is not ours. */
    CHECK(cpu_unrealized == 1 && gdt_unmapped == 1 && filter_unref_count == 1);
    free(parent_gdt);
    puts("clone cache allocation failure: child rollback, borrowed parent cache preserved");
    return 0;
}
'''


PUBLISH_PRELUDE = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); \
} } while (0)
#define CLONE_CHILD_CLEARTID 1
#define CLONE_SETTLS 2
typedef struct Filter { struct Filter *previous; int references; } Filter;
typedef struct {
    int bprm, info, signal_mask, ipc_namespace_isolated, child_tidptr;
    Filter *seccomp_filter;
} TaskState;
typedef struct { TaskState *opaque; } CPUState;
typedef struct { CPUState *cpu; } CPUArchState;
static Filter first, second;
static TaskState parent, child;
static CPUState child_cpu;
static bool list_locked, pending;
static int schedule;
static void run_tsync(void) {
    if (!pending) { return; }
    pending = false;
    second.references = 1;
    second.previous = parent.seccomp_filter;
    second.previous->references++;
    TaskState *tasks[] = { &parent, child_cpu.opaque };
    for (int i = 0; i < 2; i++) {
        TaskState *task = tasks[i];
        if (!task) { continue; }
        if (task->seccomp_filter) { task->seccomp_filter->references--; }
        task->seccomp_filter = &second;
        second.references++;
    }
    second.references--;
}
static void cpu_list_lock(void) {
    CHECK(!list_locked);
    if (schedule == 0) { run_tsync(); } /* TSYNC wins the publication lock. */
    list_locked = true;
}
static void cpu_list_unlock(void) {
    CHECK(list_locked && child_cpu.opaque == &child);
    CHECK(child.seccomp_filter == parent.seccomp_filter);
    list_locked = false;
    if (schedule == 1) { run_tsync(); } /* TSYNC follows full publication. */
}
static Filter *guest_seccomp_filter_ref(Filter *filter) {
    if (pending && schedule == 0 && !list_locked) {
        run_tsync(); /* Reproduce a TSYNC racing an unlocked clone root read. */
    }
    if (filter) { filter->references++; }
    return filter;
}
static CPUState *env_cpu(CPUArchState *env) { return env->cpu; }
static void cpu_set_tls(CPUArchState *env, unsigned long tls) {}
static void publish_child(CPUArchState *new_env, TaskState *ts,
                          TaskState *parent_ts) {
    CPUState *new_cpu;
    int flags = 0, child_tidptr = 0;
    unsigned long newtls = 0;
'''

PUBLISH_MAIN = r'''
}
int main(void) {
    for (schedule = 0; schedule < 2; schedule++) {
        memset(&first, 0, sizeof(first));
        memset(&second, 0, sizeof(second));
        memset(&child, 0, sizeof(child));
        parent = (TaskState) { .bprm = 42, .info = 43, .signal_mask = 44,
            .ipc_namespace_isolated = 45, .seccomp_filter = &first };
        first.references = 1;
        child_cpu.opaque = NULL;
        CPUArchState env = { .cpu = &child_cpu };
        pending = true;
        publish_child(&env, &child, &parent);
        CHECK(!pending && !list_locked && child_cpu.opaque == &child);
        CHECK(child.seccomp_filter == &second && parent.seccomp_filter == &second);
        CHECK(first.references == 1 && second.references == 2);
        CHECK(child.bprm == parent.bprm && child.info == parent.info);
        CHECK(child.signal_mask == parent.signal_mask);
        CHECK(child.ipc_namespace_isolated == parent.ipc_namespace_isolated);
    }
    puts("clone publication: TSYNC before/after task publication preserves exact root ownership");
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=pathlib.Path,
                        default=pathlib.Path(__file__).resolve().parents[2])
    parser.add_argument("--revision", help="Read production sources from a git revision")
    parser.add_argument("--case", choices=("all", "thread", "netlink", "clone", "publication"),
                        default="all")
    args = parser.parse_args()
    root = args.repo

    def source(path):
        if args.revision:
            return subprocess.check_output(
                ["git", "-C", str(root), "show", f"{args.revision}:{path}"], text=True)
        return (root / path).read_text()

    syscall = source("linux-user/syscall.c")
    config = source("target/i386/latx/latx-config.c")
    tu = source("target/i386/latx/optimization/tu.c")
    translate = source("accel/tcg/translate-all.c")
    reset = function(syscall, "static void reset_16k_buf(", optional=True)
    set_buffer = function(syscall, "static void set_16k_buf(")
    destroy = function(config, "void latx_lsenv_destroy(", optional=True)
    tu_destroy = function(tu, "void tu_control_destroy(", optional=True)
    # Both syscall exit and seccomp KILL_THREAD must keep the common teardown.
    assert syscall.count("exit_guest_thread_locked(env);") == 2
    cases = {}
    if args.case in ("all", "thread"):
        smc_functions = "".join(function(translate, signature) for signature in (
            "static gint smc_retrans_cmp(", "static inline void smc_retrans_tree_init(",
            "static inline void *smc_retrans_lookup(", "static bool smc_retrans_insert(",
            "static void smc_retrans_destory("))
        smc_functions += function(translate, "void latx_smc_thread_cleanup(", optional=True)
        if "void latx_smc_thread_cleanup(" not in translate:
            smc_functions += "static void latx_smc_thread_cleanup(void) {}\n"
        common = (PRELUDE + reset + set_buffer + smc_functions
                  + function(syscall, "static void cleanup_guest_seccomp(", optional=True))
        tu_functions = "".join(function(tu, signature) for signature in (
            "static gint gpc_cmp(", "static inline void tu_trees_init(",
            "void tu_control_init(")) + tu_destroy
        exit_body = function(syscall, "static void QEMU_NORETURN exit_guest_thread_locked(")
        define_destroy = "#define HAVE_LATX_DESTROY 1\n" if destroy else ""
        cases["thread-no-tu"] = common + define_destroy + destroy + exit_body + MAIN
        cases["thread-tu"] = ("#define CONFIG_LATX_TU 1\n" + common
                              + define_destroy + tu_functions + destroy + exit_body + MAIN)
    if args.case in ("all", "netlink"):
        start = syscall.index("        bool need_fix = buf_need_fix(&msg, fd);")
        end = syscall.index("\n\n        if (!is_error(ret))", start)
        cases["netlink"] = (PRELUDE + reset + set_buffer
                            + function(syscall, "static abi_long get_from_16k_buf(")
                            + NETLINK_STUBS + syscall[start:end] + NETLINK_MAIN)
    if args.case in ("all", "clone"):
        start = syscall.index("        ret = pthread_create(&info.thread,")
        start = syscall.index("        sigprocmask(SIG_SETMASK,", start)
        end = syscall.index("\n    } else {\n        /* if no CLONE_VM", start)
        cases["clone"] = (PRELUDE
                          + function(syscall, "static void cleanup_guest_thread_resources(")
                          + function(syscall, "static void cleanup_guest_seccomp(", optional=True)
                          + function(syscall, "static void cleanup_failed_guest_thread(", optional=True)
                          + CLONE_STUBS + syscall[start:end] + CLONE_MAIN)
        if "new_env->tb_jmp_cache_ptr = NULL;" in syscall:
            start = syscall.index("        new_env->tb_jmp_cache_ptr = NULL;")
            end = syscall.index("\n#endif", start)
            cases["clone-cache"] = (PRELUDE
                + function(syscall, "static void cleanup_guest_thread_resources(")
                + function(syscall, "static void cleanup_guest_seccomp(", optional=True)
                + function(syscall, "static void cleanup_failed_guest_thread(")
                + "static bool latx_fast_jmp_cache_init(CPUArchState *env) {\n"
                  "    CHECK(env->tb_jmp_cache_ptr == NULL); return false;\n}\n"
                  "static int fail_cache_init(CPUArchState *new_env) {\n"
                + syscall[start:end] + CACHE_MAIN)
    if args.case in ("all", "publication"):
        start = syscall.index("        new_cpu = env_cpu(new_env);")
        end = syscall.index("\n#ifdef CONFIG_LATX_FAST_JMPCACHE", start)
        cases["publication"] = PUBLISH_PRELUDE + syscall[start:end] + PUBLISH_MAIN
    cc = shlex.split(os.environ.get("CC", "cc"))
    glib_flags = shlex.split(subprocess.check_output(
        ["pkg-config", "--cflags", "--libs", "glib-2.0"], text=True))
    with tempfile.TemporaryDirectory(prefix="latx-thread-cleanup-") as directory:
        for name, source in cases.items():
            for mode in ([], ["-DNDEBUG"]):
                binary = pathlib.Path(directory) / name
                subprocess.run(cc + ["-std=gnu11", "-O2"] + mode
                               + ["-x", "c", "-", "-o", str(binary)] + glib_flags,
                               input=source, text=True, check=True)
                subprocess.run([str(binary)], cwd=directory, check=True)


if __name__ == "__main__":
    main()
