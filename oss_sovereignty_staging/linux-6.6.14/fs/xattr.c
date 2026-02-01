
 
#include <linux/fs.h>
#include <linux/filelock.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/xattr.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/security.h>
#include <linux/evm.h>
#include <linux/syscalls.h>
#include <linux/export.h>
#include <linux/fsnotify.h>
#include <linux/audit.h>
#include <linux/vmalloc.h>
#include <linux/posix_acl_xattr.h>

#include <linux/uaccess.h>

#include "internal.h"

static const char *
strcmp_prefix(const char *a, const char *a_prefix)
{
	while (*a_prefix && *a == *a_prefix) {
		a++;
		a_prefix++;
	}
	return *a_prefix ? NULL : a;
}

 
#define for_each_xattr_handler(handlers, handler)		\
	if (handlers)						\
		for ((handler) = *(handlers)++;			\
			(handler) != NULL;			\
			(handler) = *(handlers)++)

 
static const struct xattr_handler *
xattr_resolve_name(struct inode *inode, const char **name)
{
	const struct xattr_handler **handlers = inode->i_sb->s_xattr;
	const struct xattr_handler *handler;

	if (!(inode->i_opflags & IOP_XATTR)) {
		if (unlikely(is_bad_inode(inode)))
			return ERR_PTR(-EIO);
		return ERR_PTR(-EOPNOTSUPP);
	}
	for_each_xattr_handler(handlers, handler) {
		const char *n;

		n = strcmp_prefix(*name, xattr_prefix(handler));
		if (n) {
			if (!handler->prefix ^ !*n) {
				if (*n)
					continue;
				return ERR_PTR(-EINVAL);
			}
			*name = n;
			return handler;
		}
	}
	return ERR_PTR(-EOPNOTSUPP);
}

 
int may_write_xattr(struct mnt_idmap *idmap, struct inode *inode)
{
	if (IS_IMMUTABLE(inode))
		return -EPERM;
	if (IS_APPEND(inode))
		return -EPERM;
	if (HAS_UNMAPPED_ID(idmap, inode))
		return -EPERM;
	return 0;
}

 
static int
xattr_permission(struct mnt_idmap *idmap, struct inode *inode,
		 const char *name, int mask)
{
	if (mask & MAY_WRITE) {
		int ret;

		ret = may_write_xattr(idmap, inode);
		if (ret)
			return ret;
	}

	 
	if (!strncmp(name, XATTR_SECURITY_PREFIX, XATTR_SECURITY_PREFIX_LEN) ||
	    !strncmp(name, XATTR_SYSTEM_PREFIX, XATTR_SYSTEM_PREFIX_LEN))
		return 0;

	 
	if (!strncmp(name, XATTR_TRUSTED_PREFIX, XATTR_TRUSTED_PREFIX_LEN)) {
		if (!capable(CAP_SYS_ADMIN))
			return (mask & MAY_WRITE) ? -EPERM : -ENODATA;
		return 0;
	}

	 
	if (!strncmp(name, XATTR_USER_PREFIX, XATTR_USER_PREFIX_LEN)) {
		if (!S_ISREG(inode->i_mode) && !S_ISDIR(inode->i_mode))
			return (mask & MAY_WRITE) ? -EPERM : -ENODATA;
		if (S_ISDIR(inode->i_mode) && (inode->i_mode & S_ISVTX) &&
		    (mask & MAY_WRITE) &&
		    !inode_owner_or_capable(idmap, inode))
			return -EPERM;
	}

	return inode_permission(idmap, inode, mask);
}

 
int
xattr_supports_user_prefix(struct inode *inode)
{
	const struct xattr_handler **handlers = inode->i_sb->s_xattr;
	const struct xattr_handler *handler;

	if (!(inode->i_opflags & IOP_XATTR)) {
		if (unlikely(is_bad_inode(inode)))
			return -EIO;
		return -EOPNOTSUPP;
	}

	for_each_xattr_handler(handlers, handler) {
		if (!strncmp(xattr_prefix(handler), XATTR_USER_PREFIX,
			     XATTR_USER_PREFIX_LEN))
			return 0;
	}

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xattr_supports_user_prefix);

