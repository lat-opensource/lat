/**
 * @file segment.c
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT optimization
 */
#include "segment.h"
#include "latx-options.h"

#ifdef CONFIG_LATX_AOT
static GTree *segment_tree;
static GTree *wine_sec_tree;
static void seg_delete(gconstpointer a) {
    seg_info *oldkey = (seg_info *)a;
    lsassert(oldkey);
    lsassert(oldkey->file_name);
    free(oldkey->file_name);
    free(oldkey);
}

static gint seg_cmp(gconstpointer a, gconstpointer b)
{
    seg_info *pa = (seg_info *)a;
    seg_info *pb = (seg_info *)b;

    assert(pa && pb);
    if (pa->seg_end && pb->seg_end) {
        if ((pa->seg_begin < pb->seg_end)
            && (pa->seg_end > pb->seg_begin)) {
                return 0;
        }
        return pa->seg_begin > pb->seg_begin ? 1 : -1;
    } else if (pa->seg_end == 0) {
        if (pa->seg_begin >= pb->seg_begin &&
                pa->seg_begin < pb->seg_end) {
            return 0;
        }
    }else if (pb->seg_end == 0) {
        if (pb->seg_begin >= pa->seg_begin &&
                pb->seg_begin < pa->seg_end) {
            return 0;
        }
    }
    return pa->seg_begin > pb->seg_begin ? 1 : -1;

}

void segment_tree_init(void)
{
    segment_tree = g_tree_new_full((GCompareDataFunc)seg_cmp,
        NULL, NULL, (GDestroyNotify)seg_delete);
    lsassert(segment_tree);
}

static gboolean dump_segment_tree_node(gpointer key, gpointer val,
                                       gpointer data)
{
    static int index;
    seg_info **vec = (seg_info **)data;
    seg_info *seg = (seg_info *)val;
    if ((seg->seg_end - seg->seg_begin) / TARGET_PAGE_SIZE > 204800) {
        qemu_log_mask(LAT_LOG_AOT, "seg %s is too large\n", seg->file_name);
        return 0;
    }
    seg->first_tb_id = -1;
    seg->last_tb_id = -2;
    vec[index++] = seg;
    return 0;
}

uint64_t deal_seg(wine_sec_info *wine_sec, uint64_t aot_offset, char *buf,
        int fd, int target_prot, abi_long len, abi_long start)
{
    char path[PATH_MAX];
    int pathname_len;
    if (wine_sec) {
        aot_offset = wine_sec->offset;
        strncpy(buf, wine_sec->file_name, PATH_MAX);
        pathname_len = strlen(buf);
        assert(pathname_len >= 0 && pathname_len + 1 <= PATH_MAX);
    } else {
        sprintf(path, "/proc/self/fd/%d", fd);
        pathname_len = readlink(path, buf, PATH_MAX);
        assert(pathname_len >= 0 && pathname_len + 1 <= PATH_MAX);
        buf[pathname_len] = '\0';
    }
    if (pathname_len > 5) {
        segment_tree_insert(buf, aot_offset, start, start + len);
    }
    if (option_aot_wine && !wine_sec) {
        wine_dll_handle(buf, pathname_len, target_prot, start, len,
            aot_offset, fd);
    }
    /* debug info */
    if (option_debug_aot)
        qemu_log_mask(LAT_LOG_AOT,
                "file: %-50s\toffset: 0x%" PRIx64 "\tlen: 0x"
                TARGET_ABI_FMT_lx "\tva 0x" TARGET_ABI_FMT_lx "\n",
                buf, aot_offset, len, start);
    return aot_offset;

}

