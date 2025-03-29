#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "elf.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <link.h>
#include <unistd.h>
#include <errno.h>

#include "elfloader.h"
#include "debug.h"
#include "elfload_dump.h"
#include "elfloader_private.h"
#include "librarian.h"
#include "bridge.h"
#include "wrapper.h"
#include "box64context.h"
#include "library.h"
#include "library_private.h"
#include "dictionnary.h"
#include "symbols.h"
#include "lsenv.h"

void* my__IO_2_1_stderr_ = NULL;
void* my__IO_2_1_stdin_  = NULL;
void* my__IO_2_1_stdout_ = NULL;
void ResetSpecialCaseMainElf(elfheader_t* h)
{
    Elf64_Sym *sym = NULL;
    for (size_t i=0; i<h->numDynSym; ++i) {
        if(h->DynSym[i].st_info == 17) {
            sym = h->DynSym+i;
            const char * symname = h->DynStr+sym->st_name;
            if(strcmp(symname, "_IO_2_1_stderr_")==0 && ((void*)sym->st_value+h->delta)) {
                memcpy((void*)sym->st_value+h->delta, stderr, sym->st_size);
                my__IO_2_1_stderr_ = (void*)sym->st_value+h->delta;
                printf_log(LOG_DEBUG, "BOX64: Set @_IO_2_1_stderr_ to %p\n", my__IO_2_1_stderr_);
            } else
            if(strcmp(symname, "_IO_2_1_stdin_")==0 && ((void*)sym->st_value+h->delta)) {
                memcpy((void*)sym->st_value+h->delta, stdin, sym->st_size);
                my__IO_2_1_stdin_ = (void*)sym->st_value+h->delta;
                printf_log(LOG_DEBUG, "BOX64: Set @_IO_2_1_stdin_ to %p\n", my__IO_2_1_stdin_);
            } else
            if(strcmp(symname, "_IO_2_1_stdout_")==0 && ((void*)sym->st_value+h->delta)) {
                memcpy((void*)sym->st_value+h->delta, stdout, sym->st_size);
                my__IO_2_1_stdout_ = (void*)sym->st_value+h->delta;
                printf_log(LOG_DEBUG, "BOX64: Set @_IO_2_1_stdout_ to %p\n", my__IO_2_1_stdout_);
            } else
            if(strcmp(symname, "_IO_stderr_")==0 && ((void*)sym->st_value+h->delta)) {
                memcpy((void*)sym->st_value+h->delta, stderr, sym->st_size);
                my__IO_2_1_stderr_ = (void*)sym->st_value+h->delta;
                printf_log(LOG_DEBUG, "BOX64: Set @_IO_stderr_ to %p\n", my__IO_2_1_stderr_);
            } else
            if(strcmp(symname, "_IO_stdin_")==0 && ((void*)sym->st_value+h->delta)) {
                memcpy((void*)sym->st_value+h->delta, stdin, sym->st_size);
                my__IO_2_1_stdin_ = (void*)sym->st_value+h->delta;
                printf_log(LOG_DEBUG, "BOX64: Set @_IO_stdin_ to %p\n", my__IO_2_1_stdin_);
            } else
            if(strcmp(symname, "_IO_stdout_")==0 && ((void*)sym->st_value+h->delta)) {
                memcpy((void*)sym->st_value+h->delta, stdout, sym->st_size);
                my__IO_2_1_stdout_ = (void*)sym->st_value+h->delta;
                printf_log(LOG_DEBUG, "BOX64: Set @_IO_stdout_ to %p\n", my__IO_2_1_stdout_);
            }
        }
    }
}
void ResetSpecialCaseElf(elfheader_t* h, const char ** names, int nnames, void **rsymbol, int *nrsymbol)
{
    Elf64_Sym *sym = NULL;
    for (size_t i=0; i<h->numDynSym; ++i) {
        sym = h->DynSym+i;
        const char * symname = h->DynStr+sym->st_name;
        for (int ii = 0; ii < nnames; ii++) {
            if(!rsymbol[ii] && strlen(symname) && !strcmp(symname, names[ii]) && ((void*)sym->st_value+h->delta)) {
                rsymbol[ii] = (void*)sym->st_value+h->delta;
                (*nrsymbol)++;
                printf_dlsym(LOG_DEBUG, "BOX64: Set @%s to %p %d\n", names[ii], rsymbol[ii], h->DynSym[i].st_info);
            }
        }
        //printf_dlsym(LOG_VERBOSE, "debug %s addr %p %d\n", symname, (void*)sym->st_value+h->delta, h->DynSym[i].st_info);
    }
}
// return the index of header (-1 if it doesn't exist)
int getElfIndex(box64context_t* ctx, elfheader_t* head) {
    for (int i=0; i<ctx->elfsize; ++i)
        if(ctx->elfs[i]==head)
            return i;
    return -1;
}

elfheader_t* Init_Elfheader(void)
{
    elfheader_t *h = box_calloc(1, sizeof(elfheader_t));
    return h;
}
elfheader_t* LoadAndCheckElfHeader(FILE* f, const char* name, int exec)
{
    elfheader_t *h = ParseElfHeader(f, name, exec);
    if(!h)
        return NULL;

    if ((h->path = realpath(name, NULL)) == NULL) {
        h->path = (char*)box_malloc(1);
        h->path[0] = '\0';
    }

    h->file = f;
    h->fileno = fileno(f);
    return h;
}

elfheader_t* ParseElfHeader_SO(int fd, const char* name, struct elf64_hdr* header);
elfheader_t* LoadAndCheck_SO(int fd, const char* name, Elf64_Ehdr* header)
{
    elfheader_t *h = ParseElfHeader_SO(fd, name, header);
    if(!h)
        return NULL;

    if ((h->path = realpath(name, NULL)) == NULL) {
        h->path = (char*)box_malloc(1);
        h->path[0] = '\0';
    }
    return h;
}

#if 1
uintptr_t loadSoaddrFromMap(char * real_path)
{
    char buf[PATH_MAX];
    FILE *f = fopen("/proc/self/maps", "r");
    uintptr_t s = 0, e = 0, offset = 0;
    if(!f)
        return 0;
    while(!feof(f)) {
        char* ret = fgets(buf, sizeof(buf), f);
        (void)ret;
        if (strstr(buf, real_path)) {
            if (sscanf(buf, "%lx-%lx %*4s %lx ", &s, &e, &offset) == 3) {
                break;
            } else {
                lsassert(0);
            }
        }
    }
    fclose(f);
    return s - offset;
}
#endif
void ElfHeadReFix (elfheader_t* head, uintptr_t delta)
{
    head->latx_hasfix = 1;
    lsassert(delta);
    head->delta = delta;
    #define GO(a) if ((uintptr_t)head->a > 0) {   \
            if ((uintptr_t)head->a > (uintptr_t)head->delta) {   \
                head->delta = 0;                           \
            }                                             \
            return;                                       \
    };
    GO(VerSym)
    GO(DynStrTab)   
    GO(VerNeed)
    GO(entrypoint)
    GO(initentry)
    GO(initarray)
    GO(finientry)
    GO(finiarray)
    GO(rela)
    GO(jmprel)
    GO(gotplt_end)
    GO(pltgot)
    GO(got)
    GO(got_end)
    GO(plt)
    GO(plt_end)
    GO(text)
    GO(ehframe)
    GO(ehframe_end)
    GO(ehframehdr)
    #undef GO
}

extern struct elfheader_s * elf_header;

