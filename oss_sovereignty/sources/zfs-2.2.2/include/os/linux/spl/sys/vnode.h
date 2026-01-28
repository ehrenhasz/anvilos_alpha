

#ifndef _SPL_VNODE_H
#define	_SPL_VNODE_H

#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/buffer_head.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/mount.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/user.h>


#ifndef O_DSYNC
#define	O_DSYNC		O_SYNC
#endif

#define	F_FREESP	11 	


#if defined(SEEK_HOLE) && defined(SEEK_DATA)
#define	F_SEEK_DATA	SEEK_DATA
#define	F_SEEK_HOLE	SEEK_HOLE
#endif


#undef AT_UID
#undef AT_GID

#define	AT_MODE		ATTR_MODE
#define	AT_UID		ATTR_UID
#define	AT_GID		ATTR_GID
#define	AT_SIZE		ATTR_SIZE
#define	AT_ATIME	ATTR_ATIME
#define	AT_MTIME	ATTR_MTIME
#define	AT_CTIME	ATTR_CTIME

#define	ATTR_XVATTR	(1U << 31)
#define	AT_XVATTR	ATTR_XVATTR

#define	ATTR_IATTR_MASK	(ATTR_MODE | ATTR_UID | ATTR_GID | ATTR_SIZE | \
			ATTR_ATIME | ATTR_MTIME | ATTR_CTIME | ATTR_FILE)

#define	CRCREAT		0x01
#define	RMFILE		0x02

#define	B_INVAL		0x01
#define	B_TRUNC		0x02

#define	LOOKUP_DIR		0x01
#define	LOOKUP_XATTR		0x02
#define	CREATE_XATTR_DIR	0x04
#define	ATTR_NOACLCHECK		0x20

typedef struct vattr {
	uint32_t	va_mask;	
	ushort_t	va_mode;	
	uid_t		va_uid;		
	gid_t		va_gid;		
	long		va_fsid;	
	long		va_nodeid;	
	uint32_t	va_nlink;	
	uint64_t	va_size;	
	inode_timespec_t va_atime;	
	inode_timespec_t va_mtime;	
	inode_timespec_t va_ctime;	
	dev_t		va_rdev;	
	uint64_t	va_nblocks;	
	uint32_t	va_blksize;	
	struct dentry	*va_dentry;	
} vattr_t;
#endif 
