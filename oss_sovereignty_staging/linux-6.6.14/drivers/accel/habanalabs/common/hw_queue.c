

 

#include "habanalabs.h"

#include <linux/slab.h>

 
inline u32 hl_hw_queue_add_ptr(u32 ptr, u16 val)
{
	ptr += val;
	ptr &= ((HL_QUEUE_LENGTH << 1) - 1);
	return ptr;
}
static inline int queue_ci_get(atomic_t *ci, u32 queue_len)
{
	return atomic_read(ci) & ((queue_len << 1) - 1);
}

static inline int queue_free_slots(struct hl_hw_queue *q, u32 queue_len)
{
	int delta = (q->pi - queue_ci_get(&q->ci, queue_len));

	if (delta >= 0)
		return (queue_len - delta);
	else
		return (abs(delta) - queue_len);
}

void hl_hw_queue_update_ci(struct hl_cs *cs)
{
	struct hl_device *hdev = cs->ctx->hdev;
	struct hl_hw_queue *q;
	int i;

	if (hdev->disabled)
		return;

	q = &hdev->kernel_queues[0];

	 
	if (!hdev->asic_prop.max_queues || q->queue_type == QUEUE_TYPE_HW)
		return;

	 
	for (i = 0 ; i < hdev->asic_prop.max_queues ; i++, q++) {
		if (!cs_needs_completion(cs) || q->queue_type == QUEUE_TYPE_INT)
			atomic_add(cs->jobs_in_queue_cnt[i], &q->ci);
	}
}

 
void hl_hw_queue_submit_bd(struct hl_device *hdev, struct hl_hw_queue *q,
		u32 ctl, u32 len, u64 ptr)
{
	struct hl_bd *bd;

	bd = q->kernel_address;
	bd += hl_pi_2_offset(q->pi);
	bd->ctl = cpu_to_le32(ctl);
	bd->len = cpu_to_le32(len);
	bd->ptr = cpu_to_le64(ptr);

	q->pi = hl_queue_inc_ptr(q->pi);
	hdev->asic_funcs->ring_doorbell(hdev, q->hw_queue_id, q->pi);
}

 
static int ext_queue_sanity_checks(struct hl_device *hdev,
				struct hl_hw_queue *q, int num_of_entries,
				bool reserve_cq_entry)
{
	atomic_t *free_slots =
			&hdev->completion_queue[q->cq_id].free_slots_cnt;
	int free_slots_cnt;

	 
	free_slots_cnt = queue_free_slots(q, HL_QUEUE_LENGTH);

	if (free_slots_cnt < num_of_entries) {
		dev_dbg(hdev->dev, "Queue %d doesn't have room for %d CBs\n",
			q->hw_queue_id, num_of_entries);
		return -EAGAIN;
	}

	if (reserve_cq_entry) {
		 
		if (atomic_add_negative(num_of_entries * -1, free_slots)) {
			dev_dbg(hdev->dev, "No space for %d on CQ %d\n",
				num_of_entries, q->hw_queue_id);
			atomic_add(num_of_entries, free_slots);
			return -EAGAIN;
		}
	}

	return 0;
}

 
static int int_queue_sanity_checks(struct hl_device *hdev,
					struct hl_hw_queue *q,
					int num_of_entries)
{
	int free_slots_cnt;

	if (num_of_entries > q->int_queue_len) {
		dev_err(hdev->dev,
			"Cannot populate queue %u with %u jobs\n",
			q->hw_queue_id, num_of_entries);
		return -ENOMEM;
	}

	 
	free_slots_cnt = queue_free_slots(q, q->int_queue_len);

	if (free_slots_cnt < num_of_entries) {
		dev_dbg(hdev->dev, "Queue %d doesn't have room for %d CBs\n",
			q->hw_queue_id, num_of_entries);
		return -EAGAIN;
	}

	return 0;
}

 
static int hw_queue_sanity_checks(struct hl_device *hdev, struct hl_hw_queue *q,
					int num_of_entries)
{
	int free_slots_cnt;

	 
	free_slots_cnt = queue_free_slots(q, HL_QUEUE_LENGTH);

	if (free_slots_cnt < num_of_entries) {
		dev_dbg(hdev->dev, "Queue %d doesn't have room for %d CBs\n",
			q->hw_queue_id, num_of_entries);
		return -EAGAIN;
	}

	return 0;
}

 
int hl_hw_queue_send_cb_no_cmpl(struct hl_device *hdev, u32 hw_queue_id,
				u32 cb_size, u64 cb_ptr)
{
	struct hl_hw_queue *q = &hdev->kernel_queues[hw_queue_id];
	int rc = 0;

	hdev->asic_funcs->hw_queues_lock(hdev);

	if (hdev->disabled) {
		rc = -EPERM;
		goto out;
	}

	 
	if (q->queue_type != QUEUE_TYPE_HW) {
		rc = ext_queue_sanity_checks(hdev, q, 1, false);
		if (rc)
			goto out;
	}

	hl_hw_queue_submit_bd(hdev, q, 0, cb_size, cb_ptr);

out:
	hdev->asic_funcs->hw_queues_unlock(hdev);

	return rc;
}

 
static void ext_queue_schedule_job(struct hl_cs_job *job)
{
	struct hl_device *hdev = job->cs->ctx->hdev;
	struct hl_hw_queue *q = &hdev->kernel_queues[job->hw_queue_id];
	struct hl_cq_entry cq_pkt;
	struct hl_cq *cq;
	u64 cq_addr;
	struct hl_cb *cb;
	u32 ctl;
	u32 len;
	u64 ptr;

	 
	ctl = ((q->pi << BD_CTL_SHADOW_INDEX_SHIFT) & BD_CTL_SHADOW_INDEX_MASK);

	cb = job->patched_cb;
	len = job->job_cb_size;
	ptr = cb->bus_address;

	 
	if (!cs_needs_completion(job->cs))
		goto submit_bd;

	cq_pkt.data = cpu_to_le32(
			((q->pi << CQ_ENTRY_SHADOW_INDEX_SHIFT)
				& CQ_ENTRY_SHADOW_INDEX_MASK) |
			FIELD_PREP(CQ_ENTRY_SHADOW_INDEX_VALID_MASK, 1) |
			FIELD_PREP(CQ_ENTRY_READY_MASK, 1));

	 
	cq = &hdev->completion_queue[q->cq_id];
	cq_addr = cq->bus_address + cq->pi * sizeof(struct hl_cq_entry);

	hdev->asic_funcs->add_end_of_cb_packets(hdev, cb->kernel_address, len,
						job->user_cb_size,
						cq_addr,
						le32_to_cpu(cq_pkt.data),
						q->msi_vec,
						job->contains_dma_pkt);

	q->shadow_queue[hl_pi_2_offset(q->pi)] = job;

	cq->pi = hl_cq_inc_ptr(cq->pi);

submit_bd:
	hl_hw_queue_submit_bd(hdev, q, ctl, len, ptr);
}

 
static void int_queue_schedule_job(struct hl_cs_job *job)
{
	struct hl_device *hdev = job->cs->ctx->hdev;
	struct hl_hw_queue *q = &hdev->kernel_queues[job->hw_queue_id];
	struct hl_bd bd;
	__le64 *pi;

	bd.ctl = 0;
	bd.len = cpu_to_le32(job->job_cb_size);

	if (job->is_kernel_allocated_cb)
		 
		bd.ptr = cpu_to_le64(job->user_cb->bus_address);
	else
		bd.ptr = cpu_to_le64((u64) (uintptr_t) job->user_cb);

	pi = q->kernel_address + (q->pi & (q->int_queue_len - 1)) * sizeof(bd);

	q->pi++;
	q->pi &= ((q->int_queue_len << 1) - 1);

	hdev->asic_funcs->pqe_write(hdev, pi, &bd);

	hdev->asic_funcs->ring_doorbell(hdev, q->hw_queue_id, q->pi);
}

 
static void hw_queue_schedule_job(struct hl_cs_job *job)
{
	struct hl_device *hdev = job->cs->ctx->hdev;
	struct hl_hw_queue *q = &hdev->kernel_queues[job->hw_queue_id];
	u64 ptr;
	u32 offset, ctl, len;

	 
	offset = job->cs->sequence & (hdev->asic_prop.max_pending_cs - 1);
	ctl = ((offset << BD_CTL_COMP_OFFSET_SHIFT) & BD_CTL_COMP_OFFSET_MASK) |
		((q->pi << BD_CTL_COMP_DATA_SHIFT) & BD_CTL_COMP_DATA_MASK);

	len = job->job_cb_size;

	 
	if (job->patched_cb)
		ptr = job->patched_cb->bus_address;
	else if (job->is_kernel_allocated_cb)
		ptr = job->user_cb->bus_address;
	else
		ptr = (u64) (uintptr_t) job->user_cb;

	hl_hw_queue_submit_bd(hdev, q, ctl, len, ptr);
}

