 
 
#include <linux/seq_file.h>
#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/kref.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/pm_runtime.h>

#include <drm/drm_drv.h>
#include "amdgpu.h"
#include "amdgpu_trace.h"
#include "amdgpu_reset.h"

 

struct amdgpu_fence {
	struct dma_fence base;

	 
	struct amdgpu_ring		*ring;
	ktime_t				start_timestamp;
};

static struct kmem_cache *amdgpu_fence_slab;

int amdgpu_fence_slab_init(void)
{
	amdgpu_fence_slab = kmem_cache_create(
		"amdgpu_fence", sizeof(struct amdgpu_fence), 0,
		SLAB_HWCACHE_ALIGN, NULL);
	if (!amdgpu_fence_slab)
		return -ENOMEM;
	return 0;
}

void amdgpu_fence_slab_fini(void)
{
	rcu_barrier();
	kmem_cache_destroy(amdgpu_fence_slab);
}
 
static const struct dma_fence_ops amdgpu_fence_ops;
static const struct dma_fence_ops amdgpu_job_fence_ops;
static inline struct amdgpu_fence *to_amdgpu_fence(struct dma_fence *f)
{
	struct amdgpu_fence *__f = container_of(f, struct amdgpu_fence, base);

	if (__f->base.ops == &amdgpu_fence_ops ||
	    __f->base.ops == &amdgpu_job_fence_ops)
		return __f;

	return NULL;
}

 
static void amdgpu_fence_write(struct amdgpu_ring *ring, u32 seq)
{
	struct amdgpu_fence_driver *drv = &ring->fence_drv;

	if (drv->cpu_addr)
		*drv->cpu_addr = cpu_to_le32(seq);
}

 
static u32 amdgpu_fence_read(struct amdgpu_ring *ring)
{
	struct amdgpu_fence_driver *drv = &ring->fence_drv;
	u32 seq = 0;

	if (drv->cpu_addr)
		seq = le32_to_cpu(*drv->cpu_addr);
	else
		seq = atomic_read(&drv->last_seq);

	return seq;
}

 
int amdgpu_fence_emit(struct amdgpu_ring *ring, struct dma_fence **f, struct amdgpu_job *job,
		      unsigned int flags)
{
	struct amdgpu_device *adev = ring->adev;
	struct dma_fence *fence;
	struct amdgpu_fence *am_fence;
	struct dma_fence __rcu **ptr;
	uint32_t seq;
	int r;

	if (job == NULL) {
		 
		am_fence = kmem_cache_alloc(amdgpu_fence_slab, GFP_ATOMIC);
		if (am_fence == NULL)
			return -ENOMEM;
		fence = &am_fence->base;
		am_fence->ring = ring;
	} else {
		 
		fence = &job->hw_fence;
	}

	seq = ++ring->fence_drv.sync_seq;
	if (job && job->job_run_counter) {
		 
		fence->seqno = seq;
		 
		dma_fence_get(fence);
	} else {
		if (job) {
			dma_fence_init(fence, &amdgpu_job_fence_ops,
				       &ring->fence_drv.lock,
				       adev->fence_context + ring->idx, seq);
			 
			dma_fence_get(fence);
		} else {
			dma_fence_init(fence, &amdgpu_fence_ops,
				       &ring->fence_drv.lock,
				       adev->fence_context + ring->idx, seq);
		}
	}

	amdgpu_ring_emit_fence(ring, ring->fence_drv.gpu_addr,
			       seq, flags | AMDGPU_FENCE_FLAG_INT);
	pm_runtime_get_noresume(adev_to_drm(adev)->dev);
	ptr = &ring->fence_drv.fences[seq & ring->fence_drv.num_fences_mask];
	if (unlikely(rcu_dereference_protected(*ptr, 1))) {
		struct dma_fence *old;

		rcu_read_lock();
		old = dma_fence_get_rcu_safe(ptr);
		rcu_read_unlock();

		if (old) {
			r = dma_fence_wait(old, false);
			dma_fence_put(old);
			if (r)
				return r;
		}
	}

	to_amdgpu_fence(fence)->start_timestamp = ktime_get();

	 
	rcu_assign_pointer(*ptr, dma_fence_get(fence));

	*f = fence;

	return 0;
}

 
int amdgpu_fence_emit_polling(struct amdgpu_ring *ring, uint32_t *s,
			      uint32_t timeout)
{
	uint32_t seq;
	signed long r;

	if (!s)
		return -EINVAL;

	seq = ++ring->fence_drv.sync_seq;
	r = amdgpu_fence_wait_polling(ring,
				      seq - ring->fence_drv.num_fences_mask,
				      timeout);
	if (r < 1)
		return -ETIMEDOUT;

	amdgpu_ring_emit_fence(ring, ring->fence_drv.gpu_addr,
			       seq, 0);

	*s = seq;

	return 0;
}

 
static void amdgpu_fence_schedule_fallback(struct amdgpu_ring *ring)
{
	mod_timer(&ring->fence_drv.fallback_timer,
		  jiffies + AMDGPU_FENCE_JIFFIES_TIMEOUT);
}

 
bool amdgpu_fence_process(struct amdgpu_ring *ring)
{
	struct amdgpu_fence_driver *drv = &ring->fence_drv;
	struct amdgpu_device *adev = ring->adev;
	uint32_t seq, last_seq;

	do {
		last_seq = atomic_read(&ring->fence_drv.last_seq);
		seq = amdgpu_fence_read(ring);

	} while (atomic_cmpxchg(&drv->last_seq, last_seq, seq) != last_seq);

	if (del_timer(&ring->fence_drv.fallback_timer) &&
	    seq != ring->fence_drv.sync_seq)
		amdgpu_fence_schedule_fallback(ring);

	if (unlikely(seq == last_seq))
		return false;

	last_seq &= drv->num_fences_mask;
	seq &= drv->num_fences_mask;

	do {
		struct dma_fence *fence, **ptr;

		++last_seq;
		last_seq &= drv->num_fences_mask;
		ptr = &drv->fences[last_seq];

		 
		fence = rcu_dereference_protected(*ptr, 1);
		RCU_INIT_POINTER(*ptr, NULL);

		if (!fence)
			continue;

		dma_fence_signal(fence);
		dma_fence_put(fence);
		pm_runtime_mark_last_busy(adev_to_drm(adev)->dev);
		pm_runtime_put_autosuspend(adev_to_drm(adev)->dev);
	} while (last_seq != seq);

	return true;
}

 
static void amdgpu_fence_fallback(struct timer_list *t)
{
	struct amdgpu_ring *ring = from_timer(ring, t,
					      fence_drv.fallback_timer);

	if (amdgpu_fence_process(ring))
		DRM_WARN("Fence fallback timer expired on ring %s\n", ring->name);
}

 
int amdgpu_fence_wait_empty(struct amdgpu_ring *ring)
{
	uint64_t seq = READ_ONCE(ring->fence_drv.sync_seq);
	struct dma_fence *fence, **ptr;
	int r;

	if (!seq)
		return 0;

	ptr = &ring->fence_drv.fences[seq & ring->fence_drv.num_fences_mask];
	rcu_read_lock();
	fence = rcu_dereference(*ptr);
	if (!fence || !dma_fence_get_rcu(fence)) {
		rcu_read_unlock();
		return 0;
	}
	rcu_read_unlock();

	r = dma_fence_wait(fence, false);
	dma_fence_put(fence);
	return r;
}

 
signed long amdgpu_fence_wait_polling(struct amdgpu_ring *ring,
				      uint32_t wait_seq,
				      signed long timeout)
{

	while ((int32_t)(wait_seq - amdgpu_fence_read(ring)) > 0 && timeout > 0) {
		udelay(2);
		timeout -= 2;
	}
	return timeout > 0 ? timeout : 0;
}
 
