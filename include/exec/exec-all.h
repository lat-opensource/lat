/*
 * internal execution defines for qemu
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

#ifndef EXEC_ALL_H
#define EXEC_ALL_H

#ifdef CONFIG_LATX_FAST_JMPCACHE
#include "exec/fasttb.h"
#endif
#include "cpu.h"
#ifdef CONFIG_LATX
#include "optimize-config.h"
#endif
#include "exec/tb-context.h"
#ifdef CONFIG_TCG
#include "exec/cpu_ldst.h"
#endif
#include "sysemu/cpu-timers.h"
#include "qemu/interval-tree.h"

/* allow to see translation results - the slowdown should be negligible, so we leave it */
#define DEBUG_DISAS

/* Page tracking code uses ram addresses in system mode, and virtual
   addresses in userspace mode.  Define tb_page_addr_t to be an appropriate
   type.  */
#if defined(CONFIG_USER_ONLY)
typedef abi_ulong tb_page_addr_t;
#define TB_PAGE_ADDR_FMT TARGET_ABI_FMT_lx
#else
typedef ram_addr_t tb_page_addr_t;
#define TB_PAGE_ADDR_FMT RAM_ADDR_FMT
#endif

#include "qemu/log.h"

bool is_tu_tb(TranslationBlock *tb);
bool use_tu_jmp(TranslationBlock *tb);
void set_use_tu_jmp(TranslationBlock *tb);
void unset_use_tu_jmp(TranslationBlock *tb);
bool use_indirect_jmp(TranslationBlock *tb);
bool tb_is_unlink(TranslationBlock *tb, int index);
bool tb_need_relink(TranslationBlock *tb, int index);
void set_tb_unlink_flag(TranslationBlock *tb, int index);
void set_tb_relink_flag(TranslationBlock *tb, int index);
void clear_signal_link_flag(TranslationBlock *tb, int index);

void tb_reset_jump(TranslationBlock *tb, int n);
void restore_state_to_opc(CPUArchState *env, TranslationBlock *tb,
                          target_ulong *data);
int encode_search(TranslationBlock *tb, uint8_t *block);

/**
 * cpu_restore_state:
 * @cpu: the vCPU state is to be restore to
 * @searched_pc: the host PC the fault occurred at
 * @will_exit: true if the TB executed will be interrupted after some
               cpu adjustments. Required for maintaining the correct
               icount valus
 * @return: true if state was restored, false otherwise
 *
 * Attempt to restore the state for a fault occurring in translated
 * code. If the searched_pc is not in translated code no state is
 * restored and the function returns false.
 */
bool cpu_restore_state(CPUState *cpu, uintptr_t searched_pc, bool will_exit);

void QEMU_NORETURN cpu_loop_exit_noexc(CPUState *cpu);
void QEMU_NORETURN cpu_loop_exit(CPUState *cpu);
void QEMU_NORETURN cpu_loop_exit_restore(CPUState *cpu, uintptr_t pc);
void QEMU_NORETURN cpu_loop_exit_atomic(CPUState *cpu, uintptr_t pc);

/**
 * cpu_loop_exit_requested:
 * @cpu: The CPU state to be tested
 *
 * Indicate if somebody asked for a return of the CPU to the main loop
 * (e.g., via cpu_exit() or cpu_interrupt()).
 *
 * This is helpful for architectures that support interruptible
 * instructions. After writing back all state to registers/memory, this
 * call can be used to check if it makes sense to return to the main loop
 * or to continue executing the interruptible instruction.
 */
static inline bool cpu_loop_exit_requested(CPUState *cpu)
{
    return (int32_t)qatomic_read(&cpu_neg(cpu)->icount_decr.u32) < 0;
}

#if !defined(CONFIG_USER_ONLY)
void cpu_reloading_memory_map(void);
/**
 * cpu_address_space_init:
 * @cpu: CPU to add this address space to
 * @asidx: integer index of this address space
 * @prefix: prefix to be used as name of address space
 * @mr: the root memory region of address space
 *
 * Add the specified address space to the CPU's cpu_ases list.
 * The address space added with @asidx 0 is the one used for the
 * convenience pointer cpu->as.
 * The target-specific code which registers ASes is responsible
 * for defining what semantics address space 0, 1, 2, etc have.
 *
 * Before the first call to this function, the caller must set
 * cpu->num_ases to the total number of address spaces it needs
 * to support.
 *
 * Note that with KVM only one address space is supported.
 */