static int init_signal_cs(struct hl_device *hdev,
		struct hl_cs_job *job, struct hl_cs_compl *cs_cmpl)
{
	struct hl_sync_stream_properties *prop;
	struct hl_hw_sob *hw_sob;
	u32 q_idx;
	int rc = 0;

	q_idx = job->hw_queue_id;
	prop = &hdev->kernel_queues[q_idx].sync_stream_prop;
	hw_sob = &prop->hw_sob[prop->curr_sob_offset];

	cs_cmpl->hw_sob = hw_sob;
	cs_cmpl->sob_val = prop->next_sob_val;

	dev_dbg(hdev->dev,
		"generate signal CB, sob_id: %d, sob val: %u, q_idx: %d, seq: %llu\n",
		cs_cmpl->hw_sob->sob_id, cs_cmpl->sob_val, q_idx,
		cs_cmpl->cs_seq);

	 
	hdev->asic_funcs->gen_signal_cb(hdev, job->patched_cb,
				cs_cmpl->hw_sob->sob_id, 0, true);

	rc = hl_cs_signal_sob_wraparound_handler(hdev, q_idx, &hw_sob, 1,
								false);

	job->cs->sob_addr_offset = hw_sob->sob_addr;
	job->cs->initial_sob_count = prop->next_sob_val - 1;