int
__vfs_setxattr(struct mnt_idmap *idmap, struct dentry *dentry,
	       struct inode *inode, const char *name, const void *value,
	       size_t size, int flags)
{
	const struct xattr_handler *handler;

	if (is_posix_acl_xattr(name))
		return -EOPNOTSUPP;

	handler = xattr_resolve_name(inode, &name);
	if (IS_ERR(handler))
		return PTR_ERR(handler);
	if (!handler->set)
		return -EOPNOTSUPP;
	if (size == 0)
		value = "";   
	return handler->set(handler, idmap, dentry, inode, name, value,
			    size, flags);
}
EXPORT_SYMBOL(__vfs_setxattr);

 
int __vfs_setxattr_noperm(struct mnt_idmap *idmap,
			  struct dentry *dentry, const char *name,
			  const void *value, size_t size, int flags)
{
	struct inode *inode = dentry->d_inode;
	int error = -EAGAIN;
	int issec = !strncmp(name, XATTR_SECURITY_PREFIX,
				   XATTR_SECURITY_PREFIX_LEN);

	if (issec)
		inode->i_flags &= ~S_NOSEC;
	if (inode->i_opflags & IOP_XATTR) {
		error = __vfs_setxattr(idmap, dentry, inode, name, value,
				       size, flags);
		if (!error) {
			fsnotify_xattr(dentry);
			security_inode_post_setxattr(dentry, name, value,
						     size, flags);
		}
	} else {
		if (unlikely(is_bad_inode(inode)))
			return -EIO;
	}
	if (error == -EAGAIN) {
		error = -EOPNOTSUPP;

		if (issec) {
			const char *suffix = name + XATTR_SECURITY_PREFIX_LEN;

			error = security_inode_setsecurity(inode, suffix, value,
							   size, flags);
			if (!error)
				fsnotify_xattr(dentry);
		}
	}

	return error;
}

 
int
__vfs_setxattr_locked(struct mnt_idmap *idmap, struct dentry *dentry,
		      const char *name, const void *value, size_t size,
		      int flags, struct inode **delegated_inode)
{
	struct inode *inode = dentry->d_inode;
	int error;

	error = xattr_permission(idmap, inode, name, MAY_WRITE);
	if (error)
		return error;

	error = security_inode_setxattr(idmap, dentry, name, value, size,
					flags);
	if (error)
		goto out;

	error = try_break_deleg(inode, delegated_inode);
	if (error)
		goto out;

	error = __vfs_setxattr_noperm(idmap, dentry, name, value,
				      size, flags);

out:
	return error;
}
EXPORT_SYMBOL_GPL(__vfs_setxattr_locked);

int
vfs_setxattr(struct mnt_idmap *idmap, struct dentry *dentry,
	     const char *name, const void *value, size_t size, int flags)
{
	struct inode *inode = dentry->d_inode;
	struct inode *delegated_inode = NULL;
	const void  *orig_value = value;
	int error;

	if (size && strcmp(name, XATTR_NAME_CAPS) == 0) {
		error = cap_convert_nscap(idmap, dentry, &value, size);
		if (error < 0)
			return error;
		size = error;
	}

retry_deleg:
	inode_lock(inode);
	error = __vfs_setxattr_locked(idmap, dentry, name, value, size,
				      flags, &delegated_inode);
	inode_unlock(inode);

	if (delegated_inode) {
		error = break_deleg_wait(&delegated_inode);
		if (!error)
			goto retry_deleg;
	}
	if (value != orig_value)
		kfree(value);

	return error;
}
EXPORT_SYMBOL_GPL(vfs_setxattr);

