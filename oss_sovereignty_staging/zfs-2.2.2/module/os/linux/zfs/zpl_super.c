 
 


#include <sys/zfs_znode.h>
#include <sys/zfs_vfsops.h>
#include <sys/zfs_vnops.h>
#include <sys/zfs_ctldir.h>
#include <sys/zpl.h>


static struct inode *
zpl_inode_alloc(struct super_block *sb)
{
	struct inode *ip;

	VERIFY3S(zfs_inode_alloc(sb, &ip), ==, 0);
	inode_set_iversion(ip, 1);

	return (ip);
}

static void
zpl_inode_destroy(struct inode *ip)
{
	ASSERT(atomic_read(&ip->i_count) == 0);
	zfs_inode_destroy(ip);
}

 
#ifdef HAVE_DIRTY_INODE_WITH_FLAGS
static void
zpl_dirty_inode(struct inode *ip, int flags)
{
	fstrans_cookie_t cookie;

	cookie = spl_fstrans_mark();
	zfs_dirty_inode(ip, flags);
	spl_fstrans_unmark(cookie);
}
#else
static void
zpl_dirty_inode(struct inode *ip)
{
	fstrans_cookie_t cookie;

	cookie = spl_fstrans_mark();
	zfs_dirty_inode(ip, 0);
	spl_fstrans_unmark(cookie);
}
#endif  

 
static void
zpl_evict_inode(struct inode *ip)
{
	fstrans_cookie_t cookie;

	cookie = spl_fstrans_mark();
	truncate_setsize(ip, 0);
	clear_inode(ip);
	zfs_inactive(ip);
	spl_fstrans_unmark(cookie);
}

static void
zpl_put_super(struct super_block *sb)
{
	fstrans_cookie_t cookie;
	int error;

	cookie = spl_fstrans_mark();
	error = -zfs_umount(sb);
	spl_fstrans_unmark(cookie);
	ASSERT3S(error, <=, 0);
}

static int
zpl_sync_fs(struct super_block *sb, int wait)
{
	fstrans_cookie_t cookie;
	cred_t *cr = CRED();
	int error;

	crhold(cr);
	cookie = spl_fstrans_mark();
	error = -zfs_sync(sb, wait, cr);
	spl_fstrans_unmark(cookie);
	crfree(cr);
	ASSERT3S(error, <=, 0);

	return (error);
}

static int
zpl_statfs(struct dentry *dentry, struct kstatfs *statp)
{
	fstrans_cookie_t cookie;
	int error;

	cookie = spl_fstrans_mark();
	error = -zfs_statvfs(dentry->d_inode, statp);
	spl_fstrans_unmark(cookie);
	ASSERT3S(error, <=, 0);

	 
	if (unlikely(zpl_is_32bit_api())) {
		while (statp->f_blocks > UINT32_MAX &&
		    statp->f_bsize < SPA_MAXBLOCKSIZE) {
			statp->f_frsize <<= 1;
			statp->f_bsize <<= 1;

			statp->f_blocks >>= 1;
			statp->f_bfree >>= 1;
			statp->f_bavail >>= 1;
		}

		uint64_t usedobjs = statp->f_files - statp->f_ffree;
		statp->f_ffree = MIN(statp->f_ffree, UINT32_MAX - usedobjs);
		statp->f_files = statp->f_ffree + usedobjs;
	}

	return (error);
}

static int
zpl_remount_fs(struct super_block *sb, int *flags, char *data)
{
	zfs_mnt_t zm = { .mnt_osname = NULL, .mnt_data = data };
	fstrans_cookie_t cookie;
	int error;

	cookie = spl_fstrans_mark();
	error = -zfs_remount(sb, flags, &zm);
	spl_fstrans_unmark(cookie);
	ASSERT3S(error, <=, 0);

	return (error);
}

static int
__zpl_show_devname(struct seq_file *seq, zfsvfs_t *zfsvfs)
{
	int error;
	if ((error = zpl_enter(zfsvfs, FTAG)) != 0)
		return (error);

	char *fsname = kmem_alloc(ZFS_MAX_DATASET_NAME_LEN, KM_SLEEP);
	dmu_objset_name(zfsvfs->z_os, fsname);

	for (int i = 0; fsname[i] != 0; i++) {
		 
		if (fsname[i] == ' ') {
			seq_puts(seq, "\\040");
		} else {
			seq_putc(seq, fsname[i]);
		}
	}

	kmem_free(fsname, ZFS_MAX_DATASET_NAME_LEN);

	zpl_exit(zfsvfs, FTAG);

	return (0);
}

static int
zpl_show_devname(struct seq_file *seq, struct dentry *root)
{
	return (__zpl_show_devname(seq, root->d_sb->s_fs_info));
}

