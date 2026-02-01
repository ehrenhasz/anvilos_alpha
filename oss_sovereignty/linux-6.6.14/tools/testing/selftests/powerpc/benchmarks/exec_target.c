

 

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

void _start(void)
{
	syscall(SYS_exit, 0);
}
