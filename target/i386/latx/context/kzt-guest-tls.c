/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <dlfcn.h>
#include <linux/futex.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "box64context.h"
#include "callback.h"
#include "debug.h"
#include "elfloader.h"
#include "kzt-guest-tls.h"
#include "kzt-guest-tls-epoch.h"
#include "myalign.h"
#include "qemu.h"

#define KZT_GUEST_MAX_DTV_ENTRIES 4096
#define KZT_GUEST_MAX_STATIC_TLS_SIZE (16 * 1024 * 1024)
#define KZT_GUEST_MAX_TCB_SIZE (64 * 1024)
#define KZT_GUEST_TLS_TARGET_WAIT_RETRIES 1000
#define KZT_GUEST_ROBUST_LIST_LIMIT 2048

typedef struct kzt_guest_dtv_entry {
    uintptr_t value;
    uintptr_t to_free;
} kzt_guest_dtv_entry_t;

typedef struct kzt_guest_tls_state {
    GRecMutex execution_lock;
    GThread *execution_owner;
    gint execution_depth;
    gint propagation_refs;
    gint destroying;
    void *static_allocation;
    uintptr_t static_start;
    size_t static_size;
    uintptr_t thread_pointer;
    void *dtv_allocation;
    kzt_guest_dtv_entry_t *dtv;
    void *dynamic_allocations[KZT_PUBLIC_LOADER_MAX_OBJECTS];
    unsigned char
        dynamic_guest_owned[KZT_PUBLIC_LOADER_MAX_OBJECTS];
    size_t dynamic_module_ids[KZT_PUBLIC_LOADER_MAX_OBJECTS];
    kzt_public_loader_tls_object_t
        dynamic_objects[KZT_PUBLIC_LOADER_MAX_OBJECTS];
    size_t dynamic_module_count;
    kzt_public_loader_tls_object_t
        tls_inventory[KZT_PUBLIC_LOADER_MAX_OBJECTS];
    size_t tls_inventory_count;
    kzt_public_loader_tls_object_t
        preinitialized_static_objects[KZT_PUBLIC_LOADER_MAX_OBJECTS];
    size_t preinitialized_static_count;
    uintptr_t robust_head_addr;
    uintptr_t robust_prev_addr;
    intptr_t robust_futex_offset;
    int refreshing;
    struct kzt_guest_tls_state *next;
} kzt_guest_tls_state_t;

typedef struct kzt_x86_64_tcbhead {
    uintptr_t tcb;
    kzt_guest_dtv_entry_t *dtv;
    uintptr_t self;
    int multiple_threads;
    int gscope_flag;
    uintptr_t sysinfo;
    uintptr_t stack_guard;
    uintptr_t pointer_guard;
    uintptr_t vgetcpu_cache[2];
    unsigned int feature_1;
    int unused_1;
} kzt_x86_64_tcbhead_t;

typedef struct kzt_guest_parent_tls_snapshot {
    uintptr_t parent_tp;
    size_t tcb_bytes_size;
    size_t dtv_capacity;
    size_t robust_head_offset;
    size_t robust_prev_offset;
    size_t tid_offset;
    intptr_t robust_futex_offset;
    int robust_head_valid;
    unsigned char tcb_bytes[KZT_GUEST_MAX_TCB_SIZE + sizeof(uintptr_t)];
} kzt_guest_parent_tls_snapshot_t;

static uintptr_t guest_uselocale;
static uintptr_t guest_ctype_init;
static uintptr_t guest_get_tls_static_info;
static uintptr_t guest_allocate_tls_init;
static uintptr_t guest_free;
static uintptr_t guest_dlinfo;
static GMutex kzt_guest_tls_states_lock;
static GRecMutex kzt_guest_tls_refresh_lock;
static gsize kzt_guest_tls_refresh_lock_initialized;
static GRWLock kzt_guest_tls_fork_lock;
static gsize kzt_guest_tls_fork_lock_initialized;
static __thread unsigned int kzt_guest_tls_global_execution_depth;
static __thread int kzt_guest_tls_fork_reader_paused;
static __thread int kzt_guest_tls_fork_writer_owned;
static kzt_guest_tls_state_t *kzt_guest_tls_states;
static kzt_public_loader_tls_object_t
    kzt_guest_tls_process_inventory[KZT_PUBLIC_LOADER_MAX_OBJECTS];
static size_t kzt_guest_tls_process_inventory_count;
static uintptr_t kzt_guest_tls_process_generation;
static uintptr_t kzt_guest_tls_pending_propagation_generation;
static int kzt_guest_tls_process_inventory_initialized;
/* Zero disables reuse, odd marks a loader transition, even is stable. */
static gint kzt_guest_tls_loader_sequence;

void kzt_guest_tls_loader_tracking_enable(void)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return;
    }
    (void)g_atomic_int_compare_and_exchange(
        &kzt_guest_tls_loader_sequence, 0, 1);
}

void kzt_guest_tls_loader_tracking_reset(void)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return;
    }
    g_atomic_int_set(&kzt_guest_tls_loader_sequence, 0);
}

void kzt_guest_tls_loader_event_begin(void)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return;
    }
    gint sequence;
    uint32_t dirty;

    do {
        sequence = g_atomic_int_get(&kzt_guest_tls_loader_sequence);
        dirty = kzt_guest_tls_epoch_begin_change((uint32_t)sequence);
        if (!dirty || dirty == (uint32_t)sequence) {
            return;
        }
    } while (!g_atomic_int_compare_and_exchange(
                 &kzt_guest_tls_loader_sequence,
                 sequence, (gint)dirty));
}

static void kzt_guest_tls_publish_stable_epoch(void)
{
    gint sequence;
    uint32_t stable;

    if (qatomic_read(&kzt_guest_tls_pending_propagation_generation) ||
        !kzt_guest_loader_state_is_consistent()) {
        return;
    }
    do {
        sequence = g_atomic_int_get(&kzt_guest_tls_loader_sequence);
        stable = kzt_guest_tls_epoch_publish_stable((uint32_t)sequence);
        if (!stable || stable == (uint32_t)sequence) {
            return;
        }
    } while (!g_atomic_int_compare_and_exchange(
                 &kzt_guest_tls_loader_sequence,
                 sequence, (gint)stable));
}

static int kzt_guest_tls_can_reuse_snapshot(
    const kzt_guest_tls_state_t *state)
{
    uint32_t sequence_before = (uint32_t)g_atomic_int_get(
        &kzt_guest_tls_loader_sequence);
    uintptr_t pending_generation = qatomic_read(
        &kzt_guest_tls_pending_propagation_generation);
    uintptr_t process_generation = qatomic_read(
        &kzt_guest_tls_process_generation);
    int loader_consistent = kzt_guest_loader_state_is_consistent();
    uint32_t sequence_after = (uint32_t)g_atomic_int_get(
        &kzt_guest_tls_loader_sequence);

    return state->dtv && kzt_guest_tls_epoch_can_reuse(
        sequence_before, sequence_after, loader_consistent,
        pending_generation, state->dtv[0].value,
        process_generation);
}

static void kzt_guest_tls_initialize_fork_lock(void)
{
    if (g_once_init_enter(&kzt_guest_tls_fork_lock_initialized)) {
        g_rw_lock_init(&kzt_guest_tls_fork_lock);
        g_once_init_leave(&kzt_guest_tls_fork_lock_initialized, 1);
    }
}

static void kzt_guest_tls_initialize_refresh_lock(void)
{
    if (g_once_init_enter(&kzt_guest_tls_refresh_lock_initialized)) {
        g_rec_mutex_init(&kzt_guest_tls_refresh_lock);
        g_once_init_leave(&kzt_guest_tls_refresh_lock_initialized, 1);
    }
}

void kzt_guest_tls_execution_enter(CPUX86State *env)
{
    kzt_guest_tls_state_t *state;
    GThread *current;

    if (!env || !env->kzt_guest_tls_allocation) {
        return;
    }
    state = env->kzt_guest_tls_allocation;
    current = g_thread_self();
    kzt_guest_tls_initialize_fork_lock();
    if (!kzt_guest_tls_global_execution_depth++) {
        g_rw_lock_reader_lock(&kzt_guest_tls_fork_lock);
    }
    g_rec_mutex_lock(&state->execution_lock);
    g_assert(!g_atomic_int_get(&state->execution_depth) ||
             g_atomic_pointer_get(&state->execution_owner) == current);
    g_atomic_pointer_set(&state->execution_owner, current);
    g_atomic_int_inc(&state->execution_depth);
}

void kzt_guest_tls_execution_leave(CPUX86State *env)
{
    kzt_guest_tls_state_t *state;
    GThread *current;

    if (!env || !env->kzt_guest_tls_allocation) {
        return;
    }
    state = env->kzt_guest_tls_allocation;
    current = g_thread_self();
    g_assert(g_atomic_int_get(&state->execution_depth) > 0 &&
             g_atomic_pointer_get(&state->execution_owner) == current);
    if (g_atomic_int_dec_and_test(&state->execution_depth)) {
        g_atomic_pointer_set(&state->execution_owner, NULL);
    }
    g_rec_mutex_unlock(&state->execution_lock);
    g_assert(kzt_guest_tls_global_execution_depth > 0);
    if (!--kzt_guest_tls_global_execution_depth) {
        g_rw_lock_reader_unlock(&kzt_guest_tls_fork_lock);
    }
}

