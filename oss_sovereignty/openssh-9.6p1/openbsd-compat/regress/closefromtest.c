 

#include "includes.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_OPENS 10

void
fail(char *msg)
{
	fprintf(stderr, "closefrom: %s\n", msg);
	exit(1);
}

int
main(void)
{
	int i, max, fds[NUM_OPENS];
	char buf[512];

	for (i = 0; i < NUM_OPENS; i++)
		if ((fds[i] = open("/dev/null", O_RDONLY)) == -1)
			exit(0);	 
	max = i - 1;

	 
	closefrom(fds[max]);
	if (close(fds[max]) != -1)
		fail("failed to close highest fd");

	 
	for (i = 0; i < max; i++)
		if (read(fds[i], buf, sizeof(buf)) == -1)
			fail("closed descriptors it should not have");

	 
	closefrom(fds[0]);
	for (i = 0; i < NUM_OPENS; i++)
		if (close(fds[i]) != -1)
			fail("failed to close from lowest fd");
	return 0;
}
