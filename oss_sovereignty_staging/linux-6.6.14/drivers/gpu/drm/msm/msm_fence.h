 
 

#ifndef __MSM_FENCE_H__
#define __MSM_FENCE_H__

#include "msm_drv.h"

 
struct msm_fence_context {
	struct drm_device *dev;
	 
	char name[32];
	 
	unsigned context;
	 
	unsigned index;

	 
	uint32_t last_fence;

	 
	uint32_t completed_fence;

	 
	volatile uint32_t *fenceptr;

	spinlock_t spinlock;

	 

	 
	ktime_t next_deadline;

	 
	uint32_t next_deadline_fence;

	struct hrtimer deadline_timer;
	struct kthread_work deadline_work;
};

struct msm_fence_context * msm_fence_context_alloc(struct drm_device *dev,
		volatile uint32_t *fenceptr, const char *name);
void msm_fence_context_free(struct msm_fence_context *fctx);

bool msm_fence_completed(struct msm_fence_context *fctx, uint32_t fence);
void msm_update_fence(struct msm_fence_context *fctx, uint32_t fence);

struct dma_fence * msm_fence_alloc(void);
void msm_fence_init(struct dma_fence *fence, struct msm_fence_context *fctx);

static inline bool
fence_before(uint32_t a, uint32_t b)
{
   return (int32_t)(a - b) < 0;
}

static inline bool
fence_after(uint32_t a, uint32_t b)
{
   return (int32_t)(a - b) > 0;
}

#endif
