
 

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cred.h>
#include <linux/xattr.h>
#include <linux/posix_acl.h>
#include <linux/ratelimit.h>
#include <linux/fiemap.h>
#include <linux/fileattr.h>
#include <linux/security.h>
#include <linux/namei.h>
#include <linux/posix_acl.h>
#include <linux/posix_acl_xattr.h>
#include "overlayfs.h"


int ovl_setattr(struct mnt_idmap *idmap, struct dentry *dentry,
		struct iattr *attr)
{
	int err;
	struct ovl_fs *ofs = OVL_FS(dentry->d_sb);
	bool full_copy_up = false;
	struct dentry *upperdentry;
	const struct cred *old_cred;

	err = setattr_prepare(&nop_mnt_idmap, dentry, attr);
	if (err)
		return err;

	err = ovl_want_write(dentry);
	if (err)
		goto out;

	if (attr->ia_valid & ATTR_SIZE) {
		 
		full_copy_up = true;
	}

	if (!full_copy_up)
		err = ovl_copy_up(dentry);
	else
		err = ovl_copy_up_with_data(dentry);
	if (!err) {
		struct inode *winode = NULL;

		upperdentry = ovl_dentry_upper(dentry);

		if (attr->ia_valid & ATTR_SIZE) {
			winode = d_inode(upperdentry);
			err = get_write_access(winode);
			if (err)
				goto out_drop_write;
		}

		if (attr->ia_valid & (ATTR_KILL_SUID|ATTR_KILL_SGID))
			attr->ia_valid &= ~ATTR_MODE;

		 
		attr->ia_valid &= ~ATTR_FILE;

		 
		attr->ia_valid &= ~ATTR_OPEN;

		inode_lock(upperdentry->d_inode);
		old_cred = ovl_override_creds(dentry->d_sb);
		err = ovl_do_notify_change(ofs, upperdentry, attr);
		revert_creds(old_cred);
		if (!err)
			ovl_copyattr(dentry->d_inode);
		inode_unlock(upperdentry->d_inode);

		if (winode)
			put_write_access(winode);
	}
out_drop_write:
	ovl_drop_write(dentry);
out:
	return err;
}

static void ovl_map_dev_ino(struct dentry *dentry, struct kstat *stat, int fsid)
{
	struct ovl_fs *ofs = OVL_FS(dentry->d_sb);
	bool samefs = ovl_same_fs(ofs);
	unsigned int xinobits = ovl_xino_bits(ofs);
	unsigned int xinoshift = 64 - xinobits;

	if (samefs) {
		 
		stat->dev = dentry->d_sb->s_dev;
		return;
	} else if (xinobits) {
		 
		if (likely(!(stat->ino >> xinoshift))) {
			stat->ino |= ((u64)fsid) << (xinoshift + 1);
			stat->dev = dentry->d_sb->s_dev;
			return;
		} else if (ovl_xino_warn(ofs)) {
			pr_warn_ratelimited("inode number too big (%pd2, ino=%llu, xinobits=%d)\n",
					    dentry, stat->ino, xinobits);
		}
	}

	 
	if (S_ISDIR(dentry->d_inode->i_mode)) {
		 
		stat->dev = dentry->d_sb->s_dev;
		stat->ino = dentry->d_inode->i_ino;
	} else {
		 
		stat->dev = ofs->fs[fsid].pseudo_dev;
	}
}

int ovl_getattr(struct mnt_idmap *idmap, const struct path *path,
		struct kstat *stat, u32 request_mask, unsigned int flags)
{
	struct dentry *dentry = path->dentry;
	enum ovl_path_type type;
	struct path realpath;
	const struct cred *old_cred;
	struct inode *inode = d_inode(dentry);
	bool is_dir = S_ISDIR(inode->i_mode);
	int fsid = 0;
	int err;
	bool metacopy_blocks = false;

	metacopy_blocks = ovl_is_metacopy_dentry(dentry);

