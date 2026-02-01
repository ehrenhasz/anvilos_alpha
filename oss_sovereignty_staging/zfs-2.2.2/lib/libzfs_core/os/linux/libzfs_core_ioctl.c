 
#include <sys/types.h>
#include <sys/param.h>
#include <sys/zfs_ioctl.h>
#include <libzfs_core.h>

int
lzc_ioctl_fd(int fd, unsigned long request, zfs_cmd_t *zc)
{
	return (ioctl(fd, request, zc));
}
