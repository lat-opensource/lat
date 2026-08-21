/*
 * SPDX-FileCopyrightText: 2021-2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file hbr.c
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief HBR optimization
 */
#include "lsenv.h"
#include "ir1.h"
#include "hbr.h"
#include "hbr-semantic.h"
#include "translate.h"
#include "reg-alloc.h"
#include "latx-options.h"

#if defined(CONFIG_LATX_HBR) && defined(CONFIG_LATX_TU)

#if defined(CONFIG_CAPSTONE_DIET) && \
    !defined(CONFIG_LATX_CAPSTONE_OP_ACCESS)
#error "SHBR requires decoded x86 operand access metadata"
#endif

#define WRAP(ins) (dt_X86_INS_##ins)

typedef enum ShbrExplicitRule {
    SHBR_RULE_NONE,
    SHBR_RULE_SRC1,
    SHBR_RULE_SRC1_READ,
    SHBR_RULE_ALL_SOURCES,
    SHBR_RULE_ALL_SOURCES_READ,
    SHBR_RULE_ACCESS,
    SHBR_RULE_ACCESS_READ,
    SHBR_RULE_DEST,
    SHBR_RULE_READ_ACCESS,
    SHBR_RULE_OTHER,
    SHBR_RULE_ZERO,
    SHBR_RULE_IGNORE,
    SHBR_RULE_MOVSS,
    SHBR_RULE_MOVSD,
    SHBR_RULE_MOVD,
    SHBR_RULE_MOVQ,
    SHBR_RULE_BROADCAST,
    SHBR_RULE_EXTRACT,
    SHBR_RULE_EXTRACT128,
    SHBR_RULE_BYTE_SHIFT,
    SHBR_RULE_SCALAR_SHIFT,
    SHBR_RULE_GATHER,
    SHBR_RULE_MASKMOV,
    SHBR_RULE_PCMPSTR,
    SHBR_RULE_PSIGN,
    SHBR_RULE_SHUFPS,
    SHBR_RULE_IMPLICIT,
} ShbrExplicitRule;

typedef struct ShbrExplicitSemantic {
    uint8_t high32;
    uint8_t high64;
} ShbrExplicitSemantic;

static const ShbrExplicitSemantic shbr_explicit_semantics[dt_X86_INS_ENDING] = {
    [WRAP(PALIGNR)] = { SHBR_RULE_ACCESS_READ, SHBR_RULE_ACCESS_READ },
    [WRAP(MOVDQ2Q)] = { SHBR_RULE_READ_ACCESS, SHBR_RULE_IGNORE },
    [WRAP(PCMPESTRI)] = { SHBR_RULE_PCMPSTR, SHBR_RULE_PCMPSTR },
    [WRAP(PCMPESTRM)] = { SHBR_RULE_PCMPSTR, SHBR_RULE_PCMPSTR },
    [WRAP(PCMPISTRI)] = { SHBR_RULE_PCMPSTR, SHBR_RULE_PCMPSTR },
    [WRAP(PCMPISTRM)] = { SHBR_RULE_PCMPSTR, SHBR_RULE_PCMPSTR },
    [WRAP(PSIGNB)] = { SHBR_RULE_PSIGN, SHBR_RULE_PSIGN },
    [WRAP(PSIGND)] = { SHBR_RULE_PSIGN, SHBR_RULE_PSIGN },
    [WRAP(PSIGNW)] = { SHBR_RULE_PSIGN, SHBR_RULE_PSIGN },
    [WRAP(SHUFPS)] = { SHBR_RULE_SHUFPS, SHBR_RULE_SHUFPS },
    [WRAP(PSLLD)] = { SHBR_RULE_SCALAR_SHIFT, SHBR_RULE_SCALAR_SHIFT },
    [WRAP(PSLLDQ)] = { SHBR_RULE_BYTE_SHIFT, SHBR_RULE_BYTE_SHIFT },
    [WRAP(PSLLQ)] = { SHBR_RULE_SCALAR_SHIFT, SHBR_RULE_SCALAR_SHIFT },
    [WRAP(PSLLW)] = { SHBR_RULE_SCALAR_SHIFT, SHBR_RULE_SCALAR_SHIFT },
    [WRAP(PSRAD)] = { SHBR_RULE_SCALAR_SHIFT, SHBR_RULE_SCALAR_SHIFT },
    [WRAP(PSRAW)] = { SHBR_RULE_SCALAR_SHIFT, SHBR_RULE_SCALAR_SHIFT },
    [WRAP(PSRLD)] = { SHBR_RULE_SCALAR_SHIFT, SHBR_RULE_SCALAR_SHIFT },
    [WRAP(PSRLDQ)] = { SHBR_RULE_BYTE_SHIFT, SHBR_RULE_BYTE_SHIFT },
    [WRAP(PSRLQ)] = { SHBR_RULE_SCALAR_SHIFT, SHBR_RULE_SCALAR_SHIFT },
    [WRAP(PSRLW)] = { SHBR_RULE_SCALAR_SHIFT, SHBR_RULE_SCALAR_SHIFT },
    [WRAP(PUNPCKHQDQ)] = { SHBR_RULE_ACCESS_READ, SHBR_RULE_ACCESS_READ },
    [WRAP(UNPCKHPD)] = { SHBR_RULE_ACCESS_READ, SHBR_RULE_ACCESS_READ },
    [WRAP(VADDPD)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VADDPS)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VADDSD)] = { SHBR_RULE_ACCESS_READ, SHBR_RULE_SRC1 },
    [WRAP(VADDSS)] = { SHBR_RULE_SRC1, SHBR_RULE_SRC1 },
    [WRAP(VADDSUBPD)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VADDSUBPS)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VANDNPD)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VANDNPS)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VANDPD)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VANDPS)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VDIVPD)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VDIVPS)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VDIVSD)] = { SHBR_RULE_ACCESS_READ, SHBR_RULE_SRC1 },
    [WRAP(VDIVSS)] = { SHBR_RULE_SRC1, SHBR_RULE_SRC1 },
    [WRAP(VHADDPD)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VHADDPS)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VHSUBPD)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VHSUBPS)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VMAXPD)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VMAXPS)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VMAXSD)] = { SHBR_RULE_ACCESS_READ, SHBR_RULE_SRC1 },
    [WRAP(VMAXSS)] = { SHBR_RULE_SRC1, SHBR_RULE_SRC1 },
    [WRAP(VMINPD)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VMINPS)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VMINSD)] = { SHBR_RULE_ACCESS_READ, SHBR_RULE_SRC1 },
    [WRAP(VMINSS)] = { SHBR_RULE_SRC1, SHBR_RULE_SRC1 },
    [WRAP(VMULPD)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VMULPS)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VMULSD)] = { SHBR_RULE_ACCESS_READ, SHBR_RULE_SRC1 },
    [WRAP(VMULSS)] = { SHBR_RULE_SRC1, SHBR_RULE_SRC1 },
    [WRAP(VORPD)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VORPS)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VRCPPS)] = { SHBR_RULE_SRC1, SHBR_RULE_SRC1 },
    [WRAP(VRSQRTPS)] = { SHBR_RULE_SRC1, SHBR_RULE_SRC1 },
    [WRAP(VSQRTPD)] = { SHBR_RULE_SRC1_READ, SHBR_RULE_SRC1_READ },
    [WRAP(VSQRTPS)] = { SHBR_RULE_SRC1_READ, SHBR_RULE_SRC1_READ },
    [WRAP(VSQRTSD)] = { SHBR_RULE_ACCESS_READ, SHBR_RULE_SRC1 },
    [WRAP(VSQRTSS)] = { SHBR_RULE_SRC1, SHBR_RULE_SRC1 },
    [WRAP(VSUBPD)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VSUBPS)] = {
        SHBR_RULE_ALL_SOURCES_READ, SHBR_RULE_ALL_SOURCES_READ },
    [WRAP(VSUBSD)] = { SHBR_RULE_ACCESS_READ, SHBR_RULE_SRC1 },
    [WRAP(VSUBSS)] = { SHBR_RULE_SRC1, SHBR_RULE_SRC1 },
    [WRAP(VXORPD)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VXORPS)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPABSB)] = { SHBR_RULE_SRC1, SHBR_RULE_SRC1 },
    [WRAP(VPABSD)] = { SHBR_RULE_SRC1, SHBR_RULE_SRC1 },
    [WRAP(VPABSW)] = { SHBR_RULE_SRC1, SHBR_RULE_SRC1 },
    [WRAP(VPADDB)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPADDD)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPADDQ)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPADDSB)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPADDSW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPADDUSB)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPADDUSW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPADDW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPAND)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPANDN)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPAVGB)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPAVGW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPCMPEQB)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPCMPEQD)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPCMPEQQ)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPCMPEQW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPCMPGTB)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPCMPGTD)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPCMPGTQ)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPCMPGTW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMADDUBSW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMADDWD)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMAXSB)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMAXSD)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMAXSW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMAXUB)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMAXUD)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMAXUW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMINSB)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMINSD)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMINSW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMINUB)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMINUD)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMINUW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMULDQ)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMULHRSW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMULHUW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMULHW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMULLD)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMULLW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPMULUDQ)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPOR)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPSADBW)] = { SHBR_RULE_ACCESS_READ, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPSUBB)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPSUBD)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPSUBQ)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPSUBSB)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPSUBSW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPSUBUSB)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPSUBUSW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPSUBW)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
    [WRAP(VPXOR)] = { SHBR_RULE_ALL_SOURCES, SHBR_RULE_ALL_SOURCES },
#include "hbr-xmm-semantics.inc"
};

static inline bool shbr_opnd_is_vector(const IR1_OPND *opnd)
{
    return ir1_opnd_is_xmm(opnd) || ir1_opnd_is_ymm(opnd);
}

static inline uint16_t shbr_opnd_mask(const IR1_OPND *opnd)
{
    int reg = ir1_opnd_base_reg_num(opnd);
    lsassert(reg >= 0 && reg < XMM_NUM);
    return 1U << reg;
}

#define SHBR_ACCESS_READ  (1U << 0)
#define SHBR_ACCESS_WRITE (1U << 1)

static inline bool shbr_opnd_has_vector_index(IR1_OPND *opnd)
{
    if (!ir1_opnd_is_mem(opnd) || !ir1_opnd_has_index(opnd)) {
        return false;
    }
    dt_x86_reg index = ir1_opnd_index_reg(opnd);
    return (index >= dt_X86_REG_XMM0 && index <= dt_X86_REG_XMM15) ||
           (index >= dt_X86_REG_YMM0 && index <= dt_X86_REG_YMM15);
}

static inline uint16_t shbr_vector_index_mask(IR1_OPND *opnd)
{
    int reg = ir1_opnd_vsib_index_reg_num(opnd);
    lsassert(reg >= 0 && reg < XMM_NUM);
    return 1U << reg;
}

static inline void shbr_record_read(IR1_INST *ir1, uint32_t *xmm,
        uint16_t mask)
{
    ir1->shbr_read |= mask;
    for (int i = 0; i < XMM_NUM; i++) {
        if (mask & (1U << i)) {
            ir1->xmm_use |= xmm[i];
        }
    }
}

static void shbr_record_vector_reads(IR1_INST *ir1, uint32_t *xmm)
{
    uint16_t reads = 0;
    int opnd_num = ir1_get_opnd_num(ir1);

    for (int i = 0; i < opnd_num; i++) {
        IR1_OPND *opnd = ir1_get_opnd(ir1, i);
        if (shbr_opnd_is_vector(opnd) &&
            (opnd->access & SHBR_ACCESS_READ)) {
            reads |= shbr_opnd_mask(opnd);
        }
    }
    shbr_record_read(ir1, xmm, reads);
}

static inline void shbr_define_vector(IR1_INST *ir1, uint32_t *xmm,
        IR1_OPND *dest, uint16_t dependencies, uint32_t state)
{
    uint16_t dest_mask = shbr_opnd_mask(dest);
    ir1->shbr_def |= dest_mask;
    ir1->shbr_dep |= dependencies;
    xmm[ir1_opnd_base_reg_num(dest)] = state;
}

static IR1_OPND *shbr_vector_dest(IR1_INST *ir1);

/* XOR, integer SUB and ANDN zero the whole lane when all source operands
 * resolve to a single register (dest op dest == 0). Floating subtraction is
 * excluded because infinities and NaNs do not produce zero. */