	type = ovl_path_real(dentry, &realpath);
	old_cred = ovl_override_creds(dentry->d_sb);
	err = ovl_do_getattr(&realpath, stat, request_mask, flags);
	if (err)
		goto out;

	 
	generic_fill_statx_attr(inode, stat);

	 
	if (!is_dir || ovl_same_dev(OVL_FS(dentry->d_sb))) {
		if (!OVL_TYPE_UPPER(type)) {
			fsid = ovl_layer_lower(dentry)->fsid;
		} else if (OVL_TYPE_ORIGIN(type)) {
			struct kstat lowerstat;
			u32 lowermask = STATX_INO | STATX_BLOCKS |
					(!is_dir ? STATX_NLINK : 0);

			ovl_path_lower(dentry, &realpath);
			err = ovl_do_getattr(&realpath, &lowerstat, lowermask,
					     flags);
			if (err)
				goto out;

			 
			if (ovl_test_flag(OVL_INDEX, d_inode(dentry)) ||
			    (!ovl_verify_lower(dentry->d_sb) &&
			     (is_dir || lowerstat.nlink == 1))) {
				fsid = ovl_layer_lower(dentry)->fsid;
				stat->ino = lowerstat.ino;
			}

			 
			if (metacopy_blocks &&
			    realpath.dentry == ovl_dentry_lowerdata(dentry)) {
				stat->blocks = lowerstat.blocks;
				metacopy_blocks = false;
			}
		}

		if (metacopy_blocks) {
			 
			struct kstat lowerdatastat;
			u32 lowermask = STATX_BLOCKS;

			ovl_path_lowerdata(dentry, &realpath);
			if (realpath.dentry) {
				err = ovl_do_getattr(&realpath, &lowerdatastat,
						     lowermask, flags);
				if (err)
					goto out;
			} else {
				lowerdatastat.blocks =
					round_up(stat->size, stat->blksize) >> 9;
			}
			stat->blocks = lowerdatastat.blocks;
		}
	}

	ovl_map_dev_ino(dentry, stat, fsid);

	 
	if (is_dir && OVL_TYPE_MERGE(type))
		stat->nlink = 1;

	 
	if (!is_dir && ovl_test_flag(OVL_INDEX, d_inode(dentry)))
		stat->nlink = dentry->d_inode->i_nlink;

out:
	revert_creds(old_cred);

	return err;
}

int ovl_permission(struct mnt_idmap *idmap,
		   struct inode *inode, int mask)
{
	struct inode *upperinode = ovl_inode_upper(inode);
	struct inode *realinode;
	struct path realpath;
	const struct cred *old_cred;
	int err;

	 
	realinode = ovl_i_path_real(inode, &realpath);
	if (!realinode) {
		WARN_ON(!(mask & MAY_NOT_BLOCK));
		return -ECHILD;
	}

	 
	err = generic_permission(&nop_mnt_idmap, inode, mask);
	if (err)
		return err;

	old_cred = ovl_override_creds(inode->i_sb);
	if (!upperinode &&
	    !special_file(realinode->i_mode) && mask & MAY_WRITE) {
		mask &= ~(MAY_WRITE | MAY_APPEND);
		 
		mask |= MAY_READ;
	}
	err = inode_permission(mnt_idmap(realpath.mnt), realinode, mask);
	revert_creds(old_cred);

	return err;
}

static const char *ovl_get_link(struct dentry *dentry,
				struct inode *inode,
				struct delayed_call *done)
{
	const struct cred *old_cred;
	const char *p;

	if (!dentry)
		return ERR_PTR(-ECHILD);

	old_cred = ovl_override_creds(dentry->d_sb);
	p = vfs_get_link(ovl_dentry_real(dentry), done);
	revert_creds(old_cred);
	return p;
}

bool ovl_is_private_xattr(struct super_block *sb, const char *name)
{
	struct ovl_fs *ofs = OVL_FS(sb);

	if (ofs->config.userxattr)
		return strncmp(name, OVL_XATTR_USER_PREFIX,
			       sizeof(OVL_XATTR_USER_PREFIX) - 1) == 0;
	else
		return strncmp(name, OVL_XATTR_TRUSTED_PREFIX,
			       sizeof(OVL_XATTR_TRUSTED_PREFIX) - 1) == 0;
}

int ovl_xattr_set(struct dentry *dentry, struct inode *inode, const char *name,
		  const void *value, size_t size, int flags)
{
	int err;
	struct ovl_fs *ofs = OVL_FS(dentry->d_sb);
	struct dentry *upperdentry = ovl_i_dentry_upper(inode);
	struct dentry *realdentry = upperdentry ?: ovl_dentry_lower(dentry);
	struct path realpath;
	const struct cred *old_cred;

	err = ovl_want_write(dentry);
	if (err)
		goto out;

	if (!value && !upperdentry) {
		ovl_path_lower(dentry, &realpath);
		old_cred = ovl_override_creds(dentry->d_sb);
		err = vfs_getxattr(mnt_idmap(realpath.mnt), realdentry, name, NULL, 0);
		revert_creds(old_cred);
		if (err < 0)
			goto out_drop_write;
	}

	if (!upperdentry) {
		err = ovl_copy_up(dentry);
		if (err)
			goto out_drop_write;

		realdentry = ovl_dentry_upper(dentry);
	}

	old_cred = ovl_override_creds(dentry->d_sb);
	if (value) {
		err = ovl_do_setxattr(ofs, realdentry, name, value, size,
				      flags);
	} else {
		WARN_ON(flags != XATTR_REPLACE);
		err = ovl_do_removexattr(ofs, realdentry, name);
	}
	revert_creds(old_cred);

	 
	ovl_copyattr(inode);

out_drop_write:
	ovl_drop_write(dentry);
out:
	return err;
}

