/*
 *  mmap support for qemu
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#include "lsenv.h"
#include "qemu/osdep.h"
#include "trace.h"
#include "exec/log.h"
#include "qemu.h"
#ifdef CONFIG_LATX
#include "latx-config.h"
#endif
#ifdef CONFIG_LATX_PERF
#include "latx-perf.h"
#endif
#ifdef CONFIG_LATX_AOT
#include "segment.h"
#include "aot.h"
#include "aot_smc.h"
#include "aot_page.h"
#include "latx-options.h"
#endif
#include <sys/resource.h>

static pthread_mutex_t mmap_mutex = PTHREAD_MUTEX_INITIALIZER;
static __thread int mmap_lock_count;


void mmap_lock(void)
{
    if (mmap_lock_count++ == 0) {
        pthread_mutex_lock(&mmap_mutex);
    }
#ifdef CONFIG_LATX_PERF
    latx_timer_start(TIMER_MMAP_LOCK);
#endif
}

void mmap_trylock(void)
{
    if (mmap_lock_count++ == 0) {
        pthread_mutex_trylock(&mmap_mutex);
    }
}

void mmap_unlock(void)
{
#ifdef CONFIG_LATX_PERF
    latx_timer_stop(TIMER_MMAP_LOCK);
#endif
    if (--mmap_lock_count == 0) {
        pthread_mutex_unlock(&mmap_mutex);
    }
}

bool have_mmap_lock(void)
{
    return mmap_lock_count > 0 ? true : false;
}

/* Grab lock to make sure things are in a consistent state after fork().  */
void mmap_fork_start(void)
{
    if (mmap_lock_count)
        abort();
    pthread_mutex_lock(&mmap_mutex);
}

void mmap_fork_end(int child)
{
    if (child)
        pthread_mutex_init(&mmap_mutex, NULL);
    else
        pthread_mutex_unlock(&mmap_mutex);
}

/*
 * Validate target prot bitmask.
 * Return the prot bitmask for the host in *HOST_PROT.
 * Return 0 if the target prot bitmask is invalid, otherwise
 * the internal qemu page_flags (which will include PAGE_VALID).
 */
static int validate_prot_to_pageflags(int *host_prot, int prot)
{
    int valid = PROT_READ | PROT_WRITE | PROT_EXEC | TARGET_PROT_SEM;
    int page_flags = (prot & PAGE_BITS) | PAGE_VALID;

    /*
     * For the host, we need not pass anything except read/write/exec.
     * While PROT_SEM is allowed by all hosts, it is also ignored, so
     * don't bother transforming guest bit to host bit.  Any other
     * target-specific prot bits will not be understood by the host
     * and will need to be encoded into page_flags for qemu emulation.
     *
     * Pages that are executable by the guest will never be executed
     * by the host, but the host will need to be able to read them.
     */
    *host_prot = (prot & (PROT_READ | PROT_WRITE))
               | (prot & PROT_EXEC ? PROT_READ : 0);

#ifdef TARGET_AARCH64
    {
        ARMCPU *cpu = ARM_CPU(thread_cpu);

        /*
         * The PROT_BTI bit is only accepted if the cpu supports the feature.
         * Since this is the unusual case, don't bother checking unless
         * the bit has been requested.  If set and valid, record the bit
         * within QEMU's page_flags.
         */
        if ((prot & TARGET_PROT_BTI) && cpu_isar_feature(aa64_bti, cpu)) {
            valid |= TARGET_PROT_BTI;
            page_flags |= PAGE_BTI;
        }
        /* Similarly for the PROT_MTE bit. */
        if ((prot & TARGET_PROT_MTE) && cpu_isar_feature(aa64_mte, cpu)) {
            valid |= TARGET_PROT_MTE;
            page_flags |= PAGE_MTE;
        }
    }
#endif

    return prot & ~valid ? 0 : page_flags;
}