static bool shbr_is_self_zeroing_op(IR1_INST *ir1)
{
    switch (ir1_opcode(ir1)) {
    case WRAP(XORPS):  case WRAP(XORPD):  case WRAP(PXOR):
    case WRAP(VXORPS): case WRAP(VXORPD): case WRAP(VPXOR):
    case WRAP(PSUBB):  case WRAP(PSUBW):  case WRAP(PSUBD):
    case WRAP(PSUBQ):  case WRAP(PSUBSB): case WRAP(PSUBSW):
    case WRAP(PSUBUSB): case WRAP(PSUBUSW):
    case WRAP(VPSUBB): case WRAP(VPSUBW): case WRAP(VPSUBD):
    case WRAP(VPSUBQ): case WRAP(VPSUBSB): case WRAP(VPSUBSW):
    case WRAP(VPSUBUSB): case WRAP(VPSUBUSW):
    case WRAP(ANDNPS): case WRAP(ANDNPD): case WRAP(PANDN):
    case WRAP(VANDNPS): case WRAP(VANDNPD): case WRAP(VPANDN):
        return true;
    default:
        return false;
    }
}

/* A handful of packed ops do not keep an all-zero source state in the tracked
 * high bits: packed division (0/0 is NaN), packed subtraction (round-down can
 * produce negative zero), packed equality comparison (equal lanes become
 * all-ones), and packed reciprocal/rsqrt (1/0 is infinity).
 * Scalar div (VDIVSS/VDIVSD) and greater-than comparison (VPCMPGT*) are
 * deliberately excluded from this list: the former copies its high bits from
 * the VEX.vvvv source, the latter yields zero for equal (zero) lanes. */
static bool shbr_is_zero_preserving_op(IR1_INST *ir1)
{
    switch (ir1_opcode(ir1)) {
    case WRAP(VADDSUBPD): case WRAP(VADDSUBPS):
    case WRAP(VDIVPD): case WRAP(VDIVPS):
    case WRAP(VHSUBPD): case WRAP(VHSUBPS):
    case WRAP(VPCMPEQB): case WRAP(VPCMPEQD):
    case WRAP(VPCMPEQQ): case WRAP(VPCMPEQW):
    case WRAP(VRCPPS): case WRAP(VRSQRTPS):
    case WRAP(VSUBPD): case WRAP(VSUBPS):
        return false;
    default:
        return true;
    }
}

static bool apply_access_semantic(IR1_INST *ir1, uint32_t *xmm,
        bool direct_reads)
{
    uint16_t dependencies = 0;
    uint16_t definitions = 0;
    uint16_t address_reads = 0;
    uint32_t state = 0;
    bool external_source = false;
    bool saw_vector_access = false;
    int opnd_num = ir1_get_opnd_num(ir1);

    for (int i = 0; i < opnd_num; i++) {
        IR1_OPND *opnd = ir1_get_opnd(ir1, i);
        if (shbr_opnd_has_vector_index(opnd)) {
            uint16_t mask = shbr_vector_index_mask(opnd);
            address_reads |= mask;
            saw_vector_access = true;
        }
        if (shbr_opnd_is_vector(opnd)) {
            uint16_t mask = shbr_opnd_mask(opnd);
            if (opnd->access & SHBR_ACCESS_READ) {
                dependencies |= mask;
                state |= xmm[ir1_opnd_base_reg_num(opnd)];
                saw_vector_access = true;
            }
            if (opnd->access & SHBR_ACCESS_WRITE) {
                definitions |= mask;
                saw_vector_access = true;
            }
        } else if ((opnd->access & SHBR_ACCESS_READ) &&
                   !ir1_opnd_is_imm(opnd)) {
            external_source = true;
        }
    }

    if (!saw_vector_access) {
        return false;
    }
    if (direct_reads || !definitions) {
        shbr_record_read(ir1, xmm, dependencies | address_reads);
        return true;
    }

    if (external_source) {
        state |= SHBR_XMM_OTHER;
    }
    if (!state) {
        /* No predecessor dependency does not imply a zero result. */
        state = SHBR_XMM_OTHER;
    }
    ir1->shbr_def |= definitions;
    ir1->shbr_dep |= dependencies;
    shbr_record_read(ir1, xmm, address_reads);
    for (int i = 0; i < opnd_num; i++) {
        IR1_OPND *opnd = ir1_get_opnd(ir1, i);
        if (shbr_opnd_is_vector(opnd) &&
            (opnd->access & SHBR_ACCESS_WRITE)) {
            xmm[ir1_opnd_base_reg_num(opnd)] = state;
        }
    }
    return true;
}

static bool apply_access_read_semantic(IR1_INST *ir1, uint32_t *xmm)
{
    uint16_t reads = 0;
    uint32_t use = 0;
    int opnd_num = ir1_get_opnd_num(ir1);
    for (int i = 0; i < opnd_num; i++) {
        IR1_OPND *opnd = ir1_get_opnd(ir1, i);
        if (shbr_opnd_is_vector(opnd) &&
            (opnd->access & SHBR_ACCESS_READ)) {
            uint16_t mask = shbr_opnd_mask(opnd);
            reads |= mask;
            use |= xmm[ir1_opnd_base_reg_num(opnd)];
        }
        if (shbr_opnd_has_vector_index(opnd)) {
            uint16_t mask = shbr_vector_index_mask(opnd);
            reads |= mask;
            use |= xmm[ir1_opnd_vsib_index_reg_num(opnd)];
        }
    }
    if (!apply_access_semantic(ir1, xmm, false)) {
        return false;
    }
    ir1->shbr_read |= reads;
    ir1->xmm_use |= use;
    return true;
}

static bool apply_constant_semantic(IR1_INST *ir1, uint32_t *xmm,
        uint32_t state)
{
    IR1_OPND *dest = shbr_vector_dest(ir1);
    if (!dest) {
        return true;
    }
    shbr_define_vector(ir1, xmm, dest, 0, state);
    return true;
}

static bool apply_dest_semantic(IR1_INST *ir1, uint32_t *xmm)
{
    if (ir1_get_opnd_num(ir1) < 1) {
        return false;
    }
    IR1_OPND *dest = ir1_get_opnd(ir1, 0);
    if (!shbr_opnd_is_vector(dest)) {
        return false;
    }
    uint16_t mask = shbr_opnd_mask(dest);
    shbr_define_vector(ir1, xmm, dest, mask,
                       xmm[ir1_opnd_base_reg_num(dest)]);
    return true;
}

static IR1_OPND *shbr_first_read_vector(IR1_INST *ir1, int first)
{
    int opnd_num = ir1_get_opnd_num(ir1);
    for (int i = first; i < opnd_num; i++) {
        IR1_OPND *opnd = ir1_get_opnd(ir1, i);
        if (shbr_opnd_is_vector(opnd) &&
            (opnd->access & SHBR_ACCESS_READ)) {
            return opnd;
        }
    }
    return NULL;
}

static IR1_OPND *shbr_vector_dest(IR1_INST *ir1)
{
    int opnd_num = ir1_get_opnd_num(ir1);
    for (int i = 0; i < opnd_num; i++) {
        IR1_OPND *opnd = ir1_get_opnd(ir1, i);
        if (shbr_opnd_is_vector(opnd) &&
            (opnd->access & SHBR_ACCESS_WRITE)) {
            return opnd;
        }
    }
    return NULL;
}

static bool apply_movss_semantic(IR1_INST *ir1, uint32_t *xmm)
{
    IR1_OPND *dest = shbr_vector_dest(ir1);
    if (!dest) {
        /* Stores consume only bits 31:0. */
        return true;
    }
    if (ir1_get_opnd_num(ir1) == 2 &&
        !shbr_opnd_is_vector(ir1_get_opnd(ir1, 1))) {
        shbr_define_vector(ir1, xmm, dest, 0, SHBR_XMM_ZERO);
        return true;
    }
    IR1_OPND *src1 = shbr_first_read_vector(ir1, 1);
    if (!src1) {
        return false;
    }
    uint16_t mask = shbr_opnd_mask(src1);
    shbr_define_vector(ir1, xmm, dest, mask,
                       xmm[ir1_opnd_base_reg_num(src1)]);
    return true;
}

static bool apply_movsd_semantic(IR1_INST *ir1, uint32_t *xmm,
        int high_bits)
{
    IR1_OPND *dest = shbr_vector_dest(ir1);
    if (!dest) {
        if (high_bits == 32) {
            IR1_OPND *source = shbr_first_read_vector(ir1, 0);
            if (source) {
                shbr_record_read(ir1, xmm, shbr_opnd_mask(source));
            }
        }
        return true;
    }
    if (ir1_get_opnd_num(ir1) == 2 &&
        !shbr_opnd_is_vector(ir1_get_opnd(ir1, 1))) {
        shbr_define_vector(ir1, xmm, dest, 0,
                           high_bits == 64 ? SHBR_XMM_ZERO : SHBR_XMM_OTHER);
        return true;
    }
    if (high_bits == 64) {
        IR1_OPND *src1 = shbr_first_read_vector(ir1, 1);
        if (!src1) {
            return false;
        }
        uint16_t mask = shbr_opnd_mask(src1);
        shbr_define_vector(ir1, xmm, dest, mask,
                           xmm[ir1_opnd_base_reg_num(src1)]);
        return true;
    }
    return apply_access_semantic(ir1, xmm, false);
}

static bool apply_movd_semantic(IR1_INST *ir1, uint32_t *xmm)
{
    IR1_OPND *dest = shbr_vector_dest(ir1);
    if (dest) {
        shbr_define_vector(ir1, xmm, dest, 0, SHBR_XMM_ZERO);
    }
    /* Register and memory destinations consume only bits 31:0. */
    return true;
}

static bool apply_movq_semantic(IR1_INST *ir1, uint32_t *xmm,
        int high_bits)
{
    IR1_OPND *dest = shbr_vector_dest(ir1);
    IR1_OPND *source = shbr_first_read_vector(ir1, dest ? 1 : 0);
    if (!dest) {
        if (high_bits == 32 && source) {
            shbr_record_read(ir1, xmm, shbr_opnd_mask(source));
        }
        return true;
    }
    if (high_bits == 64) {
        shbr_define_vector(ir1, xmm, dest, 0, SHBR_XMM_ZERO);
    } else if (source) {
        uint16_t mask = shbr_opnd_mask(source);
        shbr_define_vector(ir1, xmm, dest, mask,
                           xmm[ir1_opnd_base_reg_num(source)]);
    } else {
        shbr_define_vector(ir1, xmm, dest, 0, SHBR_XMM_OTHER);
    }
    return true;
}

static int shbr_broadcast_element_bits(IR1_INST *ir1)
{
    switch (ir1_opcode(ir1)) {
    case WRAP(VBROADCASTSS):
    case WRAP(VPBROADCASTD):
        return 32;
    case WRAP(VPBROADCASTB):
        return 8;
    case WRAP(VPBROADCASTW):
        return 16;
    case WRAP(VBROADCASTSD):
    case WRAP(VPBROADCASTQ):
    case WRAP(MOVDDUP):
    case WRAP(VMOVDDUP):
        return 64;
    case WRAP(VBROADCASTF128):
    case WRAP(VBROADCASTI128):
        return 128;
    default:
        return 0;
    }
}

static bool apply_broadcast_semantic(IR1_INST *ir1, uint32_t *xmm,
        int high_bits)
{
    IR1_OPND *dest = shbr_vector_dest(ir1);
    if (!dest) {
        return false;
    }
    IR1_OPND *source = shbr_first_read_vector(ir1, 1);
    int element_bits = shbr_broadcast_element_bits(ir1);
    if (source && element_bits > high_bits) {
        uint16_t mask = shbr_opnd_mask(source);
        uint32_t state = xmm[ir1_opnd_base_reg_num(source)];
        if (high_bits == 32 && element_bits == 64) {
            /* The broadcasted low dword is outside the SHBR32 state. */
            state |= SHBR_XMM_OTHER;
        }
        shbr_define_vector(ir1, xmm, dest, mask,
                           state);
    } else {
        shbr_define_vector(ir1, xmm, dest, 0, SHBR_XMM_OTHER);
    }
    return true;
}

