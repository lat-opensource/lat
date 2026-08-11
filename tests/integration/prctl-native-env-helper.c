/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
#ifdef PRCTL_EXEC_RACE_FAIL
    (void)argc;
    (void)argv;
    return 1;
#else
    static const char fd_prefix[] = "/proc/self/fd/";
    static const char marker[] = "PR378_EXEC_RACE_ORIGINAL";
    const char *keep = getenv("KEEP");

    if (argc > 1 && !strcmp(argv[1], "carrier")) {
        char buffer[512];
        int fd;
        ssize_t len;

        if (argc < 3 ||
            strncmp(argv[2], fd_prefix, sizeof(fd_prefix) - 1)) {
            return 1;
        }
        fd = open(argv[2], O_RDONLY);
        if (fd < 0) {
            return 1;
        }
        len = read(fd, buffer, sizeof(buffer) - 1);
        close(fd);
        if (len < 0) {
            return 1;
        }
        buffer[len] = '\0';
        if (!strstr(buffer, marker)) {
            return 1;
        }
    }
    if (getenv("_LATX_GUEST_MDWE") || getenv("_LATX_GUEST_TSC")) {
        return 1;
    }
    return !keep || strcmp(keep, "1") != 0;
#endif
}
