/**
 * @file file_ctx.c
 * @author wwq <weiwenqiang@mail.ustc.edu.cn>
 * @brief AOT optimization
 */
#include<stdio.h>
#include <assert.h>
#include"dirent.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "file_ctx.h"
#ifdef CONFIG_LATX_AOT
#define AOT_D_NAME_MAX_LENGTH 1256
struct aot_info {
    char d_name[PATH_MAX];
    time_t st_actime;
};

int flock_set(int fd, int type, bool wait)
{
    struct flock fflock = {0};
#ifdef AOT_DEBUG
    fcntl(fd, F_GETLK, &fflock);
    if (fflock.l_type != F_UNLCK) {
        if (fflock.l_type == F_RDLCK) {
            qemu_log_mask(LAT_LOG_AOT, "flock has been set to read lock by %d\n",
                fflock.l_pid);
        } else if (fflock.l_type == F_WRLCK) {
            qemu_log_mask(LAT_LOG_AOT, "flock has been set to write lock by %d\n",
                fflock.l_pid);
        }
    }
#endif
    fflock.l_type = type;
    fflock.l_whence = SEEK_SET;
    fflock.l_start = 0;
    fflock.l_len = 0;
    fflock.l_pid = -1;
    int ret = -1;
    if (wait) {
        ret = fcntl(fd, F_SETLKW, &fflock);
    } else {
        ret = fcntl(fd, F_SETLK, &fflock);
    }
    if (ret < 0) {
    #ifdef AOT_DEBUG
        qemu_log_mask(LAT_LOG_AOT, "set lock failed err=%s!\n", strerror(errno));
    #endif
        return -1;
    }
    return 0;
}
int send_file_message(char *file_d, char *message)
{
    FILE *pfile = fopen(file_d, "a+");
    assert(pfile && "open file failed");
    fseek(pfile, 0, SEEK_END);
    if (fwrite(message, strlen(message) + 1, 1, pfile) != 1) {
        qemu_log_mask(LAT_LOG_AOT, "Error! write aot metadata failed!\n");
        fclose(pfile);
        return -1;
    }
    fclose(pfile);
    return 0;
}
int file_lock(char *file_name, int *fd, int type, bool wait)
{
    int mask = O_RDONLY;

    if (access(file_name, 0) < 0) {
        mask |= O_CREAT;
    }
    if (type == F_WRLCK) {
        mask |= O_RDWR;
    }
    *fd = open(file_name, mask, 0666);
    if (*fd < 0) {
    #ifdef AOT_DEBUG
        qemu_log_mask(LAT_LOG_AOT, "open %s failed err=%s!\n", file_name,
                      strerror(errno));
    #endif
        return *fd;
    }
    return flock_set(*fd, type, wait);
}
static int aot_file_cmp(const void *a, const void *b)
{
    struct aot_info * pa = *(struct aot_info **)a;
    struct aot_info * pb = *(struct aot_info **)b;
    return pa->st_actime > pb->st_actime ?
      1 : (pa->st_actime < pb->st_actime ? -1 : 0);
}

uint64_t aot_file_rmgroup(char *aotFile)
{
    char tmpfile[PATH_MAX];
    struct stat statbuf;
    uint64_t f_size = 0;
    assert(strlen(aotFile) < PATH_MAX - 1);
    for (int i = 0; i < 10000; i++) {
        if (aot_get_file_name(aotFile, tmpfile, i) < 0) {
            break;
        }
        if (stat(tmpfile, &statbuf)) {
            qemu_log_mask(LAT_LOG_AOT, "(%s:%d)lstat error %d \n",
					__FILE__, __LINE__, errno);
            continue;
        }
        if (remove(tmpfile)) {
            printf("(%s:%d)removefile %s error %d\n",
              __FILE__, __LINE__, tmpfile, errno);
        }
        f_size += statbuf.st_size;
    }
    return f_size;
}

static bool ends_with(const char *file_name, const char *suffix)
{
    size_t file_len = strlen(file_name);
    size_t suffix_len = strlen(suffix);

    return file_len >= suffix_len
        && !strncmp(file_name + file_len - suffix_len, suffix, suffix_len);
}

static bool is_aot_file(char *file_name)
{
    /* aot2 and .aot2 */
    return ends_with(file_name, "aot2") || ends_with(file_name, ".aot2");
}

