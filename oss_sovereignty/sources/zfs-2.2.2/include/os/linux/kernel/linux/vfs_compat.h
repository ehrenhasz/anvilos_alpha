



#ifndef _ZFS_VFS_H
#define	_ZFS_VFS_H

#include <sys/taskq.h>
#include <sys/cred.h>
#include <linux/backing-dev.h>
#include <linux/compat.h>


#if defined(HAVE_SUPER_SETUP_BDI_NAME)
extern atomic_long_t zfs_bdi_seq;

static inline int
zpl_bdi_setup(struct super_block *sb, char *name)
{
	return super_setup_bdi_name(sb, "%.28s-%ld", name,
	    atomic_long_inc_return(&zfs_bdi_seq));
}
static inline void
zpl_bdi_destroy(struct super_block *sb)
{
}
#elif defined(HAVE_2ARGS_BDI_SETUP_AND_REGISTER)
static inline int
zpl_bdi_setup(struct super_block *sb, char *name)
{
	struct backing_dev_info *bdi;
	int error;

	bdi = kmem_zalloc(sizeof (struct backing_dev_info), KM_SLEEP);
	error = bdi_setup_and_register(bdi, name);
	if (error) {
		kmem_free(bdi, sizeof (struct backing_dev_info));
		return (error);
	}

	sb->s_bdi = bdi;

	return (0);
}
static inline void
zpl_bdi_destroy(struct super_block *sb)
{
	struct backing_dev_info *bdi = sb->s_bdi;

	bdi_destroy(bdi);
	kmem_free(bdi, sizeof (struct backing_dev_info));
	sb->s_bdi = NULL;
}
#elif defined(HAVE_3ARGS_BDI_SETUP_AND_REGISTER)
static inline int
zpl_bdi_setup(struct super_block *sb, char *name)
{
	struct backing_dev_info *bdi;
	int error;

	bdi = kmem_zalloc(sizeof (struct backing_dev_info), KM_SLEEP);
	error = bdi_setup_and_register(bdi, name, BDI_CAP_MAP_COPY);
	if (error) {
		kmem_free(sb->s_bdi, sizeof (struct backing_dev_info));
		return (error);
	}

	sb->s_bdi = bdi;

	return (0);
}
static inline void
zpl_bdi_destroy(struct super_block *sb)
{
	struct backing_dev_info *bdi = sb->s_bdi;

	bdi_destroy(bdi);
	kmem_free(bdi, sizeof (struct backing_dev_info));
	sb->s_bdi = NULL;
}
#else
#error "Unsupported kernel"
#endif


#ifndef	SB_RDONLY
#define	SB_RDONLY	MS_RDONLY
#endif

#ifndef	SB_SILENT
#define	SB_SILENT	MS_SILENT
#endif

#ifndef	SB_ACTIVE
#define	SB_ACTIVE	MS_ACTIVE
#endif

#ifndef	SB_POSIXACL
#define	SB_POSIXACL	MS_POSIXACL
#endif

#ifndef	SB_MANDLOCK
#define	SB_MANDLOCK	MS_MANDLOCK
#endif

#ifndef	SB_NOATIME
#define	SB_NOATIME	MS_NOATIME
#endif


#if defined(HAVE_EVICT_INODE) && !defined(HAVE_CLEAR_INODE)
#define	clear_inode(ip)		end_writeback(ip)
#endif 

#if defined(SEEK_HOLE) && defined(SEEK_DATA) && !defined(HAVE_LSEEK_EXECUTE)
static inline loff_t
lseek_execute(
	struct file *filp,
	struct inode *inode,
	loff_t offset,
	loff_t maxsize)
{
	if (offset < 0 && !(filp->f_mode & FMODE_UNSIGNED_OFFSET))
		return (-EINVAL);

	if (offset > maxsize)
		return (-EINVAL);

	if (offset != filp->f_pos) {
		spin_lock(&filp->f_lock);
		filp->f_pos = offset;
		filp->f_version = 0;
		spin_unlock(&filp->f_lock);
	}

	return (offset);
}
#endif 

