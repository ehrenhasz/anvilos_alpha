
 

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "utils.h"
#include "tm.h"

void signal_segv(int signum)
{
	 
	exit(1);
}

int tm_signal_stack()
{
	int pid;

	SKIP_IF(!have_htm());
	SKIP_IF(htm_is_synthetic());

	pid = fork();
	if (pid < 0)
		exit(1);

	if (pid) {  
		 
		wait(NULL);
		return 0;
	}

	 
	if (signal(SIGSEGV, signal_segv) == SIG_ERR)
		exit(1);
	asm volatile("li 1, 0 ;"		 
		     "1:"
		     "tbegin.;"
		     "beq 1b ;"			 
		     "tsuspend.;"
		     "ld 2, 0(1) ;"		 
		     : : : "memory");

	 
	return 1;
}

int main(void)
{
	return test_harness(tm_signal_stack, "tm_signal_stack");
}
