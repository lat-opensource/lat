#include "native-image.h"
#include "lat-fallback.h"
#include "guest-loader.h"
#include "relocate.h"
#include "dispatch.h"
#include "enter-x86.h"
#include "latx-x86-env-offsets.h"

#include <elf.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <unistd.h>

extern const unsigned char latc_embedded_image_start[];
extern const unsigned char latc_embedded_image_end[];

typedef struct LatAuxvEntry {
    uint64_t type;
    uint64_t value;
} LatAuxvEntry;

static int fill_random(void *buffer, size_t size)
{
    unsigned char *cursor = buffer;
    while (size) {
        ssize_t result = getrandom(cursor, size, 0);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) return -1;
        cursor += result;
        size -= (size_t)result;
    }
    return 0;
}

static int push_stack_string(uintptr_t *cursor, uintptr_t bottom,
                             const char *string, uintptr_t *address)
{
    size_t size = strlen(string) + 1;
    if (size > *cursor - bottom) return -1;
    *cursor -= size;
    memcpy((void *)*cursor, string, size);
    *address = *cursor;
    return 0;
}

static void *prepare_x86_initial_stack(void *stack, size_t stack_size,
                                       const LatGuestMapping *mapping,
                                       int argc, char **argv, char **envp)
{
    static const char platform[] = "x86_64";
    uintptr_t cursor = (uintptr_t)stack + stack_size;
    uintptr_t bottom = (uintptr_t)stack;
    size_t envc = 0;
    while (envp && envp[envc]) envc++;
    if (!mapping || argc <= 0 || !argv || !argv[0]) return NULL;
    uintptr_t *argv_addresses = calloc((size_t)argc, sizeof(*argv_addresses));
    uintptr_t *env_addresses = calloc(envc ? envc : 1,
                                      sizeof(*env_addresses));
    if (!argv_addresses || !env_addresses) goto fail;
    for (size_t i = envc; i-- > 0;) {
        if (push_stack_string(&cursor, bottom, envp[i], &env_addresses[i]))
            goto fail;
    }
    for (int i = argc; i-- > 0;) {
        if (push_stack_string(&cursor, bottom, argv[i], &argv_addresses[i]))
            goto fail;
    }
    if (sizeof(platform) > cursor - bottom) goto fail;
    cursor -= sizeof(platform);
    memcpy((void *)cursor, platform, sizeof(platform));
    uintptr_t platform_address = cursor;
    if (cursor - bottom < 16) goto fail;
    cursor -= 16;
    if (fill_random((void *)cursor, 16)) goto fail;
    uintptr_t random_address = cursor;
    cursor &= ~(uintptr_t)15;
    long page_size = sysconf(_SC_PAGESIZE);
    long clock_ticks = sysconf(_SC_CLK_TCK);
    if (page_size <= 0 || clock_ticks <= 0) goto fail;
    LatAuxvEntry auxv[] = {
        { AT_PHDR, mapping->phdr },
        { AT_PHENT, mapping->phent },
        { AT_PHNUM, mapping->phnum },
        { AT_PAGESZ, (uint64_t)page_size },
        { AT_BASE, 0 },
        { AT_FLAGS, 0 },
        { AT_ENTRY, mapping->entry },
        { AT_UID, getuid() },
        { AT_EUID, geteuid() },
        { AT_GID, getgid() },
        { AT_EGID, getegid() },
        { AT_CLKTCK, (uint64_t)clock_ticks },
        { AT_PLATFORM, platform_address },
        { AT_HWCAP, 0 },
        { AT_SECURE, 0 },
        { AT_RANDOM, random_address },
        { AT_HWCAP2, 0 },
        { AT_EXECFN, argv_addresses[0] },
        { AT_NULL, 0 },
    };
    size_t auxv_count = sizeof(auxv) / sizeof(auxv[0]);
    size_t word_count = 1 + (size_t)argc + 1 + envc + 1 + auxv_count * 2;
    size_t word_bytes = word_count * sizeof(uint64_t);
    if (word_bytes > cursor - bottom) goto fail;
    cursor = (cursor - word_bytes) & ~(uintptr_t)15;
    if (cursor < bottom) goto fail;
    uint64_t *words = (void *)cursor;
    size_t out = 0;
    words[out++] = (uint64_t)argc;
    for (int i = 0; i < argc; i++) words[out++] = argv_addresses[i];
    words[out++] = 0;
    for (size_t i = 0; i < envc; i++) words[out++] = env_addresses[i];
    words[out++] = 0;
    memcpy(&words[out], auxv, sizeof(auxv));
    free(argv_addresses);
    free(env_addresses);
    return words;
fail:
    free(argv_addresses);
    free(env_addresses);
    return NULL;
}

