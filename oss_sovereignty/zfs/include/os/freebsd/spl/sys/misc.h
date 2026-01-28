#ifndef _OPENSOLARIS_SYS_MISC_H_
#define	_OPENSOLARIS_SYS_MISC_H_
#include <sys/limits.h>
#include <sys/filio.h>
#define	MAXUID	UID_MAX
#define	_ACL_ACLENT_ENABLED	0x1
#define	_ACL_ACE_ENABLED	0x2
#define	_FIOFFS		(INT_MIN)
#define	_FIOGDIO	(INT_MIN+1)
#define	_FIOSDIO	(INT_MIN+2)
#define	F_SEEK_DATA	FIOSEEKDATA
#define	F_SEEK_HOLE	FIOSEEKHOLE
struct opensolaris_utsname {
	char	*sysname;
	char	*nodename;
	char	*release;
	char	version[32];
	char	*machine;
};
#define	task_io_account_read(n)
#define	task_io_account_write(n)
#endif	 
