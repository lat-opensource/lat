/**
 * @file aot_merge.c
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT optimization
 */
#include "aot_merge.h"
#include "latx-options.h"
#include "file_ctx.h"

#ifdef CONFIG_LATX_AOT

static inline gint tb_pc_cmp(gconstpointer a, gconstpointer b)
{
    tb_tmp_message *pa = (tb_tmp_message *)a;
    tb_tmp_message *pb = (tb_tmp_message *)b;

    assert(pa && pb && pa != pb);
    if (pa->pc > pb->pc) {
        return 1;
    } else if (pa->pc < pb->pc) {
        return -1;
    }
    uint32_t a_cflags = pa->cflags & CF_PARALLEL;
    uint32_t b_cflags = pb->cflags & CF_PARALLEL;
    return a_cflags > b_cflags ? 1 : (a_cflags < b_cflags ? -1 : 0);
}

static GTree *merge_segment_tree;
static void merge_seg_delete(gconstpointer a)
{
    merge_seg_info *oldkey = (merge_seg_info *)a;
    lsassert(oldkey);
    lsassert(oldkey->file_name);
    free(oldkey->file_name);
    lsassert(oldkey->tb_tree);
    g_tree_destroy(oldkey->tb_tree);
    free(oldkey);
}

static gint merge_seg_cmp(gconstpointer a, gconstpointer b)
{
    merge_seg_info *pa = (merge_seg_info *)a;
    merge_seg_info *pb = (merge_seg_info *)b;

    assert(pa && pb && pa != pb);
    lsassert(pa->file_name && pb->file_name);
    int str_ret = strcmp(pa->file_name, pb->file_name);
    if (str_ret > 0) {
        return 1;
    } else if (str_ret < 0) {
        return -1;
    } else {
        if (pa->offset > pb->offset) {
            return 1;
        } else if (pa->offset < pb->offset) {
            return -1;
        } else {
            return 0;/*a_offset == b_offset*/
        }
    }
}

void merge_segment_tree_init(void)
{
    merge_segment_tree = g_tree_new_full((GCompareDataFunc)merge_seg_cmp,
        NULL, NULL, (GDestroyNotify)merge_seg_delete);
    lsassert(merge_segment_tree);
}

static int aot_merge_tmp;
static gboolean merge_dump_segment_tree_node(gpointer key, gpointer val,
    gpointer data)
{
    merge_seg_info **vec = (merge_seg_info **)data;
    merge_seg_info *seg = (merge_seg_info *)val;
    vec[aot_merge_tmp++] = seg;
    return 0;
}

static gint merge_tb_cmp(gconstpointer a, gconstpointer b)
{
    merge_tb_info *pa = (merge_tb_info *)a;
    merge_tb_info *pb = (merge_tb_info *)b;

    assert(pa && pb && pa != pb);
    assert(pa->tb && pb->tb);
    uint32 pa_cflags = pa->tb->cflags & CF_PARALLEL;
    uint32 pb_cflags = pb->tb->cflags & CF_PARALLEL;

    if (pa_cflags > pb_cflags) {
	    return 1;
    } else if (pa_cflags < pb_cflags) {
	    return -1;
    }
    return pa->offset < pb->offset ? -1 : pa->offset > pb->offset;
}

static void merge_tb_tree_init(GTree **tree)
{
    *tree = g_tree_new_full((GCompareDataFunc)merge_tb_cmp,
       NULL, NULL, NULL);
    lsassert(*tree);
}

static merge_seg_info *merge_segment_tree_lookup(char *file_name, size_t offset)
{
    merge_seg_info key = {.file_name = file_name, .offset = offset};
    return (merge_seg_info *)g_tree_lookup(merge_segment_tree, &key);
}

static gint merge_get_segment_num(void)
{
    return g_tree_nnodes(merge_segment_tree);
}

static void merge_do_segment_record(merge_seg_info **seg_info_vector)
{
    g_tree_foreach(merge_segment_tree,
        merge_dump_segment_tree_node, seg_info_vector);
}


