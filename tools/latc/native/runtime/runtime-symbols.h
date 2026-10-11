#ifndef LATC_RUNTIME_SYMBOLS_H
#define LATC_RUNTIME_SYMBOLS_H

#include <stdint.h>

uintptr_t lat_runtime_symbol_address(uint32_t symbol);
void lat_runtime_symbols_configure(uint32_t image_flags);

#endif
