 

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "proc.h"

int main(void)
{
	char buf1[64], buf2[64];
	pid_t pid;
	ssize_t rv;

	pid = sys_getpid();
	snprintf(buf1, sizeof(buf1), "%u", pid);

	rv = readlink("/proc/self", buf2, sizeof(buf2));
	assert(rv == strlen(buf1));
	buf2[rv] = '\0';
	assert(streq(buf1, buf2));

	return 0;
}
