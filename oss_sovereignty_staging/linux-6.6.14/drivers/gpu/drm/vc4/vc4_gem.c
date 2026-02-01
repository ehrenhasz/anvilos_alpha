 

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/sched/signal.h>
#include <linux/dma-fence-array.h>

#include <drm/drm_syncobj.h>

#include "uapi/drm/vc4_drm.h"
#include "vc4_drv.h"
#include "vc4_regs.h"
#include "vc4_trace.h"

static void
vc4_queue_hangcheck(struct drm_device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);

	mod_timer(&vc4->hangcheck.timer,
		  round_jiffies_up(jiffies + msecs_to_jiffies(100)));
}

struct vc4_hang_state {
	struct drm_vc4_get_hang_state user_state;

	u32 bo_count;
	struct drm_gem_object **bo;
};

static void
vc4_free_hang_state(struct drm_device *dev, struct vc4_hang_state *state)
{
	unsigned int i;

	for (i = 0; i < state->user_state.bo_count; i++)
		drm_gem_object_put(state->bo[i]);

	kfree(state);
}

int
vc4_get_hang_state_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_vc4_get_hang_state *get_state = data;
	struct drm_vc4_get_hang_state_bo *bo_state;
	struct vc4_hang_state *kernel_state;
	struct drm_vc4_get_hang_state *state;
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	unsigned long irqflags;
	u32 i;
	int ret = 0;

	if (WARN_ON_ONCE(vc4->is_vc5))
		return -ENODEV;

	if (!vc4->v3d) {
		DRM_DEBUG("VC4_GET_HANG_STATE with no VC4 V3D probed\n");
		return -ENODEV;
	}

	spin_lock_irqsave(&vc4->job_lock, irqflags);
	kernel_state = vc4->hang_state;
	if (!kernel_state) {
		spin_unlock_irqrestore(&vc4->job_lock, irqflags);
		return -ENOENT;
	}
	state = &kernel_state->user_state;

	 
	if (get_state->bo_count < state->bo_count) {
		get_state->bo_count = state->bo_count;
		spin_unlock_irqrestore(&vc4->job_lock, irqflags);
		return 0;
	}

	vc4->hang_state = NULL;
	spin_unlock_irqrestore(&vc4->job_lock, irqflags);

	 
	state->bo = get_state->bo;
	memcpy(get_state, state, sizeof(*state));

	bo_state = kcalloc(state->bo_count, sizeof(*bo_state), GFP_KERNEL);
	if (!bo_state) {
		ret = -ENOMEM;
		goto err_free;
	}

	for (i = 0; i < state->bo_count; i++) {
		struct vc4_bo *vc4_bo = to_vc4_bo(kernel_state->bo[i]);
		u32 handle;

		ret = drm_gem_handle_create(file_priv, kernel_state->bo[i],
					    &handle);

		if (ret) {
			state->bo_count = i;
			goto err_delete_handle;
		}
		bo_state[i].handle = handle;
		bo_state[i].paddr = vc4_bo->base.dma_addr;
		bo_state[i].size = vc4_bo->base.base.size;
	}

	if (copy_to_user(u64_to_user_ptr(get_state->bo),
			 bo_state,
			 state->bo_count * sizeof(*bo_state)))
		ret = -EFAULT;

err_delete_handle:
	if (ret) {
		for (i = 0; i < state->bo_count; i++)
			drm_gem_handle_delete(file_priv, bo_state[i].handle);
	}

err_free:
	vc4_free_hang_state(dev, kernel_state);
	kfree(bo_state);

	return ret;
}