static aot_rel *merge_rel_table;
static int merge_rel_entry_num;
static int merge_rel_table_capacity;
static int32_t cp_rel_entry(aot_rel *old_rel_table)
{
    if (merge_rel_table_capacity == 0) {
        merge_rel_table_capacity = 10000;
        merge_rel_entry_num = 0;
        merge_rel_table =
            (aot_rel *)malloc(merge_rel_table_capacity * sizeof(aot_rel));
    } else if (merge_rel_table_capacity == merge_rel_entry_num) {
        merge_rel_table_capacity <<= 1;
        merge_rel_table =
            (aot_rel *)realloc(merge_rel_table,
            merge_rel_table_capacity * sizeof(aot_rel));
    }
    int i = merge_rel_entry_num++;
    assert(merge_rel_entry_num <= merge_rel_table_capacity);
    memcpy(&merge_rel_table[i], old_rel_table, sizeof(aot_rel));
    return i;
}
static gboolean merge_dump_seg_tb_tree_node(gpointer key, gpointer val,
                                       gpointer data)
{
    aot_tb **vec = (aot_tb **)data;
    merge_tb_info *seg = (merge_tb_info *)val;
    vec[aot_merge_tmp++] = seg->tb;
    return 0;
}
static gboolean merge_dump_rel_table_tree_node(gpointer key, gpointer val,
                                       gpointer data)
{
    merge_tb_info *seg = (merge_tb_info *)val;
    int32_t start = -1;
    int32_t end = -1;
    for (int i = 0; i < seg->rel_table_num; i++) {
        int32_t r_index =
            cp_rel_entry(seg->aot_rel_table + i);
        if (i == 0) {
            start = r_index;
        }
        end = r_index;
    }
    seg->tb->rel_start_index = start;
    seg->tb->rel_end_index = end;
/* #ifdef AOT_DEBUG */
    uint32_t *pinsn;
    for (int i = start; i != -1 && i <= end; i++) {
        pinsn = (uint32_t *)(seg->tb->tb_cache_addr +
            merge_rel_table[i].tc_offset);
        switch (merge_rel_table[i].kind) {
        case B_PROLOGUE:
            assert(((*pinsn) & 0xfc000000) == 0x50000000);
            break;
        case B_EPILOGUE:
            lsassert(((*pinsn) & 0xfc000000) == 0x50000000 ||
              (((*pinsn) & 0xfe000000) == 0x1e000000 &&/* pcaddu18i */
              (*(pinsn + 1) & 0xfc000000) == 0x4c000000));
            break;
        case B_EPILOGUE_RET_0:
            lsassert(((*pinsn) & 0xfc000000) == 0x50000000 ||
              (((*pinsn) & 0xfe000000) == 0x1e000000 &&/* pcaddu18i */
              (*(pinsn + 1) & 0xfc000000) == 0x4c000000));
            break;

        case B_NATIVE_JMP_GLUE2:
            lsassert(((*pinsn) & 0xfc000000) == 0x50000000 ||
              (((*pinsn) & 0xfe000000) == 0x1e000000 &&/* pcaddu18i */
              (*(pinsn + 1) & 0xfc000000) == 0x4c000000));
            break;
        case LOAD_TB_ADDR:
            lsassert(merge_rel_table[i].rel_slots_num == 3);
            lsassert((*pinsn & 0xfe000000) == 0x14000000); /* lu12i.w */
            pinsn++;
            lsassert((*pinsn & 0xffc00000) == 0x03800000); /* ori */
            pinsn++;
            lsassert((*pinsn & 0xfe000000) == 0x16000000); /* lu32i.d */
            break;
        case LOAD_CALL_TARGET:
            lsassert((*pinsn & 0xfe000000) == 0x14000000); /* lu12i.w */

            pinsn++;
            lsassert((*pinsn & 0xffc00000) == 0x03800000); /* ori */

            pinsn++;
            break;
        case LOAD_HELPER_BEGIN ... LOAD_HELPER_END:
            lsassert((*pinsn & 0xfe000000) == 0x14000000); /* lu12i.w */

            pinsn++;
            lsassert((*pinsn & 0xffc00000) == 0x03800000); /* ori */

            pinsn++;
            lsassert((*pinsn & 0xfe000000) == 0x16000000); /* lu32i.d */

            break;
        default:
            lsassert((*pinsn & 0xfe000000) == 0x14000000); /* lu12i.w */

            pinsn++;
            lsassert((*pinsn & 0xffc00000) == 0x03800000); /* ori */

            pinsn++;
            lsassert((*pinsn & 0xfe000000) == 0x16000000); /* lu32i.d */

            break;
        }
    }
    return 0;
}

