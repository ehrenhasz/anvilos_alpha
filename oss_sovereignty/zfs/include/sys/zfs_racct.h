#ifndef _SYS_ZFS_RACCT_H
#define	_SYS_ZFS_RACCT_H
#include <sys/zfs_context.h>
void zfs_racct_read(uint64_t size, uint64_t iops);
void zfs_racct_write(uint64_t size, uint64_t iops);
#endif  
