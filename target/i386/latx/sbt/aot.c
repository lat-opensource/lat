/**
 * @file aot.c
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT optimization
 */
#include "tb-dump.h"
#include "qemu-def.h"
#include "segment.h"
#include "aot.h"
#include <stdlib.h>
#include <math.h>
#include "latx-options.h"
#include "translate.h"
#include "tunnel_lib.h"
#include "qemu.h"
#include "lsenv.h"
#include "file_ctx.h"
#include "aot_merge.h"
#include "aot_recover_tb.h"
#include "aot_smc.h"
#include "aot_page.h"
#include<sys/syscall.h>
#include "exec/translate-all.h"
#ifdef CONFIG_LATX_AOT
/* Tbs vector with @tb_num@ elements. */
static TranslationBlock **tb_vector;
static int tb_num;
static int curr_lib_tb_num;
/* Compare function used to sort tb_vector. */
static int tb_cmp(const void *a, const void *b)
{
    TranslationBlock *pa = *(TranslationBlock **)a;
    TranslationBlock *pb = *(TranslationBlock **)b;
    if (pa->pc > pb->pc) {
        return 1;
    } else if (pa->pc < pb->pc) {
        return -1;
    }
    uint32_t a_cflags = pa->cflags & CF_PARALLEL;
    uint32_t b_cflags = pb->cflags & CF_PARALLEL;
    return a_cflags > b_cflags ? 1 : (a_cflags < b_cflags ? -1 : 0);
}

/* Segment info vector with @seg_num@ elements. */
seg_info **seg_info_vector;
static int seg_info_num;
static int curr_lib_seg_num;
static char *curr_lib_name;
uintptr_t table_end_addr;
/* Helper functions used to dump tbs in hash table to a vector */
static void do_tb_count(void *p, uint32_t hash, void *userp)
{
    tb_num++;
}

static char not_required_tb(TranslationBlock *tb) 
{
    if (tb == NULL
            || tb->icount == 0
            || (tb->bool_flags & (IS_AOT_TB | IS_TUNNEL_LIB))
            || (tb->cflags & CF_INVALID)) {
        return true;
    }
    if (page_get_page_state(tb->pc) == PAGE_SMC) {
        return true;
    }
    return false;
}

static int record_index;
/* static target_ulong pre_tb_pc; */
static void do_tb_record(void *p, uint32_t hash, void *userp)
{
    assert(record_index < tb_num);
    TranslationBlock *tb = (TranslationBlock *)p;
    if (not_required_tb(tb)) {
        tb_num--;
        return;
    }
    tb_vector[record_index++] = (TranslationBlock *)p;
}

/* Basic Block relocation table with @rel_entry_num elements. */
aot_rel *rel_table;
static int rel_entry_num;
static int rel_table_capacity;
int add_rel_entry(aot_rel_kind kind, uint32_t **tc_offset,
                  uint32_t **rel_slots_num, uint32_t x86_rip_offset,
                  target_ulong extra_addend)
{
    if (!in_pre_translate) {
        assert(0);
    }
    if (rel_table_capacity == 0) {
        rel_table_capacity = 10000;
        rel_entry_num = 0;
        rel_table = (aot_rel *)malloc(rel_table_capacity * sizeof(aot_rel));
        assert(rel_table);
    } else if (rel_table_capacity == rel_entry_num) {
        rel_table_capacity <<= 1;
        rel_table =
            (aot_rel *)realloc(rel_table, rel_table_capacity * sizeof(aot_rel));
    }
    int i = rel_entry_num++;
    assert(rel_entry_num <= rel_table_capacity);
    rel_table[i].kind = kind;
    rel_table[i].x86_rip_offset = x86_rip_offset;
    rel_table[i].extra_addend = extra_addend;
    /* wait for patching by "ir2_relocate" */
    *tc_offset = &rel_table[i].tc_offset;
    *rel_slots_num = &rel_table[i].rel_slots_num;
    return i;
}

#ifdef CONFIG_LATX_TU
void fix_rel_entry(int fix_id, uint32_t tc_offset)
{
    rel_table[fix_id].tc_offset = tc_offset;
}
#endif

/* Position where aot file is loaded. */
void *aot_buffer;
aot_file_info *aot_buffer_all;
int aot_buffer_all_num;
/* Where the aot relocation table is loaded in buffer. */
aot_rel *aot_rel_table;
uint32_t aot_rel_entry_num;

void mk_aot_dir(char * pathname)
{
    for (int i = 0; i < strlen(pathname); i++) {
	if (pathname[i] == '/') {
	    int ret = 0;
	    pathname[i] = '\0';
	    if (strlen(pathname) && access(pathname, R_OK) < 0) {
	    	ret = mkdir(pathname, 0777);
		if (ret) {
		    qemu_log_mask(LAT_LOG_AOT, "%s errno %d, pathname %s\n",
		    __func__, errno, pathname);
	    	}
	    }
	    pathname[i] = '/';
	}
    }
}

/*
 * Prepare all TBs which will be handled later.
 * 1. Calculate TBs number.
 * 2. Allocate memory to store these TBs.
 * 3. Sort this vector and remove SMC.
 * 4. Remove duplicate TB.
*/
static void get_tb(void)
{
    tb_num = 0;
    record_index = 0;
    qht_iter(&tb_ctx.htable, do_tb_count, NULL);
    tb_vector =
        (TranslationBlock **)malloc(tb_num * sizeof(TranslationBlock *));
    assert(tb_vector);
    qht_iter(&tb_ctx.htable, do_tb_record, NULL);
    if (tb_num == 0) {
        return;
    }
    qsort(tb_vector, tb_num, sizeof(TranslationBlock *), tb_cmp);
}

/* Prepare segment infomation. */ 
static void get_seg_infomation(void)
{
    seg_info_num = get_segment_num();
    if (seg_info_num == 0) {/*not need create aot*/
       return;
    }
    seg_info_vector = (seg_info **)malloc(seg_info_num * sizeof(seg_info *));
    assert(seg_info_vector);
    do_segment_record(seg_info_vector);
}

static int page_count;
static page_table_info *aot_page_table;

static struct aot_header *get_aot_buffer(int first_seg_in_lib,
        int end_seg_in_lib)
{
    curr_lib_seg_num = end_seg_in_lib - first_seg_in_lib;
    page_count = 0;
    for (int i = first_seg_in_lib; i < end_seg_in_lib; i++) {
        seg_info *seg = seg_info_vector[i];
        page_count += (seg->seg_end - seg->seg_begin + TARGET_PAGE_SIZE)
            / TARGET_PAGE_SIZE;
    }
    assert(page_count < 204800);
    /*
     * Allocate memory to store aot infomation(metadata infomation, code cache
     * excluded). Then we will write entire buffer into aot file. */
    size_t aot_buffer_size =
        sizeof(struct aot_header) +                 /* AOT header size */
        sizeof(struct aot_segment) * curr_lib_seg_num + /* AOT segment table size */
        sizeof(struct page_table_info) * page_count + /* AOT page table */
        PATH_MAX * curr_lib_seg_num +                   /* AOT segment name size */
        sizeof(struct aot_tb) * tb_num +            /* AOT tb table size */
        sizeof(struct aot_rel) * rel_entry_num +/* AOT rel table size */
        256;                                      /* extra alignment bytes. */
    return (struct aot_header *)malloc(aot_buffer_size);
}