static void
vc4_save_hang_state(struct drm_device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	struct drm_vc4_get_hang_state *state;
	struct vc4_hang_state *kernel_state;
	struct vc4_exec_info *exec[2];
	struct vc4_bo *bo;
	unsigned long irqflags;
	unsigned int i, j, k, unref_list_count;

	kernel_state = kcalloc(1, sizeof(*kernel_state), GFP_KERNEL);
	if (!kernel_state)
		return;

	state = &kernel_state->user_state;

	spin_lock_irqsave(&vc4->job_lock, irqflags);
	exec[0] = vc4_first_bin_job(vc4);
	exec[1] = vc4_first_render_job(vc4);
	if (!exec[0] && !exec[1]) {
		spin_unlock_irqrestore(&vc4->job_lock, irqflags);
		return;
	}

	 
	state->bo_count = 0;
	for (i = 0; i < 2; i++) {
		if (!exec[i])
			continue;

		unref_list_count = 0;
		list_for_each_entry(bo, &exec[i]->unref_list, unref_head)
			unref_list_count++;
		state->bo_count += exec[i]->bo_count + unref_list_count;
	}

	kernel_state->bo = kcalloc(state->bo_count,
				   sizeof(*kernel_state->bo), GFP_ATOMIC);

	if (!kernel_state->bo) {
		spin_unlock_irqrestore(&vc4->job_lock, irqflags);
		return;
	}

	k = 0;
	for (i = 0; i < 2; i++) {
		if (!exec[i])
			continue;

		for (j = 0; j < exec[i]->bo_count; j++) {
			bo = to_vc4_bo(exec[i]->bo[j]);

			 
			WARN_ON(!refcount_read(&bo->usecnt));
			refcount_inc(&bo->usecnt);
			drm_gem_object_get(exec[i]->bo[j]);
			kernel_state->bo[k++] = exec[i]->bo[j];
		}

		list_for_each_entry(bo, &exec[i]->unref_list, unref_head) {
			 
			drm_gem_object_get(&bo->base.base);
			kernel_state->bo[k++] = &bo->base.base;
		}
	}

	WARN_ON_ONCE(k != state->bo_count);

	if (exec[0])
		state->start_bin = exec[0]->ct0ca;
	if (exec[1])
		state->start_render = exec[1]->ct1ca;

	spin_unlock_irqrestore(&vc4->job_lock, irqflags);

	state->ct0ca = V3D_READ(V3D_CTNCA(0));
	state->ct0ea = V3D_READ(V3D_CTNEA(0));

	state->ct1ca = V3D_READ(V3D_CTNCA(1));
	state->ct1ea = V3D_READ(V3D_CTNEA(1));

	state->ct0cs = V3D_READ(V3D_CTNCS(0));
	state->ct1cs = V3D_READ(V3D_CTNCS(1));

	state->ct0ra0 = V3D_READ(V3D_CT00RA0);
	state->ct1ra0 = V3D_READ(V3D_CT01RA0);

	state->bpca = V3D_READ(V3D_BPCA);
	state->bpcs = V3D_READ(V3D_BPCS);
	state->bpoa = V3D_READ(V3D_BPOA);
	state->bpos = V3D_READ(V3D_BPOS);

	state->vpmbase = V3D_READ(V3D_VPMBASE);

	state->dbge = V3D_READ(V3D_DBGE);
	state->fdbgo = V3D_READ(V3D_FDBGO);
	state->fdbgb = V3D_READ(V3D_FDBGB);
	state->fdbgr = V3D_READ(V3D_FDBGR);
	state->fdbgs = V3D_READ(V3D_FDBGS);
	state->errstat = V3D_READ(V3D_ERRSTAT);

	 
	for (i = 0; i < kernel_state->user_state.bo_count; i++) {
		struct vc4_bo *bo = to_vc4_bo(kernel_state->bo[i]);

		if (bo->madv == __VC4_MADV_NOTSUPP)
			continue;

		mutex_lock(&bo->madv_lock);
		if (!WARN_ON(bo->madv == __VC4_MADV_PURGED))
			bo->madv = VC4_MADV_WILLNEED;
		refcount_dec(&bo->usecnt);
		mutex_unlock(&bo->madv_lock);
	}

	spin_lock_irqsave(&vc4->job_lock, irqflags);
	if (vc4->hang_state) {
		spin_unlock_irqrestore(&vc4->job_lock, irqflags);
		vc4_free_hang_state(dev, kernel_state);
	} else {
		vc4->hang_state = kernel_state;
		spin_unlock_irqrestore(&vc4->job_lock, irqflags);
	}
}

static void
vc4_reset(struct drm_device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);

	DRM_INFO("Resetting GPU.\n");

	mutex_lock(&vc4->power_lock);
	if (vc4->power_refcount) {
		 
		pm_runtime_put_sync_suspend(&vc4->v3d->pdev->dev);
		pm_runtime_get_sync(&vc4->v3d->pdev->dev);
	}
	mutex_unlock(&vc4->power_lock);

	vc4_irq_reset(dev);

	 
	vc4_queue_hangcheck(dev);
}

static void
vc4_reset_work(struct work_struct *work)
{
	struct vc4_dev *vc4 =
		container_of(work, struct vc4_dev, hangcheck.reset_work);

	vc4_save_hang_state(&vc4->base);

	vc4_reset(&vc4->base);
}

