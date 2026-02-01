
 

#include "msm_gem.h"
#include "a5xx_gpu.h"

 
static inline bool try_preempt_state(struct a5xx_gpu *a5xx_gpu,
		enum preempt_state old, enum preempt_state new)
{
	enum preempt_state cur = atomic_cmpxchg(&a5xx_gpu->preempt_state,
		old, new);

	return (cur == old);
}

 
static inline void set_preempt_state(struct a5xx_gpu *gpu,
		enum preempt_state new)
{
	 
	smp_mb__before_atomic();
	atomic_set(&gpu->preempt_state, new);
	 
	smp_mb__after_atomic();
}

 
static inline void update_wptr(struct msm_gpu *gpu, struct msm_ringbuffer *ring)
{
	unsigned long flags;
	uint32_t wptr;

	if (!ring)
		return;

	spin_lock_irqsave(&ring->preempt_lock, flags);
	wptr = get_wptr(ring);
	spin_unlock_irqrestore(&ring->preempt_lock, flags);

	gpu_write(gpu, REG_A5XX_CP_RB_WPTR, wptr);
}

 
static struct msm_ringbuffer *get_next_ring(struct msm_gpu *gpu)
{
	unsigned long flags;
	int i;

	for (i = 0; i < gpu->nr_rings; i++) {
		bool empty;
		struct msm_ringbuffer *ring = gpu->rb[i];

		spin_lock_irqsave(&ring->preempt_lock, flags);
		empty = (get_wptr(ring) == gpu->funcs->get_rptr(gpu, ring));
		spin_unlock_irqrestore(&ring->preempt_lock, flags);

		if (!empty)
			return ring;
	}

	return NULL;
}

static void a5xx_preempt_timer(struct timer_list *t)
{
	struct a5xx_gpu *a5xx_gpu = from_timer(a5xx_gpu, t, preempt_timer);
	struct msm_gpu *gpu = &a5xx_gpu->base.base;
	struct drm_device *dev = gpu->dev;

	if (!try_preempt_state(a5xx_gpu, PREEMPT_TRIGGERED, PREEMPT_FAULTED))
		return;

	DRM_DEV_ERROR(dev->dev, "%s: preemption timed out\n", gpu->name);
	kthread_queue_work(gpu->worker, &gpu->recover_work);
}

 
void a5xx_preempt_trigger(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	unsigned long flags;
	struct msm_ringbuffer *ring;

	if (gpu->nr_rings == 1)
		return;

	 
	if (!try_preempt_state(a5xx_gpu, PREEMPT_NONE, PREEMPT_START))
		return;

	 
	ring = get_next_ring(gpu);

	 
	if (!ring || (a5xx_gpu->cur_ring == ring)) {
		 

		set_preempt_state(a5xx_gpu, PREEMPT_ABORT);
		update_wptr(gpu, a5xx_gpu->cur_ring);
		set_preempt_state(a5xx_gpu, PREEMPT_NONE);
		return;
	}

	 
	spin_lock_irqsave(&ring->preempt_lock, flags);
	a5xx_gpu->preempt[ring->id]->wptr = get_wptr(ring);
	spin_unlock_irqrestore(&ring->preempt_lock, flags);

	 
	gpu_write64(gpu, REG_A5XX_CP_CONTEXT_SWITCH_RESTORE_ADDR_LO,
		a5xx_gpu->preempt_iova[ring->id]);

	a5xx_gpu->next_ring = ring;

	 
	mod_timer(&a5xx_gpu->preempt_timer, jiffies + msecs_to_jiffies(10000));

	 
	set_preempt_state(a5xx_gpu, PREEMPT_TRIGGERED);

	 
	wmb();

	 
	gpu_write(gpu, REG_A5XX_CP_CONTEXT_SWITCH_CNTL, 1);
}

void a5xx_preempt_irq(struct msm_gpu *gpu)
{
	uint32_t status;
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	struct drm_device *dev = gpu->dev;

	if (!try_preempt_state(a5xx_gpu, PREEMPT_TRIGGERED, PREEMPT_PENDING))
		return;

	 
	del_timer(&a5xx_gpu->preempt_timer);

	 
	status = gpu_read(gpu, REG_A5XX_CP_CONTEXT_SWITCH_CNTL);
	if (unlikely(status)) {
		set_preempt_state(a5xx_gpu, PREEMPT_FAULTED);
		DRM_DEV_ERROR(dev->dev, "%s: Preemption failed to complete\n",
			gpu->name);
		kthread_queue_work(gpu->worker, &gpu->recover_work);
		return;
	}

	a5xx_gpu->cur_ring = a5xx_gpu->next_ring;
	a5xx_gpu->next_ring = NULL;

	update_wptr(gpu, a5xx_gpu->cur_ring);

	set_preempt_state(a5xx_gpu, PREEMPT_NONE);
}

