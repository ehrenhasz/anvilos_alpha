



#ifndef _SYS_ZFS_VFSOPS_H
#define	_SYS_ZFS_VFSOPS_H

#ifdef _KERNEL
#include <sys/zfs_vfsops_os.h>
#endif

extern void zfsvfs_update_fromname(const char *, const char *);

#endif 