static ssize_t
xattr_getsecurity(struct mnt_idmap *idmap, struct inode *inode,
		  const char *name, void *value, size_t size)
{
	void *buffer = NULL;
	ssize_t len;

	if (!value || !size) {
		len = security_inode_getsecurity(idmap, inode, name,
						 &buffer, false);
		goto out_noalloc;
	}

	len = security_inode_getsecurity(idmap, inode, name, &buffer,
					 true);
	if (len < 0)
		return len;
	if (size < len) {
		len = -ERANGE;
		goto out;
	}
	memcpy(value, buffer, len);
out:
	kfree(buffer);
out_noalloc:
	return len;
}

 
int
vfs_getxattr_alloc(struct mnt_idmap *idmap, struct dentry *dentry,
		   const char *name, char **xattr_value, size_t xattr_size,
		   gfp_t flags)
{
	const struct xattr_handler *handler;
	struct inode *inode = dentry->d_inode;
	char *value = *xattr_value;
	int error;

	error = xattr_permission(idmap, inode, name, MAY_READ);
	if (error)
		return error;

	handler = xattr_resolve_name(inode, &name);
	if (IS_ERR(handler))
		return PTR_ERR(handler);
	if (!handler->get)
		return -EOPNOTSUPP;
	error = handler->get(handler, dentry, inode, name, NULL, 0);
	if (error < 0)
		return error;

	if (!value || (error > xattr_size)) {
		value = krealloc(*xattr_value, error + 1, flags);
		if (!value)
			return -ENOMEM;
		memset(value, 0, error + 1);
	}

	error = handler->get(handler, dentry, inode, name, value, error);
	*xattr_value = value;
	return error;
}

ssize_t
__vfs_getxattr(struct dentry *dentry, struct inode *inode, const char *name,
	       void *value, size_t size)
{
	const struct xattr_handler *handler;

	if (is_posix_acl_xattr(name))
		return -EOPNOTSUPP;

	handler = xattr_resolve_name(inode, &name);
	if (IS_ERR(handler))
		return PTR_ERR(handler);
	if (!handler->get)
		return -EOPNOTSUPP;
	return handler->get(handler, dentry, inode, name, value, size);
}
EXPORT_SYMBOL(__vfs_getxattr);

ssize_t
vfs_getxattr(struct mnt_idmap *idmap, struct dentry *dentry,
	     const char *name, void *value, size_t size)
{
	struct inode *inode = dentry->d_inode;
	int error;

	error = xattr_permission(idmap, inode, name, MAY_READ);
	if (error)
		return error;

	error = security_inode_getxattr(dentry, name);
	if (error)
		return error;

	if (!strncmp(name, XATTR_SECURITY_PREFIX,
				XATTR_SECURITY_PREFIX_LEN)) {
		const char *suffix = name + XATTR_SECURITY_PREFIX_LEN;
		int ret = xattr_getsecurity(idmap, inode, suffix, value,
					    size);
		 
		if (ret == -EOPNOTSUPP)
			goto nolsm;
		return ret;
	}
nolsm:
	return __vfs_getxattr(dentry, inode, name, value, size);
}
EXPORT_SYMBOL_GPL(vfs_getxattr);

 
ssize_t
vfs_listxattr(struct dentry *dentry, char *list, size_t size)
{
	struct inode *inode = d_inode(dentry);
	ssize_t error;

	error = security_inode_listxattr(dentry);
	if (error)
		return error;

	if (inode->i_op->listxattr) {
		error = inode->i_op->listxattr(dentry, list, size);
	} else {
		error = security_inode_listsecurity(inode, list, size);
		if (size && error > size)
			error = -ERANGE;
	}
	return error;
}
EXPORT_SYMBOL_GPL(vfs_listxattr);

int
__vfs_removexattr(struct mnt_idmap *idmap, struct dentry *dentry,
		  const char *name)
{
	struct inode *inode = d_inode(dentry);
	const struct xattr_handler *handler;

	if (is_posix_acl_xattr(name))
		return -EOPNOTSUPP;

	handler = xattr_resolve_name(inode, &name);
	if (IS_ERR(handler))
		return PTR_ERR(handler);
	if (!handler->set)
		return -EOPNOTSUPP;
	return handler->set(handler, idmap, dentry, inode, name, NULL, 0,
			    XATTR_REPLACE);
}
EXPORT_SYMBOL(__vfs_removexattr);

 
int
__vfs_removexattr_locked(struct mnt_idmap *idmap,
			 struct dentry *dentry, const char *name,
			 struct inode **delegated_inode)
{
	struct inode *inode = dentry->d_inode;
	int error;

