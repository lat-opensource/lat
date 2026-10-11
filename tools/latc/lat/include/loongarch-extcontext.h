/**
 * @file loongarch-extcontext.h
 * @author huqi <spcreply@outlook.com>
 * @brief extcontext header
 */
#ifndef LOONGARCH_EXTCONTEXT_H
#define LOONGARCH_EXTCONTEXT_H
#include "config-host.h"

#if defined(CONFIG_LOONGARCH_NEW_WORLD) && defined(__loongarch__)
#include "qemu/osdep.h"
#include "asm/sigcontext.h"

struct _ctx_layout {
    struct sctx_info *addr;
    unsigned int size;
};

struct extctx_layout {
    struct _ctx_layout fpu;
    struct _ctx_layout lsx;
    struct _ctx_layout lasx;
    struct _ctx_layout lbt;
    struct _ctx_layout end;
};

__attribute__((unused))
static void *get_ctx_through_ctxinfo(struct sctx_info *info)
{
	return (info) ? (void *)((char *)info + sizeof(struct sctx_info)) : (void *)0;
}

#define UC_FPU(_uc) \
  ((struct fpu_context *)(get_ctx_through_ctxinfo((_uc)->fpu.addr)))
#define UC_LBT(_uc) \
  ((struct lbt_context *)(get_ctx_through_ctxinfo((_uc)->lbt.addr)))
#define UC_LSX(_uc) \
  ((struct lsx_context *)(get_ctx_through_ctxinfo((_uc)->lsx.addr)))
#define UC_LASX(_uc) \
  ((struct lasx_context *)(get_ctx_through_ctxinfo((_uc)->lasx.addr)))

/**
 * This file defines a set of macros for accessing and modifying various
 * registers and flags in a CPU context structure. These macros are designed
 * to handle different CPU architectures and configurations, such as LASX,
 * LSX, FPU, and LBT. Below is a general description of the macros and their
 * parameters:

 * Macros:
 * - UC_GET_*: Macros for retrieving values from specific registers or flags.
 * - UC_SET_*: Macros for setting values to specific registers or flags.

 * Parameters:
 * - _uc: The CPU context structure pointer. This is the main structure that
 *        contains the registers and flags being accessed or modified.
 * - _fp: The register index or floating-point register index. This specifies
 *        which register to access or modify.
 * - _bias: An offset or bias used for accessing specific parts of a register
 *          (e.g., LASX or LSX registers). The value of _bias also depends on
 *          the definition of _type, as an LSX or LASX register can be divided
 *          into multiple vectors based on the size of _type.
 * - _type: The data type of the value being accessed or modified. This ensures
 *          type safety when casting the value. In the UC_SET_{LSX, LASX} macros,
 *          _type also determines the size of a vector within an LSX/LASX register.
 * - _val: The **memory address** to be written to a register or flag, not the
 *         actual value. This is used in the UC_SET_* macros.
 */
#define UC_GET_SCR(_uc, _fp, _type)    \
  (g_assert((_fp) >= 0 && (_fp) <= 4), \
   UC_LBT(_uc) ? (*(_type *)&UC_LBT(_uc)->regs[_fp]) : 0)

#define UC_GET_EFLAGS(_uc, _type)                                    \
  ({                                                                 \
    QEMU_BUILD_BUG_MSG(sizeof(_type) > sizeof(uint32_t),             \
                      "UC_GET_EFLAGS: _type size exceeds uint32_t"); \
    UC_LBT(_uc) ? (*(_type *)&UC_LBT(_uc)->eflags) : 0;              \
  })

#define UC_GET_FTOP(_uc, _type)                                    \
  ({                                                               \
    QEMU_BUILD_BUG_MSG(sizeof(_type) > sizeof(uint32_t),           \
                      "UC_GET_FTOP: _type size exceeds uint32_t"); \
    UC_LBT(_uc) ? (*(_type *)&UC_LBT(_uc)->ftop) : 0;              \
  })

#define UC_GET_FCSR(_uc, _type)                                    \
  ({                                                               \
    QEMU_BUILD_BUG_MSG(sizeof(_type) > sizeof(uint32_t),           \
                      "UC_GET_FCSR: _type size exceeds uint32_t"); \
    UC_LASX(_uc)  ? (*(_type *)&UC_LASX(_uc)->fcsr)                \
      : UC_LSX(_uc) ? (*(_type *)&UC_LSX(_uc)->fcsr)               \
        : UC_FPU(_uc) ? (*(_type *)&UC_FPU(_uc)->fcsr)             \
          : 0;                                                     \
  })

#define UC_GET_LASX(_uc, _fp, _bias, _type)                      \
  (UC_LASX(_uc) ? *(_type *)(((uint8_t *)UC_LASX(_uc)->regs) +   \
                             (_fp * 32 + _bias * sizeof(_type))) \
                : 0)

#define UC_GET_LSX(_uc, _fp, _bias, _type)                      \
  (UC_LSX(_uc) ? *(_type *)(((uint8_t *)UC_LSX(_uc)->regs) +    \
                            (_fp * 16 + _bias * sizeof(_type))) \
               : UC_GET_LASX(_uc, _fp, _bias, _type))

#define UC_GET_FPR(_uc, _fp, _type)                  \
  (UC_FPU(_uc) ? *(_type *)(UC_FPU(_uc)->regs + _fp) \
               : UC_GET_LSX(_uc, _fp, 0, _type))

