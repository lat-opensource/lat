#include "qemu/osdep.h"
#include <cxxabi.h>

#ifdef CONFIG_LATX_DEBUG
extern "C"
const char *latx_demangling(char *symbol)
{
    size_t b_sz = 1024;
    char buff[b_sz];
    int status;
    char *ret = abi::__cxa_demangle(symbol, buff, &b_sz, &status);
    return (status == 0) ? ret : symbol;
}
#endif