	error = xattr_permission(idmap, inode, name, MAY_WRITE);
	if (error)
		return error;

	error = security_inode_removexattr(idmap, dentry, name);
	if (error)
		goto out;

	error = try_break_deleg(inode, delegated_inode);
	if (error)
		goto out;

	error = __vfs_removexattr(idmap, dentry, name);

	if (!error) {
		fsnotify_xattr(dentry);
		evm_inode_post_removexattr(dentry, name);
	}

out:
	return error;
}
EXPORT_SYMBOL_GPL(__vfs_removexattr_locked);

int
vfs_removexattr(struct mnt_idmap *idmap, struct dentry *dentry,
		const char *name)
{
	struct inode *inode = dentry->d_inode;
	struct inode *delegated_inode = NULL;
	int error;

retry_deleg:
	inode_lock(inode);
	error = __vfs_removexattr_locked(idmap, dentry,
					 name, &delegated_inode);
	inode_unlock(inode);

	if (delegated_inode) {
		error = break_deleg_wait(&delegated_inode);
		if (!error)
			goto retry_deleg;
	}

	return error;
}
EXPORT_SYMBOL_GPL(vfs_removexattr);

 

int setxattr_copy(const char __user *name, struct xattr_ctx *ctx)
{
	int error;

	if (ctx->flags & ~(XATTR_CREATE|XATTR_REPLACE))
		return -EINVAL;

	error = strncpy_from_user(ctx->kname->name, name,
				sizeof(ctx->kname->name));
	if (error == 0 || error == sizeof(ctx->kname->name))
		return  -ERANGE;
	if (error < 0)
		return error;

	error = 0;
	if (ctx->size) {
		if (ctx->size > XATTR_SIZE_MAX)
			return -E2BIG;

		ctx->kvalue = vmemdup_user(ctx->cvalue, ctx->size);
		if (IS_ERR(ctx->kvalue)) {
			error = PTR_ERR(ctx->kvalue);
			ctx->kvalue = NULL;
		}
	}

	return error;
}

int do_setxattr(struct mnt_idmap *idmap, struct dentry *dentry,
		struct xattr_ctx *ctx)
{
	if (is_posix_acl_xattr(ctx->kname->name))
		return do_set_acl(idmap, dentry, ctx->kname->name,
				  ctx->kvalue, ctx->size);

	return vfs_setxattr(idmap, dentry, ctx->kname->name,
			ctx->kvalue, ctx->size, ctx->flags);
}

static long
setxattr(struct mnt_idmap *idmap, struct dentry *d,
	const char __user *name, const void __user *value, size_t size,
	int flags)
{
	struct xattr_name kname;
	struct xattr_ctx ctx = {
		.cvalue   = value,
		.kvalue   = NULL,
		.size     = size,
		.kname    = &kname,
		.flags    = flags,
	};
	int error;

	error = setxattr_copy(name, &ctx);
	if (error)
		return error;

	error = do_setxattr(idmap, d, &ctx);

	kvfree(ctx.kvalue);
	return error;
}

static int path_setxattr(const char __user *pathname,
			 const char __user *name, const void __user *value,
			 size_t size, int flags, unsigned int lookup_flags)
{
	struct path path;
	int error;

retry:
	error = user_path_at(AT_FDCWD, pathname, lookup_flags, &path);
	if (error)
		return error;
	error = mnt_want_write(path.mnt);
	if (!error) {
		error = setxattr(mnt_idmap(path.mnt), path.dentry, name,
				 value, size, flags);
		mnt_drop_write(path.mnt);
	}
	path_put(&path);
	if (retry_estale(error, lookup_flags)) {
		lookup_flags |= LOOKUP_REVAL;
		goto retry;
	}
	return error;
}