static void
vc4_hangcheck_elapsed(struct timer_list *t)
{
	struct vc4_dev *vc4 = from_timer(vc4, t, hangcheck.timer);
	struct drm_device *dev = &vc4->base;
	uint32_t ct0ca, ct1ca;
	unsigned long irqflags;
	struct vc4_exec_info *bin_exec, *render_exec;

	spin_lock_irqsave(&vc4->job_lock, irqflags);

	bin_exec = vc4_first_bin_job(vc4);
	render_exec = vc4_first_render_job(vc4);

	 
	if (!bin_exec && !render_exec) {
		spin_unlock_irqrestore(&vc4->job_lock, irqflags);
		return;
	}

	ct0ca = V3D_READ(V3D_CTNCA(0));
	ct1ca = V3D_READ(V3D_CTNCA(1));

	 
	if ((bin_exec && ct0ca != bin_exec->last_ct0ca) ||
	    (render_exec && ct1ca != render_exec->last_ct1ca)) {
		if (bin_exec)
			bin_exec->last_ct0ca = ct0ca;
		if (render_exec)
			render_exec->last_ct1ca = ct1ca;
		spin_unlock_irqrestore(&vc4->job_lock, irqflags);
		vc4_queue_hangcheck(dev);
		return;
	}

	spin_unlock_irqrestore(&vc4->job_lock, irqflags);

	 
	schedule_work(&vc4->hangcheck.reset_work);
}

static void
submit_cl(struct drm_device *dev, uint32_t thread, uint32_t start, uint32_t end)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);

	 
	V3D_WRITE(V3D_CTNCA(thread), start);
	V3D_WRITE(V3D_CTNEA(thread), end);
}

int
vc4_wait_for_seqno(struct drm_device *dev, uint64_t seqno, uint64_t timeout_ns,
		   bool interruptible)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	int ret = 0;
	unsigned long timeout_expire;
	DEFINE_WAIT(wait);

	if (WARN_ON_ONCE(vc4->is_vc5))
		return -ENODEV;

	if (vc4->finished_seqno >= seqno)
		return 0;

	if (timeout_ns == 0)
		return -ETIME;

	timeout_expire = jiffies + nsecs_to_jiffies(timeout_ns);

	trace_vc4_wait_for_seqno_begin(dev, seqno, timeout_ns);
	for (;;) {
		prepare_to_wait(&vc4->job_wait_queue, &wait,
				interruptible ? TASK_INTERRUPTIBLE :
				TASK_UNINTERRUPTIBLE);

		if (interruptible && signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}

		if (vc4->finished_seqno >= seqno)
			break;

		if (timeout_ns != ~0ull) {
			if (time_after_eq(jiffies, timeout_expire)) {
				ret = -ETIME;
				break;
			}
			schedule_timeout(timeout_expire - jiffies);
		} else {
			schedule();
		}
	}

	finish_wait(&vc4->job_wait_queue, &wait);
	trace_vc4_wait_for_seqno_end(dev, seqno);

	return ret;
}

static void
vc4_flush_caches(struct drm_device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);

	 
	V3D_WRITE(V3D_L2CACTL,
		  V3D_L2CACTL_L2CCLR);

	V3D_WRITE(V3D_SLCACTL,
		  VC4_SET_FIELD(0xf, V3D_SLCACTL_T1CC) |
		  VC4_SET_FIELD(0xf, V3D_SLCACTL_T0CC) |
		  VC4_SET_FIELD(0xf, V3D_SLCACTL_UCC) |
		  VC4_SET_FIELD(0xf, V3D_SLCACTL_ICC));
}

static void
vc4_flush_texture_caches(struct drm_device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);

	V3D_WRITE(V3D_L2CACTL,
		  V3D_L2CACTL_L2CCLR);

	V3D_WRITE(V3D_SLCACTL,
		  VC4_SET_FIELD(0xf, V3D_SLCACTL_T1CC) |
		  VC4_SET_FIELD(0xf, V3D_SLCACTL_T0CC));
}

 
void
vc4_submit_next_bin_job(struct drm_device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	struct vc4_exec_info *exec;

	if (WARN_ON_ONCE(vc4->is_vc5))
		return;

again:
	exec = vc4_first_bin_job(vc4);
	if (!exec)
		return;

	vc4_flush_caches(dev);

	 
	if (exec->perfmon && vc4->active_perfmon != exec->perfmon)
		vc4_perfmon_start(vc4, exec->perfmon);

	 
	if (exec->ct0ca != exec->ct0ea) {
		trace_vc4_submit_cl(dev, false, exec->seqno, exec->ct0ca,
				    exec->ct0ea);
		submit_cl(dev, 0, exec->ct0ca, exec->ct0ea);
	} else {
		struct vc4_exec_info *next;

		vc4_move_job_to_render(dev, exec);
		next = vc4_first_bin_job(vc4);

		 
		if (next && next->perfmon == exec->perfmon)
			goto again;
	}
}

