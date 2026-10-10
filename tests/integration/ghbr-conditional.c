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

int main(void)
{
    uint64_t equal_eax;
    uint64_t equal_dest;
    uint64_t unequal_eax;
    uint64_t unequal_dest;

    cmpxchg_equal(0x22222222, &equal_eax, &equal_dest);
    cmpxchg_unequal(0x22222222, &unequal_eax, &unequal_dest);
    printf("cmpxchg=%016" PRIx64 ":%016" PRIx64 ":%016" PRIx64
           ":%016" PRIx64 "\n", equal_eax, equal_dest,
           unequal_eax, unequal_dest);
    return equal_eax != 0xdeadbeef11111111ULL ||
           equal_dest != 0x0000000022222222ULL ||
           unequal_eax != 0x0000000033333333ULL ||
           unequal_dest != 0xfeedface33333333ULL;
}
