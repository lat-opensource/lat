#ifndef __BOX64CONTEXT_H_
#define __BOX64CONTEXT_H_
#include <stdint.h>
#include "pathcoll.h"
#include "dictionnary.h"
#include <link.h>
#include "debug.h"

typedef struct elfheader_s elfheader_t;
typedef struct cleanup_s cleanup_t;
typedef struct lib_s lib_t;
typedef struct bridge_s bridge_t;
typedef struct kh_symbolmap_s kh_symbolmap_t;
typedef struct kh_symbol1map_s kh_symbol1map_t;
typedef struct library_s library_t;
typedef struct linkmap_s linkmap_t;
typedef struct kh_threadstack_s kh_threadstack_t;
typedef struct atfork_fnc_s {
    uintptr_t prepare;
    uintptr_t parent;
    uintptr_t child;
    void*     handle;
} atfork_fnc_t;
#define DYNAMAP_SHIFT 16
#define JMPTABL_SHIFT 16

typedef void* (*procaddess_t)(const char* name);
typedef void* (*vkprocaddess_t)(void* instance, const char* name);

#define MAX_SIGNAL 64

typedef struct needed_libs_s {
    int         cap;
    int         size;
    library_t   **libs;
} needed_libs_t;

void add_neededlib(needed_libs_t* needed, library_t* lib);
void free_neededlib(needed_libs_t* needed);
void add_dependedlib(needed_libs_t* depended, library_t* lib);
void free_dependedlib(needed_libs_t* depended);

typedef struct dlprivate_s {
    library_t   **libs;
    size_t      *count;
    size_t      *dlopened;
    struct link_map  *dlx86handle;
    size_t      lib_sz;
    size_t      lib_cap;
    char*       last_error;
    void *     x86dlopen;
    void *     x86dlclose;
    void *     x86dlsym;
    void *     x86dladdr1;
    void *     x86dladdr;
    void *     x86dlinfo;
} dlprivate_t;
struct latx_kzt_debug {
    char *name;
    int type;
    unsigned long map_start;
    unsigned long map_end;
};

enum r_dir_status { unknown, nonexisting, existing };
struct r_scope_elem
{
  /* Array of maps for the scope.  */
  struct link_map_x64 **r_list;
  /* Number of entries in the scope.  */
  unsigned int r_nlist;
};
struct r_search_path_elem
{
    /* This link is only used in the `all_dirs' member of `r_search_path'.  */
    struct r_search_path_elem *next;

    /* Strings saying where the definition came from.  */
    const char *what;
    const char *where;

    /* Basename for this search path element.  The string must end with
       a slash character.  */
    const char *dirname;
    size_t dirnamelen;

    enum r_dir_status status[0];
};

struct r_search_path_struct
{
    struct r_search_path_elem **dirs;
    int malloced;
};
typedef Elf64_Half Elf64_Versym;
typedef unsigned long dev_t;
struct r_file_id
{
    dev_t dev;
    ino64_t ino;
};
struct link_map_machine
{
    Elf64_Addr plt;
    Elf64_Addr gotplt;
    void *tlsdesc_table;
};
struct auditstate {
    uintptr_t cookie;
    unsigned int bindflags;
};

typedef long ptrdiff_t;

