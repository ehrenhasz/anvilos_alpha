 



#undef NDEBUG
#include <assert.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "proc-uptime.h"

int main(void)
{
	uint64_t start, u0, u1, c0, c1;
	int fd;

	fd = open("/proc/uptime", O_RDONLY);
	assert(fd >= 0);

	u0 = proc_uptime(fd);
	start = u0;
	c0 = clock_boottime();

	do {
		u1 = proc_uptime(fd);
		c1 = clock_boottime();

		 
		assert(u1 >= u0);

		 
		assert(c1 >= c0);

		 
		assert(c0 >= u0);

		u0 = u1;
		c0 = c1;
	} while (u1 - start < 100);

	return 0;
}
