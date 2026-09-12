/* SPDX-License-Identifier: GPL-2.0-only */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    uint64_t quotient;
    uint64_t remainder;
    uint64_t stale_high = UINT64_C(0x1234567800000000);
    uint64_t divisor = 3;

    __asm__ volatile(
        "mov %[stale], %%rdx\n\t"
        "mov %[divisor], %%rcx\n\t"
        "xor %%edx, %%edx\n\t"
        "mov $100, %%rax\n\t"
        "divq %%rcx"
        : "=&a"(quotient), "=&d"(remainder)
        : [stale] "r"(stale_high), [divisor] "r"(divisor)
        : "cc", "rcx");

    printf("quotient=%" PRIu64 " remainder=%" PRIu64 "\n",
           quotient, remainder);
    return quotient != 33 || remainder != 1;
}
