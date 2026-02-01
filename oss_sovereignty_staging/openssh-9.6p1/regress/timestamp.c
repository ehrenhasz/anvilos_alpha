 

 

 

#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int
main(void)
{
	struct timeval tv;
	struct tm *tm;
	char buf[1024];

	if (gettimeofday(&tv, NULL) != 0)
		exit(1);
	if ((tm = localtime(&tv.tv_sec)) == NULL)
		exit(2);
	if (strftime(buf, sizeof buf, "%Y%m%dT%H%M%S", tm) <= 0)
		exit(3);
	printf("%s.%06d\n", buf, (int)tv.tv_usec);
	exit(0);
}
