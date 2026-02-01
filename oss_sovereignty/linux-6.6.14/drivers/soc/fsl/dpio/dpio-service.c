
 
#include <linux/types.h>
#include <linux/fsl/mc.h>
#include <soc/fsl/dpaa2-io.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/dim.h>
#include <linux/slab.h>

#include "dpio.h"
#include "qbman-portal.h"

struct dpaa2_io {
	struct dpaa2_io_desc dpio_desc;
	struct qbman_swp_desc swp_desc;
	struct qbman_swp *swp;
	struct list_head node;
	 
	spinlock_t lock_mgmt_cmd;
	 
	spinlock_t lock_notifications;
	struct list_head notifications;
	struct device *dev;

	 
	struct dim rx_dim;
	 
	spinlock_t dim_lock;
	u16 event_ctr;
	u64 bytes;
	u64 frames;
};

struct dpaa2_io_store {
	unsigned int max;
	dma_addr_t paddr;
	struct dpaa2_dq *vaddr;
	void *alloced_addr;     
	unsigned int idx;       
	struct qbman_swp *swp;  
	struct device *dev;     
};

 
static struct dpaa2_io *dpio_by_cpu[NR_CPUS];
static struct list_head dpio_list = LIST_HEAD_INIT(dpio_list);
static DEFINE_SPINLOCK(dpio_list_lock);

static inline struct dpaa2_io *service_select_by_cpu(struct dpaa2_io *d,
						     int cpu)
{
	if (d)
		return d;

	if (cpu != DPAA2_IO_ANY_CPU && cpu >= num_possible_cpus())
		return NULL;

	 
	if (cpu < 0)
		cpu = raw_smp_processor_id();

	 
	return dpio_by_cpu[cpu];
}

static inline struct dpaa2_io *service_select(struct dpaa2_io *d)
{
	if (d)
		return d;

	d = service_select_by_cpu(d, -1);
	if (d)
		return d;

	spin_lock(&dpio_list_lock);
	d = list_entry(dpio_list.next, struct dpaa2_io, node);
	list_del(&d->node);
	list_add_tail(&d->node, &dpio_list);
	spin_unlock(&dpio_list_lock);

	return d;
}

 
struct dpaa2_io *dpaa2_io_service_select(int cpu)
{
	if (cpu == DPAA2_IO_ANY_CPU)
		return service_select(NULL);

	return service_select_by_cpu(NULL, cpu);
}
EXPORT_SYMBOL_GPL(dpaa2_io_service_select);

static void dpaa2_io_dim_work(struct work_struct *w)
{
	struct dim *dim = container_of(w, struct dim, work);
	struct dim_cq_moder moder =
		net_dim_get_rx_moderation(dim->mode, dim->profile_ix);
	struct dpaa2_io *d = container_of(dim, struct dpaa2_io, rx_dim);

	dpaa2_io_set_irq_coalescing(d, moder.usec);
	dim->state = DIM_START_MEASURE;
}

 
struct dpaa2_io *dpaa2_io_create(const struct dpaa2_io_desc *desc,
				 struct device *dev)
{
	struct dpaa2_io *obj = kmalloc(sizeof(*obj), GFP_KERNEL);
	u32 qman_256_cycles_per_ns;

	if (!obj)
		return NULL;

	 
	if (desc->cpu != DPAA2_IO_ANY_CPU && desc->cpu >= num_possible_cpus()) {
		kfree(obj);
		return NULL;
	}

	obj->dpio_desc = *desc;
	obj->swp_desc.cena_bar = obj->dpio_desc.regs_cena;
	obj->swp_desc.cinh_bar = obj->dpio_desc.regs_cinh;
	obj->swp_desc.qman_clk = obj->dpio_desc.qman_clk;
	obj->swp_desc.qman_version = obj->dpio_desc.qman_version;

	 
	qman_256_cycles_per_ns = 256000 / (obj->swp_desc.qman_clk / 1000000);
	obj->swp_desc.qman_256_cycles_per_ns = qman_256_cycles_per_ns;
	obj->swp = qbman_swp_init(&obj->swp_desc);

	if (!obj->swp) {
		kfree(obj);
		return NULL;
	}

