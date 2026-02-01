 
 

#ifndef _LIBSPL_SYS_STAT_H
#define	_LIBSPL_SYS_STAT_H

#include_next <sys/stat.h>

 

#if defined(__FreeBSD__)
#include <sys/mount.h>  

#define	stat64	stat

#define	MAXOFFSET_T	OFF_MAX

#ifndef _KERNEL
#include <sys/disk.h>

static __inline int
fstat64(int fd, struct stat *sb)
{
	int ret;

	ret = fstat(fd, sb);
	if (ret == 0) {
		if (S_ISCHR(sb->st_mode))
			(void) ioctl(fd, DIOCGMEDIASIZE, &sb->st_size);
	}
	return (ret);
}
#endif

 
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

 
#if defined(__APPLE__) && !(defined(__i386__) || defined(__x86_64__))
#define	stat64	stat
#define	fstat64	fstat
#endif

#endif  