/* NOTE: all the constants are the HOST ones, but addresses are target. */
int target_mprotect(abi_ulong start, abi_ulong len, int target_prot)
{
    abi_ulong end, host_start, host_end, addr;
    int prot1, ret, page_flags, host_prot;
    int prot_tmp, shadow_mask;

    trace_target_mprotect(start, len, target_prot);

    if ((start & ~TARGET_PAGE_MASK) != 0) {
        return -TARGET_EINVAL;
    }
    page_flags = validate_prot_to_pageflags(&host_prot, target_prot);
    if (!page_flags) {
        return -TARGET_EINVAL;
    }
    len = TARGET_PAGE_ALIGN(len);
    end = start + len;
    if (len == 0) {
        return 0;
    }
    if (!guest_range_valid_untagged(start, len)) {
        return -TARGET_ENOMEM;
    }

    mmap_lock();

    host_start = start & qemu_host_page_mask;
    host_end = HOST_PAGE_ALIGN(end);
    if (start > host_start) {
        /* handle host page containing start */
        prot1 = host_prot;
        for (addr = host_start; addr < start; addr += TARGET_PAGE_SIZE) {
            prot_tmp = page_get_flags(addr);
            if ((prot_tmp & PAGE_EXEC) && !(prot_tmp & PAGE_WRITE) &&
                (prot_tmp & PAGE_WRITE_ORG) && (target_prot & PAGE_WRITE)) {
                page_set_flags(addr, addr + TARGET_PAGE_SIZE,
                                prot_tmp | PAGE_WRITE);
            }
            prot1 |= prot_tmp;
        }

        if (host_end == host_start + qemu_host_page_size) {
            for (addr = end; addr < host_end; addr += TARGET_PAGE_SIZE) {
                prot_tmp = page_get_flags(addr);
                if ((prot_tmp & PAGE_EXEC) && !(prot_tmp & PAGE_WRITE) &&
                    (prot_tmp & PAGE_WRITE_ORG) && (target_prot & PAGE_WRITE)) {
                    page_set_flags(addr, addr + TARGET_PAGE_SIZE,
                                    prot_tmp | PAGE_WRITE);
                }
                prot1 |= prot_tmp;
            }
            end = host_end;
        }

        /* inside */
        abi_ulong tmp_end = end < host_start + qemu_host_page_size ?
                            end : host_start + qemu_host_page_size;
        shadow_mask = hostpage_exist_shadow_page(start);
        if (shadow_mask) {
            int i = (start - host_start) / TARGET_PAGE_SIZE;
            for (addr = start; addr < tmp_end; addr += TARGET_PAGE_SIZE, i++) {
                if (shadow_mask & (0x1 << i)) {
                    ret = mprotect_one_shadow_page(addr, host_prot);
                    if (ret != 0) {
                        goto error;
                    }
                }
            }
        } else {
            ret = mprotect(g2h_untagged(host_start), qemu_host_page_size,
                           prot1 & PAGE_BITS);
            if (ret != 0) {
                goto error;
            }
        }

        host_start += qemu_host_page_size;
    }
    if (end < host_end) {
        prot1 = host_prot;
        for (addr = end; addr < host_end; addr += TARGET_PAGE_SIZE) {
            prot_tmp = page_get_flags(addr);
            if ((prot_tmp & PAGE_EXEC) && !(prot_tmp & PAGE_WRITE) &&
                (prot_tmp & PAGE_WRITE_ORG) && (target_prot & PAGE_WRITE)) {
                page_set_flags(addr, addr + TARGET_PAGE_SIZE,
                                prot_tmp | PAGE_WRITE);
            }
            prot1 |= prot_tmp;
        }

        shadow_mask = hostpage_exist_shadow_page(end);
        if (shadow_mask) {
            int i = 0;
            for (addr = host_end - qemu_host_page_size; addr < end;
                addr += TARGET_PAGE_SIZE, i++) {
                if (shadow_mask & (0x1 << i)) {
                    ret = mprotect_one_shadow_page(addr, host_prot);
                    if (ret != 0) {
                        goto error;
                    }
                }
            }
        } else {
            ret = mprotect(g2h_untagged(host_end - qemu_host_page_size),
                           qemu_host_page_size, prot1 & PAGE_BITS);
            if (ret != 0) {
                goto error;
            }
        }
        host_end -= qemu_host_page_size;
    }

    /* handle the pages in the middle */
    if (host_start < host_end) {
        ret = mprotect_shadow_page_range_if_exist(host_start, host_end, host_prot);
        if (ret != 0) {
            goto error;
        }
    }

    page_set_flags(start, start + len, page_flags);
    if(target_prot == PROT_NONE) {
#ifdef CONFIG_LATX_AOT
        if (option_aot && segment_tree_lookup2(start, start + len)) {
            page_set_page_state_range(start, start + len, PAGE_SMC);
        }
#endif
    }

    mmap_unlock();
    return 0;
error:
    mmap_unlock();
    return ret;
}

/* map an incomplete host page */
static int mmap_frag(abi_ulong real_start,
                     abi_ulong start, abi_ulong end,
                     int prot, int flags, int fd, abi_ulong offset)
{
    abi_ulong real_end, addr;
    void *host_start;
    int prot1, prot2, prot_new;

    real_end = real_start + qemu_host_page_size;
    host_start = g2h_untagged(real_start);

    /* get the protection of the target pages outside the mapping */
    prot1 = 0;
    for (addr = real_start; addr < real_end; addr += TARGET_PAGE_SIZE) {
        if (addr < start || addr >= end)
            prot1 |= page_get_flags(addr);
    }

    prot2 = 0;
    for (addr = start; addr < end; addr += TARGET_PAGE_SIZE) {
        prot2 |= page_get_flags(addr);
    }

    if (prot1 == 0) {
        /* no page was there, so we allocate one */
        void *p = mmap(host_start, qemu_host_page_size, prot,
                       flags | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED)
            return -1;
        prot1 = prot;
    } else if ((prot2 & PAGE_OVERFLOW)) {
        void *p = mmap(host_start, qemu_host_page_size, prot1,
                       flags | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) {
            return -1;
        }
        pageflags_set_clear(real_start, real_end - 1, 0, PAGE_OVERFLOW);
        //page_clear_overflow(real_start, real_end);
    }
    prot1 &= PAGE_BITS;

    int shadow_mask = hostpage_exist_shadow_page(start);
    int page_mask = 0;
    for (addr = start; addr < end;  addr += TARGET_PAGE_SIZE) {
        page_mask |= 1U << (addr - real_start) / TARGET_PAGE_SIZE;
    }
    if (shadow_mask) {
        if (shadow_mask == page_mask) {
            shadow_page_munmap(start, end);
        } else {
            update_shadow_page_chunk(start, end, prot, flags, fd, offset);
            return 1;
        }
    }

    prot_new = prot | prot1;
    if (!(flags & MAP_ANONYMOUS)) {
        if ((flags & MAP_TYPE) == MAP_SHARED) {
            struct stat stat_info;
            fstat(fd, &stat_info);
            if (!stat_info.st_nlink ||
                (prot & PROT_WRITE)) {
                create_shadow_page_chunk(start, end, prot, flags, fd, offset);
                return 1;
            }
        }
        /* adjust protection to be able to read */
        if (!(prot1 & PROT_WRITE)) {
            mprotect(host_start, qemu_host_page_size, prot1 | PROT_WRITE);
        }

        /* read the corresponding file data */
        if (pread(fd, g2h_untagged(start), end - start, offset) == -1) {
            return -1;
        }

        /* put final protection */
        if (prot_new != (prot1 | PROT_WRITE)) {
            mprotect(host_start, qemu_host_page_size, prot_new);
        }
    } else {
        if (prot_new != prot1) {
            mprotect(host_start, qemu_host_page_size, prot_new);
        }
        if (prot_new & PROT_WRITE) {
            memset(g2h_untagged(start), 0, end - start);
        }
    }
    return 0;
}