void cpu_address_space_init(CPUState *cpu, int asidx,
                            const char *prefix, MemoryRegion *mr);
#endif

#if !defined(CONFIG_USER_ONLY) && defined(CONFIG_TCG)
/* cputlb.c */
/**
 * tlb_init - initialize a CPU's TLB
 * @cpu: CPU whose TLB should be initialized
 */
void tlb_init(CPUState *cpu);
/**
 * tlb_destroy - destroy a CPU's TLB
 * @cpu: CPU whose TLB should be destroyed
 */
void tlb_destroy(CPUState *cpu);
/**
 * tlb_flush_page:
 * @cpu: CPU whose TLB should be flushed
 * @addr: virtual address of page to be flushed
 *
 * Flush one page from the TLB of the specified CPU, for all
 * MMU indexes.
 */
void tlb_flush_page(CPUState *cpu, target_ulong addr);
/**
 * tlb_flush_page_all_cpus:
 * @cpu: src CPU of the flush
 * @addr: virtual address of page to be flushed
 *
 * Flush one page from the TLB of the specified CPU, for all
 * MMU indexes.
 */
void tlb_flush_page_all_cpus(CPUState *src, target_ulong addr);
/**
 * tlb_flush_page_all_cpus_synced:
 * @cpu: src CPU of the flush
 * @addr: virtual address of page to be flushed
 *
 * Flush one page from the TLB of the specified CPU, for all MMU
 * indexes like tlb_flush_page_all_cpus except the source vCPUs work
 * is scheduled as safe work meaning all flushes will be complete once
 * the source vCPUs safe work is complete. This will depend on when
 * the guests translation ends the TB.
 */
void tlb_flush_page_all_cpus_synced(CPUState *src, target_ulong addr);
/**
 * tlb_flush:
 * @cpu: CPU whose TLB should be flushed
 *
 * Flush the entire TLB for the specified CPU. Most CPU architectures
 * allow the implementation to drop entries from the TLB at any time
 * so this is generally safe. If more selective flushing is required
 * use one of the other functions for efficiency.
 */
void tlb_flush(CPUState *cpu);
/**
 * tlb_flush_all_cpus:
 * @cpu: src CPU of the flush
 */
void tlb_flush_all_cpus(CPUState *src_cpu);
/**
 * tlb_flush_all_cpus_synced:
 * @cpu: src CPU of the flush
 *
 * Like tlb_flush_all_cpus except this except the source vCPUs work is
 * scheduled as safe work meaning all flushes will be complete once
 * the source vCPUs safe work is complete. This will depend on when
 * the guests translation ends the TB.
 */
void tlb_flush_all_cpus_synced(CPUState *src_cpu);
/**
 * tlb_flush_page_by_mmuidx:
 * @cpu: CPU whose TLB should be flushed
 * @addr: virtual address of page to be flushed
 * @idxmap: bitmap of MMU indexes to flush
 *
 * Flush one page from the TLB of the specified CPU, for the specified
 * MMU indexes.
 */
void tlb_flush_page_by_mmuidx(CPUState *cpu, target_ulong addr,
                              uint16_t idxmap);
/**
 * tlb_flush_page_by_mmuidx_all_cpus:
 * @cpu: Originating CPU of the flush
 * @addr: virtual address of page to be flushed
 * @idxmap: bitmap of MMU indexes to flush
 *
 * Flush one page from the TLB of all CPUs, for the specified
 * MMU indexes.
 */
void tlb_flush_page_by_mmuidx_all_cpus(CPUState *cpu, target_ulong addr,
                                       uint16_t idxmap);
/**
 * tlb_flush_page_by_mmuidx_all_cpus_synced:
 * @cpu: Originating CPU of the flush
 * @addr: virtual address of page to be flushed
 * @idxmap: bitmap of MMU indexes to flush
 *
 * Flush one page from the TLB of all CPUs, for the specified MMU
 * indexes like tlb_flush_page_by_mmuidx_all_cpus except the source
 * vCPUs work is scheduled as safe work meaning all flushes will be
 * complete once  the source vCPUs safe work is complete. This will
 * depend on when the guests translation ends the TB.
 */
