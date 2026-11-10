/* SPDX-License-Identifier: GPL-2.0-only */
#include "qemu-def.h"

#ifdef CONFIG_LATX_TU
/* Exercise production TB decoding and TU member traversal. */
#include "../../optimization/ts.c"

__thread CPUState *thread_cpu;
int qemu_loglevel;
int option_aot = 3;
int option_tu = 1;
int dt_debug_enable;
__thread TCGContext *tcg_ctx;
__thread TUControl *tu_data;

/* TB creation is stubbed; member traversal uses the production ts.c code. */
static tb_tmp_message queue_messages[8];
static TranslationBlock queue_blocks[8];
static struct separated_data queue_data[8];
enum { QUEUE_TB_SIZE = 0x10 };
static unsigned int create_count[8];
static uint16_t queue_sizes[8];
static uint16_t queue_icounts[8];
static target_ulong rejected_pc;

void page_protect(tb_page_addr_t page_addr G_GNUC_UNUSED)
{
}

uint32_t cpu_lduw_code(CPUArchState *env G_GNUC_UNUSED,
                       abi_ptr addr)
{
    uint16_t value;

    return cpu_memory_rw_debug(NULL, addr, &value, sizeof(value), false) ?
           0 : value;
}

void latx_fast_jmp_cache_add(CPUState *cs G_GNUC_UNUSED, int h G_GNUC_UNUSED,
                             TranslationBlock *tb G_GNUC_UNUSED)
{
}

void target_disasm(TranslationBlock *tb G_GNUC_UNUSED,
                   int max_insns G_GNUC_UNUSED)
{
}

TranslationBlock *tb_create(CPUState *cpu G_GNUC_UNUSED,
        target_ulong pc, target_ulong cs_base G_GNUC_UNUSED,
        uint32_t flags G_GNUC_UNUSED, int cflags,
        int max_insns G_GNUC_UNUSED, uint16_t bool_flags,
        TU_TB_START_TYPE mode G_GNUC_UNUSED)
{
    int id = get_tb_id(pc, cflags);
    TranslationBlock *tb;

    g_assert(id >= 0 && (unsigned int)id < G_N_ELEMENTS(queue_blocks));
    create_count[id]++;
    if (!tu_page_range_available(pc, QUEUE_TB_SIZE)) {
        return NULL;
    }
    tb = &queue_blocks[id];
    *tb = (TranslationBlock) {
        .pc = pc,
        .size = queue_sizes[id],
        .icount = queue_icounts[id],
        .flags = flags,
        .cflags = cflags,
        .bool_flags = bool_flags,
        .s_data = &queue_data[id],
    };
    tu_data->ir1_num_in_tu += tb->icount;
    if (!aot_cross_page_tu_enabled()) {
        aot_tb_insert(tb);
    }
    return tb;
}

TranslationBlock *tu_create_fallthrough_prefix(CPUState *cpu, target_ulong pc,
        target_ulong cs_base, int max_insns, uint16_t bool_flags,
        const TranslationBlock *tail)
{
    uint32_t ir1_start = tu_data->ir1_num_in_tu;
    TranslationBlock *tb = tb_create(cpu, pc, cs_base, tail->flags,
        tail->cflags, max_insns, bool_flags, TU_TB_START_NORMAL);

    if (tb && tu_shares_fallthrough_tail(tb, tail, max_insns)) {
        return tb;
    }
    tu_data->ir1_num_in_tu = ir1_start;
    return NULL;
}

bool tu_page_range_available(target_ulong pc,
                             uint32_t size G_GNUC_UNUSED)
{
    return pc != rejected_pc;
}

bool tu_push_back(TranslationBlock *tb)
{
    if (!tb || !tu_page_range_available(tb->pc, tb->size) ||
            tu_data->tb_num >= MAX_TB_IN_CACHE) {
        return false;
    }
    g_assert(tu_member_lookup(tb->pc) == NULL);
    tu_data->tb_list[tu_data->tb_num++] = tb;
    if (aot_cross_page_tu_enabled() && in_pre_translate) {
        aot_tb_insert(tb);
    }
    return true;
}