static void merge_do_seg_tb_record(GTree *tree, aot_tb **aot_tb_vector)
{
    g_tree_foreach(tree, merge_dump_seg_tb_tree_node, aot_tb_vector);
}
static void merge_do_re_table_init_every(GTree *tree)
{
    g_tree_foreach(tree, merge_dump_rel_table_tree_node, NULL);
}
static gboolean dump_re_table_init_every_tree_node(gpointer key, gpointer val,
   gpointer data)
{
    merge_seg_info *seg = (merge_seg_info *)val;
    merge_do_re_table_init_every(seg->tb_tree);
    return 0;
}
static void merge_do_re_table_init(void)
{
    g_tree_foreach(merge_segment_tree,
        dump_re_table_init_every_tree_node, NULL);
}
static void merge_tb_tree_insert(GTree *tree, size_t offset,
    struct aot_tb *tb, aot_rel *aot_rel_table, int rel_table_num)
{
    merge_tb_info *seg = (merge_tb_info *)malloc(sizeof(merge_tb_info));
    if (seg == NULL) {
        qemu_log_mask(LAT_LOG_AOT, "Error! No memory for tree insert alloc!\n");
        _exit(-1);
    }
    seg->offset = offset;
    seg->tb = tb;
    seg->aot_rel_table = aot_rel_table;
    seg->rel_table_num = rel_table_num;
    /* Now insert this new segment into segment_tree */
    g_tree_replace(tree, seg, seg);
}

static merge_tb_info *merge_tb_tree_lookup(GTree *tree, struct aot_tb *tb)
{
    lsassert(tree);
    merge_tb_info key = {.offset = tb->offset_in_segment, .tb = tb};
    return (merge_tb_info *)g_tree_lookup(tree, &key);
}
static gboolean dump_tb_nums_tree_node(gpointer key, gpointer val,
   gpointer data)
{
    size_t *nums = (size_t *)data;
    merge_seg_info *seg = (merge_seg_info *)val;
    *nums += g_tree_nnodes(seg->tb_tree);
    return 0;
}
static size_t merge_get_tb_num(void)
{
    size_t nums = 0;
    g_tree_foreach(merge_segment_tree, dump_tb_nums_tree_node, &nums);
    return nums;
}

static void do_merge_tb_aot(merge_seg_info *seg_info,
    struct aot_segment *seg, void *cur_aot_buffer)
{
    assert(seg->details.file_offset == seg_info->s_info->file_offset);
    assert((seg->details.seg_end - seg->details.seg_begin)
            == (seg_info->s_info->seg_end - seg_info->s_info->seg_begin));
    aot_rel *cur_rel_table = cur_aot_buffer +
      ((aot_header *)cur_aot_buffer)->rel_table_offset;
    struct aot_tb *p_aot_tbs =
        (struct aot_tb *)(cur_aot_buffer + seg->segment_tbs_offset);
    for (int i = 0; i < seg->segment_tbs_num; i++) {
        GTree *tree = seg_info->tb_tree;
        size_t pc_offset = p_aot_tbs[i].offset_in_segment;
        if (merge_tb_tree_lookup(tree, &p_aot_tbs[i])) {
            continue;
        }
        int rel_table_num = -1;
        aot_rel *tmp_rel_table = NULL;
        if ((p_aot_tbs[i].rel_start_index != -1) &&
            p_aot_tbs[i].rel_end_index != -1) {
            int ret_index_tmp = p_aot_tbs[i].rel_start_index;
            lsassert(ret_index_tmp >= 0);
            tmp_rel_table = cur_rel_table + ret_index_tmp;
            rel_table_num =
                p_aot_tbs[i].rel_end_index - p_aot_tbs[i].rel_start_index + 1;
        }
        p_aot_tbs[i].tb_cache_addr =
            cur_aot_buffer + p_aot_tbs[i].tb_cache_offset;
        lsassert(p_aot_tbs[i].offset_in_segment <=
            seg_info->s_info->seg_end - seg_info->s_info->seg_begin);
        merge_tb_tree_insert(tree, pc_offset, p_aot_tbs + i,
            tmp_rel_table, rel_table_num);
    }
}

static int page_count;
static page_table_info *aot_page_table;

