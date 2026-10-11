#ifndef __ELFLOADER_PRIVATE_H_
#define __ELFLOADER_PRIVATE_H_

typedef struct library_s library_t;
typedef struct needed_libs_s needed_libs_t;

#include "elf.h"
#include "elfloader.h"

#define LATX_ELF_TYPE_MMAP 1
#define LATX_ELF_TYPE_MAIN 2
#define LATX_ELF_TYPE_EMUED 3

struct elfheader_s {
    char*       name;
    char*       path;   // Resolved path to file
    size_t      numPHEntries;
    Elf64_Phdr  *PHEntries;
    size_t      numSHEntries;
    Elf64_Shdr  *SHEntries;
    size_t      SHIdx;
    size_t      numSST;
    char*       SHStrTab;
    char*       StrTab;
    Elf64_Sym*  SymTab;
    size_t      numSymTab;
    char*       DynStr;
    Elf64_Sym*  DynSym;
    size_t      numDynSym;
    Elf64_Dyn*  Dynamic;
    size_t      numDynamic;
    char*       DynStrTab;
    size_t      szDynStrTab;
    Elf64_Half* VerSym;
    Elf64_Verneed*  VerNeed;
    int         szVerNeed;
    Elf64_Verdef*   VerDef;
    int         szVerDef;
    int         e_type;

    intptr_t    delta;  // should be 0

    uintptr_t   entrypoint;
    uintptr_t   initentry;
    uintptr_t   initarray;
    size_t      initarray_sz;
    uintptr_t   finientry;
    uintptr_t   finiarray;
    size_t      finiarray_sz;

    uintptr_t   rel;
    size_t      relsz;
    int         relent;
    uintptr_t   rela;
    size_t      relasz;
    int         relaent;
    uintptr_t   jmprel;
    size_t      pltsz;
    int         pltent;
    uint64_t    pltrel;
    uintptr_t   gotplt;
    uintptr_t   gotplt_end;
    uintptr_t   pltgot;
    uintptr_t   got;
    uintptr_t   got_end;
    uintptr_t   plt;
    uintptr_t   plt_end;
    uintptr_t   text;
    size_t      textsz;
    uintptr_t   ehframe;
    uintptr_t   ehframe_end;
    uintptr_t   ehframehdr;

    uintptr_t   paddr;
    uintptr_t   vaddr;
    size_t      align;
    uint64_t    memsz;
    uint64_t    stacksz;
    size_t      stackalign;
    uintptr_t   tlsaddr;
    uint64_t    tlssize;
    uint64_t    tlsfilesize;
    size_t      tlsalign;

    int64_t     tlsbase;    // the base of the tlsdata in the global tlsdata (always negative)

    int         init_done;
    int         fini_done;

    char*       memory; // char* and not void* to allow math on memory pointer
    void**      multiblock;
    uintptr_t*  multiblock_offs;
    uint64_t*   multiblock_size;
    int         multiblock_n;

    library_t   *lib;
    needed_libs_t *neededlibs;
    uintptr_t self_link_map;
    FILE*       file;
    int         fileno;
    int         had_RelocateElfPlt;
    int         had_RelocateElf;
    int         latx_type;
    int         latx_hasfix;
};
int LoadSHNative(int fd, Elf64_Shdr *s, void** SH, const char* name, uint32_t type);
int LoadSH(FILE *f, Elf64_Shdr *s, void** SH, const char* name, uint32_t type);
int FindSection(Elf64_Shdr *s, int n, char* SHStrTab, const char* name);

void LoadNamedSectionNative(int fd, Elf64_Shdr *s, int size, char* SHStrTab, const char* name, const char* clearname, uint32_t type, void** what, size_t* num);
void LoadNamedSection(FILE *f, Elf64_Shdr *s, int size, char* SHStrTab, const char* name, const char* clearname, uint32_t type, void** what, size_t* num);
elfheader_t* ParseElfHeader(FILE* f, const char* name, int exec);

#endif //__ELFLOADER_PRIVATE_H_
