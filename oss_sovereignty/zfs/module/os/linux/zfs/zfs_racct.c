#include <sys/zfs_racct.h>
void
zfs_racct_read(uint64_t size, uint64_t iops)
{
	(void) size, (void) iops;
}
void
zfs_racct_write(uint64_t size, uint64_t iops)
{
	(void) size, (void) iops;
}
