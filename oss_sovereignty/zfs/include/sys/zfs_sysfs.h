#ifndef	_SYS_ZFS_SYSFS_H
#define	_SYS_ZFS_SYSFS_H extern __attribute__((visibility("default")))
struct zfs_mod_supported_features;
struct zfs_mod_supported_features *zfs_mod_list_supported(const char *scope);
void zfs_mod_list_supported_free(struct zfs_mod_supported_features *);
#ifdef _KERNEL
void zfs_sysfs_init(void);
void zfs_sysfs_fini(void);
#else
#define	zfs_sysfs_init()
#define	zfs_sysfs_fini()
_SYS_ZFS_SYSFS_H boolean_t zfs_mod_supported(const char *, const char *,
    const struct zfs_mod_supported_features *);
#endif
#define	ZFS_SYSFS_POOL_PROPERTIES	"properties.pool"
#define	ZFS_SYSFS_VDEV_PROPERTIES	"properties.vdev"
#define	ZFS_SYSFS_DATASET_PROPERTIES	"properties.dataset"
#define	ZFS_SYSFS_KERNEL_FEATURES	"features.kernel"
#define	ZFS_SYSFS_POOL_FEATURES		"features.pool"
#define	ZFS_SYSFS_DIR			"/sys/module/zfs"
#define	ZFS_SYSFS_ALT_DIR		"/sys/fs/zfs"
#endif	 
