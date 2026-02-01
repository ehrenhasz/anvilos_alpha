 

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "zed_file.h"
#include "zed_log.h"

 
int
zed_file_lock(int fd)
{
	struct flock lock;

	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;

	if (fcntl(fd, F_SETLK, &lock) < 0) {
		if ((errno == EACCES) || (errno == EAGAIN))
			return (1);

		return (-1);
	}
	return (0);
}

 
int
zed_file_unlock(int fd)
{
	struct flock lock;

	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}
	lock.l_type = F_UNLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;

	if (fcntl(fd, F_SETLK, &lock) < 0)
		return (-1);

	return (0);
}

 
pid_t
zed_file_is_locked(int fd)
{
	struct flock lock;

	if (fd < 0) {
		errno = EBADF;
		return (-1);
	}
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;

	if (fcntl(fd, F_GETLK, &lock) < 0)
		return (-1);

	if (lock.l_type == F_UNLCK)
		return (0);

	return (lock.l_pid);
}


#if __APPLE__
#define	PROC_SELF_FD "/dev/fd"
#else  
#define	PROC_SELF_FD "/proc/self/fd"
#endif

 
void
zed_file_close_from(int lowfd)
{
	int errno_bak = errno;
	int maxfd = 0;
	int fd;
	DIR *fddir;
	struct dirent *fdent;

	if ((fddir = opendir(PROC_SELF_FD)) != NULL) {
		while ((fdent = readdir(fddir)) != NULL) {
			fd = atoi(fdent->d_name);
			if (fd > maxfd && fd != dirfd(fddir))
				maxfd = fd;
		}
		(void) closedir(fddir);
	} else {
		maxfd = sysconf(_SC_OPEN_MAX);
	}
	for (fd = lowfd; fd < maxfd; fd++)
		(void) close(fd);

	errno = errno_bak;
}