void FreeElfHeader(elfheader_t** head)
{
    if(!head || !*head)
        return;
    elfheader_t *h = *head;
    box_free(h->name);
    box_free(h->path);
    box_free(h->PHEntries);
    box_free(h->SHEntries);
    box_free(h->SHStrTab);
    box_free(h->StrTab);
    box_free(h->Dynamic);
    box_free(h->DynStr);
    box_free(h->SymTab);
    box_free(h->DynSym);

    box_free(h);

    *head = NULL;
}
#include "fileutils.h"
#define GO(P, N) P,
#define GOALIAS(P, N) P,
const char * wrappedlibs_name[] = {
#include "library_list.h"
};
#undef GO
#undef GOALIAS
static int qsort_strcmp(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static char* GenPathList(char * rpath, const char* origin)
{
    char *rpathref = rpath;
    while (strstr(rpath, "$ORIGIN")) {
        char* p = strrchr(origin, '/');
        if (p) *p = '\0';    // remove file name to have only full path, without last '/'
        char* tmp = (char*)box_calloc(1, strlen(rpath)-strlen("$ORIGIN")+strlen(origin)+1);
        p = strstr(rpath, "$ORIGIN");
        memcpy(tmp, rpath, p-rpath);
        strcat(tmp, origin);
        strcat(tmp, p+strlen("$ORIGIN"));
        if (rpath!=rpathref)
            box_free(rpath);
        rpath = tmp;
    }
    while (strstr(rpath, "${ORIGIN}")) {
        char* p = strrchr(origin, '/');
        if (p) *p = '\0';    // remove file name to have only full path, without last '/'
        char* tmp = (char*)box_calloc(1, strlen(rpath)-strlen("${ORIGIN}")+strlen(origin)+1);
        p = strstr(rpath, "${ORIGIN}");
        memcpy(tmp, rpath, p-rpath);
        strcat(tmp, origin);
        strcat(tmp, p+strlen("${ORIGIN}"));
        if (rpath!=rpathref)
            box_free(rpath);
        rpath = tmp;
    }
    while (strstr(rpath, "${PLATFORM}")) {
        char* platform = box_strdup("x86_64");
        char* p = strrchr(platform, '/');
        if (p) *p = '\0';    // remove file name to have only full path, without last '/'
        char* tmp = (char*)box_calloc(1, strlen(rpath)-strlen("${PLATFORM}")+strlen(platform)+1);
        p = strstr(rpath, "${PLATFORM}");
        memcpy(tmp, rpath, p-rpath);
        strcat(tmp, platform);
        strcat(tmp, p+strlen("${PLATFORM}"));
        if (rpath!=rpathref)
            box_free(rpath);
        rpath = tmp;
        box_free(platform);
    }
    if (strchr(rpath, '$')) {
        printf_log(LOG_INFO, "BOX64: Warning, RPATH with $ variable not supported yet (%s)\n", rpath);
    }
    printf_log(LOG_INFO, "Generated: %s\n", rpath);
    return rpath;
}

static char* find_so_from_path(char* filename, path_collection_t* lib_path,
                               int *so_index)
{
    char realpath[PATH_MAX];
    int j = 0;
    for (; (*so_index) < lib_path->size; ++(*so_index)) {
        j = (*so_index);
        snprintf(realpath, PATH_MAX, "%s/%s", lib_path->paths[j], filename);
        if(FileExist((const char*)realpath, IS_FILE)) {
            char *retpath = box_malloc(strlen(realpath) + 1);
            strcpy(retpath, realpath);
            return retpath;
        }
    }
    return NULL;
}
//elf file only has DT_RPATH or DT_RPATH.
static char* find_elf_rpath(char* rfilename, bool* elf_err)
{
    int fd = open(rfilename, O_RDONLY, 0);
    FILE *f = fdopen(fd, "rb");
    char *rpath;
    struct stat statbuf;

    if(!f) {
        printf_log(LOG_INFO, "%s Error: Cannot open \"%s\"\n", __func__, rfilename);
        return NULL;
    }
    if (stat(rfilename, &statbuf)) {
        printf_log(LOG_INFO, "%s: Error stat %s failed\n", __func__, rfilename);
        return NULL;
    }
    lsassert(statbuf.st_size);
    elfheader_t* h = LoadAndCheckElfHeader(f, rfilename, 0);
    if (h == NULL) {
        *elf_err = true;
        return NULL;
    }
    for (size_t i=0; i<h->numDynamic; ++i) {
        switch(h->Dynamic[i].d_tag) {
            case DT_RPATH:
            case DT_RUNPATH:
                lsassert(h->DynStr);
                rpath = h->DynStr + h->Dynamic[i].d_un.d_val;
                printf_log(LOG_INFO, "RPATH : %s\n", rpath);
                lsassert(h->path);
                char * tmporg = strdup(h->path);
                char * retpath = GenPathList(rpath, tmporg);
                if (retpath == rpath) {
                    retpath = strdup(retpath);
                }
                free(tmporg);
                FreeElfHeader(&h);
                fclose(f);
                return retpath;
        }
    }
    FreeElfHeader(&h);
    fclose(f);
    return NULL;
}
static void for_needed_check(elfheader_t* h, int needlibcnt, path_collection_t *lib_path)
{
    //for needed
    const char* rpaths[needlibcnt];
    const char* rpaths_filepath[needlibcnt];
    int j=0;
    for (int i=0; i<h->numDynamic; ++i) {
        if(h->Dynamic[i].d_tag==DT_NEEDED) {
            if (strstr(h->DynStrTab+h->delta+h->Dynamic[i].d_un.d_val, "ld-linux-x86-64.so.2")) {
                continue;
            }
            // LD_LIBRARY_PATH may have multiple fitable paths
            int so_index =0;
            while (so_index < lib_path->size) {
                char* tmp = find_so_from_path(
                    h->DynStrTab + h->delta + h->Dynamic[i].d_un.d_val,
                    lib_path, &so_index);

                if (tmp) {
                    bool elf_err = false;
                    const char* result_tmp = find_elf_rpath(tmp, &elf_err);
                    if (!elf_err) {
                        rpaths_filepath[j] = tmp;
                        rpaths[j] = result_tmp;
                        j++;
                        break;
                    }
                }
                so_index++;
            }
        }
    }
    path_collection_t   tmp_path = {0,0,0};
    for (int i = 0;i < j; i++) {
        if(rpaths[i] && strlen(rpaths[i])) {
            rpaths[i] = GenPathList((char *)rpaths[i], rpaths_filepath[i]);
            AppendListExistAndNotSys(&tmp_path, rpaths[i], 1);
        }
    }
    for (int i = 0;i < j; i++) {
        if (rpaths[i]) {
            box_free((char*)rpaths[i]);
        }
        box_free((char*)rpaths_filepath[i]);
    }
    qsort(tmp_path.paths, tmp_path.size, sizeof(char *), qsort_strcmp);
    if (tmp_path.size >= 2) {
        char * hadinsert = NULL;
        for (int i = 0; i< tmp_path.size;i++) {
            if (strlen(tmp_path.paths[i]) && (!hadinsert||strcmp(tmp_path.paths[i], hadinsert))) {
                int not_found = 0;
                for(int ii = 0; ii < lib_path->size; ii++) {
                    if (!strcmp(lib_path->paths[ii], tmp_path.paths[i])) {
                        not_found = 1;
                        break;
                    }
                }
                if (!not_found) {
                    AddPath(tmp_path.paths[i], lib_path, 1);
                    //fprintf(stderr, "latx debug add %s\n", tmp_path.paths[i]);
                }
                hadinsert = tmp_path.paths[i];
            }
        }
    }
    //free tmp_path
    FreeCollection(&tmp_path);
}

static int isChromeApp(elfheader_t* h, int con_score)
{
    Elf64_Sym *sym = NULL;
    char symnamebuf[1024];
    int find_key_score = 0;
#define KEY_SPLIT " "
#define KEY_LEVEL(level) KEY_SPLIT#level

    const char * chromekeys =
                                                           KEY_SPLIT
        "_ZdaPv"                                           KEY_LEVEL(1)KEY_SPLIT
        "ChromeMain"                                       KEY_LEVEL(10)KEY_SPLIT
        "_ZdaPvm"                                          KEY_LEVEL(1)KEY_SPLIT
        "_ZdaPvmSt11align_val_t"                           KEY_LEVEL(1)KEY_SPLIT
        "_ZdaPvRKSt9nothrow_t"                             KEY_LEVEL(1)KEY_SPLIT
        "_ZdaPvSt11align_val_t"                            KEY_LEVEL(1)KEY_SPLIT
        "_ZdaPvSt11align_val_tRKSt"                        KEY_LEVEL(1)KEY_SPLIT
        "_ZdlPv"                                           KEY_LEVEL(1)KEY_SPLIT
        "_ZdlPvm"                                          KEY_LEVEL(1)KEY_SPLIT
        "_ZdlPvmSt11align_val_t"                           KEY_LEVEL(1)KEY_SPLIT
        "_ZdlPvRKSt9nothrow_t"                             KEY_LEVEL(1)KEY_SPLIT
        "_ZdlPvSt11align_val_t"                            KEY_LEVEL(1)KEY_SPLIT
        "_ZdlPvSt11align_val_tRKSt"                        KEY_LEVEL(1)KEY_SPLIT
        "_Znam"                                            KEY_LEVEL(1)KEY_SPLIT
        "_ZnamRKSt9nothrow_t"                              KEY_LEVEL(1)KEY_SPLIT
        "_ZnamSt11align_val_t"                             KEY_LEVEL(1)KEY_SPLIT
        "_ZnamSt11align_val_tRKSt9"                        KEY_LEVEL(1)KEY_SPLIT
        "_Znwm"                                            KEY_LEVEL(1)KEY_SPLIT
        "_ZnwmRKSt9nothrow_t"                              KEY_LEVEL(1)KEY_SPLIT
        "_ZnwmSt11align_val_t"                             KEY_LEVEL(1)KEY_SPLIT
        "_ZnwmSt11align_val_tRKSt9"                        KEY_LEVEL(1)KEY_SPLIT;
    for (size_t i=0; i<h->numDynSym; ++i) {
        sym = h->DynSym+i;
        if (h->DynSym[i].st_shndx != SHN_UNDEF && sym->st_value) {
            int score = 0;
            char *find_offset;
            const char * symname = h->DynStr+sym->st_name;
            snprintf(symnamebuf, 1023, KEY_SPLIT"%s"KEY_SPLIT, symname);
            find_offset = strstr(chromekeys, symnamebuf);
            if (find_offset) {
                snprintf(symnamebuf, 1023, KEY_SPLIT"%s"KEY_SPLIT"%%d"KEY_SPLIT, symname);
                if (sscanf(find_offset, symnamebuf, &score) == 1) {
                    find_key_score += score;
                    //if find_key_score >= con_score. exe is chrome app
                    if (find_key_score >= con_score) {
                        return 1;
                    }
                }
            }
        }
    }
#undef KEY_SPLIT
    return 0;
}

int CheckEnableKZT(elfheader_t* h, char** target_argv, int target_argc)
{
    path_collection_t   lib_path = {0,0,0};
    char *rpath;

    int needlibcnt = 0;
    /* get paths from RPATH/RUNPATH/LD_LIBRARY_PATH,
     * exclude system paths and non-exist paths */
    for (size_t i=0; i<h->numDynamic; ++i) {
        switch(h->Dynamic[i].d_tag) {
            case DT_RPATH:
            case DT_RUNPATH:
                rpath =  h->DynStrTab+h->Dynamic[i].d_un.d_val + h->delta;
                    printf_log(LOG_INFO, "RPATH : %s\n", rpath);
                rpath = GenPathList(rpath, h->path);
                AppendListExistAndNotSys(&lib_path, rpath, 1);
                break;
            case DT_NEEDED:
                if (strstr(h->DynStrTab+h->delta+h->Dynamic[i].d_un.d_val, "libgtk-3.so")||//skip gtk
                    strstr(h->DynStrTab+h->delta+h->Dynamic[i].d_un.d_val, "libXt.so")) {//skip mainexec needed libXt.so
                    printf_log(LOG_INFO, "latx find libgtk or libXt, skip kzt\n");
                    return 0;
                }
                ++needlibcnt;
                break;
        }
    }
    if (getenv("LD_LIBRARY_PATH"))
        AppendListExistAndNotSys(&lib_path, getenv("LD_LIBRARY_PATH"), 1);   // in case some of the path are for x86 world

    for (int i=0; i<lib_path.size; ++i)
        printf_log(LOG_INFO, "%s\n", lib_path.paths[i]);
    for_needed_check(h, needlibcnt, &lib_path);
    /* check KZT libs in paths */
    int nb = sizeof(wrappedlibs_name) / sizeof(char*);
    for (int i=0; i<nb; ++i) {
        char *p = box_strdup(wrappedlibs_name[i]);
        char *p2 = strchr(p, '.');
        if (++p2) {
            *p2 = '\0';
        }
        for (int j=0; j<lib_path.size; ++j) {
            DIR *dir = opendir(lib_path.paths[j]);
            if (dir == NULL) {
                printf_log(LOG_INFO, "dir %s not exist\n", lib_path.paths[j]);
                continue;
            }

            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_type == DT_REG) {  // Check if it's a regular file
                    if (!strncmp(entry->d_name, p, strlen(p))) {
                        printf_log(LOG_INFO, "File starting with '%s' found: %s at %s\n", p, entry->d_name, lib_path.paths[j]);
                        closedir(dir);
                        FreeCollection(&lib_path);
                        return 0;
                    }
                }
            }
            closedir(dir);
        }
    }
    FreeCollection(&lib_path);
    //disable kzt chrome app
    for(int i = 0; i < target_argc; i++) {
        if (!strcmp(target_argv[i], "--no-sandbox")) {
            return !isChromeApp(h, 5);
        }
    }
    return !isChromeApp(h, 10);
}