#if HOST_LONG_BITS == 64 && TARGET_ABI_BITS == 64
#ifdef TARGET_AARCH64
# define TASK_UNMAPPED_BASE  0x5500000000
#else
//# define TASK_UNMAPPED_BASE  (1ul << 38)
# define TASK_UNMAPPED_BASE  (0x5500000000)
#endif
#else
# define TASK_UNMAPPED_BASE  0x40000000
#endif
abi_ulong mmap_next_start = TASK_UNMAPPED_BASE;

#ifdef TARGET_X86_64
#define MMAP_2G 0x80000000;
abi_ulong mmap_next_start_2g = MMAP_2G;
#endif

unsigned long last_brk;

/* Subroutine of mmap_find_vma, used when we have pre-allocated a chunk
   of guest address space.  */
static abi_ulong mmap_find_vma_reserved(abi_ulong start, abi_ulong size,
                                        abi_ulong align)
{
    /*
    abi_ulong tmp, addr, end_addr;
    bool looped = false;

    if (size > reserved_va) {
        return (abi_ulong)-1;
    }


    end_addr = start + size;
    if (start > reserved_va - size) {
        end_addr = ((reserved_va - size) & -align) + size;
        looped = true;
    }

    addr = start;
    while (1) {
        if (addr > end_addr || addr == 0) {
            if (looped) {
                return (abi_ulong)-1;
            }
            end_addr = ((reserved_va - size) & -align) + size;
            addr = end_addr - size;
            looped = true;
        } else {
            tmp = get_first_page(addr, end_addr - 1);
            if (tmp) {
                addr = (tmp - size) & -align;
                end_addr = addr + size;
            } else if (addr) {
                if (start == mmap_next_start) {
                    mmap_next_start = addr;
                }
                return addr;
            }
        }
    }
    */
    target_ulong ret;
    target_ulong max = reserved_va - 1;

    if (start + size > max) {
        start = mmap_next_start;
    }

    ret = page_find_range_empty(start, max, size, align);
    if (ret == -1 && start > mmap_min_addr) {
        /* Restart at the beginning of the address space. */
        ret = page_find_range_empty(mmap_min_addr, start - 1, size, align);
    }

    return ret;
}

/*
 * Find and reserve a free memory area of size 'size'. The search
 * starts at 'start'.
 * It must be called with mmap_lock() held.
 * Return -1 if error.
 */
abi_ulong mmap_find_vma(abi_ulong start, abi_ulong size, abi_ulong align)
{
    void *ptr, *prev;
    abi_ulong addr;
    int wrapped, repeat;

    align = MAX(align, qemu_host_page_size);

    /* If 'start' == 0, then a default start address is used. */
    if (start == 0) {
        start = mmap_next_start;
    } else {
        start &= qemu_host_page_mask;
    }
    start = ROUND_UP(start, align);

    size = HOST_PAGE_ALIGN(size);

    if (reserved_va) {
        return mmap_find_vma_reserved(start, size, align);
    }

    addr = start;
    wrapped = repeat = 0;
    prev = 0;

    for (;; prev = ptr) {
        /*
         * Reserve needed memory area to avoid a race.
         * It should be discarded using:
         *  - mmap() with MAP_FIXED flag
         *  - mremap() with MREMAP_FIXED flag
         *  - shmat() with SHM_REMAP flag
         */
        ptr = mmap(g2h_untagged(addr), size, PROT_NONE,
                   MAP_ANONYMOUS|MAP_PRIVATE|MAP_NORESERVE, -1, 0);

        /* ENOMEM, if host address space has no memory */
        if (ptr == MAP_FAILED) {
            return (abi_ulong)-1;
        }

        /* Count the number of sequential returns of the same address.
           This is used to modify the search algorithm below.  */
        repeat = (ptr == prev ? repeat + 1 : 0);

        if (h2g_valid(ptr + size - 1)) {
            addr = h2g(ptr);

            if ((addr & (align - 1)) == 0) {
                /* Success.  */
                if (start == mmap_next_start && addr >= TASK_UNMAPPED_BASE) {
                    mmap_next_start = addr + size;
                }
                return addr;
            }

            /* The address is not properly aligned for the target.  */
            switch (repeat) {
            case 0:
                /* Assume the result that the kernel gave us is the
                   first with enough free space, so start again at the
                   next higher target page.  */
                addr = ROUND_UP(addr, align);
                break;
            case 1:
                /* Sometimes the kernel decides to perform the allocation
                   at the top end of memory instead.  */
                addr &= -align;
                break;
            case 2:
                /* Start over at low memory.  */
                addr = 0;
                break;
            default:
                /* Fail.  This unaligned block must the last.  */
                addr = -1;
                break;
            }
        } else {
            /* Since the result the kernel gave didn't fit, start
               again at low memory.  If any repetition, fail.  */
            addr = (repeat ? -1 : 0);
        }

        /* Unmap and try again.  */
        munmap(ptr, size);

        /* ENOMEM if we checked the whole of the target address space.  */
        if (addr == (abi_ulong)-1) {
            return (abi_ulong)-1;
        } else if (addr == 0) {
            if (wrapped) {
                return (abi_ulong)-1;
            }
            wrapped = 1;
            /* Don't actually use 0 when wrapping, instead indicate
               that we'd truly like an allocation in low memory.  */
            addr = (mmap_min_addr > TARGET_PAGE_SIZE
                     ? TARGET_PAGE_ALIGN(mmap_min_addr)
                     : TARGET_PAGE_SIZE);
        } else if (wrapped && addr >= start) {
            return (abi_ulong)-1;
        }
    }
}