static bool apply_shufps_semantic(IR1_INST *ir1, uint32_t *xmm,
        int high_bits)
{
    if (ir1_get_opnd_num(ir1) != 3 ||
        !ir1_opnd_is_imm(ir1_get_opnd(ir1, 2))) {
        return false;
    }
    IR1_OPND *dest = ir1_get_opnd(ir1, 0);
    IR1_OPND *source = ir1_get_opnd(ir1, 1);
    if (!shbr_opnd_is_vector(dest)) {
        return false;
    }

    unsigned int imm = ir1_opnd_uimm(ir1_get_opnd(ir1, 2));
    unsigned int select[4] = {
        imm & 3, (imm >> 2) & 3, (imm >> 4) & 3, (imm >> 6) & 3,
    };
    unsigned int first_tracked_lane = high_bits / 32;
    uint16_t dest_mask = shbr_opnd_mask(dest);
    uint16_t source_mask = shbr_opnd_is_vector(source) ?
        shbr_opnd_mask(source) : 0;
    uint16_t dependencies = 0;
    uint16_t reads = 0;
    uint32_t state = 0;

    int low_dest_lanes = high_bits / 32;
    for (int lane = 0; lane < low_dest_lanes; lane++) {
        if (select[lane] >= first_tracked_lane) {
            reads |= dest_mask;
        }
    }

    if (high_bits == 32) {
        if (select[1] >= first_tracked_lane) {
            dependencies |= dest_mask;
            state |= xmm[ir1_opnd_base_reg_num(dest)];
        } else {
            state |= SHBR_XMM_OTHER;
        }
    }
    for (int lane = 2; lane < 4; lane++) {
        if (source_mask && select[lane] >= first_tracked_lane) {
            dependencies |= source_mask;
            state |= xmm[ir1_opnd_base_reg_num(source)];
        } else {
            state |= SHBR_XMM_OTHER;
        }
    }

    shbr_record_read(ir1, xmm, reads);
    shbr_define_vector(ir1, xmm, dest, dependencies, state);
    return true;
}

static int shbr_extract_element_bits(IR1_INST *ir1)
{
    switch (ir1_opcode(ir1)) {
    case WRAP(PEXTRB):
    case WRAP(VPEXTRB):
        return 8;
    case WRAP(PEXTRW):
    case WRAP(VPEXTRW):
        return 16;
    case WRAP(EXTRACTPS):
    case WRAP(VEXTRACTPS):
    case WRAP(PEXTRD):
    case WRAP(VPEXTRD):
        return 32;
    case WRAP(PEXTRQ):
    case WRAP(VPEXTRQ):
        return 64;
    default:
        return 0;
    }
}

static bool apply_extract_semantic(IR1_INST *ir1, uint32_t *xmm,
        int high_bits)
{
    int opnd_num = ir1_get_opnd_num(ir1);
    int element_bits = shbr_extract_element_bits(ir1);
    if (!element_bits || opnd_num < 3 ||
        !ir1_opnd_is_imm(ir1_get_opnd(ir1, opnd_num - 1))) {
        return false;
    }
    IR1_OPND *source = shbr_first_read_vector(ir1, 0);
    if (!source) {
        return false;
    }
    unsigned int elements = 128 / element_bits;
    unsigned int selected =
        ir1_opnd_uimm(ir1_get_opnd(ir1, opnd_num - 1)) & (elements - 1);
    if ((selected + 1) * element_bits > (unsigned int)high_bits) {
        shbr_record_read(ir1, xmm, shbr_opnd_mask(source));
    }
    return true;
}

static bool apply_extract128_semantic(IR1_INST *ir1, uint32_t *xmm)
{
    int opnd_num = ir1_get_opnd_num(ir1);
    if (opnd_num < 3 ||
        !ir1_opnd_is_imm(ir1_get_opnd(ir1, opnd_num - 1))) {
        return false;
    }
    IR1_OPND *source = shbr_first_read_vector(ir1, 0);
    if (!source) {
        return false;
    }
    bool low_lane = !(ir1_opnd_uimm(ir1_get_opnd(ir1, opnd_num - 1)) & 1);
    IR1_OPND *dest = shbr_vector_dest(ir1);
    if (!dest) {
        if (low_lane) {
            shbr_record_read(ir1, xmm, shbr_opnd_mask(source));
        }
    } else if (low_lane) {
        uint16_t mask = shbr_opnd_mask(source);
        shbr_define_vector(ir1, xmm, dest, mask,
                           xmm[ir1_opnd_base_reg_num(source)]);
    } else {
        shbr_define_vector(ir1, xmm, dest, 0, SHBR_XMM_OTHER);
    }
    return true;
}

static bool apply_byte_shift_semantic(IR1_INST *ir1, uint32_t *xmm,
        int high_bits)
{
    int opnd_num = ir1_get_opnd_num(ir1);
    if (opnd_num < 2 ||
        !ir1_opnd_is_imm(ir1_get_opnd(ir1, opnd_num - 1))) {
        return false;
    }
    IR1_OPND *dest = shbr_vector_dest(ir1);
    IR1_OPND *source = shbr_first_read_vector(ir1, 1);
    if (!source && dest && (dest->access & SHBR_ACCESS_READ)) {
        source = dest;
    }
    if (!dest || !source) {
        return false;
    }
    unsigned int count = ir1_opnd_uimm(ir1_get_opnd(ir1, opnd_num - 1));
    unsigned int boundary = 16 - high_bits / 8;
    bool right = ir1_opcode(ir1) == WRAP(PSRLDQ) ||
                 ir1_opcode(ir1) == WRAP(VPSRLDQ);
    uint16_t mask = shbr_opnd_mask(source);
    uint32_t source_state = xmm[ir1_opnd_base_reg_num(source)];

    if (right && count > 0 && count < 16) {
        shbr_record_read(ir1, xmm, mask);
    } else if (!right && count > 0 && count < 16) {
        source_state |= SHBR_XMM_OTHER;
    }

    if (count >= 16) {
        shbr_define_vector(ir1, xmm, dest, 0, SHBR_XMM_ZERO);
    } else if (count < boundary) {
        shbr_define_vector(ir1, xmm, dest, mask, source_state);
    } else {
        shbr_define_vector(ir1, xmm, dest, 0,
                           right ? SHBR_XMM_ZERO : SHBR_XMM_OTHER);
    }
    return true;
}

static int shbr_scalar_shift_element_bits(IR1_INST *ir1)
{
    switch (ir1_opcode(ir1)) {
    case WRAP(PSLLW):
    case WRAP(PSRAW):
    case WRAP(PSRLW):
    case WRAP(VPSLLW):
    case WRAP(VPSRAW):
    case WRAP(VPSRLW):
        return 16;
    case WRAP(PSLLD):
    case WRAP(PSRAD):
    case WRAP(PSRLD):
    case WRAP(VPSLLD):
    case WRAP(VPSRAD):
    case WRAP(VPSRLD):
        return 32;
    case WRAP(PSLLQ):
    case WRAP(PSRLQ):
    case WRAP(VPSLLQ):
    case WRAP(VPSLLVQ):
    case WRAP(VPSRLQ):
    case WRAP(VPSRLVQ):
        return 64;
    default:
        return 0;
    }
}

static bool shbr_scalar_shift_is_right(IR1_INST *ir1)
{
    switch (ir1_opcode(ir1)) {
    case WRAP(PSRAW):
    case WRAP(PSRAD):
    case WRAP(PSRLW):
    case WRAP(PSRLD):
    case WRAP(PSRLQ):
    case WRAP(VPSRAW):
    case WRAP(VPSRAD):
    case WRAP(VPSRLW):
    case WRAP(VPSRLD):
    case WRAP(VPSRLQ):
    case WRAP(VPSRLVQ):
        return true;
    default:
        return false;
    }
}

static bool shbr_scalar_shift_is_arithmetic(IR1_INST *ir1)
{
    switch (ir1_opcode(ir1)) {
    case WRAP(PSRAW):
    case WRAP(PSRAD):
    case WRAP(VPSRAW):
    case WRAP(VPSRAD):
        return true;
    default:
        return false;
    }
}

static bool shbr_scalar_shift_has_per_element_count(IR1_INST *ir1)
{
    return ir1_opcode(ir1) == WRAP(VPSLLVQ) ||
           ir1_opcode(ir1) == WRAP(VPSRLVQ);
}

static bool apply_scalar_shift_semantic(IR1_INST *ir1, uint32_t *xmm,
        int high_bits)
{
    int opnd_num = ir1_get_opnd_num(ir1);
    IR1_OPND *dest = shbr_vector_dest(ir1);
    if (!dest) {
        return false;
    }
    bool two_operand = opnd_num == 2 && (dest->access & SHBR_ACCESS_READ);
    IR1_OPND *data = two_operand ? dest : ir1_get_opnd(ir1, 1);
    if (!data || (!shbr_opnd_is_vector(data) && !ir1_opnd_is_mem(data))) {
        return false;
    }
    int element_bits = shbr_scalar_shift_element_bits(ir1);
    if (!element_bits) {
        return false;
    }
    bool vector_data = shbr_opnd_is_vector(data);
    uint16_t data_mask = vector_data ? shbr_opnd_mask(data) : 0;
    uint16_t dependencies = data_mask;
    uint32_t state = vector_data ?
        xmm[ir1_opnd_base_reg_num(data)] : SHBR_XMM_OTHER;
    IR1_OPND *count = NULL;
    int count_first = two_operand ? 1 : 2;
    for (int i = count_first; i < opnd_num; i++) {
        IR1_OPND *opnd = ir1_get_opnd(ir1, i);
        if ((shbr_opnd_is_vector(opnd) &&
             (opnd->access & SHBR_ACCESS_READ)) ||
            ir1_opnd_is_mem(opnd) || ir1_opnd_is_imm(opnd)) {
            count = opnd;
            break;
        }
    }
    if (!count) {
        return false;
    }

    bool known_count = ir1_opnd_is_imm(count);
    unsigned int count_value = known_count ? ir1_opnd_uimm(count) : 0;
    bool zero_result = known_count &&
        !shbr_scalar_shift_is_arithmetic(ir1) &&
        count_value >= (unsigned int)element_bits;
    bool data_moves_to_low = high_bits == 32 && element_bits == 64 &&
        shbr_scalar_shift_is_right(ir1) &&
        (!known_count || (count_value > 0 && !zero_result));

    if (data_moves_to_low && vector_data) {
        shbr_record_read(ir1, xmm, data_mask);
    }
    bool low_data_moves_to_high = high_bits == 32 && element_bits == 64 &&
        !shbr_scalar_shift_is_right(ir1) &&
        (!known_count || (count_value > 0 && !zero_result));
    if (low_data_moves_to_high) {
        state |= SHBR_XMM_OTHER;
    }
    bool count_affects_high = high_bits == 32 ||
        shbr_scalar_shift_has_per_element_count(ir1);
    if (shbr_opnd_is_vector(count) && count_affects_high) {
        uint16_t count_mask = shbr_opnd_mask(count);
        dependencies |= count_mask;
        state |= xmm[ir1_opnd_base_reg_num(count)];
        if (high_bits == 32) {
            shbr_record_read(ir1, xmm, count_mask);
        }
    }
    if (zero_result) {
        shbr_define_vector(ir1, xmm, dest, 0, SHBR_XMM_ZERO);
    } else {
        shbr_define_vector(ir1, xmm, dest, dependencies, state);
    }
    return true;
}

static bool apply_psign_semantic(IR1_INST *ir1, uint32_t *xmm)
{
    int opnd_num = ir1_get_opnd_num(ir1);
    IR1_OPND *dest = shbr_vector_dest(ir1);
    if (!dest || (opnd_num != 2 && opnd_num != 3)) {
        return false;
    }

    IR1_OPND *data = opnd_num == 2 ? dest : ir1_get_opnd(ir1, 1);
    IR1_OPND *sign = ir1_get_opnd(ir1, opnd_num - 1);
    bool vector_data = shbr_opnd_is_vector(data);
    bool vector_sign = shbr_opnd_is_vector(sign);
    uint16_t data_mask = vector_data ? shbr_opnd_mask(data) : 0;
    uint16_t sign_mask = vector_sign ? shbr_opnd_mask(sign) : 0;
    uint32_t data_state = vector_data ?
        xmm[ir1_opnd_base_reg_num(data)] : SHBR_XMM_OTHER;
    uint32_t sign_state = vector_sign ?
        xmm[ir1_opnd_base_reg_num(sign)] : SHBR_XMM_OTHER;

    if (data_state == SHBR_XMM_ZERO) {
        if (shbr_opnd_mask(dest) != data_mask) {
            shbr_define_vector(ir1, xmm, dest, data_mask, SHBR_XMM_ZERO);
        }
    } else if (sign_state == SHBR_XMM_ZERO) {
        shbr_define_vector(ir1, xmm, dest, sign_mask, SHBR_XMM_ZERO);
    } else {
        shbr_define_vector(ir1, xmm, dest, data_mask | sign_mask,
                           data_state | sign_state);
    }
    return true;
}

