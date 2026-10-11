#define _GNU_SOURCE

#include "precompile.h"

#include <elf.h>
#include <errno.h>
#include <glib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define IMAGE_SIZE 0x500
#define LOAD_ADDRESS UINT64_C(0x400000)

static void die(const char *message)
{
    fprintf(stderr, "test-precompile: %s\n", message);
    exit(1);
}

static void make_parent(const char *path)
{
    char *directory = g_path_get_dirname(path);
    if (g_mkdir_with_parents(directory, 0755)) die("mkdir failed");
    g_free(directory);
}

static void write_elf(const char *path, const char *interpreter,
                      const char *needed, const char *runpath)
{
    unsigned char image[IMAGE_SIZE] = {0};
    Elf64_Ehdr *header = (void *)image;
    memcpy(header->e_ident, ELFMAG, SELFMAG);
    header->e_ident[EI_CLASS] = ELFCLASS64;
    header->e_ident[EI_DATA] = ELFDATA2LSB;
    header->e_ident[EI_VERSION] = EV_CURRENT;
    header->e_type = ET_DYN;
    header->e_machine = EM_X86_64;
    header->e_version = EV_CURRENT;
    header->e_entry = LOAD_ADDRESS + (g_str_hash(path) & 0xff);
    header->e_ehsize = sizeof(*header);
    header->e_phoff = sizeof(*header);
    header->e_phentsize = sizeof(Elf64_Phdr);
    header->e_phnum = 1 + !!interpreter + !!needed;

    Elf64_Phdr *phdr = (void *)(image + header->e_phoff);
    phdr[0].p_type = PT_LOAD;
    phdr[0].p_offset = 0;
    phdr[0].p_vaddr = LOAD_ADDRESS;
    phdr[0].p_filesz = sizeof(image);
    phdr[0].p_memsz = sizeof(image);
    phdr[0].p_flags = PF_R | PF_X;
    phdr[0].p_align = 0x1000;
    unsigned int next = 1;
    if (interpreter) {
        size_t length = strlen(interpreter) + 1;
        memcpy(image + 0x200, interpreter, length);
        phdr[next].p_type = PT_INTERP;
        phdr[next].p_offset = 0x200;
        phdr[next].p_vaddr = LOAD_ADDRESS + 0x200;
        phdr[next].p_filesz = length;
        phdr[next].p_memsz = length;
        phdr[next].p_flags = PF_R;
        next++;
    }
    if (needed) {
        char *strings = (char *)image + 0x400;
        size_t needed_offset = 1;
        strcpy(strings + needed_offset, needed);
        size_t runpath_offset = needed_offset + strlen(needed) + 1;
        if (runpath) strcpy(strings + runpath_offset, runpath);
        size_t string_size = runpath_offset + (runpath ? strlen(runpath) + 1 : 0);
        Elf64_Dyn *dynamic = (void *)(image + 0x300);
        dynamic[0].d_tag = DT_STRTAB;
        dynamic[0].d_un.d_ptr = LOAD_ADDRESS + 0x400;
        dynamic[1].d_tag = DT_STRSZ;
        dynamic[1].d_un.d_val = string_size;
        dynamic[2].d_tag = DT_NEEDED;
        dynamic[2].d_un.d_val = needed_offset;
        unsigned int count = 3;
        if (runpath) {
            dynamic[count].d_tag = DT_RUNPATH;
            dynamic[count++].d_un.d_val = runpath_offset;
        }
        dynamic[count++].d_tag = DT_NULL;
        phdr[next].p_type = PT_DYNAMIC;
        phdr[next].p_offset = 0x300;
        phdr[next].p_vaddr = LOAD_ADDRESS + 0x300;
        phdr[next].p_filesz = count * sizeof(*dynamic);
        phdr[next].p_memsz = phdr[next].p_filesz;
        phdr[next].p_flags = PF_R;
    }
    make_parent(path);
    if (!g_file_set_contents(path, (const char *)image, sizeof(image), NULL)) {
        die("write ELF failed");
    }
}

static bool contains_guest(GPtrArray *dependencies, const char *path)
{
    for (guint i = 0; i < dependencies->len; i++) {
        LatcdPrecompileDependency *dependency =
            g_ptr_array_index(dependencies, i);
        if (!strcmp(dependency->guest_path, path)) return true;
    }
    return false;
}

int main(void)
{
    GError *gerror = NULL;
    char *root = g_dir_make_tmp("latcd-precompile-test-XXXXXX", &gerror);
    if (!root) die(gerror ? gerror->message : "temporary directory failed");
    char *main_path = g_build_filename(root, "usr/bin/app", NULL);
    char *loader_path = g_build_filename(root, "usr/lib/loader.so", NULL);
    char *dep_path = g_build_filename(root, "usr/lib/libdep.so", NULL);
    char *libc_path = g_build_filename(root, "usr/lib/libc.so.6", NULL);
    char *loader_link = g_build_filename(root, "lib64/ld.so", NULL);
    write_elf(main_path, "/lib64/ld.so", "libdep.so", "$ORIGIN/../lib");
    write_elf(loader_path, NULL, NULL, NULL);
    write_elf(dep_path, NULL, "libc.so.6", NULL);
    write_elf(libc_path, NULL, NULL, NULL);
    make_parent(loader_link);
    if (symlink("../usr/lib/loader.so", loader_link)) die("symlink failed");

    GPtrArray *dependencies = NULL;
    char error[512] = {0};
    if (latcd_precompile_collect(root, "/usr/bin/app", &dependencies,
                                 error, sizeof(error))) die(error);
    if (dependencies->len != 4 ||
        !contains_guest(dependencies, "/usr/bin/app") ||
        !contains_guest(dependencies, "/usr/lib/loader.so") ||
        !contains_guest(dependencies, "/usr/lib/libdep.so") ||
        !contains_guest(dependencies, "/usr/lib/libc.so.6")) {
        for (guint i = 0; i < dependencies->len; i++) {
            LatcdPrecompileDependency *dependency =
                g_ptr_array_index(dependencies, i);
            fprintf(stderr, "unexpected dependency: %s\n",
                    dependency->guest_path);
        }
        die("dependency closure mismatch");
    }
    g_ptr_array_free(dependencies, TRUE);
    if (unlink(libc_path)) die("unlink failed");
    dependencies = NULL;
    if (!latcd_precompile_collect(root, "/usr/bin/app", &dependencies,
                                  error, sizeof(error))) {
        die("missing recursive dependency was accepted");
    }

    unlink(loader_link);
    unlink(dep_path);
    unlink(loader_path);
    unlink(main_path);
    rmdir(g_build_filename(root, "lib64", NULL));
    rmdir(g_build_filename(root, "usr/bin", NULL));
    rmdir(g_build_filename(root, "usr/lib", NULL));
    rmdir(g_build_filename(root, "usr", NULL));
    rmdir(root);
    g_free(loader_link);
    g_free(libc_path);
    g_free(dep_path);
    g_free(loader_path);
    g_free(main_path);
    g_free(root);
    puts("test-precompile: ok");
    return 0;
}