void
vc4_submit_next_render_job(struct drm_device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	struct vc4_exec_info *exec = vc4_first_render_job(vc4);

	if (!exec)
		return;

	if (WARN_ON_ONCE(vc4->is_vc5))
		return;

	 
	vc4_flush_texture_caches(dev);

	trace_vc4_submit_cl(dev, true, exec->seqno, exec->ct1ca, exec->ct1ea);
	submit_cl(dev, 1, exec->ct1ca, exec->ct1ea);
}

void
vc4_move_job_to_render(struct drm_device *dev, struct vc4_exec_info *exec)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	bool was_empty = list_empty(&vc4->render_job_list);

	if (WARN_ON_ONCE(vc4->is_vc5))
		return;

	list_move_tail(&exec->head, &vc4->render_job_list);
	if (was_empty)
		vc4_submit_next_render_job(dev);
}

static void
vc4_update_bo_seqnos(struct vc4_exec_info *exec, uint64_t seqno)
{
	struct vc4_bo *bo;
	unsigned i;

	for (i = 0; i < exec->bo_count; i++) {
		bo = to_vc4_bo(exec->bo[i]);
		bo->seqno = seqno;

		dma_resv_add_fence(bo->base.base.resv, exec->fence,
				   DMA_RESV_USAGE_READ);
	}

	list_for_each_entry(bo, &exec->unref_list, unref_head) {
		bo->seqno = seqno;
	}

	for (i = 0; i < exec->rcl_write_bo_count; i++) {
		bo = to_vc4_bo(&exec->rcl_write_bo[i]->base);
		bo->write_seqno = seqno;

		dma_resv_add_fence(bo->base.base.resv, exec->fence,
				   DMA_RESV_USAGE_WRITE);
	}
}

static void
vc4_unlock_bo_reservations(struct drm_device *dev,
			   struct vc4_exec_info *exec,
			   struct ww_acquire_ctx *acquire_ctx)
{
	int i;

	for (i = 0; i < exec->bo_count; i++)
		dma_resv_unlock(exec->bo[i]->resv);

	ww_acquire_fini(acquire_ctx);
}

 
static int
vc4_lock_bo_reservations(struct drm_device *dev,
			 struct vc4_exec_info *exec,
			 struct ww_acquire_ctx *acquire_ctx)
{
	int contended_lock = -1;
	int i, ret;
	struct drm_gem_object *bo;

	ww_acquire_init(acquire_ctx, &reservation_ww_class);

retry:
	if (contended_lock != -1) {
		bo = exec->bo[contended_lock];
		ret = dma_resv_lock_slow_interruptible(bo->resv, acquire_ctx);
		if (ret) {
			ww_acquire_done(acquire_ctx);
			return ret;
		}
	}

	for (i = 0; i < exec->bo_count; i++) {
		if (i == contended_lock)
			continue;

		bo = exec->bo[i];

		ret = dma_resv_lock_interruptible(bo->resv, acquire_ctx);
		if (ret) {
			int j;

			for (j = 0; j < i; j++) {
				bo = exec->bo[j];
				dma_resv_unlock(bo->resv);
			}

			if (contended_lock != -1 && contended_lock >= i) {
				bo = exec->bo[contended_lock];

				dma_resv_unlock(bo->resv);
			}

			if (ret == -EDEADLK) {
				contended_lock = i;
				goto retry;
			}

			ww_acquire_done(acquire_ctx);
			return ret;
		}
	}

	ww_acquire_done(acquire_ctx);

	 
	for (i = 0; i < exec->bo_count; i++) {
		bo = exec->bo[i];

		ret = dma_resv_reserve_fences(bo->resv, 1);
		if (ret) {
			vc4_unlock_bo_reservations(dev, exec, acquire_ctx);
			return ret;
		}
	}

	return 0;
}

 
static int
vc4_queue_submit(struct drm_device *dev, struct vc4_exec_info *exec,
		 struct ww_acquire_ctx *acquire_ctx,
		 struct drm_syncobj *out_sync)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	struct vc4_exec_info *renderjob;
	uint64_t seqno;
	unsigned long irqflags;
	struct vc4_fence *fence;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence)
		return -ENOMEM;
	fence->dev = dev;

	spin_lock_irqsave(&vc4->job_lock, irqflags);

	seqno = ++vc4->emit_seqno;
	exec->seqno = seqno;

	dma_fence_init(&fence->base, &vc4_fence_ops, &vc4->job_lock,
		       vc4->dma_fence_context, exec->seqno);
	fence->seqno = exec->seqno;
	exec->fence = &fence->base;

	if (out_sync)
		drm_syncobj_replace_fence(out_sync, exec->fence);

	vc4_update_bo_seqnos(exec, seqno);

	vc4_unlock_bo_reservations(dev, exec, acquire_ctx);

	list_add_tail(&exec->head, &vc4->bin_job_list);

	 
	renderjob = vc4_first_render_job(vc4);
	if (vc4_first_bin_job(vc4) == exec &&
	    (!renderjob || renderjob->perfmon == exec->perfmon)) {
		vc4_submit_next_bin_job(dev);
		vc4_queue_hangcheck(dev);
	}

	spin_unlock_irqrestore(&vc4->job_lock, irqflags);

	return 0;
}

 
static int
vc4_cl_lookup_bos(struct drm_device *dev,
		  struct drm_file *file_priv,
		  struct vc4_exec_info *exec)
{
	struct drm_vc4_submit_cl *args = exec->args;
	int ret = 0;
	int i;

