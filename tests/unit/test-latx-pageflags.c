/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Test actual page metadata bodies, tree operations and RCU reclamation.
 * The lookup barrier deterministically removes a node while a reader holds it;
 * it does not model guest mappings or replace translated runtime tests.
 */
#include "qemu/osdep.h"
#include "qemu/rcu.h"
#include "qemu/thread.h"
#include "qemu/interval-tree.h"

typedef uintptr_t target_ulong;
#define PAGE_READ 1
#define PAGE_WRITE 2
#define PAGE_EXEC 4
#define PAGE_WRITE_ORG 16
#define TEST_PAGE_SIZE 4096
#define rb_to_itree(n) container_of(n, IntervalTreeNode, rb)

static __thread unsigned locks;
static unsigned reclaimed;
static bool pause_lookup;
static QemuEvent found, removed, consumed, release_reader;

static void mmap_lock(void)
{
    locks++;
}

static void mmap_unlock(void)
{
    g_assert(locks);
    locks--;
}

static bool have_mmap_lock(void)
{
    return locks;
}

static void checked_free(void *p)
{
    qatomic_inc(&reclaimed);
    g_free(p);
}

static int page_unprotect(target_ulong start, uintptr_t pc, int *emu)
{
    g_assert_not_reached();
}

static IntervalTreeNode *checked_lookup(IntervalTreeRoot *root,
                                        uint64_t first, uint64_t last)
{
    IntervalTreeNode *node = interval_tree_iter_first(root, first, last);

    if (pause_lookup && !have_mmap_lock()) {
        g_assert(node);
        qemu_event_set(&found);
        qemu_event_wait(&removed);
        g_assert(qatomic_read(&reclaimed) == 0);
    }
    return node;
}

#include "pageflags-test.h"

#define g_free checked_free
#define interval_tree_iter_first checked_lookup
#include "pageflags-test-body.h"
#undef interval_tree_iter_first
#undef g_free

static void check_generated_layout(void)
{
    /* Keep the offsets emitted by gen_test_page_flag() unchanged. */
    g_assert(offsetof(PageFlagsNode, itree.rb.rb_right) == 0x8);
    g_assert(offsetof(PageFlagsNode, itree.rb.rb_left) == 0x10);
    g_assert(offsetof(PageFlagsNode, itree.start) == 0x18);
    g_assert(offsetof(PageFlagsNode, itree.last) == 0x20);
    g_assert(offsetof(PageFlagsNode, itree.subtree_last) == 0x28);
    g_assert(offsetof(PageFlagsNode, flags) == 0x30);
}

static void *reader(void *opaque)
{
    uintptr_t mode = (uintptr_t)opaque;

    rcu_register_thread();
    switch (mode) {
    case 0:
        g_assert(page_get_flags(0x2000) == PAGE_READ);
        break;
    case 1:
        g_assert(get_first_page(0x2000, 0x2fff) == 0x2000);
        break;
    case 2:
        g_assert(page_check_range(0x2000, 4, PAGE_READ));
        break;
    default: {
        /* Model the generated reader inside cpu_exec's RCU section. */
        rcu_read_lock();
        PageFlagsNode *p = pageflags_find(0x2000, 0x2000);
        g_assert(p && p->flags == PAGE_READ);
        qemu_event_set(&consumed);
        qemu_event_wait(&release_reader);
        rcu_read_unlock();
        break;
    }
    }
    qemu_event_set(&consumed);
    rcu_unregister_thread();
    return NULL;
}

static void check_readers(void)
{
    for (uintptr_t mode = 0; mode < 4; mode++) {
        QemuThread thread;

        qatomic_set(&reclaimed, 0);
        mmap_lock();
        pageflags_create(0x2000, 0x2fff, PAGE_READ);
        mmap_unlock();
        pause_lookup = true;
        qemu_thread_create(&thread, "pageflags-reader", reader,
                           (void *)mode, QEMU_THREAD_JOINABLE);
        qemu_event_wait(&found);
        mmap_lock();
        pageflags_unset(0x2000, 0x2fff);
        mmap_unlock();
        g_assert(qatomic_read(&reclaimed) == 0);
        qemu_event_set(&removed);
        qemu_event_wait(&consumed);
        qemu_event_set(&release_reader);
        qemu_thread_join(&thread);
        pause_lookup = false;
        drain_call_rcu();
        g_assert(qatomic_read(&reclaimed) == 1);
        qemu_event_reset(&found);
        qemu_event_reset(&removed);
        qemu_event_reset(&consumed);
        qemu_event_reset(&release_reader);
    }
}

