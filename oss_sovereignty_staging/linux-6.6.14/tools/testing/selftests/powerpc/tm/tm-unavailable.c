
 

#define _GNU_SOURCE
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdbool.h>
#include <pthread.h>
#include <sched.h>

#include "tm.h"

#define DEBUG 0

 
#define FP_UNA_EXCEPTION	0
#define VEC_UNA_EXCEPTION	1
#define VSX_UNA_EXCEPTION	2

#define NUM_EXCEPTIONS		3
#define err_at_line(status, errnum, format, ...) \
	error_at_line(status, errnum,  __FILE__, __LINE__, format ##__VA_ARGS__)

#define pr_warn(code, format, ...) err_at_line(0, code, format, ##__VA_ARGS__)
#define pr_err(code, format, ...) err_at_line(1, code, format, ##__VA_ARGS__)

struct Flags {
	int touch_fp;
	int touch_vec;
	int result;
	int exception;
} flags;

bool expecting_failure(void)
{
	if (flags.touch_fp && flags.exception == FP_UNA_EXCEPTION)
		return false;

	if (flags.touch_vec && flags.exception == VEC_UNA_EXCEPTION)
		return false;

	 
	if ((flags.touch_fp && flags.touch_vec) &&
	     flags.exception == VSX_UNA_EXCEPTION)
		return false;

	return true;
}

 
bool is_failure(uint64_t condition_reg)
{
	 
	return ((condition_reg >> 28) & 0xa) == 0xa;
}

void *tm_una_ping(void *input)
{

	 
	uint64_t high_vs0 = 0x5555555555555555;
	uint64_t low_vs0 = 0xffffffffffffffff;
	uint64_t high_vs32 = 0x5555555555555555;
	uint64_t low_vs32 = 0xffffffffffffffff;

	 
	uint64_t counter = 0x1ff000000;

	 
	uint64_t cr_ = 0;

	 
	if (DEBUG)
		sleep(1);

	printf("If MSR.FP=%d MSR.VEC=%d: ", flags.touch_fp, flags.touch_vec);

	if (flags.exception != FP_UNA_EXCEPTION &&
	    flags.exception != VEC_UNA_EXCEPTION &&
	    flags.exception != VSX_UNA_EXCEPTION) {
		printf("No valid exception specified to test.\n");
		return NULL;
	}

	asm (
		 
		"	mtvsrd		33, %[high_vs0]		;"
		"	mtvsrd		34, %[low_vs0]		;"

		 
		"	xxmrghd		0, 33, 34		;"

		 
		"	xxmrghd		32, 33, 34		;"

		 
		"	mtctr		%[counter]		;"

		 
		"1:	bdnz 1b					;"

		 
		"	cmpldi		%[touch_fp], 0		;"
		"	beq		no_fp			;"
		"	fadd		10, 10, 10		;"
		"no_fp:						;"

		 
		"	cmpldi		%[touch_vec], 0		;"
		"	beq		no_vec			;"
		"	vaddcuw		10, 10, 10		;"
		"no_vec:					;"

		 
		"	tbegin.					;"
		"	beq		trans_fail		;"

		 
		"	cmpldi		%[exception], %[ex_fp]	;"
		"	bne		1f			;"
		"	fadd		10, 10, 10		;"
		"	b		done			;"

		 
		"1:	cmpldi		%[exception], %[ex_vec]	;"
		"	bne		2f			;"
		"	vaddcuw		10, 10, 10		;"
		"	b		done			;"

		 
		"2:	xxmrghd		10, 10, 10		;"

		"done:	tend. ;"

		"trans_fail: ;"

		 
		"	mfvsrd		%[high_vs0], 0		;"
		"	xxsldwi		3, 0, 0, 2		;"
		"	mfvsrd		%[low_vs0], 3		;"
		"	mfvsrd		%[high_vs32], 32	;"
		"	xxsldwi		3, 32, 32, 2		;"
		"	mfvsrd		%[low_vs32], 3		;"

		 
		"	mfcr		%[cr_]		;"

		: [high_vs0]  "+r" (high_vs0),
		  [low_vs0]   "+r" (low_vs0),
		  [high_vs32] "=r" (high_vs32),
		  [low_vs32]  "=r" (low_vs32),
		  [cr_]       "+r" (cr_)
		: [touch_fp]  "r"  (flags.touch_fp),
		  [touch_vec] "r"  (flags.touch_vec),
		  [exception] "r"  (flags.exception),
		  [ex_fp]     "i"  (FP_UNA_EXCEPTION),
		  [ex_vec]    "i"  (VEC_UNA_EXCEPTION),
		  [ex_vsx]    "i"  (VSX_UNA_EXCEPTION),
		  [counter]   "r"  (counter)

		: "cr0", "ctr", "v10", "vs0", "vs10", "vs3", "vs32", "vs33",
		  "vs34", "fr10"

		);

	 
	if (expecting_failure() && !is_failure(cr_)) {
		printf("\n\tExpecting the transaction to fail, %s",
			"but it didn't\n\t");
		flags.result++;
	}

	 
	if (!expecting_failure() && is_failure(cr_) &&
	    !failure_is_reschedule()) {
		printf("\n\tUnexpected transaction failure 0x%02lx\n\t",
			failure_code());
		return (void *) -1;
	}

	 
	if (is_failure(cr_) && !failure_is_unavailable() &&
	    !failure_is_reschedule()) {
		printf("\n\tUnexpected failure cause 0x%02lx\n\t",
			failure_code());
		return (void *) -1;
	}

	 
	if (DEBUG)
		printf("CR0: 0x%1lx ", cr_ >> 28);

	 
	if (high_vs0 != 0x5555555555555555 || low_vs0 != 0xFFFFFFFFFFFFFFFF) {
		printf("FP corrupted!");
			printf("  high = %#16" PRIx64 "  low = %#16" PRIx64 " ",
				high_vs0, low_vs0);
		flags.result++;
	} else
		printf("FP ok ");

	 
	if (high_vs32 != 0x5555555555555555 || low_vs32 != 0xFFFFFFFFFFFFFFFF) {
		printf("VEC corrupted!");
			printf("  high = %#16" PRIx64 "  low = %#16" PRIx64,
				high_vs32, low_vs32);
		flags.result++;
	} else
		printf("VEC ok");

	putchar('\n');

	return NULL;
}

 
void *tm_una_pong(void *not_used)
{
	 
	if (DEBUG)
		sleep(1);

	 
	while (1)
		sched_yield();
}

 
void test_fp_vec(int fp, int vec, pthread_attr_t *attr)
{
	int retries = 2;
	void *ret_value;
	pthread_t t0;

	flags.touch_fp = fp;
	flags.touch_vec = vec;

	 
	do {
		int rc;

		 
		rc = pthread_create(&t0, attr, tm_una_ping, (void *) &flags);
		if (rc)
			pr_err(rc, "pthread_create()");
		rc = pthread_setname_np(t0, "tm_una_ping");
		if (rc)
			pr_warn(rc, "pthread_setname_np");
		rc = pthread_join(t0, &ret_value);
		if (rc)
			pr_err(rc, "pthread_join");

		retries--;
	} while (ret_value != NULL && retries);

	if (!retries) {
		flags.result = 1;
		if (DEBUG)
			printf("All transactions failed unexpectedly\n");

	}
}

int tm_unavailable_test(void)
{
	int cpu, rc, exception;  
	pthread_t t1;
	pthread_attr_t attr;
	cpu_set_t cpuset;

	SKIP_IF(!have_htm());
	SKIP_IF(htm_is_synthetic());

	cpu = pick_online_cpu();
	FAIL_IF(cpu < 0);

	
	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);

	 
	rc = pthread_attr_init(&attr);
	if (rc)
		pr_err(rc, "pthread_attr_init()");

	 
	rc = pthread_attr_setaffinity_np(&attr, sizeof(cpu_set_t), &cpuset);
	if (rc)
		pr_err(rc, "pthread_attr_setaffinity_np()");

	rc = pthread_create(&t1, &attr  , tm_una_pong, NULL);
	if (rc)
		pr_err(rc, "pthread_create()");

	 
	rc = pthread_setname_np(t1, "tm_una_pong");
	if (rc)
		pr_warn(rc, "pthread_create()");

	flags.result = 0;

	for (exception = 0; exception < NUM_EXCEPTIONS; exception++) {
		printf("Checking if FP/VEC registers are sane after");

		if (exception == FP_UNA_EXCEPTION)
			printf(" a FP unavailable exception...\n");

		else if (exception == VEC_UNA_EXCEPTION)
			printf(" a VEC unavailable exception...\n");

		else
			printf(" a VSX unavailable exception...\n");

		flags.exception = exception;

		test_fp_vec(0, 0, &attr);
		test_fp_vec(1, 0, &attr);
		test_fp_vec(0, 1, &attr);
		test_fp_vec(1, 1, &attr);

	}

	if (flags.result > 0) {
		printf("result: failed!\n");
		exit(1);
	} else {
		printf("result: success\n");
		exit(0);
	}
}

int main(int argc, char **argv)
{
	test_harness_set_timeout(220);
	return test_harness(tm_unavailable_test, "tm_unavailable_test");
}