int kzt_guest_tls_execution_pause(CPUX86State *env)
{
    kzt_guest_tls_state_t *state;
    int execution_depth;

    if (!env || !env->kzt_guest_tls_allocation) {
        return 0;
    }
    kzt_guest_tls_loader_event_begin();
    state = env->kzt_guest_tls_allocation;
    execution_depth = g_atomic_int_get(&state->execution_depth);
    if (!execution_depth ||
        g_atomic_pointer_get(&state->execution_owner) !=
            g_thread_self()) {
        return 0;
    }
    g_atomic_pointer_set(&state->execution_owner, NULL);
    g_atomic_int_set(&state->execution_depth, 0);
    for (int index = 0; index < execution_depth; ++index) {
        g_rec_mutex_unlock(&state->execution_lock);
    }
    g_assert(kzt_guest_tls_global_execution_depth ==
             (unsigned int)execution_depth);
    kzt_guest_tls_global_execution_depth = 0;
    g_rw_lock_reader_unlock(&kzt_guest_tls_fork_lock);
    return execution_depth;
}

void kzt_guest_tls_execution_resume(CPUX86State *env, int paused)
{
    for (int index = 0; index < paused; ++index) {
        kzt_guest_tls_execution_enter(env);
    }
}

static void kzt_guest_tls_fork_lock_writer(void)
{
    kzt_guest_tls_initialize_fork_lock();
    g_assert(!kzt_guest_tls_fork_reader_paused);
    if (kzt_guest_tls_global_execution_depth) {
        g_rw_lock_reader_unlock(&kzt_guest_tls_fork_lock);
        kzt_guest_tls_fork_reader_paused = 1;
    }
    /*
     * Do not queue a blocking writer.  A loader waiter may have paused its
     * read side and must be allowed to reacquire it before that outer Guest
     * execution can drain.  Polling leaves that recovery path runnable.
     */
    while (!g_rw_lock_writer_trylock(&kzt_guest_tls_fork_lock)) {
        g_usleep(1000);
    }
    kzt_guest_tls_fork_writer_owned = 1;
}

void kzt_guest_tls_fork_prepare_early(void)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return;
    }
    g_assert(!kzt_guest_tls_fork_writer_owned);
    kzt_guest_tls_fork_lock_writer();
}

void kzt_guest_tls_fork_prepare(void)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return;
    }
    if (!kzt_guest_tls_fork_writer_owned) {
        kzt_guest_tls_fork_lock_writer();
    }
}

void kzt_guest_tls_fork_parent(void)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return;
    }
    g_assert(kzt_guest_tls_fork_writer_owned);
    g_rw_lock_writer_unlock(&kzt_guest_tls_fork_lock);
    kzt_guest_tls_fork_writer_owned = 0;
    if (kzt_guest_tls_fork_reader_paused) {
        g_rw_lock_reader_lock(&kzt_guest_tls_fork_lock);
        kzt_guest_tls_fork_reader_paused = 0;
    }
}

void kzt_guest_tls_after_fork_child(CPUX86State *env)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return;
    }
    kzt_guest_tls_state_t *current = env
        ? env->kzt_guest_tls_allocation : NULL;
    GThread *thread = g_thread_self();
    int execution_depth = current
        ? g_atomic_int_get(&current->execution_depth) : 0;

    memset(&kzt_guest_tls_fork_lock, 0,
           sizeof(kzt_guest_tls_fork_lock));
    g_rw_lock_init(&kzt_guest_tls_fork_lock);
    kzt_guest_tls_fork_lock_initialized = 1;
    kzt_guest_tls_fork_reader_paused = 0;
    kzt_guest_tls_fork_writer_owned = 0;
    g_atomic_int_set(
        &kzt_guest_tls_loader_sequence,
        (gint)kzt_guest_tls_epoch_after_fork((uint32_t)g_atomic_int_get(
            &kzt_guest_tls_loader_sequence)));
    if (kzt_guest_tls_global_execution_depth) {
        g_rw_lock_reader_lock(&kzt_guest_tls_fork_lock);
    }
    memset(&kzt_guest_tls_states_lock, 0,
           sizeof(kzt_guest_tls_states_lock));
    g_mutex_init(&kzt_guest_tls_states_lock);
    memset(&kzt_guest_tls_refresh_lock, 0,
           sizeof(kzt_guest_tls_refresh_lock));
    g_rec_mutex_init(&kzt_guest_tls_refresh_lock);
    kzt_guest_tls_refresh_lock_initialized = 1;
    if (current) {
        memset(&current->execution_lock, 0,
               sizeof(current->execution_lock));
        g_rec_mutex_init(&current->execution_lock);
        g_atomic_pointer_set(&current->execution_owner,
                             execution_depth ? thread : NULL);
        g_atomic_int_set(&current->execution_depth, 0);
        for (int index = 0; index < execution_depth; ++index) {
            g_rec_mutex_lock(&current->execution_lock);
            g_atomic_int_inc(&current->execution_depth);
        }
        current->next = NULL;
        g_atomic_int_set(&current->propagation_refs, 0);
        g_atomic_int_set(&current->destroying, 0);
        kzt_guest_tls_states = current;
        memcpy(kzt_guest_tls_process_inventory,
               current->tls_inventory,
               current->tls_inventory_count *
                   sizeof(current->tls_inventory[0]));
        kzt_guest_tls_process_inventory_count =
            current->tls_inventory_count;
        qatomic_set(&kzt_guest_tls_process_generation,
                    current->dtv[0].value);
        qatomic_set(&kzt_guest_tls_pending_propagation_generation, 0);
        kzt_guest_tls_process_inventory_initialized = 1;
    } else {
        kzt_guest_tls_states = NULL;
        memset(kzt_guest_tls_process_inventory, 0,
               sizeof(kzt_guest_tls_process_inventory));
        kzt_guest_tls_process_inventory_count = 0;
        qatomic_set(&kzt_guest_tls_process_generation, 0);
        qatomic_set(&kzt_guest_tls_pending_propagation_generation, 0);
        kzt_guest_tls_process_inventory_initialized = 0;
    }
}

static uintptr_t kzt_find_guest_libc_symbol(const char *name)
{
    if (!my_context || !name) {
        return 0;
    }

    for (int i = 0; i < my_context->elfsize; ++i) {
        elfheader_t *head = my_context->elfs[i];
        const char *elf_name;
        const char *base_name;

        if (!head) {
            continue;
        }
        elf_name = ElfName(head);
        if (!elf_name) {
            continue;
        }
        base_name = strrchr(elf_name, '/');
        base_name = base_name ? base_name + 1 : elf_name;
        if (strcmp(base_name, "libc.so.6") != 0 &&
            strncmp(base_name, "libc-", 5) != 0) {
            continue;
        }
        return FindElfSymbolAddress(head, name);
    }
    return 0;
}

static int kzt_guest_tls_resolve_loader(void)
{
    if (!guest_get_tls_static_info) {
        guest_get_tls_static_info =
            kzt_resolve_guest_symbol("_dl_get_tls_static_info");
        if (!guest_get_tls_static_info) {
            guest_get_tls_static_info =
                kzt_resolve_guest_object_symbol(
                    "ld-linux-x86-64.so.2",
                    "_dl_get_tls_static_info");
        }
    }
    if (!guest_allocate_tls_init) {
        guest_allocate_tls_init =
            kzt_resolve_guest_symbol("_dl_allocate_tls_init");
        if (!guest_allocate_tls_init) {
            guest_allocate_tls_init =
                kzt_resolve_guest_object_symbol(
                    "ld-linux-x86-64.so.2",
                    "_dl_allocate_tls_init");
        }
    }
    if (!guest_free) {
        guest_free = kzt_resolve_guest_symbol("free");
        if (!guest_free) {
            guest_free = kzt_find_guest_libc_symbol("free");
        }
    }
    if (!guest_get_tls_static_info || !guest_allocate_tls_init ||
        !guest_free) {
        printf_log(LOG_INFO,
                   "KZT Guest TLS cannot resolve loader helpers: "
                   "static_info=%p allocate_init=%p free=%p\n",
                   (void *)guest_get_tls_static_info,
                   (void *)guest_allocate_tls_init,
                   (void *)guest_free);
        return -1;
    }
    return 0;
}

