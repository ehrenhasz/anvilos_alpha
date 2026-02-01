 
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

#include <drm/amdgpu_drm.h>
#include "amdgpu.h"
#include "atom.h"

 

 
unsigned int amdgpu_ring_max_ibs(enum amdgpu_ring_type type)
{
	switch (type) {
	case AMDGPU_RING_TYPE_GFX:
		 
		return 192;
	case AMDGPU_RING_TYPE_COMPUTE:
		return 125;
	case AMDGPU_RING_TYPE_VCN_JPEG:
		return 16;
	default:
		return 49;
	}
}

 
int amdgpu_ring_alloc(struct amdgpu_ring *ring, unsigned int ndw)
{
	 
	ndw = (ndw + ring->funcs->align_mask) & ~ring->funcs->align_mask;

	 
	if (WARN_ON_ONCE(ndw > ring->max_dw))
		return -ENOMEM;

	ring->count_dw = ndw;
	ring->wptr_old = ring->wptr;

	if (ring->funcs->begin_use)
		ring->funcs->begin_use(ring);

	return 0;
}

 
void amdgpu_ring_insert_nop(struct amdgpu_ring *ring, uint32_t count)
{
	int i;

	for (i = 0; i < count; i++)
		amdgpu_ring_write(ring, ring->funcs->nop);
}

 
void amdgpu_ring_generic_pad_ib(struct amdgpu_ring *ring, struct amdgpu_ib *ib)
{
	while (ib->length_dw & ring->funcs->align_mask)
		ib->ptr[ib->length_dw++] = ring->funcs->nop;
}

 
void amdgpu_ring_commit(struct amdgpu_ring *ring)
{
	uint32_t count;

	 
	count = ring->funcs->align_mask + 1 -
		(ring->wptr & ring->funcs->align_mask);
	count %= ring->funcs->align_mask + 1;
	ring->funcs->insert_nop(ring, count);

	mb();
	amdgpu_ring_set_wptr(ring);

	if (ring->funcs->end_use)
		ring->funcs->end_use(ring);
}

 
void amdgpu_ring_undo(struct amdgpu_ring *ring)
{
	ring->wptr = ring->wptr_old;

	if (ring->funcs->end_use)
		ring->funcs->end_use(ring);
}

#define amdgpu_ring_get_gpu_addr(ring, offset)				\
	(ring->is_mes_queue ?						\
	 (ring->mes_ctx->meta_data_gpu_addr + offset) :			\
	 (ring->adev->wb.gpu_addr + offset * 4))

#define amdgpu_ring_get_cpu_addr(ring, offset)				\
	(ring->is_mes_queue ?						\
	 (void *)((uint8_t *)(ring->mes_ctx->meta_data_ptr) + offset) : \
	 (&ring->adev->wb.wb[offset]))

 
