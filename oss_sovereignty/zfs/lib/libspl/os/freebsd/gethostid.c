#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/systeminfo.h>
unsigned long
get_system_hostid(void)
{
	return (gethostid());
}
