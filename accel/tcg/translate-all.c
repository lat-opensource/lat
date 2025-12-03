/*
 *  Host code generation
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/interval-tree.h"
#include "qemu/units.h"
#include "qemu-common.h"

#define NO_CPU_IO_DEFS
#include "trace.h"
#include "disas/disas.h"
#include "exec/exec-all.h"
#include "tcg/tcg.h"
#if defined(CONFIG_USER_ONLY)
#include "qemu.h"
#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <sys/param.h>
#if __FreeBSD_version >= 700104
#define HAVE_KINFO_GETVMMAP
#define sigqueue sigqueue_freebsd  /* avoid redefinition */
#include <sys/proc.h>
#include <machine/profile.h>
#define _KERNEL
#include <sys/user.h>
#undef _KERNEL
#undef sigqueue
#include <libutil.h>
#endif
#endif
#else
#include "exec/ram_addr.h"
#endif

#include "exec/cputlb.h"
#include "exec/tb-hash.h"
#include "exec/translate-all.h"
#include "qemu/bitmap.h"
#include "qemu/error-report.h"
#include "qemu/qemu-print.h"
#include "qemu/timer.h"
#include "qemu/main-loop.h"
#include "exec/log.h"
#include "sysemu/cpus.h"
#include "sysemu/cpu-timers.h"
#include "sysemu/tcg.h"
#include "qapi/error.h"
#include "hw/core/tcg-cpu-ops.h"
#include "internal.h"
#include "loongarch-extcontext.h"

#ifdef CONFIG_LATX_PERF
#include "latx-perf.h"
#endif
#ifdef CONFIG_LATX
#include "qemu/cacheflush.h"
#include "latx-options.h"
#include "aot.h"
#include "aot_merge.h"
#include "reg-map.h"
#include "debug.h"
#include "aot_smc.h"
#include "aot_page.h"
#include "accel/tcg/internal.h"
#include "ts.h"
#include "latx-smc.h"
#endif
#ifdef CONFIG_LATX_FAST_JMPCACHE
#include "exec/fasttb.h"
#endif
#ifdef CONFIG_LATX_TU
void tu_reset_tb(TranslationBlock *tb);
#endif
#include "linux/memfd.h"
/* #define DEBUG_TB_INVALIDATE */
/* #define DEBUG_TB_FLUSH */
/* make various TB consistency checks */
/* #define DEBUG_TB_CHECK */

#ifdef DEBUG_TB_INVALIDATE
#define DEBUG_TB_INVALIDATE_GATE 1
#else
#define DEBUG_TB_INVALIDATE_GATE 0
#endif

#ifdef DEBUG_TB_FLUSH
#define DEBUG_TB_FLUSH_GATE 1
#else
#define DEBUG_TB_FLUSH_GATE 0
#endif

#if !defined(CONFIG_USER_ONLY)
/* TB consistency checks only implemented for usermode emulation.  */
#undef DEBUG_TB_CHECK
#endif

#ifdef DEBUG_TB_CHECK
#define DEBUG_TB_CHECK_GATE 1
#else
#define DEBUG_TB_CHECK_GATE 0
#endif

/* Access to the various translations structures need to be serialised via locks
 * for consistency.
 * In user-mode emulation access to the memory related structures are protected
 * with mmap_lock.
 * In !user-mode we use per-page locks.
 */
#ifdef CONFIG_SOFTMMU
#define assert_memory_lock()
#else
#define assert_memory_lock() tcg_debug_assert(have_mmap_lock())
#endif

#define SMC_BITMAP_USE_THRESHOLD 10

#define EXPAND_TO_64BIT(type, addr) ({ \
    type value = *(type *)(addr); \
    int64_t result = value; \
    for (int i = 1; i < (64 / (sizeof(type) * 8)); i++) { \
        result = (result << (sizeof(type) * 8)) | value; \
    } \
    result; \
})  /* EXPAND_TO_64BIT */

#ifdef CONFIG_LATX_SMC_OPT

/*
 * Retranslation strategy for SMC optimization.
 *
 * During the SMC process, the number of segv signal will be accumulated
 * to identify those TB that often write on smc-protected page. The profiling
 * is executed at handle_cpu_signal() -> page_unprotect() ->
 */
static __thread GTree *smc_retrans_tree;
static gint smc_retrans_cmp(gconstpointer ap, gconstpointer bp)
{
    const uint64_t *a = ap;
    const uint64_t *b = bp;
    if (*a > *b) {
        return 1;
    } else if (*a < *b) {
        return -1;
    }
    return 0;
}
static inline void smc_retrans_tree_init(void)
{
    if (smc_retrans_tree) return;
    smc_retrans_tree = g_tree_new(smc_retrans_cmp);
}
static inline void *smc_retrans_lookup(uint64_t pc)
{
    if (!smc_retrans_tree) return NULL;
    return g_tree_lookup(smc_retrans_tree, &pc);
}
static inline void smc_retrans_remove(uint64_t pc)
{
    if (!smc_retrans_tree) return;
    g_tree_remove(smc_retrans_tree, &pc);
}
static bool smc_retrans_insert(TranslationBlock *tb)
{
    if (!smc_retrans_tree) return false;
    if (!smc_retrans_lookup(tb->pc)) {
        g_tree_insert(smc_retrans_tree, &tb->pc, tb);
        return true;
    }
    return false;
}

#endif

/**
 * struct page_entry - page descriptor entry
 * @pd:     pointer to the &struct PageDesc of the page this entry represents
 * @index:  page index of the page
 * @locked: whether the page is locked
 *
 * This struct helps us keep track of the locked state of a page, without
 * bloating &struct PageDesc.
 *
 * A page lock protects accesses to all fields of &struct PageDesc.
 *
 * See also: &struct page_collection.
 */
struct page_entry {
    PageDesc *pd;
    tb_page_addr_t index;
    bool locked;
};

/**
 * struct page_collection - tracks a set of pages (i.e. &struct page_entry's)
 * @tree:   Binary search tree (BST) of the pages, with key == page index
 * @max:    Pointer to the page in @tree with the highest page index
 *
 * To avoid deadlock we lock pages in ascending order of page index.
 * When operating on a set of pages, we need to keep track of them so that
 * we can lock them in order and also unlock them later. For this we collect
 * pages (i.e. &struct page_entry's) in a binary search @tree. Given that the
 * @tree implementation we use does not provide an O(1) operation to obtain the
 * highest-ranked element, we use @max to keep track of the inserted page
 * with the highest index. This is valuable because if a page is not in
 * the tree and its index is higher than @max's, then we can lock it
 * without breaking the locking order rule.
 *
 * Note on naming: 'struct page_set' would be shorter, but we already have a few
 * page_set_*() helpers, so page_collection is used instead to avoid confusion.
 *
 * See also: page_collection_lock().
 */
struct page_collection {
    GTree *tree;
    struct page_entry *max;
};

/* list iterators for lists of tagged pointers in TranslationBlock */
#define TB_FOR_EACH_TAGGED(head, tb, n, field)                          \
    for (n = (head) & 1, tb = (TranslationBlock *)((head) & ~1);        \
         tb; tb = (TranslationBlock *)tb->field[n], n = (uintptr_t)tb & 1, \
             tb = (TranslationBlock *)((uintptr_t)tb & ~1))

#ifndef CONFIG_USER_ONLY
#define PAGE_FOR_EACH_TB(pagedesc, tb, n)                       \
    TB_FOR_EACH_TAGGED((pagedesc)->first_tb, tb, n, page_next)
#endif

#define TB_FOR_EACH_JMP(head_tb, tb, n)                                 \
    TB_FOR_EACH_TAGGED((head_tb)->jmp_list_head, tb, n, jmp_list_next)

/*
 * In system mode we want L1_MAP to be based on ram offsets,
 * while in user mode we want it to be based on virtual addresses.
 *
 * TODO: For user mode, see the caveat re host vs guest virtual
 * address spaces near GUEST_ADDR_MAX.
 */
#ifndef CONFIG_USER_ONLY
#if !defined(CONFIG_USER_ONLY)
#if HOST_LONG_BITS < TARGET_PHYS_ADDR_SPACE_BITS
# define L1_MAP_ADDR_SPACE_BITS  HOST_LONG_BITS
#else
# define L1_MAP_ADDR_SPACE_BITS  TARGET_PHYS_ADDR_SPACE_BITS
#endif
#else
# define LATX_VA_BITS 40
# define L1_MAP_ADDR_SPACE_BITS  MIN(LATX_VA_BITS, TARGET_ABI_BITS)
#endif

/* Size of the L2 (and L3, etc) page tables.  */
#define V_L2_BITS 10
#define V_L2_SIZE (1 << V_L2_BITS)

/* Make sure all possible CPU event bits fit in tb->trace_vcpu_dstate */
QEMU_BUILD_BUG_ON(CPU_TRACE_DSTATE_MAX_EVENTS >
                  sizeof_field(TranslationBlock, trace_vcpu_dstate)
                  * BITS_PER_BYTE);

/*
 * L1 Mapping properties
 */
static int v_l1_size;
static int v_l1_shift;
static int v_l2_levels;

/* The bottom level has pointers to PageDesc, and is indexed by
 * anything from 4 to (V_L2_BITS + 3) bits, depending on target page size.
 */
#define V_L1_MIN_BITS 4
#define V_L1_MAX_BITS (V_L2_BITS + 3)
#define V_L1_MAX_SIZE (1 << V_L1_MAX_BITS)

void *l1_map[V_L1_MAX_SIZE];
#endif

/* code generation context */
TCGContext tcg_init_ctx;
__thread TCGContext *tcg_ctx;
TBContext tb_ctx;

static void page_table_config_init(void)
{
#ifndef CONFIG_USER_ONLY
    uint32_t v_l1_bits;

    assert(TARGET_PAGE_BITS);
    /* The bits remaining after N lower levels of page tables.  */
    v_l1_bits = (L1_MAP_ADDR_SPACE_BITS - TARGET_PAGE_BITS) % V_L2_BITS;
    if (v_l1_bits < V_L1_MIN_BITS) {
        v_l1_bits += V_L2_BITS;
    }

    v_l1_size = 1 << v_l1_bits;
    v_l1_shift = L1_MAP_ADDR_SPACE_BITS - TARGET_PAGE_BITS - v_l1_bits;
    v_l2_levels = v_l1_shift / V_L2_BITS - 1;

    assert(v_l1_bits <= V_L1_MAX_BITS);
    assert(v_l1_shift % V_L2_BITS == 0);
    assert(v_l2_levels >= 0);
#endif
}

static void cpu_gen_init(void)
{
    tcg_context_init(&tcg_init_ctx);
}

/* Encode VAL as a signed leb128 sequence at P.
   Return P incremented past the encoded value.  */
static uint8_t *encode_sleb128(uint8_t *p, target_long val)
{
    int more, byte;

    do {
        byte = val & 0x7f;
        val >>= 7;
        more = !((val == 0 && (byte & 0x40) == 0)
                 || (val == -1 && (byte & 0x40) != 0));
        if (more) {
            byte |= 0x80;
        }
        *p++ = byte;
    } while (more);

    return p;
}

/* Decode a signed leb128 sequence at *PP; increment *PP past the
   decoded value.  Return the decoded value.  */
