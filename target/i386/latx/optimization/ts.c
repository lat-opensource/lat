/**
 * @file ts.c
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief TU optimization
 */
#include "ts.h"
#include "accel/tcg/internal.h"
#include "aot_smc.h"
#include "jrra.h"
#include "latx-options.h"
#include "latx-config.h"
#include "ir1.h"
#include "hbr.h"
#ifdef CONFIG_LATX_TU
#include "tu.h"
#endif

#ifdef CONFIG_LATX_FAST_JMPCACHE
#include "exec/fasttb.h"
#endif

/* static GTree *ts_tree; */
static __thread tb_tmp_message *dynamic_tb_message_vector;
static __thread uint32 dynamic_tb_num;
static __thread tb_tmp_message *curr_tb_message_vector;
static __thread uint64 tb_num_in_ts;
static __thread uint64 ts_vector_capacity;
__thread TranslationBlock **ts_vector;
__thread seg_info *curr_seg;
__thread int in_pre_translate;

/* Get tb id in dynamic_tb_message_vector. */
static int get_tb_id(ADDRX pc, int cflags)
{
    int left_id = curr_seg->first_tb_id;
    int right_id = curr_seg->last_tb_id;
    assert (left_id >= 0 && left_id <= right_id);
    int mid_id;
    while(left_id <= right_id) {
        mid_id = (left_id + right_id) >> 1;
        if ((curr_tb_message_vector[mid_id].cflags & CF_PARALLEL)
                < (cflags & CF_PARALLEL)){
            left_id = mid_id + 1;
        } else if ((curr_tb_message_vector[mid_id].cflags & CF_PARALLEL)
                > (cflags & CF_PARALLEL)){
            right_id = mid_id - 1;
        } else {
            if (curr_tb_message_vector[mid_id].pc < pc) {
                left_id = mid_id + 1;
            } else if (curr_tb_message_vector[mid_id].pc > pc) {
                right_id = mid_id - 1;
            } else {
                return mid_id;
            }
        }
    }
    return -1;
}

TranslationBlock *aot_tb_lookup(target_ulong pc, int cflags)
{
    int tb_id = get_tb_id(pc, cflags);
    if (tb_id < 0) {
        return NULL;
    }
    return curr_tb_message_vector[tb_id].tb;
}

int aot_tb_insert(TranslationBlock *tb)
{
    assert(tb);
    int tb_id = get_tb_id(tb->pc, tb->cflags);
    /* In current version, all aot_tb are from curr_tb_message_vector. */
    if (tb_id < 0) {
        return -1;
    }
    curr_tb_message_vector[tb_id].tb = tb;
    return tb_id;
}

static inline gint tb_sort_cmp(const void *ap, const void *bp)
{
    const TranslationBlock *a = *(const TranslationBlock **)ap;
    const TranslationBlock *b = *(const TranslationBlock **)bp;
    return a->pc < b->pc ? -1 : a->pc > b->pc;
}

target_ulong get_curr_seg_end(target_ulong pc)
{
    return curr_seg->seg_end;
}

static inline ADDRX get_page(ADDRX pc)
{
    return pc & TARGET_PAGE_MASK;
}