static bool apply_maskmov_semantic(IR1_INST *ir1, uint32_t *xmm)
{
    IR1_OPND *dest = shbr_vector_dest(ir1);
    if (!dest) {
        return apply_access_semantic(ir1, xmm, true);
    }
    IR1_OPND *mask = shbr_first_read_vector(ir1, 1);
    uint16_t dependencies = mask ? shbr_opnd_mask(mask) : 0;
    uint32_t state = SHBR_XMM_OTHER;
    if (mask) {
        state |= xmm[ir1_opnd_base_reg_num(mask)];
        shbr_record_read(ir1, xmm, dependencies);
    }
    shbr_define_vector(ir1, xmm, dest, dependencies, state);
    return true;
}

static bool apply_gather_semantic(IR1_INST *ir1, uint32_t *xmm)
{
    int opnd_num = ir1_get_opnd_num(ir1);
    if (opnd_num < 3 || !shbr_opnd_is_vector(ir1_get_opnd(ir1, 0))) {
        return false;
    }
    IR1_OPND *dest = ir1_get_opnd(ir1, 0);
    IR1_OPND *mask = shbr_first_read_vector(ir1, 1);
    if (!mask) {
        return false;
    }
    uint16_t dest_mask = shbr_opnd_mask(dest);
    uint16_t mask_mask = shbr_opnd_mask(mask);
    uint16_t address_reads = 0;
    for (int i = 0; i < opnd_num; i++) {
        IR1_OPND *opnd = ir1_get_opnd(ir1, i);
        if (shbr_opnd_has_vector_index(opnd)) {
            address_reads |= shbr_vector_index_mask(opnd);
        }
    }
    uint16_t dependencies = dest_mask | mask_mask;
    uint32_t state = xmm[ir1_opnd_base_reg_num(dest)] |
                     xmm[ir1_opnd_base_reg_num(mask)] | SHBR_XMM_OTHER;
    shbr_define_vector(ir1, xmm, dest, dependencies, state);
    shbr_record_read(ir1, xmm, mask_mask | address_reads);
    ir1->shbr_def |= mask_mask;
    xmm[ir1_opnd_base_reg_num(mask)] = SHBR_XMM_ZERO;
    return true;
}

static bool shbr_pcmpstr_writes_xmm0(IR1_INST *ir1)
{
    switch (ir1_opcode(ir1)) {
    case WRAP(PCMPESTRM):
    case WRAP(PCMPISTRM):
    case WRAP(VPCMPESTRM):
    case WRAP(VPCMPISTRM):
        return true;
    default:
        return false;
    }
}

static bool apply_pcmpstr_semantic(IR1_INST *ir1, uint32_t *xmm)
{
    uint16_t reads = 0;
    uint32_t state = 0;
    int opnd_num = ir1_get_opnd_num(ir1);
    for (int i = 0; i < opnd_num; i++) {
        IR1_OPND *opnd = ir1_get_opnd(ir1, i);
        if (shbr_opnd_is_vector(opnd) &&
            (opnd->access & SHBR_ACCESS_READ)) {
            uint16_t mask = shbr_opnd_mask(opnd);
            reads |= mask;
            state |= xmm[ir1_opnd_base_reg_num(opnd)];
        }
    }
    if (!reads) {
        return false;
    }
    shbr_record_read(ir1, xmm, reads);
    if (shbr_pcmpstr_writes_xmm0(ir1)) {
        ir1->shbr_def |= 1U;
        ir1->shbr_dep |= reads;
        xmm[0] = state | SHBR_XMM_OTHER;
    }
    return true;
}

static bool apply_explicit_semantic(IR1_INST *ir1, uint32_t *xmm,
        int high_bits)
{
    const ShbrExplicitSemantic *semantic =
        &shbr_explicit_semantics[ir1_opcode(ir1)];
    ShbrExplicitRule rule = high_bits == 32 ?
        semantic->high32 : semantic->high64;
    int opnd_num = ir1_get_opnd_num(ir1);

    switch (rule) {
    case SHBR_RULE_ACCESS:
        return apply_access_semantic(ir1, xmm, false);
    case SHBR_RULE_ACCESS_READ:
        return apply_access_read_semantic(ir1, xmm);
    case SHBR_RULE_DEST:
        return apply_dest_semantic(ir1, xmm);
    case SHBR_RULE_READ_ACCESS:
        return apply_access_semantic(ir1, xmm, true);
    case SHBR_RULE_OTHER:
        return apply_constant_semantic(ir1, xmm, SHBR_XMM_OTHER);
    case SHBR_RULE_ZERO:
        return apply_constant_semantic(ir1, xmm, SHBR_XMM_ZERO);
    case SHBR_RULE_IGNORE:
    case SHBR_RULE_IMPLICIT:
        return true;
    case SHBR_RULE_MOVSS:
        return apply_movss_semantic(ir1, xmm);
    case SHBR_RULE_MOVSD:
        return apply_movsd_semantic(ir1, xmm, high_bits);
    case SHBR_RULE_MOVD:
        return apply_movd_semantic(ir1, xmm);
    case SHBR_RULE_MOVQ:
        return apply_movq_semantic(ir1, xmm, high_bits);
    case SHBR_RULE_BROADCAST:
        return apply_broadcast_semantic(ir1, xmm, high_bits);
    case SHBR_RULE_EXTRACT:
        return apply_extract_semantic(ir1, xmm, high_bits);
    case SHBR_RULE_EXTRACT128:
        return apply_extract128_semantic(ir1, xmm);
    case SHBR_RULE_BYTE_SHIFT:
        return apply_byte_shift_semantic(ir1, xmm, high_bits);
    case SHBR_RULE_SCALAR_SHIFT:
        return apply_scalar_shift_semantic(ir1, xmm, high_bits);
    case SHBR_RULE_GATHER:
        return apply_gather_semantic(ir1, xmm);
    case SHBR_RULE_MASKMOV:
        return apply_maskmov_semantic(ir1, xmm);
    case SHBR_RULE_PCMPSTR:
        return apply_pcmpstr_semantic(ir1, xmm);
    case SHBR_RULE_PSIGN:
        return apply_psign_semantic(ir1, xmm);
    case SHBR_RULE_SHUFPS:
        return apply_shufps_semantic(ir1, xmm, high_bits);
    default:
        break;
    }

    if (rule == SHBR_RULE_NONE || opnd_num < 2) {
        return false;
    }

    IR1_OPND *dest = ir1_get_opnd(ir1, 0);
    if (!shbr_opnd_is_vector(dest)) {
        return false;
    }

    uint16_t dependencies = 0;
    uint32_t state = 0;
    bool only_same_vector_sources = true;
    uint16_t first_source_mask = 0;
    int source_end = (rule == SHBR_RULE_SRC1 ||
                      rule == SHBR_RULE_SRC1_READ) ? 2 : opnd_num;
    for (int i = 1; i < source_end; i++) {
        IR1_OPND *source = ir1_get_opnd(ir1, i);
        if (shbr_opnd_is_vector(source)) {
            uint16_t source_mask = shbr_opnd_mask(source);
            if (first_source_mask && first_source_mask != source_mask) {
                only_same_vector_sources = false;
            }
            first_source_mask = source_mask;
            dependencies |= source_mask;
            state |= xmm[ir1_opnd_base_reg_num(source)];
        } else if (ir1_opnd_is_mem(source)) {
            only_same_vector_sources = false;
            state |= SHBR_XMM_OTHER;
        } else {
            only_same_vector_sources = false;
        }
    }
    if (only_same_vector_sources && dependencies &&
        !(dependencies & (dependencies - 1)) &&
        shbr_is_self_zeroing_op(ir1)) {
        /* All sources resolve to one register and the op zeroes it, so the
         * tracked high bits are zero and carry no dependency. */
        dependencies = 0;
        state = SHBR_XMM_ZERO;
    } else if (!state) {
        /* All read XMM sources are zero. The result stays zero only for
         * zero-preserving ops; a missing XMM source is never provably zero. */
        state = (dependencies && shbr_is_zero_preserving_op(ir1)) ?
            SHBR_XMM_ZERO : SHBR_XMM_OTHER;
    }

    if (rule == SHBR_RULE_SRC1_READ ||
        rule == SHBR_RULE_ALL_SOURCES_READ) {
        shbr_record_read(ir1, xmm, dependencies);
    }
    uint16_t dest_mask = shbr_opnd_mask(dest);
    ir1->shbr_def |= dest_mask;
    ir1->shbr_dep |= dependencies;
    xmm[ir1_opnd_base_reg_num(dest)] = state;
    return true;
}

uint8_t get_inst_type(IR1_INST *ir1)
{
    int opnd_num = ir1_get_opnd_num(ir1);
    for (int i = 0; i < opnd_num; ++i) {
        if (shbr_opnd_is_vector(ir1_get_opnd(ir1, i))) {
            return SHBR_SSE;
        }
    }
    return SHBR_NTYPE;
}

