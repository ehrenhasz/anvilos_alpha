 
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/mount.h>
#include <linux/unistd.h>

static inline int fsopen(const char *fsname, unsigned int flags)
{
	return syscall(__NR_fsopen, fsname, flags);
}

static inline int fsconfig(int fd, unsigned int cmd, const char *key, const void *val, int aux)
{
	return syscall(__NR_fsconfig, fd, cmd, key, val, aux);
}

int main(void)
{
	int fsfd, ret;
	int hidepid = 2;

	assert((fsfd = fsopen("proc", 0)) != -1);

	ret = fsconfig(fsfd, FSCONFIG_SET_BINARY, "hidepid", &hidepid, 0);
	assert(ret == -1);
	assert(errno == EINVAL);

	assert(!fsconfig(fsfd, FSCONFIG_SET_STRING, "hidepid", "2", 0));
	assert(!fsconfig(fsfd, FSCONFIG_SET_STRING, "hidepid", "invisible", 0));

	assert(!close(fsfd));

	return 0;
}
