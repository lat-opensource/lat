/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    const char *prefix = getenv("LAT_LD_PREFIX");
    char *line = NULL;
    size_t capacity = 0;
    int mapped = 0;
    FILE *maps;

    if (argc != 3 || !prefix || strcmp(prefix, argv[1])) {
        fprintf(stderr, "FAIL: explicit -L was not exported to the Guest\n");
        return 1;
    }
    maps = fopen("/proc/self/maps", "r");
    if (!maps) {
        return 2;
    }
    while (getline(&line, &capacity, maps) >= 0) {
        if (strstr(line, argv[1]) && strstr(line, "libc.so.6")) {
            mapped = 1;
        }
    }
    free(line);
    fclose(maps);
    if (!mapped) {
        fprintf(stderr, "FAIL: libc did not come from the selected root\n");
        return 3;
    }
    if (!strcmp(argv[2], "parent")) {
        char *child_argv[] = { argv[0], argv[1], "child", NULL };

        execve(argv[0], child_argv, environ);
        perror("execve");
        return 4;
    }
    if (strcmp(argv[2], "child")) {
        return 5;
    }
    puts("PASS: explicit runtime prefix and mapped libc survive exec");
    return 0;
}