#define UC_SET_SCR(_uc, _fp, _val, _type)                                  \
  do {                                                                     \
    g_assert((_fp) >= 0 && (_fp) <= 4);                                    \
    if (UC_LBT(_uc))                                                       \
      (*(_type *)(UC_LBT(_uc)->regs + _fp) = *(_type *)(uintptr_t)(_val)); \
    else                                                                   \
      g_assert_not_reached();                                              \
  } while (0)

#define UC_SET_EFLAGS(_uc, _val, _type)                                   \
  do {                                                                    \
    QEMU_BUILD_BUG_MSG(sizeof(_type) > sizeof(uint32_t),                  \
                       "UC_SET_EFLAGS: _type size exceeds uint32_t");     \
    if (UC_LBT(_uc))                                                      \
      (*(_type *)(&(UC_LBT(_uc)->eflags)) = *(_type *)(uintptr_t)(_val)); \
    else                                                                  \
      g_assert_not_reached();                                             \
  } while (0)

#define UC_SET_FTOP(_uc, _val, _type)                                   \
  do {                                                                  \
    QEMU_BUILD_BUG_MSG(sizeof(_type) > sizeof(uint32_t),                \
                       "UC_SET_FTOP: _type size exceeds uint32_t");     \
    if (UC_LBT(_uc))                                                    \
      (*(_type *)(&(UC_LBT(_uc)->ftop)) = *(_type *)(uintptr_t)(_val)); \
    else                                                                \
      g_assert_not_reached();                                           \
  } while (0)

#define UC_SET_FCSR(_uc, _val, _type)                                    \
  do {                                                                   \
    QEMU_BUILD_BUG_MSG(sizeof(_type) > sizeof(uint32_t),                 \
                       "UC_SET_FCSR: _type size exceeds uint32_t");      \
    if (UC_LASX(_uc))                                                    \
      (*(_type *)(&(UC_LASX(_uc)->fcsr)) = *(_type *)(uintptr_t)(_val)); \
    else if (UC_LSX(_uc))                                                \
      (*(_type *)(&(UC_LSX(_uc)->fcsr)) = *(_type *)(uintptr_t)(_val));  \
    else if (UC_FPU(_uc))                                                \
      (*(_type *)(&(UC_FPU(_uc)->fcsr)) = *(_type *)(uintptr_t)(_val));  \
    else                                                                 \
      g_assert_not_reached();                                            \
  } while (0)

#define UC_SET_LASX(_uc, _fp, _bias, _val, _type)      \
  do {                                                 \
    if (UC_LASX(_uc)) {                                \
      *(_type *)(((uint8_t *)UC_LASX(_uc)->regs) +     \
                 (_fp * 32 + _bias * sizeof(_type))) = \
          *(_type *)(uintptr_t)(_val);                 \
    } else {                                           \
      g_assert_not_reached();                          \
    }                                                  \
  } while (0)

#define UC_SET_LSX(_uc, _fp, _bias, _val, _type)       \
  do {                                                 \
    if (UC_LSX(_uc)) {                                 \
      *(_type *)(((uint8_t *)UC_LSX(_uc)->regs) +      \
                 (_fp * 16 + _bias * sizeof(_type))) = \
          *(_type *)(uintptr_t)(_val);                 \
    } else {                                           \
      UC_SET_LASX(_uc, _fp, _bias, _val, _type);       \
    }                                                  \
  } while (0)

#define UC_SET_FPR(_uc, _fp, _val, _type)                                \
  do {                                                                   \
    if (UC_FPU(_uc)) {                                                   \
      *(_type *)(UC_FPU(_uc)->regs + _fp) = *(_type *)(uintptr_t)(_val); \
    } else {                                                             \
      UC_SET_LSX(_uc, _fp, 0, _val, _type);                              \
    }                                                                    \
  } while (0)

static inline void parse_extcontext(ucontext_t *uc, struct extctx_layout *extctx)
{
    unsigned int magic, size;
    struct sctx_info *info = (struct sctx_info *)&uc->uc_mcontext.__extcontext;
    while (1) {
        magic = info->magic;
        size  = info->size;

        switch (magic) {
            case 0: /* END */
                goto done;

            case FPU_CTX_MAGIC:
                if (size < (sizeof(struct sctx_info) +
                        sizeof(struct fpu_context)))
                    goto invalid;
                extctx->fpu.addr = info;
                extctx->fpu.size = size;
                break;

            case LSX_CTX_MAGIC:
                if (size < (sizeof(struct sctx_info) +
                        sizeof(struct lsx_context)))
                    goto invalid;
                extctx->lsx.addr = info;
                extctx->lsx.size = size;
                break;

            case LASX_CTX_MAGIC:
                if (size < (sizeof(struct sctx_info) +
                        sizeof(struct lasx_context)))
                    goto invalid;
                extctx->lasx.addr = info;
                extctx->lasx.size = size;
                break;

            case LBT_CTX_MAGIC:
                if (size < (sizeof(struct sctx_info) +
                        sizeof(struct lbt_context)))
                    goto invalid;
                extctx->lbt.addr = info;
                extctx->lbt.size = size;
                break;

            default:
                goto invalid;
        }

        info = (struct sctx_info *)((char *)info + size);
    }

done:
    return;

invalid:
    g_assert_not_reached();
}
#endif
#endif
