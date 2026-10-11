#include "cfg_decoder.h"
#include "cfg_program.h"
#include "ijmp_resolve.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_decoder(void)
{
    const unsigned char direct_call[] = {0xe8, 1, 0, 0, 0};
    Insn in = cfg_decode_insn(direct_call, sizeof(direct_call), 0x1000, 0);
    assert(in.kind == INSN_CALL && in.len == 5 && in.target == 0x1006);

    const unsigned char indirect_call[] = {0xff, 0xd0};
    in = cfg_decode_insn(indirect_call, sizeof(indirect_call), 0x2000, 0);
    assert(in.kind == INSN_ICALL && in.len == 2);

    const unsigned char syscall[] = {0x0f, 0x05};
    in = cfg_decode_insn(syscall, sizeof(syscall), 0x3000, 0);
    assert(in.kind == INSN_SYSCALL && in.len == 2);

    const unsigned char loop[] = {0xe2, 0xfe};
    in = cfg_decode_insn(loop, sizeof(loop), 0x4000, 0);
    assert(in.kind == INSN_JCC && in.target == 0x4000);
}

static void test_absolute_jump_table(void)
{
    /* jmp *0x2000(,%r8,8), followed by two case instruction boundaries. */
    unsigned char code[] = {
        0x42, 0xff, 0x24, 0xc5, 0x00, 0x20, 0x00, 0x00,
        0xb8, 0x01, 0x00, 0x00, 0x00, 0xc3,
    };
    unsigned char table[] = {
        0x08, 0x10, 0, 0, 0, 0, 0, 0,
        0x0d, 0x10, 0, 0, 0, 0, 0, 0,
    };
    uint64_t insns[] = {0x1000, 0x1008, 0x100d};
    IjmpSection section = { .addr = 0x2000, .size = sizeof(table) };
    IjmpContext ctx = {
        .file = table, .file_size = sizeof(table),
        .sections = &section, .section_count = 1,
        .insn_addrs = insns, .insn_count = 3,
        .func_addr = 0x1000, .func_size = sizeof(code),
    };
    IjmpResult result;
    assert(ijmp_resolve_jump_table(&ctx, code, sizeof(code), 0, &result));
    assert(result.table_addr == 0x2000 && result.count == 2);
    assert(result.targets[0] == 0x1008 && result.targets[1] == 0x100d);
    for (size_t size = 0; size < 8; size++) {
        assert(!ijmp_resolve_jump_table(&ctx, code, size, 0, &result));
    }
    table[0] = 0x09; /* Inside the first case's immediate, not an instruction. */
    assert(!ijmp_resolve_jump_table(&ctx, code, sizeof(code), 0, &result));
    table[0] = 0x08;
    section.size = 7;
    assert(!ijmp_resolve_jump_table(&ctx, code, sizeof(code), 0, &result));
    section.size = sizeof(table);
    ctx.file_size = 7;
    assert(!ijmp_resolve_jump_table(&ctx, code, sizeof(code), 0, &result));
    ctx.file_size = sizeof(table);
    code[0] = 0x67; /* Address-size and FS/GS bases need different semantics. */
    assert(!ijmp_resolve_jump_table(&ctx, code, sizeof(code), 0, &result));
    code[0] = 0x64;
    assert(!ijmp_resolve_jump_table(&ctx, code, sizeof(code), 0, &result));
    code[0] = 0x65;
    assert(!ijmp_resolve_jump_table(&ctx, code, sizeof(code), 0, &result));
    code[0] = 0x42;
    code[7] = 0x80;
    section.addr = UINT64_C(0xffffffff80002000);
    assert(ijmp_resolve_jump_table(&ctx, code, sizeof(code), 0, &result));
    assert(result.table_addr == section.addr && result.count == 2);
}

static void test_separate_base_jump_table(void)
{
    /* The table at 0x2000 contains offsets relative to 0x1020. */
    unsigned char code[] = {
        0x48, 0x8d, 0x35, 0x19, 0, 0, 0,
        0x48, 0x8d, 0x05, 0xf2, 0x0f, 0, 0,
        0x48, 0x63, 0x04, 0x90,
        0x48, 0x01, 0xf0,
        0xff, 0xe0,
        0xb8, 1, 0, 0, 0, 0xc3,
    };
    unsigned char table[] = {0xf7, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff};
    uint64_t insns[] = {0x1000, 0x1007, 0x100e, 0x1012, 0x1015, 0x1017, 0x101c};
    IjmpSection section = { .addr = 0x2000, .size = sizeof(table) };
    IjmpContext ctx = {
        .file = table, .file_size = sizeof(table),
        .sections = &section, .section_count = 1,
        .insn_addrs = insns, .insn_count = 7,
        .func_addr = 0x1000, .func_size = sizeof(code),
    };
    IjmpResult result;
    assert(ijmp_resolve_jump_table(&ctx, code, sizeof(code), 21, &result));
    assert(result.table_addr == 0x2000 && result.count == 2);
    assert(result.targets[0] == 0x1017 && result.targets[1] == 0x101c);
    code[2] = 0x3d; /* No known address for the offset base register RSI. */
    assert(!ijmp_resolve_jump_table(&ctx, code, sizeof(code), 21, &result));
    code[2] = 0x35;
    table[0] = 0xf8; /* Not a decoded case boundary. */
    assert(!ijmp_resolve_jump_table(&ctx, code, sizeof(code), 21, &result));
}