static int
__zpl_show_options(struct seq_file *seq, zfsvfs_t *zfsvfs)
{
	seq_printf(seq, ",%s",
	    zfsvfs->z_flags & ZSB_XATTR ? "xattr" : "noxattr");

#ifdef CONFIG_FS_POSIX_ACL
	switch (zfsvfs->z_acl_type) {
	case ZFS_ACLTYPE_POSIX:
		seq_puts(seq, ",posixacl");
		break;
	default:
		seq_puts(seq, ",noacl");
		break;
	}
#endif  

	switch (zfsvfs->z_case) {
	case ZFS_CASE_SENSITIVE:
		seq_puts(seq, ",casesensitive");
		break;
	case ZFS_CASE_INSENSITIVE:
		seq_puts(seq, ",caseinsensitive");
		break;
	default:
		seq_puts(seq, ",casemixed");
		break;
	}

	return (0);
}

static int
zpl_show_options(struct seq_file *seq, struct dentry *root)
{
	return (__zpl_show_options(seq, root->d_sb->s_fs_info));
}

static int
zpl_fill_super(struct super_block *sb, void *data, int silent)
{
	zfs_mnt_t *zm = (zfs_mnt_t *)data;
	fstrans_cookie_t cookie;
	int error;

	cookie = spl_fstrans_mark();
	error = -zfs_domount(sb, zm, silent);
	spl_fstrans_unmark(cookie);
	ASSERT3S(error, <=, 0);

	return (error);
}

static int
zpl_test_super(struct super_block *s, void *data)
{
	zfsvfs_t *zfsvfs = s->s_fs_info;
	objset_t *os = data;
	 
	return (zfsvfs != NULL && os == zfsvfs->z_os);
}

static struct super_block *
zpl_mount_impl(struct file_system_type *fs_type, int flags, zfs_mnt_t *zm)
{
	struct super_block *s;
	objset_t *os;
	int err;

	err = dmu_objset_hold(zm->mnt_osname, FTAG, &os);
	if (err)
		return (ERR_PTR(-err));

	 
	dsl_dataset_long_hold(dmu_objset_ds(os), FTAG);
	dsl_pool_rele(dmu_objset_pool(os), FTAG);

	s = sget(fs_type, zpl_test_super, set_anon_super, flags, os);

	 
	if (!IS_ERR(s) && s->s_fs_info != NULL) {
		zfsvfs_t *zfsvfs = s->s_fs_info;
		if (zpl_enter(zfsvfs, FTAG) == 0) {
			if (os != zfsvfs->z_os)
				err = -SET_ERROR(EBUSY);
			zpl_exit(zfsvfs, FTAG);
		} else {
			err = -SET_ERROR(EBUSY);
		}
	}
	dsl_dataset_long_rele(dmu_objset_ds(os), FTAG);
	dsl_dataset_rele(dmu_objset_ds(os), FTAG);

	if (IS_ERR(s))
		return (ERR_CAST(s));

	if (err) {
		deactivate_locked_super(s);
		return (ERR_PTR(err));
	}

	if (s->s_root == NULL) {
		err = zpl_fill_super(s, zm, flags & SB_SILENT ? 1 : 0);
		if (err) {
			deactivate_locked_super(s);
			return (ERR_PTR(err));
		}
		s->s_flags |= SB_ACTIVE;
	} else if ((flags ^ s->s_flags) & SB_RDONLY) {
		deactivate_locked_super(s);
		return (ERR_PTR(-EBUSY));
	}

	return (s);
}

static struct dentry *
zpl_mount(struct file_system_type *fs_type, int flags,
    const char *osname, void *data)
{
	zfs_mnt_t zm = { .mnt_osname = osname, .mnt_data = data };

	struct super_block *sb = zpl_mount_impl(fs_type, flags, &zm);
	if (IS_ERR(sb))
		return (ERR_CAST(sb));

	return (dget(sb->s_root));
}

static void
zpl_kill_sb(struct super_block *sb)
{
	zfs_preumount(sb);
	kill_anon_super(sb);
}

void
zpl_prune_sb(uint64_t nr_to_scan, void *arg)
{
	struct super_block *sb = (struct super_block *)arg;
	int objects = 0;

	(void) -zfs_prune(sb, nr_to_scan, &objects);
}

const struct super_operations zpl_super_operations = {
	.alloc_inode		= zpl_inode_alloc,
	.destroy_inode		= zpl_inode_destroy,
	.dirty_inode		= zpl_dirty_inode,
	.write_inode		= NULL,
	.evict_inode		= zpl_evict_inode,
	.put_super		= zpl_put_super,
	.sync_fs		= zpl_sync_fs,
	.statfs			= zpl_statfs,
	.remount_fs		= zpl_remount_fs,
	.show_devname		= zpl_show_devname,
	.show_options		= zpl_show_options,
	.show_stats		= NULL,
};

struct file_system_type zpl_fs_type = {
	.owner			= THIS_MODULE,
	.name			= ZFS_DRIVER,
#if defined(HAVE_IDMAP_MNT_API)
	.fs_flags		= FS_USERNS_MOUNT | FS_ALLOW_IDMAP,
#else
	.fs_flags		= FS_USERNS_MOUNT,
#endif
	.mount			= zpl_mount,
	.kill_sb		= zpl_kill_sb,
};