int kzt_guest_tls_snapshot_parent(CPUX86State *parent,
                                  CPUX86State *child)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return -1;
    }
    kzt_guest_parent_tls_snapshot_t *snapshot;
    kzt_x86_64_tcbhead_t *tcb;
    kzt_guest_dtv_entry_t *capacity_entry;
    uintptr_t parent_tp;
    struct robust_list_head *registered_head = NULL;
    size_t registered_length = 0;
    size_t offset;

    if (!parent || !child || !parent->segs[R_FS].base) {
        return -1;
    }
    if (kzt_guest_tls_resolve_loader() != 0) {
        return -1;
    }
    parent_tp = parent->segs[R_FS].base;
    snapshot = g_new0(kzt_guest_parent_tls_snapshot_t, 1);
    snapshot->parent_tp = parent_tp;
    for (offset = 0; offset < sizeof(snapshot->tcb_bytes);
         offset += sizeof(uintptr_t)) {
        uintptr_t *word = lock_user(
            VERIFY_READ, (abi_ulong)(parent_tp + offset),
            sizeof(*word), 1);

        if (!word) {
            break;
        }
        memcpy(snapshot->tcb_bytes + offset, word, sizeof(*word));
        unlock_user(word, (abi_ulong)(parent_tp + offset), 0);
        snapshot->tcb_bytes_size = offset + sizeof(*word);
    }
    if (snapshot->tcb_bytes_size < sizeof(kzt_x86_64_tcbhead_t)) {
        g_free(snapshot);
        return -1;
    }
    tcb = (kzt_x86_64_tcbhead_t *)snapshot->tcb_bytes;
    if (!tcb->dtv) {
        g_free(snapshot);
        return -1;
    }
    capacity_entry = lock_user(
        VERIFY_READ,
        (abi_ulong)((uintptr_t)tcb->dtv - sizeof(*capacity_entry)),
        sizeof(*capacity_entry), 1);
    if (!capacity_entry) {
        g_free(snapshot);
        return -1;
    }
    snapshot->dtv_capacity = capacity_entry->value;
    unlock_user(
        capacity_entry,
        (abi_ulong)((uintptr_t)tcb->dtv - sizeof(*capacity_entry)), 0);
    if (snapshot->dtv_capacity > KZT_GUEST_MAX_DTV_ENTRIES) {
        g_free(snapshot);
        return -1;
    }
    if (syscall(SYS_get_robust_list, 0, &registered_head,
                &registered_length) == 0 &&
        registered_head &&
        registered_length == sizeof(*registered_head) &&
        (uintptr_t)registered_head >= parent_tp) {
        size_t head_offset =
            (uintptr_t)registered_head - parent_tp;

        if (head_offset >= sizeof(uintptr_t) &&
            head_offset <= snapshot->tcb_bytes_size -
                               sizeof(*registered_head)) {
            struct robust_list_head *head =
                (struct robust_list_head *)(snapshot->tcb_bytes +
                                            head_offset);
            uintptr_t robust_prev;
            uint32_t current_tid = (uint32_t)syscall(SYS_gettid);
            size_t tid_offset = 0;
            size_t tid_matches = 0;

            memcpy(&robust_prev,
                   snapshot->tcb_bytes + head_offset -
                       sizeof(robust_prev),
                   sizeof(robust_prev));
            for (size_t candidate =
                     head_offset > 64 ? head_offset - 64 : 0;
                 candidate + sizeof(current_tid) <= head_offset;
                 candidate += sizeof(current_tid)) {
                uint32_t candidate_tid;

                memcpy(&candidate_tid,
                       snapshot->tcb_bytes + candidate,
                       sizeof(candidate_tid));
                if (candidate_tid == current_tid) {
                    tid_offset = candidate;
                    ++tid_matches;
                }
            }
            if ((uintptr_t)head->list.next ==
                    (uintptr_t)registered_head &&
                !head->list_op_pending &&
                robust_prev == (uintptr_t)registered_head &&
                current_tid && tid_matches == 1 &&
                head->futex_offset != INTPTR_MIN) {
                snapshot->robust_head_offset = head_offset;
                snapshot->robust_prev_offset =
                    head_offset - sizeof(robust_prev);
                snapshot->robust_futex_offset =
                    head->futex_offset;
                snapshot->tid_offset = tid_offset;
                snapshot->robust_head_valid = 1;
            }
        }
    }
    if (!snapshot->robust_head_valid) {
        g_free(snapshot);
        return -1;
    }
    child->kzt_guest_tls_parent_snapshot = snapshot;
    return 0;
}

int kzt_guest_tls_clone_parent_snapshot(const CPUX86State *parent,
                                        CPUX86State *child)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return -1;
    }
    const kzt_guest_parent_tls_snapshot_t *parent_snapshot;

    if (!parent || !child || child->kzt_guest_tls_parent_snapshot) {
        return -1;
    }
    parent_snapshot = parent->kzt_guest_tls_parent_snapshot;
    if (!parent_snapshot) {
        return -1;
    }
    child->kzt_guest_tls_parent_snapshot =
        g_new(kzt_guest_parent_tls_snapshot_t, 1);
    memcpy(child->kzt_guest_tls_parent_snapshot, parent_snapshot,
           sizeof(*parent_snapshot));
    return 0;
}

static void kzt_guest_tls_release_parent_snapshot(CPUX86State *env)
{
    if (env) {
        g_free(env->kzt_guest_tls_parent_snapshot);
        env->kzt_guest_tls_parent_snapshot = NULL;
    }
}

static size_t kzt_guest_tls_find_tcb_size(
    const kzt_guest_parent_tls_snapshot_t *snapshot,
    size_t static_size,
    size_t static_align)
{
    size_t limit = static_size < KZT_GUEST_MAX_TCB_SIZE
                       ? static_size : KZT_GUEST_MAX_TCB_SIZE;
    size_t match = 0;

    for (size_t candidate = sizeof(kzt_x86_64_tcbhead_t);
         candidate <= limit; candidate += sizeof(uintptr_t)) {
        uintptr_t allocation;
        uintptr_t aligned;

        if (candidate + sizeof(allocation) >
            snapshot->tcb_bytes_size) {
            break;
        }
        memcpy(&allocation, snapshot->tcb_bytes + candidate,
               sizeof(allocation));
        if (!allocation ||
            allocation > UINTPTR_MAX - (static_align - 1)) {
            continue;
        }
        aligned = (allocation + static_align - 1) &
                  ~(uintptr_t)(static_align - 1);
        if (aligned <= snapshot->parent_tp && static_size >= candidate &&
            snapshot->parent_tp - aligned == static_size - candidate) {
            if (match) {
                return 0;
            }
            match = candidate;
        }
    }
    return match;
}

static void kzt_guest_tls_release_loader_state(
    kzt_guest_tls_state_t *state)
{
    if (!state) {
        return;
    }
    g_free(state->dtv_allocation);
    state->dtv_allocation = NULL;
    state->dtv = NULL;
}

static int kzt_guest_tls_same_object_ignoring_module(
    const kzt_public_loader_tls_object_t *left,
    const kzt_public_loader_tls_object_t *right)
{
    return left->link_map_addr == right->link_map_addr &&
           left->load_bias == right->load_bias &&
           left->dynamic_addr == right->dynamic_addr &&
           left->image_addr == right->image_addr &&
           left->file_size == right->file_size &&
           left->memory_size == right->memory_size &&
           left->alignment == right->alignment &&
           left->first_byte_offset == right->first_byte_offset &&
           left->static_tls_offset == right->static_tls_offset &&
           left->static_tls_symbol_value ==
               right->static_tls_symbol_value &&
           left->static_tls_symbol_name_addr ==
               right->static_tls_symbol_name_addr &&
           left->load_generation == right->load_generation &&
           left->static_tls_offset_valid ==
               right->static_tls_offset_valid &&
           left->static_tls_offset_needs_validation ==
               right->static_tls_offset_needs_validation &&
           left->static_tls_offset_pending ==
               right->static_tls_offset_pending;
}

static int kzt_guest_tls_same_object(
    const kzt_public_loader_tls_object_t *left,
    const kzt_public_loader_tls_object_t *right)
{
    return kzt_guest_tls_same_object_ignoring_module(left, right) &&
           left->module_id == right->module_id;
}

static int kzt_guest_tls_find_dynamic_index(
    const kzt_guest_tls_state_t *state,
    size_t module_id)
{
    for (size_t index = 0; index < state->dynamic_module_count; ++index) {
        if (state->dynamic_module_ids[index] == module_id) {
            return (int)index;
        }
    }
    return -1;
}

static void kzt_guest_tls_free_dynamic(
    kzt_guest_tls_state_t *state, size_t index)
{
    void *allocation = state->dynamic_allocations[index];

    if (!allocation) {
        return;
    }
    if (state->dynamic_guest_owned[index]) {
        (void)RunFunctionWithStateInternalNoRefresh(
            guest_free, 1, (uint64_t)(uintptr_t)allocation);
    } else {
        g_free(allocation);
    }
    state->dynamic_allocations[index] = NULL;
    state->dynamic_module_ids[index] = 0;
    state->dynamic_guest_owned[index] = 0;
    memset(&state->dynamic_objects[index], 0,
           sizeof(state->dynamic_objects[index]));
}

static void kzt_guest_tls_retire_dynamic(
    kzt_guest_tls_state_t *state,
    const kzt_public_loader_tls_object_t *tls_objects,
    size_t tls_object_count)
{
    for (size_t allocation_index = 0;
         allocation_index < state->dynamic_module_count;
         ++allocation_index) {
        size_t module_id = state->dynamic_module_ids[allocation_index];
        int found = 0;

        if (!module_id ||
            !state->dynamic_allocations[allocation_index]) {
            continue;
        }
        for (size_t object_index = 0;
             object_index < tls_object_count; ++object_index) {
            if (tls_objects[object_index].module_id == module_id &&
                kzt_guest_tls_same_object(
                    &tls_objects[object_index],
                    &state->dynamic_objects[allocation_index])) {
                found = 1;
                break;
            }
        }
        if (found) {
            continue;
        }
        kzt_guest_tls_free_dynamic(state, allocation_index);
        if (module_id <= KZT_GUEST_MAX_DTV_ENTRIES) {
            state->dtv[module_id].value = 0;
            state->dtv[module_id].to_free = 0;
        }
    }
}

static int kzt_guest_tls_inventory_matches(
    const kzt_guest_tls_state_t *state,
    const kzt_public_loader_tls_object_t *objects,
    size_t object_count)
{
    if (state->tls_inventory_count != object_count) {
        return 0;
    }

    for (size_t index = 0; index < object_count; ++index) {
        const kzt_public_loader_tls_object_t *previous =
            &state->tls_inventory[index];
        const kzt_public_loader_tls_object_t *current = &objects[index];

        if (previous->image_addr != current->image_addr ||
            previous->link_map_addr != current->link_map_addr ||
            previous->load_bias != current->load_bias ||
            previous->dynamic_addr != current->dynamic_addr ||
            previous->file_size != current->file_size ||
            previous->memory_size != current->memory_size ||
            previous->alignment != current->alignment ||
            previous->first_byte_offset !=
                current->first_byte_offset ||
            previous->static_tls_offset !=
                current->static_tls_offset ||
            previous->static_tls_symbol_value !=
                current->static_tls_symbol_value ||
            previous->static_tls_symbol_name_addr !=
                current->static_tls_symbol_name_addr ||
            previous->module_id != current->module_id ||
            previous->load_generation != current->load_generation ||
            previous->static_tls_offset_valid !=
                current->static_tls_offset_valid ||
            previous->static_tls_offset_needs_validation !=
                current->static_tls_offset_needs_validation ||
            previous->static_tls_offset_pending !=
                current->static_tls_offset_pending) {
            return 0;
        }
    }
    return 1;
}