static void test_absolute_table_register_load(void)
{
    /* An unrelated add must not hide the adjacent mov/jmp table dispatch. */
    unsigned char code[] = {
        0x48, 0x01, 0xd8,
        0x48, 0x8b, 0x04, 0xc5, 0, 0x20, 0, 0,
        0xff, 0xe0,
        0xb8, 1, 0, 0, 0, 0xc3,
    };
    unsigned char table[] = {
        0x0d, 0x10, 0, 0, 0, 0, 0, 0,
        0x12, 0x10, 0, 0, 0, 0, 0, 0,
    };
    uint64_t insns[] = {0x1000, 0x1003, 0x100b, 0x100d, 0x1012};
    IjmpSection section = { .addr = 0x2000, .size = sizeof(table) };
    IjmpContext ctx = {
        .file = table, .file_size = sizeof(table),
        .sections = &section, .section_count = 1,
        .insn_addrs = insns, .insn_count = 5,
        .func_addr = 0x1000, .func_size = sizeof(code),
    };
    IjmpResult result;
    assert(ijmp_resolve_jump_table(&ctx, code, sizeof(code), 11, &result));
    assert(result.table_addr == 0x2000 && result.count == 2);
    assert(result.targets[0] == 0x100d && result.targets[1] == 0x1012);
    code[3] = 0x40; /* A 32-bit load is not a qword table. */
    assert(!ijmp_resolve_jump_table(&ctx, code, sizeof(code), 11, &result));
    code[3] = 0x64;
    assert(!ijmp_resolve_jump_table(&ctx, code, sizeof(code), 11, &result));

    /* mov table -> RAX; xor EAX,EAX; jmp RAX: the table value is dead. */
    unsigned char clobbered[] = {
        0x48, 0x8b, 0x04, 0xc5, 0, 0x20, 0, 0,
        0x31, 0xc0, 0xff, 0xe0, 0x90, 0xc3,
    };
    uint64_t clobbered_insns[] = {0x1000, 0x1008, 0x100a, 0x100c, 0x100d};
    ctx.insn_addrs = clobbered_insns;
    ctx.func_size = sizeof(clobbered);
    assert(!ijmp_resolve_jump_table(&ctx, clobbered, sizeof(clobbered), 10,
                                     &result));
}