int amdgpu_ring_init(struct amdgpu_device *adev, struct amdgpu_ring *ring,
		     unsigned int max_dw, struct amdgpu_irq_src *irq_src,
		     unsigned int irq_type, unsigned int hw_prio,
		     atomic_t *sched_score)
{
	int r;
	int sched_hw_submission = amdgpu_sched_hw_submission;
	u32 *num_sched;
	u32 hw_ip;
	unsigned int max_ibs_dw;

	 
	if (ring->funcs->type == AMDGPU_RING_TYPE_KIQ)
		sched_hw_submission = max(sched_hw_submission, 256);
	else if (ring == &adev->sdma.instance[0].page)
		sched_hw_submission = 256;

	if (ring->adev == NULL) {
		if (adev->num_rings >= AMDGPU_MAX_RINGS)
			return -EINVAL;

		ring->adev = adev;
		ring->num_hw_submission = sched_hw_submission;
		ring->sched_score = sched_score;
		ring->vmid_wait = dma_fence_get_stub();

		if (!ring->is_mes_queue) {
			ring->idx = adev->num_rings++;
			adev->rings[ring->idx] = ring;
		}

		r = amdgpu_fence_driver_init_ring(ring);
		if (r)
			return r;
	}

	if (ring->is_mes_queue) {
		ring->rptr_offs = amdgpu_mes_ctx_get_offs(ring,
				AMDGPU_MES_CTX_RPTR_OFFS);
		ring->wptr_offs = amdgpu_mes_ctx_get_offs(ring,
				AMDGPU_MES_CTX_WPTR_OFFS);
		ring->fence_offs = amdgpu_mes_ctx_get_offs(ring,
				AMDGPU_MES_CTX_FENCE_OFFS);
		ring->trail_fence_offs = amdgpu_mes_ctx_get_offs(ring,
				AMDGPU_MES_CTX_TRAIL_FENCE_OFFS);
		ring->cond_exe_offs = amdgpu_mes_ctx_get_offs(ring,
				AMDGPU_MES_CTX_COND_EXE_OFFS);
	} else {
		r = amdgpu_device_wb_get(adev, &ring->rptr_offs);
		if (r) {
			dev_err(adev->dev, "(%d) ring rptr_offs wb alloc failed\n", r);
			return r;
		}

		r = amdgpu_device_wb_get(adev, &ring->wptr_offs);
		if (r) {
			dev_err(adev->dev, "(%d) ring wptr_offs wb alloc failed\n", r);
			return r;
		}

		r = amdgpu_device_wb_get(adev, &ring->fence_offs);
		if (r) {
			dev_err(adev->dev, "(%d) ring fence_offs wb alloc failed\n", r);
			return r;
		}

		r = amdgpu_device_wb_get(adev, &ring->trail_fence_offs);
		if (r) {
			dev_err(adev->dev, "(%d) ring trail_fence_offs wb alloc failed\n", r);
			return r;
		}

		r = amdgpu_device_wb_get(adev, &ring->cond_exe_offs);
		if (r) {
			dev_err(adev->dev, "(%d) ring cond_exec_polling wb alloc failed\n", r);
			return r;
		}
	}

	ring->fence_gpu_addr =
		amdgpu_ring_get_gpu_addr(ring, ring->fence_offs);
	ring->fence_cpu_addr =
		amdgpu_ring_get_cpu_addr(ring, ring->fence_offs);

	ring->rptr_gpu_addr =
		amdgpu_ring_get_gpu_addr(ring, ring->rptr_offs);
	ring->rptr_cpu_addr =
		amdgpu_ring_get_cpu_addr(ring, ring->rptr_offs);

	ring->wptr_gpu_addr =
		amdgpu_ring_get_gpu_addr(ring, ring->wptr_offs);
	ring->wptr_cpu_addr =
		amdgpu_ring_get_cpu_addr(ring, ring->wptr_offs);

	ring->trail_fence_gpu_addr =
		amdgpu_ring_get_gpu_addr(ring, ring->trail_fence_offs);
	ring->trail_fence_cpu_addr =
		amdgpu_ring_get_cpu_addr(ring, ring->trail_fence_offs);

	ring->cond_exe_gpu_addr =
		amdgpu_ring_get_gpu_addr(ring, ring->cond_exe_offs);
	ring->cond_exe_cpu_addr =
		amdgpu_ring_get_cpu_addr(ring, ring->cond_exe_offs);

	 
	*ring->cond_exe_cpu_addr = 1;

	r = amdgpu_fence_driver_start_ring(ring, irq_src, irq_type);
	if (r) {
		dev_err(adev->dev, "failed initializing fences (%d).\n", r);
		return r;
	}

	max_ibs_dw = ring->funcs->emit_frame_size +
		     amdgpu_ring_max_ibs(ring->funcs->type) * ring->funcs->emit_ib_size;
	max_ibs_dw = (max_ibs_dw + ring->funcs->align_mask) & ~ring->funcs->align_mask;

	if (WARN_ON(max_ibs_dw > max_dw))
		max_dw = max_ibs_dw;

	ring->ring_size = roundup_pow_of_two(max_dw * 4 * sched_hw_submission);

	ring->buf_mask = (ring->ring_size / 4) - 1;
	ring->ptr_mask = ring->funcs->support_64bit_ptrs ?
		0xffffffffffffffff : ring->buf_mask;

	 
	if (ring->is_mes_queue) {
		int offset = 0;

		BUG_ON(ring->ring_size > PAGE_SIZE*4);

		offset = amdgpu_mes_ctx_get_offs(ring,
					 AMDGPU_MES_CTX_RING_OFFS);
		ring->gpu_addr = amdgpu_mes_ctx_get_offs_gpu_addr(ring, offset);
		ring->ring = amdgpu_mes_ctx_get_offs_cpu_addr(ring, offset);
		amdgpu_ring_clear_ring(ring);

	} else if (ring->ring_obj == NULL) {
		r = amdgpu_bo_create_kernel(adev, ring->ring_size + ring->funcs->extra_dw, PAGE_SIZE,
					    AMDGPU_GEM_DOMAIN_GTT,
					    &ring->ring_obj,
					    &ring->gpu_addr,
					    (void **)&ring->ring);
		if (r) {
			dev_err(adev->dev, "(%d) ring create failed\n", r);
			return r;
		}
		amdgpu_ring_clear_ring(ring);
	}

	ring->max_dw = max_dw;
	ring->hw_prio = hw_prio;

	if (!ring->no_scheduler) {
		hw_ip = ring->funcs->type;
		num_sched = &adev->gpu_sched[hw_ip][hw_prio].num_scheds;
		adev->gpu_sched[hw_ip][hw_prio].sched[(*num_sched)++] =
			&ring->sched;
	}

	return 0;
}

 
void amdgpu_ring_fini(struct amdgpu_ring *ring)
{

	 
	if (!(ring->adev) ||
	    (!ring->is_mes_queue && !(ring->adev->rings[ring->idx])))
		return;

	ring->sched.ready = false;

	if (!ring->is_mes_queue) {
		amdgpu_device_wb_free(ring->adev, ring->rptr_offs);
		amdgpu_device_wb_free(ring->adev, ring->wptr_offs);

		amdgpu_device_wb_free(ring->adev, ring->cond_exe_offs);
		amdgpu_device_wb_free(ring->adev, ring->fence_offs);

		amdgpu_bo_free_kernel(&ring->ring_obj,
				      &ring->gpu_addr,
				      (void **)&ring->ring);
	} else {
		kfree(ring->fence_drv.fences);
	}

	dma_fence_put(ring->vmid_wait);
	ring->vmid_wait = NULL;
	ring->me = 0;

	if (!ring->is_mes_queue)
		ring->adev->rings[ring->idx] = NULL;
}

 
void amdgpu_ring_emit_reg_write_reg_wait_helper(struct amdgpu_ring *ring,
						uint32_t reg0, uint32_t reg1,
						uint32_t ref, uint32_t mask)
{
	amdgpu_ring_emit_wreg(ring, reg0, ref);
	amdgpu_ring_emit_reg_wait(ring, reg1, mask, mask);
}

 
bool amdgpu_ring_soft_recovery(struct amdgpu_ring *ring, unsigned int vmid,
			       struct dma_fence *fence)
{
	unsigned long flags;

