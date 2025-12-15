/**
 * @file optimize-config.h
 * @author huqi <spcreply@outlook.com>
 * @brief optimize config header
 */
#ifndef _OPTIMIZE_CONFIG_H_
#define _OPTIMIZE_CONFIG_H_

#include "qemu/osdep.h"

/**
 * @brief For configure defination
 *
 * O0 - Disable all optimization, include basic
 * O1 - Open stable optimization
 * O2 - Open unstable optimization, include O1
 * O3 - Open testing optimization, include O2
 */
/* O0, close all optimization */
#ifndef CONFIG_LATX_O0
#define _OPT_BASIC_
#endif

/* O3, Open testing optimization */
#ifdef CONFIG_LATX_O3
#define CONFIG_LATX_O2
#define _OPT_TESTING_
#endif
/* O2, Open unstable optimization */
#ifdef CONFIG_LATX_O2
#define CONFIG_LATX_O1
#define _OPT_UNSTABLE_
#endif
/* O1, Open stable optimization */
#ifdef CONFIG_LATX_O1
#define _OPT_STABLE_
#endif

/**
 * @brief This area store basic optimization define
 *
 */
#ifdef _OPT_BASIC_
#undef CONFIG_LATX_TBLINK
#define CONFIG_LATX_TBLINK          /* tb-link */

#endif

/**
 * @brief This area store stable optimization define
 *
 * Note: If you want to use lib pass-through,
 * please add the define of CONFIG_LATX_KZT
 * and delete "--static" in build64-dbg.sh
 *
 */
#ifdef _OPT_STABLE_
#undef CONFIG_LATX_BNE_B
#define CONFIG_LATX_BNE_B           /* ben+b strip */
#undef CONFIG_LATX_FLAG_REDUCTION
#define CONFIG_LATX_FLAG_REDUCTION  /* flag reduction */
#undef CONFIG_LATX_LLSC
#define CONFIG_LATX_LLSC
#undef CONFIG_LATX_TUNNEL_LIB
#define CONFIG_LATX_TUNNEL_LIB
#undef CONFIG_LATX_LARGE_CC
#define CONFIG_LATX_LARGE_CC        /* code cache > 128MB */
#undef CONFIG_LATX_AOT
#define CONFIG_LATX_AOT
#undef CONFIG_LATX_INSTS_PATTERN
#define CONFIG_LATX_INSTS_PATTERN   /* insts pattern */
#undef CONFIG_LATX_JRRA
#define CONFIG_LATX_JRRA            /* jr-ra, */
#undef CONFIG_LATX_OPT_PUSH_POP
#define CONFIG_LATX_OPT_PUSH_POP
#undef CONFIG_LATX_SSSE3_SSE4
#define CONFIG_LATX_SSSE3_SSE4
#undef CONFIG_LATX_SPLIT_TB
#define CONFIG_LATX_SPLIT_TB
#undef CONFIG_LATX_XCOMISX_OPT
#define CONFIG_LATX_XCOMISX_OPT
#undef CONFIG_LATX_MONITOR_SHARED_MEM
#define CONFIG_LATX_MONITOR_SHARED_MEM
#undef CONFIG_LATX_TU
#define CONFIG_LATX_TU              /* tu, */
#undef CONFIG_LATX_IMM_REG
#define CONFIG_LATX_IMM_REG         /* imm-reg optimization */
#undef CONFIG_LATX_HBR
#define CONFIG_LATX_HBR
#undef CONFIG_LATX_SMC_OPT
#define CONFIG_LATX_SMC_OPT
#undef CONFIG_LATX_LAZYLINK
#define CONFIG_LATX_LAZYLINK
#undef CONFIG_LATX_FAST_JMPCACHE
#define CONFIG_LATX_FAST_JMPCACHE
//#undef CONFIG_LATX_GLUE_MASK
//#define CONFIG_LATX_GLUE_MASK
#endif

/**
 * @brief This area store unstable optimization define
 *
 */
#ifdef _OPT_UNSTABLE_
#undef CONFIG_LATX_RADICAL_EFLAGS
#define CONFIG_LATX_RADICAL_EFLAGS
#undef CONFIG_LATX_JRRA_STACK
#define CONFIG_LATX_JRRA_STACK      /* jr-ra-plus, */
#endif

/**
 * @brief This area store testing optimization define
 *
 */
#ifdef _OPT_TESTING_
#undef CONFIG_LATX_SYSCALL_TUNNEL
#define CONFIG_LATX_SYSCALL_TUNNEL
#undef CONFIG_LATX_AVX_OPT
#define CONFIG_LATX_AVX_OPT
#endif

/**
 * @brief This area store not impl. optimization define
 *
 */
#ifdef _OPT_NO_IMPL_
#undef CONFIG_LATX_FLAG_REDUCTION_EXTEND
#define CONFIG_LATX_FLAG_REDUCTION_EXTEND   /* flag reduction, cross TB */

#endif

#ifdef CONFIG_LATX_LOW_MEM_0
#define LOW_MEM_MODE_0
#endif
#ifdef CONFIG_LATX_LOW_MEM_1
#define LOW_MEM_MODE_0
#define LOW_MEM_MODE_1
#endif
#ifdef CONFIG_LATX_LOW_MEM_2
#define LOW_MEM_MODE_0
#define LOW_MEM_MODE_1
#define LOW_MEM_MODE_2
#endif

#ifdef LOW_MEM_MODE_0
#undef CONFIG_LATX_LARGE_CC
#undef CONFIG_LATX_JRRA
#undef CONFIG_LATX_MONITOR_SHARED_MEM
#endif

#ifdef LOW_MEM_MODE_1
#undef CONFIG_LATX_AOT
#undef CONFIG_LATX_TU
#undef CONFIG_LATX_SPLIT_TB
#endif

#ifdef LOW_MEM_MODE_2
/* If sizeof(TranslationBlock) > 192, we need to undef CONFIG_LATX_INSTS_PATTERN. */
/* #undef CONFIG_LATX_INSTS_PATTERN */
#endif

#endif
