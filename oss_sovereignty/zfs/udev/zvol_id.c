#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/fs/zfs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#if defined(ZFS_ASAN_ENABLED)
extern const char *__asan_default_options(void);
const char *__asan_default_options(void) {
	return ("abort_on_error=true:halt_on_error=true:"
		"allocator_may_return_null=true:disable_coredump=false:"
		"detect_stack_use_after_return=true:detect_leaks=false");
}
#endif
int
main(int argc, const char *const *argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s /dev/zdX\n", argv[0]);
		return (1);
	}
	const char *dev_name = argv[1];
	int fd;
	struct stat sb;
	if ((fd = open(dev_name, O_RDONLY|O_CLOEXEC)) == -1 ||
	    fstat(fd, &sb) != 0) {
		fprintf(stderr, "%s: %s\n", dev_name, strerror(errno));
		return (1);
	}
	char zvol_name[MAXNAMELEN + strlen("-part") + 10];
	if (ioctl(fd, BLKZNAME, zvol_name) == -1) {
		fprintf(stderr, "%s: BLKZNAME: %s\n",
		    dev_name, strerror(errno));
		return (1);
	}
	unsigned int dev_part = minor(sb.st_rdev) % ZVOL_MINORS;
	if (dev_part != 0)
		sprintf(zvol_name + strlen(zvol_name), "-part%u", dev_part);
	for (size_t i = 0; i < strlen(zvol_name); ++i)
		if (isblank(zvol_name[i]))
			zvol_name[i] = '+';
	puts(zvol_name);
	return (0);
}