int ovl_xattr_get(struct dentry *dentry, struct inode *inode, const char *name,
		  void *value, size_t size)
{
	ssize_t res;
	const struct cred *old_cred;
	struct path realpath;

	ovl_i_path_real(inode, &realpath);
	old_cred = ovl_override_creds(dentry->d_sb);
	res = vfs_getxattr(mnt_idmap(realpath.mnt), realpath.dentry, name, value, size);
	revert_creds(old_cred);
	return res;
}

static bool ovl_can_list(struct super_block *sb, const char *s)
{
	 
	if (ovl_is_private_xattr(sb, s))
		return false;

	 
	if (strncmp(s, XATTR_TRUSTED_PREFIX, XATTR_TRUSTED_PREFIX_LEN) != 0)
		return true;

	 
	return ns_capable_noaudit(&init_user_ns, CAP_SYS_ADMIN);
}

ssize_t ovl_listxattr(struct dentry *dentry, char *list, size_t size)
{
	struct dentry *realdentry = ovl_dentry_real(dentry);
	ssize_t res;
	size_t len;
	char *s;
	const struct cred *old_cred;

	old_cred = ovl_override_creds(dentry->d_sb);
	res = vfs_listxattr(realdentry, list, size);
	revert_creds(old_cred);
	if (res <= 0 || size == 0)
		return res;

	 
	for (s = list, len = res; len;) {
		size_t slen = strnlen(s, len) + 1;

		 
		if (WARN_ON(slen > len))
			return -EIO;

		len -= slen;
		if (!ovl_can_list(dentry->d_sb, s)) {
			res -= slen;
			memmove(s, s + slen, len);
		} else {
			s += slen;
		}
	}

	return res;
}

#ifdef CONFIG_FS_POSIX_ACL
 
static void ovl_idmap_posix_acl(const struct inode *realinode,
				struct mnt_idmap *idmap,
				struct posix_acl *acl)
{
	struct user_namespace *fs_userns = i_user_ns(realinode);

	for (unsigned int i = 0; i < acl->a_count; i++) {
		vfsuid_t vfsuid;
		vfsgid_t vfsgid;

		struct posix_acl_entry *e = &acl->a_entries[i];
		switch (e->e_tag) {
		case ACL_USER:
			vfsuid = make_vfsuid(idmap, fs_userns, e->e_uid);
			e->e_uid = vfsuid_into_kuid(vfsuid);
			break;
		case ACL_GROUP:
			vfsgid = make_vfsgid(idmap, fs_userns, e->e_gid);
			e->e_gid = vfsgid_into_kgid(vfsgid);
			break;
		}
	}
}

 
struct posix_acl *ovl_get_acl_path(const struct path *path,
				   const char *acl_name, bool noperm)
{
	struct posix_acl *real_acl, *clone;
	struct mnt_idmap *idmap;
	struct inode *realinode = d_inode(path->dentry);

	idmap = mnt_idmap(path->mnt);

	if (noperm)
		real_acl = get_inode_acl(realinode, posix_acl_type(acl_name));
	else
		real_acl = vfs_get_acl(idmap, path->dentry, acl_name);
	if (IS_ERR_OR_NULL(real_acl))
		return real_acl;

	if (!is_idmapped_mnt(path->mnt))
		return real_acl;

	 
	clone = posix_acl_clone(real_acl, GFP_KERNEL);
	posix_acl_release(real_acl);  
	if (!clone)
		return ERR_PTR(-ENOMEM);

	ovl_idmap_posix_acl(realinode, idmap, clone);
	return clone;
}

 
struct posix_acl *do_ovl_get_acl(struct mnt_idmap *idmap,
				 struct inode *inode, int type,
				 bool rcu, bool noperm)
{
	struct inode *realinode;
	struct posix_acl *acl;
	struct path realpath;

	 
	realinode = ovl_i_path_real(inode, &realpath);
	if (!realinode) {
		WARN_ON(!rcu);
		return ERR_PTR(-ECHILD);
	}

	if (!IS_POSIXACL(realinode))
		return NULL;

	if (rcu) {
		 
		if (is_idmapped_mnt(realpath.mnt))
			return ERR_PTR(-ECHILD);

		acl = get_cached_acl_rcu(realinode, type);
	} else {
		const struct cred *old_cred;

		old_cred = ovl_override_creds(inode->i_sb);
		acl = ovl_get_acl_path(&realpath, posix_acl_xattr_name(type), noperm);
		revert_creds(old_cred);
	}

	return acl;
}