static int kzt_guest_tls_inventory_has_additions(
    const kzt_guest_tls_state_t *state,
    const kzt_public_loader_tls_object_t *objects,
    size_t object_count)
{
    for (size_t index = 0; index < object_count; ++index) {
        int found = 0;

        for (size_t previous = 0;
             previous < state->tls_inventory_count; ++previous) {
            if (kzt_guest_tls_same_object(
                    &objects[index],
                    &state->tls_inventory[previous])) {
                found = 1;
                break;
            }
        }
        if (!found) {
            return 1;
        }
    }
    return 0;
}

static int kzt_guest_tls_process_inventory_matches(
    const kzt_public_loader_tls_object_t *objects,
    size_t object_count)
{
    if (!kzt_guest_tls_process_inventory_initialized ||
        kzt_guest_tls_process_inventory_count != object_count) {
        return 0;
    }
    for (size_t index = 0; index < object_count; ++index) {
        if (!kzt_guest_tls_same_object(
                &kzt_guest_tls_process_inventory[index],
                &objects[index])) {
            return 0;
        }
    }
    return 1;
}

static int kzt_guest_tls_update_process_inventory(
    const kzt_public_loader_tls_object_t *objects,
    size_t object_count)
{
    if (kzt_guest_tls_process_inventory_matches(
            objects, object_count)) {
        return 0;
    }
    if (!kzt_guest_tls_process_inventory_initialized ||
        qatomic_read(&kzt_guest_tls_process_generation) == UINTPTR_MAX) {
        return -1;
    }
    qatomic_inc(&kzt_guest_tls_process_generation);
    memcpy(kzt_guest_tls_process_inventory, objects,
           object_count * sizeof(*objects));
    kzt_guest_tls_process_inventory_count = object_count;
    return 1;
}

static int kzt_guest_tls_restore_known_module_id(
    const kzt_guest_tls_state_t *state,
    kzt_public_loader_tls_object_t *object)
{
    for (size_t index = 0; index < state->tls_inventory_count; ++index) {
        const kzt_public_loader_tls_object_t *previous =
            &state->tls_inventory[index];

        if (previous->module_id &&
            previous->link_map_addr == object->link_map_addr &&
            previous->load_bias == object->load_bias &&
            previous->dynamic_addr == object->dynamic_addr &&
            previous->image_addr == object->image_addr &&
            previous->file_size == object->file_size &&
            previous->memory_size == object->memory_size &&
            previous->alignment == object->alignment &&
            previous->first_byte_offset ==
                object->first_byte_offset &&
            previous->static_tls_offset ==
                object->static_tls_offset &&
            previous->static_tls_symbol_value ==
                object->static_tls_symbol_value &&
            previous->static_tls_symbol_name_addr ==
                object->static_tls_symbol_name_addr &&
            previous->load_generation == object->load_generation &&
            previous->static_tls_offset_valid ==
                object->static_tls_offset_valid &&
            previous->static_tls_offset_needs_validation ==
                object->static_tls_offset_needs_validation &&
            previous->static_tls_offset_pending ==
                object->static_tls_offset_pending) {
            object->module_id = previous->module_id;
            return 1;
        }
    }
    return 0;
}

static int kzt_guest_tls_resolve_new_module_ids(
    const kzt_guest_tls_state_t *state,
    kzt_public_loader_tls_object_t *objects,
    size_t object_count,
    int static_only)
{
    /*
     * A default-visible TLS definition may be interposed, so the static
     * relocation scan deliberately cannot attribute its DTPMOD64 value to
     * the defining object.  Once dlopen has returned, ask the Guest loader
     * for the authoritative module ID of each newly observed TLS object.
     */
    for (size_t index = 0; index < object_count; ++index) {
        size_t module_id = 0;
        uint64_t result;

        if (static_only &&
            !objects[index].static_tls_offset_valid) {
            continue;
        }
        if (objects[index].module_id ||
            kzt_guest_tls_restore_known_module_id(
                state, &objects[index])) {
            continue;
        }
        if (!guest_dlinfo) {
            guest_dlinfo = kzt_resolve_guest_symbol("dlinfo");
        }
        if (!guest_dlinfo) {
            guest_dlinfo = kzt_resolve_guest_symbol("__dlinfo");
        }
        if (!guest_dlinfo && my_context && my_context->dlprivate) {
            guest_dlinfo = (uintptr_t)my_context->dlprivate->x86dlinfo;
        }
        if (!guest_dlinfo) {
            fprintf(stderr,
                    "KZT Guest TLS requires a loaded Guest dlinfo "
                    "provider to resolve TLS module IDs (object %p)\n",
                    (void *)objects[index].link_map_addr);
            return -1;
        }
        result = RunFunctionWithStateInternalNoRefresh(
            guest_dlinfo, 3, objects[index].link_map_addr,
            RTLD_DI_TLS_MODID, (uint64_t)(uintptr_t)&module_id);
        if (result != 0 || !module_id ||
            module_id > KZT_GUEST_MAX_DTV_ENTRIES) {
            printf_log(LOG_INFO,
                       "KZT Guest TLS cannot query module ID for "
                       "late object %p\n",
                       (void *)objects[index].link_map_addr);
            return -1;
        }
        objects[index].module_id = module_id;
    }
    return 0;
}

static int kzt_guest_tls_validate_inventory(
    const kzt_public_loader_tls_object_t *objects,
    size_t object_count)
{
    for (size_t index = 0; index < object_count; ++index) {
        if (objects[index].module_id > KZT_GUEST_MAX_DTV_ENTRIES) {
            printf_log(LOG_INFO,
                       "KZT Guest TLS object %p has unsupported "
                       "module ID %zu\n",
                       (void *)objects[index].link_map_addr,
                       objects[index].module_id);
            return -1;
        }
    }
    return 0;
}

static int kzt_guest_tls_validate_refresh_inventory(
    const kzt_guest_tls_state_t *state,
    const kzt_public_loader_tls_object_t *objects,
    size_t object_count)
{
    if (kzt_guest_tls_validate_inventory(objects, object_count) != 0) {
        return -1;
    }
    for (size_t index = 0; index < object_count; ++index) {
        int found = 0;

        if (objects[index].module_id) {
            continue;
        }
        for (size_t previous_index = 0;
             previous_index < state->tls_inventory_count;
             ++previous_index) {
            if (kzt_guest_tls_same_object(
                    &objects[index],
                    &state->tls_inventory[previous_index])) {
                found = 1;
                break;
            }
        }
        if (!found) {
            printf_log(LOG_INFO,
                       "KZT Guest TLS cannot identify late object %p\n",
                       (void *)objects[index].link_map_addr);
            return -1;
        }
    }
    return 0;
}

static int kzt_guest_tls_initialize_static_module(
    const kzt_public_loader_tls_object_t *object,
    const kzt_guest_dtv_entry_t *entry)
{
    void *destination;
    void *image = NULL;

    if (!object || !entry || !entry->value ||
        entry->value == UINTPTR_MAX || !object->memory_size) {
        return -1;
    }
    if (object->file_size) {
        image = g_malloc(object->file_size);
        if (kzt_materialize_guest_tls_image(
                object, image, object->file_size) != 0) {
            g_free(image);
            return -1;
        }
    }
    destination = lock_user(
        VERIFY_WRITE, (abi_ulong)entry->value,
        object->memory_size, 0);
    if (!destination) {
        g_free(image);
        return -1;
    }
    memset(destination, 0, object->memory_size);
    if (image) {
        memcpy(destination, image, object->file_size);
        g_free(image);
    }
    unlock_user(destination, (abi_ulong)entry->value,
                object->memory_size);
    return 0;
}

static int kzt_guest_tls_has_object(
    const kzt_public_loader_tls_object_t *objects,
    size_t object_count,
    const kzt_public_loader_tls_object_t *object)
{
    for (size_t index = 0; index < object_count; ++index) {
        if (kzt_guest_tls_same_object_ignoring_module(
                &objects[index], object)) {
            return 1;
        }
    }
    return 0;
}

static int kzt_guest_tls_static_address(
    const kzt_guest_tls_state_t *state,
    const kzt_public_loader_tls_object_t *object,
    uintptr_t *address)
{
    uintptr_t magnitude;

    if (!state || !object || !address ||
        !object->static_tls_offset_valid ||
        object->static_tls_offset >= 0 ||
        object->static_tls_offset == INTPTR_MIN) {
        return -1;
    }
    magnitude = (uintptr_t)-object->static_tls_offset;
    if (state->thread_pointer < magnitude) {
        return -1;
    }
    *address = state->thread_pointer - magnitude;
    if (state->static_start > UINTPTR_MAX - state->static_size ||
        *address < state->static_start ||
        object->memory_size > state->static_size ||
        *address > state->static_start + state->static_size -
                       object->memory_size) {
        return -1;
    }
    return 0;
}

static int kzt_guest_tls_validate_static_offset(
    const kzt_public_loader_tls_object_t *object)
{
    if (!object->static_tls_offset_needs_validation) {
        return 0;
    }
    printf_log(LOG_INFO,
               "KZT Guest TLS cannot prove ownership of static "
               "offset for object %p\n",
               (void *)object->link_map_addr);
    return -1;
}

static int kzt_guest_tls_lock_target(
    kzt_guest_tls_state_t *target,
    const kzt_guest_tls_state_t *source,
    int wait)
{
    if (target == source) {
        return 1;
    }
    for (int attempt = 0;
         attempt < (wait ? KZT_GUEST_TLS_TARGET_WAIT_RETRIES : 1);
         ++attempt) {
        if (g_rec_mutex_trylock(&target->execution_lock)) {
            return 1;
        }
        if (wait) {
            g_usleep(1000);
        }
    }
    return 0;
}

