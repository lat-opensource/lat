STRUCT_SPECIAL(termios)

STRUCT(winsize,
       TYPE_SHORT, TYPE_SHORT, TYPE_SHORT, TYPE_SHORT)

STRUCT(serial_multiport_struct,
       TYPE_INT, TYPE_INT, TYPE_CHAR, TYPE_CHAR, TYPE_INT, TYPE_CHAR, TYPE_CHAR,
       TYPE_INT, TYPE_CHAR, TYPE_CHAR, TYPE_INT, TYPE_CHAR, TYPE_CHAR, TYPE_INT,
       MK_ARRAY(TYPE_INT, 32))

STRUCT(serial_icounter_struct,
       TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, MK_ARRAY(TYPE_INT, 16))

STRUCT(sockaddr,
       TYPE_SHORT, MK_ARRAY(TYPE_CHAR, 14))

STRUCT(rtentry,
       TYPE_ULONG, MK_STRUCT(STRUCT_sockaddr), MK_STRUCT(STRUCT_sockaddr), MK_STRUCT(STRUCT_sockaddr),
       TYPE_SHORT, TYPE_SHORT, TYPE_ULONG, TYPE_PTRVOID, TYPE_SHORT, TYPE_PTRVOID,
       TYPE_ULONG, TYPE_ULONG, TYPE_SHORT)

STRUCT(ifmap,
       TYPE_ULONG, TYPE_ULONG, TYPE_SHORT, TYPE_CHAR, TYPE_CHAR, TYPE_CHAR,
       /* Spare 3 bytes */
       TYPE_CHAR, TYPE_CHAR, TYPE_CHAR)

/* The *_ifreq_list arrays deal with the fact that struct ifreq has unions */

STRUCT(sockaddr_ifreq,
       MK_ARRAY(TYPE_CHAR, IFNAMSIZ), MK_STRUCT(STRUCT_sockaddr))

STRUCT(short_ifreq,
       MK_ARRAY(TYPE_CHAR, IFNAMSIZ), TYPE_SHORT)

STRUCT(int_ifreq,
       MK_ARRAY(TYPE_CHAR, IFNAMSIZ), TYPE_INT)

STRUCT(ifmap_ifreq,
       MK_ARRAY(TYPE_CHAR, IFNAMSIZ), MK_STRUCT(STRUCT_ifmap))

STRUCT(char_ifreq,
       MK_ARRAY(TYPE_CHAR, IFNAMSIZ),
       MK_ARRAY(TYPE_CHAR, IFNAMSIZ))

STRUCT(ptr_ifreq,
       MK_ARRAY(TYPE_CHAR, IFNAMSIZ), TYPE_PTRVOID)

STRUCT(ifconf,
       TYPE_INT, TYPE_PTRVOID)

STRUCT(arpreq,
       MK_STRUCT(STRUCT_sockaddr), MK_STRUCT(STRUCT_sockaddr), TYPE_INT, MK_STRUCT(STRUCT_sockaddr),
       MK_ARRAY(TYPE_CHAR, 16))

STRUCT(arpreq_old,
       MK_STRUCT(STRUCT_sockaddr), MK_STRUCT(STRUCT_sockaddr), TYPE_INT, MK_STRUCT(STRUCT_sockaddr))

STRUCT(cdrom_read_audio,
       TYPE_CHAR, TYPE_CHAR, TYPE_CHAR, TYPE_CHAR, TYPE_CHAR, TYPE_INT, TYPE_PTRVOID,
       TYPE_NULL)

STRUCT(hd_geometry,
       TYPE_CHAR, TYPE_CHAR, TYPE_SHORT, TYPE_ULONG)

STRUCT(dirent,
       TYPE_LONG, TYPE_LONG, TYPE_SHORT, MK_ARRAY(TYPE_CHAR, 256))

STRUCT(kbentry,
       TYPE_CHAR, TYPE_CHAR, TYPE_SHORT)

STRUCT(kbsentry,
       TYPE_CHAR, MK_ARRAY(TYPE_CHAR, 512))

STRUCT(audio_buf_info,
       TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT)

STRUCT(count_info,
       TYPE_INT, TYPE_INT, TYPE_INT)

STRUCT(buffmem_desc,
       TYPE_PTRVOID, TYPE_INT)

STRUCT(mixer_info,
       MK_ARRAY(TYPE_CHAR, 16), MK_ARRAY(TYPE_CHAR, 32), TYPE_INT, MK_ARRAY(TYPE_INT, 10))

STRUCT(snd_timer_id,
       TYPE_INT, /* dev_class */
       TYPE_INT, /* dev_sclass */
       TYPE_INT, /* card */
       TYPE_INT, /* device */
       TYPE_INT) /* subdevice */

STRUCT(snd_timer_ginfo,
       MK_STRUCT(STRUCT_snd_timer_id), /* tid */
       TYPE_INT, /* flags */
       TYPE_INT, /* card */
       MK_ARRAY(TYPE_CHAR, 64), /* id */
       MK_ARRAY(TYPE_CHAR, 80), /* name */
       TYPE_ULONG, /* reserved0 */
       TYPE_ULONG, /* resolution */
       TYPE_ULONG, /* resolution_min */
       TYPE_ULONG, /* resolution_max */
       TYPE_INT, /* clients */
       MK_ARRAY(TYPE_CHAR, 32)) /* reserved */

STRUCT(snd_timer_gparams,
       MK_STRUCT(STRUCT_snd_timer_id), /* tid */
       TYPE_ULONG, /* period_num */
       TYPE_ULONG, /* period_den */
       MK_ARRAY(TYPE_CHAR, 32)) /* reserved */

STRUCT(snd_timer_gstatus,
       MK_STRUCT(STRUCT_snd_timer_id), /* tid */
       TYPE_ULONG, /* resolution */
       TYPE_ULONG, /* resolution_num */
       TYPE_ULONG, /* resolution_den */
       MK_ARRAY(TYPE_CHAR, 32)) /* reserved */

STRUCT(snd_timer_select,
       MK_STRUCT(STRUCT_snd_timer_id), /* id */
       MK_ARRAY(TYPE_CHAR, 32)) /* reserved */

STRUCT(snd_timer_info,
       TYPE_INT, /* flags */
       TYPE_INT, /* card */
       MK_ARRAY(TYPE_CHAR, 64), /* id */
       MK_ARRAY(TYPE_CHAR, 80), /* name */
       TYPE_ULONG, /* reserved0 */
       TYPE_ULONG, /* resolution */
       MK_ARRAY(TYPE_CHAR, 64)) /* reserved */

STRUCT(snd_timer_params,
       TYPE_INT, /* flags */
       TYPE_INT, /* ticks */
       TYPE_INT, /* queue_size */
       TYPE_INT, /* reserved0 */
       TYPE_INT, /* filter */
       MK_ARRAY(TYPE_CHAR, 60)) /* reserved */

#if defined(TARGET_SPARC64) && !defined(TARGET_ABI32)
STRUCT(timeval,
       TYPE_LONG, /* tv_sec */
       TYPE_INT) /* tv_usec */

STRUCT(_kernel_sock_timeval,
       TYPE_LONG, /* tv_sec */
       TYPE_INT) /* tv_usec */
#else
STRUCT(timeval,
       TYPE_LONG, /* tv_sec */
       TYPE_LONG) /* tv_usec */

STRUCT(_kernel_sock_timeval,
       TYPE_LONGLONG, /* tv_sec */
       TYPE_LONGLONG) /* tv_usec */
#endif

STRUCT(timespec,
       TYPE_LONG, /* tv_sec */
       TYPE_LONG) /* tv_nsec */

STRUCT(_kernel_timespec,
       TYPE_LONGLONG, /* tv_sec */
       TYPE_LONGLONG) /* tv_nsec */

STRUCT(snd_timer_status,
       MK_STRUCT(STRUCT_timespec), /* tstamp */
       TYPE_INT, /* resolution */
       TYPE_INT, /* lost */
       TYPE_INT, /* overrun */
       TYPE_INT, /* queue */
       MK_ARRAY(TYPE_CHAR, 64)) /* reserved */

/* loop device ioctls */
STRUCT(loop_info,
       TYPE_INT,                 /* lo_number */
       TYPE_OLDDEVT,             /* lo_device */
       TYPE_ULONG,               /* lo_inode */
       TYPE_OLDDEVT,             /* lo_rdevice */
       TYPE_INT,                 /* lo_offset */
       TYPE_INT,                 /* lo_encrypt_type */
       TYPE_INT,                 /* lo_encrypt_key_size */
       TYPE_INT,                 /* lo_flags */
       MK_ARRAY(TYPE_CHAR, 64),  /* lo_name */
       MK_ARRAY(TYPE_CHAR, 32),  /* lo_encrypt_key */
       MK_ARRAY(TYPE_ULONG, 2),  /* lo_init */
       MK_ARRAY(TYPE_CHAR, 4))   /* reserved */

