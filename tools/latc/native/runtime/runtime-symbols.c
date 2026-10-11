#define _GNU_SOURCE

#include "runtime-symbols.h"

#include "lat-native-image.h"
#include "x86-linux-user.h"
#include "latx-x86-env-offsets.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static const unsigned char placeholder_pftable[256];
static uint32_t configured_flags;

#if defined(__loongarch__)
typedef union LatXmm128 {
    uint8_t b[16];
    uint16_t w[8];
    uint64_t q[2];
} LatXmm128;

enum {
    X86_CC_C = 0x0001,
    X86_CC_P = 0x0004,
    X86_CC_A = 0x0010,
    X86_CC_Z = 0x0040,
    X86_CC_S = 0x0080,
    X86_CC_O = 0x0800,
};

static int pcmp_ilen(const LatXmm128 *value, uint8_t control)
{
    int length = 0;
    if (control & 1) {
        while (length < 8 && value->w[length]) length++;
    } else {
        while (length < 16 && value->b[length]) length++;
    }
    return length;
}

static int pcmp_value(const LatXmm128 *value, uint8_t control, int index)
{
    switch (control & 3) {
    case 0: return value->b[index];
    case 1: return value->w[index];
    case 2: return (int8_t)value->b[index];
    default: return (int16_t)value->w[index];
    }
}

static unsigned pcmp_result(unsigned char *env, const LatXmm128 *d,
                            const LatXmm128 *s, uint8_t control)
{
    int valid_s = pcmp_ilen(s, control) - 1;
    int valid_d = pcmp_ilen(d, control) - 1;
    int upper = (control & 1) ? 7 : 15;
    unsigned result = 0;
    uint64_t *eflags = (void *)(env + LATC_X86_ENV_EFLAGS_OFFSET);

    if (valid_s < upper) *eflags |= X86_CC_Z;
    else *eflags &= ~X86_CC_Z;
    if (valid_d < upper) *eflags |= X86_CC_S;
    else *eflags &= ~X86_CC_S;

    switch ((control >> 2) & 3) {
    case 0:
        for (int j = valid_s; j >= 0; j--) {
            result <<= 1;
            int value = pcmp_value(s, control, j);
            for (int i = valid_d; i >= 0; i--) {
                result |= value == pcmp_value(d, control, i);
            }
        }
        break;
    case 1:
        for (int j = valid_s; j >= 0; j--) {
            result <<= 1;
            int value = pcmp_value(s, control, j);
            for (int i = ((valid_d - 1) | 1); i >= 0; i -= 2) {
                result |= pcmp_value(d, control, i) >= value &&
                          pcmp_value(d, control, i - 1) <= value;
            }
        }
        break;
    case 2: {
        int maximum = valid_s > valid_d ? valid_s : valid_d;
        int minimum = valid_s < valid_d ? valid_s : valid_d;
        result = (1u << (upper - maximum)) - 1;
        result <<= maximum - minimum;
        for (int i = minimum; i >= 0; i--) {
            result <<= 1;
            result |= pcmp_value(s, control, i) ==
                      pcmp_value(d, control, i);
        }
        break;
    }
    case 3:
        if (valid_d == -1) {
            result = (2u << upper) - 1;
            break;
        }
        for (int j = valid_s == upper ? valid_s : valid_s - valid_d;
             j >= 0; j--) {
            result <<= 1;
            int value = 1;
            int minimum = valid_s - j < valid_d ? valid_s - j : valid_d;
            for (int i = minimum; i >= 0; i--) {
                value &= pcmp_value(s, control, i + j) ==
                         pcmp_value(d, control, i);
            }
            result |= value;
        }
        break;
    }

    switch ((control >> 4) & 3) {
    case 1: result ^= (2u << upper) - 1; break;
    case 3: result ^= (1u << (valid_s + 1)) - 1; break;
    default: break;
    }
    if (result) *eflags |= X86_CC_C;
    else *eflags &= ~X86_CC_C;
    if (result & 1) *eflags |= X86_CC_O;
    else *eflags &= ~X86_CC_O;
    *eflags &= ~(uint64_t)(X86_CC_A | X86_CC_P);
    return result;
}

static void lat_helper_pcmpistri_xmm(unsigned char *env, LatXmm128 *d,
                                     LatXmm128 *s, uint32_t control)
{
    unsigned result = pcmp_result(env, d, s, (uint8_t)control);
    *(uint64_t *)(env + LATC_X86_ENV_RCX_OFFSET) = result ?
        ((control & (1 << 6)) ? 31u - (unsigned)__builtin_clz(result) :
                               (unsigned)__builtin_ctz(result)) :
        16u >> (control & 1);
}

