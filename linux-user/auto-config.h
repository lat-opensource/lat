#ifndef LAT_AOT_AUTO_CONFIG_H
#define LAT_AOT_AUTO_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

bool lat_aot_config_option(const char *key, const char *value);
void lat_aot_config_environment(void);
int lat_aot_config_init(int *legacy, bool explicit_legacy, const char *bindir,
                        const char *libdir, char *error, size_t error_size);
const char *lat_aot_cache_path(void);
const char *lat_aot_socket_path(void);
const char *lat_aot_module_path(void);
bool lat_aot_ensure_daemon(void);

#endif