	INIT_LIST_HEAD(&obj->node);
	spin_lock_init(&obj->lock_mgmt_cmd);
	spin_lock_init(&obj->lock_notifications);
	spin_lock_init(&obj->dim_lock);
	INIT_LIST_HEAD(&obj->notifications);

	 
	qbman_swp_interrupt_set_trigger(obj->swp,
					QBMAN_SWP_INTERRUPT_DQRI);
	qbman_swp_interrupt_clear_status(obj->swp, 0xffffffff);
	if (obj->dpio_desc.receives_notifications)
		qbman_swp_push_set(obj->swp, 0, 1);

	spin_lock(&dpio_list_lock);
	list_add_tail(&obj->node, &dpio_list);
	if (desc->cpu >= 0 && !dpio_by_cpu[desc->cpu])
		dpio_by_cpu[desc->cpu] = obj;
	spin_unlock(&dpio_list_lock);

	obj->dev = dev;

	memset(&obj->rx_dim, 0, sizeof(obj->rx_dim));
	INIT_WORK(&obj->rx_dim.work, dpaa2_io_dim_work);
	obj->event_ctr = 0;
	obj->bytes = 0;
	obj->frames = 0;

	return obj;
}

 
void dpaa2_io_down(struct dpaa2_io *d)
{
	spin_lock(&dpio_list_lock);
	dpio_by_cpu[d->dpio_desc.cpu] = NULL;
	list_del(&d->node);
	spin_unlock(&dpio_list_lock);

	kfree(d);
}

#define DPAA_POLL_MAX 32

 
irqreturn_t dpaa2_io_irq(struct dpaa2_io *obj)
{
	const struct dpaa2_dq *dq;
	int max = 0;
	struct qbman_swp *swp;
	u32 status;

	obj->event_ctr++;

	swp = obj->swp;
	status = qbman_swp_interrupt_read_status(swp);
	if (!status)
		return IRQ_NONE;

	dq = qbman_swp_dqrr_next(swp);
	while (dq) {
		if (qbman_result_is_SCN(dq)) {
			struct dpaa2_io_notification_ctx *ctx;
			u64 q64;

			q64 = qbman_result_SCN_ctx(dq);
			ctx = (void *)(uintptr_t)q64;
			ctx->cb(ctx);
		} else {
			pr_crit("fsl-mc-dpio: Unrecognised/ignored DQRR entry\n");
		}
		qbman_swp_dqrr_consume(swp, dq);
		++max;
		if (max > DPAA_POLL_MAX)
			goto done;
		dq = qbman_swp_dqrr_next(swp);
	}
done:
	qbman_swp_interrupt_clear_status(swp, status);
	qbman_swp_interrupt_set_inhibit(swp, 0);
	return IRQ_HANDLED;
}

 
int dpaa2_io_get_cpu(struct dpaa2_io *d)
{
	return d->dpio_desc.cpu;
}
EXPORT_SYMBOL(dpaa2_io_get_cpu);

 
int dpaa2_io_service_register(struct dpaa2_io *d,
			      struct dpaa2_io_notification_ctx *ctx,
			      struct device *dev)
{
	struct device_link *link;
	unsigned long irqflags;

	d = service_select_by_cpu(d, ctx->desired_cpu);
	if (!d)
		return -ENODEV;

	link = device_link_add(dev, d->dev, DL_FLAG_AUTOREMOVE_CONSUMER);
	if (!link)
		return -EINVAL;

	ctx->dpio_id = d->dpio_desc.dpio_id;
	ctx->qman64 = (u64)(uintptr_t)ctx;
	ctx->dpio_private = d;
	spin_lock_irqsave(&d->lock_notifications, irqflags);
	list_add(&ctx->node, &d->notifications);
	spin_unlock_irqrestore(&d->lock_notifications, irqflags);

	 
	if (ctx->is_cdan)
		return qbman_swp_CDAN_set_context_enable(d->swp,
							 (u16)ctx->id,
							 ctx->qman64);
	return 0;
}
EXPORT_SYMBOL_GPL(dpaa2_io_service_register);

 
void dpaa2_io_service_deregister(struct dpaa2_io *service,
				 struct dpaa2_io_notification_ctx *ctx,
				 struct device *dev)
{
	struct dpaa2_io *d = ctx->dpio_private;
	unsigned long irqflags;