	return rc;
}

void hl_hw_queue_encaps_sig_set_sob_info(struct hl_device *hdev,
			struct hl_cs *cs, struct hl_cs_job *job,
			struct hl_cs_compl *cs_cmpl)
{
	struct hl_cs_encaps_sig_handle *handle = cs->encaps_sig_hdl;
	u32 offset = 0;

	cs_cmpl->hw_sob = handle->hw_sob;

	 
	if (job->encaps_sig_wait_offset)
		offset = job->encaps_sig_wait_offset - 1;

	cs_cmpl->sob_val = handle->pre_sob_val + offset;
}

static int init_wait_cs(struct hl_device *hdev, struct hl_cs *cs,
		struct hl_cs_job *job, struct hl_cs_compl *cs_cmpl)
{
	struct hl_gen_wait_properties wait_prop;
	struct hl_sync_stream_properties *prop;
	struct hl_cs_compl *signal_cs_cmpl;
	u32 q_idx;

	q_idx = job->hw_queue_id;
	prop = &hdev->kernel_queues[q_idx].sync_stream_prop;

	signal_cs_cmpl = container_of(cs->signal_fence,
					struct hl_cs_compl,
					base_fence);

	if (cs->encaps_signals) {
		 
		hl_hw_queue_encaps_sig_set_sob_info(hdev, cs, job, cs_cmpl);

		dev_dbg(hdev->dev, "Wait for encaps signals handle, qidx(%u), CS sequence(%llu), sob val: 0x%x, offset: %u\n",
				cs->encaps_sig_hdl->q_idx,
				cs->encaps_sig_hdl->cs_seq,
				cs_cmpl->sob_val,
				job->encaps_sig_wait_offset);
	} else {
		 
		cs_cmpl->hw_sob = signal_cs_cmpl->hw_sob;
		cs_cmpl->sob_val = signal_cs_cmpl->sob_val;
	}

	 
	spin_lock(&signal_cs_cmpl->lock);

