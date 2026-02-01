 

 

#include <linux/platform_device.h>

#include <drm/drm_drv.h>

#include "vc4_drv.h"
#include "vc4_regs.h"
#include "vc4_trace.h"

#define V3D_DRIVER_IRQS (V3D_INT_OUTOMEM | \
			 V3D_INT_FLDONE | \
			 V3D_INT_FRDONE)

static void
vc4_overflow_mem_work(struct work_struct *work)
{
	struct vc4_dev *vc4 =
		container_of(work, struct vc4_dev, overflow_mem_work);
	struct vc4_bo *bo;
	int bin_bo_slot;
	struct vc4_exec_info *exec;
	unsigned long irqflags;

	mutex_lock(&vc4->bin_bo_lock);

	if (!vc4->bin_bo)
		goto complete;

	bo = vc4->bin_bo;

	bin_bo_slot = vc4_v3d_get_bin_slot(vc4);
	if (bin_bo_slot < 0) {
		DRM_ERROR("Couldn't allocate binner overflow mem\n");
		goto complete;
	}

	spin_lock_irqsave(&vc4->job_lock, irqflags);

	if (vc4->bin_alloc_overflow) {
		 
		exec = vc4_first_bin_job(vc4);
		if (!exec)
			exec = vc4_last_render_job(vc4);
		if (exec) {
			exec->bin_slots |= vc4->bin_alloc_overflow;
		} else {
			 
			vc4->bin_alloc_used &= ~vc4->bin_alloc_overflow;
		}
	}
	vc4->bin_alloc_overflow = BIT(bin_bo_slot);

	V3D_WRITE(V3D_BPOA, bo->base.dma_addr + bin_bo_slot * vc4->bin_alloc_size);
	V3D_WRITE(V3D_BPOS, bo->base.base.size);
	V3D_WRITE(V3D_INTCTL, V3D_INT_OUTOMEM);
	V3D_WRITE(V3D_INTENA, V3D_INT_OUTOMEM);
	spin_unlock_irqrestore(&vc4->job_lock, irqflags);

complete:
	mutex_unlock(&vc4->bin_bo_lock);
}

static void
vc4_irq_finish_bin_job(struct drm_device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	struct vc4_exec_info *next, *exec = vc4_first_bin_job(vc4);

	if (!exec)
		return;

	trace_vc4_bcl_end_irq(dev, exec->seqno);

	vc4_move_job_to_render(dev, exec);
	next = vc4_first_bin_job(vc4);

	 
	if (next && next->perfmon == exec->perfmon)
		vc4_submit_next_bin_job(dev);
}

static void
vc4_cancel_bin_job(struct drm_device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	struct vc4_exec_info *exec = vc4_first_bin_job(vc4);

	if (!exec)
		return;

	 
	if (exec->perfmon)
		vc4_perfmon_stop(vc4, exec->perfmon, false);

	list_move_tail(&exec->head, &vc4->bin_job_list);
	vc4_submit_next_bin_job(dev);
}

static void
vc4_irq_finish_render_job(struct drm_device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	struct vc4_exec_info *exec = vc4_first_render_job(vc4);
	struct vc4_exec_info *nextbin, *nextrender;

	if (!exec)
		return;

	trace_vc4_rcl_end_irq(dev, exec->seqno);

	vc4->finished_seqno++;
	list_move_tail(&exec->head, &vc4->job_done_list);

	nextbin = vc4_first_bin_job(vc4);
	nextrender = vc4_first_render_job(vc4);

	 
	if (exec->perfmon && !nextrender &&
	    (!nextbin || nextbin->perfmon != exec->perfmon))
		vc4_perfmon_stop(vc4, exec->perfmon, true);

	 
	if (nextrender)
		vc4_submit_next_render_job(dev);
	else if (nextbin && nextbin->perfmon != exec->perfmon)
		vc4_submit_next_bin_job(dev);

	if (exec->fence) {
		dma_fence_signal_locked(exec->fence);
		dma_fence_put(exec->fence);
		exec->fence = NULL;
	}

	wake_up_all(&vc4->job_wait_queue);
	schedule_work(&vc4->job_done_work);
}

