 

#include <linux/dma-buf.h>
#include <linux/dma-resv.h>

#include <drm/drm_file.h>

#include "vgem_drv.h"

#define VGEM_FENCE_TIMEOUT (10*HZ)

struct vgem_fence {
	struct dma_fence base;
	struct spinlock lock;
	struct timer_list timer;
};

static const char *vgem_fence_get_driver_name(struct dma_fence *fence)
{
	return "vgem";
}

static const char *vgem_fence_get_timeline_name(struct dma_fence *fence)
{
	return "unbound";
}

static void vgem_fence_release(struct dma_fence *base)
{
	struct vgem_fence *fence = container_of(base, typeof(*fence), base);

	del_timer_sync(&fence->timer);
	dma_fence_free(&fence->base);
}

static void vgem_fence_value_str(struct dma_fence *fence, char *str, int size)
{
	snprintf(str, size, "%llu", fence->seqno);
}

static void vgem_fence_timeline_value_str(struct dma_fence *fence, char *str,
					  int size)
{
	snprintf(str, size, "%llu",
		 dma_fence_is_signaled(fence) ? fence->seqno : 0);
}

static const struct dma_fence_ops vgem_fence_ops = {
	.get_driver_name = vgem_fence_get_driver_name,
	.get_timeline_name = vgem_fence_get_timeline_name,
	.release = vgem_fence_release,

	.fence_value_str = vgem_fence_value_str,
	.timeline_value_str = vgem_fence_timeline_value_str,
};

static void vgem_fence_timeout(struct timer_list *t)
{
	struct vgem_fence *fence = from_timer(fence, t, timer);

	dma_fence_signal(&fence->base);
}

static struct dma_fence *vgem_fence_create(struct vgem_file *vfile,
					   unsigned int flags)
{
	struct vgem_fence *fence;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence)
		return NULL;

	spin_lock_init(&fence->lock);
	dma_fence_init(&fence->base, &vgem_fence_ops, &fence->lock,
		       dma_fence_context_alloc(1), 1);

	timer_setup(&fence->timer, vgem_fence_timeout, 0);

	 
	mod_timer(&fence->timer, jiffies + VGEM_FENCE_TIMEOUT);

	return &fence->base;
}

 
int vgem_fence_attach_ioctl(struct drm_device *dev,
			    void *data,
			    struct drm_file *file)
{
	struct drm_vgem_fence_attach *arg = data;
	struct vgem_file *vfile = file->driver_priv;
	struct dma_resv *resv;
	struct drm_gem_object *obj;
	enum dma_resv_usage usage;
	struct dma_fence *fence;
	int ret;

	if (arg->flags & ~VGEM_FENCE_WRITE)
		return -EINVAL;

	if (arg->pad)
		return -EINVAL;

	obj = drm_gem_object_lookup(file, arg->handle);
	if (!obj)
		return -ENOENT;

	fence = vgem_fence_create(vfile, arg->flags);
	if (!fence) {
		ret = -ENOMEM;
		goto err;
	}

	 
	resv = obj->resv;
	usage = dma_resv_usage_rw(arg->flags & VGEM_FENCE_WRITE);
	if (!dma_resv_test_signaled(resv, usage)) {
		ret = -EBUSY;
		goto err_fence;
	}

	 
	dma_resv_lock(resv, NULL);
	ret = dma_resv_reserve_fences(resv, 1);
	if (!ret)
		dma_resv_add_fence(resv, fence, arg->flags & VGEM_FENCE_WRITE ?
				   DMA_RESV_USAGE_WRITE : DMA_RESV_USAGE_READ);
	dma_resv_unlock(resv);

	 
	if (ret == 0) {
		mutex_lock(&vfile->fence_mutex);
		ret = idr_alloc(&vfile->fence_idr, fence, 1, 0, GFP_KERNEL);
		mutex_unlock(&vfile->fence_mutex);
		if (ret > 0) {
			arg->out_fence = ret;
			ret = 0;
		}
	}
err_fence:
	if (ret) {
		dma_fence_signal(fence);
		dma_fence_put(fence);
	}
err:
	drm_gem_object_put(obj);
	return ret;
}

 
int vgem_fence_signal_ioctl(struct drm_device *dev,
			    void *data,
			    struct drm_file *file)
{
	struct vgem_file *vfile = file->driver_priv;
	struct drm_vgem_fence_signal *arg = data;
	struct dma_fence *fence;
	int ret = 0;

	if (arg->flags)
		return -EINVAL;

	mutex_lock(&vfile->fence_mutex);
	fence = idr_replace(&vfile->fence_idr, NULL, arg->fence);
	mutex_unlock(&vfile->fence_mutex);
	if (!fence)
		return -ENOENT;
	if (IS_ERR(fence))
		return PTR_ERR(fence);

	if (dma_fence_is_signaled(fence))
		ret = -ETIMEDOUT;

	dma_fence_signal(fence);
	dma_fence_put(fence);
	return ret;
}

int vgem_fence_open(struct vgem_file *vfile)
{
	mutex_init(&vfile->fence_mutex);
	idr_init_base(&vfile->fence_idr, 1);

	return 0;
}

static int __vgem_fence_idr_fini(int id, void *p, void *data)
{
	dma_fence_signal(p);
	dma_fence_put(p);
	return 0;
}

void vgem_fence_close(struct vgem_file *vfile)
{
	idr_for_each(&vfile->fence_idr, __vgem_fence_idr_fini, vfile);
	idr_destroy(&vfile->fence_idr);
	mutex_destroy(&vfile->fence_mutex);
}
