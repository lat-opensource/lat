#include "latx-config.h"
#include "latx-options.h"

typedef struct {
    const char* filename;
    void (*func)(void);
} FileFunMap;

static void lative_func(void)
{
    option_lative = 1;
    return;
}

FileFunMap filefunmap[] = {
   {"program", lative_func},                        // lative
};

static void extract_filename(char* filename, char* buffer,
                            size_t buffer_size)
{
    char* last_slash = strrchr(filename, '/');
    char* filename_only;
    if (last_slash != NULL) {
        filename_only = last_slash + 1;
    } else {
        filename_only = filename;
    }
    strncpy(buffer, filename_only, buffer_size - 1);
    char* extension = strchr(buffer, '.');
    if (extension != NULL) {
        *extension = '\0';
    }
}

void latx_handle_args(char *filename)
{
    int i;
    char buffer[256];
    extract_filename(filename, buffer, sizeof(buffer));

    for (i = 0; i < sizeof(filefunmap) / sizeof(FileFunMap); i++) {
        if (strcmp(buffer, filefunmap[i].filename) == 0) {
            filefunmap[i].func();
            return;
        }
    }
}