void tu_reset_tb(TranslationBlock *tb G_GNUC_UNUSED)
{
}

TranslationBlock *tu_member_lookup(target_ulong pc)
{
    for (unsigned int i = 0; i < tu_data->tb_num; i++) {
        if (tu_data->tb_list[i]->pc == pc) {
            return tu_data->tb_list[i];
        }
    }
    return NULL;
}

int qemu_log(const char *fmt G_GNUC_UNUSED, ...)
{
    return 0;
}

void print_stack_trace(void)
{
    abort();
}

static const uint8_t guest_code[] = {
    0x90, 0x75, 0xfd,             /* nop; jne start */
    0xeb, 0xfb,                   /* jmp start */
    0xe8, 0xf6, 0xff, 0xff, 0xff, /* call start */
    0xff, 0xd0,                   /* call *eax/rax */
    0xff, 0xe0,                   /* jmp *eax/rax */
    0xc3,                         /* ret */
    0x90,                         /* nop */
    0xe9,                         /* incomplete jmp */
};
static bool unreadable;

int cpu_memory_rw_debug(CPUState *cpu G_GNUC_UNUSED, target_ulong addr,
                        void *buf, target_ulong len, bool write)
{
    g_assert(!write);
    if (unreadable || addr < 0x1000 ||
        addr - 0x1000 + len > sizeof(guest_code)) {
        return -1;
    }
    memcpy(buf, guest_code + addr - 0x1000, len);
    return 0;
}

static void reset_messages(TranslationBlock **list, TranslationBlock *blocks,
        unsigned int count, seg_info *seg, TranslationBlock *entry,
        unsigned int entry_count, const unsigned int *offsets,
        const unsigned int *sizes, bool code64)
{
    struct separated_data shared = {
        .next_pc = 0xdead,
        .target_pc = 0xbeef,
        .last_ir1_type = IR1_TYPE_NORMAL,
    };
    seg_info *segments[] = { seg };
    int segment_num = 1;

    seg->first_tb_id = -1;
    seg->last_tb_id = -2;
    *entry = (TranslationBlock) { .pc = 0x1000 };
    for (unsigned int i = 0; i < count; i++) {
        list[i] = &blocks[i];
        blocks[i] = (TranslationBlock) { 0 };
        blocks[i].pc = 0x1000 + offsets[i];
        blocks[i].size = sizes[i];
        blocks[i].s_data = &shared;
        blocks[i].bool_flags = code64 ? IS_CODE64 : 0;
    }
    get_dynamic_message(list, count, segments, &segment_num);
    g_assert(shared.next_pc == 0xdead);
    g_assert(shared.target_pc == 0xbeef);
}

