
#undef NDEBUG
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "proc.h"

static uint64_t clock_boottime(void)
{
	struct timespec ts;
	int err;

	err = clock_gettime(CLOCK_BOOTTIME, &ts);
	assert(err >= 0);

	return (ts.tv_sec * 100) + (ts.tv_nsec / 10000000);
}

static uint64_t proc_uptime(int fd)
{
	uint64_t val1, val2;
	char buf[64], *p;
	ssize_t rv;

	
	memset(buf, 0, sizeof(buf));
	rv = pread(fd, buf, sizeof(buf), 0);
	assert(0 <= rv && rv <= sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';

	p = buf;

	val1 = xstrtoull(p, &p);
	assert(p[0] == '.');
	assert('0' <= p[1] && p[1] <= '9');
	assert('0' <= p[2] && p[2] <= '9');
	assert(p[3] == ' ');

	val2 = (p[1] - '0') * 10 + p[2] - '0';

	return val1 * 100 + val2;
}