void get_dynamic_message(TranslationBlock **tb_list, int tb_num,
        seg_info **seg_info_vector, int *seg_info_num)
{
    dynamic_tb_message_vector = malloc(sizeof(tb_tmp_message) * tb_num);
    dynamic_tb_num = 0;
    assert(dynamic_tb_message_vector);
    int j = 0;
    for (int i = 0; i < tb_num; i++) {
        if (!tb_list[i] || !tb_list[i]->pc) {
            continue;
        }
        if (i && (tb_list[i]->pc == tb_list[i - 1]->pc)
                    && ((tb_list[i]->cflags & CF_PARALLEL)
                    == (tb_list[i - 1]->cflags & CF_PARALLEL))) {
            continue;
        }
        dynamic_tb_message_vector[dynamic_tb_num].pc = tb_list[i]->pc;
        dynamic_tb_message_vector[dynamic_tb_num].cflags = tb_list[i]->cflags;
        dynamic_tb_message_vector[dynamic_tb_num].bool_flags = tb_list[i]->bool_flags;
        while (j < *seg_info_num &&
                seg_info_vector[j]->seg_end <= tb_list[i]->pc) {
#ifdef CONFIG_LATX_DEBUG
            if (j > 0) {
                assert(seg_info_vector[j]->seg_begin
                        > seg_info_vector[j - 1]->seg_begin);
            }
#endif
            j++;
        }
        if (j < *seg_info_num) {
            seg_info *seg = seg_info_vector[j];
            if (seg->seg_begin <= tb_list[i]->pc
                    && seg->seg_end > tb_list[i]->pc) {
                if (seg->first_tb_id == -1) {
                    seg->first_tb_id = dynamic_tb_num;
                }
                seg->last_tb_id = dynamic_tb_num;
            }
        }
#ifdef CONFIG_LATX_DEBUG
	if (dynamic_tb_num) {
	    assert((dynamic_tb_message_vector[dynamic_tb_num].pc >
		dynamic_tb_message_vector[dynamic_tb_num - 1].pc)
		|| (dynamic_tb_message_vector[dynamic_tb_num].cflags
		!= dynamic_tb_message_vector[dynamic_tb_num - 1].cflags));
	}
#endif
	dynamic_tb_num++;
    }
}

char is_pe(char *file_name)
{
    char name_type [5] = {0};
    const char * pe_file [] = {".dll", ".exe", ".sys", ".drv",
        ".tlb", ".8bf", ".ttf", ".otf", ".bin", ".dat", ".pak", "real"};
    char is_pe = false;
    int name_len = strlen(file_name);
    for (int i = 0; i < 4; i++) {
        name_type[i] = tolower(file_name[name_len - 4 + i]);
    }
    for (int i = 0; i < sizeof(pe_file) / sizeof(const char *); i++) {
        if(strstr(name_type, pe_file[i])) {
           is_pe = true;
        }
    }
    char name_type2[8] = {0};
    const char * pe_file2 [] = {".dll.so", ".exe.so", ".sys.so",
    ".drv.so"};
    name_len = strlen(file_name);
    for (int i = 0; i < 7; i++) {
        name_type2[i] = tolower(file_name[name_len - 7 + i]);
    }
    for (int i = 0; i < sizeof(pe_file2) / sizeof(const char *); i++) {
        if(strstr(name_type2, pe_file2[i])) {
           is_pe = true;
        }
    }
    return is_pe;
}

#define SIGNATURE_LENGTH 4
int is_pe_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file");
        return 0;
    }
    char signature[SIGNATURE_LENGTH];
    size_t bytes_read = fread(signature, sizeof(char), SIGNATURE_LENGTH, file);
    fclose(file);
    if (bytes_read < SIGNATURE_LENGTH) {
        return 0;
    }
    return (signature[0] == 'M' && signature[1] == 'Z');
}

void ts_push_back(TranslationBlock *tb)
{
    if (tb_num_in_ts >= ts_vector_capacity) {
        ts_vector_capacity <<= 1;
        ts_vector = (TranslationBlock **)realloc(ts_vector,
                ts_vector_capacity * sizeof(TranslationBlock *));
    }
    assert(tb_num_in_ts < ts_vector_capacity);
#ifdef CONFIG_LATX_TU
	if (unlikely(tb->s_data->tu_tb_mode == TB_GEN_CODE)) {
		tb->s_data->offset_in_tu = 0;
	}
#endif
    ts_vector[tb_num_in_ts++] = tb;
}

void pop_back(void)
{
    assert(tb_num_in_ts);
    tb_num_in_ts--;
}