static void check_messages(bool code64)
{
    const unsigned int offsets[] = { 0, 3, 5, 10, 12, 14, 15, 16 };
    const unsigned int sizes[] = { 3, 2, 5, 2, 2, 1, 1, 1 };
    const uint16_t fallthrough_offsets[] = { 3, 0, 5, 2, 0, 0, 1, 0 };
    TranslationBlock blocks[G_N_ELEMENTS(offsets)] = { 0 };
    TranslationBlock *list[G_N_ELEMENTS(offsets)];
    TranslationBlock entry;
    seg_info seg = { .seg_begin = 0x1000, .seg_end = 0x2000,
                    .first_tb_id = -1 };
    seg_info *segments[] = { &seg };
    int segment_num = 1;

    reset_messages(list, blocks, G_N_ELEMENTS(list), &seg, &entry, 1,
                   offsets, sizes, code64);
    curr_seg = &seg;
    curr_tb_message_vector = dynamic_tb_message_vector;

    for (unsigned int i = 0; i < G_N_ELEMENTS(list); i++) {
        g_assert(dynamic_tb_message_vector[i].fallthrough_offset ==
                 fallthrough_offsets[i]);
    }

    g_assert(ts_find_fallthrough_predecessor(0x1003, 0, 0x1003) ==
             &dynamic_tb_message_vector[0]);
    g_assert(ts_find_fallthrough_predecessor(0x100a, 0, 0x100a) ==
             &dynamic_tb_message_vector[2]);
    g_assert(ts_find_fallthrough_predecessor(0x100c, 0, 0x100c) ==
             &dynamic_tb_message_vector[3]);
    g_assert(ts_find_fallthrough_predecessor(0x1010, 0, 0x1010) ==
             &dynamic_tb_message_vector[6]);
    g_assert(ts_find_fallthrough_predecessor(0x1000, 0, 0x1000) == NULL);
    g_assert(ts_find_fallthrough_predecessor(0x1005, 0, 0x1005) == NULL);

    free(dynamic_tb_message_vector);

    blocks[2].lazypc[1] = 0;
    unreadable = true;
    seg.first_tb_id = -1;
    seg.last_tb_id = -2;
    segment_num = 1;
    get_dynamic_message(&list[2], 1, segments, &segment_num);
    g_assert(dynamic_tb_message_vector[0].fallthrough_offset == 0);
    free(dynamic_tb_message_vector);
    unreadable = false;

    /* Empty TBs remain ineligible even with a tagged continuation. */
    blocks[2].size = 0;
    segment_num = 1;
    get_dynamic_message(&list[2], 1, segments, &segment_num);
    g_assert(dynamic_tb_message_vector[0].fallthrough_offset == 0);
    free(dynamic_tb_message_vector);
}

static void check_fallthrough_offsets(target_ulong pc)
{
    g_assert(tb_fallthrough_offset(pc, pc) == 0);
    g_assert(tb_fallthrough_offset(pc, pc - 1) == 0);
    g_assert(tb_fallthrough_offset(pc, pc + UINT16_MAX) == UINT16_MAX);
    g_assert(tb_fallthrough_offset(pc, pc + UINT16_MAX + 1) == 0);
    /* The small low bits must not turn a distant edge into a nearby one. */
    g_assert(tb_fallthrough_offset(pc, pc + UINT16_MAX + 8) == 0);
    g_assert(tb_fallthrough_offset((target_ulong)-2, 1) == 0);
}

static void reset_member_queue(target_ulong base)
{
    static TUControl control;
    static seg_info seg;
    const uint8_t types[] = {
        IR1_TYPE_JUMP, IR1_TYPE_CALL, IR1_TYPE_BRANCH, IR1_TYPE_RET,
        IR1_TYPE_RET, IR1_TYPE_CALLIN, IR1_TYPE_NORMAL, IR1_TYPE_RET,
    };

    memset(&control, 0, sizeof(control));
    memset(create_count, 0, sizeof(create_count));
    for (unsigned int i = 0; i < G_N_ELEMENTS(queue_messages); i++) {
        queue_sizes[i] = QUEUE_TB_SIZE;
        queue_icounts[i] = 1;
    }
    for (unsigned int i = 0; i < G_N_ELEMENTS(queue_messages); i++) {
        queue_messages[i] = (tb_tmp_message) {
            .pc = base + QUEUE_TB_SIZE * i,
        };
        queue_data[i] = (struct separated_data) {
            .next_pc = queue_messages[i].pc + QUEUE_TB_SIZE,
            .last_ir1_type = types[i],
        };
    }
    /* Entry jumps over B/C to D; C branches to G, with predecessor F. */
    queue_data[0].target_pc = queue_messages[3].pc;
    queue_data[2].target_pc = queue_messages[6].pc;
    /* B's callee must not be pulled in by CALL continuation discovery. */
    queue_data[1].target_pc = queue_messages[4].pc;
    /* B, C, F, and G have sequential successors in the original graph. */
    queue_messages[1].fallthrough_offset = QUEUE_TB_SIZE;
    queue_messages[2].fallthrough_offset = QUEUE_TB_SIZE;
    queue_messages[5].fallthrough_offset = QUEUE_TB_SIZE;
    queue_messages[6].fallthrough_offset = QUEUE_TB_SIZE;
    seg = (seg_info) {
        .seg_begin = base,
        .seg_end = base + 0x100,
        .first_tb_id = 0,
        .last_tb_id = G_N_ELEMENTS(queue_messages) - 1,
    };
    curr_seg = &seg;
    curr_tb_message_vector = queue_messages;
    tu_data = &control;
    in_pre_translate = 1;
    untr_tb_id = 0;
    rejected_pc = 0;
    option_aot = 3;
}

