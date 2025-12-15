/**
 * @file aot.h
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT header
 */
#ifndef __AOT_H_
#define __AOT_H_

#include "qemu-def.h"
#include "qemu/cutils.h"
#include "qemu/units.h"
#include "segment.h"
#include "aot_link_seg.h"
#include "ts.h"
#include "aot_lib.h"
extern char pf_table[];
extern  const char *aot_file_size_optarg;
extern  const char *aot_left_file_minsize_optarg;
#define AOT_MAX_CODE_GEN_BUFFER_SIZE (400 * MiB)
/* AOT file layout:
 * +--------------+
 * |  AOT Header  |
 * +--------------+
 * |  AOT Segments|
 * +--------------+
 * |  X86 lib name|
 * +--------------+
 * | AOT TB tables|
 * +--------------+
 * |  Rel tables  |
 * +--------------+
 * |  code caches |
 * +--------------+
 */
#ifdef CONFIG_LATX_DEBUG
#define AOT_VERSION "Version: "LATX_VERSION"-debug"
#else
#define AOT_VERSION "Version: "LATX_VERSION"-release"
#endif
typedef struct aot_header {
    uint32_t lib_size;
    struct timespec last_modify_time;
    uint32_t segment_table_offset;
    uint32_t segments_num;
    uint32_t rel_table_offset; /* relocation table offset in aot file. */
    uint32_t rel_entry_num; /* relocation entries num. */
    uint32_t parallel_tb_num;
    uint32_t unparallel_tb_num;
    uint8_t is_pe;
} aot_header;

typedef struct aot_file_info {
    void *p;
    size_t maplen;
} aot_file_info;

typedef struct page_table_info {
    uint32_t page_tbs_offset;
    int16_t tb_num_in_page;
    uint32_t page_tbs_offset_parallel;
    int16_t tb_num_in_page_parallel;
} page_table_info;

typedef struct aot_segment {
    /* @details.file_offset: segment offset in x86 lib file.
     * @details.seg_bein, @details.segend: its pc ranges in qemu.
     *
     * Note! (lib_flie_name, file_offset) pair is the unique ID of a given
     * segment.
     */
    seg_info details;

    /* x86 library file name(which this segment belongs to) offset in the AOT.
     */
    uint32_t lib_name_offset;

    /* Offset of this segment in AOT. */
    uint32_t segment_tbs_offset;
    uint32_t segment_tbs_num;
    /* Offset of page_table in AOT. */
    uint32_t page_table_offset;
    bool is_pe;
} aot_segment;

typedef struct aot_tb {
    /* inner segment offset of this basic block */
    uint32_t offset_in_segment;

#ifdef CONFIG_LATX_TU
    /*tu_id is the first tb pc in TU */
    uint32_t tu_id;
    uint32_t offset_in_tu;
#endif
    /* Translation Code Cache offset in AOT. */
    void *tb_cache_addr;
    uint32_t tb_cache_offset;
    uint32_t tb_cache_size;
    union {
        uint32_t is_first_tb;
        uint32_t tu_size;
    };
    /* int tb_num; */
    uint16_t icount;
    /* qemu TranslationBlock fields which should be recorded. */
    uint32_t size;
    uint32_t flags;
    uint32_t cflags;
    union {
        uint16_t jmp_reset_offset[2];
        uint16_t tu_jmp[2];
    };
    union {
        uint16_t jmp_stub_reset_offset[2];
        uint32_t tu_search_addr_offset;
    };
    union {
        uintptr_t jmp_target_arg[2];
        struct tu_unlink tu_unlink;
    };
    union {
        uintptr_t jmp_stub_target_arg[2];
        uintptr_t jmp_indirect;
    };
    uint16_t eflags_target_arg[3];
    uint8_t  eflag_use;
    uint16_t bool_flags;
    int32_t rel_start_index;
    int32_t rel_end_index;

    /* segment index of curr tb, its fallthrough tb, and its target tb */
    int segment_idx;
    int fallthru_segment_idx;
    int target_segment_idx;
    uint16_t first_jmp_align;
    uint8_t last_ir1_type;
    uintptr_t return_target_ptr_offset;
    unsigned long next_86_pc_offset;

    int64_t lazypc[2];
    int8_t canlink[2];
} aot_tb;