SYSCALL_DEFINE5(setxattr, const char __user *, pathname,
		const char __user *, name, const void __user *, value,
		size_t, size, int, flags)
{
	return path_setxattr(pathname, name, value, size, flags, LOOKUP_FOLLOW);
}

SYSCALL_DEFINE5(lsetxattr, const char __user *, pathname,
		const char __user *, name, const void __user *, value,
		size_t, size, int, flags)
{
	return path_setxattr(pathname, name, value, size, flags, 0);
}

SYSCALL_DEFINE5(fsetxattr, int, fd, const char __user *, name,
		const void __user *,value, size_t, size, int, flags)
{
	struct fd f = fdget(fd);
	int error = -EBADF;

	if (!f.file)
		return error;
	audit_file(f.file);
	error = mnt_want_write_file(f.file);
	if (!error) {
		error = setxattr(file_mnt_idmap(f.file),
				 f.file->f_path.dentry, name,
				 value, size, flags);
		mnt_drop_write_file(f.file);
	}
	fdput(f);
	return error;
}

 
ssize_t
do_getxattr(struct mnt_idmap *idmap, struct dentry *d,
	struct xattr_ctx *ctx)
{
	ssize_t error;
	char *kname = ctx->kname->name;

	if (ctx->size) {
		if (ctx->size > XATTR_SIZE_MAX)
			ctx->size = XATTR_SIZE_MAX;
		ctx->kvalue = kvzalloc(ctx->size, GFP_KERNEL);
		if (!ctx->kvalue)
			return -ENOMEM;
	}

	if (is_posix_acl_xattr(ctx->kname->name))
		error = do_get_acl(idmap, d, kname, ctx->kvalue, ctx->size);
	else
		error = vfs_getxattr(idmap, d, kname, ctx->kvalue, ctx->size);
	if (error > 0) {
		if (ctx->size && copy_to_user(ctx->value, ctx->kvalue, error))
			error = -EFAULT;
	} else if (error == -ERANGE && ctx->size >= XATTR_SIZE_MAX) {
		 
		error = -E2BIG;
	}

	return error;
}

static ssize_t
getxattr(struct mnt_idmap *idmap, struct dentry *d,
	 const char __user *name, void __user *value, size_t size)
{
	ssize_t error;
	struct xattr_name kname;
	struct xattr_ctx ctx = {
		.value    = value,
		.kvalue   = NULL,
		.size     = size,
		.kname    = &kname,
		.flags    = 0,
	};

	error = strncpy_from_user(kname.name, name, sizeof(kname.name));
	if (error == 0 || error == sizeof(kname.name))
		error = -ERANGE;
	if (error < 0)
		return error;

	error =  do_getxattr(idmap, d, &ctx);

	kvfree(ctx.kvalue);
	return error;
}

static ssize_t path_getxattr(const char __user *pathname,
			     const char __user *name, void __user *value,
			     size_t size, unsigned int lookup_flags)
{
	struct path path;
	ssize_t error;
retry:
	error = user_path_at(AT_FDCWD, pathname, lookup_flags, &path);
	if (error)
		return error;
	error = getxattr(mnt_idmap(path.mnt), path.dentry, name, value, size);
	path_put(&path);
	if (retry_estale(error, lookup_flags)) {
		lookup_flags |= LOOKUP_REVAL;
		goto retry;
	}
	return error;
}

SYSCALL_DEFINE4(getxattr, const char __user *, pathname,
		const char __user *, name, void __user *, value, size_t, size)
{
	return path_getxattr(pathname, name, value, size, LOOKUP_FOLLOW);
}

SYSCALL_DEFINE4(lgetxattr, const char __user *, pathname,
		const char __user *, name, void __user *, value, size_t, size)
{
	return path_getxattr(pathname, name, value, size, 0);
}