const char* ElfName(elfheader_t* head)
{
    if(!head)
        return "(noelf)";
    return head->name;
}
const char* ElfPath(elfheader_t* head)
{
    if(!head)
        return NULL;
    return head->path;
}

void ElfAttachLib(elfheader_t* head, library_t* lib)
{
    if(!head)
        return;
    head->lib = lib;
}

int GetVersionIndice(elfheader_t* h, const char* vername)
{
    if(!vername)
        return 0;
    if(h->VerDef) {
        Elf64_Verdef *def = (Elf64_Verdef*)((uintptr_t)h->VerDef + h->delta);
        while(def) {
            Elf64_Verdaux *aux = (Elf64_Verdaux*)((uintptr_t)def + def->vd_aux);
            if(!strcmp(h->DynStr+aux->vda_name, vername))
                return def->vd_ndx;
            def = def->vd_next?((Elf64_Verdef*)((uintptr_t)def + def->vd_next)):NULL;
        }
    }
    return 0;
}

int GetNeededVersionCnt(elfheader_t* h, const char* libname)
{
    if(!libname)
        return 0;
    if(h->VerNeed) {
        Elf64_Verneed *ver = (Elf64_Verneed*)((uintptr_t)h->VerNeed + h->delta);
        while(ver) {
            char *filename = h->DynStr + ver->vn_file;
            if(!strcmp(filename, libname))
                return ver->vn_cnt;
            ver = ver->vn_next?((Elf64_Verneed*)((uintptr_t)ver + ver->vn_next)):NULL;
        }
    }
    return 0;
}