	if (completion_done(&cs->signal_fence->completion)) {
		spin_unlock(&signal_cs_cmpl->lock);
		return -EINVAL;
	}

	kref_get(&cs_cmpl->hw_sob->kref);

	spin_unlock(&signal_cs_cmpl->lock);

	dev_dbg(hdev->dev,
		"generate wait CB, sob_id: %d, sob_val: 0x%x, mon_id: %d, q_idx: %d, seq: %llu\n",
		cs_cmpl->hw_sob->sob_id, cs_cmpl->sob_val,
		prop->base_mon_id, q_idx, cs->sequence);

	wait_prop.data = (void *) job->patched_cb;
	wait_prop.sob_base = cs_cmpl->hw_sob->sob_id;
	wait_prop.sob_mask = 0x1;
	wait_prop.sob_val = cs_cmpl->sob_val;
	wait_prop.mon_id = prop->base_mon_id;
	wait_prop.q_idx = q_idx;
	wait_prop.size = 0;

	hdev->asic_funcs->gen_wait_cb(hdev, &wait_prop);

	mb();
	hl_fence_put(cs->signal_fence);
	cs->signal_fence = NULL;

	return 0;
}

 
static int init_signal_wait_cs(struct hl_cs *cs)
{
	struct hl_ctx *ctx = cs->ctx;
	struct hl_device *hdev = ctx->hdev;
	struct hl_cs_job *job;
	struct hl_cs_compl *cs_cmpl =
			container_of(cs->fence, struct hl_cs_compl, base_fence);
	int rc = 0;

	 
	job = list_first_entry(&cs->job_list, struct hl_cs_job,
				cs_node);

	if (cs->type & CS_TYPE_SIGNAL)
		rc = init_signal_cs(hdev, job, cs_cmpl);
	else if (cs->type & CS_TYPE_WAIT)
		rc = init_wait_cs(hdev, cs, job, cs_cmpl);

	return rc;
}