static int ovl_set_or_remove_acl(struct dentry *dentry, struct inode *inode,
				 struct posix_acl *acl, int type)
{
	int err;
	struct path realpath;
	const char *acl_name;
	const struct cred *old_cred;
	struct ovl_fs *ofs = OVL_FS(dentry->d_sb);
	struct dentry *upperdentry = ovl_dentry_upper(dentry);
	struct dentry *realdentry = upperdentry ?: ovl_dentry_lower(dentry);

	err = ovl_want_write(dentry);
	if (err)
		return err;

	 
	acl_name = posix_acl_xattr_name(type);
	if (!acl && !upperdentry) {
		struct posix_acl *real_acl;

		ovl_path_lower(dentry, &realpath);
		old_cred = ovl_override_creds(dentry->d_sb);
		real_acl = vfs_get_acl(mnt_idmap(realpath.mnt), realdentry,
				       acl_name);
		revert_creds(old_cred);
		if (IS_ERR(real_acl)) {
			err = PTR_ERR(real_acl);
			goto out_drop_write;
		}
		posix_acl_release(real_acl);
	}

	if (!upperdentry) {
		err = ovl_copy_up(dentry);
		if (err)
			goto out_drop_write;

		realdentry = ovl_dentry_upper(dentry);
	}

	old_cred = ovl_override_creds(dentry->d_sb);
	if (acl)
		err = ovl_do_set_acl(ofs, realdentry, acl_name, acl);
	else
		err = ovl_do_remove_acl(ofs, realdentry, acl_name);
	revert_creds(old_cred);

	 
	ovl_copyattr(inode);

out_drop_write:
	ovl_drop_write(dentry);
	return err;
}

int ovl_set_acl(struct mnt_idmap *idmap, struct dentry *dentry,
		struct posix_acl *acl, int type)
{
	int err;
	struct inode *inode = d_inode(dentry);
	struct dentry *workdir = ovl_workdir(dentry);
	struct inode *realinode = ovl_inode_real(inode);

	if (!IS_POSIXACL(d_inode(workdir)))
		return -EOPNOTSUPP;
	if (!realinode->i_op->set_acl)
		return -EOPNOTSUPP;
	if (type == ACL_TYPE_DEFAULT && !S_ISDIR(inode->i_mode))
		return acl ? -EACCES : 0;
	if (!inode_owner_or_capable(&nop_mnt_idmap, inode))
		return -EPERM;

	 
	if (unlikely(inode->i_mode & S_ISGID) && type == ACL_TYPE_ACCESS &&
	    !in_group_p(inode->i_gid) &&
	    !capable_wrt_inode_uidgid(&nop_mnt_idmap, inode, CAP_FSETID)) {
		struct iattr iattr = { .ia_valid = ATTR_KILL_SGID };

		err = ovl_setattr(&nop_mnt_idmap, dentry, &iattr);
		if (err)
			return err;
	}

	return ovl_set_or_remove_acl(dentry, inode, acl, type);
}
#endif

int ovl_update_time(struct inode *inode, int flags)
{
	if (flags & S_ATIME) {
		struct ovl_fs *ofs = OVL_FS(inode->i_sb);
		struct path upperpath = {
			.mnt = ovl_upper_mnt(ofs),
			.dentry = ovl_upperdentry_dereference(OVL_I(inode)),
		};

		if (upperpath.dentry) {
			touch_atime(&upperpath);
			inode->i_atime = d_inode(upperpath.dentry)->i_atime;
		}
	}
	return 0;
}