static inline void translate_init(CPUState *cpu,
        target_ulong *cs_base, uint32_t *flags, int *cflags)
{
    target_ulong pc;
    CPUArchState *env = (CPUArchState *)cpu->env_ptr;
    *cflags = curr_cflags(cpu);
    cpu_get_tb_cpu_state(env, &pc, cs_base, flags);
    if (ts_vector_capacity == 0) {
            ts_vector_capacity = 8192;
            ts_vector = (TranslationBlock **)malloc(ts_vector_capacity
            * sizeof(TranslationBlock*));

    }
#ifdef CONFIG_LATX_FAST_JMPCACHE
    latx_fast_jmp_cache_clear_all(cpu);
#endif
    for (int i = 0; i < TB_JMP_CACHE_SIZE; i++) {
        qatomic_set(&cpu->tb_jmp_cache[i], NULL);
    }
    tb_num_in_ts = 0;
}

void dump_ir1(TranslationBlock *tb)
{
#ifdef CONFIG_LATX_DEBUG
    IR1_INST *ir1_list = tb->s_data->ir1;
    for(int i = 0; i < tb->icount; i++) {
        IR1_INST pir1 = ir1_list[i];
        qemu_log_mask(LAT_LOG_AOT, ":%lx\t%s\t\t%s\n", pir1.info->address,
            pir1.info->mnemonic, pir1.info->op_str);
    }
#endif
}

#include<sys/syscall.h>
void my_debug_ts1(TranslationBlock *tb)
{
    dump_ir1(tb);
    qemu_log_mask(LAT_LOG_AOT, "-------------\n");
}

#include "translate.h"

/* #include "translate-all.c" */
char is_bad_tb(TranslationBlock *tb)
{
    if (tb == NULL
#ifdef CONFIG_LATX_TU
            || tb->s_data->tu_tb_mode == TU_TB_MODE_BROKEN
            || tb->s_data->tu_tb_mode == BAD_TB
#endif
            || tb->icount == 0
            || (tb->bool_flags & IS_TUNNEL_LIB)) {
        return true;
    }
    return false;
}

#ifndef CONFIG_LATX_TU

static inline void create_dynamic_tb(seg_info *seg, CPUState *cpu,
        target_ulong cs_base, uint32_t flags, int cflags)
{
    TranslationBlock *tb;
    for (int i = seg->first_tb_id; i <= seg->last_tb_id; i++) {
        target_ulong pc = dynamic_tb_message_vector[i].pc;
		int cflags = dynamic_tb_message_vector[i].cflags;
        assert(pc >= seg->seg_begin && pc < seg->seg_end);
        if (aot_tb_lookup(pc, cflags)) {
            continue;
        }
        mmap_trylock();
        tb = tb_gen_code(cpu, pc, cs_base, flags, cflags);
        mmap_unlock();
        if (is_bad_tb(tb)) {
            continue;
        }
        ts_push_back(tb);
    }
}

static inline int translate_static_tb(seg_info *seg, CPUState *cpu,
        target_ulong cs_base, uint32_t flags, int cflags, target_ulong pc)
{
    TranslationBlock *tb;
    if (pc >= seg->seg_begin && pc < seg->seg_end
            && !aot_tb_lookup(pc, cflags)) {
        mmap_trylock();
        tb = tb_gen_code(cpu, pc, cs_base, flags, cflags);
        mmap_unlock();
        if (is_bad_tb(tb)) {
            return -1;
        }
        ts_push_back(tb);
    }
    return 0;
}

