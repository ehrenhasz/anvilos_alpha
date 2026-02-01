 

#ifndef SELFTESTS_SYNCTEST_H
#define SELFTESTS_SYNCTEST_H

#include <stdio.h>
#include "../kselftest.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		ksft_print_msg("[ERROR]\t%s", (msg)); \
		return 1; \
	} \
} while (0)

#define RUN_TEST(x) run_test((x), #x)

 
int test_alloc_timeline(void);
int test_alloc_fence(void);
int test_alloc_fence_negative(void);

 
int test_fence_one_timeline_wait(void);
int test_fence_one_timeline_merge(void);

 
int test_fence_merge_same_fence(void);

 
int test_fence_multi_timeline_wait(void);

 
int test_stress_two_threads_shared_timeline(void);

 
int test_consumer_stress_multi_producer_single_consumer(void);

 
int test_merge_stress_random_merge(void);

#endif
