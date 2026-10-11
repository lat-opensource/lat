#ifndef LATC_GUEST_LOADER_H
#define LATC_GUEST_LOADER_H

#include "lat-native-image.h"

#include <stddef.h>
#include <stdint.h>

typedef struct LatGuestMapping {
    uint64_t base;
    uint64_t end;
    uint64_t entry;
    uint64_t phdr;
    uint16_t phent;
    uint16_t phnum;
} LatGuestMapping;

int lat_guest_map(const LatNativeImageHeaderV2 *header,
                  const unsigned char *image, size_t image_size,
                  LatGuestMapping *mapping, char *error,
                  size_t error_size);
void lat_guest_unmap(LatGuestMapping *mapping);

#endif