	if (ctx->is_cdan)
		qbman_swp_CDAN_disable(d->swp, (u16)ctx->id);

	spin_lock_irqsave(&d->lock_notifications, irqflags);
	list_del(&ctx->node);
	spin_unlock_irqrestore(&d->lock_notifications, irqflags);

}
EXPORT_SYMBOL_GPL(dpaa2_io_service_deregister);

 
int dpaa2_io_service_rearm(struct dpaa2_io *d,
			   struct dpaa2_io_notification_ctx *ctx)
{
	unsigned long irqflags;
	int err;

	d = service_select_by_cpu(d, ctx->desired_cpu);
	if (!unlikely(d))
		return -ENODEV;

	spin_lock_irqsave(&d->lock_mgmt_cmd, irqflags);
	if (ctx->is_cdan)
		err = qbman_swp_CDAN_enable(d->swp, (u16)ctx->id);
	else
		err = qbman_swp_fq_schedule(d->swp, ctx->id);
	spin_unlock_irqrestore(&d->lock_mgmt_cmd, irqflags);

	return err;
}
EXPORT_SYMBOL_GPL(dpaa2_io_service_rearm);

 
int dpaa2_io_service_pull_fq(struct dpaa2_io *d, u32 fqid,
			     struct dpaa2_io_store *s)
{
	struct qbman_pull_desc pd;
	int err;

	qbman_pull_desc_clear(&pd);
	qbman_pull_desc_set_storage(&pd, s->vaddr, s->paddr, 1);
	qbman_pull_desc_set_numframes(&pd, (u8)s->max);
	qbman_pull_desc_set_fq(&pd, fqid);

	d = service_select(d);
	if (!d)
		return -ENODEV;
	s->swp = d->swp;
	err = qbman_swp_pull(d->swp, &pd);
	if (err)
		s->swp = NULL;

	return err;
}
EXPORT_SYMBOL(dpaa2_io_service_pull_fq);

 
int dpaa2_io_service_pull_channel(struct dpaa2_io *d, u32 channelid,
				  struct dpaa2_io_store *s)
{
	struct qbman_pull_desc pd;
	int err;

	qbman_pull_desc_clear(&pd);
	qbman_pull_desc_set_storage(&pd, s->vaddr, s->paddr, 1);
	qbman_pull_desc_set_numframes(&pd, (u8)s->max);
	qbman_pull_desc_set_channel(&pd, channelid, qbman_pull_type_prio);

	d = service_select(d);
	if (!d)
		return -ENODEV;

	s->swp = d->swp;
	err = qbman_swp_pull(d->swp, &pd);
	if (err)
		s->swp = NULL;

