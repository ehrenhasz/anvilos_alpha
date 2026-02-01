
 
 

#include <linux/dma-fence-chain.h>

#include "amdgpu.h"
#include "amdgpu_trace.h"
#include "amdgpu_amdkfd.h"

struct amdgpu_sync_entry {
	struct hlist_node	node;
	struct dma_fence	*fence;
};

static struct kmem_cache *amdgpu_sync_slab;

 
void amdgpu_sync_create(struct amdgpu_sync *sync)
{
	hash_init(sync->fences);
}

 
static bool amdgpu_sync_same_dev(struct amdgpu_device *adev,
				 struct dma_fence *f)
{
	struct drm_sched_fence *s_fence = to_drm_sched_fence(f);

	if (s_fence) {
		struct amdgpu_ring *ring;

		ring = container_of(s_fence->sched, struct amdgpu_ring, sched);
		return ring->adev == adev;
	}

	return false;
}

 
static void *amdgpu_sync_get_owner(struct dma_fence *f)
{
	struct drm_sched_fence *s_fence;
	struct amdgpu_amdkfd_fence *kfd_fence;

	if (!f)
		return AMDGPU_FENCE_OWNER_UNDEFINED;

	s_fence = to_drm_sched_fence(f);
	if (s_fence)
		return s_fence->owner;

	kfd_fence = to_amdgpu_amdkfd_fence(f);
	if (kfd_fence)
		return AMDGPU_FENCE_OWNER_KFD;

	return AMDGPU_FENCE_OWNER_UNDEFINED;
}

 
static void amdgpu_sync_keep_later(struct dma_fence **keep,
				   struct dma_fence *fence)
{
	if (*keep && dma_fence_is_later(*keep, fence))
		return;

	dma_fence_put(*keep);
	*keep = dma_fence_get(fence);
}

 
static bool amdgpu_sync_add_later(struct amdgpu_sync *sync, struct dma_fence *f)
{
	struct amdgpu_sync_entry *e;

	hash_for_each_possible(sync->fences, e, node, f->context) {
		if (unlikely(e->fence->context != f->context))
			continue;

		amdgpu_sync_keep_later(&e->fence, f);
		return true;
	}
	return false;
}

 
int amdgpu_sync_fence(struct amdgpu_sync *sync, struct dma_fence *f)
{
	struct amdgpu_sync_entry *e;

	if (!f)
		return 0;

	if (amdgpu_sync_add_later(sync, f))
		return 0;

	e = kmem_cache_alloc(amdgpu_sync_slab, GFP_KERNEL);
	if (!e)
		return -ENOMEM;

	hash_add(sync->fences, &e->node, f->context);
	e->fence = dma_fence_get(f);
	return 0;
}

 
static bool amdgpu_sync_test_fence(struct amdgpu_device *adev,
				   enum amdgpu_sync_mode mode,
				   void *owner, struct dma_fence *f)
{
	void *fence_owner = amdgpu_sync_get_owner(f);

	 
	if (fence_owner == AMDGPU_FENCE_OWNER_UNDEFINED)
		return true;

	 
	if (fence_owner == AMDGPU_FENCE_OWNER_KFD &&
	    owner != AMDGPU_FENCE_OWNER_UNDEFINED)
		return false;

	 
	if (fence_owner == AMDGPU_FENCE_OWNER_VM &&
	    owner != AMDGPU_FENCE_OWNER_UNDEFINED)
		return false;

	 
	switch (mode) {
	case AMDGPU_SYNC_ALWAYS:
		return true;

	case AMDGPU_SYNC_NE_OWNER:
		if (amdgpu_sync_same_dev(adev, f) &&
		    fence_owner == owner)
			return false;
		break;

	case AMDGPU_SYNC_EQ_OWNER:
		if (amdgpu_sync_same_dev(adev, f) &&
		    fence_owner != owner)
			return false;
		break;

	case AMDGPU_SYNC_EXPLICIT:
		return false;
	}

	WARN(debug_evictions && fence_owner == AMDGPU_FENCE_OWNER_KFD,
	     "Adding eviction fence to sync obj");
	return true;
}

 
int amdgpu_sync_resv(struct amdgpu_device *adev, struct amdgpu_sync *sync,
		     struct dma_resv *resv, enum amdgpu_sync_mode mode,
		     void *owner)
{
	struct dma_resv_iter cursor;
	struct dma_fence *f;
	int r;

	if (resv == NULL)
		return -EINVAL;

	 
	dma_resv_for_each_fence(&cursor, resv, DMA_RESV_USAGE_BOOKKEEP, f) {
		dma_fence_chain_for_each(f, f) {
			struct dma_fence *tmp = dma_fence_chain_contained(f);

			if (amdgpu_sync_test_fence(adev, mode, owner, tmp)) {
				r = amdgpu_sync_fence(sync, f);
				dma_fence_put(f);
				if (r)
					return r;
				break;
			}
		}
	}
	return 0;
}

 
static void amdgpu_sync_entry_free(struct amdgpu_sync_entry *e)
{
	hash_del(&e->node);
	dma_fence_put(e->fence);
	kmem_cache_free(amdgpu_sync_slab, e);
}

 
struct dma_fence *amdgpu_sync_peek_fence(struct amdgpu_sync *sync,
					 struct amdgpu_ring *ring)
{
	struct amdgpu_sync_entry *e;
	struct hlist_node *tmp;
	int i;

