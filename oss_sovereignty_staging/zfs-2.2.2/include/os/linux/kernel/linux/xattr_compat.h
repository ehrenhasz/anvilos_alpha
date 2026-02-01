 

 

#ifndef _ZFS_XATTR_H
#define	_ZFS_XATTR_H

#include <linux/posix_acl_xattr.h>

 
typedef const struct xattr_handler	xattr_handler_t;

 
#if defined(HAVE_XATTR_LIST_SIMPLE)
#define	ZPL_XATTR_LIST_WRAPPER(fn)					\
static bool								\
fn(struct dentry *dentry)						\
{									\
	return (!!__ ## fn(dentry->d_inode, NULL, 0, NULL, 0));		\
}
 
#elif defined(HAVE_XATTR_LIST_DENTRY)
#define	ZPL_XATTR_LIST_WRAPPER(fn)					\
static size_t								\
fn(struct dentry *dentry, char *list, size_t list_size,			\
    const char *name, size_t name_len, int type)			\
{									\
	return (__ ## fn(dentry->d_inode,				\
	    list, list_size, name, name_len));				\
}
 
#elif defined(HAVE_XATTR_LIST_HANDLER)
#define	ZPL_XATTR_LIST_WRAPPER(fn)					\
static size_t								\
fn(const struct xattr_handler *handler, struct dentry *dentry,		\
    char *list, size_t list_size, const char *name, size_t name_len)	\
{									\
	return (__ ## fn(dentry->d_inode,				\
	    list, list_size, name, name_len));				\
}
#else
#error "Unsupported kernel"
#endif

 
#if defined(HAVE_XATTR_GET_DENTRY_INODE)
#define	ZPL_XATTR_GET_WRAPPER(fn)					\
static int								\
fn(const struct xattr_handler *handler, struct dentry *dentry,		\
    struct inode *inode, const char *name, void *buffer, size_t size)	\
{									\
	return (__ ## fn(inode, name, buffer, size));			\
}
 
#elif defined(HAVE_XATTR_GET_HANDLER)
#define	ZPL_XATTR_GET_WRAPPER(fn)					\
static int								\
fn(const struct xattr_handler *handler, struct dentry *dentry,		\
    const char *name, void *buffer, size_t size)			\
{									\
	return (__ ## fn(dentry->d_inode, name, buffer, size));		\
}
 
#elif defined(HAVE_XATTR_GET_DENTRY)
#define	ZPL_XATTR_GET_WRAPPER(fn)					\
static int								\
fn(struct dentry *dentry, const char *name, void *buffer, size_t size,	\
    int unused_handler_flags)						\
{									\
	return (__ ## fn(dentry->d_inode, name, buffer, size));		\
}
 
#elif defined(HAVE_XATTR_GET_DENTRY_INODE_FLAGS)
#define	ZPL_XATTR_GET_WRAPPER(fn)					\
static int								\
fn(const struct xattr_handler *handler, struct dentry *dentry,		\
    struct inode *inode, const char *name, void *buffer,		\
    size_t size, int flags)						\
{									\
	return (__ ## fn(inode, name, buffer, size));			\
}
#else
#error "Unsupported kernel"
#endif

 
#if defined(HAVE_XATTR_SET_IDMAP)
#define	ZPL_XATTR_SET_WRAPPER(fn)					\
static int								\
fn(const struct xattr_handler *handler, struct mnt_idmap *user_ns,	\
    struct dentry *dentry, struct inode *inode, const char *name,	\
    const void *buffer, size_t size, int flags)	\
{									\
	return (__ ## fn(user_ns, inode, name, buffer, size, flags));	\
}
 
#elif defined(HAVE_XATTR_SET_USERNS)
#define	ZPL_XATTR_SET_WRAPPER(fn)					\
static int								\
fn(const struct xattr_handler *handler, struct user_namespace *user_ns, \
    struct dentry *dentry, struct inode *inode, const char *name,	\
    const void *buffer, size_t size, int flags)	\
{									\
	return (__ ## fn(user_ns, inode, name, buffer, size, flags));	\
}
 
#elif defined(HAVE_XATTR_SET_DENTRY_INODE)
#define	ZPL_XATTR_SET_WRAPPER(fn)					\
static int								\
fn(const struct xattr_handler *handler, struct dentry *dentry,		\
    struct inode *inode, const char *name, const void *buffer,		\
    size_t size, int flags)						\
{									\
	return (__ ## fn(kcred->user_ns, inode, name, buffer, size, flags));\
}
 
#elif defined(HAVE_XATTR_SET_HANDLER)
#define	ZPL_XATTR_SET_WRAPPER(fn)					\
static int								\
fn(const struct xattr_handler *handler, struct dentry *dentry,		\
    const char *name, const void *buffer, size_t size, int flags)	\
{									\
	return (__ ## fn(kcred->user_ns, dentry->d_inode, name,	\
	    buffer, size, flags));					\
}
 
#elif defined(HAVE_XATTR_SET_DENTRY)
#define	ZPL_XATTR_SET_WRAPPER(fn)					\
static int								\
fn(struct dentry *dentry, const char *name, const void *buffer,		\
    size_t size, int flags, int unused_handler_flags)			\
{									\
	return (__ ## fn(kcred->user_ns, dentry->d_inode, name,	\
	    buffer, size, flags));					\
}
#else
#error "Unsupported kernel"
#endif

 
static inline struct posix_acl *
zpl_acl_from_xattr(const void *value, int size)
{
	return (posix_acl_from_xattr(kcred->user_ns, value, size));
}

static inline int
zpl_acl_to_xattr(struct posix_acl *acl, void *value, int size)
{
	return (posix_acl_to_xattr(kcred->user_ns, acl, value, size));
}

#endif  
