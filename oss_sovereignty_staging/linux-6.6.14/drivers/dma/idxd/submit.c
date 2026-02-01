
 
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <uapi/linux/idxd.h>
#include "idxd.h"
#include "registers.h"

static struct idxd_desc *__get_desc(struct idxd_wq *wq, int idx, int cpu)
{
	struct idxd_desc *desc;
	struct idxd_device *idxd = wq->idxd;

	desc = wq->descs[idx];
	memset(desc->hw, 0, sizeof(struct dsa_hw_desc));
	memset(desc->completion, 0, idxd->data->compl_size);
	desc->cpu = cpu;

	if (device_pasid_enabled(idxd))
		desc->hw->pasid = idxd->pasid;

	return desc;
}

struct idxd_desc *idxd_alloc_desc(struct idxd_wq *wq, enum idxd_op_type optype)
{
	int cpu, idx;
	struct idxd_device *idxd = wq->idxd;
	DEFINE_SBQ_WAIT(wait);
	struct sbq_wait_state *ws;
	struct sbitmap_queue *sbq;

	if (idxd->state != IDXD_DEV_ENABLED)
		return ERR_PTR(-EIO);

	sbq = &wq->sbq;
	idx = sbitmap_queue_get(sbq, &cpu);
	if (idx < 0) {
		if (optype == IDXD_OP_NONBLOCK)
			return ERR_PTR(-EAGAIN);
	} else {
		return __get_desc(wq, idx, cpu);
	}

	ws = &sbq->ws[0];
	for (;;) {
		sbitmap_prepare_to_wait(sbq, ws, &wait, TASK_INTERRUPTIBLE);
		if (signal_pending_state(TASK_INTERRUPTIBLE, current))
			break;
		idx = sbitmap_queue_get(sbq, &cpu);
		if (idx >= 0)
			break;
		schedule();
	}

	sbitmap_finish_wait(sbq, ws, &wait);
	if (idx < 0)
		return ERR_PTR(-EAGAIN);

	return __get_desc(wq, idx, cpu);
}

void idxd_free_desc(struct idxd_wq *wq, struct idxd_desc *desc)
{
	int cpu = desc->cpu;

	desc->cpu = -1;
	sbitmap_queue_clear(&wq->sbq, desc->id, cpu);
}

static struct idxd_desc *list_abort_desc(struct idxd_wq *wq, struct idxd_irq_entry *ie,
					 struct idxd_desc *desc)
{
	struct idxd_desc *d, *n;

	lockdep_assert_held(&ie->list_lock);
	list_for_each_entry_safe(d, n, &ie->work_list, list) {
		if (d == desc) {
			list_del(&d->list);
			return d;
		}
	}

	 
	return NULL;
}

static void llist_abort_desc(struct idxd_wq *wq, struct idxd_irq_entry *ie,
			     struct idxd_desc *desc)
{
	struct idxd_desc *d, *t, *found = NULL;
	struct llist_node *head;
	LIST_HEAD(flist);

	desc->completion->status = IDXD_COMP_DESC_ABORT;
	 
	spin_lock(&ie->list_lock);
	head = llist_del_all(&ie->pending_llist);
	if (head) {
		llist_for_each_entry_safe(d, t, head, llnode) {
			if (d == desc) {
				found = desc;
				continue;
			}

			if (d->completion->status)
				list_add_tail(&d->list, &flist);
			else
				list_add_tail(&d->list, &ie->work_list);
		}
	}

	if (!found)
		found = list_abort_desc(wq, ie, desc);
	spin_unlock(&ie->list_lock);

	if (found)
		idxd_dma_complete_txd(found, IDXD_COMPLETE_ABORT, false);

	 
	list_for_each_entry_safe(d, t, &flist, list) {
		list_del_init(&d->list);
		idxd_dma_complete_txd(found, IDXD_COMPLETE_ABORT, true);
	}
}

 
int idxd_enqcmds(struct idxd_wq *wq, void __iomem *portal, const void *desc)
{
	unsigned int retries = wq->enqcmds_retries;
	int rc;

	do {
		rc = enqcmds(portal, desc);
		if (rc == 0)
			break;
		cpu_relax();
	} while (retries--);

	return rc;
}

int idxd_submit_desc(struct idxd_wq *wq, struct idxd_desc *desc)
{
	struct idxd_device *idxd = wq->idxd;
	struct idxd_irq_entry *ie = NULL;
	u32 desc_flags = desc->hw->flags;
	void __iomem *portal;
	int rc;

	if (idxd->state != IDXD_DEV_ENABLED)
		return -EIO;

	if (!percpu_ref_tryget_live(&wq->wq_active)) {
		wait_for_completion(&wq->wq_resurrect);
		if (!percpu_ref_tryget_live(&wq->wq_active))
			return -ENXIO;
	}

	portal = idxd_wq_portal_addr(wq);

	 
	if (desc_flags & IDXD_OP_FLAG_RCI) {
		ie = &wq->ie;
		desc->hw->int_handle = ie->int_handle;
		llist_add(&desc->llnode, &ie->pending_llist);
	}

	 
	wmb();

	if (wq_dedicated(wq)) {
		iosubmit_cmds512(portal, desc->hw, 1);
	} else {
		rc = idxd_enqcmds(wq, portal, desc->hw);
		if (rc < 0) {
			percpu_ref_put(&wq->wq_active);
			 
			if (ie)
				llist_abort_desc(wq, ie, desc);
			return rc;
		}
	}

	percpu_ref_put(&wq->wq_active);
	return 0;
}