static void lat_helper_pcmpistrm_xmm(unsigned char *env, LatXmm128 *d,
                                     LatXmm128 *s, uint32_t control)
{
    unsigned result = pcmp_result(env, d, s, (uint8_t)control);
    LatXmm128 *xmm0 = (void *)(env + LATC_X86_ENV_XMM0_OFFSET);
    if ((control >> 6) & 1) {
        if (control & 1) {
            for (int i = 0; i < 8; i++, result >>= 1) {
                xmm0->w[i] = (result & 1) ? UINT16_MAX : 0;
            }
        } else {
            for (int i = 0; i < 16; i++, result >>= 1) {
                xmm0->b[i] = (result & 1) ? UINT8_MAX : 0;
            }
        }
    } else {
        xmm0->q[0] = result;
        xmm0->q[1] = 0;
    }
}

static void lat_helper_cpuid(unsigned char *env)
{
    uint64_t *rax = (void *)(env + LATC_X86_ENV_RAX_OFFSET);
    uint64_t *rcx = (void *)(env + LATC_X86_ENV_RCX_OFFSET);
    uint64_t *rdx = (void *)(env + LATC_X86_ENV_RDX_OFFSET);
    uint64_t *rbx = (void *)(env + LATC_X86_ENV_RBX_OFFSET);
    uint32_t leaf = (uint32_t)*rax;
    uint32_t subleaf = (uint32_t)*rcx;
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;

    switch (leaf) {
    case 0:
        eax = 0x0d;
        ebx = 0x68747541; /* AuthenticAMD */
        edx = 0x69746e65;
        ecx = 0x444d4163;
        break;
    case 1:
        eax = 0x00060fb1;
        ebx = 0x00000800;
        ecx = 0x82982203;
        edx = 0x078bfbfd;
        break;
    case 2:
        eax = 0x00000001;
        ecx = 0x0000004d;
        edx = 0x002c307d;
        break;
    case 4: {
        static const uint32_t cache[4][4] = {
            { 0x00000121, 0x01c0003f, 0x0000003f, 0x00000001 },
            { 0x00000122, 0x01c0003f, 0x0000003f, 0x00000001 },
            { 0x00000143, 0x03c0003f, 0x00000fff, 0x00000001 },
            { 0x00000163, 0x03c0003f, 0x00003fff, 0x00000006 },
        };
        if (subleaf < 4) {
            eax = cache[subleaf][0];
            ebx = cache[subleaf][1];
            ecx = cache[subleaf][2];
            edx = cache[subleaf][3];
        }
        break;
    }
    case 0x80000000u:
        eax = 0x8000000au;
        ebx = 0x68747541;
        edx = 0x69746e65;
        ecx = 0x444d4163;
        break;
    case 0x80000001u:
        eax = 0x00060fb1;
        ecx = 0x00000005;
        edx = 0x2193fbfd;
        break;
    case 0x80000005u:
        eax = 0x01ff01ff;
        ebx = 0x01ff01ff;
        ecx = 0x40020140;
        edx = 0x40020140;
        break;
    case 0x80000006u:
        ebx = 0x42004200;
        ecx = 0x02008140;
        edx = 0x00808140;
        break;
    case 0x80000008u:
        eax = 0x00003028;
        break;
    default:
        break;
    }
    *rax = eax;
    *rbx = ebx;
    *rcx = ecx;
    *rdx = edx;
}

static void lat_helper_preserve_fpregs(void)
{
}

static void lat_helper_update_fp_status(unsigned char *env)
{
    (void)env;
}

extern void lat_native_x86_dispatch_jirl(void);
extern void lat_native_x86_dispatch_env_eip(void);
extern void lat_native_x86_syscall(void);

void lat_native_x86_syscall_impl(void)
{
    register unsigned char *env __asm__("$s8");
    lat_x86_linux_user_syscall(env);
}
#endif

