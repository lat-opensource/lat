/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdint.h>
#include <errno.h>
#include <locale.h>

static __thread long tls_value;
static uintptr_t observer;
static int dso_anchor;

long guest_add(long a, long b)
{
    return a + b;
}

long guest_tls_next(void)
{
    return ++tls_value;
}

long guest_recurse(uintptr_t callback, long depth)
{
    return depth ? ((long (*)(long))callback)(depth - 1) + 1 : 0;
}

void guest_set_observer(uintptr_t callback)
{
    observer = callback;
}

void *guest_dso_anchor(void)
{
    return &dso_anchor;
}

void guest_cleanup_one(void)
{
    ((void (*)(long))observer)(1);
}

void guest_cleanup_three(void)
{
    ((void (*)(long))observer)(3);
}

void guest_cleanup_four(void)
{
    ((void (*)(long))observer)(4);
}

void guest_cleanup_argument(void *value)
{
    ((void (*)(long))observer)((long)value);
}

long guest_errno_roundtrip(long expected)
{
    long matches = errno == expected;
    errno = EAGAIN;
    return matches;
}

const char *guest_decimal_point(void)
{
    return localeconv()->decimal_point;
}

static long guest_syscall(long number, long a, long b, long c,
                          long d, long e, long f)
{
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    register long r9 __asm__("r9") = f;
    long result;
    __asm__ volatile("syscall" : "=a"(result)
                     : "a"(number), "D"(a), "S"(b), "d"(c),
                       "r"(r10), "r"(r8), "r"(r9)
                     : "rcx", "r11", "memory");
    return result;
}

long guest_noreplace(uintptr_t address, long size)
{
    return guest_syscall(9, address, size, 3, 0x100022, -1, 0);
}

long guest_fixed(uintptr_t address, long size)
{
    return guest_syscall(9, address, size, 3, 0x32, -1, 0);
}

long guest_unmap(uintptr_t address, long size)
{
    return guest_syscall(11, address, size, 0, 0, 0, 0);
}

long guest_remap_into_host(uintptr_t address, long size)
{
    long source = guest_syscall(9, 0, size, 3, 0x22, -1, 0);
    long result;

    if (source < 0) {
        return -999;
    }
    *(volatile unsigned char *)source = 0x66;
    result = guest_syscall(25, source, size, size, 3, address, 0);
    if (result < 0) {
        if (*(volatile unsigned char *)source != 0x66) {
            result = -998;
        }
        guest_unmap(source, size);
    } else {
        guest_unmap(result, size);
        result = 1;
    }
    return result;
}

long guest_shmat_into_host(uintptr_t address, long size)
{
    long id = guest_syscall(29, 0, size, 0600 | 01000, 0, 0, 0);
    long result;

    if (id < 0) {
        return -997;
    }
    result = guest_syscall(30, id, address, 0x4000, 0, 0, 0);
    guest_syscall(31, id, 0, 0, 0, 0, 0);
    if (result >= 0) {
        guest_syscall(67, result, 0, 0, 0, 0, 0);
        return 1;
    }
    return result;
}

long guest_shm_roundtrip(void)
{
    long id = guest_syscall(29, 0, 16384, 0600 | 01000, 0, 0, 0);
    long address, result;

    if (id < 0) {
        return -997;
    }
    address = guest_syscall(30, id, 0, 0, 0, 0, 0);
    if (address < 0) {
        guest_syscall(31, id, 0, 0, 0, 0, 0);
        return address;
    }
    if (guest_unmap(address, 16384) != 0 ||
        guest_syscall(30, id, address, 0, 0, 0, 0) != address) {
        guest_syscall(31, id, 0, 0, 0, 0, 0);
        return -995;
    }
    guest_syscall(31, id, 0, 0, 0, 0, 0);
    *(volatile unsigned char *)address = 0x44;
    result = *(volatile unsigned char *)address == 0x44;
    return guest_syscall(67, address, 0, 0, 0, 0, 0) == 0 ? result : -996;
}

long guest_shmdt(uintptr_t address)
{
    return guest_syscall(67, address, 0, 0, 0, 0, 0);
}

long guest_shm_subpage(long host_page_size)
{
    long source = guest_syscall(9, 0, host_page_size, 3, 0x22, -1, 0);
    long id, result;

    if (source < 0) {
        return -999;
    }
    *(volatile unsigned char *)(source + 4096) = 0x77;
    id = guest_syscall(29, 0, 4096, 0600 | 01000, 0, 0, 0);
    if (id < 0) {
        guest_unmap(source, host_page_size);
        return -997;
    }
    result = guest_syscall(30, id, source, 0x4000, 0, 0, 0);
    guest_syscall(31, id, 0, 0, 0, 0, 0);
    if (result == -EINVAL &&
        *(volatile unsigned char *)(source + 4096) == 0x77) {
        guest_unmap(source, host_page_size);
        return 1;
    }
    if (result >= 0) {
        guest_syscall(67, result, 0, 0, 0, 0, 0);
    } else {
        guest_unmap(source, host_page_size);
    }
    return -994;
}

long guest_shm_partial(long host_page_size)
{
    long id = guest_syscall(29, 0, host_page_size * 2, 0600 | 01000, 0, 0, 0);
    long first, second, result;

    if (id < 0) {
        return -997;
    }
    first = guest_syscall(30, id, 0, 0, 0, 0, 0);
    if (first < 0) {
        guest_syscall(31, id, 0, 0, 0, 0, 0);
        return first;
    }
    *(volatile unsigned char *)first = 0x33;
    if (guest_unmap(first + host_page_size, host_page_size) != 0) {
        return -993;
    }
    second = guest_syscall(30, id, 0, 0, 0, 0, 0);
    guest_syscall(31, id, 0, 0, 0, 0, 0);
    result = second >= 0 && *(volatile unsigned char *)first == 0x33 &&
             guest_syscall(67, first, 0, 0, 0, 0, 0) == 0;
    if (second >= 0) {
        guest_syscall(67, second, 0, 0, 0, 0, 0);
    }
    return result;
}

long guest_shm_replace(long host_page_size, long variant)
{
    long id = guest_syscall(29, 0, host_page_size * 2, 0600 | 01000, 0, 0, 0);
    long first, replacement, result;
    long flags = (variant & 1) ? 0x31 : 0x32;

    if (id < 0) {
        return -997;
    }
    first = guest_syscall(30, id, 0, 0, 0, 0, 0);
    guest_syscall(31, id, 0, 0, 0, 0, 0);
    if (first < 0) {
        return -996;
    }
    replacement = first + ((variant & 2) ? 0 : host_page_size);
    result = guest_syscall(9, replacement, host_page_size, 3, flags, -1, 0);
    if (result != replacement) {
        guest_syscall(67, first, 0, 0, 0, 0, 0);
        return -995;
    }
    *(volatile unsigned char *)replacement = 0x77;
    if (guest_syscall(67, first, 0, 0, 0, 0, 0) != 0) {
        return -994;
    }
    if (*(volatile unsigned char *)replacement != 0x77) {
        return -993;
    }
    result = guest_noreplace(first + ((variant & 2) ? host_page_size : 0),
                             host_page_size);
    if (result != first + ((variant & 2) ? host_page_size : 0)) {
        return -992;
    }
    guest_unmap(result, host_page_size);
    result = guest_unmap(replacement, host_page_size);
    return result == 0 ? 1 : result;
}
