#include <sys/zfs_znode.h>
#include <sys/zfs_vfsops.h>
#include <sys/zfs_vnops.h>
#include <sys/zfs_ctldir.h>
#include <sys/zpl.h>
#include <sys/dmu.h>
#include <sys/dsl_dataset.h>
#include <sys/zap.h>
static int
zpl_common_open(struct inode *ip, struct file *filp)
{
	if (blk_mode_is_open_write(filp->f_mode))
		return (-EACCES);
	return (generic_file_open(ip, filp));
}
static int
zpl_root_iterate(struct file *filp, zpl_dir_context_t *ctx)
{
	zfsvfs_t *zfsvfs = ITOZSB(file_inode(filp));
	int error = 0;
	if ((error = zpl_enter(zfsvfs, FTAG)) != 0)
		return (error);
	if (!zpl_dir_emit_dots(filp, ctx))
		goto out;
	if (ctx->pos == 2) {
		if (!zpl_dir_emit(ctx, ZFS_SNAPDIR_NAME,
		    strlen(ZFS_SNAPDIR_NAME), ZFSCTL_INO_SNAPDIR, DT_DIR))
			goto out;
		ctx->pos++;
	}
	if (ctx->pos == 3) {
		if (!zpl_dir_emit(ctx, ZFS_SHAREDIR_NAME,
		    strlen(ZFS_SHAREDIR_NAME), ZFSCTL_INO_SHARES, DT_DIR))
			goto out;
		ctx->pos++;
	}
out:
	zpl_exit(zfsvfs, FTAG);
	return (error);
}
#if !defined(HAVE_VFS_ITERATE) && !defined(HAVE_VFS_ITERATE_SHARED)
static int
zpl_root_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	zpl_dir_context_t ctx =
	    ZPL_DIR_CONTEXT_INIT(dirent, filldir, filp->f_pos);
	int error;
	error = zpl_root_iterate(filp, &ctx);
	filp->f_pos = ctx.pos;
	return (error);
}
#endif  
static int
#ifdef HAVE_IDMAP_IOPS_GETATTR
zpl_root_getattr_impl(struct mnt_idmap *user_ns,
    const struct path *path, struct kstat *stat, u32 request_mask,
    unsigned int query_flags)
#elif defined(HAVE_USERNS_IOPS_GETATTR)
zpl_root_getattr_impl(struct user_namespace *user_ns,
    const struct path *path, struct kstat *stat, u32 request_mask,
    unsigned int query_flags)
#else
zpl_root_getattr_impl(const struct path *path, struct kstat *stat,
    u32 request_mask, unsigned int query_flags)
