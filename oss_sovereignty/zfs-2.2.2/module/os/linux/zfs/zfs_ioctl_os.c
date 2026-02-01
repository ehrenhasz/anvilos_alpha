 

 

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/kmem.h>
#include <sys/stat.h>
#include <sys/zfs_ioctl.h>
#include <sys/zfs_vfsops.h>
#include <sys/zap.h>
#include <sys/spa.h>
#include <sys/nvpair.h>
#include <sys/fs/zfs.h>
#include <sys/zfs_ctldir.h>
#include <sys/zfs_dir.h>
#include <sys/zfs_onexit.h>
#include <sys/zvol.h>
#include <sys/fm/util.h>
#include <sys/dsl_crypt.h>
#include <sys/crypto/icp.h>
#include <sys/zstd/zstd.h>

#include <sys/zfs_ioctl_impl.h>

#include <sys/zfs_sysfs.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>

boolean_t
zfs_vfs_held(zfsvfs_t *zfsvfs)
{
	return (zfsvfs->z_sb != NULL);
}

int
zfs_vfs_ref(zfsvfs_t **zfvp)
{
	if (*zfvp == NULL || (*zfvp)->z_sb == NULL ||
	    !atomic_inc_not_zero(&((*zfvp)->z_sb->s_active))) {
		return (SET_ERROR(ESRCH));
	}
	return (0);
}

void
zfs_vfs_rele(zfsvfs_t *zfsvfs)
{
	deactivate_super(zfsvfs->z_sb);
}

void
zfsdev_private_set_state(void *priv, zfsdev_state_t *zs)
{
	struct file *filp = priv;

	filp->private_data = zs;
}

zfsdev_state_t *
zfsdev_private_get_state(void *priv)
{
	struct file *filp = priv;

	return (filp->private_data);
}

static int
zfsdev_open(struct inode *ino, struct file *filp)
{
	int error;

	mutex_enter(&zfsdev_state_lock);
	error = zfsdev_state_init(filp);
	mutex_exit(&zfsdev_state_lock);

	return (-error);
}

static int
zfsdev_release(struct inode *ino, struct file *filp)
{
	zfsdev_state_destroy(filp);

	return (0);
}

static long
zfsdev_ioctl(struct file *filp, unsigned cmd, unsigned long arg)
{
	uint_t vecnum;
	zfs_cmd_t *zc;
	int error, rc;

	vecnum = cmd - ZFS_IOC_FIRST;

	zc = vmem_zalloc(sizeof (zfs_cmd_t), KM_SLEEP);

	if (ddi_copyin((void *)(uintptr_t)arg, zc, sizeof (zfs_cmd_t), 0)) {
		error = -SET_ERROR(EFAULT);
		goto out;
	}
	error = -zfsdev_ioctl_common(vecnum, zc, 0);
	rc = ddi_copyout(zc, (void *)(uintptr_t)arg, sizeof (zfs_cmd_t), 0);
	if (error == 0 && rc != 0)
		error = -SET_ERROR(EFAULT);
out:
	vmem_free(zc, sizeof (zfs_cmd_t));
	return (error);

}

static int
zfs_ioc_userns_attach(zfs_cmd_t *zc)
{
	int error;

	if (zc == NULL)
		return (SET_ERROR(EINVAL));

	error = zone_dataset_attach(CRED(), zc->zc_name, zc->zc_cleanup_fd);

	 
	if (error == ENOTTY)
		error = ZFS_ERR_NOT_USER_NAMESPACE;

	return (error);
}

static int
zfs_ioc_userns_detach(zfs_cmd_t *zc)
{
	int error;

	if (zc == NULL)
		return (SET_ERROR(EINVAL));

	error = zone_dataset_detach(CRED(), zc->zc_name, zc->zc_cleanup_fd);

	 
	if (error == ENOTTY)
		error = ZFS_ERR_NOT_USER_NAMESPACE;

	return (error);
}

uint64_t
zfs_max_nvlist_src_size_os(void)
{
	if (zfs_max_nvlist_src_size != 0)
		return (zfs_max_nvlist_src_size);

	return (MIN(ptob(zfs_totalram_pages) / 4, 128 * 1024 * 1024));
}

 
void
zfs_ioctl_update_mount_cache(const char *dsname)
{
}

void
zfs_ioctl_init_os(void)
{
	zfs_ioctl_register_dataset_nolog(ZFS_IOC_USERNS_ATTACH,
	    zfs_ioc_userns_attach, zfs_secpolicy_config, POOL_CHECK_NONE);
	zfs_ioctl_register_dataset_nolog(ZFS_IOC_USERNS_DETACH,
	    zfs_ioc_userns_detach, zfs_secpolicy_config, POOL_CHECK_NONE);
}