#define DEFINE_UNSUPPORTED_RUNTIME_ENTRY(name)                         \
    __attribute__((noreturn)) static void unsupported_##name(void)    \
    {                                                                 \
        dprintf(STDERR_FILENO, "latc: unsupported runtime symbol %s\n", \
                #name);                                               \
        abort();                                                       \
    }

DEFINE_UNSUPPORTED_RUNTIME_ENTRY(EPILOGUE_RET_ID_1)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(EPILOGUE_RET_ID_0)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(JIRL_EPILOGUE_RET_ID_1)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(JIRL_EPILOGUE_RET_ID_0)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(EPILOGUE_RET_0)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(UPDATE_MXCSR_STATUS)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(FXSAVE)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(FXRSTOR)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(FPREGS_X80_TO_64)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(FPREGS_64_TO_X80)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(UPDATE_FP_STATUS)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(CPUID)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(RAISE_ILLOP)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(RAISE_GPF)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(RAISE_SYSCALL)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(PCMPISTRI_XMM)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(PCMPISTRM_XMM)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(EFLAGTF)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(XSAVE)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(XSAVEOPT)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(XRSTOR)
DEFINE_UNSUPPORTED_RUNTIME_ENTRY(XSETBV)

#define RETURN_UNSUPPORTED_RUNTIME_ENTRY(name)             \
    case LAT_NATIVE_SYMBOL_##name:                         \
        return (uintptr_t)unsupported_##name

uintptr_t lat_runtime_symbol_address(uint32_t symbol)
{
    if (symbol == LAT_NATIVE_SYMBOL_PFTABLE) {
        return (uintptr_t)placeholder_pftable;
    }
#if defined(__loongarch__)
    if ((symbol == LAT_NATIVE_SYMBOL_EPILOGUE_RET_ID_1 ||
         symbol == LAT_NATIVE_SYMBOL_EPILOGUE_RET_ID_0 ||
         symbol == LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_1 ||
         symbol == LAT_NATIVE_SYMBOL_JIRL_EPILOGUE_RET_ID_0) &&
        (configured_flags & LAT_NATIVE_IMAGE_X86_STATIC_EXEC)) {
        return (uintptr_t)lat_native_x86_dispatch_env_eip;
    }
    if (symbol == LAT_NATIVE_SYMBOL_EPILOGUE_RET_0 &&
        (configured_flags & LAT_NATIVE_IMAGE_X86_STATIC_EXEC)) {
        return (uintptr_t)lat_native_x86_dispatch_jirl;
    }
    if (symbol == LAT_NATIVE_SYMBOL_RAISE_SYSCALL &&
        (configured_flags & LAT_NATIVE_IMAGE_X86_STATIC_EXEC)) {
        return (uintptr_t)lat_native_x86_syscall;
    }
    if (symbol == LAT_NATIVE_SYMBOL_PCMPISTRI_XMM &&
        (configured_flags & LAT_NATIVE_IMAGE_X86_STATIC_EXEC)) {
        return (uintptr_t)lat_helper_pcmpistri_xmm;
    }
    if (symbol == LAT_NATIVE_SYMBOL_PCMPISTRM_XMM &&
        (configured_flags & LAT_NATIVE_IMAGE_X86_STATIC_EXEC)) {
        return (uintptr_t)lat_helper_pcmpistrm_xmm;
    }
    if (symbol == LAT_NATIVE_SYMBOL_CPUID &&
        (configured_flags & LAT_NATIVE_IMAGE_X86_STATIC_EXEC)) {
        return (uintptr_t)lat_helper_cpuid;
    }
    if ((symbol == LAT_NATIVE_SYMBOL_FPREGS_X80_TO_64 ||
         symbol == LAT_NATIVE_SYMBOL_FPREGS_64_TO_X80) &&
        (configured_flags & LAT_NATIVE_IMAGE_X86_STATIC_EXEC)) {
        return (uintptr_t)lat_helper_preserve_fpregs;
    }
    if (symbol == LAT_NATIVE_SYMBOL_UPDATE_FP_STATUS &&
        (configured_flags & LAT_NATIVE_IMAGE_X86_STATIC_EXEC)) {
        return (uintptr_t)lat_helper_update_fp_status;
    }
    if (symbol == LAT_NATIVE_SYMBOL_UPDATE_MXCSR_STATUS &&
        (configured_flags & LAT_NATIVE_IMAGE_X86_STATIC_EXEC)) {
        return (uintptr_t)lat_helper_update_fp_status;
    }
#endif
    switch (symbol) {
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(EPILOGUE_RET_ID_1);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(EPILOGUE_RET_ID_0);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(JIRL_EPILOGUE_RET_ID_1);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(JIRL_EPILOGUE_RET_ID_0);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(EPILOGUE_RET_0);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(UPDATE_MXCSR_STATUS);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(FXSAVE);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(FXRSTOR);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(FPREGS_X80_TO_64);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(FPREGS_64_TO_X80);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(UPDATE_FP_STATUS);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(CPUID);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(RAISE_ILLOP);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(RAISE_GPF);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(RAISE_SYSCALL);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(PCMPISTRI_XMM);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(PCMPISTRM_XMM);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(EFLAGTF);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(XSAVE);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(XSAVEOPT);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(XRSTOR);
    RETURN_UNSUPPORTED_RUNTIME_ENTRY(XSETBV);
    default:
        break;
    }
    return 0;
}

void lat_runtime_symbols_configure(uint32_t image_flags)
{
    configured_flags = image_flags;
}