void tlb_flush_page_by_mmuidx_all_cpus_synced(CPUState *cpu, target_ulong addr,
                                              uint16_t idxmap);
/**
 * tlb_flush_by_mmuidx:
 * @cpu: CPU whose TLB should be flushed
 * @wait: If true ensure synchronisation by exiting the cpu_loop
 * @idxmap: bitmap of MMU indexes to flush
 *
 * Flush all entries from the TLB of the specified CPU, for the specified
 * MMU indexes.
 */
void tlb_flush_by_mmuidx(CPUState *cpu, uint16_t idxmap);
/**
 * tlb_flush_by_mmuidx_all_cpus:
 * @cpu: Originating CPU of the flush
 * @idxmap: bitmap of MMU indexes to flush
 *
 * Flush all entries from all TLBs of all CPUs, for the specified
 * MMU indexes.
 */
void tlb_flush_by_mmuidx_all_cpus(CPUState *cpu, uint16_t idxmap);
/**
 * tlb_flush_by_mmuidx_all_cpus_synced:
 * @cpu: Originating CPU of the flush
 * @idxmap: bitmap of MMU indexes to flush
 *
 * Flush all entries from all TLBs of all CPUs, for the specified
 * MMU indexes like tlb_flush_by_mmuidx_all_cpus except except the source
 * vCPUs work is scheduled as safe work meaning all flushes will be
 * complete once  the source vCPUs safe work is complete. This will
 * depend on when the guests translation ends the TB.
 */
void tlb_flush_by_mmuidx_all_cpus_synced(CPUState *cpu, uint16_t idxmap);

/**
 * tlb_flush_page_bits_by_mmuidx
 * @cpu: CPU whose TLB should be flushed
 * @addr: virtual address of page to be flushed
 * @idxmap: bitmap of mmu indexes to flush
 * @bits: number of significant bits in address
 *
 * Similar to tlb_flush_page_mask, but with a bitmap of indexes.
 */
void tlb_flush_page_bits_by_mmuidx(CPUState *cpu, target_ulong addr,
                                   uint16_t idxmap, unsigned bits);

/* Similarly, with broadcast and syncing. */
void tlb_flush_page_bits_by_mmuidx_all_cpus(CPUState *cpu, target_ulong addr,
                                            uint16_t idxmap, unsigned bits);
void tlb_flush_page_bits_by_mmuidx_all_cpus_synced
    (CPUState *cpu, target_ulong addr, uint16_t idxmap, unsigned bits);

/**
 * tlb_set_page_with_attrs:
 * @cpu: CPU to add this TLB entry for
 * @vaddr: virtual address of page to add entry for
 * @paddr: physical address of the page
 * @attrs: memory transaction attributes
 * @prot: access permissions (PAGE_READ/PAGE_WRITE/PAGE_EXEC bits)
 * @mmu_idx: MMU index to insert TLB entry for
 * @size: size of the page in bytes
 *
 * Add an entry to this CPU's TLB (a mapping from virtual address
 * @vaddr to physical address @paddr) with the specified memory
 * transaction attributes. This is generally called by the target CPU
 * specific code after it has been called through the tlb_fill()
 * entry point and performed a successful page table walk to find
 * the physical address and attributes for the virtual address
 * which provoked the TLB miss.
 *
 * At most one entry for a given virtual address is permitted. Only a
 * single TARGET_PAGE_SIZE region is mapped; the supplied @size is only
 * used by tlb_flush_page.
 */
void tlb_set_page_with_attrs(CPUState *cpu, target_ulong vaddr,
                             hwaddr paddr, MemTxAttrs attrs,
                             int prot, int mmu_idx, target_ulong size);
/* tlb_set_page:
 *
 * This function is equivalent to calling tlb_set_page_with_attrs()
 * with an @attrs argument of MEMTXATTRS_UNSPECIFIED. It's provided
 * as a convenience for CPUs which don't use memory transaction attributes.
 */
void tlb_set_page(CPUState *cpu, target_ulong vaddr,
                  hwaddr paddr, int prot,
                  int mmu_idx, target_ulong size);
