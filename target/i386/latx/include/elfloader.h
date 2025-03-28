#ifndef __ELF_LOADER_H_
#define __ELF_LOADER_H_
#include <stdio.h>
#include "elf.h"
extern uintptr_t pltResolver;
extern uintptr_t dl_runtime_resolver;
extern uintptr_t link_map_obj;
typedef struct elfheader_s elfheader_t;
typedef struct lib_s lib_t;
typedef struct library_s library_t;
typedef struct kh_mapsymbols_s kh_mapsymbols_t;
typedef struct box64context_s box64context_t;
typedef struct needed_libs_s needed_libs_t;

//#define LATX_RELOCATION_SAVE_SYMBOLS
void ResetSpecialCaseMainElf(elfheader_t* h);
void ResetSpecialCaseElf(elfheader_t* h, const char ** names, int nnames, void **rsymbol, int *nrsymbol);
elfheader_t* Init_Elfheader(void);
elfheader_t* LoadAndCheckElfHeader(FILE* f, const char* name, int exec); // exec : 0 = lib, 1 = exec
elfheader_t* LoadAndCheck_SO(int fd, const char* name, Elf64_Ehdr* header);
int getElfIndex(box64context_t* ctx, elfheader_t* head); 
void FreeElfHeader(elfheader_t** head);
const char* ElfName(elfheader_t* head);
const char* ElfPath(elfheader_t* head);
void ElfAttachLib(elfheader_t* head, library_t* lib);

// return 0 if OK
int isElfHasNeededVer(elfheader_t* head, const char* libname, elfheader_t* verneeded);
int CalcLoadAddr(elfheader_t* head);
int AllocLoadElfMemory(box64context_t* context, elfheader_t* head, int mainbin);
int RelocateElf(lib_t *maplib, lib_t *local_maplib, int bindnow, elfheader_t* head);
int RelocateElfPlt(lib_t *maplib, lib_t* local_maplib, int bindnow, elfheader_t* head);
void CalcStack(elfheader_t* h, uint64_t* stacksz, size_t* stackalign);
void AddSymbols(lib_t *maplib, kh_mapsymbols_t* mapsymbols, kh_mapsymbols_t* weaksymbols, kh_mapsymbols_t* localsymbols, elfheader_t* h);
int LoadNeededLibs(elfheader_t* h, lib_t *maplib, needed_libs_t* neededlibs, library_t *deplib, int local, int bindnow, box64context_t *box64, int init_main_elf);

uintptr_t GetElfInit(elfheader_t* h);
uintptr_t GetElfFini(elfheader_t* h);
void RunElfInit(elfheader_t* h);
void RunElfFini(elfheader_t* h);
void RunDeferedElfInit(void);
void* GetBaseAddress(elfheader_t* h);
void* GetElfDelta(elfheader_t* h);
uint32_t GetBaseSize(elfheader_t* h);
int IsAddressInElfSpace(const elfheader_t* h, uintptr_t addr);
elfheader_t* FindElfAddress(box64context_t *context, uintptr_t addr);
const char* FindNearestSymbolName(elfheader_t* h, void* p, uintptr_t* start, uint64_t* sz);
void* GetDynamicSection(elfheader_t* h);

const char* GetSymbolVersion(elfheader_t* h, int version);
const char* GetParentSymbolVersion(elfheader_t* h, int index);
const char* VersionnedName(const char* name, int ver, const char* vername);
int SameVersionnedSymbol(const char* name1, int ver1, const char* vername1, const char* name2, int ver2, const char* vername2);
int GetVersionIndice(elfheader_t* h, const char* vername);
int GetNeededVersionCnt(elfheader_t* h, const char* libname);
const char* GetNeededVersionString(elfheader_t* h, const char* libname, int idx);

void* GetNativeSymbolUnversionned(void* lib, const char* name);

void AddMainElfToLinkmap(elfheader_t* lib);
void PltResolver(void);

int RelocateElfRELA(lib_t *maplib, lib_t *local_maplib, int bindnow, elfheader_t* head, int cnt, Elf64_Rela *rela, int* need_resolv);
uintptr_t loadSoaddrFromMap(char * real_path);
void ElfHeadReFix (elfheader_t* head, uintptr_t delta);
int CheckEnableKZT(elfheader_t* h, char** target_argv, int target_argc);
#endif //__ELF_LOADER_H_