#ifdef CONFIG_COMPAT
static long
zfsdev_compat_ioctl(struct file *filp, unsigned cmd, unsigned long arg)
{
	return (zfsdev_ioctl(filp, cmd, arg));
}
#else
#define	zfsdev_compat_ioctl	NULL
#endif

static const struct file_operations zfsdev_fops = {
	.open		= zfsdev_open,
	.release	= zfsdev_release,
	.unlocked_ioctl	= zfsdev_ioctl,
	.compat_ioctl	= zfsdev_compat_ioctl,
	.owner		= THIS_MODULE,
};

static struct miscdevice zfs_misc = {
	.minor		= ZFS_DEVICE_MINOR,
	.name		= ZFS_DRIVER,
	.fops		= &zfsdev_fops,
};

MODULE_ALIAS_MISCDEV(ZFS_DEVICE_MINOR);
MODULE_ALIAS("devname:zfs");

int
zfsdev_attach(void)
{
	int error;

	error = misc_register(&zfs_misc);
	if (error == -EBUSY) {
		 
		printk(KERN_INFO "ZFS: misc_register() with static minor %d "
		    "failed %d, retrying with MISC_DYNAMIC_MINOR\n",
		    ZFS_DEVICE_MINOR, error);

		zfs_misc.minor = MISC_DYNAMIC_MINOR;
		error = misc_register(&zfs_misc);
	}

	if (error)
		printk(KERN_INFO "ZFS: misc_register() failed %d\n", error);

	return (error);
}

void
zfsdev_detach(void)
{
	misc_deregister(&zfs_misc);
}

#ifdef ZFS_DEBUG
#define	ZFS_DEBUG_STR	" (DEBUG mode)"
#else
#define	ZFS_DEBUG_STR	""
#endif

zidmap_t *zfs_init_idmap;

static int
openzfs_init_os(void)
{
	int error;

	if ((error = zfs_kmod_init()) != 0) {
		printk(KERN_NOTICE "ZFS: Failed to Load ZFS Filesystem v%s-%s%s"
		    ", rc = %d\n", ZFS_META_VERSION, ZFS_META_RELEASE,
		    ZFS_DEBUG_STR, error);

		return (-error);
	}

	zfs_sysfs_init();

	printk(KERN_NOTICE "ZFS: Loaded module v%s-%s%s, "
	    "ZFS pool version %s, ZFS filesystem version %s\n",
	    ZFS_META_VERSION, ZFS_META_RELEASE, ZFS_DEBUG_STR,
	    SPA_VERSION_STRING, ZPL_VERSION_STRING);
#ifndef CONFIG_FS_POSIX_ACL
	printk(KERN_NOTICE "ZFS: Posix ACLs disabled by kernel\n");
#endif  

	zfs_init_idmap = (zidmap_t *)zfs_get_init_idmap();

	return (0);
}

static void
openzfs_fini_os(void)
{
	zfs_sysfs_fini();
	zfs_kmod_fini();

	printk(KERN_NOTICE "ZFS: Unloaded module v%s-%s%s\n",
	    ZFS_META_VERSION, ZFS_META_RELEASE, ZFS_DEBUG_STR);
}


extern int __init zcommon_init(void);
extern void zcommon_fini(void);

static int __init
openzfs_init(void)
{
	int err;
	if ((err = zcommon_init()) != 0)
		goto zcommon_failed;
	if ((err = icp_init()) != 0)
		goto icp_failed;
	if ((err = zstd_init()) != 0)
		goto zstd_failed;
	if ((err = openzfs_init_os()) != 0)
		goto openzfs_os_failed;
	return (0);

openzfs_os_failed:
	zstd_fini();
zstd_failed:
	icp_fini();
icp_failed:
	zcommon_fini();
zcommon_failed:
	return (err);
}

static void __exit
openzfs_fini(void)
{
	openzfs_fini_os();
	zstd_fini();
	icp_fini();
	zcommon_fini();
}

#if defined(_KERNEL)
module_init(openzfs_init);
module_exit(openzfs_fini);
#endif

MODULE_ALIAS("zavl");
MODULE_ALIAS("icp");
MODULE_ALIAS("zlua");
MODULE_ALIAS("znvpair");
MODULE_ALIAS("zunicode");
MODULE_ALIAS("zcommon");
MODULE_ALIAS("zzstd");
MODULE_DESCRIPTION("ZFS");
MODULE_AUTHOR(ZFS_META_AUTHOR);
MODULE_LICENSE("Dual MIT/GPL");  
MODULE_LICENSE("Dual BSD/GPL");  
MODULE_LICENSE(ZFS_META_LICENSE);
MODULE_VERSION(ZFS_META_VERSION "-" ZFS_META_RELEASE);
