 
 
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <unistd.h>

int main(void)
{
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
	 
	if (mount(NULL, "/tmp", "ramfs", 0, NULL) == -1) {
		fprintf(stderr, "error: mount ramfs, errno %d\n", errno);
		return 1;
	}

#define FILENAME "/tmp/1"

	int fd = creat(FILENAME, 0700);
	if (fd == -1) {
		fprintf(stderr, "error: creat, errno %d\n", errno);
		return 1;
	}
#define S "#!" FILENAME "\n"
	if (write(fd, S, strlen(S)) != strlen(S)) {
		fprintf(stderr, "error: write, errno %d\n", errno);
		return 1;
	}
	close(fd);

	int rv = execve(FILENAME, NULL, NULL);
	if (rv == -1 && errno == ELOOP) {
		return 0;
	}
	fprintf(stderr, "error: execve, rv %d, errno %d\n", rv, errno);
	return 1;
}