#else
static inline void tlb_init(CPUState *cpu)
{
}
static inline void tlb_destroy(CPUState *cpu)
{
}
static inline void tlb_flush_page(CPUState *cpu, target_ulong addr)
{
}
static inline void tlb_flush_page_all_cpus(CPUState *src, target_ulong addr)
{
}
static inline void tlb_flush_page_all_cpus_synced(CPUState *src,
                                                  target_ulong addr)
{
}
static inline void tlb_flush(CPUState *cpu)
{
}
static inline void tlb_flush_all_cpus(CPUState *src_cpu)
{
}
static inline void tlb_flush_all_cpus_synced(CPUState *src_cpu)
{
}
static inline void tlb_flush_page_by_mmuidx(CPUState *cpu,
                                            target_ulong addr, uint16_t idxmap)
{
}

static inline void tlb_flush_by_mmuidx(CPUState *cpu, uint16_t idxmap)
{
}
static inline void tlb_flush_page_by_mmuidx_all_cpus(CPUState *cpu,
                                                     target_ulong addr,
                                                     uint16_t idxmap)
{
}
static inline void tlb_flush_page_by_mmuidx_all_cpus_synced(CPUState *cpu,
                                                            target_ulong addr,
                                                            uint16_t idxmap)
{
}
static inline void tlb_flush_by_mmuidx_all_cpus(CPUState *cpu, uint16_t idxmap)
{
}

static inline void tlb_flush_by_mmuidx_all_cpus_synced(CPUState *cpu,
                                                       uint16_t idxmap)
{
}
static inline void tlb_flush_page_bits_by_mmuidx(CPUState *cpu,
                                                 target_ulong addr,
                                                 uint16_t idxmap,
                                                 unsigned bits)
{
}
static inline void tlb_flush_page_bits_by_mmuidx_all_cpus(CPUState *cpu,
                                                          target_ulong addr,
                                                          uint16_t idxmap,
                                                          unsigned bits)
{
}
static inline void
tlb_flush_page_bits_by_mmuidx_all_cpus_synced(CPUState *cpu, target_ulong addr,
                                              uint16_t idxmap, unsigned bits)
{
}
#endif
/**
 * probe_access:
 * @env: CPUArchState
 * @addr: guest virtual address to look up
 * @size: size of the access
 * @access_type: read, write or execute permission
 * @mmu_idx: MMU index to use for lookup
 * @retaddr: return address for unwinding
 *
 * Look up the guest virtual address @addr.  Raise an exception if the
 * page does not satisfy @access_type.  Raise an exception if the
 * access (@addr, @size) hits a watchpoint.  For writes, mark a clean
 * page as dirty.
 *
 * Finally, return the host address for a page that is backed by RAM,
 * or NULL if the page requires I/O.
 */
void *probe_access(CPUArchState *env, target_ulong addr, int size,
                   MMUAccessType access_type, int mmu_idx, uintptr_t retaddr);

static inline void *probe_write(CPUArchState *env, target_ulong addr, int size,
                                int mmu_idx, uintptr_t retaddr)
{
    return probe_access(env, addr, size, MMU_DATA_STORE, mmu_idx, retaddr);
}

static inline void *probe_read(CPUArchState *env, target_ulong addr, int size,
                               int mmu_idx, uintptr_t retaddr)
{
    return probe_access(env, addr, size, MMU_DATA_LOAD, mmu_idx, retaddr);
}

/**
 * probe_access_flags:
 * @env: CPUArchState
 * @addr: guest virtual address to look up
 * @access_type: read, write or execute permission
 * @mmu_idx: MMU index to use for lookup
 * @nonfault: suppress the fault
 * @phost: return value for host address
 * @retaddr: return address for unwinding
 *
 * Similar to probe_access, loosely returning the TLB_FLAGS_MASK for
 * the page, and storing the host address for RAM in @phost.
 *
 * If @nonfault is set, do not raise an exception but return TLB_INVALID_MASK.
 * Do not handle watchpoints, but include TLB_WATCHPOINT in the returned flags.
 * Do handle clean pages, so exclude TLB_NOTDIRY from the returned flags.
 * For simplicity, all "mmio-like" flags are folded to TLB_MMIO.
 */
int probe_access_flags(CPUArchState *env, target_ulong addr,
                       MMUAccessType access_type, int mmu_idx,
                       bool nonfault, void **phost, uintptr_t retaddr);