unsigned int amdgpu_fence_count_emitted(struct amdgpu_ring *ring)
{
	uint64_t emitted;

	 
	emitted = 0x100000000ull;
	emitted -= atomic_read(&ring->fence_drv.last_seq);
	emitted += READ_ONCE(ring->fence_drv.sync_seq);
	return lower_32_bits(emitted);
}

 
u64 amdgpu_fence_last_unsignaled_time_us(struct amdgpu_ring *ring)
{
	struct amdgpu_fence_driver *drv = &ring->fence_drv;
	struct dma_fence *fence;
	uint32_t last_seq, sync_seq;

	last_seq = atomic_read(&ring->fence_drv.last_seq);
	sync_seq = READ_ONCE(ring->fence_drv.sync_seq);
	if (last_seq == sync_seq)
		return 0;

	++last_seq;
	last_seq &= drv->num_fences_mask;
	fence = drv->fences[last_seq];
	if (!fence)
		return 0;

	return ktime_us_delta(ktime_get(),
		to_amdgpu_fence(fence)->start_timestamp);
}

 
void amdgpu_fence_update_start_timestamp(struct amdgpu_ring *ring, uint32_t seq, ktime_t timestamp)
{
	struct amdgpu_fence_driver *drv = &ring->fence_drv;
	struct dma_fence *fence;

	seq &= drv->num_fences_mask;
	fence = drv->fences[seq];
	if (!fence)
		return;

	to_amdgpu_fence(fence)->start_timestamp = timestamp;
}

 
int amdgpu_fence_driver_start_ring(struct amdgpu_ring *ring,
				   struct amdgpu_irq_src *irq_src,
				   unsigned int irq_type)
{
	struct amdgpu_device *adev = ring->adev;
	uint64_t index;

	if (ring->funcs->type != AMDGPU_RING_TYPE_UVD) {
		ring->fence_drv.cpu_addr = ring->fence_cpu_addr;
		ring->fence_drv.gpu_addr = ring->fence_gpu_addr;
	} else {
		 
		index = ALIGN(adev->uvd.fw->size, 8);
		ring->fence_drv.cpu_addr = adev->uvd.inst[ring->me].cpu_addr + index;
		ring->fence_drv.gpu_addr = adev->uvd.inst[ring->me].gpu_addr + index;
	}
	amdgpu_fence_write(ring, atomic_read(&ring->fence_drv.last_seq));

	ring->fence_drv.irq_src = irq_src;
	ring->fence_drv.irq_type = irq_type;
	ring->fence_drv.initialized = true;

	DRM_DEV_DEBUG(adev->dev, "fence driver on ring %s use gpu addr 0x%016llx\n",
		      ring->name, ring->fence_drv.gpu_addr);
	return 0;
}

 
int amdgpu_fence_driver_init_ring(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;

	if (!adev)
		return -EINVAL;

	if (!is_power_of_2(ring->num_hw_submission))
		return -EINVAL;

	ring->fence_drv.cpu_addr = NULL;
	ring->fence_drv.gpu_addr = 0;
	ring->fence_drv.sync_seq = 0;
	atomic_set(&ring->fence_drv.last_seq, 0);
	ring->fence_drv.initialized = false;

	timer_setup(&ring->fence_drv.fallback_timer, amdgpu_fence_fallback, 0);

	ring->fence_drv.num_fences_mask = ring->num_hw_submission * 2 - 1;
	spin_lock_init(&ring->fence_drv.lock);
	ring->fence_drv.fences = kcalloc(ring->num_hw_submission * 2, sizeof(void *),
					 GFP_KERNEL);

	if (!ring->fence_drv.fences)
		return -ENOMEM;

	return 0;
}

 
int amdgpu_fence_driver_sw_init(struct amdgpu_device *adev)
{
	return 0;
}

 
static bool amdgpu_fence_need_ring_interrupt_restore(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;
	bool is_gfx_power_domain = false;

	switch (ring->funcs->type) {
	case AMDGPU_RING_TYPE_SDMA:
	 
		if (adev->ip_versions[SDMA0_HWIP][0] >= IP_VERSION(5, 0, 0))
			is_gfx_power_domain = true;
		break;
	case AMDGPU_RING_TYPE_GFX:
	case AMDGPU_RING_TYPE_COMPUTE:
	case AMDGPU_RING_TYPE_KIQ:
	case AMDGPU_RING_TYPE_MES:
		is_gfx_power_domain = true;
		break;
	default:
		break;
	}

	return !(adev->in_s0ix && is_gfx_power_domain);
}

 
void amdgpu_fence_driver_hw_fini(struct amdgpu_device *adev)
{
	int i, r;

	for (i = 0; i < AMDGPU_MAX_RINGS; i++) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (!ring || !ring->fence_drv.initialized)
			continue;

		 
		if (!drm_dev_is_unplugged(adev_to_drm(adev)))
			r = amdgpu_fence_wait_empty(ring);
		else
			r = -ENODEV;
		 
		if (r)
			amdgpu_fence_driver_force_completion(ring);

		if (!drm_dev_is_unplugged(adev_to_drm(adev)) &&
		    ring->fence_drv.irq_src &&
		    amdgpu_fence_need_ring_interrupt_restore(ring))
			amdgpu_irq_put(adev, ring->fence_drv.irq_src,
				       ring->fence_drv.irq_type);

		del_timer_sync(&ring->fence_drv.fallback_timer);
	}
}

 
void amdgpu_fence_driver_isr_toggle(struct amdgpu_device *adev, bool stop)
{
	int i;

	for (i = 0; i < AMDGPU_MAX_RINGS; i++) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (!ring || !ring->fence_drv.initialized || !ring->fence_drv.irq_src)
			continue;

		if (stop)
			disable_irq(adev->irq.irq);
		else
			enable_irq(adev->irq.irq);
	}
}