static irqreturn_t
vc4_irq(int irq, void *arg)
{
	struct drm_device *dev = arg;
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	uint32_t intctl;
	irqreturn_t status = IRQ_NONE;

	barrier();
	intctl = V3D_READ(V3D_INTCTL);

	 
	V3D_WRITE(V3D_INTCTL, intctl);

	if (intctl & V3D_INT_OUTOMEM) {
		 
		V3D_WRITE(V3D_INTDIS, V3D_INT_OUTOMEM);
		schedule_work(&vc4->overflow_mem_work);
		status = IRQ_HANDLED;
	}

	if (intctl & V3D_INT_FLDONE) {
		spin_lock(&vc4->job_lock);
		vc4_irq_finish_bin_job(dev);
		spin_unlock(&vc4->job_lock);
		status = IRQ_HANDLED;
	}

	if (intctl & V3D_INT_FRDONE) {
		spin_lock(&vc4->job_lock);
		vc4_irq_finish_render_job(dev);
		spin_unlock(&vc4->job_lock);
		status = IRQ_HANDLED;
	}

	return status;
}

static void
vc4_irq_prepare(struct drm_device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);

	if (!vc4->v3d)
		return;

	init_waitqueue_head(&vc4->job_wait_queue);
	INIT_WORK(&vc4->overflow_mem_work, vc4_overflow_mem_work);

	 
	V3D_WRITE(V3D_INTCTL, V3D_DRIVER_IRQS);
}

void
vc4_irq_enable(struct drm_device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);

	if (WARN_ON_ONCE(vc4->is_vc5))
		return;

	if (!vc4->v3d)
		return;

	 
	V3D_WRITE(V3D_INTENA, V3D_INT_FLDONE | V3D_INT_FRDONE);
}

void
vc4_irq_disable(struct drm_device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);

	if (WARN_ON_ONCE(vc4->is_vc5))
		return;

	if (!vc4->v3d)
		return;

	 
	V3D_WRITE(V3D_INTDIS, V3D_DRIVER_IRQS);

	 
	V3D_WRITE(V3D_INTCTL, V3D_DRIVER_IRQS);

	 
	synchronize_irq(vc4->irq);

	cancel_work_sync(&vc4->overflow_mem_work);
}

int vc4_irq_install(struct drm_device *dev, int irq)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	int ret;

	if (WARN_ON_ONCE(vc4->is_vc5))
		return -ENODEV;

	if (irq == IRQ_NOTCONNECTED)
		return -ENOTCONN;

	vc4_irq_prepare(dev);

	ret = request_irq(irq, vc4_irq, 0, dev->driver->name, dev);
	if (ret)
		return ret;

	vc4_irq_enable(dev);

	return 0;
}

void vc4_irq_uninstall(struct drm_device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);

	if (WARN_ON_ONCE(vc4->is_vc5))
		return;

	vc4_irq_disable(dev);
	free_irq(vc4->irq, dev);
}

 
void vc4_irq_reset(struct drm_device *dev)
{
	struct vc4_dev *vc4 = to_vc4_dev(dev);
	unsigned long irqflags;

	if (WARN_ON_ONCE(vc4->is_vc5))
		return;

	 
	V3D_WRITE(V3D_INTCTL, V3D_DRIVER_IRQS);

	 
	V3D_WRITE(V3D_INTENA, V3D_DRIVER_IRQS);

	spin_lock_irqsave(&vc4->job_lock, irqflags);
	vc4_cancel_bin_job(dev);
	vc4_irq_finish_render_job(dev);
	spin_unlock_irqrestore(&vc4->job_lock, irqflags);
}
