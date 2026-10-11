#define _GNU_SOURCE

#include "lat-aot-v2.h"
#include "latx-x86-env-offsets.h"
#include "module-loader.h"
#include "module-pack.h"
#include "native-image.h"
#include "enter-x86.h"
#include "guest-loader.h"

#include <errno.h>
#include <setjmp.h>
#include <glib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static _Thread_local sigjmp_buf syscall_jump;

static void capture_syscall(void *opaque)
{
    (void)opaque;
    siglongjmp(syscall_jump, 1);
}

static void *capture_bound_syscall_in_thread(void *opaque)
{
    bool *captured = opaque;
    if (!sigsetjmp(syscall_jump, 1)) {
        lat_aot_runtime_raise_syscall();
    }
    *captured = true;
    return NULL;
}

static uint64_t load_u64(const unsigned char *environment, size_t offset)
{
    uint64_t value;
    memcpy(&value, environment + offset, sizeof(value));
    return value;
}

static size_t guest_page_storage_count(const LatAotModuleV2 *module)
{
    size_t address_count = module->guest_slot_begin ?
        (size_t)(module->guest_slot_end - module->guest_slot_begin) : 0;
    size_t leaf_pages =
        (address_count + LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT - 1) /
        LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
    if (!(module->module_flags & LAT_AOT_MODULE_THREE_LEVEL_GUEST_SLOTS)) {
        return leaf_pages * LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
    }
    size_t root_pages =
        (leaf_pages + LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT - 1) /
        LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
    return (root_pages + leaf_pages) * LAT_AOT_V2_GUEST_PAGE_SLOT_COUNT;
}

static int execute_instance(LatAotRegistryV2 *registry,
                            LatAotModuleInstanceV2 *instance,
                            uint64_t entry_rva, unsigned char *environment,
                            void *stack_top, void *jump_cache)
{
    size_t page_storage_count =
        guest_page_storage_count(instance->module->descriptor);
    uint64_t *page_storage = calloc(page_storage_count ? page_storage_count : 1,
                                    sizeof(*page_storage));
    if (!page_storage) {
        perror("allocate guest slot pages");
        return -1;
    }
    LatAotTargetV2 target;
    size_t context_slot_count;
    int prepare_result = lat_aot_v2_context_apply_guest_table(
            instance->module->descriptor, instance->guest_load_bias,
            jump_cache, page_storage, page_storage_count, &context_slot_count);
    if (!prepare_result) {
        prepare_result = lat_aot_v2_registry_lookup(
            registry, instance->guest_load_bias + entry_rva,
            LAT_AOT_TB_CODE64 | LAT_AOT_TB_PARALLEL, &target);
    }
    int saved_errno = errno;
    free(page_storage);
    if (prepare_result) {
        errno = saved_errno;
        perror("prepare translated module execution");
        return -1;
    }
    memset(environment, 0, 4096);
    if (!sigsetjmp(syscall_jump, 1)) {
        lat_native_enter_x86_static_exec((void *)target.host_address,
                                         environment, stack_top, jump_cache);
        fprintf(stderr, "translated TB returned without raising a syscall\n");
        lat_aot_v2_registry_target_release(&target);
        return -1;
    }
    lat_aot_v2_registry_target_release(&target);
    return 0;
}

static int expect_syscall(const unsigned char *environment,
                          uint64_t number, uint64_t first_argument)
{
    if (load_u64(environment, LATC_X86_ENV_RAX_OFFSET) != number ||
        load_u64(environment, LATC_X86_ENV_RDI_OFFSET) != first_argument) {
        fprintf(stderr, "wrong syscall state: rax=%llu rdi=%llu\n",
                (unsigned long long)load_u64(environment,
                                             LATC_X86_ENV_RAX_OFFSET),
                (unsigned long long)load_u64(environment,
                                             LATC_X86_ENV_RDI_OFFSET));
        return -1;
    }
    return 0;
}