static target_long decode_sleb128(const uint8_t **pp)
{
    const uint8_t *p = *pp;
    target_long val = 0;
    int byte, shift = 0;

    do {
        byte = *p++;
        val |= (target_ulong)(byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    if (shift < TARGET_LONG_BITS && (byte & 0x40)) {
        val |= -(target_ulong)1 << shift;
    }

    *pp = p;
    return val;
}

/* Encode the data collected about the instructions while compiling TB.
   Place the data at BLOCK, and return the number of bytes consumed.

   The logical table consists of TARGET_INSN_START_WORDS target_ulong's,
   which come from the target's insn_start data, followed by a uintptr_t
   which comes from the host pc of the end of the code implementing the insn.

   Each line of the table is encoded as sleb128 deltas from the previous
   line.  The seed for the first line is { tb->pc, 0..., tb->tc.ptr }.
   That is, the first column is seeded with the guest pc, the last column
   with the host pc, and the middle columns with zeros.  */

int encode_search(TranslationBlock *tb, uint8_t *block)
{
    uint8_t *highwater = tcg_ctx->code_gen_highwater;
    uint8_t *p = block;
    int i, j, n;

    for (i = 0, n = tb->icount; i < n; ++i) {
        target_ulong prev;

        for (j = 0; j < TARGET_INSN_START_WORDS; ++j) {
            if (i == 0) {
                prev = (j == 0 ? tb->pc : 0);
            } else {
                prev = tcg_ctx->gen_insn_data[i - 1][j];
            }
            p = encode_sleb128(p, tcg_ctx->gen_insn_data[i][j] - prev);
        }
        prev = (i == 0 ? 0 : tcg_ctx->gen_insn_end_off[i - 1]);
        p = encode_sleb128(p, tcg_ctx->gen_insn_end_off[i] - prev);

        /* Test for (pending) buffer overflow.  The assumption is that any
           one row beginning below the high water mark cannot overrun
           the buffer completely.  Thus we can test for overflow after
           encoding a row without having to check during encoding.  */
        if (!in_pre_translate && unlikely(p > highwater)) {
            return -1;
        }
    }

    return p - block;
}

#ifdef CONFIG_LATX_OPT_PUSH_POP
extern void ir2_opt_push_pop_fix(TranslationBlock *tb, CPUState *cpu, int i);
#endif
/* The cpu state corresponding to 'searched_pc' is restored.
 * When reset_icount is true, current TB will be interrupted and
 * icount should be recalculated.
 */
static int cpu_restore_state_from_tb(CPUState *cpu, TranslationBlock *tb,
                                     uintptr_t searched_pc, bool reset_icount)
{
    target_ulong data[TARGET_INSN_START_WORDS] = { tb->pc };
    uintptr_t host_pc = (uintptr_t)tb->tc.ptr;
    CPUArchState *env = cpu->env_ptr;
    const uint8_t *p;
#ifdef CONFIG_LATX_TU
    if (is_tu_tb(tb)) {
        p = tb->tc.ptr + tb->tu_search_addr_offset;
    } else {
        p = tb->tc.ptr + tb->tc.size;
    }
#else
    p = tb->tc.ptr + tb->tc.size;
#endif
    int i, j, num_insns = tb->icount;
#ifdef CONFIG_PROFILER
    TCGProfile *prof = &tcg_ctx->prof;
    int64_t ti = profile_getclock();
#endif

    searched_pc -= GETPC_ADJ;

    if (searched_pc < host_pc) {
        return -1;
    }

    /* Reconstruct the stored insn data while looking for the point at
       which the end of the insn exceeds the searched_pc.  */
    for (i = 0; i < num_insns; ++i) {
        for (j = 0; j < TARGET_INSN_START_WORDS; ++j) {
            data[j] += decode_sleb128(&p);
        }
        host_pc += decode_sleb128(&p);
        if (host_pc > searched_pc) {
            goto found;
        }
    }
    return -1;

 found:
    if (reset_icount && (tb_cflags(tb) & CF_USE_ICOUNT)) {
        assert(icount_enabled());
        /* Reset the cycle counter to the start of the block
           and shift if to the number of actually executed instructions */
        cpu_neg(cpu)->icount_decr.u16.low += num_insns - i;
    }
    restore_state_to_opc(env, tb, data);
#ifdef CONFIG_LATX_OPT_PUSH_POP
    uint32_t parallel = cpu->tcg_cflags & CF_PARALLEL;
    if (!parallel) {
        ir2_opt_push_pop_fix(tb, cpu, (searched_pc - (uintptr_t)tb->tc.ptr) / 4);
    }
#endif
#ifdef CONFIG_PROFILER
    qatomic_set(&prof->restore_time,
                prof->restore_time + profile_getclock() - ti);
    qatomic_set(&prof->restore_count, prof->restore_count + 1);
#endif
    return 0;
}

void tb_destroy(TranslationBlock *tb)
{
    qemu_spin_destroy(&tb->jmp_lock);
}

bool cpu_restore_state(CPUState *cpu, uintptr_t host_pc, bool will_exit)
{
    /*
     * The host_pc has to be in the rx region of the code buffer.
     * If it is not we will not be able to resolve it here.
     * The two cases where host_pc will not be correct are:
     *
     *  - fault during translation (instruction fetch)
     *  - fault from helper (not using GETPC() macro)
     *
     * Either way we need return early as we can't resolve it here.
     */
    if (in_code_gen_buffer((const void *)(host_pc - tcg_splitwx_diff))) {
        TranslationBlock *tb = tcg_tb_lookup(host_pc);
        if (tb) {
            cpu_restore_state_from_tb(cpu, tb, host_pc, will_exit);
            return true;
        }
    }
    return false;
}

static void page_init(void)
{
    page_size_init();
    page_table_config_init();

}


unsigned long shadow_page_map_start = -1;
unsigned long shadow_page_map_end = 0;

static inline void spm_set(uint64_t addr, uint64_t len)
{
    if (shadow_page_map_start > addr)
        shadow_page_map_start = addr;
    if (shadow_page_map_end < addr + len)
        shadow_page_map_end = addr + len;
}

static inline void spm_clear(uint64_t addr, uint64_t len)
{
    if (shadow_page_map_start == addr)
        shadow_page_map_start = addr + len;

    if (shadow_page_map_end == addr + len)
        shadow_page_map_end = addr;
}

static inline bool spm_exist(uint64_t addr, uint64_t len)
{
    if (!(addr >= shadow_page_map_end || (addr + len) <= shadow_page_map_start)) {
        return true;
    } else {
        return false;
    }
}

static bool is_shadow_page(target_ulong address)
{
    return page_get_target_data(address);
}

static bool is_shadow_page_shmm(target_ulong address)
{
    ShadowPageDesc *spd = page_get_target_data(address);
    return spd && spd->is_shmm;
}

static bool is_shadow_page_not_shmm(target_ulong address)
{
    ShadowPageDesc *spd = page_get_target_data(address);
    return spd && !(spd->is_shmm);
}

#ifndef CONFIG_USER_ONLY
PageDesc *page_find_alloc(tb_page_addr_t index, int alloc)
{
    PageDesc *pd;
    void **lp;
    int i;

    /* Level 1.  Always allocated.  */
    lp = l1_map + ((index >> v_l1_shift) & (v_l1_size - 1));

    /* Level 2..N-1.  */
    for (i = v_l2_levels; i > 0; i--) {
        void **p = qatomic_rcu_read(lp);

        if (p == NULL) {
            void *existing;

            if (!alloc) {
                return NULL;
            }
            p = g_new0(void *, V_L2_SIZE);
            existing = qatomic_cmpxchg(lp, NULL, p);
            if (unlikely(existing)) {
                g_free(p);
                p = existing;
            }
        }

        lp = p + ((index >> (i * V_L2_BITS)) & (V_L2_SIZE - 1));
    }

    pd = qatomic_rcu_read(lp);
    if (pd == NULL) {
        void *existing;

        if (!alloc) {
            return NULL;
        }
        pd = g_new0(PageDesc, V_L2_SIZE);
#ifndef CONFIG_USER_ONLY
        {
            int i;

            for (i = 0; i < V_L2_SIZE; i++) {
                qemu_spin_init(&pd[i].lock);
            }
        }
#endif
        existing = qatomic_cmpxchg(lp, NULL, pd);
        if (unlikely(existing)) {
#ifndef CONFIG_USER_ONLY
            {
                int i;

                for (i = 0; i < V_L2_SIZE; i++) {
                    qemu_spin_destroy(&pd[i].lock);
                }
            }
#endif
            g_free(pd);
            pd = existing;
        }
    }

    return pd + (index & (V_L2_SIZE - 1));
}

static inline PageDesc *page_find(tb_page_addr_t index)
{
    return page_find_alloc(index, 0);
}
#endif

static void page_lock_pair(PageDesc **ret_p1, tb_page_addr_t phys1,
                           PageDesc **ret_p2, tb_page_addr_t phys2, int alloc);

/* In user-mode page locks aren't used; mmap_lock is enough */
#ifdef CONFIG_USER_ONLY

#define assert_page_locked(pd) tcg_debug_assert(have_mmap_lock())

static inline void page_lock(PageDesc *pd)
{ }

static inline void page_unlock(PageDesc *pd)
{ }

static inline void page_lock_tb(const TranslationBlock *tb)
{ }

static inline void page_unlock_tb(const TranslationBlock *tb)
{ }

struct page_collection *
page_collection_lock(tb_page_addr_t start, tb_page_addr_t end)
{
    return NULL;
}

void page_collection_unlock(struct page_collection *set)
{ }

#define X86_MAX_INSN_LENGTH 15
int get_insn_len_readable(target_ulong address)
{
    int max_insn_len = X86_MAX_INSN_LENGTH;
    tb_page_addr_t start_page_addr = address & TARGET_PAGE_MASK;
    tb_page_addr_t end_page_addr = (address + X86_MAX_INSN_LENGTH) & TARGET_PAGE_MASK;
    if (start_page_addr != end_page_addr) {
        /* end page should be readable */
        if (!(page_get_flags(end_page_addr) & PAGE_READ)) {
            max_insn_len = end_page_addr - address;
        }
    }
    return max_insn_len;
}

#else /* !CONFIG_USER_ONLY */

#ifdef CONFIG_DEBUG_TCG

static __thread GHashTable *ht_pages_locked_debug;

static void ht_pages_locked_debug_init(void)
{
    if (ht_pages_locked_debug) {
        return;
    }
    ht_pages_locked_debug = g_hash_table_new(NULL, NULL);
}

static bool page_is_locked(const PageDesc *pd)
{
    PageDesc *found;

    ht_pages_locked_debug_init();
    found = g_hash_table_lookup(ht_pages_locked_debug, pd);
    return !!found;
}

static void page_lock__debug(PageDesc *pd)
{
    ht_pages_locked_debug_init();
    g_assert(!page_is_locked(pd));
    g_hash_table_insert(ht_pages_locked_debug, pd, pd);
}

static void page_unlock__debug(const PageDesc *pd)
{
    bool removed;

    ht_pages_locked_debug_init();
    g_assert(page_is_locked(pd));
    removed = g_hash_table_remove(ht_pages_locked_debug, pd);
    g_assert(removed);
}

static void
do_assert_page_locked(const PageDesc *pd, const char *file, int line)
{
    if (unlikely(!page_is_locked(pd))) {
        error_report("assert_page_lock: PageDesc %p not locked @ %s:%d",
                     pd, file, line);
        abort();
    }
}

#define assert_page_locked(pd) do_assert_page_locked(pd, __FILE__, __LINE__)

void assert_no_pages_locked(void)
{
    ht_pages_locked_debug_init();
    g_assert(g_hash_table_size(ht_pages_locked_debug) == 0);
}

#else /* !CONFIG_DEBUG_TCG */

#define assert_page_locked(pd)

static inline void page_lock__debug(const PageDesc *pd)
{
}

static inline void page_unlock__debug(const PageDesc *pd)
{
}

#endif /* CONFIG_DEBUG_TCG */

static inline void page_lock(PageDesc *pd)
{
    page_lock__debug(pd);
    qemu_spin_lock(&pd->lock);
}

static inline void page_unlock(PageDesc *pd)
{
    qemu_spin_unlock(&pd->lock);
    page_unlock__debug(pd);
}

/* lock the page(s) of a TB in the correct acquisition order */
static inline void page_lock_tb(const TranslationBlock *tb)
{
    page_lock_pair(NULL, tb_page_addr0(tb), NULL, tb_page_addr1(tb), false);
}

static inline void page_unlock_tb(const TranslationBlock *tb)
{
    PageDesc *p1 = page_find(tb_page_addr0(tb) >> TARGET_PAGE_BITS);

    page_unlock(p1);
    if (unlikely(tb_page_addr1(tb) != -1)) {
        PageDesc *p2 = page_find(tb_page_addr1(tb) >> TARGET_PAGE_BITS);

        if (p2 != p1) {
            page_unlock(p2);
        }
    }
}

static inline struct page_entry *
page_entry_new(PageDesc *pd, tb_page_addr_t index)
{
    struct page_entry *pe = g_malloc(sizeof(*pe));

    pe->index = index;
    pe->pd = pd;
    pe->locked = false;
    return pe;
}

static void page_entry_destroy(gpointer p)
{
    struct page_entry *pe = p;

    g_assert(pe->locked);
    page_unlock(pe->pd);
    g_free(pe);
}

/* returns false on success */
static bool page_entry_trylock(struct page_entry *pe)
{
    bool busy;

    busy = qemu_spin_trylock(&pe->pd->lock);
    if (!busy) {
        g_assert(!pe->locked);
        pe->locked = true;
        page_lock__debug(pe->pd);
    }
    return busy;
}

static void do_page_entry_lock(struct page_entry *pe)
{
    page_lock(pe->pd);
    g_assert(!pe->locked);
    pe->locked = true;
}

static gboolean page_entry_lock(gpointer key, gpointer value, gpointer data)
{
    struct page_entry *pe = value;

    do_page_entry_lock(pe);
    return FALSE;
}

static gboolean page_entry_unlock(gpointer key, gpointer value, gpointer data)
{
    struct page_entry *pe = value;

    if (pe->locked) {
        pe->locked = false;
        page_unlock(pe->pd);
    }
    return FALSE;
}

/*
 * Trylock a page, and if successful, add the page to a collection.
 * Returns true ("busy") if the page could not be locked; false otherwise.
 */
static bool page_trylock_add(struct page_collection *set, tb_page_addr_t addr)
{
    tb_page_addr_t index = addr >> TARGET_PAGE_BITS;
    struct page_entry *pe;
    PageDesc *pd;

    pe = g_tree_lookup(set->tree, &index);
    if (pe) {
        return false;
    }

    pd = page_find(index);
    if (pd == NULL) {
        return false;
    }

    pe = page_entry_new(pd, index);
    g_tree_insert(set->tree, &pe->index, pe);

    /*
     * If this is either (1) the first insertion or (2) a page whose index
     * is higher than any other so far, just lock the page and move on.
     */
    if (set->max == NULL || pe->index > set->max->index) {
        set->max = pe;
        do_page_entry_lock(pe);
        return false;
    }
    /*
     * Try to acquire out-of-order lock; if busy, return busy so that we acquire
     * locks in order.
     */
    return page_entry_trylock(pe);
}

static gint tb_page_addr_cmp(gconstpointer ap, gconstpointer bp, gpointer udata)
{
    tb_page_addr_t a = *(const tb_page_addr_t *)ap;
    tb_page_addr_t b = *(const tb_page_addr_t *)bp;

    if (a == b) {
        return 0;
    } else if (a < b) {
        return -1;
    }
    return 1;
}

/*
 * Lock a range of pages ([@start,@end[) as well as the pages of all
 * intersecting TBs.
 * Locking order: acquire locks in ascending order of page index.
 */
struct page_collection *
page_collection_lock(tb_page_addr_t start, tb_page_addr_t end)
{
    struct page_collection *set = g_malloc(sizeof(*set));
    tb_page_addr_t index;
    PageDesc *pd;

    start >>= TARGET_PAGE_BITS;
    end   >>= TARGET_PAGE_BITS;
    g_assert(start <= end);

    set->tree = g_tree_new_full(tb_page_addr_cmp, NULL, NULL,
                                page_entry_destroy);
    set->max = NULL;
    assert_no_pages_locked();

 retry:
    g_tree_foreach(set->tree, page_entry_lock, NULL);

    for (index = start; index <= end; index++) {
        TranslationBlock *tb;
        PageForEachNext n;

        pd = page_find(index);
        if (pd == NULL) {
            continue;
        }
        if (page_trylock_add(set, index << TARGET_PAGE_BITS)) {
            g_tree_foreach(set->tree, page_entry_unlock, NULL);
            goto retry;
        }
        assert_page_locked(pd);
        PAGE_FOR_EACH_TB(unused, unused, pd, tb, n) {
        if (page_trylock_add(set, tb_page_addr0(tb)) ||
                (tb_page_addr1(tb) != -1 &&
                 page_trylock_add(set, tb_page_addr1(tb)))) {
                /* drop all locks, and reacquire in order */
                g_tree_foreach(set->tree, page_entry_unlock, NULL);
                goto retry;
            }
        }
    }
    return set;
}

void page_collection_unlock(struct page_collection *set)
{
    /* entries are unlocked and freed via page_entry_destroy */
    g_tree_destroy(set->tree);
    g_free(set);
}

#endif /* !CONFIG_USER_ONLY */

static void page_lock_pair(PageDesc **ret_p1, tb_page_addr_t phys1,
                           PageDesc **ret_p2, tb_page_addr_t phys2, int alloc)
{
#ifndef CONFIG_USER_ONLY
    PageDesc *p1, *p2;
    tb_page_addr_t page1;
    tb_page_addr_t page2;

    assert_memory_lock();
    g_assert(phys1 != -1);

    page1 = phys1 >> TARGET_PAGE_BITS;
    page2 = phys2 >> TARGET_PAGE_BITS;

    p1 = page_find_alloc(page1, alloc);
    if (ret_p1) {
        *ret_p1 = p1;
    }
    if (likely(phys2 == -1)) {
        page_lock(p1);
        return;
    } else if (page1 == page2) {
        page_lock(p1);
        if (ret_p2) {
            *ret_p2 = p1;
        }
        return;
    }
    p2 = page_find_alloc(page2, alloc);
    if (ret_p2) {
        *ret_p2 = p2;
    }
    if (page1 < page2) {
        page_lock(p1);
        page_lock(p2);
    } else {
        page_lock(p2);
        page_lock(p1);
    }
#endif
}

/* Minimum size of the code gen buffer.  This number is randomly chosen,
   but not so small that we can't have a fair number of TB's live.  */
#define MIN_CODE_GEN_BUFFER_SIZE     (1 * MiB)

/* Maximum size of the code gen buffer we'd like to use.  Unless otherwise
   indicated, this is constrained by the range of direct branches on the
   host cpu, as used by the TCG implementation of goto_tb.  */
#if defined(__x86_64__)
# define MAX_CODE_GEN_BUFFER_SIZE  (2 * GiB)
#elif defined(__sparc__)
# define MAX_CODE_GEN_BUFFER_SIZE  (2 * GiB)
#elif defined(__powerpc64__)
# define MAX_CODE_GEN_BUFFER_SIZE  (2 * GiB)
#elif defined(__powerpc__)
# define MAX_CODE_GEN_BUFFER_SIZE  (32 * MiB)
#elif defined(__aarch64__)
# define MAX_CODE_GEN_BUFFER_SIZE  (2 * GiB)
#elif defined(__s390x__)
  /* We have a +- 4GB range on the branches; leave some slop.  */
# define MAX_CODE_GEN_BUFFER_SIZE  (3 * GiB)
#elif defined(__mips__) || defined(__loongarch__)
  /* We have a 256MB branch region, but leave room to make sure the
     main executable is also within that region.  */
#ifdef CONFIG_LATX_LARGE_CC
# define MAX_CODE_GEN_BUFFER_SIZE  (512 * MiB)
#else
# define MAX_CODE_GEN_BUFFER_SIZE  (128 * MiB)
#endif
#else
# define MAX_CODE_GEN_BUFFER_SIZE  ((size_t)-1)
#endif

#if TCG_TARGET_REG_BITS == 32
#define DEFAULT_CODE_GEN_BUFFER_SIZE_1 (32 * MiB)
#ifdef CONFIG_USER_ONLY
/*
 * For user mode on smaller 32 bit systems we may run into trouble
 * allocating big chunks of data in the right place. On these systems
 * we utilise a static code generation buffer directly in the binary.
 */
#define USE_STATIC_CODE_GEN_BUFFER
#endif
#else /* TCG_TARGET_REG_BITS == 64 */
#ifdef CONFIG_USER_ONLY
/*
 * As user-mode emulation typically means running multiple instances
 * of the translator don't go too nuts with our default code gen
 * buffer lest we make things too hard for the OS.
 */
#ifdef CONFIG_LATX_LARGE_CC
#define DEFAULT_CODE_GEN_BUFFER_SIZE_1 (512 * MiB)
#else
#ifdef LOW_MEM_MODE_2
#define DEFAULT_CODE_GEN_BUFFER_SIZE_1 (64 * MiB)
#else
#define DEFAULT_CODE_GEN_BUFFER_SIZE_1 (128 * MiB)
#endif
#endif
#else
/*
 * We expect most system emulation to run one or two guests per host.
 * Users running large scale system emulation may want to tweak their
 * runtime setup via the tb-size control on the command line.
 */
#define DEFAULT_CODE_GEN_BUFFER_SIZE_1 (1 * GiB)
#endif
#endif

#define DEFAULT_CODE_GEN_BUFFER_SIZE \
  (DEFAULT_CODE_GEN_BUFFER_SIZE_1 < MAX_CODE_GEN_BUFFER_SIZE \
   ? DEFAULT_CODE_GEN_BUFFER_SIZE_1 : MAX_CODE_GEN_BUFFER_SIZE)

static size_t size_code_gen_buffer(size_t tb_size)
{
    /* Size the buffer.  */
    if (tb_size == 0) {
        size_t phys_mem = qemu_get_host_physmem();
        if (phys_mem == 0) {
            tb_size = DEFAULT_CODE_GEN_BUFFER_SIZE;
        } else {
            tb_size = MIN(DEFAULT_CODE_GEN_BUFFER_SIZE, phys_mem / 8);
        }
    }
    if (tb_size < MIN_CODE_GEN_BUFFER_SIZE) {
        tb_size = MIN_CODE_GEN_BUFFER_SIZE;
    }
    if (tb_size > MAX_CODE_GEN_BUFFER_SIZE) {
        tb_size = MAX_CODE_GEN_BUFFER_SIZE;
    }
    return tb_size;
}

#ifdef __mips__
/* In order to use J and JAL within the code_gen_buffer, we require
   that the buffer not cross a 256MB boundary.  */
static inline bool cross_256mb(void *addr, size_t size)
{
    return ((uintptr_t)addr ^ ((uintptr_t)addr + size)) & ~0x0ffffffful;
}

/* We weren't able to allocate a buffer without crossing that boundary,
   so make do with the larger portion of the buffer that doesn't cross.
   Returns the new base of the buffer, and adjusts code_gen_buffer_size.  */
static inline void *split_cross_256mb(void *buf1, size_t size1)
{
    void *buf2 = (void *)(((uintptr_t)buf1 + size1) & ~0x0ffffffful);
    size_t size2 = buf1 + size1 - buf2;

    size1 = buf2 - buf1;
    if (size1 < size2) {
        size1 = size2;
        buf1 = buf2;
    }

    tcg_ctx->code_gen_buffer_size = size1;
    return buf1;
}
#endif

#ifdef USE_STATIC_CODE_GEN_BUFFER
static uint8_t static_code_gen_buffer[DEFAULT_CODE_GEN_BUFFER_SIZE]
    __attribute__((aligned(CODE_GEN_ALIGN)));

static bool alloc_code_gen_buffer(size_t tb_size, int splitwx, Error **errp)
{
    void *buf, *end;
    size_t size;

    if (splitwx > 0) {
        error_setg(errp, "jit split-wx not supported");
        return false;
    }

    /* page-align the beginning and end of the buffer */
    buf = static_code_gen_buffer;
    end = static_code_gen_buffer + sizeof(static_code_gen_buffer);
    buf = QEMU_ALIGN_PTR_UP(buf, qemu_real_host_page_size);
    end = QEMU_ALIGN_PTR_DOWN(end, qemu_real_host_page_size);

    size = end - buf;

    /* Honor a command-line option limiting the size of the buffer.  */
    if (size > tb_size) {
        size = QEMU_ALIGN_DOWN(tb_size, qemu_real_host_page_size);
    }
    tcg_ctx->code_gen_buffer_size = size;

#ifdef __mips__
    if (cross_256mb(buf, size)) {
        buf = split_cross_256mb(buf, size);
        size = tcg_ctx->code_gen_buffer_size;
    }
#endif

    if (qemu_mprotect_rwx(buf, size)) {
        error_setg_errno(errp, errno, "mprotect of jit buffer");
        return false;
    }
    qemu_madvise(buf, size, QEMU_MADV_HUGEPAGE);

    tcg_ctx->code_gen_buffer = buf;
    return true;
}
#elif defined(_WIN32)
static bool alloc_code_gen_buffer(size_t size, int splitwx, Error **errp)
{
    void *buf;

    if (splitwx > 0) {
        error_setg(errp, "jit split-wx not supported");
        return false;
    }

    buf = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT,
                             PAGE_EXECUTE_READWRITE);
    if (buf == NULL) {
        error_setg_win32(errp, GetLastError(),
                         "allocate %zu bytes for jit buffer", size);
        return false;
    }

    tcg_ctx->code_gen_buffer = buf;
    tcg_ctx->code_gen_buffer_size = size;
    return true;
}
#else
static bool alloc_code_gen_buffer_anon(size_t size, int prot,
                                       int flags, Error **errp)
{
    void *buf;

    buf = mmap(NULL, size, prot, flags, -1, 0);
    if (buf == MAP_FAILED) {
        error_setg_errno(errp, errno,
                         "allocate %zu bytes for jit buffer", size);
        return false;
    }
    tcg_ctx->code_gen_buffer_size = size;

#ifdef __mips__
    if (cross_256mb(buf, size)) {
        /*
         * Try again, with the original still mapped, to avoid re-acquiring
         * the same 256mb crossing.
         */
        size_t size2;
        void *buf2 = mmap(NULL, size, prot, flags, -1, 0);
        switch ((int)(buf2 != MAP_FAILED)) {
        case 1:
            if (!cross_256mb(buf2, size)) {
                /* Success!  Use the new buffer.  */
                munmap(buf, size);
                break;
            }
            /* Failure.  Work with what we had.  */
            munmap(buf2, size);
            /* fallthru */
        default:
            /* Split the original buffer.  Free the smaller half.  */
            buf2 = split_cross_256mb(buf, size);
            size2 = tcg_ctx->code_gen_buffer_size;
            if (buf == buf2) {
                munmap(buf + size2, size - size2);
            } else {
                munmap(buf, size - size2);
            }
            size = size2;
            break;
        }
        buf = buf2;
    }
#endif

    /* Request large pages for the buffer.  */
#ifdef LOW_MEM_MODE_0
    qemu_madvise(buf, size, QEMU_MADV_NOHUGEPAGE);
#else
    qemu_madvise(buf, size, QEMU_MADV_HUGEPAGE);
#endif

    tcg_ctx->code_gen_buffer = buf;
    return true;
}

static bool alloc_tb_gen_buffer(size_t size)
{
    void *buf;

    buf = mmap(NULL, size, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buf == MAP_FAILED) {
        return false;
    }

    /* Request large pages for the buffer.  */
#ifdef LOW_MEM_MODE_0
    qemu_madvise(buf, size, QEMU_MADV_NOHUGEPAGE);
#else
    qemu_madvise(buf, size, QEMU_MADV_HUGEPAGE);
#endif

    tcg_ctx->tb_gen_buffer = buf;
    tcg_ctx->tb_gen_highwater = buf + size - 1024;
    return true;
}

#ifndef CONFIG_TCG_INTERPRETER
#ifdef CONFIG_POSIX
#include "qemu/memfd.h"

static bool alloc_code_gen_buffer_splitwx_memfd(size_t size, Error **errp)
{
    void *buf_rw = NULL, *buf_rx = MAP_FAILED;
    int fd = -1;

#ifdef __mips__
    /* Find space for the RX mapping, vs the 256MiB regions. */
    if (!alloc_code_gen_buffer_anon(size, PROT_NONE,
                                    MAP_PRIVATE | MAP_ANONYMOUS |
                                    MAP_NORESERVE, errp)) {
        return false;
    }
    /* The size of the mapping may have been adjusted. */
    size = tcg_ctx->code_gen_buffer_size;
    buf_rx = tcg_ctx->code_gen_buffer;
#endif

    buf_rw = qemu_memfd_alloc("tcg-jit", size, 0, &fd, errp);
    if (buf_rw == NULL) {
        goto fail;
    }

#ifdef __mips__
    void *tmp = mmap(buf_rx, size, PROT_READ | PROT_EXEC,
                     MAP_SHARED | MAP_FIXED, fd, 0);
    if (tmp != buf_rx) {
        goto fail_rx;
    }
#else
    buf_rx = mmap(NULL, size, PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);
    if (buf_rx == MAP_FAILED) {
        goto fail_rx;
    }
#endif

    close(fd);
    tcg_ctx->code_gen_buffer = buf_rw;
    tcg_ctx->code_gen_buffer_size = size;
    tcg_splitwx_diff = buf_rx - buf_rw;

    /* Request large pages for the buffer and the splitwx.  */
    qemu_madvise(buf_rw, size, QEMU_MADV_HUGEPAGE);
    qemu_madvise(buf_rx, size, QEMU_MADV_HUGEPAGE);
    return true;

 fail_rx:
    error_setg_errno(errp, errno, "failed to map shared memory for execute");
 fail:
    if (buf_rx != MAP_FAILED) {
        munmap(buf_rx, size);
    }
    if (buf_rw) {
        munmap(buf_rw, size);
    }
    if (fd >= 0) {
        close(fd);
    }
    return false;
}
#endif /* CONFIG_POSIX */

#ifdef CONFIG_DARWIN
#include <mach/mach.h>

extern kern_return_t mach_vm_remap(vm_map_t target_task,
                                   mach_vm_address_t *target_address,
                                   mach_vm_size_t size,
                                   mach_vm_offset_t mask,
                                   int flags,
                                   vm_map_t src_task,
                                   mach_vm_address_t src_address,
                                   boolean_t copy,
                                   vm_prot_t *cur_protection,
                                   vm_prot_t *max_protection,
                                   vm_inherit_t inheritance);

static bool alloc_code_gen_buffer_splitwx_vmremap(size_t size, Error **errp)
{
    kern_return_t ret;
    mach_vm_address_t buf_rw, buf_rx;
    vm_prot_t cur_prot, max_prot;

    /* Map the read-write portion via normal anon memory. */
    if (!alloc_code_gen_buffer_anon(size, PROT_READ | PROT_WRITE,
                                    MAP_PRIVATE | MAP_ANONYMOUS, errp)) {
        return false;
    }

    buf_rw = (mach_vm_address_t)tcg_ctx->code_gen_buffer;
    buf_rx = 0;
    ret = mach_vm_remap(mach_task_self(),
                        &buf_rx,
                        size,
                        0,
                        VM_FLAGS_ANYWHERE,
                        mach_task_self(),
                        buf_rw,
                        false,
                        &cur_prot,
                        &max_prot,
                        VM_INHERIT_NONE);
    if (ret != KERN_SUCCESS) {
        /* TODO: Convert "ret" to a human readable error message. */
        error_setg(errp, "vm_remap for jit splitwx failed");
        munmap((void *)buf_rw, size);
        return false;
    }

    if (mprotect((void *)buf_rx, size, PROT_READ | PROT_EXEC) != 0) {
        error_setg_errno(errp, errno, "mprotect for jit splitwx");
        munmap((void *)buf_rx, size);
        munmap((void *)buf_rw, size);
        return false;
    }

    tcg_splitwx_diff = buf_rx - buf_rw;
    return true;
}
#endif /* CONFIG_DARWIN */
#endif /* CONFIG_TCG_INTERPRETER */

static bool alloc_code_gen_buffer_splitwx(size_t size, Error **errp)
{
#ifndef CONFIG_TCG_INTERPRETER
# ifdef CONFIG_DARWIN
    return alloc_code_gen_buffer_splitwx_vmremap(size, errp);
# endif
# ifdef CONFIG_POSIX
    return alloc_code_gen_buffer_splitwx_memfd(size, errp);
# endif
#endif
    error_setg(errp, "jit split-wx not supported");
    return false;
}

static bool alloc_code_gen_buffer(size_t size, int splitwx, Error **errp)
{
    ERRP_GUARD();
    int prot, flags;

    if (splitwx) {
        if (alloc_code_gen_buffer_splitwx(size, errp)) {
            return true;
        }
        /*
         * If splitwx force-on (1), fail;
         * if splitwx default-on (-1), fall through to splitwx off.
         */
        if (splitwx > 0) {
            return false;
        }
        error_free_or_abort(errp);
    }

    prot = PROT_READ | PROT_WRITE | PROT_EXEC;
    flags = MAP_PRIVATE | MAP_ANONYMOUS;
#ifdef CONFIG_TCG_INTERPRETER
    /* The tcg interpreter does not need execute permission. */
    prot = PROT_READ | PROT_WRITE;
#elif defined(CONFIG_DARWIN)
    /* Applicable to both iOS and macOS (Apple Silicon). */
    if (!splitwx) {
        flags |= MAP_JIT;
    }
#endif

    return alloc_code_gen_buffer_anon(size, prot, flags, errp);
}
#endif /* USE_STATIC_CODE_GEN_BUFFER, WIN32, POSIX */

static bool tb_cmp(const void *ap, const void *bp)
{
    const TranslationBlock *a = ap;
    const TranslationBlock *b = bp;

    return a->pc == b->pc &&
        /* a->cs_base == b->cs_base && */
        a->flags == b->flags &&
        (tb_cflags(a) & ~CF_INVALID) == (tb_cflags(b) & ~CF_INVALID) &&
        tb_page_addr0(a) == tb_page_addr0(b) &&
        tb_page_addr1(a) == tb_page_addr1(b);
}

static void tb_htable_init(void)
{
    unsigned int mode = QHT_MODE_AUTO_RESIZE;

    qht_init(&tb_ctx.htable, tb_cmp, CODE_GEN_HTABLE_SIZE, mode);
}

/* Must be called before using the QEMU cpus. 'tb_size' is the size
   (in bytes) allocated to the translation buffer. Zero means default
   size. */
void tcg_exec_init(unsigned long tb_size, int splitwx)
{
    bool ok __attribute__((unused));

    tcg_allowed = true;
    cpu_gen_init();
    page_init();
    tb_htable_init();

    ok = alloc_code_gen_buffer(size_code_gen_buffer(tb_size),
                               splitwx, &error_fatal);
    assert(ok);

    ok = alloc_tb_gen_buffer(size_code_gen_buffer(tb_size));
    if (!ok) {
        option_split_tb = 0;
    }

#if defined(CONFIG_SOFTMMU)
    /* There's no guest base to take into account, so go ahead and
       initialize the prologue now.  */
    tcg_prologue_init(tcg_ctx);
#endif
}

/* call with @p->lock held */
static inline void invalidate_page_bitmap(PageDesc *p)
{
    assert_page_locked(p);
#ifdef CONFIG_SOFTMMU
    g_free(p->code_bitmap);
    p->code_bitmap = NULL;
    p->code_write_count = 0;
#endif
}

#ifdef CONFIG_USER_ONLY
/*
 * For user-only, since we are protecting all of memory with a single lock,
 * and because the two pages of a TranslationBlock are always contiguous,
 * use a single data structure to record all TranslationBlocks.
 */
static IntervalTreeRoot tb_root;

static void tb_remove_all(void)
{
    assert_memory_lock();
    memset(&tb_root, 0, sizeof(tb_root));
#ifdef CONFIG_LATX_AOT
    page_flush_page_state(0, 0);
#endif
}

/* Call with mmap_lock held. */
static void tb_record(TranslationBlock *tb, PageDesc *p1, PageDesc *p2)
{
    target_ulong addr;
    int flags __attribute__((unused));

    assert_memory_lock();

    tb->itree.last = tb->itree.start + tb->size - 1;

    /* translator_loop() must have made all TB pages non-writable */
    addr = tb_page_addr0(tb);
    flags = page_get_flags(addr);
    assert(!(flags & PAGE_WRITE));

    addr = tb_page_addr1(tb);
    if (addr != -1) {
        flags = page_get_flags(addr);
        assert(!(flags & PAGE_WRITE));
    }

    interval_tree_insert(&tb->itree, &tb_root);
}

/* Call with mmap_lock held. */
static void tb_remove(TranslationBlock *tb)
{
    assert_memory_lock();
    interval_tree_remove(&tb->itree, &tb_root);
}

/* TODO: For now, still shared with translate-all.c for system mode. */
#define PAGE_FOR_EACH_TB(start, end, pagedesc, T, N)    \
    for (T = foreach_tb_first(start, end),              \
         N = foreach_tb_next(T, start, end);            \
         T != NULL;                                     \
         T = N, N = foreach_tb_next(N, start, end))

typedef TranslationBlock *PageForEachNext;

static PageForEachNext foreach_tb_first(tb_page_addr_t start,
                                        tb_page_addr_t end)
{
    IntervalTreeNode *n = interval_tree_iter_first(&tb_root, start, end - 1);
    return n ? container_of(n, TranslationBlock, itree) : NULL;
}

static PageForEachNext foreach_tb_next(PageForEachNext tb,
                                       tb_page_addr_t start,
                                       tb_page_addr_t end)
{
    IntervalTreeNode *n;

    if (tb) {
        n = interval_tree_iter_next(&tb->itree, start, end - 1);
        if (n) {
            return container_of(n, TranslationBlock, itree);
        }
    }
    return NULL;
}

#else
/* Set to NULL all the 'first_tb' fields in all PageDescs. */
static void tb_remove_all_1(int level, void **lp)
{
    int i;

    if (*lp == NULL) {
        return;
    }
    if (level == 0) {
        PageDesc *pd = *lp;
        for (i = 0; i < V_L2_SIZE; ++i) {
            page_lock(&pd[i]);
            pd[i].first_tb = (uintptr_t)NULL;
#ifdef CONFIG_LATX_AOT
            if (pd[i].page_state == PAGE_LOADED) {
	        pd[i].page_state = PAGE_FLUSH;
            }
#endif
            invalidate_page_bitmap(pd + i);
            page_unlock(&pd[i]);
        }
    } else {
        void **pp = *lp;

        for (i = 0; i < V_L2_SIZE; ++i) {
            tb_remove_all_1(level - 1, pp + i);
        }
    }
}

static void tb_remove_all(void)
{
    int i, l1_sz = v_l1_size;

    for (i = 0; i < l1_sz; i++) {
        tb_remove_all_1(v_l2_levels, l1_map + i);
    }
}

/* add the tb in the target page and protect it if necessary
 *
 * Called with mmap_lock held for user-mode emulation.
 * Called with @p->lock held in !user-mode.
 */
static inline void tb_page_add(PageDesc *p, TranslationBlock *tb,
                               unsigned int n)
{
    bool page_already_protected;

    assert_page_locked(p);

    tb->page_addr[n] = page_addr;
    tb->page_next[n] = p->first_tb;
    page_already_protected = p->first_tb != 0;

    p->first_tb = (uintptr_t)tb | n;
    invalidate_page_bitmap(p);

    /* if some code is already present, then the pages are already
       protected. So we handle the case where only the first TB is
       allocated in a physical page */
    if (!page_already_protected) {
        tlb_protect_code(tb->page_addr[n] & TARGET_PAGE_MASK);
    }
}

static void tb_record(TranslationBlock *tb, PageDesc *p1, PageDesc *p2)
{
    tb_page_add(p1, tb, 0);
    if (unlikely(p2)) {
        tb_page_add(p2, tb, 1);
    }
}

/*
 * user-mode: call with mmap_lock held
 * !user-mode: call with @pd->lock held
 */
static inline void tb_page_remove(PageDesc *pd, TranslationBlock *tb)
{
    TranslationBlock *tb1;
    uintptr_t *pprev;
    unsigned int n1;

    assert_page_locked(pd);
    pprev = &pd->first_tb;
    PAGE_FOR_EACH_TB(unused, unused, pd, tb1, n1) {
        if (tb1 == tb) {
            *pprev = tb1->page_next[n1];
            return;
        }
        pprev = &tb1->page_next[n1];
    }
    g_assert_not_reached();
}

static void tb_remove(TranslationBlock *tb)
{
    PageDesc *pd;

    pd = page_find(tb->page_addr[0] >> TARGET_PAGE_BITS);
    tb_page_remove(pd, tb);
    if (unlikely(tb->page_addr[1] != -1)) {
        pd = page_find(tb->page_addr[1] >> TARGET_PAGE_BITS);
        tb_page_remove(pd, tb);
    }
}
#endif /* CONFIG_USER_ONLY */

static gboolean tb_host_size_iter(gpointer key, gpointer value, gpointer data)
{
    const TranslationBlock *tb = value;
    size_t *size = data;

    *size += tb->tc.size;
    return false;
}

/* flush all the translation blocks */
void do_tb_flush(CPUState *cpu, run_on_cpu_data tb_flush_count)
{
#ifdef CONFIG_LATX_AOT
    if (option_aot && in_pre_translate) {
	qemu_log_mask(LAT_LOG_AOT, "FIXME: tb flush in pre translate\n");
	_exit(0);
    }
#endif

    bool did_flush = false;

    mmap_lock();
    /* If it is already been done on request of another CPU,
     * just retry.
     */
    if (tb_ctx.tb_flush_count != tb_flush_count.host_int) {
        goto done;
    }
    did_flush = true;

    if (DEBUG_TB_FLUSH_GATE) {
        size_t nb_tbs = tcg_nb_tbs();
        size_t host_size = 0;

        tcg_tb_foreach(tb_host_size_iter, &host_size);
        printf("qemu: flush code_size=%zu nb_tbs=%zu avg_tb_size=%zu\n",
               tcg_code_size(), nb_tbs, nb_tbs > 0 ? host_size / nb_tbs : 0);
    }

    CPU_FOREACH(cpu) {
#ifdef CONFIG_LATX_FAST_JMPCACHE
        latx_fast_jmp_cache_clear_all(cpu);
#endif
        cpu_tb_jmp_cache_clear(cpu);
    }

    qht_reset_size(&tb_ctx.htable, CODE_GEN_HTABLE_SIZE);
    tb_remove_all();

    tcg_region_reset_all();
    /* XXX: flush processor icache at this point if cache flush is
       expensive */
    qatomic_mb_set(&tb_ctx.tb_flush_count, tb_ctx.tb_flush_count + 1);

done:
    mmap_unlock();
    if (did_flush) {
        qemu_plugin_flush_cb();
    }
#if defined(CONFIG_LATX_KZT)
    CPU_FOREACH(cpu) {
        if (cpu && option_kzt) {
            init_tb_callback_bridge(cpu, &info1);
        }
    }
#endif
}

static void gen_aot_and_flush(CPUState *cpu, run_on_cpu_data tb_flush_count)
{
#ifdef CONFIG_LATX_AOT
    aot_exit_entry(cpu, false);
#endif
    do_tb_flush(cpu, tb_flush_count);
}

void tb_flush(CPUState *cpu)
{
    if (tcg_enabled()) {
        unsigned tb_flush_count = qatomic_mb_read(&tb_ctx.tb_flush_count);

        if (cpu_in_exclusive_context(cpu)) {
            /* do_tb_flush(cpu, RUN_ON_CPU_HOST_INT(tb_flush_count)); */
            gen_aot_and_flush(cpu, RUN_ON_CPU_HOST_INT(tb_flush_count));
        } else {
            /* async_safe_run_on_cpu(cpu, do_tb_flush, */
            /*                       RUN_ON_CPU_HOST_INT(tb_flush_count)); */
            async_safe_run_on_cpu(cpu, gen_aot_and_flush,
                                   RUN_ON_CPU_HOST_INT(tb_flush_count));
        }
    }
}

/*
 * Formerly ifdef DEBUG_TB_CHECK. These debug functions are user-mode-only,
 * so in order to prevent bit rot we compile them unconditionally in user-mode,
 * and let the optimizer get rid of them by wrapping their user-only callers
 * with if (DEBUG_TB_CHECK_GATE).
 */
#ifdef CONFIG_USER_ONLY

static void do_tb_page_check(void *p, uint32_t hash, void *userp)
{
    TranslationBlock *tb = p;
    int flags1, flags2;

    flags1 = page_get_flags(tb->pc);
    flags2 = page_get_flags(tb->pc + tb->size - 1);
    if ((flags1 & PAGE_WRITE) || (flags2 & PAGE_WRITE)) {
        printf("ERROR page flags: PC=%08lx size=%04x f1=%x f2=%x\n",
               (long)tb->pc, tb->size, flags1, flags2);
    }
}

/* verify that all the pages have correct rights for code */
static void tb_page_check(void)
{
    qht_iter(&tb_ctx.htable, do_tb_page_check, NULL);
}

#endif /* CONFIG_USER_ONLY */

#include<sys/syscall.h>

/* remove @orig from its @n_orig-th jump list */
static inline void tb_remove_from_jmp_list(TranslationBlock *orig, int n_orig)
{
    uintptr_t ptr, ptr_locked;
    TranslationBlock *dest;
    TranslationBlock *tb;
    uintptr_t *pprev;
    int n;

    /* mark the LSB of jmp_dest[] so that no further jumps can be inserted */
    ptr = qatomic_or_fetch(&orig->jmp_dest[n_orig], 1);
    dest = (TranslationBlock *)(ptr & ~1);
    if (dest == NULL) {
        return;
    }

    qemu_spin_lock(&dest->jmp_lock);
    /*
     * While acquiring the lock, the jump might have been removed if the
     * destination TB was invalidated; check again.
     */
    ptr_locked = qatomic_read(&orig->jmp_dest[n_orig]);
    if (ptr_locked != ptr) {
        qemu_spin_unlock(&dest->jmp_lock);
        /*
         * The only possibility is that the jump was unlinked via
         * tb_jump_unlink(dest). Seeing here another destination would be a bug,
         * because we set the LSB above.
         */
        g_assert(ptr_locked == 1 && dest->cflags & CF_INVALID);
        return;
    }
    /*
     * We first acquired the lock, and since the destination pointer matches,
     * we know for sure that @orig is in the jmp list.
     */
    pprev = &dest->jmp_list_head;
    TB_FOR_EACH_JMP(dest, tb, n) {
        if (tb == orig && n == n_orig) {
            *pprev = tb->jmp_list_next[n];
            /* no need to set orig->jmp_dest[n]; setting the LSB was enough */
            qemu_spin_unlock(&dest->jmp_lock);
            return;
        }
        pprev = &tb->jmp_list_next[n];
    }
    g_assert_not_reached();
}

/* reset the jump entry 'n' of a TB so that it is not chained to
   another TB */
void tb_reset_jump(TranslationBlock *tb, int n)
{
    assert(!use_tu_jmp(tb));
    uintptr_t addr = (uintptr_t)(tb->tc.ptr + tb->jmp_reset_offset[n]);
    tb_set_jmp_target(tb, n, addr);
#ifdef CONFIG_LATX_INSTS_PATTERN
    if (tb->eflags_target_arg[n] !=
        TB_JMP_RESET_OFFSET_INVALID) {
        tb_eflag_recover(tb, n);
    }
#endif
#ifdef CONFIG_LATX_XCOMISX_OPT
    if (tb->jmp_stub_reset_offset[n] != TB_JMP_RESET_OFFSET_INVALID) {
        uintptr_t offset = tb->jmp_stub_target_arg[n];
        uintptr_t tc_ptr = (uintptr_t)tb->tc.ptr;
        uintptr_t jmp_rx = tc_ptr + offset;
        uintptr_t jmp_rw = jmp_rx - tcg_splitwx_diff;
        tb_target_set_nop(tc_ptr, jmp_rx, jmp_rw, addr);
    }
#endif
}

/* remove any jumps to the TB */
static inline void tb_jmp_unlink(TranslationBlock *dest)
{
    TranslationBlock *tb;
    int n;

    qemu_spin_lock(&dest->jmp_lock);

    TB_FOR_EACH_JMP(dest, tb, n) {
        tb_reset_jump(tb, n);
        qatomic_and(&tb->jmp_dest[n], (uintptr_t)NULL | 1);
        /* No need to clear the list entry; setting the dest ptr is enough */
    }
    dest->jmp_list_head = (uintptr_t)NULL;

    qemu_spin_unlock(&dest->jmp_lock);
}

/*
 * In user-mode, call with mmap_lock held.
 * In !user-mode, if @rm_from_page_list is set, call with the TB's pages'
 * locks held.
 */
static void do_tb_phys_invalidate(TranslationBlock *tb, bool rm_from_page_list)
{
    CPUState *cpu;
    uint32_t h;
    tb_page_addr_t phys_pc;
    uint32_t orig_cflags = tb_cflags(tb);

    assert_memory_lock();

    /* make sure no further incoming jumps will be chained to this TB */
    qemu_spin_lock(&tb->jmp_lock);
    qatomic_set(&tb->cflags, tb->cflags | CF_INVALID);
    qemu_spin_unlock(&tb->jmp_lock);

    /* remove the TB from the hash list */
    phys_pc = tb_page_addr0(tb);

    h = tb_hash_func(phys_pc, tb->pc, tb->flags, orig_cflags, 0);
    if (!qht_remove(&tb_ctx.htable, tb, h)) {
        return;
    }

    /* remove the TB from the page list */
    if (rm_from_page_list) {
        tb_remove(tb);
    }

    /* remove the TB from the hash list */
    h = tb_jmp_cache_hash_func(tb->pc);
    CPU_FOREACH(cpu) {
        if (qatomic_read(&cpu->tb_jmp_cache[h]) == tb) {
#ifdef CONFIG_LATX_FAST_JMPCACHE
            latx_fast_jmp_cache_clear(cpu, h);
#endif
            qatomic_set(&cpu->tb_jmp_cache[h], NULL);
        }
    }

    /* suppress this TB from the two jump lists */
    tb_remove_from_jmp_list(tb, 0);
    tb_remove_from_jmp_list(tb, 1);

    /* suppress any remaining jumps to this TB */
    tb_jmp_unlink(tb);

    qatomic_set(&tcg_ctx->tb_phys_invalidate_count,
               tcg_ctx->tb_phys_invalidate_count + 1);

#ifdef CONFIG_LATX_JRRA
    if (option_jr_ra) {
        qatomic_set((uint32_t *)tb->tc.ptr, SMC_ILL_INST);
        flush_idcache_range((uintptr_t)tb->tc.ptr, (uintptr_t)tb->tc.ptr, 4);
    }
#endif
}

static void tb_phys_invalidate__locked(TranslationBlock *tb)
{
    qemu_thread_jit_write();
    do_tb_phys_invalidate(tb, true);
    qemu_thread_jit_execute();
}

/* invalidate one TB
 *
 * Called with mmap_lock held in user-mode.
 */
void tb_phys_invalidate(TranslationBlock *tb, tb_page_addr_t page_addr)
{
    if (page_addr == -1 && tb_page_addr0(tb) != -1) {
        page_lock_tb(tb);
        do_tb_phys_invalidate(tb, true);
        page_unlock_tb(tb);
    } else {
        do_tb_phys_invalidate(tb, false);
    }
}

#ifdef CONFIG_SOFTMMU
/* call with @p->lock held */
static void build_page_bitmap(PageDesc *p)
{
    int n, tb_start, tb_end;
    TranslationBlock *tb;

    assert_page_locked(p);
    p->code_bitmap = bitmap_new(TARGET_PAGE_SIZE);

    PAGE_FOR_EACH_TB(p, tb, n) {
        /* NOTE: this is subtle as a TB may span two physical pages */
        if (n == 0) {
            /* NOTE: tb_end may be after the end of the page, but
               it is not a problem */
            tb_start = tb->pc & ~TARGET_PAGE_MASK;
            tb_end = tb_start + tb->size;
            if (tb_end > TARGET_PAGE_SIZE) {
                tb_end = TARGET_PAGE_SIZE;
             }
        } else {
            tb_start = 0;
            tb_end = ((tb->pc + tb->size) & ~TARGET_PAGE_MASK);
        }
        bitmap_set(p->code_bitmap, tb_start, tb_end - tb_start);
    }
}
#endif

/*
 * Add a new TB and link it to the physical page tables. phys_page2 is
 * (-1) to indicate that only one page contains the TB.
 *
 * Called with mmap_lock held for user-mode emulation.
 *
 * Returns a pointer @tb, or a pointer to an existing TB that matches @tb.
 * Note that in !user-mode, another thread might have already added a TB
 * for the same block of guest code that @tb corresponds to. In that case,
 * the caller should discard the original @tb, and use instead the returned TB.
 */
TranslationBlock *
tb_link_page(TranslationBlock *tb, tb_page_addr_t phys_pc,
             tb_page_addr_t phys_page2)
{
    PageDesc *p;
    PageDesc *p2 = NULL;
    void *existing_tb = NULL;
    uint32_t h;

    assert_memory_lock();
    tcg_debug_assert(!(tb->cflags & CF_INVALID));

    /*
     * Add the TB to the page list, acquiring first the pages's locks.
     * We keep the locks held until after inserting the TB in the hash table,
     * so that if the insertion fails we know for sure that the TBs are still
     * in the page descriptors.
     * Note that inserting into the hash table first isn't an option, since
     * we can only insert TBs that are fully initialized.
     */
    page_lock_pair(&p, phys_pc, &p2, phys_page2, 1);
    tb_set_page_addr0(tb, phys_pc);
    page_protect(phys_pc);
    if (phys_page2 != -1) {
        tb_set_page_addr1(tb, phys_page2);
        page_protect(phys_page2);
    } else {
        tb_set_page_addr1(tb, -1);
    }
    tb_record(tb, p, p2);

    /* add in the hash table */
    h = tb_hash_func(phys_pc, tb->pc, tb->flags, tb->cflags, 0);
    qht_insert(&tb_ctx.htable, tb, h, &existing_tb);

    /* remove TB from the page(s) if we couldn't insert it */
    if (unlikely(existing_tb)) {
        tb_remove(tb);
        tb = existing_tb;
    }

    if (p2 && p2 != p) {
        page_unlock(p2);
    }
    page_unlock(p);

#ifdef CONFIG_USER_ONLY
    if (DEBUG_TB_CHECK_GATE) {
        tb_page_check();
    }
#endif
    return tb;
}

#ifdef CONFIG_LATX_TBMINI_ENABLE
static inline void tbmini_set_pointer(uint64_t* tbm, uint64_t tb_addr)
{
    *tbm = (tb_addr & MAKE_64BIT_MASK(0, HOST_VIRT_ADDR_SPACE_BITS)) |
                (TB_MAGIC << HOST_VIRT_ADDR_SPACE_BITS);
}
#endif

/* Called with mmap_lock held for user mode emulation.  */
TranslationBlock *tb_gen_code(CPUState *cpu,
                              target_ulong pc, target_ulong cs_base,
                              uint32_t flags, int cflags)
{
    CPUArchState *env = cpu->env_ptr;
    TranslationBlock *tb, *existing_tb;
    tb_page_addr_t phys_pc, phys_page2;
    target_ulong virt_page2;
    tcg_insn_unit *gen_code_buf;
    int max_insns;
    int gen_code_size, search_size;
#ifdef CONFIG_PROFILER
    TCGProfile *prof = &tcg_ctx->prof;
    int64_t ti;
#endif
    void *host_pc;

    assert_memory_lock();
    qemu_thread_jit_write();

    phys_pc = get_page_addr_code_hostp(env, pc, &host_pc);

    if (phys_pc == -1) {
        /* Generate a one-shot TB with 1 insn in it */
        cflags = (cflags & ~CF_COUNT_MASK) | CF_LAST_IO | 1;
    }

    max_insns = cflags & CF_COUNT_MASK;
    if (max_insns == 0) {
        max_insns = CF_COUNT_MASK;
    }
    if (max_insns > TCG_MAX_INSNS) {
        max_insns = TCG_MAX_INSNS;
    }
    if (cpu->singlestep_enabled || singlestep) {
        max_insns = 1;
    }

 buffer_overflow:

#ifdef CONFIG_LATX_AOT
    if (!in_pre_translate) {
        tb = tcg_tb_alloc(tcg_ctx);
    } else {
        tb = tcg_tb_alloc_full(tcg_ctx);
    }
#else
    tb = tcg_tb_alloc(tcg_ctx);
#endif

    if (unlikely(!tb)) {
        /* flush must be done */
        tb_flush(cpu);
        mmap_unlock();
        /* Make the execution loop process the flush as soon as possible.  */
        cpu->exception_index = EXCP_INTERRUPT;
        cpu_loop_exit(cpu);
    }

    gen_code_buf = tcg_ctx->code_gen_ptr;
    tb->tc.ptr = tcg_splitwx_to_rx(gen_code_buf);
    tb->pc = pc;
    /* tb->cs_base = cs_base; */
    tb->flags = flags;
    tb->cflags = cflags;
    tb->jmp_target_arg[0] = TB_JMP_RESET_OFFSET_INVALID;
    tb->jmp_target_arg[1] = TB_JMP_RESET_OFFSET_INVALID;
    tcg_ctx->tb_cflags = cflags;
#ifndef CONFIG_LATX
 tb_overflow:
#else
    tb->bool_flags = OPT_BCC;
    tb->s_data->_top_out = -1;
    tb->s_data->_top_in = -1;
#ifdef CONFIG_LATX_AOT
    tb->s_data->rel_start = -1;
    tb->s_data->rel_end = -1;
#endif
#ifdef CONFIG_LATX_PROFILER
    CLN_TB_PROFILE(tb);
#endif
#ifdef CONFIG_LATX_JRRA
    tb->next_86_pc = 0;
    tb->return_target_ptr = NULL;
#endif
#ifdef CONFIG_LATX_SMC_OPT
    if (smc_retrans_lookup(pc)) {
        tb->smc_data |= TBSMC_OPTED_MASK;
        smc_retrans_remove(pc);
    }
#endif
#endif

#ifndef CONFIG_LATX
#ifdef CONFIG_PROFILER
    /* includes aborted translations because of exceptions */
    qatomic_set(&prof->tb_count1, prof->tb_count1 + 1);
    ti = profile_getclock();
#endif

    /* TODO: new */
    gen_code_size = sigsetjmp(tcg_ctx->jmp_trans, 0);
    if (unlikely(gen_code_size != 0)) {
        goto error_return;
    }

    tcg_func_start(tcg_ctx);

    tcg_ctx->cpu = env_cpu(env);
    gen_intermediate_code(cpu, tb, max_insns, pc, host_pc);
    tcg_ctx->cpu = NULL;
    max_insns = tb->icount;

    trace_translate_block(tb, tb->pc, tb->tc.ptr);
#endif

    /* generate machine code */
    tb->jmp_reset_offset[0] = TB_JMP_RESET_OFFSET_INVALID;
    tb->jmp_reset_offset[1] = TB_JMP_RESET_OFFSET_INVALID;
    tb->jmp_stub_reset_offset[0] = TB_JMP_RESET_OFFSET_INVALID;
    tb->jmp_stub_reset_offset[1] = TB_JMP_RESET_OFFSET_INVALID;
    tb->jmp_indirect = TB_JMP_RESET_OFFSET_INVALID;
#ifdef CONFIG_LATX_INSTS_PATTERN
    tb->eflags_target_arg[0] = TB_JMP_RESET_OFFSET_INVALID;
    tb->eflags_target_arg[1] = TB_JMP_RESET_OFFSET_INVALID;
    tb->eflags_target_arg[2] = TB_JMP_RESET_OFFSET_INVALID;
#endif
#ifdef CONFIG_LATX_TU
    tu_reset_tb(tb);
    tb->s_data->tu_tb_mode = TB_GEN_CODE;
#endif
#ifdef CONFIG_LATX
    clear_signal_link_flag(tb, 0);
    clear_signal_link_flag(tb, 1);
    tb->first_jmp_align = TB_JMP_RESET_OFFSET_INVALID;
#endif
    tcg_ctx->tb_jmp_reset_offset = tb->jmp_reset_offset;
    if (TCG_TARGET_HAS_direct_jump) {
        tcg_ctx->tb_jmp_insn_offset = tb->jmp_target_arg;
        tcg_ctx->tb_jmp_target_addr = NULL;
    } else {
        tcg_ctx->tb_jmp_insn_offset = NULL;
        tcg_ctx->tb_jmp_target_addr = tb->jmp_target_arg;
    }

#ifndef CONFIG_LATX
#ifdef CONFIG_PROFILER
    qatomic_set(&prof->tb_count, prof->tb_count + 1);
    qatomic_set(&prof->interm_time,
                prof->interm_time + profile_getclock() - ti);
    ti = profile_getclock();
#endif

    gen_code_size = tcg_gen_code(tcg_ctx, tb);
    if (unlikely(gen_code_size < 0)) {
 error_return:
        switch (gen_code_size) {
        case -1:
            /*
             * Overflow of code_gen_buffer, or the current slice of it.
             *
             * TODO: We don't need to re-do gen_intermediate_code, nor
             * should we re-do the tcg optimization currently hidden
             * inside tcg_gen_code.  All that should be required is to
             * flush the TBs, allocate a new TB, re-initialize it per
             * above, and re-do the actual code generation.
             */
            qemu_log_mask(CPU_LOG_TB_OP | CPU_LOG_TB_OP_OPT,
                          "Restarting code generation for "
                          "code_gen_buffer overflow\n");
            goto buffer_overflow;

        case -2:
            /*
             * The code generated for the TranslationBlock is too large.
             * The maximum size allowed by the unwind info is 64k.
             * There may be stricter constraints from relocations
             * in the tcg backend.
             *
             * Try again with half as many insns as we attempted this time.
             * If a single insn overflows, there's a bug somewhere...
             */
            assert(max_insns > 1);
            max_insns /= 2;
            qemu_log_mask(CPU_LOG_TB_OP | CPU_LOG_TB_OP_OPT,
                          "Restarting code generation with "
                          "smaller translation block (max %d insns)\n",
                          max_insns);
            goto tb_overflow;

        default:
            g_assert_not_reached();
        }
    }
    search_size = encode_search(tb, (void *)gen_code_buf + gen_code_size);
    if (unlikely(search_size < 0)) {
        goto buffer_overflow;
    }

    tb->tc.size = gen_code_size;

#ifdef CONFIG_PROFILER
    qatomic_set(&prof->code_time, prof->code_time + profile_getclock() - ti);
    qatomic_set(&prof->code_in_len, prof->code_in_len + tb->size);
    qatomic_set(&prof->code_out_len, prof->code_out_len + gen_code_size);
    qatomic_set(&prof->search_out_len, prof->search_out_len + search_size);
#endif

#ifdef DEBUG_DISAS
    if (qemu_loglevel_mask(CPU_LOG_TB_OUT_ASM) &&
        qemu_log_in_addr_range(tb->pc)) {
        FILE *logfile = qemu_log_lock();
        int code_size, data_size;
        const tcg_target_ulong *rx_data_gen_ptr;
        size_t chunk_start;
        int insn = 0;

        if (tcg_ctx->data_gen_ptr) {
            rx_data_gen_ptr = tcg_splitwx_to_rx(tcg_ctx->data_gen_ptr);
            code_size = (const void *)rx_data_gen_ptr - tb->tc.ptr;
            data_size = gen_code_size - code_size;
        } else {
            rx_data_gen_ptr = 0;
            code_size = gen_code_size;
            data_size = 0;
        }

        /* Dump header and the first instruction */
        qemu_log("OUT: [size=%d]\n", gen_code_size);
        qemu_log("  -- guest addr 0x" TARGET_FMT_lx " + tb prologue\n",
                 tcg_ctx->gen_insn_data[insn][0]);
        chunk_start = tcg_ctx->gen_insn_end_off[insn];
        log_disas(tb->tc.ptr, chunk_start);

        /*
         * Dump each instruction chunk, wrapping up empty chunks into
         * the next instruction. The whole array is offset so the
         * first entry is the beginning of the 2nd instruction.
         */
        while (insn < tb->icount) {
            size_t chunk_end = tcg_ctx->gen_insn_end_off[insn];
            if (chunk_end > chunk_start) {
                qemu_log("  -- guest addr 0x" TARGET_FMT_lx "\n",
                         tcg_ctx->gen_insn_data[insn][0]);
                log_disas(tb->tc.ptr + chunk_start, chunk_end - chunk_start);
                chunk_start = chunk_end;
            }
            insn++;
        }

        if (chunk_start < code_size) {
            qemu_log("  -- tb slow paths + alignment\n");
            log_disas(tb->tc.ptr + chunk_start, code_size - chunk_start);
        }

        /* Finally dump any data we may have after the block */
        if (data_size) {
            int i;
            qemu_log("  data: [size=%d]\n", data_size);
            for (i = 0; i < data_size / sizeof(tcg_target_ulong); i++) {
                qemu_log("0x%08" PRIxPTR ":  .quad  0x%" TCG_PRIlx "\n",
                         (uintptr_t)&rx_data_gen_ptr[i], rx_data_gen_ptr[i]);
            }
        }
        qemu_log("\n");
        qemu_log_flush();
        qemu_log_unlock(logfile);
    }
#endif

#else /* CONFIG_LATX */
#ifdef CONFIG_PROFILER
    /* TODO: tb_count1 */
    qatomic_set(&prof->tb_count1, prof->tb_count1 + 1);
    qatomic_set(&prof->tb_count, prof->tb_count + 1);
    ti = profile_getclock();
#endif

    /*
     * remove write prot of the page in case of paralell code modification,
     * but the paired page cross 16K boundary is still not protected.
     */
    int p_flags = page_get_flags(pc);
    if ((p_flags & PAGE_WRITE) && !is_shadow_page_not_shmm(pc)) {
        mprotect((void*)(pc & qemu_host_page_mask), qemu_host_page_size, PROT_READ);
    }

#ifdef CONFIG_LATX_MONITOR_SHARED_MEM
    if (option_monitor_shared_mem) {
        tb->checksum = p_flags & PAGE_MEMSHARE;
    }
#endif
    gen_code_size = target_latx_host(env, tb, max_insns);
    if (unlikely(gen_code_size < 0)) {
        /*
         * Overflow of code_gen_buffer, or the current slice of it.
         *
         * TODO: We don't need to re-do gen_intermediate_code, nor
         * should we re-do the tcg optimization currently hidden
         * inside tcg_gen_code.  All that should be required is to
         * flush the TBs, allocate a new TB, re-initialize it per
         * above, and re-do the actual code generation.
         */
        goto buffer_overflow;
    }

    if (gen_code_size == 0) {
        return NULL;
    }

    /*
     * To handle segv case, every insn has encode data at the end of tb
     * Calculate the search_size and set the new gen_code_buf
     */
    search_size = encode_search(tb, (void *)gen_code_buf + gen_code_size);
    if (unlikely(search_size < 0)) {
        qatomic_set(&tcg_ctx->code_gen_ptr, tcg_ctx->code_gen_highwater + 1);
        goto buffer_overflow;
    }
    /*
     * The tail of each TB's code buffer stores the TBMini structure of
     * the next TB.
     *
     *                  dcache
     *                / aligned
     *    +-----------+--------+
     *    |   codes   | TBMini |
     *    +-----------+--------+
     *                  ^  ^
     *          magic  /   | 48 bits
     *          number    TB pointer
     */
#if defined(CONFIG_LATX) && defined(CONFIG_LATX_TBMINI_ENABLE)
    /* set TBMini */
    uint64_t *tbm;
    tbm = (uint64_t *)((uintptr_t)gen_code_buf - sizeof(struct TBMini));
    tbmini_set_pointer(tbm, (uint64_t)tb);
#endif
    tb->tc.size = gen_code_size;

#ifdef CONFIG_LATX_AOT
    if (option_aot) {
        tb->s_data->tu_size = ROUND_UP(gen_code_size + search_size, CODE_GEN_ALIGN);
    }
#endif

#ifdef CONFIG_PROFILER
    qatomic_set(&prof->code_time, prof->code_time + profile_getclock() - ti);
    qatomic_set(&prof->code_in_len, prof->code_in_len + tb->size);
    qatomic_set(&prof->code_out_len, prof->code_out_len + gen_code_size);
    qatomic_set(&prof->search_out_len, prof->search_out_len + search_size);
#endif
#endif

    uintptr_t sptr = (uintptr_t)gen_code_buf + gen_code_size;
#if defined(CONFIG_LATX) && defined(CONFIG_LATX_TBMINI_ENABLE)
    sptr = sptr + sizeof(struct TBMini);
#endif
    qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
        ROUND_UP(sptr + search_size, CODE_GEN_ALIGN));

    /* init jump list */
    qemu_spin_init(&tb->jmp_lock);
    tb->jmp_list_head = (uintptr_t)NULL;
    tb->jmp_list_next[0] = (uintptr_t)NULL;
    tb->jmp_list_next[1] = (uintptr_t)NULL;
    tb->jmp_dest[0] = (uintptr_t)NULL;
    tb->jmp_dest[1] = (uintptr_t)NULL;

    assert(!use_tu_jmp(tb));
    /* init original jump addresses which have been set during tcg_gen_code() */
    if (tb->jmp_reset_offset[0] != TB_JMP_RESET_OFFSET_INVALID) {
        tb_reset_jump(tb, 0);
    }
    if (tb->jmp_reset_offset[1] != TB_JMP_RESET_OFFSET_INVALID) {
        tb_reset_jump(tb, 1);
    }

    /*
     * If the TB is not associated with a physical RAM page then
     * it must be a temporary one-insn TB, and we have nothing to do
     * except fill in the page_addr[] fields. Return early before
     * attempting to link to other TBs or add to the lookup table.
     */
    if (phys_pc == -1) {
        tb_set_page_addr0(tb, -1);
        tb_set_page_addr1(tb, -1);
        return tb;
    }

    /* check next page if needed */
    virt_page2 = (pc + tb->size - 1) & TARGET_PAGE_MASK;
    phys_page2 = -1;
    if ((pc & TARGET_PAGE_MASK) != virt_page2) {
        phys_page2 = get_page_addr_code(env, virt_page2);
    }

#ifdef CONFIG_LATX
    flush_idcache_range(0, 0, 0);
#endif

#ifdef CONFIG_LATX_AOT
    if (in_pre_translate) {
        return tb;
    }
#endif

    tcg_tb_insert(tb);

    /*
     * No explicit memory barrier is required -- tb_link_page() makes the
     * TB visible in a consistent state.
     */
    existing_tb = tb_link_page(tb, phys_pc, phys_page2);
    /* if the TB already exists, discard what we just translated */
    if (unlikely(existing_tb != tb)) {
        uintptr_t orig_aligned = (uintptr_t)gen_code_buf;

        if (!option_split_tb) {
            orig_aligned -= ROUND_UP(sizeof(*tb), qemu_icache_linesize);
        }

        qatomic_set(&tcg_ctx->code_gen_ptr, (void *)orig_aligned);
        tb_destroy(tb);
        tcg_tb_remove(tb);
        return existing_tb;
    }
    return tb;
}