SYSCALL_DEFINE4(fgetxattr, int, fd, const char __user *, name,
		void __user *, value, size_t, size)
{
	struct fd f = fdget(fd);
	ssize_t error = -EBADF;

	if (!f.file)
		return error;
	audit_file(f.file);
	error = getxattr(file_mnt_idmap(f.file), f.file->f_path.dentry,
			 name, value, size);
	fdput(f);
	return error;
}

 
static ssize_t
listxattr(struct dentry *d, char __user *list, size_t size)
{
	ssize_t error;
	char *klist = NULL;

	if (size) {
		if (size > XATTR_LIST_MAX)
			size = XATTR_LIST_MAX;
		klist = kvmalloc(size, GFP_KERNEL);
		if (!klist)
			return -ENOMEM;
	}

	error = vfs_listxattr(d, klist, size);
	if (error > 0) {
		if (size && copy_to_user(list, klist, error))
			error = -EFAULT;
	} else if (error == -ERANGE && size >= XATTR_LIST_MAX) {
		 
		error = -E2BIG;
	}

	kvfree(klist);

	return error;
}

static ssize_t path_listxattr(const char __user *pathname, char __user *list,
			      size_t size, unsigned int lookup_flags)
{
	struct path path;
	ssize_t error;
retry:
	error = user_path_at(AT_FDCWD, pathname, lookup_flags, &path);
	if (error)
		return error;
	error = listxattr(path.dentry, list, size);
	path_put(&path);
	if (retry_estale(error, lookup_flags)) {
		lookup_flags |= LOOKUP_REVAL;
		goto retry;
	}
	return error;
}

SYSCALL_DEFINE3(listxattr, const char __user *, pathname, char __user *, list,
		size_t, size)
{
	return path_listxattr(pathname, list, size, LOOKUP_FOLLOW);
}

SYSCALL_DEFINE3(llistxattr, const char __user *, pathname, char __user *, list,
		size_t, size)
{
	return path_listxattr(pathname, list, size, 0);
}

SYSCALL_DEFINE3(flistxattr, int, fd, char __user *, list, size_t, size)
{
	struct fd f = fdget(fd);
	ssize_t error = -EBADF;

	if (!f.file)
		return error;
	audit_file(f.file);
	error = listxattr(f.file->f_path.dentry, list, size);
	fdput(f);
	return error;
}

 
static long
removexattr(struct mnt_idmap *idmap, struct dentry *d,
	    const char __user *name)
{
	int error;
	char kname[XATTR_NAME_MAX + 1];

	error = strncpy_from_user(kname, name, sizeof(kname));
	if (error == 0 || error == sizeof(kname))
		error = -ERANGE;
	if (error < 0)
		return error;

	if (is_posix_acl_xattr(kname))
		return vfs_remove_acl(idmap, d, kname);

	return vfs_removexattr(idmap, d, kname);
}

static int path_removexattr(const char __user *pathname,
			    const char __user *name, unsigned int lookup_flags)
{
	struct path path;
	int error;
retry:
	error = user_path_at(AT_FDCWD, pathname, lookup_flags, &path);
	if (error)
		return error;
	error = mnt_want_write(path.mnt);
	if (!error) {
		error = removexattr(mnt_idmap(path.mnt), path.dentry, name);
		mnt_drop_write(path.mnt);
	}
	path_put(&path);
	if (retry_estale(error, lookup_flags)) {
		lookup_flags |= LOOKUP_REVAL;
		goto retry;
	}
	return error;
}

SYSCALL_DEFINE2(removexattr, const char __user *, pathname,
		const char __user *, name)
{
	return path_removexattr(pathname, name, LOOKUP_FOLLOW);
}

SYSCALL_DEFINE2(lremovexattr, const char __user *, pathname,
		const char __user *, name)
{
	return path_removexattr(pathname, name, 0);
}

SYSCALL_DEFINE2(fremovexattr, int, fd, const char __user *, name)
{
	struct fd f = fdget(fd);
	int error = -EBADF;

	if (!f.file)
		return error;
	audit_file(f.file);
	error = mnt_want_write_file(f.file);
	if (!error) {
		error = removexattr(file_mnt_idmap(f.file),
				    f.file->f_path.dentry, name);
		mnt_drop_write_file(f.file);
	}
	fdput(f);
	return error;
}