	exec->bo_count = args->bo_handle_count;

	if (!exec->bo_count) {
		 
		DRM_DEBUG("Rendering requires BOs to validate\n");
		return -EINVAL;
	}

	ret = drm_gem_objects_lookup(file_priv, u64_to_user_ptr(args->bo_handles),
				     exec->bo_count, &exec->bo);

	if (ret)
		goto fail_put_bo;

	for (i = 0; i < exec->bo_count; i++) {
		ret = vc4_bo_inc_usecnt(to_vc4_bo(exec->bo[i]));
		if (ret)
			goto fail_dec_usecnt;
	}

	return 0;

fail_dec_usecnt:
	 
	for (i-- ; i >= 0; i--)
		vc4_bo_dec_usecnt(to_vc4_bo(exec->bo[i]));

fail_put_bo:
	 
	for (i = 0; i < exec->bo_count && exec->bo[i]; i++)
		drm_gem_object_put(exec->bo[i]);

	kvfree(exec->bo);
	exec->bo = NULL;
	return ret;
}

static int
vc4_get_bcl(struct drm_device *dev, struct vc4_exec_info *exec)
{
	struct drm_vc4_submit_cl *args = exec->args;
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	void *temp = NULL;
	void *bin;
	int ret = 0;
	uint32_t bin_offset = 0;
	uint32_t shader_rec_offset = roundup(bin_offset + args->bin_cl_size,
					     16);
	uint32_t uniforms_offset = shader_rec_offset + args->shader_rec_size;
	uint32_t exec_size = uniforms_offset + args->uniforms_size;
	uint32_t temp_size = exec_size + (sizeof(struct vc4_shader_state) *
					  args->shader_rec_count);
	struct vc4_bo *bo;

	if (shader_rec_offset < args->bin_cl_size ||
	    uniforms_offset < shader_rec_offset ||
	    exec_size < uniforms_offset ||
	    args->shader_rec_count >= (UINT_MAX /
					  sizeof(struct vc4_shader_state)) ||
	    temp_size < exec_size) {
		DRM_DEBUG("overflow in exec arguments\n");
		ret = -EINVAL;
		goto fail;
	}

	 
	temp = kvmalloc_array(temp_size, 1, GFP_KERNEL);
	if (!temp) {
		DRM_ERROR("Failed to allocate storage for copying "
			  "in bin/render CLs.\n");
		ret = -ENOMEM;
		goto fail;
	}
	bin = temp + bin_offset;
	exec->shader_rec_u = temp + shader_rec_offset;
	exec->uniforms_u = temp + uniforms_offset;
	exec->shader_state = temp + exec_size;
	exec->shader_state_size = args->shader_rec_count;

	if (copy_from_user(bin,
			   u64_to_user_ptr(args->bin_cl),
			   args->bin_cl_size)) {
		ret = -EFAULT;
		goto fail;
	}

	if (copy_from_user(exec->shader_rec_u,
			   u64_to_user_ptr(args->shader_rec),
			   args->shader_rec_size)) {
		ret = -EFAULT;
		goto fail;
	}

	if (copy_from_user(exec->uniforms_u,
			   u64_to_user_ptr(args->uniforms),
			   args->uniforms_size)) {
		ret = -EFAULT;
		goto fail;
	}

	bo = vc4_bo_create(dev, exec_size, true, VC4_BO_TYPE_BCL);
	if (IS_ERR(bo)) {
		DRM_ERROR("Couldn't allocate BO for binning\n");
		ret = PTR_ERR(bo);
		goto fail;
	}
	exec->exec_bo = &bo->base;

	list_add_tail(&to_vc4_bo(&exec->exec_bo->base)->unref_head,
		      &exec->unref_list);

	exec->ct0ca = exec->exec_bo->dma_addr + bin_offset;

	exec->bin_u = bin;

	exec->shader_rec_v = exec->exec_bo->vaddr + shader_rec_offset;
	exec->shader_rec_p = exec->exec_bo->dma_addr + shader_rec_offset;
	exec->shader_rec_size = args->shader_rec_size;

	exec->uniforms_v = exec->exec_bo->vaddr + uniforms_offset;
	exec->uniforms_p = exec->exec_bo->dma_addr + uniforms_offset;
	exec->uniforms_size = args->uniforms_size;

	ret = vc4_validate_bin_cl(dev,
				  exec->exec_bo->vaddr + bin_offset,
				  bin,
				  exec);
	if (ret)
		goto fail;

	ret = vc4_validate_shader_recs(dev, exec);
	if (ret)
		goto fail;

	if (exec->found_tile_binning_mode_config_packet) {
		ret = vc4_v3d_bin_bo_get(vc4, &exec->bin_bo_used);
		if (ret)
			goto fail;
	}

	 
	ret = vc4_wait_for_seqno(dev, exec->bin_dep_seqno, ~0ull, true);

fail:
	kvfree(temp);
	return ret;
}

