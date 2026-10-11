#include "cfg_program.h"
#include "bundle.h"
#include "tbset.h"
#include "native-image.h"
#include "module-pack.h"
#include "module-inspect.h"
#include "latc-build-id.h"

#include <errno.h>
#include <elf.h>
#include <glib.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void usage(const char *name)
{
    fprintf(stderr, "usage:\n"
            "  %s analyze [--json] X86_ELF\n"
            "  %s emit-static-tbset X86_ELF -o FILE\n"
            "  %s compile X86_ELF -o OUTPUT --runner RUNNER [--tbset FILE]"
            " [--tbset-ignore-outside-exec] [--aot FILE]\n"
            "  %s inspect [--json] BUNDLE\n", name, name, name, name);
    fprintf(stderr, "  %s inspect-native [--json] IMAGE\n", name);
    fprintf(stderr, "  %s mark-native-x86 IMAGE\n", name);
    fprintf(stderr, "  %s merge-native BASE DELTA -o OUTPUT\n", name);
    fprintf(stderr, "  %s emit-aot-v2 NATIVE_IMAGE OUTPUT_DIRECTORY"
                    " [X86_ELF [TBSET]]\n",
            name);
    fprintf(stderr,
            "  %s compile-module X86_ELF -o MODULE --runner RUNNER"
            " --runtime-dir DIRECTORY [--tbset FILE [--static-tbset]]\n",
            name);
    fprintf(stderr, "  %s inspect-module [--json] MODULE\n", name);
    fprintf(stderr, "  %s build-id\n", name);
}

static int inspect_native(const char *path, int json)
{
    LatNativeImageHeaderV2 header;
    char error[256] = {0};
    if (lat_native_image_inspect_file(path, &header, error, sizeof(error))) {
        fprintf(stderr, "latc: %s\n", error);
        return 1;
    }
    if (json) {
        printf("{\"image\":\"%s\",\"execution_model\":\"lat-native-image\""
               ",\"flags\":%u,\"guest_entry\":%" PRIu64
               ",\"preferred_guest_base\":%" PRIu64
               ",\"guest_size\":%" PRIu64
               ",\"code_size\":%" PRIu64 ",\"tbs\":%" PRIu64
               ",\"relocations\":%" PRIu64 ",\"pc_maps\":%" PRIu64
               ",\"lat_build_id\":\"%s\"}\n",
               path, header.flags, header.guest_entry,
               header.preferred_guest_base, header.guest_image_size,
               header.code_size, header.tb_count, header.relocation_count,
               header.pc_map_count, header.lat_build_id);
    } else {
        printf("image=%s\nexecution_model=lat-native-image\n"
               "flags=0x%x\nguest_entry=0x%" PRIx64
               "\npreferred_guest_base=0x%" PRIx64
               "\nguest_size=%" PRIu64
               "\ncode_size=%" PRIu64 "\ntbs=%" PRIu64
               "\nrelocations=%" PRIu64 "\npc_maps=%" PRIu64
               "\nlat_build_id=%s\n",
               path, header.flags, header.guest_entry,
               header.preferred_guest_base, header.guest_image_size,
               header.code_size, header.tb_count, header.relocation_count,
               header.pc_map_count, header.lat_build_id);
    }
    return 0;
}

static void digest_hex(const uint8_t digest[32], char hex[65])
{
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < 32; i++) {
        hex[i * 2] = digits[digest[i] >> 4];
        hex[i * 2 + 1] = digits[digest[i] & 15];
    }
    hex[64] = '\0';
}

static int file_digest(const char *path, uint8_t digest[32],
                       char *error, size_t error_size)
{
    gchar *contents = NULL;
    gsize size = 0;
    GError *gerror = NULL;
    if (!g_file_get_contents(path, &contents, &size, &gerror)) {
        g_snprintf(error, error_size, "cannot read %s: %s", path,
                   gerror ? gerror->message : "unknown error");
        g_clear_error(&gerror);
        return -1;
    }
    GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
    g_checksum_update(checksum, (const guchar *)contents, size);
    gsize digest_size = 32;
    g_checksum_get_digest(checksum, digest, &digest_size);
    g_checksum_free(checksum);
    g_free(contents);
    return digest_size == 32 ? 0 : -1;
}

