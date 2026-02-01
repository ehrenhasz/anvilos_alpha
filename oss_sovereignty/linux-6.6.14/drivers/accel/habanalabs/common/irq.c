

 

#include "habanalabs.h"

#include <linux/slab.h>

 
struct hl_eqe_work {
	struct work_struct	eq_work;
	struct hl_device	*hdev;
	struct hl_eq_entry	eq_entry;
};

 
inline u32 hl_cq_inc_ptr(u32 ptr)
{
	ptr++;
	if (unlikely(ptr == HL_CQ_LENGTH))
		ptr = 0;
	return ptr;
}

 
static inline u32 hl_eq_inc_ptr(u32 ptr)
{
	ptr++;
	if (unlikely(ptr == HL_EQ_LENGTH))
		ptr = 0;
	return ptr;
}

static void irq_handle_eqe(struct work_struct *work)
{
	struct hl_eqe_work *eqe_work = container_of(work, struct hl_eqe_work,
							eq_work);
	struct hl_device *hdev = eqe_work->hdev;

	hdev->asic_funcs->handle_eqe(hdev, &eqe_work->eq_entry);

	kfree(eqe_work);
}

 
static void job_finish(struct hl_device *hdev, u32 cs_seq, struct hl_cq *cq, ktime_t timestamp)
{
	struct hl_hw_queue *queue;
	struct hl_cs_job *job;

	queue = &hdev->kernel_queues[cq->hw_queue_id];
	job = queue->shadow_queue[hl_pi_2_offset(cs_seq)];
	job->timestamp = timestamp;
	queue_work(hdev->cq_wq[cq->cq_idx], &job->finish_work);

	atomic_inc(&queue->ci);
}

 
static void cs_finish(struct hl_device *hdev, u16 cs_seq, ktime_t timestamp)
{
	struct asic_fixed_properties *prop = &hdev->asic_prop;
	struct hl_hw_queue *queue;
	struct hl_cs *cs;
	struct hl_cs_job *job;

	cs = hdev->shadow_cs_queue[cs_seq & (prop->max_pending_cs - 1)];
	if (!cs) {
		dev_warn(hdev->dev,
			"No pointer to CS in shadow array at index %d\n",
			cs_seq);
		return;
	}

	list_for_each_entry(job, &cs->job_list, cs_node) {
		queue = &hdev->kernel_queues[job->hw_queue_id];
		atomic_inc(&queue->ci);
	}

	cs->completion_timestamp = timestamp;
	queue_work(hdev->cs_cmplt_wq, &cs->finish_work);
}

 
irqreturn_t hl_irq_handler_cq(int irq, void *arg)
{
	struct hl_cq *cq = arg;
	struct hl_device *hdev = cq->hdev;
	bool shadow_index_valid, entry_ready;
	u16 shadow_index;
	struct hl_cq_entry *cq_entry, *cq_base;
	ktime_t timestamp = ktime_get();

	if (hdev->disabled) {
		dev_dbg(hdev->dev,
			"Device disabled but received IRQ %d for CQ %d\n",
			irq, cq->hw_queue_id);
		return IRQ_HANDLED;
	}

	cq_base = cq->kernel_address;

	while (1) {
		cq_entry = (struct hl_cq_entry *) &cq_base[cq->ci];

		entry_ready = !!FIELD_GET(CQ_ENTRY_READY_MASK,
				le32_to_cpu(cq_entry->data));
		if (!entry_ready)
			break;

		 
		dma_rmb();

		shadow_index_valid =
			!!FIELD_GET(CQ_ENTRY_SHADOW_INDEX_VALID_MASK,
					le32_to_cpu(cq_entry->data));

		shadow_index = FIELD_GET(CQ_ENTRY_SHADOW_INDEX_MASK,
				le32_to_cpu(cq_entry->data));

		 
		if (shadow_index_valid && !hdev->disabled) {
			if (hdev->asic_prop.completion_mode ==
					HL_COMPLETION_MODE_CS)
				cs_finish(hdev, shadow_index, timestamp);
			else
				job_finish(hdev, shadow_index, cq, timestamp);
		}

		 
		cq_entry->data = cpu_to_le32(le32_to_cpu(cq_entry->data) &
						~CQ_ENTRY_READY_MASK);

		cq->ci = hl_cq_inc_ptr(cq->ci);

		 
		atomic_inc(&cq->free_slots_cnt);
	}

	return IRQ_HANDLED;
}

 
static void hl_ts_free_objects(struct work_struct *work)
{
	struct timestamp_reg_work_obj *job =
			container_of(work, struct timestamp_reg_work_obj, free_obj);
	struct timestamp_reg_free_node *free_obj, *temp_free_obj;
	struct list_head *free_list_head = job->free_obj_head;
	struct hl_device *hdev = job->hdev;

	list_for_each_entry_safe(free_obj, temp_free_obj, free_list_head, free_objects_node) {
		dev_dbg(hdev->dev, "About to put refcount to buf (%p) cq_cb(%p)\n",
					free_obj->buf,
					free_obj->cq_cb);

		hl_mmap_mem_buf_put(free_obj->buf);
		hl_cb_put(free_obj->cq_cb);
		kfree(free_obj);
	}

	kfree(free_list_head);
	kfree(job);
}

 
static int handle_registration_node(struct hl_device *hdev, struct hl_user_pending_interrupt *pend,
						struct list_head **free_list, ktime_t now)
{
	struct timestamp_reg_free_node *free_node;
	u64 timestamp;