#ifdef TARGET_X86_64
abi_ulong mmap_find_vma_2g(abi_ulong start, abi_ulong size, abi_ulong align)
{

    bool looped = false;
    align = MAX(align, qemu_host_page_size);

    /* If 'start' == 0, then a default start address is used. */
    if (start == 0) {
        start = mmap_next_start_2g;
    } else {
        start &= qemu_host_page_mask;
    }
    start = ROUND_UP(start, align);

    size = HOST_PAGE_ALIGN(size);

    if (reserved_va) {
        abi_ulong addr, end_addr, incr = qemu_host_page_size;
        int prot;

        if (size > reserved_va) {
            return (abi_ulong)-1;
        }

        end_addr = mmap_next_start_2g;

        /* Search downward from END_ADDR, checking to see if a page is in use.  */
        addr = end_addr;
        while (1) {
            addr -= incr;
            if (addr > end_addr) {
                if (looped) {
                    /* Failure.  The entire address space has been searched.  */
                    qemu_log("MAP_32BIT failed after a loop find.\n");
                    return (abi_ulong)-1;
                }
                /* Re-start at the top of the address space.  */
                addr = end_addr = MMAP_2G;
                looped = true;
            } else {
                prot = page_get_flags(addr);
                if (prot) {
                    /* Page in use.  Restart below this page. */
                qemu_log("Page in use %lx prot %x\n", addr, prot);
                    addr = end_addr = ((addr - size) & -align) + size;
                } else if (addr && addr + size == end_addr) {
                    /* Success!  All pages between ADDR and END_ADDR are free.  */
                    if (start == mmap_next_start_2g) {
                        mmap_next_start_2g = addr;
                    }
                    return addr;
                }
            }
        }
    } else {
        qemu_log("MAP_32BIT not supported when reserved_va not used.\n");
        return (abi_ulong)-1;
    }
}
#endif

static int create_shadow_file(int fd, uint64 offset, abi_ulong start, abi_ulong len)
{
    char *tmp_buf;
    char file_name[64];
    snprintf(file_name, sizeof(file_name), "/tmp/lat_%d", getpid());
    FILE *file = fopen(file_name, "wb");
    if (file == NULL) {
        return -1;
    }

    size_t mov_len = start % qemu_host_page_size;
    if (mov_len) {
        /* fprintf(stderr, "mov_len %lx\n", mov_len); */
        tmp_buf = (char *)malloc(mov_len);
        if (!tmp_buf || fwrite(tmp_buf, 1, mov_len, file) != mov_len) {
            free(tmp_buf);
            fclose(file);
            unlink(file_name);
            return -1;
        }
        free(tmp_buf);
    }

    tmp_buf = (char *)malloc(len);
    if (!tmp_buf) {
        fclose(file);
        unlink(file_name);
        return -1;
    }
    ssize_t bytes_read = pread(fd, tmp_buf, len, offset);
    if (bytes_read != len || fwrite(tmp_buf, 1, len, file) != len) {
        free(tmp_buf);
        fclose(file);
        unlink(file_name);
        return -1;
    }
    free(tmp_buf);
    /* Reopen the file to refresh it. */
    fclose(file);
    fd = open(file_name, O_RDONLY);
    unlink(file_name);
    return fd;
}

/*
 * NOTE: all the constants are the HOST ones
 * Expand offset type from abi_ulong to uint64 to make mmap2 happy.
 * refer to syscall.c mmap2 syscall for detial information
 */