STRUCT(loop_info64,
       TYPE_ULONGLONG,           /* lo_device */
       TYPE_ULONGLONG,           /* lo_inode */
       TYPE_ULONGLONG,           /* lo_rdevice */
       TYPE_ULONGLONG,           /* lo_offset */
       TYPE_ULONGLONG,           /* lo_sizelimit */
       TYPE_INT,                 /* lo_number */
       TYPE_INT,                 /* lo_encrypt_type */
       TYPE_INT,                 /* lo_encrypt_key_size */
       TYPE_INT,                 /* lo_flags */
       MK_ARRAY(TYPE_CHAR, 64),  /* lo_name */
       MK_ARRAY(TYPE_CHAR, 64),  /* lo_crypt_name */
       MK_ARRAY(TYPE_CHAR, 32),  /* lo_encrypt_key */
       MK_ARRAY(TYPE_ULONGLONG, 2))  /* lo_init */

/* mag tape ioctls */
STRUCT(mtop, TYPE_SHORT, TYPE_INT)
STRUCT(mtget, TYPE_LONG, TYPE_LONG, TYPE_LONG, TYPE_LONG, TYPE_LONG,
       TYPE_INT, TYPE_INT)
STRUCT(mtpos, TYPE_LONG)

STRUCT(fb_fix_screeninfo,
       MK_ARRAY(TYPE_CHAR, 16), /* id */
       TYPE_ULONG, /* smem_start */
       TYPE_INT, /* smem_len */
       TYPE_INT, /* type */
       TYPE_INT, /* type_aux */
       TYPE_INT, /* visual */
       TYPE_SHORT, /* xpanstep */
       TYPE_SHORT, /* ypanstep */
       TYPE_SHORT, /* ywrapstep */
       TYPE_INT, /* line_length */
       TYPE_ULONG, /* mmio_start */
       TYPE_INT, /* mmio_len */
       TYPE_INT, /* accel */
       MK_ARRAY(TYPE_CHAR, 3)) /* reserved */

STRUCT(fb_var_screeninfo,
       TYPE_INT, /* xres */
       TYPE_INT, /* yres */
       TYPE_INT, /* xres_virtual */
       TYPE_INT, /* yres_virtual */
       TYPE_INT, /* xoffset */
       TYPE_INT, /* yoffset */
       TYPE_INT, /* bits_per_pixel */
       TYPE_INT, /* grayscale */
       MK_ARRAY(TYPE_INT, 3), /* red */
       MK_ARRAY(TYPE_INT, 3), /* green */
       MK_ARRAY(TYPE_INT, 3), /* blue */
       MK_ARRAY(TYPE_INT, 3), /* transp */
       TYPE_INT, /* nonstd */
       TYPE_INT, /* activate */
       TYPE_INT, /* height */
       TYPE_INT, /* width */
       TYPE_INT, /* accel_flags */
       TYPE_INT, /* pixclock */
       TYPE_INT, /* left_margin */
       TYPE_INT, /* right_margin */
       TYPE_INT, /* upper_margin */
       TYPE_INT, /* lower_margin */
       TYPE_INT, /* hsync_len */
       TYPE_INT, /* vsync_len */
       TYPE_INT, /* sync */
       TYPE_INT, /* vmode */
       TYPE_INT, /* rotate */
       MK_ARRAY(TYPE_INT, 5)) /* reserved */

STRUCT(fb_cmap,
       TYPE_INT, /* start  */
       TYPE_INT, /* len    */
       TYPE_PTRVOID, /* red    */
       TYPE_PTRVOID, /* green  */
       TYPE_PTRVOID, /* blue   */
       TYPE_PTRVOID) /* transp */

STRUCT(fb_con2fbmap,
       TYPE_INT, /* console     */
       TYPE_INT) /* framebuffer */


STRUCT(vt_stat,
       TYPE_SHORT, /* v_active */
       TYPE_SHORT, /* v_signal */
       TYPE_SHORT) /* v_state */

STRUCT(vt_mode,
       TYPE_CHAR,  /* mode   */
       TYPE_CHAR,  /* waitv  */
       TYPE_SHORT, /* relsig */
       TYPE_SHORT, /* acqsig */
       TYPE_SHORT) /* frsig  */

STRUCT(dm_ioctl,
       MK_ARRAY(TYPE_INT, 3), /* version */
       TYPE_INT, /* data_size */
       TYPE_INT, /* data_start */
       TYPE_INT, /* target_count*/
       TYPE_INT, /* open_count */
       TYPE_INT, /* flags */
       TYPE_INT, /* event_nr */
       TYPE_INT, /* padding */
       TYPE_ULONGLONG, /* dev */
       MK_ARRAY(TYPE_CHAR, 128), /* name */
       MK_ARRAY(TYPE_CHAR, 129), /* uuid */
       MK_ARRAY(TYPE_CHAR, 7)) /* data */

STRUCT(dm_target_spec,
       TYPE_ULONGLONG, /* sector_start */
       TYPE_ULONGLONG, /* length */
       TYPE_INT, /* status */
       TYPE_INT, /* next */
       MK_ARRAY(TYPE_CHAR, 16)) /* target_type */

STRUCT(dm_target_deps,
       TYPE_INT, /* count */
       TYPE_INT) /* padding */

STRUCT(dm_name_list,
       TYPE_ULONGLONG, /* dev */
       TYPE_INT) /* next */

STRUCT(dm_target_versions,
       TYPE_INT, /* next */
       MK_ARRAY(TYPE_INT, 3)) /* version*/

STRUCT(dm_target_msg,
       TYPE_ULONGLONG) /* sector */

STRUCT(drm_version,
       TYPE_INT, /* version_major */
       TYPE_INT, /* version_minor */
       TYPE_INT, /* version_patchlevel */
       TYPE_ULONG, /* name_len */
       TYPE_PTRVOID, /* name */
       TYPE_ULONG, /* date_len */
       TYPE_PTRVOID, /* date */
       TYPE_ULONG, /* desc_len */
       TYPE_PTRVOID) /* desc */

STRUCT(drm_i915_getparam,
       TYPE_INT, /* param */
       TYPE_PTRVOID) /* value */

STRUCT(file_clone_range,
       TYPE_LONGLONG, /* src_fd */
       TYPE_ULONGLONG, /* src_offset */
       TYPE_ULONGLONG, /* src_length */
       TYPE_ULONGLONG) /* dest_offset */

STRUCT(file_dedupe_range_info,
       TYPE_LONGLONG,  /* dest_fd */
       TYPE_ULONGLONG, /* dest_offset */
       TYPE_ULONGLONG, /* byte_deduped */
       TYPE_INT,       /* status */
       TYPE_INT)      /* reserved */

STRUCT(file_dedupe_range,
       TYPE_ULONGLONG, /* src_offset */
       TYPE_ULONGLONG, /* src_length */
       TYPE_SHORT,     /* dest_count */
       TYPE_SHORT,     /* reserved1  */
       TYPE_INT,      /* reserved2  */
       MK_ARRAY(MK_STRUCT(STRUCT_file_dedupe_range_info),0)
       )

STRUCT(fiemap_extent,
       TYPE_ULONGLONG, /* fe_logical */
       TYPE_ULONGLONG, /* fe_physical */
       TYPE_ULONGLONG, /* fe_length */
       MK_ARRAY(TYPE_ULONGLONG, 2), /* fe_reserved64[2] */
       TYPE_INT, /* fe_flags */
       MK_ARRAY(TYPE_INT, 3)) /* fe_reserved[3] */

STRUCT(fiemap,
       TYPE_ULONGLONG, /* fm_start */
       TYPE_ULONGLONG, /* fm_length */
       TYPE_INT, /* fm_flags */
       TYPE_INT, /* fm_mapped_extents */
       TYPE_INT, /* fm_extent_count */
       TYPE_INT) /* fm_reserved */

STRUCT(blkpg_partition,
       TYPE_LONGLONG, /* start */
       TYPE_LONGLONG, /* length */
       TYPE_INT, /* pno */
       MK_ARRAY(TYPE_CHAR, BLKPG_DEVNAMELTH), /* devname */
       MK_ARRAY(TYPE_CHAR, BLKPG_VOLNAMELTH)) /* volname */

#if defined(BTRFS_IOC_SUBVOL_CREATE) || defined(BTRFS_IOC_SNAP_CREATE) || \
    defined(BTRFS_IOC_SNAP_DESTROY)  || defined(BTRFS_IOC_SCAN_DEV)  || \
    defined(BTRFS_IOC_FORGET_DEV)    || defined(BTRFS_IOC_ADD_DEV) || \
    defined(BTRFS_IOC_RM_DEV)        || defined(BTRFS_IOC_DEV_INFO)