	if (!(*free_list)) {
		 
		*free_list = kmalloc(sizeof(struct list_head), GFP_ATOMIC);
		if (!(*free_list))
			return -ENOMEM;

		INIT_LIST_HEAD(*free_list);
	}

	free_node = kmalloc(sizeof(*free_node), GFP_ATOMIC);
	if (!free_node)
		return -ENOMEM;

	timestamp = ktime_to_ns(now);

	*pend->ts_reg_info.timestamp_kernel_addr = timestamp;

	dev_dbg(hdev->dev, "Timestamp is set to ts cb address (%p), ts: 0x%llx\n",
			pend->ts_reg_info.timestamp_kernel_addr,
			*(u64 *)pend->ts_reg_info.timestamp_kernel_addr);

	list_del(&pend->wait_list_node);

	 
	pend->ts_reg_info.in_use = 0;

	 
	free_node->buf = pend->ts_reg_info.buf;
	free_node->cq_cb = pend->ts_reg_info.cq_cb;
	list_add(&free_node->free_objects_node, *free_list);

	return 0;
}

static void handle_user_interrupt(struct hl_device *hdev, struct hl_user_interrupt *intr)
{
	struct hl_user_pending_interrupt *pend, *temp_pend;
	struct list_head *ts_reg_free_list_head = NULL;
	struct timestamp_reg_work_obj *job;
	bool reg_node_handle_fail = false;
	int rc;

	 
	job = kmalloc(sizeof(*job), GFP_ATOMIC);
	if (!job)
		return;

	spin_lock(&intr->wait_list_lock);
	list_for_each_entry_safe(pend, temp_pend, &intr->wait_list_head, wait_list_node) {
		if ((pend->cq_kernel_addr && *(pend->cq_kernel_addr) >= pend->cq_target_value) ||
				!pend->cq_kernel_addr) {
			if (pend->ts_reg_info.buf) {
				if (!reg_node_handle_fail) {
					rc = handle_registration_node(hdev, pend,
							&ts_reg_free_list_head, intr->timestamp);
					if (rc)
						reg_node_handle_fail = true;
				}
			} else {
				 
				pend->fence.timestamp = intr->timestamp;
				complete_all(&pend->fence.completion);
			}
		}
	}
	spin_unlock(&intr->wait_list_lock);

	if (ts_reg_free_list_head) {
		INIT_WORK(&job->free_obj, hl_ts_free_objects);
		job->free_obj_head = ts_reg_free_list_head;
		job->hdev = hdev;
		queue_work(hdev->ts_free_obj_wq, &job->free_obj);
	} else {
		kfree(job);
	}
}

static void handle_tpc_interrupt(struct hl_device *hdev)
{
	u64 event_mask;
	u32 flags;

	event_mask = HL_NOTIFIER_EVENT_TPC_ASSERT |
		HL_NOTIFIER_EVENT_USER_ENGINE_ERR |
		HL_NOTIFIER_EVENT_DEVICE_RESET;

	flags = HL_DRV_RESET_DELAY;

	dev_err_ratelimited(hdev->dev, "Received TPC assert\n");
	hl_device_cond_reset(hdev, flags, event_mask);
}