void amdgpu_fence_driver_sw_fini(struct amdgpu_device *adev)
{
	unsigned int i, j;

	for (i = 0; i < AMDGPU_MAX_RINGS; i++) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (!ring || !ring->fence_drv.initialized)
			continue;

		 
		if (ring->sched.ops)
			drm_sched_fini(&ring->sched);

		for (j = 0; j <= ring->fence_drv.num_fences_mask; ++j)
			dma_fence_put(ring->fence_drv.fences[j]);
		kfree(ring->fence_drv.fences);
		ring->fence_drv.fences = NULL;
		ring->fence_drv.initialized = false;
	}
}

 
void amdgpu_fence_driver_hw_init(struct amdgpu_device *adev)
{
	int i;

	for (i = 0; i < AMDGPU_MAX_RINGS; i++) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (!ring || !ring->fence_drv.initialized)
			continue;

		 
		if (ring->fence_drv.irq_src &&
		    amdgpu_fence_need_ring_interrupt_restore(ring))
			amdgpu_irq_get(adev, ring->fence_drv.irq_src,
				       ring->fence_drv.irq_type);
	}
}

 
void amdgpu_fence_driver_clear_job_fences(struct amdgpu_ring *ring)
{
	int i;
	struct dma_fence *old, **ptr;

	for (i = 0; i <= ring->fence_drv.num_fences_mask; i++) {
		ptr = &ring->fence_drv.fences[i];
		old = rcu_dereference_protected(*ptr, 1);
		if (old && old->ops == &amdgpu_job_fence_ops) {
			struct amdgpu_job *job;

			 
			job = container_of(old, struct amdgpu_job, hw_fence);
			if (!job->base.s_fence && !dma_fence_is_signaled(old))
				dma_fence_signal(old);
			RCU_INIT_POINTER(*ptr, NULL);
			dma_fence_put(old);
		}
	}
}

 
void amdgpu_fence_driver_set_error(struct amdgpu_ring *ring, int error)
{
	struct amdgpu_fence_driver *drv = &ring->fence_drv;
	unsigned long flags;

	spin_lock_irqsave(&drv->lock, flags);
	for (unsigned int i = 0; i <= drv->num_fences_mask; ++i) {
		struct dma_fence *fence;

		fence = rcu_dereference_protected(drv->fences[i],
						  lockdep_is_held(&drv->lock));
		if (fence && !dma_fence_is_signaled_locked(fence))
			dma_fence_set_error(fence, error);
	}
	spin_unlock_irqrestore(&drv->lock, flags);
}

 
void amdgpu_fence_driver_force_completion(struct amdgpu_ring *ring)
{
	amdgpu_fence_driver_set_error(ring, -ECANCELED);
	amdgpu_fence_write(ring, ring->fence_drv.sync_seq);
	amdgpu_fence_process(ring);
}

 