static inline void get_static_tb(int begin_id_in_ts,
        seg_info *seg, CPUState *cpu, target_ulong cs_base,
        uint32_t flags, int cflags)
{
    TranslationBlock *tb;
    int64_t max_num_id  = dynamic_tb_num * 3 + begin_id_in_ts;
    if (max_num_id > 300000) {
		max_num_id = 300000;
    }
    for(int i = begin_id_in_ts;
            i < tb_num_in_ts && i < max_num_id ; i++) {
        tb = ts_vector[i];
        target_ulong ir1_next_pc = tb->s_data->next_pc;
        target_ulong ir1_target_pc = tb->s_data->target_pc;
        lsassert(ir1_next_pc);
        if (translate_static_tb(seg, cpu, cs_base,
                flags, cflags, ir1_next_pc) == -1) {
                return;
        }
        switch (tb->s_data->last_ir1_type) {
        case IR1_TYPE_BRANCH:
        case IR1_TYPE_JUMP:
        case IR1_TYPE_CALL:
            translate_static_tb(seg, cpu, cs_base,
                    flags, cflags, ir1_target_pc);
            break;
        }
    }
}

static inline void translate_by_tb(seg_info *seg, CPUState *cpu,
        target_ulong cs_base, uint32_t flags, int cflags)
{
    int begin_id_in_ts = tb_num_in_ts;
    create_dynamic_tb(seg, cpu, cs_base, flags, cflags);
    if (begin_id_in_ts == tb_num_in_ts) {
        return;
    }
    qsort(ts_vector + begin_id_in_ts, tb_num_in_ts - begin_id_in_ts,
            sizeof(TranslationBlock *), tb_sort_cmp);
}

#else

static inline unsigned int tb_jmp_cache_hash_func(target_ulong pc)
{
    return pc & (TB_JMP_CACHE_SIZE - 1);
}

static TranslationBlock *lookup_static_tb(target_ulong pc, int cflags,
        CPUState *cpu)
{
    TranslationBlock *tb;
    uint32_t hash = tb_jmp_cache_hash_func(pc);
    tb = qatomic_read(&cpu->tb_jmp_cache[hash]);
    if (tb && tb->pc == pc && tb->cflags == cflags) {
        return tb;
    } else {
        return NULL;
    }
}

static void save_static_tb(TranslationBlock *tb, CPUState *cpu)
{
    uint32_t hash = tb_jmp_cache_hash_func(tb->pc);
#ifdef CONFIG_LATX_FAST_JMPCACHE
    latx_fast_jmp_cache_add(cpu, hash, tb);
#endif
    qatomic_set(&cpu->tb_jmp_cache[hash], tb);
}

static TranslationBlock* create_static_tb(CPUState *cpu, target_ulong pc,
        target_ulong cs_base, uint32_t flags, int cflags, int max_insns)
{
    TranslationBlock* tb;
    if (unlikely(pc == 0)) {
	return NULL;
    }

    /* todo: now check 16bit(should 15bit for x86) */
    uint32_t check_zero = cpu_lduw_code((CPUArchState *)cpu->env_ptr, pc);
    if (unlikely(check_zero == 0)){
	return NULL;
    }

    tb = (TranslationBlock *)malloc(sizeof(TranslationBlock));
    tb->s_data = (struct separated_data *)malloc(sizeof(struct separated_data));
    if (tb == NULL || tb->s_data == NULL) {
        exit(-1);
    }
    tu_reset_tb(tb);
    tb->pc = pc;
    /* tb->cs_base = cs_base; */
    tb->flags = flags;
    tb->cflags = cflags;
    tcg_ctx->tb_cflags = cflags;
    tcg_ctx->tb_jmp_reset_offset = tb->jmp_reset_offset;
    if (TCG_TARGET_HAS_direct_jump) {
        tcg_ctx->tb_jmp_insn_offset = tb->jmp_target_arg;
        tcg_ctx->tb_jmp_target_addr = NULL;
    } else {
        tcg_ctx->tb_jmp_insn_offset = NULL;
        tcg_ctx->tb_jmp_target_addr = tb->jmp_target_arg;
    }

    target_disasm(tb, max_insns);

    tb->s_data->tu_tb_mode = TU_TB_MODE_STATIC;
    save_static_tb(tb, cpu);
    return tb;
}

