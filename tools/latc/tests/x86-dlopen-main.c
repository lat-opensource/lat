#define _GNU_SOURCE

#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ucontext.h>

#define LATC_DLOPEN_PLUGIN "/tmp/latc-m3-dlopen-plugin.so"

typedef int (*latc_plugin_apply_fn)(int value, int (*callback)(int));
typedef int (*latc_plugin_signal_fn)(void);

static volatile sig_atomic_t signal_count;
static volatile sig_atomic_t concurrent_invalidation;
static uintptr_t expected_signal_pc;
static uintptr_t signal_resume_pc;
static int unload_request[2] = { -1, -1 };
static int unload_complete[2] = { -1, -1 };
static volatile sig_atomic_t unload_status;
static uintptr_t expected_signal_rsp __attribute__((used));
static pthread_t unload_thread;
static void *unload_handle;

static void report_hex(const char *label, uintptr_t value)
{
    char output[64];
    size_t length = 0;

    while (label[length]) {
        output[length] = label[length];
        length++;
    }
    for (int shift = (int)(sizeof(value) * 8) - 4; shift >= 0; shift -= 4) {
        unsigned int digit = (unsigned int)(value >> shift) & 15;
        output[length++] = "0123456789abcdef"[digit];
    }
    output[length++] = '\n';
    (void)write(STDERR_FILENO, output, length);
}

/* Tail-call the plugin while recording the guest stack seen at its entry. */
__attribute__((naked, noinline)) static int invoke_signal(
    latc_plugin_signal_fn signal_site)
{
    __asm__ volatile("movq %rsp, expected_signal_rsp(%rip)\n\t"
                     "jmp *%rdi");
}

/*
 * Return from the unmapped plugin without jumping into the middle of main().
 * The faulting plugin is a leaf function, so its stack already contains the
 * return address for the indirect call in main().
 */
__attribute__((naked)) static void concurrent_signal_resume(void)
{
    __asm__ volatile("xorl %eax, %eax\n\t"
                     "ret");
}

static void *unload_during_signal(void *opaque)
{
    char token;

    if (read(unload_request[0], &token, 1) != 1) {
        unload_status = 1;
        return NULL;
    }
    if (dlclose(opaque)) {
        unload_status = 2;
    }
    token = 1;
    if (write(unload_complete[1], &token, 1) != 1) {
        unload_status = 3;
    }
    return NULL;
}

static void handle_sigfpe(int signal, siginfo_t *info, void *opaque)
{
    ucontext_t *context = opaque;

    if (signal != SIGFPE || info->si_signo != SIGFPE || !context ||
        (uintptr_t)context->uc_mcontext.gregs[REG_RIP] !=
            expected_signal_pc ||
        (uint64_t)context->uc_mcontext.gregs[REG_R8] !=
            UINT64_C(0x445566778899aabb)) {
        _Exit(10);
    }
    if ((uintptr_t)context->uc_mcontext.gregs[REG_RSP] !=
        expected_signal_rsp) {
        report_hex("signal_rsp_expected=0x", expected_signal_rsp);
        report_hex("signal_rsp_actual=0x",
                   (uintptr_t)context->uc_mcontext.gregs[REG_RSP]);
        _Exit(12);
    }
    if (concurrent_invalidation) {
        char token = 1;

        /*
         * Create the guest worker only after the AOT fault.  Creating it
         * earlier sets CF_PARALLEL and makes this single-thread AOT module
         * ineligible before the signal PC can be tested.
         */
        if (pthread_create(&unload_thread, NULL, unload_during_signal,
                           unload_handle) ||
            write(unload_request[1], &token, 1) != 1 ||
            read(unload_complete[0], &token, 1) != 1 || unload_status) {
            _Exit(11);
        }
    }
    context->uc_mcontext.gregs[REG_RIP] = (greg_t)signal_resume_pc;
    signal_count++;
}

static int main_callback(int value)
{
    return value * 3;
}

int main(void)
{
    const char *race_mode = getenv("LATC_SIGNAL_INVALIDATION_RACE");
    int iterations = race_mode ? 1 : 100;
    struct sigaction action = {
        .sa_sigaction = handle_sigfpe,
        .sa_flags = SA_SIGINFO,
    };
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGFPE, &action, NULL)) {
        return 1;
    }
    uintptr_t first_base = 0;
    uintptr_t last_base = 0;
    int result = 0;
    for (int iteration = 0; iteration < iterations; iteration++) {
        void *handle = dlopen(getenv("LATC_DLOPEN_PLUGIN") ?
                              getenv("LATC_DLOPEN_PLUGIN") : LATC_DLOPEN_PLUGIN,
                              RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            fprintf(stderr, "dlopen failed: %s\n", dlerror());
            return 2;
        }
        dlerror();
        latc_plugin_apply_fn apply =
            (latc_plugin_apply_fn)dlsym(handle, "latc_plugin_apply");
        latc_plugin_signal_fn signal_site = (latc_plugin_signal_fn)dlsym(
            handle, "latc_plugin_signal_site");
        expected_signal_pc = (uintptr_t)dlsym(
            handle, "latc_plugin_signal_fault");
        uintptr_t plugin_signal_resume_pc = (uintptr_t)dlsym(
            handle, "latc_plugin_signal_resume");
        const char *error = dlerror();
        Dl_info info;
        if (error || !apply || !signal_site || !expected_signal_pc ||
            !plugin_signal_resume_pc || !dladdr((void *)apply, &info)) {
            fprintf(stderr, "dlsym failed: %s\n", error ? error : "null symbol");
            dlclose(handle);
            return 3;
        }
        result = apply(11, main_callback);
        concurrent_invalidation = race_mode != NULL;
        if (concurrent_invalidation) {
            unload_status = 0;
            unload_handle = handle;
            if (pipe(unload_request) || pipe(unload_complete)) {
                return 7;
            }
            signal_resume_pc = (uintptr_t)concurrent_signal_resume;
        } else {
            signal_resume_pc = plugin_signal_resume_pc;
        }
        if (invoke_signal(signal_site)) {
            return 6;
        }
        last_base = (uintptr_t)info.dli_fbase;
        if (!first_base) {
            first_base = last_base;
        }
        if (concurrent_invalidation) {
            if (pthread_join(unload_thread, NULL) || unload_status) {
                return 8;
            }
            close(unload_request[0]);
            close(unload_request[1]);
            close(unload_complete[0]);
            close(unload_complete[1]);
        } else {
            if (dlclose(handle)) {
                fprintf(stderr, "dlclose failed: %s\n", dlerror());
                return 4;
            }
        }
        mmap((void *)last_base, 0x5000, PROT_NONE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    }
    int moved = first_base != last_base;
    printf("dlopen loads=%d signals=%d concurrent_invalidation=%d result=%d "
           "moved=%d\n", iterations, signal_count,
           concurrent_invalidation != 0, result, moved);
    return result == 40 && signal_count == iterations &&
           (race_mode || moved) ? 0 : 5;
}