/* 1. Fill segment table. */ 
/* 2. Save lib name to string table */
/* return the end address of string table */
static void fill_seg_table(int seg_info_begin, int seg_info_end,
        struct aot_header *p_header, struct aot_segment *p_segments)
{
    bool is_pe = is_pe_file(curr_lib_name);
    p_header->is_pe |= is_pe;
    int seg_num_in_file = seg_info_end - seg_info_begin;
    p_header->segments_num = seg_num_in_file;
    p_header->segment_table_offset =
        (uintptr_t)p_segments - (uintptr_t)p_header;
    aot_page_table = (page_table_info *)(p_segments + seg_num_in_file);
    /* aot_page_table = (page_table_info *)ROUND_UP((uintptr_t)aot_page_table, 8); */
    char *aot_x86_lib_names = (char *)(aot_page_table + page_count);
    /* @last_name: Temp pointer points last handled x86 lib file name.
     * @curr_name: Temp pointer points current handled x86 lib file name. */
    char *last_name = aot_x86_lib_names;
    char *curr_name = aot_x86_lib_names;
    int page_index = 0;
    for (int i = seg_info_begin; i < seg_info_end; i++) {
        seg_info *curr_seg_info = seg_info_vector[i];
        int seg_id_in_lib = i - seg_info_begin;
        memcpy(&p_segments[seg_id_in_lib].details, curr_seg_info, sizeof(seg_info));
        if (option_debug_aot) {
            fprintf(
                    stderr,
                    "Gen seg name %s offset 0x" TARGET_FMT_lx
                    ", p_segments.details %p details.file_offset 0x"
                    TARGET_FMT_lx "\n",
                    curr_seg_info->file_name, curr_seg_info->file_offset,
                    &p_segments[seg_id_in_lib].details, 
                    p_segments[seg_id_in_lib].details.file_offset);
        }

        if (last_name == curr_name ||
            strcmp(last_name, curr_seg_info->file_name)) {
            strcpy(curr_name, curr_seg_info->file_name);
            last_name = curr_name;
            curr_name += strlen(curr_seg_info->file_name) + 1;
        }

        p_segments[seg_id_in_lib].is_pe = is_pe;
        p_segments[seg_id_in_lib].lib_name_offset =
            (uintptr_t)last_name - (uintptr_t)p_header;
        p_segments[seg_id_in_lib].segment_tbs_num = 0;
        p_segments[seg_id_in_lib].page_table_offset = 
            (uintptr_t)(aot_page_table + page_index) - (uintptr_t)p_header;
        page_index += (curr_seg_info->seg_end - curr_seg_info->seg_begin
                + TARGET_PAGE_SIZE) / TARGET_PAGE_SIZE;
    }
    table_end_addr = (uintptr_t)curr_name;
}

static aot_segment *find_seg(target_ulong target_seg_begin,
        aot_segment *p_segments)
{
    aot_segment *curr_seg;
    curr_seg = p_segments;
    int jj;
    for (jj = 0; jj < curr_lib_seg_num; jj++) {
        if (curr_seg->details.seg_begin == target_seg_begin) {
            break;
        }
        curr_seg++;
    }
    if (jj == curr_lib_seg_num) {
        return NULL;
    }
    return curr_seg;
}

static inline int create_aot_tb(aot_tb *curr_aot_tb, TranslationBlock *tb, 
        aot_segment *curr_seg, aot_segment *p_segments)
{
#ifdef CONFIG_LATX_TU
    curr_aot_tb->tu_id = tb->s_data->tu_id;
    curr_aot_tb->offset_in_tu = tb->s_data->offset_in_tu;
    if (is_tu_tb(tb)) {
        curr_aot_tb->tu_search_addr_offset = tb->tu_search_addr_offset;
    }
#endif
    curr_aot_tb->jmp_stub_target_arg[0] = tb->jmp_stub_target_arg[0];
    curr_aot_tb->jmp_stub_target_arg[1] = tb->jmp_stub_target_arg[1];
    if (use_tu_jmp(tb)) {
        curr_aot_tb->tu_jmp[0] = tb->tu_jmp[0];
        curr_aot_tb->tu_jmp[1] = tb->tu_jmp[1];
        curr_aot_tb->tu_unlink.ins = tb->tu_unlink.ins;
        curr_aot_tb->tu_unlink.stub_offset = tb->tu_unlink.stub_offset;
    } else {
        curr_aot_tb->jmp_indirect = tb->jmp_indirect;
        curr_aot_tb->jmp_target_arg[0] = tb->jmp_target_arg[0];
        curr_aot_tb->jmp_target_arg[1] = tb->jmp_target_arg[1];
        curr_aot_tb->jmp_reset_offset[0] = tb->jmp_reset_offset[0];
        curr_aot_tb->jmp_reset_offset[1] = tb->jmp_reset_offset[1];
        curr_aot_tb->jmp_stub_reset_offset[0] = tb->jmp_stub_reset_offset[0];
        curr_aot_tb->jmp_stub_reset_offset[1] = tb->jmp_stub_reset_offset[1];
    }

    curr_aot_tb->icount = tb->icount;
    curr_aot_tb->tu_size = tb->s_data->tu_size;
    curr_aot_tb->offset_in_segment =
        tb->pc - curr_seg->details.seg_begin;
    assert(curr_aot_tb->offset_in_segment < 
            curr_seg->details.seg_end - curr_seg->details.seg_begin);
    curr_aot_tb->tb_cache_addr = (void *)tb->tc.ptr;
    curr_aot_tb->tb_cache_size = tb->tc.size;
    curr_aot_tb->size = tb->size;
    curr_aot_tb->flags = tb->flags;
    curr_aot_tb->cflags = tb->cflags;
    curr_aot_tb->rel_start_index = tb->s_data->rel_start;
    curr_aot_tb->rel_end_index = tb->s_data->rel_end;
    curr_aot_tb->segment_idx = curr_seg - p_segments;
    curr_aot_tb->first_jmp_align = tb->first_jmp_align;
    curr_aot_tb->last_ir1_type = tb->s_data->last_ir1_type;
    curr_aot_tb->bool_flags = tb->bool_flags | IS_AOT_TB;
    curr_aot_tb->eflag_use = tb->eflag_use;
    #ifdef CONFIG_LATX_INSTS_PATTERN
    curr_aot_tb->eflags_target_arg[0] = tb->eflags_target_arg[0];
    curr_aot_tb->eflags_target_arg[1] = tb->eflags_target_arg[1];
    curr_aot_tb->eflags_target_arg[2] = tb->eflags_target_arg[2];
    #endif
#if defined(CONFIG_LATX_JRRA) || defined(CONFIG_LATX_JRRA_STACK)
    if ((tb->bool_flags & IS_ENABLE_JRRA) && tb->return_target_ptr) {
        curr_aot_tb->return_target_ptr_offset =
            (uintptr_t)tb->return_target_ptr -
                (uintptr_t)tb->tc.ptr;
    } else {
        curr_aot_tb->return_target_ptr_offset = 0;
    }
    curr_aot_tb->next_86_pc_offset =
        tb->next_86_pc - curr_seg->details.seg_begin;
#endif
    /* Dump this tb info for debug */
    if (option_debug_aot) {
        qemu_log_mask(LAT_LOG_AOT, "Tb 0x" TARGET_FMT_lx
                " offset_in_seg 0x%-8x tc_size 0x%x\n",
                tb->pc, curr_aot_tb->offset_in_segment,
                curr_aot_tb->tb_cache_size);
    }
    for (int k = 0; k < 2; k++) {
        if (!use_tu_jmp(tb) && (curr_aot_tb->jmp_reset_offset[k] !=
                    TB_JMP_RESET_OFFSET_INVALID)) {
            uint32_t *p_insn =
                (uint32_t *)(curr_aot_tb->tb_cache_addr +
                        curr_aot_tb->jmp_target_arg[k]);
            if (((*p_insn) & 0xfc000000) != 0x50000000 &&
                    !(((*p_insn) & 0xfe000000) == 0x1e000000 &&/* pcaddu18i */
                        (*(p_insn + 1) & 0xfc000000) == 0x4c000000)) {/* jirl */
                qemu_log_mask(LAT_LOG_AOT,
                        "Error! jmp_reset_offset is not B or jirl insn\n");
                return -1;
            }
        }
    }
    return 0;
}

static void init_page_table(struct aot_segment *p_segments, aot_header *p_header) 
{
    aot_segment curr_seg;
    for (int i = 0; i < curr_lib_seg_num; i++) {
        curr_seg = p_segments[i];
        int curr_seg_page_num = (curr_seg.details.seg_end
                - curr_seg.details.seg_begin
                + TARGET_PAGE_SIZE) / TARGET_PAGE_SIZE;
        page_table_info *pt = 
            (page_table_info *)(curr_seg.page_table_offset + (uintptr_t)p_header);
        for (int j = 0; j < curr_seg_page_num; j++) {
            pt[j].tb_num_in_page = 0;
            pt[j].tb_num_in_page_parallel = 0;
        }
    }
}