static void kzt_guest_tls_unlock_target(
    kzt_guest_tls_state_t *target,
    const kzt_guest_tls_state_t *source)
{
    if (target != source) {
        g_rec_mutex_unlock(&target->execution_lock);
    }
}

static int kzt_guest_tls_initialize_pending_static(
    kzt_guest_tls_state_t *source,
    const kzt_public_loader_tls_object_t *objects,
    size_t object_count,
    int complete_inventory,
    uintptr_t generation)
{
    if (complete_inventory) {
        size_t retained = 0;

        for (size_t index = 0;
             index < source->preinitialized_static_count; ++index) {
            if (kzt_guest_tls_has_object(
                    objects, object_count,
                    &source->preinitialized_static_objects[index])) {
                source->preinitialized_static_objects[retained++] =
                    source->preinitialized_static_objects[index];
            }
        }
        source->preinitialized_static_count = retained;
    }

    for (size_t index = 0; index < object_count; ++index) {
        const kzt_public_loader_tls_object_t *object = &objects[index];
        kzt_guest_dtv_entry_t entry;
        uintptr_t address;

        if (!object->static_tls_offset_valid ||
            kzt_guest_tls_has_object(
                source->tls_inventory, source->tls_inventory_count,
                object) ||
            kzt_guest_tls_has_object(
                source->preinitialized_static_objects,
                source->preinitialized_static_count, object)) {
            continue;
        }
        if (kzt_guest_tls_validate_static_offset(object) != 0) {
            return -1;
        }
        if (source->preinitialized_static_count ==
                KZT_PUBLIC_LOADER_MAX_OBJECTS ||
            kzt_guest_tls_static_address(
                source, object, &address) != 0) {
            return -1;
        }
        entry = (kzt_guest_dtv_entry_t) {
            .value = address,
            .to_free = 0,
        };
        if (kzt_guest_tls_initialize_static_module(
                object, &entry) != 0) {
            return -1;
        }
        if (object->module_id) {
            source->dtv[object->module_id] = entry;
        }
        if (generation) {
            source->dtv[0].value = generation;
        }
        source->preinitialized_static_objects[
            source->preinitialized_static_count++] = *object;
    }
    return 0;
}

static int kzt_guest_tls_adopt_dynamic(
    kzt_guest_tls_state_t *state,
    const kzt_public_loader_tls_object_t *object,
    kzt_guest_dtv_entry_t *entry)
{
    int dynamic_index = kzt_guest_tls_find_dynamic_index(
        state, object->module_id);

    if (!entry->to_free) {
        return -1;
    }
    if (dynamic_index >= 0 &&
        (state->dynamic_allocations[dynamic_index] !=
             (void *)entry->to_free ||
         !kzt_guest_tls_same_object(
             &state->dynamic_objects[dynamic_index], object))) {
        kzt_guest_tls_free_dynamic(state, dynamic_index);
    }
    if (dynamic_index < 0 ||
        !state->dynamic_allocations[dynamic_index]) {
        if (dynamic_index < 0) {
            for (size_t candidate = 0;
                 candidate < state->dynamic_module_count; ++candidate) {
                if (!state->dynamic_allocations[candidate]) {
                    dynamic_index = (int)candidate;
                    break;
                }
            }
        }
        if (dynamic_index < 0) {
            if (state->dynamic_module_count ==
                KZT_PUBLIC_LOADER_MAX_OBJECTS) {
                return -1;
            }
            dynamic_index = (int)state->dynamic_module_count++;
        }
    }
    state->dynamic_allocations[dynamic_index] =
        (void *)entry->to_free;
    state->dynamic_module_ids[dynamic_index] = object->module_id;
    state->dynamic_objects[dynamic_index] = *object;
    state->dynamic_guest_owned[dynamic_index] = 1;
    entry->to_free = 0;
    return 0;
}

static void kzt_guest_tls_remember_inventory(
    kzt_guest_tls_state_t *state,
    const kzt_public_loader_tls_object_t *objects,
    size_t object_count)
{
    memcpy(state->tls_inventory, objects,
           object_count * sizeof(*objects));
    state->tls_inventory_count = object_count;
}

static int kzt_guest_tls_populate_dynamic_object(
    kzt_guest_tls_state_t *state,
    const kzt_public_loader_tls_object_t *object)
{
    kzt_guest_dtv_entry_t *entry;
    void *allocation;
    void *tls_address;
    int dynamic_index;
    size_t alignment;
    size_t allocation_size;
    size_t address_offset;

    if (!object->module_id ||
        object->module_id > KZT_GUEST_MAX_DTV_ENTRIES) {
        return -1;
    }
    entry = &state->dtv[object->module_id];
    if (entry->to_free) {
        return kzt_guest_tls_adopt_dynamic(state, object, entry);
    }
    if (entry->value != UINTPTR_MAX) {
        return 0;
    }
    dynamic_index = kzt_guest_tls_find_dynamic_index(
        state, object->module_id);
    allocation = dynamic_index >= 0
                     ? state->dynamic_allocations[dynamic_index]
                     : NULL;
    if (allocation &&
        !kzt_guest_tls_same_object(
            &state->dynamic_objects[dynamic_index], object)) {
        kzt_guest_tls_free_dynamic(state, dynamic_index);
        allocation = NULL;
    }
    alignment = object->alignment < sizeof(void *)
                    ? sizeof(void *) : object->alignment;
    if (!allocation) {
        if (object->memory_size > SIZE_MAX - (alignment - 1)) {
            return -1;
        }
        allocation_size = object->memory_size + alignment - 1;
        allocation = g_try_malloc(allocation_size);
        if (!allocation) {
            return -1;
        }
        address_offset =
            (object->first_byte_offset -
             ((uintptr_t)allocation & (alignment - 1))) &
            (alignment - 1);
        tls_address = (unsigned char *)allocation + address_offset;
        memset(tls_address, 0, object->memory_size);
        if (object->file_size &&
            kzt_materialize_guest_tls_image(
                object, tls_address, object->file_size) != 0) {
            g_free(allocation);
            return -1;
        }
        if (dynamic_index < 0) {
            for (size_t candidate = 0;
                 candidate < state->dynamic_module_count;
                 ++candidate) {
                if (!state->dynamic_allocations[candidate]) {
                    dynamic_index = (int)candidate;
                    break;
                }
            }
            if (dynamic_index < 0) {
                if (state->dynamic_module_count ==
                    KZT_PUBLIC_LOADER_MAX_OBJECTS) {
                    g_free(allocation);
                    return -1;
                }
                dynamic_index = (int)state->dynamic_module_count++;
            }
        }
        state->dynamic_allocations[dynamic_index] = allocation;
        state->dynamic_module_ids[dynamic_index] = object->module_id;
        state->dynamic_objects[dynamic_index] = *object;
        state->dynamic_guest_owned[dynamic_index] = 0;
    } else {
        address_offset =
            (object->first_byte_offset -
             ((uintptr_t)allocation & (alignment - 1))) &
            (alignment - 1);
        tls_address = (unsigned char *)allocation + address_offset;
    }
    entry->value = (uintptr_t)tls_address;
    entry->to_free = 0;
    return 0;
}

static int kzt_guest_tls_populate_dynamic(
    kzt_guest_tls_state_t *state,
    const kzt_public_loader_tls_object_t *tls_objects,
    size_t tls_object_count)
{
    size_t dynamic_slot_count = 0;
    size_t known_dynamic_count = 0;

    for (size_t previous_index = 0;
         previous_index < state->tls_inventory_count;
         ++previous_index) {
        const kzt_public_loader_tls_object_t *previous =
            &state->tls_inventory[previous_index];

        if (previous->module_id &&
            previous->module_id <= KZT_GUEST_MAX_DTV_ENTRIES &&
            !kzt_guest_tls_has_object(
                tls_objects, tls_object_count, previous)) {
            state->dtv[previous->module_id].value = 0;
            state->dtv[previous->module_id].to_free = 0;
        }
    }
    kzt_guest_tls_retire_dynamic(
        state, tls_objects, tls_object_count);
    for (size_t index = 0; index < tls_object_count; ++index) {
        size_t module_id = tls_objects[index].module_id;

        if (module_id && module_id <= KZT_GUEST_MAX_DTV_ENTRIES &&
            state->dtv[module_id].value == 0) {
            state->dtv[module_id].value = UINTPTR_MAX;
        }
    }
    for (size_t index = 1; index <= KZT_GUEST_MAX_DTV_ENTRIES; ++index) {
        if (state->dtv[index].value == UINTPTR_MAX) {
            int live = 0;

            for (size_t object_index = 0;
                 object_index < tls_object_count; ++object_index) {
                if (tls_objects[object_index].module_id == index) {
                    live = 1;
                    break;
                }
            }
            if (live) {
                ++dynamic_slot_count;
            } else {
                state->dtv[index].value = 0;
                state->dtv[index].to_free = 0;
            }
        }
    }
    for (size_t index = 0; index < tls_object_count; ++index) {
        const kzt_public_loader_tls_object_t *object = &tls_objects[index];

        if (object->module_id &&
            object->module_id <= KZT_GUEST_MAX_DTV_ENTRIES &&
            state->dtv[object->module_id].value == UINTPTR_MAX) {
            ++known_dynamic_count;
        }
    }
    if (known_dynamic_count != dynamic_slot_count) {
        return -1;
    }

    for (size_t index = 0; index < tls_object_count; ++index) {
        const kzt_public_loader_tls_object_t *object = &tls_objects[index];

        if (!object->module_id ||
            object->module_id > KZT_GUEST_MAX_DTV_ENTRIES ||
            (state->dtv[object->module_id].value != UINTPTR_MAX &&
             !state->dtv[object->module_id].to_free)) {
            continue;
        }
        if (kzt_guest_tls_populate_dynamic_object(
                state, object) != 0) {
            return -1;
        }
    }
    return 0;
}