	ktime_t deadline = ktime_add_us(ktime_get(), 10000);

	if (amdgpu_sriov_vf(ring->adev) || !ring->funcs->soft_recovery || !fence)
		return false;

	spin_lock_irqsave(fence->lock, flags);
	if (!dma_fence_is_signaled_locked(fence))
		dma_fence_set_error(fence, -ENODATA);
	spin_unlock_irqrestore(fence->lock, flags);

	atomic_inc(&ring->adev->gpu_reset_counter);
	while (!dma_fence_is_signaled(fence) &&
	       ktime_to_ns(ktime_sub(deadline, ktime_get())) > 0)
		ring->funcs->soft_recovery(ring, vmid);

	return dma_fence_is_signaled(fence);
}

 
#if defined(CONFIG_DEBUG_FS)

 
static ssize_t amdgpu_debugfs_ring_read(struct file *f, char __user *buf,
					size_t size, loff_t *pos)
{
	struct amdgpu_ring *ring = file_inode(f)->i_private;
	int r, i;
	uint32_t value, result, early[3];

	if (*pos & 3 || size & 3)
		return -EINVAL;

	result = 0;

	if (*pos < 12) {
		early[0] = amdgpu_ring_get_rptr(ring) & ring->buf_mask;
		early[1] = amdgpu_ring_get_wptr(ring) & ring->buf_mask;
		early[2] = ring->wptr & ring->buf_mask;
		for (i = *pos / 4; i < 3 && size; i++) {
			r = put_user(early[i], (uint32_t *)buf);
			if (r)
				return r;
			buf += 4;
			result += 4;
			size -= 4;
			*pos += 4;
		}
	}

	while (size) {
		if (*pos >= (ring->ring_size + 12))
			return result;

		value = ring->ring[(*pos - 12)/4];
		r = put_user(value, (uint32_t *)buf);
		if (r)
			return r;
		buf += 4;
		result += 4;
		size -= 4;
		*pos += 4;
	}

	return result;
}