STRUCT(btrfs_ioctl_vol_args,
       TYPE_LONGLONG, /* fd */
       MK_ARRAY(TYPE_CHAR, BTRFS_PATH_NAME_MAX + 1)) /* name */
#endif

#ifdef BTRFS_IOC_GET_SUBVOL_INFO
STRUCT(btrfs_ioctl_timespec,
       TYPE_ULONGLONG, /* sec */
       TYPE_INT) /* nsec */

STRUCT(btrfs_ioctl_get_subvol_info_args,
       TYPE_ULONGLONG, /* treeid */
       MK_ARRAY(TYPE_CHAR, BTRFS_VOL_NAME_MAX + 1),
       TYPE_ULONGLONG, /* parentid */
       TYPE_ULONGLONG, /* dirid */
       TYPE_ULONGLONG, /* generation */
       TYPE_ULONGLONG, /* flags */
       MK_ARRAY(TYPE_CHAR, BTRFS_UUID_SIZE), /* uuid */
       MK_ARRAY(TYPE_CHAR, BTRFS_UUID_SIZE), /* parent_uuid */
       MK_ARRAY(TYPE_CHAR, BTRFS_UUID_SIZE), /* received_uuid */
       TYPE_ULONGLONG, /* ctransid */
       TYPE_ULONGLONG, /* otransid */
       TYPE_ULONGLONG, /* stransid */
       TYPE_ULONGLONG, /* rtransid */
       MK_STRUCT(STRUCT_btrfs_ioctl_timespec), /* ctime */
       MK_STRUCT(STRUCT_btrfs_ioctl_timespec), /* otime */
       MK_STRUCT(STRUCT_btrfs_ioctl_timespec), /* stime */
       MK_STRUCT(STRUCT_btrfs_ioctl_timespec), /* rtime */
       MK_ARRAY(TYPE_ULONGLONG, 8)) /* reserved */
#endif

#ifdef BTRFS_IOC_INO_LOOKUP
STRUCT(btrfs_ioctl_ino_lookup_args,
       TYPE_ULONGLONG, /* treeid */
       TYPE_ULONGLONG, /* objectid */
       MK_ARRAY(TYPE_CHAR, BTRFS_INO_LOOKUP_PATH_MAX)) /* name */
#endif

#ifdef BTRFS_IOC_INO_PATHS
STRUCT(btrfs_ioctl_ino_path_args,
       TYPE_ULONGLONG, /* inum */
       TYPE_ULONGLONG, /* size */
       MK_ARRAY(TYPE_ULONGLONG, 4), /* reserved */
       TYPE_ULONGLONG) /* fspath */
#endif

#if defined(BTRFS_IOC_LOGICAL_INO) || defined(BTRFS_IOC_LOGICAL_INO_V2)
STRUCT(btrfs_ioctl_logical_ino_args,
       TYPE_ULONGLONG, /* logical */
       TYPE_ULONGLONG, /* size */
       MK_ARRAY(TYPE_ULONGLONG, 3), /* reserved */
       TYPE_ULONGLONG, /* flags */
       TYPE_ULONGLONG) /* inodes */
#endif

#ifdef BTRFS_IOC_INO_LOOKUP_USER
STRUCT(btrfs_ioctl_ino_lookup_user_args,
       TYPE_ULONGLONG, /* dirid */
       TYPE_ULONGLONG, /* treeid */
       MK_ARRAY(TYPE_CHAR, BTRFS_VOL_NAME_MAX + 1), /* name */
       MK_ARRAY(TYPE_CHAR, BTRFS_INO_LOOKUP_USER_PATH_MAX)) /* path */
#endif

#if defined(BTRFS_IOC_SCRUB) || defined(BTRFS_IOC_SCRUB_PROGRESS)
STRUCT(btrfs_scrub_progress,
       TYPE_ULONGLONG, /* data_extents_scrubbed */
       TYPE_ULONGLONG, /* tree_extents_scrubbed */
       TYPE_ULONGLONG, /* data_bytes_scrubbed */
       TYPE_ULONGLONG, /* tree_bytes_scrubbed */
       TYPE_ULONGLONG, /* read_errors */
       TYPE_ULONGLONG, /* csum_errors */
       TYPE_ULONGLONG, /* verify_errors */
       TYPE_ULONGLONG, /* no_csum */
       TYPE_ULONGLONG, /* csum_discards */
       TYPE_ULONGLONG, /* super_errors */
       TYPE_ULONGLONG, /* malloc_errors */
       TYPE_ULONGLONG, /* uncorrectable_errors */
       TYPE_ULONGLONG, /* corrected_er */
       TYPE_ULONGLONG, /* last_physical */
       TYPE_ULONGLONG) /* unverified_errors */

STRUCT(btrfs_ioctl_scrub_args,
       TYPE_ULONGLONG, /* devid */
       TYPE_ULONGLONG, /* start */
       TYPE_ULONGLONG, /* end */
       TYPE_ULONGLONG, /* flags */
       MK_STRUCT(STRUCT_btrfs_scrub_progress), /* progress */
       MK_ARRAY(TYPE_ULONGLONG,
                (1024 - 32 -
                 sizeof(struct btrfs_scrub_progress)) / 8)) /* unused */
#endif

#ifdef BTRFS_IOC_DEV_INFO
STRUCT(btrfs_ioctl_dev_info_args,
       TYPE_ULONGLONG, /* devid */
       MK_ARRAY(TYPE_CHAR, BTRFS_UUID_SIZE), /* uuid */
       TYPE_ULONGLONG, /* bytes_used */
       TYPE_ULONGLONG, /* total_bytes */
       MK_ARRAY(TYPE_ULONGLONG, 379), /* unused */
       MK_ARRAY(TYPE_CHAR, BTRFS_DEVICE_PATH_NAME_MAX)) /* path */
#endif

#ifdef BTRFS_IOC_GET_SUBVOL_ROOTREF
STRUCT(rootref,
       TYPE_ULONGLONG, /* treeid */
       TYPE_ULONGLONG) /* dirid */

STRUCT(btrfs_ioctl_get_subvol_rootref_args,
       TYPE_ULONGLONG, /* min_treeid */
       MK_ARRAY(MK_STRUCT(STRUCT_rootref),
                BTRFS_MAX_ROOTREF_BUFFER_NUM), /* rootref */
       TYPE_CHAR, /* num_items */
       MK_ARRAY(TYPE_CHAR, 7)) /* align */
#endif

#ifdef BTRFS_IOC_GET_DEV_STATS
STRUCT(btrfs_ioctl_get_dev_stats,
       TYPE_ULONGLONG, /* devid */
       TYPE_ULONGLONG, /* nr_items */
       TYPE_ULONGLONG, /* flags */
       MK_ARRAY(TYPE_ULONGLONG, BTRFS_DEV_STAT_VALUES_MAX), /* values */
       MK_ARRAY(TYPE_ULONGLONG,
                128 - 2 - BTRFS_DEV_STAT_VALUES_MAX)) /* unused */
#endif

STRUCT(btrfs_ioctl_quota_ctl_args,
       TYPE_ULONGLONG, /* cmd */
       TYPE_ULONGLONG) /* status */

STRUCT(btrfs_ioctl_quota_rescan_args,
       TYPE_ULONGLONG, /* flags */
       TYPE_ULONGLONG, /* progress */
       MK_ARRAY(TYPE_ULONGLONG, 6)) /* reserved */

STRUCT(btrfs_ioctl_qgroup_assign_args,
       TYPE_ULONGLONG, /* assign */
       TYPE_ULONGLONG, /* src */
       TYPE_ULONGLONG) /* dst */

STRUCT(btrfs_ioctl_qgroup_create_args,
       TYPE_ULONGLONG, /* create */
       TYPE_ULONGLONG) /* qgroupid */

STRUCT(btrfs_qgroup_limit,
       TYPE_ULONGLONG, /* flags */
       TYPE_ULONGLONG, /* max_rfer */
       TYPE_ULONGLONG, /* max_excl */
       TYPE_ULONGLONG, /* rsv_rfer */
       TYPE_ULONGLONG) /* rsv_excl */

STRUCT(btrfs_ioctl_qgroup_limit_args,
       TYPE_ULONGLONG, /* qgroupid */
       MK_STRUCT(STRUCT_btrfs_qgroup_limit)) /* lim */

STRUCT(btrfs_ioctl_feature_flags,
       TYPE_ULONGLONG, /* compat_flags */
       TYPE_ULONGLONG, /* compat_ro_flags */
       TYPE_ULONGLONG) /* incompat_flags */

STRUCT(rtc_time,
       TYPE_INT, /* tm_sec */
       TYPE_INT, /* tm_min */
       TYPE_INT, /* tm_hour */
       TYPE_INT, /* tm_mday */
       TYPE_INT, /* tm_mon */
       TYPE_INT, /* tm_year */
       TYPE_INT, /* tm_wday */
       TYPE_INT, /* tm_yday */
       TYPE_INT) /* tm_isdst */

