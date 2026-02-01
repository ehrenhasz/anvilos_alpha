 

#include "includes.h"

#if !defined(HAVE_STATVFS) || !defined(HAVE_FSTATVFS)

#ifdef HAVE_SYS_MOUNT_H
# include <sys/mount.h>
#endif

#include <errno.h>

#ifndef MNAMELEN
# define MNAMELEN 32
#endif

#ifdef HAVE_STRUCT_STATFS_F_FILES
# define HAVE_STRUCT_STATFS
#endif

#ifdef HAVE_STRUCT_STATFS
static void
copy_statfs_to_statvfs(struct statvfs *to, struct statfs *from)
{
	to->f_bsize = from->f_bsize;
	to->f_frsize = from->f_bsize;	 
	to->f_blocks = from->f_blocks;
	to->f_bfree = from->f_bfree;
	to->f_bavail = from->f_bavail;
	to->f_files = from->f_files;
	to->f_ffree = from->f_ffree;
	to->f_favail = from->f_ffree;	 
	to->f_fsid = 0;			 
#ifdef HAVE_STRUCT_STATFS_F_FLAGS
	to->f_flag = from->f_flags;
#else
	to->f_flag = 0;
#endif
	to->f_namemax = MNAMELEN;
}
#endif

# ifndef HAVE_STATVFS
int statvfs(const char *path, struct statvfs *buf)
{
#  if defined(HAVE_STATFS) && defined(HAVE_STRUCT_STATFS)
	struct statfs fs;

	memset(&fs, 0, sizeof(fs));
	if (statfs(path, &fs) == -1)
		return -1;
	copy_statfs_to_statvfs(buf, &fs);
	return 0;
#  else
	errno = ENOSYS;
	return -1;
#  endif
}
# endif

# ifndef HAVE_FSTATVFS
int fstatvfs(int fd, struct statvfs *buf)
{
#  if defined(HAVE_FSTATFS) && defined(HAVE_STRUCT_STATFS)
	struct statfs fs;

	memset(&fs, 0, sizeof(fs));
	if (fstatfs(fd, &fs) == -1)
		return -1;
	copy_statfs_to_statvfs(buf, &fs);
	return 0;
#  else
	errno = ENOSYS;
	return -1;
#  endif
}
# endif

#endif