static inline void src_des_update_des(IR1_INST *ir1, uint32_t *xmm)
{

    IR1_OPND *opnd0 = ir1_get_opnd(ir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(ir1, 1);

    if (!shbr_opnd_is_vector(opnd1)) {
        return;
    } else if (!shbr_opnd_is_vector(opnd0)) {
        return;
    }

    assert(shbr_opnd_is_vector(opnd0) && shbr_opnd_is_vector(opnd1));
    int dest_num = ir1_opnd_base_reg_num(opnd0);
    int src_num = ir1_opnd_base_reg_num(opnd1);
    ir1->shbr_def |= (1U << dest_num);
    ir1->shbr_dep |= (1U << dest_num) | (1U << src_num);
    xmm[dest_num] |= xmm[src_num];
}

static inline void src_update_des(IR1_INST *ir1, uint32_t *xmm)
{
    IR1_OPND *opnd0 = ir1_get_opnd(ir1, 0);
    IR1_OPND *opnd1 = ir1_get_opnd(ir1, 1);
    if (!shbr_opnd_is_vector(opnd1)) {
        return;
    } else if (!shbr_opnd_is_vector(opnd0)) {
        return;
    }
    assert(shbr_opnd_is_vector(opnd0) && shbr_opnd_is_vector(opnd1));
    int src_num = ir1_opnd_base_reg_num(opnd1);
    int dest_num = ir1_opnd_base_reg_num(opnd0);
    ir1->shbr_def |= (1U << dest_num);
    ir1->shbr_dep |= (1U << src_num);
    xmm[dest_num] = xmm[src_num];
}

static inline void other_update_des(IR1_INST *ir1, uint32_t *xmm)
{
    IR1_OPND *opnd0 = ir1_get_opnd(ir1, 0);
    if (!shbr_opnd_is_vector(opnd0)) {
        return;
    }
    int dest_num = ir1_opnd_base_reg_num(opnd0);
    ir1->shbr_def |= (1U << dest_num);
    xmm[dest_num] = SHBR_XMM_OTHER;
}

static inline void external_update_des(IR1_INST *ir1, uint32_t *xmm)
{
    IR1_OPND *opnd0 = ir1_get_opnd(ir1, 0);
    if (!shbr_opnd_is_vector(opnd0)) {
        return;
    }
    int dest_num = ir1_opnd_base_reg_num(opnd0);
    ir1->shbr_def |= (1U << dest_num);
    ir1->shbr_dep |= (1U << dest_num);
    xmm[dest_num] |= SHBR_XMM_OTHER;
}

static inline void zero_update_des(IR1_INST *ir1, uint32_t *xmm)
{
    IR1_OPND *opnd0 = ir1_get_opnd(ir1, 0);
    if (!shbr_opnd_is_vector(opnd0)) {
        return;
    }
    int dest_num = ir1_opnd_base_reg_num(opnd0);
    ir1->shbr_def |= (1U << dest_num);
    xmm[dest_num] = SHBR_XMM_ZERO;
}

static inline void src_no_opt(IR1_INST *ir1, uint32_t *xmm)
{
    IR1_OPND *opnd1 = ir1_get_opnd(ir1, 1);
    if (!shbr_opnd_is_vector(opnd1)) {
        return;
    }
    int src_num = ir1_opnd_base_reg_num(opnd1);
    ir1->shbr_read |= (1U << src_num);
    ir1->xmm_use |= xmm[src_num];
}

static inline void des_no_opt(IR1_INST *ir1, uint32_t *xmm)
{
    IR1_OPND *opnd0 = ir1_get_opnd(ir1, 0);
    if (!shbr_opnd_is_vector(opnd0)) {
        return;
    }
    int des_num = ir1_opnd_base_reg_num(opnd0);
    ir1->shbr_read |= (1U << des_num);
    ir1->xmm_use |= xmm[des_num];
}

static bool deal_xmm_common(TranslationBlock *tb, IR1_INST *ir1, uint32_t *xmm)
{
    IR1_OPND *des_opnd = ir1_get_opnd(ir1, 0);
    IR1_OPND *src_opnd = ir1_get_opnd(ir1, 1);
    int src_num = -1, des_num = -2;
    if (ir1_opnd_is_xmm(src_opnd)) {
        src_num = ir1_opnd_base_reg_num(src_opnd);
    }
    if (ir1_opnd_is_xmm(des_opnd)) {
        des_num = ir1_opnd_base_reg_num(des_opnd);
    }

    switch (ir1_opcode(ir1)) {
    case WRAP(PADDB):
    case WRAP(PADDW):
    case WRAP(PADDD):
    case WRAP(PADDQ):
    case WRAP(PADDSB):
    case WRAP(PADDSW):
    case WRAP(PADDUSB):
    case WRAP(PADDUSW):
    case WRAP(PMADDWD):
    case WRAP(PMADDUBSW):
    case WRAP(PMULDQ):
    case WRAP(PMULUDQ):
    case WRAP(PMULLW):
    case WRAP(PMULLD):
    case WRAP(PMULHW):
    case WRAP(PMULHUW):
    case WRAP(PMULHRSW):
        /* src is xmm. */
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
        } else if (ir1_opnd_is_mem(src_opnd)) {
            external_update_des(ir1, xmm);
        }
        return true;
    case WRAP(ADDPD):
    case WRAP(ADDPS):
    case WRAP(MULPD):
    case WRAP(MULPS):
        /* Capture the input provenance before an aliased destination is
         * overwritten below.  Packed FP inputs are architectural reads even
         * when the vector result is dead because they can update MXCSR. */
        shbr_record_vector_reads(ir1, xmm);
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
        } else if (ir1_opnd_is_mem(src_opnd)) {
            external_update_des(ir1, xmm);
        }
        return true;
    case WRAP(ADDSUBPS):
    case WRAP(ADDSUBPD):
    case WRAP(DIVPS):
    case WRAP(DIVPD):
    case WRAP(SUBPS):
    case WRAP(SUBPD):
        shbr_record_vector_reads(ir1, xmm);
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
        } else if (ir1_opnd_is_mem(src_opnd)) {
            external_update_des(ir1, xmm);
        }
        return true;
    /* Integer subtraction is self-zeroing when dest == src. */
    case WRAP(PSUBB):
    case WRAP(PSUBW):
    case WRAP(PSUBD):
    case WRAP(PSUBQ):
    case WRAP(PSUBSB):
    case WRAP(PSUBSW):
    case WRAP(PSUBUSB):
    case WRAP(PSUBUSW):
        if (ir1_opnd_is_xmm(src_opnd)) {
            if (src_num == des_num) {
                zero_update_des(ir1, xmm);
            } else {
                src_des_update_des(ir1, xmm);
            }
        } else if (ir1_opnd_is_mem(src_opnd)) {
            external_update_des(ir1, xmm);
        }
        return true;
    /* Packed FP division of zero produces NaN.  Packed FP subtraction of
     * positive zero can produce negative zero under round-toward-negative.
     * Keep the source dependencies, but never propagate an exact-zero fact
     * through these operations. */
    case WRAP(ADDSUBPS):
    case WRAP(ADDSUBPD):
    case WRAP(DIVPS):
    case WRAP(DIVPD):
    case WRAP(SUBPS):
    case WRAP(SUBPD):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
            if (xmm[des_num] == SHBR_XMM_ZERO) {
                xmm[des_num] = SHBR_XMM_OTHER;
            }
        } else if (ir1_opnd_is_mem(src_opnd)) {
            external_update_des(ir1, xmm);
        }
        return true;
    /* 32 ~ 127 Unmodified. */
    case WRAP(ADDSS):
    case WRAP(DIVSS):
    case WRAP(MULSS):
    case WRAP(SUBSS):
    case WRAP(CMPSS):
    case WRAP(MAXSS):
    case WRAP(MINSS):
    case WRAP(RSQRTSS):
    case WRAP(SQRTSS):
    case WRAP(RCPSS):
    case WRAP(COMISS):  // no change
    case WRAP(UCOMISS): // no change
    /* des 0 ~ 63 from src 0 ~ 31. */
    case WRAP(CVTSS2SI):
    case WRAP(CVTTSS2SI):
    case WRAP(CVTSS2SD):
    /* des: xmm, src: r/m . */
    /* if src is 64 bit: dest 0 ~ 31 from src 0 ~ 63. */
    /* if src is 32 bit: dest 0 ~ 31 from src 0 ~ 31. */
    case WRAP(CVTSI2SS):
    case WRAP(CVTSI2SD):
        return true;
    /* mov 128. */
    case WRAP(MOVUPS):
    case WRAP(MOVUPD):
    case WRAP(MOVAPS):
    case WRAP(MOVAPD):
    case WRAP(MOVDQA):
    case WRAP(MOVDQU):
    case WRAP(MOVNTPS):
    case WRAP(MOVNTPD):
        if (!ir1_opnd_is_xmm(des_opnd)) {
            /* src will write to mm. */
            src_no_opt(ir1, xmm);
        } else if (ir1_opnd_is_mem(src_opnd)) {
            other_update_des(ir1, xmm);
        } else {
            src_update_des(ir1, xmm);
        }
        return true;
    /* MOVD r/m32, xmm */
    /* MOVD xmm, r/m32 : 32 ~ 127 to be zero. */
    case WRAP(MOVD):
        zero_update_des(ir1, xmm);
        return true;
    /* MOVSS xmm1, xmm2 : 32 ~ 127 keep. */
    /* MOVSS xmm1, m32 : 32 ~ 127 zero. */
    case WRAP(MOVSS):
        if (ir1_opnd_is_mem(src_opnd)) {
            zero_update_des(ir1, xmm);
        }
        return true;
    /* need 0 ~ 127 every high bit. */
    case WRAP(PMOVMSKB):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_no_opt(ir1, xmm);
        }
        return true;
    /* src 0 ~ 31 update des 0 ~ 127. */
    case WRAP(PMOVSXBD):
    case WRAP(PMOVSXBQ):
    case WRAP(PMOVSXWQ):
        other_update_des(ir1, xmm);
        return true;
    case WRAP(CMPPD):
    case WRAP(CMPPS):
        /* The immediate predicate and NaN inputs determine the result even
         * when both encoded register operands alias. */
        shbr_record_vector_reads(ir1, xmm);
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
            if (xmm[des_num] == SHBR_XMM_ZERO) {
                xmm[des_num] = SHBR_XMM_OTHER;
            }
        } else if (ir1_opnd_is_mem(src_opnd)) {
            external_update_des(ir1, xmm);
        }
        return true;
    case WRAP(PCMPEQB):
    case WRAP(PCMPEQW):
    case WRAP(PCMPEQD):
    case WRAP(PCMPEQQ):
        /* Equal registers compare equal regardless of their contents.  Two
         * distinct registers that are currently zero still produce all-ones,
         * so do not propagate the exact-zero state through the result. */
        if (src_num == des_num) {
            other_update_des(ir1, xmm);
        } else if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
            if (xmm[des_num] == SHBR_XMM_ZERO) {
                xmm[des_num] = SHBR_XMM_OTHER;
            }
        } else if (ir1_opnd_is_mem(src_opnd)) {
            external_update_des(ir1, xmm);
        }
        return true;
    case WRAP(PCMPGTB):
    case WRAP(PCMPGTW):
    case WRAP(PCMPGTD):
    case WRAP(PCMPGTQ):
        if (src_num == des_num) {
            zero_update_des(ir1, xmm);
        } else if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
        } else if (ir1_opnd_is_mem(src_opnd)) {
            external_update_des(ir1, xmm);
        }
        return true;

    /* src and des h64 change des l64. */
    case WRAP(PUNPCKHBW):
    case WRAP(PUNPCKHDQ):
    case WRAP(PUNPCKHWD):
    case WRAP(UNPCKHPS):
        des_no_opt(ir1, xmm);
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
            src_no_opt(ir1, xmm);
        } else if (ir1_opnd_is_mem(src_opnd)) {
            external_update_des(ir1, xmm);
        }
        return true;
    case WRAP(PACKUSDW):
    case WRAP(PACKSSWB):
    case WRAP(PACKSSDW):
    case WRAP(PACKUSWB):
        des_no_opt(ir1, xmm);
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
            src_no_opt(ir1, xmm);
        } else if (ir1_opnd_is_mem(src_opnd)) {
            external_update_des(ir1, xmm);
        }
        xmm[des_num] |= SHBR_XMM_OTHER;
        return true;
    default:
        break;
    }

    switch (ir1_opcode(ir1)) {
    case WRAP(POR):
    case WRAP(ORPS):
    case WRAP(ORPD):
    case WRAP(PMINSW):
    case WRAP(PMINSD):
    case WRAP(PMINSB):
    case WRAP(PMINUW):
    case WRAP(PMINUD):
    case WRAP(PMINUB):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
        } else if (ir1_opnd_is_mem(src_opnd)) {
            external_update_des(ir1, xmm);
        }
        return true;
    /* ANDN is self-zeroing when dest == src (~x & x == 0). */
    case WRAP(ANDNPD):
    case WRAP(ANDNPS):
    case WRAP(PANDN):
        if (ir1_opnd_is_xmm(src_opnd)) {
            if (src_num == des_num) {
                zero_update_des(ir1, xmm);
            } else {
                src_des_update_des(ir1, xmm);
            }
        } else if (ir1_opnd_is_mem(src_opnd)) {
            external_update_des(ir1, xmm);
        }
        return true;
    case WRAP(ANDPD):
    case WRAP(ANDPS):
    case WRAP(PAND):
        if (ir1_opnd_is_mem(src_opnd)) {
            external_update_des(ir1, xmm);
            return true;
        }
        if (!ir1_opnd_is_xmm(src_opnd)) {
            return true;
        }
        if (xmm[des_num] == SHBR_XMM_ZERO) {
            /* ZERO! */
            /* ir1->xmm_def = SHBR_UPDATE_DES; */
            /* xmm[dest_num] = SHBR_XMM_ZERO; */
        } else if (xmm[src_num] == SHBR_XMM_ZERO) {
            src_update_des(ir1, xmm);
        } else if (src_num == des_num) {
            /* inherit dest => keep */
        } else {
            src_des_update_des(ir1, xmm);
        }
        return true;
    case WRAP(PXOR):
    case WRAP(XORPD):
    case WRAP(XORPS):
        if (ir1_opnd_is_mem(src_opnd)) {
            external_update_des(ir1, xmm);
            return true;
        }
        if (!ir1_opnd_is_xmm(src_opnd)) {
            return true;
        }
        if (src_num == des_num) {
            zero_update_des(ir1, xmm);
        } else {
            src_des_update_des(ir1, xmm);
        }
        return true;
    default:
        break;
    }
    return false;
}

/* FIX ME! */
static bool deal_dest_not_xmm_32(TranslationBlock *tb,
        IR1_INST *ir1, uint32_t xmm[XMM_NUM])
{
    IR1_OPND *opnd1 = ir1_get_opnd(ir1, 1);
    if (!ir1_opnd_is_xmm(opnd1)) {
        return false;
    }

    switch (ir1_opcode(ir1)) {
    /* MOVDQ2Q mm, xmm : xmm mov l64 to mmx. */
    default:
        break;
    }

    return false;
}

