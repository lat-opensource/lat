#define TARGET_DRM_IOCTL_AMDGPU_GEM_CREATE \
    TARGET_IOWR('d', 0x40, union drm_amdgpu_gem_create)
#define TARGET_DRM_IOCTL_AMDGPU_GEM_MMAP \
    TARGET_IOWR('d', 0x41, union drm_amdgpu_gem_mmap)
#define TARGET_DRM_IOCTL_AMDGPU_CTX \
    TARGET_IOWR('d', 0x42, union drm_amdgpu_ctx)
#define TARGET_DRM_IOCTL_AMDGPU_BO_LIST \
    TARGET_IOWR('d', 0x43, union drm_amdgpu_bo_list)
#define TARGET_DRM_IOCTL_AMDGPU_CS \
    TARGET_IOWR('d', 0x44, union drm_amdgpu_cs)
#define TARGET_DRM_IOCTL_AMDGPU_INFO \
    TARGET_IOW('d', 0x45, struct drm_amdgpu_info)
#define TARGET_DRM_IOCTL_AMDGPU_GEM_METADATA \
    TARGET_IOWR('d', 0x46, struct target_drm_amdgpu_gem_metadata)
#define TARGET_DRM_IOCTL_AMDGPU_GEM_WAIT_IDLE \
    TARGET_IOWR('d', 0x47, union drm_amdgpu_gem_wait_idle)
#define TARGET_DRM_IOCTL_AMDGPU_GEM_VA \
    TARGET_IOWR('d', 0x48, struct drm_amdgpu_gem_va)
#if defined(TARGET_I386) && !defined(TARGET_X86_64) && \
    !defined(CONFIG_AMDGPU_GEN_VA_OLD)
/* Older libdrm clients pass this 40-byte pre-timeline request. */
#define TARGET_DRM_IOCTL_AMDGPU_GEM_VA_OLD \
    TARGET_IOWR('d', 0x48, struct target_drm_amdgpu_gem_va_old)
#define DRM_IOCTL_AMDGPU_GEM_VA_OLD DRM_IOCTL_AMDGPU_GEM_VA
#endif
#define TARGET_DRM_IOCTL_AMDGPU_WAIT_CS \
    TARGET_IOWR('d', 0x49, union drm_amdgpu_wait_cs)
#define TARGET_DRM_IOCTL_AMDGPU_GEM_OP \
    TARGET_IOWR('d', 0x50, struct drm_amdgpu_gem_op)
#define TARGET_DRM_IOCTL_AMDGPU_GEM_USERPTR \
    TARGET_IOWR('d', 0x51, struct drm_amdgpu_gem_userptr)
#define TARGET_DRM_IOCTL_AMDGPU_WAIT_FENCES \
    TARGET_IOWR('d', 0x52, union drm_amdgpu_wait_fences)
#define TARGET_DRM_IOCTL_AMDGPU_VM \
    TARGET_IOWR('d', 0x53, union drm_amdgpu_vm)
#define TARGET_DRM_IOCTL_AMDGPU_FENCE_TO_HANDLE \
    TARGET_IOWR('d', 0x54, union drm_amdgpu_fence_to_handle)
#define TARGET_DRM_IOCTL_AMDGPU_SCHED \
    TARGET_IOW('d', 0x55, union drm_amdgpu_sched)
struct target_drm_amdgpu_query_fw {
    abi_uint fw_type;
    abi_uint ip_instance;
    abi_uint index;
    abi_uint _pad;
};
struct target_drm_amdgpu_fence {
    abi_uint ctx_id;
    abi_uint ip_type;
    abi_uint ip_instance;
    abi_uint ring;
    abi_ullong seq_no;
};
struct target_drm_amdgpu_info {
    abi_ullong return_pointer;
    abi_uint return_size;
    abi_uint query;
    union {
        struct {
            abi_uint id;
            abi_uint _pad;
        } mode_crtc;
        struct {
            abi_uint type;
            abi_uint ip_instance;
        } query_hw_ip;
        struct {
            abi_uint dword_offset;
            abi_uint count;
            abi_uint instance;
            abi_uint flags;
        } read_mmr_reg;
        struct target_drm_amdgpu_query_fw query_fw;
        struct {
            abi_uint type;
            abi_uint offset;
        } vbios_info;
        struct {
            abi_uint type;
        } sensor_info;
    };
};
struct target_drm_amdgpu_gem_metadata {
    abi_uint handle;
    abi_uint op;
    struct {
        abi_ullong flags;
        abi_ullong tiling_info;
        abi_uint data_size_bytes;
        abi_uint data[64];
    } data;
};
struct target_drm_amdgpu_fence_to_handle_in {
    struct target_drm_amdgpu_fence fence;
    abi_uint what;
    abi_uint pad;
};

#if defined(TARGET_I386) && !defined(TARGET_X86_64) && \
    !defined(CONFIG_AMDGPU_GEN_VA_OLD)
struct target_drm_amdgpu_gem_va_old {
    abi_uint handle;
    abi_uint _pad;
    abi_uint operation;
    abi_uint flags;
    abi_ullong va_address;
    abi_ullong offset_in_bo;
    abi_ullong map_size;
};
#endif
