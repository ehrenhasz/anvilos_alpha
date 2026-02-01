 

#include "sync.h"
#include "sw_sync.h"
#include "synctest.h"

int test_fence_merge_same_fence(void)
{
	int fence, valid, merged;
	int timeline = sw_sync_timeline_create();

	valid = sw_sync_timeline_is_valid(timeline);
	ASSERT(valid, "Failure allocating timeline\n");

	fence = sw_sync_fence_create(timeline, "allocFence", 5);
	valid = sw_sync_fence_is_valid(fence);
	ASSERT(valid, "Failure allocating fence\n");

	merged = sync_merge("mergeFence", fence, fence);
	valid = sw_sync_fence_is_valid(fence);
	ASSERT(valid, "Failure merging fence\n");

	ASSERT(sync_fence_count_with_status(merged, FENCE_STATUS_SIGNALED) == 0,
	       "fence signaled too early!\n");

	sw_sync_timeline_inc(timeline, 5);
	ASSERT(sync_fence_count_with_status(merged, FENCE_STATUS_SIGNALED) == 1,
	       "fence did not signal!\n");

	sw_sync_fence_destroy(merged);
	sw_sync_fence_destroy(fence);
	sw_sync_timeline_destroy(timeline);

	return 0;
}