#include "ts.h"
#include <syscall.h>
void segment_tree_insert(char *name, target_ulong offset, target_ulong begin,
        target_ulong end)
{
    struct stat statbuf;
    if(stat(name, &statbuf)) {
        qemu_log_mask(LAT_LOG_AOT, "segment_tree_insert stat %s failed\n", name);
        return;
    }

    seg_info *seg = (seg_info *)malloc(sizeof(seg_info));
    if (seg == NULL) {
        qemu_log_mask(LAT_LOG_AOT, "Error! No memory for segment_tree_insert alloc!\n");
        _exit(-1);
    }

    seg->file_name = (char *)malloc(strlen(name) + 1);
    if (seg->file_name == NULL) {
        qemu_log_mask(LAT_LOG_AOT, "Error! No memory for segment_tree_insert alloc!\n");
        _exit(-1);
    }
    strncpy(seg->file_name, name, strlen(name) + 1);

    seg->file_offset = offset;
    seg->seg_begin = begin;
    seg->seg_end = end;
    seg->tv_sec = statbuf.st_mtim.tv_sec;
    seg->lib_size = statbuf.st_size;
    seg->buffer = NULL;
    seg->p_segment = NULL;
    seg->is_running = false;
    seg_info * old_seg_info = segment_tree_lookup2(begin, end);
    while(old_seg_info) {
       segment_tree_remove(old_seg_info);
       old_seg_info = segment_tree_lookup2(begin, end);
    }
    /* Now insert this new segment into segment_tree */
    g_tree_replace(segment_tree, seg, seg);
}

static void wine_sec_delete(gconstpointer a) {
    wine_sec_info *oldkey = (wine_sec_info *)a;
    lsassert(oldkey);
    lsassert(oldkey->file_name);
    free(oldkey->file_name);
    free(oldkey);
}

static gint wine_sec_cmp(gconstpointer a, gconstpointer b)
{
    wine_sec_info *pa = (wine_sec_info *)a;
    wine_sec_info *pb = (wine_sec_info *)b;

    if (pa->addr < pb->addr) {
        return -1;
    } else if (pb->addr < pa->addr) {
        return 1;
    } else {
        return 0;
    }
}
void wine_sec_tree_init(void)
{
    wine_sec_tree = g_tree_new_full((GCompareDataFunc)wine_sec_cmp,
        NULL, NULL, (GDestroyNotify)wine_sec_delete);
    lsassert(wine_sec_tree);
}

static void wine_sec_tree_insert(target_ulong addr, uint64_t offset, char* file_name)
{
    wine_sec_info *sec = (wine_sec_info *)malloc(sizeof(wine_sec_info));
    if (sec == NULL) {
        qemu_log_mask(LAT_LOG_AOT, "Error! No memory for segment_tree_insert alloc!\n");
        _exit(-1);
    }

    sec->addr = addr;
    sec->offset = offset;
    sec->file_name = (char *)malloc(strlen(file_name) + 1);
    if (sec->file_name == NULL) {
        qemu_log_mask(LAT_LOG_AOT, "Error! No memory for segment_tree_insert alloc!\n");
        _exit(-1);
    }
    strncpy(sec->file_name, file_name, strlen(file_name) + 1);
    g_tree_replace(wine_sec_tree, sec, sec);
}

wine_sec_info *wine_sec_tree_lookup(target_ulong pc)
{
    if (option_aot) {
        wine_sec_info key = {.addr = pc, .offset = 0, .file_name = NULL};
        return (wine_sec_info *)g_tree_lookup(wine_sec_tree, &key);
    }
    return NULL;
}

static bool wine_dll_is_code(char * type)
{
    const char * code_type [] = {".text", "_text32"};
    for (int i = 0; i < sizeof(code_type) / sizeof(const char *); i++) {
        if(!strncmp(type, code_type[i], 8)) {
            return true;
        }
    }
    return false;
}