static const char *amdgpu_fence_get_driver_name(struct dma_fence *fence)
{
	return "amdgpu";
}

static const char *amdgpu_fence_get_timeline_name(struct dma_fence *f)
{
	return (const char *)to_amdgpu_fence(f)->ring->name;
}

static const char *amdgpu_job_fence_get_timeline_name(struct dma_fence *f)
{
	struct amdgpu_job *job = container_of(f, struct amdgpu_job, hw_fence);

	return (const char *)to_amdgpu_ring(job->base.sched)->name;
}

 
static bool amdgpu_fence_enable_signaling(struct dma_fence *f)
{
	if (!timer_pending(&to_amdgpu_fence(f)->ring->fence_drv.fallback_timer))
		amdgpu_fence_schedule_fallback(to_amdgpu_fence(f)->ring);

	return true;
}

 
static bool amdgpu_job_fence_enable_signaling(struct dma_fence *f)
{
	struct amdgpu_job *job = container_of(f, struct amdgpu_job, hw_fence);

	if (!timer_pending(&to_amdgpu_ring(job->base.sched)->fence_drv.fallback_timer))
		amdgpu_fence_schedule_fallback(to_amdgpu_ring(job->base.sched));

	return true;
}

 
static void amdgpu_fence_free(struct rcu_head *rcu)
{
	struct dma_fence *f = container_of(rcu, struct dma_fence, rcu);

	 
	kmem_cache_free(amdgpu_fence_slab, to_amdgpu_fence(f));
}

 
static void amdgpu_job_fence_free(struct rcu_head *rcu)
{
	struct dma_fence *f = container_of(rcu, struct dma_fence, rcu);

	 
	kfree(container_of(f, struct amdgpu_job, hw_fence));
}

 
static void amdgpu_fence_release(struct dma_fence *f)
{
	call_rcu(&f->rcu, amdgpu_fence_free);
}

 
static void amdgpu_job_fence_release(struct dma_fence *f)
{
	call_rcu(&f->rcu, amdgpu_job_fence_free);
}