#ifdef CONFIG_LATX_AOT
void aot_tb_register(TranslationBlock *tb)
{
    TranslationBlock *existing_tb;
    tb_page_addr_t phys_pc, phys_page2;
    target_ulong virt_page2;

    target_ulong pc = tb->pc;
    phys_pc = pc;

    /* check next page if needed */
    virt_page2 = (pc + tb->size - 1) & TARGET_PAGE_MASK;
    phys_page2 = -1;
    if ((pc & TARGET_PAGE_MASK) != virt_page2) {
        phys_page2 = virt_page2;
    }
    /*
     * No explicit memory barrier is required -- tb_link_page() makes the
     * TB visible in a consistent state.
     */
    existing_tb = tb_link_page(tb, phys_pc, phys_page2);
    /* if the TB already exists, discard what we just translated */
    if (existing_tb != tb) {
#ifdef CONFIG_LATX_DEBUG
        fprintf(stderr, "aot_register_tb failed due to tb_link page\n");
#endif
    }
    tcg_tb_insert(tb);
}

#endif

#ifdef CONFIG_USER_ONLY
/*
 * Invalidate all TBs which intersect with the target address range.
 * Called with mmap_lock held for user-mode emulation.
 * NOTE: this function must not be called while a TB is running.
 */
void tb_invalidate_phys_range(tb_page_addr_t start, tb_page_addr_t end)
{
    TranslationBlock *tb;
    PageForEachNext n;

    assert_memory_lock();

    PAGE_FOR_EACH_TB(start, end, unused, tb, n) {
        tb_phys_invalidate__locked(tb);
    }

#ifdef CONFIG_LATX_AOT
    page_flush_page_state(start, end);
#endif
}

/*
 * Invalidate all TBs which intersect with the target address page @addr.
 * Called with mmap_lock held for user-mode emulation
 * NOTE: this function must not be called while a TB is running.
 */
void tb_invalidate_phys_page(tb_page_addr_t addr)
{
    tb_page_addr_t start, end;

    start = addr & TARGET_PAGE_MASK;
    end = start + TARGET_PAGE_SIZE;
    tb_invalidate_phys_range(start, end);
}

static inline bool tb_is_covered(TranslationBlock *tb, uint64_t addr, int size)
{
    /*
     *  addr           |     addr      |         addr    |  addr
     *  | size         |     | size    |         | size  |  | size
     *  +++++          |     +++++     |         +++++   |  +++++++++++++
     *     +++++++++   |   +++++++++   |   +++++++++     |    +++++++++
     *     | tb->size  |   | tb->size  |   | tb->size    |    | tb->size
     *    tb->pc       |  tb->pc       |  tb->pc         |   tb->pc
     */
    return (addr + size - 1) >= (tb->pc) && (tb->pc + tb->size - 1) >= addr;
}

/*
 * Called with mmap_lock held. If pc is not 0 then it indicates the
 * host PC of the faulting store instruction that caused this invalidate.
 * Returns true if the caller needs to abort execution of the current
 * TB (because it was modified by this store and the guest CPU has
 * precise-SMC semantics).
 *
 * When @tbcount != NULL means we should invalidate only the specific TB
 * that might be modified by the store inst.
 * The @addr is the memory address of the store.
 * The *tbcount is the size of the store inst.
 * The @pc points to the store inst in code cache. (+ GETPC_ADJ)
 *
 * When return, the *tbcount indicates that wether there are TB in the page,
 * which means the page is writable or not.
 */
bool tb_invalidate_phys_page_unwind(tb_page_addr_t addr, uintptr_t pc, int *tbcount)
{
    assert(pc != 0);
#ifdef TARGET_HAS_PRECISE_SMC
    assert_memory_lock();
    {
        TranslationBlock *current_tb = tcg_tb_lookup(pc);
        bool current_tb_modified = false;
        TranslationBlock *tb;
        PageForEachNext n;
        uint64_t smcaddr = 0;
        int smcsize = 0;
        bool smc_crosspage = false;

        uint32_t inst = 0;

        if (tbcount) {
            smcaddr = addr;
            smcsize = *tbcount;
            *tbcount = 0;
            smc_crosspage = ((addr) & TARGET_PAGE_MASK) !=
                            ((addr + smcsize - 1) & TARGET_PAGE_MASK);
        }
        addr &= TARGET_PAGE_MASK;

#ifdef CONFIG_LATX_AOT
        if (option_aot) {
            if (segment_tree_winepe_lookup(addr)) {
                return false;
            }
        }
#endif

        /* start from the 2nd page when cross page */
        if (smc_crosspage) {
            addr += TARGET_PAGE_SIZE;
        }

foreach_tb_start:

        PAGE_FOR_EACH_TB(addr, addr + TARGET_PAGE_SIZE, unused, tb, n) {
            if (tbcount && !tb_is_covered(tb, smcaddr, smcsize)) {
                // this TB is not affected, mark it and continue
                *tbcount |= 0x1;
                continue;
            }
            if (current_tb == tb &&
                (tb_cflags(current_tb) & CF_COUNT_MASK) != 1) {
                /*
                 * If we are modifying the current TB, we must stop its
                 * execution. We could be more precise by checking that
                 * the modification is after the current PC, but it would
                 * require a specialized function to partially restore
                 * the CPU state.
                 */
                current_tb_modified = true;
                cpu_restore_state_from_tb(current_cpu, current_tb, pc, true);
            }
            if (option_jr_ra && current_tb == tb && !current_tb_modified) {
                inst = qatomic_read((uint32_t *)tb->tc.ptr);
            }
            tb_phys_invalidate__locked(tb);
            if (option_jr_ra && current_tb == tb && !current_tb_modified) {
                qatomic_set((uint32_t *)tb->tc.ptr, inst);
                flush_idcache_range((uintptr_t)tb->tc.ptr, (uintptr_t)tb->tc.ptr, 4);
            }
        }

        /* back to handle the 1st page when cross page */
        if (smc_crosspage) {
            addr -= TARGET_PAGE_SIZE;
            smc_crosspage = false;
            *tbcount = *tbcount << 1;
            goto foreach_tb_start;
        }

        /*
         * At here, the *tbcount indicate that if there are TB in the page.
         * The bit[0] for 1st page and bit[1] for 2nd page when cross.
         */

        if (current_tb_modified) {
            /* Force execution of one insn next time.  */
            CPUState *cpu = current_cpu;
            cpu->cflags_next_tb = 1 | curr_cflags(current_cpu);
            return true;
        }
    }
#else
    tb_invalidate_phys_page(addr);
#endif /* TARGET_HAS_PRECISE_SMC */
    return false;
}

#else

/*
 * @p must be non-NULL.
 * user-mode: call with mmap_lock held.
 * !user-mode: call with all @pages locked.
 */
static void
tb_invalidate_phys_page_range__locked(struct page_collection *pages,
                                      PageDesc *p, tb_page_addr_t start,
                                      tb_page_addr_t end,
                                      uintptr_t retaddr)
{
    TranslationBlock *tb;
    tb_page_addr_t tb_start, tb_end;
    PageForEachNext n;
#ifdef TARGET_HAS_PRECISE_SMC
    CPUArchState *env = NULL;
    bool current_tb_modified = false;
    TranslationBlock *current_tb = retaddr ? tcg_tb_lookup(retaddr) : NULL;
    target_ulong current_pc = 0;
    target_ulong current_cs_base = 0;
    uint32_t current_flags = 0;
#endif /* TARGET_HAS_PRECISE_SMC */

#if defined(TARGET_HAS_PRECISE_SMC)
    if (cpu != NULL) {
        env = cpu->env_ptr;
    }
#endif

    /* we remove all the TBs in the range [start, end[ */
    /* XXX: see if in some cases it could be faster to invalidate all
       the code */
    PAGE_FOR_EACH_TB(start, end, p, tb, n) {
        /* NOTE: this is subtle as a TB may span two physical pages */
        if (n == 0) {
            /* NOTE: tb_end may be after the end of the page, but
               it is not a problem */
            tb_start = tb_page_addr0(tb);
            tb_end = tb_start + tb->size;
        } else {
            tb_start = tb_page_addr1(tb);
            tb_end = tb_start + ((tb_page_addr0(tb) + tb->size)
                                 & ~TARGET_PAGE_MASK);
        }
        if (!(tb_end <= start || tb_start >= end)) {
#ifdef TARGET_HAS_PRECISE_SMC
            if (current_tb == tb &&
                (tb_cflags(current_tb) & CF_COUNT_MASK) != 1) {
                /*
                 * If we are modifying the current TB, we must stop
                 * its execution. We could be more precise by checking
                 * that the modification is after the current PC, but it
                 * would require a specialized function to partially
                 * restore the CPU state.
                 */
                current_tb_modified = true;
                cpu_restore_state_from_tb(current_cpu, current_tb, retaddr, true);
                cpu_get_tb_cpu_state(env, &current_pc, &current_cs_base,
                                     &current_flags);
            }
#endif /* TARGET_HAS_PRECISE_SMC */
            tb_phys_invalidate__locked(tb);
        }
    }

    /* if no code remaining, no need to continue to use slow writes */
    if (!p->first_tb) {
        invalidate_page_bitmap(p);
        tlb_unprotect_code(start);
    }

#ifdef TARGET_HAS_PRECISE_SMC
    if (current_tb_modified) {
        page_collection_unlock(pages);
        /* Force execution of one insn next time.  */
        current_cpu->cflags_next_tb = 1 | curr_cflags(current_cpu);
        mmap_unlock();
        cpu_loop_exit_noexc(current_cpu);
    }
#endif
}

/*
 * Invalidate all TBs which intersect with the target physical
 * address page @addr.
 *
 * Called with mmap_lock held for user-mode emulation
 */
void tb_invalidate_phys_page(tb_page_addr_t addr)
{
    struct page_collection *pages;
    tb_page_addr_t start, end;
    PageDesc *p;

    p = page_find(addr >> TARGET_PAGE_BITS);
    if (p == NULL) {
        return;
    }

    start = addr & TARGET_PAGE_MASK;
    end = start + TARGET_PAGE_SIZE;
    pages = page_collection_lock(start, end);
    tb_invalidate_phys_page_range__locked(pages, p, start, end, 0);
    page_collection_unlock(pages);
}

/*
 * Invalidate all TBs which intersect with the target physical address range
 * [start;end[. NOTE: start and end may refer to *different* physical pages.
 * 'is_cpu_write_access' should be true if called from a real cpu write
 * access: the virtual CPU will exit the current TB if code is modified inside
 * this TB.
 *
 * Called with mmap_lock held for user-mode emulation.
 */
#ifdef CONFIG_SOFTMMU
void tb_invalidate_phys_range(ram_addr_t start, ram_addr_t end)
#else
void tb_invalidate_phys_range(target_ulong start, target_ulong end)
#endif
{
    struct page_collection *pages;
    tb_page_addr_t next;

    pages = page_collection_lock(start, end);
    for (next = (start & TARGET_PAGE_MASK) + TARGET_PAGE_SIZE;
         start < end;
         start = next, next += TARGET_PAGE_SIZE) {
        PageDesc *pd = page_find(start >> TARGET_PAGE_BITS);
        tb_page_addr_t bound = MIN(next, end);

        if (pd == NULL) {
            continue;
        }
#ifdef CONFIG_LATX_AOT
        if (pd->page_state == PAGE_LOADED) {
            pd->page_state = PAGE_FLUSH;
        }
#endif
        assert_page_locked(pd);
        tb_invalidate_phys_page_range__locked(pages, pd, start, bound, 0);
    }
    page_collection_unlock(pages);
}

/* len must be <= 8 and start must be a multiple of len.
 * Called via softmmu_template.h when code areas are written to with
 * iothread mutex not held.
 *
 * Call with all @pages in the range [@start, @start + len[ locked.
 */
void tb_invalidate_phys_page_fast(struct page_collection *pages,
                                  tb_page_addr_t start, int len,
                                  uintptr_t retaddr)
{
    PageDesc *p;

    p = page_find(start >> TARGET_PAGE_BITS);
    if (!p) {
        return;
    }

    assert_page_locked(p);
    if (!p->code_bitmap &&
        ++p->code_write_count >= SMC_BITMAP_USE_THRESHOLD) {
        build_page_bitmap(p);
    }
    if (p->code_bitmap) {
        unsigned int nr;
        unsigned long b;

        nr = start & ~TARGET_PAGE_MASK;
        b = p->code_bitmap[BIT_WORD(nr)] >> (nr & (BITS_PER_LONG - 1));
        if (b & ((1 << len) - 1)) {
            goto do_invalidate;
        }
    } else {
    do_invalidate:
        tb_invalidate_phys_page_range__locked(pages, p, start, start + len,
                                              retaddr);
    }
}

#endif

/* user-mode: call with mmap_lock held */
void tb_check_watchpoint(CPUState *cpu, uintptr_t retaddr)
{
    TranslationBlock *tb;

    assert_memory_lock();

    tb = tcg_tb_lookup(retaddr);
    if (tb) {
        /* We can use retranslation to find the PC.  */
        cpu_restore_state_from_tb(cpu, tb, retaddr, true);
        tb_phys_invalidate(tb, -1);
    } else {
        /* The exception probably happened in a helper.  The CPU state should
           have been saved before calling it. Fetch the PC from there.  */
        CPUArchState *env = cpu->env_ptr;
        target_ulong pc, cs_base;
        tb_page_addr_t addr;
        uint32_t flags;

        cpu_get_tb_cpu_state(env, &pc, &cs_base, &flags);
        addr = get_page_addr_code(env, pc);
        if (addr != -1) {
            tb_invalidate_phys_range(addr, addr + 1);
        }
    }
}

#if !defined(CONFIG_USER_ONLY) || defined(CONFIG_PROFILER)
static void print_qht_statistics(struct qht_stats hst)
{
    uint32_t hgram_opts;
    size_t hgram_bins;
    char *hgram;

    if (!hst.head_buckets) {
        return;
    }
    qemu_log("TB hash buckets     %zu/%zu (%0.2f%% head buckets used)\n",
                hst.used_head_buckets, hst.head_buckets,
                (double)hst.used_head_buckets / hst.head_buckets * 100);

    hgram_opts =  QDIST_PR_BORDER | QDIST_PR_LABELS;
    hgram_opts |= QDIST_PR_100X   | QDIST_PR_PERCENT;
    if (qdist_xmax(&hst.occupancy) - qdist_xmin(&hst.occupancy) == 1) {
        hgram_opts |= QDIST_PR_NODECIMAL;
    }
    hgram = qdist_pr(&hst.occupancy, 10, hgram_opts);
    qemu_log("TB hash occupancy   %0.2f%% avg chain occ. Histogram: %s\n",
                qdist_avg(&hst.occupancy) * 100, hgram);
    g_free(hgram);

    hgram_opts = QDIST_PR_BORDER | QDIST_PR_LABELS;
    hgram_bins = qdist_xmax(&hst.chain) - qdist_xmin(&hst.chain);
    if (hgram_bins > 10) {
        hgram_bins = 10;
    } else {
        hgram_bins = 0;
        hgram_opts |= QDIST_PR_NODECIMAL | QDIST_PR_NOBINRANGE;
    }
    hgram = qdist_pr(&hst.chain, hgram_bins, hgram_opts);
    qemu_log("TB hash avg chain   %0.3f buckets. Histogram: %s\n",
                qdist_avg(&hst.chain), hgram);
    g_free(hgram);
}