static void fill_page_table(struct aot_segment *curr_seg, aot_header *p_header,
        struct aot_tb *curr_aot_tb)
{
    page_table_info *pt = (page_table_info *)(curr_seg->page_table_offset + (uintptr_t)p_header);
    int curr_page_id = curr_aot_tb->offset_in_segment / TARGET_PAGE_SIZE;
    assert(curr_aot_tb->offset_in_segment <
            (curr_seg->details.seg_end - curr_seg->details.seg_begin));
    if (curr_aot_tb->cflags & CF_PARALLEL) {
        if (pt[curr_page_id].tb_num_in_page_parallel++ == 0) {
            pt[curr_page_id].page_tbs_offset_parallel = 
                (uintptr_t) curr_aot_tb - (uintptr_t)p_header;
        }
    } else {
        if (pt[curr_page_id].tb_num_in_page++ == 0) {
            pt[curr_page_id].page_tbs_offset = 
                (uintptr_t) curr_aot_tb - (uintptr_t)p_header;
        }
    }
}

static unsigned long fill_tb_table(struct aot_tb *p_aot_tbs, 
        aot_header *p_header, aot_segment *p_segments)
{
    /* fill in aot tbs table. We use two pointers to finish this work,
     * @curr_seg points current segment where current tbs will store.
     * @curr_tb points current handled aot_tb, if it's in the range of @curr_seg
     * then we can mark it as the element of @curr_seg. Otherwise we should
     * move @curr_seg to the next one. */

    struct aot_segment *curr_seg = p_segments;
    struct aot_tb *curr_aot_tb = p_aot_tbs;
    unsigned long total_code_cache_size = 0;

    curr_lib_tb_num = 0;
    uint32_t parallel_tb_num = 0;
    for (int i = 0; i < tb_num;) {
        if (tb_vector[i] == NULL) {
            i++;
            continue;
        }
        if (tb_vector[i]->cflags & CF_PARALLEL) {
            parallel_tb_num++;
        }
        seg_info *curr_tb_seg = segment_tree_lookup(tb_vector[i]->pc);
        /* Tbs in tb_vector are sorted according to it's x86 pc, and segments
         * in seg_info_table are also sorted according to pc. So if we handle
         * tb increasingly, current tb can ONLY be in `curr_seg` or after
         * `curr_seg` */
        if (!curr_tb_seg) {
            i++;
            continue;
        }
        curr_seg = find_seg(curr_tb_seg->seg_begin, p_segments);
        if (curr_seg == NULL) {
            i++;
            continue;
        }

        assert(curr_tb_seg->seg_begin >= curr_seg->details.seg_begin);

        /* If current tb is in current seg, there are two cases:
         * 1. current tb(tb_vector[i]) is the first tb in curr_seg, we should
         *    fill in curr_seg->segment_tbs_offset.
         * 2. Oherwise.
         * In both two cases we should update curr_seg->segment_tb_num. */
        if (curr_tb_seg->seg_begin == curr_seg->details.seg_begin) {
            curr_lib_tb_num++;
            if (curr_seg->segment_tbs_num++ == 0) {
                curr_seg->segment_tbs_offset =
                    (uintptr_t)curr_aot_tb - (uintptr_t)p_header;
                /* Dump this segment info for debug. */
                if (option_debug_aot) {
                    qemu_log_mask(LAT_LOG_AOT, "-----------------------------------------\n");
                    qemu_log_mask(LAT_LOG_AOT,
                            "Seg: %-20s file_off_addr %p file_offset:0x" TARGET_FMT_lx
                            " tbs_offset %x\n",
                            (char *)p_header + curr_seg->lib_name_offset,
                            &curr_seg->details,
                            curr_seg->details.file_offset,
                            curr_seg->segment_tbs_offset);
                    qemu_log_mask(LAT_LOG_AOT, "-----------------------------------------\n");
                }
            }
            /* Construct a aot_tb. */
            if(create_aot_tb(curr_aot_tb, tb_vector[i], curr_seg, p_segments)) {
                return 0;
            }
            assert(tb_vector[i]->pc >= curr_seg->details.seg_begin && 
                    tb_vector[i]->pc < curr_seg->details.seg_end);

            fill_page_table(curr_seg, p_header, curr_aot_tb);

#ifdef CONFIG_LATX_TU
            if (curr_aot_tb->is_first_tb) {
                total_code_cache_size += curr_aot_tb->tu_size;
            }
#else
            total_code_cache_size += curr_aot_tb->tu_size;
#endif
            curr_aot_tb++;
            i++;
        } else {
            if (option_debug_aot)
                qemu_log_mask(LAT_LOG_AOT, "Warning! tb " TARGET_FMT_lx
                        " is jmp over seg %s:" TARGET_FMT_lx "\n",
                        tb_vector[i]->pc, curr_seg->details.file_name,
                        curr_seg->details.file_offset);
            curr_seg++;
        }
    }
    p_header->parallel_tb_num = parallel_tb_num;
    p_header->unparallel_tb_num = tb_num - parallel_tb_num;
    table_end_addr = (uintptr_t) curr_aot_tb;
    if (curr_aot_tb == p_aot_tbs) {
        return 0;
    }
    #ifdef AOT_DEBUG
    if (option_load_aot) {
        qemu_log_mask(LAT_LOG_AOT, "aot: %s nums=%ld\n", aot_file_path,
            curr_aot_tb - p_aot_tbs);
    }
    #endif
    return total_code_cache_size;
}

/* fill in relocation table, we simply copy the whole rel_table into
* AOT file. */
static void fill_rel_table(aot_header *p_header)
{
    struct aot_rel *p_aot_rel =
        (struct aot_rel *)ROUND_UP(table_end_addr, 8);
    p_header->rel_table_offset = (uintptr_t)p_aot_rel - (uintptr_t)p_header;
    uint32_t total_rel_entry_num = rel_entry_num;
    memcpy(p_aot_rel, rel_table, rel_entry_num * sizeof(struct aot_rel));
    p_header->rel_entry_num = total_rel_entry_num;
    table_end_addr = (uintptr_t)(p_aot_rel + total_rel_entry_num);
}

/* we now write tranlsation code cache to memory. First we use
 * another buffer to store code cache, and fixup all @tb_cache_offset of
 * Tbs. Then we write aot_buffer and insn_buffer into aot file. */
static uint32_t *fill_ins_buff(uint64_t insn_table_offset, aot_tb *p_aot_tbs, 
        uintptr_t tb_table_end, unsigned long total_code_cache_size)
{
    uint32_t *insn_buffer = (uint32_t *)malloc(total_code_cache_size);
    assert(insn_buffer && "insn_buffer malloc failed!");
    uint32_t *curr_insn = insn_buffer;
#ifdef CONFIG_LATX_TU
    uint32_t *new_pos = insn_buffer;
#endif
    /* Fixup all tb_cache_offset. */
    for (struct aot_tb *p = p_aot_tbs; p < (aot_tb *)tb_table_end; ++p) {
#ifdef CONFIG_LATX_TU
        if (p->is_first_tb) {
            curr_insn = new_pos;
            memcpy(curr_insn, p->tb_cache_addr, p->tu_size);
            new_pos += p->tu_size >> 2;
        }
        p->tb_cache_offset = (uintptr_t)curr_insn - (uintptr_t)insn_buffer 
            + insn_table_offset + p->offset_in_tu;
#else
        memcpy(curr_insn, p->tb_cache_addr, p->tu_size);
        /* TODO! unlink this TB. */
        p->tb_cache_offset =
            (uintptr_t)curr_insn - (uintptr_t)insn_buffer + insn_table_offset;
        curr_insn += p->tu_size >> 2;
#endif
   }
#ifdef CONFIG_LATX_TU
    assert((void *)new_pos - (void *)insn_buffer == total_code_cache_size);
#else
    assert((void *)curr_insn - (void *)insn_buffer == total_code_cache_size);
#endif
    return insn_buffer;
}

