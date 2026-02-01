 

 

 

#ifndef	_COMPAT_POLL_H_
#define	_COMPAT_POLL_H_

#include <sys/types.h>
#ifdef HAVE_POLL_H
# include <poll.h>
#elif HAVE_SYS_POLL_H
# include <sys/poll.h>
#endif

#ifndef HAVE_STRUCT_POLLFD_FD
typedef struct pollfd {
	int	fd;
	short	events;
	short	revents;
} pollfd_t;

#ifndef POLLIN
# define POLLIN		0x0001
#endif
#ifndef POLLPRI
# define POLLPRI	0x0002
#endif
#ifndef POLLOUT
# define POLLOUT	0x0004
#endif
#ifndef POLLERR
# define POLLERR	0x0008
#endif
#ifndef POLLHUP
# define POLLHUP	0x0010
#endif
#ifndef POLLNVAL
# define POLLNVAL	0x0020
#endif

#if 0
 
#define	POLLRDNORM	0x0040
#define POLLNORM	POLLRDNORM
#define POLLWRNORM      POLLOUT
#define	POLLRDBAND	0x0080
#define	POLLWRBAND	0x0100
#endif

#define INFTIM		(-1)	 
#endif  

#ifndef HAVE_NFDS_T
typedef unsigned int	nfds_t;
#endif

#ifndef HAVE_POLL
int   poll(struct pollfd *, nfds_t, int);
#endif

#ifndef HAVE_PPOLL
int   ppoll(struct pollfd *, nfds_t, const struct timespec *, const sigset_t *);
#endif

#endif  
