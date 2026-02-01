
 

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sched.h>
#include <sys/types.h>
#include <signal.h>

#include "tm.h"

int tm_poison_test(void)
{
	int cpu, pid;
	cpu_set_t cpuset;
	uint64_t poison = 0xdeadbeefc0dec0fe;
	uint64_t unknown = 0;
	bool fail_fp = false;
	bool fail_vr = false;

	SKIP_IF(!have_htm());
	SKIP_IF(htm_is_synthetic());

	cpu = pick_online_cpu();
	FAIL_IF(cpu < 0);

	 
	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);
	FAIL_IF(sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0);

	pid = fork();
	if (!pid) {
		 
		while (1) {
			sched_yield();
			asm (
				"mtvsrd 31, %[poison];"  
				"mtvsrd 63, %[poison];"  

				: : [poison] "r" (poison) : );
		}
	}

	 
	asm (
		 
		"       li      3, 0x1          ;"
		"       li      4, 0x1          ;"
		"       mtvsrd  31, 4           ;"

		 
		"       lis     5, 14           ;"
		"       ori     5, 5, 19996     ;"
		"       sldi    5, 5, 16        ;"  

		"       mfspr   6, 268          ;"  
		"1:     mfspr   7, 268          ;"  
		"       subf    7, 6, 7         ;"  
		"       cmpd    7, 5            ;"
		"       bgt     3f              ;"  

		 
		"       tbegin.                 ;"  
		"       beq     1b              ;"  
		"       mfvsrd  3, 31           ;"  
		"       cmpd    3, 4            ;"  
		"       bne     2f              ;"  
		"       tabort. 3               ;"  
		"2:     tend.                   ;"  
		"3:     mr    %[unknown], 3     ;"  

		: [unknown] "=r" (unknown)
		:
		: "cr0", "r3", "r4", "r5", "r6", "r7", "vs31"

		);

	 
	fail_fp = unknown != 0x1;
	if (fail_fp)
		printf("Unknown value %#"PRIx64" leaked into f31!\n", unknown);
	else
		printf("Good, no poison or leaked value into FP registers\n");

	asm (
		 
		"       li      3, 0x1          ;"
		"       li      4, 0x1          ;"
		"       mtvsrd  63, 4           ;"

		"       lis     5, 14           ;"
		"       ori     5, 5, 19996     ;"
		"       sldi    5, 5, 16        ;" 

		"       mfspr   6, 268          ;" 
		"1:     mfspr   7, 268          ;" 
		"       subf    7, 6, 7         ;" 
		"       cmpd    7, 5            ;"
		"       bgt     3f              ;" 

		 
		"       tbegin.                 ;" 
		"       beq     1b              ;" 
		"       mfvsrd  3, 63           ;" 
		"       cmpd    3, 4            ;" 
		"       bne     2f              ;" 
		"       tabort. 3               ;" 
		"2:     tend.                   ;" 
		"3:     mr    %[unknown], 3     ;" 

		: [unknown] "=r" (unknown)
		:
		: "cr0", "r3", "r4", "r5", "r6", "r7", "vs63"

		);

	 
	fail_vr = unknown != 0x1;
	if (fail_vr)
		printf("Unknown value %#"PRIx64" leaked into vr31!\n", unknown);
	else
		printf("Good, no poison or leaked value into VEC registers\n");

	kill(pid, SIGKILL);

	return (fail_fp | fail_vr);
}

int main(int argc, char *argv[])
{
	 
	test_harness_set_timeout(250);
	return test_harness(tm_poison_test, "tm_poison_test");
}