static void check_member_queue(target_ulong base)
{
    reset_member_queue(base);
    get_ts_queue(NULL, 0, 0, 0, 1, get_page(base));
    g_assert(tu_data->tb_num == 7);
    for (unsigned int i = 0; i < G_N_ELEMENTS(queue_messages); i++) {
        if (i == 4) {
            g_assert(create_count[i] == 0);
            g_assert(queue_messages[i].tb == NULL);
        } else {
            g_assert(create_count[i] == 1);
            g_assert(tu_member_lookup(base + 0x10 * i) == &queue_blocks[i]);
        }
    }
    g_assert(queue_blocks[2].s_data->next_tb[TU_TB_INDEX_TARGET] ==
             &queue_blocks[6]);
    g_assert(queue_blocks[6].s_data->next_tb[TU_TB_INDEX_NEXT] ==
             &queue_blocks[7]);

    reset_member_queue(base);
    /* B belongs to another TU and must never be created or admitted here. */
    queue_messages[1].tb = &queue_blocks[1];
    get_ts_queue(NULL, 0, 0, 0, 1, get_page(base));
    g_assert(tu_data->tb_num == 6);
    g_assert(create_count[1] == 0);
    g_assert(tu_member_lookup(queue_messages[1].pc) == NULL);

    reset_member_queue(base);
    rejected_pc = queue_messages[2].pc;
    get_ts_queue(NULL, 0, 0, 0, 1, get_page(base));
    g_assert(tu_data->tb_num == 2);
    g_assert(queue_messages[2].tb == NULL);

    reset_member_queue(base);
    tu_data->ir1_num_in_tu = MAX_IR1_IN_TU - 2;
    get_ts_queue(NULL, 0, 0, 0, 1, get_page(base));
    g_assert(tu_data->tb_num == 1);
    g_assert(create_count[1] == 0);

    reset_member_queue(base);
    /* A temporary member without a message still needs predecessor lookup. */
    struct separated_data temporary_data = { .last_ir1_type = IR1_TYPE_RET };
    TranslationBlock temporary = {
        .pc = base + 0x80, .size = 1, .icount = 1, .s_data = &temporary_data,
    };
    queue_data[7].last_ir1_type = IR1_TYPE_NORMAL;
    queue_messages[7].fallthrough_offset = QUEUE_TB_SIZE;
    tu_data->tb_list[tu_data->tb_num++] = &temporary;
    get_ts_outgoing(NULL, 0, 0, 0, 1);
    g_assert(tu_data->tb_num == 4);
    g_assert(tu_member_lookup(queue_messages[7].pc) == &queue_blocks[7]);
    g_assert(tu_member_lookup(queue_messages[5].pc) == &queue_blocks[5]);

    reset_member_queue(base);
    option_aot = 2;
    get_ts_queue(NULL, 0, 0, 0, 1, get_page(base));
    g_assert(tu_data->tb_num == 2);
    g_assert(create_count[2] == 0);
    option_aot = 3;
}

static const uint8_t shared_call[] = {
    0x90, 0x90, 0x66, 0x90, 0x90,       /* nop; nop; nopw; nop */
    0xe8, 0x26, 0x00, 0x00, 0x00,       /* call base + 0x30 */
};