static inline void get_next_tb(TranslationBlock *curr_tb, CPUState *cpu,
        target_ulong cs_base, uint32_t flags, int cflags, int max_insns)
{
    ADDRX next_tb_pc = curr_tb->s_data->next_pc;
    lsassert(next_tb_pc);
    curr_tb->s_data->next_tb[TU_TB_INDEX_NEXT] = NULL;
    curr_tb->tu_jmp[TU_TB_INDEX_NEXT] = TB_JMP_RESET_OFFSET_INVALID;
    int tb_id = get_tb_id(next_tb_pc, cflags);
    ADDRX curr_page = get_page(curr_tb->pc);
    TranslationBlock *next_tb;
    if (get_page(next_tb_pc) != curr_page) {
        return;
    }
    if (tb_id >= 0) {
        /* Get dynamic tb. */
        next_tb = aot_tb_lookup(next_tb_pc, cflags);
        bool tb_is_code64 = !!(curr_tb_message_vector[tb_id].bool_flags & IS_CODE64);
        if (!next_tb) {
            next_tb = tb_create(cpu, next_tb_pc, cs_base, flags,
                    cflags, max_insns, tb_is_code64, TU_TB_START_NORMAL);
            if (is_bad_tb(next_tb)) {
                next_tb = NULL;
            }
            tu_push_back(next_tb);
        }
        if (curr_tb->s_data->last_ir1_type == IR1_TYPE_BRANCH &&
                next_tb && next_tb->tc.ptr != NULL) {
            curr_tb->tu_jmp[TU_TB_INDEX_NEXT] = 0;
            /* next_tb = NULL; */
        }
    } else {
        /* Get static tb. */
        next_tb = lookup_static_tb(next_tb_pc, cflags, cpu);
        if (!next_tb) {
            next_tb = create_static_tb(cpu, next_tb_pc,
                    cs_base, flags, cflags, max_insns);
            if (next_tb) {
                next_tb->bool_flags |= (curr_tb->bool_flags & IS_CODE64);
                tu_push_back(next_tb);
            }
        }
    }
    curr_tb->s_data->next_tb[TU_TB_INDEX_NEXT] = next_tb;
}

static inline void get_target_tb(TranslationBlock *curr_tb, CPUState *cpu,
        target_ulong cs_base, uint32_t flags, int cflags, int max_insns)
{
    curr_tb->tu_jmp[TU_TB_INDEX_TARGET] = TB_JMP_RESET_OFFSET_INVALID;
    curr_tb->s_data->next_tb[TU_TB_INDEX_TARGET] = NULL;
    ADDRX target_tb_pc = curr_tb->s_data->target_pc;
    lsassert(target_tb_pc);
    int tb_id = get_tb_id(target_tb_pc, cflags);
    ADDRX curr_page = get_page(curr_tb->pc);
    TranslationBlock *target_tb;
    if (get_page(target_tb_pc) != curr_page) {
        return;
    }
    if (tb_id >= 0) {
        /* Get dynamic tb. */
        target_tb = aot_tb_lookup(target_tb_pc, cflags);
        bool tb_is_code64 = !!(curr_tb_message_vector[tb_id].bool_flags & IS_CODE64);
        if (!target_tb) {
            target_tb = tb_create(cpu, target_tb_pc, cs_base, flags,
                    cflags, max_insns, tb_is_code64, TU_TB_START_NORMAL);
            if (is_bad_tb(target_tb)) {
                target_tb = NULL;
            }
            tu_push_back(target_tb);
        }
        if (curr_tb->s_data->last_ir1_type == IR1_TYPE_BRANCH &&
                target_tb && target_tb->tc.ptr != NULL) {
            curr_tb->tu_jmp[TU_TB_INDEX_TARGET] = 0;
            /* target_tb = NULL; */
        }
    } else {
        /* Get static tb. */
        target_tb = lookup_static_tb(target_tb_pc, cflags, cpu);
        if (!target_tb) {
            target_tb = create_static_tb(cpu, target_tb_pc,
                    cs_base, flags, cflags, max_insns);
            if (target_tb) {
                target_tb->bool_flags |= (curr_tb->bool_flags & IS_CODE64);
                tu_push_back(target_tb);
            }
        }
    }
    curr_tb->s_data->next_tb[TU_TB_INDEX_TARGET] = target_tb;
}