void get_aot_path(const char *lib_name, char *file_path) 
{
    assert(lib_name && file_path);
    char *home = getenv("HOME");
    if (likely(home)) {
    	snprintf(file_path, PATH_MAX, "%s%s", home, "/.cache/latx/");
    } else {
    	snprintf(file_path, PATH_MAX, "%s", "/.cache/latx/");
    }
    char temp[PATH_MAX];
    strcpy(temp, "");
    strcat(temp, lib_name);
    for (int i = 0; i < strlen(temp); i++) {
        if (temp[i] == '/') {
            temp[i] = '+';
        }
    }
    strcat(file_path, temp);
    strcat(file_path, ".aot2");
}

static long get_fileSize(FILE* file) 
{
    long fileSize = -1;
    if (file != NULL) {
        if (fseek(file, 0L, SEEK_END) == 0) {
            fileSize = ftell(file);
        }
        rewind(file);
    }
    return fileSize / 1024;
}

static bool generated_aot_file;

static int write_aot_file(int *lockfd, aot_header *p_header, uint32_t *p_insn, 
        uint32_t *insn_buffer, unsigned long total_code_cache_size)
{
    char *pathtmp;
    get_aot_path(curr_lib_name, aot_file_path);
    pathtmp = aot_file_path;
    if (access(pathtmp, 0) >= 0) {
        strcat(pathtmp, "A");
        if (access(pathtmp, 0) >= 0) {
            qemu_log_mask(LAT_LOG_AOT, "ERROR: other process generated aot file %s\n",
                pathtmp);
            return -1;
        }
    }

    /* Write file metadata infomation(aot_buffer) into aot. */
    int fd = open(pathtmp, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	assert(fd > 2);
	if (fd <= 2) {
	    exit(EXIT_FAILURE);
	}
    FILE *pfile = fdopen(fd, "w");
    size_t write_size = (uintptr_t)p_insn - (uintptr_t)p_header;

    /* FILE *tp = pfile; */
    if (fwrite(p_header, write_size, 1, pfile) != 1) {
        qemu_log_mask(LAT_LOG_AOT, "Error! write aot metadata failed!\n");
        fclose(pfile);
        return -1;
    }
    if (fwrite(insn_buffer, total_code_cache_size, 1, pfile) != 1) {
        qemu_log_mask(LAT_LOG_AOT, "Error! write aot translated code failed!\n");
        fclose(pfile);
        return -1;
    }
    if (fwrite(AOT_VERSION, strlen(AOT_VERSION), 1, pfile) != 1) {
        qemu_log_mask(LAT_LOG_AOT, "Error! write aot AOT_VERSION failed!\n");
        fclose(pfile);
        return -1;
    }
    if (option_debug_aot) {
        qemu_log_mask(LAT_LOG_AOT, "!!!! name %s size %ld curr_tb_num %d pid %d!!!\n",
                pathtmp, get_fileSize(pfile), curr_lib_tb_num, getpid());
    }
    if (fclose(pfile) != 0) {
        qemu_log_mask(LAT_LOG_AOT, "Error! close aot file failed\n");
        return -1;
    }

    generated_aot_file = true;

    return 0;
}

void do_generate_aot(int first_seg_in_lib, int end_seg_in_lib) 
{
    if (tb_num == 0) {
        return;
    }
    TaskState *ts = (TaskState *)thread_cpu->opaque;
    sigset_t oldmask, mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGSEGV);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    qatomic_xchg(&ts->signal_pending, 1);
    reset_all_locks(&tb_ctx.htable);
    struct aot_header *p_header = 
        get_aot_buffer(first_seg_in_lib, end_seg_in_lib);
    assert(p_header && "allocatge aot_buffer failed\n");
    struct stat statbuf;
    if (stat(curr_lib_name, &statbuf)) {
        qemu_log_mask(LAT_LOG_AOT, "ERROT stat %s failed\n", curr_lib_name);
        return;
    }
    for (int i = first_seg_in_lib; i < end_seg_in_lib; i++) {
        seg_info *seg = seg_info_vector[i];
        if (seg->lib_size != statbuf.st_size ||
                seg->tv_sec != statbuf.st_mtim.tv_sec) {
            fprintf(stderr, "error, orignal lib changed %s\n", curr_lib_name);
            return;
        }
    }
    p_header->lib_size = statbuf.st_size;
    p_header->last_modify_time = statbuf.st_mtim;
    struct aot_segment *p_segments;
    p_segments =
        (struct aot_segment *)ROUND_UP((uintptr_t)(p_header + 1), 8);
    /* Fill segment table and save lib name to string table */
    fill_seg_table(first_seg_in_lib, end_seg_in_lib, p_header, p_segments);
    init_page_table(p_segments, p_header);
    struct aot_tb *p_aot_tbs; 
    p_aot_tbs = (struct aot_tb *)ROUND_UP(table_end_addr, 8);
    /* Fill aot table */
    unsigned long total_code_cache_size = 
        fill_tb_table(p_aot_tbs, p_header, p_segments);
    uintptr_t tb_table_end = table_end_addr;
    if(total_code_cache_size == 0) {
        return;
    }
    /* fill relocation table */
    fill_rel_table(p_header);
    /* fill ins buffer */
    uint32_t *p_insn =
        (uint32_t *)ROUND_UP(table_end_addr, 8);
    uint32_t *insn_buffer = fill_ins_buff((void *)p_insn - (void *)p_header,
            p_aot_tbs, tb_table_end, total_code_cache_size);
    int lockfd = -1;
    write_aot_file(&lockfd, p_header, p_insn, 
            insn_buffer, total_code_cache_size);
}

static const char lib_black_list[][PATH_MAX] =
{
    {"end"},
};

static const char lib_white_list[][PATH_MAX] =
{
    {"end"},
};

static bool in_list(char *lib, const char lib_list[][PATH_MAX]) 
{
    for(int i = 0; strcmp(lib_list[i], "end"); i++) {
        if (strcmp(lib, lib_list[i]) == 0) {
            return true;
        }
    }
    return false;
}

char in_black_list(char *lib) 
{
    return in_list(lib, lib_black_list);
}

char in_white_list(char *lib) 
{
    return in_list(lib, lib_white_list);
}

static int get_tb_num(char *lib_name, CPUState *cpu) {
    get_aot_path(lib_name, aot_file_path);
    if (access(aot_file_path, 0) < 0) {
        return 0;
    }
    int fd = open(aot_file_path, O_RDONLY);
    FILE *pf = fdopen(fd, "r");
    lsassert(pf && ("open aot file failed!"));
    /* Get file size */
    fseek(pf, 0, SEEK_END);      /* seek to end of file */
    size_t file_sz = ftell(pf);  /* get current file pointer */
    char aot_version[strlen(AOT_VERSION) + 1];
    /*check aot complete.*/
    if (fseek(pf, -strlen(AOT_VERSION), SEEK_END) != 0) {
        qemu_log_mask(LAT_LOG_AOT, "can't fseek aot file\n");
        fclose(pf);
        return 0;
    }
    if (fread(aot_version, strlen(AOT_VERSION), 1, pf) != 1) {
        qemu_log_mask(LAT_LOG_AOT, "get error.\n");
        fclose(pf);
        return 0;
    }
    if (!strstr(aot_version, AOT_VERSION)) {
        qemu_log_mask(LAT_LOG_AOT, "aot file is not complete %s\n", lib_name);
        remove(aot_file_path);
        return 0;
    }
    fseek(pf, 0, SEEK_SET);      /* seek back to beginning of file */

    /* Read aot file */
    /* buffer = malloc(file_sz); */
    void *buffer = mmap(NULL, file_sz, PROT_READ, MAP_SHARED, fd, 0);
    if (buffer < 0) {
        qemu_log_mask(LAT_LOG_AOT, "aot file mmap error\n");
        return 0;
    }
    assert(buffer);
    aot_header *p_header = (aot_header *)buffer;
    /* dump_aot_buffer(p_header); */
    struct stat statbuf;
    if (stat(lib_name, &statbuf)
            || p_header->lib_size != statbuf.st_size
            || p_header->last_modify_time.tv_sec != statbuf.st_mtim.tv_sec
            || p_header->last_modify_time.tv_nsec != statbuf.st_mtim.tv_nsec) {
        qemu_log_mask(LAT_LOG_AOT, "need remove old aot file. %s lib_size %d %ld\n",
                aot_file_path, p_header->lib_size, statbuf.st_size);
        remove(aot_file_path);
        qemu_log_mask(LAT_LOG_AOT, "remove end\n");
        return 0;
    }

    int aim_tb_num = 0;
    if (cpu->tcg_cflags & CF_PARALLEL) {
        aim_tb_num =  p_header->parallel_tb_num;
    } else {
        aim_tb_num = p_header->unparallel_tb_num;
    }

    munmap(buffer, file_sz);
    fclose(pf);
    return aim_tb_num;
}

