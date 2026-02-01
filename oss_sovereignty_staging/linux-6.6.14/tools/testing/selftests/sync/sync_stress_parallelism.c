 

#include <pthread.h>

#include "sync.h"
#include "sw_sync.h"
#include "synctest.h"

static struct {
	int iterations;
	int timeline;
	int counter;
} test_data_two_threads;

static int test_stress_two_threads_shared_timeline_thread(void *d)
{
	int thread_id = (long)d;
	int timeline = test_data_two_threads.timeline;
	int iterations = test_data_two_threads.iterations;
	int fence, valid, ret, i;

	for (i = 0; i < iterations; i++) {
		fence = sw_sync_fence_create(timeline, "fence",
					     i * 2 + thread_id);
		valid = sw_sync_fence_is_valid(fence);
		ASSERT(valid, "Failure allocating fence\n");

		 
		ret = sync_wait(fence, -1);
		ASSERT(ret > 0, "Problem occurred on prior thread\n");

		 
		ASSERT(test_data_two_threads.counter == i * 2 + thread_id,
		       "Counter got damaged!\n");
		test_data_two_threads.counter++;

		 
		ret = sw_sync_timeline_inc(timeline, 1);
		ASSERT(ret == 0, "Advancing timeline failed\n");

		sw_sync_fence_destroy(fence);
	}

	return 0;
}

int test_stress_two_threads_shared_timeline(void)
{
	pthread_t a, b;
	int valid;
	int timeline = sw_sync_timeline_create();

	valid = sw_sync_timeline_is_valid(timeline);
	ASSERT(valid, "Failure allocating timeline\n");

	test_data_two_threads.iterations = 1 << 16;
	test_data_two_threads.counter = 0;
	test_data_two_threads.timeline = timeline;

	 

	pthread_create(&a, NULL, (void *(*)(void *))
		       test_stress_two_threads_shared_timeline_thread,
		       (void *)0);
	pthread_create(&b, NULL, (void *(*)(void *))
		       test_stress_two_threads_shared_timeline_thread,
		       (void *)1);

	pthread_join(a, NULL);
	pthread_join(b, NULL);

	 
	ASSERT(test_data_two_threads.counter ==
	       test_data_two_threads.iterations * 2,
	       "Counter has unexpected value\n");

	sw_sync_timeline_destroy(timeline);

	return 0;
}
