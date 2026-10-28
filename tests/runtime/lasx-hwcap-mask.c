/* SPDX-License-Identifier: GPL-2.0-only */
/* Test-only: hide LASX in auxv; does not emulate a CPU without LASX. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/auxv.h>
#include <asm/hwcap.h>

unsigned long getauxval(unsigned long type)
{
    unsigned long (*real_getauxval)(unsigned long);
    unsigned long value;
    real_getauxval = dlsym(RTLD_NEXT, "getauxval");
    if (!real_getauxval) {
        fputs("mask-lasx: cannot resolve getauxval\n", stderr);
        _Exit(125);
    }
    value = real_getauxval(type);
    if (type == AT_HWCAP) {
        fprintf(stderr, "mask-lasx: HWCAP %lx -> %lx\n", value,
                value & ~((unsigned long)HWCAP_LOONGARCH_LASX));
        value &= ~((unsigned long)HWCAP_LOONGARCH_LASX);
    }
    return value;
}