/* FIX ME! */
static bool deal_src_not_xmm_32(TranslationBlock *tb,
        IR1_INST *ir1, uint32_t xmm[XMM_NUM])
{
    return false;
}


/* analyse 32 ~ 127 bit. */
static bool xmm_analyse_32(TranslationBlock *tb,
        IR1_INST *ir1, uint32_t xmm[XMM_NUM])
{
    if (apply_explicit_semantic(ir1, xmm, 32)) {
        return true;
    }
    if (deal_xmm_common(tb, ir1, xmm)) {
        return true;
    }

    IR1_OPND *src_opnd = ir1_get_opnd(ir1, 1);
    IR1_OPND *des_opnd = ir1_get_opnd(ir1, 0);

    ir1_opnd_is_imm(src_opnd);
    switch (ir1_opcode(ir1)) {
    /* src 0 ~ 63 from src and des 0 ~ 63. */
    case WRAP(CMPSD):
    case WRAP(ADDSD):
    case WRAP(DIVSD):
    case WRAP(MAXSD):
    case WRAP(MINSD):
    case WRAP(MULSD):
    case WRAP(SUBSD):
    case WRAP(SQRTSD):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
        } else if (ir1_opnd_is_mem(src_opnd)) {
            external_update_des(ir1, xmm);
        }
        return true;
    case WRAP(COMISD):
    case WRAP(UCOMISD):
        des_no_opt(ir1, xmm);
        src_no_opt(ir1, xmm);
        return true;
    /* MOVSD xmm, m64 : h64 zero. */
    /* MOVSD xmm, xmm : h64 keep. */
    /* MOVSD m64, xmm : */
    case WRAP(MOVSD):
        if (ir1_opnd_is_mem(src_opnd)) {
            other_update_des(ir1, xmm);
        } else if(!ir1_opnd_is_xmm(des_opnd)) {
            src_no_opt(ir1, xmm);
        } else {
            /* 0 ~ 63 from src, 64 ~ 127 from des, so 32 ~ 127 need src and des. */
            src_des_update_des(ir1, xmm);
        }
        return true;
    /* xmm, m64 or m64, xmm */
    /* mov src to src h64. */
    case WRAP(MOVHPS):
    case WRAP(MOVHPD):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_no_opt(ir1, xmm);
        }
        return true;

    /* MOVQ xmm1, xmm2 : h64 zero. */
    /* MOVQ xmm1, mm : h64 zero. */
    /* MOVQ r/m64, xmm : h64 zero. */
    case WRAP(MOVQ):
        if (ir1_opnd_is_mem(src_opnd)) {
            other_update_des(ir1, xmm);
        } else if (!ir1_opnd_is_xmm(des_opnd)) {
            src_no_opt(ir1, xmm);
        } else if(ir1_opnd_is_xmm(src_opnd)) {
            src_update_des(ir1, xmm);
        }
        return true;
    /* mov l64 to h64.  The source low dword is outside SHBR32. */
    case WRAP(MOVLHPS):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
            xmm[ir1_opnd_base_reg_num(des_opnd)] |= SHBR_XMM_OTHER;
        }
        return true;
    /* mov l64 to l64, h64 unchange. */
    case WRAP(MOVLPD):
    case WRAP(MOVLPS):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
        } else if (ir1_opnd_is_mem(src_opnd)) {
            external_update_des(ir1, xmm);
        }
        return true;
    /* mov h64 to l64. */
    case WRAP(MOVHLPS):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
            src_no_opt(ir1, xmm);
        }
        return true;
    /* des 0 ~ 64: need des and src 0 ~ 31. */
    /* des 64 ~ 127: need des and src 32 ~ 64. */
    case WRAP(PUNPCKLBW):
    case WRAP(PUNPCKLWD):
    case WRAP(PUNPCKLDQ):
    /* des 0 ~ 64: need src 0 ~ 64 keep. */
    /* des 64 ~ 127: need des 0 ~ 64. */
    case WRAP(PUNPCKLQDQ):
    case WRAP(UNPCKLPS):
    case WRAP(UNPCKLPD):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_des_update_des(ir1, xmm);
        } else if (ir1_opnd_is_mem(src_opnd)) {
            external_update_des(ir1, xmm);
        }
        xmm[ir1_opnd_base_reg_num(des_opnd)] |= SHBR_XMM_OTHER;
        return true;
    /* src 0 ~ 63 update des 0 ~ 127. */
    case WRAP(PMOVSXBW):
    case WRAP(PMOVSXWD):
    case WRAP(PMOVSXDQ):
        other_update_des(ir1, xmm);
        src_no_opt(ir1,xmm);
        return true;
    /* des 0 ~ 31 from src 0 ~ 63. */
    case WRAP(CVTSD2SS):
    /* des 0 ~ 63 from src 0 ~ 63. */
    case WRAP(CVTSD2SI):
    case WRAP(CVTTSD2SI):
    case WRAP(CVTTPS2PI):
        if (ir1_opnd_is_xmm(src_opnd)) {
            src_no_opt(ir1, xmm);
        }
        return true;
    default:
        break;
    }

    if (ir1_get_opnd_num(ir1) == 3 && ir1_opnd_is_imm(ir1_get_opnd(ir1, 2))) {
        uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(ir1, 2));
        switch (ir1_opcode(ir1)) {
        case WRAP(PSHUFD):
            if (ir1_opnd_is_mem(src_opnd) || imm < 1) {
                other_update_des(ir1, xmm);
            } else {
               src_no_opt(ir1, xmm);
               src_update_des(ir1, xmm);
            }
            xmm[ir1_opnd_base_reg_num(ir1_get_opnd(ir1, 0))] |=
                SHBR_XMM_OTHER;
            return true;
        default:
            break;
        }
    }

    /* deal dest not xmm. */
    if (!ir1_opnd_is_xmm(des_opnd)) {
        return deal_dest_not_xmm_32(tb, ir1, xmm);
    }
    /* src not xmm. */
    if(!ir1_opnd_is_xmm(src_opnd)) {
        return deal_src_not_xmm_32(tb, ir1, xmm);
    }

    return false;
}

static bool deal_dest_not_xmm_64(TranslationBlock *tb,
        IR1_INST *ir1, uint32_t xmm[XMM_NUM])
{
    return false;
}

static bool deal_src_not_xmm_64(TranslationBlock *tb,
        IR1_INST *ir1, uint32_t xmm[XMM_NUM])
{
    return false;
}

/* analyse 64 ~ 127 bit. */
/* TODO: analyze more instructions. */
static bool xmm_analyse_64(TranslationBlock *tb,
        IR1_INST *ir1, uint32_t xmm[XMM_NUM])
{
    if (apply_explicit_semantic(ir1, xmm, 64)) {
        return true;
    }
    if (deal_xmm_common(tb, ir1, xmm)) {
        return true;
    }

    /* src_opnd. */
    IR1_OPND *src_opnd = ir1_get_opnd(ir1, 1);

    switch (ir1_opcode(ir1)) {
    /* src 0 ~ 63 from src and des 0 ~ 63. */
    case WRAP(CMPSD):
    case WRAP(ADDSD):
    case WRAP(DIVSD):
    case WRAP(MAXSD):
    case WRAP(MINSD):
    case WRAP(MULSD):
    case WRAP(SUBSD):
    case WRAP(SQRTSD):
    case WRAP(COMISD):
    case WRAP(UCOMISD):
    /* mov l64 to l64, h64 unchange. */
    case WRAP(MOVLPD):
    case WRAP(MOVLPS):
    /* des 0 ~ 31 from src 0 ~ 63. */
    case WRAP(CVTSD2SS):
    /* des 0 ~ 63 from src 0 ~ 63. */
    case WRAP(CVTSD2SI):
    case WRAP(CVTTSD2SI):
    case WRAP(CVTTPS2PI):
        return true;
    /* MOVSD xmm, xmm : h64 keep. */
    /* MOVSD xmm, m64 : h64 zero. */
    case WRAP(MOVSD):
        if (ir1_opnd_is_mem(src_opnd)) {
            zero_update_des(ir1, xmm);
        }
        return true;
    /* MOVQ xmm1, xmm2 : h64 zero. */
    /* MOVQ xmm1, mm : h64 zero. */
    case WRAP(MOVQ):
        zero_update_des(ir1, xmm);
        return true;
    /* xmm, m64 or m64, xmm */
    /* mov src to src h64. */
    case WRAP(MOVHPS):
    case WRAP(MOVHPD):
        if (ir1_opnd_is_mem(src_opnd)) {
            other_update_des(ir1, xmm);
        } else {
            src_no_opt(ir1, xmm);
        }
        return true;
    /* des 0 ~ 64: need des and src 0 ~ 31. */
    /* des 64 ~ 127: need des and src 32 ~ 64. */
    case WRAP(PUNPCKLBW):
    case WRAP(PUNPCKLWD):
    case WRAP(PUNPCKLDQ):
    /* des 0 ~ 64: need src 0 ~ 64. */
    /* des 64 ~ 127: need des 0 ~ 64. */
    case WRAP(PUNPCKLQDQ):
    case WRAP(UNPCKLPS):
    case WRAP(UNPCKLPD):
    /* mov l64 to h64. */
    case WRAP(MOVLHPS):
    /* src 0 ~ 63 update des 0 ~ 127. */
    case WRAP(PMOVSXBW):
    case WRAP(PMOVSXWD):
    case WRAP(PMOVSXDQ):
        other_update_des(ir1, xmm);
        return true;
    /* mov h64 to l64. */
    case WRAP(MOVHLPS):
        src_no_opt(ir1, xmm);
        return true;
    default:
        break;
    }

    if (ir1_get_opnd_num(ir1) == 3 && ir1_opnd_is_imm(ir1_get_opnd(ir1, 2))) {
        uint8_t imm = ir1_opnd_uimm(ir1_get_opnd(ir1, 2));
        switch (ir1_opcode(ir1)) {
        case WRAP(PSHUFD):
            if (ir1_opnd_is_mem(src_opnd) || imm <= 1) {
                other_update_des(ir1, xmm);
            } else {
               src_no_opt(ir1, xmm);
               src_update_des(ir1, xmm);
            }
            xmm[ir1_opnd_base_reg_num(ir1_get_opnd(ir1, 0))] |=
                SHBR_XMM_OTHER;
            return true;
        default:
            break;
        }
    }

    IR1_OPND *des_opnd = ir1_get_opnd(ir1, 0);
    /* deal dest not xmm. */
    if (!ir1_opnd_is_xmm(des_opnd)) {
        return deal_dest_not_xmm_64(tb, ir1, xmm);
    }
    /* src not xmm. */
    if(!ir1_opnd_is_xmm(src_opnd)) {
        return deal_src_not_xmm_64(tb, ir1, xmm);
    }

    return false;
}

static void init_xmm_state(uint32_t xmm[XMM_NUM])
{
    for (int i = 0; i < XMM_NUM; ++i) {
        xmm[i] = 1 << i;
    }
}

static void conservative_explicit_semantic(IR1_INST *ir1, uint32_t *xmm)
{
    int opnd_num = ir1_get_opnd_num(ir1);
    for (int i = 0; i < opnd_num; i++) {
        IR1_OPND *opnd = ir1_get_opnd(ir1, i);
        if (!shbr_opnd_is_vector(opnd)) {
            continue;
        }
        int reg = ir1_opnd_base_reg_num(opnd);
        ir1->shbr_read |= (1U << reg);
        ir1->xmm_use |= xmm[reg];
    }
}

