#define _GNU_SOURCE

/*
 * cfg_dump entry point.
 *
 * Complex binary analysis lives in the modules included below. This file keeps
 * only command-line parsing, top-level orchestration, summary printing, and
 * ownership cleanup.
 */

#include <elf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_ownership.h"
#include "capstone_shadow.h"
#include "cfg_check.h"
#include "cfg_graph.h"
#include "common.h"
#include "elf_image.h"
#include "function_symbols.h"
#include "got_plt.h"

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [--shadow-capstone] [binary] [function-name-substring]\n"
            "\n"
            "Default binary: ./crafty_base.Of.gcc830.dyn\n"
            "Prints ELF symbol functions and a basic-block CFG built from x86-64 bytes.\n",
            argv0);
}

static void print_summary(const char *path, const ElfFile *elf,
                          const FuncVec *funcs, const char *func_source,
                          const GotPltSymVec *got,
                          const GotPltEntryVec *plt,
                          const char *filter,
                          bool shadow_capstone)
{
    size_t got_external = gotplt_count_kind(got, GOTPLT_SYM_EXTERNAL);
    size_t got_ifunc = gotplt_count_kind(got, GOTPLT_SYM_IRELATIVE);

    printf("summary\n");
    printf("  binary=%s\n", path);
    printf("  elf_type=%s\n", elf_type_name(elf->eh->e_type));
    printf("  linking=%s\n", elf_linking_kind(elf));
    printf("  function_source=%s\n", func_source);
    printf("  functions=%zu\n", funcs->n);
    printf("  got_symbols=%zu\n", got->n);
    printf("  got_external=%zu\n", got_external);
    printf("  got_ifunc_irelative=%zu\n", got_ifunc);
    printf("  plt_entries=%zu\n", plt->n);
    printf("  call_modes=direct");
    if (plt->n && got_ifunc) {
        printf(", plt-ifunc");
    }
    if (got_external) {
        printf(", got-external");
    }
    printf(", jump-table, typed-unresolved-indirect\n");
    printf("  cfg_check=enabled\n");
    if (shadow_capstone) {
        printf("  shadow_capstone=enabled\n");
    }
    if (filter) {
        printf("  filter=%s\n", filter);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    bool shadow_capstone = false;
    int argi = 1;
    while (argi < argc && argv[argi][0] == '-') {
        if (strcmp(argv[argi], "-h") == 0 ||
            strcmp(argv[argi], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[argi], "--shadow-capstone") == 0) {
            shadow_capstone = true;
            argi++;
            continue;
        }
        usage(argv[0]);
        return 1;
    }

    if (argc - argi > 2) {
        usage(argv[0]);
        return 1;
    }

    const char *path = argc - argi >= 1 ? argv[argi] : "crafty_base.Of.gcc830.dyn";
    const char *filter = argc - argi >= 2 ? argv[argi + 1] : NULL;

    ElfFile elf = {0};
    FuncVec funcs = {0};
    GotPltSymVec got = {0};
    GotPltEntryVec plt = {0};
    CfgCheckSummary check_summary;
    CfgCheckSummary app_check_summary;
    CfgGraphSummary graph_summary;
    CfgGraphSummary app_graph_summary;
    CapstoneShadowSummary shadow_summary;
    AppOwnership app_owner;

    cfg_check_summary_init(&check_summary);
    cfg_check_summary_init(&app_check_summary);
    cfg_graph_summary_init(&graph_summary);
    cfg_graph_summary_init(&app_graph_summary);
    capstone_shadow_summary_init(&shadow_summary);
    if (shadow_capstone && capstone_shadow_init() != 0) {
        die("failed to initialize capstone");
    }

    /* Load global metadata first; all later modules borrow this file buffer. */
    elf_load(path, &elf);
    GotPltElf got_elf = elf_gotplt_view(&elf);
    gotplt_load_symbols(&got_elf, &got);
    gotplt_load_plt_entries(&got_elf, &plt);

    /* Function discovery chooses symbol tables or .eh_frame internally. */
    const char *func_source = NULL;
    funcs_load_all(&elf, &funcs, &func_source);
    app_ownership_detect(&funcs, &app_owner);

    /* Allocated sections are needed for jump-table candidate reads. */
    size_t section_count = 0;
    IjmpSection *sections = elf_build_ijmp_sections(&elf, &section_count);

    print_summary(path, &elf, &funcs, func_source, &got, &plt, filter,
                  shadow_capstone);
    app_ownership_print_filter_summary(stdout, &app_owner);

    for (size_t i = 0; i < funcs.n; i++) {
        uint64_t off;
        if (!elf_function_file_offset(&elf, funcs.v[i].addr, funcs.v[i].size,
                                      funcs.v[i].shndx, &off)) {
            fprintf(stderr, "skip %s: cannot map function bytes\n",
                    funcs.v[i].name);
            continue;
        }
        CfgSummarySink sinks[2] = {
            {
                .check = &check_summary,
                .graph = &graph_summary,
            },
        };
        size_t sink_count = 1;
        if (app_ownership_contains(&app_owner, &funcs.v[i])) {
            sinks[sink_count++] = (CfgSummarySink){
                .check = &app_check_summary,
                .graph = &app_graph_summary,
            };
        }

        /* cfg_print_function owns decoding, CFG construction, and checks. */
        cfg_print_function(elf.data, elf.size, off, &funcs.v[i], &funcs,
                           sections,
                           section_count, &got, &plt, filter,
                           sinks, sink_count, shadow_capstone,
                           &shadow_summary);
    }

    cfg_check_print_summary(stdout, &check_summary);
    cfg_graph_print_summary(stdout, "cfg_transfer_summary", &graph_summary);
    app_ownership_print_result_summary(stdout, &app_owner,
                                       &app_check_summary,
                                       &app_graph_summary);
    if (shadow_capstone) {
        capstone_shadow_print_summary(stdout, &shadow_summary);
        capstone_shadow_shutdown();
    }

    free(sections);
    free(plt.v);
    gotplt_free_symbols(&got);
    funcs_free(&funcs);
    elf_free(&elf);
    return 0;
}
