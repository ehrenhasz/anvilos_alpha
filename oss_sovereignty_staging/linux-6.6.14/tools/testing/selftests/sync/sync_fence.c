 

#include "sync.h"
#include "sw_sync.h"
#include "synctest.h"

int test_fence_one_timeline_wait(void)
{
	int fence, valid, ret;
	int timeline = sw_sync_timeline_create();

	valid = sw_sync_timeline_is_valid(timeline);
	ASSERT(valid, "Failure allocating timeline\n");

	fence = sw_sync_fence_create(timeline, "allocFence", 5);
	valid = sw_sync_fence_is_valid(fence);
	ASSERT(valid, "Failure allocating fence\n");

	 
	ret = sync_wait(fence, 0);
	ASSERT(ret == 0, "Failure waiting on fence until timeout\n");

	 
	ret = sw_sync_timeline_inc(timeline, 1);
	ASSERT(ret == 0, "Failure advancing timeline\n");

	 
	ret = sync_wait(fence, 0);
	ASSERT(ret == 0, "Failure waiting on fence until timeout\n");

	 
	ret = sw_sync_timeline_inc(timeline, 4);
	ASSERT(ret == 0, "Failure signaling the fence\n");

	 
	ret = sync_wait(fence, 0);
	ASSERT(ret > 0, "Failure waiting on fence\n");

	 
	ret = sw_sync_timeline_inc(timeline, 10);
	ASSERT(ret == 0, "Failure going further\n");
	ret = sync_wait(fence, 0);
	ASSERT(ret > 0, "Failure waiting ahead\n");

	sw_sync_fence_destroy(fence);
	sw_sync_timeline_destroy(timeline);

	return 0;
}

int test_fence_one_timeline_merge(void)
{
	int a, b, c, d, valid;
	int timeline = sw_sync_timeline_create();

	 
	a = sw_sync_fence_create(timeline, "allocFence", 1);
	b = sw_sync_fence_create(timeline, "allocFence", 2);
	c = sw_sync_fence_create(timeline, "allocFence", 3);

	valid = sw_sync_fence_is_valid(a) &&
		sw_sync_fence_is_valid(b) &&
		sw_sync_fence_is_valid(c);
	ASSERT(valid, "Failure allocating fences\n");

	d = sync_merge("mergeFence", b, a);
	d = sync_merge("mergeFence", c, d);
	valid = sw_sync_fence_is_valid(d);
	ASSERT(valid, "Failure merging fences\n");

	 
	ASSERT(sync_fence_count_with_status(a, FENCE_STATUS_ACTIVE) == 1,
	       "a has too many active fences!\n");
	ASSERT(sync_fence_count_with_status(a, FENCE_STATUS_ACTIVE) == 1,
	       "b has too many active fences!\n");
	ASSERT(sync_fence_count_with_status(a, FENCE_STATUS_ACTIVE) == 1,
	       "c has too many active fences!\n");
	ASSERT(sync_fence_count_with_status(a, FENCE_STATUS_ACTIVE) == 1,
	       "d has too many active fences!\n");

	 
	sw_sync_timeline_inc(timeline, 1);
	ASSERT(sync_fence_count_with_status(a, FENCE_STATUS_SIGNALED) == 1,
	       "a did not signal!\n");
	ASSERT(sync_fence_count_with_status(d, FENCE_STATUS_ACTIVE) == 1,
	       "d signaled too early!\n");

	sw_sync_timeline_inc(timeline, 1);
	ASSERT(sync_fence_count_with_status(b, FENCE_STATUS_SIGNALED) == 1,
	       "b did not signal!\n");
	ASSERT(sync_fence_count_with_status(d, FENCE_STATUS_ACTIVE) == 1,
	       "d signaled too early!\n");

	sw_sync_timeline_inc(timeline, 1);
	ASSERT(sync_fence_count_with_status(c, FENCE_STATUS_SIGNALED) == 1,
	       "c did not signal!\n");
	ASSERT(sync_fence_count_with_status(d, FENCE_STATUS_ACTIVE) == 0 &&
	       sync_fence_count_with_status(d, FENCE_STATUS_SIGNALED) == 1,
	       "d did not signal!\n");

	sw_sync_fence_destroy(d);
	sw_sync_fence_destroy(c);
	sw_sync_fence_destroy(b);
	sw_sync_fence_destroy(a);
	sw_sync_timeline_destroy(timeline);
	return 0;
}