static int inspect_module(const char *path, int json)
{
    LatAotModuleInfoV2 info;
    char error[256] = {0};
    if (lat_aot_v2_module_inspect_file(path, &info, error, sizeof(error))) {
        fprintf(stderr, "latc: %s\n", error);
        return 1;
    }
    char source[65];
    char codegen[65];
    digest_hex(info.note.source_sha256, source);
    digest_hex(info.note.codegen_id, codegen);
    int precise = !!(info.note.module_flags & LAT_AOT_MODULE_PRECISE_PC_MAP);
    int test_only = !!(info.note.module_flags & LAT_AOT_MODULE_M1_TEST_ONLY);
    if (json) {
        printf("{\"module\":\"%s\",\"execution_model\":\"lat-aot-v2\""
               ",\"abi_version\":%u,\"module_flags\":%" PRIu64
               ",\"required_features\":%" PRIu64
               ",\"text_size\":%" PRIu64 ",\"tbs\":%" PRIu64
               ",\"pc_maps\":%" PRIu64
               ",\"precise_pc_map\":%s,\"test_only\":%s"
               ",\"source_sha256\":\"%s\",\"codegen_id\":\"%s\"}\n",
               path, info.note.abi_version, info.note.module_flags,
               info.note.required_features, info.text_size, info.tb_count,
               info.pc_map_count, precise ? "true" : "false",
               test_only ? "true" : "false", source, codegen);
    } else {
        printf("module=%s\nexecution_model=lat-aot-v2\nabi_version=%u"
               "\nmodule_flags=0x%" PRIx64
               "\nrequired_features=0x%" PRIx64
               "\ntext_size=%" PRIu64 "\ntbs=%" PRIu64
               "\npc_maps=%" PRIu64 "\nprecise_pc_map=%s\ntest_only=%s"
               "\nsource_sha256=%s\ncodegen_id=%s\n",
               path, info.note.abi_version, info.note.module_flags,
               info.note.required_features, info.text_size, info.tb_count,
               info.pc_map_count, precise ? "true" : "false",
               test_only ? "true" : "false", source, codegen);
    }
    return 0;
}

static int compile_module(const char *input, const char *output,
                          const char *runner,
                          const char *runtime_dir, const char *tbset,
                          bool static_tbset)
{
    GError *gerror = NULL;
    gchar *executable = g_file_read_link("/proc/self/exe", &gerror);
    if (!executable) {
        fprintf(stderr, "latc: cannot locate compiler executable: %s\n",
                gerror ? gerror->message : "unknown error");
        g_clear_error(&gerror);
        return 1;
    }
#ifdef LATC_INSTALLED_SCRIPT_DIR
    gchar *script = g_build_filename(LATC_INSTALLED_SCRIPT_DIR,
                                     "compile-aot-v2-module.sh", NULL);
#else
    gchar *build_dir = g_path_get_dirname(executable);
    gchar *tool_dir = g_path_get_dirname(build_dir);
    gchar *script = g_build_filename(tool_dir, "scripts",
                                     "compile-aot-v2-module.sh", NULL);
#endif
    pid_t child = fork();
    if (child == 0) {
        if (tbset && static_tbset) {
            execl(script, script, "--static-tbset", executable, runner,
                  input, runtime_dir, output, tbset, (char *)NULL);
        } else if (tbset) {
            execl(script, script, executable, runner, input, runtime_dir, output,
                  tbset, (char *)NULL);
        } else {
            execl(script, script, executable, runner, input, runtime_dir, output,
                  (char *)NULL);
        }
        fprintf(stderr, "latc: cannot execute %s: %s\n", script,
                strerror(errno));
        _exit(127);
    }
    g_free(script);
#ifndef LATC_INSTALLED_SCRIPT_DIR
    g_free(tool_dir);
    g_free(build_dir);
#endif
    g_free(executable);
    if (child < 0) {
        fprintf(stderr, "latc: cannot start module compiler: %s\n",
                strerror(errno));
        return 1;
    }
    int status;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            fprintf(stderr, "latc: cannot wait for module compiler: %s\n",
                    strerror(errno));
            return 1;
        }
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

