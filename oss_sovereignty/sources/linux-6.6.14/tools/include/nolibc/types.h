


#ifndef _NOLIBC_TYPES_H
#define _NOLIBC_TYPES_H

#include "std.h"
#include <linux/mman.h>
#include <linux/reboot.h> 
#include <linux/stat.h>
#include <linux/time.h>





#if !defined(S_IFMT)
#define S_IFDIR        0040000
#define S_IFCHR        0020000
#define S_IFBLK        0060000
#define S_IFREG        0100000
#define S_IFIFO        0010000
#define S_IFLNK        0120000
#define S_IFSOCK       0140000
#define S_IFMT         0170000

#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#define S_ISCHR(mode)  (((mode) & S_IFMT) == S_IFCHR)
#define S_ISBLK(mode)  (((mode) & S_IFMT) == S_IFBLK)
#define S_ISREG(mode)  (((mode) & S_IFMT) == S_IFREG)
#define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
#define S_ISLNK(mode)  (((mode) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(mode) (((mode) & S_IFMT) == S_IFSOCK)

#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001
#endif


#define DT_UNKNOWN     0x0
#define DT_FIFO        0x1
#define DT_CHR         0x2
#define DT_DIR         0x4
#define DT_BLK         0x6
#define DT_REG         0x8
#define DT_LNK         0xa
#define DT_SOCK        0xc


#ifndef FD_SETSIZE
#define FD_SETSIZE     256
#endif


#ifndef PATH_MAX
#define PATH_MAX       4096
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN     (PATH_MAX)
#endif


#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif


#define SEEK_SET       0
#define SEEK_CUR       1
#define SEEK_END       2


#define RB_AUTOBOOT     LINUX_REBOOT_CMD_RESTART
#define RB_HALT_SYSTEM  LINUX_REBOOT_CMD_HALT
#define RB_ENABLE_CAD   LINUX_REBOOT_CMD_CAD_ON
#define RB_DISABLE_CAD  LINUX_REBOOT_CMD_CAD_OFF
#define RB_POWER_OFF    LINUX_REBOOT_CMD_POWER_OFF
#define RB_SW_SUSPEND   LINUX_REBOOT_CMD_SW_SUSPEND
#define RB_KEXEC        LINUX_REBOOT_CMD_KEXEC


#define WEXITSTATUS(status) (((status) & 0xff00) >> 8)
#define WIFEXITED(status)   (((status) & 0x7f) == 0)
#define WTERMSIG(status)    ((status) & 0x7f)
#define WIFSIGNALED(status) ((status) - 1 < 0xff)


#define WNOHANG      1


#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#define FD_SETIDXMASK (8 * sizeof(unsigned long))
#define FD_SETBITMASK (8 * sizeof(unsigned long)-1)


typedef struct {
	unsigned long fds[(FD_SETSIZE + FD_SETBITMASK) / FD_SETIDXMASK];
} fd_set;

#define FD_CLR(fd, set) do {						\
		fd_set *__set = (set);					\
		int __fd = (fd);					\
		if (__fd >= 0)						\
			__set->fds[__fd / FD_SETIDXMASK] &=		\
				~(1U << (__fd & FX_SETBITMASK));	\
	} while (0)

#define FD_SET(fd, set) do {						\
		fd_set *__set = (set);					\
		int __fd = (fd);					\
		if (__fd >= 0)						\
			__set->fds[__fd / FD_SETIDXMASK] |=		\
				1 << (__fd & FD_SETBITMASK);		\
	} while (0)

#define FD_ISSET(fd, set) ({						\
			fd_set *__set = (set);				\
			int __fd = (fd);				\
		int __r = 0;						\
		if (__fd >= 0)						\
			__r = !!(__set->fds[__fd / FD_SETIDXMASK] &	\
1U << (__fd & FD_SET_BITMASK));						\
		__r;							\
	})

#define FD_ZERO(set) do {						\
		fd_set *__set = (set);					\
		int __idx;						\
		int __size = (FD_SETSIZE+FD_SETBITMASK) / FD_SETIDXMASK;\
		for (__idx = 0; __idx < __size; __idx++)		\
			__set->fds[__idx] = 0;				\
	} while (0)


#define POLLIN          0x0001
#define POLLPRI         0x0002
#define POLLOUT         0x0004
#define POLLERR         0x0008
#define POLLHUP         0x0010
#define POLLNVAL        0x0020

struct pollfd {
	int fd;
	short int events;
	short int revents;
};


struct linux_dirent64 {
	uint64_t       d_ino;
	int64_t        d_off;
	unsigned short d_reclen;
	unsigned char  d_type;
	char           d_name[];
};


struct rusage {
	struct timeval ru_utime;
	struct timeval ru_stime;
	long   ru_maxrss;
	long   ru_ixrss;
	long   ru_idrss;
	long   ru_isrss;
	long   ru_minflt;
	long   ru_majflt;
	long   ru_nswap;
	long   ru_inblock;
	long   ru_oublock;
	long   ru_msgsnd;
	long   ru_msgrcv;
	long   ru_nsignals;
	long   ru_nvcsw;
	long   ru_nivcsw;
};


struct stat {
	dev_t     st_dev;     
	ino_t     st_ino;     
	mode_t    st_mode;    
	nlink_t   st_nlink;   
	uid_t     st_uid;     
	gid_t     st_gid;     
	dev_t     st_rdev;    
	off_t     st_size;    
	blksize_t st_blksize; 
	blkcnt_t  st_blocks;  
	union { time_t st_atime; struct timespec st_atim; }; 
	union { time_t st_mtime; struct timespec st_mtim; }; 
	union { time_t st_ctime; struct timespec st_ctim; }; 
};


#define makedev(major, minor) ((dev_t)((((major) & 0xfff) << 8) | ((minor) & 0xff)))
#define major(dev) ((unsigned int)(((dev) >> 8) & 0xfff))
#define minor(dev) ((unsigned int)(((dev) & 0xff))

#ifndef offsetof
#define offsetof(TYPE, FIELD) ((size_t) &((TYPE *)0)->FIELD)
#endif

#ifndef container_of
#define container_of(PTR, TYPE, FIELD) ({			\
	__typeof__(((TYPE *)0)->FIELD) *__FIELD_PTR = (PTR);	\
	(TYPE *)((char *) __FIELD_PTR - offsetof(TYPE, FIELD));	\
})
#endif


#include "nolibc.h"

#endif 