void a5xx_preempt_hw_init(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	int i;

	 
	a5xx_gpu->cur_ring = gpu->rb[0];

	 
	if (gpu->nr_rings == 1)
		return;

	for (i = 0; i < gpu->nr_rings; i++) {
		a5xx_gpu->preempt[i]->wptr = 0;
		a5xx_gpu->preempt[i]->rptr = 0;
		a5xx_gpu->preempt[i]->rbase = gpu->rb[i]->iova;
		a5xx_gpu->preempt[i]->rptr_addr = shadowptr(a5xx_gpu, gpu->rb[i]);
	}

	 
	gpu_write64(gpu, REG_A5XX_CP_CONTEXT_SWITCH_SMMU_INFO_LO, 0);

	 
	set_preempt_state(a5xx_gpu, PREEMPT_NONE);
}

static int preempt_init_ring(struct a5xx_gpu *a5xx_gpu,
		struct msm_ringbuffer *ring)
{
	struct adreno_gpu *adreno_gpu = &a5xx_gpu->base;
	struct msm_gpu *gpu = &adreno_gpu->base;
	struct a5xx_preempt_record *ptr;
	void *counters;
	struct drm_gem_object *bo = NULL, *counters_bo = NULL;
	u64 iova = 0, counters_iova = 0;

	ptr = msm_gem_kernel_new(gpu->dev,
		A5XX_PREEMPT_RECORD_SIZE + A5XX_PREEMPT_COUNTER_SIZE,
		MSM_BO_WC | MSM_BO_MAP_PRIV, gpu->aspace, &bo, &iova);

	if (IS_ERR(ptr))
		return PTR_ERR(ptr);

	 
	counters = msm_gem_kernel_new(gpu->dev,
		A5XX_PREEMPT_COUNTER_SIZE,
		MSM_BO_WC, gpu->aspace, &counters_bo, &counters_iova);
	if (IS_ERR(counters)) {
		msm_gem_kernel_put(bo, gpu->aspace);
		return PTR_ERR(counters);
	}

	msm_gem_object_set_name(bo, "preempt");
	msm_gem_object_set_name(counters_bo, "preempt_counters");

	a5xx_gpu->preempt_bo[ring->id] = bo;
	a5xx_gpu->preempt_counters_bo[ring->id] = counters_bo;
	a5xx_gpu->preempt_iova[ring->id] = iova;
	a5xx_gpu->preempt[ring->id] = ptr;

	 

	ptr->magic = A5XX_PREEMPT_RECORD_MAGIC;
	ptr->info = 0;
	ptr->data = 0;
	ptr->cntl = MSM_GPU_RB_CNTL_DEFAULT | AXXX_CP_RB_CNTL_NO_UPDATE;

	ptr->counter = counters_iova;

	return 0;
}

void a5xx_preempt_fini(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	int i;

	for (i = 0; i < gpu->nr_rings; i++) {
		msm_gem_kernel_put(a5xx_gpu->preempt_bo[i], gpu->aspace);
		msm_gem_kernel_put(a5xx_gpu->preempt_counters_bo[i], gpu->aspace);
	}
}

void a5xx_preempt_init(struct msm_gpu *gpu)
{
	struct adreno_gpu *adreno_gpu = to_adreno_gpu(gpu);
	struct a5xx_gpu *a5xx_gpu = to_a5xx_gpu(adreno_gpu);
	int i;

	 
	if (gpu->nr_rings <= 1)
		return;

	for (i = 0; i < gpu->nr_rings; i++) {
		if (preempt_init_ring(a5xx_gpu, gpu->rb[i])) {
			 
			a5xx_preempt_fini(gpu);
			gpu->nr_rings = 1;

			return;
		}
	}

	timer_setup(&a5xx_gpu->preempt_timer, a5xx_preempt_timer, 0);
}