static int kzt_guest_tls_propagate_inventory(
    kzt_guest_tls_state_t *source,
    const kzt_public_loader_tls_object_t *tls_objects,
    size_t tls_object_count,
    uintptr_t generation)
{
    kzt_guest_tls_state_t
        *targets[KZT_PUBLIC_LOADER_MAX_OBJECTS];
    size_t target_count = 0;
    int result = 0;
    int deferred = 0;

    g_mutex_lock(&kzt_guest_tls_states_lock);
    for (kzt_guest_tls_state_t *target = kzt_guest_tls_states;
         target; target = target->next) {
        if (target == source ||
            g_atomic_int_get(&target->destroying)) {
            continue;
        }
        if (target_count == KZT_PUBLIC_LOADER_MAX_OBJECTS) {
            result = -1;
            break;
        }
        g_atomic_int_inc(&target->propagation_refs);
        targets[target_count++] = target;
    }
    g_mutex_unlock(&kzt_guest_tls_states_lock);

    for (size_t index = 0; index < target_count; ++index) {
        kzt_guest_tls_state_t *target = targets[index];

        if (result != 0) {
            g_atomic_int_dec_and_test(&target->propagation_refs);
            continue;
        }
        if (!kzt_guest_tls_lock_target(target, source, 1)) {
            /* The target will apply the pending generation on next entry. */
            deferred = 1;
            g_atomic_int_dec_and_test(&target->propagation_refs);
            continue;
        }
        if (target->dtv[0].value >= generation) {
            kzt_guest_tls_unlock_target(target, source);
            g_atomic_int_dec_and_test(&target->propagation_refs);
            continue;
        }
        mmap_lock();
        if (kzt_guest_tls_initialize_pending_static(
                target, tls_objects, tls_object_count, 1,
                generation) != 0 ||
            kzt_guest_tls_populate_dynamic(
                target, tls_objects, tls_object_count) != 0) {
            fprintf(stderr,
                    "KZT Guest TLS propagation could not initialize "
                    "target %p\n", (void *)target);
            result = -1;
        } else {
            target->dtv[0].value = generation;
            kzt_guest_tls_remember_inventory(
                target, tls_objects, tls_object_count);
        }
        mmap_unlock();
        kzt_guest_tls_unlock_target(target, source);
        g_atomic_int_dec_and_test(&target->propagation_refs);
    }
    return result != 0 ? result : deferred;
}

static int kzt_guest_tls_clone_static(CPUX86State *parent,
                                      CPUX86State *child)
{
    kzt_public_loader_tls_object_t
        tls_objects[KZT_PUBLIC_LOADER_MAX_OBJECTS];
    kzt_guest_tls_state_t *state = NULL;
    kzt_guest_parent_tls_snapshot_t *parent_snapshot;
    kzt_guest_dtv_entry_t *child_dtv;
    kzt_x86_64_tcbhead_t *parent_tcb;
    kzt_x86_64_tcbhead_t *child_tcb;
    TaskState *child_ts;
    uintptr_t aligned_static;
    uintptr_t child_tp;
    size_t static_size;
    size_t static_align;
    size_t tcb_size;
    size_t tls_object_count = 0;
    size_t index;
    int collect_result;

    if (!parent || !child || !parent->segs[R_FS].base ||
        !child->kzt_guest_tls_parent_snapshot) {
        return -1;
    }
    parent_snapshot = child->kzt_guest_tls_parent_snapshot;
    if (parent_snapshot->parent_tp != parent->segs[R_FS].base) {
        return -1;
    }
    collect_result = kzt_collect_guest_tls_objects(
        tls_objects, KZT_PUBLIC_LOADER_MAX_OBJECTS, &tls_object_count);
    if (collect_result != 0) {
        /* No Guest allocation or helper call has taken place yet. */
        return collect_result;
    }
    if (kzt_guest_tls_validate_inventory(
            tls_objects, tls_object_count) != 0) {
        return -1;
    }
    if (kzt_guest_tls_resolve_loader() != 0) {
        return -1;
    }
    static_size = 0;
    static_align = 0;
    RunFunctionWithStateInternal(guest_get_tls_static_info, 2,
                         (uint64_t)(uintptr_t)&static_size,
                         (uint64_t)(uintptr_t)&static_align);
    if (!static_size || static_size > KZT_GUEST_MAX_STATIC_TLS_SIZE ||
        !static_align || (static_align & (static_align - 1)) ||
        static_align > KZT_GUEST_MAX_TCB_SIZE) {
        return -1;
    }
    tcb_size = kzt_guest_tls_find_tcb_size(
        parent_snapshot, static_size, static_align);
    if (!tcb_size || tcb_size > static_size) {
        return -1;
    }

    state = g_new0(kzt_guest_tls_state_t, 1);
    g_rec_mutex_init(&state->execution_lock);
    state->static_allocation = g_malloc(
        static_size + static_align - 1);
    aligned_static = ((uintptr_t)state->static_allocation +
                      static_align - 1) &
                     ~(uintptr_t)(static_align - 1);
    memset((void *)aligned_static, 0, static_size);
    child_tp = aligned_static + static_size - tcb_size;
    state->static_start = aligned_static;
    state->static_size = static_size;
    state->thread_pointer = child_tp;

    state->dtv_allocation = g_malloc0(
        (KZT_GUEST_MAX_DTV_ENTRIES + 2) *
        sizeof(kzt_guest_dtv_entry_t));
    child_dtv = state->dtv_allocation;
    child_dtv[0].value = KZT_GUEST_MAX_DTV_ENTRIES;
    ++child_dtv;
    state->dtv = child_dtv;

    parent_tcb = (kzt_x86_64_tcbhead_t *)parent_snapshot->tcb_bytes;
    child_tcb = (kzt_x86_64_tcbhead_t *)child_tp;
    child_ts = env_cpu(child)->opaque;
    child_tcb->tcb = child_tp;
    child_tcb->dtv = child_dtv;
    child_tcb->self = child_tp;
    child_tcb->multiple_threads = 1;
    child_tcb->sysinfo = parent_tcb->sysinfo;
    child_tcb->stack_guard = parent_tcb->stack_guard;
    child_tcb->pointer_guard = parent_tcb->pointer_guard;
    child_tcb->feature_1 = parent_tcb->feature_1;

    if (!child_ts || child_ts->ts_tid <= 0 ||
        parent_snapshot->tid_offset >
            tcb_size - sizeof(uint32_t) ||
        parent_snapshot->robust_head_offset >
            tcb_size - sizeof(struct robust_list_head) ||
        parent_snapshot->robust_prev_offset >
            tcb_size - sizeof(uintptr_t)) {
        goto fail;
    }
    {
        struct robust_list_head *robust_head =
            (struct robust_list_head *)(child_tp +
                parent_snapshot->robust_head_offset);
        uintptr_t *robust_prev = (uintptr_t *)(child_tp +
            parent_snapshot->robust_prev_offset);
        uint32_t *tid = (uint32_t *)(child_tp +
            parent_snapshot->tid_offset);

        *tid = (uint32_t)child_ts->ts_tid;
        robust_head->list.next = &robust_head->list;
        robust_head->futex_offset =
            parent_snapshot->robust_futex_offset;
        robust_head->list_op_pending = NULL;
        *robust_prev = (uintptr_t)robust_head;
        state->robust_head_addr = (uintptr_t)robust_head;
        state->robust_prev_addr = (uintptr_t)robust_prev;
        state->robust_futex_offset =
            parent_snapshot->robust_futex_offset;
    }
    /*
     * Guest loader helpers may acquire pthread recursive locks.  A zero
     * TID would alias the unlocked owner value and bypass mutual exclusion.
     * Publish a complete thread descriptor before executing Guest code.
     */
    child->segs[R_FS].base = child_tp;
    child->kzt_guest_tls_allocation = state;
    state->refreshing = 1;
    if (RunFunctionWithStateInternal(
            guest_allocate_tls_init, 2,
            (uint64_t)child_tp, (uint64_t)1) != child_tp) {
        goto fail;
    }
    if (child_tcb->dtv != child_dtv) {
        goto fail;
    }
    if (kzt_guest_tls_resolve_new_module_ids(
            state, tls_objects, tls_object_count, 0) != 0) {
        goto fail;
    }
    if (kzt_guest_tls_populate_dynamic(
            state, tls_objects, tls_object_count) != 0) {
        goto fail;
    }
    kzt_guest_tls_remember_inventory(
        state, tls_objects, tls_object_count);
    if (!kzt_guest_tls_process_inventory_initialized) {
        qatomic_set(&kzt_guest_tls_process_generation,
                    state->dtv[0].value);
        memcpy(kzt_guest_tls_process_inventory, tls_objects,
               tls_object_count * sizeof(*tls_objects));
        kzt_guest_tls_process_inventory_count = tls_object_count;
        kzt_guest_tls_process_inventory_initialized = 1;
    } else {
        int changed = kzt_guest_tls_update_process_inventory(
            tls_objects, tls_object_count);
        uintptr_t generation;

        if (changed < 0) {
            goto fail;
        }
        generation = qatomic_read(&kzt_guest_tls_process_generation);
        /*
         * Guest loader activity can precede the next attached callback.
         * The new DTV supplies a live generation; do not move it backwards.
         * Existing states reconcile the published inventory before reuse.
         */
        if (state->dtv[0].value > generation) {
            generation = state->dtv[0].value;
            qatomic_set(&kzt_guest_tls_process_generation, generation);
            changed = 1;
        }
        if (changed) {
            qatomic_set(&kzt_guest_tls_pending_propagation_generation,
                        generation);
        }
        state->dtv[0].value = generation;
    }
    state->refreshing = 0;
    return 0;

fail:
    if (state) {
        for (index = 0; index < state->dynamic_module_count; ++index) {
            kzt_guest_tls_free_dynamic(state, index);
        }
        kzt_guest_tls_release_loader_state(state);
        g_free(state->static_allocation);
        g_rec_mutex_clear(&state->execution_lock);
        g_free(state);
    }
    child->kzt_guest_tls_allocation = NULL;
    return -1;
}