#define PROLIFERATION_RATE 10
static inline int need_gen_aot(CPUState *cpu,
        char *lib_name, int *lockfd, uint32_t tb_count) 
{
    lib_info *lib = lib_tree_lookup(lib_name);
    int aim_tb_num = 0;;
    if (lib) {
        aot_header *p_head = (aot_header *)lib->buffer;
        if (cpu->tcg_cflags & CF_PARALLEL) {
            aim_tb_num = p_head->parallel_tb_num;
        } else {
            aim_tb_num = p_head->unparallel_tb_num;
        }
    } else {
        aim_tb_num = get_tb_num(lib_name, cpu);
    }
    /* Too few tb number. */
    if (tb_count * PROLIFERATION_RATE < aim_tb_num) {
        return -1;
    }
    get_aot_path(curr_lib_name, aot_file_path);
    strcpy(aot_file_lock, aot_file_path);
    strcat(aot_file_lock, ".lock");
    assert(aot_file_lock);
    
    if (file_lock(aot_file_lock, lockfd, F_WRLCK, false) < 0) {
        return -1;
    }

    if (lib && lib->is_unmapped) {
        flock_set(*lockfd, F_UNLCK, true);
        close(*lockfd);
        return -1;
    }

    return *lockfd;
}

void pre_translate(int begin_id, int end_id, CPUState *cpu,
		tb_tmp_message *tb_message_vector)
{
    if ((tcg_ctx->code_gen_ptr + tcg_ctx->code_gen_buffer_size / 2
            > tcg_ctx->code_gen_highwater) && tcg_enabled()) {
        unsigned tb_flush_count = qatomic_mb_read(&tb_ctx.tb_flush_count);
        if (cpu_in_exclusive_context(cpu)) {
            do_tb_flush(cpu, RUN_ON_CPU_HOST_INT(tb_flush_count));
        } else {
            assert(0);
            async_safe_run_on_cpu(cpu, do_tb_flush,
                                  RUN_ON_CPU_HOST_INT(tb_flush_count));
        }
    }
    uintptr_t flush_ptr = (uintptr_t)tcg_ctx->code_gen_buffer;
    qatomic_set(&tcg_ctx->code_gen_ptr, (void *)
    			ROUND_UP(flush_ptr, CODE_GEN_HTABLE_BITS));
    qatomic_set(&tcg_ctx->tb_gen_ptr, tcg_ctx->tb_gen_buffer);
    clear_rel_table();

    get_aot_path(curr_lib_name, aot_file_path);
    if (access(aot_file_path, 0) >= 0) {
        strcat(aot_file_path, "A");
        if (access(aot_file_path, 0) >= 0) {
            tb_num = 0;
            return;
        }
    }

    tb_num = translate_lib(seg_info_vector, begin_id,
                    end_id, cpu, tb_message_vector);
    tb_vector = ts_vector;
}

static inline void gen_aot_by_lib(int first_seg_id, int end_seg_id,
		CPUState *cpu, uint32_t tb_count)
{
    curr_lib_name = seg_info_vector[first_seg_id]->file_name;
    int lockfd = -1;
    if (need_gen_aot(cpu, curr_lib_name, &lockfd, tb_count) >= 0) {
    	pre_translate(first_seg_id, end_seg_id, cpu, NULL);
        do_generate_aot(first_seg_id, end_seg_id);
        aot2_merge(curr_lib_name, first_seg_id, end_seg_id, cpu);
    	flock_set(lockfd, F_UNLCK, true);
    	close(lockfd);
    }
}

static void generate_aot_v2(CPUState *cpu) 
{
    generated_aot_file = false;
    int first_seg_in_lib = 0;
    uint32_t tb_count = seg_info_vector[0]->last_tb_id - 
        seg_info_vector[0]->first_tb_id + 1;
    for (int i = 1; i < seg_info_num; i++) {
        if (strcmp(seg_info_vector[i]->file_name, 
                    seg_info_vector[first_seg_in_lib]->file_name)) {
            gen_aot_by_lib(first_seg_in_lib, i, cpu, tb_count);
            first_seg_in_lib = i;
            tb_count = seg_info_vector[i]->last_tb_id 
                - seg_info_vector[i]->first_tb_id + 1;
        } else {
            tb_count += seg_info_vector[i]->last_tb_id 
                - seg_info_vector[i]->first_tb_id + 1;
        }
    }
    gen_aot_by_lib(first_seg_in_lib, seg_info_num, cpu, tb_count);
    if (generated_aot_file) {
        /* If the total size of the aot file exceeds 11.5 GB, */
        /* it will be deleted to 6 GB. */
        aot_file_ctx(12 * 1024 , 500);
    }
}

void clear_rel_table(void)
{
    if (rel_entry_num) {
        free(rel_table);
    }
    rel_entry_num = 0;
    rel_table_capacity = 0;
}

void aot_generate(CPUState *cpu)
{
    char tmp_file[2] = "t";
    get_aot_path(tmp_file, aot_file_path);
    int fd = open(aot_file_path, O_CREAT | O_RDONLY, 0777);
    /* make fd > 2. */
    while(fd <= 2) {
	if (fd < 0) {
	    qemu_log_mask(LAT_LOG_AOT, "aot_generate failed: open %s failed\n", aot_file_path);
	    exit(EXIT_FAILURE);
	}
    	fd = open(aot_file_path, O_CREAT | O_RDONLY, 0777);
    }
    close(fd);
    reset_all_locks(&tb_ctx.htable);
    get_tb();
    get_seg_infomation();

    if (tb_num == 0 || seg_info_num == 0) {
        return;
    }

    get_dynamic_message(tb_vector, tb_num, seg_info_vector, &seg_info_num);

    generate_aot_v2(cpu);
}

int aot_get_file_init(char *aot_file)
{
    int count = 0;
    char pathtmp[PATH_MAX] = {0};
    if (access(aot_file, 0) >= 0) {
        strncpy(pathtmp, aot_file, PATH_MAX - 1);
        pathtmp[PATH_MAX - 1] = '\0';
        strcat(pathtmp, "A");
        for (int jj = 1; jj < 100; jj++) {
            if (access(pathtmp, 0) < 0) {
                count = jj;
                break;
            }
            pathtmp[strlen(pathtmp) - 1]++;
        }

    }
    lsassert(count > 0);
    aot_buffer_all = (aot_file_info *)malloc(count * sizeof(aot_file_info));
    assert(aot_buffer_all);
    aot_buffer_all_num = count;
    return count;
}

int aot_get_file_name(char *aot_file, char *buff, int index)
{
    lsassert(index >= 0);
    if (index == 0) {
        strcpy(buff, aot_file);
        goto out;
    }
    strcpy(buff, aot_file);
    strcat(buff, "A");
    buff[strlen(buff) - 1] += (index - 1);
out:
    if (access(buff, 0) < 0) {
        return -1;
    }
    return 0;
}

