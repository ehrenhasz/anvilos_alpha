 

#undef NDEBUG
#include <assert.h>
#include <sched.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include "proc.h"

int f(void *arg)
{
	char buf1[64], buf2[64];
	pid_t pid, tid;
	ssize_t rv;

	pid = sys_getpid();
	tid = sys_gettid();
	snprintf(buf1, sizeof(buf1), "%u/task/%u", pid, tid);

	rv = readlink("/proc/thread-self", buf2, sizeof(buf2));
	assert(rv == strlen(buf1));
	buf2[rv] = '\0';
	assert(streq(buf1, buf2));

	if (arg)
		exit(0);
	return 0;
}

int main(void)
{
	const int PAGE_SIZE = sysconf(_SC_PAGESIZE);
	pid_t pid;
	void *stack;

	 
	f((void *)0);

	stack = mmap(NULL, 2 * PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	assert(stack != MAP_FAILED);
	 
	pid = clone(f, stack + PAGE_SIZE, CLONE_THREAD|CLONE_SIGHAND|CLONE_VM, (void *)1);
	assert(pid > 0);
	pause();

	return 0;
}