#if defined(CONFIG_FS_POSIX_ACL)


#include <linux/posix_acl.h>

#if defined(HAVE_POSIX_ACL_RELEASE) && !defined(HAVE_POSIX_ACL_RELEASE_GPL_ONLY)
#define	zpl_posix_acl_release(arg)		posix_acl_release(arg)
#else
void zpl_posix_acl_release_impl(struct posix_acl *);

static inline void
zpl_posix_acl_release(struct posix_acl *acl)
{
	if ((acl == NULL) || (acl == ACL_NOT_CACHED))
		return;
#ifdef HAVE_ACL_REFCOUNT
	if (refcount_dec_and_test(&acl->a_refcount))
		zpl_posix_acl_release_impl(acl);
#else
	if (atomic_dec_and_test(&acl->a_refcount))
		zpl_posix_acl_release_impl(acl);
#endif
}
#endif 

#ifdef HAVE_SET_CACHED_ACL_USABLE
#define	zpl_set_cached_acl(ip, ty, n)		set_cached_acl(ip, ty, n)
#define	zpl_forget_cached_acl(ip, ty)		forget_cached_acl(ip, ty)
#else
static inline void
zpl_set_cached_acl(struct inode *ip, int type, struct posix_acl *newer)
{
	struct posix_acl *older = NULL;

	spin_lock(&ip->i_lock);

	if ((newer != ACL_NOT_CACHED) && (newer != NULL))
		posix_acl_dup(newer);

	switch (type) {
	case ACL_TYPE_ACCESS:
		older = ip->i_acl;
		rcu_assign_pointer(ip->i_acl, newer);
		break;
	case ACL_TYPE_DEFAULT:
		older = ip->i_default_acl;
		rcu_assign_pointer(ip->i_default_acl, newer);
		break;
	}

	spin_unlock(&ip->i_lock);

	zpl_posix_acl_release(older);
}

static inline void
zpl_forget_cached_acl(struct inode *ip, int type)
{
	zpl_set_cached_acl(ip, type, (struct posix_acl *)ACL_NOT_CACHED);
}
#endif 


#ifndef HAVE___POSIX_ACL_CHMOD
#ifdef HAVE_POSIX_ACL_CHMOD
#define	__posix_acl_chmod(acl, gfp, mode)	posix_acl_chmod(acl, gfp, mode)
#define	__posix_acl_create(acl, gfp, mode)	posix_acl_create(acl, gfp, mode)
#else
#error "Unsupported kernel"
#endif 
#endif 


#ifdef HAVE_POSIX_ACL_VALID_WITH_NS
#define	zpl_posix_acl_valid(ip, acl)  posix_acl_valid(ip->i_sb->s_user_ns, acl)
#else
#define	zpl_posix_acl_valid(ip, acl)  posix_acl_valid(acl)
#endif

#endif 


#ifndef HAVE_FILE_INODE
static inline struct inode *file_inode(const struct file *f)
{
	return (f->f_dentry->d_inode);
}
#endif 


#ifndef HAVE_FILE_DENTRY
static inline struct dentry *file_dentry(const struct file *f)
{
	return (f->f_path.dentry);
}
#endif 

static inline uid_t zfs_uid_read_impl(struct inode *ip)
{
	return (from_kuid(kcred->user_ns, ip->i_uid));
}

static inline uid_t zfs_uid_read(struct inode *ip)
{
	return (zfs_uid_read_impl(ip));
}

static inline gid_t zfs_gid_read_impl(struct inode *ip)
{
	return (from_kgid(kcred->user_ns, ip->i_gid));
}

static inline gid_t zfs_gid_read(struct inode *ip)
{
	return (zfs_gid_read_impl(ip));
}

static inline void zfs_uid_write(struct inode *ip, uid_t uid)
{
	ip->i_uid = make_kuid(kcred->user_ns, uid);
}

static inline void zfs_gid_write(struct inode *ip, gid_t gid)
{
	ip->i_gid = make_kgid(kcred->user_ns, gid);
}