#define CODE_GEN_ALIGN           16 /* must be >= of the size of a icache line */

/* Estimated block size for TB allocation.  */
/* ??? The following is based on a 2015 survey of x86_64 host output.
   Better would seem to be some sort of dynamically sized TB array,
   adapting to the block sizes actually being produced.  */
#if defined(CONFIG_SOFTMMU)
#define CODE_GEN_AVG_BLOCK_SIZE 400
#else
#define CODE_GEN_AVG_BLOCK_SIZE 150
#endif

/*
 * Translation Cache-related fields of a TB.
 * This struct exists just for convenience; we keep track of TB's in a binary
 * search tree, and the only fields needed to compare TB's in the tree are
 * @ptr and @size.
 * Note: the address of search data can be obtained by adding @size to @ptr.
 */
struct tb_tc {
    const void *ptr;    /* pointer to the translated code */
    size_t size;
};

struct tu_unlink {
    uint32_t stub_offset; /* The offset of the unlink instruction used by TU. */
    uint32_t ins;
};

typedef struct TBProfile
{
    uint64_t nr_code;       /* ir2 number */
    uint64_t jrra_in;       /* jrra in count, return count */
    uint64_t jrra_miss;     /* jrra miss count */
    uint64_t exec_times;    /* tb executes times */
    uint64_t exit_times;    /* tb context switch times */
    /* eflags related */
    uint64_t sta_eliminate; /* static eliminate, need plus exec_times */
    uint64_t sta_generate;  /* static generate, include eliminate */
    uint64_t sta_simulate;  /* static simulate */
    uint64_t dyn_eliminate; /* dynamic eliminate */
    uint64_t dyn_generate;  /* dynamic generate */
} TBProfile;

#define CLN_TB_PROFILE(tb)              (memset(&(tb->profile), 0, sizeof(TBProfile)))
#define GET_TB_PROFILE(tb, area)        (((tb->profile).area)           )
#define SET_TB_PROFILE(tb, area, value) (((tb->profile).area)  = (value))
#define ADD_TB_PROFILE(tb, area, value) (((tb->profile).area) += (value))


#if defined(CONFIG_LATX_TU) || defined(CONFIG_LATX_AOT)
#define TU_TB_INDEX_NEXT 0
#define TU_TB_INDEX_TARGET 1
typedef enum tu_tb_mode_type {
    TU_TB_MODE_NONE = 0,
    TU_TB_MODE_SWITCH_TO_TB,
    TU_TB_MODE_BROKEN,
    TU_TB_MODE_STATIC,
    TB_GEN_CODE,
    BAD_TB
} tu_tb_mode_type;
#endif

struct separated_data{
    uint8_t eflag_out;
    uint8_t  _top_in;
    uint8_t  _top_out;
    TranslationBlock *next_tb[2];
    void     *ir1;
    int32_t rel_start;
    int32_t rel_end;
    uint8_t last_ir1_type;
    union {
        int is_first_tb;
        int tu_size;
    };
#ifdef CONFIG_LATX_TU
    tu_tb_mode_type tu_tb_mode;
    size_t offset_in_tu;
    target_ulong tu_id;
#endif
    target_ulong next_pc;
    target_ulong target_pc;

#ifdef CONFIG_LATX_HBR
    union {
        uint32_t xmm_in;
        uint32_t gpr_in;
    };
    union {
        uint32_t xmm_out;
        uint32_t gpr_out;
    };
    union {
        uint32_t xmm_use;
        /* Need the previous TB to provide the correct GPR. */
        uint32_t gpr_use;
    };
    union {
        uint32_t xmm_def;
        /* Can provide the correct GPR for the next TB. */
        uint32_t gpr_def;
    };
    uint8_t shbr_type;
#endif
};

struct TranslationBlock {
    /*
     * Each TB has a NULL-terminated list (jmp_list_head) of incoming jumps.
     * Each TB can have two outgoing jumps, and therefore can participate
     * in two lists. The list entries are kept in jmp_list_next[2]. The least
     * significant bit (LSB) of the pointers in these lists is used to encode
     * which of the two list entries is to be used in the pointed TB.
     *
     * List traversals are protected by jmp_lock. The destination TB of each
     * outgoing jump is kept in jmp_dest[] so that the appropriate jmp_lock
     * can be acquired from any origin TB.
     *
     * jmp_dest[] are tagged pointers as well. The LSB is set when the TB is
     * being invalidated, so that no further outgoing jumps from it can be set.
     *
     * jmp_lock also protects the CF_INVALID cflag; a jump must not be chained
     * to a destination TB that has CF_INVALID set.
     */
    uintptr_t jmp_list_head;
    uintptr_t jmp_list_next[2];
    uintptr_t jmp_dest[2];

