 
 

#ifndef	_SYS_ZFS_PROJECT_H
#define	_SYS_ZFS_PROJECT_H

#ifndef _KERNEL
#ifndef _SYS_MOUNT_H
 
#define	_SYS_MOUNT_H
#endif
#endif

#include <sys/vfs.h>

#ifdef FS_PROJINHERIT_FL
#define	ZFS_PROJINHERIT_FL	FS_PROJINHERIT_FL
#else
#define	ZFS_PROJINHERIT_FL	0x20000000
#endif

#ifdef FS_IOC_FSGETXATTR
typedef struct fsxattr zfsxattr_t;

#define	ZFS_IOC_FSGETXATTR	FS_IOC_FSGETXATTR
#define	ZFS_IOC_FSSETXATTR	FS_IOC_FSSETXATTR
#else
struct zfsxattr {
	uint32_t	fsx_xflags;	 
	uint32_t	fsx_extsize;	 
	uint32_t	fsx_nextents;	 
	uint32_t	fsx_projid;	 
	uint32_t	fsx_cowextsize;
	unsigned char	fsx_pad[8];
};
typedef struct zfsxattr zfsxattr_t;

#define	ZFS_IOC_FSGETXATTR	_IOR('X', 31, zfsxattr_t)
#define	ZFS_IOC_FSSETXATTR	_IOW('X', 32, zfsxattr_t)
#endif

#define	ZFS_DEFAULT_PROJID	(0ULL)
 
#define	ZFS_INVALID_PROJID	(-1ULL)

static inline boolean_t
zpl_is_valid_projid(uint32_t projid)
{
	 
	if ((uint32_t)ZFS_INVALID_PROJID == projid)
		return (B_FALSE);
	return (B_TRUE);
}

#endif	 
