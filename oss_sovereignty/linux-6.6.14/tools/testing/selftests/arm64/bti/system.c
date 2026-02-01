
 

#include "system.h"

#include <asm/unistd.h>

void __noreturn exit(int n)
{
	syscall(__NR_exit, n);
	unreachable();
}

ssize_t write(int fd, const void *buf, size_t size)
{
	return syscall(__NR_write, fd, buf, size);
}