    union {
        uintptr_t jmp_target_arg[2];  /* target address or offset */
        struct tu_unlink tu_unlink;
    };

    union {
        uintptr_t jmp_stub_target_arg[2];
        uintptr_t jmp_indirect;
    };

    struct tb_tc tc;

    /*
     * Track tb_page_addr_t intervals that intersect this TB.
     * For user-only, the virtual addresses are always contiguous,
     * and we use a unified interval tree.  For system, we use a
     * linked list headed in each PageDesc.  Within the list, the lsb
     * of the previous pointer tells the index of page_next[], and the
     * list is protected by the PageDesc lock(s).
     */
#ifdef CONFIG_USER_ONLY
    IntervalTreeNode itree;
#else
    uintptr_t page_next[2];
    tb_page_addr_t page_addr[2];
#endif

    target_ulong pc;   /* simulated PC corresponding to this block (EIP + CS base) */
    /* jmp_lock placed here to fill a 4-byte hole. Its documentation is below */
    QemuSpin jmp_lock;

    uint32_t flags; /* flags defining in which context the code was generated */
    uint32_t cflags;    /* compile flags */
#define CF_COUNT_MASK  0x00007fff
#define CF_LAST_IO     0x00008000 /* Last insn may be an IO access.  */
#define CF_MEMI_ONLY   0x00010000 /* Only instrument memory ops */
#define CF_USE_ICOUNT  0x00020000
#define CF_INVALID     0x00040000 /* TB is stale. Set with @jmp_lock held */
#define CF_PARALLEL    0x00080000 /* Generate code for a parallel context */
#define CF_CLUSTER_MASK 0xff000000 /* Top 8 bits are cluster ID */
#define CF_CLUSTER_SHIFT 24

    /* The following data are used to directly call another TB from
     * the code of this one. This can be done either by emitting direct or
     * indirect native jump instructions. These jumps are reset so that the TB
     * just continues its execution. The TB can be linked to another one by
     * setting one of the jump targets (or patching the jump instruction). Only
     * two of such jumps are supported.
     */
#define TB_JMP_RESET_OFFSET_INVALID 0xffff /* indicates no jump generated */
    union {
        uint16_t jmp_reset_offset[2]; /* offset of original jump target */
        uint16_t tu_jmp[2]; /* The offset of the jump instruction used by TU. */
    };

    uint16_t jmp_stub_reset_offset[2];

    uint16_t first_jmp_align;

    /* size of target code for this block (1 <= size <= TARGET_PAGE_SIZE) */
    uint16_t size;
    uint16_t icount;

#ifdef CONFIG_LATX
#ifdef CONFIG_LATX_TU
    uint32_t tu_search_addr_offset;
#endif
#define TARGET1_ELIMINATE 0x01
#define OPT_BCC           0x02
#define IS_ENABLE_JRRA    0x04
#define IS_AOT_TB         0x08
#define IS_TUNNEL_LIB     0x10
#define IS_TU_TB          0x20
#define IS_TU_JMP         0x40
#define IS_INDIRECT_JMP   0x80
#define SIGNAL_UNLINK0    0x100
#define SIGNAL_RELINK0    0x200
#define SIGNAL_UNLINK1    0x400
#define SIGNAL_RELINK1    0x800
#define IS_CODE64        0x1000
    uint16_t bool_flags;
    uint8_t  eflag_use;
#ifdef CONFIG_LATX_INSTS_PATTERN
    /*
     * [0] : not taken
     * [1] : taken
     * [2] : the backup of the eflags instruction, which is used
     *       to recover the eflags instruction when tb unlink.
     */
    #define EFLAG_BACKUP 2
    uint16_t eflags_target_arg[EFLAG_BACKUP + 1];
#endif
#ifdef CONFIG_LATX_PROFILER
    TBProfile profile __attribute__((aligned(8)));
#endif
    /* remember to free these memory when QEMU recycle one TB */
#ifdef CONFIG_LATX_JRRA
    unsigned long *return_target_ptr;
    unsigned long next_86_pc;
#endif
#ifdef CONFIG_LATX_MONITOR_SHARED_MEM
    unsigned long checksum;
#endif
#ifdef CONFIG_LATX_SMC_OPT
#define TBSMC_COUNT_MASK        0xffff
#define TBSMC_OPT_THRESHOLD     0xff
#define TBSMC_OPTED_MASK        (0x1 << 16)
    unsigned long smc_data;
#endif
    int64_t lazypc[2];
    int8_t lazylink[2];
    uint64_t lazylinkinst[2];
    struct separated_data *s_data;
#endif
};

