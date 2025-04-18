#include "linux-user/ioctl/amdgpu_drm.h"

static inline abi_long target_to_host_drm_amdgpu_info
                        (struct drm_amdgpu_info *host_ver,
         struct target_drm_amdgpu_info *target_ver)
{
    __get_user(host_ver->return_pointer, &target_ver->return_pointer);
    __get_user(host_ver->return_size, &target_ver->return_size);
    __get_user(host_ver->query, &target_ver->query);
    __get_user(host_ver->query_fw.fw_type, &target_ver->query_fw.fw_type);
    __get_user(host_ver->query_fw.ip_instance, &target_ver->query_fw.ip_instance);
    __get_user(host_ver->query_fw.index, &target_ver->query_fw.index);
    __get_user(host_ver->query_fw._pad, &target_ver->query_fw._pad);
    return 0;
}

static inline void host_to_target_drm_amdgpu_info
                        (struct target_drm_amdgpu_info *target_ver,
         struct drm_amdgpu_info *host_ver)
{
    __put_user(host_ver->return_pointer, &target_ver->return_pointer);
    __put_user(host_ver->return_size, &target_ver->return_size);
    __put_user(host_ver->query, &target_ver->query);
    __put_user(host_ver->query_fw.fw_type, &target_ver->query_fw.fw_type);
    __put_user(host_ver->query_fw.ip_instance, &target_ver->query_fw.ip_instance);
    __put_user(host_ver->query_fw.index, &target_ver->query_fw.index);
    __put_user(host_ver->query_fw._pad, &target_ver->query_fw._pad);
}

static inline abi_long target_to_host_drm_amdgpu_gem_metadata
                        (struct drm_amdgpu_gem_metadata *host_ver,
         struct target_drm_amdgpu_gem_metadata *target_ver)
{
    __get_user(host_ver->handle, &target_ver->handle);
    __get_user(host_ver->op, &target_ver->op);
    __get_user(host_ver->data.flags, &target_ver->data.flags);
    __get_user(host_ver->data.tiling_info, &target_ver->data.tiling_info);
    __get_user(host_ver->data.data_size_bytes, &target_ver->data.data_size_bytes);
    memcpy(host_ver->data.data, target_ver->data.data,
                sizeof(uint32_t) * host_ver->data.data_size_bytes);
    return 0;
}

static inline void host_to_target_drm_amdgpu_gem_metadata
                        (struct target_drm_amdgpu_gem_metadata *target_ver,
         struct drm_amdgpu_gem_metadata *host_ver)
{
    __put_user(host_ver->handle, &target_ver->handle);
    __put_user(host_ver->op, &target_ver->op);
    __put_user(host_ver->data.flags, &target_ver->data.flags);
    __put_user(host_ver->data.tiling_info, &target_ver->data.tiling_info);
    __put_user(host_ver->data.data_size_bytes, &target_ver->data.data_size_bytes);
    memcpy(target_ver->data.data, &host_ver->data.data,
                sizeof(uint32_t) * target_ver->data.data_size_bytes);
}

static inline abi_long target_to_host_drm_amdgpu_fence_to_handle_in
                        (union drm_amdgpu_fence_to_handle *host_ver,
         struct target_drm_amdgpu_fence_to_handle_in *target_ver)
{
    __get_user(host_ver->in.fence.ctx_id, &target_ver->fence.ctx_id);
    __get_user(host_ver->in.fence.ip_type, &target_ver->fence.ip_type);
    __get_user(host_ver->in.fence.ip_instance, &target_ver->fence.ip_instance);
    __get_user(host_ver->in.fence.ring, &target_ver->fence.ring);
    __get_user(host_ver->in.fence.seq_no, &target_ver->fence.seq_no);
    __get_user(host_ver->in.what, &target_ver->what);
    __get_user(host_ver->in.pad, &target_ver->pad);
    return 0;
}