const char* GetNeededVersionString(elfheader_t* h, const char* libname, int idx)
{
    if(!libname)
        return 0;
    if(h->VerNeed) {
        Elf64_Verneed *ver = (Elf64_Verneed*)((uintptr_t)h->VerNeed + h->delta);
        while(ver) {
            char *filename = h->DynStr + ver->vn_file;
            Elf64_Vernaux *aux = (Elf64_Vernaux*)((uintptr_t)ver + ver->vn_aux);
            if(!strcmp(filename, libname)) {
                for(int j=0; j<ver->vn_cnt; ++j) {
                    if(j==idx)
                        return h->DynStr+aux->vna_name;
                    aux = (Elf64_Vernaux*)((uintptr_t)aux + aux->vna_next);
                }
                return NULL;    // idx out of bound, return NULL...
           }
            ver = ver->vn_next?((Elf64_Verneed*)((uintptr_t)ver + ver->vn_next)):NULL;
        }
    }
    return NULL;
}
/*
static int FindR64COPYRel(elfheader_t* h, const char* name, uintptr_t *offs, uint64_t** p, int version, const char* vername)
{
    if(!h)
        return 0;
    Elf64_Rela * rela = (Elf64_Rela *)(h->rela + h->delta);
    if(!h->rela)
        return 0;
    int cnt = h->relasz / h->relaent;
    for (int i=0; i<cnt; ++i) {
        int t = ELF64_R_TYPE(rela[i].r_info);
        Elf64_Sym *sym = &h->DynSym[ELF64_R_SYM(rela[i].r_info)];
        const char* symname = SymName(h, sym);
        if(t==R_X86_64_COPY && symname && !strcmp(symname, name)) {
            int version2 = h->VerSym?((Elf64_Half*)((uintptr_t)h->VerSym+h->delta))[ELF64_R_SYM(rela[i].r_info)]:-1;
            if(version2!=-1) version2 &= 0x7fff;
            if(version && !version2) version2=-1;   // match a versionned symbol against a global "local" symbol
            const char* vername2 = GetSymbolVersion(h, version2);
            if(SameVersionnedSymbol(name, version, vername, symname, version2, vername2)) {
                *offs = sym->st_value + h->delta;
                *p = (uint64_t*)(rela[i].r_offset + h->delta + rela[i].r_addend);
                return 1;
            }
        }
    }
    return 0;
}
*/

int RelocateElfRELA(lib_t *maplib, lib_t *local_maplib, int bindnow, elfheader_t* head, int cnt, Elf64_Rela *rela, int* need_resolv)
{
//    int ret_ok = 0;
    for (int i=0; i<cnt; ++i) {
        int t = ELF64_R_TYPE(rela[i].r_info);
        //we only process the type of R_X86_64_GLOB_DAT and R_X86_64_JUMP_SLO
	if(!(t == R_X86_64_GLOB_DAT || t == R_X86_64_JUMP_SLOT))
	    continue;

	Elf64_Sym *sym = &head->DynSym[ELF64_R_SYM(rela[i].r_info)];
        int bind = ELF64_ST_BIND(sym->st_info);
        const char* symname = SymName(head, sym);
        uint64_t *p = (uint64_t*)(rela[i].r_offset + head->delta);
        uintptr_t offs = 0;
        uintptr_t end = 0;
        int version = head->VerSym?((Elf64_Half*)((uintptr_t)head->VerSym+head->delta))[ELF64_R_SYM(rela[i].r_info)]:-1;
        if(version!=-1) version &=0x7fff;
        const char* vername = GetSymbolVersion(head, version);
        if(bind==STB_LOCAL) {
            offs = sym->st_value + head->delta;
            end = offs + sym->st_size;
        } else {
            // this is probably very very wrong. A proprer way to get reloc need to be writen, but this hack seems ok for now
            // at least it work for half-life, unreal, ut99, zsnes, Undertale, ColinMcRae Remake, FTL, ShovelKnight...
            /*if(bind==STB_GLOBAL && (ndx==10 || ndx==19) && t!=R_X86_64_GLOB_DAT) {
                offs = sym->st_value + head->delta;
                end = offs + sym->st_size;
            }*/
            // so weak symbol are the one left
            if(!offs && !end) {
                GetGlobalSymbolStartEnd(maplib, symname, &offs, &end, head, version, vername);
                if(!offs && !end && local_maplib) {
                    GetGlobalSymbolStartEnd(local_maplib, symname, &offs, &end, head, version, vername);
                }
            }
        }

        //uintptr_t globoffs=0, globend=0;
        uintptr_t tmp = 0;
        switch(t) {
             case R_X86_64_GLOB_DAT:
                    // Look for same symbol already loaded but not in self (so no need for local_maplib here)
                   // if (GetGlobalNoWeakSymbolStartEnd(local_maplib?local_maplib:maplib, symname, &globoffs, &globend, version, vername)) {
                   //     offs = globoffs;
                   //     end = globend;
                   // }
                    if (offs) {
                        printf_log(LOG_INFO, "Apply %s R_X86_64_GLOB_DAT @%p (%p -> %p) on sym=%s (ver=%d/%s)\n", (bind==STB_LOCAL)?"Local":"Global", p, (void*)(p?(*p):0), (void*)offs, symname, version, vername?vername:"(none)");
                        *p = offs/* + rela[i].r_addend*/;   // not addend it seems
                    }
                break;
            case R_X86_64_JUMP_SLOT:
                // apply immediatly for gobject closure marshal or for LOCAL binding. Also, apply immediatly if it doesn't jump in the got
                tmp = (uintptr_t)(*p);
                if (bind==STB_LOCAL 
                  || !tmp
                  || !((tmp>=head->plt && tmp<head->plt_end) || (tmp>=head->gotplt && tmp<head->gotplt_end))
                  || !need_resolv
                  || bindnow
                  ) {
                    if (offs){
                        if(p) {
                            printf_log(LOG_INFO, "RelocateElfRELA : Apply %s R_X86_64_JUMP_SLOT @%p with sym=%s (%p -> %p)\n", (bind==STB_LOCAL)?"Local":"Global", p, symname, *(void**)p, (void*)(offs+rela[i].r_addend));
                            *p =(uint64_t) (offs + rela[i].r_addend);
                        } else {
                            printf_log(LOG_INFO, "Warning, Symbol %s found, but Jump Slot Offset is NULL \n", symname);
                        }
                    }
                } else {
                    printf_log(LOG_INFO, "Preparing (if needed) %s R_X86_64_JUMP_SLOT @%p (0x%lx->0x%0lx) with sym=%s to be apply later (addend=%ld)\n", 
                        (bind==STB_LOCAL)?"Local":"Global", p, *p, *p+head->delta, symname, rela[i].r_addend);
                    *p += head->delta;
                    *need_resolv = 1;
                }
                break;
            /*
            case R_X86_64_NONE:
                break;
            case R_X86_64_PC32:
                break;
            case R_X86_64_RELATIVE:
                break;
            case R_X86_64_IRELATIVE:
                break;
            case R_X86_64_COPY:
                break;
            case R_X86_64_64:
                break;
            */
            default:
                printf_log(LOG_INFO, "RELA: Warning, don't know of to handle rela #%d %s on %s\n", i, DumpRelType(ELF64_R_TYPE(rela[i].r_info)), symname);
        }
    }
    return 0;
}