static void test_program(const char *path)
{
    CfgProgram p;
    CfgAnalyzeOptions options = { .resolve_jump_tables = true };
    char error[256] = {0};
    assert(cfg_analyze_elf(path, &options, &p, error, sizeof(error)) == 0);
    const CfgProgramFunction *start = NULL;
    const CfgProgramFunction *jump_source = NULL;
    const CfgProgramFunction *jump_container = NULL;
    const CfgProgramFunction *jump_table_source = NULL;
    const CfgProgramFunction *jump_plt_source = NULL;
    const CfgProgramFunction *computed_stride_source = NULL;
    const CfgProgramFunction *plt = NULL;
    const CfgProgramFunction *fallthrough_check = NULL;
    const CfgProgramFunction *fallthrough_next = NULL;
    const CfgProgramFunction *zero_sized = NULL;
    const CfgProgramFunction *init_array = NULL;
    const CfgProgramFunction *array_direct_target = NULL;
    for (size_t i = 0; i < p.function_count; i++) {
        if (strcmp(p.functions[i].name, "_start") == 0) start = &p.functions[i];
        if (strcmp(p.functions[i].name, "jump_source") == 0)
            jump_source = &p.functions[i];
        if (strcmp(p.functions[i].name, "jump_container") == 0)
            jump_container = &p.functions[i];
        if (strcmp(p.functions[i].name, "jump_table_source") == 0)
            jump_table_source = &p.functions[i];
        if (strcmp(p.functions[i].name, "jump_plt_source") == 0)
            jump_plt_source = &p.functions[i];
        if (strcmp(p.functions[i].name, "computed_stride_source") == 0)
            computed_stride_source = &p.functions[i];
        if (strcmp(p.functions[i].name, "_plt") == 0)
            plt = &p.functions[i];
        if (strcmp(p.functions[i].name, "fallthrough_check") == 0)
            fallthrough_check = &p.functions[i];
        if (strcmp(p.functions[i].name, "fallthrough_next") == 0)
            fallthrough_next = &p.functions[i];
        if (strcmp(p.functions[i].name, "zero_sized") == 0)
            zero_sized = &p.functions[i];
        if (strncmp(p.functions[i].name, "init_array_", 11) == 0)
            init_array = &p.functions[i];
        if (strncmp(p.functions[i].name, "direct_target_", 14) == 0)
            array_direct_target = &p.functions[i];
    }
    assert(start && start->tb_count >= 4);
    int call = 0, jcc = 0, icall = 0, syscall = 0;
    for (size_t i = start->first_tb; i < start->first_tb + start->tb_count; i++) {
        call += p.tbs[i].terminator == CFG_TB_CALL;
        jcc += p.tbs[i].terminator == CFG_TB_CONDITIONAL;
        icall += p.tbs[i].terminator == CFG_TB_INDIRECT_CALL;
        syscall += p.tbs[i].terminator == CFG_TB_SYSCALL;
    }
    assert(call == 1 && jcc == 1 && icall == 1 && syscall == 1);
    assert(start->status == CFG_FUNCTION_OPEN);
    assert(jump_source && jump_source->tb_count == 1 && jump_container);
    const CfgTb *source_tb = &p.tbs[jump_source->first_tb];
    assert(source_tb->terminator == CFG_TB_JUMP && source_tb->edge_count == 1);
    uint64_t cross_target = p.edges[source_tb->first_edge].to;
    assert(cross_target > jump_container->start &&
           cross_target < jump_container->start + jump_container->size);
    int target_is_leader = 0;
    for (size_t i = jump_container->first_tb;
         i < jump_container->first_tb + jump_container->tb_count; i++) {
        target_is_leader |= p.tbs[i].start == cross_target;
    }
    assert(target_is_leader);

    assert(jump_table_source);
    int case_edges = 0;
    int local_case_edges = 0;
    for (size_t i = jump_table_source->first_tb;
         i < jump_table_source->first_tb + jump_table_source->tb_count; i++) {
        const CfgTb *tb = &p.tbs[i];
        for (size_t j = tb->first_edge; j < tb->first_edge + tb->edge_count;
             j++) {
            const CfgProgramEdge *edge = &p.edges[j];
            if (edge->kind != CFG_EDGE_CASE) continue;
            case_edges++;
            if (edge->to > jump_table_source->start &&
                edge->to < jump_table_source->start + jump_table_source->size) {
                local_case_edges++;
            }
        }
    }
    assert(case_edges == 8 && local_case_edges == 5);

    assert(computed_stride_source);
    case_edges = 0;
    for (size_t i = computed_stride_source->first_tb;
         i < computed_stride_source->first_tb + computed_stride_source->tb_count;
         i++) {
        const CfgTb *tb = &p.tbs[i];
        for (size_t j = tb->first_edge; j < tb->first_edge + tb->edge_count;
             j++) {
            case_edges += p.edges[j].kind == CFG_EDGE_CASE;
        }
    }
    assert(case_edges == 4);

    assert(jump_plt_source && jump_plt_source->tb_count == 1 && plt);
    source_tb = &p.tbs[jump_plt_source->first_tb];
    assert(source_tb->terminator == CFG_TB_JUMP && source_tb->edge_count == 1);
    cross_target = p.edges[source_tb->first_edge].to;
    assert(cross_target > plt->start && cross_target < plt->start + plt->size);
    target_is_leader = 0;
    for (size_t i = plt->first_tb; i < plt->first_tb + plt->tb_count; i++) {
        target_is_leader |= p.tbs[i].start == cross_target;
    }
    assert(target_is_leader);

    assert(fallthrough_check && fallthrough_next);
    int found_cross_symbol_fallthrough = 0;
    for (size_t i = fallthrough_check->first_tb;
         i < fallthrough_check->first_tb + fallthrough_check->tb_count; i++) {
        const CfgTb *tb = &p.tbs[i];
        if (tb->terminator != CFG_TB_CONDITIONAL) continue;
        for (size_t j = tb->first_edge; j < tb->first_edge + tb->edge_count;
             j++) {
            const CfgProgramEdge *edge = &p.edges[j];
            if (edge->kind == CFG_EDGE_FALSE &&
                edge->to < fallthrough_next->start) {
                found_cross_symbol_fallthrough = 1;
            }
        }
    }
    assert(found_cross_symbol_fallthrough);
    assert(zero_sized && zero_sized->size > 0 && zero_sized->tb_count == 1);
    assert(init_array && init_array->size > 0 && init_array->tb_count == 1);
    assert(array_direct_target && array_direct_target->size > 0 &&
           array_direct_target->tb_count == 1);
    cfg_program_destroy(&p);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    test_decoder();
    test_absolute_jump_table();
    test_separate_base_jump_table();
    test_absolute_table_register_load();
    test_program(argv[1]);
    puts("test-cfg: PASS");
    return 0;
}