static int encaps_sig_first_staged_cs_handler
			(struct hl_device *hdev, struct hl_cs *cs)
{
	struct hl_cs_compl *cs_cmpl =
			container_of(cs->fence,
					struct hl_cs_compl, base_fence);
	struct hl_cs_encaps_sig_handle *encaps_sig_hdl;
	struct hl_encaps_signals_mgr *mgr;
	int rc = 0;

	mgr = &cs->ctx->sig_mgr;

	spin_lock(&mgr->lock);
	encaps_sig_hdl = idr_find(&mgr->handles, cs->encaps_sig_hdl_id);
	if (encaps_sig_hdl) {
		 
		encaps_sig_hdl->cs_seq = cs->sequence;
		 
		cs_cmpl->encaps_signals = true;
		cs_cmpl->encaps_sig_hdl = encaps_sig_hdl;

		 
		cs_cmpl->hw_sob = encaps_sig_hdl->hw_sob;
		cs_cmpl->sob_val = encaps_sig_hdl->pre_sob_val +
						encaps_sig_hdl->count;

		dev_dbg(hdev->dev, "CS seq (%llu) added to encaps signal handler id (%u), count(%u), qidx(%u), sob(%u), val(%u)\n",
				cs->sequence, encaps_sig_hdl->id,
				encaps_sig_hdl->count,
				encaps_sig_hdl->q_idx,
				cs_cmpl->hw_sob->sob_id,
				cs_cmpl->sob_val);

	} else {
		dev_err(hdev->dev, "encaps handle id(%u) wasn't found!\n",
				cs->encaps_sig_hdl_id);
		rc = -EINVAL;
	}

	spin_unlock(&mgr->lock);

	return rc;
}

 
int hl_hw_queue_schedule_cs(struct hl_cs *cs)
{
	enum hl_device_status status;
	struct hl_cs_counters_atomic *cntr;
	struct hl_ctx *ctx = cs->ctx;
	struct hl_device *hdev = ctx->hdev;
	struct hl_cs_job *job, *tmp;
	struct hl_hw_queue *q;
	int rc = 0, i, cq_cnt;
	bool first_entry;
	u32 max_queues;

	cntr = &hdev->aggregated_cs_counters;

	hdev->asic_funcs->hw_queues_lock(hdev);

	if (!hl_device_operational(hdev, &status)) {
		atomic64_inc(&cntr->device_in_reset_drop_cnt);
		atomic64_inc(&ctx->cs_counters.device_in_reset_drop_cnt);
		dev_err(hdev->dev,
			"device is %s, CS rejected!\n", hdev->status[status]);
		rc = -EPERM;
		goto out;
	}

	max_queues = hdev->asic_prop.max_queues;

	q = &hdev->kernel_queues[0];
	for (i = 0, cq_cnt = 0 ; i < max_queues ; i++, q++) {
		if (cs->jobs_in_queue_cnt[i]) {
			switch (q->queue_type) {
			case QUEUE_TYPE_EXT:
				rc = ext_queue_sanity_checks(hdev, q,
						cs->jobs_in_queue_cnt[i],
						cs_needs_completion(cs) ?
								true : false);
				break;
			case QUEUE_TYPE_INT:
				rc = int_queue_sanity_checks(hdev, q,
						cs->jobs_in_queue_cnt[i]);
				break;
			case QUEUE_TYPE_HW:
				rc = hw_queue_sanity_checks(hdev, q,
						cs->jobs_in_queue_cnt[i]);
				break;
			default:
				dev_err(hdev->dev, "Queue type %d is invalid\n",
					q->queue_type);
				rc = -EINVAL;
				break;
			}

			if (rc) {
				atomic64_inc(
					&ctx->cs_counters.queue_full_drop_cnt);
				atomic64_inc(&cntr->queue_full_drop_cnt);
				goto unroll_cq_resv;
			}

			if (q->queue_type == QUEUE_TYPE_EXT)
				cq_cnt++;
		}
	}

	if ((cs->type == CS_TYPE_SIGNAL) || (cs->type == CS_TYPE_WAIT)) {
		rc = init_signal_wait_cs(cs);
		if (rc)
			goto unroll_cq_resv;
	} else if (cs->type == CS_TYPE_COLLECTIVE_WAIT) {
		rc = hdev->asic_funcs->collective_wait_init_cs(cs);
		if (rc)
			goto unroll_cq_resv;
	}

	rc = hdev->asic_funcs->pre_schedule_cs(cs);
	if (rc) {
		dev_err(hdev->dev,
			"Failed in pre-submission operations of CS %d.%llu\n",
			ctx->asid, cs->sequence);
		goto unroll_cq_resv;
	}

	hdev->shadow_cs_queue[cs->sequence &
				(hdev->asic_prop.max_pending_cs - 1)] = cs;

	if (cs->encaps_signals && cs->staged_first) {
		rc = encaps_sig_first_staged_cs_handler(hdev, cs);
		if (rc)
			goto unroll_cq_resv;
	}

	spin_lock(&hdev->cs_mirror_lock);

	 
	if (cs->staged_cs && !cs->staged_first) {
		struct hl_cs *staged_cs;

		staged_cs = hl_staged_cs_find_first(hdev, cs->staged_sequence);
		if (!staged_cs) {
			dev_err(hdev->dev,
				"Cannot find staged submission sequence %llu",
				cs->staged_sequence);
			rc = -EINVAL;
			goto unlock_cs_mirror;
		}

		if (is_staged_cs_last_exists(hdev, staged_cs)) {
			dev_err(hdev->dev,
				"Staged submission sequence %llu already submitted",
				cs->staged_sequence);
			rc = -EINVAL;
			goto unlock_cs_mirror;
		}

		list_add_tail(&cs->staged_cs_node, &staged_cs->staged_cs_node);

		 
		if (hdev->supports_wait_for_multi_cs)
			staged_cs->fence->stream_master_qid_map |=
					cs->fence->stream_master_qid_map;
	}

	list_add_tail(&cs->mirror_node, &hdev->cs_mirror_list);

	 
	first_entry = list_first_entry(&hdev->cs_mirror_list,
					struct hl_cs, mirror_node) == cs;
	if ((hdev->timeout_jiffies != MAX_SCHEDULE_TIMEOUT) &&
				first_entry && cs_needs_timeout(cs)) {
		cs->tdr_active = true;
		schedule_delayed_work(&cs->work_tdr, cs->timeout_jiffies);

	}

	spin_unlock(&hdev->cs_mirror_lock);

	list_for_each_entry_safe(job, tmp, &cs->job_list, cs_node)
		switch (job->queue_type) {
		case QUEUE_TYPE_EXT:
			ext_queue_schedule_job(job);
			break;
		case QUEUE_TYPE_INT:
			int_queue_schedule_job(job);
			break;
		case QUEUE_TYPE_HW:
			hw_queue_schedule_job(job);
			break;
		default:
			break;
		}

	cs->submitted = true;

	goto out;

unlock_cs_mirror:
	spin_unlock(&hdev->cs_mirror_lock);
unroll_cq_resv:
	q = &hdev->kernel_queues[0];
	for (i = 0 ; (i < max_queues) && (cq_cnt > 0) ; i++, q++) {
		if ((q->queue_type == QUEUE_TYPE_EXT) &&
						(cs->jobs_in_queue_cnt[i])) {
			atomic_t *free_slots =
				&hdev->completion_queue[i].free_slots_cnt;
			atomic_add(cs->jobs_in_queue_cnt[i], free_slots);
			cq_cnt--;
		}
	}

out:
	hdev->asic_funcs->hw_queues_unlock(hdev);

	return rc;
}

 
void hl_hw_queue_inc_ci_kernel(struct hl_device *hdev, u32 hw_queue_id)
{
	struct hl_hw_queue *q = &hdev->kernel_queues[hw_queue_id];

	atomic_inc(&q->ci);
}

