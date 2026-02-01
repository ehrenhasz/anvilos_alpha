 
 
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>

static void pass(const char *fmt, unsigned long a, unsigned long b)
{
	char name[64];
	char buf[64];

	snprintf(name, sizeof(name), fmt, a, b);
	if (readlink(name, buf, sizeof(buf)) == -1)
		exit(1);
}

static void fail(const char *fmt, unsigned long a, unsigned long b)
{
	char name[64];
	char buf[64];

	snprintf(name, sizeof(name), fmt, a, b);
	if (readlink(name, buf, sizeof(buf)) == -1 && errno == ENOENT)
		return;
	exit(1);
}

int main(void)
{
	const int PAGE_SIZE = sysconf(_SC_PAGESIZE);
	 
	const unsigned long va_max = 1UL << 20;
	unsigned long va;
	void *p;
	int fd;
	unsigned long a, b;

	fd = open("/dev/zero", O_RDONLY);
	if (fd == -1)
		return 1;

	for (va = 0; va < va_max; va += PAGE_SIZE) {
		p = mmap((void *)va, PAGE_SIZE, PROT_NONE, MAP_PRIVATE|MAP_FILE|MAP_FIXED, fd, 0);
		if (p == (void *)va)
			break;
	}
	if (va == va_max) {
		fprintf(stderr, "error: mmap doesn't like you\n");
		return 1;
	}

	a = (unsigned long)p;
	b = (unsigned long)p + PAGE_SIZE;

	pass("/proc/self/map_files/%lx-%lx", a, b);
	fail("/proc/self/map_files/ %lx-%lx", a, b);
	fail("/proc/self/map_files/%lx -%lx", a, b);
	fail("/proc/self/map_files/%lx- %lx", a, b);
	fail("/proc/self/map_files/%lx-%lx ", a, b);
	fail("/proc/self/map_files/0%lx-%lx", a, b);
	fail("/proc/self/map_files/%lx-0%lx", a, b);
	if (sizeof(long) == 4) {
		fail("/proc/self/map_files/100000000%lx-%lx", a, b);
		fail("/proc/self/map_files/%lx-100000000%lx", a, b);
	} else if (sizeof(long) == 8) {
		fail("/proc/self/map_files/10000000000000000%lx-%lx", a, b);
		fail("/proc/self/map_files/%lx-10000000000000000%lx", a, b);
	} else
		return 1;

	return 0;
}