#ifdef TARGET_X86_64
extern int latx_wine;
void kzt_wine_bridge(abi_ulong start, int fd);
#endif
abi_ulong option_mmap_fixed;
abi_long target_mmap(abi_ulong start, abi_ulong len, int target_prot,
                     int flags, int fd, uint64_t offset, int rlimit_as_account)
{
    abi_ulong ret, end, real_start, real_end, retaddr, host_len;
    int page_flags, temp_flags, host_prot;
    uint64_t host_offset;
    int shadow_fd = -1;

#ifndef TARGET_X86_64
    /* Hacking wine user_shared_data mapping to avoid shadow page */
    if (start == 0x7ffe0000 && len == 0x1000 && (flags == (MAP_FIXED | MAP_SHARED)) && fd > 0
        && (qemu_host_page_size > TARGET_PAGE_SIZE)) {
        len = 0x4000;
    }
#endif
    if (start && option_mmap_fixed)
        flags |= MAP_FIXED;

    mmap_lock();
    trace_target_mmap(start, len, target_prot, flags, fd, offset);

    if (!len) {
        errno = EINVAL;
        goto fail;
    }

    page_flags = validate_prot_to_pageflags(&host_prot, target_prot);
    if (!page_flags) {
        errno = EINVAL;
        goto fail;
    }

    /* Also check for overflows... */
    len = TARGET_PAGE_ALIGN(len);
    if (!len) {
        errno = ENOMEM;
        goto fail;
    }

    if (option_prlimit && rlimit_as_account && (vir_rlimit_as != RLIM_INFINITY)
        && (len + vir_rlimit_as_acc >= vir_rlimit_as)) {
        errno = ENOMEM;
        goto fail;
    }

    if (offset & ~TARGET_PAGE_MASK) {
        errno = EINVAL;
        goto fail;
    }

    /*
     * If we're mapping shared memory, ensure we generate code for parallel
     * execution and flush old translations.  This will work up to the level
     * supported by the host -- anything that requires EXCP_ATOMIC will not
     * be atomic with respect to an external process.
     */
    if (flags & MAP_SHARED) {
        if (option_monitor_shared_mem) {
            page_flags |= PAGE_MEMSHARE;
        }
        CPUState *cpu = thread_cpu;
        if (!(cpu->tcg_cflags & CF_PARALLEL)) {
            cpu->tcg_cflags |= CF_PARALLEL;
            tb_flush(cpu);
        }
    }

    real_start = start & qemu_host_page_mask;
    host_offset = offset & qemu_host_page_mask;

    /* If the user is asking for the kernel to find a location, do that
       before we truncate the length for mapping files below.  */
    if (!(flags & MAP_FIXED)) {
        host_len = len + offset - host_offset;
        host_len = HOST_PAGE_ALIGN(host_len);
        if (flags & MAP_HUGETLB) {
             start = mmap_find_vma(real_start, host_len, TARGET_HUGEPAGE_SIZE);
#ifdef TARGET_X86_64
        } else if (flags & X86_64_MAP_32BIT) {
            start = mmap_find_vma_2g(real_start, host_len, TARGET_PAGE_SIZE);
            flags &= ~X86_64_MAP_32BIT;
#endif
        } else {
             start = mmap_find_vma(real_start, host_len, TARGET_PAGE_SIZE);
        }
        if (start == (abi_ulong)-1) {
            errno = ENOMEM;
            goto fail;
        }
#ifdef TARGET_X86_64
        if (latx_wine && real_start && (start != real_start)) {
            errno = EINVAL;
            goto fail;
        }
#endif
    }


    /* When mapping files into a memory area larger than the file, accesses
       to pages beyond the file size will cause a SIGBUS. 

       For example, if mmaping a file of 100 bytes on a host with 4K pages
       emulating a target with 8K pages, the target expects to be able to
       access the first 8K. But the host will trap us on any access beyond
       4K.  

       When emulating a target with a larger page-size than the hosts, we
       may need to truncate file maps at EOF and add extra anonymous pages
       up to the targets page boundary.  */

    if ((qemu_real_host_page_size < qemu_host_page_size) &&
        !(flags & MAP_ANONYMOUS)) {
        struct stat sb;

       if (fstat (fd, &sb) == -1)
           goto fail;

       /* Are we trying to create a map beyond EOF?.  */
       if (offset + len > sb.st_size) {
           /* If so, truncate the file map at eof aligned with 
              the hosts real pagesize. Additional anonymous maps
              will be created beyond EOF.  */
           len = REAL_HOST_PAGE_ALIGN(sb.st_size - offset);
       }
    }

    if (flags & MAP_ANONYMOUS) {
        page_flags |= PAGE_ANON;
    }

    if (!(flags & MAP_FIXED)) {
        unsigned long host_start;
        void *p;

        host_len = len + offset - host_offset;
        host_len = HOST_PAGE_ALIGN(host_len);

        /* Note: we prefer to control the mapping address. It is
           especially important if qemu_host_page_size >
           qemu_real_host_page_size */
        p = mmap(g2h_untagged(start), host_len, host_prot,
                 flags | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) {
            goto fail;
        }
        /* update start so that it points to the file position at 'offset' */
        host_start = (unsigned long)p;
        if (!(flags & MAP_ANONYMOUS)) {
            p = mmap(g2h_untagged(start), host_len, host_prot,
                     flags | MAP_FIXED, fd, host_offset);
            if (p == MAP_FAILED) {
                munmap(g2h_untagged(start), host_len);
                goto fail;
            }
            host_start += offset - host_offset;
        }
        start = h2g(host_start);
        real_start = start;
        real_end = start + len;
    } else {
        if (start & ~TARGET_PAGE_MASK) {
            errno = EINVAL;
            goto fail;
        }
        end = start + len;
        real_end = HOST_PAGE_ALIGN(end);

        /*
         * Test if requested memory area fits target address space
         * It can fail only on 64-bit host with 32-bit target.
         * On any other target/host host mmap() handles this error correctly.
         */
        if (end < start || !guest_range_valid_untagged(start, len)) {
            errno = ENOMEM;
            goto fail;
        }

        /* worst case: we cannot map the file because the offset is not
           aligned, so we read it */
#ifdef TARGET_X86_64
        if (!(flags & MAP_ANONYMOUS) && ((flags & MAP_TYPE) == MAP_SHARED) &&
            (offset & ~qemu_host_page_mask) != (start & ~qemu_host_page_mask)) {
            create_shadow_page_chunk(start, end, target_prot, flags, fd, offset);
            page_set_flags(start, start + len, page_flags);
            goto the_end1;
        }
#endif
        if (option_shadow_file && !(flags & MAP_ANONYMOUS) &&
            (offset & ~qemu_host_page_mask) != (start & ~qemu_host_page_mask)
            && (len >= 0x4000)) {
            shadow_fd = create_shadow_file(fd, offset, start, len);
            if (shadow_fd != -1) {
                fd = shadow_fd;
                offset = start % qemu_host_page_size;
            }
        }

        if (!(flags & MAP_ANONYMOUS) &&
            (offset & ~qemu_host_page_mask) != (start & ~qemu_host_page_mask)) {
            retaddr = target_mmap(start, len, target_prot | PROT_WRITE,
                                  MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS,
                                  -1, 0, 0);
            if (retaddr == -1)
                goto fail;

            abi_ulong tmp_start = start;
            abi_ulong tmp_end = end;

            if (tmp_start > real_start) {
                if (hostpage_exist_shadow_page(tmp_start)) {
                    for (abi_ulong curr_addr = tmp_start;
                            curr_addr < end && curr_addr < HOST_PAGE_ALIGN(start);
                            curr_addr += TARGET_PAGE_SIZE) {
                        ShadowPageDesc *shadow_pd
                            = page_get_target_data((target_ulong)curr_addr);
                        if (!shadow_pd) {
                            assert(0);
                            break;
                        }
                        uintptr_t shadow_addr = curr_addr + shadow_pd->access_off;
                        if (pread(fd, (void *)(shadow_addr),
                                    TARGET_PAGE_SIZE, offset) == -1) {
                            goto fail;
                        }
                        offset += TARGET_PAGE_SIZE;
                        tmp_start += TARGET_PAGE_SIZE;
                    }
                    if (tmp_start == tmp_end) {
                        goto the_end;
                    }
                }
            }

            if (tmp_end < real_end) {
                if (hostpage_exist_shadow_page(tmp_end)) {
                    uint64_t curr_offset = offset +
                        (tmp_end & qemu_host_page_mask) - tmp_start;
                    for (abi_ulong curr_addr = tmp_end & qemu_host_page_mask;
                            curr_addr < tmp_end;
                            curr_addr += TARGET_PAGE_SIZE) {
                        ShadowPageDesc *shadow_pd
                            = page_get_target_data((target_ulong)curr_addr);
                        if (!shadow_pd) {
                            assert(0);
                            break;
                        }
                        uintptr_t shadow_addr = curr_addr + shadow_pd->access_off;
                        if (pread(fd, (void *)(shadow_addr),
                                    TARGET_PAGE_SIZE, curr_offset) == -1) {
                            goto fail;
                        }
                        curr_offset += TARGET_PAGE_SIZE;
                    }
                    tmp_end = tmp_end & qemu_host_page_mask;
                }
            }

            if (tmp_start < tmp_end) {
#ifdef CONFIG_LATX_DEBUG
                for (abi_ulong curr_addr = tmp_start; curr_addr < tmp_end;
                        curr_addr += qemu_host_page_size) {
                    if (page_get_target_data(curr_addr)) {
                        fprintf(stderr, "Shadow page maybe cause error\n");
                    }
                }
#endif
                len = tmp_end - tmp_start;
                if (pread(fd, g2h_untagged(tmp_start), len, offset) == -1) {
                    goto fail;
                }
                if (!(host_prot & PROT_WRITE)) {
                    ret = target_mprotect(tmp_start, len, target_prot);
                    assert(ret == 0);
                }
            }
            goto the_end;
        }

        /* handle the start of the mapping */
        if (start > real_start) {
            temp_flags = 0;
            if (real_end == real_start + qemu_host_page_size) {
                /* one single host page */
                ret = mmap_frag(real_start, start, end,
                                host_prot, flags, fd, offset);
                if (ret == -1)
                    goto fail;
                if (!ret) {
                    temp_flags = PAGE_RESET;
                }
                page_set_flags(start, start + len, page_flags | temp_flags);
                goto the_end1;
            }
            ret = mmap_frag(real_start, start, real_start + qemu_host_page_size,
                            host_prot, flags, fd, offset);
            if (ret == -1)
                goto fail;
            real_start += qemu_host_page_size;
            if (!ret) {
                temp_flags = PAGE_RESET;
            }
            page_set_flags(start, real_start, page_flags | temp_flags);
        }
        /* handle the end of the mapping */
        if (end < real_end) {
            ret = mmap_frag(real_end - qemu_host_page_size,
                            real_end - qemu_host_page_size, end,
                            host_prot, flags, fd,
                            offset + real_end - qemu_host_page_size - start);
            if (ret == -1)
                goto fail;
            real_end -= qemu_host_page_size;
            temp_flags = 0;
            if (!ret) {
                temp_flags = PAGE_RESET;
            }
            page_set_flags(real_end, end, page_flags | temp_flags);
        }

        /* map the middle (easier) */
        if (real_start < real_end) {
            void *p;
            unsigned long offset1;
            if (flags & MAP_ANONYMOUS)
                offset1 = 0;
            else
                offset1 = offset + real_start - start;
            p = mmap(g2h_untagged(real_start), real_end - real_start,
                     host_prot, flags, fd, offset1);
            if (p == MAP_FAILED)
                goto fail;
        }
    }

    page_flags |= PAGE_RESET;
    if (real_start != real_end) {
        page_set_flags(real_start, real_end, page_flags);
    }
 the_end1:

    /*
     * When mapping files into a memory area larger than the file, accesses
     * to pages beyond the file size will cause a SIGBUS.
     *
     * When emulating a target with a smaller page-size than the hosts, we
     * need to add a PAGE_OVERFLOW prot to the extra pages which is beyond
     * the file size.
     */
    if ((qemu_host_page_size > TARGET_PAGE_SIZE) &&
        !(flags & MAP_ANONYMOUS)) {
        struct stat sb;

        if (fstat(fd, &sb) == -1) {
            goto fail;
        }

        if ((sb.st_size != 0) && (HOST_PAGE_ALIGN(start + sb.st_size - offset) <
            start + len)) {
            page_set_flags(HOST_PAGE_ALIGN(start + sb.st_size - offset),
                           start + len, page_flags | PAGE_OVERFLOW);
        }
    }

 the_end:
    trace_target_mmap_complete(start);
    if (qemu_loglevel_mask(CPU_LOG_PAGE)) {
        log_page_dump(__func__);
    }
#ifdef CONFIG_LATX
#ifndef TARGET_X86_64
    if (fd > 2 && (target_prot & PROT_EXEC)) {
        ht_pc_thunk_invalidate(start, start + len);
    }
#endif
#endif

#ifdef CONFIG_LATX_AOT
    wine_sec_info * wine_sec = NULL;
    uint64_t aot_offset = offset;
    if (option_aot_wine && option_aot) {
        wine_sec = wine_sec_tree_lookup(start);
    }
    if (option_aot && ((fd > 2 && (target_prot & PROT_EXEC)) || wine_sec)) {
        char buf[PATH_MAX];
        aot_offset = deal_seg(wine_sec, aot_offset, buf, fd, 
                target_prot, len, start);
        if (option_load_aot) {
            recover_aot_tb(buf, aot_offset, start, len);
        }
    }
#endif
#if defined(CONFIG_LATX_KZT) && defined(TARGET_X86_64)
    kzt_wine_bridge(start, fd);
#endif
    if (option_prlimit && rlimit_as_account && vir_rlimit_as != RLIM_INFINITY) {
        vir_rlimit_as_acc += len;
    }
    if (shadow_fd != -1) {
        close(shadow_fd);
    }
    mmap_unlock();
    return start;
fail:
    if (shadow_fd != -1) {
        close(shadow_fd);
    }
    mmap_unlock();
    return -1;
}