static int untr_tb_id;

static inline void get_ts_queue(CPUState *cpu, target_ulong cs_base,
        uint32_t flags, int cflags, int max_insns, target_ulong curr_page)
{
    TranslationBlock** tb_list = tu_data->tb_list;
    uint32_t *tb_num_in_tu = &tu_data->tb_num;
    target_ulong curr_tb_pc, curr_tb_cflags;
    TranslationBlock *tb;
    uint32_t tb_id = 0;
    while (untr_tb_id <= curr_seg->last_tb_id && *tb_num_in_tu < MAX_TB_IN_TS) {
        curr_tb_pc = curr_tb_message_vector[untr_tb_id].pc;
        curr_tb_cflags = curr_tb_message_vector[untr_tb_id].cflags;
        if (get_page(curr_tb_pc) != curr_page || curr_tb_cflags != cflags) {
            break;
        }
	if(curr_tb_message_vector[untr_tb_id].tb) {
            continue;
        }
        bool tb_is_code64 = !!(curr_tb_message_vector[untr_tb_id].bool_flags & IS_CODE64);
        tb = tb_create(cpu, curr_tb_pc, cs_base, flags,
                curr_tb_cflags, max_insns, tb_is_code64 , TU_TB_START_ENTRY);
        tu_push_back(tb);
        untr_tb_id++;
        for (; tb_id <  *tb_num_in_tu && *tb_num_in_tu < MAX_TB_IN_TS;  tb_id++) {
#ifdef CONFIG_LATX_DEBUG
            if (get_tb_id(tb->pc, tb->cflags) < 0) {
                continue;
            }
#endif
            if (tu_data->ir1_num_in_tu + 2 * max_insns >= MAX_IR1_IN_TU) {
                fprintf(stderr, "WARNING too many ir1 %d\n", tu_data->ir1_num_in_tu);
                return;
            }
            tb = tb_list[tb_id];
            if (is_bad_tb(tb)) {
                continue;
            }
            switch (tb->s_data->last_ir1_type) {
                case IR1_TYPE_BRANCH:
                    if (get_tb_id(tb->s_data->target_pc, cflags) < 0 ||
                            get_tb_id(tb->s_data->next_pc, cflags) < 0) {
                        break;
                    }
                    get_next_tb(tb, cpu, cs_base, flags, cflags, max_insns);
                    get_target_tb(tb, cpu, cs_base, flags, cflags, max_insns);
                    break;
                case IR1_TYPE_CALLIN:
                case IR1_TYPE_NORMAL:
                case IR1_TYPE_SYSCALL:
                case IR1_TYPE_CALL:
                    get_next_tb(tb, cpu, cs_base, flags, cflags, max_insns);
                    break;
                case IR1_TYPE_JUMP:
                    get_target_tb(tb, cpu, cs_base, flags, cflags, max_insns);
                    break;
                case IR1_TYPE_JUMPIN:
                case IR1_TYPE_RET:
                    break;
                default:
                    lsassert(0);
            }
        }
        /* Big TU have bug, so break in there. */
        break;
    }
    lsassert(*tb_num_in_tu <= MAX_TB_IN_TS);
}

/* static int max_tb_num; */

