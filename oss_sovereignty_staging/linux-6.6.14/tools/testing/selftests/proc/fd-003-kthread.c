 


#undef NDEBUG
#include <sys/syscall.h>
#include <assert.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "proc.h"

#define PF_KHTREAD 0x00200000

 
static int kernel_thread_fd(unsigned int pid)
{
	unsigned int flags = 0;
	char buf[4096];
	int dir_fd, fd;
	ssize_t rv;

	snprintf(buf, sizeof(buf), "/proc/%u", pid);
	dir_fd = open(buf, O_RDONLY|O_DIRECTORY);
	if (dir_fd == -1)
		return -1;

	 
	fd = openat(dir_fd, "stat", O_RDONLY);
	if (fd == -1) {
		close(dir_fd);
		return -1;
	}
	rv = read(fd, buf, sizeof(buf));
	close(fd);
	if (0 < rv && rv <= sizeof(buf)) {
		unsigned long long flags_ull;
		char *p, *end;
		int i;

		assert(buf[rv - 1] == '\n');
		buf[rv - 1] = '\0';

		 
		for (i = 0; i < 43; i++) {
			p = strrchr(buf, ' ');
			assert(p);
			*p = '\0';
		}

		p = strrchr(buf, ' ');
		assert(p);

		flags_ull = xstrtoull(p + 1, &end);
		assert(*end == '\0');
		assert(flags_ull == (unsigned int)flags_ull);

		flags = flags_ull;
	}

	fd = -1;
	if (flags & PF_KHTREAD) {
		fd = openat(dir_fd, "fd", O_RDONLY|O_DIRECTORY);
	}
	close(dir_fd);
	return fd;
}

static void test_readdir(int fd)
{
	DIR *d;
	struct dirent *de;

	d = fdopendir(fd);
	assert(d);

	de = xreaddir(d);
	assert(streq(de->d_name, "."));
	assert(de->d_type == DT_DIR);

	de = xreaddir(d);
	assert(streq(de->d_name, ".."));
	assert(de->d_type == DT_DIR);

	de = xreaddir(d);
	assert(!de);
}

static inline int sys_statx(int dirfd, const char *pathname, int flags,
			    unsigned int mask, void *stx)
{
	return syscall(SYS_statx, dirfd, pathname, flags, mask, stx);
}

static void test_lookup_fail(int fd, const char *pathname)
{
	char stx[256] __attribute__((aligned(8)));
	int rv;

	rv = sys_statx(fd, pathname, AT_SYMLINK_NOFOLLOW, 0, (void *)stx);
	assert(rv == -1 && errno == ENOENT);
}

static void test_lookup(int fd)
{
	char buf[64];
	unsigned int u;
	int i;

	for (i = INT_MIN; i < INT_MIN + 1024; i++) {
		snprintf(buf, sizeof(buf), "%d", i);
		test_lookup_fail(fd, buf);
	}
	for (i = -1024; i < 1024; i++) {
		snprintf(buf, sizeof(buf), "%d", i);
		test_lookup_fail(fd, buf);
	}
	for (u = INT_MAX - 1024; u < (unsigned int)INT_MAX + 1024; u++) {
		snprintf(buf, sizeof(buf), "%u", u);
		test_lookup_fail(fd, buf);
	}
	for (u = UINT_MAX - 1024; u != 0; u++) {
		snprintf(buf, sizeof(buf), "%u", u);
		test_lookup_fail(fd, buf);
	}
}

int main(void)
{
	unsigned int pid;
	int fd;

	 
	pid = 2;
	while ((fd = kernel_thread_fd(pid)) == -1 && pid < 1024) {
		pid++;
	}
	 
	if (pid >= 1024)
		return 1;

	test_readdir(fd);
	test_lookup(fd);

	return 0;
}
