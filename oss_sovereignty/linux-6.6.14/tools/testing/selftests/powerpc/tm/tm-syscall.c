
 

#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <asm/tm.h>
#include <sys/time.h>
#include <stdlib.h>

#include "utils.h"
#include "tm.h"

#ifndef PPC_FEATURE2_SCV
#define PPC_FEATURE2_SCV               0x00100000  
#endif

extern int getppid_tm_active(void);
extern int getppid_tm_suspended(void);
extern int getppid_scv_tm_active(void);
extern int getppid_scv_tm_suspended(void);

unsigned retries = 0;

#define TEST_DURATION 10  

pid_t getppid_tm(bool scv, bool suspend)
{
	int i;
	pid_t pid;

	for (i = 0; i < TM_RETRIES; i++) {
		if (suspend) {
			if (scv)
				pid = getppid_scv_tm_suspended();
			else
				pid = getppid_tm_suspended();
		} else {
			if (scv)
				pid = getppid_scv_tm_active();
			else
				pid = getppid_tm_active();
		}

		if (pid >= 0)
			return pid;

		if (failure_is_persistent()) {
			if (failure_is_syscall())
				return -1;

			printf("Unexpected persistent transaction failure.\n");
			printf("TEXASR 0x%016lx, TFIAR 0x%016lx.\n",
			       __builtin_get_texasr(), __builtin_get_tfiar());
			exit(-1);
		}

		retries++;
	}

	printf("Exceeded limit of %d temporary transaction failures.\n", TM_RETRIES);
	printf("TEXASR 0x%016lx, TFIAR 0x%016lx.\n",
	       __builtin_get_texasr(), __builtin_get_tfiar());

	exit(-1);
}

int tm_syscall(void)
{
	unsigned count = 0;
	struct timeval end, now;

	SKIP_IF(!have_htm_nosc());
	SKIP_IF(htm_is_synthetic());

	setbuf(stdout, NULL);

	printf("Testing transactional syscalls for %d seconds...\n", TEST_DURATION);

	gettimeofday(&end, NULL);
	now.tv_sec = TEST_DURATION;
	now.tv_usec = 0;
	timeradd(&end, &now, &end);

	for (count = 0; timercmp(&now, &end, <); count++) {
		 
		FAIL_IF(getppid_tm(false, true) == -1);  

		 
		FAIL_IF(getppid_tm(false, false) != -1);   
		FAIL_IF(!failure_is_persistent());  
		FAIL_IF(!failure_is_syscall());     

		 
		if (have_hwcap2(PPC_FEATURE2_SCV)) {
			FAIL_IF(getppid_tm(true, true) == -1);  
			FAIL_IF(getppid_tm(true, false) != -1);   
			FAIL_IF(!failure_is_persistent());  
			FAIL_IF(!failure_is_syscall());     
		}

		gettimeofday(&now, 0);
	}

	printf("%d active and suspended transactions behaved correctly.\n", count);
	printf("(There were %d transaction retries.)\n", retries);

	return 0;
}

int main(void)
{
	return test_harness(tm_syscall, "tm_syscall");
}
