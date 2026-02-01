 
 

#ifndef _LIBSPL_SYS_STAT_H
#define	_LIBSPL_SYS_STAT_H

#include_next <sys/stat.h>

#include <sys/mount.h>  

 
static inline int
fstat64_blk(int fd, struct stat64 *st)
{
	if (fstat64(fd, st) == -1)
		return (-1);

	 
	if (S_ISBLK(st->st_mode)) {
		if (ioctl(fd, BLKGETSIZE64, &st->st_size) != 0)
			return (-1);
	}

	return (0);
}
#endif  