#ifdef CONFIG_LATX_SMC_OPT
static inline int tb_use_smc_opt(TranslationBlock *tb)
{
    if (tb) {
        return tb->smc_data & TBSMC_OPTED_MASK;
    } else {
        return 0;
    }
}
#endif

#define TB_MAGIC 0xbeefUL
#define HOST_VIRT_ADDR_SPACE_BITS 48
typedef struct MagicTBP {
    unsigned short tb_pointer[3]; /* 48 bits are reserved for vadd */
    unsigned short magic; /* 0xbeef */
} MagicTBP;

typedef struct TBMini {
    union {
        MagicTBP mtbp_struct;
        uint64_t mtbp_uint64;
    };
} TBMini;

typedef enum IR1_TYPE {
    IR1_TYPE_NORMAL = 0,
    IR1_TYPE_BRANCH,
    IR1_TYPE_JUMP,
    IR1_TYPE_CALL,
    IR1_TYPE_RET,
    IR1_TYPE_JUMPIN,
    IR1_TYPE_CALLIN,
    IR1_TYPE_SYSCALL,
    IR1_TYPE_COUNT,
} IR1_TYPE;

/* Hide the qatomic_read to make code a little easier on the eyes */
static inline uint32_t tb_cflags(const TranslationBlock *tb)
{
    return qatomic_read(&tb->cflags);
}

static inline tb_page_addr_t tb_page_addr0(const TranslationBlock *tb)
{
#ifdef CONFIG_USER_ONLY
    return tb->itree.start;
#else
    return tb->page_addr[0];
#endif
}

static inline tb_page_addr_t tb_page_addr1(const TranslationBlock *tb)
{
#ifdef CONFIG_USER_ONLY
    tb_page_addr_t next = tb->itree.last & TARGET_PAGE_MASK;
    return next == (tb->itree.start & TARGET_PAGE_MASK) ? -1 : next;
#else
    return tb->page_addr[1];
#endif
}

static inline void tb_set_page_addr0(TranslationBlock *tb,
                                     tb_page_addr_t addr)
{
#ifdef CONFIG_USER_ONLY
    tb->itree.start = addr;
    /*
     * To begin, we record an interval of one byte.  When the translation
     * loop encounters a second page, the interval will be extended to
     * include the first byte of the second page, which is sufficient to
     * allow tb_page_addr1() above to work properly.  The final corrected
     * interval will be set by tb_page_add() from tb->size before the
     * node is added to the interval tree.
     */
    tb->itree.last = addr;
#else
    tb->page_addr[0] = addr;
#endif
}

static inline void tb_set_page_addr1(TranslationBlock *tb,
                                     tb_page_addr_t addr)
{
#ifdef CONFIG_USER_ONLY
    /* Extend the interval to the first byte of the second page.  See above. */
    tb->itree.last = addr;
#else
    tb->page_addr[1] = addr;
#endif
}

/* current cflags for hashing/comparison */
static inline uint32_t curr_cflags(CPUState *cpu)
{
    return cpu->tcg_cflags;
}

/* TranslationBlock invalidate API */
#if defined(CONFIG_USER_ONLY)
void tb_invalidate_phys_addr(target_ulong addr);
void tb_invalidate_phys_range(target_ulong start, target_ulong end);
bool tb_invalidate_phys_page_unwind(tb_page_addr_t addr, uintptr_t pc, int *n);
#else
void tb_invalidate_phys_addr(AddressSpace *as, hwaddr addr, MemTxAttrs attrs);
#endif
void tb_flush(CPUState *cpu);