static void mmap_reserve(abi_ulong start, abi_ulong size)
{
    abi_ulong real_start;
    abi_ulong real_end;
    abi_ulong addr;
    abi_ulong end;
    int prot;

    real_start = start & qemu_host_page_mask;
    real_end = HOST_PAGE_ALIGN(start + size);
    end = start + size;
    if (start > real_start) {
        /* handle host page containing start */
        prot = 0;
        for (addr = real_start; addr < start; addr += TARGET_PAGE_SIZE) {
            prot |= page_get_flags(addr);
        }
        if (real_end == real_start + qemu_host_page_size) {
            for (addr = end; addr < real_end; addr += TARGET_PAGE_SIZE) {
                prot |= page_get_flags(addr);
            }
            end = real_end;
        }
        if (prot != 0)
            real_start += qemu_host_page_size;
    }
    if (end < real_end) {
        prot = 0;
        for (addr = end; addr < real_end; addr += TARGET_PAGE_SIZE) {
            prot |= page_get_flags(addr);
        }
        if (prot != 0)
            real_end -= qemu_host_page_size;
    }
    if (real_start != real_end) {
        mmap(g2h_untagged(real_start), real_end - real_start, PROT_NONE,
                 MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE,
                 -1, 0);
    }
}

int target_munmap(abi_ulong start, abi_ulong len, int rlimit_as_account)
{
    abi_ulong end, real_start, real_end, addr;
    int prot, ret;

    trace_target_munmap(start, len);

    if (start & ~TARGET_PAGE_MASK)
        return -TARGET_EINVAL;
    len = TARGET_PAGE_ALIGN(len);
    if (len == 0 || !guest_range_valid_untagged(start, len)) {
        return -TARGET_EINVAL;
    }

    mmap_lock();
    end = start + len;
    real_start = start & qemu_host_page_mask;
    real_end = HOST_PAGE_ALIGN(end);


    if (start > real_start) {
        /* handle host page containing start */
        prot = 0;
        for(addr = real_start; addr < start; addr += TARGET_PAGE_SIZE) {
            prot |= page_get_flags(addr);
        }
        if (real_end == real_start + qemu_host_page_size) {
            for(addr = end; addr < real_end; addr += TARGET_PAGE_SIZE) {
                prot |= page_get_flags(addr);
            }
            end = real_end;
        }
        if (prot != 0)
            real_start += qemu_host_page_size;
    }
    if (end < real_end) {
        prot = 0;
        for(addr = end; addr < real_end; addr += TARGET_PAGE_SIZE) {
            prot |= page_get_flags(addr);
        }
        if (prot != 0)
            real_end -= qemu_host_page_size;
    }

    ret = 0;
    /* unmap what we can */
    if (real_start < real_end) {
        if (reserved_va) {
            mmap_reserve(real_start, real_end - real_start);
        } else {
            ret = munmap(g2h_untagged(real_start), real_end - real_start);
        }
    }

    if (ret == 0) {
#ifdef CONFIG_LATX_AOT
        if (option_aot) {
            seg_info *seg = segment_tree_lookup2(start, start + len);
            if (seg) {
                segment_tree_remove(seg);
            }
        }
#endif
        page_set_flags(start, start + len, 0);
    }
    mmap_unlock();

    if (option_prlimit && rlimit_as_account && vir_rlimit_as != RLIM_INFINITY) {
        if (vir_rlimit_as_acc > len) {
            vir_rlimit_as_acc -= len;
        } else {
            vir_rlimit_as_acc = 0;
        }
    }
    return ret;
}