#endif
{
	(void) request_mask, (void) query_flags;
	struct inode *ip = path->dentry->d_inode;
#if (defined(HAVE_USERNS_IOPS_GETATTR) || defined(HAVE_IDMAP_IOPS_GETATTR))
#ifdef HAVE_GENERIC_FILLATTR_USERNS
	generic_fillattr(user_ns, ip, stat);
#elif defined(HAVE_GENERIC_FILLATTR_IDMAP)
	generic_fillattr(user_ns, ip, stat);
#elif defined(HAVE_GENERIC_FILLATTR_IDMAP_REQMASK)
	generic_fillattr(user_ns, request_mask, ip, stat);
#else
	(void) user_ns;
#endif
#else
	generic_fillattr(ip, stat);
#endif
	stat->atime = current_time(ip);
	return (0);
}
ZPL_GETATTR_WRAPPER(zpl_root_getattr);
static struct dentry *
zpl_root_lookup(struct inode *dip, struct dentry *dentry, unsigned int flags)
{
	cred_t *cr = CRED();
	struct inode *ip;
	int error;
	crhold(cr);
	error = -zfsctl_root_lookup(dip, dname(dentry), &ip, 0, cr, NULL, NULL);
	ASSERT3S(error, <=, 0);
	crfree(cr);
	if (error) {
		if (error == -ENOENT)
			return (d_splice_alias(NULL, dentry));
		else
			return (ERR_PTR(error));
	}
	return (d_splice_alias(ip, dentry));
}
const struct file_operations zpl_fops_root = {
	.open		= zpl_common_open,
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir,
#ifdef HAVE_VFS_ITERATE_SHARED
	.iterate_shared	= zpl_root_iterate,
#elif defined(HAVE_VFS_ITERATE)
	.iterate	= zpl_root_iterate,
#else
	.readdir	= zpl_root_readdir,
#endif
};
const struct inode_operations zpl_ops_root = {
	.lookup		= zpl_root_lookup,
	.getattr	= zpl_root_getattr,
};
static struct vfsmount *
zpl_snapdir_automount(struct path *path)
{
	int error;
	error = -zfsctl_snapshot_mount(path, 0);
	if (error)
		return (ERR_PTR(error));
	return (NULL);
}
static int
#ifdef HAVE_D_REVALIDATE_NAMEIDATA
zpl_snapdir_revalidate(struct dentry *dentry, struct nameidata *i)
#else
zpl_snapdir_revalidate(struct dentry *dentry, unsigned int flags)
#endif
{
	return (!!dentry->d_inode);
}
static dentry_operations_t zpl_dops_snapdirs = {
	.d_automount	= zpl_snapdir_automount,
	.d_revalidate	= zpl_snapdir_revalidate,
};
static struct dentry *
zpl_snapdir_lookup(struct inode *dip, struct dentry *dentry,
    unsigned int flags)
{
	fstrans_cookie_t cookie;
	cred_t *cr = CRED();
	struct inode *ip = NULL;
	int error;
	crhold(cr);
	cookie = spl_fstrans_mark();
	error = -zfsctl_snapdir_lookup(dip, dname(dentry), &ip,
	    0, cr, NULL, NULL);
	ASSERT3S(error, <=, 0);
	spl_fstrans_unmark(cookie);
	crfree(cr);
	if (error && error != -ENOENT)
		return (ERR_PTR(error));
	ASSERT(error == 0 || ip == NULL);
	d_clear_d_op(dentry);
	d_set_d_op(dentry, &zpl_dops_snapdirs);
	dentry->d_flags |= DCACHE_NEED_AUTOMOUNT;
	return (d_splice_alias(ip, dentry));
}
static int
zpl_snapdir_iterate(struct file *filp, zpl_dir_context_t *ctx)
{
	zfsvfs_t *zfsvfs = ITOZSB(file_inode(filp));
	fstrans_cookie_t cookie;
	char snapname[MAXNAMELEN];
	boolean_t case_conflict;
	uint64_t id, pos;
	int error = 0;
	if ((error = zpl_enter(zfsvfs, FTAG)) != 0)
		return (error);
	cookie = spl_fstrans_mark();
	if (!zpl_dir_emit_dots(filp, ctx))
		goto out;
	pos = (ctx->pos == 2 ? 0 : ctx->pos);
	while (error == 0) {
		dsl_pool_config_enter(dmu_objset_pool(zfsvfs->z_os), FTAG);
		error = -dmu_snapshot_list_next(zfsvfs->z_os, MAXNAMELEN,
		    snapname, &id, &pos, &case_conflict);
		dsl_pool_config_exit(dmu_objset_pool(zfsvfs->z_os), FTAG);
		if (error)
			goto out;
		if (!zpl_dir_emit(ctx, snapname, strlen(snapname),
		    ZFSCTL_INO_SHARES - id, DT_DIR))
			goto out;
		ctx->pos = pos;
	}
out:
	spl_fstrans_unmark(cookie);
	zpl_exit(zfsvfs, FTAG);
	if (error == -ENOENT)
		return (0);
	return (error);
}
#if !defined(HAVE_VFS_ITERATE) && !defined(HAVE_VFS_ITERATE_SHARED)
static int
zpl_snapdir_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	zpl_dir_context_t ctx =
	    ZPL_DIR_CONTEXT_INIT(dirent, filldir, filp->f_pos);
	int error;
	error = zpl_snapdir_iterate(filp, &ctx);
	filp->f_pos = ctx.pos;
	return (error);
}
#endif  
static int
#ifdef HAVE_IOPS_RENAME_USERNS
zpl_snapdir_rename2(struct user_namespace *user_ns, struct inode *sdip,
    struct dentry *sdentry, struct inode *tdip, struct dentry *tdentry,
    unsigned int flags)
