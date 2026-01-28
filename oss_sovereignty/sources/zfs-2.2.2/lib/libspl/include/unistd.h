


#include_next <unistd.h>

#ifndef _LIBSPL_UNISTD_H
#define	_LIBSPL_UNISTD_H

#include <sys/ioctl.h>

#if !defined(HAVE_ISSETUGID)
#include <sys/types.h>
#define	issetugid() (geteuid() == 0 || getegid() == 0)
#endif

#endif 