static void delet_static_tb(TranslationBlock **tb_list, uint32_t *tb_num_in_tu)
{
    TranslationBlock *tb, *target_tb, *next_tb;
    /* Make next_tb or target_tb do not point to static tb. */
    for (int i = 0; i < *tb_num_in_tu; i++) {
        tb = tb_list[i];
        assert(tb);
        target_tb = tb->s_data->next_tb[TU_TB_INDEX_TARGET];
        next_tb = tb->s_data->next_tb[TU_TB_INDEX_NEXT];
        if (next_tb && next_tb->s_data->tu_tb_mode == TU_TB_MODE_STATIC) {
            tb->s_data->next_tb[TU_TB_INDEX_NEXT] = NULL;
        }
        if (target_tb && target_tb->s_data->tu_tb_mode == TU_TB_MODE_STATIC) {
            tb->s_data->next_tb[TU_TB_INDEX_TARGET] = NULL;
        }
    }
    /* Free static tb. */
    int last_tb_id = 0;
    for (int i = 0; i < *tb_num_in_tu; i++) {
        tb = tb_list[i];
        assert(tb);
        if (tb->s_data->tu_tb_mode == TU_TB_MODE_STATIC) {
            free(tb->s_data);
            free(tb);
            tb = NULL;
        } else {
            tb_list[last_tb_id++] = tb;
        }
    }
    *tb_num_in_tu = last_tb_id;
}

static void ts_tb_explore(CPUState *cpu, target_ulong cs_base,
		uint32_t flags, int cflags)
{
    TranslationBlock** tb_list = tu_data->tb_list;
    uint32_t *tb_num_in_tu = &tu_data->tb_num;
    uint32_t *ir1_num_in_tu = &tu_data->ir1_num_in_tu;
    int max_insns;

    max_insns = cflags & CF_COUNT_MASK;
    if (max_insns == 0) {
        max_insns = CF_COUNT_MASK;
    }
    if (max_insns > TCG_MAX_INSNS) {
        max_insns = TCG_MAX_INSNS;
    }
    if (cpu->singlestep_enabled || singlestep) {
        max_insns = 1;
    }
    *tb_num_in_tu = 0;
    *ir1_num_in_tu = 0;
    while(untr_tb_id <= curr_seg->last_tb_id &&
        curr_tb_message_vector[untr_tb_id].tb) {
	untr_tb_id++;
    }
    if (untr_tb_id > curr_seg->last_tb_id) {
	return;
    }
    target_ulong curr_page = get_page(curr_tb_message_vector[untr_tb_id].pc);
    cflags = curr_tb_message_vector[untr_tb_id].cflags;
    cpu->tcg_cflags = cflags;

    get_ts_queue(cpu, cs_base, flags, cflags, max_insns, curr_page);
    if (*tb_num_in_tu == 0) {
        return;
    }

#ifdef CONFIG_LATX_HBR
    hbr_opt(tb_list, *tb_num_in_tu);
#endif
    tu_ir1_optimization(tb_list, *tb_num_in_tu);
    delet_static_tb(tb_list, tb_num_in_tu);
    qsort(tb_list, *tb_num_in_tu, sizeof(TranslationBlock *), tb_sort_cmp);
    solve_tb_overlap(*tb_num_in_tu, tb_list, max_insns);

    for (int i = 0; i < *tb_num_in_tu; i++) {
        tb_list[i]->s_data->tu_id = tb_list[0]->pc;
        assert((tb_list[i]->cflags & CF_PARALLEL) == (cflags & CF_PARALLEL));
        assert(get_page(tb_list[i]->pc) == get_page(tb_list[0]->pc));
    }
}

static void save_tu_to_ts(void) {
    TranslationBlock** tb_list = tu_data->tb_list;
    for (int i = 0; i < tu_data->tb_num; i++){
	ts_push_back(tb_list[i]);
    }
}

static inline gint tmp_message_sort_cmp(const void *ap, const void *bp)
{
    const tb_tmp_message a = *(const tb_tmp_message *)ap;
    const tb_tmp_message b = *(const tb_tmp_message *)bp;
    uint32_t pa_cflags = (a.cflags & CF_PARALLEL);
    uint32_t pb_cflags = (b.cflags & CF_PARALLEL);
    if (pa_cflags > pb_cflags) {
    	return 1;
    } else if (pa_cflags < pb_cflags) {
    	return -1;
    }
    return a.pc < b.pc ? -1 : a.pc > b.pc;
}

