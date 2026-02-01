 
 

#include_next <sys/mount.h>

#ifndef _LIBSPL_SYS_MOUNT_H
#define	_LIBSPL_SYS_MOUNT_H

#include <assert.h>
#include <string.h>
#include <stdlib.h>

 
#if !defined(BLKGETSIZE64)
#define	BLKGETSIZE64		_IOR(0x12, 114, size_t)
#endif

 
#if !defined(MS_DIRSYNC)
#define	MS_DIRSYNC		S_WRITE
#endif

 
#if !defined(MS_POSIXACL)
#define	MS_POSIXACL		(1<<16)
#endif

#define	MS_USERS	(MS_NOEXEC|MS_NOSUID|MS_NODEV)
#define	MS_OWNER	(MS_NOSUID|MS_NODEV)
#define	MS_GROUP	(MS_NOSUID|MS_NODEV)
#define	MS_COMMENT	0

 
#ifdef MNT_FORCE
#define	MS_FORCE	MNT_FORCE
#else
#define	MS_FORCE	0x00000001
#endif  

#ifdef MNT_DETACH
#define	MS_DETACH	MNT_DETACH
#else
#define	MS_DETACH	0x00000002
#endif  

 
#define	MS_OVERLAY	0x00000004

 
#define	MS_CRYPT	0x00000008

#endif  
