/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef _LATX_NATIVE_ASM_H_
#define _LATX_NATIVE_ASM_H_

static inline void __fld32_native_0(uint64_t *src)  { __asm__ __volatile__ ("fld.s $f0,  %0\n\r"::"m"(*src):); }
static inline void __fld32_native_1(uint64_t *src)  { __asm__ __volatile__ ("fld.s $f1,  %0\n\r"::"m"(*src):); }
static inline void __fld32_native_2(uint64_t *src)  { __asm__ __volatile__ ("fld.s $f2,  %0\n\r"::"m"(*src):); }
static inline void __fld32_native_3(uint64_t *src)  { __asm__ __volatile__ ("fld.s $f3,  %0\n\r"::"m"(*src):); }
static inline void __fld32_native_4(uint64_t *src)  { __asm__ __volatile__ ("fld.s $f4,  %0\n\r"::"m"(*src):); }
static inline void __fld32_native_5(uint64_t *src)  { __asm__ __volatile__ ("fld.s $f5,  %0\n\r"::"m"(*src):); }
static inline void __fld32_native_6(uint64_t *src)  { __asm__ __volatile__ ("fld.s $f6,  %0\n\r"::"m"(*src):); }
static inline void __fld32_native_7(uint64_t *src)  { __asm__ __volatile__ ("fld.s $f7,  %0\n\r"::"m"(*src):); }
static inline void __fld32_native_8(uint64_t *src)  { __asm__ __volatile__ ("fld.s $f8,  %0\n\r"::"m"(*src):); }
static inline void __fld32_native_9(uint64_t *src)  { __asm__ __volatile__ ("fld.s $f9,  %0\n\r"::"m"(*src):); }
static inline void __fld32_native_10(uint64_t *src) { __asm__ __volatile__ ("fld.s $f10, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_11(uint64_t *src) { __asm__ __volatile__ ("fld.s $f11, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_12(uint64_t *src) { __asm__ __volatile__ ("fld.s $f12, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_13(uint64_t *src) { __asm__ __volatile__ ("fld.s $f13, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_14(uint64_t *src) { __asm__ __volatile__ ("fld.s $f14, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_15(uint64_t *src) { __asm__ __volatile__ ("fld.s $f15, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_16(uint64_t *src) { __asm__ __volatile__ ("fld.s $f16, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_17(uint64_t *src) { __asm__ __volatile__ ("fld.s $f17, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_18(uint64_t *src) { __asm__ __volatile__ ("fld.s $f18, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_19(uint64_t *src) { __asm__ __volatile__ ("fld.s $f19, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_20(uint64_t *src) { __asm__ __volatile__ ("fld.s $f20, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_21(uint64_t *src) { __asm__ __volatile__ ("fld.s $f21, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_22(uint64_t *src) { __asm__ __volatile__ ("fld.s $f22, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_23(uint64_t *src) { __asm__ __volatile__ ("fld.s $f23, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_24(uint64_t *src) { __asm__ __volatile__ ("fld.s $f24, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_25(uint64_t *src) { __asm__ __volatile__ ("fld.s $f25, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_26(uint64_t *src) { __asm__ __volatile__ ("fld.s $f26, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_27(uint64_t *src) { __asm__ __volatile__ ("fld.s $f27, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_28(uint64_t *src) { __asm__ __volatile__ ("fld.s $f28, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_29(uint64_t *src) { __asm__ __volatile__ ("fld.s $f29, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_30(uint64_t *src) { __asm__ __volatile__ ("fld.s $f30, %0\n\r"::"m"(*src):); }
static inline void __fld32_native_31(uint64_t *src) { __asm__ __volatile__ ("fld.s $f31, %0\n\r"::"m"(*src):); }

static inline void __fst32_native_0(uint64_t *src)  { __asm__ __volatile__ ("fst.s $f0,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_1(uint64_t *src)  { __asm__ __volatile__ ("fst.s $f1,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_2(uint64_t *src)  { __asm__ __volatile__ ("fst.s $f2,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_3(uint64_t *src)  { __asm__ __volatile__ ("fst.s $f3,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_4(uint64_t *src)  { __asm__ __volatile__ ("fst.s $f4,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_5(uint64_t *src)  { __asm__ __volatile__ ("fst.s $f5,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_6(uint64_t *src)  { __asm__ __volatile__ ("fst.s $f6,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_7(uint64_t *src)  { __asm__ __volatile__ ("fst.s $f7,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_8(uint64_t *src)  { __asm__ __volatile__ ("fst.s $f8,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_9(uint64_t *src)  { __asm__ __volatile__ ("fst.s $f9,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_10(uint64_t *src) { __asm__ __volatile__ ("fst.s $f10, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_11(uint64_t *src) { __asm__ __volatile__ ("fst.s $f11, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_12(uint64_t *src) { __asm__ __volatile__ ("fst.s $f12, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_13(uint64_t *src) { __asm__ __volatile__ ("fst.s $f13, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_14(uint64_t *src) { __asm__ __volatile__ ("fst.s $f14, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_15(uint64_t *src) { __asm__ __volatile__ ("fst.s $f15, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_16(uint64_t *src) { __asm__ __volatile__ ("fst.s $f16, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_17(uint64_t *src) { __asm__ __volatile__ ("fst.s $f17, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_18(uint64_t *src) { __asm__ __volatile__ ("fst.s $f18, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_19(uint64_t *src) { __asm__ __volatile__ ("fst.s $f19, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_20(uint64_t *src) { __asm__ __volatile__ ("fst.s $f20, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_21(uint64_t *src) { __asm__ __volatile__ ("fst.s $f21, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_22(uint64_t *src) { __asm__ __volatile__ ("fst.s $f22, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_23(uint64_t *src) { __asm__ __volatile__ ("fst.s $f23, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_24(uint64_t *src) { __asm__ __volatile__ ("fst.s $f24, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_25(uint64_t *src) { __asm__ __volatile__ ("fst.s $f25, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_26(uint64_t *src) { __asm__ __volatile__ ("fst.s $f26, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_27(uint64_t *src) { __asm__ __volatile__ ("fst.s $f27, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_28(uint64_t *src) { __asm__ __volatile__ ("fst.s $f28, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_29(uint64_t *src) { __asm__ __volatile__ ("fst.s $f29, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_30(uint64_t *src) { __asm__ __volatile__ ("fst.s $f30, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst32_native_31(uint64_t *src) { __asm__ __volatile__ ("fst.s $f31, %0\n\r"::"m"(*src):"memory"); }

static inline void __fld64_native_0(uint64_t *src)  { __asm__ __volatile__ ("fld.d $f0,  %0\n\r"::"m"(*src):); }
static inline void __fld64_native_1(uint64_t *src)  { __asm__ __volatile__ ("fld.d $f1,  %0\n\r"::"m"(*src):); }
static inline void __fld64_native_2(uint64_t *src)  { __asm__ __volatile__ ("fld.d $f2,  %0\n\r"::"m"(*src):); }
static inline void __fld64_native_3(uint64_t *src)  { __asm__ __volatile__ ("fld.d $f3,  %0\n\r"::"m"(*src):); }
static inline void __fld64_native_4(uint64_t *src)  { __asm__ __volatile__ ("fld.d $f4,  %0\n\r"::"m"(*src):); }
static inline void __fld64_native_5(uint64_t *src)  { __asm__ __volatile__ ("fld.d $f5,  %0\n\r"::"m"(*src):); }
static inline void __fld64_native_6(uint64_t *src)  { __asm__ __volatile__ ("fld.d $f6,  %0\n\r"::"m"(*src):); }
static inline void __fld64_native_7(uint64_t *src)  { __asm__ __volatile__ ("fld.d $f7,  %0\n\r"::"m"(*src):); }
static inline void __fld64_native_8(uint64_t *src)  { __asm__ __volatile__ ("fld.d $f8,  %0\n\r"::"m"(*src):); }
static inline void __fld64_native_9(uint64_t *src)  { __asm__ __volatile__ ("fld.d $f9,  %0\n\r"::"m"(*src):); }
static inline void __fld64_native_10(uint64_t *src) { __asm__ __volatile__ ("fld.d $f10, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_11(uint64_t *src) { __asm__ __volatile__ ("fld.d $f11, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_12(uint64_t *src) { __asm__ __volatile__ ("fld.d $f12, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_13(uint64_t *src) { __asm__ __volatile__ ("fld.d $f13, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_14(uint64_t *src) { __asm__ __volatile__ ("fld.d $f14, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_15(uint64_t *src) { __asm__ __volatile__ ("fld.d $f15, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_16(uint64_t *src) { __asm__ __volatile__ ("fld.d $f16, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_17(uint64_t *src) { __asm__ __volatile__ ("fld.d $f17, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_18(uint64_t *src) { __asm__ __volatile__ ("fld.d $f18, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_19(uint64_t *src) { __asm__ __volatile__ ("fld.d $f19, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_20(uint64_t *src) { __asm__ __volatile__ ("fld.d $f20, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_21(uint64_t *src) { __asm__ __volatile__ ("fld.d $f21, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_22(uint64_t *src) { __asm__ __volatile__ ("fld.d $f22, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_23(uint64_t *src) { __asm__ __volatile__ ("fld.d $f23, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_24(uint64_t *src) { __asm__ __volatile__ ("fld.d $f24, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_25(uint64_t *src) { __asm__ __volatile__ ("fld.d $f25, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_26(uint64_t *src) { __asm__ __volatile__ ("fld.d $f26, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_27(uint64_t *src) { __asm__ __volatile__ ("fld.d $f27, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_28(uint64_t *src) { __asm__ __volatile__ ("fld.d $f28, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_29(uint64_t *src) { __asm__ __volatile__ ("fld.d $f29, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_30(uint64_t *src) { __asm__ __volatile__ ("fld.d $f30, %0\n\r"::"m"(*src):); }
static inline void __fld64_native_31(uint64_t *src) { __asm__ __volatile__ ("fld.d $f31, %0\n\r"::"m"(*src):); }

static inline void __fst64_native_0(uint64_t *src)  { __asm__ __volatile__ ("fst.d $f0,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_1(uint64_t *src)  { __asm__ __volatile__ ("fst.d $f1,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_2(uint64_t *src)  { __asm__ __volatile__ ("fst.d $f2,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_3(uint64_t *src)  { __asm__ __volatile__ ("fst.d $f3,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_4(uint64_t *src)  { __asm__ __volatile__ ("fst.d $f4,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_5(uint64_t *src)  { __asm__ __volatile__ ("fst.d $f5,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_6(uint64_t *src)  { __asm__ __volatile__ ("fst.d $f6,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_7(uint64_t *src)  { __asm__ __volatile__ ("fst.d $f7,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_8(uint64_t *src)  { __asm__ __volatile__ ("fst.d $f8,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_9(uint64_t *src)  { __asm__ __volatile__ ("fst.d $f9,  %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_10(uint64_t *src) { __asm__ __volatile__ ("fst.d $f10, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_11(uint64_t *src) { __asm__ __volatile__ ("fst.d $f11, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_12(uint64_t *src) { __asm__ __volatile__ ("fst.d $f12, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_13(uint64_t *src) { __asm__ __volatile__ ("fst.d $f13, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_14(uint64_t *src) { __asm__ __volatile__ ("fst.d $f14, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_15(uint64_t *src) { __asm__ __volatile__ ("fst.d $f15, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_16(uint64_t *src) { __asm__ __volatile__ ("fst.d $f16, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_17(uint64_t *src) { __asm__ __volatile__ ("fst.d $f17, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_18(uint64_t *src) { __asm__ __volatile__ ("fst.d $f18, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_19(uint64_t *src) { __asm__ __volatile__ ("fst.d $f19, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_20(uint64_t *src) { __asm__ __volatile__ ("fst.d $f20, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_21(uint64_t *src) { __asm__ __volatile__ ("fst.d $f21, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_22(uint64_t *src) { __asm__ __volatile__ ("fst.d $f22, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_23(uint64_t *src) { __asm__ __volatile__ ("fst.d $f23, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_24(uint64_t *src) { __asm__ __volatile__ ("fst.d $f24, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_25(uint64_t *src) { __asm__ __volatile__ ("fst.d $f25, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_26(uint64_t *src) { __asm__ __volatile__ ("fst.d $f26, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_27(uint64_t *src) { __asm__ __volatile__ ("fst.d $f27, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_28(uint64_t *src) { __asm__ __volatile__ ("fst.d $f28, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_29(uint64_t *src) { __asm__ __volatile__ ("fst.d $f29, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_30(uint64_t *src) { __asm__ __volatile__ ("fst.d $f30, %0\n\r"::"m"(*src):"memory"); }
static inline void __fst64_native_31(uint64_t *src) { __asm__ __volatile__ ("fst.d $f31, %0\n\r"::"m"(*src):"memory"); }

static inline void __fld32_native_freg(uint64_t *src, int freg)
{
    switch (freg) {
    case 0:  __fld32_native_0(src);  break;
    case 1:  __fld32_native_1(src);  break;
    case 2:  __fld32_native_2(src);  break;
    case 3:  __fld32_native_3(src);  break;
    case 4:  __fld32_native_4(src);  break;
    case 5:  __fld32_native_5(src);  break;
    case 6:  __fld32_native_6(src);  break;
    case 7:  __fld32_native_7(src);  break;
    case 8:  __fld32_native_8(src);  break;
    case 9:  __fld32_native_9(src);  break;
    case 10: __fld32_native_10(src); break;
    case 11: __fld32_native_11(src); break;
    case 12: __fld32_native_12(src); break;
    case 13: __fld32_native_13(src); break;
    case 14: __fld32_native_14(src); break;
    case 15: __fld32_native_15(src); break;
    case 16: __fld32_native_16(src); break;
    case 17: __fld32_native_17(src); break;
    case 18: __fld32_native_18(src); break;
    case 19: __fld32_native_19(src); break;
    case 20: __fld32_native_20(src); break;
    case 21: __fld32_native_21(src); break;
    case 22: __fld32_native_22(src); break;
    case 23: __fld32_native_23(src); break;
    case 24: __fld32_native_24(src); break;
    case 25: __fld32_native_25(src); break;
    case 26: __fld32_native_26(src); break;
    case 27: __fld32_native_27(src); break;
    case 28: __fld32_native_28(src); break;
    case 29: __fld32_native_29(src); break;
    case 30: __fld32_native_30(src); break;
    case 31: __fld32_native_31(src); break;
    default: break;
    }
}

static inline void __fst32_native_freg(uint64_t *src, int freg)
{
    switch (freg) {
    case 0:   __fst32_native_0(src); break;
    case 1:   __fst32_native_1(src); break;
    case 2:   __fst32_native_2(src); break;
    case 3:   __fst32_native_3(src); break;
    case 4:   __fst32_native_4(src); break;
    case 5:   __fst32_native_5(src); break;
    case 6:   __fst32_native_6(src); break;
    case 7:   __fst32_native_7(src); break;
    case 8:   __fst32_native_8(src); break;
    case 9:   __fst32_native_9(src); break;
    case 10: __fst32_native_10(src); break;
    case 11: __fst32_native_11(src); break;
    case 12: __fst32_native_12(src); break;
    case 13: __fst32_native_13(src); break;
    case 14: __fst32_native_14(src); break;
    case 15: __fst32_native_15(src); break;
    case 16: __fst32_native_16(src); break;
    case 17: __fst32_native_17(src); break;
    case 18: __fst32_native_18(src); break;
    case 19: __fst32_native_19(src); break;
    case 20: __fst32_native_20(src); break;
    case 21: __fst32_native_21(src); break;
    case 22: __fst32_native_22(src); break;
    case 23: __fst32_native_23(src); break;
    case 24: __fst32_native_24(src); break;
    case 25: __fst32_native_25(src); break;
    case 26: __fst32_native_26(src); break;
    case 27: __fst32_native_27(src); break;
    case 28: __fst32_native_28(src); break;
    case 29: __fst32_native_29(src); break;
    case 30: __fst32_native_30(src); break;
    case 31: __fst32_native_31(src); break;
    default: break;
    }
}

static inline void __fld64_native_freg(uint64_t *src, int freg)
{
    switch (freg) {
    case 0:  __fld64_native_0(src);  break;
    case 1:  __fld64_native_1(src);  break;
    case 2:  __fld64_native_2(src);  break;
    case 3:  __fld64_native_3(src);  break;
    case 4:  __fld64_native_4(src);  break;
    case 5:  __fld64_native_5(src);  break;
    case 6:  __fld64_native_6(src);  break;
    case 7:  __fld64_native_7(src);  break;
    case 8:  __fld64_native_8(src);  break;
    case 9:  __fld64_native_9(src);  break;
    case 10: __fld64_native_10(src); break;
    case 11: __fld64_native_11(src); break;
    case 12: __fld64_native_12(src); break;
    case 13: __fld64_native_13(src); break;
    case 14: __fld64_native_14(src); break;
    case 15: __fld64_native_15(src); break;
    case 16: __fld64_native_16(src); break;
    case 17: __fld64_native_17(src); break;
    case 18: __fld64_native_18(src); break;
    case 19: __fld64_native_19(src); break;
    case 20: __fld64_native_20(src); break;
    case 21: __fld64_native_21(src); break;
    case 22: __fld64_native_22(src); break;
    case 23: __fld64_native_23(src); break;
    case 24: __fld64_native_24(src); break;
    case 25: __fld64_native_25(src); break;
    case 26: __fld64_native_26(src); break;
    case 27: __fld64_native_27(src); break;
    case 28: __fld64_native_28(src); break;
    case 29: __fld64_native_29(src); break;
    case 30: __fld64_native_30(src); break;
    case 31: __fld64_native_31(src); break;
    default: break;
    }
}

static inline void __fst64_native_freg(uint64_t *src, int freg)
{
    switch (freg) {
    case 0:  __fst64_native_0(src);  break;
    case 1:  __fst64_native_1(src);  break;
    case 2:  __fst64_native_2(src);  break;
    case 3:  __fst64_native_3(src);  break;
    case 4:  __fst64_native_4(src);  break;
    case 5:  __fst64_native_5(src);  break;
    case 6:  __fst64_native_6(src);  break;
    case 7:  __fst64_native_7(src);  break;
    case 8:  __fst64_native_8(src);  break;
    case 9:  __fst64_native_9(src);  break;
    case 10: __fst64_native_10(src); break;
    case 11: __fst64_native_11(src); break;
    case 12: __fst64_native_12(src); break;
    case 13: __fst64_native_13(src); break;
    case 14: __fst64_native_14(src); break;
    case 15: __fst64_native_15(src); break;
    case 16: __fst64_native_16(src); break;
    case 17: __fst64_native_17(src); break;
    case 18: __fst64_native_18(src); break;
    case 19: __fst64_native_19(src); break;
    case 20: __fst64_native_20(src); break;
    case 21: __fst64_native_21(src); break;
    case 22: __fst64_native_22(src); break;
    case 23: __fst64_native_23(src); break;
    case 24: __fst64_native_24(src); break;
    case 25: __fst64_native_25(src); break;
    case 26: __fst64_native_26(src); break;
    case 27: __fst64_native_27(src); break;
    case 28: __fst64_native_28(src); break;
    case 29: __fst64_native_29(src); break;
    case 30: __fst64_native_30(src); break;
    case 31: __fst64_native_31(src); break;
    default: break;
    }
}

static inline void __movgr2fr32_native_0(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.w $f0,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_1(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.w $f1,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_2(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.w $f2,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_3(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.w $f3,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_4(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.w $f4,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_5(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.w $f5,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_6(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.w $f6,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_7(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.w $f7,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_8(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.w $f8,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_9(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.w $f9,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_10(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f10, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_11(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f11, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_12(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f12, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_13(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f13, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_14(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f14, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_15(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f15, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_16(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f16, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_17(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f17, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_18(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f18, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_19(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f19, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_20(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f20, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_21(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f21, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_22(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f22, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_23(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f23, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_24(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f24, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_25(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f25, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_26(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f26, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_27(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f27, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_28(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f28, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_29(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f29, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_30(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f30, %0\n\r"::"r"(src):); }
static inline void __movgr2fr32_native_31(uint64_t src) { __asm__ __volatile__ ("movgr2fr.w $f31, %0\n\r"::"r"(src):); }

static inline int32_t __movfr2gr32_native_0(void)  { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f0 \n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_1(void)  { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f1 \n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_2(void)  { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f2 \n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_3(void)  { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f3 \n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_4(void)  { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f4 \n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_5(void)  { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f5 \n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_6(void)  { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f6 \n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_7(void)  { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f7 \n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_8(void)  { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f8 \n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_9(void)  { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f9 \n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_10(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f10\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_11(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f11\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_12(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f12\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_13(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f13\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_14(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f14\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_15(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f15\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_16(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f16\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_17(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f17\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_18(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f18\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_19(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f19\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_20(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f20\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_21(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f21\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_22(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f22\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_23(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f23\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_24(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f24\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_25(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f25\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_26(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f26\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_27(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f27\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_28(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f28\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_29(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f29\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_30(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f30\n\r":"=r"(val)::); return val; }
static inline int32_t __movfr2gr32_native_31(void) { int32_t val; __asm__ __volatile__ ("movfr2gr.s %0, $f31\n\r":"=r"(val)::); return val; }

static inline void __movgr2fr64_native_0(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.d $f0,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_1(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.d $f1,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_2(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.d $f2,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_3(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.d $f3,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_4(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.d $f4,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_5(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.d $f5,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_6(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.d $f6,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_7(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.d $f7,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_8(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.d $f8,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_9(uint64_t src)  { __asm__ __volatile__ ("movgr2fr.d $f9,  %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_10(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f10, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_11(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f11, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_12(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f12, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_13(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f13, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_14(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f14, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_15(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f15, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_16(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f16, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_17(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f17, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_18(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f18, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_19(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f19, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_20(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f20, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_21(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f21, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_22(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f22, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_23(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f23, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_24(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f24, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_25(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f25, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_26(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f26, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_27(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f27, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_28(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f28, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_29(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f29, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_30(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f30, %0\n\r"::"r"(src):); }
static inline void __movgr2fr64_native_31(uint64_t src) { __asm__ __volatile__ ("movgr2fr.d $f31, %0\n\r"::"r"(src):); }

static inline uint64_t __movfr2gr64_native_0(void)  { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f0 \n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_1(void)  { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f1 \n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_2(void)  { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f2 \n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_3(void)  { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f3 \n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_4(void)  { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f4 \n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_5(void)  { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f5 \n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_6(void)  { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f6 \n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_7(void)  { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f7 \n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_8(void)  { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f8 \n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_9(void)  { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f9 \n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_10(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f10\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_11(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f11\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_12(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f12\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_13(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f13\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_14(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f14\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_15(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f15\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_16(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f16\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_17(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f17\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_18(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f18\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_19(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f19\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_20(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f20\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_21(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f21\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_22(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f22\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_23(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f23\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_24(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f24\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_25(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f25\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_26(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f26\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_27(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f27\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_28(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f28\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_29(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f29\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_30(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f30\n\r":"=r"(val)::); return val; }
static inline uint64_t __movfr2gr64_native_31(void) { uint64_t val; __asm__ __volatile__ ("movfr2gr.d %0, $f31\n\r":"=r"(val)::); return val; }

static inline void __movgr2fr32_native_freg(uint64_t src, int freg)
{
    switch (freg) {
    case 0:  __movgr2fr32_native_0(src);  break;
    case 1:  __movgr2fr32_native_1(src);  break;
    case 2:  __movgr2fr32_native_2(src);  break;
    case 3:  __movgr2fr32_native_3(src);  break;
    case 4:  __movgr2fr32_native_4(src);  break;
    case 5:  __movgr2fr32_native_5(src);  break;
    case 6:  __movgr2fr32_native_6(src);  break;
    case 7:  __movgr2fr32_native_7(src);  break;
    case 8:  __movgr2fr32_native_8(src);  break;
    case 9:  __movgr2fr32_native_9(src);  break;
    case 10: __movgr2fr32_native_10(src); break;
    case 11: __movgr2fr32_native_11(src); break;
    case 12: __movgr2fr32_native_12(src); break;
    case 13: __movgr2fr32_native_13(src); break;
    case 14: __movgr2fr32_native_14(src); break;
    case 15: __movgr2fr32_native_15(src); break;
    case 16: __movgr2fr32_native_16(src); break;
    case 17: __movgr2fr32_native_17(src); break;
    case 18: __movgr2fr32_native_18(src); break;
    case 19: __movgr2fr32_native_19(src); break;
    case 20: __movgr2fr32_native_20(src); break;
    case 21: __movgr2fr32_native_21(src); break;
    case 22: __movgr2fr32_native_22(src); break;
    case 23: __movgr2fr32_native_23(src); break;
    case 24: __movgr2fr32_native_24(src); break;
    case 25: __movgr2fr32_native_25(src); break;
    case 26: __movgr2fr32_native_26(src); break;
    case 27: __movgr2fr32_native_27(src); break;
    case 28: __movgr2fr32_native_28(src); break;
    case 29: __movgr2fr32_native_29(src); break;
    case 30: __movgr2fr32_native_30(src); break;
    case 31: __movgr2fr32_native_31(src); break;
    default: break;
    }
}

static inline int32_t __movfr2gr32_native_freg(int freg)
{
    int32_t val = 0;
    switch (freg) {
    case 0:  val = __movfr2gr32_native_0();  break;
    case 1:  val = __movfr2gr32_native_1();  break;
    case 2:  val = __movfr2gr32_native_2();  break;
    case 3:  val = __movfr2gr32_native_3();  break;
    case 4:  val = __movfr2gr32_native_4();  break;
    case 5:  val = __movfr2gr32_native_5();  break;
    case 6:  val = __movfr2gr32_native_6();  break;
    case 7:  val = __movfr2gr32_native_7();  break;
    case 8:  val = __movfr2gr32_native_8();  break;
    case 9:  val = __movfr2gr32_native_9();  break;
    case 10: val = __movfr2gr32_native_10(); break;
    case 11: val = __movfr2gr32_native_11(); break;
    case 12: val = __movfr2gr32_native_12(); break;
    case 13: val = __movfr2gr32_native_13(); break;
    case 14: val = __movfr2gr32_native_14(); break;
    case 15: val = __movfr2gr32_native_15(); break;
    case 16: val = __movfr2gr32_native_16(); break;
    case 17: val = __movfr2gr32_native_17(); break;
    case 18: val = __movfr2gr32_native_18(); break;
    case 19: val = __movfr2gr32_native_19(); break;
    case 20: val = __movfr2gr32_native_20(); break;
    case 21: val = __movfr2gr32_native_21(); break;
    case 22: val = __movfr2gr32_native_22(); break;
    case 23: val = __movfr2gr32_native_23(); break;
    case 24: val = __movfr2gr32_native_24(); break;
    case 25: val = __movfr2gr32_native_25(); break;
    case 26: val = __movfr2gr32_native_26(); break;
    case 27: val = __movfr2gr32_native_27(); break;
    case 28: val = __movfr2gr32_native_28(); break;
    case 29: val = __movfr2gr32_native_29(); break;
    case 30: val = __movfr2gr32_native_30(); break;
    case 31: val = __movfr2gr32_native_31(); break;
    default: break;
    }
    return val;
}

static inline void __movgr2fr64_native_freg(uint64_t src, int freg)
{
    switch (freg) {
    case 0:  __movgr2fr64_native_0(src);  break;
    case 1:  __movgr2fr64_native_1(src);  break;
    case 2:  __movgr2fr64_native_2(src);  break;
    case 3:  __movgr2fr64_native_3(src);  break;
    case 4:  __movgr2fr64_native_4(src);  break;
    case 5:  __movgr2fr64_native_5(src);  break;
    case 6:  __movgr2fr64_native_6(src);  break;
    case 7:  __movgr2fr64_native_7(src);  break;
    case 8:  __movgr2fr64_native_8(src);  break;
    case 9:  __movgr2fr64_native_9(src);  break;
    case 10: __movgr2fr64_native_10(src); break;
    case 11: __movgr2fr64_native_11(src); break;
    case 12: __movgr2fr64_native_12(src); break;
    case 13: __movgr2fr64_native_13(src); break;
    case 14: __movgr2fr64_native_14(src); break;
    case 15: __movgr2fr64_native_15(src); break;
    case 16: __movgr2fr64_native_16(src); break;
    case 17: __movgr2fr64_native_17(src); break;
    case 18: __movgr2fr64_native_18(src); break;
    case 19: __movgr2fr64_native_19(src); break;
    case 20: __movgr2fr64_native_20(src); break;
    case 21: __movgr2fr64_native_21(src); break;
    case 22: __movgr2fr64_native_22(src); break;
    case 23: __movgr2fr64_native_23(src); break;
    case 24: __movgr2fr64_native_24(src); break;
    case 25: __movgr2fr64_native_25(src); break;
    case 26: __movgr2fr64_native_26(src); break;
    case 27: __movgr2fr64_native_27(src); break;
    case 28: __movgr2fr64_native_28(src); break;
    case 29: __movgr2fr64_native_29(src); break;
    case 30: __movgr2fr64_native_30(src); break;
    case 31: __movgr2fr64_native_31(src); break;
    default: break;
    }
}

static inline uint64_t __movfr2gr64_native_freg(int freg)
{
    uint64_t val = 0;
    switch (freg) {
    case 0:  val = __movfr2gr64_native_0();  break;
    case 1:  val = __movfr2gr64_native_1();  break;
    case 2:  val = __movfr2gr64_native_2();  break;
    case 3:  val = __movfr2gr64_native_3();  break;
    case 4:  val = __movfr2gr64_native_4();  break;
    case 5:  val = __movfr2gr64_native_5();  break;
    case 6:  val = __movfr2gr64_native_6();  break;
    case 7:  val = __movfr2gr64_native_7();  break;
    case 8:  val = __movfr2gr64_native_8();  break;
    case 9:  val = __movfr2gr64_native_9();  break;
    case 10: val = __movfr2gr64_native_10(); break;
    case 11: val = __movfr2gr64_native_11(); break;
    case 12: val = __movfr2gr64_native_12(); break;
    case 13: val = __movfr2gr64_native_13(); break;
    case 14: val = __movfr2gr64_native_14(); break;
    case 15: val = __movfr2gr64_native_15(); break;
    case 16: val = __movfr2gr64_native_16(); break;
    case 17: val = __movfr2gr64_native_17(); break;
    case 18: val = __movfr2gr64_native_18(); break;
    case 19: val = __movfr2gr64_native_19(); break;
    case 20: val = __movfr2gr64_native_20(); break;
    case 21: val = __movfr2gr64_native_21(); break;
    case 22: val = __movfr2gr64_native_22(); break;
    case 23: val = __movfr2gr64_native_23(); break;
    case 24: val = __movfr2gr64_native_24(); break;
    case 25: val = __movfr2gr64_native_25(); break;
    case 26: val = __movfr2gr64_native_26(); break;
    case 27: val = __movfr2gr64_native_27(); break;
    case 28: val = __movfr2gr64_native_28(); break;
    case 29: val = __movfr2gr64_native_29(); break;
    case 30: val = __movfr2gr64_native_30(); break;
    case 31: val = __movfr2gr64_native_31(); break;
    default: break;
    }
    return val;
}

static inline void __vld128_native_0(unsigned char *src)  { __asm__ __volatile__ ("vld $vr0 , %0\n\r"::"m"(*src):); }
static inline void __vld128_native_1(unsigned char *src)  { __asm__ __volatile__ ("vld $vr1 , %0\n\r"::"m"(*src):); }
static inline void __vld128_native_2(unsigned char *src)  { __asm__ __volatile__ ("vld $vr2 , %0\n\r"::"m"(*src):); }
static inline void __vld128_native_3(unsigned char *src)  { __asm__ __volatile__ ("vld $vr3 , %0\n\r"::"m"(*src):); }
static inline void __vld128_native_4(unsigned char *src)  { __asm__ __volatile__ ("vld $vr4 , %0\n\r"::"m"(*src):); }
static inline void __vld128_native_5(unsigned char *src)  { __asm__ __volatile__ ("vld $vr5 , %0\n\r"::"m"(*src):); }
static inline void __vld128_native_6(unsigned char *src)  { __asm__ __volatile__ ("vld $vr6 , %0\n\r"::"m"(*src):); }
static inline void __vld128_native_7(unsigned char *src)  { __asm__ __volatile__ ("vld $vr7 , %0\n\r"::"m"(*src):); }
static inline void __vld128_native_8(unsigned char *src)  { __asm__ __volatile__ ("vld $vr8 , %0\n\r"::"m"(*src):); }
static inline void __vld128_native_9(unsigned char *src)  { __asm__ __volatile__ ("vld $vr9 , %0\n\r"::"m"(*src):); }
static inline void __vld128_native_10(unsigned char *src) { __asm__ __volatile__ ("vld $vr10, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_11(unsigned char *src) { __asm__ __volatile__ ("vld $vr11, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_12(unsigned char *src) { __asm__ __volatile__ ("vld $vr12, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_13(unsigned char *src) { __asm__ __volatile__ ("vld $vr13, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_14(unsigned char *src) { __asm__ __volatile__ ("vld $vr14, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_15(unsigned char *src) { __asm__ __volatile__ ("vld $vr15, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_16(unsigned char *src) { __asm__ __volatile__ ("vld $vr16, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_17(unsigned char *src) { __asm__ __volatile__ ("vld $vr17, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_18(unsigned char *src) { __asm__ __volatile__ ("vld $vr18, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_19(unsigned char *src) { __asm__ __volatile__ ("vld $vr19, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_20(unsigned char *src) { __asm__ __volatile__ ("vld $vr20, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_21(unsigned char *src) { __asm__ __volatile__ ("vld $vr21, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_22(unsigned char *src) { __asm__ __volatile__ ("vld $vr22, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_23(unsigned char *src) { __asm__ __volatile__ ("vld $vr23, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_24(unsigned char *src) { __asm__ __volatile__ ("vld $vr24, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_25(unsigned char *src) { __asm__ __volatile__ ("vld $vr25, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_26(unsigned char *src) { __asm__ __volatile__ ("vld $vr26, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_27(unsigned char *src) { __asm__ __volatile__ ("vld $vr27, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_28(unsigned char *src) { __asm__ __volatile__ ("vld $vr28, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_29(unsigned char *src) { __asm__ __volatile__ ("vld $vr29, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_30(unsigned char *src) { __asm__ __volatile__ ("vld $vr30, %0\n\r"::"m"(*src):); }
static inline void __vld128_native_31(unsigned char *src) { __asm__ __volatile__ ("vld $vr31, %0\n\r"::"m"(*src):); }

static inline void __vst128_native_0(unsigned char *res)  { __asm__ __volatile__ ("vst $vr0,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_1(unsigned char *res)  { __asm__ __volatile__ ("vst $vr1,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_2(unsigned char *res)  { __asm__ __volatile__ ("vst $vr2,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_3(unsigned char *res)  { __asm__ __volatile__ ("vst $vr3,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_4(unsigned char *res)  { __asm__ __volatile__ ("vst $vr4,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_5(unsigned char *res)  { __asm__ __volatile__ ("vst $vr5,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_6(unsigned char *res)  { __asm__ __volatile__ ("vst $vr6,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_7(unsigned char *res)  { __asm__ __volatile__ ("vst $vr7,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_8(unsigned char *res)  { __asm__ __volatile__ ("vst $vr8,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_9(unsigned char *res)  { __asm__ __volatile__ ("vst $vr9,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_10(unsigned char *res) { __asm__ __volatile__ ("vst $vr10, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_11(unsigned char *res) { __asm__ __volatile__ ("vst $vr11, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_12(unsigned char *res) { __asm__ __volatile__ ("vst $vr12, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_13(unsigned char *res) { __asm__ __volatile__ ("vst $vr13, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_14(unsigned char *res) { __asm__ __volatile__ ("vst $vr14, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_15(unsigned char *res) { __asm__ __volatile__ ("vst $vr15, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_16(unsigned char *res) { __asm__ __volatile__ ("vst $vr16, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_17(unsigned char *res) { __asm__ __volatile__ ("vst $vr17, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_18(unsigned char *res) { __asm__ __volatile__ ("vst $vr18, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_19(unsigned char *res) { __asm__ __volatile__ ("vst $vr19, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_20(unsigned char *res) { __asm__ __volatile__ ("vst $vr20, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_21(unsigned char *res) { __asm__ __volatile__ ("vst $vr21, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_22(unsigned char *res) { __asm__ __volatile__ ("vst $vr22, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_23(unsigned char *res) { __asm__ __volatile__ ("vst $vr23, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_24(unsigned char *res) { __asm__ __volatile__ ("vst $vr24, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_25(unsigned char *res) { __asm__ __volatile__ ("vst $vr25, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_26(unsigned char *res) { __asm__ __volatile__ ("vst $vr26, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_27(unsigned char *res) { __asm__ __volatile__ ("vst $vr27, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_28(unsigned char *res) { __asm__ __volatile__ ("vst $vr28, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_29(unsigned char *res) { __asm__ __volatile__ ("vst $vr29, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_30(unsigned char *res) { __asm__ __volatile__ ("vst $vr30, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst128_native_31(unsigned char *res) { __asm__ __volatile__ ("vst $vr31, %0\n\r"::"m"(*res):"memory"); }

static inline void __vld128_native_vreg(void *ptr, int vreg)
{
    switch (vreg) {
    case 0:  __vld128_native_0(ptr);  break;
    case 1:  __vld128_native_1(ptr);  break;
    case 2:  __vld128_native_2(ptr);  break;
    case 3:  __vld128_native_3(ptr);  break;
    case 4:  __vld128_native_4(ptr);  break;
    case 5:  __vld128_native_5(ptr);  break;
    case 6:  __vld128_native_6(ptr);  break;
    case 7:  __vld128_native_7(ptr);  break;
    case 8:  __vld128_native_8(ptr);  break;
    case 9:  __vld128_native_9(ptr);  break;
    case 10: __vld128_native_10(ptr); break;
    case 11: __vld128_native_11(ptr); break;
    case 12: __vld128_native_12(ptr); break;
    case 13: __vld128_native_13(ptr); break;
    case 14: __vld128_native_14(ptr); break;
    case 15: __vld128_native_15(ptr); break;
    case 16: __vld128_native_16(ptr); break;
    case 17: __vld128_native_17(ptr); break;
    case 18: __vld128_native_18(ptr); break;
    case 19: __vld128_native_19(ptr); break;
    case 20: __vld128_native_20(ptr); break;
    case 21: __vld128_native_21(ptr); break;
    case 22: __vld128_native_22(ptr); break;
    case 23: __vld128_native_23(ptr); break;
    case 24: __vld128_native_24(ptr); break;
    case 25: __vld128_native_25(ptr); break;
    case 26: __vld128_native_26(ptr); break;
    case 27: __vld128_native_27(ptr); break;
    case 28: __vld128_native_28(ptr); break;
    case 29: __vld128_native_29(ptr); break;
    case 30: __vld128_native_30(ptr); break;
    case 31: __vld128_native_31(ptr); break;
    default: break;
    }
}

static inline void __vst128_native_vreg(void *ptr, int vreg)
{
    switch (vreg) {
    case 0:  __vst128_native_0(ptr);  break;
    case 1:  __vst128_native_1(ptr);  break;
    case 2:  __vst128_native_2(ptr);  break;
    case 3:  __vst128_native_3(ptr);  break;
    case 4:  __vst128_native_4(ptr);  break;
    case 5:  __vst128_native_5(ptr);  break;
    case 6:  __vst128_native_6(ptr);  break;
    case 7:  __vst128_native_7(ptr);  break;
    case 8:  __vst128_native_8(ptr);  break;
    case 9:  __vst128_native_9(ptr);  break;
    case 10: __vst128_native_10(ptr); break;
    case 11: __vst128_native_11(ptr); break;
    case 12: __vst128_native_12(ptr); break;
    case 13: __vst128_native_13(ptr); break;
    case 14: __vst128_native_14(ptr); break;
    case 15: __vst128_native_15(ptr); break;
    case 16: __vst128_native_16(ptr); break;
    case 17: __vst128_native_17(ptr); break;
    case 18: __vst128_native_18(ptr); break;
    case 19: __vst128_native_19(ptr); break;
    case 20: __vst128_native_20(ptr); break;
    case 21: __vst128_native_21(ptr); break;
    case 22: __vst128_native_22(ptr); break;
    case 23: __vst128_native_23(ptr); break;
    case 24: __vst128_native_24(ptr); break;
    case 25: __vst128_native_25(ptr); break;
    case 26: __vst128_native_26(ptr); break;
    case 27: __vst128_native_27(ptr); break;
    case 28: __vst128_native_28(ptr); break;
    case 29: __vst128_native_29(ptr); break;
    case 30: __vst128_native_30(ptr); break;
    case 31: __vst128_native_31(ptr); break;
    default: break;
    }
}

static inline void __vld256_native_0(unsigned char *src)  { __asm__ __volatile__ ("xvld $xr0,  %0\n\r"::"m"(*src):); }
static inline void __vld256_native_1(unsigned char *src)  { __asm__ __volatile__ ("xvld $xr1,  %0\n\r"::"m"(*src):); }
static inline void __vld256_native_2(unsigned char *src)  { __asm__ __volatile__ ("xvld $xr2,  %0\n\r"::"m"(*src):); }
static inline void __vld256_native_3(unsigned char *src)  { __asm__ __volatile__ ("xvld $xr3,  %0\n\r"::"m"(*src):); }
static inline void __vld256_native_4(unsigned char *src)  { __asm__ __volatile__ ("xvld $xr4,  %0\n\r"::"m"(*src):); }
static inline void __vld256_native_5(unsigned char *src)  { __asm__ __volatile__ ("xvld $xr5,  %0\n\r"::"m"(*src):); }
static inline void __vld256_native_6(unsigned char *src)  { __asm__ __volatile__ ("xvld $xr6,  %0\n\r"::"m"(*src):); }
static inline void __vld256_native_7(unsigned char *src)  { __asm__ __volatile__ ("xvld $xr7,  %0\n\r"::"m"(*src):); }
static inline void __vld256_native_8(unsigned char *src)  { __asm__ __volatile__ ("xvld $xr8,  %0\n\r"::"m"(*src):); }
static inline void __vld256_native_9(unsigned char *src)  { __asm__ __volatile__ ("xvld $xr9,  %0\n\r"::"m"(*src):); }
static inline void __vld256_native_10(unsigned char *src) { __asm__ __volatile__ ("xvld $xr10, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_11(unsigned char *src) { __asm__ __volatile__ ("xvld $xr11, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_12(unsigned char *src) { __asm__ __volatile__ ("xvld $xr12, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_13(unsigned char *src) { __asm__ __volatile__ ("xvld $xr13, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_14(unsigned char *src) { __asm__ __volatile__ ("xvld $xr14, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_15(unsigned char *src) { __asm__ __volatile__ ("xvld $xr15, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_16(unsigned char *src) { __asm__ __volatile__ ("xvld $xr16, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_17(unsigned char *src) { __asm__ __volatile__ ("xvld $xr17, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_18(unsigned char *src) { __asm__ __volatile__ ("xvld $xr18, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_19(unsigned char *src) { __asm__ __volatile__ ("xvld $xr19, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_20(unsigned char *src) { __asm__ __volatile__ ("xvld $xr20, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_21(unsigned char *src) { __asm__ __volatile__ ("xvld $xr21, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_22(unsigned char *src) { __asm__ __volatile__ ("xvld $xr22, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_23(unsigned char *src) { __asm__ __volatile__ ("xvld $xr23, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_24(unsigned char *src) { __asm__ __volatile__ ("xvld $xr24, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_25(unsigned char *src) { __asm__ __volatile__ ("xvld $xr25, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_26(unsigned char *src) { __asm__ __volatile__ ("xvld $xr26, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_27(unsigned char *src) { __asm__ __volatile__ ("xvld $xr27, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_28(unsigned char *src) { __asm__ __volatile__ ("xvld $xr28, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_29(unsigned char *src) { __asm__ __volatile__ ("xvld $xr29, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_30(unsigned char *src) { __asm__ __volatile__ ("xvld $xr30, %0\n\r"::"m"(*src):); }
static inline void __vld256_native_31(unsigned char *src) { __asm__ __volatile__ ("xvld $xr31, %0\n\r"::"m"(*src):); }

static inline void __vst256_native_0(unsigned char *res)  { __asm__ __volatile__ ("xvst $xr0,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_1(unsigned char *res)  { __asm__ __volatile__ ("xvst $xr1,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_2(unsigned char *res)  { __asm__ __volatile__ ("xvst $xr2,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_3(unsigned char *res)  { __asm__ __volatile__ ("xvst $xr3,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_4(unsigned char *res)  { __asm__ __volatile__ ("xvst $xr4,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_5(unsigned char *res)  { __asm__ __volatile__ ("xvst $xr5,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_6(unsigned char *res)  { __asm__ __volatile__ ("xvst $xr6,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_7(unsigned char *res)  { __asm__ __volatile__ ("xvst $xr7,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_8(unsigned char *res)  { __asm__ __volatile__ ("xvst $xr8,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_9(unsigned char *res)  { __asm__ __volatile__ ("xvst $xr9,  %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_10(unsigned char *res) { __asm__ __volatile__ ("xvst $xr10, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_11(unsigned char *res) { __asm__ __volatile__ ("xvst $xr11, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_12(unsigned char *res) { __asm__ __volatile__ ("xvst $xr12, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_13(unsigned char *res) { __asm__ __volatile__ ("xvst $xr13, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_14(unsigned char *res) { __asm__ __volatile__ ("xvst $xr14, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_15(unsigned char *res) { __asm__ __volatile__ ("xvst $xr15, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_16(unsigned char *res) { __asm__ __volatile__ ("xvst $xr16, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_17(unsigned char *res) { __asm__ __volatile__ ("xvst $xr17, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_18(unsigned char *res) { __asm__ __volatile__ ("xvst $xr18, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_19(unsigned char *res) { __asm__ __volatile__ ("xvst $xr19, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_20(unsigned char *res) { __asm__ __volatile__ ("xvst $xr20, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_21(unsigned char *res) { __asm__ __volatile__ ("xvst $xr21, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_22(unsigned char *res) { __asm__ __volatile__ ("xvst $xr22, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_23(unsigned char *res) { __asm__ __volatile__ ("xvst $xr23, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_24(unsigned char *res) { __asm__ __volatile__ ("xvst $xr24, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_25(unsigned char *res) { __asm__ __volatile__ ("xvst $xr25, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_26(unsigned char *res) { __asm__ __volatile__ ("xvst $xr26, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_27(unsigned char *res) { __asm__ __volatile__ ("xvst $xr27, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_28(unsigned char *res) { __asm__ __volatile__ ("xvst $xr28, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_29(unsigned char *res) { __asm__ __volatile__ ("xvst $xr29, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_30(unsigned char *res) { __asm__ __volatile__ ("xvst $xr30, %0\n\r"::"m"(*res):"memory"); }
static inline void __vst256_native_31(unsigned char *res) { __asm__ __volatile__ ("xvst $xr31, %0\n\r"::"m"(*res):"memory"); }

static inline void __vld256_native_vreg(void *ptr, int vreg)
{
    switch (vreg) {
    case 0:  __vld256_native_0(ptr);  break;
    case 1:  __vld256_native_1(ptr);  break;
    case 2:  __vld256_native_2(ptr);  break;
    case 3:  __vld256_native_3(ptr);  break;
    case 4:  __vld256_native_4(ptr);  break;
    case 5:  __vld256_native_5(ptr);  break;
    case 6:  __vld256_native_6(ptr);  break;
    case 7:  __vld256_native_7(ptr);  break;
    case 8:  __vld256_native_8(ptr);  break;
    case 9:  __vld256_native_9(ptr);  break;
    case 10: __vld256_native_10(ptr); break;
    case 11: __vld256_native_11(ptr); break;
    case 12: __vld256_native_12(ptr); break;
    case 13: __vld256_native_13(ptr); break;
    case 14: __vld256_native_14(ptr); break;
    case 15: __vld256_native_15(ptr); break;
    case 16: __vld256_native_16(ptr); break;
    case 17: __vld256_native_17(ptr); break;
    case 18: __vld256_native_18(ptr); break;
    case 19: __vld256_native_19(ptr); break;
    case 20: __vld256_native_20(ptr); break;
    case 21: __vld256_native_21(ptr); break;
    case 22: __vld256_native_22(ptr); break;
    case 23: __vld256_native_23(ptr); break;
    case 24: __vld256_native_24(ptr); break;
    case 25: __vld256_native_25(ptr); break;
    case 26: __vld256_native_26(ptr); break;
    case 27: __vld256_native_27(ptr); break;
    case 28: __vld256_native_28(ptr); break;
    case 29: __vld256_native_29(ptr); break;
    case 30: __vld256_native_30(ptr); break;
    case 31: __vld256_native_31(ptr); break;
    default: break;
    }
}

static inline void __vst256_native_vreg(void *ptr, int vreg)
{
    switch (vreg) {
    case 0:  __vst256_native_0(ptr);  break;
    case 1:  __vst256_native_1(ptr);  break;
    case 2:  __vst256_native_2(ptr);  break;
    case 3:  __vst256_native_3(ptr);  break;
    case 4:  __vst256_native_4(ptr);  break;
    case 5:  __vst256_native_5(ptr);  break;
    case 6:  __vst256_native_6(ptr);  break;
    case 7:  __vst256_native_7(ptr);  break;
    case 8:  __vst256_native_8(ptr);  break;
    case 9:  __vst256_native_9(ptr);  break;
    case 10: __vst256_native_10(ptr); break;
    case 11: __vst256_native_11(ptr); break;
    case 12: __vst256_native_12(ptr); break;
    case 13: __vst256_native_13(ptr); break;
    case 14: __vst256_native_14(ptr); break;
    case 15: __vst256_native_15(ptr); break;
    case 16: __vst256_native_16(ptr); break;
    case 17: __vst256_native_17(ptr); break;
    case 18: __vst256_native_18(ptr); break;
    case 19: __vst256_native_19(ptr); break;
    case 20: __vst256_native_20(ptr); break;
    case 21: __vst256_native_21(ptr); break;
    case 22: __vst256_native_22(ptr); break;
    case 23: __vst256_native_23(ptr); break;
    case 24: __vst256_native_24(ptr); break;
    case 25: __vst256_native_25(ptr); break;
    case 26: __vst256_native_26(ptr); break;
    case 27: __vst256_native_27(ptr); break;
    case 28: __vst256_native_28(ptr); break;
    case 29: __vst256_native_29(ptr); break;
    case 30: __vst256_native_30(ptr); break;
    case 31: __vst256_native_31(ptr); break;
    default: break;
    }
}

#endif