typedef enum aot_rel_kind {

    B_PROLOGUE,
    B_EPILOGUE,
    B_EPILOGUE_RET_ID_3,
    B_EPILOGUE_RET_ID_1,
    B_EPILOGUE_RET_ID_0,
    JIRL_EPILOGUE_RET_ID_1,
    JIRL_EPILOGUE_RET_ID_0,
    B_EPILOGUE_RET_0,
    B_NATIVE_JMP_GLUE2,

    LOAD_TB_ADDR,
    LOAD_RIP_OFF,
    LOAD_CALL_TARGET,
    LOAD_SYSCALL_OPTIMIZE_CONFIRM,

    /* Helper functions relocation. Assume we need to call a helper in our tb,
     * we'll load its address into a tmp reg and then jmp to that address, but
     * if latx has changed its code and all helper address has changed, all aot
     * tb will need to be relocated. */
    LOAD_HELPER_BEGIN,

    LOAD_HELPER_TRACE_SESSION_BEGIN,
    LOAD_HELPER_UPDATE_MXCSR_STATUS,
    LOAD_HELPER_FPATAN,
    LOAD_HELPER_FPTAN,
    LOAD_HELPER_FPREM,
    LOAD_HELPER_FPREM1,
    LOAD_HELPER_FRNDINT,
    LOAD_HELPER_F2XM1,
    LOAD_HELPER_FXTRACT,
    LOAD_HELPER_FYL2X,
    LOAD_HELPER_FYL2XP1,
    LOAD_HELPER_FSINCOS,
    LOAD_HELPER_FSIN,
    LOAD_HELPER_FCOS,
    LOAD_HELPER_FBLD_ST0,
    LOAD_HELPER_FBST_ST0,
    LOAD_HELPER_FXSAVE,
    LOAD_HELPER_FXRSTOR,
    LOAD_HELPER_CONVERT_FPREGS_X80_TO_64,
    LOAD_HELPER_CONVERT_FPREGS_64_TO_X80,
    LOAD_HELPER_UPDATE_FP_STATUS,
    LOAD_HELPER_CPUID,
    LOAD_HELPER_RAISE_INT,
    LOAD_HELPER_RAISE_ILLOP,
    LOAD_HELPER_RAISE_TRAPOP,
    LOAD_HELPER_RAISE_GPF,
    LOAD_HELPER_RAISE_INTO,
    LOAD_HELPER_RAISE_BOUND,
    LOAD_HELPER_RAISE_SYSCALL,
    LOAD_HELPER_XGETBV,
    LOAD_HELPER_EFLAGTF,
    LOAD_HELPER_SMC_ST,
    LOAD_HELPER_SMC_VST,
    LOAD_HELPER_SMC_VST_X4,

    LOAD_HOST_POW,
    LOAD_HOST_SIN,
    LOAD_HOST_COS,
    LOAD_HOST_ATAN2,
    LOAD_HOST_LOGB,
    LOAD_HOST_LOG2,
    LOAD_HOST_SINCOS,
    LOAD_HOST_PFTABLE,
    LOAD_HOST_LATLOCK,
    LOAD_HOST_RAISE_EX,
    LOAD_PAGEFLAGS_ROOT,

    LOAD_HELPER_END,

    LOAD_TUNNEL_ADDR_BEGIN,
} aot_rel_kind;

typedef struct aot_rel {
    aot_rel_kind kind;

    /* @tc_offset: Offset of this relocation in translated code.
     * @rel_slot_num: How many instruction slots this relocation occupy.
     */
    uint32_t tc_offset;
    uint32_t rel_slots_num;

    /* @x86_rip_offset: Offset of this X86 instruction in x86 baisc block.
     * @extra_addend: extra addend to this relocation.
     *
     * For example, if we want to reloacate *0xdeadbeef(rip)*, we'll fill
     * relocation slots with (rip + 0xdeadbeef). rip can be determined by
     * segment_base_addr + @x86_rip_offset while 0xdeadbeef should be
     * determined by @extra_addend.
     */
    uint32_t x86_rip_offset;
    target_ulong extra_addend;

} aot_rel;

extern aot_rel *rel_table;
extern seg_info **seg_info_vector;
void mk_aot_dir(char * pathname);
void dump_aot_buffer(aot_header *p_header);
void dump_seg(aot_segment *p_segment, aot_header *p_header);
lib_info *aot_load(char *lib_name);
void aot_tb_register(TranslationBlock *tb);
void aot_do_tb_reloc(TranslationBlock *tb, struct aot_tb *stb,
    target_ulong seg_begin, target_ulong seg_end);
int aot_get_file_name(char *aot_file, char *buff, int index);
int add_rel_entry(aot_rel_kind kind, uint32_t **tc_offset,
                  uint32_t **rel_slots_num, uint32_t x86_rip_offset,
                  target_ulong extra_addent);
void aot_exit_entry(CPUState *cpu, int is_end);
void aot_init(void);
target_ulong aot_get_call_offset(ADDRX addr);
void aot_generate(CPUState *cpu);
int aot_get_file_init(char *aot_file);
void clear_rel_table(void);
void recover_aot_tb(char *lib_name, uint64_t aot_offset,
        abi_long start, abi_long len);

void do_generate_aot(int first_seg_in_lib, int end_seg_in_lib);
struct aot_segment *aot_find_segment(char *path, int offset);

void recover_tb(char *buf, uint64_t aot_offset, abi_long start,
        abi_long len);

#ifdef CONFIG_LATX_TU
void fix_rel_entry(int fix_id, uint32_t tc_offset);
#endif

#define TB_SET_REL(tb, rel_index)                   \
    do {                                            \
        if ((tb)->s_data->rel_start == -1) {                \
            (tb)->s_data->rel_start = rel_index;            \
            (tb)->s_data->rel_end = rel_index;              \
        } else {                                    \
            assert((tb)->s_data->rel_end + 1 == rel_index); \
            (tb)->s_data->rel_end = rel_index;              \
        }                                           \
    } while (0)


extern void *aot_buffer;
extern char *aot_file_path;
extern char *aot_file_lock;
extern aot_rel *aot_rel_table;
extern aot_file_info *aot_buffer_all;
extern int aot_buffer_all_num;

void get_aot_path(const char *lib_name, char *file_path);
char in_share_list(char *lib);
char in_black_list(char *lib);
char in_white_list(char *lib);
void set_lock_name(char *aot_file_path);

#endif