STRUCT(rtc_wkalrm,
       TYPE_CHAR, /* enabled */
       TYPE_CHAR, /* pending */
       MK_STRUCT(STRUCT_rtc_time)) /* time */

STRUCT(rtc_pll_info,
       TYPE_INT, /* pll_ctrl */
       TYPE_INT, /* pll_value */
       TYPE_INT, /* pll_max */
       TYPE_INT, /* pll_min */
       TYPE_INT, /* pll_posmult */
       TYPE_INT, /* pll_negmult */
       TYPE_LONG) /* pll_clock */

STRUCT(blkpg_ioctl_arg,
       TYPE_INT, /* op */
       TYPE_INT, /* flags */
       TYPE_INT, /* datalen */
       TYPE_PTRVOID) /* data */

STRUCT(format_descr,
       TYPE_INT,     /* device */
       TYPE_INT,     /* head */
       TYPE_INT)     /* track */

STRUCT(floppy_max_errors,
       TYPE_INT, /* abort */
       TYPE_INT, /* read_track */
       TYPE_INT, /* reset */
       TYPE_INT, /* recal */
       TYPE_INT) /* reporting */

STRUCT(drm_unique,
       TYPE_LONG, /* unique_len (size_t) */
       TYPE_PTRVOID) /* unique (char *) */

STRUCT(drm_auth,
       TYPE_INT) /* magic (u32) */

STRUCT(drm_block,
       TYPE_INT) /* unused */

STRUCT(drm_control,
       TYPE_INT, /* func (enum) */
       TYPE_INT) /* irq */

STRUCT(drm_irq_busid,
       TYPE_INT, /* irq */
       TYPE_INT, /* busnum */
       TYPE_INT, /* devnum */
       TYPE_INT) /* funcnum */

STRUCT(drm_map,
       TYPE_ULONG, /* offset */
       TYPE_ULONG, /* size */
       TYPE_INT, /* type (enum drm_map_type) */
       TYPE_INT, /* flags (enum drm_map_flags) */
       TYPE_PTRVOID, /* handle */
       TYPE_INT) /* mtrr */

STRUCT(drm_buf_desc,
       TYPE_INT, /* count */
       TYPE_INT, /* size */
       TYPE_INT, /* low_mark */
       TYPE_INT, /* high_mark */
       TYPE_INT, /* flags (enum) */
       TYPE_ULONG) /* agp_start */

STRUCT(drm_buf_info,
       TYPE_INT, /* count */
       TYPE_PTRVOID) /* list (struct drm_buf_desc *) */

STRUCT(drm_buf_pub,
       TYPE_INT, /* idx */
       TYPE_INT, /* total */
       TYPE_INT, /* used */
       TYPE_PTRVOID) /* address (void *) */

STRUCT(drm_buf_map,
       TYPE_INT, /* count */
       TYPE_PTRVOID, /* virtual */
       TYPE_PTRVOID) /* list (struct drm_buf_pub *) */

STRUCT(drm_buf_free,
       TYPE_INT, /* count */
       TYPE_PTRVOID) /* list (int *) */

STRUCT(drm_ctx_priv_map,
       TYPE_INT, /* ctx_id (u32) */
       TYPE_PTRVOID) /* handle */

STRUCT(drm_client,
       TYPE_INT, /* idx */
       TYPE_INT, /* auth */
       TYPE_ULONG, /* pid */
       TYPE_ULONG, /* uid */
       TYPE_ULONG, /* magic */
       TYPE_ULONG) /* iocs */

STRUCT(drm_stats_internal,
       TYPE_ULONG, /* value */
       TYPE_INT) /* type (enum drm_stat_type) */

STRUCT(drm_stats,
       TYPE_ULONG, /* count */
       MK_ARRAY(MK_STRUCT(STRUCT_drm_stats_internal), 15)) /* data */

STRUCT(drm_set_version,
       TYPE_INT, /* drm_di_major */
       TYPE_INT, /* drm_di_minor */
       TYPE_INT, /* drm_dd_major */
       TYPE_INT) /* drm_dd_minor */

STRUCT(drm_modeset_ctl,
       TYPE_INT, /* crtc (u32) */
       TYPE_INT) /* cmd (u32) */

STRUCT(drm_gem_close,
       TYPE_INT, /* handle (u32) */
       TYPE_INT) /* pad (u32) */

STRUCT(drm_gem_flink,
       TYPE_INT, /* handle (u32) */
       TYPE_INT) /* name (u32) */

STRUCT(drm_gem_open,
       TYPE_INT, /* name (u32) */
       TYPE_INT, /* handle (u32) */
       TYPE_ULONGLONG) /* size (u64) */

STRUCT(drm_get_cap,
       TYPE_ULONGLONG, /* capability (u64) */
       TYPE_ULONGLONG) /* value (u64) */

STRUCT(drm_set_client_cap,
       TYPE_ULONGLONG, /* capability (u64) */
       TYPE_ULONGLONG) /* value (u64) */

STRUCT(drm_ctx,
       TYPE_INT, /* handle (u32) */
       TYPE_INT) /* flags (enum drm_ctx_flags) */

STRUCT(drm_ctx_res,
       TYPE_INT, /* count */
       TYPE_PTRVOID) /* contexts (struct drm_ctx *) */

STRUCT(drm_draw,
       TYPE_INT) /* handle (u32) */

STRUCT(drm_dma,
       TYPE_INT, /* context */
       TYPE_INT, /* send_count */
       TYPE_PTRVOID, /* send_indices */
       TYPE_PTRVOID, /* send_sizes */
       TYPE_INT, /* flags (enum drm_dma_flags) */
       TYPE_INT, /* request_count */
       TYPE_INT, /* request_size */
       TYPE_PTRVOID, /* request_indices */
       TYPE_PTRVOID, /* request_sizes */
       TYPE_INT) /* granted_count */

STRUCT(drm_lock,
       TYPE_INT, /* context */
       TYPE_INT) /* flags (enum drm_lock_flags) */

STRUCT(drm_prime_handle,
       TYPE_INT, /* handle (u32) */
       TYPE_INT, /* flags (u32) */
       TYPE_INT) /* fd (s32) */

STRUCT(drm_agp_mode,
       TYPE_ULONG) /* mode */

STRUCT(drm_agp_info,
       TYPE_INT, /* agp_version_major */
       TYPE_INT, /* agp_version_minor */
       TYPE_ULONG, /* mode */
       TYPE_ULONG, /* aperture_base */
       TYPE_ULONG, /* aperture_size */
       TYPE_ULONG, /* memory_allowed */
       TYPE_ULONG, /* memory_used */
       TYPE_SHORT, /* id_vendor (u16) */
       TYPE_SHORT) /* id_device (u16) */

STRUCT(drm_agp_buffer,
       TYPE_ULONG, /* size */
       TYPE_ULONG, /* handle */
       TYPE_ULONG, /* type */
       TYPE_ULONG) /* physical */

STRUCT(drm_agp_binding,
       TYPE_ULONG, /* handle */
       TYPE_ULONG) /* offset */

STRUCT(drm_scatter_gather,
       TYPE_ULONG, /* size */
       TYPE_ULONG) /* handle */

STRUCT(drm_wait_vblank_reply,
       TYPE_INT, /* type (enum drm_vblank_seq_type) */
       TYPE_INT, /* sequence (u32) */
       TYPE_ULONG, /* tval_sec */
       TYPE_ULONG) /* tval_usec */

STRUCT(drm_update_draw,
       TYPE_INT, /* handle (u32) */
       TYPE_INT, /* type (u32) */
       TYPE_INT, /* num (u32) */
       TYPE_ULONGLONG) /* data (u64) */

STRUCT(drm_mode_card_res,
       TYPE_ULONGLONG, /* fb_id_ptr (u64) */
       TYPE_ULONGLONG, /* crtc_id_ptr (u64) */
       TYPE_ULONGLONG, /* connector_id_ptr (u64) */
       TYPE_ULONGLONG, /* encoder_id_ptr (u64) */
       TYPE_INT, /* count_fbs (u32) */
       TYPE_INT, /* count_crtcs (u32) */
       TYPE_INT, /* count_connectors (u32) */
       TYPE_INT, /* count_encoders (u32) */
       TYPE_INT, /* min_width (u32) */
       TYPE_INT, /* max_width (u32) */
       TYPE_INT, /* min_height (u32) */
       TYPE_INT) /* max_height (u32) */

