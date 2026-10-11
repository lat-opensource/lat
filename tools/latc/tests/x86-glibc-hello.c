#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *value = getenv("LATC_TEST_ENV");
    if (argc != 3 || strcmp(argv[1], "alpha") || strcmp(argv[2], "beta") ||
        !value || strcmp(value, "works")) {
        return 2;
    }
    puts("Hello from glibc!");
    return 0;
}