	return err;
}
EXPORT_SYMBOL_GPL(dpaa2_io_service_pull_channel);

 
int dpaa2_io_service_enqueue_fq(struct dpaa2_io *d,
				u32 fqid,
				const struct dpaa2_fd *fd)
{
	struct qbman_eq_desc ed;

	d = service_select(d);
	if (!d)
		return -ENODEV;

	qbman_eq_desc_clear(&ed);
	qbman_eq_desc_set_no_orp(&ed, 0);
	qbman_eq_desc_set_fq(&ed, fqid);

	return qbman_swp_enqueue(d->swp, &ed, fd);
}
EXPORT_SYMBOL(dpaa2_io_service_enqueue_fq);

 
int dpaa2_io_service_enqueue_multiple_fq(struct dpaa2_io *d,
				u32 fqid,
				const struct dpaa2_fd *fd,
				int nb)
{
	struct qbman_eq_desc ed;

	d = service_select(d);
	if (!d)
		return -ENODEV;

	qbman_eq_desc_clear(&ed);
	qbman_eq_desc_set_no_orp(&ed, 0);
	qbman_eq_desc_set_fq(&ed, fqid);

	return qbman_swp_enqueue_multiple(d->swp, &ed, fd, NULL, nb);
}
EXPORT_SYMBOL(dpaa2_io_service_enqueue_multiple_fq);

 
int dpaa2_io_service_enqueue_multiple_desc_fq(struct dpaa2_io *d,
				u32 *fqid,
				const struct dpaa2_fd *fd,
				int nb)
{
	struct qbman_eq_desc *ed;
	int i, ret;

	ed = kcalloc(sizeof(struct qbman_eq_desc), 32, GFP_KERNEL);
	if (!ed)
		return -ENOMEM;

	d = service_select(d);
	if (!d) {
		ret = -ENODEV;
		goto out;
	}

	for (i = 0; i < nb; i++) {
		qbman_eq_desc_clear(&ed[i]);
		qbman_eq_desc_set_no_orp(&ed[i], 0);
		qbman_eq_desc_set_fq(&ed[i], fqid[i]);
	}

	ret = qbman_swp_enqueue_multiple_desc(d->swp, &ed[0], fd, nb);
out:
	kfree(ed);
	return ret;
}
EXPORT_SYMBOL(dpaa2_io_service_enqueue_multiple_desc_fq);

 
int dpaa2_io_service_enqueue_qd(struct dpaa2_io *d,
				u32 qdid, u8 prio, u16 qdbin,
				const struct dpaa2_fd *fd)
{
	struct qbman_eq_desc ed;

	d = service_select(d);
	if (!d)
		return -ENODEV;

	qbman_eq_desc_clear(&ed);
	qbman_eq_desc_set_no_orp(&ed, 0);
	qbman_eq_desc_set_qd(&ed, qdid, qdbin, prio);

	return qbman_swp_enqueue(d->swp, &ed, fd);
}
EXPORT_SYMBOL_GPL(dpaa2_io_service_enqueue_qd);

 
int dpaa2_io_service_release(struct dpaa2_io *d,
			     u16 bpid,
			     const u64 *buffers,
			     unsigned int num_buffers)
{
	struct qbman_release_desc rd;

	d = service_select(d);
	if (!d)
		return -ENODEV;

	qbman_release_desc_clear(&rd);
	qbman_release_desc_set_bpid(&rd, bpid);

	return qbman_swp_release(d->swp, &rd, buffers, num_buffers);
}
EXPORT_SYMBOL_GPL(dpaa2_io_service_release);

 
int dpaa2_io_service_acquire(struct dpaa2_io *d,
			     u16 bpid,
			     u64 *buffers,
			     unsigned int num_buffers)
{
	unsigned long irqflags;
	int err;

	d = service_select(d);
	if (!d)
		return -ENODEV;

	spin_lock_irqsave(&d->lock_mgmt_cmd, irqflags);
	err = qbman_swp_acquire(d->swp, bpid, buffers, num_buffers);
	spin_unlock_irqrestore(&d->lock_mgmt_cmd, irqflags);

	return err;
}
EXPORT_SYMBOL_GPL(dpaa2_io_service_acquire);

 

 
struct dpaa2_io_store *dpaa2_io_store_create(unsigned int max_frames,
					     struct device *dev)
{
	struct dpaa2_io_store *ret;
	size_t size;

	if (!max_frames || (max_frames > 32))
		return NULL;

	ret = kmalloc(sizeof(*ret), GFP_KERNEL);
	if (!ret)
		return NULL;

	ret->max = max_frames;
	size = max_frames * sizeof(struct dpaa2_dq) + 64;
	ret->alloced_addr = kzalloc(size, GFP_KERNEL);
	if (!ret->alloced_addr) {
		kfree(ret);
		return NULL;
	}

	ret->vaddr = PTR_ALIGN(ret->alloced_addr, 64);
	ret->paddr = dma_map_single(dev, ret->vaddr,
				    sizeof(struct dpaa2_dq) * max_frames,
				    DMA_FROM_DEVICE);
	if (dma_mapping_error(dev, ret->paddr)) {
		kfree(ret->alloced_addr);
		kfree(ret);
		return NULL;
	}

	ret->idx = 0;
	ret->dev = dev;

	return ret;
}
EXPORT_SYMBOL_GPL(dpaa2_io_store_create);

 
void dpaa2_io_store_destroy(struct dpaa2_io_store *s)
{
	dma_unmap_single(s->dev, s->paddr, sizeof(struct dpaa2_dq) * s->max,
			 DMA_FROM_DEVICE);
	kfree(s->alloced_addr);
	kfree(s);
}
EXPORT_SYMBOL_GPL(dpaa2_io_store_destroy);

 
struct dpaa2_dq *dpaa2_io_store_next(struct dpaa2_io_store *s, int *is_last)
{
	int match;
	struct dpaa2_dq *ret = &s->vaddr[s->idx];