static struct aot_header *get_aot_buffer(
        merge_seg_info **merge_seg_info_vector, int curr_lib_seg_num)
{
    int aot_tb_num = merge_get_tb_num();
    lsassert(aot_tb_num);
    page_count = 0;
    for (int i = 0; i < curr_lib_seg_num; i++) {
        seg_info *seg = merge_seg_info_vector[i]->s_info;
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
        sizeof(struct aot_tb) * aot_tb_num +            /* AOT tb table size */
        sizeof(struct aot_rel) * merge_rel_entry_num +/* AOT rel table size */
        256;                                      /* extra alignment bytes. */
    return (struct aot_header *)malloc(aot_buffer_size);
}

static void init_page_table(struct aot_segment *p_segments, 
        aot_header *p_header, int curr_lib_seg_num) 
{
    aot_segment *curr_seg;
    curr_seg = p_segments;
    for (int i = 0; i < curr_lib_seg_num; i++) {
        int curr_seg_page_num = (curr_seg->details.seg_end
                - curr_seg->details.seg_begin
                + TARGET_PAGE_SIZE) / TARGET_PAGE_SIZE;
        page_table_info *pt = 
            (page_table_info *)(curr_seg->page_table_offset + (uintptr_t)p_header);
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

static void merge_aot_generate(void)
{
    /*
     * Prepare all TBs which will be handled later.
     * 1. Calculate TBs number.
     * 2. Allocate memory to store these TBs.
     * 3. Sort this vector so we can handle continuously.
     */
    int seg_info_num = merge_get_segment_num();
    lsassert(seg_info_num);
    merge_seg_info **merge_seg_info_vector =
        (merge_seg_info **)malloc(seg_info_num * sizeof(merge_seg_info *));
    aot_merge_tmp = 0;
    merge_do_segment_record(merge_seg_info_vector);
    merge_do_re_table_init();

    struct aot_header *p_header = 
        get_aot_buffer(merge_seg_info_vector, seg_info_num);
    assert(p_header && "allocatge aot_buffer failed\n");
    p_header->segments_num = seg_info_num;
    struct stat statbuf;
    if (stat(merge_seg_info_vector[0]->file_name, &statbuf)) {
        qemu_log_mask(LAT_LOG_AOT, "ERROT stat %s failed\n", merge_seg_info_vector[0]->file_name);
        return;
    }
    p_header->lib_size = statbuf.st_size;
    p_header->last_modify_time = statbuf.st_mtim;

    /* STEP 2: fill in struct aot_segment table. */
    struct aot_segment *p_segments =
        (struct aot_segment *)ROUND_UP((uintptr_t)(p_header + 1), 8);
    p_header->segment_table_offset =
        (uintptr_t)p_segments - (uintptr_t)p_header;

    aot_page_table = (page_table_info *)(p_segments + seg_info_num);
    char *aot_x86_lib_names = (char *)(aot_page_table + page_count);
    char *last_name = aot_x86_lib_names;
    char *curr_name = aot_x86_lib_names;
    bool is_pe = is_pe_file(merge_seg_info_vector[0]->file_name);
    p_header->is_pe |= is_pe;
    int page_index = 0;
    for (int i = 0; i < seg_info_num; i++) {
        seg_info *curr_seg_info = merge_seg_info_vector[i]->s_info;
        char *file_name = merge_seg_info_vector[i]->file_name;
        memcpy(&p_segments[i].details, curr_seg_info, sizeof(seg_info));
        if (option_debug_aot) {
            fprintf(
                stderr,
                "Gen seg name %s offset 0x" TARGET_FMT_lx
                ", p_segments.details %p details.file_offset 0x"
                TARGET_FMT_lx "\n",
                curr_seg_info->file_name, curr_seg_info->file_offset,
                &p_segments[i].details, p_segments[i].details.file_offset);
        }

        if (last_name == curr_name ||
            strcmp(last_name, file_name)) {
            strcpy(curr_name, file_name);
            last_name = curr_name;
            curr_name += strlen(file_name) + 1;
        }

        p_segments[i].is_pe = is_pe;
        p_segments[i].lib_name_offset =
            (uintptr_t)last_name - (uintptr_t)p_header;
        p_segments[i].segment_tbs_num = 0;
        p_segments[i].page_table_offset = 
            (uintptr_t)(aot_page_table + page_index) - (uintptr_t)p_header;
        page_index += (curr_seg_info->seg_end - curr_seg_info->seg_begin
                + TARGET_PAGE_SIZE) / TARGET_PAGE_SIZE;
    }

    init_page_table(p_segments, p_header, seg_info_num);

    struct aot_tb *p_aot_tbs =
        (struct aot_tb *)ROUND_UP((uintptr_t)curr_name, 8);

    struct aot_segment *curr_seg = p_segments;
    struct aot_tb *curr_aot_tb = p_aot_tbs;
    unsigned long total_code_cache_size = 0;
	/* unsigned long total_tb_num = 0; */
    for (int i = 0; i < seg_info_num; i++) {
        int cur_aot_tb_vector_num =
            g_tree_nnodes(merge_seg_info_vector[i]->tb_tree);
        if (cur_aot_tb_vector_num > 0) {
			/* total_tb_num += cur_aot_tb_vector_num; */
            aot_tb **cur_aot_tb_vector =
                (aot_tb **)malloc(cur_aot_tb_vector_num * sizeof(aot_tb *));
            aot_merge_tmp = 0;
            merge_do_seg_tb_record(merge_seg_info_vector[i]->tb_tree,
                cur_aot_tb_vector);
            for (int j = 0; j < cur_aot_tb_vector_num; j++) {
                if (curr_seg->segment_tbs_num++ == 0) {
                    curr_seg->segment_tbs_offset =
                        (uintptr_t)curr_aot_tb - (uintptr_t)p_header;
                }
                if (j > 0) {
		            if (cur_aot_tb_vector[j]->offset_in_segment ==
                        cur_aot_tb_vector[j - 1]->offset_in_segment) {
			            assert((cur_aot_tb_vector[j]->offset_in_segment ==
                                cur_aot_tb_vector[j - 1]->offset_in_segment) &&
                                (cur_aot_tb_vector[j]->cflags != 
                                cur_aot_tb_vector[j - 1]->cflags));
		            }
                }
                memcpy(curr_aot_tb, cur_aot_tb_vector[j], sizeof(aot_tb));
                total_code_cache_size += curr_aot_tb->tu_size;
                fill_page_table(curr_seg, p_header, curr_aot_tb);
                curr_aot_tb++;
            }
            free(cur_aot_tb_vector);
        }
        curr_seg++;
    }

    struct aot_rel *p_aot_rel =
        (struct aot_rel *)ROUND_UP((uintptr_t)curr_aot_tb, 8);
    p_header->rel_table_offset = (uintptr_t)p_aot_rel - (uintptr_t)p_header;
    memcpy(p_aot_rel, merge_rel_table,
        merge_rel_entry_num * sizeof(struct aot_rel));
    p_header->rel_entry_num = merge_rel_entry_num;
    uint32_t *p_insn =
        (uint32_t *)ROUND_UP((uintptr_t)(p_aot_rel + merge_rel_entry_num), 8);
    uint32_t *insn_buffer = (uint32_t *)malloc(total_code_cache_size);
    assert(insn_buffer && "insn_buffer malloc failed!");

    uint64_t two_buffer_off = (void *)insn_buffer - (void *)p_insn;
    uint32_t *curr_insn = insn_buffer;
    /* Fixup all tb_cache_offset. */
    for (struct aot_tb *p = p_aot_tbs; p < curr_aot_tb; ++p) {
        memcpy(curr_insn, p->tb_cache_addr, p->tu_size);
        /* TODO! unlink this TB. */
        p->tb_cache_offset =
            (uintptr_t)curr_insn - two_buffer_off - (uintptr_t)p_header;
        curr_insn += p->tu_size >> 2;
    }
    assert((void *)curr_insn - (void *)insn_buffer == total_code_cache_size);
    if (total_code_cache_size == 0) {
        goto out;
    }
    /* Write file metadata infomation(aot_buffer) into aot. */

    aot_file_rmgroup(aot_file_path);
    int fd = open(aot_file_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    FILE *pfile = fdopen(fd, "w");
    if (pfile == NULL) {
        qemu_log_mask(LAT_LOG_AOT, "Error! write aot metadata failed!\n");
        goto out;
    }
    size_t write_size = (uintptr_t)p_insn - (uintptr_t)p_header;
    if (fwrite(p_header, write_size, 1, pfile) != 1) {
        qemu_log_mask(LAT_LOG_AOT, "Error! write aot metadata failed!\n");
        fclose(pfile);
        goto out;
    }
    if (fwrite(insn_buffer, total_code_cache_size, 1, pfile) != 1) {
        qemu_log_mask(LAT_LOG_AOT, "Error! write aot translated code failed!\n");
        fclose(pfile);
        goto out;
    }
    if (fwrite(AOT_VERSION, strlen(AOT_VERSION), 1, pfile) != 1) {
        qemu_log_mask(LAT_LOG_AOT, "Error! write aot AOT_VERSION failed!\n");
        fclose(pfile);
        goto out;
    }
    if (fclose(pfile) != 0) {
        qemu_log_mask(LAT_LOG_AOT, "Error! close aot file failed\n");
        goto out;
    }
out:
    free(p_header);
    free(insn_buffer);
    free(merge_seg_info_vector);
    return;
}

void do_merge_seg_aot(void)
{
    struct stat statbuf;
    TaskState *ts = (TaskState *)thread_cpu->opaque;
    sigset_t oldmask, mask;
    sigfillset(&mask);
    sigdelset(&mask, SIGSEGV);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    qatomic_xchg(&ts->signal_pending, 1);
    if (lstat(aot_file_path, &statbuf)) {
        goto out;
    }
    if (statbuf.st_ctime != aot_st_ctime) {
        goto out;
    }

    for (int buf_index = 0;
        buf_index < aot_buffer_all_num; buf_index++) {
		/* int tb_count_in_buffer = 0; */
        struct aot_header *p_header =
            (struct aot_header *)aot_buffer_all[buf_index].p;
        struct aot_segment *p_segment =
            (struct aot_segment *)(aot_buffer_all[buf_index].p +
                p_header->segment_table_offset);
        for (int i = 0; i < p_header->segments_num; i++) {
            if (p_segment[i].segment_tbs_num == 0) {
                continue;
            }

	    /* tb_count_in_buffer += p_segment[i].segment_tbs_num; */
            char *lib_name = (char *)(aot_buffer_all[buf_index].p +
                p_segment[i].lib_name_offset);
            int seg_off_in_file = p_segment[i].details.file_offset;
            merge_seg_info *seg_info =
		    merge_segment_tree_lookup(lib_name, seg_off_in_file);
            if (!seg_info) {
                seg_info = (merge_seg_info *)malloc(sizeof(merge_seg_info));
                if (seg_info == NULL) {
                    qemu_log_mask(LAT_LOG_AOT,
                        "Error! No memory for tree insert alloc!\n");
                    _exit(-1);
                }
                seg_info->file_name = (char *)malloc(strlen(lib_name) + 1);
                if (seg_info->file_name == NULL) {
                    qemu_log_mask(LAT_LOG_AOT,
                        "Error! No memory for segment_tree_insert alloc!\n");
                    _exit(-1);
                }
                strncpy(seg_info->file_name, lib_name, strlen(lib_name) + 1);
                seg_info->offset = seg_off_in_file;
                seg_info->s_info = &p_segment[i].details;
                merge_tb_tree_init(&seg_info->tb_tree);
                g_tree_insert(merge_segment_tree, seg_info, seg_info);
            }
            do_merge_tb_aot(seg_info, &p_segment[i],
                aot_buffer_all[buf_index].p);
        }
    }
    merge_aot_generate();
out:
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    qatomic_xchg(&ts->signal_pending, 0);
}

static int aot_load_no_lock(char *lib_name)
{
    aot_buffer_all_num = 0;
    if (lib_name == NULL) {
        return -1;
    }
    get_aot_path(lib_name, aot_file_path);
    if (access(aot_file_path, 0) < 0) {
        return -1;
    }
    void *buffer;
    struct stat statbuf;
    int count = aot_get_file_init(aot_file_path);
    size_t total_file_sz = 0;
    char aot_version[strlen(AOT_VERSION) + 1];
    lsassert(count > 0);
    char tmp_file_path[PATH_MAX];
    if (lstat(aot_file_path, &statbuf)) {
        aot_buffer_all_num = 0;
        return -1;
    }
    aot_st_ctime = statbuf.st_ctime;
    int j = 0;
    for (int i = 0; i < count; i++) {
        memset(tmp_file_path, 0, PATH_MAX);
        aot_get_file_name(aot_file_path, tmp_file_path, i);
        int fd = open(tmp_file_path, O_RDONLY);
        FILE *pf = fdopen(fd, "r");
        lsassert(pf && ("open aot file failed!"));
        /* Get file size */
        fseek(pf, 0, SEEK_END);      /* seek to end of file */
        size_t file_sz = ftell(pf);  /* get current file pointer */
        /*check aot complete.*/
        if (fseek(pf, -strlen(AOT_VERSION), SEEK_END) != 0) {
            aot_buffer_all_num--;
            fclose(pf);
            continue;
        }
        if (fread(aot_version, strlen(AOT_VERSION), 1, pf) != 1) {
            aot_buffer_all_num--;
            fclose(pf);
            continue;
        }
        if (!strstr(aot_version, AOT_VERSION)) {
            fclose(pf);
            for (int ii = i; ii < count; ii++) {
                aot_get_file_name(aot_file_path, tmp_file_path, ii);
                remove(tmp_file_path);
            }
            aot_buffer_all_num = j;
            return 0;
        }

        fseek(pf, 0, SEEK_SET);      /* seek back to beginning of file */

        /* Read aot file */
        buffer = malloc(file_sz);
        assert(buffer);
        aot_buffer_all[j].maplen = file_sz;
        if ((int)(intptr_t)buffer == -1) {
            aot_buffer_all_num--;
            fclose(pf);
            continue;
        }
        if (fread(buffer, file_sz, 1, pf) != 1) {
            qemu_log_mask(LAT_LOG_AOT, "fread error %s.\n", strerror(errno));
            aot_buffer_all_num--;
            fclose(pf);
            continue;
        }
        fclose(pf);
        aot_buffer_all[j].p = buffer;
        j++;
        /* align with 8 bytes. */
        total_file_sz += file_sz;
    }
    return total_file_sz;
}

#ifdef CONFIG_LATX_TU
static int get_tb_num(struct aot_segment *p_segment, int seg_num) {
	int tb_num = 0;
	for (int i = 0; i < seg_num; i++) {
		tb_num += p_segment[i].segment_tbs_num;
	}
	return tb_num;
}

static tb_tmp_message *tb_message_vector;
static int curr_tb_num, all_tb_num;

static struct aot_segment *get_segment(struct aot_segment *p_segment,
		int seg_num, target_ulong offset) {
    for (int i = 0; i < seg_num; i++) {
    	if (p_segment[i].details.file_offset == offset) {
    		return &p_segment[i];
    	}
    }
    return NULL;
}

static void merge_seg(seg_info *curr_seg, void *buffer1, void *buffer2,
		struct aot_segment *seg_in_aot1, struct aot_segment *seg_in_aot2)
{
	int tb_num1 = 0;
	int tb_num2 = 0;
	struct aot_tb *p_aot_tbs1 = NULL, *p_aot_tbs2 = NULL;
	if (seg_in_aot1) {
		tb_num1 = seg_in_aot1->segment_tbs_num;
		p_aot_tbs1 = 
			(struct aot_tb*)(buffer1 + seg_in_aot1->segment_tbs_offset);
	}
	if (seg_in_aot2) {
		tb_num2 = seg_in_aot2->segment_tbs_num;
		p_aot_tbs2 = 
			(struct aot_tb*)(buffer2 + seg_in_aot2->segment_tbs_offset);
	}
	if (tb_num1 || tb_num2) {
		curr_seg->first_tb_id = curr_tb_num;
	} else {
		curr_seg->first_tb_id = -1;
		return;
	}
	for (int i = 0; i < tb_num1; i++) {
#ifdef CONFIG_LATX_DEBUG
		if (curr_tb_num >= all_tb_num) {
			qemu_log_mask(LAT_LOG_AOT, "error %s\n", curr_seg->file_name);
		}
		assert(curr_tb_num < all_tb_num);
#endif
		tb_message_vector[curr_tb_num].pc = 
			p_aot_tbs1[i].offset_in_segment + curr_seg->seg_begin;	
		assert(tb_message_vector[curr_tb_num].pc < curr_seg->seg_end);
		tb_message_vector[curr_tb_num++].cflags = p_aot_tbs1[i].cflags;
	}
	for (int i = 0; i < tb_num2; i++) {
#ifdef CONFIG_LATX_DEBUG
		if (curr_tb_num >= all_tb_num) {
			qemu_log_mask(LAT_LOG_AOT, "error %s\n", curr_seg->file_name);
		}
		assert(curr_tb_num < all_tb_num);
#endif
		tb_message_vector[curr_tb_num].pc = 
			p_aot_tbs2[i].offset_in_segment + curr_seg->seg_begin;	
		tb_message_vector[curr_tb_num++].cflags = p_aot_tbs2[i].cflags;	
	}
}

static void get_seg_tb(int first_seg_id, int last_seg_id)
{
    if (!curr_tb_num) {
        return;
    }
    int end_id = 0;
    for (int i = 1; i < curr_tb_num; i++) {
    	assert(tb_message_vector[i].pc >= tb_message_vector[end_id].pc);
    	if ((tb_message_vector[end_id].pc != tb_message_vector[i].pc)
    			|| ((tb_message_vector[end_id].cflags & CF_PARALLEL)
    				!= (tb_message_vector[i].cflags & CF_PARALLEL))) {
    		tb_message_vector[++end_id].pc = tb_message_vector[i].pc;		
    		tb_message_vector[end_id].cflags = tb_message_vector[i].cflags;		
        }
    }
    curr_tb_num = end_id + 1;
    
    for (int i = first_seg_id; i < last_seg_id; i++) {
    	seg_info_vector[i]->first_tb_id = -1;
    	seg_info_vector[i]->last_tb_id = -2;
    }

    int seg_id = first_seg_id;
    seg_info *curr_seg;
    for (int tb_id = 0; tb_id < curr_tb_num; tb_id++) {
        /* If the current TB's PC is greater than the end of the current segment, */
        /* it is necessary to find a segment with a larger address. */
        /* If the current TB's PC is smaller than the starting address of the current segment, */
        /* then this TB will be skipped, and this should not happen under normal circumstances. */
        while(seg_id < last_seg_id && 
            seg_info_vector[seg_id]->seg_end <= tb_message_vector[tb_id].pc) {
            seg_id++;
        }
        if (seg_id >= last_seg_id) {
            /* If seg_id is added to last_seg_id, */ 
            /* it indicates that a TB does not belong to any segment. */
            assert(0);
            break;
        }
        curr_seg = seg_info_vector[seg_id];
        /* If TB belongs to the current segment, it will be recorded. */
        /* Otherwise, it will be skipped */
        if (tb_message_vector[tb_id].pc >= curr_seg->seg_begin &&
                tb_message_vector[tb_id].pc < curr_seg->seg_end) {
            if (curr_seg->first_tb_id == -1) {
                curr_seg->first_tb_id = tb_id;
            }
            curr_seg->last_tb_id = tb_id;
        } 
    }
}

static void aot2_merge_tu(char *curr_lib_name, int first_seg_id,
		int last_seg_id, CPUState *cpu)
{
    void *buffer1 = aot_buffer_all[0].p;
    void *buffer2 = aot_buffer_all[1].p;
    assert(buffer1);
    assert(buffer2);
    struct aot_header *p_header1 = (struct aot_header *)buffer1;
    struct aot_header *p_header2 = (struct aot_header *)buffer2;
    struct aot_segment *p_segment1 = (struct aot_segment *)(buffer1 
    		+ p_header1->segment_table_offset);
    struct aot_segment *p_segment2 = (struct aot_segment *)(buffer2 
    		+ p_header2->segment_table_offset);
    int seg_num1 = p_header1->segments_num;
    int seg_num2 = p_header2->segments_num;
    if (seg_num1 > 1200000) {
    	return;
    }
    int tb_num1 = get_tb_num(p_segment1, seg_num1);
    int tb_num2 = get_tb_num(p_segment2, seg_num2);
    all_tb_num = tb_num1 + tb_num2;
    curr_tb_num = 0;
    assert(tb_num1 && tb_num2);
    tb_message_vector = malloc((tb_num1 + tb_num2) * sizeof(tb_tmp_message));
        
#ifdef CONFIG_LATX_DEBUG
    for (int i = 1; i < seg_num1; i++) {
    	assert(p_segment1[i].details.file_offset > 
    			p_segment1[i - 1].details.file_offset);
    }
    for (int i = 1; i < seg_num2; i++) {
    	assert(p_segment2[i].details.file_offset > 
    			p_segment2[i - 1].details.file_offset);
    }
#endif

    seg_info *curr_seg;
    struct aot_segment *seg_in_aot1, *seg_in_aot2;
    for (int i = first_seg_id; i < last_seg_id; i++) {
    	curr_seg = seg_info_vector[i];
#ifdef CONFIG_LATX_DEBUG
	if (i > first_seg_id) {
		assert(curr_seg->file_offset != seg_info_vector[i - 1]->file_offset);
	}
#endif
	seg_in_aot1 = 
		get_segment(p_segment1, seg_num1, curr_seg->file_offset);
	seg_in_aot2 = 
		get_segment(p_segment2, seg_num2, curr_seg->file_offset);
#ifdef CONFIG_LATX_DEBUG
	target_ulong seg_len = curr_seg->seg_end - curr_seg->seg_begin;
        if (seg_in_aot1) {
	    assert((seg_in_aot1->details.seg_end - seg_in_aot1->details.seg_begin) == seg_len);
        }
        if (seg_in_aot2) {
	    assert((seg_in_aot2->details.seg_end - seg_in_aot2->details.seg_begin) == seg_len);
        }
#endif
	merge_seg(curr_seg, buffer1, buffer2, seg_in_aot1, seg_in_aot2);
    }
    qsort(tb_message_vector, curr_tb_num, sizeof(tb_tmp_message), tb_pc_cmp);
    get_seg_tb(first_seg_id, last_seg_id);
    
#ifdef CONFIG_LATX_DEBUG
    for (int i = 1; i < curr_tb_num; i++) {
    	assert(tb_message_vector[i].pc >= tb_message_vector[i - 1].pc);
    	assert(tb_message_vector[i].pc);
    }
#endif

    char tmp_file_path[PATH_MAX]; 
    strcpy(tmp_file_path, aot_file_path);
    strcat(tmp_file_path, "A");
    remove(tmp_file_path);
    /* The second and first AOT files are duplicated. */
    if (curr_tb_num <= tb_num1) {
    	assert(curr_tb_num >= tb_num1);
    	free(tb_message_vector);
    	return;
    }
    remove(aot_file_path);
    pre_translate(first_seg_id, last_seg_id, cpu, tb_message_vector);
    do_generate_aot(first_seg_id, last_seg_id);
    free(tb_message_vector);
}
#endif

#include<sys/syscall.h>
void aot2_merge(char *curr_lib_name, int first_seg_id, 
		int last_seg_id, CPUState *cpu)
{
    get_aot_path(curr_lib_name, aot_file_path);
    aot_load_no_lock(curr_lib_name);
    if (aot_buffer_all_num < 2) {
        return;
    }
#ifdef CONFIG_LATX_TU
	aot2_merge_tu(curr_lib_name, first_seg_id, last_seg_id, cpu);
#else
    g_tree_destroy(merge_segment_tree);
    merge_segment_tree_init();
    merge_rel_entry_num = 0;
    do_merge_seg_aot();
#endif
    aot_buffer_all_num = 0;
}
#endif