static void
vc4_complete_exec(struct drm_device *dev, struct vc4_exec_info *exec)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	unsigned long irqflags;
	unsigned i;

	 
	if (exec->fence) {
		dma_fence_signal(exec->fence);
		dma_fence_put(exec->fence);
	}

	if (exec->bo) {
		for (i = 0; i < exec->bo_count; i++) {
			struct vc4_bo *bo = to_vc4_bo(exec->bo[i]);

			vc4_bo_dec_usecnt(bo);
			drm_gem_object_put(exec->bo[i]);
		}
		kvfree(exec->bo);
	}

	while (!list_empty(&exec->unref_list)) {
		struct vc4_bo *bo = list_first_entry(&exec->unref_list,
						     struct vc4_bo, unref_head);
		list_del(&bo->unref_head);
		drm_gem_object_put(&bo->base.base);
	}

	 
	spin_lock_irqsave(&vc4->job_lock, irqflags);
	vc4->bin_alloc_used &= ~exec->bin_slots;
	spin_unlock_irqrestore(&vc4->job_lock, irqflags);

	 
	if (exec->bin_bo_used)
		vc4_v3d_bin_bo_put(vc4);

	 
	vc4_perfmon_put(exec->perfmon);

	vc4_v3d_pm_put(vc4);

	kfree(exec);
}

void
vc4_job_handle_completed(struct vc4_dev *vc4)
{
	unsigned long irqflags;
	struct vc4_seqno_cb *cb, *cb_temp;

	if (WARN_ON_ONCE(vc4->is_vc5))
		return;

	spin_lock_irqsave(&vc4->job_lock, irqflags);
	while (!list_empty(&vc4->job_done_list)) {
		struct vc4_exec_info *exec =
			list_first_entry(&vc4->job_done_list,
					 struct vc4_exec_info, head);
		list_del(&exec->head);

		spin_unlock_irqrestore(&vc4->job_lock, irqflags);
		vc4_complete_exec(&vc4->base, exec);
		spin_lock_irqsave(&vc4->job_lock, irqflags);
	}

	list_for_each_entry_safe(cb, cb_temp, &vc4->seqno_cb_list, work.entry) {
		if (cb->seqno <= vc4->finished_seqno) {
			list_del_init(&cb->work.entry);
			schedule_work(&cb->work);
		}
	}

	spin_unlock_irqrestore(&vc4->job_lock, irqflags);
}

static void vc4_seqno_cb_work(struct work_struct *work)
{
	struct vc4_seqno_cb *cb = container_of(work, struct vc4_seqno_cb, work);

	cb->func(cb);
}

int vc4_queue_seqno_cb(struct drm_device *dev,
		       struct vc4_seqno_cb *cb, uint64_t seqno,
		       void (*func)(struct vc4_seqno_cb *cb))
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	unsigned long irqflags;

	if (WARN_ON_ONCE(vc4->is_vc5))
		return -ENODEV;

	cb->func = func;
	INIT_WORK(&cb->work, vc4_seqno_cb_work);

	spin_lock_irqsave(&vc4->job_lock, irqflags);
	if (seqno > vc4->finished_seqno) {
		cb->seqno = seqno;
		list_add_tail(&cb->work.entry, &vc4->seqno_cb_list);
	} else {
		schedule_work(&cb->work);
	}
	spin_unlock_irqrestore(&vc4->job_lock, irqflags);

	return 0;
}

 
static void
vc4_job_done_work(struct work_struct *work)
{
	struct vc4_dev *vc4 =
		container_of(work, struct vc4_dev, job_done_work);

	vc4_job_handle_completed(vc4);
}

