 
 

#include "includes.h"

#include <sys/uio.h>

#include <errno.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#else
# ifdef HAVE_SYS_POLL_H
#  include <sys/poll.h>
# endif
#endif
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "atomicio.h"

 
size_t
atomicio6(ssize_t (*f) (int, void *, size_t), int fd, void *_s, size_t n,
    int (*cb)(void *, size_t), void *cb_arg)
{
	char *s = _s;
	size_t pos = 0;
	ssize_t res;
	struct pollfd pfd;

	pfd.fd = fd;
#ifndef BROKEN_READ_COMPARISON
	pfd.events = f == read ? POLLIN : POLLOUT;
#else
	pfd.events = POLLIN|POLLOUT;
#endif
	while (n > pos) {
		res = (f) (fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR) {
				 
				if (cb != NULL && cb(cb_arg, 0) == -1) {
					errno = EINTR;
					return pos;
				}
				continue;
			} else if (errno == EAGAIN || errno == EWOULDBLOCK) {
				(void)poll(&pfd, 1, -1);
				continue;
			}
			return 0;
		case 0:
			errno = EPIPE;
			return pos;
		default:
			pos += (size_t)res;
			if (cb != NULL && cb(cb_arg, (size_t)res) == -1) {
				errno = EINTR;
				return pos;
			}
		}
	}
	return pos;
}

size_t
atomicio(ssize_t (*f) (int, void *, size_t), int fd, void *_s, size_t n)
{
	return atomicio6(f, fd, _s, n, NULL, NULL);
}

 
size_t
atomiciov6(ssize_t (*f) (int, const struct iovec *, int), int fd,
    const struct iovec *_iov, int iovcnt,
    int (*cb)(void *, size_t), void *cb_arg)
{
	size_t pos = 0, rem;
	ssize_t res;
	struct iovec iov_array[IOV_MAX], *iov = iov_array;
	struct pollfd pfd;

	if (iovcnt < 0 || iovcnt > IOV_MAX) {
		errno = EINVAL;
		return 0;
	}
	 
	memcpy(iov, _iov, (size_t)iovcnt * sizeof(*_iov));

	pfd.fd = fd;
#ifndef BROKEN_READV_COMPARISON
	pfd.events = f == readv ? POLLIN : POLLOUT;
#else
	pfd.events = POLLIN|POLLOUT;
#endif
	for (; iovcnt > 0 && iov[0].iov_len > 0;) {
		res = (f) (fd, iov, iovcnt);
		switch (res) {
		case -1:
			if (errno == EINTR) {
				 
				if (cb != NULL && cb(cb_arg, 0) == -1) {
					errno = EINTR;
					return pos;
				}
				continue;
			} else if (errno == EAGAIN || errno == EWOULDBLOCK) {
				(void)poll(&pfd, 1, -1);
				continue;
			}
			return 0;
		case 0:
			errno = EPIPE;
			return pos;
		default:
			rem = (size_t)res;
			pos += rem;
			 
			while (iovcnt > 0 && rem >= iov[0].iov_len) {
				rem -= iov[0].iov_len;
				iov++;
				iovcnt--;
			}
			 
			if (rem > 0 && (iovcnt <= 0 || rem > iov[0].iov_len)) {
				errno = EFAULT;
				return 0;
			}
			if (iovcnt == 0)
				break;
			 
			iov[0].iov_base = ((char *)iov[0].iov_base) + rem;
			iov[0].iov_len -= rem;
		}
		if (cb != NULL && cb(cb_arg, (size_t)res) == -1) {
			errno = EINTR;
			return pos;
		}
	}
	return pos;
}

size_t
atomiciov(ssize_t (*f) (int, const struct iovec *, int), int fd,
    const struct iovec *_iov, int iovcnt)
{
	return atomiciov6(f, fd, _iov, iovcnt, NULL, NULL);
}