void do_tb_flush(CPUState *cpu, run_on_cpu_data tb_flush_count);
void tb_phys_invalidate(TranslationBlock *tb, tb_page_addr_t page_addr);
TranslationBlock *tb_htable_lookup(CPUState *cpu, target_ulong pc,
                                   target_ulong cs_base, uint32_t flags,
                                   uint32_t cflags);
void tb_eflag_eliminate(TranslationBlock *tb, int n);
void tb_eflag_recover(TranslationBlock *tb, int n);
#ifdef CONFIG_LATX_XCOMISX_OPT
void tb_stub_bypass(TranslationBlock *tb, int n, uintptr_t addr);
#endif
void tb_set_jmp_target(TranslationBlock *tb, int n, uintptr_t addr);
#ifdef CONFIG_LATX_TU
void tu_relink(TranslationBlock *tb);
#endif

/* GETPC is the true target of the return instruction that we'll execute.  */
#if defined(CONFIG_TCG_INTERPRETER)
extern __thread uintptr_t tci_tb_ptr;
# define GETPC() tci_tb_ptr
#else
# define GETPC() \
    ((uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0)))
#endif

/* The true return address will often point to a host insn that is part of
   the next translated guest insn.  Adjust the address backward to point to
   the middle of the call insn.  Subtracting one would do the job except for
   several compressed mode architectures (arm, mips) which set the low bit
   to indicate the compressed mode; subtracting two works around that.  It
   is also the case that there are no host isas that contain a call insn
   smaller than 4 bytes, so we don't worry about special-casing this.  */
#define GETPC_ADJ   2

#if !defined(CONFIG_USER_ONLY) && defined(CONFIG_DEBUG_TCG)
void assert_no_pages_locked(void);
#else
static inline void assert_no_pages_locked(void)
{
}
#endif

#if !defined(CONFIG_USER_ONLY)

/**
 * iotlb_to_section:
 * @cpu: CPU performing the access
 * @index: TCG CPU IOTLB entry
 *
 * Given a TCG CPU IOTLB entry, return the MemoryRegionSection that
 * it refers to. @index will have been initially created and returned
 * by memory_region_section_get_iotlb().
 */
struct MemoryRegionSection *iotlb_to_section(CPUState *cpu,
                                             hwaddr index, MemTxAttrs attrs);
#endif

void mmap_trylock(void);

/**
 * get_page_addr_code_hostp()
 * @env: CPUArchState
 * @addr: guest virtual address of guest code
 *
 * See get_page_addr_code() (full-system version) for documentation on the
 * return value.
 *
 * Sets *@hostp (when @hostp is non-NULL) as follows.
 * If the return value is -1, sets *@hostp to NULL. Otherwise, sets *@hostp
 * to the host address where @addr's content is kept.
 *
 * Note: this function can trigger an exception.
 */
tb_page_addr_t get_page_addr_code_hostp(CPUArchState *env, target_ulong addr,
                                        void **hostp);

/**
 * get_page_addr_code()
 * @env: CPUArchState
 * @addr: guest virtual address of guest code
 *
 * If we cannot translate and execute from the entire RAM page, or if
 * the region is not backed by RAM, returns -1. Otherwise, returns the
 * ram_addr_t corresponding to the guest code at @addr.
 *
 * Note: this function can trigger an exception.
 */
static inline tb_page_addr_t get_page_addr_code(CPUArchState *env,
                                                target_ulong addr)
{
    return get_page_addr_code_hostp(env, addr, NULL);
}

#if defined(CONFIG_USER_ONLY)
void mmap_lock(void);
void mmap_unlock(void);
bool have_mmap_lock(void);

#else
static inline void mmap_lock(void) {}
static inline void mmap_unlock(void) {}

void tlb_reset_dirty(CPUState *cpu, ram_addr_t start1, ram_addr_t length);
void tlb_set_dirty(CPUState *cpu, target_ulong vaddr);

MemoryRegionSection *
address_space_translate_for_iotlb(CPUState *cpu, int asidx, hwaddr addr,
                                  hwaddr *xlat, hwaddr *plen,
                                  MemTxAttrs attrs, int *prot);
hwaddr memory_region_section_get_iotlb(CPUState *cpu,
                                       MemoryRegionSection *section);
#endif

#endif