#elif defined(HAVE_IOPS_RENAME_IDMAP)
zpl_snapdir_rename2(struct mnt_idmap *user_ns, struct inode *sdip,
    struct dentry *sdentry, struct inode *tdip, struct dentry *tdentry,
    unsigned int flags)
#else
zpl_snapdir_rename2(struct inode *sdip, struct dentry *sdentry,
    struct inode *tdip, struct dentry *tdentry, unsigned int flags)
#endif
{
	cred_t *cr = CRED();
	int error;
	if (flags)
		return (-EINVAL);
	crhold(cr);
	error = -zfsctl_snapdir_rename(sdip, dname(sdentry),
	    tdip, dname(tdentry), cr, 0);
	ASSERT3S(error, <=, 0);
	crfree(cr);
	return (error);
}
#if (!defined(HAVE_RENAME_WANTS_FLAGS) && \
	!defined(HAVE_IOPS_RENAME_USERNS) && \
	!defined(HAVE_IOPS_RENAME_IDMAP))
static int
zpl_snapdir_rename(struct inode *sdip, struct dentry *sdentry,
    struct inode *tdip, struct dentry *tdentry)
{
	return (zpl_snapdir_rename2(sdip, sdentry, tdip, tdentry, 0));
}
#endif
static int
zpl_snapdir_rmdir(struct inode *dip, struct dentry *dentry)
{
	cred_t *cr = CRED();
	int error;
	crhold(cr);
	error = -zfsctl_snapdir_remove(dip, dname(dentry), cr, 0);
	ASSERT3S(error, <=, 0);
	crfree(cr);
	return (error);
}
static int
#ifdef HAVE_IOPS_MKDIR_USERNS
zpl_snapdir_mkdir(struct user_namespace *user_ns, struct inode *dip,
    struct dentry *dentry, umode_t mode)
#elif defined(HAVE_IOPS_MKDIR_IDMAP)
zpl_snapdir_mkdir(struct mnt_idmap *user_ns, struct inode *dip,
    struct dentry *dentry, umode_t mode)
#else
zpl_snapdir_mkdir(struct inode *dip, struct dentry *dentry, umode_t mode)
#endif
{
	cred_t *cr = CRED();
	vattr_t *vap;
	struct inode *ip;
	int error;
	crhold(cr);
	vap = kmem_zalloc(sizeof (vattr_t), KM_SLEEP);
#if (defined(HAVE_IOPS_MKDIR_USERNS) || defined(HAVE_IOPS_MKDIR_IDMAP))
	zpl_vap_init(vap, dip, mode | S_IFDIR, cr, user_ns);
#else
	zpl_vap_init(vap, dip, mode | S_IFDIR, cr, zfs_init_idmap);
#endif
	error = -zfsctl_snapdir_mkdir(dip, dname(dentry), vap, &ip, cr, 0);
	if (error == 0) {
		d_clear_d_op(dentry);
		d_set_d_op(dentry, &zpl_dops_snapdirs);
		d_instantiate(dentry, ip);
	}
	kmem_free(vap, sizeof (vattr_t));
	ASSERT3S(error, <=, 0);
	crfree(cr);
	return (error);
}
static int
#ifdef HAVE_IDMAP_IOPS_GETATTR
zpl_snapdir_getattr_impl(struct mnt_idmap *user_ns,
    const struct path *path, struct kstat *stat, u32 request_mask,
    unsigned int query_flags)
#elif defined(HAVE_USERNS_IOPS_GETATTR)
zpl_snapdir_getattr_impl(struct user_namespace *user_ns,
    const struct path *path, struct kstat *stat, u32 request_mask,
    unsigned int query_flags)
#else
zpl_snapdir_getattr_impl(const struct path *path, struct kstat *stat,
    u32 request_mask, unsigned int query_flags)
