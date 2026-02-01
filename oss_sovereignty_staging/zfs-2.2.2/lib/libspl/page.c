 

#include <unistd.h>
#include <sys/param.h>

static size_t pagesize = 0;

size_t
spl_pagesize(void)
{
	if (pagesize == 0)
		pagesize = sysconf(_SC_PAGESIZE);

	return (pagesize);
}