static int compile_bundle(const char *input, const char *output,
                          const char *runner, const char *tbset,
                          int tbset_ignore_outside_exec, const char *aot)
{
    CfgProgram program;
    uint8_t guest_digest[32];
    const uint8_t *known_guest_digest = NULL;
    char error[256] = {0};
    CfgAnalyzeOptions options = { .resolve_jump_tables = true };
    int analyze_result =
        cfg_analyze_elf(input, &options, &program, error, sizeof(error));
    if (analyze_result != 0) {
        fprintf(stderr, "latc: %s\n", error); return 1;
    }
    if (tbset) {
        size_t matched = 0, unmatched = 0, ignored = 0;
        if (latc_tbset_apply(tbset, input, &program,
                            tbset_ignore_outside_exec,
                            &matched, &unmatched, &ignored, guest_digest,
                            error, sizeof(error)) != 0) {
            fprintf(stderr, "latc: %s\n", error);
            cfg_program_destroy(&program);
            return 1;
        }
        fprintf(stderr, "latc: TB set matched=%zu added=%zu ignored=%zu\n",
                matched, unmatched, ignored);
        known_guest_digest = guest_digest;
    }
    int rc = latc_bundle_write(runner, input, output, aot, &program,
                               known_guest_digest,
                               error, sizeof(error));
    cfg_program_destroy(&program);
    if (rc) { fprintf(stderr, "latc: %s\n", error); return 1; }
    printf("output=%s\n", output);
    return 0;
}

static int inspect_bundle(const char *path, int json)
{
    LatcBundleInfo info;
    char error[256] = {0};
    if (latc_bundle_inspect(path, &info, 1, error, sizeof(error)) != 0) {
        fprintf(stderr, "latc: %s\n", error); return 1;
    }
    if (json) {
        printf("{\"bundle\":\"%s\",\"execution_model\":\"%s\""
               ",\"runner_size\":%" PRIu64
               ",\"guest_size\":%" PRIu64 ",\"cfg_size\":%" PRIu64
               ",\"functions\":%" PRIu64 ",\"tbs\":%" PRIu64
               ",\"edges\":%" PRIu64 ",\"selected_tbs\":%" PRIu64
               ",\"aot_size\":%" PRIu64 ",\"aot_name\":\"%s\""
               ",\"guest_sha256\":\"%s\"}\n",
               path, LATC_EXECUTION_MODEL, info.runner_size, info.guest_size,
               info.cfg_size,
               info.function_count, info.tb_count, info.edge_count,
               info.selected_tb_count,
               info.aot_size, info.aot_name,
               info.guest_sha256);
    } else {
        printf("bundle=%s\nexecution_model=%s\nrunner_size=%" PRIu64
               "\nguest_size=%" PRIu64
               "\ncfg_size=%" PRIu64 "\nfunctions=%" PRIu64
               "\ntbs=%" PRIu64 "\nedges=%" PRIu64 "\nselected_tbs=%" PRIu64
               "\naot_size=%" PRIu64 "\naot_name=%s"
               "\nguest_sha256=%s\n", path, LATC_EXECUTION_MODEL,
               info.runner_size,
               info.guest_size, info.cfg_size, info.function_count,
               info.tb_count, info.edge_count, info.selected_tb_count,
               info.aot_size, info.aot_name,
               info.guest_sha256);
    }
    return 0;
}