#ifdef CONFIG_LATX_PROFILER
struct tb_exec_stats {
    target_ulong pc;
    target_ulong end_pc;
    TBProfile profile;
    uint64_t ir1_num;
    double   exp_rate;
    uint64_t run_ir1_num;
    uint64_t run_ir2_num;
    double   run_exp_rate;
    uint64_t eflags_run;
};

static void tb_stats_copy(const TranslationBlock *tb, void *ptr)
{
    struct tb_exec_stats *stats = (struct tb_exec_stats *)ptr;
    uint64_t exec_times = GET_TB_PROFILE(tb, exec_times);
    uint64_t ir2_num = GET_TB_PROFILE(tb, nr_code);
    uint64_t eflags_gen = GET_TB_PROFILE(tb, sta_generate);
    uint64_t eflags_eli = GET_TB_PROFILE(tb, sta_eliminate);
    memcpy(&(stats->profile), &(tb->profile), sizeof(TBProfile));

    stats->pc = tb->pc;
    stats->end_pc = tb->pc + tb->size;
    stats->ir1_num = tb->icount;
    stats->run_ir1_num = tb->icount * exec_times;
    stats->run_ir2_num = ir2_num * exec_times;
    stats->exp_rate = ir2_num / (double)tb->icount;
    stats->run_exp_rate = (stats->exp_rate - 1) * exec_times;
    stats->eflags_run = (eflags_gen - eflags_eli) * exec_times;
}
#endif

struct tb_tree_stats {
    size_t nb_tbs;
    size_t host_size;
    size_t target_size;
    size_t max_target_size;
    size_t direct_jmp_count;
    size_t direct_jmp2_count;
    size_t cross_page;
#ifdef CONFIG_LATX_PROFILER
    uint64_t dynamic_insts;
    uint64_t exec_times;
    uint64_t exit_times;
    uint64_t jrra_in;
    uint64_t jrra_miss;
    uint64_t sta_eliminate;
    uint64_t sta_generate;
    uint64_t sta_simulate;
    GArray *tb_execinfo;
#endif
};

#ifdef CONFIG_LATX_PROFILER
static void tb_exec_dump_info(void *ptr)
{
    struct tb_tree_stats *tst = (struct tb_tree_stats *)ptr;
    GArray *exec_info = tst->tb_execinfo;
    struct tb_exec_stats *ele = NULL;
    TBProfile *tb_pro = NULL;
    guint i;
    qemu_log("TB exec detail info:\n");
    qemu_log("pc                end_pc            exec_times  exit_times  ");
    qemu_log("run_ir1_num  run_ir2_num  run_exp_rate  ");
    qemu_log("ir1_num  ir2_num  exp_rate    ");
    qemu_log("eflags \n");
    for (i = 0; i < exec_info->len; ++i) {
        ele = &g_array_index(exec_info, struct tb_exec_stats, i);
        tb_pro = &(ele->profile);
        qemu_log(TARGET_FMT_lx "  " TARGET_FMT_lx "  %-10" PRIu64 "  %-10" PRIu64 "  ",
                    ele->pc, ele->end_pc, tb_pro->exec_times, tb_pro->exit_times);
        qemu_log("%-10" PRIu64 "   %-10" PRIu64 "   %-10.2f    ",
                    ele->run_ir1_num, ele->run_ir2_num, ele->run_exp_rate);
        qemu_log("%-7" PRIu64 "  %-7" PRIu64 "  %-10.2f  ",
                    ele->ir1_num, tb_pro->nr_code, ele->exp_rate);
        qemu_log("%-7" PRIu64 "\n", ele->eflags_run);
    }
}
#endif

static gboolean tb_tree_stats_iter(gpointer key, gpointer value, gpointer data)
{
    const TranslationBlock *tb = value;
    struct tb_tree_stats *tst = data;

    tst->nb_tbs++;
    tst->host_size += tb->tc.size;
    tst->target_size += tb->size;
    if (tb->size > tst->max_target_size) {
        tst->max_target_size = tb->size;
    }
    if (tb_page_addr1(tb) != -1) {
        tst->cross_page++;
    }
    if (!use_tu_jmp(tb) && tb->jmp_reset_offset[0] != TB_JMP_RESET_OFFSET_INVALID) {
        tst->direct_jmp_count++;
        if (tb->jmp_reset_offset[1] != TB_JMP_RESET_OFFSET_INVALID) {
            tst->direct_jmp2_count++;
        }
    }
#ifdef CONFIG_LATX_PROFILER
    uint64_t exec_times = GET_TB_PROFILE(tb, exec_times);
    tst->dynamic_insts  += GET_TB_PROFILE(tb, nr_code) * exec_times;
    tst->exec_times     += exec_times;
    tst->exit_times     += GET_TB_PROFILE(tb, exit_times);
    tst->jrra_in        += GET_TB_PROFILE(tb, jrra_in);
    tst->jrra_miss      += GET_TB_PROFILE(tb, jrra_miss);

    tst->sta_eliminate  += GET_TB_PROFILE(tb, sta_eliminate) * exec_times;
    tst->sta_generate   += GET_TB_PROFILE(tb, sta_generate) * exec_times;
    tst->sta_simulate   += GET_TB_PROFILE(tb, sta_simulate) * exec_times;
    /* get tb exec stats */
    struct tb_exec_stats tb_stats = {};
    /* copy tb data */
    tb_stats_copy(tb, (void *)&tb_stats);
    g_array_append_val(tst->tb_execinfo, tb_stats);
#endif
    return false;
}
#endif

#ifndef CONFIG_USER_ONLY
/*
 * In deterministic execution mode, instructions doing device I/Os
 * must be at the end of the TB.
 *
 * Called by softmmu_template.h, with iothread mutex not held.
 */
void cpu_io_recompile(CPUState *cpu, uintptr_t retaddr)
{
    TranslationBlock *tb;
    CPUClass *cc;
    uint32_t n;

    tb = tcg_tb_lookup(retaddr);
    if (!tb) {
        cpu_abort(cpu, "cpu_io_recompile: could not find TB for pc=%p",
                  (void *)retaddr);
    }
    cpu_restore_state_from_tb(cpu, tb, retaddr, true);

    /*
     * Some guests must re-execute the branch when re-executing a delay
     * slot instruction.  When this is the case, adjust icount and N
     * to account for the re-execution of the branch.
     */
    n = 1;
    cc = CPU_GET_CLASS(cpu);
    if (cc->tcg_ops->io_recompile_replay_branch &&
        cc->tcg_ops->io_recompile_replay_branch(cpu, tb)) {
        cpu_neg(cpu)->icount_decr.u16.low++;
        n = 2;
    }

    /*
     * Exit the loop and potentially generate a new TB executing the
     * just the I/O insns. We also limit instrumentation to memory
     * operations only (which execute after completion) so we don't
     * double instrument the instruction.
     */
    cpu->cflags_next_tb = curr_cflags(cpu) | CF_MEMI_ONLY | CF_LAST_IO | n;

    qemu_log_mask_and_addr(CPU_LOG_EXEC, tb->pc,
                           "cpu_io_recompile: rewound execution of TB to "
                           TARGET_FMT_lx "\n", tb->pc);

    cpu_loop_exit_noexc(cpu);
}

void dump_exec_info(void)
{
    struct tb_tree_stats tst = {};
    struct qht_stats hst;
    size_t nb_tbs, flush_full, flush_part, flush_elide;

    tcg_tb_foreach(tb_tree_stats_iter, &tst);
    nb_tbs = tst.nb_tbs;
    /* XXX: avoid using doubles ? */
    qemu_printf("Translation buffer state:\n");
    /*
     * Report total code size including the padding and TB structs;
     * otherwise users might think "-accel tcg,tb-size" is not honoured.
     * For avg host size we use the precise numbers from tb_tree_stats though.
     */
    qemu_printf("gen code size       %zu/%zu\n",
                tcg_code_size(), tcg_code_capacity());
    qemu_printf("TB count            %zu\n", nb_tbs);
    qemu_printf("TB avg target size  %zu max=%zu bytes\n",
                nb_tbs ? tst.target_size / nb_tbs : 0,
                tst.max_target_size);
    qemu_printf("TB avg host size    %zu bytes (expansion ratio: %0.1f)\n",
                nb_tbs ? tst.host_size / nb_tbs : 0,
                tst.target_size ? (double)tst.host_size / tst.target_size : 0);
    qemu_printf("cross page TB count %zu (%zu%%)\n", tst.cross_page,
                nb_tbs ? (tst.cross_page * 100) / nb_tbs : 0);
    qemu_printf("direct jump count   %zu (%zu%%) (2 jumps=%zu %zu%%)\n",
                tst.direct_jmp_count,
                nb_tbs ? (tst.direct_jmp_count * 100) / nb_tbs : 0,
                tst.direct_jmp2_count,
                nb_tbs ? (tst.direct_jmp2_count * 100) / nb_tbs : 0);

    qht_statistics_init(&tb_ctx.htable, &hst);
    print_qht_statistics(hst);
    qht_statistics_destroy(&hst);

    qemu_printf("\nStatistics:\n");
    qemu_printf("TB flush count      %u\n",
                qatomic_read(&tb_ctx.tb_flush_count));
    qemu_printf("TB invalidate count %zu\n",
                tcg_tb_phys_invalidate_count());

    tlb_flush_counts(&flush_full, &flush_part, &flush_elide);
    qemu_printf("TLB full flushes    %zu\n", flush_full);
    qemu_printf("TLB partial flushes %zu\n", flush_part);
    qemu_printf("TLB elided flushes  %zu\n", flush_elide);
    tcg_dump_info();
}

void dump_opcount_info(void)
{
    tcg_dump_op_count();
}

#else /* CONFIG_USER_ONLY */
#  ifdef CONFIG_LATX_PROFILER

void dump_exec_info(void)
{
    struct tb_tree_stats tst = {};
    struct qht_stats hst;
    size_t nb_tbs;
#ifdef CONFIG_LATX
    tst.tb_execinfo = g_array_new(FALSE, FALSE, sizeof(struct tb_exec_stats));
#endif
    qemu_log("\n");
    qemu_log("[Profile] ===== Summary =====\n");
    tcg_tb_foreach(tb_tree_stats_iter, &tst);
    nb_tbs = tst.nb_tbs;
    /* show all tb exec information */
    tb_exec_dump_info((void *)&tst);
    /* XXX: avoid using doubles ? */
    qemu_log("Translation buffer state:\n");
    /*
     * Report total code size including the padding and TB structs;
     * otherwise users might think "-accel tcg,tb-size" is not honoured.
     * For avg host size we use the precise numbers from tb_tree_stats though.
     */
    qemu_log("gen code size       %zu/%zu\n",
                tcg_code_size(), tcg_code_capacity());
    qemu_log("TB count            %zu\n", nb_tbs);
    qemu_log("TB avg target size  %zu max=%zu bytes\n",
                nb_tbs ? tst.target_size / nb_tbs : 0,
                tst.max_target_size);
    qemu_log("TB avg host size    %zu bytes (expansion ratio: %0.1f)\n",
                nb_tbs ? tst.host_size / nb_tbs : 0,
                tst.target_size ? (double)tst.host_size / tst.target_size : 0);
    qemu_log("cross page TB count %zu (%zu%%)\n", tst.cross_page,
                nb_tbs ? (tst.cross_page * 100) / nb_tbs : 0);
    qemu_log("direct jump count   %zu (%zu%%) (2 jumps=%zu %zu%%)\n",
                tst.direct_jmp_count,
                nb_tbs ? (tst.direct_jmp_count * 100) / nb_tbs : 0,
                tst.direct_jmp2_count,
                nb_tbs ? (tst.direct_jmp2_count * 100) / nb_tbs : 0);

    qemu_log("-- Summary:\n");
    qemu_log("dynamic inst        %zu\n", tst.dynamic_insts);
    qemu_log("tb run times        %zu\n", tst.exec_times);
    qemu_log("tb exit times       %zu\n", tst.exit_times);
    qemu_log("JRRA in times       %zu\n", tst.jrra_in);
    qemu_log("JRRA miss times     %zu (%0.1f%%)\n", tst.jrra_miss,
             tst.jrra_in ? (double)tst.jrra_miss * 100 / tst.jrra_in : 0);

    uint64_t eflags_has_gen = tst.sta_generate - tst.sta_eliminate;
    qemu_log("-- Flag reduction:\n");
    qemu_log("Need generate flags number    %" PRId64 "\n", tst.sta_generate);
    qemu_log(" ├ generate flags number      %" PRId64 "\n", eflags_has_gen);
    qemu_log(" │ ├ LBT number               %" PRId64 "\n", eflags_has_gen - tst.sta_simulate);
    qemu_log(" │ └ code sim number          %" PRId64 "\n", tst.sta_simulate);
    qemu_log(" └ stripped flag number       %" PRId64 " (%0.1f%%)\n",
             tst.sta_eliminate, (double)tst.sta_eliminate / tst.sta_generate * 100);

    qht_statistics_init(&tb_ctx.htable, &hst);
    print_qht_statistics(hst);
    qht_statistics_destroy(&hst);

    tcg_dump_info();
#ifdef CONFIG_LATX
    g_array_free(tst.tb_execinfo, TRUE);
#endif
}
#  endif

void cpu_interrupt(CPUState *cpu, int mask)
{
    g_assert(qemu_mutex_iothread_locked());
    cpu->interrupt_request |= mask;
    qatomic_set(&cpu_neg(cpu)->icount_decr.u16.high, -1);
}

typedef struct PageFlagsNode {
    IntervalTreeNode itree;
    int flags;
} PageFlagsNode;

IntervalTreeRoot pageflags_root;

static PageFlagsNode *pageflags_find(target_ulong start, target_ulong last)
{
    IntervalTreeNode *n;

    n = interval_tree_iter_first(&pageflags_root, start, last);
    return n ? container_of(n, PageFlagsNode, itree) : NULL;
}

static PageFlagsNode *pageflags_next(PageFlagsNode *p, target_ulong start,
                                     target_ulong last)
{
    IntervalTreeNode *n;

    n = interval_tree_iter_next(&p->itree, start, last);
    return n ? container_of(n, PageFlagsNode, itree) : NULL;
}

int walk_memory_regions(void *priv, walk_memory_regions_fn fn)
{
    IntervalTreeNode *n;
    int rc = 0;

    mmap_lock();
    for (n = interval_tree_iter_first(&pageflags_root, 0, -1);
         n != NULL;
         n = interval_tree_iter_next(n, 0, -1)) {
        PageFlagsNode *p = container_of(n, PageFlagsNode, itree);

        rc = fn(priv, n->start, n->last + 1, p->flags);
        if (rc != 0) {
            break;
        }
    }
    mmap_unlock();

    return rc;
}

static int dump_region(void *priv, target_ulong start,
                       target_ulong end, unsigned long prot)
{
    FILE *f = (FILE *)priv;

    fprintf(f, TARGET_FMT_lx"-"TARGET_FMT_lx" "TARGET_FMT_lx" %c%c%c\n",
            start, end, end - start,
            ((prot & PAGE_READ) ? 'r' : '-'),
            ((prot & PAGE_WRITE) ? 'w' : '-'),
            ((prot & PAGE_EXEC) ? 'x' : '-'));
    return 0;
}

/* dump memory mappings */
void page_dump(FILE *f)
{
    const int length = sizeof(target_ulong) * 2;

    fprintf(f, "%-*s %-*s %-*s %s\n",
            length, "start", length, "end", length, "size", "prot");
    walk_memory_regions(f, dump_region);
}

target_ulong get_first_page(target_ulong start, target_ulong end)
{
    PageFlagsNode *p = pageflags_find(start, end);

    /*
     * See util/interval-tree.c re lockless lookups: no false positives but
     * there are false negatives.  If we find nothing, retry with the mmap
     * lock acquired.
     */
    if (p) {
        return p->itree.start;
    }
    if (have_mmap_lock()) {
        return 0;
    }

    mmap_lock();
    p = pageflags_find(start, end);
    mmap_unlock();
    return p ? p->itree.start : 0;
}

int page_get_flags(target_ulong address)
{
    PageFlagsNode *p = pageflags_find(address, address);

    /*
     * See util/interval-tree.c re lockless lookups: no false positives but
     * there are false negatives.  If we find nothing, retry with the mmap
     * lock acquired.
     */
    if (p) {
        return p->flags;
    }
    if (have_mmap_lock()) {
        return 0;
    }

    mmap_lock();
    p = pageflags_find(address, address);
    mmap_unlock();
    return p ? p->flags : 0;
}

#define rb_to_itree(N)  container_of(N, IntervalTreeNode, rb)

/* Cut down some code, Maybe have bug. */
bool test_flags(target_ulong address, int flags)
{

    IntervalTreeNode *node;
    if (!pageflags_root.rb_root.rb_node) {
        return true;
    }
    node = rb_to_itree(pageflags_root.rb_root.rb_node);
    while (true) {
        if (node->rb.rb_left) {
            IntervalTreeNode *left = rb_to_itree(node->rb.rb_left);
            if (address <= left->subtree_last) {
                node = left;
                continue;
            }
        }
        if (node->start <= address) {         /* Cond1 */
            if (address <= node->last) {     /* Cond2 */
                break;
            }
            if (node->rb.rb_right) {
                node = rb_to_itree(node->rb.rb_right);
                if (address <= node->subtree_last) {
                    continue;
                }
            }
        }
        return true; /* no match */
    }

    PageFlagsNode *p = container_of(node, PageFlagsNode, itree);
    if (p) {
        return p->flags & flags;
    }
    return true;
}

/* A subroutine of page_set_flags: insert a new node for [start,last]. */
static void pageflags_create(target_ulong start, target_ulong last, int flags)
{
    PageFlagsNode *p = g_new(PageFlagsNode, 1);

    p->itree.start = start;
    p->itree.last = last;
    p->flags = flags;
    interval_tree_insert(&p->itree, &pageflags_root);
}

/*
 * A subroutine of page_set_flags: nothing overlaps [start,last],
 * but check adjacent mappings and maybe merge into a single range.
 */
static void pageflags_create_merge(target_ulong start, target_ulong last,
                                   int flags)
{
    PageFlagsNode *next = NULL, *prev = NULL;

    if (start > 0) {
        prev = pageflags_find(start - 1, start - 1);
        if (prev) {
            if (prev->flags == flags) {
                interval_tree_remove(&prev->itree, &pageflags_root);
            } else {
                prev = NULL;
            }
        }
    }
    if (last + 1 != 0) {
        next = pageflags_find(last + 1, last + 1);
        if (next) {
            if (next->flags == flags) {
                interval_tree_remove(&next->itree, &pageflags_root);
            } else {
                next = NULL;
            }
        }
    }

    if (prev) {
        if (next) {
            prev->itree.last = next->itree.last;
            g_free(next);
        } else {
            prev->itree.last = last;
        }
        interval_tree_insert(&prev->itree, &pageflags_root);
    } else if (next) {
        next->itree.start = start;
        interval_tree_insert(&next->itree, &pageflags_root);
    } else {
        pageflags_create(start, last, flags);
    }
}

/* A subroutine of page_set_flags: remove everything in [start,last]. */
static bool pageflags_unset(target_ulong start, target_ulong last)
{
    bool inval_tb = false;

    while (true) {
        PageFlagsNode *p = pageflags_find(start, last);
        target_ulong p_last;

        if (!p) {
            break;
        }

        if (p->flags & PAGE_EXEC) {
            inval_tb = true;
        }

        interval_tree_remove(&p->itree, &pageflags_root);
        p_last = p->itree.last;

        if (p->itree.start < start) {
            /* Truncate the node from the end, or split out the middle. */
            p->itree.last = start - 1;
            interval_tree_insert(&p->itree, &pageflags_root);
            if (last < p_last) {
                pageflags_create_merge(last + 1, p_last, p->flags);
                break;
            }
        } else if (p_last <= last) {
            /* Range completely covers node -- remove it. */
            g_free(p);
        } else {
            /* Truncate the node from the start. */
            p->itree.start = last + 1;
            interval_tree_insert(&p->itree, &pageflags_root);
            break;
        }
    }

    return inval_tb;
}

/* A subroutine of page_set_flags: add flags to [start,last]. */
bool pageflags_set_clear(target_ulong start, target_ulong last,
                                int set_flags, int clear_flags)
{
    PageFlagsNode *p;
    target_ulong p_start, p_last;
    int p_flags, merge_flags;
    bool inval_tb = false;

 restart:
    p = pageflags_find(start, last);
    if (!p) {
        if (set_flags) {
            pageflags_create_merge(start, last, set_flags);
        }
        goto done;
    }

    p_start = p->itree.start;
    p_last = p->itree.last;
    p_flags = p->flags;
    /* Using mprotect on a page does not change sticky bits. */
    merge_flags = (p_flags & ~clear_flags) | set_flags;

    /*
     * Need to flush if an overlapping executable region
     * removes exec, or adds write.
     */
    if ((p_flags & PAGE_EXEC)
        && (!(merge_flags & PAGE_EXEC)
            || (merge_flags & ~p_flags & PAGE_WRITE))) {
        inval_tb = true;
    }

    /*
     * If there is an exact range match, update and return without
     * attempting to merge with adjacent regions.
     */
    if (start == p_start && last == p_last) {
        if (merge_flags) {
            p->flags = merge_flags;
        } else {
            interval_tree_remove(&p->itree, &pageflags_root);
            g_free(p);
        }
        goto done;
    }

    /*
     * If sticky bits affect the original mapping, then we must be more
     * careful about the existing intervals and the separate flags.
     */
    if (set_flags != merge_flags) {
        if (p_start < start) {
            interval_tree_remove(&p->itree, &pageflags_root);
            p->itree.last = start - 1;
            interval_tree_insert(&p->itree, &pageflags_root);

            if (last < p_last) {
                if (merge_flags) {
                    pageflags_create_merge(start, last, merge_flags);
                }
                pageflags_create_merge(last + 1, p_last, p_flags);
            } else {
                if (merge_flags) {
                    pageflags_create_merge(start, p_last, merge_flags);
                }
                if (p_last < last) {
                    start = p_last + 1;
                    goto restart;
                }
            }
        } else {
            if (start < p_start && set_flags) {
                pageflags_create_merge(start, p_start - 1, set_flags);
            }
            if (last < p_last) {
                interval_tree_remove(&p->itree, &pageflags_root);
                p->itree.start = last + 1;
                interval_tree_insert(&p->itree, &pageflags_root);
                if (merge_flags) {
                    pageflags_create_merge(start, last, merge_flags);
                }
            } else {
                if (merge_flags) {
                    p->flags = merge_flags;
                } else {
                    interval_tree_remove(&p->itree, &pageflags_root);
                    g_free(p);
                }
                if (p_last < last) {
                    start = p_last + 1;
                    goto restart;
                }
            }
        }
        goto done;
    }

    /* If flags are not changing for this range, incorporate it. */
    if (set_flags == p_flags) {
        if (start < p_start) {
            interval_tree_remove(&p->itree, &pageflags_root);
            p->itree.start = start;
            interval_tree_insert(&p->itree, &pageflags_root);
        }
        if (p_last < last) {
            start = p_last + 1;
            goto restart;
        }
        goto done;
    }

    /* Maybe split out head and/or tail ranges with the original flags. */
    interval_tree_remove(&p->itree, &pageflags_root);
    if (p_start < start) {
        p->itree.last = start - 1;
        interval_tree_insert(&p->itree, &pageflags_root);

        if (p_last < last) {
            goto restart;
        }
        if (last < p_last) {
            pageflags_create_merge(last + 1, p_last, p_flags);
        }
    } else if (last < p_last) {
        p->itree.start = last + 1;
        interval_tree_insert(&p->itree, &pageflags_root);
    } else {
        g_free(p);
        goto restart;
    }
    if (set_flags) {
        pageflags_create_merge(start, last, set_flags);
    }

 done:
    return inval_tb;
}

#ifdef CONFIG_LATX_AOT
static seg_info *p_info;
#endif
/* Modify the flags of a page and invalidate the code if necessary.
   The flag PAGE_WRITE_ORG is positioned automatically depending
   on PAGE_WRITE.  The mmap_lock should already be held.  */
void page_set_flags(target_ulong start, target_ulong end, int flags)
{
#ifdef CONFIG_LATX_PERF
    latx_timer_start(TIMER_PAGE_FLAGS);
#endif
    target_ulong last;
    bool reset = false;
    bool inval_tb = false;

    /* This function should never be called with addresses outside the
       guest address space.  If this assert fires, it probably indicates
       a missing call to h2g_valid.  */
    assert(end - 1 <= GUEST_ADDR_MAX);
    assert(start < end);
    /* Only set PAGE_ANON with new mappings. */
#ifndef CONFIG_LATX
    assert(!(flags & PAGE_ANON) || (flags & PAGE_RESET));
#endif
    if (!(!(flags & PAGE_ANON) || (flags & PAGE_RESET))) {
        qemu_log_mask(LAT_LOG_MEM,
                "[LATX_16K] %s assert(!(flags & PAGE_ANON) || "
                "(flags & PAGE_RESET)) faild. (The message was just a "
                "reminder, not an error)" TARGET_FMT_lx TARGET_FMT_lx " %x\n",
                __func__, start, end, flags);
    }
    assert_memory_lock();

    start = start & TARGET_PAGE_MASK;
    end = TARGET_PAGE_ALIGN(end);
    last = end - 1;

    if (!(flags & PAGE_VALID)) {
        flags = 0;
    } else {
        reset = flags & PAGE_RESET;
        flags &= ~PAGE_RESET;
        if (flags & PAGE_WRITE) {
            flags |= PAGE_WRITE_ORG;
        }
    }

    if (!flags || reset) {
        page_reset_target_data(start, end);
        inval_tb |= pageflags_unset(start, last);
    }

#ifdef CONFIG_LATX_AOT
    if (option_aot && (flags & PAGE_WRITE)) {
        p_info = segment_tree_lookup2(start, end);

    }
#endif

#ifdef CONFIG_LATX_AOT
    target_ulong addr, len;
    if (option_aot && (flags & PAGE_WRITE)) {
        for (addr = start, len = end - start;
             len != 0;
             len -= TARGET_PAGE_SIZE, addr += TARGET_PAGE_SIZE) {
            PageFlagsNode *p = pageflags_find(addr, addr);
            if (p && !(p->flags & PAGE_WRITE)) {
                if (foreach_tb_first(addr, addr + TARGET_PAGE_SIZE)) {
                    page_set_page_state(addr, PAGE_SMC);
                } else if ((p->flags & PAGE_EXEC) && p_info) {
                    aot_segment *p_segment = (aot_segment *)(p_info->p_segment);
                    if ((p_info->is_running || (p_segment && !(p_segment->is_pe)))) {
                        page_set_page_state(addr, PAGE_SMC);
                    }
                }
            }
        }
    }
#endif

    if (flags) {
        inval_tb |= pageflags_set_clear(start, last, flags,
            ~(reset ? 0 : PAGE_ANON | PAGE_OVERFLOW | PAGE_MEMSHARE));
    }

    if (inval_tb) {
        tb_invalidate_phys_range(start, end);
    }
#ifdef CONFIG_LATX_PERF
    latx_timer_stop(TIMER_PAGE_FLAGS);
#endif
}

typedef struct TargetPageDataNode {
    IntervalTreeNode itree;
    void *target_data;
} TargetPageDataNode;

static IntervalTreeRoot targetdata_root;

void page_reset_target_data(target_ulong start, target_ulong end)
{
    IntervalTreeNode *n, *next;
    target_ulong last;

    assert_memory_lock();

    start = start & TARGET_PAGE_MASK;
    last = TARGET_PAGE_ALIGN(end) - 1;

    if (!spm_exist(start, last-start))
        return;

    for (n = interval_tree_iter_first(&targetdata_root, start, last),
         next = n ? interval_tree_iter_next(n, start, last) : NULL;
         n != NULL;
         n = next,
         next = next ? interval_tree_iter_next(n, start, last) : NULL) {
        TargetPageDataNode *t;

        if (n->start >= start && n->last <= last) {
            t = container_of(n, TargetPageDataNode, itree);
            ShadowPageDesc *shadow_pd = t->target_data;
            if (shadow_pd) {
                munmap(shadow_pd->p_addr, qemu_host_page_size);
            }
            if ((n->last - n->start) != TARGET_PAGE_SIZE -1) {
                printf("%s interval_tree_node start %lx end %lx size %lx not TARGET_PAGE_SIZE!!!!\n",
                    __func__, n->start, n->last, n->last - n->start + 1);
                assert(0);
            }
            spm_clear(n->start, n->last - n->start + 1);
            g_free(t->target_data);
            t->target_data = NULL;
            interval_tree_remove(n, &targetdata_root);
            g_free(n);
            continue;
        }
        printf("should not arrive");
        assert(0);
    }
}

void *page_get_target_data(target_ulong address)
{
    IntervalTreeNode *n;
    TargetPageDataNode *t;
    target_ulong page;

    page = address & TARGET_PAGE_MASK;

    if (!spm_exist(page, TARGET_PAGE_SIZE))
        return NULL;

    n = interval_tree_iter_first(&targetdata_root, page, page);
    if (!n) {
       return NULL;
    }

    t = container_of(n, TargetPageDataNode, itree);
    void *ret = t->target_data;
    return ret;
}

void *page_alloc_target_data(target_ulong address, size_t size)
{
    IntervalTreeNode *n;
    TargetPageDataNode *t;
    target_ulong page;

    page = address & TARGET_PAGE_MASK;

    if(!(page_get_flags(address) & PAGE_VALID))
        return NULL;

    n = interval_tree_iter_first(&targetdata_root, page, page);
    if (!n) {
        /*
         * See util/interval-tree.c re lockless lookups: no false positives
         * but there are false negatives.  If we find nothing, retry with
         * the mmap lock acquired.  We also need the lock for the
         * allocation + insert.
         */
        mmap_lock();
        n = interval_tree_iter_first(&targetdata_root, page, page);
        if (!n) {
            t = g_new0(TargetPageDataNode, 1);
            n = &t->itree;
            n->start = page;
            n->last = page + TARGET_PAGE_SIZE - 1;
            interval_tree_insert(n, &targetdata_root);
        }
        mmap_unlock();
    }

    if ((n->last- n->start) != TARGET_PAGE_SIZE -1) {
        printf("%s interval_tree_node start %lx end %lx size %lx not TARGET_PAGE_SIZE!!!!\n",
            __func__, n->start, n->last, n->last - n->start + 1);
        assert(0);
    }

    t = container_of(n, TargetPageDataNode, itree);
    void *ret = t->target_data;
    if (!ret) {
        ret = g_malloc0(size);
        t->target_data = ret;
    }
    return ret;
}

bool page_check_range(target_ulong start, target_ulong len, int flags)
{
#if defined(CONFIG_LATX_KZT)
    if (option_kzt && start > reserved_va) {
        return true;
    }
#endif
    target_ulong last;
    int locked;  /* tri-state: =0: unlocked, +1: global, -1: local */
    bool ret;

    if (len == 0) {
        return true;  /* trivial length */
    }

    last = start + len - 1;
    if (last < start) {
        return false; /* wrap around */
    }

    locked = have_mmap_lock();
    while (true) {
        PageFlagsNode *p = pageflags_find(start, last);
        int missing;

        if (!p) {
            if (!locked) {
                /*
                 * Lockless lookups have false negatives.
                 * Retry with the lock held.
                 */
                mmap_lock();
                locked = -1;
                p = pageflags_find(start, last);
            }
            if (!p) {
                ret = false; /* entire region invalid */
                break;
            }
        }
        if (start < p->itree.start) {
            ret = false; /* initial bytes invalid */
            break;
        }

        missing = flags & ~p->flags;
        if (missing & PAGE_READ) {
            ret = false; /* page not readable */
            break;
        }
        if (missing & PAGE_WRITE) {
            if (!(p->flags & PAGE_WRITE_ORG)) {
                ret = false; /* page not writable */
                break;
            }
            /* Asking about writable, but has been protected: undo. */
            if (!page_unprotect(start, 0, NULL)) {
                ret = false;
                break;
            }
            /* TODO: page_unprotect should take a range, not a single page. */
            if (last - start < TARGET_PAGE_SIZE) {
                ret = true; /* ok */
                break;
            }
            start += TARGET_PAGE_SIZE;
            continue;
        }

        if (last <= p->itree.last) {
            ret = true; /* ok */
            break;
        }
        start = p->itree.last + 1;
    }

    /* Release the lock if acquired locally. */
    if (locked < 0) {
        mmap_unlock();
    }
    return ret;
}