static int
vc4_wait_for_seqno_ioctl_helper(struct drm_device *dev,
				uint64_t seqno,
				uint64_t *timeout_ns)
{
	unsigned long start = jiffies;
	int ret = vc4_wait_for_seqno(dev, seqno, *timeout_ns, true);

	if ((ret == -EINTR || ret == -ERESTARTSYS) && *timeout_ns != ~0ull) {
		uint64_t delta = jiffies_to_nsecs(jiffies - start);

		if (*timeout_ns >= delta)
			*timeout_ns -= delta;
	}

	return ret;
}

int
vc4_wait_seqno_ioctl(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	struct drm_vc4_wait_seqno *args = data;

	if (WARN_ON_ONCE(vc4->is_vc5))
		return -ENODEV;

	return vc4_wait_for_seqno_ioctl_helper(dev, args->seqno,
					       &args->timeout_ns);
}

int
vc4_wait_bo_ioctl(struct drm_device *dev, void *data,
		  struct drm_file *file_priv)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	int ret;
	struct drm_vc4_wait_bo *args = data;
	struct drm_gem_object *gem_obj;
	struct vc4_bo *bo;

	if (WARN_ON_ONCE(vc4->is_vc5))
		return -ENODEV;

	if (args->pad != 0)
		return -EINVAL;

	gem_obj = drm_gem_object_lookup(file_priv, args->handle);
	if (!gem_obj) {
		DRM_DEBUG("Failed to look up GEM BO %d\n", args->handle);
		return -EINVAL;
	}
	bo = to_vc4_bo(gem_obj);

	ret = vc4_wait_for_seqno_ioctl_helper(dev, bo->seqno,
					      &args->timeout_ns);

	drm_gem_object_put(gem_obj);
	return ret;
}

 
int
vc4_submit_cl_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	struct vc4_file *vc4file = file_priv->driver_priv;
	struct drm_vc4_submit_cl *args = data;
	struct drm_syncobj *out_sync = NULL;
	struct vc4_exec_info *exec;
	struct ww_acquire_ctx acquire_ctx;
	struct dma_fence *in_fence;
	int ret = 0;

	trace_vc4_submit_cl_ioctl(dev, args->bin_cl_size,
				  args->shader_rec_size,
				  args->bo_handle_count);

	if (WARN_ON_ONCE(vc4->is_vc5))
		return -ENODEV;

	if (!vc4->v3d) {
		DRM_DEBUG("VC4_SUBMIT_CL with no VC4 V3D probed\n");
		return -ENODEV;
	}

	if ((args->flags & ~(VC4_SUBMIT_CL_USE_CLEAR_COLOR |
			     VC4_SUBMIT_CL_FIXED_RCL_ORDER |
			     VC4_SUBMIT_CL_RCL_ORDER_INCREASING_X |
			     VC4_SUBMIT_CL_RCL_ORDER_INCREASING_Y)) != 0) {
		DRM_DEBUG("Unknown flags: 0x%02x\n", args->flags);
		return -EINVAL;
	}

	if (args->pad2 != 0) {
		DRM_DEBUG("Invalid pad: 0x%08x\n", args->pad2);
		return -EINVAL;
	}

	exec = kcalloc(1, sizeof(*exec), GFP_KERNEL);
	if (!exec) {
		DRM_ERROR("malloc failure on exec struct\n");
		return -ENOMEM;
	}
	exec->dev = vc4;

	ret = vc4_v3d_pm_get(vc4);
	if (ret) {
		kfree(exec);
		return ret;
	}

	exec->args = args;
	INIT_LIST_HEAD(&exec->unref_list);

	ret = vc4_cl_lookup_bos(dev, file_priv, exec);
	if (ret)
		goto fail;

	if (args->perfmonid) {
		exec->perfmon = vc4_perfmon_find(vc4file,
						 args->perfmonid);
		if (!exec->perfmon) {
			ret = -ENOENT;
			goto fail;
		}
	}

	if (args->in_sync) {
		ret = drm_syncobj_find_fence(file_priv, args->in_sync,
					     0, 0, &in_fence);
		if (ret)
			goto fail;

		 
		if (!dma_fence_match_context(in_fence,
					     vc4->dma_fence_context)) {
			ret = dma_fence_wait(in_fence, true);
			if (ret) {
				dma_fence_put(in_fence);
				goto fail;
			}
		}

		dma_fence_put(in_fence);
	}

	if (exec->args->bin_cl_size != 0) {
		ret = vc4_get_bcl(dev, exec);
		if (ret)
			goto fail;
	} else {
		exec->ct0ca = 0;
		exec->ct0ea = 0;
	}

	ret = vc4_get_rcl(dev, exec);
	if (ret)
		goto fail;

	ret = vc4_lock_bo_reservations(dev, exec, &acquire_ctx);
	if (ret)
		goto fail;

	if (args->out_sync) {
		out_sync = drm_syncobj_find(file_priv, args->out_sync);
		if (!out_sync) {
			ret = -EINVAL;
			goto fail;
		}

		 
	}

	 
	exec->args = NULL;

	ret = vc4_queue_submit(dev, exec, &acquire_ctx, out_sync);

	 
	if (out_sync)
		drm_syncobj_put(out_sync);

	if (ret)
		goto fail;

	 
	args->seqno = vc4->emit_seqno;

	return 0;