static int ext_and_cpu_queue_init(struct hl_device *hdev, struct hl_hw_queue *q,
					bool is_cpu_queue)
{
	void *p;
	int rc;

	if (is_cpu_queue)
		p = hl_cpu_accessible_dma_pool_alloc(hdev, HL_QUEUE_SIZE_IN_BYTES, &q->bus_address);
	else
		p = hl_asic_dma_alloc_coherent(hdev, HL_QUEUE_SIZE_IN_BYTES, &q->bus_address,
						GFP_KERNEL | __GFP_ZERO);
	if (!p)
		return -ENOMEM;

	q->kernel_address = p;

	q->shadow_queue = kmalloc_array(HL_QUEUE_LENGTH, sizeof(struct hl_cs_job *), GFP_KERNEL);
	if (!q->shadow_queue) {
		dev_err(hdev->dev,
			"Failed to allocate shadow queue for H/W queue %d\n",
			q->hw_queue_id);
		rc = -ENOMEM;
		goto free_queue;
	}

	 
	atomic_set(&q->ci, 0);
	q->pi = 0;

	return 0;

free_queue:
	if (is_cpu_queue)
		hl_cpu_accessible_dma_pool_free(hdev, HL_QUEUE_SIZE_IN_BYTES, q->kernel_address);
	else
		hl_asic_dma_free_coherent(hdev, HL_QUEUE_SIZE_IN_BYTES, q->kernel_address,
						q->bus_address);

	return rc;
}

