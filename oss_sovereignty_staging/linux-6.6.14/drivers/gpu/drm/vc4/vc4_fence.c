 

#include "vc4_drv.h"

static const char *vc4_fence_get_driver_name(struct dma_fence *fence)
{
	return "vc4";
}

static const char *vc4_fence_get_timeline_name(struct dma_fence *fence)
{
	return "vc4-v3d";
}

static bool vc4_fence_signaled(struct dma_fence *fence)
{
	struct vc4_fence *f = to_vc4_fence(fence);
	struct vc4_dev *vc4 = to_vc4_dev(f->dev);

	return vc4->finished_seqno >= f->seqno;
}

const struct dma_fence_ops vc4_fence_ops = {
	.get_driver_name = vc4_fence_get_driver_name,
	.get_timeline_name = vc4_fence_get_timeline_name,
	.signaled = vc4_fence_signaled,
};