static inline void host_to_target_drm_amdgpu_fence_to_handle_in
                        (struct target_drm_amdgpu_fence_to_handle_in *target_ver,
         union drm_amdgpu_fence_to_handle *host_ver)
{
    __put_user(host_ver->in.fence.ctx_id, &target_ver->fence.ctx_id);
    __put_user(host_ver->in.fence.ip_type, &target_ver->fence.ip_type);
    __put_user(host_ver->in.fence.ip_instance, &target_ver->fence.ip_instance);
    __put_user(host_ver->in.fence.ring, &target_ver->fence.ring);
    __put_user(host_ver->in.fence.seq_no, &target_ver->fence.seq_no);
    __put_user(host_ver->in.what, &target_ver->what);
    __put_user(host_ver->in.pad, &target_ver->pad);
}

static abi_long do_ioctl_amdgpu_drm(const IOCTLEntry *ie,
                             uint8_t *buf_temp,
                             int fd, int cmd, abi_long arg)
{
    abi_long ret;
    struct target_drm_amdgpu_info *target_drm_amdgpu_info;
    struct drm_amdgpu_info *drm_amdgpu_info;
    struct target_drm_amdgpu_gem_metadata *target_drm_amdgpu_gem_metadata;
    struct drm_amdgpu_gem_metadata *drm_amdgpu_gem_metadata;
    struct target_drm_amdgpu_fence_to_handle_in *target_drm_amdgpu_fence_to_handle_in;
    union drm_amdgpu_fence_to_handle *drm_amdgpu_fence_to_handle;
    switch (ie->host_cmd) {
    case DRM_IOCTL_AMDGPU_INFO:
        if (!lock_user_struct(VERIFY_READ, target_drm_amdgpu_info, arg, 0)) {
            return -TARGET_EFAULT;
        }
        drm_amdgpu_info = (struct drm_amdgpu_info *)buf_temp;
        ret = target_to_host_drm_amdgpu_info(drm_amdgpu_info, target_drm_amdgpu_info);
        if (!is_error(ret)) {
            ret = get_errno(safe_ioctl(fd, ie->host_cmd, drm_amdgpu_info));
            if (is_error(ret)) {
                /* do nothing */
            } else {
                host_to_target_drm_amdgpu_info(target_drm_amdgpu_info, drm_amdgpu_info);
            }
        }
        unlock_user_struct(target_drm_amdgpu_info, arg, 0);
        return ret;
    case DRM_IOCTL_AMDGPU_GEM_METADATA:
        if (!lock_user_struct(VERIFY_READ, target_drm_amdgpu_gem_metadata, arg, 0)) {
            return -TARGET_EFAULT;
        }
        drm_amdgpu_gem_metadata = (struct drm_amdgpu_gem_metadata *)buf_temp;
        ret = target_to_host_drm_amdgpu_gem_metadata(
                    drm_amdgpu_gem_metadata, target_drm_amdgpu_gem_metadata);
        if (!is_error(ret)) {
            ret = get_errno(safe_ioctl(fd, ie->host_cmd, drm_amdgpu_gem_metadata));
            if (is_error(ret)) {
                /* do nothing */
            } else {
                host_to_target_drm_amdgpu_gem_metadata(
                        target_drm_amdgpu_gem_metadata, drm_amdgpu_gem_metadata);
            }
        }
        unlock_user_struct(target_drm_amdgpu_gem_metadata, arg, 0);
        return ret;
    case DRM_IOCTL_AMDGPU_FENCE_TO_HANDLE:
        if (!lock_user_struct(VERIFY_READ,
                    target_drm_amdgpu_fence_to_handle_in, arg, 0)) {
            return -TARGET_EFAULT;
        }
        drm_amdgpu_fence_to_handle = (union drm_amdgpu_fence_to_handle *)buf_temp;
        ret = target_to_host_drm_amdgpu_fence_to_handle_in(
                    drm_amdgpu_fence_to_handle, target_drm_amdgpu_fence_to_handle_in);
        if (!is_error(ret)) {
            ret = get_errno(safe_ioctl(fd, ie->host_cmd, drm_amdgpu_fence_to_handle));
            if (is_error(ret)) {
                /* do nothing */
            } else {
                host_to_target_drm_amdgpu_fence_to_handle_in(
                        target_drm_amdgpu_fence_to_handle_in, drm_amdgpu_fence_to_handle);
            }
        }
        unlock_user_struct(target_drm_amdgpu_fence_to_handle_in, arg, 0);
        return ret;
    }
    return -TARGET_ENOSYS;
}

