#ifndef LAT_AOT_V2_MODULE_LOADER_H
#define LAT_AOT_V2_MODULE_LOADER_H

#include "registry.h"

#include <stddef.h>

int lat_aot_v2_module_open(const char *path,
                           const LatAotExpectedV2 *expected,
                           LatAotLoadedModuleV2 *module,
                           char *error, size_t error_size);

#endif