void dump_seg(aot_segment *p_segment, aot_header *p_header) 
{
    int pt_num = 
            (p_segment->details.seg_end - p_segment->details.seg_begin) / TARGET_PAGE_SIZE;
    fprintf(stderr, " --------seg start %lx tb_num %d pt_num %d env parallel %d ------\n", 
            p_segment->details.seg_begin, p_segment->segment_tbs_num,
            pt_num, tcg_ctx->tb_cflags & CF_PARALLEL);

    fprintf(stderr, "parallel:\n");
    aot_tb *p_aot_tbs;
    page_table_info *pt = (page_table_info *)((uintptr_t)p_header 
                + (uintptr_t)(p_segment->page_table_offset));
    for (int i = 0; i < pt_num; i++) {
        fprintf(stderr,"page %d:\n", i);
        p_aot_tbs = (aot_tb *)((uintptr_t)p_header + 
                (uintptr_t)pt[i].page_tbs_offset_parallel);
        int tb_num = pt[i].tb_num_in_page_parallel;
        for (int j = 0; j < tb_num; j++){
            fprintf(stderr, "j %d offset_in_segment %x\n", 
                    j, p_aot_tbs[j].offset_in_segment);
        }
    }

    fprintf(stderr, "unparallel:\n");
    for (int i = 0; i < pt_num; i++) {
        fprintf(stderr,"page %d:\n", i);
        pt = (page_table_info *)((uintptr_t)p_header 
                    + (uintptr_t)(p_segment->page_table_offset));
        p_aot_tbs = (aot_tb *)((uintptr_t)p_header + (uintptr_t)pt[i].page_tbs_offset);
        tb_num = pt[i].tb_num_in_page;
        for (int j = 0; j < tb_num; j++){
            fprintf(stderr, "j %d offset_in_segment %x\n", 
                    j, p_aot_tbs[j].offset_in_segment);
        }
    }
}

void dump_aot_buffer(aot_header *p_header)
{
    aot_segment *p_segment =
        (aot_segment *)((uintptr_t)p_header + p_header->segment_table_offset);
    for (int i = 0; i < p_header->segments_num; i++) {
        fprintf(stderr, "segments %d details:\n start %lx end %lx  page_table %lx \n",
                i, p_segment[i].details.seg_begin, p_segment[i].details.seg_end,
                p_segment[i].segment_tbs_num + (uintptr_t)p_header);
    }

    for (int i = 0; i < p_header->segments_num; i++) {
        dump_seg(&p_segment[i], p_header);
    }

}

static void remove_curr_aot_file(void)
{
    remove(aot_file_path);
    strcat(aot_file_path, "A");
    if (access(aot_file_path, 0) >= 0) {
        remove(aot_file_path);
    }
}

time_t aot_st_ctime;

lib_info *aot_load(char *lib_name)
{
    /* Get aot_file path and lock file. */
    assert(lib_name);
    get_aot_path(lib_name, aot_file_path);
    if (access(aot_file_path, 0) < 0) {
        return NULL;
    }
    int lockfd = -1;
    strcpy(aot_file_lock, aot_file_path);
    strcat(aot_file_lock, ".lock");
    if (file_lock(aot_file_lock, &lockfd, F_RDLCK, false) < 0) {
        close(lockfd);
        return NULL;
    }

    /* Open aot_file. */
    void *buffer;
    struct stat statbuf;
    lib_info *curr_lib_info = NULL;
    char aot_version[strlen(AOT_VERSION) + 1];
    int fd = open(aot_file_path, O_RDONLY);
    FILE *pf = NULL;
    if (fd < 0) {
        goto exit_aot_load;
    }
    pf = fdopen(fd, "r");
    lsassert(pf && ("open aot file failed!"));
    fseek(pf, 0, SEEK_END);      /* seek to end of file */
    size_t file_sz = ftell(pf);  /* get current file pointer */

    /*check aot complete.*/
    if (fseek(pf, -strlen(AOT_VERSION), SEEK_END) != 0
            || fread(aot_version, strlen(AOT_VERSION), 1, pf) != 1) {
        goto exit_aot_load;
    }
    if (!strstr(aot_version, AOT_VERSION)) {
        qemu_log_mask(LAT_LOG_AOT, "aot file is not complete %s\n", lib_name);
        remove_curr_aot_file();
        goto exit_aot_load;
    }

    /* Read aot file */
    fseek(pf, 0, SEEK_SET);      /* seek back to beginning of file */
    buffer = mmap(NULL, file_sz, PROT_READ, MAP_SHARED, fd, 0);
    if (buffer <= 0) {
        qemu_log_mask(LAT_LOG_AOT, "aot file mmap error\n");
        goto exit_aot_load;
    }
    assert(buffer);
    aot_header *p_header = (aot_header *)buffer;

    /* Test original file state. */
    if (stat(lib_name, &statbuf)
            || p_header->lib_size != statbuf.st_size
            || p_header->last_modify_time.tv_sec != statbuf.st_mtim.tv_sec
            || p_header->last_modify_time.tv_nsec != statbuf.st_mtim.tv_nsec) {
        qemu_log_mask(LAT_LOG_AOT, "need remove old aot file. %s lib_size %d %ld\n",
                aot_file_path, p_header->lib_size, statbuf.st_size);
        remove_curr_aot_file();
        goto exit_aot_load;
    }

    /* dump_aot_buffer(p_header); */
    aot_buffer = buffer;
    curr_lib_info = lib_tree_insert(lib_name, buffer);

exit_aot_load:
    if (likely(pf)) {
        fclose(pf);
    }
    close(fd);
    flock_set(lockfd, F_UNLCK, true);
    close(lockfd);
    return curr_lib_info;
}

struct aot_segment *aot_find_segment(char *path, int offset)
{
    struct aot_header *p_header = (struct aot_header *)aot_buffer;
    struct aot_segment *p_segment =
        (struct aot_segment *)(aot_buffer + p_header->segment_table_offset);
    for (int i = 0; i < p_header->segments_num; i++) {
        if (p_segment[i].segment_tbs_num == 0) {
            continue;
        }
        char *lib_name = (char *)(aot_buffer + p_segment[i].lib_name_offset);
        int seg_off_in_file = p_segment[i].details.file_offset;
        if (strcmp(path, lib_name) == 0 && seg_off_in_file == offset) {
            return &p_segment[i];
        } 
    }
    return NULL;
}

extern void trace_session_begin(uint64_t ins_length, uint64_t reg_store_addr,
    uint64_t eip, uint64_t mem_access_count);
static void* relkind_to_fixup_addr[] = {
    [LOAD_HELPER_TRACE_SESSION_BEGIN] = trace_session_begin,
    [LOAD_HELPER_UPDATE_MXCSR_STATUS] = update_mxcsr_status,
    [LOAD_HELPER_FPATAN] = helper_fpatan,
    [LOAD_HELPER_FPTAN] = helper_fptan,
    [LOAD_HELPER_FPREM] = helper_fprem,
    [LOAD_HELPER_FPREM1] = helper_fprem1,
    [LOAD_HELPER_FRNDINT] = helper_frndint,
    [LOAD_HELPER_F2XM1] = helper_f2xm1,
    [LOAD_HELPER_FXTRACT] = helper_fxtract,
    [LOAD_HELPER_FYL2X] = helper_fyl2x,
    [LOAD_HELPER_FYL2XP1] = helper_fyl2xp1,
    [LOAD_HELPER_FSINCOS] = helper_fsincos,
    [LOAD_HELPER_FSIN] = helper_fsin,
    [LOAD_HELPER_FCOS] = helper_fcos,
    [LOAD_HELPER_FBLD_ST0] = helper_fbld_ST0,
    [LOAD_HELPER_FBST_ST0] = helper_fbst_ST0,
    [LOAD_HELPER_FXSAVE] = helper_fxsave,
    [LOAD_HELPER_FXRSTOR] = helper_fxrstor,
    [LOAD_HELPER_UPDATE_FP_STATUS] = update_fp_status,
    [LOAD_HELPER_CONVERT_FPREGS_64_TO_X80] = convert_fpregs_64_to_x80,
    [LOAD_HELPER_CONVERT_FPREGS_X80_TO_64] = convert_fpregs_x80_to_64,
    [LOAD_HELPER_CPUID] = helper_cpuid,
    [LOAD_HELPER_RAISE_INT] = helper_raise_int,
    [LOAD_HELPER_RAISE_ILLOP] = helper_raise_illop,
    [LOAD_HELPER_RAISE_TRAPOP] = helper_raise_trapop,
    [LOAD_HELPER_RAISE_GPF] = helper_raise_gpf,
#ifdef TARGET_X86_64
    [LOAD_HELPER_RAISE_SYSCALL] = helper_raise_syscall,
#endif

    [LOAD_HOST_POW] = pow,
    [LOAD_HOST_SIN] = sin,
    [LOAD_HOST_COS] = cos,
    [LOAD_HOST_ATAN2] = atan2,
    [LOAD_HOST_LOGB] = logb,
    [LOAD_HOST_LOG2] = log2,
    [LOAD_HOST_SINCOS] = sincos,
    [LOAD_HOST_PFTABLE] = pf_table,
    [LOAD_HOST_LATLOCK] = lat_lock,
    [LOAD_HOST_RAISE_EX] = helper_raise_exception,
    [LOAD_HELPER_XGETBV] = helper_xgetbv,
    [LOAD_HELPER_EFLAGTF] = helper_eflagtf,
    [LOAD_PAGEFLAGS_ROOT] = &pageflags_root,
};

