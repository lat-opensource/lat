#include "lat-aot-v2.h"
#include "latc-build-id.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>

static LatAotRuntimeSyscallCallbackV2 syscall_callback;
static void *syscall_opaque;
__attribute__((visibility("hidden")))
uintptr_t lat_aot_runtime_target_slots_v2[LAT_AOT_RUNTIME_TARGET_COUNT];

uint32_t lat_aot_runtime_abi_version(void)
{
    return LAT_AOT_V2_ABI_VERSION;
}

const char *lat_aot_runtime_build_id(void)
{
    return LATC_BUILD_ID;
}

int lat_aot_runtime_bind_syscall(LatAotRuntimeSyscallCallbackV2 callback,
                                 void *opaque)
{
    if (!callback) {
        errno = EINVAL;
        return -1;
    }
    syscall_callback = callback;
    syscall_opaque = opaque;
    return 0;
}

int lat_aot_runtime_bind_targets(const LatAotRuntimeTargetsV2 *targets)
{
    if (!targets || targets->struct_size != sizeof(*targets) ||
        targets->reserved) {
        errno = EINVAL;
        return -1;
    }
    for (size_t i = 0; i < LAT_AOT_RUNTIME_TARGET_COUNT; i++) {
        if (!targets->target[i]) {
            errno = EINVAL;
            return -1;
        }
    }
    memcpy(lat_aot_runtime_target_slots_v2, targets->target,
           sizeof(lat_aot_runtime_target_slots_v2));
    return 0;
}

void lat_aot_runtime_raise_syscall(void)
{
    if (syscall_callback) {
        syscall_callback(syscall_opaque);
    }
    abort();
}
