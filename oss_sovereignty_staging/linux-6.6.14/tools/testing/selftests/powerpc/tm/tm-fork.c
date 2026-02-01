
 

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils.h"
#include "tm.h"

int test_fork(void)
{
	SKIP_IF(!have_htm());
	SKIP_IF(htm_is_synthetic());

	asm __volatile__(
		"tbegin.;"
		"blt    1f; "
		"li     0, 2;"   
		"sc  ;"
		"tend.;"
		"1: ;"
		: : : "memory", "r0");
	 

	return 0;
}

int main(int argc, char *argv[])
{
	return test_harness(test_fork, "tm_fork");
}