static int kzt_guest_tls_refresh_internal(CPUX86State *env,
                                          int allow_propagation)
{
    kzt_public_loader_tls_object_t
        tls_objects[KZT_PUBLIC_LOADER_MAX_OBJECTS];
    kzt_guest_tls_state_t *state;
    kzt_x86_64_tcbhead_t *tcb;
    size_t tls_object_count = 0;
    int inventory_changed;
    int inventory_has_additions;
    int process_inventory_changed;
    int propagate_inventory = 0;
    uintptr_t propagation_generation = 0;
    int result = -1;
    int collect_result;
    const char *failure_stage = "collect";

    if (!env || !env->kzt_guest_tls_allocation) {
        return 0;
    }
    state = env->kzt_guest_tls_allocation;
    g_rec_mutex_lock(&state->execution_lock);
    if (state->refreshing) {
        g_rec_mutex_unlock(&state->execution_lock);
        return 0;
    }
    mmap_lock();
    g_mutex_lock(&kzt_guest_tls_states_lock);
    if (state->refreshing) {
        result = 0;
        goto unlock;
    }
    collect_result = kzt_collect_guest_tls_objects(
        tls_objects, KZT_PUBLIC_LOADER_MAX_OBJECTS,
        &tls_object_count);
    if (collect_result != 0) {
        if (collect_result == KZT_GUEST_TLS_REFRESH_BUSY) {
            result = KZT_GUEST_TLS_REFRESH_BUSY;
        }
        goto unlock;
    }
    if (kzt_guest_tls_resolve_new_module_ids(
            state, tls_objects, tls_object_count, 0) != 0) {
        failure_stage = "module-id";
        goto unlock;
    }
    if (kzt_guest_tls_initialize_pending_static(
            state, tls_objects, tls_object_count, 1, 0) != 0) {
        failure_stage = "static-image";
        goto unlock;
    }
    if (kzt_guest_tls_validate_refresh_inventory(
            state, tls_objects, tls_object_count) != 0) {
        failure_stage = "inventory-validation";
        goto unlock;
    }
    process_inventory_changed =
        kzt_guest_tls_update_process_inventory(
            tls_objects, tls_object_count);
    if (process_inventory_changed < 0) {
        failure_stage = "process-generation";
        goto unlock;
    }
    inventory_changed = !kzt_guest_tls_inventory_matches(
        state, tls_objects, tls_object_count);
    inventory_has_additions = inventory_changed &&
        kzt_guest_tls_inventory_has_additions(
            state, tls_objects, tls_object_count);
    if (inventory_has_additions && process_inventory_changed) {
        qatomic_set(&kzt_guest_tls_pending_propagation_generation,
                    qatomic_read(&kzt_guest_tls_process_generation));
    }

    state->refreshing = 1;
    tcb = (kzt_x86_64_tcbhead_t *)env->segs[R_FS].base;
    if (!tcb || tcb->dtv != state->dtv) {
        failure_stage = "dtv-ownership";
        goto out;
    }
    if (kzt_guest_tls_populate_dynamic(
            state, tls_objects, tls_object_count) != 0) {
        failure_stage = "dynamic-image";
        goto out;
    }
    state->dtv[0].value = qatomic_read(
        &kzt_guest_tls_process_generation);
    propagate_inventory = allow_propagation &&
        qatomic_read(&kzt_guest_tls_pending_propagation_generation) != 0;
    propagation_generation = state->dtv[0].value;
    if (inventory_changed) {
        kzt_guest_tls_remember_inventory(
            state, tls_objects, tls_object_count);
    }
    result = 0;

out:
    state->refreshing = 0;
unlock:
    g_mutex_unlock(&kzt_guest_tls_states_lock);
    mmap_unlock();
    g_rec_mutex_unlock(&state->execution_lock);
    if (result == 0 && propagate_inventory) {
        int propagation_result = kzt_guest_tls_propagate_inventory(
            state, tls_objects, tls_object_count,
            propagation_generation);

        if (propagation_result < 0) {
            failure_stage = "thread-propagation";
            result = -1;
        } else if (propagation_result == 0) {
            g_mutex_lock(&kzt_guest_tls_states_lock);
            if (qatomic_read(
                    &kzt_guest_tls_pending_propagation_generation) ==
                    propagation_generation) {
                qatomic_set(
                    &kzt_guest_tls_pending_propagation_generation, 0);
            }
            g_mutex_unlock(&kzt_guest_tls_states_lock);
        }
    }
    if (result != 0 && result != KZT_GUEST_TLS_REFRESH_BUSY) {
        fprintf(stderr,
                "KZT Guest TLS refresh failed at %s for state %p\n",
                failure_stage, (void *)state);
    }
    if (result == 0) {
        kzt_guest_tls_publish_stable_epoch();
    }
    return result;
}

int kzt_guest_tls_refresh(CPUX86State *env)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return 0;
    }
    int result;

    kzt_guest_tls_initialize_refresh_lock();
    g_rec_mutex_lock(&kzt_guest_tls_refresh_lock);
    result = kzt_guest_tls_refresh_internal(env, 1);
    g_rec_mutex_unlock(&kzt_guest_tls_refresh_lock);
    return result;
}

int kzt_guest_tls_refresh_if_needed(CPUX86State *env)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return 0;
    }
    kzt_guest_tls_state_t *state;
    int result;

    if (!env || !env->kzt_guest_tls_allocation) {
        return 0;
    }
    state = env->kzt_guest_tls_allocation;
    g_rec_mutex_lock(&state->execution_lock);
    if (state->refreshing ||
        kzt_guest_tls_can_reuse_snapshot(state)) {
        g_rec_mutex_unlock(&state->execution_lock);
        return 0;
    }
    g_rec_mutex_unlock(&state->execution_lock);

    /* Slow refresh has one writer.  Waiters must not pin their state lock. */
    kzt_guest_tls_initialize_refresh_lock();
    g_rec_mutex_lock(&kzt_guest_tls_refresh_lock);
    g_rec_mutex_lock(&state->execution_lock);
    if (state->refreshing ||
        kzt_guest_tls_can_reuse_snapshot(state)) {
        result = 0;
    } else {
        result = kzt_guest_tls_refresh_internal(env, 1);
    }
    g_rec_mutex_unlock(&state->execution_lock);
    g_rec_mutex_unlock(&kzt_guest_tls_refresh_lock);
    return result;
}

int kzt_guest_tls_refresh_local(CPUX86State *env)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return 0;
    }
    int result;

    kzt_guest_tls_initialize_refresh_lock();
    g_rec_mutex_lock(&kzt_guest_tls_refresh_lock);
    result = kzt_guest_tls_refresh_internal(env, 0);
    g_rec_mutex_unlock(&kzt_guest_tls_refresh_lock);
    return result;
}

int kzt_guest_tls_preinitialize_static(CPUX86State *env,
                                       uintptr_t link_map_addr)
{
    kzt_public_loader_tls_object_t tls_object;
    kzt_guest_tls_state_t *state;
    uintptr_t pending_generation;
    int has_tls = 0;
    int result = -1;

    if (!env || !env->kzt_guest_tls_allocation || !link_map_addr) {
        return 0;
    }
    state = env->kzt_guest_tls_allocation;
    g_rec_mutex_lock(&state->execution_lock);
    mmap_lock();
    g_mutex_lock(&kzt_guest_tls_states_lock);
    result = kzt_collect_guest_tls_object(
        link_map_addr, &tls_object, &has_tls);
    if (result != 0) {
        goto out;
    }
    if (!has_tls) {
        result = 0;
        goto out;
    }
    result = kzt_guest_tls_resolve_new_module_ids(
        state, &tls_object, 1, 0);
    if (result != 0) {
        goto out;
    }
    if (!kzt_guest_tls_process_inventory_initialized ||
        qatomic_read(&kzt_guest_tls_process_generation) == UINTPTR_MAX) {
        goto out;
    }
    pending_generation = qatomic_read(
        &kzt_guest_tls_process_generation) + 1;
    if (tls_object.static_tls_offset_valid) {
        result = kzt_guest_tls_initialize_pending_static(
            state, &tls_object, 1, 0, pending_generation);
    } else if (!tls_object.module_id ||
               tls_object.module_id > KZT_GUEST_MAX_DTV_ENTRIES) {
        result = -1;
    } else {
        kzt_guest_dtv_entry_t *entry =
            &state->dtv[tls_object.module_id];
        int dynamic_index = kzt_guest_tls_find_dynamic_index(
            state, tls_object.module_id);

        if (dynamic_index < 0 ||
            !kzt_guest_tls_same_object(
                &state->dynamic_objects[dynamic_index],
                &tls_object)) {
            entry->value = UINTPTR_MAX;
            entry->to_free = 0;
        }
        result = kzt_guest_tls_populate_dynamic_object(
            state, &tls_object);
        if (result == 0 &&
            kzt_guest_tls_initialize_static_module(
                &tls_object, entry) != 0) {
            result = -1;
        }
        if (result == 0) {
            state->dtv[0].value = pending_generation;
        }
    }
out:
    g_mutex_unlock(&kzt_guest_tls_states_lock);
    mmap_unlock();
    g_rec_mutex_unlock(&state->execution_lock);
    return result;
}