static int ovl_fiemap(struct inode *inode, struct fiemap_extent_info *fieinfo,
		      u64 start, u64 len)
{
	int err;
	struct inode *realinode = ovl_inode_realdata(inode);
	const struct cred *old_cred;

	if (!realinode)
		return -EIO;

	if (!realinode->i_op->fiemap)
		return -EOPNOTSUPP;

	old_cred = ovl_override_creds(inode->i_sb);
	err = realinode->i_op->fiemap(realinode, fieinfo, start, len);
	revert_creds(old_cred);

	return err;
}

 
static int ovl_security_fileattr(const struct path *realpath, struct fileattr *fa,
				 bool set)
{
	struct file *file;
	unsigned int cmd;
	int err;

	file = dentry_open(realpath, O_RDONLY, current_cred());
	if (IS_ERR(file))
		return PTR_ERR(file);

	if (set)
		cmd = fa->fsx_valid ? FS_IOC_FSSETXATTR : FS_IOC_SETFLAGS;
	else
		cmd = fa->fsx_valid ? FS_IOC_FSGETXATTR : FS_IOC_GETFLAGS;

	err = security_file_ioctl(file, cmd, 0);
	fput(file);

	return err;
}

int ovl_real_fileattr_set(const struct path *realpath, struct fileattr *fa)
{
	int err;

	err = ovl_security_fileattr(realpath, fa, true);
	if (err)
		return err;

	return vfs_fileattr_set(mnt_idmap(realpath->mnt), realpath->dentry, fa);
}

int ovl_fileattr_set(struct mnt_idmap *idmap,
		     struct dentry *dentry, struct fileattr *fa)
{
	struct inode *inode = d_inode(dentry);
	struct path upperpath;
	const struct cred *old_cred;
	unsigned int flags;
	int err;

	err = ovl_want_write(dentry);
	if (err)
		goto out;

	err = ovl_copy_up(dentry);
	if (!err) {
		ovl_path_real(dentry, &upperpath);

		old_cred = ovl_override_creds(inode->i_sb);
		 
		err = ovl_set_protattr(inode, upperpath.dentry, fa);
		if (!err)
			err = ovl_real_fileattr_set(&upperpath, fa);
		revert_creds(old_cred);

		 
		flags = ovl_inode_real(inode)->i_flags & OVL_COPY_I_FLAGS_MASK;

		BUILD_BUG_ON(OVL_PROT_I_FLAGS_MASK & ~OVL_COPY_I_FLAGS_MASK);
		flags |= inode->i_flags & OVL_PROT_I_FLAGS_MASK;
		inode_set_flags(inode, flags, OVL_COPY_I_FLAGS_MASK);

		 
		ovl_copyattr(inode);
	}
	ovl_drop_write(dentry);
out:
	return err;
}

 
static void ovl_fileattr_prot_flags(struct inode *inode, struct fileattr *fa)
{
	BUILD_BUG_ON(OVL_PROT_FS_FLAGS_MASK & ~FS_COMMON_FL);
	BUILD_BUG_ON(OVL_PROT_FSX_FLAGS_MASK & ~FS_XFLAG_COMMON);

	if (inode->i_flags & S_APPEND) {
		fa->flags |= FS_APPEND_FL;
		fa->fsx_xflags |= FS_XFLAG_APPEND;
	}
	if (inode->i_flags & S_IMMUTABLE) {
		fa->flags |= FS_IMMUTABLE_FL;
		fa->fsx_xflags |= FS_XFLAG_IMMUTABLE;
	}
}

int ovl_real_fileattr_get(const struct path *realpath, struct fileattr *fa)
{
	int err;

	err = ovl_security_fileattr(realpath, fa, false);
	if (err)
		return err;

	err = vfs_fileattr_get(realpath->dentry, fa);
	if (err == -ENOIOCTLCMD)
		err = -ENOTTY;
	return err;
}

int ovl_fileattr_get(struct dentry *dentry, struct fileattr *fa)
{
	struct inode *inode = d_inode(dentry);
	struct path realpath;
	const struct cred *old_cred;
	int err;

	ovl_path_real(dentry, &realpath);

	old_cred = ovl_override_creds(inode->i_sb);
	err = ovl_real_fileattr_get(&realpath, fa);
	ovl_fileattr_prot_flags(inode, fa);
	revert_creds(old_cred);

	return err;
}

static const struct inode_operations ovl_file_inode_operations = {
	.setattr	= ovl_setattr,
	.permission	= ovl_permission,
	.getattr	= ovl_getattr,
	.listxattr	= ovl_listxattr,
	.get_inode_acl	= ovl_get_inode_acl,
	.get_acl	= ovl_get_acl,
	.set_acl	= ovl_set_acl,
	.update_time	= ovl_update_time,
	.fiemap		= ovl_fiemap,
	.fileattr_get	= ovl_fileattr_get,
	.fileattr_set	= ovl_fileattr_set,
};

static const struct inode_operations ovl_symlink_inode_operations = {
	.setattr	= ovl_setattr,
	.get_link	= ovl_get_link,
	.getattr	= ovl_getattr,
	.listxattr	= ovl_listxattr,
	.update_time	= ovl_update_time,
};