static bool wine_dll_inset_sec(abi_ulong map_start, abi_ulong map_len, int map_fd)
{
    wine_dos_mz_head *dos_head = NULL;
    wine_pe_nt_head *pe_head = NULL;
    wine_sec_head sec[WINE_PE_H_SEC_MAX_NUMS];
    if (WINE_HEAD_MIN_SIZE >= map_len) {
        return false;
    }
    dos_head = (wine_dos_mz_head *)(uintptr_t)map_start;
    if (dos_head->e_magic != IMAGE_DOS_SIGNATURE) {
        qemu_log_mask(LAT_LOG_AOT, "err magic %x \n", dos_head->e_magic);
        return false;
    }
    pe_head = (wine_pe_nt_head *)(uintptr_t)(map_start + dos_head->e_lfanew);
    if (pe_head->Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        return false;
    }
    if (pe_head->NumberOfSections <= 0 ||
        pe_head->NumberOfSections > WINE_PE_H_SEC_MAX_NUMS) {
        return false;
    }
    memcpy(&sec,((char *)pe_head +
        pe_head->sec_pos + WINE_PE_H_SEC_POS),
        pe_head->NumberOfSections * sizeof(wine_sec_head));
    for (int i = 0; i < pe_head->NumberOfSections; i++) {
        if (wine_dll_is_code(sec[i].Name)) {
            char path[PATH_MAX],buf[PATH_MAX];
            int pathname_len;
            wine_sec_info info = {0};
            info.addr = map_start + sec[i].VirtualAddress;
            info.offset = sec[i].VirtualAddress;
            sprintf(path, "/proc/self/fd/%d", map_fd);
            pathname_len = readlink(path, buf, PATH_MAX);
            assert(pathname_len >= 0 && pathname_len + 1 <= PATH_MAX);
            buf[pathname_len] = '\0';
            wine_sec_tree_insert(info.addr, info.offset, buf);
        }
    }
    return true;
}

bool wine_dll_handle(char *file_name, int name_len, int target_prot,
    abi_ulong map_start, abi_ulong map_len, int map_offset, int map_fd)
{
    char name_type [5] = {0};
    const char * pe_file [] = {".dll", ".exe", ".sys", ".drv"};
    bool is_pe = false;
    for (int i = 0; i < 4; i++) {
        name_type[i] = tolower(file_name[name_len - 4 + i]);
    }
    for (int i = 0; i < sizeof(pe_file) / sizeof(const char *); i++) {
        if(strstr(name_type, pe_file[i])) {
           is_pe = true;
           break;
        }
    }
    if (is_pe && (target_prot & PROT_EXEC) && map_offset == 0) {
        return wine_dll_inset_sec(map_start, map_len, map_fd);
    }

    return false;
}

seg_info *segment_tree_lookup(target_ulong pc)
{
    if (option_aot) {
        seg_info key = {.file_name = NULL, .file_offset = 0,
                        .seg_begin = pc, .seg_end = 0};
        return (seg_info *)g_tree_lookup(segment_tree, &key);
    }
    return NULL;
}

seg_info *segment_tree_lookup2(target_ulong begin, target_ulong end)
{
    if (option_aot) {
        seg_info key = {.file_name = NULL, .file_offset = 0,
                        .seg_begin = begin, .seg_end = end};
        return (seg_info *)g_tree_lookup(segment_tree, &key);
    }
    return NULL;
}

void segment_tree_remove(seg_info* val)
{
    g_tree_remove(segment_tree, val);
}

static bool check_winepe_segment(seg_info * res) {
    char *file_name = basename(res->file_name);
    for (int i = 0; i < 100; i++) {
        if (!latx_aot_wine_pefiles_cache[i]) {
            return false;
        }
        if (!strcmp(latx_aot_wine_pefiles_cache[i], file_name)) {
            return true;
        }
    }
    return false;
}

bool segment_tree_winepe_lookup(target_ulong pc)
{
    if (!option_aot || !latx_aot_wine_pefiles_cache)
        return false;
    seg_info * res;
    seg_info key = {.file_name = NULL, .file_offset = 0, .seg_begin = pc,
                    .seg_end = 0};
    res = (seg_info *)g_tree_lookup(segment_tree, &key);
    if(res) {
        if(check_winepe_segment(res))
            return true;
    }
    return false;
}

gint get_segment_num(void)
{
    return g_tree_nnodes(segment_tree);
}

void do_segment_record(seg_info **seg_info_vector)
{
    g_tree_foreach(segment_tree, dump_segment_tree_node, seg_info_vector);
}
#endif