static bool apply_implicit_semantic(TranslationBlock *tb, IR1_INST *ir1,
        uint32_t *xmm)
{
    switch (ir1_opcode(ir1)) {
    case WRAP(FXSAVE):
    case WRAP(FXSAVE64):
    case WRAP(XSAVE):
    case WRAP(XSAVEOPT):
        ir1->shbr_read |= SHBR_XMM_ALL;
        for (int i = 0; i < XMM_NUM; i++) {
            ir1->xmm_use |= xmm[i];
        }
        return true;
    case WRAP(FXRSTOR):
    case WRAP(FXRSTOR64):
        ir1->shbr_def |= SHBR_XMM_ALL;
        for (int i = 0; i < XMM_NUM; i++) {
            xmm[i] = SHBR_XMM_OTHER;
        }
        return true;
    case WRAP(XRSTOR):
        /* XRSTOR preserves XMM when EDX:EAX does not request SSE state. */
        ir1->shbr_read |= SHBR_XMM_ALL;
        ir1->shbr_def |= SHBR_XMM_ALL;
        for (int i = 0; i < XMM_NUM; i++) {
            ir1->xmm_use |= xmm[i];
            xmm[i] = SHBR_XMM_OTHER;
        }
        return true;
    case WRAP(VZEROALL):
        ir1->shbr_def |= SHBR_XMM_ALL;
        for (int i = 0; i < XMM_NUM; i++) {
            xmm[i] = SHBR_XMM_ZERO;
        }
        return true;
    case WRAP(BLENDVPD):
    case WRAP(BLENDVPS):
    case WRAP(PBLENDVB):
    case WRAP(SHA256RNDS2):
        ir1->shbr_read |= 1U;
        ir1->xmm_use |= xmm[0];
        return false;
    case WRAP(VZEROUPPER):
        return true;
    default:
        return false;
    }
}

static bool has_implicit_semantic(IR1_INST *ir1)
{
    switch (ir1_opcode(ir1)) {
    case WRAP(FXSAVE):
    case WRAP(FXSAVE64):
    case WRAP(XSAVE):
    case WRAP(XSAVEOPT):
    case WRAP(FXRSTOR):
    case WRAP(FXRSTOR64):
    case WRAP(XRSTOR):
    case WRAP(VZEROALL):
    case WRAP(BLENDVPD):
    case WRAP(BLENDVPS):
    case WRAP(PBLENDVB):
    case WRAP(SHA256RNDS2):
    case WRAP(VZEROUPPER):
        return true;
    default:
        return false;
    }
}

#include "tu.h"
typedef bool (*xmm_analyse_func)(TranslationBlock *, IR1_INST *, uint32_t *);

/* static int xcount, scount, ncount; */
static void tb_xmm_analyse(TranslationBlock *tb,
        xmm_analyse_func analyse_func, uint32_t *xmm)
{
    init_xmm_state(xmm);
    tb->s_data->shbr_type = SHBR_NTYPE;
    tb->s_data->xmm_use = 0;
    tb->s_data->xmm_def = 0;
    IR1_INST *ir1 = NULL;
    for (int i = 0; i < tb_ir1_num(tb); ++i) {
        ir1 = tb_ir1_inst(tb, i);
        ir1->xmm_def = 0;
        ir1->xmm_use = 0;
        ir1->shbr_read = 0;
        ir1->shbr_def = 0;
        ir1->shbr_dep = 0;
        uint8_t curr = get_inst_type(ir1);
        bool implicit = has_implicit_semantic(ir1);
        if (curr == SHBR_NTYPE && !implicit) {
            continue;
        } else if (tb->s_data->shbr_type == SHBR_NTYPE) {
            tb->s_data->shbr_type = SHBR_SSE;
        }
        bool handled = apply_implicit_semantic(tb, ir1, xmm);
        if (!handled && curr != SHBR_NTYPE) {
            handled = analyse_func(tb, ir1, xmm);
        }
        if (!handled && curr != SHBR_NTYPE) {
            conservative_explicit_semantic(ir1, xmm);
        }
        /* tb->s_data->xmm_def |= ir1->xmm_def; */
        tb->s_data->xmm_use |= ir1->xmm_use;
    }
    tb->s_data->xmm_use &= SHBR_XMM_MASK;
}

static void get_xmm_in(TranslationBlock *tb, uint32 *xmm)
{
    /* curr tb use. */
    tb->s_data->xmm_in = tb->s_data->xmm_use;
    for (int i = 0; i < XMM_NUM; i++) {
        /* next tb use but curr tb not cover. */
        if (tb->s_data->xmm_out & (1 << i)) {
            tb->s_data->xmm_in |= xmm[i] & SHBR_XMM_MASK;
        }
    }
}

static TranslationBlock *hbr_in_tu_successor(TranslationBlock **tb_list,
        int tb_num_in_tu, TranslationBlock *successor)
{
    for (int i = 0; successor && i < tb_num_in_tu; i++) {
        if (tb_list[i] == successor) {
            return successor;
        }
    }
    return NULL;
}

static void over_tb_shbr_opt(TranslationBlock **tb_list, int tb_num_in_tu,
        uint32 opt_flag, uint32 xmm[][XMM_NUM])
{
    TranslationBlock *next_tbs[tb_num_in_tu];
    TranslationBlock *target_tbs[tb_num_in_tu];
    for (int i = 0; i < tb_num_in_tu; i++) {
        TranslationBlock *tb = tb_list[i];
        /* Recovered or previously translated TBs may share or retain stale
         * separated_data.  Only summaries recomputed for this TU are valid. */
        next_tbs[i] = hbr_in_tu_successor(tb_list, tb_num_in_tu,
            (TranslationBlock *)tb->s_data->next_tb[TU_TB_INDEX_NEXT]);
        target_tbs[i] = hbr_in_tu_successor(tb_list, tb_num_in_tu,
            (TranslationBlock *)tb->s_data->next_tb[TU_TB_INDEX_TARGET]);
    }

    bool continue_flag = true;
    uint32_t old_live_in, old_live_out;
    while(continue_flag) {
        continue_flag = false;
        for (int i = tb_num_in_tu - 1; i >= 0; i--) {
            TranslationBlock *tb = tb_list[i];
            old_live_in = tb->s_data->xmm_in;
            old_live_out = tb->s_data->xmm_out;
            TranslationBlock *next_tb = next_tbs[i];
            TranslationBlock *target_tb = target_tbs[i];
            switch (tb->s_data->last_ir1_type) {
                case IR1_TYPE_BRANCH:
                    tb->s_data->xmm_out =
                        (next_tb ? next_tb->s_data->xmm_in : SHBR_XMM_ALL);
                    tb->s_data->xmm_out |=
                        (target_tb ? target_tb->s_data->xmm_in : SHBR_XMM_ALL);
                    break;
                case IR1_TYPE_JUMP:
                case IR1_TYPE_CALL:
                    tb->s_data->xmm_out =
                        (target_tb ? target_tb->s_data->xmm_in : SHBR_XMM_ALL);
                    break;
                case IR1_TYPE_NORMAL:
                    tb->s_data->xmm_out =
                        (next_tb ? next_tb->s_data->xmm_in : SHBR_XMM_ALL);
                    break;
                case IR1_TYPE_SYSCALL:
                    tb->s_data->xmm_out = SHBR_XMM_ALL;
                    break;
                case IR1_TYPE_CALLIN:
                case IR1_TYPE_JUMPIN:
                    tb->s_data->xmm_out = SHBR_XMM_ALL;
                    break;
                case IR1_TYPE_RET:
                    tb->s_data->xmm_out = SHBR_XMM_ALL;
                    /* tb->s_data->xmm_out = 0x3; */
                    break;
                default:
                    lsassert(0);
            }
            get_xmm_in(tb, xmm[i]);
            if (old_live_in != tb->s_data->xmm_in || old_live_out != tb->s_data->xmm_out) {
                continue_flag = true;
            }
        }

    }

    /* bool need_print_tu = false; */
    for (int i = 0; i < tb_num_in_tu; i++) {
        TranslationBlock *tb = tb_list[i];
        if (tb->s_data->shbr_type != SHBR_NTYPE) {
            IR1_INST *ir1 = NULL;
            uint32_t no_opt_xmm = tb->s_data->xmm_out & SHBR_XMM_MASK;
            for (int j = tb_ir1_num(tb) - 1; j >= 0; j--) {
                /* fprintf(stderr, "%x\n", tb->s_data->xmm_out); */
                ir1 = tb_ir1_inst(tb, j);
                uint8_t curr = get_inst_type(ir1);
                if (curr == SHBR_NTYPE && !has_implicit_semantic(ir1)) {
                    continue;
                }
                uint16_t live_after = no_opt_xmm;
                if (ir1_get_opnd_num(ir1) > 0) {
                    IR1_OPND *opnd0 = ir1_get_opnd(ir1, 0);
                    if (shbr_opnd_is_vector(opnd0)) {
                        uint16_t dest = shbr_opnd_mask(opnd0);
                        if (shbr_dest_is_dead(live_after, dest,
                                ir1->shbr_read)) {
                            ir1->hbr_flag |= opt_flag;
                        }
                    }
                }
                no_opt_xmm = shbr_live_before(live_after, ir1->shbr_def,
                        ir1->shbr_dep, ir1->shbr_read);
            }
        }

    }
    /* if (need_print_tu) { */
    /*     for (int i = 0; i < tb_num_in_tu; i++) { */
    /*         fprintf(stderr, "tb i %d\n", i); */
    /*         print_ir1(tb_list[i]); */
    /*     } */
    /* } */
}

static void do_shbr_opt32(TranslationBlock **tb_list, int tb_num_in_tu)
{
    uint32_t xmm[tb_num_in_tu][XMM_NUM];
    for (int i = 0; i < tb_num_in_tu; i++) {
        TranslationBlock *tb = tb_list[i];
        tb_xmm_analyse(tb, xmm_analyse_32, xmm[i]);
        tb->s_data->xmm_in = tb->s_data->xmm_use;
        tb->s_data->xmm_out = 0;
    }
    over_tb_shbr_opt(tb_list, tb_num_in_tu, SHBR_CAN_OPT32, xmm);
}

static void do_shbr_opt64(TranslationBlock **tb_list, int tb_num_in_tu)
{
    uint32_t xmm[tb_num_in_tu][XMM_NUM];
    for (int i = 0; i < tb_num_in_tu; i++) {
        TranslationBlock *tb = tb_list[i];
        tb_xmm_analyse(tb, xmm_analyse_64, xmm[i]);
        tb->s_data->xmm_in = tb->s_data->xmm_use;
        tb->s_data->xmm_out = 0;
    }
    over_tb_shbr_opt(tb_list, tb_num_in_tu, SHBR_CAN_OPT64, xmm);
}

static void clear_ir1_flag(TranslationBlock **tb_list, int tb_num_in_tu)
{
    for (int i = 0; i < tb_num_in_tu; i++) {
        TranslationBlock *tb = tb_list[i];
        IR1_INST *ir1 = NULL;
        for (int j = 0; j < tb_ir1_num(tb); j++) {
            ir1 = tb_ir1_inst(tb, j);
            ir1->hbr_flag = 0;
        }
    }
}

#ifdef TARGET_X86_64
static void over_tb_gpr_opt(TranslationBlock **tb_list, int tb_num_in_tu)
{
    TranslationBlock *next_tbs[tb_num_in_tu];
    TranslationBlock *target_tbs[tb_num_in_tu];
    for (int i = 0; i < tb_num_in_tu; i++) {
        TranslationBlock *tb = tb_list[i];
        next_tbs[i] = hbr_in_tu_successor(tb_list, tb_num_in_tu,
            (TranslationBlock *)tb->s_data->next_tb[TU_TB_INDEX_NEXT]);
        target_tbs[i] = hbr_in_tu_successor(tb_list, tb_num_in_tu,
            (TranslationBlock *)tb->s_data->next_tb[TU_TB_INDEX_TARGET]);
    }

    bool continue_flag = true;
    uint32_t old_live_in, old_live_out;
    while(continue_flag) {
        continue_flag = false;
        for (int i = tb_num_in_tu - 1; i >= 0; i--) {
            TranslationBlock *tb = tb_list[i];
            old_live_in = tb->s_data->gpr_in;
            old_live_out = tb->s_data->gpr_out;
            TranslationBlock *next_tb = next_tbs[i];
            TranslationBlock *target_tb = target_tbs[i];
            switch (tb->s_data->last_ir1_type) {
                case IR1_TYPE_BRANCH:
                    tb->s_data->gpr_out =
                        (next_tb ? next_tb->s_data->gpr_in : GHBR_GPR_ALL);
                    tb->s_data->gpr_out |=
                        (target_tb ? target_tb->s_data->gpr_in : GHBR_GPR_ALL);
                    break;
                case IR1_TYPE_JUMP:
                case IR1_TYPE_CALL:
                    tb->s_data->gpr_out =
                        (target_tb ? target_tb->s_data->gpr_in : GHBR_GPR_ALL);
                    break;
                case IR1_TYPE_NORMAL:
                    tb->s_data->gpr_out =
                        (next_tb ? next_tb->s_data->gpr_in : GHBR_GPR_ALL);
                    break;
                case IR1_TYPE_SYSCALL:
                    tb->s_data->gpr_out = GHBR_GPR_ALL;
                    break;
                case IR1_TYPE_CALLIN:
                case IR1_TYPE_JUMPIN:
                    tb->s_data->gpr_out = GHBR_GPR_ALL;
                    break;
                case IR1_TYPE_RET:
                    tb->s_data->gpr_out = GHBR_GPR_ALL;
                    break;
                default:
                    lsassert(0);
            }

            tb->s_data->gpr_in = tb->s_data->gpr_use |
                (tb->s_data->gpr_out & ~tb->s_data->gpr_def);

            if (old_live_in != tb->s_data->gpr_in || old_live_out != tb->s_data->gpr_out) {
                continue_flag = true;
            }
        }
    }

    for (int i = 0; i < tb_num_in_tu; i++) {
        TranslationBlock *tb = tb_list[i];
        uint32_t gpr_out = tb->s_data->gpr_out;
        IR1_INST *ir1;
        for (int j = tb_ir1_num(tb) - 1; j >= 0; j--) {
            ir1 = tb_ir1_inst(tb, j);
            if (ir1->gpr_def & gpr_out) {
                gpr_out &= ~ir1->gpr_def;
            } else if(ir1->gpr_def) {
                /* fprintf(stderr, "%x %x\n", ir1->gpr_def, gpr_out); */
                ir1->hbr_flag |= GHBR_CAN_OPT;
            }
            gpr_out |= ir1->gpr_use;
        }
    }
}