	match = qbman_result_has_new_result(s->swp, ret);
	if (!match) {
		*is_last = 0;
		return NULL;
	}

	s->idx++;

	if (dpaa2_dq_is_pull_complete(ret)) {
		*is_last = 1;
		s->idx = 0;
		 
		if (!(dpaa2_dq_flags(ret) & DPAA2_DQ_STAT_VALIDFRAME))
			ret = NULL;
	} else {
		prefetch(&s->vaddr[s->idx]);
		*is_last = 0;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(dpaa2_io_store_next);

 
int dpaa2_io_query_fq_count(struct dpaa2_io *d, u32 fqid,
			    u32 *fcnt, u32 *bcnt)
{
	struct qbman_fq_query_np_rslt state;
	struct qbman_swp *swp;
	unsigned long irqflags;
	int ret;

	d = service_select(d);
	if (!d)
		return -ENODEV;

	swp = d->swp;
	spin_lock_irqsave(&d->lock_mgmt_cmd, irqflags);
	ret = qbman_fq_query_state(swp, fqid, &state);
	spin_unlock_irqrestore(&d->lock_mgmt_cmd, irqflags);
	if (ret)
		return ret;
	*fcnt = qbman_fq_state_frame_count(&state);
	*bcnt = qbman_fq_state_byte_count(&state);

	return 0;
}
EXPORT_SYMBOL_GPL(dpaa2_io_query_fq_count);

 
int dpaa2_io_query_bp_count(struct dpaa2_io *d, u16 bpid, u32 *num)
{
	struct qbman_bp_query_rslt state;
	struct qbman_swp *swp;
	unsigned long irqflags;
	int ret;

	d = service_select(d);
	if (!d)
		return -ENODEV;

	swp = d->swp;
	spin_lock_irqsave(&d->lock_mgmt_cmd, irqflags);
	ret = qbman_bp_query(swp, bpid, &state);
	spin_unlock_irqrestore(&d->lock_mgmt_cmd, irqflags);
	if (ret)
		return ret;
	*num = qbman_bp_info_num_free_bufs(&state);
	return 0;
}
EXPORT_SYMBOL_GPL(dpaa2_io_query_bp_count);

 
int dpaa2_io_set_irq_coalescing(struct dpaa2_io *d, u32 irq_holdoff)
{
	struct qbman_swp *swp = d->swp;

	return qbman_swp_set_irq_coalescing(swp, swp->dqrr.dqrr_size - 1,
					    irq_holdoff);
}
EXPORT_SYMBOL(dpaa2_io_set_irq_coalescing);

 
void dpaa2_io_get_irq_coalescing(struct dpaa2_io *d, u32 *irq_holdoff)
{
	struct qbman_swp *swp = d->swp;

	qbman_swp_get_irq_coalescing(swp, NULL, irq_holdoff);
}
EXPORT_SYMBOL(dpaa2_io_get_irq_coalescing);

 
void dpaa2_io_set_adaptive_coalescing(struct dpaa2_io *d,
				      int use_adaptive_rx_coalesce)
{
	d->swp->use_adaptive_rx_coalesce = use_adaptive_rx_coalesce;
}
EXPORT_SYMBOL(dpaa2_io_set_adaptive_coalescing);

 
int dpaa2_io_get_adaptive_coalescing(struct dpaa2_io *d)
{
	return d->swp->use_adaptive_rx_coalesce;
}
EXPORT_SYMBOL(dpaa2_io_get_adaptive_coalescing);

 
void dpaa2_io_update_net_dim(struct dpaa2_io *d, __u64 frames, __u64 bytes)
{
	struct dim_sample dim_sample = {};

	if (!d->swp->use_adaptive_rx_coalesce)
		return;

	spin_lock(&d->dim_lock);

	d->bytes += bytes;
	d->frames += frames;

	dim_update_sample(d->event_ctr, d->frames, d->bytes, &dim_sample);
	net_dim(&d->rx_dim, dim_sample);

	spin_unlock(&d->dim_lock);
}
EXPORT_SYMBOL(dpaa2_io_update_net_dim);