target_ulong page_find_range_empty(target_ulong min, target_ulong max,
                                   target_ulong len, target_ulong align)
{
    target_ulong len_m1, align_m1;

    target_ulong start = min;
    assert(min <= max);
    assert(max <= GUEST_ADDR_MAX);
    assert(len != 0);
    assert(is_power_of_2(align));
    assert_memory_lock();

    len_m1 = len - 1;
    align_m1 = align - 1;

    /* Iteratively narrow the search region. */
    while (1) {
        PageFlagsNode *p;

        /* Align min and double-check there's enough space remaining. */
        min = (min + align_m1) & ~align_m1;
        if (min > max) {
            return -1;
        }
        if (len_m1 > max - min) {
            return -1;
        }

        p = pageflags_find(min, min + len_m1);
        if (p == NULL) {
            /* Found! */
            if (start == mmap_next_start) {
                mmap_next_start = min;
            }
            return min;
        }
        if (max <= p->itree.last) {
            /* Existing allocation fills the remainder of the search region. */
            return -1;
        }
        /* Skip across existing allocation. */
        min = p->itree.last + 1;
    }
}

void page_protect(tb_page_addr_t address)
{
    PageFlagsNode *p;
    target_ulong start, last;

    assert_memory_lock();

    start = address & qemu_host_page_mask;
    last = start + qemu_host_page_size - 1;

    p = pageflags_find(address, address);
    if (p && (p->flags & PAGE_WRITE)) {
        int prot = p->flags;

        if (unlikely(p->itree.last < last)) {
            /* More than one protection region covers the one host page. */
            p = pageflags_find(start, last);
            assert(TARGET_PAGE_SIZE < qemu_host_page_size);
            while ((p = pageflags_next(p, start, last)) != NULL) {
                prot |= p->flags;
            }
        }

        pageflags_set_clear(start, last, 0, PAGE_WRITE);
        if (!is_shadow_page_not_shmm(address)) {
            mprotect(g2h_untagged(start), qemu_host_page_size,
                    (prot & PAGE_BITS) & ~PAGE_WRITE);
        }
    }
}

/*
 * SMC shared mmap strategy
 *
 * For a guest page at @addr with @size = host page size
 *
 * 0. create shmm_fd with memfd and mmap to get @shmm_base
 * 1. alloc one page from shmm as @shmm_offset
 * 2. mmap(0, size, READ|WRITE, MAP_SHARED, shmm_fd, shmm_offset)
 * 3. munmap(addr, size) after copy data to shmm space
 * 4. mmap(addr, size, READ, MAP_FIXED | MAP_SHARED, shmm_fd, shmm_offset)
 *
 * guest space : +++++++++--------+++++++    shadow page +--------+
 *                       ^  size                         ^  size
 *                       addr                            ptr
 *                     /                                   \
 *            shared map --------->   offset   <---------- shared map
 *                                    |  size  |
 *  shmm space : ++++++++++++++++++++++--------+++++++
 *
 * 5. access with original guest address @addr or @shmm_base + shmm_offset
 */

static int smc_shmm_fd;
static int smc_shmm_npages;
static int smc_shmm_pn;
#define SMC_SHMM_INIT_PAGES     512
#define SMC_SHMM_EXPAND_PAGES   128

static inline void smc_shmm_create(void)
{
    if (smc_shmm_fd) return;

    smc_shmm_fd = memfd_create("smc_shmm", 0);
    if (smc_shmm_fd == -1) {
        fprintf(stderr, "%s:%d SMC shmm create fail\n", __func__, __LINE__);
        latx_smc_shmm_disable();
        return;
    }

    smc_shmm_pn = 0; /* start index of pages */
    smc_shmm_npages = SMC_SHMM_INIT_PAGES; /* total number of pages */
    if (ftruncate(smc_shmm_fd, smc_shmm_npages * qemu_host_page_size) == -1) {
        fprintf(stderr, "%s:%d SMC shmm ftruncate fail\n", __func__, __LINE__);
        latx_smc_shmm_disable();
        close(smc_shmm_fd);
    }
}

/*
 * Alloc a new host page with smc_shmm_fd.
 *
 * On success, return the offset used with smc_shmm_fd.
 * On fail, return -1ULL.
 */
static inline uint64_t smc_shmm_alloc_page(void)
{
    uint64_t offset = -1ULL;
    uint64_t new_size;

    smc_shmm_create();

    if ((smc_shmm_pn + 1) < smc_shmm_npages) {
        goto alloc_and_return;
    }

    smc_shmm_npages += SMC_SHMM_EXPAND_PAGES;
    new_size = smc_shmm_npages * qemu_host_page_size;

    if (ftruncate(smc_shmm_fd, new_size) == -1) {
        fprintf(stderr, "%s:%d SMC shmm fail to expand\n", __func__, __LINE__);
        smc_shmm_npages -= SMC_SHMM_EXPAND_PAGES;
        goto just_return;
    }

alloc_and_return:
    offset = (uint64_t)(smc_shmm_pn * qemu_host_page_size);
    smc_shmm_pn += 1;
just_return:
    return offset;
}

/*
 * Check if the guest pages belonging to the same one host page are all anon
 *
 * return 1 if at least one guest page is not anon
 * return 0 if all guest pages are anon
 *
 * return *prot with accumulate prot of guest pages
 */
static inline int smc_shmm_checke_page_anon(uint64_t start, int *prot)
{
    PageFlagsNode *p;
    uint64_t end, addr;

    start = start & qemu_host_page_mask;
    end   = start + qemu_host_page_size;

    *prot = PAGE_READ | PAGE_WRITE;

    for (addr = start; addr < end; addr += TARGET_PAGE_SIZE) {
        p = pageflags_find(addr, addr);
        if (p && !(p->flags & PAGE_ANON)) {
            return 1;
        }
        *prot &= (p->flags & (PAGE_READ | PAGE_WRITE));
    }

    return 0;
}

/*
 * @start should be host page aligned
 *
 * might crash when error occurs
 *
 * return 0 when successfully created
 * return -1 when fail
 */
static int smc_create_shadow_page_shmm(uint64_t start)
{
    uint64_t real_start, real_end, addr, len;
    uint64_t shmm_offset;
    void *host_start, *ptr, *shmm_ptr;
    int64_t access_off;
    int prot;
    ShadowPageDesc *spd;

    /* can NOT use shmm if guest page is not MAP_ANON */
    if (smc_shmm_checke_page_anon(start, &prot)) {
        return -1;
    }

    /* alloc the shmm space */
    shmm_offset = smc_shmm_alloc_page();
    if (shmm_offset == -1ULL) {
        return -1;
    }

    real_start = start & qemu_host_page_mask;
    host_start = g2h_untagged(real_start);
    len = qemu_host_page_size;
    real_end = real_start + qemu_host_page_size;

    /* protect before memcpy */
    mprotect(host_start, len, PROT_READ);

    /* create shadow page, which is shared mapped to shmm space */
    shmm_ptr = mmap(NULL, len,
               PAGE_READ | PAGE_WRITE, /* allow rw on shadow page */
               MAP_SHARED,
               smc_shmm_fd, shmm_offset);
    if (shmm_ptr == MAP_FAILED) {
        fprintf(stderr, "%s:%d SMC shmm map share fail\n", __func__, __LINE__);
        fflush(stderr);
        g_assert_not_reached();
    }

    /* copy original guest data into the shmm space */
    memcpy(shmm_ptr, g2h_untagged(real_start), len);

    /* release the original guest space */
    munmap(host_start, len);

    /* create shared map from guest space to shmm space */
    ptr = mmap(host_start, len,
               prot & PROT_READ, /* allow read on guest space */
               MAP_SHARED | MAP_FIXED,
               smc_shmm_fd, shmm_offset);
    if (ptr != host_start) {
        fprintf(stderr, "%s:%d SMC shmm map guest fail\n", __func__, __LINE__);
        fflush(stderr);
        g_assert_not_reached();
    }

    /*
     * Register shadow page descriptor.
     *
     * Only one host page is used for multiple guest pages
     * with the same access offset.
     */
    access_off = (int64_t)shmm_ptr - (int64_t)real_start;
    for (addr = real_start; addr < real_end; addr += TARGET_PAGE_SIZE) {
        spd = set_shadow_page(addr, shmm_ptr, access_off);
        spd->is_shmm = 1;
    }

    return 0;
}

#ifdef CONFIG_LATX_SMC_OPT

static void smc_retrans_triger(TranslationBlock *ctb)
{
    if (!latx_smc_use_store_helper()) return;

    if (!ctb) return;
    /* already optomized for SMC */
    if (ctb->smc_data & TBSMC_OPTED_MASK) return;

    int count = (ctb->smc_data & TBSMC_COUNT_MASK) + 1;
    if (count > TBSMC_OPT_THRESHOLD) {
        smc_retrans_tree_init();
        if (smc_retrans_insert(ctb)) {
            tb_phys_invalidate__locked(ctb);
        }
    } else {
        ctb->smc_data &= ~TBSMC_COUNT_MASK;
        ctb->smc_data |= count & TBSMC_COUNT_MASK;
    }
}

#endif

#define SMC_CREATE_SHADOW_PAGE_SHMM(address, p) do {                        \
    if (!is_shadow_page(address) && !(p->flags & PAGE_WRITE)) {             \
        smc_create_shadow_page_shmm(address & qemu_host_page_mask);         \
    }                                                                       \
} while (0)

#define SMC_SHMM_RECOVER_WRITE(address, flags) do {                         \
    if (is_shadow_page_shmm(address)) {                                     \
        mprotect(g2h_untagged(address & qemu_host_page_mask),               \
                 qemu_host_page_size, flags);                               \
    }                                                                       \
} while (0)

#define SMC_CREATE_SHADOW_PAGE(address, p) do {                             \
    if (!is_shadow_page(address) && !(p->flags & PAGE_WRITE)) {             \
        target_ulong start, end;                                            \
        start = address & qemu_host_page_mask;                              \
        end = start + qemu_host_page_size;                                  \
        create_shadow_page_chunk(start, end,                                \
                PAGE_READ | PAGE_WRITE,                                     \
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);                        \
    }                                                                       \
} while (0)

#define SMC_ENABLE_PAGE_WRITE(addr, p) do {                                 \
    if (p && (p->flags & PAGE_WRITE_ORG) && !(p->flags & PAGE_WRITE)) {     \
        uint64_t _addr = addr & TARGET_PAGE_MASK;                           \
        pageflags_set_clear(_addr, _addr + TARGET_PAGE_SIZE - 1,            \
                            PAGE_WRITE, 0);                                 \
    }                                                                       \
} while (0)

/* TODO: handle cross page */
#define SMC_HELPER_CHECK(spd, p, addr, addr2, size) do {    \
    if (!(spd)) return 1;                                   \
    if (!((p)->flags & PAGE_WRITE_ORG)) return 1;           \
    (address2) = (address) + (size) - 1;                    \
    if (((address)  & TARGET_PAGE_MASK) !=                  \
        ((address2) & TARGET_PAGE_MASK)) {                  \
        return 1;                                           \
    }                                                       \
} while (0)

#ifdef SMC_HELPER_USE_PAGE_UNPROTECT
#define SMC_HELPER_TBINV(address, pc, size, p, ret) do {                \
    ret = page_unprotect((address), (pc), &(size));                     \
    if (!ret) g_assert_not_reached();                                   \
} while (0)
#else
#define SMC_HELPER_TBINV(address, pc, size, p, ret) do {                \
    ret = tb_invalidate_phys_page_unwind((address), (pc), &(size));     \
    if (!(size)) {                                                      \
        SMC_ENABLE_PAGE_WRITE((address), (p));                          \
    }                                                                   \
    ret = ret ? 2 : 1;                                                  \
} while (0)
#endif

/* If current TB is modified, restore cpu state and exit cpu loop.
 * Guest PC is restored during TB invalidation. */
#define SMC_HELPER_TBINV_AFTER(ret, env) do {                           \
    if (ret == 2) {                                                     \
        mmap_unlock();                                                  \
        smc_store_helper_restore_cpu_state(env);                        \
        cpu_loop_exit_noexc(env_cpu(env));                              \
        g_assert_not_reached();                                         \
    }                                                                   \
} while (0)

int smc_store_helper_vst_x4(void *env,
        uint64_t address, int r0, int r1, int r2, int r3)
{
    ShadowPageDesc *spd = page_get_target_data(address);
    PageFlagsNode *p = pageflags_find(address, address);
    uint64_t pc = GETPC();
    uint64_t address2;
    int size = 16 * 4;
    uint64_t val[2];
    int64_t mem_addr;
    int ret;

    SMC_HELPER_CHECK(spd, p, address, address2, size);

    mmap_lock();

    /* finish the store inst with shadow page */
    mem_addr = (int64_t)address + spd->access_off;
    smc_hook_get_xmm(env, r0, val);
    *(uint64_t*)(mem_addr + 8 * 0) = val[0];
    *(uint64_t*)(mem_addr + 8 * 1) = val[1];
    smc_hook_get_xmm(env, r1, val);
    *(uint64_t*)(mem_addr + 8 * 2) = val[0];
    *(uint64_t*)(mem_addr + 8 * 3) = val[1];
    smc_hook_get_xmm(env, r2, val);
    *(uint64_t*)(mem_addr + 8 * 4) = val[0];
    *(uint64_t*)(mem_addr + 8 * 5) = val[1];
    smc_hook_get_xmm(env, r3, val);
    *(uint64_t*)(mem_addr + 8 * 6) = val[0];
    *(uint64_t*)(mem_addr + 8 * 7) = val[1];

    /* This guest page is writable : no SMC */
    if (p->flags & PAGE_WRITE) {
        goto do_return;
    }

    /* This guest page is not writable : check SMC */
    SMC_HELPER_TBINV(address, pc, size, p, ret);
    SMC_HELPER_TBINV_AFTER(ret, env);

do_return:
    mmap_unlock();
    return 0;
}

int smc_store_helper_vst(void *env,
        uint64_t address, uint64_t val0, uint64_t val1)
{
    ShadowPageDesc *spd = page_get_target_data(address);
    PageFlagsNode *p = pageflags_find(address, address);
    uint64_t pc = GETPC();
    uint64_t address2;
    int size = 16;
    int64_t mem_addr;
    int ret;

    SMC_HELPER_CHECK(spd, p, address, address2, size);

    mmap_lock();

    /* finish the store inst with shadow page */
    mem_addr = (int64_t)address + spd->access_off;
    *(uint64_t*)(mem_addr + 0) = val0;
    *(uint64_t*)(mem_addr + 8) = val1;

    /* This guest page is writable : no SMC */
    if (p->flags & PAGE_WRITE) {
        goto do_return;
    }

    /* This guest page is not writable : check SMC */
    SMC_HELPER_TBINV(address, pc, size, p, ret);
    SMC_HELPER_TBINV_AFTER(ret, env);

do_return:
    mmap_unlock();
    return 0;
}

int smc_store_helper_st(void *env,
        uint64_t address, uint64_t val, int s)
{
    ShadowPageDesc *spd = page_get_target_data(address);
    PageFlagsNode *p = pageflags_find(address, address);
    uint64_t pc = GETPC();
    uint64_t address2;
    int size = s; // 1,2,4,8
    int64_t mem_addr;
    int ret;

    SMC_HELPER_CHECK(spd, p, address, address2, size);

    mmap_lock();

    /* finish the store inst with shadow page */
    mem_addr = (int64_t)address + spd->access_off;
    switch (size) {
    case 8: *(uint64_t *)(mem_addr) = (uint64_t)val; break;
    case 4: *(uint32_t *)(mem_addr) = (uint32_t)val; break;
    case 2: *(uint16_t *)(mem_addr) = (uint16_t)val; break;
    case 1: *(uint8_t  *)(mem_addr) = (uint8_t )val; break;
    default:
        mmap_unlock();
        return 1;
    }

    /* This guest page is writable : no SMC */
    if (p->flags & PAGE_WRITE) {
        goto do_return;
    }

    /* This guest page is not writable : check SMC */
    SMC_HELPER_TBINV(address, pc, size, p, ret);
    SMC_HELPER_TBINV_AFTER(ret, env);

do_return:
    mmap_unlock();
    return 0;
}

/* called from signal handler: invalidate the code and unprotect the
 * page. Return 0 if the fault was not handled, 1 if it was handled,
 * and 2 if it was handled but the caller must cause the TB to be
 * immediately exited. (We can only return 2 if the 'pc' argument is
 * non-zero.)
 *
 * @emu != NULL means we should invalidate only the specific TB
 * that might be modified by the store inst.
 * The *emu contains the size of the store inst.
 *
 * When return, the *emu indicates that wether the store inst needs
 * to be interpreted because the page is still not writable.
 */
int page_unprotect(target_ulong address, uintptr_t pc, int *emu)
{
    PageFlagsNode *p = NULL, *p2 = NULL;
    bool current_tb_invalidated;
    int do_mprotect = 1;
    int inv_one_tb = 0;
    int size = 1;

    /* Technically this isn't safe inside a signal handler.  However we
       know this only ever happens in a synchronous SEGV handler, so in
       practice it seems to be ok.  */
    mmap_lock();

#ifdef CONFIG_LATX_SMC_OPT
    smc_retrans_triger(tcg_tb_lookup(pc));
#endif

    if (emu) {
        inv_one_tb = 1;
        size = *emu;
        *emu = 0;
    }

    target_ulong address2 = address + size - 1;
    int is_cross = (address  & TARGET_PAGE_MASK) !=
                   (address2 & TARGET_PAGE_MASK);
    int is_cross_host = (address  & qemu_host_page_mask) !=
                        (address2 & qemu_host_page_mask);
    /* If @emu == NULL, the size will be 1 which makes address == address2.
     * Then is_cross and is_cross_host will both be false. */

    p = pageflags_find(address, address);

    /* If this address was not really writable, nothing to do. */
    if (!p || !(p->flags & PAGE_WRITE_ORG)) {
        mmap_unlock();
        return 0;
    }
    if (is_cross) {
        p2 = pageflags_find(address2, address2);
        if (!p2 || !(p2->flags & PAGE_WRITE_ORG)) {
            mmap_unlock();
            return 0;
        }
    }

    current_tb_invalidated = false;
    if ((!is_cross && p->flags & PAGE_WRITE) ||
        ( is_cross && p->flags & PAGE_WRITE && p2->flags & PAGE_WRITE)) {
        /*
         * If the page is actually marked WRITE then assume this is because
         * this thread raced with another one which got here first and
         * set the page to PAGE_WRITE and did the TB invalidate for us.
         */
#ifdef TARGET_HAS_PRECISE_SMC
        TranslationBlock *current_tb = tcg_tb_lookup(pc);
        if (current_tb) {
            current_tb_invalidated = tb_cflags(current_tb) & CF_INVALID;
        }
#endif
    } else {
#ifdef CONFIG_LATX_AOT
        if (option_aot) {
            target_ulong host_start, host_end;
            host_start = address & qemu_host_page_mask;
            host_end = host_start + qemu_host_page_size;
            page_set_page_state_range(host_start, host_end, PAGE_SMC);
            /* fprintf(stderr, "page_unprotect\n"); */
        }
#endif
        target_ulong start, len, i;
        int prot;

        if (inv_one_tb) {
            /* === create shadow page for smc === */
            if (latx_smc_shmm()) {
                smc_shmm_create();
                SMC_CREATE_SHADOW_PAGE_SHMM(address, p);
                if (is_cross_host) {
                    SMC_CREATE_SHADOW_PAGE_SHMM(address2, p2);
                }
            }
            /*
             * If shmm is used, the following creating shadow page operation
             * will not be performed. Because the shared-map shadow page is
             * already created.
             *
             * If shmm is disabled or fails, then just fallthrough to use the
             * original shadow page strategy.
             */
            SMC_CREATE_SHADOW_PAGE(address, p);
            if (is_cross_host) {
                SMC_CREATE_SHADOW_PAGE(address2, p2);
            }
            /* The smc page must be shadow page here, no need mprotect */
            do_mprotect = 0;

            /* === do tb invalidate === */
            int tbcount = size;
            /* when call : tbcount contains the size of the store inst. */
            current_tb_invalidated =
                tb_invalidate_phys_page_unwind(address, pc, &tbcount);
            /* when return:
             * if not cross page :
             *     tbcount = 1 : page contains other tb : not allow write
             *     tbcount = 0 : page contains no tb    : ok to write
             * if cross page :
             *     tbcount bit[0] for page of address
             *     tbcount bit[1] for page of the next
             */

            /* === wether guest page is writable === */
            if (tbcount) {
                /*
                 * write is still not allowed on this page,
                 * or at least one of the two pages when cross.
                 * The original store inst needs to be interpreted.
                 *
                 * If cross, the value could be 0b01 or 0b10, which
                 * means one of the two pages becomes writable.
                 * But for simplify, just interpret here.
                 */
                *emu = 1;
            } else {
                /*
                 * Enable write on this page.
                 *
                 * When cross, here the 2nd page is also writable.
                 * Enable write on the 2nd page too.
                 */
                if (qemu_host_page_size <= TARGET_PAGE_SIZE) {
                    SMC_ENABLE_PAGE_WRITE(address, p);
                    if (is_cross) {
                        SMC_ENABLE_PAGE_WRITE(address2, p2);
                    }
                    /* no need interpret since the pages are writable */
                    *emu = 0;
                    /* make guest page writable if shmm used */
                    if (latx_smc_shmm()) {
                        SMC_SHMM_RECOVER_WRITE(address, p->flags & PAGE_BITS);
                        if (is_cross) {
                            SMC_SHMM_RECOVER_WRITE(address2, p2->flags & PAGE_BITS);
                        }
                    }
                } else {
                    start = address & qemu_host_page_mask;
                    len = qemu_host_page_size;
                    prot = PAGE_BITS;
                    /* for all guest pages in this host pages */
                    for (i = 0; i < len; i += TARGET_PAGE_SIZE) {
                        target_ulong addr = start + i;
                        p = pageflags_find(addr, addr);
                        if (!p) continue;
                        /* enable write on the related guest page */
                        if (addr == (address & TARGET_PAGE_MASK))
                            SMC_ENABLE_PAGE_WRITE(address, p);
                        if (is_cross && addr == (address2 & TARGET_PAGE_MASK))
                            SMC_ENABLE_PAGE_WRITE(address2, p);
                        /* accumulate to get the final prot of the host page */
                        prot &= p->flags;
                    }
                    /* enable write on the 2nd guest page if cross host */
                    if (is_cross_host) {
                        SMC_ENABLE_PAGE_WRITE(address2, p2);
                    }
                    /*
                     * The original store inst needs to be interpreted if the
                     * host page is not writable when not cross host page.
                     *
                     * If cross host, since the 2nd host page's prot is not
                     * parsed here, so we do not know if it is writable.
                     * Just interpret the store inst for simplify.
                     */
                    if ((prot & PAGE_WRITE) && !is_cross_host) {
                        *emu = 0;
                        if (latx_smc_shmm()) {
                            SMC_SHMM_RECOVER_WRITE(address, prot & PAGE_BITS);
                        }
                    } else {
                        *emu = 1;
                    }
                }
            }
        } else if (qemu_host_page_size <= TARGET_PAGE_SIZE) {
            start = address & TARGET_PAGE_MASK;
            len = TARGET_PAGE_SIZE;
            prot = p->flags | PAGE_WRITE;
            pageflags_set_clear(start, start + len - 1, PAGE_WRITE, 0);
            current_tb_invalidated = tb_invalidate_phys_page_unwind(start, pc, NULL);
        } else {
            start = address & qemu_host_page_mask;
            len = qemu_host_page_size;
            prot = 0;

            for (i = 0; i < len; i += TARGET_PAGE_SIZE) {
                target_ulong addr = start + i;

                p = pageflags_find(addr, addr);
                if (p) {
                    prot |= p->flags;
                    if (p->flags & PAGE_WRITE_ORG) {
                        prot |= PAGE_WRITE;
                        pageflags_set_clear(addr, addr + TARGET_PAGE_SIZE - 1,
                                            PAGE_WRITE, 0);
                    }
                }
                /*
                * Since the content will be modified, we must invalidate
                * the corresponding translated code.
                */
                current_tb_invalidated |=
                    tb_invalidate_phys_page_unwind(addr, pc, NULL);
            }
        }
        if (do_mprotect) {
            if (prot & PAGE_EXEC) {
                prot = (prot & ~PAGE_EXEC) | PAGE_READ;
            }
            if (!is_shadow_page_not_shmm(address)) {
                mprotect((void *)g2h_untagged(start), len, prot & PAGE_BITS);
            }
        }
    }
    mmap_unlock();

    /* If current TB was invalidated return to main loop */
    return current_tb_invalidated ? 2 : 1;
}

#ifdef CONFIG_LATX_AOT
typedef struct PageStateNode {
    IntervalTreeNode itree;
    uint8 page_state;
} PageStateNode;

static IntervalTreeRoot page_state_root;

static PageStateNode *page_state_find(target_ulong start, target_ulong last)
{
    IntervalTreeNode *n;

    n = interval_tree_iter_first(&page_state_root, start, last);
    return n ? container_of(n, PageStateNode, itree) : NULL;
}

int page_get_page_state(target_ulong address)
{
    PageStateNode *p = page_state_find(address, address);

    if (p) {
        return p->page_state;
    } else {
        return 0;
    }
}

static void page_state_create(target_ulong start, target_ulong last, int page_state)
{
    PageStateNode *p = g_new(PageStateNode, 1);

    p->itree.start = start;
    p->itree.last = last;
    p->page_state = page_state;
    interval_tree_insert(&p->itree, &page_state_root);
}

static void page_state_unset(target_ulong start, target_ulong last)
{
    while (true) {
        PageStateNode *p = page_state_find(start, last);
        target_ulong p_last;

        if (!p) {
            break;
        }

        interval_tree_remove(&p->itree, &page_state_root);
        p_last = p->itree.last;

        if (p->itree.start < start) {
            /* Truncate the node from the end, or split out the middle. */
            p->itree.last = start - 1;
            interval_tree_insert(&p->itree, &page_state_root);
            if (last < p_last) {
                page_state_create(last + 1, p_last, p->page_state);
                break;
            }
        } else if (p_last <= last) {
            /* Range completely covers node -- remove it. */
            g_free(p);
        } else {
            /* Truncate the node from the start. */
            p->itree.start = last + 1;
            interval_tree_insert(&p->itree, &page_state_root);
            break;
        }
    }
    return;
}

static void page_state_create_merge(target_ulong start, target_ulong last,
                                   int page_state)
{
    PageStateNode *next = NULL, *prev = NULL;

    if (start > 0) {
        prev = page_state_find((start - 1) & TARGET_PAGE_MASK, start - 1);
        if (prev) {
            if (prev->page_state == page_state) {
                interval_tree_remove(&prev->itree, &page_state_root);
            } else {
                prev = NULL;
            }
        }
    }
    if (last + TARGET_PAGE_SIZE != 0) {
        next = page_state_find(last + 1, last + TARGET_PAGE_SIZE);
        if (next) {
            if (next->page_state == page_state) {
                interval_tree_remove(&next->itree, &page_state_root);
            } else {
                next = NULL;
            }
        }
    }

    if (prev) {
        if (next) {
            prev->itree.last = next->itree.last;
            g_free(next);
        } else {
            prev->itree.last = last;
        }
        interval_tree_insert(&prev->itree, &page_state_root);
    } else if (next) {
        next->itree.start = start;
        interval_tree_insert(&next->itree, &page_state_root);
    } else {
        page_state_create(start, last, page_state);
    }
}

static void page_state_set_clear(target_ulong start, target_ulong last,
                                int set_state)
{
    PageStateNode *p;
    target_ulong p_start, p_last;
    int p_state;

 restart:
    p = page_state_find(start, last);
    if (!p) {
        if (set_state) {
            page_state_create_merge(start, last, set_state);
        }
        goto done;
    }

    p_start = p->itree.start;
    p_last = p->itree.last;
    p_state = p->page_state;

    if (start == p_start && last == p_last) {
        if (set_state) {
            p->page_state = set_state;
        } else {
            interval_tree_remove(&p->itree, &page_state_root);
            g_free(p);
        }
        goto done;
    }

    if (set_state == p_state) {
        if (start < p_start) {
            interval_tree_remove(&p->itree, &page_state_root);
            p->itree.start = start;
            interval_tree_insert(&p->itree, &page_state_root);
        }
        if (p_last < last) {
            start = p_last + 1;
            goto restart;
        }
        goto done;
    }

    interval_tree_remove(&p->itree, &page_state_root);
    if (p_start < start) {
        p->itree.last = start - 1;
        interval_tree_insert(&p->itree, &page_state_root);

        if (p_last < last) {
            goto restart;
        }
        if (last < p_last) {
            page_state_create(last + 1, p_last, p_state);
        }
    } else if (last < p_last) {
        p->itree.start = last + 1;
        interval_tree_insert(&p->itree, &page_state_root);
    } else {
        g_free(p);
        goto restart;
    }
    if (set_state) {
        page_state_create(start, last, set_state);
    }

 done:
    return;
}

void page_set_page_state_range(target_ulong start, target_ulong end, int state)
{
    start = start & TARGET_PAGE_MASK;
    end = TARGET_PAGE_ALIGN(end);
    target_ulong last = end -1;

    if (!state) {
        page_state_unset(start, last);
    }

    if (state) {
        page_state_set_clear(start, last, state);
    }
}

void page_set_page_state(target_ulong addr, int state)
{
    page_set_page_state_range(addr & TARGET_PAGE_MASK,
            (addr + TARGET_PAGE_SIZE) & TARGET_PAGE_MASK, state);
}

void page_flush_page_state(target_ulong start, target_ulong end)
{
    IntervalTreeNode *n, *next;
    target_ulong last;

    if (start && end) {
        start = start & TARGET_PAGE_MASK;
        last = TARGET_PAGE_ALIGN(end) - 1;
    } else {
        start = 0;
        last =-1;
    }

    for (n = interval_tree_iter_first(&page_state_root, start, last),
        next = n ? interval_tree_iter_next(n, start, last) : NULL;
        n != NULL;
        n = next,
        next = next ? interval_tree_iter_next(n, start, last) : NULL) {
        PageStateNode *p = container_of(n, PageStateNode, itree);
        if (p->page_state == PAGE_LOADED) {
            if (p->itree.start >= start && p->itree.last <= last) {
                p->page_state = PAGE_FLUSH;
            } else {
                target_ulong tmp_start, tmp_last;
                tmp_start = start > p->itree.start ? start : p->itree.start;
                tmp_last = last < p->itree.last ? last : p->itree.last;
                page_set_page_state_range(tmp_start, tmp_last, PAGE_FLUSH);
            }
        }
    }
}

#endif

#endif /* CONFIG_USER_ONLY */

/* This is a wrapper for common code that can not use CONFIG_SOFTMMU */
void tcg_flush_softmmu_tlb(CPUState *cs)
{
#ifdef CONFIG_SOFTMMU
    tlb_flush(cs);
#endif
}

#ifdef CONFIG_USER_ONLY
void *set_shadow_page(target_ulong orig_page, void *shadow_p, int64_t access_off)
{
    ShadowPageDesc *shadow_pd = page_get_target_data(orig_page);
    if (shadow_pd == NULL) {
        /* manage the target page for the first time */
        size_t alloc_size = sizeof(ShadowPageDesc);
        shadow_pd = page_alloc_target_data(orig_page, alloc_size);
        assert(shadow_pd != NULL);
        shadow_pd->p_addr = shadow_p;
        shadow_pd->access_off = access_off;
            qemu_log_mask(LAT_LOG_MEM, "[LATX_16K] %s orig_p 0x"
                    TARGET_FMT_lx " shadow_p %p access_off 0x%lx\n",
                    __func__, orig_page, shadow_p, access_off);
        spm_set(orig_page, TARGET_PAGE_SIZE);
    } else {
        /* this page has been managed, shadow page needs to be released first */
        mmap_lock();
        munmap(shadow_pd->p_addr, qemu_host_page_size);
        shadow_pd->p_addr = shadow_p;
        shadow_pd->access_off = access_off;
        qemu_log_mask(LAT_LOG_MEM, "[LATX_16K] %s orig_p 0x"
                TARGET_FMT_lx " shadow_p %p access_off 0x%lx "
                "(this page has been managed)\n",
                __func__, orig_page, shadow_p, access_off);
        spm_clear(orig_page, TARGET_PAGE_SIZE);
        mmap_unlock();
    }
    return shadow_pd;
}
/*
 * Now, we assume that it's not the worst case in the target_mmap.
 */
