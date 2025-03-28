#ifndef __LIBRARY_H_
#define __LIBRARY_H_
#include <stdint.h>
#include "symbols.h"

typedef struct library_s       library_t;
typedef struct lib_s           lib_t;
typedef struct kh_symbolmap_s  kh_symbolmap_t;
typedef struct box64context_s  box64context_t;
typedef struct needed_libs_s   needed_libs_t;

#define LIB_WRAPPED     0
#define LIB_EMULATED    1
#define LIB_UNNKNOW     -1

char* Path2Name(const char* path);
int NbDot(const char* name);
void NativeLib_CommonInit(library_t *lib); 
void EmuLib_Fini(library_t* lib);
void NativeLib_FinishFini(library_t* lib);
int WrappedLib_defget(library_t* lib, const char* name, khint_t pre_k, uintptr_t *offs, uintptr_t *sz, int version, const char* vername, int local);
int EmuLib_Get(library_t* lib, const char* name, uintptr_t *offs, uintptr_t *sz, int version, const char* vername, int local);
int WrappedLib_defgetnoweak(library_t* lib, const char* name, khint_t pre_k, uintptr_t *offs, uintptr_t *sz, int version, const char* vername, int local);
int EmuLib_GetNoWeak(library_t* lib, const char* name, uintptr_t *offs, uintptr_t *sz, int version, const char* vername, int local);
int EmuLib_GetLocal(library_t* lib, const char* name, uintptr_t *offs, uintptr_t *sz, int version, const char* vername, int local);
int NativeLib_GetLocal(library_t* lib, const char* name, khint_t pre_k, uintptr_t *offs, uintptr_t *sz, int version, const char* vername, int local);

void FreeLoadedLibs(void);
GList* FindLoadedLibs(const char* path);
void AppendLoadedLibs(const char* path);
library_t *NewLibrary(const char* path, box64context_t* box64);
int ReloadLibrary(library_t* lib);
int FinalizeLibrary(library_t* lib, lib_t* local_maplib, int bindnow);
int FindLibIsWrapped(char * name);
int AddSymbolsLibrary(lib_t* maplib, library_t* lib);
void InactiveLibrary(library_t* lib);
void Free1Library(library_t **lib);

char* GetNameLib(library_t *lib);
int IsSameLib(library_t* lib, const char* path);    // check if lib is same (path -> name)
int GetLibSymbolStartEnd(library_t* lib, const char* name, khint_t pre_k, uintptr_t* start, uintptr_t* end, int version, const char* vername, int local);
int GetLibNoWeakSymbolStartEnd(library_t* lib, const char* name, khint_t pre_k, uintptr_t* start, uintptr_t* end, int version, const char* vername, int local);
int GetLibLocalSymbolStartEnd(library_t* lib, const char* name, khint_t pre_k, uintptr_t* start, uintptr_t* end, int version, const char* vername, int local);
void fillGLProcWrapper(void);
void freeGLProcWrapper(void);
void fillALProcWrapper(box64context_t* context);
void freeALProcWrapper(box64context_t* context);
needed_libs_t* GetNeededLibs(library_t* lib);
int GetNeededLibN(library_t* lib);
library_t* GetNeededLib(library_t* lib, int idx);
lib_t* GetMaplib(library_t* lib);

int GetElfIndex(library_t* lib);    // -1 if no elf (i.e. native)
void* GetHandle(library_t* lib);    // NULL if not native
int IsEmuLib(library_t* lib);

#endif //__LIBRARY_H_
