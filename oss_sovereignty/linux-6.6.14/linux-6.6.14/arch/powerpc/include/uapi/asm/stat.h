#ifndef _ASM_POWERPC_STAT_H
#define _ASM_POWERPC_STAT_H
#include <linux/types.h>
#define STAT_HAVE_NSEC 1
#ifndef __powerpc64__
struct __old_kernel_stat {
	unsigned short st_dev;
	unsigned short st_ino;
	unsigned short st_mode;
	unsigned short st_nlink;
	unsigned short st_uid;
	unsigned short st_gid;
	unsigned short st_rdev;
	unsigned long  st_size;
	unsigned long  st_atime;
	unsigned long  st_mtime;
	unsigned long  st_ctime;
};
#endif  
struct stat {
	unsigned long	st_dev;
	__kernel_ino_t	st_ino;
#ifdef __powerpc64__
	unsigned long	st_nlink;
	__kernel_mode_t	st_mode;
#else
	__kernel_mode_t	st_mode;
	unsigned short	st_nlink;
#endif
	__kernel_uid32_t st_uid;
	__kernel_gid32_t st_gid;
	unsigned long	st_rdev;
	long		st_size;
	unsigned long	st_blksize;
	unsigned long	st_blocks;
	unsigned long	st_atime;
	unsigned long	st_atime_nsec;
	unsigned long	st_mtime;
	unsigned long	st_mtime_nsec;
	unsigned long	st_ctime;
	unsigned long	st_ctime_nsec;
	unsigned long	__unused4;
	unsigned long	__unused5;
#ifdef __powerpc64__
	unsigned long	__unused6;
#endif
};
struct stat64 {
	unsigned long long st_dev;		 
	unsigned long long st_ino;		 
	unsigned int	st_mode;	 
	unsigned int	st_nlink;	 
	unsigned int	st_uid;		 
	unsigned int	st_gid;		 
	unsigned long long st_rdev;	 
	unsigned short	__pad2;
	long long	st_size;	 
	int		st_blksize;	 
	long long	st_blocks;	 
	int		st_atime;	 
	unsigned int	st_atime_nsec;
	int		st_mtime;	 
	unsigned int	st_mtime_nsec;
	int		st_ctime;	 
	unsigned int	st_ctime_nsec;
	unsigned int	__unused4;
	unsigned int	__unused5;
};
#endif  