static int kzt_guest_tls_initialize_libc(CPUX86State *env)
{
    if (!env || !env->kzt_guest_tls_allocation) {
        return -1;
    }
    if (!guest_uselocale) {
        guest_uselocale = kzt_resolve_guest_symbol("uselocale");
        if (!guest_uselocale) {
            guest_uselocale = kzt_resolve_guest_symbol("__uselocale");
        }
    }
    if (!guest_uselocale) {
        guest_uselocale = kzt_find_guest_libc_symbol("uselocale");
        if (!guest_uselocale) {
            guest_uselocale = kzt_find_guest_libc_symbol("__uselocale");
        }
    }
    if (!guest_uselocale) {
        printf_log(LOG_INFO,
                   "KZT cannot resolve the Guest libc locale initializer\n");
        return -1;
    }
    if (!guest_ctype_init) {
        guest_ctype_init =
            kzt_find_guest_libc_symbol("__ctype_init");
        if (!guest_ctype_init) {
            guest_ctype_init =
                kzt_resolve_guest_symbol("__ctype_init");
        }
    }
    if (!guest_ctype_init) {
        printf_log(LOG_INFO,
                   "KZT cannot resolve Guest __ctype_init\n");
        return -1;
    }

    if (RunFunctionWithStateInternalNoRefresh(
            guest_uselocale, 1, (uint64_t)-1) == 0) {
        return -1;
    }
    (void)RunFunctionWithStateInternalNoRefresh(
        guest_ctype_init, 0);
    return 0;
}

int kzt_guest_tls_initialize(CPUX86State *parent, CPUX86State *child)
{
    if (!latx_kzt_guest_tls_enabled()) {
        return -1;
    }
    int result;

    if (!parent || !child) {
        return -1;
    }
    kzt_guest_tls_initialize_refresh_lock();
    g_rec_mutex_lock(&kzt_guest_tls_refresh_lock);
    mmap_lock();
    g_mutex_lock(&kzt_guest_tls_states_lock);
    result = kzt_guest_tls_clone_static(parent, child);
    kzt_guest_tls_release_parent_snapshot(child);
    if (result != 0) {
        g_mutex_unlock(&kzt_guest_tls_states_lock);
        mmap_unlock();
        g_rec_mutex_unlock(&kzt_guest_tls_refresh_lock);
        return result;
    }
    result = kzt_guest_tls_initialize_libc(child);
    if (result == 0) {
        kzt_guest_tls_state_t *state =
            child->kzt_guest_tls_allocation;

        state->next = kzt_guest_tls_states;
        kzt_guest_tls_states = state;
    }
    g_mutex_unlock(&kzt_guest_tls_states_lock);
    mmap_unlock();
    if (result == 0) {
        kzt_guest_tls_publish_stable_epoch();
    }
    g_rec_mutex_unlock(&kzt_guest_tls_refresh_lock);
    return result;
}

void kzt_guest_tls_destroy(CPUX86State *env)
{
    kzt_guest_tls_state_t *state;

    if (!env) {
        return;
    }
    kzt_guest_tls_release_parent_snapshot(env);
    state = env->kzt_guest_tls_allocation;
    if (!state) {
        return;
    }
    if (g_atomic_int_get(&state->execution_depth) != 0 ||
        g_atomic_pointer_get(&state->execution_owner) != NULL) {
        fprintf(stderr,
                "KZT Guest TLS teardown raced active Guest execution; "
                "refusing to continue\n");
        _exit(EXIT_FAILURE);
    }
    g_mutex_lock(&kzt_guest_tls_states_lock);
    g_atomic_int_set(&state->destroying, 1);
    if (kzt_guest_tls_states == state) {
        kzt_guest_tls_states = state->next;
    } else {
        kzt_guest_tls_state_t *previous = kzt_guest_tls_states;

        while (previous && previous->next != state) {
            previous = previous->next;
        }
        if (previous) {
            previous->next = state->next;
        }
    }
    if (!kzt_guest_tls_states) {
        memset(kzt_guest_tls_process_inventory, 0,
               sizeof(kzt_guest_tls_process_inventory));
        kzt_guest_tls_process_inventory_count = 0;
        qatomic_set(&kzt_guest_tls_process_generation, 0);
        qatomic_set(&kzt_guest_tls_pending_propagation_generation, 0);
        kzt_guest_tls_process_inventory_initialized = 0;
    }
    g_mutex_unlock(&kzt_guest_tls_states_lock);
    for (int attempt = 0;
         g_atomic_int_get(&state->propagation_refs) != 0 &&
         attempt < KZT_GUEST_TLS_TARGET_WAIT_RETRIES;
         ++attempt) {
        g_usleep(1000);
    }
    if (g_atomic_int_get(&state->propagation_refs) != 0) {
        fprintf(stderr,
                "KZT Guest TLS state remained in propagation during "
                "thread teardown; refusing to continue\n");
        _exit(EXIT_FAILURE);
    }
    for (size_t index = 0; index < state->dynamic_module_count; ++index) {
        kzt_guest_tls_free_dynamic(state, index);
    }
    kzt_guest_tls_release_loader_state(state);
    g_free(state->static_allocation);
    g_rec_mutex_clear(&state->execution_lock);
    g_free(state);
    env->kzt_guest_tls_allocation = NULL;
}

static int kzt_guest_tls_robust_futex_address(
    uintptr_t list_addr, intptr_t futex_offset,
    uintptr_t *futex_addr)
{
    if (!list_addr || !futex_addr ||
        (futex_offset > 0 &&
         list_addr > UINTPTR_MAX - (uintptr_t)futex_offset) ||
        (futex_offset < 0 &&
         list_addr < (uintptr_t)-futex_offset)) {
        return -1;
    }
    *futex_addr = (uintptr_t)((intptr_t)list_addr + futex_offset);
    return 0;
}

static int kzt_guest_tls_mark_robust_owner_died(
    uintptr_t list_addr, intptr_t futex_offset, uint32_t tid)
{
    uintptr_t futex_addr;
    uint32_t *futex_word;
    uint32_t old_value;

    if ((list_addr & 1) ||
        kzt_guest_tls_robust_futex_address(
            list_addr, futex_offset, &futex_addr) != 0) {
        return -1;
    }
    futex_word = lock_user(
        VERIFY_WRITE, (abi_ulong)futex_addr,
        sizeof(*futex_word), 0);
    if (!futex_word) {
        return -1;
    }
    old_value = qatomic_read(futex_word);
    while ((old_value & FUTEX_TID_MASK) == tid) {
        uint32_t new_value =
            (old_value & FUTEX_WAITERS) | FUTEX_OWNER_DIED;
        uint32_t observed = qatomic_cmpxchg(
            futex_word, old_value, new_value);

        if (observed == old_value) {
            if (old_value & FUTEX_WAITERS) {
                long woken = syscall(
                    SYS_futex, futex_word, FUTEX_WAKE, 1,
                    NULL, NULL, 0);

                if (woken == 0) {
                    (void)syscall(
                        SYS_futex, futex_word,
                        FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1,
                        NULL, NULL, 0);
                }
            }
            break;
        }
        old_value = observed;
    }
    unlock_user(futex_word, (abi_ulong)futex_addr,
                sizeof(*futex_word));
    return 0;
}

int kzt_guest_tls_cleanup_robust_list(CPUX86State *env)
{
    kzt_guest_tls_state_t *state;
    CPUState *cpu;
    TaskState *ts;
    struct robust_list_head head;
    struct robust_list_head *locked_head;
    uintptr_t current;
    uintptr_t pending;
    size_t count = 0;
    int result = 0;

    if (!env || !env->kzt_guest_tls_allocation) {
        return 0;
    }
    state = env->kzt_guest_tls_allocation;
    cpu = env_cpu(env);
    ts = cpu ? cpu->opaque : NULL;
    if (!state->robust_head_addr || !ts || ts->ts_tid <= 0) {
        return -1;
    }
    locked_head = lock_user(
        VERIFY_READ, (abi_ulong)state->robust_head_addr,
        sizeof(head), 1);
    if (!locked_head) {
        return -1;
    }
    memcpy(&head, locked_head, sizeof(head));
    unlock_user(locked_head, (abi_ulong)state->robust_head_addr, 0);
    current = (uintptr_t)head.list.next;
    pending = (uintptr_t)head.list_op_pending;
    for (;
         current && current != state->robust_head_addr &&
         count < KZT_GUEST_ROBUST_LIST_LIMIT;
         ++count) {
        struct robust_list *entry;
        uintptr_t next;

        entry = lock_user(
            VERIFY_READ, (abi_ulong)(current & ~(uintptr_t)1),
            sizeof(*entry), 1);
        if (!entry) {
            result = -1;
            break;
        }
        next = (uintptr_t)entry->next;
        unlock_user(entry,
                    (abi_ulong)(current & ~(uintptr_t)1), 0);
        if (kzt_guest_tls_mark_robust_owner_died(
                current, state->robust_futex_offset,
                (uint32_t)ts->ts_tid) != 0) {
            result = -1;
        }
        current = next;
    }
    if (current && current != state->robust_head_addr) {
        result = -1;
    }
    if (pending) {
        if (kzt_guest_tls_mark_robust_owner_died(
                pending, state->robust_futex_offset,
                (uint32_t)ts->ts_tid) != 0) {
            result = -1;
        }
    }
    locked_head = lock_user(
        VERIFY_WRITE, (abi_ulong)state->robust_head_addr,
        sizeof(head), 0);
    if (locked_head) {
        locked_head->list.next =
            (struct robust_list *)state->robust_head_addr;
        locked_head->list_op_pending = NULL;
        unlock_user(locked_head,
                    (abi_ulong)state->robust_head_addr,
                    sizeof(head));
    } else {
        result = -1;
    }
    if (state->robust_prev_addr) {
        uintptr_t *robust_prev = lock_user(
            VERIFY_WRITE, (abi_ulong)state->robust_prev_addr,
            sizeof(*robust_prev), 0);

        if (robust_prev) {
            *robust_prev = state->robust_head_addr;
            unlock_user(robust_prev,
                        (abi_ulong)state->robust_prev_addr,
                        sizeof(*robust_prev));
        } else {
            result = -1;
        }
    }
    return result;
}
