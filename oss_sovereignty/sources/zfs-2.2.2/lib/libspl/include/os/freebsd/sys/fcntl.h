

#ifndef _LIBSPL_SYS_FCNTL_H_
#define	_LIBSPL_SYS_FCNTL_H_

#include_next <sys/fcntl.h>

#define	O_LARGEFILE	0
#define	O_RSYNC		0

#ifndef O_DSYNC
#define	O_DSYNC		0
#endif

#endif	