static int int_queue_init(struct hl_device *hdev, struct hl_hw_queue *q)
{
	void *p;

	p = hdev->asic_funcs->get_int_queue_base(hdev, q->hw_queue_id,
					&q->bus_address, &q->int_queue_len);
	if (!p) {
		dev_err(hdev->dev,
			"Failed to get base address for internal queue %d\n",
			q->hw_queue_id);
		return -EFAULT;
	}

	q->kernel_address = p;
	q->pi = 0;
	atomic_set(&q->ci, 0);

	return 0;
}

static int cpu_queue_init(struct hl_device *hdev, struct hl_hw_queue *q)
{
	return ext_and_cpu_queue_init(hdev, q, true);
}

static int ext_queue_init(struct hl_device *hdev, struct hl_hw_queue *q)
{
	return ext_and_cpu_queue_init(hdev, q, false);
}

static int hw_queue_init(struct hl_device *hdev, struct hl_hw_queue *q)
{
	void *p;

	p = hl_asic_dma_alloc_coherent(hdev, HL_QUEUE_SIZE_IN_BYTES, &q->bus_address,
					GFP_KERNEL | __GFP_ZERO);
	if (!p)
		return -ENOMEM;

	q->kernel_address = p;

	 
	atomic_set(&q->ci, 0);
	q->pi = 0;

	return 0;
}

static void sync_stream_queue_init(struct hl_device *hdev, u32 q_idx)
{
	struct hl_sync_stream_properties *sync_stream_prop;
	struct asic_fixed_properties *prop = &hdev->asic_prop;
	struct hl_hw_sob *hw_sob;
	int sob, reserved_mon_idx, queue_idx;

	sync_stream_prop = &hdev->kernel_queues[q_idx].sync_stream_prop;

	 
	if (hdev->kernel_queues[q_idx].collective_mode ==
			HL_COLLECTIVE_MASTER) {
		reserved_mon_idx = hdev->collective_mon_idx;

		 
		sync_stream_prop->collective_mstr_mon_id[0] =
			prop->collective_first_mon + reserved_mon_idx;

		 
		sync_stream_prop->collective_mstr_mon_id[1] =
			prop->collective_first_mon + reserved_mon_idx + 1;

		hdev->collective_mon_idx += HL_COLLECTIVE_RSVD_MSTR_MONS;
	} else if (hdev->kernel_queues[q_idx].collective_mode ==
			HL_COLLECTIVE_SLAVE) {
		reserved_mon_idx = hdev->collective_mon_idx++;

		 
		sync_stream_prop->collective_slave_mon_id =
			prop->collective_first_mon + reserved_mon_idx;
	}

	if (!hdev->kernel_queues[q_idx].supports_sync_stream)
		return;

	queue_idx = hdev->sync_stream_queue_idx++;

	sync_stream_prop->base_sob_id = prop->sync_stream_first_sob +
			(queue_idx * HL_RSVD_SOBS);
	sync_stream_prop->base_mon_id = prop->sync_stream_first_mon +
			(queue_idx * HL_RSVD_MONS);
	sync_stream_prop->next_sob_val = 1;
	sync_stream_prop->curr_sob_offset = 0;

	for (sob = 0 ; sob < HL_RSVD_SOBS ; sob++) {
		hw_sob = &sync_stream_prop->hw_sob[sob];
		hw_sob->hdev = hdev;
		hw_sob->sob_id = sync_stream_prop->base_sob_id + sob;
		hw_sob->sob_addr =
			hdev->asic_funcs->get_sob_addr(hdev, hw_sob->sob_id);
		hw_sob->q_idx = q_idx;
		kref_init(&hw_sob->kref);
	}
}

