 

#include "sync.h"
#include "sw_sync.h"
#include "synctest.h"

int test_fence_multi_timeline_wait(void)
{
	int timelineA, timelineB, timelineC;
	int fenceA, fenceB, fenceC, merged;
	int valid, active, signaled, ret;

	timelineA = sw_sync_timeline_create();
	timelineB = sw_sync_timeline_create();
	timelineC = sw_sync_timeline_create();

	fenceA = sw_sync_fence_create(timelineA, "fenceA", 5);
	fenceB = sw_sync_fence_create(timelineB, "fenceB", 5);
	fenceC = sw_sync_fence_create(timelineC, "fenceC", 5);

	merged = sync_merge("mergeFence", fenceB, fenceA);
	merged = sync_merge("mergeFence", fenceC, merged);

	valid = sw_sync_fence_is_valid(merged);
	ASSERT(valid, "Failure merging fence from various timelines\n");

	 
	active = sync_fence_count_with_status(merged, FENCE_STATUS_ACTIVE);
	ASSERT(active == 3, "Fence signaled too early!\n");

	ret = sync_wait(merged, 0);
	ASSERT(ret == 0,
	       "Failure waiting on fence until timeout\n");

	ret = sw_sync_timeline_inc(timelineA, 5);
	active = sync_fence_count_with_status(merged, FENCE_STATUS_ACTIVE);
	signaled = sync_fence_count_with_status(merged, FENCE_STATUS_SIGNALED);
	ASSERT(active == 2 && signaled == 1,
	       "Fence did not signal properly!\n");

	ret = sw_sync_timeline_inc(timelineB, 5);
	active = sync_fence_count_with_status(merged, FENCE_STATUS_ACTIVE);
	signaled = sync_fence_count_with_status(merged, FENCE_STATUS_SIGNALED);
	ASSERT(active == 1 && signaled == 2,
	       "Fence did not signal properly!\n");

	ret = sw_sync_timeline_inc(timelineC, 5);
	active = sync_fence_count_with_status(merged, FENCE_STATUS_ACTIVE);
	signaled = sync_fence_count_with_status(merged, FENCE_STATUS_SIGNALED);
	ASSERT(active == 0 && signaled == 3,
	       "Fence did not signal properly!\n");

	 
	ret = sync_wait(merged, 100);
	ASSERT(ret > 0, "Failure waiting on signaled fence\n");

	sw_sync_fence_destroy(merged);
	sw_sync_fence_destroy(fenceC);
	sw_sync_fence_destroy(fenceB);
	sw_sync_fence_destroy(fenceA);
	sw_sync_timeline_destroy(timelineC);
	sw_sync_timeline_destroy(timelineB);
	sw_sync_timeline_destroy(timelineA);

	return 0;
}