static const struct file_operations amdgpu_debugfs_ring_fops = {
	.owner = THIS_MODULE,
	.read = amdgpu_debugfs_ring_read,
	.llseek = default_llseek
};

static ssize_t amdgpu_debugfs_mqd_read(struct file *f, char __user *buf,
				       size_t size, loff_t *pos)
{
	struct amdgpu_ring *ring = file_inode(f)->i_private;
	volatile u32 *mqd;
	int r;
	uint32_t value, result;

	if (*pos & 3 || size & 3)
		return -EINVAL;

	result = 0;

	r = amdgpu_bo_reserve(ring->mqd_obj, false);
	if (unlikely(r != 0))
		return r;

	r = amdgpu_bo_kmap(ring->mqd_obj, (void **)&mqd);
	if (r) {
		amdgpu_bo_unreserve(ring->mqd_obj);
		return r;
	}

	while (size) {
		if (*pos >= ring->mqd_size)
			goto done;

		value = mqd[*pos/4];
		r = put_user(value, (uint32_t *)buf);
		if (r)
			goto done;
		buf += 4;
		result += 4;
		size -= 4;
		*pos += 4;
	}

done:
	amdgpu_bo_kunmap(ring->mqd_obj);
	mqd = NULL;
	amdgpu_bo_unreserve(ring->mqd_obj);
	if (r)
		return r;

	return result;
}

static const struct file_operations amdgpu_debugfs_mqd_fops = {
	.owner = THIS_MODULE,
	.read = amdgpu_debugfs_mqd_read,
	.llseek = default_llseek
};

