#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

static __attribute__((noinline)) void cmpxchg_equal(uint32_t src,
                                                    uint64_t *eax_out,
                                                    uint64_t *dest_out)
{
    uint64_t dest;
    uint64_t eax;

    __asm__ volatile(
        "movabsq $0xdeadbeef11111111, %%rax\n\t"
        "movabsq $0xfeedface11111111, %%rbx\n\t"
        "cmpxchgl %[src], %%ebx\n\t"
        "movq %%rax, %[eax]\n\t"
        "movq %%rbx, %[dest]"
        : [eax] "=r"(eax), [dest] "=r"(dest)
        : [src] "r"(src)
        : "rax", "rbx", "cc");
    *eax_out = eax;
    *dest_out = dest;
}

static __attribute__((noinline)) void cmpxchg_unequal(uint32_t src,
                                                      uint64_t *eax_out,
                                                      uint64_t *dest_out)
{
    uint64_t dest;
    uint64_t eax;

    __asm__ volatile(
        "movabsq $0xdeadbeef11111111, %%rax\n\t"
        "movabsq $0xfeedface33333333, %%rbx\n\t"
        "cmpxchgl %[src], %%ebx\n\t"
        "movq %%rax, %[eax]\n\t"
        "movq %%rbx, %[dest]"
        : [eax] "=r"(eax), [dest] "=r"(dest)
        : [src] "r"(src)
        : "rax", "rbx", "cc");
    *eax_out = eax;
    *dest_out = dest;
}

static __attribute__((noinline)) uint64_t cmovne32(uint32_t condition)
{
    uint64_t result;

    __asm__ volatile(
        "movabsq $0xfeedface87654321, %%rax\n\t"
        "movabsq $0xdeadbeef12345678, %%rcx\n\t"
        "testl %[condition], %[condition]\n\t"
        "cmovnel %%ecx, %%eax"
        : "=&a"(result)
        : [condition] "r"(condition)
        : "rcx", "cc");
    return result;
}

static __attribute__((noinline)) uint64_t cmovne16_false(void)
{
    uint64_t result;

    __asm__ volatile(
        "movabsq $0xfeedface87654321, %%rax\n\t"
        "xorl %%ecx, %%ecx\n\t"
        "cmovnew %%cx, %%ax"
        : "=a"(result) : : "rcx", "cc");
    return result;
}

static __attribute__((noinline)) uint64_t cmovne64_false(void)
{
    uint64_t result;

    __asm__ volatile(
        "movabsq $0xfeedface87654321, %%rax\n\t"
        "xorl %%ecx, %%ecx\n\t"
        "cmovneq %%rcx, %%rax"
        : "=a"(result) : : "rcx", "cc");
    return result;
}

int main(void)
{
    uint64_t equal_eax;
    uint64_t equal_dest;
    uint64_t unequal_eax;
    uint64_t unequal_dest;
    uint64_t cmov_false = cmovne32(0);
    uint64_t cmov_true = cmovne32(1);
    uint64_t cmov16 = cmovne16_false();
    uint64_t cmov64 = cmovne64_false();

    cmpxchg_equal(0x22222222, &equal_eax, &equal_dest);
    cmpxchg_unequal(0x22222222, &unequal_eax, &unequal_dest);
    printf("cmpxchg=%016" PRIx64 ":%016" PRIx64 ":%016" PRIx64
           ":%016" PRIx64 "\n", equal_eax, equal_dest,
           unequal_eax, unequal_dest);
    printf("cmov=%016" PRIx64 ":%016" PRIx64 ":%016" PRIx64
           ":%016" PRIx64 "\n", cmov_false, cmov_true, cmov16, cmov64);
    return equal_eax != 0xdeadbeef11111111ULL ||
           equal_dest != 0x0000000022222222ULL ||
           unequal_eax != 0x0000000033333333ULL ||
           unequal_dest != 0xfeedface33333333ULL ||
           cmov_false != 0x0000000087654321ULL ||
           cmov_true != 0x0000000012345678ULL ||
           cmov16 != 0xfeedface87654321ULL ||
           cmov64 != 0xfeedface87654321ULL;
}