static bool is_aot_lock(char *file_name)
{
    return ends_with(file_name, ".lock");
}

static void aot_file_release_oldfile(struct aot_info **f_info,
  int count, int needRelaeseMB)
{
    uint64_t hasReleaseSize = 0;
    qsort(f_info, count, sizeof(struct aot_info *), aot_file_cmp);
    assert(needRelaeseMB);
    int fd = -1;
    for (int i = 0; i < count; i++) {
        if (is_aot_lock(f_info[i]->d_name)) {
            strcpy(aot_file_path, f_info[i]->d_name);
            int l = strlen(aot_file_path);
            for (int j = l - 1; j > 0; j--) {
                if (aot_file_path[j] == '.') {
                   aot_file_path[j] = '\0';
                   break;
                }
            }
            if(access(aot_file_path, R_OK) < 0) {
                if (file_lock(f_info[i]->d_name, &fd, F_WRLCK, false) >= 0) {
                    remove(f_info[i]->d_name);
                }
            }
            continue;
        }

        strcpy(aot_file_lock, f_info[i]->d_name);
        strcat(aot_file_lock, ".lock");
        if (file_lock(aot_file_lock, &fd, F_WRLCK, false) >= 0) {
            hasReleaseSize += aot_file_rmgroup(f_info[i]->d_name) / (1024 * 1024);
            if (hasReleaseSize >= needRelaeseMB) {
                break;
            }
            remove(aot_file_lock);
        }
    }
    close(fd);
}

int aot_file_ctx(uint64_t maxSize, uint64_t leftMinSize)
{
    size_t aot_total_size = 0;
    DIR *p_dir;
    struct dirent *p_dirent;
    int i_count = 0;
    int max_aot_file_count = 1000;
    char *aot_dir = malloc(PATH_MAX * sizeof(char));
    struct aot_info **f_info =
        malloc(max_aot_file_count * sizeof(struct aot_info *));

    char *home = getenv("HOME");
    if (likely(home)) {
        snprintf(aot_dir, PATH_MAX, "%s%s", home, "/.cache/latx/");
    } else {
        snprintf(aot_dir, PATH_MAX, "%s", "/.cache/latx/");
    }
    aot_dir[PATH_MAX - 1] = 0;
    p_dir = opendir(aot_dir);
    if (p_dir == NULL) {
        qemu_log_mask(LAT_LOG_AOT, "---->can\'t open %s\n", aot_dir);
        return -1;
    }
    while ((p_dirent = readdir(p_dir))) {
        struct stat statbuf;
        char subdir[PATH_MAX + 1];
        snprintf(subdir, PATH_MAX, "%s%s", aot_dir, p_dirent->d_name);
        if (stat(subdir, &statbuf)) {
            qemu_log_mask(LAT_LOG_AOT, "(%s:%d)lstat error\n", __FILE__, __LINE__);
	    qemu_log_mask(LAT_LOG_AOT, "lstat %s \n", subdir);
	    qemu_log_mask(LAT_LOG_AOT, "lstat %d \n", errno);
            continue;
        }
        if (!is_aot_file(p_dirent->d_name) && !is_aot_lock(p_dirent->d_name)) {
            continue;
        }
        aot_total_size += statbuf.st_size;
        struct aot_info *my_info = malloc(sizeof(struct aot_info));
        strncpy(my_info->d_name, subdir, PATH_MAX - 1);
        my_info->d_name[PATH_MAX - 1] = '\0';
        my_info->st_actime = statbuf.st_atime;
        if (i_count >= max_aot_file_count) {
            max_aot_file_count += 1000;
            f_info = realloc(f_info, max_aot_file_count * sizeof(struct aot_info *));
        }
        f_info[i_count] = my_info;
        i_count++;
    }

    closedir(p_dir);
    aot_total_size /= 1024 * 1024;/*B to MB*/
    if (aot_total_size >= (maxSize - leftMinSize)) {
        aot_file_release_oldfile(f_info, i_count,
            aot_total_size - (maxSize >> 1));
    }

    if (aot_dir) {
        free(aot_dir);
    }
    for (int i = 0; i < i_count; i++) {
        if (f_info[i]) {
            free(f_info[i]);
        }
    }
    if (f_info) {
        free(f_info);
    }
    return 0;
}
#endif