static const struct inode_operations ovl_special_inode_operations = {
	.setattr	= ovl_setattr,
	.permission	= ovl_permission,
	.getattr	= ovl_getattr,
	.listxattr	= ovl_listxattr,
	.get_inode_acl	= ovl_get_inode_acl,
	.get_acl	= ovl_get_acl,
	.set_acl	= ovl_set_acl,
	.update_time	= ovl_update_time,
};

static const struct address_space_operations ovl_aops = {
	 
	.direct_IO		= noop_direct_IO,
};

 
#define OVL_MAX_NESTING FILESYSTEM_MAX_STACK_DEPTH

static inline void ovl_lockdep_annotate_inode_mutex_key(struct inode *inode)
{
#ifdef CONFIG_LOCKDEP
	static struct lock_class_key ovl_i_mutex_key[OVL_MAX_NESTING];
	static struct lock_class_key ovl_i_mutex_dir_key[OVL_MAX_NESTING];
	static struct lock_class_key ovl_i_lock_key[OVL_MAX_NESTING];

	int depth = inode->i_sb->s_stack_depth - 1;

	if (WARN_ON_ONCE(depth < 0 || depth >= OVL_MAX_NESTING))
		depth = 0;

	if (S_ISDIR(inode->i_mode))
		lockdep_set_class(&inode->i_rwsem, &ovl_i_mutex_dir_key[depth]);
	else
		lockdep_set_class(&inode->i_rwsem, &ovl_i_mutex_key[depth]);

	lockdep_set_class(&OVL_I(inode)->lock, &ovl_i_lock_key[depth]);
#endif
}

static void ovl_next_ino(struct inode *inode)
{
	struct ovl_fs *ofs = OVL_FS(inode->i_sb);

	inode->i_ino = atomic_long_inc_return(&ofs->last_ino);
	if (unlikely(!inode->i_ino))
		inode->i_ino = atomic_long_inc_return(&ofs->last_ino);
}

static void ovl_map_ino(struct inode *inode, unsigned long ino, int fsid)
{
	struct ovl_fs *ofs = OVL_FS(inode->i_sb);
	int xinobits = ovl_xino_bits(ofs);
	unsigned int xinoshift = 64 - xinobits;

	 
	inode->i_ino = ino;
	if (ovl_same_fs(ofs)) {
		return;
	} else if (xinobits && likely(!(ino >> xinoshift))) {
		inode->i_ino |= (unsigned long)fsid << (xinoshift + 1);
		return;
	}

	 
	if (S_ISDIR(inode->i_mode)) {
		ovl_next_ino(inode);
		if (xinobits) {
			inode->i_ino &= ~0UL >> xinobits;
			inode->i_ino |= 1UL << xinoshift;
		}
	}
}

void ovl_inode_init(struct inode *inode, struct ovl_inode_params *oip,
		    unsigned long ino, int fsid)
{
	struct inode *realinode;
	struct ovl_inode *oi = OVL_I(inode);

	oi->__upperdentry = oip->upperdentry;
	oi->oe = oip->oe;
	oi->redirect = oip->redirect;
	oi->lowerdata_redirect = oip->lowerdata_redirect;

	realinode = ovl_inode_real(inode);
	ovl_copyattr(inode);
	ovl_copyflags(realinode, inode);
	ovl_map_ino(inode, ino, fsid);
}

static void ovl_fill_inode(struct inode *inode, umode_t mode, dev_t rdev)
{
	inode->i_mode = mode;
	inode->i_flags |= S_NOCMTIME;
#ifdef CONFIG_FS_POSIX_ACL
	inode->i_acl = inode->i_default_acl = ACL_DONT_CACHE;
#endif

	ovl_lockdep_annotate_inode_mutex_key(inode);

	switch (mode & S_IFMT) {
	case S_IFREG:
		inode->i_op = &ovl_file_inode_operations;
		inode->i_fop = &ovl_file_operations;
		inode->i_mapping->a_ops = &ovl_aops;
		break;

	case S_IFDIR:
		inode->i_op = &ovl_dir_inode_operations;
		inode->i_fop = &ovl_dir_operations;
		break;

	case S_IFLNK:
		inode->i_op = &ovl_symlink_inode_operations;
		break;

	default:
		inode->i_op = &ovl_special_inode_operations;
		init_special_inode(inode, mode, rdev);
		break;
	}
}

 
#define OVL_NLINK_ADD_UPPER	(1 << 0)

 