int RelocateElfPlt(lib_t *maplib, lib_t *local_maplib, int bindnow, elfheader_t* head)
{
    int need_resolver = 0;
    head->had_RelocateElfPlt = 1;
    if(head->pltrel) {
        int cnt = head->pltsz / head->pltent;
        if(head->pltrel==DT_REL) {
            //DumpRelTable(head, cnt, (Elf64_Rel *)(head->jmprel + head->delta), "PLT");
            //printf_log(LOG_INFO, "Applying %d PLT Relocation(s) for %s\n", cnt, head->name);
            //if(RelocateElfREL(maplib, local_maplib, bindnow, head, cnt, (Elf64_Rel *)(head->jmprel + head->delta)))
            //    return -1;
            return 0;
        } else if(head->pltrel==DT_RELA) {
            DumpRelATable(head, cnt, (Elf64_Rela *)(head->jmprel + head->delta), "PLT");
            printf_log(LOG_INFO, "Applying %d PLT Relocation(s) with Addend for %s\n", cnt, head->name);
            if(RelocateElfRELA(maplib, local_maplib, bindnow, head, cnt, (Elf64_Rela *)(head->jmprel + head->delta), &need_resolver))
                //return -1;
                printf_log(LOG_INFO, "RelocateElfRELA run ERROR!");
        }
        if(need_resolver) {
            if(pltResolver==~0LL) {
                pltResolver = AddBridge(my_context->system, vFE, PltResolver, 0, "PltResolver");
            }
            if(head->pltgot) {
                if(dl_runtime_resolver ==~0LL){
                    dl_runtime_resolver =  *(uintptr_t*)(head->pltgot+head->delta+16);
                }
                *(uintptr_t*)(head->pltgot+head->delta+16) = pltResolver;
                head->self_link_map = *(uintptr_t*)(head->pltgot+head->delta+8);
                *(uintptr_t*)(head->pltgot+head->delta+8) = (uintptr_t)head;
                printf_log(LOG_INFO, "PLT Resolver injected in plt.got at %p\n", (void*)(head->pltgot+head->delta+16));
            } else if(head->got) {
                if(dl_runtime_resolver ==~0LL){
                    dl_runtime_resolver =  *(uintptr_t*)(head->got+head->delta+16);
                }
                *(uintptr_t*)(head->got+head->delta+16) = pltResolver;
                head->self_link_map = *(uintptr_t*)(head->got+head->delta+8);
                *(uintptr_t*)(head->got+head->delta+8) = (uintptr_t)head;
                printf_log(LOG_INFO, "PLT Resolver injected in got at %p\n", (void*)(head->got+head->delta+16));
            }
        }
    }
   
    return 0;
}

#if 0
#include <link.h>
#ifdef DL_RO_DYN_SECTION
# define D_PTR(map, i) ((map)->i->d_un.d_ptr + (map)->l_addr)
#else
# define D_PTR(map, i) (map)->i->d_un.d_ptr
#endif

#define	DT_NUM		35		/* Number used */
#define DT_THISPROCNUM	0

#define	DT_VERNEEDNUM	0x6fffffff	/* Number of needed versions */
#define DT_VERSIONTAGIDX(tag)	(DT_VERNEEDNUM - (tag))	/* Reverse order! */
#define VERSYMIDX(sym)	(DT_NUM + DT_THISPROCNUM + DT_VERSIONTAGIDX (sym))

int GetElfHeadFromLinkmap (struct link_map *map, elfheader_t* h)
{
    h->jmprel = D_PTR(map,l_info[DT_JMPREL])
    h->pltrel = map.l_info[DT_PLTREL]->d_un.d_val
    h->pltsz  = map.l_info[DT_PLTRELSZ]->d_un.d_val
    h->rel = map.l_info[DT_REL].d_tag
    h->rela = map.l_info[DT_RELA].d_tag
    h->DynSym = D_PTR(map,l_info[DT_SYMTAB])
    h->DynStr = D_PTR(map,l_info[DT_STRTAB])
    h->VerNeed = D_PTR (map, l_info[VERSYMIDX (DT_VERNEED)])
    h->VerDef = D_PTR (map, l_info[VERSYMIDX (DT_VERDEF)])
    h->VerSym = D_PTR (map, l_info[VERSYMIDX (DT_VERSYM)])
    return 0;
}
#endif
int RelocateElf(lib_t *maplib, lib_t *local_maplib, int bindnow, elfheader_t* head)
{
    if(head->rel) {
//        int cnt = head->relsz / head->relent;
//        DumpRelTable(head, cnt, (Elf64_Rel *)(head->rel + head->delta), "Rel");
//        printf_dump(LOG_DEBUG, "Applying %d Relocation(s) for %s\n", cnt, head->name);
//        if(RelocateElfREL(maplib, local_maplib, bindnow, head, cnt, (Elf64_Rel *)(head->rel + head->delta)))
///            return -1;
    }
    if(head->rela) {
        int cnt = head->relasz / head->relaent;
        DumpRelATable(head, cnt, (Elf64_Rela *)(head->rela + head->delta), "RelA");
        printf_dump(LOG_DEBUG, "Applying %d Relocation(s) with Addend for %s\n", cnt, head->name);
        if(RelocateElfRELA(maplib, local_maplib, bindnow, head, cnt, (Elf64_Rela *)(head->rela + head->delta), NULL))
            return -1;
    }
    head->had_RelocateElf = 1;
    return 0;
}

void CalcStack(elfheader_t* elf, uint64_t* stacksz, size_t* stackalign)
{
    if(*stacksz < elf->stacksz)
        *stacksz = elf->stacksz;
    if(*stackalign < elf->stackalign)
        *stackalign = elf->stackalign;
}