fail:
	vc4_complete_exec(&vc4->base, exec);

	return ret;
}

static void vc4_gem_destroy(struct drm_device *dev, void *unused);
int vc4_gem_init(struct drm_device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	int ret;

	if (WARN_ON_ONCE(vc4->is_vc5))
		return -ENODEV;

	vc4->dma_fence_context = dma_fence_context_alloc(1);

	INIT_LIST_HEAD(&vc4->bin_job_list);
	INIT_LIST_HEAD(&vc4->render_job_list);
	INIT_LIST_HEAD(&vc4->job_done_list);
	INIT_LIST_HEAD(&vc4->seqno_cb_list);
	spin_lock_init(&vc4->job_lock);

	INIT_WORK(&vc4->hangcheck.reset_work, vc4_reset_work);
	timer_setup(&vc4->hangcheck.timer, vc4_hangcheck_elapsed, 0);

	INIT_WORK(&vc4->job_done_work, vc4_job_done_work);

	ret = drmm_mutex_init(dev, &vc4->power_lock);
	if (ret)
		return ret;

	INIT_LIST_HEAD(&vc4->purgeable.list);

	ret = drmm_mutex_init(dev, &vc4->purgeable.lock);
	if (ret)
		return ret;

	return drmm_add_action_or_reset(dev, vc4_gem_destroy, NULL);
}

static void vc4_gem_destroy(struct drm_device *dev, void *unused)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);

	 
	WARN_ON(vc4->emit_seqno != vc4->finished_seqno);

	 
	if (vc4->bin_bo) {
		drm_gem_object_put(&vc4->bin_bo->base.base);
		vc4->bin_bo = NULL;
	}

	if (vc4->hang_state)
		vc4_free_hang_state(dev, vc4->hang_state);
}

int vc4_gem_madvise_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	struct drm_vc4_gem_madvise *args = data;
	struct drm_gem_object *gem_obj;
	struct vc4_bo *bo;
	int ret;

	if (WARN_ON_ONCE(vc4->is_vc5))
		return -ENODEV;

	switch (args->madv) {
	case VC4_MADV_DONTNEED:
	case VC4_MADV_WILLNEED:
		break;
	default:
		return -EINVAL;
	}

	if (args->pad != 0)
		return -EINVAL;

	gem_obj = drm_gem_object_lookup(file_priv, args->handle);
	if (!gem_obj) {
		DRM_DEBUG("Failed to look up GEM BO %d\n", args->handle);
		return -ENOENT;
	}

	bo = to_vc4_bo(gem_obj);

	 
	if (bo->madv == __VC4_MADV_NOTSUPP) {
		DRM_DEBUG("madvise not supported on this BO\n");
		ret = -EINVAL;
		goto out_put_gem;
	}

	 
	if (gem_obj->import_attach) {
		DRM_DEBUG("madvise not supported on imported BOs\n");
		ret = -EINVAL;
		goto out_put_gem;
	}

	mutex_lock(&bo->madv_lock);

	if (args->madv == VC4_MADV_DONTNEED && bo->madv == VC4_MADV_WILLNEED &&
	    !refcount_read(&bo->usecnt)) {
		 
		vc4_bo_add_to_purgeable_pool(bo);
	} else if (args->madv == VC4_MADV_WILLNEED &&
		   bo->madv == VC4_MADV_DONTNEED &&
		   !refcount_read(&bo->usecnt)) {
		 
		vc4_bo_remove_from_purgeable_pool(bo);
	}

	 
	args->retained = bo->madv != __VC4_MADV_PURGED;

	 
	if (bo->madv != __VC4_MADV_PURGED)
		bo->madv = args->madv;

	mutex_unlock(&bo->madv_lock);

	ret = 0;

out_put_gem:
	drm_gem_object_put(gem_obj);

	return ret;
}
