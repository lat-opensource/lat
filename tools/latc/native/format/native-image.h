#ifndef LATC_NATIVE_IMAGE_READER_H
#define LATC_NATIVE_IMAGE_READER_H

#include "lat-native-image.h"

#include <stddef.h>

int lat_native_image_validate(const void *data, size_t size,
                              char *error, size_t error_size);
int lat_native_image_inspect_file(const char *path,
                                  LatNativeImageHeaderV2 *header,
                                  char *error, size_t error_size);
int lat_native_image_mark_x86_static_file(const char *path,
                                          char *error, size_t error_size);
int lat_native_image_merge_files(const char *base_path,
                                 const char *delta_path,
                                 const char *output_path,
                                 char *error, size_t error_size);

#endif
