
 

#include <linux/export.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/sched/signal.h>
#include <linux/capability.h>
#include <linux/fsnotify.h>
#include <linux/fcntl.h>
#include <linux/filelock.h>
#include <linux/security.h>
#include <linux/evm.h>
#include <linux/ima.h>

#include "internal.h"

 
int setattr_should_drop_sgid(struct mnt_idmap *idmap,
			     const struct inode *inode)
{
	umode_t mode = inode->i_mode;

	if (!(mode & S_ISGID))
		return 0;
	if (mode & S_IXGRP)
		return ATTR_KILL_SGID;
	if (!in_group_or_capable(idmap, inode, i_gid_into_vfsgid(idmap, inode)))
		return ATTR_KILL_SGID;
	return 0;
}
EXPORT_SYMBOL(setattr_should_drop_sgid);

 
int setattr_should_drop_suidgid(struct mnt_idmap *idmap,
				struct inode *inode)
{
	umode_t mode = inode->i_mode;
	int kill = 0;

	 
	if (unlikely(mode & S_ISUID))
		kill = ATTR_KILL_SUID;

	kill |= setattr_should_drop_sgid(idmap, inode);

	if (unlikely(kill && !capable(CAP_FSETID) && S_ISREG(mode)))
		return kill;

	return 0;
}
EXPORT_SYMBOL(setattr_should_drop_suidgid);

 
static bool chown_ok(struct mnt_idmap *idmap,
		     const struct inode *inode, vfsuid_t ia_vfsuid)
{
	vfsuid_t vfsuid = i_uid_into_vfsuid(idmap, inode);
	if (vfsuid_eq_kuid(vfsuid, current_fsuid()) &&
	    vfsuid_eq(ia_vfsuid, vfsuid))
		return true;
	if (capable_wrt_inode_uidgid(idmap, inode, CAP_CHOWN))
		return true;
	if (!vfsuid_valid(vfsuid) &&
	    ns_capable(inode->i_sb->s_user_ns, CAP_CHOWN))
		return true;
	return false;
}

 
static bool chgrp_ok(struct mnt_idmap *idmap,
		     const struct inode *inode, vfsgid_t ia_vfsgid)
{
	vfsgid_t vfsgid = i_gid_into_vfsgid(idmap, inode);
	vfsuid_t vfsuid = i_uid_into_vfsuid(idmap, inode);
	if (vfsuid_eq_kuid(vfsuid, current_fsuid())) {
		if (vfsgid_eq(ia_vfsgid, vfsgid))
			return true;
		if (vfsgid_in_group_p(ia_vfsgid))
			return true;
	}
	if (capable_wrt_inode_uidgid(idmap, inode, CAP_CHOWN))
		return true;
	if (!vfsgid_valid(vfsgid) &&
	    ns_capable(inode->i_sb->s_user_ns, CAP_CHOWN))
		return true;
	return false;
}

 
int setattr_prepare(struct mnt_idmap *idmap, struct dentry *dentry,
		    struct iattr *attr)
{
	struct inode *inode = d_inode(dentry);
	unsigned int ia_valid = attr->ia_valid;

	 
	if (ia_valid & ATTR_SIZE) {
		int error = inode_newsize_ok(inode, attr->ia_size);
		if (error)
			return error;
	}

	 
	if (ia_valid & ATTR_FORCE)
		goto kill_priv;

	 
	if ((ia_valid & ATTR_UID) &&
	    !chown_ok(idmap, inode, attr->ia_vfsuid))
		return -EPERM;

	 
	if ((ia_valid & ATTR_GID) &&
	    !chgrp_ok(idmap, inode, attr->ia_vfsgid))
		return -EPERM;

	 
	if (ia_valid & ATTR_MODE) {
		vfsgid_t vfsgid;

		if (!inode_owner_or_capable(idmap, inode))
			return -EPERM;

		if (ia_valid & ATTR_GID)
			vfsgid = attr->ia_vfsgid;
		else
			vfsgid = i_gid_into_vfsgid(idmap, inode);

		 
		if (!in_group_or_capable(idmap, inode, vfsgid))
			attr->ia_mode &= ~S_ISGID;
	}

	 
	if (ia_valid & (ATTR_MTIME_SET | ATTR_ATIME_SET | ATTR_TIMES_SET)) {
		if (!inode_owner_or_capable(idmap, inode))
			return -EPERM;
	}

kill_priv:
	 
	if (ia_valid & ATTR_KILL_PRIV) {
		int error;

		error = security_inode_killpriv(idmap, dentry);
		if (error)
			return error;
	}