abi_long target_mremap(abi_ulong old_addr, abi_ulong old_size,
                       abi_ulong new_size, unsigned long flags,
                       abi_ulong new_addr, int rlimit_as_account)
{
    int prot;
    void *host_addr;

    if (flags & ~(MREMAP_FIXED | MREMAP_MAYMOVE)) {
        errno = EINVAL;
        return -1;
    }

    if (flags & MREMAP_FIXED && !(flags & MREMAP_MAYMOVE)) {
        errno = EINVAL;
        return -1;
    }

    if (old_addr & ~TARGET_PAGE_MASK) {
        errno = EINVAL;
        return -1;
    }

    if (!guest_range_valid_untagged(old_addr, old_size) ||
        ((flags & MREMAP_FIXED) &&
         !guest_range_valid_untagged(new_addr, new_size)) ||
        ((flags & MREMAP_MAYMOVE) == 0 &&
         !guest_range_valid_untagged(old_addr, new_size))) {
        errno = ENOMEM;
        return -1;
    }

    if (option_prlimit && rlimit_as_account && (new_size > old_size)
        && (vir_rlimit_as != RLIM_INFINITY)) {
        abi_ulong diff = new_size - old_size;
        if (diff + vir_rlimit_as_acc > vir_rlimit_as ) {
            errno = ENOMEM;
            return -1;
        }
    }

    mmap_lock();

    if (flags & MREMAP_FIXED) {
        host_addr = mremap(g2h_untagged(old_addr), old_size, new_size,
                           flags, g2h_untagged(new_addr));

        if (reserved_va && host_addr != MAP_FAILED) {
            /* If new and old addresses overlap then the above mremap will
               already have failed with EINVAL.  */
            mmap_reserve(old_addr, old_size);
        }
    } else if (flags & MREMAP_MAYMOVE) {
        abi_ulong mmap_start;

        mmap_start = mmap_find_vma(0, new_size, TARGET_PAGE_SIZE);

        if (mmap_start == -1) {
            errno = ENOMEM;
            host_addr = MAP_FAILED;
        } else {
            host_addr = mremap(g2h_untagged(old_addr), old_size, new_size,
                               flags | MREMAP_FIXED,
                               g2h_untagged(mmap_start));
            if (host_addr == MAP_FAILED)
                errno = EFAULT;
            else {
                if (reserved_va) {
                    mmap_reserve(old_addr, old_size);
                }
            }
        }
    } else {
        int prot = 0;
        int num_pages = 0;
        if (reserved_va && old_size < new_size) {
            abi_ulong addr;
            for (addr = TARGET_PAGE_ALIGN(old_addr + old_size);
                 addr < TARGET_PAGE_ALIGN(old_addr + new_size);
                 addr += TARGET_PAGE_SIZE) {
                prot |= page_get_flags(addr);
                num_pages++;
            }
        }
        if (prot == 0) {
            if (reserved_va && new_size > old_size && num_pages)
                munmap(g2h_untagged(TARGET_PAGE_ALIGN(old_addr + old_size)),
                                        num_pages*TARGET_PAGE_SIZE);
            host_addr = mremap(g2h_untagged(old_addr),
                               old_size, new_size, flags);

            if (host_addr != MAP_FAILED) {
                /* Check if address fits target address space */
                if (!guest_range_valid_untagged(h2g(host_addr), new_size)) {
                    /* Revert mremap() changes */
                    host_addr = mremap(g2h_untagged(old_addr),
                                       new_size, old_size, flags);
                    errno = ENOMEM;
                    host_addr = MAP_FAILED;
                } else if (reserved_va && old_size > new_size) {
                    mmap_reserve(old_addr + old_size, old_size - new_size);
                }
            }
        } else {
            errno = ENOMEM;
            host_addr = MAP_FAILED;
        }
    }

    if (host_addr == MAP_FAILED) {
        new_addr = -1;
    } else {
        new_addr = h2g(host_addr);
        prot = page_get_flags(old_addr);
        page_set_flags(old_addr, old_addr + old_size, 0);
        if (option_monitor_shared_mem && (flags & MAP_TYPE) == MAP_SHARED) {
            prot |= PAGE_MEMSHARE;
        }
        page_set_flags(new_addr, new_addr + new_size,
                       prot | PAGE_VALID | PAGE_RESET);
    }

#ifdef CONFIG_LATX_AOT
    if (option_aot) {
        seg_info *seg = segment_tree_lookup2(old_addr, old_addr + old_size);
        if (seg) {
            segment_tree_remove(seg);
        }
        seg = segment_tree_lookup2(new_addr, new_addr + new_size);
        if (seg) {
            segment_tree_remove(seg);
        }
    }
#endif

    mmap_unlock();

    if (option_prlimit && rlimit_as_account && (new_size != old_size)
        && (vir_rlimit_as != RLIM_INFINITY)) {
        abi_long diff = new_size - old_size;
        if ((diff > 0) || (vir_rlimit_as_acc > -diff)) {
            vir_rlimit_as_acc += diff;
        } else {
            vir_rlimit_as_acc = 0;
        }
    }
    return new_addr;
}

int target_msync(abi_ulong start, abi_ulong len, int flags)
{
    abi_ulong addr;
    int ret;

    ret = msync(g2h_untagged(start & qemu_host_page_mask),
                len + (start & ~qemu_host_page_mask), flags);
    if (ret) {
        return -1;
    }

    for (addr = start; addr < start + len; addr += TARGET_PAGE_SIZE) {
        ShadowPageDesc *shadow_pd = page_get_target_data(addr);
        if (shadow_pd) {
            ret = msync(shadow_pd->p_addr, qemu_host_page_size, flags);
            if (ret) {
                qemu_log_mask(LAT_LOG_MEM, "[LATX_16K] %s failed, addr 0x"
                        TARGET_FMT_lx " shadow_p %p ret %d\n", __func__,
                        addr, shadow_pd ? shadow_pd->p_addr : NULL, ret);
                return -1;
            }
        }
    }
    return 0;
}