#endif
{
	(void) request_mask, (void) query_flags;
	struct inode *ip = path->dentry->d_inode;
	zfsvfs_t *zfsvfs = ITOZSB(ip);
	int error;
	if ((error = zpl_enter(zfsvfs, FTAG)) != 0)
		return (error);
#if (defined(HAVE_USERNS_IOPS_GETATTR) || defined(HAVE_IDMAP_IOPS_GETATTR))
#ifdef HAVE_GENERIC_FILLATTR_USERNS
	generic_fillattr(user_ns, ip, stat);
#elif defined(HAVE_GENERIC_FILLATTR_IDMAP)
	generic_fillattr(user_ns, ip, stat);
#elif defined(HAVE_GENERIC_FILLATTR_IDMAP_REQMASK)
	generic_fillattr(user_ns, request_mask, ip, stat);
#else
	(void) user_ns;
#endif
#else
	generic_fillattr(ip, stat);
#endif
	stat->nlink = stat->size = 2;
	dsl_dataset_t *ds = dmu_objset_ds(zfsvfs->z_os);
	if (dsl_dataset_phys(ds)->ds_snapnames_zapobj != 0) {
		uint64_t snap_count;
		int err = zap_count(
		    dmu_objset_pool(ds->ds_objset)->dp_meta_objset,
		    dsl_dataset_phys(ds)->ds_snapnames_zapobj, &snap_count);
		if (err != 0) {
			zpl_exit(zfsvfs, FTAG);
			return (-err);
		}
		stat->nlink += snap_count;
	}
	stat->ctime = stat->mtime = dmu_objset_snap_cmtime(zfsvfs->z_os);
	stat->atime = current_time(ip);
	zpl_exit(zfsvfs, FTAG);
	return (0);
}
ZPL_GETATTR_WRAPPER(zpl_snapdir_getattr);
const struct file_operations zpl_fops_snapdir = {
	.open		= zpl_common_open,
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir,
#ifdef HAVE_VFS_ITERATE_SHARED
	.iterate_shared	= zpl_snapdir_iterate,
#elif defined(HAVE_VFS_ITERATE)
	.iterate	= zpl_snapdir_iterate,
#else
	.readdir	= zpl_snapdir_readdir,
#endif
};
const struct inode_operations zpl_ops_snapdir = {
	.lookup		= zpl_snapdir_lookup,
	.getattr	= zpl_snapdir_getattr,
#if (defined(HAVE_RENAME_WANTS_FLAGS) || \
	defined(HAVE_IOPS_RENAME_USERNS) || \
	defined(HAVE_IOPS_RENAME_IDMAP))
	.rename		= zpl_snapdir_rename2,
#else
	.rename		= zpl_snapdir_rename,
#endif
	.rmdir		= zpl_snapdir_rmdir,
	.mkdir		= zpl_snapdir_mkdir,
};
static struct dentry *
zpl_shares_lookup(struct inode *dip, struct dentry *dentry,
    unsigned int flags)
{
	fstrans_cookie_t cookie;
	cred_t *cr = CRED();
	struct inode *ip = NULL;
	int error;
	crhold(cr);
	cookie = spl_fstrans_mark();
	error = -zfsctl_shares_lookup(dip, dname(dentry), &ip,
	    0, cr, NULL, NULL);
	ASSERT3S(error, <=, 0);
	spl_fstrans_unmark(cookie);
	crfree(cr);
	if (error) {
		if (error == -ENOENT)
			return (d_splice_alias(NULL, dentry));
		else
			return (ERR_PTR(error));
	}
	return (d_splice_alias(ip, dentry));
}
static int
zpl_shares_iterate(struct file *filp, zpl_dir_context_t *ctx)
{
	fstrans_cookie_t cookie;
	cred_t *cr = CRED();
	zfsvfs_t *zfsvfs = ITOZSB(file_inode(filp));
	znode_t *dzp;
	int error = 0;
	if ((error = zpl_enter(zfsvfs, FTAG)) != 0)
		return (error);
	cookie = spl_fstrans_mark();
	if (zfsvfs->z_shares_dir == 0) {
		zpl_dir_emit_dots(filp, ctx);
		goto out;
	}
	error = -zfs_zget(zfsvfs, zfsvfs->z_shares_dir, &dzp);
	if (error)
		goto out;
	crhold(cr);
	error = -zfs_readdir(ZTOI(dzp), ctx, cr);
	crfree(cr);
	iput(ZTOI(dzp));
out:
	spl_fstrans_unmark(cookie);
	zpl_exit(zfsvfs, FTAG);
	ASSERT3S(error, <=, 0);
	return (error);
}
#if !defined(HAVE_VFS_ITERATE) && !defined(HAVE_VFS_ITERATE_SHARED)
static int
zpl_shares_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	zpl_dir_context_t ctx =
	    ZPL_DIR_CONTEXT_INIT(dirent, filldir, filp->f_pos);
	int error;
	error = zpl_shares_iterate(filp, &ctx);
	filp->f_pos = ctx.pos;
	return (error);
}
#endif  
static int
#ifdef HAVE_USERNS_IOPS_GETATTR
zpl_shares_getattr_impl(struct user_namespace *user_ns,
    const struct path *path, struct kstat *stat, u32 request_mask,
    unsigned int query_flags)