static int test_exit42(LatAotRegistryV2 *registry,
                       LatAotModuleInstanceV2 *first,
                       LatAotModuleInstanceV2 *second, uint64_t entry_rva,
                       unsigned char *environment, void *stack_top,
                       void *jump_cache)
{
    if (execute_instance(registry, first, entry_rva, environment,
                         stack_top, jump_cache) ||
        expect_syscall(environment, 60, 42) ||
        load_u64(environment, LATC_X86_ENV_EXCEPTION_NEXT_EIP_OFFSET) !=
            first->guest_load_bias + entry_rva + 12 ||
        execute_instance(registry, second, entry_rva, environment,
                         stack_top, jump_cache) ||
        expect_syscall(environment, 60, 42) ||
        load_u64(environment, LATC_X86_ENV_EXCEPTION_NEXT_EIP_OFFSET) !=
            second->guest_load_bias + entry_rva + 12) {
        fprintf(stderr, "exit42 translated state mismatch\n");
        return -1;
    }
    return 0;
}

static int test_hello(const unsigned char *image, size_t image_size,
                      LatAotRegistryV2 *registry,
                      LatAotModuleInstanceV2 *first,
                      LatAotModuleInstanceV2 *second, uint64_t entry_rva,
                      unsigned char *environment, void *stack_top,
                      void *jump_cache, char *error, size_t error_size)
{
    static const char message[] = "Hello, LATC!\n";
    const LatAotModuleV2 *descriptor = first->module->descriptor;
    size_t tb_count = (size_t)(descriptor->tb_end - descriptor->tb_begin);
    if (tb_count != 2 ||
        execute_instance(registry, first, entry_rva, environment,
                         stack_top, jump_cache) ||
        expect_syscall(environment, 1, 1)) {
        fprintf(stderr, "hello write TB did not raise write(1)\n");
        return -1;
    }
    uint64_t message_address = load_u64(environment,
                                        LATC_X86_ENV_RSI_OFFSET);
    uint64_t next_pc = load_u64(environment,
        LATC_X86_ENV_EXCEPTION_NEXT_EIP_OFFSET);
    if (load_u64(environment, LATC_X86_ENV_RDX_OFFSET) !=
            sizeof(message) - 1 ||
        next_pc != first->guest_load_bias + descriptor->tb_begin[1].guest_rva) {
        fprintf(stderr, "hello write arguments are incorrect\n");
        return -1;
    }
    LatGuestMapping mapping = {0};
    const LatNativeImageHeaderV2 *header = (const void *)image;
    if (lat_guest_map(header, image, image_size, &mapping,
                      error, error_size)) {
        fprintf(stderr, "cannot map hello guest: %s\n", error);
        return -1;
    }
    int result = 0;
    if (message_address < mapping.base ||
        message_address + sizeof(message) - 1 > mapping.end ||
        memcmp((const void *)(uintptr_t)message_address,
               message, sizeof(message) - 1)) {
        fprintf(stderr, "hello guest message pointer is incorrect\n");
        result = -1;
    }
    lat_guest_unmap(&mapping);
    if (result || execute_instance(registry, first,
            descriptor->tb_begin[1].guest_rva, environment,
            stack_top, jump_cache) || expect_syscall(environment, 60, 0)) {
        fprintf(stderr, "hello exit TB did not raise exit(0)\n");
        return -1;
    }
    if (execute_instance(registry, second, entry_rva, environment,
                         stack_top, jump_cache) ||
        expect_syscall(environment, 1, 1) ||
        load_u64(environment, LATC_X86_ENV_RSI_OFFSET) !=
            second->guest_load_bias +
            (message_address - first->guest_load_bias) ||
        load_u64(environment, LATC_X86_ENV_EXCEPTION_NEXT_EIP_OFFSET) !=
            second->guest_load_bias + descriptor->tb_begin[1].guest_rva) {
        fprintf(stderr, "hello second load bias is incorrect\n");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3 || argc > 4 ||
        (argc == 4 && strcmp(argv[3], "--exit42"))) {
        fprintf(stderr,
                "usage: %s AOT_MODULE NATIVE_IMAGE [--exit42]\n", argv[0]);
        return 2;
    }
    bool force_exit42 = argc == 4;
    gchar *image = NULL;
    gsize image_size = 0;
    if (!g_file_get_contents(argv[2], &image, &image_size, NULL) ||
        lat_native_image_validate(image, image_size, NULL, 0)) {
        fprintf(stderr, "cannot read native image\n");
        g_free(image);
        return 1;
    }
    const LatNativeImageHeaderV2 *header = (const void *)image;
    LatAotExpectedV2 expected = {
        .available_features = LAT_AOT_V2_REQUIRED_BASE_FEATURES |
                              LAT_AOT_FEATURE_LASX,
    };
    memcpy(expected.source_sha256, header->guest_sha256,
           sizeof(expected.source_sha256));
    lat_aot_v2_codegen_digest(header->lat_build_id, expected.codegen_id);
    LatAotLoadedModuleV2 module;
    char error[256] = {0};
    if (lat_aot_v2_module_open(argv[1], &expected, &module,
                               error, sizeof(error))) {
        fprintf(stderr, "cannot load translated module: %s\n", error);
        g_free(image);
        return 1;
    }
    if (lat_aot_runtime_bind_syscall(capture_syscall, NULL)) {
        perror("bind syscall callback");
        g_free(image);
        return 1;
    }
    bool thread_captured = false;
    pthread_t thread;
    if (pthread_create(&thread, NULL, capture_bound_syscall_in_thread,
                       &thread_captured) ||
        pthread_join(thread, NULL) || !thread_captured) {
        fprintf(stderr, "worker thread did not inherit syscall callback\n");
        g_free(image);
        return 1;
    }
    LatAotRegistryV2 registry;
    if (lat_aot_v2_registry_init(&registry)) {
        perror("registry init");
        g_free(image);
        return 1;
    }
    uint64_t entry_rva = header->guest_entry - header->preferred_guest_base;
    LatAotModuleInstanceV2 first = {
        .module = &module,
        .guest_load_bias = header->preferred_guest_base,
        .guest_begin = header->preferred_guest_base,
        .guest_end = header->preferred_guest_base + 0x100000,
    };
    LatAotModuleInstanceV2 second = {
        .module = &module,
        .guest_load_bias = header->preferred_guest_base + 0x300000,
        .guest_begin = header->preferred_guest_base + 0x300000,
        .guest_end = header->preferred_guest_base + 0x400000,
    };
    unsigned char *environment = calloc(1, 4096);
    unsigned char *context = calloc(1, 4096);
    unsigned char *stack = malloc(1024 * 1024);
    void *jump_cache = context ? context + 2048 : NULL;
    int result = 1;
    if (!environment || !context || !stack ||
        lat_aot_v2_registry_register(&registry, &first) ||
        lat_aot_v2_registry_register(&registry, &second) ||
        ((force_exit42 ||
          (size_t)(module.descriptor->tb_end - module.descriptor->tb_begin) == 1) ?
          test_exit42(&registry, &first, &second, entry_rva, environment,
                      stack + 1024 * 1024, jump_cache) :
          test_hello((const unsigned char *)image, image_size, &registry,
                     &first, &second, entry_rva, environment,
                     stack + 1024 * 1024, jump_cache,
                     error, sizeof(error)))) {
        fprintf(stderr, "translated module execution failed\n");
    } else {
        puts("test-aot-v2-translated-module: PASS");
        result = 0;
    }
    free(stack);
    free(context);
    free(environment);
    lat_aot_v2_registry_destroy(&registry);
    g_free(image);
    return result;
}
