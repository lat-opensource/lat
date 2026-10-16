/* Verify /proc/<other-task>/cmdline hides the translator prefix. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int read_other_cmdline(pid_t pid, const char *expected)
{
    char path[64];
    char buf[4096];
    ssize_t len;
    int fd;

    snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open cmdline");
        return 3;
    }
    len = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (len <= 0) {
        fprintf(stderr, "unreadable cmdline\n");
        return 4;
    }
    buf[len] = '\0';
    printf("first argv field: %s\n", buf);
    fflush(stdout);
    if (strcmp(buf, expected) != 0) {
        fprintf(stderr, "first field '%s' does not match '%s'\n",
                buf, expected);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    pid_t self = getpid();
    pid_t child;
    int status;

    child = fork();
    if (child < 0) {
        perror("fork");
        return 2;
    }
    if (child == 0) {
        _exit(read_other_cmdline(self, argc > 0 && argv[0] ? argv[0] : ""));
    }
    if (waitpid(child, &status, 0) != child) {
        perror("waitpid");
        return 5;
    }
    if (WIFSIGNALED(status)) {
        fprintf(stderr, "child killed by signal %d\n", WTERMSIG(status));
        return 1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
