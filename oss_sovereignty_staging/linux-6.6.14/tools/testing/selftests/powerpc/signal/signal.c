
 

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <altivec.h>

#include "utils.h"

#define MAX_ATTEMPT 500000
#define TIMEOUT 5

extern long signal_self(pid_t pid, int sig);

static sig_atomic_t signaled;
static sig_atomic_t fail;

static void signal_handler(int sig)
{
	if (sig == SIGUSR1)
		signaled = 1;
	else
		fail = 1;
}

static int test_signal()
{
	int i;
	struct sigaction act;
	pid_t ppid = getpid();
	pid_t pid;

	act.sa_handler = signal_handler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	if (sigaction(SIGUSR1, &act, NULL) < 0) {
		perror("sigaction SIGUSR1");
		exit(1);
	}
	if (sigaction(SIGALRM, &act, NULL) < 0) {
		perror("sigaction SIGALRM");
		exit(1);
	}

	 
	for(i  = 0; i < 1000; i++) {
		pid = fork();
		if (pid == -1) {
			perror("fork");
			exit(1);
		}
		if (pid == 0) {
			signal_self(ppid, SIGUSR1);
			exit(1);
		} else {
			alarm(0);  
			alarm(2);
			while (!signaled && !fail)
				asm volatile("": : :"memory");
			if (!signaled) {
				fprintf(stderr, "Didn't get signal from child\n");
				FAIL_IF(1);  
			}
			 
			waitpid(pid, NULL, 0);
		}
	}

	for (i = 0; i < MAX_ATTEMPT; i++) {
		long rc;

		alarm(0);  
		signaled = 0;
		alarm(TIMEOUT);
		rc = signal_self(ppid, SIGUSR1);
		if (rc) {
			fprintf(stderr, "(%d) Fail reason: %d rc=0x%lx",
					i, fail, rc);
			FAIL_IF(1);  
		}
		while (!signaled && !fail)
			asm volatile("": : :"memory");
		if (!signaled) {
			fprintf(stderr, "(%d) Fail reason: %d rc=0x%lx",
					i, fail, rc);
			FAIL_IF(1);  
		}
	}

	return 0;
}

int main(void)
{
	test_harness_set_timeout(300);
	return test_harness(test_signal, "signal");
}
