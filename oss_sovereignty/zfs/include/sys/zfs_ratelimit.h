#ifndef _SYS_ZFS_RATELIMIT_H
#define	_SYS_ZFS_RATELIMIT_H
#include <sys/zfs_context.h>
typedef struct {
	hrtime_t start;
	unsigned int count;
	unsigned int *burst;
	unsigned int interval;	 
	kmutex_t lock;
} zfs_ratelimit_t;
int zfs_ratelimit(zfs_ratelimit_t *rl);
void zfs_ratelimit_init(zfs_ratelimit_t *rl, unsigned int *burst,
    unsigned int interval);
void zfs_ratelimit_fini(zfs_ratelimit_t *rl);
#endif	 