static void des_def_gpr(TranslationBlock *tb, IR1_INST *ir1)
{
    IR1_OPND *des_opnd = ir1_get_opnd(ir1, 0);
    if (!ir1_opnd_is_gpr(des_opnd)) {
        return;
    }
    int dest_num = ir1_opnd_base_reg_num(des_opnd);
    assert(dest_num >= 0 && dest_num < 16);
    ir1->gpr_def |= 1 << dest_num;
    tb->s_data->gpr_def |= 1 << dest_num;
}

static void deal_hide_opnd_def(TranslationBlock *tb, IR1_INST *ir1)
{
    switch (ir1_opcode(ir1)) {
    case WRAP(CWDE):
        ir1->gpr_def |= 1 << eax_index;
        tb->s_data->gpr_def |= 1 << eax_index;
        break;
    case WRAP(CQO):
        ir1->gpr_def |= 1 << edx_index;
        tb->s_data->gpr_def |= 1 << edx_index;
        break;
    case WRAP(POPAW):
        ir1->gpr_def |= 0xff & ~esp_index;
        tb->s_data->gpr_def |= 0xff & ~esp_index;
        break;
    default:
        break;
    }
}

/* Some ins can update the h32 bits of des opnd
 * without using the h32 bits of des opnd. */
static bool def_h32(TranslationBlock *tb, IR1_INST *ir1)
{
    deal_hide_opnd_def(tb, ir1);

    if (!ir1_get_opnd_num(ir1)) {
        return false;
    }
    IR1_OPND *des_opnd = ir1_get_opnd(ir1, 0);
    if (!ir1_opnd_is_gpr(des_opnd)) {
        return false;
    }

    switch (ir1_opcode(ir1)) {
    case WRAP(MOVSX):
    case WRAP(MOVZX):
    /* case WRAP(MOVSXD): */
        if (ir1_opnd_size(des_opnd) == 64 &&
                ir1_opnd_size(ir1_get_opnd(ir1, 1)) == 32) {
            des_def_gpr(tb, ir1);
            return true;
        }
        return false;
    case WRAP(MOV):
        if ((ir1_opnd_size(ir1_get_opnd(ir1, 0)) == 64)
                && ir1_opnd_size(des_opnd) == 64) {
            des_def_gpr(tb, ir1);
            return true;
        }
        return false;
    case WRAP(MOVD):
        if (ir1_opnd_is_xmm(ir1_get_opnd(ir1, 1))) {
            des_def_gpr(tb, ir1);
            return true;
        }
        return false;
    default:
        break;
    }

    if (ir1_opnd_size(des_opnd) != 32) {
        return false;
    }
    switch (ir1_opcode(ir1)) {
    case WRAP(XOR):
    case WRAP(AND):
    case WRAP(OR):
    case WRAP(NOT):
    case WRAP(ROL):
    case WRAP(ROR):
    case WRAP(RCL):
    case WRAP(RCR):
    case WRAP(SHRD):
    case WRAP(SHLD):
    case WRAP(ADD):
    case WRAP(ADC):
    case WRAP(INC):
    case WRAP(DEC):
    case WRAP(SUB):
    case WRAP(SBB):
    case WRAP(NEG):
    case WRAP(XADD):
        des_def_gpr(tb, ir1);
        return true;
    case WRAP(XCHG):
        if (ir1_opnd_is_same_reg(des_opnd, ir1_get_opnd(ir1, 1))) {
            des_def_gpr(tb, ir1);
            return true;
        }
        return false;
    default:
        return false;
    }

    return false;
}

static void set_use_reg(TranslationBlock *tb, IR1_INST *ir1, int reg_num)
{
    if (reg_num >= 0 && reg_num < 16) {
        ir1->gpr_use |= 1 << reg_num;
        /* Need pre tb provide if curr tb  not def this reg. */
        if (!(tb->s_data->gpr_def & (1 << reg_num))) {
            tb->s_data->gpr_use |= 1 << reg_num;
        }
    }
}

static void deal_hide_opnd_use(TranslationBlock *tb, IR1_INST *ir1)
{
    switch (ir1_opcode(ir1)) {
    case WRAP(CMPXCHG):
    case WRAP(SALC):
    case WRAP(CWD):
    case WRAP(CDQ):
    case WRAP(CBW):
    case WRAP(CWDE):
    case WRAP(CDQE):
        set_use_reg(tb, ir1, eax_index);
        break;
    case WRAP(XLATB):
        set_use_reg(tb, ir1, ebx_index);
        break;
    case WRAP(JRCXZ):
    case WRAP(JCXZ):
    case WRAP(JECXZ):
    case WRAP(LOOPE):
    case WRAP(LOOPNE):
    case WRAP(LOOP):
        set_use_reg(tb, ir1, ecx_index);
        break;
    case WRAP(MASKMOVQ):
    case WRAP(MASKMOVDQU):
        set_use_reg(tb, ir1, edi_index);
        break;
    case WRAP(POPF):
    case WRAP(POP):
    case WRAP(PUSHF):
    case WRAP(PUSH):
    case WRAP(CALL):
    case WRAP(RET):
    case WRAP(RETF):
    case WRAP(IRET):
    case WRAP(IRETQ):
        set_use_reg(tb, ir1, esp_index);
        break;
    case WRAP(LODSB):
    case WRAP(LODSQ):
    case WRAP(LODSW):
    case WRAP(LODSD):
        set_use_reg(tb, ir1, esi_index);
        set_use_reg(tb, ir1, ecx_index);
        break;
    case WRAP(MUL):
    case WRAP(DIV):
    case WRAP(IDIV):
    case WRAP(RDTSC):
    case WRAP(CQO):
        set_use_reg(tb, ir1, eax_index);
        break;
    case WRAP(IMUL):
        if (ir1_opnd_num(ir1) == 1) {
            set_use_reg(tb, ir1, eax_index);
            set_use_reg(tb, ir1, edx_index);
        }
        break;
    case WRAP(ENTER):
    case WRAP(LEAVE):
        set_use_reg(tb, ir1, esp_index);
        set_use_reg(tb, ir1, ebp_index);
        break;
    case WRAP(MOVSD):
    case WRAP(CMPSD):
        set_use_reg(tb, ir1, esi_index);
        set_use_reg(tb, ir1, edi_index);
        set_use_reg(tb, ir1, ecx_index);
        break;
    case WRAP(STOSB):
    case WRAP(STOSW):
    case WRAP(STOSD):
    case WRAP(STOSQ):
    case WRAP(SCASB):
    case WRAP(SCASW):
    case WRAP(SCASD):
    case WRAP(SCASQ):
        set_use_reg(tb, ir1, eax_index);
        set_use_reg(tb, ir1, edi_index);
        set_use_reg(tb, ir1, ecx_index);
        break;
    case WRAP(RDTSCP):
        set_use_reg(tb, ir1, eax_index);
        set_use_reg(tb, ir1, ecx_index);
        set_use_reg(tb, ir1, edx_index);
        break;
    case WRAP(CMPXCHG8B):
    case WRAP(CMPXCHG16B):
        set_use_reg(tb, ir1, eax_index);
        set_use_reg(tb, ir1, ebx_index);
        set_use_reg(tb, ir1, ecx_index);
        set_use_reg(tb, ir1, edx_index);
        break;
    case WRAP(INT3):
    case WRAP(PUSHAW):
    case WRAP(PUSHAL):
        ir1->gpr_use |= GHBR_GPR_ALL;
        tb->s_data->gpr_use |= ~tb->s_data->gpr_def;
        break;
    default:
        break;
    }
}

static void use_h32(TranslationBlock *tb, IR1_INST *ir1)
{
    deal_hide_opnd_use(tb, ir1);
    int opnd_num = ir1_get_opnd_num(ir1);
    /* We roughly assume that the high 32 bit of all gpr in curr ins will be used,
     * except for updating their own h32 des opnd without using their own h32 bit. */
    int i = 0;
    if (ir1->gpr_def) {
        i = 1;
    }
    for (; i < opnd_num; ++i) {
        IR1_OPND *opnd = ir1_get_opnd(ir1, i);
        if (ir1_opnd_is_gpr(opnd) && ir1_opnd_size(opnd) == 64) {
            set_use_reg(tb, ir1, ir1_opnd_base_reg_num(opnd));
        } else if (ir1_opnd_is_mem(opnd)) {
            if (ir1_opnd_has_base(opnd)) {
                int base_num = ir1_opnd_base_reg_num(opnd);
                set_use_reg(tb, ir1, base_num);
            }
            if (ir1_opnd_has_index(opnd)) {
                int index_num = ir1_opnd_index_reg_num(opnd);
                set_use_reg(tb, ir1, index_num);
            }
        }
    }
}

/* The current strategy used is relatively conservative, */
/* only including analysis of a small number of instructions.
 * If the strategy is changed to analyze more instructions,
 * better results may be achieved. */
static void get_gpr_use_def(TranslationBlock *tb)
{
    tb->s_data->gpr_use = 0;
    tb->s_data->gpr_def = 0;
    tb->s_data->gpr_out = 0;
    tb->s_data->gpr_in = 0;
    IR1_INST *ir1;
    for (int i = 0; i < tb_ir1_num(tb); ++i) {
        ir1 = tb_ir1_inst(tb, i);
        ir1->gpr_def = 0;
        ir1->gpr_use = 0;
        def_h32(tb, ir1);
        use_h32(tb, ir1);
    }
}

static void do_gpr_opt(TranslationBlock **tb_list, int tb_num_in_tu)
{
    for (int i = 0; i < tb_num_in_tu; i++) {
        get_gpr_use_def(tb_list[i]);
    }
    over_tb_gpr_opt(tb_list, tb_num_in_tu);
}
#endif

void hbr_opt(TranslationBlock **tb_list, int tb_num_in_tu)
{
    clear_ir1_flag(tb_list, tb_num_in_tu);
    do_shbr_opt32(tb_list, tb_num_in_tu);
    do_shbr_opt64(tb_list, tb_num_in_tu);
#ifdef TARGET_X86_64
    do_gpr_opt(tb_list, tb_num_in_tu);
#endif
}

bool can_shbr_opt64(IR1_INST *ir1)
{
    if (!in_pre_translate) {
        return false;
    }
    if (ir1->hbr_flag & SHBR_CAN_OPT64) {
        return true;
    }
    return false;
}

bool can_shbr_opt32(IR1_INST *ir1)
{
    if (!in_pre_translate) {
        return false;
    }
    if (ir1->hbr_flag & SHBR_CAN_OPT32) {
        return true;
    }
    return false;
}

#ifdef TARGET_X86_64
/* static int opt, noopt; */
bool can_ghbr_opt(IR1_INST *ir1)
{
    if (!in_pre_translate) {
        return false;
    }
    if (ir1->hbr_flag & GHBR_CAN_OPT) {
        return true;
    }
    return false;
}
#endif

#undef WRAP
#endif
