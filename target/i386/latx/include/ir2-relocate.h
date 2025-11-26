#ifndef _IR2_RELOCATE_H_
#define _IR2_RELOCATE_H_
#include <stdint.h>
#include "ir2.h"
#include "env.h"
#include "qemu-def.h"

/**
 * @brief Add pseudo inst to hint align
 *
 * @param align align size (byte), recommend {2^n}
 * @param filling filling data/code in aligned space
 */
IR2_INST *la_code_align(int align, uint32_t filling);

/**
 * @brief generate a data load imm inst
 * @details
 * This function is used to generate a pseudo-instruction that load imm data (64
 * bits) into data area. This data area is different from temp register, which
 * is only a translate-time data, similar to the label. This data_temp will only
 * be used at translate time, so *DO NOT* use it as a runtime temp!
 * @note This is a pseudo-instruction, but will generate some real instructions.
 * @param data_temp data temp opnd
 * @param data storage data
 * @return IR2_INST* this ir2_inst
 * @link ra_alloc_data
 */
IR2_INST *la_data_li(IR2_OPND data_temp, uint64_t data);

/**
 * @brief generate a data add inst
 * @details
 * This function is used to generate a pseudo-instruction that do add function.
 * The data/label can use this inst to do add operation.
 * - If src1/2 is DATA, it will use the data in memory (need load data by using
 * la_data_li).
 * - If src1/2 is LABEL, it will use the offset (from tc.ptr).
 * - If src1/2 is PSEUDO, it will use the IR2_OPND value (x.val)
 * @note This is a pseudo-instruction, will not generate any real instruction.
 * @param dest dest opnd, must be DATA type
 * @param src1 src1, must be DATA/LABEL/PSEUDO type
 * @param src2 src2, must be DATA/LABEL/PSEUDO type
 * @return IR2_INST* this ir2_inst
 */
IR2_INST *la_data_add(IR2_OPND dest, IR2_OPND src1, IR2_OPND src2);

/**
 * @brief generate a inst for calculate the number of insts in 2 labels
 * @details
 * This function is used to generate a pseudo-instruction that do count within 2
 * labels. This maybe do below:
 * " dest = INST_NUM(src1 - src2) "
 * So, if this the data, it will: " dest = (data[src1] - data[src2]) >> 2 "
 *
 * @param dest dest opnd, must be DATA type
 * @param label1 subtracted, must be DATA/LABEL type
 * @param label2 subtraction, must be DATA/LABEL type
 * @return IR2_INST* this ir2_inst
 */
IR2_INST *la_inst_diff(IR2_OPND dest, IR2_OPND label1, IR2_OPND label2);

/**
 * @brief generate far jump insts (usually 2 insts)
 * @details
 * This function is used to hint to generate a far jump stub. The stub usually
 * contain 2 insts, 'b+nop' or 'pcaddu18i+jirl'.
 * First opnd 'jump_target' is jump target (absolute address). Second opnd
 * 'counter_base' is TB/TU first inst absolute address, which is the address
 * when 'counter = 1'.
 * The generater will use under algorithm to caculate offset:
 * <jump_target - counter_base - counter * 4>
 * So, two opnd *MUST* be IR2_OPND_DATA
 * @note This is a pseudo-instruction, but will generate some real instructions.
 * @param jmp_target jump target address
 * @param counter_base base address, for insts counter (or say address when
 * counter = 1)
 * @return IR2_INST* this ir2_inst
 */
IR2_INST *la_far_jump(IR2_OPND jmp_target, IR2_OPND counter_base);

/**
 * @brief generate pcaddi inst
 * @details
 * This function is used to hint to generate a pcaddi inst.
 * First opnd 'rd' is pcaddi dest. Second opnd 'jump_target' is jump
 * target (absolute address). Last opnd 'counter_base' is TB/TU first
 * inst absolute address.
 * The generater will use under algorithm to caculate offset:
 * <jump_target - counter_base - counter * 4>
 * So, two opnd *MUST* be IR2_OPND_DATA
 * @note This is a pseudo-instruction, but will generate some real instructions.
 * @param jmp_target jump target address
 * @param counter_base base address, for insts counter (or say address when
 * counter = 1)
 * @return IR2_INST* this ir2_inst
 */
IR2_INST *la_pcaddi_relocate(IR2_OPND rd, IR2_OPND jmp_target, IR2_OPND counter_base);

/**
 * @brief generate translator data store operation
 * @details
 * This function is used to hint the relocator to save the data into translator
 * environment struct area. The opration will like:
 * " *(uint64_t *)mem = data_temp "
 * We recommend this sequence:
 * * IR2_OPND data = ra_alloc_data();
 * * IR2_OPND env = ra_alloc_data();
 * * la_data_li(data, imm); // load the data which want to save
 * * la_data_li(env, struct_ptr); // load translator struct data address
 * * la_data_st(env, data); // store!
 * @note This is a pseudo-instruction, will not generate any real instruction.
 * @param mem translator environment struct area, must be DATA type
 * @param data data which want to store, must be DATA/LABEL type
 * @return IR2_INST* this ir2_inst
 */
IR2_INST *la_data_st(IR2_OPND mem, IR2_OPND data);
IR2_INST *la_data_st_rel_table(uint32_t table_index,
    uint32_t offset, IR2_OPND data_temp);

/**
 * @brief generate profile begin/end pseudo-instruction
 * @details
 * This is a pseudo-instruction mark profile part begin/end, which did not count
 * the number of instructions in this area.
 */
#define PROFILE_BEGIN (0)
#define PROFILE_END (1)
IR2_INST *la_profile_begin(void);
IR2_INST *la_profile_end(void);

/**
 * @brief ir2 relocate worker
 * @details
 * 0. always do insts count
 * 1. align code create
 * 2. ???
 *
 * @param lat_ctx lat translate context
 * @param current current ir2 inst
 * @param counter inst counter, maybe add 1 (for pseudo inst turn to real inst)
 * @param label input label information
 * @param data input data information
 * @return currnet ptr of IR2_INST.
 * @note return is wanted because this function may cause ir2 expansion, which
 * due to realloc(). This may cause some error if you use ptr in the outer loop
 * to record the current information.
 * @link label_dispose
 */
IR2_INST *ir2_relocate(TRANSLATION_DATA *lat_ctx, IR2_INST *current, int *counter,
                  int *label, uint64_t *data);

/**
 * @brief generate hardcode instruction
 * @details
 * This function will generate a pseudo-instruction that will enable the asm
 * backend to generate the code given by this function. For example, if you want
 * to make a hardcode somewhere, you can use this function to create an ir2_inst
 * and insert it in the appropriate place, and then the backend will generate
 * this hardcode to that place.
 *
 * this place.
 * @param code the hardcode
 * @return IR2_INST* this ir2_inst
 */
IR2_INST *generate_code(uint32_t code);

#endif
