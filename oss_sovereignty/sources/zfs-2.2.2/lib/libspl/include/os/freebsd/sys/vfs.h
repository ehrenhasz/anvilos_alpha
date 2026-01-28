

#ifndef ZFS_SYS_VFS_H_
#define	ZFS_SYS_VFS_H_

#include_next <sys/statvfs.h>

int fsshare(const char *, const char *, const char *);
int fsunshare(const char *, const char *);

#endif 