int xattr_list_one(char **buffer, ssize_t *remaining_size, const char *name)
{
	size_t len;

	len = strlen(name) + 1;
	if (*buffer) {
		if (*remaining_size < len)
			return -ERANGE;
		memcpy(*buffer, name, len);
		*buffer += len;
	}
	*remaining_size -= len;
	return 0;
}

 
ssize_t
generic_listxattr(struct dentry *dentry, char *buffer, size_t buffer_size)
{
	const struct xattr_handler *handler, **handlers = dentry->d_sb->s_xattr;
	ssize_t remaining_size = buffer_size;
	int err = 0;

	for_each_xattr_handler(handlers, handler) {
		if (!handler->name || (handler->list && !handler->list(dentry)))
			continue;
		err = xattr_list_one(&buffer, &remaining_size, handler->name);
		if (err)
			return err;
	}

	return err ? err : buffer_size - remaining_size;
}
EXPORT_SYMBOL(generic_listxattr);

 
const char *xattr_full_name(const struct xattr_handler *handler,
			    const char *name)
{
	size_t prefix_len = strlen(xattr_prefix(handler));

	return name - prefix_len;
}
EXPORT_SYMBOL(xattr_full_name);

 
size_t simple_xattr_space(const char *name, size_t size)
{
	 
	return 40 + size + strlen(name);
}

 
void simple_xattr_free(struct simple_xattr *xattr)
{
	if (xattr)
		kfree(xattr->name);
	kvfree(xattr);
}

 
struct simple_xattr *simple_xattr_alloc(const void *value, size_t size)
{
	struct simple_xattr *new_xattr;
	size_t len;

	 
	len = sizeof(*new_xattr) + size;
	if (len < sizeof(*new_xattr))
		return NULL;

	new_xattr = kvmalloc(len, GFP_KERNEL_ACCOUNT);
	if (!new_xattr)
		return NULL;

	new_xattr->size = size;
	memcpy(new_xattr->value, value, size);
	return new_xattr;
}

 
static int rbtree_simple_xattr_cmp(const void *key, const struct rb_node *node)
{
	const char *xattr_name = key;
	const struct simple_xattr *xattr;

	xattr = rb_entry(node, struct simple_xattr, rb_node);
	return strcmp(xattr->name, xattr_name);
}

 
static int rbtree_simple_xattr_node_cmp(struct rb_node *new_node,
					const struct rb_node *node)
{
	struct simple_xattr *xattr;
	xattr = rb_entry(new_node, struct simple_xattr, rb_node);
	return rbtree_simple_xattr_cmp(xattr->name, node);
}

 
int simple_xattr_get(struct simple_xattrs *xattrs, const char *name,
		     void *buffer, size_t size)
{
	struct simple_xattr *xattr = NULL;
	struct rb_node *rbp;
	int ret = -ENODATA;

	read_lock(&xattrs->lock);
	rbp = rb_find(name, &xattrs->rb_root, rbtree_simple_xattr_cmp);
	if (rbp) {
		xattr = rb_entry(rbp, struct simple_xattr, rb_node);
		ret = xattr->size;
		if (buffer) {
			if (size < xattr->size)
				ret = -ERANGE;
			else
				memcpy(buffer, xattr->value, xattr->size);
		}
	}
	read_unlock(&xattrs->lock);
	return ret;
}

 
struct simple_xattr *simple_xattr_set(struct simple_xattrs *xattrs,
				      const char *name, const void *value,
				      size_t size, int flags)
{
	struct simple_xattr *old_xattr = NULL, *new_xattr = NULL;
	struct rb_node *parent = NULL, **rbp;
	int err = 0, ret;

	 
	if (value) {
		new_xattr = simple_xattr_alloc(value, size);
		if (!new_xattr)
			return ERR_PTR(-ENOMEM);

		new_xattr->name = kstrdup(name, GFP_KERNEL_ACCOUNT);
		if (!new_xattr->name) {
			simple_xattr_free(new_xattr);
			return ERR_PTR(-ENOMEM);
		}
	}

	write_lock(&xattrs->lock);
	rbp = &xattrs->rb_root.rb_node;
	while (*rbp) {
		parent = *rbp;
		ret = rbtree_simple_xattr_cmp(name, *rbp);
		if (ret < 0)
			rbp = &(*rbp)->rb_left;
		else if (ret > 0)
			rbp = &(*rbp)->rb_right;
		else
			old_xattr = rb_entry(*rbp, struct simple_xattr, rb_node);
		if (old_xattr)
			break;
	}

	if (old_xattr) {
		 
		if (flags & XATTR_CREATE) {
			err = -EEXIST;
			goto out_unlock;
		}

		if (new_xattr)
			rb_replace_node(&old_xattr->rb_node,
					&new_xattr->rb_node, &xattrs->rb_root);
		else
			rb_erase(&old_xattr->rb_node, &xattrs->rb_root);
	} else {
		 
		if (flags & XATTR_REPLACE) {
			err = -ENODATA;
			goto out_unlock;
		}

		 
		if (new_xattr) {
			rb_link_node(&new_xattr->rb_node, parent, rbp);
			rb_insert_color(&new_xattr->rb_node, &xattrs->rb_root);
		}

		 
	}

out_unlock:
	write_unlock(&xattrs->lock);
	if (!err)
		return old_xattr;
	simple_xattr_free(new_xattr);
	return ERR_PTR(err);
}