#elif defined(HAVE_IDMAP_IOPS_GETATTR)
zpl_shares_getattr_impl(struct mnt_idmap *user_ns,
    const struct path *path, struct kstat *stat, u32 request_mask,
    unsigned int query_flags)
#else
zpl_shares_getattr_impl(const struct path *path, struct kstat *stat,
    u32 request_mask, unsigned int query_flags)
#endif
{
	(void) request_mask, (void) query_flags;
	struct inode *ip = path->dentry->d_inode;
	zfsvfs_t *zfsvfs = ITOZSB(ip);
	znode_t *dzp;
	int error;
	if ((error = zpl_enter(zfsvfs, FTAG)) != 0)
		return (error);
	if (zfsvfs->z_shares_dir == 0) {
#if (defined(HAVE_USERNS_IOPS_GETATTR) || defined(HAVE_IDMAP_IOPS_GETATTR))
#ifdef HAVE_GENERIC_FILLATTR_USERNS
		generic_fillattr(user_ns, path->dentry->d_inode, stat);
#elif defined(HAVE_GENERIC_FILLATTR_IDMAP)
		generic_fillattr(user_ns, path->dentry->d_inode, stat);
#elif defined(HAVE_GENERIC_FILLATTR_IDMAP_REQMASK)
	generic_fillattr(user_ns, request_mask, ip, stat);
#else
		(void) user_ns;
#endif
#else
		generic_fillattr(path->dentry->d_inode, stat);
#endif
		stat->nlink = stat->size = 2;
		stat->atime = current_time(ip);
		zpl_exit(zfsvfs, FTAG);
		return (0);
	}
	error = -zfs_zget(zfsvfs, zfsvfs->z_shares_dir, &dzp);
	if (error == 0) {
#ifdef HAVE_GENERIC_FILLATTR_IDMAP_REQMASK
		error = -zfs_getattr_fast(user_ns, request_mask, ZTOI(dzp),
		    stat);
#elif (defined(HAVE_USERNS_IOPS_GETATTR) || defined(HAVE_IDMAP_IOPS_GETATTR))
		error = -zfs_getattr_fast(user_ns, ZTOI(dzp), stat);
#else
		error = -zfs_getattr_fast(kcred->user_ns, ZTOI(dzp), stat);
#endif
		iput(ZTOI(dzp));
	}
	zpl_exit(zfsvfs, FTAG);
	ASSERT3S(error, <=, 0);
	return (error);
}
ZPL_GETATTR_WRAPPER(zpl_shares_getattr);
const struct file_operations zpl_fops_shares = {
	.open		= zpl_common_open,
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir,
#ifdef HAVE_VFS_ITERATE_SHARED
	.iterate_shared	= zpl_shares_iterate,
#elif defined(HAVE_VFS_ITERATE)
	.iterate	= zpl_shares_iterate,
#else
	.readdir	= zpl_shares_readdir,
#endif
};
const struct inode_operations zpl_ops_shares = {
	.lookup		= zpl_shares_lookup,
	.getattr	= zpl_shares_getattr,
};