	return 0;
}
EXPORT_SYMBOL(setattr_prepare);

 
int inode_newsize_ok(const struct inode *inode, loff_t offset)
{
	if (offset < 0)
		return -EINVAL;
	if (inode->i_size < offset) {
		unsigned long limit;

		limit = rlimit(RLIMIT_FSIZE);
		if (limit != RLIM_INFINITY && offset > limit)
			goto out_sig;
		if (offset > inode->i_sb->s_maxbytes)
			goto out_big;
	} else {
		 
		if (IS_SWAPFILE(inode))
			return -ETXTBSY;
	}

	return 0;
out_sig:
	send_sig(SIGXFSZ, current, 0);
out_big:
	return -EFBIG;
}
EXPORT_SYMBOL(inode_newsize_ok);

 
void setattr_copy(struct mnt_idmap *idmap, struct inode *inode,
		  const struct iattr *attr)
{
	unsigned int ia_valid = attr->ia_valid;

	i_uid_update(idmap, attr, inode);
	i_gid_update(idmap, attr, inode);
	if (ia_valid & ATTR_ATIME)
		inode->i_atime = attr->ia_atime;
	if (ia_valid & ATTR_MTIME)
		inode->i_mtime = attr->ia_mtime;
	if (ia_valid & ATTR_CTIME)
		inode_set_ctime_to_ts(inode, attr->ia_ctime);
	if (ia_valid & ATTR_MODE) {
		umode_t mode = attr->ia_mode;
		if (!in_group_or_capable(idmap, inode,
					 i_gid_into_vfsgid(idmap, inode)))
			mode &= ~S_ISGID;
		inode->i_mode = mode;
	}
}
EXPORT_SYMBOL(setattr_copy);

int may_setattr(struct mnt_idmap *idmap, struct inode *inode,
		unsigned int ia_valid)
{
	int error;

	if (ia_valid & (ATTR_MODE | ATTR_UID | ATTR_GID | ATTR_TIMES_SET)) {
		if (IS_IMMUTABLE(inode) || IS_APPEND(inode))
			return -EPERM;
	}

	 
	if (ia_valid & ATTR_TOUCH) {
		if (IS_IMMUTABLE(inode))
			return -EPERM;

		if (!inode_owner_or_capable(idmap, inode)) {
			error = inode_permission(idmap, inode, MAY_WRITE);
			if (error)
				return error;
		}
	}
	return 0;
}
EXPORT_SYMBOL(may_setattr);

 
int notify_change(struct mnt_idmap *idmap, struct dentry *dentry,
		  struct iattr *attr, struct inode **delegated_inode)
{
	struct inode *inode = dentry->d_inode;
	umode_t mode = inode->i_mode;
	int error;
	struct timespec64 now;
	unsigned int ia_valid = attr->ia_valid;

	WARN_ON_ONCE(!inode_is_locked(inode));

	error = may_setattr(idmap, inode, ia_valid);
	if (error)
		return error;

	if ((ia_valid & ATTR_MODE)) {
		 
		if (S_ISLNK(inode->i_mode))
			return -EOPNOTSUPP;

		 
		if (is_sxid(attr->ia_mode))
			inode->i_flags &= ~S_NOSEC;
	}

	now = current_time(inode);

	attr->ia_ctime = now;
	if (!(ia_valid & ATTR_ATIME_SET))
		attr->ia_atime = now;
	else
		attr->ia_atime = timestamp_truncate(attr->ia_atime, inode);
	if (!(ia_valid & ATTR_MTIME_SET))
		attr->ia_mtime = now;
	else
		attr->ia_mtime = timestamp_truncate(attr->ia_mtime, inode);

	if (ia_valid & ATTR_KILL_PRIV) {
		error = security_inode_need_killpriv(dentry);
		if (error < 0)
			return error;
		if (error == 0)
			ia_valid = attr->ia_valid &= ~ATTR_KILL_PRIV;
	}

	 
	if ((ia_valid & (ATTR_KILL_SUID|ATTR_KILL_SGID)) &&
	    (ia_valid & ATTR_MODE))
		BUG();

	if (ia_valid & ATTR_KILL_SUID) {
		if (mode & S_ISUID) {
			ia_valid = attr->ia_valid |= ATTR_MODE;
			attr->ia_mode = (inode->i_mode & ~S_ISUID);
		}
	}
	if (ia_valid & ATTR_KILL_SGID) {
		if (mode & S_ISGID) {
			if (!(ia_valid & ATTR_MODE)) {
				ia_valid = attr->ia_valid |= ATTR_MODE;
				attr->ia_mode = inode->i_mode;
			}
			attr->ia_mode &= ~S_ISGID;
		}
	}
	if (!(attr->ia_valid & ~(ATTR_KILL_SUID | ATTR_KILL_SGID)))
		return 0;

	 
	if (ia_valid & ATTR_UID &&
	    !vfsuid_has_fsmapping(idmap, inode->i_sb->s_user_ns,
				  attr->ia_vfsuid))
		return -EOVERFLOW;
	if (ia_valid & ATTR_GID &&
	    !vfsgid_has_fsmapping(idmap, inode->i_sb->s_user_ns,
				  attr->ia_vfsgid))
		return -EOVERFLOW;

	 
	if (!(ia_valid & ATTR_UID) &&
	    !vfsuid_valid(i_uid_into_vfsuid(idmap, inode)))
		return -EOVERFLOW;
	if (!(ia_valid & ATTR_GID) &&
	    !vfsgid_valid(i_gid_into_vfsgid(idmap, inode)))
		return -EOVERFLOW;

	error = security_inode_setattr(idmap, dentry, attr);
	if (error)
		return error;
	error = try_break_deleg(inode, delegated_inode);
	if (error)
		return error;

	if (inode->i_op->setattr)
		error = inode->i_op->setattr(idmap, dentry, attr);
	else
		error = simple_setattr(idmap, dentry, attr);

	if (!error) {
		fsnotify_change(dentry, ia_valid);
		ima_inode_post_setattr(idmap, dentry);
		evm_inode_post_setattr(dentry, ia_valid);
	}

	return error;
}
EXPORT_SYMBOL(notify_change);