#if defined(__loongarch__)
static void write_by_byte(int64_t addr, int64_t value, int byte_count)
{
    for (int i = 0; i < byte_count; i++) {
        *(char *)addr = (char)(value >> (i * 8) & 0xff);
        addr += 1;
    }
}

static int64_t read_by_byte(int64_t addr, int64_t value,
        int byte_count, int offset)
{
    for (int i = 0; i < byte_count; i++) {
        value |= (int64_t)(*(char *)addr & 0xff) << ((i + offset) << 3);
        addr += 1;
    }
    return value;
}

static int64_t over_page_read(int64_t mem_addr, int deal_byte_count)
{
    int64_t page2_addr = (mem_addr & TARGET_PAGE_MASK) + TARGET_PAGE_SIZE;
    int page1_byte_count = page2_addr - mem_addr;
    if (deal_byte_count < page1_byte_count) {
        page1_byte_count = deal_byte_count;
    }

    ShadowPageDesc *spd = page_get_target_data(mem_addr);
    if (spd) {
        mem_addr += spd->access_off;
    }

    if (page1_byte_count >= 8) {
        return *(int64_t *)mem_addr;
    }

    int64_t value = read_by_byte(mem_addr, 0, page1_byte_count, 0);
    if (page1_byte_count < deal_byte_count) {
        spd = page_get_target_data(page2_addr);
        if (spd) {
            page2_addr += spd->access_off;
        }
        return read_by_byte(page2_addr, value, deal_byte_count - page1_byte_count,
                page1_byte_count);
    }
    return value;
}

static uint64_t read_by_byte_u(int64_t addr, uint64_t value,
        int byte_count, int offset)
{
    for (int i = 0; i < byte_count; i++) {
        value |= (uint64_t)(*(char *)addr & 0xff) << ((i + offset) << 3);
        addr += 1;
    }
    return value;
}

static uint64_t over_page_read_u(int64_t mem_addr, int deal_byte_count)
{
    int64_t page2_addr = (mem_addr & TARGET_PAGE_MASK) + TARGET_PAGE_SIZE;
    int page1_byte_count = page2_addr - mem_addr;
    if (deal_byte_count < page1_byte_count) {
        page1_byte_count = deal_byte_count;
    }

    ShadowPageDesc *spd = page_get_target_data(mem_addr);
    if (spd) {
        mem_addr += spd->access_off;
    }
    uint64_t value = read_by_byte_u(mem_addr, 0, page1_byte_count, 0);
    if (page1_byte_count < deal_byte_count) {
        spd = page_get_target_data(page2_addr);
        if (spd) {
            page2_addr += spd->access_off;
        }
        return read_by_byte_u(page2_addr, value, deal_byte_count - page1_byte_count,
                page1_byte_count);
    }
    return value;
}

/* Only support st/ld in current version. */
static void over_page_write(int64_t mem_addr, int64_t value, int deal_byte_count)
{
    int64_t page2_addr = (mem_addr & TARGET_PAGE_MASK) + TARGET_PAGE_SIZE;
    int page1_byte_count = page2_addr - mem_addr;
    if (deal_byte_count < page1_byte_count) {
        page1_byte_count = deal_byte_count;
    }
    ShadowPageDesc *spd = page_get_target_data(mem_addr);
    if (spd) {
        mem_addr += spd->access_off;
    }
    /* Write page1. */
    write_by_byte(mem_addr, value, page1_byte_count);
    /* Write page2. */
    if (page1_byte_count < deal_byte_count) {
        deal_byte_count -= page1_byte_count;
        value >>= (page1_byte_count << 3);
        spd = page_get_target_data(page2_addr);
        if (spd) {
            page2_addr += spd->access_off;
        }
        write_by_byte(page2_addr, value, deal_byte_count);
    }
}

static bool is_over_page(int64_t addr1, int64_t addr2)
{
    return (addr1 & TARGET_PAGE_MASK) != (addr2 & TARGET_PAGE_MASK);
}

static bool no_right(int64_t addr, int bit_count,
        uint32_t test_flags, int64_t *siaddr)
{
    int page_flags = page_get_flags(addr);
    if (!(page_flags & test_flags)) {
        *siaddr = addr;
        return true;
    }
    if (is_over_page(addr, addr + bit_count - 1)) {
        page_flags = page_get_flags(addr + bit_count - 1);
        if (!(page_flags & test_flags)) {
            *siaddr = (addr + bit_count) & TARGET_PAGE_MASK;
            return true;
        }
    }
    return false;
}

#ifndef CONFIG_LOONGARCH_NEW_WORLD
static void set_interpret_glue_code(ucontext_t *uc, unsigned int inst, int rj)
{
    int cpu_index = current_cpu->cpu_index;
    uint32_t *glue = cpu_index * 4 + interpret_glue;

    /* save guest addr to scr0 */
    UC_SCR(uc)[0] = UC_GR(uc)[rj];
    /* save epc to scr1 */
    UC_SCR(uc)[1] = UC_PC(uc) + 4;
    /*
     * glue:
     *     inst
     *     movscr2gr rj,0
     *     jiscr1 0
     */
    glue[0] = inst;
    glue[1] = ((0x18 << 7) | rj);
    glue[2] = 0x48000300;
    UC_PC(uc) = (unsigned long)glue;
}
void smc_set_interpret_glue_code(void *uc, unsigned int inst, int rj)
{
    set_interpret_glue_code(uc, inst, rj);
}
#endif

