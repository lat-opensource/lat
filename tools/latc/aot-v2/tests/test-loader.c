#include "module-loader.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void fill(unsigned char value, unsigned char output[32])
{
    memset(output, value, 32);
}

int main(int argc, char **argv)
{
    int expect_reject = argc == 3 && !strcmp(argv[1], "--expect-reject");
    int expect_pc_reject =
        argc == 3 && !strcmp(argv[1], "--expect-pc-map-reject");
    if (argc != 2 && !expect_reject && !expect_pc_reject) {
        fprintf(stderr,
                "usage: %s [--expect-reject|--expect-pc-map-reject] "
                "AOT_MODULE\n", argv[0]);
        return 2;
    }
    if (lat_aot_runtime_abi_version() != LAT_AOT_V2_ABI_VERSION) {
        fprintf(stderr, "runtime ABI version mismatch\n");
        return 1;
    }
    LatAotExpectedV2 expected = {
        .available_features = LAT_AOT_V2_REQUIRED_BASE_FEATURES,
    };
    fill(0x11, expected.source_sha256);
    fill(0x22, expected.codegen_id);
    LatAotLoadedModuleV2 module;
    char error[256] = {0};
    const char *module_path = argv[expect_reject || expect_pc_reject ? 2 : 1];
    int open_result = lat_aot_v2_module_open(module_path, &expected, &module,
                                             error, sizeof(error));
    if (expect_reject) {
        if (!open_result || !strstr(error, "descriptor")) {
            fprintf(stderr, "bad descriptor was not rejected: %s\n", error);
            return 1;
        }
        puts("test-aot-v2-loader-reject: PASS");
        return 0;
    }
    if (expect_pc_reject) {
        if (!open_result || !strstr(error, "PC map")) {
            fprintf(stderr, "bad PC map was not rejected: %s\n", error);
            return 1;
        }
        puts("test-aot-v2-loader-pc-map-reject: PASS");
        return 0;
    }
    if (open_result) {
        fprintf(stderr, "cannot load fixture: %s\n", error);
        return 1;
    }
    if (module.backing_fd < 0 || fcntl(module.backing_fd, F_GETFD) < 0) {
        fprintf(stderr, "validated module backing fd was not retained\n");
        return 1;
    }

    LatAotRegistryV2 registry;
    if (lat_aot_v2_registry_init(&registry)) {
        perror("registry init");
        return 1;
    }
    LatAotModuleInstanceV2 first = {
        .module = &module,
        .guest_load_bias = 0x400000,
        .guest_begin = 0x400000,
        .guest_end = 0x500000,
    };
    LatAotModuleInstanceV2 second = {
        .module = &module,
        .guest_load_bias = 0x700000,
        .guest_begin = 0x700000,
        .guest_end = 0x800000,
    };
    if (lat_aot_v2_registry_register(&registry, &first) ||
        lat_aot_v2_registry_register(&registry, &second)) {
        perror("registry register");
        return 1;
    }
    LatAotTargetV2 target;
    if (lat_aot_v2_registry_lookup(&registry, 0x401000,
                                   LAT_AOT_TB_CODE64 |
                                   LAT_AOT_TB_PARALLEL, &target) ||
        target.instance != &first) {
        fprintf(stderr, "first module instance lookup failed\n");
        return 1;
    }
    uint64_t (*entry)(void) = (uint64_t (*)(void))target.host_address;
    if (entry() != 42) {
        fprintf(stderr, "fixture TB returned the wrong result\n");
        return 1;
    }
    lat_aot_v2_registry_target_release(&target);
    if (lat_aot_v2_registry_lookup(&registry, 0x701000,
                                   LAT_AOT_TB_CODE64 |
                                   LAT_AOT_TB_PARALLEL, &target) ||
        target.instance != &second) {
        fprintf(stderr, "second module instance lookup failed\n");
        return 1;
    }
    lat_aot_v2_registry_target_release(&target);
    if (lat_aot_v2_registry_deactivate(&registry, &first) ||
        lat_aot_v2_registry_lookup(&registry, 0x401000,
                                   LAT_AOT_TB_CODE64 |
                                   LAT_AOT_TB_PARALLEL, &target) == 0) {
        fprintf(stderr, "multiple instance or deactivation test failed\n");
        return 1;
    }
    lat_aot_v2_registry_destroy(&registry);
    puts("test-aot-v2-loader: PASS");
    return 0;
}