STRUCT(drm_mode_modeinfo,
       TYPE_INT, /* clock (u32) */
       TYPE_SHORT, /* hdisplay (u16) */
       TYPE_SHORT, /* hsync_start (u16) */
       TYPE_SHORT, /* hsync_end (u16) */
       TYPE_SHORT, /* htotal (u16) */
       TYPE_SHORT, /* hskew (u16) */
       TYPE_SHORT, /* vdisplay (u16) */
       TYPE_SHORT, /* vsync_start (u16) */
       TYPE_SHORT, /* vsync_end (u16) */
       TYPE_SHORT, /* vtotal (u16) */
       TYPE_SHORT, /* vscan (u16) */
       TYPE_INT, /* vrefresh (u32) */
       TYPE_INT, /* flags (u32) */
       TYPE_INT, /* type (u32) */
       MK_ARRAY(TYPE_CHAR, DRM_DISPLAY_MODE_LEN)) /* name */

STRUCT(drm_mode_crtc,
       TYPE_ULONGLONG, /* set_connectors_ptr (u64) */
       TYPE_INT, /* count_connectors (u32) */
       TYPE_INT, /* crtc_id (u32) */
       TYPE_INT, /* fb_id (u32) */
       TYPE_INT, /* x (u32) */
       TYPE_INT, /* y (u32) */
       TYPE_INT, /* gamma_size (u32) */
       TYPE_INT, /* mode_valid (u32) */
       MK_STRUCT(STRUCT_drm_mode_modeinfo)) /* mode */

STRUCT(drm_mode_cursor,
       TYPE_INT, /* flags (u32) */
       TYPE_INT, /* crtc_id (u32) */
       TYPE_INT, /* x (s32) */
       TYPE_INT, /* y (s32) */
       TYPE_INT, /* width (u32) */
       TYPE_INT, /* height (u32) */
       TYPE_INT) /* handle (u32) */

STRUCT(drm_mode_crtc_lut,
       TYPE_INT, /* crtc_id (u32) */
       TYPE_INT, /* gamma_size (u32) */
       TYPE_ULONGLONG, /* red (u64) */
       TYPE_ULONGLONG, /* green (u64) */
       TYPE_ULONGLONG) /* blue (u64) */

STRUCT(drm_mode_get_encoder,
       TYPE_INT, /* encoder_id (u32) */
       TYPE_INT, /* encoder_type (u32) */
       TYPE_INT, /* crtc_id (u32) */
       TYPE_INT, /* possible_crtcs (u32) */
       TYPE_INT) /* possible_clones (u32) */

STRUCT(drm_mode_get_connector,
       TYPE_ULONGLONG, /* encoders_ptr (u64) */
       TYPE_ULONGLONG, /* modes_ptr (u64) */
       TYPE_ULONGLONG, /* props_ptr (u64) */
       TYPE_ULONGLONG, /* prop_values_ptr (u64) */
       TYPE_INT, /* count_modes (u32) */
       TYPE_INT, /* count_props (u32) */
       TYPE_INT, /* count_encoders (u32) */
       TYPE_INT, /* encoder_id (u32) */
       TYPE_INT, /* connector_id (u32) */
       TYPE_INT, /* connector_type (u32) */
       TYPE_INT, /* connector_type_id (u32) */
       TYPE_INT, /* connection (u32) */
       TYPE_INT, /* mm_width (u32) */
       TYPE_INT, /* mm_height (u32) */
       TYPE_INT, /* subpixel (u32) */
       TYPE_INT) /* pad (u32) */

STRUCT(drm_mode_mode_cmd,
       TYPE_INT, /* connector_id (u32) */
       MK_STRUCT(STRUCT_drm_mode_modeinfo)) /* mode */

STRUCT(drm_mode_get_property,
       TYPE_ULONGLONG, /* values_ptr (u64) */
       TYPE_ULONGLONG, /* enum_blob_ptr (u64) */
       TYPE_INT, /* prop_id (u32) */
       TYPE_INT, /* flags (u32) */
       MK_ARRAY(TYPE_CHAR, DRM_PROP_NAME_LEN), /* name (u32) */
       TYPE_INT, /* count_values (u32) */
       TYPE_INT) /* count_enum_blobs (u32) */

STRUCT(drm_mode_connector_set_property,
       TYPE_ULONGLONG, /* value (u64) */
       TYPE_INT, /* prop_id (u32) */
       TYPE_INT) /* connector_id (u32) */

STRUCT(drm_mode_get_blob,
       TYPE_INT, /* blob_id (u32) */
       TYPE_INT, /* length (u32) */
       TYPE_ULONGLONG) /* data (u64) */

STRUCT(drm_mode_fb_cmd,
       TYPE_INT, /* width (u32) */
       TYPE_INT, /* height (u32) */
       TYPE_INT, /* pitch (u32) */
       TYPE_INT, /* bpp (u32) */
       TYPE_INT, /* depth (u32) */
       TYPE_INT, /* flags (u32) */
       TYPE_INT, /* flags (u32) */
       TYPE_INT) /* handle (u32) */

STRUCT(drm_mode_crtc_page_flip,
       TYPE_INT, /* crtc_id (u32) */
       TYPE_INT, /* fb_id (u32) */
       TYPE_INT, /* gamma_size (u32) */
       TYPE_INT, /* flags (u32) */
       TYPE_INT, /* reserved (u32) */
       TYPE_ULONGLONG) /* user_data (u64) */

STRUCT(drm_mode_fb_dirty_cmd,
       TYPE_INT, /* fb_id (u32) */
       TYPE_INT, /* flags (u32) */
       TYPE_INT, /* color (u32) */
       TYPE_INT, /* num_clips (u32) */
       TYPE_ULONGLONG) /* clips_ptr (u64) */

STRUCT(drm_mode_create_dumb,
       TYPE_INT, /* height (u32) */
       TYPE_INT, /* width (u32) */
       TYPE_INT, /* bpp (u32) */
       TYPE_INT, /* flags (u32) */
       TYPE_INT, /* handle (u32) */
       TYPE_INT, /* pitch (u32) */
       TYPE_ULONGLONG) /* size (u64) */

STRUCT(drm_mode_map_dumb,
       TYPE_INT, /* handle (u32) */
       TYPE_INT, /* pad (u32) */
       TYPE_ULONGLONG) /* offset (u64) */

STRUCT(drm_mode_destroy_dumb,
       TYPE_INT) /* handle (u32) */

STRUCT(drm_mode_get_plane_res,
       TYPE_ULONGLONG, /* plane_id_ptr (u64) */
       TYPE_INT) /* count_planes (u32) */

STRUCT(drm_mode_get_plane,
       TYPE_INT, /* plane_id (u32) */
       TYPE_INT, /* crtc_id (u32) */
       TYPE_INT, /* fb_id (u32) */
       TYPE_INT, /* possible_crtcs (u32) */
       TYPE_INT, /* gamma_size (u32) */
       TYPE_INT, /* count_format_types (u32) */
       TYPE_INT, /* flags (u32) */
       TYPE_ULONGLONG) /* format_type_ptr (u64) */

STRUCT(drm_mode_set_plane,
       TYPE_INT, /* plane_id (u32) */
       TYPE_INT, /* crtc_id (u32) */
       TYPE_INT, /* fb_id (u32) */
       TYPE_INT, /* flags (u32) */
       TYPE_INT, /* crtc_x (s32) */
       TYPE_INT, /* crtc_y (s32) */
       TYPE_INT, /* crtc_w (u32) */
       TYPE_INT, /* crtc_h (u32) */
       TYPE_INT, /* src_x (u32) */
       TYPE_INT, /* src_y (u32) */
       TYPE_INT, /* src_h (u32) */
       TYPE_INT) /* src_w (u32) */

STRUCT(drm_mode_fb_cmd2,
       TYPE_INT, /* fb_id (u32) */
       TYPE_INT, /* width (u32) */
       TYPE_INT, /* height (u32) */
       TYPE_INT, /* pixel_format (u32) */
       TYPE_INT, /* flags (u32) */
       MK_ARRAY(TYPE_INT, 4), /* handles (u32) */
       MK_ARRAY(TYPE_INT, 4), /* pitches (u32) */
       MK_ARRAY(TYPE_INT, 4)) /* offsets (u32) */

STRUCT(drm_mode_obj_get_properties,
       TYPE_ULONGLONG, /* props_ptr (u64) */
       TYPE_ULONGLONG, /* prop_values_ptr (u64) */
       TYPE_INT, /* count_props (u32) */
       TYPE_INT, /* obj_id (u32) */
       TYPE_INT) /* obj_type (u32) */

STRUCT(drm_mode_obj_set_property,
       TYPE_ULONGLONG, /* value (u64) */
       TYPE_INT, /* prop_id (u32) */
       TYPE_INT, /* obj_id (u32) */
       TYPE_INT) /* obj_type (u32) */