	hash_for_each_safe(sync->fences, i, tmp, e, node) {
		struct dma_fence *f = e->fence;
		struct drm_sched_fence *s_fence = to_drm_sched_fence(f);

		if (dma_fence_is_signaled(f)) {
			amdgpu_sync_entry_free(e);
			continue;
		}
		if (ring && s_fence) {
			 
			if (s_fence->sched == &ring->sched) {
				if (dma_fence_is_signaled(&s_fence->scheduled))
					continue;

				return &s_fence->scheduled;
			}
		}

		return f;
	}

	return NULL;
}

 
struct dma_fence *amdgpu_sync_get_fence(struct amdgpu_sync *sync)
{
	struct amdgpu_sync_entry *e;
	struct hlist_node *tmp;
	struct dma_fence *f;
	int i;

	hash_for_each_safe(sync->fences, i, tmp, e, node) {

		f = e->fence;

		hash_del(&e->node);
		kmem_cache_free(amdgpu_sync_slab, e);

		if (!dma_fence_is_signaled(f))
			return f;

		dma_fence_put(f);
	}
	return NULL;
}

 
int amdgpu_sync_clone(struct amdgpu_sync *source, struct amdgpu_sync *clone)
{
	struct amdgpu_sync_entry *e;
	struct hlist_node *tmp;
	struct dma_fence *f;
	int i, r;

	hash_for_each_safe(source->fences, i, tmp, e, node) {
		f = e->fence;
		if (!dma_fence_is_signaled(f)) {
			r = amdgpu_sync_fence(clone, f);
			if (r)
				return r;
		} else {
			amdgpu_sync_entry_free(e);
		}
	}

	return 0;
}

 
int amdgpu_sync_push_to_job(struct amdgpu_sync *sync, struct amdgpu_job *job)
{
	struct amdgpu_sync_entry *e;
	struct hlist_node *tmp;
	struct dma_fence *f;
	int i, r;

	hash_for_each_safe(sync->fences, i, tmp, e, node) {
		f = e->fence;
		if (dma_fence_is_signaled(f)) {
			amdgpu_sync_entry_free(e);
			continue;
		}

		dma_fence_get(f);
		r = drm_sched_job_add_dependency(&job->base, f);
		if (r) {
			dma_fence_put(f);
			return r;
		}
	}
	return 0;
}

int amdgpu_sync_wait(struct amdgpu_sync *sync, bool intr)
{
	struct amdgpu_sync_entry *e;
	struct hlist_node *tmp;
	int i, r;

	hash_for_each_safe(sync->fences, i, tmp, e, node) {
		r = dma_fence_wait(e->fence, intr);
		if (r)
			return r;

		amdgpu_sync_entry_free(e);
	}

	return 0;
}

 
void amdgpu_sync_free(struct amdgpu_sync *sync)
{
	struct amdgpu_sync_entry *e;
	struct hlist_node *tmp;
	unsigned int i;

	hash_for_each_safe(sync->fences, i, tmp, e, node)
		amdgpu_sync_entry_free(e);
}

 
int amdgpu_sync_init(void)
{
	amdgpu_sync_slab = kmem_cache_create(
		"amdgpu_sync", sizeof(struct amdgpu_sync_entry), 0,
		SLAB_HWCACHE_ALIGN, NULL);
	if (!amdgpu_sync_slab)
		return -ENOMEM;

	return 0;
}

 
void amdgpu_sync_fini(void)
{
	kmem_cache_destroy(amdgpu_sync_slab);
}