static inline bool need_flush(void)
{
    if (unlikely((tcg_ctx->code_gen_ptr + MAX_TU_SIZE >= tcg_ctx->code_gen_highwater)
                || (tcg_ctx->tb_gen_ptr + MAX_TB_IN_CACHE * sizeof(TranslationBlock)
                    >= tcg_ctx->tb_gen_highwater))) {
        qemu_log_mask(LAT_LOG_AOT, "WARNING need flush in_pre_translate\n");
        return true;
    }
    return false;
}

static inline void gen_tu(CPUState *cpu,
        target_ulong cs_base, uint32_t flags, int cflags)
{
    ts_tb_explore(cpu, cs_base, flags, cflags);
    if (!tu_data->tb_num) {
        return;
    }
    translate_tu(tu_data->tb_num, tu_data->tb_list);
    save_tu_to_ts();
}

static void translate_by_tu(CPUState *cpu,
        target_ulong cs_base, uint32_t flags, int cflags)
{
    untr_tb_id = curr_seg->first_tb_id;
#ifdef CONFIG_LATX_DEBUG
    for (int i = curr_seg->first_tb_id; i < curr_seg->last_tb_id; i++) {
	assert(curr_tb_message_vector[i].pc >= curr_seg->seg_begin
			&& curr_tb_message_vector[i].pc <= curr_seg->seg_end);
    }
#endif
    int tb_num_in_seg = curr_seg->last_tb_id - curr_seg->first_tb_id + 1;
    /* sort with cflags and pc in seg*/
    qsort(curr_tb_message_vector + curr_seg->first_tb_id, tb_num_in_seg,
            sizeof(tb_tmp_message), tmp_message_sort_cmp);
    mmap_trylock();
    while (untr_tb_id <= curr_seg->last_tb_id) {
	if (need_flush()) {
    	    break;
    	}
        gen_tu(cpu, cs_base, flags, cflags);
    }
    mmap_unlock();
}
#endif

static void translate_seg(seg_info *seg, CPUState *cpu,
        target_ulong cs_base, uint32_t flags, int cflags)
{
    for (int i = curr_seg->first_tb_id; i <= curr_seg->last_tb_id; i++) {
        curr_tb_message_vector[i].tb = NULL;
    }
#ifndef CONFIG_LATX_TU
    translate_by_tb(seg, cpu, cs_base, flags, cflags);
#else
    translate_by_tu(cpu, cs_base, flags, cflags);
#endif
    for (int i = curr_seg->first_tb_id; i < curr_seg->last_tb_id; i++) {
        TranslationBlock *tb = curr_tb_message_vector[i].tb;
        if (tb != NULL) {
            jrra_pre_translate((void **)&tb, 1, cpu, tb->flags, tb->cflags);
        }
    }
}

uint64 translate_lib(seg_info **seg_info_vector, int begin_id,
        int end_id, CPUState *cpu, tb_tmp_message *tb_message_vector)
{
    int cflags;
    uint32_t flags;
    target_ulong cs_base;
    translate_init(cpu, &cs_base, &flags, &cflags);
    if (tb_message_vector) {
    	curr_tb_message_vector = tb_message_vector;
    } else {
    	curr_tb_message_vector = dynamic_tb_message_vector;
    }
    assert(in_pre_translate == 0);
    in_pre_translate = 1;
    for(int i = begin_id; i < end_id; i++) {
        curr_seg = seg_info_vector[i];
        if (curr_seg->first_tb_id == -1) {
            continue;
        }
        translate_seg(curr_seg, cpu, cs_base, flags, cflags);
    }
    in_pre_translate = 0;
    return tb_num_in_ts;
}