void AddSymbols(lib_t *maplib, kh_mapsymbols_t* mapsymbols, kh_mapsymbols_t* weaksymbols, kh_mapsymbols_t* localsymbols, elfheader_t* h)
{
#if !defined(LATX_RELOCATION_SAVE_SYMBOLS)
    return;
#endif
    if(relocation_dump && h->DynSym) DumpDynSym(h);
    int libcef = (strstr(h->name, "libcef.so"))?1:0;
    //libcef.so is linked with tcmalloc staticaly, but this cannot be easily supported in box64, so hacking some "unlink" here
    const char* avoid_libcef[] = {"malloc", "realloc", "free", "calloc", "cfree",
        "__libc_malloc", "__libc_calloc", "__libc_free", "__libc_memallign", "__libc_pvalloc",
        "__libc_realloc", "__libc_valloc", "__posix_memalign",
        "valloc", "pvalloc", "posix_memalign", "malloc_stats", "malloc_usable_size",
        /*"mallopt",*/ "localtime_r",
        //c++ symbol from libstdc++ too
        //"_ZnwmRKSt9nothrow_t", "_ZdaPv",    // operator new(unsigned long, std::nothrow_t const&), operator delete[](void*)
        //"_Znwm", "_ZdlPv", "_Znam",         // operator new(unsigned long), operator delete(void*), operator new[](unsigned long)
        //"_ZnwmSt11align_val_t", "_ZnwmSt11align_val_tRKSt9nothrow_t",   // operator new(unsigned long, std::align_val_t)
        //"_ZnamSt11align_val_t", "_ZnamSt11align_val_tRKSt9nothrow_t",   // operator new[](unsigned long, std::align_val_t)
        //"_ZdlPvRKSt9nothrow_t", "_ZdaPvSt11align_val_tRKSt9nothrow_t",  // more delete operators
        //"_ZdlPvmSt11align_val_t", "_ZdaPvRKSt9nothrow_t",
        //"_ZdaPvSt11align_val_t", "_ZdlPvSt11align_val_t",
    };
    printf_log(LOG_INFO, "Will look for Symbol to add in SymTable(%zu)\n", h->numSymTab);
    for (size_t i=0; i<h->numSymTab; ++i) {
        const char * symname = h->StrTab+h->SymTab[i].st_name;
        int bind = ELF64_ST_BIND(h->SymTab[i].st_info);
        int type = ELF64_ST_TYPE(h->SymTab[i].st_info);
        int vis = h->SymTab[i].st_other&0x3;
        size_t sz = h->SymTab[i].st_size;
        if((type==STT_OBJECT || type==STT_FUNC || type==STT_COMMON || type==STT_TLS  || type==STT_NOTYPE) 
        && (vis==STV_DEFAULT || vis==STV_PROTECTED) && (h->SymTab[i].st_shndx!=0)) {
            if(sz && strstr(symname, "@@")) {
                char symnameversionned[strlen(symname)+1];
                strcpy(symnameversionned, symname);
                // extact symname@@vername
                char* p = strchr(symnameversionned, '@');
                *p=0;
                p+=2;
                symname = AddDictionnary(my_context->versym, symnameversionned);
                const char* vername = AddDictionnary(my_context->versym, p);
                if((bind==STB_GNU_UNIQUE /*|| (bind==STB_GLOBAL && type==STT_FUNC)*/) && FindGlobalSymbol(maplib, symname, 2, p))
                    continue;
                uintptr_t offs = (type==STT_TLS)?h->SymTab[i].st_value:(h->SymTab[i].st_value + h->delta);
                printf_log(LOG_INFO, "Adding Default Versionned Symbol(bind=%s) \"%s@%s\" with offset=%p sz=%zu\n", (bind==STB_LOCAL)?"LOCAL":((bind==STB_WEAK)?"WEAK":"GLOBAL"), symname, vername, (void*)offs, sz);
                if(bind==STB_LOCAL)
                    AddSymbol(localsymbols, symname, offs, sz, 2, vername);
                else    // add in local and global map 
                    if(bind==STB_WEAK) {
                        AddSymbol(weaksymbols, symname, offs, sz, 2, vername);
                    } else {
                        AddSymbol(mapsymbols, symname, offs, sz, 2, vername);
                    }
            } else {
                int to_add = 1;
                if(libcef) {
                    if(strstr(symname, "_Zn")==symname || strstr(symname, "_Zd")==symname)
                        to_add = 0;
                    for(int j=0; j<sizeof(avoid_libcef)/sizeof(avoid_libcef[0]) && to_add; ++j)
                        if(!strcmp(symname, avoid_libcef[j]))
                            to_add = 0;
                }
                if(!to_add || (bind==STB_GNU_UNIQUE && FindGlobalSymbol(maplib, symname, -1, NULL)))
                    continue;
                uintptr_t offs = (type==STT_TLS)?h->SymTab[i].st_value:(h->SymTab[i].st_value + h->delta);
                printf_log(LOG_INFO, "Adding Symbol(bind=%s,type=%s) \"%s\" with offset=%p sz=%zu\n", (bind==STB_LOCAL)?"LOCAL":((bind==STB_WEAK)?"WEAK":"GLOBAL"),(type==STT_OBJECT)?"OBJECT":((type==STT_FUNC)?"FUNC":"UNKWON"), symname, (void*)offs, sz);
                if(bind==STB_LOCAL)
                    AddSymbol(localsymbols, symname, offs, sz, 1, NULL);
                else    // add in local and global map 
                    if(bind==STB_WEAK) {
                        AddSymbol(weaksymbols, symname, offs, sz, 1, NULL);
                    } else {
                        AddSymbol(mapsymbols, symname, offs, sz, 1, NULL);
                    }
            }
        }
    }
    
    printf_log(LOG_INFO, "Will look for Symbol to add in DynSym (%zu)\n", h->numDynSym);
    for (size_t i=0; i<h->numDynSym; ++i) {
        const char * symname = h->DynStr+h->DynSym[i].st_name;
        int bind = ELF64_ST_BIND(h->DynSym[i].st_info);
        int type = ELF64_ST_TYPE(h->DynSym[i].st_info);
        int vis = h->DynSym[i].st_other&0x3;
        if((type==STT_OBJECT || type==STT_FUNC || type==STT_COMMON || type==STT_TLS  || type==STT_NOTYPE) 
        && (vis==STV_DEFAULT || vis==STV_PROTECTED) && (h->DynSym[i].st_shndx!=0 && h->DynSym[i].st_shndx<=65521)) {
            uintptr_t offs = (type==STT_TLS)?h->DynSym[i].st_value:(h->DynSym[i].st_value + h->delta);
            size_t sz = h->DynSym[i].st_size;
            int version = h->VerSym?((Elf64_Half*)((uintptr_t)h->VerSym+h->delta))[i]:-1;
            if(version!=-1) version &= 0x7fff;
            const char* vername = GetSymbolVersion(h, version);
            int to_add = 1;
            if(libcef) {
                if(strstr(symname, "_Zn")==symname || strstr(symname, "_Zd")==symname)
                    to_add = 0;
                for(int j=0; j<sizeof(avoid_libcef)/sizeof(avoid_libcef[0]) && to_add; ++j)
                    if(!strcmp(symname, avoid_libcef[j]))
                        to_add = 0;
            }
            if(!to_add || (bind==STB_GNU_UNIQUE && FindGlobalSymbol(maplib, symname, version, vername)))
                continue;
            printf_log(LOG_INFO, "Adding Versionned Symbol(bind=%s) \"%s\" (ver=%d/%s) with offset=%p sz=%zu\n", (bind==STB_LOCAL)?"LOCAL":((bind==STB_WEAK)?"WEAK":"GLOBAL"), symname, version, vername?vername:"(none)", (void*)offs, sz);
            if(bind==STB_LOCAL)
                AddSymbol(localsymbols, symname, offs, sz, version, vername);
            else // add in local and global map 
                if(bind==STB_WEAK) {
                    AddSymbol(weaksymbols, symname, offs, sz, version, vername);
                } else {
                    AddWeakSymbol(mapsymbols, symname, offs, sz, version?version:1, vername);
                }
        }
    }
    
}