static void set_call_message(unsigned int id, target_ulong base,
                             unsigned int offset, bool code64)
{
    static struct la_dt_insn info[8][sizeof(shared_call)];
    static IR1_INST ir1[8][sizeof(shared_call)];
    unsigned int count = 0;
    target_ulong end = base + sizeof(shared_call);

    queue_messages[id].pc = base + offset;
    queue_messages[id].fallthrough_offset = sizeof(shared_call) - offset;
    queue_messages[id].bool_flags = code64 ? IS_CODE64 : 0;
    queue_data[id].last_ir1_type = IR1_TYPE_CALL;
    queue_data[id].next_pc = end;
    queue_data[id].target_pc = base + 0x30;
    queue_data[id].ir1 = ir1[id];
    queue_sizes[id] = sizeof(shared_call) - offset;
    while (offset < sizeof(shared_call)) {
        int n = la_disa_v1(shared_call + offset, sizeof(shared_call) - offset,
                           base + offset, 1, &ir1[id][count].info,
                           0, &info[id][count], code64);

        g_assert(n == 1 && ir1[id][count].info->size);
        offset += ir1[id][count++].info->size;
    }
    g_assert(offset == sizeof(shared_call));
    queue_icounts[id] = count;
}

static void check_shared_call_tail(target_ulong base, bool code64)
{
    enum { PREFIX, INTERIOR, TAIL, NEXT, ENTRY, CALLEE };
    enum {
        SHARED_TAIL, MID_INSTRUCTION, OWNED, INTERVENING, MODE_MISMATCH,
        CALLEE_IN_TU,
    };

    for (int scenario = SHARED_TAIL; scenario <= CALLEE_IN_TU; scenario++) {
        reset_member_queue(base);
        curr_seg->last_tb_id = CALLEE;
        untr_tb_id = ENTRY;
        queue_messages[INTERIOR].pc = base + 1;
        queue_data[INTERIOR].last_ir1_type = IR1_TYPE_RET;
        queue_messages[NEXT].pc = base + sizeof(shared_call);
        queue_messages[ENTRY].pc = base + 0x20;
        queue_data[ENTRY].last_ir1_type = IR1_TYPE_JUMP;
        queue_data[ENTRY].target_pc = queue_messages[NEXT].pc;
        queue_messages[CALLEE].pc = base + 0x30;
        queue_messages[CALLEE].fallthrough_offset = 0;
        queue_data[CALLEE].last_ir1_type = IR1_TYPE_RET;
        set_call_message(PREFIX, base, 0, code64);
        /* Offset 3 decodes as a NOP but is inside the prefix's NOPW. */
        set_call_message(TAIL, base, scenario == MID_INSTRUCTION ? 3 : 4,
                         code64);
        if (scenario == OWNED) {
            queue_messages[PREFIX].tb = &queue_blocks[PREFIX];
        } else if (scenario == INTERVENING) {
            TranslationBlock *tb = tb_create(NULL, base + 1, 0, 0, 0,
                                             16, 0, TU_TB_START_NORMAL);

            g_assert(tu_push_back(tb));
        } else if (scenario == MODE_MISMATCH) {
            queue_messages[PREFIX].bool_flags ^= IS_CODE64;
        } else if (scenario == CALLEE_IN_TU) {
            TranslationBlock *tb = tb_create(NULL, queue_messages[CALLEE].pc,
                0, 0, 0, 16, 0, TU_TB_START_NORMAL);

            g_assert(tb);
            g_assert(tu_push_back(tb));
        }

        get_ts_queue(NULL, 0, 0, 0, 16, get_page(base));
        g_assert(tu_member_lookup(queue_messages[TAIL].pc) ==
                 &queue_blocks[TAIL]);
        g_assert(tu_member_lookup(queue_messages[NEXT].pc) ==
                 &queue_blocks[NEXT]);
        g_assert(tu_member_lookup(queue_messages[PREFIX].pc) ==
                 (scenario == SHARED_TAIL ? &queue_blocks[PREFIX] : NULL));
        g_assert(create_count[PREFIX] == (scenario <= MID_INSTRUCTION));
        g_assert(create_count[CALLEE] == (scenario == CALLEE_IN_TU));
        g_assert(tu_data->ir1_num_in_tu == queue_icounts[TAIL] + 2u +
                 (scenario == SHARED_TAIL ? queue_icounts[PREFIX] : 0u) +
                 (scenario == INTERVENING || scenario == CALLEE_IN_TU));
    }
}

