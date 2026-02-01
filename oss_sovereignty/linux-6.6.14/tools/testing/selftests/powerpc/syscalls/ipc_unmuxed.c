
 

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "utils.h"


#define DO_TEST(_name, _num)	\
static int test_##_name(void)			\
{						\
	int rc;					\
	printf("Testing " #_name);		\
	errno = 0;				\
	rc = syscall(_num, -1, 0, 0, 0, 0, 0);	\
	printf("\treturned %d, errno %d\n", rc, errno); \
	return errno == ENOSYS;			\
}

#include "ipc.h"
#undef DO_TEST

static int ipc_unmuxed(void)
{
	int tests_done = 0;

#define DO_TEST(_name, _num)		\
	FAIL_IF(test_##_name());	\
	tests_done++;

#include "ipc.h"
#undef DO_TEST

	 
	SKIP_IF(tests_done == 0);

	return 0;
}

int main(void)
{
	return test_harness(ipc_unmuxed, "ipc_unmuxed");
}
