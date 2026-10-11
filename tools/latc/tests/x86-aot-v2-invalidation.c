#define _GNU_SOURCE

#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <link.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef int (*ValueFn)(void);

typedef struct Module {
    void *handle;
    ValueFn value;
    ValueFn cross_page;
    uintptr_t exec_begin;
    uintptr_t exec_end;
} Module;

typedef struct FindExec {
    uintptr_t address;
    uintptr_t begin;
    uintptr_t end;
} FindExec;

static size_t page_size;

static uintptr_t page_floor(uintptr_t value)
{
    return value & ~(page_size - 1);
}

static uintptr_t page_ceil(uintptr_t value)
{
    return (value + page_size - 1) & ~(page_size - 1);
}

static int find_exec_segment(struct dl_phdr_info *info, size_t size,
                             void *opaque)
{
    (void)size;
    FindExec *find = opaque;
    for (ElfW(Half) i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr) *phdr = &info->dlpi_phdr[i];
        if (phdr->p_type != PT_LOAD || !(phdr->p_flags & PF_X)) {
            continue;
        }
        uintptr_t begin = page_floor(info->dlpi_addr + phdr->p_vaddr);
        uintptr_t end = page_ceil(info->dlpi_addr + phdr->p_vaddr +
                                  phdr->p_memsz);
        if (find->address >= begin && find->address < end) {
            find->begin = begin;
            find->end = end;
            return 1;
        }
    }
    return 0;
}

static int load_module(const char *path, Module *module)
{
    memset(module, 0, sizeof(*module));
    fprintf(stderr, "LATC_INVALIDATION_LOAD_BEGIN\n");
    module->handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!module->handle) {
        fprintf(stderr, "dlopen %s: %s\n", path, dlerror());
        return -1;
    }
    module->value = (ValueFn)dlsym(module->handle, "latc_invalidation_value");
    module->cross_page = (ValueFn)dlsym(
        module->handle, "latc_invalidation_cross_page");
    if (!module->value || !module->cross_page) {
        fprintf(stderr, "dlsym: %s\n", dlerror());
        return -1;
    }
    FindExec find = { .address = (uintptr_t)module->value };
    dl_iterate_phdr(find_exec_segment, &find);
    if (!find.begin || find.end <= find.begin) {
        fprintf(stderr, "cannot find executable segment\n");
        return -1;
    }
    module->exec_begin = find.begin;
    module->exec_end = find.end;
    fprintf(stderr, "LATC_INVALIDATION_LOAD_END\n");
    return 0;
}

