 




#undef NDEBUG
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "proc-uptime.h"

static inline int sys_sched_getaffinity(pid_t pid, unsigned int len, unsigned long *m)
{
	return syscall(SYS_sched_getaffinity, pid, len, m);
}

static inline int sys_sched_setaffinity(pid_t pid, unsigned int len, unsigned long *m)
{
	return syscall(SYS_sched_setaffinity, pid, len, m);
}

int main(void)
{
	uint64_t u0, u1, c0, c1;
	unsigned int len;
	unsigned long *m;
	unsigned int cpu;
	int fd;

	 
	m = NULL;
	len = 0;
	do {
		len += sizeof(unsigned long);
		free(m);
		m = malloc(len);
	} while (sys_sched_getaffinity(0, len, m) == -1 && errno == EINVAL);

	fd = open("/proc/uptime", O_RDONLY);
	assert(fd >= 0);

	u0 = proc_uptime(fd);
	c0 = clock_boottime();

	for (cpu = 0; cpu < len * 8; cpu++) {
		memset(m, 0, len);
		m[cpu / (8 * sizeof(unsigned long))] |= 1UL << (cpu % (8 * sizeof(unsigned long)));

		 
		sys_sched_setaffinity(0, len, m);

		u1 = proc_uptime(fd);
		c1 = clock_boottime();

		 
		assert(u1 >= u0);

		 
		assert(c1 >= c0);

		 
		assert(c0 >= u0);

		u0 = u1;
		c0 = c1;
	}

	return 0;
}