STRUCT(drm_mode_cursor2,
       TYPE_INT, /* flags (u32) */
       TYPE_INT, /* crtc_id (u32) */
       TYPE_INT, /* x (s32) */
       TYPE_INT, /* y (s32) */
       TYPE_INT, /* width (u32) */
       TYPE_INT, /* height (u32) */
       TYPE_INT, /* handle (u32) */
       TYPE_INT, /* hot_x (s32) */
       TYPE_INT) /* hot_y (s32) */

STRUCT(drm_radeon_init,
       TYPE_INT,
       TYPE_ULONG,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_ULONG,
       TYPE_ULONG,
       TYPE_ULONG,
       TYPE_ULONG,
       TYPE_ULONG,
       TYPE_ULONG)

STRUCT(drm_radeon_cp_stop,
       TYPE_ULONGLONG,
       TYPE_ULONGLONG)

STRUCT(drm_radeon_fullscreen,
       TYPE_INT)

STRUCT(drm_radeon_clear,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       MK_STRUCT(STRUCT_drm_radeon_clear_rect_t))

STRUCT(drm_radeon_clear_rect_t,
       MK_ARRAY(TYPE_CHAR, 5),
       MK_ARRAY(TYPE_CHAR, 5))


STRUCT(drm_radeon_vertex,
       TYPE_ULONGLONG,
       TYPE_ULONGLONG,
       TYPE_ULONGLONG,
       TYPE_ULONGLONG)

STRUCT(drm_radeon_indices,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT)

STRUCT(drm_radeon_stipple,
       TYPE_PTRVOID)

STRUCT(drm_radeon_indirect,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT)

//STRUCT(drm_radeon_texture_t,

//STRUCT(drm_radeon_vertex2_t,

//STRUCT(drm_radeon_cmd_buffer_t,

STRUCT(drm_radeon_getparam,
       TYPE_INT,
       TYPE_PTRVOID)

STRUCT(drm_radeon_mem_alloc,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT)

STRUCT(drm_radeon_mem_free,
       TYPE_INT,
       TYPE_INT)

STRUCT(drm_radeon_mem_init_heap,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT)

STRUCT(drm_radeon_irq_emit,
       TYPE_PTRVOID)

STRUCT(drm_radeon_irq_wait,
       TYPE_INT)

STRUCT(drm_radeon_setparam,
       TYPE_INT,
       TYPE_LONGLONG)

STRUCT(drm_radeon_surface_alloc,
       TYPE_ULONG,
       TYPE_ULONG,
       TYPE_ULONG)


STRUCT(drm_radeon_gem_info,
       TYPE_ULONGLONG, /* gart_size (u64) */
       TYPE_ULONGLONG, /* vram_size (u64) */
       TYPE_ULONGLONG) /* vram_visible (u64) */

STRUCT(drm_radeon_gem_create,
       TYPE_ULONGLONG, /* size (u64) */
       TYPE_ULONGLONG, /* alignment (u64) */
       TYPE_INT,       /* handle (u32) */
       TYPE_INT,       /* initla_domain */
       TYPE_INT)       /* flags (u32) */

STRUCT(drm_radeon_gem_mmap,
       TYPE_INT, /* handle (u32) */
       TYPE_INT, /* pad (u32)*/
       TYPE_ULONGLONG, /* offset (u64) */
       TYPE_ULONGLONG, /* size (u64) */
       TYPE_ULONGLONG) /* addr_ptr (u64) */

STRUCT(drm_radeon_gem_pread,
       TYPE_INT, /* handle (u32) */
       TYPE_INT, /* pad (u32)*/
       TYPE_ULONGLONG, /* offset (u64) */
       TYPE_ULONGLONG, /* size (u64) */
       TYPE_ULONGLONG) /* addr_ptr (u64) */

STRUCT(drm_radeon_gem_pwrite,
       TYPE_INT, /* handle (u32) */
       TYPE_INT, /* pad (u32)*/
       TYPE_ULONGLONG, /* offset (u64) */
       TYPE_ULONGLONG, /* size (u64) */
       TYPE_ULONGLONG) /* addr_ptr (u64) */

STRUCT(drm_radeon_gem_set_domain,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT)

STRUCT(drm_radeon_gem_wait_idle,
       TYPE_INT, /* handle (u32) */
       TYPE_INT) /* pad (u32) */

STRUCT(drm_radeon_cs,
       TYPE_INT,
       TYPE_INT,
       TYPE_ULONGLONG,
       TYPE_ULONGLONG,
       TYPE_ULONGLONG)

STRUCT(drm_radeon_info,
       TYPE_INT, /* reqeust (u32) */
       TYPE_INT, /* pad (u32) */
       TYPE_ULONGLONG) /* value (u64) */

STRUCT(drm_radeon_gem_set_tiling,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT)

STRUCT(drm_radeon_gem_get_tiling,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT)

STRUCT(drm_radeon_gem_busy,
       TYPE_INT, /* handle (u32) */
       TYPE_INT) /* domain (u32) */

STRUCT(drm_radeon_gem_va,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_ULONGLONG)

STRUCT(drm_radeon_gem_op,
       TYPE_INT,
       TYPE_INT,
       TYPE_ULONGLONG)

STRUCT(drm_radeon_gem_userptr,
       TYPE_ULONGLONG, /* addr (u64) */
       TYPE_ULONGLONG, /* size (u64) */
       TYPE_INT,       /* flags (u32) */
       TYPE_INT)       /* handle (u32) */

#if defined(CONFIG_USBFS)
/* usb device ioctls */
STRUCT(usbdevfs_ctrltransfer,
        TYPE_CHAR, /* bRequestType */
        TYPE_CHAR, /* bRequest */
        TYPE_SHORT, /* wValue */
        TYPE_SHORT, /* wIndex */
        TYPE_SHORT, /* wLength */
        TYPE_INT, /* timeout */
        TYPE_PTRVOID) /* data */

STRUCT(usbdevfs_bulktransfer,
        TYPE_INT, /* ep */
        TYPE_INT, /* len */
        TYPE_INT, /* timeout */
        TYPE_PTRVOID) /* data */

STRUCT(usbdevfs_setinterface,
        TYPE_INT, /* interface */
        TYPE_INT) /* altsetting */

STRUCT(usbdevfs_disconnectsignal,
        TYPE_INT, /* signr */
        TYPE_PTRVOID) /* context */

STRUCT(usbdevfs_getdriver,
        TYPE_INT, /* interface */
        MK_ARRAY(TYPE_CHAR, USBDEVFS_MAXDRIVERNAME + 1)) /* driver */

STRUCT(usbdevfs_connectinfo,
        TYPE_INT, /* devnum */
        TYPE_CHAR) /* slow */

STRUCT(usbdevfs_iso_packet_desc,
        TYPE_INT, /* length */
        TYPE_INT, /* actual_length */
        TYPE_INT) /* status */

STRUCT(usbdevfs_urb,
        TYPE_CHAR, /* type */
        TYPE_CHAR, /* endpoint */
        TYPE_INT, /* status */
        TYPE_INT, /* flags */
        TYPE_PTRVOID, /* buffer */
        TYPE_INT, /* buffer_length */
        TYPE_INT, /* actual_length */
        TYPE_INT, /* start_frame */
        TYPE_INT, /* union number_of_packets stream_id */
        TYPE_INT, /* error_count */
        TYPE_INT, /* signr */
        TYPE_PTRVOID, /* usercontext */
        MK_ARRAY(MK_STRUCT(STRUCT_usbdevfs_iso_packet_desc), 0)) /* desc */

STRUCT(usbdevfs_ioctl,
        TYPE_INT, /* ifno */
        TYPE_INT, /* ioctl_code */
        TYPE_PTRVOID) /* data */

STRUCT(usbdevfs_hub_portinfo,
        TYPE_CHAR, /* nports */
        MK_ARRAY(TYPE_CHAR, 127)) /* port */

STRUCT(usbdevfs_disconnect_claim,
        TYPE_INT, /* interface */
        TYPE_INT, /* flags */
        MK_ARRAY(TYPE_CHAR, USBDEVFS_MAXDRIVERNAME + 1)) /* driver */
#endif /* CONFIG_USBFS */

STRUCT(v4l2_capability,
        MK_ARRAY(TYPE_CHAR, 16),
        MK_ARRAY(TYPE_CHAR, 32),
        MK_ARRAY(TYPE_CHAR, 32),
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 3))

STRUCT(v4l2_query_ext_ctrl,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 32),
        TYPE_LONGLONG,
        TYPE_LONGLONG,
        TYPE_LONGLONG,
        TYPE_LONGLONG,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 4),
        MK_ARRAY(TYPE_INT, 32))

STRUCT(v4l2_ext_controls,
        TYPE_INT, /*unoin*/
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 2),
        TYPE_PTRVOID)

STRUCT(v4l2_control,
        TYPE_INT,
        TYPE_INT)

STRUCT(v4l2_queryctrl,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 32),
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 2)
)
STRUCT(v4l2_rect,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT)

