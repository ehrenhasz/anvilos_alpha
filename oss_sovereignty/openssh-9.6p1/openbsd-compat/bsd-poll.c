 

#include "includes.h"
#if !defined(HAVE_PPOLL) || !defined(HAVE_POLL) || defined(BROKEN_POLL)

#include <sys/types.h>
#include <sys/time.h>
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "bsd-poll.h"

#if !defined(HAVE_PPOLL) || defined(BROKEN_POLL)
 

int
ppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *tmoutp,
    const sigset_t *sigmask)
{
	nfds_t i;
	int ret, fd, maxfd = 0;
	fd_set readfds, writefds, exceptfds;

	for (i = 0; i < nfds; i++) {
		fd = fds[i].fd;
		if (fd != -1 && fd >= FD_SETSIZE) {
			errno = EINVAL;
			return -1;
		}
		maxfd = MAX(maxfd, fd);
	}

	 
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);
	for (i = 0; i < nfds; i++) {
		fd = fds[i].fd;
		if (fd == -1)
			continue;
		if (fds[i].events & POLLIN)
			FD_SET(fd, &readfds);
		if (fds[i].events & POLLOUT)
			FD_SET(fd, &writefds);
		if (fds[i].events & POLLPRI)
			FD_SET(fd, &exceptfds);
	}

	ret = pselect(maxfd + 1, &readfds, &writefds, &exceptfds, tmoutp, sigmask);

	 
	for (i = 0; i < nfds; i++) {
		fd = fds[i].fd;
		fds[i].revents = 0;
		if (fd == -1)
			continue;
		if ((fds[i].events & POLLIN) && FD_ISSET(fd, &readfds))
			fds[i].revents |= POLLIN;
		if ((fds[i].events & POLLOUT) && FD_ISSET(fd, &writefds))
			fds[i].revents |= POLLOUT;
		if ((fds[i].events & POLLPRI) && FD_ISSET(fd, &exceptfds))
			fds[i].revents |= POLLPRI;
	}

	return ret;
}
#endif  

#if !defined(HAVE_POLL) || defined(BROKEN_POLL)
int
poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	struct timespec ts, *tsp = NULL;

	 
	if (timeout >= 0) {
		ts.tv_sec = timeout / 1000;
		ts.tv_nsec = (timeout % 1000) * 1000000;
		tsp = &ts;
	}

	return ppoll(fds, nfds, tsp, NULL);
}
#endif  

#endif  
