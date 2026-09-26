#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

struct result128 {
    uint64_t high;
    uint64_t low;
};

static __attribute__((noinline)) struct result128
div64_after_edx_write(uint64_t seed)
{
    struct result128 result;
    uint64_t divisor = 100;

    __asm__ volatile(
        "movq %[seed], %%rdx\n\t"
        "movl %k[seed], %%edx\n\t"
        "movl $0x12345678, %%eax\n\t"
        "jmp 1f\n\t"
        "1:\n\t"
        "divq %[divisor]\n\t"
        : "=&a"(result.low), "=&d"(result.high)
        : [seed] "r"(seed), [divisor] "r"(divisor)
        : "cc");

    return result;
}

static __attribute__((noinline)) struct result128 mul64_implicit_rax(void)
{
    struct result128 result;
    uint64_t multiplier = 0x100000001ULL;

    __asm__ volatile(
        "movabsq $0x123456789abcdef0, %%rax\n\t"
        "movl %k[seed], %%eax\n\t"
        "jmp 1f\n\t"
        "1:\n\t"
        "mulq %[multiplier]\n\t"
        : "=&a"(result.low), "=&d"(result.high)
        : [multiplier] "r"(multiplier),
          [seed] "r"(0xdeadbeef00000003ULL)
        : "cc");

    return result;
}

static __attribute__((noinline)) struct result128 mul64_explicit_source(void)
{
    struct result128 result;

    __asm__ volatile(
        "movabsq $0xdeadbeef00000002, %%rbx\n\t"
        "movl %%ebx, %%ecx\n\t"
        "movl $3, %%eax\n\t"
        "xorl %%edx, %%edx\n\t"
        "mulq %%rcx"
        : "=a"(result.low), "=d"(result.high)
        :
        : "rbx", "rcx", "cc");

    return result;
}

int main(void)
{
    struct result128 div = div64_after_edx_write(0xdeadbeef00000001ULL);
    struct result128 implicit = mul64_implicit_rax();
    struct result128 explicit = mul64_explicit_source();

    printf("div64=%016" PRIx64 ":%016" PRIx64 "\n",
           div.low, div.high);
    printf("mul64=%016" PRIx64 ":%016" PRIx64 "\n",
           implicit.high, implicit.low);
    printf("mul64-source=%016" PRIx64 ":%016" PRIx64 "\n",
           explicit.high, explicit.low);

    return div.low != 0x028f5c28f5f129d3ULL || div.high != 12 ||
           implicit.high != 0 || implicit.low != 0x0000000300000003ULL ||
           explicit.high != 0 || explicit.low != 6;
}