static void install_return_value(ValueFn function, int value)
{
    unsigned char *code = (unsigned char *)(uintptr_t)function;
    code[0] = 0xb8;
    *(volatile uint32_t *)(code + 1) = (uint32_t)value;
    code[5] = 0xc3;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

static int replace_target_page(ValueFn function, int unmap_first)
{
    uintptr_t page = page_floor((uintptr_t)function);
    if (unmap_first && munmap((void *)page, page_size)) {
        perror("munmap target page");
        return -1;
    }
    void *mapped = mmap((void *)page, page_size,
                        PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (mapped != (void *)page) {
        perror("mmap target page");
        return -1;
    }
    install_return_value(function, 42);
    return 0;
}

static int run_map_fixed(Module *module)
{
    return module->value() == 17 &&
           !replace_target_page(module->value, 0) && module->value() == 42 ?
           0 : -1;
}

static int run_map_fixed_failed(Module *module)
{
    uintptr_t page = (uintptr_t)module->value & ~(uintptr_t)0x3fff;
    size_t size = 0x4000;
    if (module->value() != 17 ||
        mmap((void *)page, size, PROT_READ | PROT_EXEC,
             MAP_PRIVATE | MAP_FIXED, -1, 0) != MAP_FAILED ||
        errno != EBADF) {
        fprintf(stderr, "MAP_FIXED with invalid fd did not fail with EBADF\n");
        return -1;
    }
    return module->value() == 17 ? 0 : -1;
}

static int run_munmap(Module *module, int complete)
{
    if (module->value() != 17) {
        return -1;
    }
    uintptr_t target_page = page_floor((uintptr_t)module->value);
    uintptr_t begin = complete ? module->exec_begin : target_page;
    uintptr_t end = complete ? module->exec_end : target_page + page_size;
    if (munmap((void *)begin, end - begin)) {
        perror("munmap executable range");
        return -1;
    }
    void *mapped = mmap((void *)target_page, page_size,
                        PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (mapped != (void *)target_page) {
        perror("remap target page");
        return -1;
    }
    install_return_value(module->value, 42);
    return module->value() == 42 ? 0 : -1;
}

static int run_mprotect(Module *module)
{
    uintptr_t page = page_floor((uintptr_t)module->value);
    if (module->value() != 17 ||
        mprotect((void *)page, page_size, PROT_READ | PROT_WRITE)) {
        perror("mprotect writable");
        return -1;
    }
    install_return_value(module->value, 42);
    if (mprotect((void *)page, page_size, PROT_READ | PROT_EXEC)) {
        perror("mprotect executable");
        return -1;
    }
    return module->value() == 42 ? 0 : -1;
}

static int run_mprotect_unchanged(Module *module)
{
    uintptr_t page = page_floor((uintptr_t)module->value);
    if (module->value() != 17 ||
        mprotect((void *)page, page_size, PROT_READ | PROT_WRITE)) {
        perror("mprotect unchanged writable");
        return -1;
    }
    if (mprotect((void *)page, page_size, PROT_READ | PROT_EXEC)) {
        perror("mprotect unchanged executable");
        return -1;
    }
    return module->value() == 17 ? 0 : -1;
}

static int run_mprotect_failed(Module *module)
{
    uintptr_t page = page_floor((uintptr_t)module->value);
    if (module->value() != 17 ||
        mprotect((void *)(page + 1), page_size,
                 PROT_READ | PROT_WRITE) != -1 || errno != EINVAL) {
        fprintf(stderr, "misaligned mprotect did not fail with EINVAL\n");
        return -1;
    }
    return module->value() == 17 ? 0 : -1;
}

static int run_mremap_failed(Module *module)
{
    uintptr_t page = page_floor((uintptr_t)module->value);
    if (module->value() != 17 ||
        mremap((void *)(page + 1), page_size, page_size,
               MREMAP_MAYMOVE) != MAP_FAILED || errno != EINVAL) {
        fprintf(stderr, "misaligned mremap did not fail with EINVAL\n");
        return -1;
    }
    return module->value() == 17 ? 0 : -1;
}

static int run_cross_page(Module *module)
{
    unsigned char *code = (unsigned char *)(uintptr_t)module->cross_page;
    uintptr_t first_page = page_floor((uintptr_t)code);
    if (page_floor((uintptr_t)(code + 1)) ==
            page_floor((uintptr_t)(code + 4)) ||
        module->cross_page() != 17) {
        fprintf(stderr, "cross-page fixture is not page crossing\n");
        return -1;
    }
    if (mprotect((void *)first_page, 2 * page_size,
                 PROT_READ | PROT_WRITE)) {
        perror("mprotect cross-page writable");
        return -1;
    }
    volatile unsigned char *immediate = code + 1;
    immediate[0] = 42;
    immediate[1] = 0;
    immediate[2] = 0;
    immediate[3] = 0;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    if (mprotect((void *)first_page, 2 * page_size,
                 PROT_READ | PROT_EXEC)) {
        perror("mprotect cross-page executable");
        return -1;
    }
    return module->cross_page() == 42 ? 0 : -1;
}

typedef struct ThreadWrite {
    ValueFn function;
    _Atomic int ready;
    _Atomic int stop;
    _Atomic int failed;
} ThreadWrite;

static void *write_target(void *opaque)
{
    ThreadWrite *test = opaque;
    uintptr_t page = page_floor((uintptr_t)test->function);
    if (mprotect((void *)page, page_size,
                 PROT_READ | PROT_WRITE | PROT_EXEC)) {
        atomic_store_explicit(&test->failed, 1, memory_order_release);
        return NULL;
    }
    *(volatile uint32_t *)((unsigned char *)(uintptr_t)test->function + 1) = 42;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    if (mprotect((void *)page, page_size, PROT_READ | PROT_EXEC)) {
        atomic_store_explicit(&test->failed, 1, memory_order_release);
        return NULL;
    }
    atomic_store_explicit(&test->ready, 1, memory_order_release);
    return NULL;
}

static void *call_target(void *opaque)
{
    ThreadWrite *test = opaque;
    atomic_store_explicit(&test->ready, 1, memory_order_release);
    while (!atomic_load_explicit(&test->stop, memory_order_acquire)) {
        int value = test->function();
        if (value != 17 && value != 42) {
            atomic_store_explicit(&test->failed, 1, memory_order_release);
            break;
        }
    }
    return NULL;
}

static int run_thread_write(Module *module)
{
    ThreadWrite test = { .function = module->value };
    pthread_t thread;
    if (module->value() != 17 ||
        pthread_create(&thread, NULL, write_target, &test) ||
        pthread_join(thread, NULL)) {
        return -1;
    }
    return !atomic_load_explicit(&test.failed, memory_order_acquire) &&
           atomic_load_explicit(&test.ready, memory_order_acquire) &&
           module->value() == 42 ? 0 : -1;
}

static int run_concurrent(Module *module)
{
    ThreadWrite test = { .function = module->value };
    pthread_t thread;
    if (module->value() != 17 ||
        pthread_create(&thread, NULL, call_target, &test)) {
        return -1;
    }
    while (!atomic_load_explicit(&test.ready, memory_order_acquire)) {
        sched_yield();
    }
    for (int i = 0; i < 1000; i++) {
        sched_yield();
    }
    uintptr_t page = page_floor((uintptr_t)module->value);
    if (mprotect((void *)page, page_size,
                 PROT_READ | PROT_WRITE | PROT_EXEC)) {
        atomic_store_explicit(&test.stop, 1, memory_order_release);
        pthread_join(thread, NULL);
        return -1;
    }
    *(volatile uint32_t *)((unsigned char *)(uintptr_t)module->value + 1) = 42;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    if (mprotect((void *)page, page_size, PROT_READ | PROT_EXEC)) {
        atomic_store_explicit(&test.stop, 1, memory_order_release);
        pthread_join(thread, NULL);
        return -1;
    }
    atomic_store_explicit(&test.stop, 1, memory_order_release);
    if (pthread_join(thread, NULL) ||
        atomic_load_explicit(&test.failed, memory_order_acquire)) {
        return -1;
    }
    return module->value() == 42 ? 0 : -1;
}

static int run_unaffected(const char *second_path, Module *first)
{
    Module second;
    if (load_module(second_path, &second) || first->value() != 17 ||
        second.value() != 17 || run_mprotect(first)) {
        return -1;
    }
    return first->value() == 42 && second.value() == 17 ? 0 : -1;
}

static int run_reload(const char *path, Module *module)
{
    if (module->value() != 17 || dlclose(module->handle)) {
        return -1;
    }
    memset(module, 0, sizeof(*module));
    if (load_module(path, module)) {
        return -1;
    }
    return module->value() == 17 ? 0 : -1;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: %s MODE PLUGIN SECOND_PLUGIN\n", argv[0]);
        return 2;
    }
    page_size = (size_t)sysconf(_SC_PAGESIZE);
    if (!page_size || (page_size & (page_size - 1))) {
        return 2;
    }
    Module module;
    if (load_module(argv[2], &module)) {
        return 1;
    }
    int result;
    if (!strcmp(argv[1], "map-fixed")) {
        result = run_map_fixed(&module);
    } else if (!strcmp(argv[1], "map-fixed-failed")) {
        result = run_map_fixed_failed(&module);
    } else if (!strcmp(argv[1], "munmap-partial")) {
        result = run_munmap(&module, 0);
    } else if (!strcmp(argv[1], "munmap-complete")) {
        result = run_munmap(&module, 1);
    } else if (!strcmp(argv[1], "mprotect")) {
        result = run_mprotect(&module);
    } else if (!strcmp(argv[1], "mprotect-unchanged")) {
        result = run_mprotect_unchanged(&module);
    } else if (!strcmp(argv[1], "mprotect-failed")) {
        result = run_mprotect_failed(&module);
    } else if (!strcmp(argv[1], "mremap-failed")) {
        result = run_mremap_failed(&module);
    } else if (!strcmp(argv[1], "smc-cross")) {
        result = run_cross_page(&module);
    } else if (!strcmp(argv[1], "smc-thread")) {
        result = run_thread_write(&module);
    } else if (!strcmp(argv[1], "concurrent")) {
        result = run_concurrent(&module);
    } else if (!strcmp(argv[1], "unaffected")) {
        result = run_unaffected(argv[3], &module);
    } else if (!strcmp(argv[1], "reload")) {
        result = run_reload(argv[2], &module);
    } else {
        fprintf(stderr, "unknown mode: %s\n", argv[1]);
        return 2;
    }
    if (result) {
        fprintf(stderr, "%s: FAIL\n", argv[1]);
        fflush(NULL);
        _exit(1);
    }
    printf("%s: PASS\n", argv[1]);
    fflush(NULL);
    _exit(0);
}
