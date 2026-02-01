 

#include "includes.h"

#if !defined(HAVE_STATVFS) || !defined(HAVE_FSTATVFS)

#include <sys/types.h>

#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif

#ifndef HAVE_FSBLKCNT_T
typedef unsigned long fsblkcnt_t;
#endif
#ifndef HAVE_FSFILCNT_T
typedef unsigned long fsfilcnt_t;
#endif

#ifndef ST_RDONLY
#define ST_RDONLY	1
#endif
#ifndef ST_NOSUID
#define ST_NOSUID	2
#endif

	 
struct statvfs {
	unsigned long f_bsize;	 
	unsigned long f_frsize;	 
	fsblkcnt_t f_blocks;	 
				 
	fsblkcnt_t    f_bfree;	 
	fsblkcnt_t    f_bavail;	 
				 
	fsfilcnt_t    f_files;	 
	fsfilcnt_t    f_ffree;	 
	fsfilcnt_t    f_favail;	 
				 
	unsigned long f_fsid;	 
	unsigned long f_flag;	 
	unsigned long f_namemax; 
};
#endif

#ifndef HAVE_STATVFS
int statvfs(const char *, struct statvfs *);
#endif

#ifndef HAVE_FSTATVFS
int fstatvfs(int, struct statvfs *);
#endif