int shared_private_interpret(siginfo_t *info, ucontext_t *uc)
{
    /* fd: fd/vd/xd */
    uint32_t inst, rd, rj, rk, fd;
    int64_t mem_addr, value, siaddr;

    siaddr = (int64_t)info->si_addr;
#ifdef CONFIG_LATX_PROFILER
    TCGProfile *prof = &tcg_ctx->prof;
    qatomic_inc(&prof->acc_spage_count);
#endif

    ShadowPageDesc *shadow_pd = page_get_target_data((target_ulong)siaddr);
    assert(shadow_pd != NULL);

    /* extract inst info */
    inst = *(uint32_t *)UC_PC(uc);
    rd = inst & 0x1f;
    rj = (inst >> 5) & 0x1f;
    int16_t imm12 = (inst >> 10) & 0xfff;
    imm12 = (int16_t)(imm12 << 4) >> 4;
    int64_t real_guest_addr = UC_GR(uc)[rj] + imm12;

    /* calculate the address */
    mem_addr = siaddr + shadow_pd->access_off;

#ifndef CONFIG_LOONGARCH_NEW_WORLD
    /*
     * In old world:
     * WARNING: the offset of __fregs in uc->uc_mcontext is incorrent,
     * we fix the issue by adding a extra offset, the solution is only
     * temporary!
     */
    fd = rd + 1;
#else
    fd = rd;
    struct extctx_layout extctx;
    memset(&extctx, 0, sizeof(extctx));
    /* we need to parse the extcontext data */
    parse_extcontext(uc, &extctx);
#endif

#ifdef CONFIG_SHDWRT_DBGMSG
    qemu_log_mask(LAT_LOG_MEM, "[LATX_16K] %s inst 0x%x epc 0x%llx "
            "siaddr 0x%lx h2g(siaddr) 0x%lx shadow page 0x%lx\n",
            __func__, inst, UC_PC(uc), siaddr,
            h2g(siaddr), mem_addr);
#endif
    switch (inst >> 24) {
    case 0x24: /* LDPTR.W */
        if (no_right(real_guest_addr, 4, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = over_page_read(real_guest_addr, 4);
        value = value << 32 >> 32;
        UC_GR(uc)[rd] = value;
        goto end;
    case 0x25: /* STPTR.W */
        if (no_right(real_guest_addr, 4, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = UC_GR(uc)[rd];
        over_page_write(real_guest_addr, value, 4);
        goto end;
    case 0x26: /* LDPTR.D */
        if (no_right(real_guest_addr, 8, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = over_page_read(real_guest_addr, 8);
        UC_GR(uc)[rd] = value;
        goto end;
    case 0x27: /* STPTR.D */
        if (no_right(real_guest_addr, 8, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = UC_GR(uc)[rd];
        over_page_write(real_guest_addr, value, 8);
        goto end;
    case 0x20: /* LL.W */
    case 0x22: /* LL.D */
        /* FIXME. */
        if (no_right(siaddr, 4, PAGE_READ | PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        assert(rj >= 11 && rj <= 20);
        UC_GR(uc)[rj] += shadow_pd->access_off;
        UC_PC(uc) -= 4;
        goto end;
    }

    switch (inst >> 22) {
    case 0xa0: /* LD.B */
        if (no_right(real_guest_addr, 1, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = *(char *)mem_addr;
        value = value << 56 >> 56;
        UC_GR(uc)[rd] = value;
        goto end;
    case 0xa1: /* LD.H */
        if (no_right(real_guest_addr, 2, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = over_page_read(real_guest_addr, 2);
        value = value << 48 >> 48;
        UC_GR(uc)[rd] = value;
        goto end;
    case 0xa2: /* LD.W */
        if (no_right(real_guest_addr, 4, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = over_page_read(real_guest_addr, 4);
        value = value << 32 >> 32;
        UC_GR(uc)[rd] = value;
        goto end;
    case 0xa3: /* LD.D */
        if (no_right(real_guest_addr, 8, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = over_page_read(real_guest_addr, 8);
        UC_GR(uc)[rd] = value;
        goto end;
    case 0xa4: /* ST.B */
        if (no_right(real_guest_addr, 1, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = UC_GR(uc)[rd];
        *(char *)mem_addr = (char)value;
        goto end;
    case 0xa5: /* ST.H */
        if (no_right(real_guest_addr, 2, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = UC_GR(uc)[rd];
        over_page_write(real_guest_addr, value, 2);
        goto end;
    case 0xa6: /* ST.W */
        if (no_right(real_guest_addr, 4, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = UC_GR(uc)[rd];
        over_page_write(real_guest_addr, value, 4);
        goto end;
    case 0xa7: /* ST.D */
        if (no_right(real_guest_addr, 8, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = UC_GR(uc)[rd];
        over_page_write(real_guest_addr, value, 8);
        goto end;
    case 0xa8: /* LD.BU */
        if (no_right(real_guest_addr, 1, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = *(char *)mem_addr;
        value = (uint64_t)value << 56 >> 56;
        UC_GR(uc)[rd] = value;
        goto end;
    case 0xa9: /* LD.HU */
        if (no_right(real_guest_addr, 2, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = over_page_read_u(real_guest_addr, 2);
        value = (uint64_t)value << 48 >> 48;
        UC_GR(uc)[rd] = value;
        goto end;
    case 0xaa: /* LD.WU */
        if (no_right(real_guest_addr, 4, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = over_page_read_u(real_guest_addr, 4);
        value = (uint64_t)value << 32 >> 32;
        UC_GR(uc)[rd] = value;
        goto end;
    case 0xab: /* PRELD */
        goto end;
#ifndef CONFIG_LOONGARCH_NEW_WORLD
    case 0xac: /* FLD.S */
        if (no_right(real_guest_addr, 4, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        assert(siaddr == real_guest_addr);
        if (reg_itemp_reverse_map[rj] == INVALID_TEMP) {
            set_interpret_glue_code(uc, inst, rj);
        }
        UC_GR(uc)[rj] += shadow_pd->access_off;
        goto ret;
    case 0xad: /* FST.S */
        if (no_right(real_guest_addr, 4, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        assert(siaddr == real_guest_addr);
        if (reg_itemp_reverse_map[rj] == INVALID_TEMP) {
            set_interpret_glue_code(uc, inst, rj);
        }
        UC_GR(uc)[rj] += shadow_pd->access_off;
        goto ret;
    case 0xae: /* FLD.D */
        if (no_right(real_guest_addr, 8, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        assert(siaddr == real_guest_addr);
        if (reg_itemp_reverse_map[rj] == INVALID_TEMP) {
            set_interpret_glue_code(uc, inst, rj);
        }
        UC_GR(uc)[rj] += shadow_pd->access_off;
        goto ret;
    case 0xaf: /* FST.D */
        if (no_right(real_guest_addr, 8, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        assert(siaddr == real_guest_addr);
        if (reg_itemp_reverse_map[rj] == INVALID_TEMP) {
            set_interpret_glue_code(uc, inst, rj);
        }
        UC_GR(uc)[rj] += shadow_pd->access_off;
        goto ret;
    case 0xb0: /* VLD */
        if (no_right(real_guest_addr, 16, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        UC_FREG(uc)[fd].__val64[0] = over_page_read(real_guest_addr, 8);
        UC_FREG(uc)[fd].__val64[1] = over_page_read(real_guest_addr + 8, 8);
        goto end;
    case 0xb1: /* VST */
        if (no_right(real_guest_addr, 16, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        over_page_write(real_guest_addr, UC_FREG(uc)[fd].__val64[0], 8);
        over_page_write(real_guest_addr + 8, UC_FREG(uc)[fd].__val64[1], 8);
        goto end;
    case 0xb2: /* XVLD */
        if (no_right(real_guest_addr, 32, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        UC_FREG(uc)[fd].__val64[0] = over_page_read(real_guest_addr, 8);
        UC_FREG(uc)[fd].__val64[1] = over_page_read(real_guest_addr + 8, 8);
        UC_FREG(uc)[fd].__val64[2] = over_page_read(real_guest_addr + 16, 8);
        UC_FREG(uc)[fd].__val64[3] = over_page_read(real_guest_addr + 24, 8);
        goto end;
    case 0xb3: /* XVST */
        if (no_right(real_guest_addr, 32, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        over_page_write(real_guest_addr, UC_FREG(uc)[fd].__val64[0], 8);
        over_page_write(real_guest_addr + 8, UC_FREG(uc)[fd].__val64[1], 8);
        over_page_write(real_guest_addr + 16, UC_FREG(uc)[fd].__val64[2], 8);
        over_page_write(real_guest_addr + 24, UC_FREG(uc)[fd].__val64[3], 8);
        goto end;
    case 0xc0:
        if (inst & (1<<21)) {
            /* VLDREPL.W */
            for(int i = 0; i < 4; i++) {
                UC_FREG(uc)[fd].__val32[i] = *(int32_t *)mem_addr;
            }
            goto end;
        }
        /* VLDREPL.D */
        for(int i = 0; i < 2; i++) {
            UC_FREG(uc)[fd].__val64[i] = *(int64_t *)mem_addr;
        }
        goto end;
    case 0xc1: /* VLDREPL.H */
        for(int i = 0; i < 4; i++) {
            *((int16_t *)&UC_FREG(uc)[fd].__val32[i] + 0) = *(int16_t *)mem_addr;
            *((int16_t *)&UC_FREG(uc)[fd].__val32[i] + 1) = *(int16_t *)mem_addr;
        }
        goto end;
    case 0xc2: /* VLDREPL.B */
        for(int i = 0; i < 4; i++) {
            *((int8_t *)&UC_FREG(uc)[fd].__val32[i] + 0) = *(int8_t *)mem_addr;
            *((int8_t *)&UC_FREG(uc)[fd].__val32[i] + 1) = *(int8_t *)mem_addr;
            *((int8_t *)&UC_FREG(uc)[fd].__val32[i] + 2) = *(int8_t *)mem_addr;
            *((int8_t *)&UC_FREG(uc)[fd].__val32[i] + 3) = *(int8_t *)mem_addr;
        }
        goto end;
    case 0xc4:
        if (inst & (1<<21)) {
            /* VSTELM.W */
            int idx = (inst >> 18) & 0x3;
            *(int32_t *)mem_addr = UC_FREG(uc)[fd].__val32[idx];
        } else {
            /* VSTELM.D */
            int idx = (inst >> 18) & 0x1;
            *(int64_t *)mem_addr = UC_FREG(uc)[fd].__val64[idx];
        }
        goto end;
    case 0xc5:
    {
        /* VSTELM.H */
        int idx = (inst >> 18) & 0x7;
        int i = idx & 0x1;
        idx = idx >> 1;
        *(int16_t *)mem_addr = *((int16_t *)&UC_FREG(uc)[fd].__val32[idx] + i);
        goto end;
    }
    case 0xc6:
    {
        /* VSTELM.B */
        int idx = (inst >> 18) & 0xf;
        int i = idx & 0x3;
        idx = idx >> 2;
        *(int8_t *)mem_addr = *((int8_t *)&UC_FREG(uc)[fd].__val32[idx] + i);
        goto end;
    }
    case 0xc8:
        if (inst & (1<<21)) {
            /* XVLDREPL.W */
            for(int i = 0; i < 8; i++) {
                UC_FREG(uc)[fd].__val32[i] = *(int32_t *)mem_addr;
            }
            goto end;
        }
        /* XVLDREPL.D */
        for(int i = 0; i < 4; i++) {
            UC_FREG(uc)[fd].__val64[i] = *(int64_t *)mem_addr;
        }
        goto end;
    case 0xc9:
        /* XVLDREPL.H */
        for(int i = 0; i < 8; i++) {
            *((int16_t *)&UC_FREG(uc)[fd].__val32[i] + 0) = *(int16_t *)mem_addr;
            *((int16_t *)&UC_FREG(uc)[fd].__val32[i] + 1) = *(int16_t *)mem_addr;
        }
        goto end;
    case 0xca:
        /* XVLDREPL.B */
        for(int i = 0; i < 8; i++) {
            *((int8_t *)&UC_FREG(uc)[fd].__val32[i] + 0) = *(int8_t *)mem_addr;
            *((int8_t *)&UC_FREG(uc)[fd].__val32[i] + 1) = *(int8_t *)mem_addr;
            *((int8_t *)&UC_FREG(uc)[fd].__val32[i] + 2) = *(int8_t *)mem_addr;
            *((int8_t *)&UC_FREG(uc)[fd].__val32[i] + 3) = *(int8_t *)mem_addr;
        }
        goto end;
    case 0xcc:
        if (inst & (1<<21)) {
            /* XVSTELM.W */
            int idx = (inst >> 18) & 0x7;
            *(int32_t *)mem_addr = UC_FREG(uc)[fd].__val32[idx];
        } else {
            /* XVSTELM.D */
            int idx = (inst >> 18) & 0x3;
            *(int64_t *)mem_addr = UC_FREG(uc)[fd].__val64[idx];
        }
        goto end;
    case 0xcd:
    {
        /* XVSTELM.H */
        int idx = (inst >> 18) & 0xf;
        int i = idx & 0x1;
        idx = idx >> 1;
        *(int16_t *)mem_addr = *((int16_t *)&UC_FREG(uc)[fd].__val32[idx] + i);
        goto end;
    }
    case 0xce:
    case 0xcf:
    {
        /* XVSTELM.B */
        int idx = (inst >> 18) & 0x1f;
        int i = idx & 0x3;
        idx = idx >> 2;
        *(int8_t *)mem_addr = *((int8_t *)&UC_FREG(uc)[fd].__val32[idx] + i);
        goto end;
    }
#else /* CONFIG_LOONGARCH_NEW_WORLD */
    case 0xac: /* FLD.S */
      if (no_right(real_guest_addr, 4, PAGE_READ, &siaddr)) {
        info->si_addr = (void *)siaddr;
        return 1;
      }
      assert(siaddr == real_guest_addr);
      fd = fd < 8 ? (fd + UC_GET_FTOP(&extctx, uint32_t)) & 7 : fd;
      UC_SET_FPR(&extctx, fd, mem_addr, uint32_t);
      goto end;
    case 0xad: /* FST.S */
      if (no_right(real_guest_addr, 4, PAGE_WRITE, &siaddr)) {
        info->si_addr = (void *)siaddr;
        return 1;
      }
      assert(siaddr == real_guest_addr);
      fd = fd < 8 ? (fd + UC_GET_FTOP(&extctx, uint32_t)) & 7 : fd;
      *(uint32_t *)mem_addr = UC_GET_FPR(&extctx, fd, uint32_t);
      goto end;
    case 0xae: /* FLD.D */
      if (no_right(real_guest_addr, 8, PAGE_READ, &siaddr)) {
        info->si_addr = (void *)siaddr;
        return 1;
      }
      assert(siaddr == real_guest_addr);
      fd = fd < 8 ? (fd + UC_GET_FTOP(&extctx, uint32_t)) & 7 : fd;
      UC_SET_FPR(&extctx, fd, mem_addr, uint64_t);
      goto end;
    case 0xaf: /* FST.D */
      if (no_right(real_guest_addr, 8, PAGE_WRITE, &siaddr)) {
        info->si_addr = (void *)siaddr;
        return 1;
      }
      assert(siaddr == real_guest_addr);
      fd = fd < 8 ? (fd + UC_GET_FTOP(&extctx, uint32_t)) & 7 : fd;
      *(uint64_t *)mem_addr = UC_GET_FPR(&extctx, fd, uint64_t);
      goto end;
    case 0xb0: /* VLD */
      UC_SET_LSX(&extctx, fd, 0, mem_addr + 0, uint64_t);
      UC_SET_LSX(&extctx, fd, 1, mem_addr + 8, uint64_t);
      goto end;
    case 0xb1: /* VST */
      *(uint64_t *)(mem_addr + 0) = UC_GET_LSX(&extctx, fd, 0, uint64_t);
      *(uint64_t *)(mem_addr + 8) = UC_GET_LSX(&extctx, fd, 1, uint64_t);
      goto end;
    case 0xb2: /* XVLD */
      UC_SET_LASX(&extctx, fd, 0, mem_addr + 0, uint64_t);
      UC_SET_LASX(&extctx, fd, 1, mem_addr + 8, uint64_t);
      UC_SET_LASX(&extctx, fd, 2, mem_addr + 16, uint64_t);
      UC_SET_LASX(&extctx, fd, 3, mem_addr + 24, uint64_t);
      goto end;
    case 0xb3: /* XVST */
      *(uint64_t *)(mem_addr + 0) = UC_GET_LASX(&extctx, fd, 0, uint64_t);
      *(uint64_t *)(mem_addr + 8) = UC_GET_LASX(&extctx, fd, 1, uint64_t);
      *(uint64_t *)(mem_addr + 16) = UC_GET_LASX(&extctx, fd, 2, uint64_t);
      *(uint64_t *)(mem_addr + 24) = UC_GET_LASX(&extctx, fd, 3, uint64_t);
      goto end;
    case 0xc0:
      if (inst & (1 << 21)) {
        /* VLDREPL.W */
        for (int i = 0; i < 4; i++) {
          UC_SET_LSX(&extctx, fd, i, mem_addr, uint32_t);
        }
        goto end;
      }
      /* VLDREPL.D */
      for (int i = 0; i < 2; i++) {
        UC_SET_LSX(&extctx, fd, i, mem_addr, uint64_t);
      }
      goto end;
    case 0xc1: {
      /* VLDREPL.H */
      for (int i = 0; i < 8; i++) {
        UC_SET_LSX(&extctx, fd, i, mem_addr, uint16_t);
      }
      goto end;
    }
    case 0xc2: {
      /*VLDREPL.B */
      for (int i = 0; i < 16; i++) {
        UC_SET_LSX(&extctx, fd, i, mem_addr, uint8_t);
      }
      goto end;
    }
    case 0xc4:
      if (inst & (1 << 21)) {
        /* VSTELM.W */
        int idx = (inst >> 18) & 0x3;
        *(uint32_t *)mem_addr = UC_GET_LSX(&extctx, fd, idx, uint32_t);
      } else {
        /* VSTELM.D */
        int idx = (inst >> 18) & 0x1;
        *(uint64_t *)mem_addr = UC_GET_LSX(&extctx, fd, idx, uint64_t);
      }
      goto end;
    case 0xc5: {
      /* VSTELM.H */
      int idx = (inst >> 18) & 0x7;
      *(uint16_t *)mem_addr = UC_GET_LSX(&extctx, fd, idx, uint16_t);
      goto end;
    }
    case 0xc6: {
      /* VSTELM.B */
      int idx = (inst >> 18) & 0xf;
      *(uint8_t *)mem_addr = UC_GET_LSX(&extctx, fd, idx, uint8_t);
      goto end;
    }
    case 0xc8:
      if (inst & (1 << 21)) {
        /* XVLDREPL.W */
        for (int i = 0; i < 8; i++) {
          UC_SET_LASX(&extctx, fd, i, mem_addr, uint32_t);
        }
        goto end;
      }
      /* XVLDREPL.D */
      for (int i = 0; i < 4; i++) {
        UC_SET_LASX(&extctx, fd, i, mem_addr, uint64_t);
      }
      goto end;
    case 0xc9: {
      /* XVLDREPL.H */
      for (int i = 0; i < 16; i++) {
        UC_SET_LASX(&extctx, fd, i, mem_addr, uint16_t);
      }
      goto end;
    }
    case 0xca: {
      /* XVLDREPL.B */
      for (int i = 0; i < 32; i++) {
        UC_SET_LASX(&extctx, fd, i, mem_addr, uint8_t);
      }
      goto end;
    }
    case 0xcc:
      if (inst & (1 << 21)) {
        /* XVSTELM.W */
        int idx = (inst >> 18) & 0x7;
        *(uint32_t *)mem_addr = UC_GET_LASX(&extctx, fd, idx, uint32_t);
      } else {
        /* XVSTELM.D */
        int idx = (inst >> 18) & 0x3;
        *(uint64_t *)mem_addr = UC_GET_LASX(&extctx, fd, idx, uint64_t);
      }
      goto end;
    case 0xcd: {
      /* XVSTELM.H */
      int idx = (inst >> 18) & 0xf;
      *(uint16_t *)mem_addr = UC_GET_LASX(&extctx, fd, idx, uint16_t);
      goto end;
    }
    case 0xce:
    case 0xcf: {
      /* XVSTELM.B */
      int idx = (inst >> 18) & 0x1f;
      *(uint8_t *)mem_addr = UC_GET_LASX(&extctx, fd, idx, uint8_t);
      goto end;
    }
#endif
    }

    rk = (inst >> 10) & 0x1f;
    int64_t new_value = UC_GR(uc)[rk];

    real_guest_addr -= imm12;
    switch (inst >> 15) {
    case 0x7000: /* LDX.B */
        if (no_right(real_guest_addr, 1, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = *(char *)mem_addr;
        value = value << 56 >> 56;
        UC_GR(uc)[rd] = value;
        goto end;
    case 0x7008: /* LDX.H */
        if (no_right(real_guest_addr, 2, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = over_page_read(real_guest_addr, 2);
        value = value << 48 >> 48;
        UC_GR(uc)[rd] = value;
        goto end;
    case 0x7010: /* LDX.W */
        if (no_right(real_guest_addr, 4, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = over_page_read(real_guest_addr, 4);
        value = value << 32 >> 32;
        UC_GR(uc)[rd] = value;
        goto end;
    case 0x7018: /* LDX.D */
        if (no_right(real_guest_addr, 8, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = over_page_read(real_guest_addr, 8);
        UC_GR(uc)[rd] = value;
        goto end;
    case 0x7020: /* STX.B */
        if (no_right(real_guest_addr, 1, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = UC_GR(uc)[rd];
        *(char *)mem_addr = (char)value;
        goto end;
    case 0x7028: /* STX.H */
        if (no_right(real_guest_addr, 2, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = UC_GR(uc)[rd];
        over_page_write(real_guest_addr, value, 2);
        goto end;
    case 0x7030: /* STX.W */
        if (no_right(real_guest_addr, 4, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = UC_GR(uc)[rd];
        over_page_write(real_guest_addr, value, 4);
        goto end;
    case 0x7038: /* STX.D */
        if (no_right(real_guest_addr, 8, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = UC_GR(uc)[rd];
        over_page_write(real_guest_addr, value, 8);
        goto end;
    case 0x7040: /* LDX.BU */
        if (no_right(real_guest_addr, 1, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = *(char *)mem_addr;
        value = (uint64_t)value << 56 >> 56;
        UC_GR(uc)[rd] = value;
        goto end;
    case 0x7048: /* LDX.HU */
        if (no_right(real_guest_addr, 2, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = over_page_read_u(real_guest_addr, 2);
        value = (uint64_t)value << 48 >> 48;
        UC_GR(uc)[rd] = value;
        goto end;
    case 0x7050: /* LDX.WU */
        if (no_right(real_guest_addr, 4, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = over_page_read_u(real_guest_addr, 4);
        value = (uint64_t)value << 32 >> 32;
        UC_GR(uc)[rd] = value;
        goto end;
#ifndef CONFIG_LOONGARCH_NEW_WORLD
    case 0x7060: /* FLDX.S */
        if (no_right(real_guest_addr, 4, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        assert(siaddr == real_guest_addr);
        if (reg_itemp_reverse_map[rj] == INVALID_TEMP) {
            set_interpret_glue_code(uc, inst, rj);
        }
        UC_GR(uc)[rj] += shadow_pd->access_off;
        goto ret;
    case 0x7068: /* FLDX.D */
        if (no_right(real_guest_addr, 4, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        assert(siaddr == real_guest_addr);
        if (reg_itemp_reverse_map[rj] == INVALID_TEMP) {
            set_interpret_glue_code(uc, inst, rj);
        }
        UC_GR(uc)[rj] += shadow_pd->access_off;
        goto ret;
    case 0x7070: /* FSTX.S */
        if (no_right(real_guest_addr, 8, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        assert(siaddr == real_guest_addr);
        if (reg_itemp_reverse_map[rj] == INVALID_TEMP) {
            set_interpret_glue_code(uc, inst, rj);
        }
        UC_GR(uc)[rj] += shadow_pd->access_off;
        goto ret;
    case 0x7078: /* FSTX.D */
        if (no_right(real_guest_addr, 8, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        assert(siaddr == real_guest_addr);
        if (reg_itemp_reverse_map[rj] == INVALID_TEMP) {
            set_interpret_glue_code(uc, inst, rj);
        }
        UC_GR(uc)[rj] += shadow_pd->access_off;
        goto ret;
    case 0x7080: /* VLDX */
        if (no_right(real_guest_addr, 16, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        UC_FREG(uc)[fd].__val64[0] = over_page_read(real_guest_addr, 8);
        UC_FREG(uc)[fd].__val64[1] = over_page_read(real_guest_addr + 8, 8);
        goto end;
    case 0x7088: /* VSTX */
        if (no_right(real_guest_addr, 16, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        over_page_write(real_guest_addr, UC_FREG(uc)[fd].__val64[0], 8);
        over_page_write(real_guest_addr + 8, UC_FREG(uc)[fd].__val64[1], 8);
        goto end;
    case 0x7090: /* XVLDX */
        if (no_right(real_guest_addr, 32, PAGE_READ, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        UC_FREG(uc)[fd].__val64[0] = over_page_read(real_guest_addr, 8);
        UC_FREG(uc)[fd].__val64[1] = over_page_read(real_guest_addr + 8, 8);
        UC_FREG(uc)[fd].__val64[2] = over_page_read(real_guest_addr + 16, 8);
        UC_FREG(uc)[fd].__val64[3] = over_page_read(real_guest_addr + 24, 8);
        goto end;
    case 0x7098: /* XVSTX */
        if (no_right(real_guest_addr, 32, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        over_page_write(real_guest_addr, UC_FREG(uc)[fd].__val64[0], 8);
        over_page_write(real_guest_addr + 8, UC_FREG(uc)[fd].__val64[1], 8);
        over_page_write(real_guest_addr + 16, UC_FREG(uc)[fd].__val64[2], 8);
        over_page_write(real_guest_addr + 24, UC_FREG(uc)[fd].__val64[3], 8);
        goto end;
#else /* CONFIG_LOONGARCH_NEW_WORLD */
    case 0x7060: /* FLDX.S */
      if (no_right(real_guest_addr, 4, PAGE_READ, &siaddr)) {
        info->si_addr = (void *)siaddr;
        return 1;
      }
      assert(siaddr == real_guest_addr);
      fd = fd < 8 ? (fd + UC_GET_FTOP(&extctx, uint32_t)) & 7 : fd;
      UC_SET_FPR(&extctx, fd, mem_addr, uint32_t);
      goto end;
    case 0x7068: /* FLDX.D */
      if (no_right(real_guest_addr, 8, PAGE_READ, &siaddr)) {
        info->si_addr = (void *)siaddr;
        return 1;
      }
      assert(siaddr == real_guest_addr);
      fd = fd < 8 ? (fd + UC_GET_FTOP(&extctx, uint32_t)) & 7 : fd;
      UC_SET_FPR(&extctx, fd, mem_addr, uint64_t);
      goto end;
    case 0x7070: /* FSTX.S */
      if (no_right(real_guest_addr, 4, PAGE_WRITE, &siaddr)) {
        info->si_addr = (void *)siaddr;
        return 1;
      }
      assert(siaddr == real_guest_addr);
      fd = fd < 8 ? (fd + UC_GET_FTOP(&extctx, uint32_t)) & 7 : fd;
      *(int32_t *)mem_addr = UC_GET_FPR(&extctx, fd, uint32_t);
      goto end;
    case 0x7078: /* FSTX.D */
      if (no_right(real_guest_addr, 8, PAGE_WRITE, &siaddr)) {
        info->si_addr = (void *)siaddr;
        return 1;
      }
      assert(siaddr == real_guest_addr);
      fd = fd < 8 ? (fd + UC_GET_FTOP(&extctx, uint32_t)) & 7 : fd;
      *(int64_t *)mem_addr = UC_GET_FPR(&extctx, fd, uint64_t);
      goto end;
    case 0x7080: /* VLDX */
      if (no_right(real_guest_addr, 16, PAGE_READ, &siaddr)) {
        info->si_addr = (void *)siaddr;
        return 1;
      }
      for (int i = 0; i < 2; i++) {
        int64_t v64 = over_page_read(real_guest_addr + (i * 8), 8);
        UC_SET_LSX(&extctx, fd, i, &v64, int64_t);
      }
      goto end;
    case 0x7088: /* VSTX */
      if (no_right(real_guest_addr, 16, PAGE_WRITE, &siaddr)) {
        info->si_addr = (void *)siaddr;
        return 1;
      }
      for (int i = 0; i < 2; i++) {
        over_page_write(real_guest_addr + (i * 8),
                        UC_GET_LSX(&extctx, fd, i, int64_t), 8);
      }
      goto end;
    case 0x7090: /* XVLDX */
      if (no_right(real_guest_addr, 32, PAGE_READ, &siaddr)) {
        info->si_addr = (void *)siaddr;
        return 1;
      }
      for (int i = 0; i < 4; i++) {
        int64_t v64 = over_page_read(real_guest_addr + (i * 8), 8);
        UC_SET_LASX(&extctx, fd, i, &v64, int64_t);
      }
      goto end;
    case 0x7098: /* XVSTX */
      if (no_right(real_guest_addr, 32, PAGE_WRITE, &siaddr)) {
        info->si_addr = (void *)siaddr;
        return 1;
      }
      for (int i = 0; i < 4; i++) {
        over_page_write(real_guest_addr + (i * 8),
                        UC_GET_LASX(&extctx, fd, i, int64_t), 8);
      }
      goto end;
#endif
    case 0x70c0: /* AMSWAP.W */
    case 0x70d2: /* AMSWAP_DB.W */
        /* set old value */
        if (no_right(real_guest_addr, 4, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = over_page_read(real_guest_addr, 4);
        value = value << 32 >> 32;
        UC_GR(uc)[rd] = value;
        /* set new value */
        over_page_write(real_guest_addr, new_value, 4);
        break;
    case 0x70c1: /* AMSWAP.D */
    case 0x70d3: /* AMSWAP_DB.D */
        if (no_right(real_guest_addr, 8, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        /* set old value */
        value = over_page_read(real_guest_addr, 8);
        UC_GR(uc)[rd] = value;
        /* set new value */
        over_page_write(real_guest_addr, new_value, 8);
        break;
    case 0x70c2: /* AMADD.W */
    case 0x70d4: /* AMADD_DB.W */
        /* set old value */
        if (no_right(real_guest_addr, 4, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        value = over_page_read(real_guest_addr, 4);
        value = value << 32 >> 32;
        UC_GR(uc)[rd] = value;
        /* calculate */
        new_value += value;
        /* set new value */
        over_page_write(real_guest_addr, new_value, 4);
        break;
    case 0x70c3: /* AMADD.D */
    case 0x70d5: /* AMADD_DB.D */
        if (no_right(real_guest_addr, 8, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        /* set old value */
        value = over_page_read(real_guest_addr, 8);
        UC_GR(uc)[rd] = value;
        /* calculate */
        new_value += value;
        /* set new value */
        over_page_write(real_guest_addr, new_value, 8);
        break;
    case 0x70c4: /* AMAND.W */
    case 0x70d6: /* AMAND_DB.W */
        if (no_right(real_guest_addr, 4, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        /* set old value */
        value = over_page_read(real_guest_addr, 4);
        value = value << 32 >> 32;
        UC_GR(uc)[rd] = value;
        /* calculate */
        new_value &= value;
        /* set new value */
        over_page_write(real_guest_addr, new_value, 4);
        break;
    case 0x70c5: /* AMAND.D */
    case 0x70d7: /* AMAND_DB.D */
        if (no_right(real_guest_addr, 8, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        /* set old value */
        value = over_page_read(real_guest_addr, 8);
        UC_GR(uc)[rd] = value;
        /* calculate */
        new_value &= value;
        /* set new value */
        over_page_write(real_guest_addr, new_value, 8);
        break;
    case 0x70c6: /* AMOR.W */
    case 0x70d8: /* AMOR_DB.W */
        if (no_right(real_guest_addr, 4, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        /* set old value */
        value = over_page_read(real_guest_addr, 4);
        value = value << 32 >> 32;
        UC_GR(uc)[rd] = value;
        /* calculate */
        new_value |= value;
        /* set new value */
        over_page_write(real_guest_addr, new_value, 4);
        break;
    case 0x70c7: /* AMOR.D */
    case 0x70d9: /* AMOR_DB.D */
        if (no_right(real_guest_addr, 8, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        /* set old value */
        value = over_page_read(real_guest_addr, 8);
        UC_GR(uc)[rd] = value;
        /* calculate */
        new_value |= value;
        /* set new value */
        over_page_write(real_guest_addr, new_value, 8);
        break;
    case 0x70c8: /* AMXOR.W */
    case 0x70da: /* AMXOR_DB.W */
        if (no_right(real_guest_addr, 4, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        /* set old value */
        value = over_page_read(real_guest_addr, 4);
        value = value << 32 >> 32;
        UC_GR(uc)[rd] = value;
        /* calculate */
        new_value ^= value;
        /* set new value */
        over_page_write(real_guest_addr, new_value, 4);
        break;
    case 0x70c9: /* AMXOR.D */
    case 0x70db: /* AMXOR_DB.D */
        if (no_right(real_guest_addr, 8, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        /* set old value */
        value = over_page_read(real_guest_addr, 8);
        UC_GR(uc)[rd] = value;
        /* calculate */
        new_value ^= value;
        /* set new value */
        over_page_write(real_guest_addr, new_value, 8);
        break;
    case 0x70ca: /* AMMAX.W */
    case 0x70dc: /* AMMAX_DB.W */
        if (no_right(real_guest_addr, 4, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        /* set old value */
        value = over_page_read(real_guest_addr, 4);
        value = value << 32 >> 32;
        UC_GR(uc)[rd] = value;
        /* calculate */
        new_value = new_value << 32 >> 32;
        new_value = new_value > value ? new_value : value;
        /* set new value */
        over_page_write(real_guest_addr, new_value, 4);
        break;
    case 0x70cb: /* AMMAX.D */
    case 0x70cf: /* AMMAX.DU */
    case 0x70dd: /* AMMAX_DB.D */
    case 0x70e1: /* AMMAX_DB.DU */
        if (no_right(real_guest_addr, 8, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        /* set old value */
        value = over_page_read(real_guest_addr, 8);
        UC_GR(uc)[rd] = value;
        /* calculate */
        new_value = new_value > value ? new_value : value;
        /* set new value */
        over_page_write(real_guest_addr, new_value, 8);
        break;
    case 0x70cc: /* AMMIN.W */
    case 0x70de: /* AMMIN_DB.W */
        if (no_right(real_guest_addr, 4, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        /* set old value */
        value = over_page_read(real_guest_addr, 4);
        value = value << 32 >> 32;
        UC_GR(uc)[rd] = value;
        /* calculate */
        new_value = new_value << 32 >> 32;
        new_value = new_value < value ? new_value : value;
        /* set new value */
        over_page_write(real_guest_addr, new_value, 4);
        break;
    case 0x70cd: /* AMMIN.D */
    case 0x70d1: /* AMMIN.DU */
    case 0x70df: /* AMMIN_DB.D */
    case 0x70e3: /* AMMIN_DB.DU */
        if (no_right(real_guest_addr, 8, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        /* set old value */
        value = over_page_read(real_guest_addr, 8);
        UC_GR(uc)[rd] = value;
        /* calculate */
        new_value = new_value < value ? new_value : value;
        /* set new value */
        over_page_write(real_guest_addr, new_value, 8);
        break;
    case 0x70ce: /* AMMAX.WU */
    case 0x70e0: /* AMMAX_DB.WU */
        if (no_right(real_guest_addr, 4, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        /* set old value */
        value = over_page_read(real_guest_addr, 4);
        value = (uint64_t)value << 32 >> 32;
        UC_GR(uc)[rd] = value;
        /* calculate */
        new_value = new_value << 32 >> 32;
        new_value = new_value > value ? new_value : value;
        /* set new value */
        over_page_write(real_guest_addr, new_value, 4);
        break;
    case 0x70d0: /* AMMIN.WU */
    case 0x70e2: /* AMMIN_DB.WU */
        if (no_right(real_guest_addr, 4, PAGE_WRITE, &siaddr)) {
            info->si_addr = (void *)siaddr;
            return 1;
        }
        /* set old value */
        value = over_page_read(real_guest_addr, 4);
        value = (uint64_t)value << 32 >> 32;
        UC_GR(uc)[rd] = value;
        /* calculate */
        new_value = (uint64_t)new_value << 32 >> 32;
        new_value = new_value < value ? new_value : value;
        /* set new value */
        over_page_write(real_guest_addr, new_value, 4);
        break;
    case 0x70e8: /* FLDGT.S */
        printf("error: %s:%d unsupport inst FLDGT.S\n", __func__, __LINE__);
        goto end;
    case 0x70e9: /* FLDGT.D */
        printf("error: %s:%d unsupport inst FLDGT.D\n", __func__, __LINE__);
        goto end;
    case 0x70ea: /* FLDLE.S */
        printf("error: %s:%d unsupport inst FLDLE.S\n", __func__, __LINE__);
        goto end;
    case 0x70eb: /* FLDLE.D */
        printf("error: %s:%d unsupport inst FLDLE.D\n", __func__, __LINE__);
        goto end;
    case 0x70ec: /* FSTGT.S */
        printf("error: %s:%d unsupport inst FSTGT.S\n", __func__, __LINE__);
        goto end;
    case 0x70ed: /* FSTGT.D */
        printf("error: %s:%d unsupport inst FSTGT.D\n", __func__, __LINE__);
        goto end;
    case 0x70ee: /* FSTLE.S */
        printf("error: %s:%d unsupport inst FSTLE.S\n", __func__, __LINE__);
        goto end;
    case 0x70ef: /* FSTLE.D */
        printf("error: %s:%d unsupport inst FSTLE.D\n", __func__, __LINE__);
        goto end;
    case 0x70f0: /* LDGT.B*/
        printf("error: %s:%d unsupport inst LDGT.B\n", __func__, __LINE__);
        goto end;
    case 0x70f1: /* LDGT.H */
        printf("error: %s:%d unsupport inst LDGT.H\n", __func__, __LINE__);
        goto end;
    case 0x70f2: /* LDGT.W */
        printf("error: %s:%d unsupport inst LDGT.W\n", __func__, __LINE__);
        goto end;
    case 0x70f3: /* LDGT.D */
        printf("error: %s:%d unsupport inst LDGT.D\n", __func__, __LINE__);
        goto end;
    case 0x70f4: /* LDLE.B */
        printf("error: %s:%d unsupport inst LDLE.B\n", __func__, __LINE__);
        goto end;
    case 0x70f5: /* LDLE.H */
        printf("error: %s:%d unsupport inst LDLE.H\n", __func__, __LINE__);
        goto end;
    case 0x70f6: /* LDLE.W */
        printf("error: %s:%d unsupport inst LDLE.W\n", __func__, __LINE__);
        goto end;
    case 0x70f7: /* LDLE.D */
        printf("error: %s:%d unsupport inst LDLE.D\n", __func__, __LINE__);
        goto end;
    case 0x70f8: /* STGT.B */
        printf("error: %s:%d unsupport inst STGT.B\n", __func__, __LINE__);
        goto end;
    case 0x70f9: /* STGT.H */
        printf("error: %s:%d unsupport inst STGT.H\n", __func__, __LINE__);
        goto end;
    case 0x70fa: /* STGT.W */
        printf("error: %s:%d unsupport inst STGT.W\n", __func__, __LINE__);
        goto end;
    case 0x70fb: /* STGT.D */
        printf("error: %s:%d unsupport inst STGT.D\n", __func__, __LINE__);
        goto end;
    case 0x70fc: /* STLE.B */
        printf("error: %s:%d unsupport inst STLE.B\n", __func__, __LINE__);
        goto end;
    case 0x70fd: /* STLE.H */
        printf("error: %s:%d unsupport inst STLE.H\n", __func__, __LINE__);
        goto end;
    case 0x70fe: /* STLE.W */
        printf("error: %s:%d unsupport inst STLE.W\n", __func__, __LINE__);
        goto end;
    case 0x70ff: /* STLE.D */
        printf("error: %s:%d unsupport inst STLE.D\n", __func__, __LINE__);
        goto end;
    default:
        printf("error: %s:%d unsupport inst 0x%x\n", __func__, __LINE__, inst);
        assert(0);
    }
end:
    UC_PC(uc) += 4;
#ifndef CONFIG_LOONGARCH_NEW_WORLD
ret:
#endif
    return 0;
}

static void *set_interpret_newpage(abi_ulong oldpage, int host_page_num)
{
    void *newpage;
    void *ret;
    int prot = 0;
    abi_long len = qemu_host_page_size * host_page_num;
    int i;
    abi_ulong oldpage_temp;

    /* make smc page writable and clear code cache */
    for (i = 0; i < host_page_num; i++) {
        bool flush = false;
        abi_ulong addr;
        oldpage_temp = oldpage + qemu_host_page_size * i;
        abi_ulong host_end = oldpage_temp + qemu_host_page_size;

        for (addr = oldpage_temp; addr < host_end; addr += TARGET_PAGE_SIZE) {
            int prot_tmp = page_get_flags(addr);
            if ((prot_tmp & PAGE_WRITE_ORG) && !(prot_tmp & PAGE_WRITE)) {
                flush = true;
            }
            prot |= prot_tmp;
        }

        if (flush) {
            mmap_lock();
            mprotect(g2h_untagged(oldpage_temp), qemu_host_page_size,
                    (prot & PAGE_BITS) | PAGE_WRITE);
            tb_invalidate_phys_page(oldpage_temp);
            mmap_unlock();

#ifdef CONFIG_LATX_AOT
            if (option_aot) {
                page_set_page_state(oldpage_temp, PAGE_SMC);
            }
#endif
        }
    }

    newpage = mmap(NULL, len, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANON, -1, 0);
    ret = mremap(g2h_untagged(oldpage), len, len,
                    MREMAP_MAYMOVE | MREMAP_FIXED, newpage);
    if (ret == MAP_FAILED) {
        qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s %d mremap failed\n",
                    __func__, __LINE__);
        return NULL;
    }

    return newpage;
}

static int recover_interpret_oldpage(abi_ulong oldpage, void * newpage,
                                    int page_num)
{
    void *ret;

    if (oldpage == (abi_ulong)(uintptr_t)newpage) {
        return 0;
    }

    abi_long len = qemu_host_page_size * page_num;
    ret = mremap(newpage, len, len, MREMAP_MAYMOVE | MREMAP_FIXED,
                g2h_untagged(oldpage));
    if (ret == MAP_FAILED) {
        qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s %d mremap failed\n",
                    __func__, __LINE__);
        return 1;
    }

    return 0;
}


static int interpret_cmpxchg8b(ucontext_t *uc, uint32_t* inst)
{
    uint32_t rd0, rd1, rd2, rj0;
    int64_t siaddr;
    int off;
    abi_ulong page_addr;
    uint64_t newaddr;
    void *newpage;
    int page_num;

    /*
     * cmpxchg8b:
     *   epc ->             0x00: ll.d
     *                      0x04: bne
     *                      0x08: sc.d
     *                      0x0c: beq
     *   bne not taken ->   0x10: calculate eflags
     *                      0x14: b
     *   bne taken ->       0x18: dbar
     */
    rj0 = (inst[0] >> 5) & 0x1f;
    off = (inst[0] & ~0x3ff) << 8 >> 16;
    rd0 = inst[0] & 0x1f;
    rd1 = inst[1] & 0x1f;
    rd2 = inst[2] & 0x1f;

    /* get siaddr and page */
    siaddr = UC_GR(uc)[rj0] + off;
    page_addr = siaddr & qemu_host_page_mask;
    int opnd0_size = (inst[-2] & 0xffc003ff) == 0x03400000 ?
                ((inst[-2] << 10)) >> 20 : 64;
    if (page_addr != ((siaddr + (opnd0_size >> 3)) & qemu_host_page_mask)) {
        page_num = 2;
    } else {
        page_num = 1;
    }

    newpage = set_interpret_newpage(page_addr, page_num);
    if (newpage == NULL) {
        return 1;
    }

    int64_t page_off = (int64_t)newpage - page_addr;
    newaddr = page_off + siaddr;

    if ((inst[-2] & 0xffc003ff) == 0x03400000) {
        /* andi 0, 0, opnd0_size */
        qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s opnd0_size %d\n",
                    __func__, opnd0_size);
        if (opnd0_size == 32) {
            UC_GR(uc)[rd0] = (int64_t)(*(int32_t *)newaddr);
        } else if (opnd0_size == 16) {
            UC_GR(uc)[rd0] = (int64_t)(*(int16_t *)newaddr);
        } else {
            assert(0);
        }
    } else {
        UC_GR(uc)[rd0] = *(int64_t *)newaddr;
    }

    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] mem_old 0x%llx reg 0x%llx\n",
                UC_GR(uc)[rd0], UC_GR(uc)[rd1]);

    if (UC_GR(uc)[rd1] != UC_GR(uc)[rd0]) {
        /* bne taken */
        off = (inst[1] & ~0x3ff) << 6 >> 14;
        UC_PC(uc) += off + 4;
        qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] taken epc 0x%llx\n", UC_PC(uc));
    } else {
        if ((inst[-2] & 0xffc003ff) == 0x03400000) {
            /* andi 0, 0, opnd0_size */
            int opnd0_size = (inst[-2] << 10) >> 20;
            qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s opnd0_size %d\n",
                        __func__, opnd0_size);
            if (opnd0_size == 32) {
                *(int32_t *)newaddr = UC_GR(uc)[rd2];
            } else if (opnd0_size == 16) {
                *(int16_t *)newaddr = UC_GR(uc)[rd2];
            } else {
                assert(0);
            }
        } else {
           *(int64_t *)newaddr = UC_GR(uc)[rd2];
        }
        UC_PC(uc) += 0x10;
        qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] not taken epc 0x%llx\n", UC_PC(uc));
    }

    return recover_interpret_oldpage(page_addr, newpage, page_num);
}

static int interpret_add(ucontext_t *uc, uint32_t* inst)
{
    uint32_t rj0, rd0, rk0;
    abi_ulong page_addr;
    uint64_t siaddr, newaddr;
    void *newpage;
    int page_num;

    rd0 = inst[0] & 0x1f;
    rj0 = (inst[0] >> 5) & 0x1f;
    rk0 = (inst[0] >> 10) & 0x1f;
    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s rd0 = %d rj0 = %d rk0 = %d\n",
                __func__, rd0, rj0, rk0);

    /* get siaddr and page */
    siaddr = UC_GR(uc)[rj0];
    page_addr = siaddr & qemu_host_page_mask;

    int opnd0_size = (inst[-2] & 0xffc003ff) == 0x03400000 ?
                ((inst[-2] << 10)) >> 20 : 64;
    if (page_addr != ((siaddr + (opnd0_size >> 3)) & qemu_host_page_mask)) {
        page_num = 2;
    } else {
        page_num = 1;
    }

    newpage = set_interpret_newpage(page_addr, page_num);
    if (newpage == NULL) {
        return 1;
    }

    int64_t page_off = (int64_t)newpage - page_addr;
    newaddr = page_off + siaddr;
    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s siaddr = %lx newaddr = %lx\n",
                __func__, siaddr, newaddr);

    /*
     * add.d
     */
    if ((inst[-2] & 0xffc003ff) == 0x03400000) {
        /* andi 0, 0, opnd0_size */
        int opnd0_size = (inst[-2] << 10) >> 20;
        qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s opnd0_size %d\n",
                    __func__, opnd0_size);
        if (opnd0_size == 32) {
            UC_GR(uc)[rd0] = (int64_t)(*(int32_t *)newaddr);
            *(int32_t *)newaddr = UC_GR(uc)[rd0] + UC_GR(uc)[rk0];
        } else if (opnd0_size == 16) {
            UC_GR(uc)[rd0] = (int64_t)(*(int16_t *)newaddr);
            *(int16_t *)newaddr = UC_GR(uc)[rd0] + UC_GR(uc)[rk0];
        } else {
            assert(0);
        }
    } else {
        UC_GR(uc)[rd0] = *(int64_t *)newaddr;
        *(int64_t *)newaddr = UC_GR(uc)[rd0] + UC_GR(uc)[rk0];
    }
    UC_PC(uc) += 4;
    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s UC_GR(uc)[rd0] = 0x%llx"
                " UC_GR(uc)[rj0] = 0x%llx UC_GR(uc)[rk0] = 0x%llx"
                " *new mem = 0x%lx\n", __func__, UC_GR(uc)[rd0],
                UC_GR(uc)[rj0], UC_GR(uc)[rk0], *(int64_t *)newaddr);

    return recover_interpret_oldpage(page_addr, newpage, page_num);
}

static int interpret_and(ucontext_t *uc, uint32_t* inst)
{
    uint32_t rj0, rd0, rk0;
    abi_ulong page_addr;
    uint64_t siaddr, newaddr;
    void *newpage;
    int page_num;

    rd0 = inst[0] & 0x1f;
    rj0 = (inst[0] >> 5) & 0x1f;
    rk0 = (inst[0] >> 10) & 0x1f;
    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s rd0 = %d rj0 = %d rk0 = %d\n",
                __func__, rd0, rj0, rk0);

    /* get siaddr and page */
    siaddr = UC_GR(uc)[rj0];
    page_addr = siaddr & qemu_host_page_mask;

    int opnd0_size = (inst[-1] & 0xffc003ff) == 0x03400000 ?
                ((inst[-1] << 10)) >> 20 : 64;
    if (page_addr != ((siaddr + (opnd0_size >> 3)) & qemu_host_page_mask)) {
        page_num = 2;
    } else {
        page_num = 1;
    }

    newpage = set_interpret_newpage(page_addr, page_num);
    if (newpage == NULL) {
        return 1;
    }

    int64_t page_off = (int64_t)newpage - page_addr;
    newaddr = page_off + siaddr;
    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s siaddr %lx newaddr %lx\n",
                __func__, siaddr, newaddr);

    /*
     * and
     */
    if ((inst[-1] & 0xffc003ff) == 0x03400000) {
        /* andi 0, 0, opnd0_size */
        int opnd0_size = (inst[-1] << 10) >> 20;
        qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s opnd0_size %d\n",
                    __func__, opnd0_size);
        if (opnd0_size == 32) {
            UC_GR(uc)[rd0] = (int64_t)(*(int32_t *)newaddr);
            *(int32_t *)newaddr = UC_GR(uc)[rd0] & UC_GR(uc)[rk0];
        } else if (opnd0_size == 16) {
            UC_GR(uc)[rd0] = (int64_t)(*(int16_t *)newaddr);
            *(int16_t *)newaddr = UC_GR(uc)[rd0] & UC_GR(uc)[rk0];
        } else {
            assert(0);
        }
    } else {
        UC_GR(uc)[rd0] = *(int64_t *)newaddr;
        *(int64_t *)newaddr = UC_GR(uc)[rd0] & UC_GR(uc)[rk0];
    }
    UC_PC(uc) += 4;
    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s UC_GR(uc)[rd0] = 0x%llx"
                " UC_GR(uc)[rj0] = 0x%llx UC_GR(uc)[rk0] = 0x%llx"
                " *new mem = 0x%lx\n", __func__, UC_GR(uc)[rd0],
                UC_GR(uc)[rj0], UC_GR(uc)[rk0], *(int64_t *)newaddr);

    return recover_interpret_oldpage(page_addr, newpage, page_num);
}

static int interpret_or(ucontext_t *uc, uint32_t* inst)
{
    uint32_t rj0, rd0, rk0;
    abi_ulong page_addr;
    uint64_t siaddr, newaddr;
    void *newpage;
    int page_num;

    rd0 = inst[0] & 0x1f;
    rj0 = (inst[0] >> 5) & 0x1f;
    rk0 = (inst[0] >> 10) & 0x1f;
    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s rd0 = %d rj0 = %d rk0 = %d\n",
                __func__, rd0, rj0, rk0);

    /* get siaddr and page */
    siaddr = UC_GR(uc)[rj0];
    page_addr = siaddr & qemu_host_page_mask;

    int opnd0_size = (inst[-1] & 0xffc003ff) == 0x03400000 ?
                ((inst[-1] << 10)) >> 20 : 64;
    if (page_addr != ((siaddr + (opnd0_size >> 3)) & qemu_host_page_mask)) {
        page_num = 2;
    } else {
        page_num = 1;
    }

    newpage = set_interpret_newpage(page_addr, page_num);
    if (newpage == NULL) {
        return 1;
    }

    int64_t page_off = (int64_t)newpage - page_addr;
    newaddr = page_off + siaddr;
    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s siaddr %lx newaddr %lx\n",
                __func__, siaddr, newaddr);

    /*
     * or
     */
    if ((inst[-1] & 0xffc003ff) == 0x03400000) {
        /* andi 0, 0, opnd0_size */
        int opnd0_size = (inst[-1] << 10) >> 20;
        qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s opnd0_size %d\n",
                    __func__, opnd0_size);
        if (opnd0_size == 32) {
            UC_GR(uc)[rd0] = (int64_t)(*(int32_t *)newaddr);
            *(int32_t *)newaddr = UC_GR(uc)[rd0] | UC_GR(uc)[rk0];
        } else if (opnd0_size == 16) {
            UC_GR(uc)[rd0] = (int64_t)(*(int16_t *)newaddr);
            *(int16_t *)newaddr = UC_GR(uc)[rd0] | UC_GR(uc)[rk0];
        } else {
            assert(0);
        }
    } else {
        UC_GR(uc)[rd0] = *(int64_t *)newaddr;
        *(int64_t *)newaddr = UC_GR(uc)[rd0] | UC_GR(uc)[rk0];
    }
    UC_PC(uc) += 4;
    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s UC_GR(uc)[rd0] = 0x%llx"
                " UC_GR(uc)[rj0] = 0x%llx UC_GR(uc)[rk0] = 0x%llx"
                " *new mem = 0x%lx\n", __func__, UC_GR(uc)[rd0],
                UC_GR(uc)[rj0], UC_GR(uc)[rk0], *(int64_t *)newaddr);

    return recover_interpret_oldpage(page_addr, newpage, page_num);
}

static int interpret_xor(ucontext_t *uc, uint32_t* inst)
{
    uint32_t rj0, rd0, rk0;
    abi_ulong page_addr;
    uint64_t siaddr, newaddr;
    void *newpage;
    int page_num;

    rd0 = inst[0] & 0x1f;
    rj0 = (inst[0] >> 5) & 0x1f;
    rk0 = (inst[0] >> 10) & 0x1f;
    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s rd0 = %d rj0 = %d rk0 = %d\n",
                __func__, rd0, rj0, rk0);

    /* get siaddr and page */
    siaddr = UC_GR(uc)[rj0];
    page_addr = siaddr & qemu_host_page_mask;

    int opnd0_size = (inst[-2] & 0xffc003ff) == 0x03400000 ?
                ((inst[-2] << 10)) >> 20 : 64;
    if (page_addr != ((siaddr + (opnd0_size >> 3)) & qemu_host_page_mask)) {
        page_num = 2;
    } else {
        page_num = 1;
    }

    newpage = set_interpret_newpage(page_addr, page_num);
    if (newpage == NULL) {
        return 1;
    }

    int64_t page_off = (int64_t)newpage - page_addr;
    newaddr = page_off + siaddr;
    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s siaddr %lx newaddr %lx\n",
                __func__, siaddr, newaddr);

    /*
     * or
     */
    if ((inst[-2] & 0xffc003ff) == 0x03400000) {
        /* andi 0, 0, opnd0_size */
        int opnd0_size = (inst[-2] << 10) >> 20;
        qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s opnd0_size %d\n",
                    __func__, opnd0_size);
        if (opnd0_size == 32) {
            UC_GR(uc)[rd0] = (int64_t)(*(int32_t *)newaddr);
            *(int32_t *)newaddr = UC_GR(uc)[rd0] ^ UC_GR(uc)[rk0];
        } else if (opnd0_size == 16) {
            UC_GR(uc)[rd0] = (int64_t)(*(int16_t *)newaddr);
            *(int16_t *)newaddr = UC_GR(uc)[rd0] ^ UC_GR(uc)[rk0];
        } else {
            assert(0);
        }
    } else {
        UC_GR(uc)[rd0] = *(int64_t *)newaddr;
        *(int64_t *)newaddr = UC_GR(uc)[rd0] ^ UC_GR(uc)[rk0];
    }
    UC_PC(uc) += 4;
    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s UC_GR(uc)[rd0] = 0x%llx"
                " UC_GR(uc)[rj0] = 0x%llx UC_GR(uc)[rk0] = 0x%llx"
                " *new mem = 0x%lx\n", __func__, UC_GR(uc)[rd0],
                UC_GR(uc)[rj0], UC_GR(uc)[rk0], *(int64_t *)newaddr);

    return recover_interpret_oldpage(page_addr, newpage, page_num);
}

static int interpret_sub(ucontext_t *uc, uint32_t* inst)
{
    uint32_t rj0, rd0, rd1, rj1, rk1;
    abi_ulong page_addr;
    uint64_t siaddr, newaddr;
    void *newpage;
    int page_num;

    rd0 = inst[0] & 0x1f;
    rj0 = (inst[0] >> 5) & 0x1f;
    rd1 = inst[1] & 0x1f;
    rj1 = (inst[1] >> 5) & 0x1f;
    rk1 = (inst[1] >> 10) & 0x1f;
    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s"
                " rd0 = %d rj0 = %d rd1 = %d rj1 = %d rk1 = %d\n",
                __func__, rd0, rj0, rd1, rj1, rk1);

    /* get siaddr and page */
    siaddr = UC_GR(uc)[rj0];
    page_addr = siaddr & qemu_host_page_mask;

    int opnd0_size = (inst[-1] & 0xffc003ff) == 0x03400000 ?
                ((inst[-1] << 10)) >> 20 : 64;
    if (page_addr != ((siaddr + (opnd0_size >> 3)) & qemu_host_page_mask)) {
        page_num = 2;
    } else {
        page_num = 1;
    }

    newpage = set_interpret_newpage(page_addr, page_num);
    if (newpage == NULL) {
        return 1;
    }

    int64_t page_off = (int64_t)newpage - page_addr;
    newaddr = page_off + siaddr;
    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s siaddr %lx newaddr %lx\n",
                __func__, siaddr, newaddr);

    /*
     * sub.d
     */
    if ((inst[-1] & 0xffc003ff) == 0x03400000) {
        /* andi 0, 0, opnd0_size */
        int opnd0_size = (inst[-1] << 10) >> 20;
        if (opnd0_size == 32) {
            UC_GR(uc)[rd0] = (int64_t)(*(int32_t *)newaddr);
            UC_GR(uc)[rd1] = UC_GR(uc)[rj1] - UC_GR(uc)[rk1];
            *(int32_t *)newaddr = UC_GR(uc)[rd1];
        } else if (opnd0_size == 16) {
            UC_GR(uc)[rd0] = (int64_t)(*(int16_t *)newaddr);
            UC_GR(uc)[rd1] = UC_GR(uc)[rj1] - UC_GR(uc)[rk1];
            *(int16_t *)newaddr = UC_GR(uc)[rd1];
        } else {
            qemu_log_mask(LAT_LOG_MEM, "[latx_lock] %s error opnd0_size %d\n",
                        __func__, opnd0_size);
        }
    } else {
        UC_GR(uc)[rd0] = *(int64_t *)newaddr;
        UC_GR(uc)[rd1] = UC_GR(uc)[rj1] - UC_GR(uc)[rk1];
        *(int64_t *)newaddr = UC_GR(uc)[rd1];
    }
    UC_PC(uc) += 16;
    qemu_log_mask(LAT_LOG_MEM, "[latx_lock] %s UC_GR(uc)[rd0]= 0x%llx\n",
                __func__, UC_GR(uc)[rd0]);
    qemu_log_mask(LAT_LOG_MEM, "[latx_lock] %s UC_GR(uc)[rd1] = 0x%llx"
                " UC_GR(uc)[rj1] = 0x%llx UC_GR(uc)[rk1] = 0x%llx\n",
                __func__, UC_GR(uc)[rd1], UC_GR(uc)[rj1], UC_GR(uc)[rk1]);


    return recover_interpret_oldpage(page_addr, newpage, page_num);
}

static int interpret_xchg(ucontext_t *uc, uint32_t* inst)
{
    uint32_t rj0, rd0, rk0;
    abi_ulong page_addr;
    uint64_t siaddr, newaddr;
    void *newpage;
    int page_num;

    rd0 = inst[0] & 0x1f;
    rj0 = (inst[0] >> 5) & 0x1f;
    rk0 = (inst[0] >> 10) & 0x1f;
    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s rd0 = %d rj0 = %d rk0 = %d\n",
                __func__, rd0, rj0, rk0);

    /* get siaddr and page */
    siaddr = UC_GR(uc)[rj0];
    page_addr = siaddr & qemu_host_page_mask;

    int opnd0_size = (inst[-1] & 0xffc003ff) == 0x03400000 ?
                ((inst[-1] << 10)) >> 20 : 64;
    if (page_addr != ((siaddr + (opnd0_size >> 3)) & qemu_host_page_mask)) {
        page_num = 2;
    } else {
        page_num = 1;
    }

    newpage = set_interpret_newpage(page_addr, page_num);
    if (newpage == NULL) {
        return 1;
    }

    int64_t page_off = (int64_t)newpage - page_addr;
    newaddr = page_off + siaddr;
    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s siaddr = %lx newaddr = %lx\n",
                __func__, siaddr, newaddr);

    /*
     * amswap.d
     */
    if ((inst[-1] & 0xffc003ff) == 0x03400000) {
        /* andi 0, 0, opnd0_size */
        int opnd0_size = (inst[-1] << 10) >> 20;
        qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s opnd0_size %d\n",
                    __func__, opnd0_size);
        if (opnd0_size == 32) {
            UC_GR(uc)[rd0] = (int64_t)(*(int32_t *)newaddr);
            *(int32_t *)newaddr = UC_GR(uc)[rk0];
        } else if (opnd0_size == 16) {
            UC_GR(uc)[rd0] = (int64_t)(*(int16_t *)newaddr);
            *(int16_t *)newaddr = UC_GR(uc)[rk0];
        } else {
            assert(0);
        }
    } else {
        UC_GR(uc)[rd0] = *(int64_t *)newaddr;
        *(int64_t *)newaddr = UC_GR(uc)[rk0];
    }
    UC_PC(uc) += 4;
    qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s UC_GR(uc)[rd0] = 0x%llx"
                " UC_GR(uc)[rj0] = 0x%llx UC_GR(uc)[rk0] = 0x%llx"
                " *new mem = 0x%lx\n", __func__, UC_GR(uc)[rd0],
                UC_GR(uc)[rj0], UC_GR(uc)[rk0], *(int64_t *)newaddr);

    return recover_interpret_oldpage(page_addr, newpage, page_num);
}

#ifdef CONFIG_LATX
static int interpret_cmpxchg16b(ucontext_t *uc, uint32_t* inst)
{
    uint32_t rj, rd;
    abi_ulong page_addr;
    uint64_t siaddr, newaddr, mem_old_hi, mem_old_lo;
    void *newpage;
    int page_num;

    /*
     * cmpxchg16b:
     *   epc ->             0x00: ll.d
     *                      0x04: amswap.d
     */
    rd = inst[1] & 0x1f;
    rj = (inst[1] >> 5) & 0x1f;

    /* get siaddr and page */
    siaddr = UC_GR(uc)[rj];
    page_addr = siaddr & qemu_host_page_mask;

    if (page_addr != ((siaddr + 16) & qemu_host_page_mask)) {
        page_num = 2;
    } else {
        page_num = 1;
    }

    newpage = set_interpret_newpage(page_addr, page_num);
    if (newpage == NULL) {
        return 1;
    }

    int64_t page_off = (int64_t)newpage - page_addr;
    newaddr = page_off + siaddr;

    mem_old_lo = *(uint64_t *)newaddr;
    mem_old_hi = *(uint64_t *)(newaddr + 8);

    if (mem_old_hi == UC_GR(uc)[reg_gpr_map[edx_index]] &&
        mem_old_lo == UC_GR(uc)[reg_gpr_map[eax_index]]) {
        *(uint64_t *)newaddr = UC_GR(uc)[reg_gpr_map[ebx_index]];
        *(uint64_t *)(newaddr + 8) = UC_GR(uc)[reg_gpr_map[ecx_index]];
        UC_GR(uc)[rd] = 0;
    } else {
        UC_GR(uc)[reg_gpr_map[edx_index]] = mem_old_hi;
        UC_GR(uc)[reg_gpr_map[eax_index]] = mem_old_lo;
        UC_GR(uc)[rd] = 1;
    }
    UC_PC(uc) += 8;

    return recover_interpret_oldpage(page_addr, newpage, page_num);
}
#else
static int interpret_cmpxchg16b(ucontext_t *uc, uint32_t* inst)
{
    fprintf(stderr, "%s error\n", __func__);
    abort();
}
#endif

int lock_interpret(siginfo_t *info, ucontext_t *uc)
{
    if (page_get_target_data((uint64_t)info->si_addr)) {
        /* TODO */
        qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] shadow page TODO\n");
    } else {
        uint32_t* inst = (uint32_t *)UC_PC(uc);

        qemu_log_mask(LAT_LOG_MEM, "[LATX_LOCK] %s"
                " inst[0] = 0x%x inst[1] = 0x%x inst[2] = 0x%x\n",
                __func__, inst[0], inst[1], inst[2]);
        /* ll.d */
        if ((inst[0] >> 24) == 0x22) {
            /* sc.d */
            if ((inst[2] >> 24) == 0x23) {
                if ((inst[1] >> 26) == 0x17) {
                    /*
                     * cmpxchg and cmpxchg8b:
                     * bne
                     */
                    return interpret_cmpxchg8b(uc, inst);
                } else if ((inst[1] >> 15) == 0x23) {
                    /*
                     * sub.d:
                     *  lock neg
                     */
                    return interpret_sub(uc, inst);
                }
            } else if (inst[0] == 0x22000400 &&
                        (inst[1] & 0xfffffc00) == 0x38608000) {
                /*
                 * ll.d zero, zero, 4
                 * amswap.d itemp, zero, mem_opnd
                 */
                return interpret_cmpxchg16b(uc, inst);
            }
        }

        /* AM* */
        switch (inst[0] >> 15) {
        case 0x70c1:
        case 0x70d3:
            /*
             * amswap.d/amswap_db.d:
             *  xchg
             */
            return interpret_xchg(uc, inst);
        case 0x70c3:
        case 0x70d5:
            /*
             * amadd.d:
             *  lock add
             *  lock adc
             *  lock inc
             *  lock dec
             *  lock sub
             *  lock sbb
             *  lock xadd
             */
            return interpret_add(uc, inst);
        case 0x70c5:
        case 0x70d7:
            /*
             * lock and
             */
            return interpret_and(uc, inst);
        case 0x70c7:
        case 0x70d9:
            /*
             * lock or
             */
            return interpret_or(uc, inst);
        case 0x70c9:
        case 0x70db:
            /*
             * amxor.d
             *  lock xor
             *  lock not
             */
            return interpret_xor(uc, inst);
        default:
            break;
        }
    }

    return 1;
}
#elif defined(__mips__)
int shared_private_interpret(siginfo_t *info, ucontext_t *uc)
{
    uint32_t inst, rt, ft, wd;
    int64_t mem_addr, value, siaddr;

    siaddr = (int64_t)info->si_addr;
    if ((page_get_flags(h2g(siaddr)) & PAGE_BITS) == PROT_NONE) {
#ifdef CONFIG_SHDWRT_DBGMSG
        qemu_log_mask(LAT_LOG_MEM, "[LATX_16K] %s inst 0x%x epc 0x%llx "
                "siaddr 0x%lx PROT_NONE return 1\n", __func__,
                *(uint32_t *)uc->uc_mcontext.pc, uc->uc_mcontext.pc, siaddr);
#endif
        return 1;
    }

    ShadowPageDesc *shadow_pd = page_get_target_data(h2g(siaddr));
    assert(shadow_pd != NULL);

    /* extract inst info */
    inst = *(uint32_t *)uc->uc_mcontext.pc;
    rt = (inst >> 16) & 0x1f;
    ft = (inst >> 16) & 0x1f;
    wd = (inst >> 6) & 0x1f;

    /* calculate the address */
    mem_addr = h2g(siaddr) + shadow_pd->access_off;

#ifdef CONFIG_SHDWRT_DBGMSG
    qemu_log_mask(LAT_LOG_MEM, "[LATX_16K] %s inst 0x%x epc 0x%llx "
            "siaddr 0x%lx h2g(siaddr) 0x%x shadow page 0x%lx\n",
            __func__, inst, uc->uc_mcontext.pc, siaddr,
            h2g(siaddr), mem_addr);
#endif
    switch (inst >> 26)
    {
	case 0x20: // LB
            value = *(char *)mem_addr;
            value = value << 56 >> 56;
            uc->uc_mcontext.gregs[rt] = value;
            break;
	case 0x21: // LH
            value = *(short *)mem_addr;
            value = value << 48 >> 48;
            uc->uc_mcontext.gregs[rt] = value;
            break;
	case 0x23: // LW
            value = *(int *)mem_addr;
            value = value << 32 >> 32;
            uc->uc_mcontext.gregs[rt] = value;
            break;
	case 0x37: // LD
            value = *(int64_t *)mem_addr;
            uc->uc_mcontext.gregs[rt] = value;
            break;
        case 0x28: // SB
            value = uc->uc_mcontext.gregs[rt];
            *(char *)mem_addr = (char)value;
            break;
        case 0x29: // SH
            value = uc->uc_mcontext.gregs[rt];
            *(short *)mem_addr = (short)value;
            break;
        case 0x2b: // SW
            value = uc->uc_mcontext.gregs[rt];
            *(int *)mem_addr = (int)value;
            break;
        case 0x3f: // SD
            value = uc->uc_mcontext.gregs[rt];
            *(int64_t *)mem_addr = (int64_t)value;
            break;
	case 0x24: // LBU
            value = *(char *)mem_addr;
            value = (uint64_t)value << 56 >> 56;
            uc->uc_mcontext.gregs[rt] = value;
            break;
	case 0x25: // LHU
            value = *(short *)mem_addr;
            value = (uint64_t)value << 48 >> 48;
            uc->uc_mcontext.gregs[rt] = value;
            break;
	case 0x27: // LWU
            value = *(int *)mem_addr;
            value = (uint64_t)value << 32 >> 32;
            uc->uc_mcontext.gregs[rt] = value;
            break;
	case 0x31: // LWC1
	    uc->uc_mcontext.fpregs.fp_r.fp_fregs[ft]._fp_fregs = *(int32_t *)mem_addr;
	    break;
	case 0x35: // LDC1
	    uc->uc_mcontext.fpregs.fp_r.fp_dregs[ft] = *(int64_t *)mem_addr;
	    break;
	case 0x39: // SWC1
	    *(int32_t *)mem_addr = uc->uc_mcontext.fpregs.fp_r.fp_fregs[ft]._fp_fregs;
	    break;
	case 0x3d: // SDC1
	    *(int64_t *)mem_addr = uc->uc_mcontext.fpregs.fp_r.fp_dregs[ft];
	    break;
    case 0x1e:
        /*
         * 656 : offset of (ucontext_t, uc_extcontext)
         * uc_extcontext : msa_extcontext
         * 0x784d5341 : magic word of msa_extcontext
         * sizeof(ucontext_t) has difference with kernel due to sizeof(sigset_t)
         */
        assert(*((int*)((void*)uc + 656)) == 0x784d5341);
        if ((inst & 0x3f) == 0x20) { //ldb
            uc->uc_mcontext.fpregs.fp_r.fp_dregs[wd] = *(int64_t *)mem_addr;
            *((int64_t*)((void*)uc + 656 + 8 + (wd << 3))) = *((int64_t *)mem_addr + 1);
        } else if ((inst & 0x3f) == 0x24){  //stb
            *(int64_t *)mem_addr = uc->uc_mcontext.fpregs.fp_r.fp_dregs[wd];
            *((int64_t *)mem_addr + 1) = *((int64_t*)((void*)uc + 656 + 8 + (wd << 3))) ;
        } else {
            printf("error: %s:%d unsupport inst 0x%x\n", __func__, __LINE__, inst);
            assert(0);
        }
        break;
    default:
        printf("error: %s:%d unsupport inst 0x%x\n", __func__, __LINE__, inst);
        assert(0);
    }
    uc->uc_mcontext.pc += 4;
    return 0;
}
#else
# error "Not support yet!"
#endif

#if defined(CONFIG_LATX_KZT)
int elf_data_interpret(siginfo_t *info, ucontext_t *uc)
{
    uint32_t inst, rd, fd;
    int64_t value, mem_addr;
    uintptr_t fix_addr = (uintptr_t)info->si_addr;
    //must be without PAGE_WRITE
    lsassertm(!(page_get_flags(fix_addr) & PAGE_WRITE),"p->flags=0x%x\n", page_get_flags(fix_addr));
    if(relocation_log) printf_log(LOG_INFO, "elf_data_interpret fix addr 0x%lx\n", fix_addr);
    //set page writeable.
    mprotect((void*)(fix_addr & qemu_host_page_mask), qemu_host_page_size,
        (page_get_flags(fix_addr) | PAGE_WRITE) & PAGE_BITS);
    //fix mem
    inst = *(uint32_t *)UC_PC(uc);
    rd = inst & 0x1f;
    mem_addr = fix_addr;
#ifndef CONFIG_LOONGARCH_NEW_WORLD
        /*
         * In old world:
         * WARNING: the offset of __fregs in uc->uc_mcontext is incorrent,
         * we fix the issue by adding a extra offset, the solution is only
         * temporary!
         */
        fd = rd + 1;
#else
        fd = rd;
        struct extctx_layout extctx;
        memset(&extctx, 0, sizeof(extctx));
        /* we need to parse the extcontext data */
        parse_extcontext(uc, &extctx);
#endif

    switch (inst >> 22) {
    case 0xa4: /* ST.B */
        value = UC_GR(uc)[rd];
        *(char *)mem_addr = (char)value;
        goto suc;
    case 0xa5: /* ST.H */
        value = UC_GR(uc)[rd];
        *(short *)mem_addr = (short)value;
        goto suc;
    case 0xa6: /* ST.W */
        value = UC_GR(uc)[rd];
        *(int *)mem_addr = (int)value;
        goto suc;
    case 0xa7: /* ST.D */
        value = UC_GR(uc)[rd];
        *(int64_t *)mem_addr = value;
        goto suc;
#ifndef CONFIG_LOONGARCH_NEW_WORLD
    case 0xad: /* FST.S */
        *(int32_t *)mem_addr = UC_FREG(uc)[fd].__val32[0];
        goto suc;
    case 0xaf: /* FST.D */
        *(int64_t *)mem_addr = UC_FREG(uc)[fd].__val64[0];
        goto suc;
    case 0xb1: /* VST */
        *(int64_t *)mem_addr = UC_FREG(uc)[fd].__val64[0];
        *(int64_t *)(mem_addr + 8) = UC_FREG(uc)[fd].__val64[1];
        goto suc;
    case 0xb3: /* XVST */
        *(int64_t *)mem_addr = UC_FREG(uc)[fd].__val64[0];
        *(int64_t *)(mem_addr + 8) = UC_FREG(uc)[fd].__val64[1];
        *(int64_t *)(mem_addr + 16) = UC_FREG(uc)[fd].__val64[2];
        *(int64_t *)(mem_addr + 24) = UC_FREG(uc)[fd].__val64[3];
        goto suc;
#else /* CONFIG_LOONGARCH_NEW_WORLD */
    case 0xad: /* FST.S */
        *(int32_t *)mem_addr = UC_GET_FPR(&extctx, fd, int32_t);
        goto suc;
    case 0xaf: /* FST.D */
        *(int64_t *)mem_addr = UC_GET_FPR(&extctx, fd, int64_t);
        goto suc;
    case 0xb1: /* VST */
        *(int64_t *)(mem_addr + 0) = UC_GET_LSX(&extctx, fd, 0, int64_t);
        *(int64_t *)(mem_addr + 8) = UC_GET_LSX(&extctx, fd, 1, int64_t);
        goto suc;
    case 0xb3: /* XVST */
        *(int64_t *)(mem_addr +  0) = UC_GET_LASX(&extctx, fd, 0, int64_t);
        *(int64_t *)(mem_addr +  8) = UC_GET_LASX(&extctx, fd, 1, int64_t);
        *(int64_t *)(mem_addr + 16) = UC_GET_LASX(&extctx, fd, 2, int64_t);
        *(int64_t *)(mem_addr + 24) = UC_GET_LASX(&extctx, fd, 3, int64_t);
        goto suc;
#endif
    default:
        switch (inst >> 24) {
        case 0x25: /* STPTR.W */
           value = UC_GR(uc)[rd];
           *(int *)mem_addr = (int)value;
           goto suc;
        case 0x27: /* STPTR.D */
           value = UC_GR(uc)[rd];
           *(int64_t *)mem_addr = value;
           goto suc;
        }
        switch (inst >> 18) {
        case 0xe04: /* STX.B */
            value = UC_GR(uc)[rd];
            *(char *)mem_addr = (char)value;
            goto suc;
        case 0xe05: /* STX.H */
            value = UC_GR(uc)[rd];
            *(short *)mem_addr = (short)value;
            goto suc;
        case 0xe06: /* STX.W */
            value = UC_GR(uc)[rd];
            *(int *)mem_addr = (int)value;
            goto suc;
        case 0xe07: /* STX.D */
            value = UC_GR(uc)[rd];
            *(int64_t *)mem_addr = value;
            goto suc;
        }
        lsassert(0);
        return -1;
    }
    return -1;
suc:
    UC_PC(uc) += 4;
    //set page back to old flags.
    mprotect((void*)(fix_addr & qemu_host_page_mask), qemu_host_page_size,
        page_get_flags(fix_addr) & PAGE_BITS);
    return 0;
}
#endif

int hostpage_exist_shadow_page(uint64_t host_addr)
{
    uint64_t addr, real_start, real_end;
    int ret, i;

    real_start = host_addr & qemu_host_page_mask;
    real_end = real_start + qemu_host_page_size;
    ret = 0;

    if (!spm_exist(real_start, qemu_host_page_size))
        return 0;

    for (addr = real_start, i = 0; addr < real_end; addr += TARGET_PAGE_SIZE, i++) {
        if (page_get_target_data(addr)) {
            qemu_log_mask(LAT_LOG_MEM, "[LATX_16K] %s %lx target_data %p\n",
                    __func__, host_addr, page_get_target_data(addr));
            ret = ret | (1U << i);
        }
    }
    return ret;
}

int mprotect_one_shadow_page(abi_ulong addr, int prot)
{
    int ret = 0;

    /* fast path */
    if ((page_get_flags(addr) & PAGE_BITS) == prot) {
        return ret;
    }

    ShadowPageDesc *shadow_pd = page_get_target_data(addr);
    assert(shadow_pd != NULL);
    ret = mprotect(shadow_pd->p_addr, qemu_host_page_size, prot);
    qemu_log_mask(LAT_LOG_MEM, "[LATX_16K] %s addr 0x"
            TARGET_FMT_lx " shadow_p %p prot 0x%x return %d\n",
            __func__, addr, shadow_pd->p_addr, prot, ret);
    return ret;
}


int mprotect_shadow_page_range_if_exist(abi_ulong start, abi_ulong end, int prot)
{
    int shadow_page_count;

    uint64_t addr, real_start;
    uint64_t host_mprotect_start, host_mprotect_end;
    int i;

    real_start = start & TARGET_PAGE_MASK;
    int ret = 0;
    assert(real_start <= (end & TARGET_PAGE_MASK));

    //fast path
    if (!spm_exist(start, end - start)) {
        /* no shadow page in the host page, mprotect directly. */
        ret = mprotect(g2h_untagged(start), end - start, prot);
        if (ret != 0) {
            qemu_log_mask(LAT_LOG_MEM, "[LATX_16K] %s host mprotect addr "
                    TARGET_FMT_lx " size " TARGET_FMT_lx " prot 0x%x failed\n",
                    __func__, start, end - start, prot);
            return -1;
        }
        return 0;
    }

    host_mprotect_start = real_start;
    host_mprotect_end = real_start;

    for (addr = start; addr < end; addr += qemu_host_page_size) {
        shadow_page_count = 0;
        for (i = 0; i < qemu_host_page_size; i += TARGET_PAGE_SIZE) {

            ShadowPageDesc *shadow_pd = page_get_target_data(addr + i);
            if (shadow_pd) {
                shadow_page_count ++;
                /* fast path */
                if ((page_get_flags(addr + i) & PAGE_BITS) == prot) {
                    continue;
                }
                ret = mprotect(shadow_pd->p_addr, qemu_host_page_size, prot);
                if (ret != 0) {
                    qemu_log_mask(LAT_LOG_MEM, "[LATX_16K] %s %lx shadow_p %p prot 0x%x mprotect failed\n",
                            __func__, addr + i, shadow_pd->p_addr, prot);
                    return -1;
                }
                qemu_log_mask(LAT_LOG_MEM, "[LATX_16K] %s %lx shadow_p %p prot 0x%x\n",
                        __func__, addr + i, shadow_pd->p_addr, prot);
            }
        }
        if (shadow_page_count == 0) {
            host_mprotect_end = addr + qemu_host_page_size;
        } else {
            /* no shadow page in the host page range, mprotect directly. */
            if (host_mprotect_end != host_mprotect_start) {
                ret = mprotect(g2h_untagged(host_mprotect_start), host_mprotect_end - host_mprotect_start, prot);
                if (ret != 0) {
                    qemu_log_mask(LAT_LOG_MEM, "[LATX_16K] %s host mprotect addr %lx size%lx prot 0x%x failed\n",
                            __func__, host_mprotect_start, host_mprotect_end - host_mprotect_start, prot);
                    return -1;
                }
            }
            host_mprotect_start = host_mprotect_end = addr + qemu_host_page_size;
        }
    }

    /* no shadow page in the host page range, mprotect directly. */
    if (host_mprotect_end != host_mprotect_start) {
        ret = mprotect(g2h_untagged(host_mprotect_start), host_mprotect_end - host_mprotect_start, prot);
        if (ret != 0) {
            qemu_log_mask(LAT_LOG_MEM, "[LATX_16K] %s host mprotect addr %lx size%lx prot 0x%x failed\n",
                    __func__, host_mprotect_start, host_mprotect_end - host_mprotect_start, prot);
            return -1;
        }
    }

    return 0;
}

/*
 * shadow_page_mprotect:
 *      return zero means "hit shadow pages"
 *      return negative value means error
 */
int shadow_page_mprotect(abi_ulong *old_start, abi_ulong *old_end, int prot)
{
    abi_ulong start, end, host_start, host_end, addr;
    int flags;
    int outside_prot;
    int ret = 0;
    bool handle_prot_none;

    start = *old_start;
    end = *old_end;
    host_start = start & qemu_host_page_mask;
    host_end = HOST_PAGE_ALIGN(end);
    flags = MAP_PRIVATE | MAP_ANONYMOUS;
    handle_prot_none = false;

    /* handle the start */
    if (start > host_start) {
        /* tmp_end = min(end, real_start + qemu_host_page_size) */
        abi_ulong tmp_end = end < host_start + qemu_host_page_size ?
                            end : host_start + qemu_host_page_size;
        if (hostpage_exist_shadow_page(start)) {
            for (addr = start; addr < tmp_end; addr += TARGET_PAGE_SIZE) {
                ret = mprotect_one_shadow_page(addr, prot);
                if (ret != 0) {
                    return ret;
                }
            }
            *old_start = host_start + qemu_host_page_size;
        } else if (handle_prot_none) {
            /*
             * shadow pages doesn't exist, it means that all target
             * pages is private.
             */
            outside_prot = 0;

            /* checking if outside pages are PROT_NONE. */
            for (addr = host_start; addr < host_start + qemu_host_page_size;
                 addr += TARGET_PAGE_SIZE) {
                if (addr < start || addr >= tmp_end) {
                    outside_prot |= page_get_flags(addr);
                }
            }

            if (prot == PROT_NONE) {
                /*
                 * just do nothing if outside pages are PROT_NONE.
                 */
                if (outside_prot & PAGE_BITS) {
                    create_shadow_page_chunk(start, tmp_end, prot, flags, -1, 0);
                    *old_start = host_start + qemu_host_page_size;
                }
            }
        }

        if (host_end == host_start + qemu_host_page_size) {
            /* one single host page */
            if (*old_start != start) {
                *old_end = *old_start;
            }
            return ret;
        }

        host_start += qemu_host_page_size;
    }
    /* handle the end */
    if (end < host_end) {
        abi_ulong tmp_start = host_end - qemu_host_page_size;
        outside_prot = 0;
        if (hostpage_exist_shadow_page(end)) {
            for (addr = tmp_start; addr < end; addr += TARGET_PAGE_SIZE) {
                ret = mprotect_one_shadow_page(addr, prot);
                if (ret != 0) {
                    return ret;
                }
            }
            *old_end = host_end - qemu_host_page_size;
        } else if (handle_prot_none) {
            /*
             * shadow pages doesn't exist, it means that all target
             * pages is private.
             */
            outside_prot = 0;

            /* checking if outside pages are PROT_NONE. */
            for (addr = end; addr < host_end; addr += TARGET_PAGE_SIZE) {
                outside_prot |= page_get_flags(addr);
            }

            if (prot == PROT_NONE) {
                /*
                 * just do nothing if outside pages are PROT_NONE.
                 */
                if (outside_prot & PAGE_BITS) {
                    create_shadow_page_chunk(tmp_start, end, prot, flags, -1, 0);
                    *old_end = host_end - qemu_host_page_size;
                }
            }
        }

        host_end -= qemu_host_page_size;
    }

#ifdef CONFIG_LATX_16K_DEBUG
    /* we assume that there are no shadow pages in the middle */
    if (host_start < host_end) {
        for (addr = host_start; addr < host_end; addr += qemu_host_page_size) {
            if (hostpage_exist_shadow_page(host_start)) {
                qemu_log_mask(LAT_LOG_MEM,
                    "[LATX_16K] %s assert!\n" __func__);
                assert(0);
            }
        }
    }
#endif

    return ret;
}

void shadow_page_munmap(abi_ulong start, abi_ulong end)
{
    abi_ulong addr;
    int ret __attribute__((unused));

    for (addr = start; addr < end; addr += TARGET_PAGE_SIZE) {
        ShadowPageDesc *shadow_pd = page_get_target_data(addr);
        if (shadow_pd) {
            ret = munmap(shadow_pd->p_addr, qemu_host_page_size);
            assert(ret == 0);
            spm_clear(addr, TARGET_PAGE_SIZE);
            qemu_log_mask(LAT_LOG_MEM, "[LATX_16K] %s addr 0x"
                    TARGET_FMT_lx " shadow_p %p\n",
                    __func__, addr, shadow_pd->p_addr);

        }
    }
}

void create_shadow_page_chunk(abi_ulong start, abi_ulong end,
                              int inside_prot, int inside_flags,
                              int fd, abi_ulong offset)
{
    abi_ulong real_start, real_end, addr, len;
    void *outside_p, *inside_p;
    void *host_start;
    int t_prot;
    int64_t access_off;

    real_start = start & qemu_host_page_mask;
    host_start = g2h_untagged(real_start);
    real_end = HOST_PAGE_ALIGN(end);
    len = real_end - real_start;

    /*
     * We need remove the write permission before memcpy,
     * because concurrent writes are possible.
     */
    mprotect(host_start, len, PROT_READ);
    for (addr = real_start; addr < real_end; addr += TARGET_PAGE_SIZE) {
        /* inside */
        if (addr >= start && addr < end) {
            qemu_log_mask(LAT_LOG_MEM,
                    "[LATX_16K] %s [inside]  0x"
                    TARGET_FMT_lx " %s %s prot 0x%x\n",
                    __func__, addr, fd > 0 ? "file" : "anon",
                    inside_flags & MAP_SHARED ? "shared" : "private",
                    inside_prot);
            if (fd > 0) {
                /* file mapping */
                inside_p = mmap(0, qemu_host_page_size, inside_prot,
                                (inside_flags & ~MAP_FIXED), fd,
                                offset & qemu_host_page_mask);
                assert(inside_p != MAP_FAILED);
                access_off = (int64_t)inside_p - addr +
                             offset % qemu_host_page_size;
                set_shadow_page(addr, inside_p, access_off);
                offset += TARGET_PAGE_SIZE;
            } else {
                /* anonymous, need memcpy */
                inside_p = mmap(0, qemu_host_page_size, inside_prot | PAGE_WRITE,
                                (inside_flags & ~MAP_FIXED), -1, 0);
                assert(inside_p != MAP_FAILED);
                memcpy(inside_p, g2h_untagged(addr), TARGET_PAGE_SIZE);
                mprotect(inside_p, qemu_host_page_size, inside_prot & PAGE_BITS);
                access_off = (int64_t)inside_p - addr;
                set_shadow_page(addr, inside_p, access_off);
            }
            continue;
        }

        /* outside */
        t_prot = page_get_flags(h2g(addr));
        t_prot &= PAGE_BITS;
        if (t_prot && !page_get_target_data(addr)) {
            outside_p = mmap(0, qemu_host_page_size, t_prot | PAGE_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            assert(outside_p != MAP_FAILED);
            memcpy(outside_p, g2h_untagged(addr), TARGET_PAGE_SIZE);
            mprotect(outside_p, qemu_host_page_size, t_prot & PAGE_BITS);
            access_off = (int64_t)outside_p - addr;
            qemu_log_mask(LAT_LOG_MEM,
                    "[LATX_16K] %s [outside] 0x"
                    TARGET_FMT_lx " %s private prot 0x%x\n",
                    __func__, addr,
                    t_prot & PAGE_ANON ? "anon" : "file", t_prot);
            set_shadow_page(addr, outside_p, access_off);
        }
    }
    mprotect(host_start, len, PROT_NONE);
}

void update_shadow_page_chunk(abi_ulong start, abi_ulong end, int prot,
                              int flags, int fd, abi_ulong offset)
{
    abi_ulong addr;
    void *shadow_p;
    int64_t access_off;

    for (addr = start; addr < end; addr += TARGET_PAGE_SIZE) {
        shadow_p = mmap(0, qemu_host_page_size, prot, flags & ~MAP_FIXED,
                        fd, offset & qemu_host_page_mask);
        assert(shadow_p != MAP_FAILED);
        access_off = (int64_t)shadow_p - addr + offset % qemu_host_page_size;
        if (page_get_target_data(addr)) {
            int t_prot = page_get_flags(h2g(addr));
            qemu_log_mask(LAT_LOG_MEM, "[LATX_16K] %s 0x"
                    TARGET_FMT_lx " %s prot 0x%x => %s %s prot 0x%x\n",
                    __func__, addr, t_prot & PAGE_ANON ? "anon" : "file",
                    t_prot, fd > 0 ? "file" : "anon",
                    flags & MAP_SHARED ? "shared" : "private", prot);
        } else {
            qemu_log_mask(LAT_LOG_MEM, "[LATX_16K] %s 0x"
                    TARGET_FMT_lx " %s %s prot 0x%x\n",
                    __func__, addr, fd > 0 ? "file" : "anon",
                    flags & MAP_SHARED ? "shared" : "private", prot);
        }
        set_shadow_page(addr, shadow_p, access_off);
        if (fd > 0) {
            offset += TARGET_PAGE_SIZE;
        }
    }
}

bool use_indirect_jmp(TranslationBlock *tb)
{
    return (tb->bool_flags & IS_INDIRECT_JMP) ? true : false;
}
#endif
