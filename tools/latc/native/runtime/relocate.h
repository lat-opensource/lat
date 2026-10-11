#ifndef LATC_NATIVE_RELOCATE_H
#define LATC_NATIVE_RELOCATE_H

#include "lat-native-image.h"

#include <stddef.h>

typedef struct LatNativeCode {
    void *address;
    size_t size;
} LatNativeCode;

int lat_native_code_load(const LatNativeImageHeaderV2 *header,
                         const unsigned char *image, size_t image_size,
                         LatNativeCode *code, char *error,
                         size_t error_size);
void lat_native_code_unload(LatNativeCode *code);

#endif