void aot_do_tb_reloc(TranslationBlock *tb, struct aot_tb *stb,
    target_ulong seg_begin, target_ulong seg_end)
{
    uint32_t *pinsn;
    uint64_t offset;
    uintptr_t helper_address;
    int lib_method_index;
    CPUArchState* env = (CPUArchState*)(lsenv->cpu_state);
    target_ulong base = env->segs[R_CS].base;
    uint32_t parallel = tb->cflags & CF_PARALLEL;
    ADDR loacl_indirect_jmp_glue = indirect_jmp_glue;
    if (close_latx_parallel || parallel) {
        loacl_indirect_jmp_glue = parallel_indirect_jmp_glue;
    }

    aot_rel_table = aot_buffer +
        ((aot_header *)aot_buffer)->rel_table_offset;
    for (int i = stb->rel_start_index; i != -1 && i <= stb->rel_end_index; i++) {
        pinsn = (uint32_t *)(tb->tc.ptr + aot_rel_table[i].tc_offset);
        switch (aot_rel_table[i].kind) {
        case B_PROLOGUE:
            assert(((*pinsn) & 0xfc000000) == 0x50000000);
            offset = (context_switch_bt_to_native - (uintptr_t)pinsn) >> 2;
            *pinsn = (0x50000000 | ((offset & 0xffff) << 10) |
                      ((offset >> 16) & 0x3ff));
            break;
        case B_EPILOGUE:
            lsassert(((*pinsn) & 0xfc000000) == 0x50000000 ||
            (((*pinsn) & 0xfe000000) == 0x1e000000 &&/* pcaddu18i */
            (*(pinsn + 1) & 0xfc000000) == 0x4c000000));
            light_tb_target_set_jmp_target((uintptr_t)tb->tc.ptr,
            (uintptr_t)pinsn, (uintptr_t)pinsn, context_switch_native_to_bt);
            break;
        case B_EPILOGUE_RET_0:
            lsassert(((*pinsn) & 0xfc000000) == 0x50000000 ||
            (((*pinsn) & 0xfe000000) == 0x1e000000 &&/* pcaddu18i */
            (*(pinsn + 1) & 0xfc000000) == 0x4c000000));
            light_tb_target_set_jmp_target((uintptr_t)tb->tc.ptr,
            (uintptr_t)pinsn, (uintptr_t)pinsn, context_switch_native_to_bt_ret_0);
            break;
        case B_NATIVE_JMP_GLUE2:
            lsassert(((*pinsn) & 0xfc000000) == 0x50000000 ||
            (((*pinsn) & 0xfe000000) == 0x1e000000 &&/* pcaddu18i */
            (*(pinsn + 1) & 0xfc000000) == 0x4c000000));
            light_tb_target_set_jmp_target((uintptr_t)tb->tc.ptr,
            (uintptr_t)pinsn, (uintptr_t)pinsn, loacl_indirect_jmp_glue);
            break;
        case LOAD_TB_ADDR:
            lsassert(aot_rel_table[i].rel_slots_num == 3);
            lsassert((*pinsn & 0xfe000000) == 0x14000000); /* lu12i.w */
            *pinsn &= 0xfe00001f;
            *pinsn |= (((uintptr_t)tb >> 12) & 0xfffff) << 5;

            pinsn++;
            lsassert((*pinsn & 0xffc00000) == 0x03800000); /* ori */
            *pinsn &= 0xffc003ff;
            *pinsn |= ((uintptr_t)tb & 0xfff) << 10;

            pinsn++;
            lsassert((*pinsn & 0xfe000000) == 0x16000000); /* lu32i.d */
            *pinsn &= 0xfe00001f;
            *pinsn |= (((uintptr_t)tb >> 32) & 0xfffff) << 5;

            break;
        case LOAD_CALL_TARGET:
            lsassert((seg_begin <= tb->pc) && (seg_end >= tb->pc));
            uintptr_t call_target = aot_rel_table[i].extra_addend +
                seg_begin;
            if (!use_tu_jmp(tb) && ((*pinsn) & 0x1f) == 21) {
                for (int j = 0; j < 2; j++) {
                    if ((tb->jmp_reset_offset[j] != TB_JMP_RESET_OFFSET_INVALID) &&
                            (aot_rel_table[i].tc_offset == 
                             (tb->jmp_target_arg[j] + 20))) {
                        aot_link_tree_insert(thread_cpu, tb, (target_ulong)call_target,
                                (uint32_t *)(tb->tc.ptr + tb->jmp_target_arg[j]),
                                base, tb->flags, tb->flags, AOT_LINK_TYPE_TB_LINK);
                    }
                }
            }
            lsassert((*pinsn & 0xfe000000) == 0x14000000); /* lu12i.w */
            *pinsn &= 0xfe00001f;
            *pinsn |= ((call_target >> 12) & 0xfffff) << 5;

            pinsn++;
            lsassert((*pinsn & 0xffc00000) == 0x03800000); /* ori */
            *pinsn &= 0xffc003ff;
            *pinsn |= (call_target & 0xfff) << 10;

            pinsn++;
            if (aot_rel_table[i].rel_slots_num == 3) {
                *pinsn &= 0xfe00001f;
                *pinsn |= ((call_target >> 32) & 0xfffff) << 5;
            }else if(aot_rel_table[i].rel_slots_num == 2) {
               if (call_target >> 31) {
                    qemu_log_mask(LAT_LOG_AOT, "call_target %lx h32 no zero, but only have two slot\n",
                            call_target);
                    exit(-1);
               }
            }
            break;
        case LOAD_HELPER_BEGIN ... LOAD_HELPER_END:
            helper_address = (uintptr_t)relkind_to_fixup_addr[aot_rel_table[i].kind];
            lsassert(helper_address);
            lsassert((*pinsn & 0xfe000000) == 0x14000000); /* lu12i.w */
            *pinsn &= 0xfe00001f;
            *pinsn |= ((helper_address >> 12) & 0xfffff) << 5;

            pinsn++;
            lsassert((*pinsn & 0xffc00000) == 0x03800000); /* ori */
            *pinsn &= 0xffc003ff;
            *pinsn |= (helper_address & 0xfff) << 10;

            pinsn++;
            lsassert((*pinsn & 0xfe000000) == 0x16000000); /* lu32i.d */
            *pinsn &= 0xfe00001f;
            *pinsn |= ((helper_address >> 32) & 0xfffff) << 5;

            break;
        default:
            lib_method_index = aot_rel_table[i].kind - LOAD_TUNNEL_ADDR_BEGIN;
            lsassert(lib_method_index >= 0 &&
                lib_method_index < method_table_size);
            helper_address =
                (uintptr_t)method_table[lib_method_index].loongarch_addr;
            lsassert(helper_address);
            lsassert((*pinsn & 0xfe000000) == 0x14000000); /* lu12i.w */
            *pinsn &= 0xfe00001f;
            *pinsn |= ((helper_address >> 12) & 0xfffff) << 5;

            pinsn++;
            lsassert((*pinsn & 0xffc00000) == 0x03800000); /* ori */
            *pinsn &= 0xffc003ff;
            *pinsn |= (helper_address & 0xfff) << 10;

            pinsn++;
            lsassert((*pinsn & 0xfe000000) == 0x16000000); /* lu32i.d */
            *pinsn &= 0xfe00001f;
            *pinsn |= ((helper_address >> 32) & 0xfffff) << 5;

            break;
        }
    }
}