static int ovl_set_nlink_common(struct dentry *dentry,
				struct dentry *realdentry, const char *format)
{
	struct inode *inode = d_inode(dentry);
	struct inode *realinode = d_inode(realdentry);
	char buf[13];
	int len;

	len = snprintf(buf, sizeof(buf), format,
		       (int) (inode->i_nlink - realinode->i_nlink));

	if (WARN_ON(len >= sizeof(buf)))
		return -EIO;

	return ovl_setxattr(OVL_FS(inode->i_sb), ovl_dentry_upper(dentry),
			    OVL_XATTR_NLINK, buf, len);
}

int ovl_set_nlink_upper(struct dentry *dentry)
{
	return ovl_set_nlink_common(dentry, ovl_dentry_upper(dentry), "U%+i");
}

int ovl_set_nlink_lower(struct dentry *dentry)
{
	return ovl_set_nlink_common(dentry, ovl_dentry_lower(dentry), "L%+i");
}

unsigned int ovl_get_nlink(struct ovl_fs *ofs, struct dentry *lowerdentry,
			   struct dentry *upperdentry,
			   unsigned int fallback)
{
	int nlink_diff;
	int nlink;
	char buf[13];
	int err;

	if (!lowerdentry || !upperdentry || d_inode(lowerdentry)->i_nlink == 1)
		return fallback;

	err = ovl_getxattr_upper(ofs, upperdentry, OVL_XATTR_NLINK,
				 &buf, sizeof(buf) - 1);
	if (err < 0)
		goto fail;

	buf[err] = '\0';
	if ((buf[0] != 'L' && buf[0] != 'U') ||
	    (buf[1] != '+' && buf[1] != '-'))
		goto fail;

	err = kstrtoint(buf + 1, 10, &nlink_diff);
	if (err < 0)
		goto fail;

	nlink = d_inode(buf[0] == 'L' ? lowerdentry : upperdentry)->i_nlink;
	nlink += nlink_diff;

	if (nlink <= 0)
		goto fail;

	return nlink;

fail:
	pr_warn_ratelimited("failed to get index nlink (%pd2, err=%i)\n",
			    upperdentry, err);
	return fallback;
}

struct inode *ovl_new_inode(struct super_block *sb, umode_t mode, dev_t rdev)
{
	struct inode *inode;

	inode = new_inode(sb);
	if (inode)
		ovl_fill_inode(inode, mode, rdev);

	return inode;
}

static int ovl_inode_test(struct inode *inode, void *data)
{
	return inode->i_private == data;
}

static int ovl_inode_set(struct inode *inode, void *data)
{
	inode->i_private = data;
	return 0;
}

static bool ovl_verify_inode(struct inode *inode, struct dentry *lowerdentry,
			     struct dentry *upperdentry, bool strict)
{
	 
	if (S_ISDIR(inode->i_mode) && strict) {
		 
		if (!lowerdentry && ovl_inode_lower(inode))
			return false;

		 
		if (!upperdentry && ovl_inode_upper(inode))
			return false;
	}

	 
	if (lowerdentry && ovl_inode_lower(inode) != d_inode(lowerdentry))
		return false;

	 
	if (upperdentry && ovl_inode_upper(inode) != d_inode(upperdentry))
		return false;

	return true;
}

struct inode *ovl_lookup_inode(struct super_block *sb, struct dentry *real,
			       bool is_upper)
{
	struct inode *inode, *key = d_inode(real);

	inode = ilookup5(sb, (unsigned long) key, ovl_inode_test, key);
	if (!inode)
		return NULL;

	if (!ovl_verify_inode(inode, is_upper ? NULL : real,
			      is_upper ? real : NULL, false)) {
		iput(inode);
		return ERR_PTR(-ESTALE);
	}

	return inode;
}

bool ovl_lookup_trap_inode(struct super_block *sb, struct dentry *dir)
{
	struct inode *key = d_inode(dir);
	struct inode *trap;
	bool res;

	trap = ilookup5(sb, (unsigned long) key, ovl_inode_test, key);
	if (!trap)
		return false;

	res = IS_DEADDIR(trap) && !ovl_inode_upper(trap) &&
				  !ovl_inode_lower(trap);

	iput(trap);
	return res;
}

 
struct inode *ovl_get_trap_inode(struct super_block *sb, struct dentry *dir)
{
	struct inode *key = d_inode(dir);
	struct inode *trap;

	if (!d_is_dir(dir))
		return ERR_PTR(-ENOTDIR);

	trap = iget5_locked(sb, (unsigned long) key, ovl_inode_test,
			    ovl_inode_set, key);
	if (!trap)
		return ERR_PTR(-ENOMEM);