static void check_member_queue_capacity(target_ulong base, bool cross_page,
                                        bool have_target, bool have_next)
{
    TranslationBlock fillers[MAX_TB_IN_TS - 2] = { 0 };
    struct separated_data ret = { .last_ir1_type = IR1_TYPE_RET };
    unsigned int nr_fillers = G_N_ELEMENTS(fillers) - have_target - cross_page;

    reset_member_queue(base);
    option_aot = cross_page ? 3 : 2;
    if (have_target) {
        /* G is already present; keep it from discovering more members. */
        queue_data[5].last_ir1_type = IR1_TYPE_RET;
        queue_data[6].last_ir1_type = IR1_TYPE_RET;
        queue_messages[5].fallthrough_offset = 0;
        queue_messages[6].fallthrough_offset = 0;
        TranslationBlock *tb = tb_create(NULL, queue_messages[6].pc, 0, 0, 0,
                                         1, 0, TU_TB_START_NORMAL);
        bool added = tu_push_back(tb);

        g_assert(added);
        queue_data[3].last_ir1_type = IR1_TYPE_JUMP;
        queue_data[3].target_pc = queue_messages[6].pc;
    }
    if (have_next) {
        TranslationBlock *tb = tb_create(NULL, queue_messages[3].pc, 0, 0, 0,
                                         1, 0, TU_TB_START_NORMAL);
        bool added = tu_push_back(tb);

        g_assert(added);
    }
    for (unsigned int i = 0; i < nr_fillers; i++) {
        fillers[i] = (TranslationBlock) {
            .pc = base + 0x1000 + 0x10 * i,
            .icount = 1,
            .s_data = &ret,
        };
        tu_data->tb_list[tu_data->tb_num++] = &fillers[i];
    }

    /* Explore C: B is its predecessor, D its next TB, G its target. */
    untr_tb_id = 2;
    get_ts_queue(NULL, 0, 0, 0, 1, get_page(base));
    g_assert(tu_data->tb_num == MAX_TB_IN_TS);
    g_assert(queue_messages[1].tb == (cross_page ? &queue_blocks[1] : NULL));
    g_assert(queue_messages[3].tb == &queue_blocks[3]);
    g_assert(create_count[6] == have_target);
    g_assert(queue_blocks[2].s_data->next_tb[TU_TB_INDEX_NEXT] ==
             &queue_blocks[3]);
    g_assert(queue_blocks[2].s_data->next_tb[TU_TB_INDEX_TARGET] ==
             (have_target ? &queue_blocks[6] : NULL));
    if (have_target) {
        g_assert(queue_blocks[3].s_data->next_tb[TU_TB_INDEX_TARGET] ==
                 &queue_blocks[6]);
    }
}

int main(void)
{
    gitcapstone_init(64);
    option_aot = 3;
    check_messages(false);
    check_messages(true);
    check_fallthrough_offsets(0x1000);
    check_member_queue(0x1000);
    check_shared_call_tail(0x1000, false);
    check_member_queue_capacity(0x1000, true, false, false);
    check_member_queue_capacity(0x1000, true, true, false);
    check_member_queue_capacity(0x1000, true, true, true);
    check_member_queue_capacity(0x1000, false, false, false);
    check_member_queue_capacity(0x1000, false, true, false);
#if TARGET_ABI_BITS == 64
    check_fallthrough_offsets(UINT64_C(0x100000000) + 0x1000);
    check_member_queue(UINT64_C(0x100000000) + 0x1000);
    check_shared_call_tail(UINT64_C(0x100000000) + 0x1000, true);
#endif
    puts("PASS: full-TB fallthrough closure");
    return 0;
}
#else
int main(void)
{
    puts("SKIP: TU support is unavailable in this build");
    return 77;
}
#endif