static void sync_stream_queue_reset(struct hl_device *hdev, u32 q_idx)
{
	struct hl_sync_stream_properties *prop =
			&hdev->kernel_queues[q_idx].sync_stream_prop;

	 
	kref_init(&prop->hw_sob[prop->curr_sob_offset].kref);
	prop->curr_sob_offset = 0;
	prop->next_sob_val = 1;
}

 
static int queue_init(struct hl_device *hdev, struct hl_hw_queue *q,
			u32 hw_queue_id)
{
	int rc;

	q->hw_queue_id = hw_queue_id;

	switch (q->queue_type) {
	case QUEUE_TYPE_EXT:
		rc = ext_queue_init(hdev, q);
		break;
	case QUEUE_TYPE_INT:
		rc = int_queue_init(hdev, q);
		break;
	case QUEUE_TYPE_CPU:
		rc = cpu_queue_init(hdev, q);
		break;
	case QUEUE_TYPE_HW:
		rc = hw_queue_init(hdev, q);
		break;
	case QUEUE_TYPE_NA:
		q->valid = 0;
		return 0;
	default:
		dev_crit(hdev->dev, "wrong queue type %d during init\n",
			q->queue_type);
		rc = -EINVAL;
		break;
	}

	sync_stream_queue_init(hdev, q->hw_queue_id);

	if (rc)
		return rc;

	q->valid = 1;

	return 0;
}

 
static void queue_fini(struct hl_device *hdev, struct hl_hw_queue *q)
{
	if (!q->valid)
		return;

	 

	if (q->queue_type == QUEUE_TYPE_INT)
		return;

	kfree(q->shadow_queue);

	if (q->queue_type == QUEUE_TYPE_CPU)
		hl_cpu_accessible_dma_pool_free(hdev, HL_QUEUE_SIZE_IN_BYTES, q->kernel_address);
	else
		hl_asic_dma_free_coherent(hdev, HL_QUEUE_SIZE_IN_BYTES, q->kernel_address,
						q->bus_address);
}

int hl_hw_queues_create(struct hl_device *hdev)
{
	struct asic_fixed_properties *asic = &hdev->asic_prop;
	struct hl_hw_queue *q;
	int i, rc, q_ready_cnt;

	hdev->kernel_queues = kcalloc(asic->max_queues,
				sizeof(*hdev->kernel_queues), GFP_KERNEL);

	if (!hdev->kernel_queues) {
		dev_err(hdev->dev, "Not enough memory for H/W queues\n");
		return -ENOMEM;
	}

	 
	for (i = 0, q_ready_cnt = 0, q = hdev->kernel_queues;
			i < asic->max_queues ; i++, q_ready_cnt++, q++) {

		q->queue_type = asic->hw_queues_props[i].type;
		q->supports_sync_stream =
				asic->hw_queues_props[i].supports_sync_stream;
		q->collective_mode = asic->hw_queues_props[i].collective_mode;
		rc = queue_init(hdev, q, i);
		if (rc) {
			dev_err(hdev->dev,
				"failed to initialize queue %d\n", i);
			goto release_queues;
		}
	}

	return 0;

release_queues:
	for (i = 0, q = hdev->kernel_queues ; i < q_ready_cnt ; i++, q++)
		queue_fini(hdev, q);

	kfree(hdev->kernel_queues);

	return rc;
}

void hl_hw_queues_destroy(struct hl_device *hdev)
{
	struct hl_hw_queue *q;
	u32 max_queues = hdev->asic_prop.max_queues;
	int i;

	for (i = 0, q = hdev->kernel_queues ; i < max_queues ; i++, q++)
		queue_fini(hdev, q);

	kfree(hdev->kernel_queues);
}

void hl_hw_queue_reset(struct hl_device *hdev, bool hard_reset)
{
	struct hl_hw_queue *q;
	u32 max_queues = hdev->asic_prop.max_queues;
	int i;

	for (i = 0, q = hdev->kernel_queues ; i < max_queues ; i++, q++) {
		if ((!q->valid) ||
			((!hard_reset) && (q->queue_type == QUEUE_TYPE_CPU)))
			continue;
		q->pi = 0;
		atomic_set(&q->ci, 0);

		if (q->supports_sync_stream)
			sync_stream_queue_reset(hdev, q->hw_queue_id);
	}
}