struct link_map_x64 {
    Elf64_Addr l_addr;
    char *l_name;
    Elf64_Dyn *l_ld;
    struct link_map_x64 *l_next;
    struct link_map_x64 *l_prev;
    struct link_map_x64 *l_real;
    Lmid_t l_ns;
    struct libname_list *l_libname;
    Elf64_Dyn *l_info[77];//get form gdb ptype    ---latx
    const Elf64_Phdr *l_phdr;
    Elf64_Addr l_entry;
    Elf64_Half l_phnum;
    Elf64_Half l_ldnum;
    struct r_scope_elem l_searchlist;
    struct r_scope_elem l_symbolic_searchlist;
    struct link_map_x64 *l_loader;
    struct r_found_version *l_versions;
    unsigned int l_nversions;
    Elf_Symndx l_nbuckets;
    Elf32_Word l_gnu_bitmask_idxbits;
    Elf32_Word l_gnu_shift;
    const Elf64_Addr *l_gnu_bitmask;
    union {
        const Elf32_Word *l_gnu_buckets;
        const Elf_Symndx *l_chain;
    };
    union {
        const Elf32_Word *l_gnu_chain_zero;
        const Elf_Symndx *l_buckets;
    };
    unsigned int l_direct_opencount;
    enum {lt_executable, lt_library, lt_loaded} l_type : 2;
    unsigned int l_relocated : 1;
    unsigned int l_init_called : 1;
    unsigned int l_global : 1;
    unsigned int l_reserved : 2;
    unsigned int l_phdr_allocated : 1;
    unsigned int l_soname_added : 1;
    unsigned int l_faked : 1;
    unsigned int l_need_tls_init : 1;
    unsigned int l_auditing : 1;
    unsigned int l_audit_any_plt : 1;
    unsigned int l_removed : 1;
    unsigned int l_contiguous : 1;
    unsigned int l_symbolic_in_local_scope : 1;
    unsigned int l_free_initfini : 1;
    enum {lc_unknown, lc_none, lc_ibt, lc_shstk = 4, lc_ibt_and_shstk = 6} l_cet : 3;
    struct r_search_path_struct l_rpath_dirs;
    struct reloc_result *l_reloc_result;
    Elf64_Versym *l_versyms;
    const char *l_origin;
    Elf64_Addr l_map_start;
    Elf64_Addr l_map_end;
    Elf64_Addr l_text_end;
    struct r_scope_elem *l_scope_mem[4];
    size_t l_scope_max;
    struct r_scope_elem **l_scope;
    struct r_scope_elem *l_local_scope[2];
    struct r_file_id l_file_id;
    struct r_search_path_struct l_runpath_dirs;
    struct link_map_x64 **l_initfini;
    struct link_map_reldeps *l_reldeps;
    unsigned int l_reldepsmax;
    unsigned int l_used;
    Elf64_Word l_feature_1;
    Elf64_Word l_flags_1;
    Elf64_Word l_flags;
    int l_idx;
    struct link_map_machine l_mach;
    struct {
        const Elf64_Sym *sym;
        int type_class;
        struct link_map *value;
        const Elf64_Sym *ret;
    } l_lookup_cache;
    void *l_tls_initimage;
    size_t l_tls_initimage_size;
    size_t l_tls_blocksize;
    size_t l_tls_align;
    size_t l_tls_firstbyte_offset;
    ptrdiff_t l_tls_offset;
    size_t l_tls_modid;
    size_t l_tls_dtor_count;
    Elf64_Addr l_relro_addr;
    size_t l_relro_size;
    unsigned long long l_serial;
    struct auditstate l_audit[];
};
struct malloc_map {
    void* mallocp;
    void* freep;
    void* reallocp;
    void* h;
};
typedef struct box64context_s {
    path_collection_t   box64_path;     // PATH env. variable
    path_collection_t   box64_ld_lib;   // LD_LIBRARY_PATH env. variable

    path_collection_t   box64_emulated_libs;    // Collection of libs that should not be wrapped

    int                 x64trace;
    int                 trace_tid;

    uint32_t            sel_serial;     // will be increment each time selectors changes

    void*               box64lib;       // dlopen on box64 itself

    int                 argc;
    char**              argv;

    int                 envc;
    char**              envv;

    char*               fullpath;
    char*               box64path;      // path of current box64 executable
    char*               box86path;      // path of box86 executable (if present)
    char*               bashpath;       // path of x86_64 bash (defined with BOX64_BASH or by running bash directly)

    uint64_t            stacksz;
    size_t              stackalign;
    void*               stack;          // alocated stack

    elfheader_t         **elfs;         // elf headers and memory
    int                 elfcap;
    int                 elfsize;        // number of elf loaded

    struct malloc_map          **mallocmaps;         // elf filepath and memory
    int                 mallocmapcap;
    int                 mallocmapsize;        // number of elf filepath

    needed_libs_t       neededlibs;     // needed libs for main elf

    uintptr_t           ep;             // entry point

    lib_t               *maplib;        // lib and symbols handling
    lib_t               *local_maplib;  // libs and symbols openned has local (only collection of libs, no symbols)
    dic_t               *versym;        // dictionnary of versionned symbols

    kh_threadstack_t    *stacksizes;    // stack sizes attributes for thread (temporary)
    bridge_t            *system;        // other bridges
    uintptr_t           vsyscall;       // vsyscall bridge value
    uintptr_t           vsyscalls[3];   // the 3 x86 VSyscall pseudo bridges (mapped at 0xffffffffff600000+)
    dlprivate_t         *dlprivate;     // dlopen library map
    kh_symbolmap_t      *glwrappers;    // the map of wrapper for glProcs (for GLX or SDL1/2)
    kh_symbolmap_t      *glmymap;       // link to the mysymbolmap of libGL
    procaddess_t        glxprocaddress;
    kh_symbolmap_t      *alwrappers;    // the map of wrapper for alGetProcAddress
    kh_symbolmap_t      *almymap;       // link to the mysymbolmap if libOpenAL
    kh_symbol1map_t      *vkwrappers;    // the map of wrapper for VulkanProcs (TODO: check SDL2)
    kh_symbol1map_t      *vkmymap;       // link to the mysymbolmap of libGL
    vkprocaddess_t      vkprocaddress;
    #ifndef DYNAREC
    pthread_mutex_t     mutex_lock;     // dynarec build will use their own mecanism
    pthread_mutex_t     mutex_trace;
    pthread_mutex_t     mutex_tls;
    pthread_mutex_t     mutex_thread;
    pthread_mutex_t     mutex_bridge;
    #else
    #ifdef USE_CUSTOM_MUTEX
    uint32_t            mutex_dyndump;
    uint32_t            mutex_trace;
    uint32_t            mutex_tls;
    uint32_t            mutex_thread;
    uint32_t            mutex_bridge;
    #else
    pthread_mutex_t     mutex_dyndump;
    pthread_mutex_t     mutex_trace;
    pthread_mutex_t     mutex_tls;
    pthread_mutex_t     mutex_thread;
    pthread_mutex_t     mutex_bridge;
    #endif
    uintptr_t           max_db_size;    // the biggest (in x86_64 instructions bytes) built dynablock
    int                 trace_dynarec;
    pthread_mutex_t     mutex_lock;     // this is for the Test interpreter
    #ifdef __riscv
    uint32_t            mutex_16b;
    #endif
    #endif

    library_t           *libclib;       // shortcut to libc library (if loaded, so probably yes)
    library_t           *sdl1lib;       // shortcut to SDL1 library (if loaded)
    void*               sdl1allocrw;
    void*               sdl1freerw;
    library_t           *sdl1mixerlib;
    library_t           *sdl2lib;       // shortcut to SDL2 library (if loaded)
    void*               sdl2allocrw;
    void*               sdl2freerw;
    library_t           *sdl2mixerlib;
    library_t           *x11lib;
    library_t           *zlib;
    library_t           *vorbisfile;
    library_t           *vorbis;
    library_t           *asound;
    library_t           *pulse;
    library_t           *d3dadapter9;
    library_t           *libglu;
    linkmap_t           *linkmap;

    int                 deferedInit;
    elfheader_t         **deferedInitList;
    int                 deferedInitSz;
    int                 deferedInitCap;

    uintptr_t           *auxval_start;

    cleanup_t   *cleanups;          // atexit functions
    int         clean_sz;
    int         clean_cap;


    int                 forked;         //  how many forks... cleanup only when < 0

    atfork_fnc_t        *atforks;       // fnc for atfork...
    int                 atfork_sz;
    int                 atfork_cap;

    uint8_t             canary[8];

    uintptr_t           signals[MAX_SIGNAL];
    uintptr_t           restorer[MAX_SIGNAL];
    int                 onstack[MAX_SIGNAL];
    int                 is_sigaction[MAX_SIGNAL];
    int                 no_sigsegv;
    int                 no_sigill;
    void*               stack_clone;
    int                 stack_clone_used;

    int                 current_line;
#ifdef CONFIG_LATX_DEBUG
    struct latx_kzt_debug **latx_kzt_debugs;
    int                 latx_kzt_debugcap;
    int                 latx_kzt_debugsize;        // number of latx_kzt_debug
#endif
} box64context_t;

extern box64context_t *my_context; // global context

box64context_t *NewBox64Context(int argc);
void FreeBox64Context(box64context_t** context);

// return the index of the added header
int AddElfHeader(box64context_t* ctx, elfheader_t* head);
int AddMallocMap(box64context_t* ctx, struct malloc_map* map);
struct malloc_map * SearchMallocMap(box64context_t* ctx, char *elfname);
#if defined(CONFIG_LATX_KZT) && defined(CONFIG_LATX_DEBUG)
int AddKztDebugInfo(box64context_t* ctx, struct latx_kzt_debug* debuginfo);
#endif
#endif //__BOX64CONTEXT_H_