static const struct dma_fence_ops amdgpu_fence_ops = {
	.get_driver_name = amdgpu_fence_get_driver_name,
	.get_timeline_name = amdgpu_fence_get_timeline_name,
	.enable_signaling = amdgpu_fence_enable_signaling,
	.release = amdgpu_fence_release,
};

static const struct dma_fence_ops amdgpu_job_fence_ops = {
	.get_driver_name = amdgpu_fence_get_driver_name,
	.get_timeline_name = amdgpu_job_fence_get_timeline_name,
	.enable_signaling = amdgpu_job_fence_enable_signaling,
	.release = amdgpu_job_fence_release,
};

 
#if defined(CONFIG_DEBUG_FS)
static int amdgpu_debugfs_fence_info_show(struct seq_file *m, void *unused)
{
	struct amdgpu_device *adev = m->private;
	int i;

	for (i = 0; i < AMDGPU_MAX_RINGS; ++i) {
		struct amdgpu_ring *ring = adev->rings[i];

		if (!ring || !ring->fence_drv.initialized)
			continue;

		amdgpu_fence_process(ring);

		seq_printf(m, "--- ring %d (%s) ---\n", i, ring->name);
		seq_printf(m, "Last signaled fence          0x%08x\n",
			   atomic_read(&ring->fence_drv.last_seq));
		seq_printf(m, "Last emitted                 0x%08x\n",
			   ring->fence_drv.sync_seq);

		if (ring->funcs->type == AMDGPU_RING_TYPE_GFX ||
		    ring->funcs->type == AMDGPU_RING_TYPE_SDMA) {
			seq_printf(m, "Last signaled trailing fence 0x%08x\n",
				   le32_to_cpu(*ring->trail_fence_cpu_addr));
			seq_printf(m, "Last emitted                 0x%08x\n",
				   ring->trail_seq);
		}

		if (ring->funcs->type != AMDGPU_RING_TYPE_GFX)
			continue;

		 
		seq_printf(m, "Last preempted               0x%08x\n",
			   le32_to_cpu(*(ring->fence_drv.cpu_addr + 2)));
		 
		seq_printf(m, "Last reset                   0x%08x\n",
			   le32_to_cpu(*(ring->fence_drv.cpu_addr + 4)));
		 
		seq_printf(m, "Last both                    0x%08x\n",
			   le32_to_cpu(*(ring->fence_drv.cpu_addr + 6)));
	}
	return 0;
}

 
static int gpu_recover_get(void *data, u64 *val)
{
	struct amdgpu_device *adev = (struct amdgpu_device *)data;
	struct drm_device *dev = adev_to_drm(adev);
	int r;

	r = pm_runtime_get_sync(dev->dev);
	if (r < 0) {
		pm_runtime_put_autosuspend(dev->dev);
		return 0;
	}

	if (amdgpu_reset_domain_schedule(adev->reset_domain, &adev->reset_work))
		flush_work(&adev->reset_work);

	*val = atomic_read(&adev->reset_domain->reset_res);

	pm_runtime_mark_last_busy(dev->dev);
	pm_runtime_put_autosuspend(dev->dev);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(amdgpu_debugfs_fence_info);
DEFINE_DEBUGFS_ATTRIBUTE(amdgpu_debugfs_gpu_recover_fops, gpu_recover_get, NULL,
			 "%lld\n");

static void amdgpu_debugfs_reset_work(struct work_struct *work)
{
	struct amdgpu_device *adev = container_of(work, struct amdgpu_device,
						  reset_work);

	struct amdgpu_reset_context reset_context;

	memset(&reset_context, 0, sizeof(reset_context));

	reset_context.method = AMD_RESET_METHOD_NONE;
	reset_context.reset_req_dev = adev;
	set_bit(AMDGPU_NEED_FULL_RESET, &reset_context.flags);

	amdgpu_device_gpu_recover(adev, NULL, &reset_context);
}

#endif

void amdgpu_debugfs_fence_init(struct amdgpu_device *adev)
{
#if defined(CONFIG_DEBUG_FS)
	struct drm_minor *minor = adev_to_drm(adev)->primary;
	struct dentry *root = minor->debugfs_root;

	debugfs_create_file("amdgpu_fence_info", 0444, root, adev,
			    &amdgpu_debugfs_fence_info_fops);

	if (!amdgpu_sriov_vf(adev)) {

		INIT_WORK(&adev->reset_work, amdgpu_debugfs_reset_work);
		debugfs_create_file("amdgpu_gpu_recover", 0444, root, adev,
				    &amdgpu_debugfs_gpu_recover_fops);
	}
#endif
}