static int amdgpu_debugfs_ring_error(void *data, u64 val)
{
	struct amdgpu_ring *ring = data;

	amdgpu_fence_driver_set_error(ring, val);
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE_SIGNED(amdgpu_debugfs_error_fops, NULL,
				amdgpu_debugfs_ring_error, "%lld\n");

#endif

void amdgpu_debugfs_ring_init(struct amdgpu_device *adev,
			      struct amdgpu_ring *ring)
{
#if defined(CONFIG_DEBUG_FS)
	struct drm_minor *minor = adev_to_drm(adev)->primary;
	struct dentry *root = minor->debugfs_root;
	char name[32];

	sprintf(name, "amdgpu_ring_%s", ring->name);
	debugfs_create_file_size(name, S_IFREG | 0444, root, ring,
				 &amdgpu_debugfs_ring_fops,
				 ring->ring_size + 12);

	if (ring->mqd_obj) {
		sprintf(name, "amdgpu_mqd_%s", ring->name);
		debugfs_create_file_size(name, S_IFREG | 0444, root, ring,
					 &amdgpu_debugfs_mqd_fops,
					 ring->mqd_size);
	}

	sprintf(name, "amdgpu_error_%s", ring->name);
	debugfs_create_file(name, 0200, root, ring,
			    &amdgpu_debugfs_error_fops);

#endif
}

 
int amdgpu_ring_test_helper(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;
	int r;

	r = amdgpu_ring_test_ring(ring);
	if (r)
		DRM_DEV_ERROR(adev->dev, "ring %s test failed (%d)\n",
			      ring->name, r);
	else
		DRM_DEV_DEBUG(adev->dev, "ring test on %s succeeded\n",
			      ring->name);

	ring->sched.ready = !r;
	return r;
}

static void amdgpu_ring_to_mqd_prop(struct amdgpu_ring *ring,
				    struct amdgpu_mqd_prop *prop)
{
	struct amdgpu_device *adev = ring->adev;

	memset(prop, 0, sizeof(*prop));

	prop->mqd_gpu_addr = ring->mqd_gpu_addr;
	prop->hqd_base_gpu_addr = ring->gpu_addr;
	prop->rptr_gpu_addr = ring->rptr_gpu_addr;
	prop->wptr_gpu_addr = ring->wptr_gpu_addr;
	prop->queue_size = ring->ring_size;
	prop->eop_gpu_addr = ring->eop_gpu_addr;
	prop->use_doorbell = ring->use_doorbell;
	prop->doorbell_index = ring->doorbell_index;

	 
	prop->hqd_active = ring->funcs->type == AMDGPU_RING_TYPE_KIQ;

	if ((ring->funcs->type == AMDGPU_RING_TYPE_COMPUTE &&
	     amdgpu_gfx_is_high_priority_compute_queue(adev, ring)) ||
	    (ring->funcs->type == AMDGPU_RING_TYPE_GFX &&
	     amdgpu_gfx_is_high_priority_graphics_queue(adev, ring))) {
		prop->hqd_pipe_priority = AMDGPU_GFX_PIPE_PRIO_HIGH;
		prop->hqd_queue_priority = AMDGPU_GFX_QUEUE_PRIORITY_MAXIMUM;
	}
}

int amdgpu_ring_init_mqd(struct amdgpu_ring *ring)
{
	struct amdgpu_device *adev = ring->adev;
	struct amdgpu_mqd *mqd_mgr;
	struct amdgpu_mqd_prop prop;

	amdgpu_ring_to_mqd_prop(ring, &prop);

	ring->wptr = 0;

	if (ring->funcs->type == AMDGPU_RING_TYPE_KIQ)
		mqd_mgr = &adev->mqds[AMDGPU_HW_IP_COMPUTE];
	else
		mqd_mgr = &adev->mqds[ring->funcs->type];

	return mqd_mgr->init_mqd(adev, ring->mqd_ptr, &prop);
}

void amdgpu_ring_ib_begin(struct amdgpu_ring *ring)
{
	if (ring->is_sw_ring)
		amdgpu_sw_ring_ib_begin(ring);
}

void amdgpu_ring_ib_end(struct amdgpu_ring *ring)
{
	if (ring->is_sw_ring)
		amdgpu_sw_ring_ib_end(ring);
}

void amdgpu_ring_ib_on_emit_cntl(struct amdgpu_ring *ring)
{
	if (ring->is_sw_ring)
		amdgpu_sw_ring_ib_mark_offset(ring, AMDGPU_MUX_OFFSET_TYPE_CONTROL);
}

void amdgpu_ring_ib_on_emit_ce(struct amdgpu_ring *ring)
{
	if (ring->is_sw_ring)
		amdgpu_sw_ring_ib_mark_offset(ring, AMDGPU_MUX_OFFSET_TYPE_CE);
}

void amdgpu_ring_ib_on_emit_de(struct amdgpu_ring *ring)
{
	if (ring->is_sw_ring)
		amdgpu_sw_ring_ib_mark_offset(ring, AMDGPU_MUX_OFFSET_TYPE_DE);
}