STRUCT(v4l2_selection,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_STRUCT(STRUCT_v4l2_rect),
        MK_ARRAY(TYPE_INT, 9)
)

STRUCT(v4l2_format,
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 200)
)

STRUCT(v4l2_tuner,
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 32),
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 4))

STRUCT(v4l2_frequency,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 8))

STRUCT(v4l2_modulator,
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 32),
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 3))

STRUCT(v4l2_input,
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 32),
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_ULONGLONG,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 3))

STRUCT(v4l2_streamparm,
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 200))

STRUCT(v4l2_fract,
        TYPE_INT,
        TYPE_INT)

STRUCT(v4l2_cropcap,
        TYPE_INT,
        MK_STRUCT(STRUCT_v4l2_rect),
        MK_STRUCT(STRUCT_v4l2_rect),
        MK_STRUCT(STRUCT_v4l2_fract))

STRUCT(v4l2_fmtdesc,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 32),
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 4))

STRUCT(fmt,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT)

STRUCT(v4l2_framebuffer,
        TYPE_INT,
        TYPE_INT,
        TYPE_PTRVOID,
        MK_STRUCT(STRUCT_fmt))

STRUCT(v4l2_audio,
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 32),
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 2))

STRUCT(v4l2_audioout,
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 32),
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 2))
STRUCT(v4l2_crop,
        TYPE_INT,
        MK_STRUCT(STRUCT_v4l2_rect))

STRUCT(v4l2_jpegcompression,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 60),
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 60),
        TYPE_INT)

STRUCT(v4l2_dv_timings,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 32))

STRUCT(v4l2_dv_timings_cap,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 2),
        MK_ARRAY(TYPE_INT, 32))
STRUCT(v4l2_standard,
        TYPE_INT,
        TYPE_ULONGLONG,
        MK_ARRAY(TYPE_CHAR, 24),
        MK_STRUCT(STRUCT_v4l2_fract),
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 4))
STRUCT(v4l2_requestbuffers,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 2))

STRUCT(v4l2_timecode,
        TYPE_INT,
        TYPE_INT,
        TYPE_CHAR,
        TYPE_CHAR,
        TYPE_CHAR,
        TYPE_CHAR,
        MK_ARRAY(TYPE_CHAR, 4))

STRUCT(v4l2_buffer,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_STRUCT(STRUCT_timeval),
        MK_STRUCT(STRUCT_v4l2_timecode),
        TYPE_INT,
        TYPE_INT,
        TYPE_ULONG,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT)

STRUCT(v4l2_exportbuffer,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 11))

STRUCT(v4l2_frmsize_discrete,
        TYPE_INT,
        TYPE_INT)

STRUCT(v4l2_frmsize_stepwise,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT)

STRUCT(v4l2_frmsizeenum,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_STRUCT(STRUCT_v4l2_frmsize_stepwise),
        MK_ARRAY(TYPE_INT, 2))

STRUCT(v4l2_querymenu,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 32),
        TYPE_INT)

STRUCT(v4l2_event_subscription,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 5))

STRUCT(v4l2_sliced_vbi_cap,
TYPE_SHORT,
TYPE_SHORT,
MK_ARRAY(TYPE_SHORT, 48),
TYPE_INT,
        MK_ARRAY(TYPE_INT, 3))

STRUCT(v4l2_output,
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 32),
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_ULONGLONG,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 3))

STRUCT(v4l2_hw_freq_seek,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 5))

STRUCT(v4l2_enc_idx_entry,
        TYPE_ULONGLONG,
        TYPE_ULONGLONG,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 2))

STRUCT(v4l2_enc_idx,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 4),
        MK_ARRAY(MK_STRUCT(STRUCT_v4l2_enc_idx_entry), 64))

STRUCT(v4l2_encoder_cmd,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 8))

STRUCT(v4l2_edid,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 5),
        TYPE_PTRVOID)

STRUCT(v4l2_decoder_cmd,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 16))

STRUCT(v4l2_frmival_stepwise,
        MK_STRUCT(STRUCT_v4l2_fract),
        MK_STRUCT(STRUCT_v4l2_fract),
        MK_STRUCT(STRUCT_v4l2_fract))

STRUCT(v4l2_frmivalenum,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_STRUCT(STRUCT_v4l2_frmival_stepwise),
        MK_ARRAY(TYPE_INT, 2))

STRUCT(v4l2_create_buffers,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_STRUCT(STRUCT_v4l2_format),
        MK_ARRAY(TYPE_INT, 8))
STRUCT(v4l2_dbg_match,
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 32))


STRUCT(v4l2_dbg_register,
        MK_STRUCT(STRUCT_v4l2_create_buffers),
        TYPE_INT,
       TYPE_ULONGLONG,
       TYPE_ULONGLONG)

STRUCT(v4l2_enum_dv_timings,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 2),
        MK_STRUCT(STRUCT_v4l2_dv_timings))

STRUCT(v4l2_event,
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 64),
        TYPE_INT,
        TYPE_INT,
        MK_STRUCT(STRUCT_timespec),
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 8))


STRUCT(snd_seq_running_info,
        TYPE_CHAR,
        TYPE_CHAR,
        TYPE_CHAR,
        TYPE_CHAR,
        MK_ARRAY(TYPE_CHAR, 12))

STRUCT(snd_seq_client_info,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 64),
        TYPE_INT,
        MK_ARRAY(TYPE_CHAR, 8),
        MK_ARRAY(TYPE_CHAR, 32),
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        MK_ARRAY(TYPE_INT, 56))

STRUCT(snd_seq_addr,
        TYPE_CHAR,
        TYPE_CHAR)

STRUCT(snd_seq_port_info,
        MK_STRUCT(STRUCT_snd_seq_addr),
        MK_ARRAY(TYPE_CHAR, 64),
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_PTRVOID,
        TYPE_INT,
        TYPE_CHAR,
        MK_ARRAY(TYPE_CHAR, 59))

STRUCT(snd_seq_port_subscribe,
        MK_STRUCT(STRUCT_snd_seq_addr),
        MK_STRUCT(STRUCT_snd_seq_addr),
        TYPE_INT,
        TYPE_INT,
        TYPE_CHAR,
        MK_ARRAY(TYPE_CHAR, 3),
        MK_ARRAY(TYPE_CHAR, 64))

STRUCT(snd_ctl_card_info,
       TYPE_INT, /* card */
       TYPE_INT, /* pad */
       MK_ARRAY(TYPE_CHAR, 16), /* id */
       MK_ARRAY(TYPE_CHAR, 16), /* driver */
       MK_ARRAY(TYPE_CHAR, 32), /* name */
       MK_ARRAY(TYPE_CHAR, 80), /* longname */
       MK_ARRAY(TYPE_CHAR, 16), /* reserved_ */
       MK_ARRAY(TYPE_CHAR, 80), /* mixername */
       MK_ARRAY(TYPE_CHAR, 128)) /* components */

STRUCT(snd_pcm_info,
        TYPE_INT, /* device */
        TYPE_INT, /* subdevice */
        TYPE_INT, /* stream */
        TYPE_INT, /* card */
        MK_ARRAY(TYPE_CHAR, 64), /* id */
        MK_ARRAY(TYPE_CHAR, 80), /* name */
        MK_ARRAY(TYPE_CHAR, 32), /* subname */
        TYPE_INT, /* dev_class */
        TYPE_INT, /* dev_subclass */
        TYPE_INT, /* subdevices_count */
        TYPE_INT, /* subdevices_avail */
        MK_ARRAY(TYPE_CHAR, 16), /* sync */
        MK_ARRAY(TYPE_CHAR, 64)) /* reserved */

STRUCT(snd_pcm_sync_ptr,
        TYPE_INT, /* flags */
        MK_ARRAY(TYPE_CHAR, 64), /* reserved */
        MK_ARRAY(TYPE_CHAR, 64)) /* reserved */

STRUCT(snd_mask,
        MK_ARRAY(TYPE_INT, (SNDRV_MASK_MAX + 31) / 32))

STRUCT(snd_interval,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT)

STRUCT(snd_pcm_hw_params,
        TYPE_INT,
        MK_ARRAY(MK_STRUCT(STRUCT_snd_mask), SNDRV_PCM_HW_PARAM_LAST_MASK -
                  SNDRV_PCM_HW_PARAM_FIRST_MASK + 1),
        MK_ARRAY(MK_STRUCT(STRUCT_snd_mask), 5),
        MK_ARRAY(MK_STRUCT(STRUCT_snd_interval), SNDRV_PCM_HW_PARAM_LAST_INTERVAL -
                  SNDRV_PCM_HW_PARAM_FIRST_INTERVAL + 1),
        MK_ARRAY(MK_STRUCT(STRUCT_snd_interval), 9),
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_LONG,
        MK_ARRAY(TYPE_CHAR, 64))

