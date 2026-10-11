/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdint.h>
#include <stdio.h>

#define INTEGER_BRANCH_CASE(name, operation, successor, branch, mask) \
static uint64_t name(uint64_t a, uint64_t b, uint64_t count)             \
{                                                                     \
    uint64_t flags;                                                   \
    __asm__ volatile("clc\n\t" operation "\n\t"                        \
                     branch " 1f\n\t" successor "\n\t"              \
                     "pushfq\n\tpopq %%rdx\n\tjmp 2f\n\t"              \
                     "1:\n\t" successor "\n\t"                        \
                     "pushfq\n\tpopq %%rdx\n\t2:"                      \
                     : "+a"(a), "+b"(b), "+c"(count), "=d"(flags)     \
                     : : "cc", "memory");                             \
    return a ^ (flags & (mask));                                     \
}

#define INTEGER_CASE(name, operation, successor)                      \
    INTEGER_BRANCH_CASE(name, operation, successor, "jz", 0x8c5)

#define CASES(name, operation)                                        \
    INTEGER_CASE(name##_live, operation, "nop")                       \
    INTEGER_CASE(name##_dead, operation, "cmpq %%rcx, %%rcx\n\tcld")

CASES(cmp8, "cmpb %b1, %b0")
CASES(cmp16, "cmpw %w1, %w0")
CASES(cmp32, "cmpl %k1, %k0")
CASES(cmp64, "cmpq %1, %0")
CASES(test8, "testb %b1, %b0")
CASES(test16, "testw %w1, %w0")
CASES(test32, "testl %k1, %k0")
CASES(test64, "testq %1, %0")
CASES(and64, "andq %1, %0")
CASES(or64, "orq %1, %0")
CASES(xor64, "xorq %1, %0")
CASES(add64, "addq %1, %0")
CASES(sub64, "subq %1, %0")
CASES(inc64, "incq %0")
CASES(dec64, "decq %0")
CASES(shl64, "shlq %%cl, %0")
CASES(shr64, "shrq %%cl, %0")
CASES(sar64, "sarq %%cl, %0")
CASES(shli64, "shlq $1, %0")
CASES(shri64, "shrq $1, %0")
CASES(sari64, "sarq $1, %0")
CASES(cmpxx, "cmpq %1, %0\n\tmovq %%rax, %%rax")
CASES(testxx, "testq %1, %0\n\tmovq %%rax, %%rax")

#define BIT_CASES(name, operation)                                    \
    INTEGER_BRANCH_CASE(name##_live, operation, "nop", "jc", 1)      \
    INTEGER_BRANCH_CASE(name##_dead, operation,                       \
                        "cmpq %%rcx, %%rcx\n\tcld", "jc", 1)
BIT_CASES(bt64, "btq %1, %0")
BIT_CASES(btxx, "btq %1, %0\n\tmovq %%rax, %%rax")

#define NONZERO_CASES(name, operation)                                \
    INTEGER_BRANCH_CASE(name##_live, operation, "nop", "jnz", 0x8c5) \
    INTEGER_BRANCH_CASE(name##_dead, operation,                       \
                        "cmpq %%rcx, %%rcx\n\tcld", "jnz", 0x8c5)
NONZERO_CASES(andjne, "andq %1, %0")
NONZERO_CASES(shrjne, "shrq $1, %0")

#define FLOAT_SEQUENCE_CASE(name, operation, middle, successor, type) \
static uint64_t name(type a, type b)                                  \
{                                                                     \
    uint64_t flags;                                                   \
    __asm__ volatile(operation " %2, %1\n\t" middle "\n\tjbe 1f\n\t" \
                     successor "\n\tpushfq\n\tpopq %0\n\tjmp 2f\n\t"   \
                     "1:\n\t" successor "\n\tpushfq\n\tpopq %0\n\t2:"  \
                     : "=r"(flags) : "x"(a), "x"(b)                  \
                     : "rcx", "cc", "memory");                       \
    return flags & 0x8c5;                                            \
}

#define FLOAT_TYPED_CASE(name, operation, successor, type)            \
    FLOAT_SEQUENCE_CASE(name, operation, "", successor, type)

#define FLOAT_CASE(name, operation, successor)                        \
    FLOAT_TYPED_CASE(name, operation, successor, double)

FLOAT_CASE(comisd_live, "comisd", "nop")
FLOAT_CASE(comisd_dead, "comisd", "cmpq %%rcx, %%rcx\n\tcld")
FLOAT_CASE(ucomisd_live, "ucomisd", "nop")
FLOAT_CASE(ucomisd_dead, "ucomisd", "cmpq %%rcx, %%rcx\n\tcld")
FLOAT_TYPED_CASE(comiss_live, "comiss", "nop", float)
FLOAT_TYPED_CASE(comiss_dead, "comiss", "cmpq %%rcx, %%rcx\n\tcld", float)
FLOAT_TYPED_CASE(ucomiss_live, "ucomiss", "nop", float)
FLOAT_TYPED_CASE(ucomiss_dead, "ucomiss", "cmpq %%rcx, %%rcx\n\tcld", float)

#define FLOAT_XX_CASES(name, operation, type)                          \
    FLOAT_SEQUENCE_CASE(name##_live, operation, "movq %%rcx, %%rcx", \
                        "nop", type)                                \
    FLOAT_SEQUENCE_CASE(name##_dead, operation, "movq %%rcx, %%rcx", \
                        "cmpq %%rcx, %%rcx\n\tcld", type)
FLOAT_XX_CASES(comisdxx, "comisd", double)
FLOAT_XX_CASES(ucomisdxx, "ucomisd", double)
FLOAT_XX_CASES(comissxx, "comiss", float)
FLOAT_XX_CASES(ucomissxx, "ucomiss", float)

#define RUN(name) do {                                                \
    uint64_t sum = 0;                                                 \
    for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); i++) { \
        for (unsigned j = 0; j < sizeof(values) / sizeof(values[0]); j++) { \
            sum = sum * 33 + name##_live(values[i], values[j], 1);     \
            sum = sum * 33 + name##_dead(values[i], values[j], 1);     \
        }                                                            \
    }                                                                \
    printf(#name " %016llx\n", (unsigned long long)sum);               \
} while (0)

int main(void)
{
    static const uint64_t values[] = {
        0, 1, 127, 128, 255, 32768, 0x80000000,
        UINT64_C(0x8000000000000000), UINT64_MAX,
    };
    RUN(cmp8); RUN(cmp16); RUN(cmp32); RUN(cmp64);
    RUN(test8); RUN(test16); RUN(test32); RUN(test64);
    RUN(and64); RUN(or64); RUN(xor64); RUN(add64); RUN(sub64);
    RUN(inc64); RUN(dec64); RUN(shl64); RUN(shr64); RUN(sar64);
    RUN(shli64); RUN(shri64); RUN(sari64);
    RUN(cmpxx); RUN(testxx); RUN(bt64); RUN(btxx);
    RUN(andjne); RUN(shrjne);
    for (int i = -2; i <= 2; i++) {
        printf("fp %d %llx %llx %llx %llx\n", i,
               (unsigned long long)comisd_live(i, 0),
               (unsigned long long)comisd_dead(i, 0),
               (unsigned long long)ucomisd_live(i, 0),
               (unsigned long long)ucomisd_dead(i, 0));
        printf("fp32 %d %llx %llx %llx %llx\n", i,
               (unsigned long long)comiss_live(i, 0),
               (unsigned long long)comiss_dead(i, 0),
               (unsigned long long)ucomiss_live(i, 0),
               (unsigned long long)ucomiss_dead(i, 0));
        printf("fpxx %d %llx %llx %llx %llx\n", i,
               (unsigned long long)comisdxx_live(i, 0),
               (unsigned long long)comisdxx_dead(i, 0),
               (unsigned long long)ucomisdxx_live(i, 0),
               (unsigned long long)ucomisdxx_dead(i, 0));
        printf("fp32xx %d %llx %llx %llx %llx\n", i,
               (unsigned long long)comissxx_live(i, 0),
               (unsigned long long)comissxx_dead(i, 0),
               (unsigned long long)ucomissxx_live(i, 0),
               (unsigned long long)ucomissxx_dead(i, 0));
    }
    return 0;
}
