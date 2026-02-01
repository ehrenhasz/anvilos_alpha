
 

 

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include "timeout.h"

static volatile bool timeout;

 
void sigalrm(int signo)
{
	timeout = true;
}

 
void timeout_begin(unsigned int seconds)
{
	alarm(seconds);
}

 
void timeout_check(const char *operation)
{
	if (timeout) {
		fprintf(stderr, "%s timed out\n", operation);
		exit(EXIT_FAILURE);
	}
}

 
void timeout_end(void)
{
	alarm(0);
	timeout = false;
}