static void *late_reader(void *opaque)
{
    rcu_register_thread();
    rcu_read_lock();
    qemu_event_set(&found);
    qemu_event_wait(&release_reader);
    rcu_read_unlock();
    rcu_unregister_thread();
    return NULL;
}

static void check_new_reader(void)
{
    QemuThread thread;

    qatomic_set(&reclaimed, 0);
    rcu_read_lock();
    mmap_lock();
    pageflags_create(0x1000, 0x1fff, PAGE_READ);
    pageflags_unset(0x1000, 0x1fff);
    mmap_unlock();
    qemu_thread_create(&thread, "new-reader", late_reader, NULL,
                       QEMU_THREAD_JOINABLE);
    qemu_event_wait(&found);
    rcu_read_unlock();
    /* A new peer changes a private retirement into a real grace period. */
    g_assert(qatomic_read(&reclaimed) == 0);
    qemu_event_set(&release_reader);
    qemu_thread_join(&thread);
    drain_call_rcu();
    g_assert(qatomic_read(&reclaimed) == 1);
    qemu_event_reset(&found);
    qemu_event_reset(&release_reader);
}

static void check_mutators(void)
{
    /* Every path which releases a removed node must defer reclamation. */
    qatomic_set(&reclaimed, 0);
    rcu_read_lock();
    mmap_lock();
    pageflags_create(0x1000, 0x1fff, PAGE_READ);
    pageflags_create(0x3000, 0x3fff, PAGE_READ);
    pageflags_create_merge(0x2000, 0x2fff, PAGE_READ);
    g_assert(page_get_flags(0x2000) == PAGE_READ);
    g_assert(get_first_page(0x1000, 0x3fff) == 0x1000);
    g_assert(page_check_range(0x1000, 0x3000, PAGE_READ));
    pageflags_set_clear(0x1000, 0x3fff, 0, PAGE_READ);
    g_assert(page_get_flags(0x2000) == 0);
    pageflags_create(0x1000, 0x1fff, PAGE_READ);
    pageflags_set_clear(0x1000, 0x2fff, 0, PAGE_READ);
    pageflags_create(0x1000, 0x1fff, PAGE_READ);
    pageflags_set_clear(0, 0x2fff, PAGE_WRITE, PAGE_READ);
    g_assert(page_get_flags(0x1000) == PAGE_WRITE);
    pageflags_unset(0, UINTPTR_MAX);
    mmap_unlock();
    g_assert(qatomic_read(&reclaimed) == 0);
    rcu_read_unlock();
    drain_call_rcu();
    g_assert(qatomic_read(&reclaimed) == 5);
    g_assert(interval_tree_is_empty(&pageflags_root));
    g_assert(page_check_range(0, 0, PAGE_READ));
    g_assert(!page_check_range(UINTPTR_MAX, 2, PAGE_READ));
    g_assert(!page_check_range(0x1000, 4, PAGE_READ));
    g_assert(test_flags(0x1000, PAGE_READ));
}

int main(void)
{
    qemu_event_init(&found, false);
    qemu_event_init(&removed, false);
    qemu_event_init(&consumed, false);
    qemu_event_init(&release_reader, false);
    alarm(15);
    check_generated_layout();
    /* The sole guest must not gain a translator-only thread on munmap. */
    g_assert(!rcu_call_thread_is_running());
    mmap_lock();
    pageflags_create(0x1000, 0x1fff, PAGE_READ);
    pageflags_unset(0x1000, 0x1fff);
    mmap_unlock();
    g_assert(reclaimed == 1);
    g_assert(!rcu_call_thread_is_running());
    rcu_read_lock();
    mmap_lock();
    pageflags_create(0x1000, 0x1fff, PAGE_READ);
    pageflags_unset(0x1000, 0x1fff);
    mmap_unlock();
    g_assert(reclaimed == 1);
    rcu_read_unlock();
    g_assert(reclaimed == 2);
    g_assert(!rcu_call_thread_is_running());
    check_new_reader();
    check_readers();
    check_mutators();
    qemu_event_destroy(&found);
    qemu_event_destroy(&removed);
    qemu_event_destroy(&consumed);
    qemu_event_destroy(&release_reader);
    puts("PASS: page metadata lookup lifetime and all removal paths");
    return 0;
}