#ifndef RENAME_NOREPLACE
#define	RENAME_NOREPLACE	(1 << 0) 
#endif
#ifndef RENAME_EXCHANGE
#define	RENAME_EXCHANGE		(1 << 1) 
#endif
#ifndef RENAME_WHITEOUT
#define	RENAME_WHITEOUT		(1 << 2) 
#endif


#if !(defined(HAVE_SETATTR_PREPARE_NO_USERNS) || \
    defined(HAVE_SETATTR_PREPARE_USERNS) || \
    defined(HAVE_SETATTR_PREPARE_IDMAP))
static inline int
setattr_prepare(struct dentry *dentry, struct iattr *ia)
{
	return (inode_change_ok(dentry->d_inode, ia));
}
#endif



#ifndef STATX_BASIC_STATS
#define	STATX_BASIC_STATS	0
#endif

#ifndef AT_STATX_SYNC_AS_STAT
#define	AT_STATX_SYNC_AS_STAT	0
#endif



#ifdef HAVE_VFSMOUNT_IOPS_GETATTR
#define	ZPL_GETATTR_WRAPPER(func)					\
static int								\
func(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat)	\
{									\
	struct path path = { .mnt = mnt, .dentry = dentry };		\
	return func##_impl(&path, stat, STATX_BASIC_STATS,		\
	    AT_STATX_SYNC_AS_STAT);					\
}
#elif defined(HAVE_PATH_IOPS_GETATTR)
#define	ZPL_GETATTR_WRAPPER(func)					\
static int								\
func(const struct path *path, struct kstat *stat, u32 request_mask,	\
    unsigned int query_flags)						\
{									\
	return (func##_impl(path, stat, request_mask, query_flags));	\
}
#elif defined(HAVE_USERNS_IOPS_GETATTR)
#define	ZPL_GETATTR_WRAPPER(func)					\
static int								\
func(struct user_namespace *user_ns, const struct path *path,	\
    struct kstat *stat, u32 request_mask, unsigned int query_flags)	\
{									\
	return (func##_impl(user_ns, path, stat, request_mask, \
	    query_flags));	\
}
#elif defined(HAVE_IDMAP_IOPS_GETATTR)
#define	ZPL_GETATTR_WRAPPER(func)					\
static int								\
func(struct mnt_idmap *user_ns, const struct path *path,	\
    struct kstat *stat, u32 request_mask, unsigned int query_flags)	\
{									\
	return (func##_impl(user_ns, path, stat, request_mask,	\
	    query_flags));	\
}
#else
#error
#endif


#if !defined(HAVE_CURRENT_TIME)
static inline struct timespec
current_time(struct inode *ip)
{
	return (timespec_trunc(current_kernel_time(), ip->i_sb->s_time_gran));
}
#endif


#ifdef HAVE_INODE_SET_IVERSION
#include <linux/iversion.h>
#else
static inline void
inode_set_iversion(struct inode *ip, u64 val)
{
	ip->i_version = val;
}
#endif


static inline int
zpl_is_32bit_api(void)
{
#ifdef CONFIG_COMPAT
#ifdef HAVE_IN_COMPAT_SYSCALL
	return (in_compat_syscall());
#else
	return (is_compat_task());
#endif
#else
	return (BITS_PER_LONG == 32);
#endif
}


#ifdef HAVE_GENERIC_FILLATTR_IDMAP
#define	zpl_generic_fillattr(idmap, ip, sp)	\
    generic_fillattr(idmap, ip, sp)
#elif defined(HAVE_GENERIC_FILLATTR_IDMAP_REQMASK)
#define	zpl_generic_fillattr(idmap, rqm, ip, sp)	\
    generic_fillattr(idmap, rqm, ip, sp)
#elif defined(HAVE_GENERIC_FILLATTR_USERNS)
#define	zpl_generic_fillattr(user_ns, ip, sp)	\
    generic_fillattr(user_ns, ip, sp)
#else
#define	zpl_generic_fillattr(user_ns, ip, sp)	generic_fillattr(ip, sp)
#endif

#endif 
