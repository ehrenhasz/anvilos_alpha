#ifndef _OPENSOLARIS_SYS_MOUNT_H_
#define	_OPENSOLARIS_SYS_MOUNT_H_
#include <sys/param.h>
#include_next <sys/mount.h>
#ifdef BUILDING_ZFS
#include <sys/vfs.h>
#endif
#define	MS_FORCE	MNT_FORCE
#define	MS_REMOUNT	MNT_UPDATE
typedef	struct fid		fid_t;
#endif	 
