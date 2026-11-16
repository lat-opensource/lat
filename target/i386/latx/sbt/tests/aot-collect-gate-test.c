/* SPDX-License-Identifier: GPL-2.0-only */
#include "qemu/osdep.h"
#include <sys/wait.h>
#include <poll.h>
#ifndef AOT_SOURCE
#define AOT_SOURCE "../aot.c"
#endif
#include AOT_SOURCE
static char path_buffer[PATH_MAX], lock_buffer[PATH_MAX], directory[PATH_MAX];
char *aot_file_path = path_buffer;
char *aot_file_lock = lock_buffer;
static int existing_count;
static bool fail_lock;
static int failed_fd;
lib_info *lib_tree_lookup(char *name) { return NULL; }
int aot_get_tb_num(char *name, char *file, CPUState *cpu) { return existing_count; }
int get_aot_path(const char *name, char *path, size_t size)
{ return snprintf(path, size, "%s/cache", directory) >= size ? -1 : 0; }
int aot_file_get_lock_path(const char *path, char *out, size_t size)
{ return snprintf(out, size, "%s.lock", path) >= size ? -1 : 0; }
int flock_set(int fd, int type, bool wait)
{
    struct flock lock = {.l_type=type, .l_whence=SEEK_SET};
    return fcntl(fd, wait ? F_SETLKW : F_SETLK, &lock);
}
int file_lock(const char *path, int *fd, int type, bool wait)
{
    *fd = open(path, O_CREAT|O_RDWR, 0600);
    if (*fd < 0) return -1;
    if (fail_lock) { failed_fd=*fd; errno=EIO; return -1; }
    return flock_set(*fd,type,wait);
}
static void release(int fd) { g_assert_cmpint(flock_set(fd,F_UNLCK,true),==,0); close(fd); }
static int gate(int *fd) { char name[]="module"; return need_gen_aot(NULL,name,name,fd,1); }
static void test_ratio(void)
{
    int fd=-1; existing_count=1000;
    aot_collect_snapshot=false; g_assert_cmpint(gate(&fd),==,-1);
    aot_collect_snapshot=true; g_assert_cmpint(gate(&fd),>=,0); release(fd);
    existing_count=0;
}
static void test_error(void)
{
    int fd=-1; fail_lock=true; aot_collect_snapshot=true;
    g_assert_cmpint(gate(&fd),==,-1); g_assert_cmpint(fd,==,-1);
    g_assert_cmpint(fcntl(failed_fd,F_GETFD),==,-1); g_assert_cmpint(errno,==,EBADF);
    fail_lock=false;
    pid_t pid=fork(); g_assert_cmpint(pid,>=,0);
    if (!pid) { aot_collect_generation_failed("test failure"); _exit(0); }
    int status; g_assert_cmpint(waitpid(pid,&status,0),==,pid);
    g_assert(WIFEXITED(status)); g_assert_cmpint(WEXITSTATUS(status),==,EXIT_FAILURE);
}
static void test_contention(bool collect)
{
    int fd=-1, ready[2], done[2]; char ch;
    existing_count=0; aot_collect_snapshot=false; g_assert_cmpint(gate(&fd),>=,0);
    g_assert_cmpint(pipe(ready),==,0); g_assert_cmpint(pipe(done),==,0);
    pid_t pid=fork(); g_assert_cmpint(pid,>=,0);
    if (!pid) {
        close(fd); close(ready[0]); close(done[0]);
        aot_collect_snapshot=collect; int childfd=-1;
        if (write(ready[1],"R",1)!=1) _exit(4);
        int ret=gate(&childfd);
        if (write(done[1],ret>=0?"Y":"N",1)!=1) _exit(5);
        if (ret>=0) release(childfd);
        _exit(0);
    }
    close(ready[1]); close(done[1]);
    g_assert_cmpint(read(ready[0],&ch,1),==,1);
    struct pollfd p={.fd=done[0],.events=POLLIN}; int early=poll(&p,1,200);
    release(fd);
    g_assert_cmpint(read(done[0],&ch,1),==,1);
    int status; g_assert_cmpint(waitpid(pid,&status,0),==,pid); g_assert_cmpint(status,==,0);
    close(ready[0]); close(done[0]);
    if (collect) { g_assert_cmpint(early,==,0); g_assert_cmpint(ch,==,'Y'); }
    else { g_assert_cmpint(early,==,1); g_assert_cmpint(ch,==,'N'); }
}
int main(int argc,char **argv)
{
    char temp[]="/tmp/aot-collect-gate-XXXXXX"; char *dir=mkdtemp(temp);
    g_assert_nonnull(dir); snprintf(directory,sizeof(directory),"%s",dir);
    if (argc>1 && !strcmp(argv[1],"ratio")) test_ratio();
    else if (argc>1 && !strcmp(argv[1],"error")) test_error();
    else { test_contention(false); test_contention(true); }
    unlink(lock_buffer); rmdir(directory); puts("PASS collect generation gate"); return 0;
}