static int analyze(const char *path, int json)
{
    CfgProgram program;
    CfgAnalyzeOptions options = { .resolve_jump_tables = true };
    char error[256] = {0};
    if (cfg_analyze_elf(path, &options, &program, error, sizeof(error)) != 0) {
        fprintf(stderr, "latc: %s\n", error[0] ? error : "analysis failed");
        return 1;
    }
    if (json) {
        printf("{\"input\":\"%s\",\"functions\":%zu,\"tbs\":%zu,"
               "\"edges\":%zu,\"open_functions\":%zu,"
               "\"error_functions\":%zu,\"jump_tables\":%zu,"
               "\"jump_table_targets\":%zu}\n",
               path, program.function_count, program.tb_count,
               program.edge_count, program.open_function_count,
               program.error_function_count, program.resolved_jump_tables,
               program.resolved_jump_table_targets);
    } else {
        printf("input=%s\nfunctions=%zu\ntbs=%zu\nedges=%zu\n",
               path, program.function_count, program.tb_count,
               program.edge_count);
        printf("open_functions=%zu\nerror_functions=%zu\n",
               program.open_function_count, program.error_function_count);
        printf("resolved_jump_tables=%zu\nresolved_jump_table_targets=%zu\n",
               program.resolved_jump_tables,
               program.resolved_jump_table_targets);
        for (size_t i = 0; i < program.function_count; i++) {
            if (program.functions[i].status == CFG_FUNCTION_ERROR)
                printf("error_function=%s@0x%" PRIx64 "\n",
                       program.functions[i].name, program.functions[i].start);
        }
    }
    cfg_program_destroy(&program);
    return 0;
}

