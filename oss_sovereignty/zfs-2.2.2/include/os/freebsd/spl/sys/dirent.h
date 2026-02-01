 

#ifndef _OPENSOLARIS_SYS_DIRENT_H_
#define	_OPENSOLARIS_SYS_DIRENT_H_

#include <sys/types.h>

#include_next <sys/dirent.h>

typedef	struct dirent	dirent64_t;
typedef ino_t		ino64_t;

#define	dirent64	dirent

#define	d_ino	d_fileno

#define	DIRENT64_RECLEN(len)	_GENERIC_DIRLEN(len)

#endif	 