	if (!(trap->i_state & I_NEW)) {
		 
		iput(trap);
		return ERR_PTR(-ELOOP);
	}

	trap->i_mode = S_IFDIR;
	trap->i_flags = S_DEAD;
	unlock_new_inode(trap);

	return trap;
}

 
static bool ovl_hash_bylower(struct super_block *sb, struct dentry *upper,
			     struct dentry *lower, bool index)
{
	struct ovl_fs *ofs = OVL_FS(sb);

	 
	if (!lower)
		return false;

	 
	if (index)
		return true;

	 
	if (!ovl_upper_mnt(ofs))
		return true;

	 
	if ((upper || !ovl_indexdir(sb)) &&
	    !d_is_dir(lower) && d_inode(lower)->i_nlink > 1)
		return false;

	 
	if (ofs->config.nfs_export && upper)
		return false;

	 
	return true;
}

static struct inode *ovl_iget5(struct super_block *sb, struct inode *newinode,
			       struct inode *key)
{
	return newinode ? inode_insert5(newinode, (unsigned long) key,
					 ovl_inode_test, ovl_inode_set, key) :
			  iget5_locked(sb, (unsigned long) key,
				       ovl_inode_test, ovl_inode_set, key);
}

struct inode *ovl_get_inode(struct super_block *sb,
			    struct ovl_inode_params *oip)
{
	struct ovl_fs *ofs = OVL_FS(sb);
	struct dentry *upperdentry = oip->upperdentry;
	struct ovl_path *lowerpath = ovl_lowerpath(oip->oe);
	struct inode *realinode = upperdentry ? d_inode(upperdentry) : NULL;
	struct inode *inode;
	struct dentry *lowerdentry = lowerpath ? lowerpath->dentry : NULL;
	struct path realpath = {
		.dentry = upperdentry ?: lowerdentry,
		.mnt = upperdentry ? ovl_upper_mnt(ofs) : lowerpath->layer->mnt,
	};
	bool bylower = ovl_hash_bylower(sb, upperdentry, lowerdentry,
					oip->index);
	int fsid = bylower ? lowerpath->layer->fsid : 0;
	bool is_dir;
	unsigned long ino = 0;
	int err = oip->newinode ? -EEXIST : -ENOMEM;

	if (!realinode)
		realinode = d_inode(lowerdentry);

	 
	is_dir = S_ISDIR(realinode->i_mode);
	if (upperdentry || bylower) {
		struct inode *key = d_inode(bylower ? lowerdentry :
						      upperdentry);
		unsigned int nlink = is_dir ? 1 : realinode->i_nlink;

		inode = ovl_iget5(sb, oip->newinode, key);
		if (!inode)
			goto out_err;
		if (!(inode->i_state & I_NEW)) {
			 
			if (!ovl_verify_inode(inode, lowerdentry, upperdentry,
					      true)) {
				iput(inode);
				err = -ESTALE;
				goto out_err;
			}

			dput(upperdentry);
			ovl_free_entry(oip->oe);
			kfree(oip->redirect);
			kfree(oip->lowerdata_redirect);
			goto out;
		}

		 
		if (!is_dir)
			nlink = ovl_get_nlink(ofs, lowerdentry, upperdentry,
					      nlink);
		set_nlink(inode, nlink);
		ino = key->i_ino;
	} else {
		 
		inode = new_inode(sb);
		if (!inode) {
			err = -ENOMEM;
			goto out_err;
		}
		ino = realinode->i_ino;
		fsid = lowerpath->layer->fsid;
	}
	ovl_fill_inode(inode, realinode->i_mode, realinode->i_rdev);
	ovl_inode_init(inode, oip, ino, fsid);

	if (upperdentry && ovl_is_impuredir(sb, upperdentry))
		ovl_set_flag(OVL_IMPURE, inode);

	if (oip->index)
		ovl_set_flag(OVL_INDEX, inode);

	if (bylower)
		ovl_set_flag(OVL_CONST_INO, inode);

	 
	if (is_dir) {
		if (((upperdentry && lowerdentry) || ovl_numlower(oip->oe) > 1) ||
		    ovl_path_check_origin_xattr(ofs, &realpath)) {
			ovl_set_flag(OVL_WHITEOUTS, inode);
		}
	}

	 
	if (upperdentry)
		ovl_check_protattr(inode, upperdentry);

	if (inode->i_state & I_NEW)
		unlock_new_inode(inode);
out:
	return inode;

out_err:
	pr_warn_ratelimited("failed to get inode (%i)\n", err);
	inode = ERR_PTR(err);
	goto out;
}