static void handle_unexpected_user_interrupt(struct hl_device *hdev)
{
	dev_err_ratelimited(hdev->dev, "Received unexpected user error interrupt\n");
}

 
irqreturn_t hl_irq_handler_user_interrupt(int irq, void *arg)
{
	struct hl_user_interrupt *user_int = arg;

	user_int->timestamp = ktime_get();

	return IRQ_WAKE_THREAD;
}

 
irqreturn_t hl_irq_user_interrupt_thread_handler(int irq, void *arg)
{
	struct hl_user_interrupt *user_int = arg;
	struct hl_device *hdev = user_int->hdev;

	switch (user_int->type) {
	case HL_USR_INTERRUPT_CQ:
		handle_user_interrupt(hdev, &hdev->common_user_cq_interrupt);

		 
		handle_user_interrupt(hdev, user_int);
		break;
	case HL_USR_INTERRUPT_DECODER:
		handle_user_interrupt(hdev, &hdev->common_decoder_interrupt);

		 
		handle_user_interrupt(hdev, user_int);
		break;
	case HL_USR_INTERRUPT_TPC:
		handle_tpc_interrupt(hdev);
		break;
	case HL_USR_INTERRUPT_UNEXPECTED:
		handle_unexpected_user_interrupt(hdev);
		break;
	default:
		break;
	}

	return IRQ_HANDLED;
}

 
irqreturn_t hl_irq_handler_eq(int irq, void *arg)
{
	struct hl_eq *eq = arg;
	struct hl_device *hdev = eq->hdev;
	struct hl_eq_entry *eq_entry;
	struct hl_eq_entry *eq_base;
	struct hl_eqe_work *handle_eqe_work;
	bool entry_ready;
	u32 cur_eqe, ctl;
	u16 cur_eqe_index, event_type;

	eq_base = eq->kernel_address;

	while (1) {
		cur_eqe = le32_to_cpu(eq_base[eq->ci].hdr.ctl);
		entry_ready = !!FIELD_GET(EQ_CTL_READY_MASK, cur_eqe);

		if (!entry_ready)
			break;

		cur_eqe_index = FIELD_GET(EQ_CTL_INDEX_MASK, cur_eqe);
		if ((hdev->event_queue.check_eqe_index) &&
				(((eq->prev_eqe_index + 1) & EQ_CTL_INDEX_MASK) != cur_eqe_index)) {
			dev_err(hdev->dev,
				"EQE %#x in queue is ready but index does not match %d!=%d",
				cur_eqe,
				((eq->prev_eqe_index + 1) & EQ_CTL_INDEX_MASK),
				cur_eqe_index);
			break;
		}

		eq->prev_eqe_index++;

		eq_entry = &eq_base[eq->ci];

		 
		dma_rmb();

		if (hdev->disabled && !hdev->reset_info.in_compute_reset) {
			ctl = le32_to_cpu(eq_entry->hdr.ctl);
			event_type = ((ctl & EQ_CTL_EVENT_TYPE_MASK) >> EQ_CTL_EVENT_TYPE_SHIFT);
			dev_warn(hdev->dev,
				"Device disabled but received an EQ event (%u)\n", event_type);
			goto skip_irq;
		}

		handle_eqe_work = kmalloc(sizeof(*handle_eqe_work), GFP_ATOMIC);
		if (handle_eqe_work) {
			INIT_WORK(&handle_eqe_work->eq_work, irq_handle_eqe);
			handle_eqe_work->hdev = hdev;

			memcpy(&handle_eqe_work->eq_entry, eq_entry,
					sizeof(*eq_entry));

			queue_work(hdev->eq_wq, &handle_eqe_work->eq_work);
		}
skip_irq:
		 
		eq_entry->hdr.ctl =
			cpu_to_le32(le32_to_cpu(eq_entry->hdr.ctl) &
							~EQ_CTL_READY_MASK);

		eq->ci = hl_eq_inc_ptr(eq->ci);

		hdev->asic_funcs->update_eq_ci(hdev, eq->ci);
	}

	return IRQ_HANDLED;
}

 
irqreturn_t hl_irq_handler_dec_abnrm(int irq, void *arg)
{
	struct hl_dec *dec = arg;

	schedule_work(&dec->abnrm_intr_work);

	return IRQ_HANDLED;
}

 
int hl_cq_init(struct hl_device *hdev, struct hl_cq *q, u32 hw_queue_id)
{
	void *p;

	p = hl_asic_dma_alloc_coherent(hdev, HL_CQ_SIZE_IN_BYTES, &q->bus_address,
					GFP_KERNEL | __GFP_ZERO);
	if (!p)
		return -ENOMEM;

	q->hdev = hdev;
	q->kernel_address = p;
	q->hw_queue_id = hw_queue_id;
	q->ci = 0;
	q->pi = 0;

	atomic_set(&q->free_slots_cnt, HL_CQ_LENGTH);

	return 0;
}

 
void hl_cq_fini(struct hl_device *hdev, struct hl_cq *q)
{
	hl_asic_dma_free_coherent(hdev, HL_CQ_SIZE_IN_BYTES, q->kernel_address, q->bus_address);
}

void hl_cq_reset(struct hl_device *hdev, struct hl_cq *q)
{
	q->ci = 0;
	q->pi = 0;

	atomic_set(&q->free_slots_cnt, HL_CQ_LENGTH);

	 

	memset(q->kernel_address, 0, HL_CQ_SIZE_IN_BYTES);
}

 
int hl_eq_init(struct hl_device *hdev, struct hl_eq *q)
{
	void *p;

	p = hl_cpu_accessible_dma_pool_alloc(hdev, HL_EQ_SIZE_IN_BYTES, &q->bus_address);
	if (!p)
		return -ENOMEM;

	q->hdev = hdev;
	q->kernel_address = p;
	q->ci = 0;
	q->prev_eqe_index = 0;

	return 0;
}

 
void hl_eq_fini(struct hl_device *hdev, struct hl_eq *q)
{
	flush_workqueue(hdev->eq_wq);

	hl_cpu_accessible_dma_pool_free(hdev, HL_EQ_SIZE_IN_BYTES, q->kernel_address);
}

void hl_eq_reset(struct hl_device *hdev, struct hl_eq *q)
{
	q->ci = 0;
	q->prev_eqe_index = 0;

	 

	memset(q->kernel_address, 0, HL_EQ_SIZE_IN_BYTES);
}
