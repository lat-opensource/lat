/* Steam pressure-vessel linux-user launch helpers. */

#ifndef LINUX_USER_PRESSURE_VESSEL_H
#define LINUX_USER_PRESSURE_VESSEL_H

#include "qemu/envlist.h"

#ifdef CONFIG_LATX
void latx_pressure_vessel_prepare(const char *program, char **target_argv,
                                  envlist_t *envlist);
void latx_pressure_vessel_exec_payload(char **target_environ);
#endif

#endif /* LINUX_USER_PRESSURE_VESSEL_H */
