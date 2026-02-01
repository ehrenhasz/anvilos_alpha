 

#include "sync.h"
#include "sw_sync.h"
#include "synctest.h"

int test_alloc_timeline(void)
{
	int timeline, valid;

	timeline = sw_sync_timeline_create();
	valid = sw_sync_timeline_is_valid(timeline);
	ASSERT(valid, "Failure allocating timeline\n");

	sw_sync_timeline_destroy(timeline);
	return 0;
}

int test_alloc_fence(void)
{
	int timeline, fence, valid;

	timeline = sw_sync_timeline_create();
	valid = sw_sync_timeline_is_valid(timeline);
	ASSERT(valid, "Failure allocating timeline\n");

	fence = sw_sync_fence_create(timeline, "allocFence", 1);
	valid = sw_sync_fence_is_valid(fence);
	ASSERT(valid, "Failure allocating fence\n");

	sw_sync_fence_destroy(fence);
	sw_sync_timeline_destroy(timeline);
	return 0;
}

int test_alloc_fence_negative(void)
{
	int fence, timeline;

	timeline = sw_sync_timeline_create();
	ASSERT(timeline > 0, "Failure allocating timeline\n");

	fence = sw_sync_fence_create(-1, "fence", 1);
	ASSERT(fence < 0, "Success allocating negative fence\n");

	sw_sync_fence_destroy(fence);
	sw_sync_timeline_destroy(timeline);
	return 0;
}
