// SPDX-License-Identifier: GPL-2.0-or-later

/* A zombie provides an empty cmdline that can be scanned repeatedly. */
#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int count_fds(void)
{
    DIR *dir = opendir("/proc/self/fd");
    struct dirent *entry;
    int count = 0;

    if (!dir) {
        return -1;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            count++;
        }
    }
    closedir(dir);
    return count;
}

int main(void)
{
    siginfo_t info = { 0 };
    int status;
    int before;
    int after;
    char path[80];
    char data[32];
    pid_t child = fork();

    if (child < 0) {
        return 2;
    }
    if (!child) {
        _exit(0);
    }
    if (waitid(P_PID, child, &info, WEXITED | WNOWAIT)) {
        return 3;
    }
    snprintf(path, sizeof(path), "/proc/%d/cmdline", child);
    before = count_fds();
    if (before < 0) {
        return 4;
    }
    for (int i = 0; i < 256; i++) {
        int fd = open(path, O_RDONLY);
        ssize_t size;

        if (fd < 0) {
            perror("open empty cmdline");
            return 5;
        }
        size = read(fd, data, sizeof(data));
        close(fd);
        if (size != 0) {
            fprintf(stderr, "expected zombie empty cmdline, got %zd\n", size);
            return 6;
        }
    }
    after = count_fds();
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status) ||
        WEXITSTATUS(status)) {
        return 7;
    }
    printf("empty_reads=256 fds_before=%d fds_after=%d delta=%d\n",
           before, after, after - before);
    return after == before ? 0 : 1;
}