static aot_segment *get_segment(char *lib_name, uint64_t aot_offset,
        abi_long start, abi_long end) 
{
    aot_segment *p_segment;
    lib_info *lib = lib_tree_lookup(lib_name);
    if (lib == NULL) {
        aot_buffer = NULL;
        lib = aot_load(lib_name);
    } else if (!lib->is_unmapped){
        aot_buffer = lib->buffer;
    }
    if (aot_buffer == NULL) {
        return NULL;
    }
    p_segment = aot_find_segment(lib_name, aot_offset);
    if (p_segment == NULL) {
        return NULL;
    }

    if (p_segment->is_pe) {
        if (p_segment->details.seg_begin == start) {
            if (lib->is_unmapped) {
                return NULL;
            }
        } else {
            assert(lib);
            lib->is_unmapped = 1;
            return NULL;
        }
    }
    if ((p_segment->details.seg_begin >> 31 == 0)
            && (((uint64)(end) >> 31) != 0)
            && (start > p_segment->details.seg_begin)){
        qemu_log_mask(LAT_LOG_AOT, "seg start h32 not zero\n");
        assert(lib);
        lib->is_unmapped = 1;
        return NULL;
    }
        
    return p_segment;
}

static void aot_guest_code_protect(target_ulong seg_begin,
        target_ulong seg_end, aot_segment *p_segment)
{
    page_table_info *pt = (page_table_info *)
        (p_segment->page_table_offset + (uintptr_t)aot_buffer);
    int pt_num =
        (p_segment->details.seg_end - p_segment->details.seg_begin) >> TARGET_PAGE_BITS;
#ifdef CONFIG_LATX_DEBUG
    struct aot_tb *p_aot_tbs =
        (struct aot_tb *)(aot_buffer + p_segment->segment_tbs_offset);
    assert((uintptr_t)pt + (pt_num - 1) * sizeof(page_table_info) < (uintptr_t)p_aot_tbs);
#endif
    for (int i = 0; i < pt_num; i++) {
        if (pt[i].tb_num_in_page || pt[i].tb_num_in_page_parallel) {
            tb_page_addr_t page_addr = seg_begin + i * TARGET_PAGE_SIZE;
            int flags = page_get_flags(page_addr);
            if (flags & PAGE_WRITE) {
                target_ulong addr;
                int flags2;
                int prot;
                page_addr &= qemu_host_page_mask;
                prot = 0;
                for (addr = page_addr; addr < page_addr + qemu_host_page_size;
                        addr += TARGET_PAGE_SIZE) {
                    flags2 = page_get_flags(addr);
                    prot |= flags2;
                    page_set_flags(addr, addr + TARGET_PAGE_SIZE, flags2 & ~PAGE_WRITE);
                }
                mprotect(g2h_untagged(page_addr), qemu_host_page_size,
                        (prot & PAGE_BITS) & ~PAGE_WRITE);
            }

        }
    }
}

void recover_aot_tb(char *lib_name, uint64_t aot_offset, abi_long start, 
        abi_long len) 
{
    if (tcg_ctx->code_gen_ptr - tcg_ctx->code_gen_buffer >
        AOT_MAX_CODE_GEN_BUFFER_SIZE) {
        #ifdef CONFIG_LATX_DEBUG
        qemu_log_mask(LAT_LOG_AOT, "code buff is too large, no need aot.\n");
        #endif
        return;
    }
    /* First, we should identify whether this segment is in aot. */

    struct aot_segment *p_segment = 
        get_segment(lib_name, aot_offset, start, start + len);

    if (p_segment == NULL) {
        return;
    }
    /* Dump this segment info */
    if (option_debug_aot) {
        qemu_log_mask(LAT_LOG_AOT, "---------------------------------------------\n");
        qemu_log_mask(LAT_LOG_AOT,"buff %s\n", lib_name);
        qemu_log_mask(LAT_LOG_AOT, "---------------------------------------------\n");
    }

    seg_info *seg = segment_tree_lookup2(start, start + len);
    if (seg && (seg->seg_begin == start) && (seg->seg_end == start + len)) {
        seg->buffer = aot_buffer;
        seg->p_segment = p_segment;
    }
    aot_guest_code_protect(start, start + len, p_segment);
}

static void close_all_fd(void) 
{
    DIR *dir;
    struct dirent *entry;
    int retval, fd;
    
    dir = opendir("/dev/fd");
    if (dir == NULL) {
    	return;
    }
    /* rewind = 0; */
    while ((entry = readdir(dir)) != NULL) {
    	if (entry->d_name[0] == '.') {
    		continue;
    	}
    	fd = atoi(entry->d_name);
    	if (dirfd(dir) == fd) {
    		continue;
    	}
    	if (fd < 3) {
    		continue;
    	}
    
    	retval = close(fd);
    	if (retval) {
    		break;
    	}
    }
    
    closedir(dir);
    return;
}

static void creat_daemon(bool is_end)
{
    pid_t ppid = getpid();
    pid_t pid1 = fork();
    if (pid1) {
    	_exit(0);
    }
    close_all_fd();
    if (setsid() < 0) {
    	qemu_log_mask(LAT_LOG_AOT, "setid failed\n");
    	_exit(0);
    }
    /* chdir("~/"); */
    while (getppid() == ppid) {};
    umask(0);
}

void aot_exit_entry(CPUState *cpu, int is_end)
{
    if (!option_aot) {
        return;
    }
    if (in_pre_translate) {
	qemu_log_mask(LAT_LOG_AOT, "FIXME: tb flush in pre translate\n");
	_exit(0);
    }

    bool in_exclusive_context = cpu_in_exclusive_context(cpu);
    if (!in_exclusive_context) {
        start_exclusive();
    }

    sigset_t sigset;
    int signum = SIGCHLD;
    struct sigaction old_sa;

    /* Get sigset. */
    if (sigprocmask(0, NULL, &sigset) < 0) {
	qemu_log_mask(LAT_LOG_AOT, "get sigprocmask error\n");
        goto parent_exit;
    }

    /* Get old_sa. */
    if (sigaction(signum, NULL, &old_sa) < 0) {
        qemu_log_mask(LAT_LOG_AOT, "get sigaction error\n");
        goto parent_exit;
    }

    if (old_sa.sa_handler != SIG_DFL) {
        struct sigaction new_sa;
        new_sa.sa_handler = SIG_DFL;
        sigemptyset(&new_sa.sa_mask);
        new_sa.sa_flags = 0;
        if (sigaction(signum, &new_sa, NULL) < 0) {
            qemu_log_mask(LAT_LOG_AOT, "sigaction new_sa error\n");
            goto parent_exit;
        }
    }

    if (sigismember(&sigset, signum)) {
        sigset_t unblock_set;
        sigemptyset(&unblock_set);
        sigaddset(&unblock_set, signum);
        if (sigprocmask(SIG_UNBLOCK, &unblock_set, NULL) < 0) {
            qemu_log_mask(LAT_LOG_AOT, "unblock SIGCHLD error\n");
            goto parent_exit;
        }
    }

    pid_t pid = fork();
    if (pid) {
        if (pid > 0) {
            wait(NULL);
            if (old_sa.sa_handler != SIG_DFL) {
                sigaction(signum, &old_sa, NULL);
            }
            if (sigismember(&sigset, signum)) {
                sigemptyset(&sigset);
                sigaddset(&sigset, signum);
                sigprocmask(SIG_BLOCK, &sigset, NULL);
            }
        } else {
            qemu_log_mask(LAT_LOG_AOT, "fork 0 failed\n");
        }

parent_exit:
	if (!in_exclusive_context) {
	    end_exclusive();
	}
	return;
    }

    creat_daemon(is_end);

    aot_generate(cpu);
    _exit(EXIT_SUCCESS);
}

void aot_init(void)
{
    if (!option_aot) {
        return;
    }
    segment_tree_init();
    wine_sec_tree_init();
    smc_tree_init();
    if (option_load_aot) {
        aot_link_tree_init();
        merge_segment_tree_init();
        lib_tree_init();
        page_tree_init();
    }
}

target_ulong aot_get_call_offset(ADDRX addr)
{
    TranslationBlock *tb = lsenv->tr_data->curr_tb;
    target_ulong call_offset = 0;
    seg_info *curr_tb_seg = segment_tree_lookup(tb->pc);
    if (curr_tb_seg) {
        call_offset = addr - curr_tb_seg->seg_begin;
    }
    return call_offset;
}
#else
target_ulong aot_get_call_offset(ADDRX addr)
{
    return 0;
}
#endif
