 
 
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <unistd.h>

int main(void)
{
	int fd;

	if (unshare(CLONE_NEWNS) == -1) {
		if (errno == ENOSYS || errno == EPERM) {
			fprintf(stderr, "error: unshare, errno %d\n", errno);
			return 4;
		}
		fprintf(stderr, "error: unshare, errno %d\n", errno);
		return 1;
	}
	if (mount(NULL, "/", NULL, MS_PRIVATE|MS_REC, NULL) == -1) {
		fprintf(stderr, "error: mount '/', errno %d\n", errno);
		return 1;
	}

	 
	if (mount(NULL, "/tmp", "tmpfs", 0, "nr_inodes=3") == -1) {
		fprintf(stderr, "error: mount tmpfs, errno %d\n", errno);
		return 1;
	}

	fd = openat(AT_FDCWD, "/tmp", O_WRONLY|O_TMPFILE, 0600);
	if (fd == -1) {
		fprintf(stderr, "error: open 1, errno %d\n", errno);
		return 1;
	}
	if (linkat(fd, "", AT_FDCWD, "/tmp/1", AT_EMPTY_PATH) == -1) {
		fprintf(stderr, "error: linkat, errno %d\n", errno);
		return 1;
	}
	close(fd);

	fd = openat(AT_FDCWD, "/tmp", O_WRONLY|O_TMPFILE, 0600);
	if (fd == -1) {
		fprintf(stderr, "error: open 2, errno %d\n", errno);
		return 1;
	}

	return 0;
}