/*
$ORIGIN – Provides the directory the object was loaded from. This token is typical
used for locating dependencies in unbundled packages. For more details of this
token expansion, see “Locating Associated Dependencies”
$OSNAME – Expands to the name of the operating system (see the uname(1) man
page description of the -s option). For more details of this token expansion, see
“System Specific Shared Objects”
$OSREL – Expands to the operating system release level (see the uname(1) man
page description of the -r option). For more details of this token expansion, see
“System Specific Shared Objects”
$PLATFORM – Expands to the processor type of the current machine (see the
uname(1) man page description of the -i option). For more details of this token
expansion, see “System Specific Shared Objects”
*/
extern const char *interp_prefix;
int LoadNeededLibs(elfheader_t* h, lib_t *maplib, needed_libs_t* neededlibs, library_t *deplib, int local, int bindnow, box64context_t *box64, int init_main_elf)
{
    if (!h->latx_hasfix) return 1;
    if(relocation_dump) DumpDynamicRPath(h);
    // update RPATH first
    for (size_t i=0; i<h->numDynamic; ++i)
        if(h->Dynamic[i].d_tag==DT_RPATH || h->Dynamic[i].d_tag==DT_RUNPATH) {
            char *rpathref = h->DynStrTab+h->delta+h->Dynamic[i].d_un.d_val;
            char* rpath = rpathref;
            while(strstr(rpath, "$ORIGIN")) {
                char* origin = box_strdup(h->path);
                char* p = strrchr(origin, '/');
                if(p) *p = '\0';    // remove file name to have only full path, without last '/'
                char* tmp = (char*)box_calloc(1, strlen(rpath)-strlen("$ORIGIN")+strlen(origin)+1);
                p = strstr(rpath, "$ORIGIN");
                memcpy(tmp, rpath, p-rpath);
                strcat(tmp, origin);
                strcat(tmp, p+strlen("$ORIGIN"));
                if(rpath!=rpathref)
                    box_free(rpath);
                rpath = tmp;
                box_free(origin);
            }
            while(strstr(rpath, "${ORIGIN}")) {
                char* origin = box_strdup(h->path);
                char* p = strrchr(origin, '/');
                if(p) *p = '\0';    // remove file name to have only full path, without last '/'
                char* tmp = (char*)box_calloc(1, strlen(rpath)-strlen("${ORIGIN}")+strlen(origin)+1);
                p = strstr(rpath, "${ORIGIN}");
                memcpy(tmp, rpath, p-rpath);
                strcat(tmp, origin);
                strcat(tmp, p+strlen("${ORIGIN}"));
                if(rpath!=rpathref)
                    box_free(rpath);
                rpath = tmp;
                box_free(origin);
            }
            while(strstr(rpath, "${PLATFORM}")) {
                char* platform = box_strdup("x86_64");
                char* p = strrchr(platform, '/');
                if(p) *p = '\0';    // remove file name to have only full path, without last '/'
                char* tmp = (char*)box_calloc(1, strlen(rpath)-strlen("${PLATFORM}")+strlen(platform)+1);
                p = strstr(rpath, "${PLATFORM}");
                memcpy(tmp, rpath, p-rpath);
                strcat(tmp, platform);
                strcat(tmp, p+strlen("${PLATFORM}"));
                if(rpath!=rpathref)
                    box_free(rpath);
                rpath = tmp;
                box_free(platform);
            }
            if(strchr(rpath, '$')) {
                printf_log(LOG_INFO, "BOX64: Warning, RPATH with $ variable not supported yet (%s)\n", rpath);
            } else {
                char latxpath [PATH_MAX] = {0};
                snprintf(latxpath , PATH_MAX, "%s%s:%s", strlen(interp_prefix)?interp_prefix:"/usr/gnemul/latx-x86_64", rpath,rpath);
                printf_log(LOG_INFO, "Prepending path \"%s\" to BOX64_LD_LIBRARY_PATH\n", latxpath);
                PrependList(&box64->box64_ld_lib, latxpath, 1);
            }
            if(rpath!=rpathref)
                box_free(rpath);
        }

    if(!h->neededlibs && neededlibs)
        h->neededlibs = neededlibs;

    DumpDynamicNeeded(h);
    int cnt = 0;
    for (int i=0; i<h->numDynamic; ++i)
        if(h->Dynamic[i].d_tag==DT_NEEDED)
            ++cnt;
    const char* nlibs[cnt];
    int j=0;
    for (int i=0; i<h->numDynamic; ++i)
        if(h->Dynamic[i].d_tag==DT_NEEDED) {
            if (strstr(h->DynStrTab+h->delta+h->Dynamic[i].d_un.d_val, "ld-linux-x86-64.so.2")) {
                cnt--;
                continue;
            }
            nlibs[j++] = h->DynStrTab+h->delta+h->Dynamic[i].d_un.d_val;
        }

    // TODO: Add LD_LIBRARY_PATH and RPATH handling
    if(AddNeededLib(maplib, neededlibs, deplib, local, bindnow, nlibs, cnt, box64, init_main_elf)) {
        printf_log(LOG_INFO, "Error loading one of needed lib\n");
        if(!allow_missing_libs)
            return 1;   //error...
    }
    return 0;
}

void RunElfInit(elfheader_t* h)
{
}

EXPORTDYN
void RunDeferedElfInit(void)
{
}

void RunElfFini(elfheader_t* h)
{
}

uintptr_t GetElfInit(elfheader_t* h)
{
    return h->initentry + h->delta;
}
uintptr_t GetElfFini(elfheader_t* h)
{
    return h->finientry + h->delta;
}

void* GetBaseAddress(elfheader_t* h)
{
    return h->memory;
}

void* GetElfDelta(elfheader_t* h)
{
    return (void*)h->delta;
}

uint32_t GetBaseSize(elfheader_t* h)
{
    return h->memsz;
}

int IsAddressInElfSpace(const elfheader_t* h, uintptr_t addr)
{
    if(!h)
        return 0;
    //Todo: multiblock_ need to be init
    for(int i=0; i<h->multiblock_n; ++i) {
        uintptr_t base = h->multiblock_offs[i];
        uintptr_t end = h->multiblock_offs[i] + h->multiblock_size[i] - 1;
        if(addr>=base && addr<=end)
            return 1;
    }
    return 0;
}

elfheader_t* FindElfAddress(box64context_t *context, uintptr_t addr)
{
    for (int i=0; i<context->elfsize; ++i)
        if(IsAddressInElfSpace(context->elfs[i], addr))
            return context->elfs[i];
    if(context->elfsize >0) return elf_header;  
    return NULL;
}

const char* FindNearestSymbolName(elfheader_t* h, void* p, uintptr_t* start, uint64_t* sz)
{
    uintptr_t addr = (uintptr_t)p;

    uint32_t distance = 0x7fffffff;
    const char* ret = NULL;
    uintptr_t s = 0;
    uint64_t size = 0;
    if(!h || h->fini_done)
        return ret;

    for (size_t i=0; i<h->numSymTab && distance!=0; ++i) {
        const char * symname = h->StrTab+h->SymTab[i].st_name;
        uintptr_t offs = h->SymTab[i].st_value + h->delta;

        if(offs<=addr) {
            if(distance>addr-offs) {
                distance = addr-offs;
                ret = symname;
                s = offs;
                size = h->SymTab[i].st_size;
            }
        }
    }
    for (size_t i=0; i<h->numDynSym && distance!=0; ++i) {
        const char * symname = h->DynStr+h->DynSym[i].st_name;
        uintptr_t offs = h->DynSym[i].st_value + h->delta;

        if(offs<=addr) {
            if(distance>addr-offs) {
                distance = addr-offs;
                ret = symname;
                s = offs;
                size = h->DynSym[i].st_size;
            }
        }
    }

    if(start)
        *start = s;
    if(sz)
        *sz = size;

    return ret;
}

const char* VersionnedName(const char* name, int ver, const char* vername)
{
    if(ver==-1)
        return name;
    const char *v=NULL;
    if(ver==0)
        v="";
    if(ver==1)
        v="*";
    if(!v && !vername)
        return name;
    if(ver>1)
        v = vername;
    char buf[strlen(name)+strlen(v)+1+1];
    strcpy(buf, name);
    strcat(buf, "@");
    strcat(buf, v);
    return AddDictionnary(my_context->versym, buf);
}

