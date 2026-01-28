#include <sys/zfs_znode.h>
#include <sys/zfs_vfsops.h>
#include <sys/zfs_vnops.h>
#include <sys/zap.h>
#include <sys/vfs.h>
#include <sys/zpl.h>
#include <linux/vfs_compat.h>
enum xattr_permission {
	XAPERM_DENY,
	XAPERM_ALLOW,
	XAPERM_COMPAT,
};
typedef struct xattr_filldir {
	size_t size;
	size_t offset;
	char *buf;
	struct dentry *dentry;
} xattr_filldir_t;
static enum xattr_permission zpl_xattr_permission(xattr_filldir_t *,
    const char *, int);
static int zfs_xattr_compat = 0;
static int
zpl_xattr_filldir(xattr_filldir_t *xf, const char *name, int name_len)
{
	enum xattr_permission perm;
	perm = zpl_xattr_permission(xf, name, name_len);
	if (perm == XAPERM_DENY)
		return (0);
	if (perm == XAPERM_COMPAT) {
		if (xf->buf) {
			if (xf->offset + XATTR_USER_PREFIX_LEN + 1 > xf->size)
				return (-ERANGE);
			memcpy(xf->buf + xf->offset, XATTR_USER_PREFIX,
			    XATTR_USER_PREFIX_LEN);
			xf->buf[xf->offset + XATTR_USER_PREFIX_LEN] = '\0';
		}
		xf->offset += XATTR_USER_PREFIX_LEN;
	}
	if (xf->buf) {
		if (xf->offset + name_len + 1 > xf->size)
			return (-ERANGE);
		memcpy(xf->buf + xf->offset, name, name_len);
		xf->buf[xf->offset + name_len] = '\0';
	}
	xf->offset += (name_len + 1);
	return (0);
}
static int
zpl_xattr_readdir(struct inode *dxip, xattr_filldir_t *xf)
{
	zap_cursor_t zc;
	zap_attribute_t	zap;
	int error;
	zap_cursor_init(&zc, ITOZSB(dxip)->z_os, ITOZ(dxip)->z_id);
	while ((error = -zap_cursor_retrieve(&zc, &zap)) == 0) {
		if (zap.za_integer_length != 8 || zap.za_num_integers != 1) {
			error = -ENXIO;
			break;
		}
		error = zpl_xattr_filldir(xf, zap.za_name, strlen(zap.za_name));
		if (error)
			break;
		zap_cursor_advance(&zc);
	}
	zap_cursor_fini(&zc);
	if (error == -ENOENT)
		error = 0;
	return (error);
}
static ssize_t
zpl_xattr_list_dir(xattr_filldir_t *xf, cred_t *cr)
{
	struct inode *ip = xf->dentry->d_inode;
	struct inode *dxip = NULL;
	znode_t *dxzp;
	int error;
	error = -zfs_lookup(ITOZ(ip), NULL, &dxzp, LOOKUP_XATTR,
	    cr, NULL, NULL);
	if (error) {
		if (error == -ENOENT)
			error = 0;
		return (error);
	}
	dxip = ZTOI(dxzp);
	error = zpl_xattr_readdir(dxip, xf);
	iput(dxip);
	return (error);
}
static ssize_t
zpl_xattr_list_sa(xattr_filldir_t *xf)
{
	znode_t *zp = ITOZ(xf->dentry->d_inode);
	nvpair_t *nvp = NULL;
	int error = 0;
	mutex_enter(&zp->z_lock);
	if (zp->z_xattr_cached == NULL)
		error = -zfs_sa_get_xattr(zp);
	mutex_exit(&zp->z_lock);
	if (error)
		return (error);
	ASSERT(zp->z_xattr_cached);
	while ((nvp = nvlist_next_nvpair(zp->z_xattr_cached, nvp)) != NULL) {
		ASSERT3U(nvpair_type(nvp), ==, DATA_TYPE_BYTE_ARRAY);
		error = zpl_xattr_filldir(xf, nvpair_name(nvp),
		    strlen(nvpair_name(nvp)));
		if (error)
			return (error);
	}
	return (0);
}
ssize_t
zpl_xattr_list(struct dentry *dentry, char *buffer, size_t buffer_size)
{
	znode_t *zp = ITOZ(dentry->d_inode);
	zfsvfs_t *zfsvfs = ZTOZSB(zp);
	xattr_filldir_t xf = { buffer_size, 0, buffer, dentry };
	cred_t *cr = CRED();
	fstrans_cookie_t cookie;
	int error = 0;
	crhold(cr);
	cookie = spl_fstrans_mark();
	if ((error = zpl_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		goto out1;
	rw_enter(&zp->z_xattr_lock, RW_READER);
	if (zfsvfs->z_use_sa && zp->z_is_sa) {
		error = zpl_xattr_list_sa(&xf);
		if (error)
			goto out;
	}
	error = zpl_xattr_list_dir(&xf, cr);
	if (error)
		goto out;
	error = xf.offset;
out:
	rw_exit(&zp->z_xattr_lock);
	zpl_exit(zfsvfs, FTAG);
out1:
	spl_fstrans_unmark(cookie);
	crfree(cr);
	return (error);
}
static int
zpl_xattr_get_dir(struct inode *ip, const char *name, void *value,
    size_t size, cred_t *cr)
{
	fstrans_cookie_t cookie;
	struct inode *xip = NULL;
	znode_t *dxzp = NULL;
	znode_t *xzp = NULL;
	int error;
	error = -zfs_lookup(ITOZ(ip), NULL, &dxzp, LOOKUP_XATTR,
	    cr, NULL, NULL);
	if (error)
		goto out;
	error = -zfs_lookup(dxzp, (char *)name, &xzp, 0, cr, NULL, NULL);
	if (error)
		goto out;
	xip = ZTOI(xzp);
	if (!size) {
		error = i_size_read(xip);
		goto out;
	}
	if (size < i_size_read(xip)) {
		error = -ERANGE;
		goto out;
	}
	struct iovec iov;
	iov.iov_base = (void *)value;
	iov.iov_len = size;
	zfs_uio_t uio;
	zfs_uio_iovec_init(&uio, &iov, 1, 0, UIO_SYSSPACE, size, 0);
	cookie = spl_fstrans_mark();
	error = -zfs_read(ITOZ(xip), &uio, 0, cr);
	spl_fstrans_unmark(cookie);
	if (error == 0)
		error = size - zfs_uio_resid(&uio);
out:
	if (xzp)
		zrele(xzp);
	if (dxzp)
		zrele(dxzp);
	return (error);
}
static int
zpl_xattr_get_sa(struct inode *ip, const char *name, void *value, size_t size)
{
	znode_t *zp = ITOZ(ip);
	uchar_t *nv_value;
	uint_t nv_size;
	int error = 0;
	ASSERT(RW_LOCK_HELD(&zp->z_xattr_lock));
	mutex_enter(&zp->z_lock);
	if (zp->z_xattr_cached == NULL)
		error = -zfs_sa_get_xattr(zp);
	mutex_exit(&zp->z_lock);
	if (error)
		return (error);
	ASSERT(zp->z_xattr_cached);
	error = -nvlist_lookup_byte_array(zp->z_xattr_cached, name,
	    &nv_value, &nv_size);
	if (error)
		return (error);
	if (size == 0 || value == NULL)
		return (nv_size);
	if (size < nv_size)
		return (-ERANGE);
	memcpy(value, nv_value, nv_size);
	return (nv_size);
}
static int
__zpl_xattr_get(struct inode *ip, const char *name, void *value, size_t size,
    cred_t *cr)
{
	znode_t *zp = ITOZ(ip);
	zfsvfs_t *zfsvfs = ZTOZSB(zp);
	int error;
	ASSERT(RW_LOCK_HELD(&zp->z_xattr_lock));
	if (zfsvfs->z_use_sa && zp->z_is_sa) {
		error = zpl_xattr_get_sa(ip, name, value, size);
		if (error != -ENOENT)
			goto out;
	}
	error = zpl_xattr_get_dir(ip, name, value, size, cr);
out:
	if (error == -ENOENT)
		error = -ENODATA;
	return (error);
}
#define	XATTR_NOENT	0x0
#define	XATTR_IN_SA	0x1
#define	XATTR_IN_DIR	0x2
static int
__zpl_xattr_where(struct inode *ip, const char *name, int *where, cred_t *cr)
{
	znode_t *zp = ITOZ(ip);
	zfsvfs_t *zfsvfs = ZTOZSB(zp);
	int error;
	ASSERT(where);
	ASSERT(RW_LOCK_HELD(&zp->z_xattr_lock));
	*where = XATTR_NOENT;
	if (zfsvfs->z_use_sa && zp->z_is_sa) {
		error = zpl_xattr_get_sa(ip, name, NULL, 0);
		if (error >= 0)
			*where |= XATTR_IN_SA;
		else if (error != -ENOENT)
			return (error);
	}
	error = zpl_xattr_get_dir(ip, name, NULL, 0, cr);
	if (error >= 0)
		*where |= XATTR_IN_DIR;
	else if (error != -ENOENT)
		return (error);
	if (*where == (XATTR_IN_SA|XATTR_IN_DIR))
		cmn_err(CE_WARN, "ZFS: inode %p has xattr \"%s\""
		    " in both SA and dir", ip, name);
	if (*where == XATTR_NOENT)
		error = -ENODATA;
	else
		error = 0;
	return (error);
}
static int
zpl_xattr_get(struct inode *ip, const char *name, void *value, size_t size)
{
	znode_t *zp = ITOZ(ip);
	zfsvfs_t *zfsvfs = ZTOZSB(zp);
	cred_t *cr = CRED();
	fstrans_cookie_t cookie;
	int error;
	crhold(cr);
	cookie = spl_fstrans_mark();
	if ((error = zpl_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		goto out;
	rw_enter(&zp->z_xattr_lock, RW_READER);
	error = __zpl_xattr_get(ip, name, value, size, cr);
	rw_exit(&zp->z_xattr_lock);
	zpl_exit(zfsvfs, FTAG);
out:
	spl_fstrans_unmark(cookie);
	crfree(cr);
	return (error);
}
static int
zpl_xattr_set_dir(struct inode *ip, const char *name, const void *value,
    size_t size, int flags, cred_t *cr)
{
	znode_t *dxzp = NULL;
	znode_t *xzp = NULL;
	vattr_t *vap = NULL;
	int lookup_flags, error;
	const int xattr_mode = S_IFREG | 0644;
	loff_t pos = 0;
	lookup_flags = LOOKUP_XATTR;
	if (value != NULL)
		lookup_flags |= CREATE_XATTR_DIR;
	error = -zfs_lookup(ITOZ(ip), NULL, &dxzp, lookup_flags,
	    cr, NULL, NULL);
	if (error)
		goto out;
	error = -zfs_lookup(dxzp, (char *)name, &xzp, 0, cr, NULL, NULL);
	if (error && (error != -ENOENT))
		goto out;
	error = 0;
	if (value == NULL) {
		if (xzp)
			error = -zfs_remove(dxzp, (char *)name, cr, 0);
		goto out;
	}
	if (xzp == NULL) {
		vap = kmem_zalloc(sizeof (vattr_t), KM_SLEEP);
		vap->va_mode = xattr_mode;
		vap->va_mask = ATTR_MODE;
		vap->va_uid = crgetuid(cr);
		vap->va_gid = crgetgid(cr);
		error = -zfs_create(dxzp, (char *)name, vap, 0, 0644, &xzp,
		    cr, ATTR_NOACLCHECK, NULL, zfs_init_idmap);
		if (error)
			goto out;
	}
	ASSERT(xzp != NULL);
	error = -zfs_freesp(xzp, 0, 0, xattr_mode, TRUE);
	if (error)
		goto out;
	error = -zfs_write_simple(xzp, value, size, pos, NULL);
out:
	if (error == 0) {
		zpl_inode_set_ctime_to_ts(ip, current_time(ip));
		zfs_mark_inode_dirty(ip);
	}
	if (vap)
		kmem_free(vap, sizeof (vattr_t));
	if (xzp)
		zrele(xzp);
	if (dxzp)
		zrele(dxzp);
	if (error == -ENOENT)
		error = -ENODATA;
	ASSERT3S(error, <=, 0);
	return (error);
}
static int
zpl_xattr_set_sa(struct inode *ip, const char *name, const void *value,
    size_t size, int flags, cred_t *cr)
{
	znode_t *zp = ITOZ(ip);
	nvlist_t *nvl;
	size_t sa_size;
	int error = 0;
	mutex_enter(&zp->z_lock);
	if (zp->z_xattr_cached == NULL)
		error = -zfs_sa_get_xattr(zp);
	mutex_exit(&zp->z_lock);
	if (error)
		return (error);
	ASSERT(zp->z_xattr_cached);
	nvl = zp->z_xattr_cached;
	if (value == NULL) {
		error = -nvlist_remove(nvl, name, DATA_TYPE_BYTE_ARRAY);
		if (error == -ENOENT)
			error = zpl_xattr_set_dir(ip, name, NULL, 0, flags, cr);
	} else {
		if (size > DXATTR_MAX_ENTRY_SIZE)
			return (-EFBIG);
		error = -nvlist_size(nvl, &sa_size, NV_ENCODE_XDR);
		if (error)
			return (error);
		if (sa_size > DXATTR_MAX_SA_SIZE)
			return (-EFBIG);
		error = -nvlist_add_byte_array(nvl, name,
		    (uchar_t *)value, size);
	}
	if (error == 0)
		error = -zfs_sa_set_xattr(zp, name, value, size);
	if (error) {
		nvlist_free(nvl);
		zp->z_xattr_cached = NULL;
	}
	ASSERT3S(error, <=, 0);
	return (error);
}
static int
zpl_xattr_set(struct inode *ip, const char *name, const void *value,
    size_t size, int flags)
{
	znode_t *zp = ITOZ(ip);
	zfsvfs_t *zfsvfs = ZTOZSB(zp);
	cred_t *cr = CRED();
	fstrans_cookie_t cookie;
	int where;
	int error;
	crhold(cr);
	cookie = spl_fstrans_mark();
	if ((error = zpl_enter_verify_zp(zfsvfs, zp, FTAG)) != 0)
		goto out1;
	rw_enter(&zp->z_xattr_lock, RW_WRITER);
	error = __zpl_xattr_where(ip, name, &where, cr);
	if (error < 0) {
		if (error != -ENODATA)
			goto out;
		if (flags & XATTR_REPLACE)
			goto out;
		error = 0;
		if (value == NULL)
			goto out;
	} else {
		error = -EEXIST;
		if (flags & XATTR_CREATE)
			goto out;
	}
	if (zfsvfs->z_use_sa && zp->z_is_sa &&
	    (zfsvfs->z_xattr_sa || (value == NULL && where & XATTR_IN_SA))) {
		error = zpl_xattr_set_sa(ip, name, value, size, flags, cr);
		if (error == 0) {
			if (where & XATTR_IN_DIR)
				zpl_xattr_set_dir(ip, name, NULL, 0, 0, cr);
			goto out;
		}
	}
	error = zpl_xattr_set_dir(ip, name, value, size, flags, cr);
	if (error == 0 && (where & XATTR_IN_SA))
		zpl_xattr_set_sa(ip, name, NULL, 0, 0, cr);
out:
	rw_exit(&zp->z_xattr_lock);
	zpl_exit(zfsvfs, FTAG);
out1:
	spl_fstrans_unmark(cookie);
	crfree(cr);
	ASSERT3S(error, <=, 0);
	return (error);
}
static int
__zpl_xattr_user_list(struct inode *ip, char *list, size_t list_size,
    const char *name, size_t name_len)
{
	return (ITOZSB(ip)->z_flags & ZSB_XATTR);
}
ZPL_XATTR_LIST_WRAPPER(zpl_xattr_user_list);
static int
__zpl_xattr_user_get(struct inode *ip, const char *name,
    void *value, size_t size)
{
	int error;
#ifndef HAVE_XATTR_HANDLER_NAME
	if (strcmp(name, "") == 0)
		return (-EINVAL);
#endif
	if (ZFS_XA_NS_PREFIX_FORBIDDEN(name))
		return (-EINVAL);
	if (!(ITOZSB(ip)->z_flags & ZSB_XATTR))
		return (-EOPNOTSUPP);
	char *xattr_name = kmem_asprintf("%s%s", XATTR_USER_PREFIX, name);
	error = zpl_xattr_get(ip, xattr_name, value, size);
	kmem_strfree(xattr_name);
	if (error == -ENODATA)
		error = zpl_xattr_get(ip, name, value, size);
	return (error);
}
ZPL_XATTR_GET_WRAPPER(zpl_xattr_user_get);
static int
__zpl_xattr_user_set(zidmap_t *user_ns,
    struct inode *ip, const char *name,
    const void *value, size_t size, int flags)
{
	(void) user_ns;
	int error = 0;
#ifndef HAVE_XATTR_HANDLER_NAME
	if (strcmp(name, "") == 0)
		return (-EINVAL);
#endif
	if (ZFS_XA_NS_PREFIX_FORBIDDEN(name))
		return (-EINVAL);
	if (!(ITOZSB(ip)->z_flags & ZSB_XATTR))
		return (-EOPNOTSUPP);
	char *prefixed_name = kmem_asprintf("%s%s", XATTR_USER_PREFIX, name);
	const char *clear_name, *set_name;
	if (zfs_xattr_compat) {
		clear_name = prefixed_name;
		set_name = name;
	} else {
		clear_name = name;
		set_name = prefixed_name;
	}
	error = zpl_xattr_set(ip, clear_name, NULL, 0, flags);
	if (error == -EEXIST)
		goto out;
	if (error == 0)
		flags &= ~XATTR_REPLACE;
	error = zpl_xattr_set(ip, set_name, value, size, flags);
out:
	kmem_strfree(prefixed_name);
	return (error);
}
ZPL_XATTR_SET_WRAPPER(zpl_xattr_user_set);
static xattr_handler_t zpl_xattr_user_handler =
{
	.prefix	= XATTR_USER_PREFIX,
	.list	= zpl_xattr_user_list,
	.get	= zpl_xattr_user_get,
	.set	= zpl_xattr_user_set,
};
static int
__zpl_xattr_trusted_list(struct inode *ip, char *list, size_t list_size,
    const char *name, size_t name_len)
{
	return (capable(CAP_SYS_ADMIN));
}
ZPL_XATTR_LIST_WRAPPER(zpl_xattr_trusted_list);
static int
__zpl_xattr_trusted_get(struct inode *ip, const char *name,
    void *value, size_t size)
{
	char *xattr_name;
	int error;
	if (!capable(CAP_SYS_ADMIN))
		return (-EACCES);
#ifndef HAVE_XATTR_HANDLER_NAME
	if (strcmp(name, "") == 0)
		return (-EINVAL);
#endif
	xattr_name = kmem_asprintf("%s%s", XATTR_TRUSTED_PREFIX, name);
	error = zpl_xattr_get(ip, xattr_name, value, size);
	kmem_strfree(xattr_name);
	return (error);
}
ZPL_XATTR_GET_WRAPPER(zpl_xattr_trusted_get);
static int
__zpl_xattr_trusted_set(zidmap_t *user_ns,
    struct inode *ip, const char *name,
    const void *value, size_t size, int flags)
{
	(void) user_ns;
	char *xattr_name;
	int error;
	if (!capable(CAP_SYS_ADMIN))
		return (-EACCES);
#ifndef HAVE_XATTR_HANDLER_NAME
	if (strcmp(name, "") == 0)
		return (-EINVAL);
#endif
	xattr_name = kmem_asprintf("%s%s", XATTR_TRUSTED_PREFIX, name);
	error = zpl_xattr_set(ip, xattr_name, value, size, flags);
	kmem_strfree(xattr_name);
	return (error);
}
ZPL_XATTR_SET_WRAPPER(zpl_xattr_trusted_set);
static xattr_handler_t zpl_xattr_trusted_handler = {
	.prefix	= XATTR_TRUSTED_PREFIX,
	.list	= zpl_xattr_trusted_list,
	.get	= zpl_xattr_trusted_get,
	.set	= zpl_xattr_trusted_set,
};
static int
__zpl_xattr_security_list(struct inode *ip, char *list, size_t list_size,
    const char *name, size_t name_len)
{
	return (1);
}
ZPL_XATTR_LIST_WRAPPER(zpl_xattr_security_list);
static int
__zpl_xattr_security_get(struct inode *ip, const char *name,
    void *value, size_t size)
{
	char *xattr_name;
	int error;
#ifndef HAVE_XATTR_HANDLER_NAME
	if (strcmp(name, "") == 0)
		return (-EINVAL);
#endif
	xattr_name = kmem_asprintf("%s%s", XATTR_SECURITY_PREFIX, name);
	error = zpl_xattr_get(ip, xattr_name, value, size);
	kmem_strfree(xattr_name);
	return (error);
}
ZPL_XATTR_GET_WRAPPER(zpl_xattr_security_get);
static int
__zpl_xattr_security_set(zidmap_t *user_ns,
    struct inode *ip, const char *name,
    const void *value, size_t size, int flags)
{
	(void) user_ns;
	char *xattr_name;
	int error;
#ifndef HAVE_XATTR_HANDLER_NAME
	if (strcmp(name, "") == 0)
		return (-EINVAL);
#endif
	xattr_name = kmem_asprintf("%s%s", XATTR_SECURITY_PREFIX, name);
	error = zpl_xattr_set(ip, xattr_name, value, size, flags);
	kmem_strfree(xattr_name);
	return (error);
}
ZPL_XATTR_SET_WRAPPER(zpl_xattr_security_set);
static int
zpl_xattr_security_init_impl(struct inode *ip, const struct xattr *xattrs,
    void *fs_info)
{
	const struct xattr *xattr;
	int error = 0;
	for (xattr = xattrs; xattr->name != NULL; xattr++) {
		error = __zpl_xattr_security_set(NULL, ip,
		    xattr->name, xattr->value, xattr->value_len, 0);
		if (error < 0)
			break;
	}
	return (error);
}
int
zpl_xattr_security_init(struct inode *ip, struct inode *dip,
    const struct qstr *qstr)
{
	return security_inode_init_security(ip, dip, qstr,
	    &zpl_xattr_security_init_impl, NULL);
}
static xattr_handler_t zpl_xattr_security_handler = {
	.prefix	= XATTR_SECURITY_PREFIX,
	.list	= zpl_xattr_security_list,
	.get	= zpl_xattr_security_get,
	.set	= zpl_xattr_security_set,
};
#ifdef CONFIG_FS_POSIX_ACL
static int
zpl_set_acl_impl(struct inode *ip, struct posix_acl *acl, int type)
{
	char *name, *value = NULL;
	int error = 0;
	size_t size = 0;
	if (S_ISLNK(ip->i_mode))
		return (-EOPNOTSUPP);
	switch (type) {
	case ACL_TYPE_ACCESS:
		name = XATTR_NAME_POSIX_ACL_ACCESS;
		if (acl) {
			umode_t mode = ip->i_mode;
			error = posix_acl_equiv_mode(acl, &mode);
			if (error < 0) {
				return (error);
			} else {
				if (ip->i_mode != mode) {
					ip->i_mode = ITOZ(ip)->z_mode = mode;
					zpl_inode_set_ctime_to_ts(ip,
					    current_time(ip));
					zfs_mark_inode_dirty(ip);
				}
				if (error == 0)
					acl = NULL;
			}
		}
		break;
	case ACL_TYPE_DEFAULT:
		name = XATTR_NAME_POSIX_ACL_DEFAULT;
		if (!S_ISDIR(ip->i_mode))
			return (acl ? -EACCES : 0);
		break;
	default:
		return (-EINVAL);
	}
	if (acl) {
		size = posix_acl_xattr_size(acl->a_count);
		value = kmem_alloc(size, KM_SLEEP);
		error = zpl_acl_to_xattr(acl, value, size);
		if (error < 0) {
			kmem_free(value, size);
			return (error);
		}
	}
	error = zpl_xattr_set(ip, name, value, size, 0);
	if (value)
		kmem_free(value, size);
	if (!error) {
		if (acl)
			zpl_set_cached_acl(ip, type, acl);
		else
			zpl_forget_cached_acl(ip, type);
	}
	return (error);
}
#ifdef HAVE_SET_ACL
int
#ifdef HAVE_SET_ACL_USERNS
zpl_set_acl(struct user_namespace *userns, struct inode *ip,
    struct posix_acl *acl, int type)
#elif defined(HAVE_SET_ACL_IDMAP_DENTRY)
zpl_set_acl(struct mnt_idmap *userns, struct dentry *dentry,
    struct posix_acl *acl, int type)
#elif defined(HAVE_SET_ACL_USERNS_DENTRY_ARG2)
zpl_set_acl(struct user_namespace *userns, struct dentry *dentry,
    struct posix_acl *acl, int type)
#else
zpl_set_acl(struct inode *ip, struct posix_acl *acl, int type)
#endif  
{
#ifdef HAVE_SET_ACL_USERNS_DENTRY_ARG2
	return (zpl_set_acl_impl(d_inode(dentry), acl, type));
#elif defined(HAVE_SET_ACL_IDMAP_DENTRY)
	return (zpl_set_acl_impl(d_inode(dentry), acl, type));
#else
	return (zpl_set_acl_impl(ip, acl, type));
#endif  
}
#endif  
static struct posix_acl *
zpl_get_acl_impl(struct inode *ip, int type)
{
	struct posix_acl *acl;
	void *value = NULL;
	char *name;
#ifndef HAVE_KERNEL_GET_ACL_HANDLE_CACHE
	acl = get_cached_acl(ip, type);
	if (acl != ACL_NOT_CACHED)
		return (acl);
#endif
	switch (type) {
	case ACL_TYPE_ACCESS:
		name = XATTR_NAME_POSIX_ACL_ACCESS;
		break;
	case ACL_TYPE_DEFAULT:
		name = XATTR_NAME_POSIX_ACL_DEFAULT;
		break;
	default:
		return (ERR_PTR(-EINVAL));
	}
	int size = zpl_xattr_get(ip, name, NULL, 0);
	if (size > 0) {
		value = kmem_alloc(size, KM_SLEEP);
		size = zpl_xattr_get(ip, name, value, size);
	}
	if (size > 0) {
		acl = zpl_acl_from_xattr(value, size);
	} else if (size == -ENODATA || size == -ENOSYS) {
		acl = NULL;
	} else {
		acl = ERR_PTR(-EIO);
	}
	if (size > 0)
		kmem_free(value, size);
#ifndef HAVE_KERNEL_GET_ACL_HANDLE_CACHE
	if (!IS_ERR(acl))
		zpl_set_cached_acl(ip, type, acl);
#endif
	return (acl);
}
#if defined(HAVE_GET_ACL_RCU) || defined(HAVE_GET_INODE_ACL)
struct posix_acl *
zpl_get_acl(struct inode *ip, int type, bool rcu)
{
	if (rcu)
		return (ERR_PTR(-ECHILD));
	return (zpl_get_acl_impl(ip, type));
}
#elif defined(HAVE_GET_ACL)
struct posix_acl *
zpl_get_acl(struct inode *ip, int type)
{
	return (zpl_get_acl_impl(ip, type));
}
#else
#error "Unsupported iops->get_acl() implementation"
#endif  
int
zpl_init_acl(struct inode *ip, struct inode *dir)
{
	struct posix_acl *acl = NULL;
	int error = 0;
	if (ITOZSB(ip)->z_acl_type != ZFS_ACLTYPE_POSIX)
		return (0);
	if (!S_ISLNK(ip->i_mode)) {
		acl = zpl_get_acl_impl(dir, ACL_TYPE_DEFAULT);
		if (IS_ERR(acl))
			return (PTR_ERR(acl));
		if (!acl) {
			ITOZ(ip)->z_mode = (ip->i_mode &= ~current_umask());
			zpl_inode_set_ctime_to_ts(ip, current_time(ip));
			zfs_mark_inode_dirty(ip);
			return (0);
		}
	}
	if (acl) {
		umode_t mode;
		if (S_ISDIR(ip->i_mode)) {
			error = zpl_set_acl_impl(ip, acl, ACL_TYPE_DEFAULT);
			if (error)
				goto out;
		}
		mode = ip->i_mode;
		error = __posix_acl_create(&acl, GFP_KERNEL, &mode);
		if (error >= 0) {
			ip->i_mode = ITOZ(ip)->z_mode = mode;
			zfs_mark_inode_dirty(ip);
			if (error > 0) {
				error = zpl_set_acl_impl(ip, acl,
				    ACL_TYPE_ACCESS);
			}
		}
	}
out:
	zpl_posix_acl_release(acl);
	return (error);
}
int
zpl_chmod_acl(struct inode *ip)
{
	struct posix_acl *acl;
	int error;
	if (ITOZSB(ip)->z_acl_type != ZFS_ACLTYPE_POSIX)
		return (0);
	if (S_ISLNK(ip->i_mode))
		return (-EOPNOTSUPP);
	acl = zpl_get_acl_impl(ip, ACL_TYPE_ACCESS);
	if (IS_ERR(acl) || !acl)
		return (PTR_ERR(acl));
	error = __posix_acl_chmod(&acl, GFP_KERNEL, ip->i_mode);
	if (!error)
		error = zpl_set_acl_impl(ip, acl, ACL_TYPE_ACCESS);
	zpl_posix_acl_release(acl);
	return (error);
}
static int
__zpl_xattr_acl_list_access(struct inode *ip, char *list, size_t list_size,
    const char *name, size_t name_len)
{
	char *xattr_name = XATTR_NAME_POSIX_ACL_ACCESS;
	size_t xattr_size = sizeof (XATTR_NAME_POSIX_ACL_ACCESS);
	if (ITOZSB(ip)->z_acl_type != ZFS_ACLTYPE_POSIX)
		return (0);
	if (list && xattr_size <= list_size)
		memcpy(list, xattr_name, xattr_size);
	return (xattr_size);
}
ZPL_XATTR_LIST_WRAPPER(zpl_xattr_acl_list_access);
static int
__zpl_xattr_acl_list_default(struct inode *ip, char *list, size_t list_size,
    const char *name, size_t name_len)
{
	char *xattr_name = XATTR_NAME_POSIX_ACL_DEFAULT;
	size_t xattr_size = sizeof (XATTR_NAME_POSIX_ACL_DEFAULT);
	if (ITOZSB(ip)->z_acl_type != ZFS_ACLTYPE_POSIX)
		return (0);
	if (list && xattr_size <= list_size)
		memcpy(list, xattr_name, xattr_size);
	return (xattr_size);
}
ZPL_XATTR_LIST_WRAPPER(zpl_xattr_acl_list_default);
static int
__zpl_xattr_acl_get_access(struct inode *ip, const char *name,
    void *buffer, size_t size)
{
	struct posix_acl *acl;
	int type = ACL_TYPE_ACCESS;
	int error;
#ifndef HAVE_XATTR_HANDLER_NAME
	if (strcmp(name, "") != 0)
		return (-EINVAL);
#endif
	if (ITOZSB(ip)->z_acl_type != ZFS_ACLTYPE_POSIX)
		return (-EOPNOTSUPP);
	acl = zpl_get_acl_impl(ip, type);
	if (IS_ERR(acl))
		return (PTR_ERR(acl));
	if (acl == NULL)
		return (-ENODATA);
	error = zpl_acl_to_xattr(acl, buffer, size);
	zpl_posix_acl_release(acl);
	return (error);
}
ZPL_XATTR_GET_WRAPPER(zpl_xattr_acl_get_access);
static int
__zpl_xattr_acl_get_default(struct inode *ip, const char *name,
    void *buffer, size_t size)
{
	struct posix_acl *acl;
	int type = ACL_TYPE_DEFAULT;
	int error;
#ifndef HAVE_XATTR_HANDLER_NAME
	if (strcmp(name, "") != 0)
		return (-EINVAL);
#endif
	if (ITOZSB(ip)->z_acl_type != ZFS_ACLTYPE_POSIX)
		return (-EOPNOTSUPP);
	acl = zpl_get_acl_impl(ip, type);
	if (IS_ERR(acl))
		return (PTR_ERR(acl));
	if (acl == NULL)
		return (-ENODATA);
	error = zpl_acl_to_xattr(acl, buffer, size);
	zpl_posix_acl_release(acl);
	return (error);
}
ZPL_XATTR_GET_WRAPPER(zpl_xattr_acl_get_default);
static int
__zpl_xattr_acl_set_access(zidmap_t *mnt_ns,
    struct inode *ip, const char *name,
    const void *value, size_t size, int flags)
{
	struct posix_acl *acl;
	int type = ACL_TYPE_ACCESS;
	int error = 0;
#ifndef HAVE_XATTR_HANDLER_NAME
	if (strcmp(name, "") != 0)
		return (-EINVAL);
#endif
	if (ITOZSB(ip)->z_acl_type != ZFS_ACLTYPE_POSIX)
		return (-EOPNOTSUPP);
#if defined(HAVE_XATTR_SET_USERNS) || defined(HAVE_XATTR_SET_IDMAP)
	if (!zpl_inode_owner_or_capable(mnt_ns, ip))
		return (-EPERM);
#else
	(void) mnt_ns;
	if (!zpl_inode_owner_or_capable(zfs_init_idmap, ip))
		return (-EPERM);
#endif
	if (value) {
		acl = zpl_acl_from_xattr(value, size);
		if (IS_ERR(acl))
			return (PTR_ERR(acl));
		else if (acl) {
			error = zpl_posix_acl_valid(ip, acl);
			if (error) {
				zpl_posix_acl_release(acl);
				return (error);
			}
		}
	} else {
		acl = NULL;
	}
	error = zpl_set_acl_impl(ip, acl, type);
	zpl_posix_acl_release(acl);
	return (error);
}
ZPL_XATTR_SET_WRAPPER(zpl_xattr_acl_set_access);
static int
__zpl_xattr_acl_set_default(zidmap_t *mnt_ns,
    struct inode *ip, const char *name,
    const void *value, size_t size, int flags)
{
	struct posix_acl *acl;
	int type = ACL_TYPE_DEFAULT;
	int error = 0;
#ifndef HAVE_XATTR_HANDLER_NAME
	if (strcmp(name, "") != 0)
		return (-EINVAL);
#endif
	if (ITOZSB(ip)->z_acl_type != ZFS_ACLTYPE_POSIX)
		return (-EOPNOTSUPP);
#if defined(HAVE_XATTR_SET_USERNS) || defined(HAVE_XATTR_SET_IDMAP)
	if (!zpl_inode_owner_or_capable(mnt_ns, ip))
		return (-EPERM);
#else
	(void) mnt_ns;
	if (!zpl_inode_owner_or_capable(zfs_init_idmap, ip))
		return (-EPERM);
#endif
	if (value) {
		acl = zpl_acl_from_xattr(value, size);
		if (IS_ERR(acl))
			return (PTR_ERR(acl));
		else if (acl) {
			error = zpl_posix_acl_valid(ip, acl);
			if (error) {
				zpl_posix_acl_release(acl);
				return (error);
			}
		}
	} else {
		acl = NULL;
	}
	error = zpl_set_acl_impl(ip, acl, type);
	zpl_posix_acl_release(acl);
	return (error);
}
ZPL_XATTR_SET_WRAPPER(zpl_xattr_acl_set_default);
static xattr_handler_t zpl_xattr_acl_access_handler = {
#ifdef HAVE_XATTR_HANDLER_NAME
	.name	= XATTR_NAME_POSIX_ACL_ACCESS,
#else
	.prefix	= XATTR_NAME_POSIX_ACL_ACCESS,
#endif
	.list	= zpl_xattr_acl_list_access,
	.get	= zpl_xattr_acl_get_access,
	.set	= zpl_xattr_acl_set_access,
#if defined(HAVE_XATTR_LIST_SIMPLE) || \
    defined(HAVE_XATTR_LIST_DENTRY) || \
    defined(HAVE_XATTR_LIST_HANDLER)
	.flags	= ACL_TYPE_ACCESS,
#endif
};
static xattr_handler_t zpl_xattr_acl_default_handler = {
#ifdef HAVE_XATTR_HANDLER_NAME
	.name	= XATTR_NAME_POSIX_ACL_DEFAULT,
#else
	.prefix	= XATTR_NAME_POSIX_ACL_DEFAULT,
#endif
	.list	= zpl_xattr_acl_list_default,
	.get	= zpl_xattr_acl_get_default,
	.set	= zpl_xattr_acl_set_default,
#if defined(HAVE_XATTR_LIST_SIMPLE) || \
    defined(HAVE_XATTR_LIST_DENTRY) || \
    defined(HAVE_XATTR_LIST_HANDLER)
	.flags	= ACL_TYPE_DEFAULT,
#endif
};
#endif  
xattr_handler_t *zpl_xattr_handlers[] = {
	&zpl_xattr_security_handler,
	&zpl_xattr_trusted_handler,
	&zpl_xattr_user_handler,
#ifdef CONFIG_FS_POSIX_ACL
	&zpl_xattr_acl_access_handler,
	&zpl_xattr_acl_default_handler,
#endif  
	NULL
};
static const struct xattr_handler *
zpl_xattr_handler(const char *name)
{
	if (strncmp(name, XATTR_USER_PREFIX,
	    XATTR_USER_PREFIX_LEN) == 0)
		return (&zpl_xattr_user_handler);
	if (strncmp(name, XATTR_TRUSTED_PREFIX,
	    XATTR_TRUSTED_PREFIX_LEN) == 0)
		return (&zpl_xattr_trusted_handler);
	if (strncmp(name, XATTR_SECURITY_PREFIX,
	    XATTR_SECURITY_PREFIX_LEN) == 0)
		return (&zpl_xattr_security_handler);
#ifdef CONFIG_FS_POSIX_ACL
	if (strncmp(name, XATTR_NAME_POSIX_ACL_ACCESS,
	    sizeof (XATTR_NAME_POSIX_ACL_ACCESS)) == 0)
		return (&zpl_xattr_acl_access_handler);
	if (strncmp(name, XATTR_NAME_POSIX_ACL_DEFAULT,
	    sizeof (XATTR_NAME_POSIX_ACL_DEFAULT)) == 0)
		return (&zpl_xattr_acl_default_handler);
#endif  
	return (NULL);
}
static enum xattr_permission
zpl_xattr_permission(xattr_filldir_t *xf, const char *name, int name_len)
{
	const struct xattr_handler *handler;
	struct dentry *d __maybe_unused = xf->dentry;
	enum xattr_permission perm = XAPERM_ALLOW;
	handler = zpl_xattr_handler(name);
	if (handler == NULL) {
		if (ZFS_XA_NS_PREFIX_MATCH(FREEBSD, name))
			return (XAPERM_DENY);
		perm = XAPERM_COMPAT;
		handler = &zpl_xattr_user_handler;
	}
	if (handler->list) {
#if defined(HAVE_XATTR_LIST_SIMPLE)
		if (!handler->list(d))
			return (XAPERM_DENY);
#elif defined(HAVE_XATTR_LIST_DENTRY)
		if (!handler->list(d, NULL, 0, name, name_len, 0))
			return (XAPERM_DENY);
#elif defined(HAVE_XATTR_LIST_HANDLER)
		if (!handler->list(handler, d, NULL, 0, name, name_len))
			return (XAPERM_DENY);
#endif
	}
	return (perm);
}
#if defined(CONFIG_FS_POSIX_ACL) && \
	(!defined(HAVE_POSIX_ACL_RELEASE) || \
		defined(HAVE_POSIX_ACL_RELEASE_GPL_ONLY))
struct acl_rel_struct {
	struct acl_rel_struct *next;
	struct posix_acl *acl;
	clock_t time;
};
#define	ACL_REL_GRACE	(60*HZ)
#define	ACL_REL_WINDOW	(1*HZ)
#define	ACL_REL_SCHED	(ACL_REL_GRACE+ACL_REL_WINDOW)
static struct acl_rel_struct *acl_rel_head = NULL;
static struct acl_rel_struct **acl_rel_tail = &acl_rel_head;
static void
zpl_posix_acl_free(void *arg)
{
	struct acl_rel_struct *freelist = NULL;
	struct acl_rel_struct *a;
	clock_t new_time;
	boolean_t refire = B_FALSE;
	ASSERT3P(acl_rel_head, !=, NULL);
	while (acl_rel_head) {
		a = acl_rel_head;
		if (ddi_get_lbolt() - a->time >= ACL_REL_GRACE) {
			if (acl_rel_tail == &a->next) {
				acl_rel_head = NULL;
				if (cmpxchg(&acl_rel_tail, &a->next,
				    &acl_rel_head) == &a->next) {
					ASSERT3P(a->next, ==, NULL);
					a->next = freelist;
					freelist = a;
					break;
				}
			}
			while (READ_ONCE(a->next) == NULL)
				cpu_relax();
			acl_rel_head = a->next;
			a->next = freelist;
			freelist = a;
		} else {
			new_time = a->time + ACL_REL_SCHED;
			refire = B_TRUE;
			break;
		}
	}
	if (refire)
		taskq_dispatch_delay(system_delay_taskq, zpl_posix_acl_free,
		    NULL, TQ_SLEEP, new_time);
	while (freelist) {
		a = freelist;
		freelist = a->next;
		kfree(a->acl);
		kmem_free(a, sizeof (struct acl_rel_struct));
	}
}
void
zpl_posix_acl_release_impl(struct posix_acl *acl)
{
	struct acl_rel_struct *a, **prev;
	a = kmem_alloc(sizeof (struct acl_rel_struct), KM_SLEEP);
	a->next = NULL;
	a->acl = acl;
	a->time = ddi_get_lbolt();
	prev = xchg(&acl_rel_tail, &a->next);
	ASSERT3P(*prev, ==, NULL);
	*prev = a;
	if (prev == &acl_rel_head)
		taskq_dispatch_delay(system_delay_taskq, zpl_posix_acl_free,
		    NULL, TQ_SLEEP, ddi_get_lbolt() + ACL_REL_SCHED);
}
#endif
ZFS_MODULE_PARAM(zfs, zfs_, xattr_compat, INT, ZMOD_RW,
	"Use legacy ZFS xattr naming for writing new user namespace xattrs");