static int emit_static_tbset(const char *input, const char *output)
{
    CfgProgram program;
    CfgAnalyzeOptions options = { .resolve_jump_tables = true };
    char error[256] = {0};
    if (cfg_analyze_elf(input, &options, &program, error, sizeof(error))) {
        fprintf(stderr, "latc: %s\n", error[0] ? error : "analysis failed");
        return 1;
    }
    size_t written = 0;
    int result = latc_tbset_write_static(output, input, &program,
                                         &written,
                                         error, sizeof(error));
    cfg_program_destroy(&program);
    if (result) {
        fprintf(stderr, "latc: %s\n", error);
        return 1;
    }
    printf("output=%s\nstatic_tb_keys=%zu\n", output, written);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "build-id") == 0) {
        puts(LATC_BUILD_ID);
        return 0;
    }
    if (argc < 3) {
        usage(argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "emit-static-tbset") == 0) {
        const char *input = argv[2];
        const char *output = NULL;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                output = argv[++i];
            } else {
                usage(argv[0]);
                return 2;
            }
        }
        if (!output) {
            usage(argv[0]);
            return 2;
        }
        return emit_static_tbset(input, output);
    }
    if (strcmp(argv[1], "compile") == 0) {
        const char *input = argv[2], *output = NULL, *runner = NULL;
        const char *tbset = NULL;
        const char *aot = NULL;
        int tbset_ignore_outside_exec = 0;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) output = argv[++i];
            else if (strcmp(argv[i], "--runner") == 0 && i + 1 < argc) runner = argv[++i];
            else if (strcmp(argv[i], "--tbset") == 0 && i + 1 < argc) tbset = argv[++i];
            else if (strcmp(argv[i], "--tbset-ignore-outside-exec") == 0)
                tbset_ignore_outside_exec = 1;
            else if (strcmp(argv[i], "--aot") == 0 && i + 1 < argc) aot = argv[++i];
            else { usage(argv[0]); return 2; }
        }
        if (!output || !runner) { usage(argv[0]); return 2; }
        return compile_bundle(input, output, runner, tbset,
                              tbset_ignore_outside_exec, aot);
    }
    if (strcmp(argv[1], "compile-module") == 0) {
        const char *input = argv[2];
        const char *output = NULL;
        const char *runner = getenv("LATC_AOT_RUNNER");
        const char *runtime_dir = getenv("LATC_AOT_RUNTIME_DIR");
        const char *tbset = NULL;
        bool static_tbset = false;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                output = argv[++i];
            } else if (strcmp(argv[i], "--runner") == 0 && i + 1 < argc) {
                runner = argv[++i];
            } else if (strcmp(argv[i], "--runtime-dir") == 0 &&
                       i + 1 < argc) {
                runtime_dir = argv[++i];
            } else if (strcmp(argv[i], "--tbset") == 0 && i + 1 < argc) {
                tbset = argv[++i];
            } else if (strcmp(argv[i], "--static-tbset") == 0) {
                static_tbset = true;
            } else {
                usage(argv[0]);
                return 2;
            }
        }
        if (!output || !runner || !*runner || !runtime_dir || !*runtime_dir ||
            (static_tbset && !tbset)) {
            usage(argv[0]);
            return 2;
        }
        return compile_module(input, output, runner, runtime_dir, tbset,
                              static_tbset);
    }
    if (strcmp(argv[1], "inspect") == 0) {
        int json = 0; const char *path = NULL;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--json") == 0) json = 1;
            else if (!path) path = argv[i]; else { usage(argv[0]); return 2; }
        }
        if (!path) { usage(argv[0]); return 2; }
        return inspect_bundle(path, json);
    }
    if (strcmp(argv[1], "inspect-native") == 0) {
        int json = 0;
        const char *path = NULL;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--json") == 0) json = 1;
            else if (!path) path = argv[i];
            else { usage(argv[0]); return 2; }
        }
        if (!path) { usage(argv[0]); return 2; }
        return inspect_native(path, json);
    }
    if (strcmp(argv[1], "inspect-module") == 0) {
        int json = 0;
        const char *path = NULL;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--json") == 0) {
                json = 1;
            } else if (!path) {
                path = argv[i];
            } else {
                usage(argv[0]);
                return 2;
            }
        }
        if (!path) {
            usage(argv[0]);
            return 2;
        }
        return inspect_module(path, json);
    }
    if (strcmp(argv[1], "mark-native-x86") == 0) {
        char error[256] = {0};
        if (argc != 3) { usage(argv[0]); return 2; }
        if (lat_native_image_mark_x86_static_file(argv[2], error,
                                                  sizeof(error))) {
            fprintf(stderr, "latc: %s\n", error);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "merge-native") == 0) {
        const char *output = NULL;
        char error[256] = {0};
        if (argc == 6 && !strcmp(argv[4], "-o")) output = argv[5];
        if (!output) { usage(argv[0]); return 2; }
        if (lat_native_image_merge_files(argv[2], argv[3], output,
                                         error, sizeof(error))) {
            fprintf(stderr, "latc: %s\n", error);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "emit-aot-v2") == 0) {
        char error[256] = {0};
        if (argc < 4 || argc > 6) {
            usage(argv[0]);
            return 2;
        }
        int result;
        if (argc == 4) {
            result = lat_aot_v2_emit_module_sources(
                argv[2], argv[3], error, sizeof(error));
        } else {
            CfgProgram program;
            CfgAnalyzeOptions options = {.resolve_jump_tables = true};
            uint8_t digest[32];
            if (cfg_analyze_elf(argv[4], &options, &program,
                                error, sizeof(error)) ||
                (argc == 5 && file_digest(argv[4], digest,
                                          error, sizeof(error)))) {
                fprintf(stderr, "latc: %s\n", error);
                return 1;
            }
            if (argc == 6) {
                size_t matched, unmatched, ignored;
                if (latc_tbset_apply(argv[5], argv[4], &program, true,
                                     &matched, &unmatched, &ignored, digest,
                                     error, sizeof(error))) {
                    fprintf(stderr, "latc: %s\n", error);
                    cfg_program_destroy(&program);
                    return 1;
                }
            }
            result = lat_aot_v2_emit_module_sources_with_cfg(
                argv[2], argv[3], &program, digest, error, sizeof(error));
            cfg_program_destroy(&program);
        }
        if (result) {
            fprintf(stderr, "latc: %s\n", error);
            return 1;
        }
        printf("output_directory=%s\n", argv[3]);
        return 0;
    }
    if (strcmp(argv[1], "analyze") != 0) { usage(argv[0]); return 2; }
    int json = 0;
    const char *path = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) json = 1;
        else if (!path) path = argv[i];
        else { usage(argv[0]); return 2; }
    }
    if (!path) { usage(argv[0]); return 2; }
    return analyze(path, json);
}
