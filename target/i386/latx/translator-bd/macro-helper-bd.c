/**
 * @file macro-helper.c
 * @author Hanlu Li <heuleehanlu@gmail.com>
 * @brief Add some macro-inst helper functions
 */
#include "translate-bd.h"
#include "reg-alloc.h"

/**
* @brief imm_helper - The base interface that load immediate to gpr
*
* @param opnd2 - the dest opnd
* @param hi32
* @param lo32
*/
static void imm_helper(IR2_OPND opnd2, uint32_t hi32, uint32_t lo32)
{
    /*
     * We pay attention to sign extend beacause it is chance of
     * reduce insn. The exception is 12-bit and hi-12-bit unsigned,
     * we need a 'ori' or a 'lu52i.d' accordingly.
     */
    char sign_bitmap, all0_bitmap, allf_bitmap;
    char sign_extension_by_previous;
    char extension_by_previous_hi, extension_by_previous_lo;

    /*
     * We divided the imm64 into the following four parts and used
     * bitmap to represent the status of each part.
     *
     * | imm64      | bitmap |       |
     * | ---------- | ------ | ----- |
     * | imm[11:0]  | bit0   | part0 |
     * | imm[31:12] | bit1   | part1 |
     * | imm[51:32] | bit2   | part2 |
     * | imm[63:52] | bit3   | part3 |
     *
     */
    sign_bitmap = (((hi32 & 0x80000000) != 0) << 3) |
                  (((hi32 & 0x00080000) != 0) << 2) |
                  (((lo32 & 0x80000000) != 0) << 1) |
                  ((lo32 & 0x00000800) != 0);
    /* 1 means that this part equals 0 */
    all0_bitmap = (((hi32 & 0xfff00000) == 0) << 3) |
                  (((hi32 & 0x000fffff) == 0) << 2) |
                  (((lo32 & 0xfffff000) == 0) << 1) |
                  ((lo32 & 0x00000fff) == 0);
    /* 1 means that this part equals -1 */
    allf_bitmap = (((hi32 & 0xfff00000) == 0xfff00000) << 3) |
                  (((hi32 & 0x000fffff) == 0x000fffff) << 2) |
                  (((lo32 & 0xfffff000) == 0xfffff000) << 1) |
                  ((lo32 & 0x00000fff) == 0x00000fff);

    /*
     * Except part0, if the part is equal to 0 or -1, it may be a
     * sign extension of the previous part.
     *
     * bit0 equals 1 means part0 equals 0.
     */
    sign_extension_by_previous = (all0_bitmap ^ allf_bitmap) &
                                 (all0_bitmap ^ (sign_bitmap << 1));
    extension_by_previous_lo = sign_extension_by_previous & 0x3;
    extension_by_previous_hi = sign_extension_by_previous >> 2;

    /* the fast path */
    if (sign_extension_by_previous == 0x7) {
        /*
         * xxx 00000 00000 000
         * part0, part1 and part2 = 0, we just load part3
         */
        la_lu52i_d(opnd2, zero_ir2_opnd, hi32 >> 20);
        return;
    }

    /*
     * load lo32
     */
    switch (extension_by_previous_lo) {
    case 0x0:
        /*
         * lo32: 00000/xxxxx xxx
         */
        if ((all0_bitmap & 0x3) == 0x2) {
            /*
             * lo32: 00000 xxx
             * part1 = 0, we just load part0
             */
            la_ori(opnd2, zero_ir2_opnd, lo32 & 0xfff);
        } else {
            /*
             * lo32: xxxxx xxx
             */
            la_lu12i_w(opnd2, lo32 >> 12);
            la_ori(opnd2, opnd2, lo32 & 0xfff);
        }
        break;
    case 0x1:
        /*
         * lo32: xxxxx 000
         */
        la_lu12i_w(opnd2, lo32 >> 12);
        break;
    case 0x2:
        /*
         * lo32: 00000/fffff xxx
         * part0 != 0, part1 is sign extension of the part0
         */
        la_addi_w(opnd2, zero_ir2_opnd, lo32);
        break;
    case 0x3:
        /*
         * lo32: 00000 000
         */
        la_or(opnd2, zero_ir2_opnd, zero_ir2_opnd);
        break;
    }

    /*
     * load hi32:
     * now, the lower 32 bits of the imm64 are loaded, and the
     * higher 32 bits of the opnd2 are signed extensions of the
     * lower.
     */
    switch (extension_by_previous_hi) {
    case 0x0:
        /*
         * hi32: xxx xxxxx
         */
        la_lu32i_d(opnd2, hi32 & 0xfffff);
        la_lu52i_d(opnd2, opnd2, hi32 >> 20);
        break;
    case 0x1:
        /*
         * hi32: xxx 00000/fffff
         */
        la_lu52i_d(opnd2, opnd2, hi32 >> 20);
        break;
    case 0x2:
        /*
         * hi32: 000/fff xxxxx
         */
        la_lu32i_d(opnd2, hi32 & 0xfffff);
        break;
    case 0x3:
        /*
         * hi32: 00000000/ffffffff
         */
        break;
    }
}

/**
* @brief li_d - macro instruction, semantic equivalent to li.d rd, {s,u}64
*
* @param opnd2 - the dest opnd
* @param value
*/
void li_d(IR2_OPND opnd2, int64_t value)
{
    uint32_t hi32, lo32;

    lo32 = value & 0xffffffff;
    hi32 = value >> 32;
    imm_helper(opnd2, hi32, lo32);
}

/**
* @brief li_w - macro instruction, semantic equivalent to li.d rd, s32
*
* @param opnd2 - the dest opnd
* @param lo32
*/
void li_w(IR2_OPND opnd2, uint32_t lo32)
{
    uint32_t hi32;

    hi32 = lo32 & 0x80000000 ? 0xffffffff : 0;
    imm_helper(opnd2, hi32, lo32);
}

/**
* @brief li_wu - macro instruction, semantic equivalent to li.d rd, u32
*
* @param opnd2 - the dest opnd
* @param lo32
*/
void li_wu(IR2_OPND opnd2, uint32_t lo32)
{
    imm_helper(opnd2, 0, lo32);
}

/**
* @brief la_ld_by_op_size - load by opnd size
*
* @param rd
* @param rj
* @param imm_si12
* @param op_size
*/
void la_ld_by_op_size(IR2_OPND rd, IR2_OPND rj, int imm_si12, int op_size)
{
    switch (op_size) {
    case 8:
        la_ld_b(rd, rj, imm_si12);
        break;
    case 16:
        la_ld_h(rd, rj, imm_si12);
        break;
    case 32:
        la_ld_w(rd, rj, imm_si12);
        break;
    case 64:
        la_ld_d(rd, rj, imm_si12);
        break;
    default:
        lsassertm(0, "op_size error!");
        break;
    }
}

/**
* @brief la_st_by_op_size - store by opnd size
*
* @param rd
* @param rj
* @param imm_si12
* @param op_size
*/
void la_st_by_op_size(IR2_OPND rd, IR2_OPND rj, int imm_si12, int op_size)
{
    switch (op_size) {
    case 8:
        la_st_b(rd, rj, imm_si12);
        break;
    case 16:
        la_st_h(rd, rj, imm_si12);
        break;
    case 32:
        la_st_w(rd, rj, imm_si12);
        break;
    case 64:
        la_st_d(rd, rj, imm_si12);
        break;
    default:
        lsassertm(0, "op_size error!");
        break;
    }
}