static bool xattr_is_trusted(const char *name)
{
	return !strncmp(name, XATTR_TRUSTED_PREFIX, XATTR_TRUSTED_PREFIX_LEN);
}

 
ssize_t simple_xattr_list(struct inode *inode, struct simple_xattrs *xattrs,
			  char *buffer, size_t size)
{
	bool trusted = ns_capable_noaudit(&init_user_ns, CAP_SYS_ADMIN);
	struct simple_xattr *xattr;
	struct rb_node *rbp;
	ssize_t remaining_size = size;
	int err = 0;

	err = posix_acl_listxattr(inode, &buffer, &remaining_size);
	if (err)
		return err;

	read_lock(&xattrs->lock);
	for (rbp = rb_first(&xattrs->rb_root); rbp; rbp = rb_next(rbp)) {
		xattr = rb_entry(rbp, struct simple_xattr, rb_node);

		 
		if (!trusted && xattr_is_trusted(xattr->name))
			continue;

		err = xattr_list_one(&buffer, &remaining_size, xattr->name);
		if (err)
			break;
	}
	read_unlock(&xattrs->lock);

	return err ? err : size - remaining_size;
}

 
static bool rbtree_simple_xattr_less(struct rb_node *new_node,
				     const struct rb_node *node)
{
	return rbtree_simple_xattr_node_cmp(new_node, node) < 0;
}

 
void simple_xattr_add(struct simple_xattrs *xattrs,
		      struct simple_xattr *new_xattr)
{
	write_lock(&xattrs->lock);
	rb_add(&new_xattr->rb_node, &xattrs->rb_root, rbtree_simple_xattr_less);
	write_unlock(&xattrs->lock);
}

 
void simple_xattrs_init(struct simple_xattrs *xattrs)
{
	xattrs->rb_root = RB_ROOT;
	rwlock_init(&xattrs->lock);
}

 
void simple_xattrs_free(struct simple_xattrs *xattrs, size_t *freed_space)
{
	struct rb_node *rbp;

	if (freed_space)
		*freed_space = 0;
	rbp = rb_first(&xattrs->rb_root);
	while (rbp) {
		struct simple_xattr *xattr;
		struct rb_node *rbp_next;

		rbp_next = rb_next(rbp);
		xattr = rb_entry(rbp, struct simple_xattr, rb_node);
		rb_erase(&xattr->rb_node, &xattrs->rb_root);
		if (freed_space)
			*freed_space += simple_xattr_space(xattr->name,
							   xattr->size);
		simple_xattr_free(xattr);
		rbp = rbp_next;
	}
}