static int inspect_image(const LatNativeImageHeaderV2 *header)
{
    printf("execution_model=lat-native-pie-shell\n"
           "guest_entry=0x%" PRIx64 "\n"
           "guest_size=%" PRIu64 "\n"
           "code_size=%" PRIu64 "\n"
           "tbs=%" PRIu64 "\n"
           "relocations=%" PRIu64 "\n"
           "flags=0x%x\n"
           "lat_build_id=%s\n",
           header->guest_entry, header->guest_image_size,
           header->code_size, header->tb_count,
           header->relocation_count, header->flags,
           header->lat_build_id);
    return 0;
}

int main(int argc, char **argv, char **envp)
{
    size_t image_size = (size_t)(latc_embedded_image_end -
                                 latc_embedded_image_start);
    char error[256] = {0};
    if (lat_native_image_validate(latc_embedded_image_start, image_size,
                                  error, sizeof(error)) != 0) {
        fprintf(stderr, "latc: invalid embedded native image: %s\n", error);
        return 125;
    }
    const LatNativeImageHeaderV2 *header =
        (const void *)latc_embedded_image_start;
    if (argc == 2 && strcmp(argv[1], "--latc-inspect") == 0) {
        return inspect_image(header);
    }
    if (argc == 2 && strcmp(argv[1], "--latc-map") == 0) {
        LatGuestMapping mapping = {0};
        if (lat_guest_map(header, latc_embedded_image_start, image_size,
                          &mapping, error, sizeof(error))) {
            fprintf(stderr, "latc: cannot map embedded guest: %s\n", error);
            return 124;
        }
        printf("guest_base=0x%" PRIx64 "\n"
               "guest_end=0x%" PRIx64 "\n"
               "guest_entry=0x%" PRIx64 "\n",
               mapping.base, mapping.end, mapping.entry);
        lat_guest_unmap(&mapping);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--latc-relocate") == 0) {
        LatNativeCode code = {0};
        if (lat_native_code_load(header, latc_embedded_image_start, image_size,
                                 &code, error, sizeof(error))) {
            fprintf(stderr, "latc: cannot relocate native code: %s\n", error);
            return 123;
        }
        printf("native_code=%p\nnative_code_size=%zu\nrelocations=%" PRIu64
               "\n", code.address, code.size, header->relocation_count);
        lat_native_code_unload(&code);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--latc-run-smoke") == 0) {
        if (!(header->flags & LAT_NATIVE_IMAGE_C_ABI_SMOKE)) {
            fprintf(stderr, "latc: image is not a C ABI smoke test\n");
            return 122;
        }
        const LatNativeTbV1 *tb = lat_native_tb_find(
            header, latc_embedded_image_start, image_size,
            header->guest_entry, 0);
        if (!tb) {
            fprintf(stderr, "latc: smoke entry TB is missing\n");
            return 121;
        }
        LatNativeCode code = {0};
        if (lat_native_code_load(header, latc_embedded_image_start, image_size,
                                 &code, error, sizeof(error))) {
            fprintf(stderr, "latc: cannot load smoke code: %s\n", error);
            return 120;
        }
        int (*entry)(void) = (void *)((unsigned char *)code.address +
                                      tb->code_offset);
        int result = entry();
        lat_native_code_unload(&code);
        printf("smoke_result=%d\n", result);
        return result == 42 ? 0 : 119;
    }
    if (argc == 2 && strcmp(argv[1], "--latc-run-state-smoke") == 0) {
        if (!(header->flags & LAT_NATIVE_IMAGE_C_ABI_STATE_SMOKE)) {
            fprintf(stderr, "latc: image is not a state ABI smoke test\n");
            return 118;
        }
        const LatNativeTbV1 *tb = lat_native_tb_find(
            header, latc_embedded_image_start, image_size,
            header->guest_entry, 0);
        LatNativeCode code = {0};
        if (!tb || lat_native_code_load(header, latc_embedded_image_start,
                                        image_size, &code, error,
                                        sizeof(error))) {
            fprintf(stderr, "latc: cannot load state smoke code: %s\n",
                    tb ? error : "entry TB is missing");
            return 117;
        }
        LatX86StateV1 state = {0};
        state.gpr[0] = 35;
        int (*entry)(LatX86StateV1 *) =
            (void *)((unsigned char *)code.address + tb->code_offset);
        int result = entry(&state);
        lat_native_code_unload(&code);
        printf("state_result=%d\ngpr0=%" PRIu64 "\nrip=0x%" PRIx64 "\n",
               result, state.gpr[0], state.rip);
        return result == 42 && state.gpr[0] == 42 && state.rip == 0x1234 ?
            0 : 116;
    }
    if (argc == 2 && strcmp(argv[1], "--latc-run-dispatch-smoke") == 0) {
        if (!(header->flags & LAT_NATIVE_IMAGE_C_ABI_DISPATCH_SMOKE)) {
            fprintf(stderr, "latc: image is not a dispatch smoke test\n");
            return 115;
        }
        LatNativeCode code = {0};
        if (lat_native_code_load(header, latc_embedded_image_start, image_size,
                                 &code, error, sizeof(error))) {
            fprintf(stderr, "latc: cannot load dispatch smoke code: %s\n",
                    error);
            return 114;
        }
        LatX86StateV1 state = {0};
        state.gpr[0] = 5;
        state.rip = header->guest_entry;
        unsigned steps = 0;
        int tb_result = 0;
        while (state.rip && steps < 8) {
            const LatNativeTbV1 *tb = lat_native_tb_find(
                header, latc_embedded_image_start, image_size, state.rip, 0);
            if (!tb) {
                tb_result = -1;
                break;
            }
            int (*entry)(LatX86StateV1 *) =
                (void *)((unsigned char *)code.address + tb->code_offset);
            tb_result = entry(&state);
            steps++;
            if (tb_result) break;
        }
        lat_native_code_unload(&code);
        printf("dispatch_steps=%u\ngpr0=%" PRIu64 "\nrip=0x%" PRIx64
               "\ntb_result=%d\n", steps, state.gpr[0], state.rip,
               tb_result);
        return steps == 2 && state.gpr[0] == 24 && state.rip == 0 &&
               tb_result == 0 ? 0 : 113;
    }
    if ((header->flags & LAT_NATIVE_IMAGE_X86_STATIC_EXEC) &&
        (argc == 1 || strncmp(argv[1], "--latc-", 7))) {
        if (strcmp(header->lat_build_id, LATC_X86_ENV_BUILD_ID)) {
            fprintf(stderr, "latc: native image LAT build ID does not match\n");
            return 112;
        }
        LatGuestMapping mapping = {0};
        LatNativeCode code = {0};
        const LatNativeTbV1 *tb = lat_native_tb_find(
            header, latc_embedded_image_start, image_size,
            header->guest_entry, 0);
        if (!tb || lat_guest_map(header, latc_embedded_image_start, image_size,
                                 &mapping, error, sizeof(error)) ||
            lat_native_code_load(header, latc_embedded_image_start, image_size,
                                 &code, error, sizeof(error))) {
            fprintf(stderr, "latc: cannot prepare static x86 execution: %s\n",
                    tb ? error : "entry TB is missing");
            lat_guest_unmap(&mapping);
            lat_native_code_unload(&code);
            return 111;
        }
        void *environment = calloc(1, 4096);
        size_t stack_size = 128 * 1024 * 1024;
        void *stack = malloc(stack_size);
        size_t jump_cache_count = LAT_NATIVE_X86_JMP_CACHE_SIZE;
        LatNativeX86FastTb *jump_cache = calloc(jump_cache_count,
                                                sizeof(*jump_cache));
        if (!environment || !stack || !jump_cache) {
            fprintf(stderr, "latc: cannot allocate static x86 state\n");
            free(environment);
            free(stack);
            free(jump_cache);
            lat_guest_unmap(&mapping);
            lat_native_code_unload(&code);
            return 110;
        }
        void *entry = (unsigned char *)code.address + tb->code_offset;
        lat_native_x86_dispatch_configure(header, latc_embedded_image_start,
                                           image_size, code.address,
                                           jump_cache, jump_cache_count);
        *(void **)((unsigned char *)environment +
                   LATC_X86_ENV_TB_JMP_CACHE_PTR_OFFSET) = jump_cache;
        void *stack_top = prepare_x86_initial_stack(stack, stack_size,
                                                    &mapping, argc, argv,
                                                    envp);
        if (!stack_top) {
            fprintf(stderr, "latc: cannot prepare x86 initial stack\n");
            free(environment);
            free(stack);
            free(jump_cache);
            lat_guest_unmap(&mapping);
            lat_native_code_unload(&code);
            return 109;
        }
        lat_native_enter_x86_static_exec(entry, environment,
                                         stack_top, jump_cache);
    }
    if (argc > 1 && !strncmp(argv[1], "--latc-", 7)) {
        fprintf(stderr,
                "usage: %s [--latc-inspect|--latc-map|--latc-relocate|"
                "--latc-run-smoke|--latc-run-state-smoke|"
                "--latc-run-dispatch-smoke]\n",
                argv[0]);
        return 2;
    }
    fprintf(stderr,
            "latc: native PIE shell is valid but guest execution is not linked yet\n");
    return 126;
}
