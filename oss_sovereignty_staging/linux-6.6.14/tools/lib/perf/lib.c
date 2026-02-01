
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <linux/kernel.h>
#include <internal/lib.h>

unsigned int page_size;

static ssize_t ion(bool is_read, int fd, void *buf, size_t n)
{
	void *buf_start = buf;
	size_t left = n;

	while (left) {
		 
		ssize_t ret = is_read ? read(fd, buf, left) :
					write(fd, buf, left);

		if (ret < 0 && errno == EINTR)
			continue;
		if (ret <= 0)
			return ret;

		left -= ret;
		buf  += ret;
	}

	BUG_ON((size_t)(buf - buf_start) != n);
	return n;
}

 
ssize_t readn(int fd, void *buf, size_t n)
{
	return ion(true, fd, buf, n);
}

ssize_t preadn(int fd, void *buf, size_t n, off_t offs)
{
	size_t left = n;

	while (left) {
		ssize_t ret = pread(fd, buf, left, offs);

		if (ret < 0 && errno == EINTR)
			continue;
		if (ret <= 0)
			return ret;

		left -= ret;
		buf  += ret;
		offs += ret;
	}

	return n;
}

 
ssize_t writen(int fd, const void *buf, size_t n)
{
	 
	return ion(false, fd, (void *)buf, n);
}