int SameVersionnedSymbol(const char* name1, int ver1, const char* vername1, const char* name2, int ver2, const char* vername2)
{
    if(strcmp(name1, name2))    //name are different, no need to go further
        return 0;
    if(ver1==-1 || ver2==-1)    // don't check version, so ok
        return 1;
    if(ver1==ver2 && ver1<2)    // same ver (local or global), ok
        return 1;
    if(ver1==0 || ver2==0)  // one is local, the other is not, no match
        return 0;
    if(ver1==1 || ver2==1)  // one if global, ok
        return 1;
    if(!strcmp(vername1, vername2))  // same vername
        return 1;
    return 0;
}

void* GetDynamicSection(elfheader_t* h)
{
    if(!h)
        return NULL;
    return h->Dynamic;
}

typedef struct search_symbol_s{
    const char* name;
    void*       addr;
    void*       lib;
} search_symbol_t;

static int dl_iterate_phdr_findsymbol(struct dl_phdr_info* info, size_t size, void* data)
{
    search_symbol_t* s = (search_symbol_t*)data;

    for(int j = 0; j<info->dlpi_phnum; ++j) {
        if (info->dlpi_phdr[j].p_type == PT_DYNAMIC) {
            //ElfW(Sym)* sym = NULL;
            //ElfW(Word) sym_cnt = 0;
            ElfW(Verdef)* verdef = NULL;
            ElfW(Word) verdef_cnt = 0;
            char *strtab = NULL;
            ElfW(Dyn)* dyn = (ElfW(Dyn)*)(info->dlpi_addr +  info->dlpi_phdr[j].p_vaddr); //Dynamic Section
            // grab the needed info
            while(dyn->d_tag != DT_NULL) {
                switch(dyn->d_tag) {
                    case DT_STRTAB:
                        strtab = (char *)(dyn->d_un.d_ptr);
                        break;
                    case DT_VERDEF:
                        verdef = (ElfW(Verdef)*)(info->dlpi_addr +  dyn->d_un.d_ptr);
                        break;
                    case DT_VERDEFNUM:
                        verdef_cnt = dyn->d_un.d_val;
                        break;
                }
                ++dyn;
            }
            if(strtab && verdef && verdef_cnt) {
                if((uintptr_t)strtab < (uintptr_t)info->dlpi_addr) // this test is need for linux-vdso on PI and some other OS (looks like a bug to me)
                    strtab=(char*)((uintptr_t)strtab + info->dlpi_addr);
                // Look fr all defined versions now
                ElfW(Verdef)* v = verdef;
                while(v) {
                    ElfW(Verdaux)* vda = (ElfW(Verdaux)*)(((uintptr_t)v) + v->vd_aux);
                    if(v->vd_version>0 && !v->vd_flags)
                        for(int i=0; i<v->vd_cnt; ++i) {
                            const char* vername = (strtab+vda->vda_name);
                            if(vername && vername[0] && (s->addr = dlvsym(s->lib, s->name, vername))) {
                                printf_log(/*LOG_DEBUG*/LOG_INFO, "Found symbol with version %s, value = %p\n", vername, s->addr);
                                return 1;   // stop searching
                            }
                            vda = (ElfW(Verdaux)*)(((uintptr_t)vda) + vda->vda_next);
                        }
                    v = v->vd_next?(ElfW(Verdef)*)((uintptr_t)v + v->vd_next):NULL;
                }
            }
        }
    }
    return 0;
}

void* GetNativeSymbolUnversionned(void* lib, const char* name)
{
    // try to find "name" in loaded elf, whithout checking for the symbol version (like dlsym, but no version check)
    search_symbol_t s;
    s.name = name;
    s.addr = NULL;
    if(lib) 
        s.lib = lib;
    else 
        s.lib = my_context->box64lib;
    printf_log(LOG_INFO, "Look for %s in loaded elfs\n", name);
    dl_iterate_phdr(dl_iterate_phdr_findsymbol, &s);
    return s.addr;
}

static int64_t Pop64(CPUX86State *cpu)
{
    uint64_t* st = ((uint64_t*)(cpu->regs[R_ESP]));
    cpu->regs[R_ESP] += 8;

    return *st;
}
static void Push64(CPUX86State *cpu, uint64_t v)
{
    cpu->regs[R_ESP] -= 8;
    *((uint64_t*)cpu->regs[R_ESP]) = v;

}
uintptr_t pltResolver = ~0LL;
uintptr_t dl_runtime_resolver = ~0LL;
uintptr_t link_map_obj=0;
void PltResolver(void)
{
    CPUX86State *cpu = (CPUX86State*)lsenv->cpu_state;
    uintptr_t addr = Pop64(cpu);
    int slot = (int)Pop64(cpu);
    elfheader_t *h = (elfheader_t*)addr;
    printf_log(LOG_INFO, "PltResolver: Addr=%p, Slot=%d Return=%p: elf is %s (VerSym=%p)\n", (void*)addr, slot, *(void**)(cpu->regs[R_ESP]), h->name, h->VerSym);

    Elf64_Rela * rel = (Elf64_Rela *)(h->jmprel + h->delta) + slot;
    Elf64_Sym *sym = &h->DynSym[ELF64_R_SYM(rel->r_info)];
 #if defined(CONFIG_LATX_KZT) && defined(CONFIG_LATX_DEBUG)
    int bind = ELF64_ST_BIND(sym->st_info);
 #endif
    const char* symname = SymName(h, sym);
    int version = h->VerSym?((Elf64_Half*)((uintptr_t)h->VerSym+h->delta))[ELF64_R_SYM(rel->r_info)]:-1;
    if(version!=-1) version &= 0x7fff;
    const char* vername = GetSymbolVersion(h, version);
    uint64_t *p = (uint64_t*)(rel->r_offset + h->delta);
    uintptr_t offs = 0;
    uintptr_t end = 0;

    library_t* lib = h->lib;
    lib_t* local_maplib = GetMaplib(lib);
    GetGlobalSymbolStartEnd(my_context->maplib, symname, &offs, &end, h, version, vername);
    if(!offs && !end && local_maplib) {
        GetGlobalSymbolStartEnd(local_maplib, symname, &offs, &end, h, version, vername);
    }
    if(!offs && !end && !version)
        GetGlobalSymbolStartEnd(my_context->maplib, symname, &offs, &end, h, -1, NULL);

    if (!offs) {
//        printf_log(LOG_INFO, "Error: PltResolver: Symbol %s(ver %d: %s%s%s) not found, cannot apply R_X86_64_JUMP_SLOT %p (%p) in %s\n", symname, version, symname, vername?"@":"", vername?vername:"", p, *(void**)p, h->name);
        //return to __dl_runtime_resolver
        Push64(cpu, slot);
        Push64(cpu, h->self_link_map);
        Push64(cpu, dl_runtime_resolver);
        return;
    } else {
        offs = (uintptr_t)getAlternate((void*)offs);
        if(p) {
            printf_log(LOG_INFO, "            Apply %s R_X86_64_JUMP_SLOT %p with sym=%s(ver %d: %s%s%s) (%p -> %p / %s)\n", (bind==STB_LOCAL)?"Local":"Global", p, symname, version, symname, vername?"@":"", vername?vername:"",*(void**)p, (void*)offs, ElfName(FindElfAddress(my_context, offs)));
            *p = offs;
        } else {
            printf_log(LOG_INFO, "PltResolver: Warning, Symbol %s(ver %d: %s%s%s) found, but Jump Slot Offset is NULL \n", symname, version, symname, vername?"@":"", vername?vername:"");
        }
        //next_tb is the onebridge of the function
        Push64(cpu, offs);
    }
}
