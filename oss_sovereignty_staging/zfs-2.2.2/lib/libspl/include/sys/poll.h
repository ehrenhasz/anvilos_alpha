 
 

 
#ifndef _LIBSPL_SYS_POLL_H
#define	_LIBSPL_SYS_POLL_H
#if defined(__GLIBC__) || defined(__KLIBC__) || defined(__UCLIBC__)
#include_next <sys/poll.h>
#else
#include <poll.h>
#endif
#endif  