STRUCT(snd_pcm_sw_params,
    TYPE_INT,
    TYPE_INT,
    TYPE_INT,
    TYPE_LONG,
    TYPE_LONG,
    TYPE_LONG,
    TYPE_LONG,
    TYPE_LONG,
    TYPE_LONG,
    TYPE_LONG,
    TYPE_INT,
    TYPE_INT,
MK_ARRAY(TYPE_CHAR, 56))

STRUCT(snd_pcm_channel_info,
    TYPE_INT,
    TYPE_LONG,
    TYPE_INT,
    TYPE_INT)

STRUCT(snd_xferi,
    TYPE_LONG,
    TYPE_LONG,
    TYPE_LONG)

STRUCT(mpt3_ioctl_header,
    TYPE_INT,
    TYPE_INT,
    TYPE_INT)
STRUCT(MPT3_IOCTL_EVENTS,
    TYPE_INT,
    TYPE_INT,
    MK_ARRAY(TYPE_CHAR, MPT3_EVENT_DATA_SIZE))
STRUCT(mpt3_ioctl_pci_info,
       TYPE_INT,
       TYPE_INT)
STRUCT(mpt3_ioctl_iocinfo,
       MK_STRUCT(STRUCT_mpt3_ioctl_header),
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       MK_ARRAY(TYPE_CHAR, MPT2_IOCTL_VERSION_LENGTH),
       TYPE_CHAR,
       TYPE_CHAR,
       TYPE_SHORT,
       MK_STRUCT(STRUCT_mpt3_ioctl_pci_info))
STRUCT(mpt3_ioctl_command,
       MK_STRUCT(STRUCT_mpt3_ioctl_header),
       TYPE_INT,
       TYPE_LONG,
       TYPE_LONG,
       TYPE_LONG,
       TYPE_LONG,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       MK_ARRAY(TYPE_CHAR, 1))
STRUCT(mpt3_ioctl_eventquery,
       MK_STRUCT(STRUCT_mpt3_ioctl_header),
       TYPE_SHORT,
       TYPE_SHORT,
       MK_ARRAY(TYPE_INT, MPI2_EVENT_NOTIFY_EVENTMASK_WORDS))
STRUCT(mpt3_ioctl_eventenable,
       MK_STRUCT(STRUCT_mpt3_ioctl_header),
       MK_ARRAY(TYPE_INT, 4))
STRUCT(mpt3_ioctl_eventreport,
       MK_STRUCT(STRUCT_mpt3_ioctl_header),
       MK_ARRAY(MK_STRUCT(STRUCT_MPT3_IOCTL_EVENTS), 1))
STRUCT(mpt3_ioctl_diag_reset,
       MK_STRUCT(STRUCT_mpt3_ioctl_header))
STRUCT(mpt3_ioctl_btdh_mapping,
       MK_STRUCT(STRUCT_mpt3_ioctl_header),
       TYPE_INT,
       TYPE_INT,
       TYPE_SHORT,
       TYPE_SHORT)
STRUCT(mpt3_diag_register,
       MK_STRUCT(STRUCT_mpt3_ioctl_header),
       TYPE_CHAR,
       TYPE_CHAR,
       TYPE_SHORT,
       TYPE_INT,
       MK_ARRAY(TYPE_INT, MPT3_PRODUCT_SPECIFIC_DWORDS),
       TYPE_INT,
       TYPE_INT)
STRUCT(mpt3_diag_release,
       MK_STRUCT(STRUCT_mpt3_ioctl_header),
       TYPE_INT)
STRUCT(mpt3_diag_unregister,
       MK_STRUCT(STRUCT_mpt3_ioctl_header),
       TYPE_INT)
STRUCT(mpt3_diag_query,
       MK_STRUCT(STRUCT_mpt3_ioctl_header),
       TYPE_CHAR,
       TYPE_CHAR,
       TYPE_SHORT,
       TYPE_INT,
       MK_ARRAY(TYPE_INT, MPT3_PRODUCT_SPECIFIC_DWORDS),
       TYPE_INT,
       TYPE_INT,
       TYPE_INT)
STRUCT(mpt3_diag_read_buffer,
       MK_STRUCT(STRUCT_mpt3_ioctl_header),
       TYPE_CHAR,
       TYPE_CHAR,
       TYPE_SHORT,
       TYPE_INT,
       TYPE_INT,
       TYPE_INT,
       MK_ARRAY(TYPE_INT, 1))
STRUCT(drm_mode_atomic,
        TYPE_INT,
        TYPE_INT,
        TYPE_ULONGLONG,
        TYPE_ULONGLONG,
        TYPE_ULONGLONG,
        TYPE_ULONGLONG,
        TYPE_ULONGLONG,
        TYPE_ULONGLONG)
STRUCT(drm_mode_create_blob,
        TYPE_ULONGLONG,
        TYPE_INT,
        TYPE_INT)
STRUCT(drm_mode_destroy_blob,
        TYPE_INT)
STRUCT(drm_crtc_get_sequence,
        TYPE_INT,
        TYPE_INT,
        TYPE_ULONGLONG,
        TYPE_ULONGLONG)
STRUCT(drm_crtc_queue_sequence,
        TYPE_INT,
        TYPE_INT,
        TYPE_ULONGLONG,
        TYPE_ULONGLONG)
STRUCT(drm_syncobj_create,
        TYPE_INT,
        TYPE_INT)
STRUCT(drm_syncobj_destroy,
        TYPE_INT,
        TYPE_INT)
STRUCT(drm_syncobj_handle,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT)
STRUCT(drm_syncobj_wait_old,
         TYPE_ULONGLONG,
         TYPE_ULONGLONG,
         TYPE_INT,
         TYPE_INT,
         TYPE_INT,
         TYPE_INT)
STRUCT(drm_syncobj_wait,
         TYPE_ULONGLONG,
         TYPE_ULONGLONG,
         TYPE_INT,
         TYPE_INT,
         TYPE_INT,
         TYPE_INT,
         TYPE_ULONGLONG)
STRUCT(drm_syncobj_array,
         TYPE_ULONGLONG,
         TYPE_INT,
         TYPE_INT)
STRUCT(drm_mode_create_lease,
         TYPE_ULONGLONG,
         TYPE_INT,
         TYPE_INT,
         TYPE_INT,
         TYPE_INT)
STRUCT(drm_mode_list_lessees,
        TYPE_INT,
        TYPE_INT,
        TYPE_ULONGLONG)
STRUCT(drm_mode_get_lease,
        TYPE_INT,
        TYPE_INT,
        TYPE_ULONGLONG)
STRUCT(drm_mode_revoke_lease,
        TYPE_INT)
STRUCT(drm_syncobj_timeline_wait_old,
         TYPE_ULONGLONG,
         TYPE_ULONGLONG,
         TYPE_ULONGLONG,
         TYPE_INT,
         TYPE_INT,
         TYPE_INT,
         TYPE_INT)
STRUCT(drm_syncobj_timeline_wait,
         TYPE_ULONGLONG,
         TYPE_ULONGLONG,
         TYPE_ULONGLONG,
         TYPE_INT,
         TYPE_INT,
         TYPE_INT,
         TYPE_INT,
         TYPE_ULONGLONG)
STRUCT(drm_syncobj_timeline_array,
         TYPE_ULONGLONG,
         TYPE_ULONGLONG,
         TYPE_INT,
         TYPE_INT)
STRUCT(drm_syncobj_transfer,
        TYPE_INT,
        TYPE_INT,
        TYPE_ULONGLONG,
        TYPE_ULONGLONG,
        TYPE_INT,
        TYPE_INT)
STRUCT(sg_io_hdr,
        TYPE_INT,
        TYPE_INT,
        TYPE_CHAR,
        TYPE_CHAR,
        TYPE_SHORT,
        TYPE_INT,
        TYPE_PTRVOID,
        TYPE_PTRVOID,
        TYPE_PTRVOID,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT,
        TYPE_PTRVOID,
        TYPE_CHAR,
        TYPE_CHAR,
        TYPE_CHAR,
        TYPE_CHAR,
        TYPE_SHORT,
        TYPE_SHORT,
        TYPE_INT,
        TYPE_INT,
        TYPE_INT)
STRUCT(termios2,
         TYPE_INT,
         TYPE_INT,
         TYPE_INT,
         TYPE_INT,
         TYPE_CHAR,
         MK_ARRAY(TYPE_CHAR, 19),
         TYPE_INT,
         TYPE_INT)
STRUCT(hidraw_devinfo,
         TYPE_INT,
         TYPE_SHORT,
         TYPE_SHORT)
STRUCT(hidraw_report_descriptor,
         TYPE_INT,
         MK_ARRAY(TYPE_CHAR, 4096))
#include "ioctl/ioctl_type/type_amdgpu_drm.h"
