
 

#include <linux/hash.h>
#include <linux/bitops.h>
#include <linux/lockdep.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <rdma/ib_verbs.h>
#include <rdma/ib_hdrs.h>
#include <rdma/opa_addr.h>
#include <rdma/uverbs_ioctl.h>
#include "qp.h"
#include "vt.h"
#include "trace.h"

#define RVT_RWQ_COUNT_THRESHOLD 16

static void rvt_rc_timeout(struct timer_list *t);
static void rvt_reset_qp(struct rvt_dev_info *rdi, struct rvt_qp *qp,
			 enum ib_qp_type type);

 
static const u32 ib_rvt_rnr_table[32] = {
	655360,  
	10,      
	20,      
	30,      
	40,      
	60,      
	80,      
	120,     
	160,     
	240,     
	320,     
	480,     
	640,     
	960,     
	1280,    
	1920,    
	2560,    
	3840,    
	5120,    
	7680,    
	10240,   
	15360,   
	20480,   
	30720,   
	40960,   
	61440,   
	81920,   
	122880,  
	163840,  
	245760,  
	327680,  
	491520   
};

 
const int ib_rvt_state_ops[IB_QPS_ERR + 1] = {
	[IB_QPS_RESET] = 0,
	[IB_QPS_INIT] = RVT_POST_RECV_OK,
	[IB_QPS_RTR] = RVT_POST_RECV_OK | RVT_PROCESS_RECV_OK,
	[IB_QPS_RTS] = RVT_POST_RECV_OK | RVT_PROCESS_RECV_OK |
	    RVT_POST_SEND_OK | RVT_PROCESS_SEND_OK |
	    RVT_PROCESS_NEXT_SEND_OK,
	[IB_QPS_SQD] = RVT_POST_RECV_OK | RVT_PROCESS_RECV_OK |
	    RVT_POST_SEND_OK | RVT_PROCESS_SEND_OK,
	[IB_QPS_SQE] = RVT_POST_RECV_OK | RVT_PROCESS_RECV_OK |
	    RVT_POST_SEND_OK | RVT_FLUSH_SEND,
	[IB_QPS_ERR] = RVT_POST_RECV_OK | RVT_FLUSH_RECV |
	    RVT_POST_SEND_OK | RVT_FLUSH_SEND,
};
EXPORT_SYMBOL(ib_rvt_state_ops);

 
static int rvt_wss_llc_size(void)
{
	 
	return boot_cpu_data.x86_cache_size;
}

 
static void cacheless_memcpy(void *dst, void *src, size_t n)
{
	 
	__copy_user_nocache(dst, (void __user *)src, n);
}

void rvt_wss_exit(struct rvt_dev_info *rdi)
{
	struct rvt_wss *wss = rdi->wss;

	if (!wss)
		return;

	 
	kfree(wss->entries);
	wss->entries = NULL;
	kfree(rdi->wss);
	rdi->wss = NULL;
}

 
int rvt_wss_init(struct rvt_dev_info *rdi)
{
	unsigned int sge_copy_mode = rdi->dparms.sge_copy_mode;
	unsigned int wss_threshold = rdi->dparms.wss_threshold;
	unsigned int wss_clean_period = rdi->dparms.wss_clean_period;
	long llc_size;
	long llc_bits;
	long table_size;
	long table_bits;
	struct rvt_wss *wss;
	int node = rdi->dparms.node;

	if (sge_copy_mode != RVT_SGE_COPY_ADAPTIVE) {
		rdi->wss = NULL;
		return 0;
	}

	rdi->wss = kzalloc_node(sizeof(*rdi->wss), GFP_KERNEL, node);
	if (!rdi->wss)
		return -ENOMEM;
	wss = rdi->wss;

	 
	if (wss_threshold < 1 || wss_threshold > 100)
		wss_threshold = 80;

	 
	if (wss_clean_period > 1000000)
		wss_clean_period = 256;

	 
	if (wss_clean_period == 0)
		wss_clean_period = 1;

	 
	llc_size = rvt_wss_llc_size() * 1024;
	table_size = roundup_pow_of_two(llc_size);

	 
	llc_bits = llc_size / PAGE_SIZE;
	table_bits = table_size / PAGE_SIZE;
	wss->pages_mask = table_bits - 1;
	wss->num_entries = table_bits / BITS_PER_LONG;

	wss->threshold = (llc_bits * wss_threshold) / 100;
	if (wss->threshold == 0)
		wss->threshold = 1;

	wss->clean_period = wss_clean_period;
	atomic_set(&wss->clean_counter, wss_clean_period);

	wss->entries = kcalloc_node(wss->num_entries, sizeof(*wss->entries),
				    GFP_KERNEL, node);
	if (!wss->entries) {
		rvt_wss_exit(rdi);
		return -ENOMEM;
	}

	return 0;
}

 
static void wss_advance_clean_counter(struct rvt_wss *wss)
{
	int entry;
	int weight;
	unsigned long bits;

	 
	if (atomic_dec_and_test(&wss->clean_counter)) {
		 
		atomic_set(&wss->clean_counter, wss->clean_period);

		 
		entry = (atomic_inc_return(&wss->clean_entry) - 1)
			& (wss->num_entries - 1);

		 
		bits = xchg(&wss->entries[entry], 0);
		weight = hweight64((u64)bits);
		 
		if (weight)
			atomic_sub(weight, &wss->total_count);
	}
}

 
static void wss_insert(struct rvt_wss *wss, void *address)
{
	u32 page = ((unsigned long)address >> PAGE_SHIFT) & wss->pages_mask;
	u32 entry = page / BITS_PER_LONG;  
	u32 nr = page & (BITS_PER_LONG - 1);

	if (!test_and_set_bit(nr, &wss->entries[entry]))
		atomic_inc(&wss->total_count);

	wss_advance_clean_counter(wss);
}

 
static inline bool wss_exceeds_threshold(struct rvt_wss *wss)
{
	return atomic_read(&wss->total_count) >= wss->threshold;
}

static void get_map_page(struct rvt_qpn_table *qpt,
			 struct rvt_qpn_map *map)
{
	unsigned long page = get_zeroed_page(GFP_KERNEL);

	 

	spin_lock(&qpt->lock);
	if (map->page)
		free_page(page);
	else
		map->page = (void *)page;
	spin_unlock(&qpt->lock);
}

 
static int init_qpn_table(struct rvt_dev_info *rdi, struct rvt_qpn_table *qpt)
{
	u32 offset, i;
	struct rvt_qpn_map *map;
	int ret = 0;

	if (!(rdi->dparms.qpn_res_end >= rdi->dparms.qpn_res_start))
		return -EINVAL;

	spin_lock_init(&qpt->lock);

	qpt->last = rdi->dparms.qpn_start;
	qpt->incr = rdi->dparms.qpn_inc << rdi->dparms.qos_shift;

	 

	 
	qpt->nmaps = rdi->dparms.qpn_res_start / RVT_BITS_PER_PAGE;

	 
	offset = rdi->dparms.qpn_res_start & RVT_BITS_PER_PAGE_MASK;

	 
	map = &qpt->map[qpt->nmaps];

	rvt_pr_info(rdi, "Reserving QPNs from 0x%x to 0x%x for non-verbs use\n",
		    rdi->dparms.qpn_res_start, rdi->dparms.qpn_res_end);
	for (i = rdi->dparms.qpn_res_start; i <= rdi->dparms.qpn_res_end; i++) {
		if (!map->page) {
			get_map_page(qpt, map);
			if (!map->page) {
				ret = -ENOMEM;
				break;
			}
		}
		set_bit(offset, map->page);
		offset++;
		if (offset == RVT_BITS_PER_PAGE) {
			 
			qpt->nmaps++;
			map++;
			offset = 0;
		}
	}
	return ret;
}

 
static void free_qpn_table(struct rvt_qpn_table *qpt)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(qpt->map); i++)
		free_page((unsigned long)qpt->map[i].page);
}

 
int rvt_driver_qp_init(struct rvt_dev_info *rdi)
{
	int i;
	int ret = -ENOMEM;

	if (!rdi->dparms.qp_table_size)
		return -EINVAL;

	 
	if (!rdi->driver_f.free_all_qps ||
	    !rdi->driver_f.qp_priv_alloc ||
	    !rdi->driver_f.qp_priv_free ||
	    !rdi->driver_f.notify_qp_reset ||
	    !rdi->driver_f.notify_restart_rc)
		return -EINVAL;

	 
	rdi->qp_dev = kzalloc_node(sizeof(*rdi->qp_dev), GFP_KERNEL,
				   rdi->dparms.node);
	if (!rdi->qp_dev)
		return -ENOMEM;

	 
	rdi->qp_dev->qp_table_size = rdi->dparms.qp_table_size;
	rdi->qp_dev->qp_table_bits = ilog2(rdi->dparms.qp_table_size);
	rdi->qp_dev->qp_table =
		kmalloc_array_node(rdi->qp_dev->qp_table_size,
			     sizeof(*rdi->qp_dev->qp_table),
			     GFP_KERNEL, rdi->dparms.node);
	if (!rdi->qp_dev->qp_table)
		goto no_qp_table;

	for (i = 0; i < rdi->qp_dev->qp_table_size; i++)
		RCU_INIT_POINTER(rdi->qp_dev->qp_table[i], NULL);

	spin_lock_init(&rdi->qp_dev->qpt_lock);

	 
	if (init_qpn_table(rdi, &rdi->qp_dev->qpn_table))
		goto fail_table;

	spin_lock_init(&rdi->n_qps_lock);

	return 0;

fail_table:
	kfree(rdi->qp_dev->qp_table);
	free_qpn_table(&rdi->qp_dev->qpn_table);

no_qp_table:
	kfree(rdi->qp_dev);

	return ret;
}

 
static void rvt_free_qp_cb(struct rvt_qp *qp, u64 v)
{
	unsigned int *qp_inuse = (unsigned int *)v;
	struct rvt_dev_info *rdi = ib_to_rvt(qp->ibqp.device);

	 
	rvt_reset_qp(rdi, qp, qp->ibqp.qp_type);

	 
	(*qp_inuse)++;
}

 
static unsigned rvt_free_all_qps(struct rvt_dev_info *rdi)
{
	unsigned int qp_inuse = 0;

	qp_inuse += rvt_mcast_tree_empty(rdi);

	rvt_qp_iter(rdi, (u64)&qp_inuse, rvt_free_qp_cb);

	return qp_inuse;
}

 
void rvt_qp_exit(struct rvt_dev_info *rdi)
{
	u32 qps_inuse = rvt_free_all_qps(rdi);

	if (qps_inuse)
		rvt_pr_err(rdi, "QP memory leak! %u still in use\n",
			   qps_inuse);

	kfree(rdi->qp_dev->qp_table);
	free_qpn_table(&rdi->qp_dev->qpn_table);
	kfree(rdi->qp_dev);
}

static inline unsigned mk_qpn(struct rvt_qpn_table *qpt,
			      struct rvt_qpn_map *map, unsigned off)
{
	return (map - qpt->map) * RVT_BITS_PER_PAGE + off;
}

 
static int alloc_qpn(struct rvt_dev_info *rdi, struct rvt_qpn_table *qpt,
		     enum ib_qp_type type, u8 port_num, u8 exclude_prefix)
{
	u32 i, offset, max_scan, qpn;
	struct rvt_qpn_map *map;
	u32 ret;
	u32 max_qpn = exclude_prefix == RVT_AIP_QP_PREFIX ?
		RVT_AIP_QPN_MAX : RVT_QPN_MAX;

	if (rdi->driver_f.alloc_qpn)
		return rdi->driver_f.alloc_qpn(rdi, qpt, type, port_num);

	if (type == IB_QPT_SMI || type == IB_QPT_GSI) {
		unsigned n;

		ret = type == IB_QPT_GSI;
		n = 1 << (ret + 2 * (port_num - 1));
		spin_lock(&qpt->lock);
		if (qpt->flags & n)
			ret = -EINVAL;
		else
			qpt->flags |= n;
		spin_unlock(&qpt->lock);
		goto bail;
	}

	qpn = qpt->last + qpt->incr;
	if (qpn >= max_qpn)
		qpn = qpt->incr | ((qpt->last & 1) ^ 1);
	 
	offset = qpn & RVT_BITS_PER_PAGE_MASK;
	map = &qpt->map[qpn / RVT_BITS_PER_PAGE];
	max_scan = qpt->nmaps - !offset;
	for (i = 0;;) {
		if (unlikely(!map->page)) {
			get_map_page(qpt, map);
			if (unlikely(!map->page))
				break;
		}
		do {
			if (!test_and_set_bit(offset, map->page)) {
				qpt->last = qpn;
				ret = qpn;
				goto bail;
			}
			offset += qpt->incr;
			 
			qpn = mk_qpn(qpt, map, offset);
		} while (offset < RVT_BITS_PER_PAGE && qpn < RVT_QPN_MAX);
		 
		if (++i > max_scan) {
			if (qpt->nmaps == RVT_QPNMAP_ENTRIES)
				break;
			map = &qpt->map[qpt->nmaps++];
			 
			offset = qpt->incr | (offset & 1);
		} else if (map < &qpt->map[qpt->nmaps]) {
			++map;
			 
			offset = qpt->incr | (offset & 1);
		} else {
			map = &qpt->map[0];
			 
			offset = qpt->incr | ((offset & 1) ^ 1);
		}
		 
		WARN_ON(rdi->dparms.qos_shift > 1 &&
			offset & ((BIT(rdi->dparms.qos_shift - 1) - 1) << 1));
		qpn = mk_qpn(qpt, map, offset);
	}

	ret = -ENOMEM;

bail:
	return ret;
}

 
static void rvt_clear_mr_refs(struct rvt_qp *qp, int clr_sends)
{
	unsigned n;
	struct rvt_dev_info *rdi = ib_to_rvt(qp->ibqp.device);

	if (test_and_clear_bit(RVT_R_REWIND_SGE, &qp->r_aflags))
		rvt_put_ss(&qp->s_rdma_read_sge);

	rvt_put_ss(&qp->r_sge);

	if (clr_sends) {
		while (qp->s_last != qp->s_head) {
			struct rvt_swqe *wqe = rvt_get_swqe_ptr(qp, qp->s_last);

			rvt_put_qp_swqe(qp, wqe);
			if (++qp->s_last >= qp->s_size)
				qp->s_last = 0;
			smp_wmb();  
		}
		if (qp->s_rdma_mr) {
			rvt_put_mr(qp->s_rdma_mr);
			qp->s_rdma_mr = NULL;
		}
	}

	for (n = 0; qp->s_ack_queue && n < rvt_max_atomic(rdi); n++) {
		struct rvt_ack_entry *e = &qp->s_ack_queue[n];

		if (e->rdma_sge.mr) {
			rvt_put_mr(e->rdma_sge.mr);
			e->rdma_sge.mr = NULL;
		}
	}
}

 
static bool rvt_swqe_has_lkey(struct rvt_swqe *wqe, u32 lkey)
{
	int i;

	for (i = 0; i < wqe->wr.num_sge; i++) {
		struct rvt_sge *sge = &wqe->sg_list[i];

		if (rvt_mr_has_lkey(sge->mr, lkey))
			return true;
	}
	return false;
}

 
static bool rvt_qp_sends_has_lkey(struct rvt_qp *qp, u32 lkey)
{
	u32 s_last = qp->s_last;

	while (s_last != qp->s_head) {
		struct rvt_swqe *wqe = rvt_get_swqe_ptr(qp, s_last);

		if (rvt_swqe_has_lkey(wqe, lkey))
			return true;

		if (++s_last >= qp->s_size)
			s_last = 0;
	}
	if (qp->s_rdma_mr)
		if (rvt_mr_has_lkey(qp->s_rdma_mr, lkey))
			return true;
	return false;
}

 
static bool rvt_qp_acks_has_lkey(struct rvt_qp *qp, u32 lkey)
{
	int i;
	struct rvt_dev_info *rdi = ib_to_rvt(qp->ibqp.device);

	for (i = 0; qp->s_ack_queue && i < rvt_max_atomic(rdi); i++) {
		struct rvt_ack_entry *e = &qp->s_ack_queue[i];

		if (rvt_mr_has_lkey(e->rdma_sge.mr, lkey))
			return true;
	}
	return false;
}

 
void rvt_qp_mr_clean(struct rvt_qp *qp, u32 lkey)
{
	bool lastwqe = false;

	if (qp->ibqp.qp_type == IB_QPT_SMI ||
	    qp->ibqp.qp_type == IB_QPT_GSI)
		 
		return;
	spin_lock_irq(&qp->r_lock);
	spin_lock(&qp->s_hlock);
	spin_lock(&qp->s_lock);

	if (qp->state == IB_QPS_ERR || qp->state == IB_QPS_RESET)
		goto check_lwqe;

	if (rvt_ss_has_lkey(&qp->r_sge, lkey) ||
	    rvt_qp_sends_has_lkey(qp, lkey) ||
	    rvt_qp_acks_has_lkey(qp, lkey))
		lastwqe = rvt_error_qp(qp, IB_WC_LOC_PROT_ERR);
check_lwqe:
	spin_unlock(&qp->s_lock);
	spin_unlock(&qp->s_hlock);
	spin_unlock_irq(&qp->r_lock);
	if (lastwqe) {
		struct ib_event ev;

		ev.device = qp->ibqp.device;
		ev.element.qp = &qp->ibqp;
		ev.event = IB_EVENT_QP_LAST_WQE_REACHED;
		qp->ibqp.event_handler(&ev, qp->ibqp.qp_context);
	}
}

 
static void rvt_remove_qp(struct rvt_dev_info *rdi, struct rvt_qp *qp)
{
	struct rvt_ibport *rvp = rdi->ports[qp->port_num - 1];
	u32 n = hash_32(qp->ibqp.qp_num, rdi->qp_dev->qp_table_bits);
	unsigned long flags;
	int removed = 1;

	spin_lock_irqsave(&rdi->qp_dev->qpt_lock, flags);

	if (rcu_dereference_protected(rvp->qp[0],
			lockdep_is_held(&rdi->qp_dev->qpt_lock)) == qp) {
		RCU_INIT_POINTER(rvp->qp[0], NULL);
	} else if (rcu_dereference_protected(rvp->qp[1],
			lockdep_is_held(&rdi->qp_dev->qpt_lock)) == qp) {
		RCU_INIT_POINTER(rvp->qp[1], NULL);
	} else {
		struct rvt_qp *q;
		struct rvt_qp __rcu **qpp;

		removed = 0;
		qpp = &rdi->qp_dev->qp_table[n];
		for (; (q = rcu_dereference_protected(*qpp,
			lockdep_is_held(&rdi->qp_dev->qpt_lock))) != NULL;
			qpp = &q->next) {
			if (q == qp) {
				RCU_INIT_POINTER(*qpp,
				     rcu_dereference_protected(qp->next,
				     lockdep_is_held(&rdi->qp_dev->qpt_lock)));
				removed = 1;
				trace_rvt_qpremove(qp, n);
				break;
			}
		}
	}

	spin_unlock_irqrestore(&rdi->qp_dev->qpt_lock, flags);
	if (removed) {
		synchronize_rcu();
		rvt_put_qp(qp);
	}
}

 
int rvt_alloc_rq(struct rvt_rq *rq, u32 size, int node,
		 struct ib_udata *udata)
{
	if (udata) {
		rq->wq = vmalloc_user(sizeof(struct rvt_rwq) + size);
		if (!rq->wq)
			goto bail;
		 
		rq->kwq = kzalloc_node(sizeof(*rq->kwq), GFP_KERNEL, node);
		if (!rq->kwq)
			goto bail;
		rq->kwq->curr_wq = rq->wq->wq;
	} else {
		 
		rq->kwq =
			vzalloc_node(sizeof(struct rvt_krwq) + size, node);
		if (!rq->kwq)
			goto bail;
		rq->kwq->curr_wq = rq->kwq->wq;
	}

	spin_lock_init(&rq->kwq->p_lock);
	spin_lock_init(&rq->kwq->c_lock);
	return 0;
bail:
	rvt_free_rq(rq);
	return -ENOMEM;
}

 
static void rvt_init_qp(struct rvt_dev_info *rdi, struct rvt_qp *qp,
			enum ib_qp_type type)
{
	qp->remote_qpn = 0;
	qp->qkey = 0;
	qp->qp_access_flags = 0;
	qp->s_flags &= RVT_S_SIGNAL_REQ_WR;
	qp->s_hdrwords = 0;
	qp->s_wqe = NULL;
	qp->s_draining = 0;
	qp->s_next_psn = 0;
	qp->s_last_psn = 0;
	qp->s_sending_psn = 0;
	qp->s_sending_hpsn = 0;
	qp->s_psn = 0;
	qp->r_psn = 0;
	qp->r_msn = 0;
	if (type == IB_QPT_RC) {
		qp->s_state = IB_OPCODE_RC_SEND_LAST;
		qp->r_state = IB_OPCODE_RC_SEND_LAST;
	} else {
		qp->s_state = IB_OPCODE_UC_SEND_LAST;
		qp->r_state = IB_OPCODE_UC_SEND_LAST;
	}
	qp->s_ack_state = IB_OPCODE_RC_ACKNOWLEDGE;
	qp->r_nak_state = 0;
	qp->r_aflags = 0;
	qp->r_flags = 0;
	qp->s_head = 0;
	qp->s_tail = 0;
	qp->s_cur = 0;
	qp->s_acked = 0;
	qp->s_last = 0;
	qp->s_ssn = 1;
	qp->s_lsn = 0;
	qp->s_mig_state = IB_MIG_MIGRATED;
	qp->r_head_ack_queue = 0;
	qp->s_tail_ack_queue = 0;
	qp->s_acked_ack_queue = 0;
	qp->s_num_rd_atomic = 0;
	qp->r_sge.num_sge = 0;
	atomic_set(&qp->s_reserved_used, 0);
}

 
static void _rvt_reset_qp(struct rvt_dev_info *rdi, struct rvt_qp *qp,
			  enum ib_qp_type type)
	__must_hold(&qp->s_lock)
	__must_hold(&qp->s_hlock)
	__must_hold(&qp->r_lock)
{
	lockdep_assert_held(&qp->r_lock);
	lockdep_assert_held(&qp->s_hlock);
	lockdep_assert_held(&qp->s_lock);
	if (qp->state != IB_QPS_RESET) {
		qp->state = IB_QPS_RESET;

		 
		rdi->driver_f.flush_qp_waiters(qp);
		rvt_stop_rc_timers(qp);
		qp->s_flags &= ~(RVT_S_TIMER | RVT_S_ANY_WAIT);
		spin_unlock(&qp->s_lock);
		spin_unlock(&qp->s_hlock);
		spin_unlock_irq(&qp->r_lock);

		 
		rdi->driver_f.stop_send_queue(qp);
		rvt_del_timers_sync(qp);
		 
		rdi->driver_f.quiesce_qp(qp);

		 
		rvt_remove_qp(rdi, qp);

		 
		spin_lock_irq(&qp->r_lock);
		spin_lock(&qp->s_hlock);
		spin_lock(&qp->s_lock);

		rvt_clear_mr_refs(qp, 1);
		 
		rdi->driver_f.notify_qp_reset(qp);
	}
	rvt_init_qp(rdi, qp, type);
	lockdep_assert_held(&qp->r_lock);
	lockdep_assert_held(&qp->s_hlock);
	lockdep_assert_held(&qp->s_lock);
}

 
static void rvt_reset_qp(struct rvt_dev_info *rdi, struct rvt_qp *qp,
			 enum ib_qp_type type)
{
	spin_lock_irq(&qp->r_lock);
	spin_lock(&qp->s_hlock);
	spin_lock(&qp->s_lock);
	_rvt_reset_qp(rdi, qp, type);
	spin_unlock(&qp->s_lock);
	spin_unlock(&qp->s_hlock);
	spin_unlock_irq(&qp->r_lock);
}

 
static void rvt_free_qpn(struct rvt_qpn_table *qpt, u32 qpn)
{
	struct rvt_qpn_map *map;

	if ((qpn & RVT_AIP_QP_PREFIX_MASK) == RVT_AIP_QP_BASE)
		qpn &= RVT_AIP_QP_SUFFIX;

	map = qpt->map + (qpn & RVT_QPN_MASK) / RVT_BITS_PER_PAGE;
	if (map->page)
		clear_bit(qpn & RVT_BITS_PER_PAGE_MASK, map->page);
}

 
static u8 get_allowed_ops(enum ib_qp_type type)
{
	return type == IB_QPT_RC ? IB_OPCODE_RC : type == IB_QPT_UC ?
		IB_OPCODE_UC : IB_OPCODE_UD;
}

 
static void free_ud_wq_attr(struct rvt_qp *qp)
{
	struct rvt_swqe *wqe;
	int i;

	for (i = 0; qp->allowed_ops == IB_OPCODE_UD && i < qp->s_size; i++) {
		wqe = rvt_get_swqe_ptr(qp, i);
		kfree(wqe->ud_wr.attr);
		wqe->ud_wr.attr = NULL;
	}
}

 
static int alloc_ud_wq_attr(struct rvt_qp *qp, int node)
{
	struct rvt_swqe *wqe;
	int i;

	for (i = 0; qp->allowed_ops == IB_OPCODE_UD && i < qp->s_size; i++) {
		wqe = rvt_get_swqe_ptr(qp, i);
		wqe->ud_wr.attr = kzalloc_node(sizeof(*wqe->ud_wr.attr),
					       GFP_KERNEL, node);
		if (!wqe->ud_wr.attr) {
			free_ud_wq_attr(qp);
			return -ENOMEM;
		}
	}

	return 0;
}

 
int rvt_create_qp(struct ib_qp *ibqp, struct ib_qp_init_attr *init_attr,
		  struct ib_udata *udata)
{
	struct rvt_qp *qp = ibqp_to_rvtqp(ibqp);
	int ret = -ENOMEM;
	struct rvt_swqe *swq = NULL;
	size_t sz;
	size_t sg_list_sz = 0;
	struct rvt_dev_info *rdi = ib_to_rvt(ibqp->device);
	void *priv = NULL;
	size_t sqsize;
	u8 exclude_prefix = 0;

	if (!rdi)
		return -EINVAL;

	if (init_attr->create_flags & ~IB_QP_CREATE_NETDEV_USE)
		return -EOPNOTSUPP;

	if (init_attr->cap.max_send_sge > rdi->dparms.props.max_send_sge ||
	    init_attr->cap.max_send_wr > rdi->dparms.props.max_qp_wr)
		return -EINVAL;

	 
	if (!init_attr->srq) {
		if (init_attr->cap.max_recv_sge >
		    rdi->dparms.props.max_recv_sge ||
		    init_attr->cap.max_recv_wr > rdi->dparms.props.max_qp_wr)
			return -EINVAL;

		if (init_attr->cap.max_send_sge +
		    init_attr->cap.max_send_wr +
		    init_attr->cap.max_recv_sge +
		    init_attr->cap.max_recv_wr == 0)
			return -EINVAL;
	}
	sqsize =
		init_attr->cap.max_send_wr + 1 +
		rdi->dparms.reserved_operations;
	switch (init_attr->qp_type) {
	case IB_QPT_SMI:
	case IB_QPT_GSI:
		if (init_attr->port_num == 0 ||
		    init_attr->port_num > ibqp->device->phys_port_cnt)
			return -EINVAL;
		fallthrough;
	case IB_QPT_UC:
	case IB_QPT_RC:
	case IB_QPT_UD:
		sz = struct_size(swq, sg_list, init_attr->cap.max_send_sge);
		swq = vzalloc_node(array_size(sz, sqsize), rdi->dparms.node);
		if (!swq)
			return -ENOMEM;

		if (init_attr->srq) {
			struct rvt_srq *srq = ibsrq_to_rvtsrq(init_attr->srq);

			if (srq->rq.max_sge > 1)
				sg_list_sz = sizeof(*qp->r_sg_list) *
					(srq->rq.max_sge - 1);
		} else if (init_attr->cap.max_recv_sge > 1)
			sg_list_sz = sizeof(*qp->r_sg_list) *
				(init_attr->cap.max_recv_sge - 1);
		qp->r_sg_list =
			kzalloc_node(sg_list_sz, GFP_KERNEL, rdi->dparms.node);
		if (!qp->r_sg_list)
			goto bail_qp;
		qp->allowed_ops = get_allowed_ops(init_attr->qp_type);

		RCU_INIT_POINTER(qp->next, NULL);
		if (init_attr->qp_type == IB_QPT_RC) {
			qp->s_ack_queue =
				kcalloc_node(rvt_max_atomic(rdi),
					     sizeof(*qp->s_ack_queue),
					     GFP_KERNEL,
					     rdi->dparms.node);
			if (!qp->s_ack_queue)
				goto bail_qp;
		}
		 
		timer_setup(&qp->s_timer, rvt_rc_timeout, 0);
		hrtimer_init(&qp->s_rnr_timer, CLOCK_MONOTONIC,
			     HRTIMER_MODE_REL);
		qp->s_rnr_timer.function = rvt_rc_rnr_retry;

		 
		priv = rdi->driver_f.qp_priv_alloc(rdi, qp);
		if (IS_ERR(priv)) {
			ret = PTR_ERR(priv);
			goto bail_qp;
		}
		qp->priv = priv;
		qp->timeout_jiffies =
			usecs_to_jiffies((4096UL * (1UL << qp->timeout)) /
				1000UL);
		if (init_attr->srq) {
			sz = 0;
		} else {
			qp->r_rq.size = init_attr->cap.max_recv_wr + 1;
			qp->r_rq.max_sge = init_attr->cap.max_recv_sge;
			sz = (sizeof(struct ib_sge) * qp->r_rq.max_sge) +
				sizeof(struct rvt_rwqe);
			ret = rvt_alloc_rq(&qp->r_rq, qp->r_rq.size * sz,
					   rdi->dparms.node, udata);
			if (ret)
				goto bail_driver_priv;
		}

		 
		spin_lock_init(&qp->r_lock);
		spin_lock_init(&qp->s_hlock);
		spin_lock_init(&qp->s_lock);
		atomic_set(&qp->refcount, 0);
		atomic_set(&qp->local_ops_pending, 0);
		init_waitqueue_head(&qp->wait);
		INIT_LIST_HEAD(&qp->rspwait);
		qp->state = IB_QPS_RESET;
		qp->s_wq = swq;
		qp->s_size = sqsize;
		qp->s_avail = init_attr->cap.max_send_wr;
		qp->s_max_sge = init_attr->cap.max_send_sge;
		if (init_attr->sq_sig_type == IB_SIGNAL_REQ_WR)
			qp->s_flags = RVT_S_SIGNAL_REQ_WR;
		ret = alloc_ud_wq_attr(qp, rdi->dparms.node);
		if (ret)
			goto bail_rq_rvt;

		if (init_attr->create_flags & IB_QP_CREATE_NETDEV_USE)
			exclude_prefix = RVT_AIP_QP_PREFIX;

		ret = alloc_qpn(rdi, &rdi->qp_dev->qpn_table,
				init_attr->qp_type,
				init_attr->port_num,
				exclude_prefix);
		if (ret < 0)
			goto bail_rq_wq;

		qp->ibqp.qp_num = ret;
		if (init_attr->create_flags & IB_QP_CREATE_NETDEV_USE)
			qp->ibqp.qp_num |= RVT_AIP_QP_BASE;
		qp->port_num = init_attr->port_num;
		rvt_init_qp(rdi, qp, init_attr->qp_type);
		if (rdi->driver_f.qp_priv_init) {
			ret = rdi->driver_f.qp_priv_init(rdi, qp, init_attr);
			if (ret)
				goto bail_rq_wq;
		}
		break;

	default:
		 
		return -EOPNOTSUPP;
	}

	init_attr->cap.max_inline_data = 0;

	 
	if (udata && udata->outlen >= sizeof(__u64)) {
		if (!qp->r_rq.wq) {
			__u64 offset = 0;

			ret = ib_copy_to_udata(udata, &offset,
					       sizeof(offset));
			if (ret)
				goto bail_qpn;
		} else {
			u32 s = sizeof(struct rvt_rwq) + qp->r_rq.size * sz;

			qp->ip = rvt_create_mmap_info(rdi, s, udata,
						      qp->r_rq.wq);
			if (IS_ERR(qp->ip)) {
				ret = PTR_ERR(qp->ip);
				goto bail_qpn;
			}

			ret = ib_copy_to_udata(udata, &qp->ip->offset,
					       sizeof(qp->ip->offset));
			if (ret)
				goto bail_ip;
		}
		qp->pid = current->pid;
	}

	spin_lock(&rdi->n_qps_lock);
	if (rdi->n_qps_allocated == rdi->dparms.props.max_qp) {
		spin_unlock(&rdi->n_qps_lock);
		ret = -ENOMEM;
		goto bail_ip;
	}

	rdi->n_qps_allocated++;
	 
	if (init_attr->qp_type == IB_QPT_RC) {
		rdi->n_rc_qps++;
		rdi->busy_jiffies = rdi->n_rc_qps / RC_QP_SCALING_INTERVAL;
	}
	spin_unlock(&rdi->n_qps_lock);

	if (qp->ip) {
		spin_lock_irq(&rdi->pending_lock);
		list_add(&qp->ip->pending_mmaps, &rdi->pending_mmaps);
		spin_unlock_irq(&rdi->pending_lock);
	}

	return 0;

bail_ip:
	if (qp->ip)
		kref_put(&qp->ip->ref, rvt_release_mmap_info);

bail_qpn:
	rvt_free_qpn(&rdi->qp_dev->qpn_table, qp->ibqp.qp_num);

bail_rq_wq:
	free_ud_wq_attr(qp);

bail_rq_rvt:
	rvt_free_rq(&qp->r_rq);

bail_driver_priv:
	rdi->driver_f.qp_priv_free(rdi, qp);

bail_qp:
	kfree(qp->s_ack_queue);
	kfree(qp->r_sg_list);
	vfree(swq);
	return ret;
}

 
int rvt_error_qp(struct rvt_qp *qp, enum ib_wc_status err)
{
	struct ib_wc wc;
	int ret = 0;
	struct rvt_dev_info *rdi = ib_to_rvt(qp->ibqp.device);

	lockdep_assert_held(&qp->r_lock);
	lockdep_assert_held(&qp->s_lock);
	if (qp->state == IB_QPS_ERR || qp->state == IB_QPS_RESET)
		goto bail;

	qp->state = IB_QPS_ERR;

	if (qp->s_flags & (RVT_S_TIMER | RVT_S_WAIT_RNR)) {
		qp->s_flags &= ~(RVT_S_TIMER | RVT_S_WAIT_RNR);
		del_timer(&qp->s_timer);
	}

	if (qp->s_flags & RVT_S_ANY_WAIT_SEND)
		qp->s_flags &= ~RVT_S_ANY_WAIT_SEND;

	rdi->driver_f.notify_error_qp(qp);

	 
	if (READ_ONCE(qp->s_last) != qp->s_head)
		rdi->driver_f.schedule_send(qp);

	rvt_clear_mr_refs(qp, 0);

	memset(&wc, 0, sizeof(wc));
	wc.qp = &qp->ibqp;
	wc.opcode = IB_WC_RECV;

	if (test_and_clear_bit(RVT_R_WRID_VALID, &qp->r_aflags)) {
		wc.wr_id = qp->r_wr_id;
		wc.status = err;
		rvt_cq_enter(ibcq_to_rvtcq(qp->ibqp.recv_cq), &wc, 1);
	}
	wc.status = IB_WC_WR_FLUSH_ERR;

	if (qp->r_rq.kwq) {
		u32 head;
		u32 tail;
		struct rvt_rwq *wq = NULL;
		struct rvt_krwq *kwq = NULL;

		spin_lock(&qp->r_rq.kwq->c_lock);
		 
		if (qp->ip) {
			wq = qp->r_rq.wq;
			head = RDMA_READ_UAPI_ATOMIC(wq->head);
			tail = RDMA_READ_UAPI_ATOMIC(wq->tail);
		} else {
			kwq = qp->r_rq.kwq;
			head = kwq->head;
			tail = kwq->tail;
		}
		 
		if (head >= qp->r_rq.size)
			head = 0;
		if (tail >= qp->r_rq.size)
			tail = 0;
		while (tail != head) {
			wc.wr_id = rvt_get_rwqe_ptr(&qp->r_rq, tail)->wr_id;
			if (++tail >= qp->r_rq.size)
				tail = 0;
			rvt_cq_enter(ibcq_to_rvtcq(qp->ibqp.recv_cq), &wc, 1);
		}
		if (qp->ip)
			RDMA_WRITE_UAPI_ATOMIC(wq->tail, tail);
		else
			kwq->tail = tail;
		spin_unlock(&qp->r_rq.kwq->c_lock);
	} else if (qp->ibqp.event_handler) {
		ret = 1;
	}

bail:
	return ret;
}
EXPORT_SYMBOL(rvt_error_qp);

 
static void rvt_insert_qp(struct rvt_dev_info *rdi, struct rvt_qp *qp)
{
	struct rvt_ibport *rvp = rdi->ports[qp->port_num - 1];
	unsigned long flags;

	rvt_get_qp(qp);
	spin_lock_irqsave(&rdi->qp_dev->qpt_lock, flags);

	if (qp->ibqp.qp_num <= 1) {
		rcu_assign_pointer(rvp->qp[qp->ibqp.qp_num], qp);
	} else {
		u32 n = hash_32(qp->ibqp.qp_num, rdi->qp_dev->qp_table_bits);

		qp->next = rdi->qp_dev->qp_table[n];
		rcu_assign_pointer(rdi->qp_dev->qp_table[n], qp);
		trace_rvt_qpinsert(qp, n);
	}

	spin_unlock_irqrestore(&rdi->qp_dev->qpt_lock, flags);
}

 
int rvt_modify_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr,
		  int attr_mask, struct ib_udata *udata)
{
	struct rvt_dev_info *rdi = ib_to_rvt(ibqp->device);
	struct rvt_qp *qp = ibqp_to_rvtqp(ibqp);
	enum ib_qp_state cur_state, new_state;
	struct ib_event ev;
	int lastwqe = 0;
	int mig = 0;
	int pmtu = 0;  
	int opa_ah;

	if (attr_mask & ~IB_QP_ATTR_STANDARD_BITS)
		return -EOPNOTSUPP;

	spin_lock_irq(&qp->r_lock);
	spin_lock(&qp->s_hlock);
	spin_lock(&qp->s_lock);

	cur_state = attr_mask & IB_QP_CUR_STATE ?
		attr->cur_qp_state : qp->state;
	new_state = attr_mask & IB_QP_STATE ? attr->qp_state : cur_state;
	opa_ah = rdma_cap_opa_ah(ibqp->device, qp->port_num);

	if (!ib_modify_qp_is_ok(cur_state, new_state, ibqp->qp_type,
				attr_mask))
		goto inval;

	if (rdi->driver_f.check_modify_qp &&
	    rdi->driver_f.check_modify_qp(qp, attr, attr_mask, udata))
		goto inval;

	if (attr_mask & IB_QP_AV) {
		if (opa_ah) {
			if (rdma_ah_get_dlid(&attr->ah_attr) >=
				opa_get_mcast_base(OPA_MCAST_NR))
				goto inval;
		} else {
			if (rdma_ah_get_dlid(&attr->ah_attr) >=
				be16_to_cpu(IB_MULTICAST_LID_BASE))
				goto inval;
		}

		if (rvt_check_ah(qp->ibqp.device, &attr->ah_attr))
			goto inval;
	}

	if (attr_mask & IB_QP_ALT_PATH) {
		if (opa_ah) {
			if (rdma_ah_get_dlid(&attr->alt_ah_attr) >=
				opa_get_mcast_base(OPA_MCAST_NR))
				goto inval;
		} else {
			if (rdma_ah_get_dlid(&attr->alt_ah_attr) >=
				be16_to_cpu(IB_MULTICAST_LID_BASE))
				goto inval;
		}

		if (rvt_check_ah(qp->ibqp.device, &attr->alt_ah_attr))
			goto inval;
		if (attr->alt_pkey_index >= rvt_get_npkeys(rdi))
			goto inval;
	}

	if (attr_mask & IB_QP_PKEY_INDEX)
		if (attr->pkey_index >= rvt_get_npkeys(rdi))
			goto inval;

	if (attr_mask & IB_QP_MIN_RNR_TIMER)
		if (attr->min_rnr_timer > 31)
			goto inval;

	if (attr_mask & IB_QP_PORT)
		if (qp->ibqp.qp_type == IB_QPT_SMI ||
		    qp->ibqp.qp_type == IB_QPT_GSI ||
		    attr->port_num == 0 ||
		    attr->port_num > ibqp->device->phys_port_cnt)
			goto inval;

	if (attr_mask & IB_QP_DEST_QPN)
		if (attr->dest_qp_num > RVT_QPN_MASK)
			goto inval;

	if (attr_mask & IB_QP_RETRY_CNT)
		if (attr->retry_cnt > 7)
			goto inval;

	if (attr_mask & IB_QP_RNR_RETRY)
		if (attr->rnr_retry > 7)
			goto inval;

	 
	if (attr_mask & IB_QP_PATH_MTU) {
		pmtu = rdi->driver_f.get_pmtu_from_attr(rdi, qp, attr);
		if (pmtu < 0)
			goto inval;
	}

	if (attr_mask & IB_QP_PATH_MIG_STATE) {
		if (attr->path_mig_state == IB_MIG_REARM) {
			if (qp->s_mig_state == IB_MIG_ARMED)
				goto inval;
			if (new_state != IB_QPS_RTS)
				goto inval;
		} else if (attr->path_mig_state == IB_MIG_MIGRATED) {
			if (qp->s_mig_state == IB_MIG_REARM)
				goto inval;
			if (new_state != IB_QPS_RTS && new_state != IB_QPS_SQD)
				goto inval;
			if (qp->s_mig_state == IB_MIG_ARMED)
				mig = 1;
		} else {
			goto inval;
		}
	}

	if (attr_mask & IB_QP_MAX_DEST_RD_ATOMIC)
		if (attr->max_dest_rd_atomic > rdi->dparms.max_rdma_atomic)
			goto inval;

	switch (new_state) {
	case IB_QPS_RESET:
		if (qp->state != IB_QPS_RESET)
			_rvt_reset_qp(rdi, qp, ibqp->qp_type);
		break;

	case IB_QPS_RTR:
		 
		qp->r_flags &= ~RVT_R_COMM_EST;
		qp->state = new_state;
		break;

	case IB_QPS_SQD:
		qp->s_draining = qp->s_last != qp->s_cur;
		qp->state = new_state;
		break;

	case IB_QPS_SQE:
		if (qp->ibqp.qp_type == IB_QPT_RC)
			goto inval;
		qp->state = new_state;
		break;

	case IB_QPS_ERR:
		lastwqe = rvt_error_qp(qp, IB_WC_WR_FLUSH_ERR);
		break;

	default:
		qp->state = new_state;
		break;
	}

	if (attr_mask & IB_QP_PKEY_INDEX)
		qp->s_pkey_index = attr->pkey_index;

	if (attr_mask & IB_QP_PORT)
		qp->port_num = attr->port_num;

	if (attr_mask & IB_QP_DEST_QPN)
		qp->remote_qpn = attr->dest_qp_num;

	if (attr_mask & IB_QP_SQ_PSN) {
		qp->s_next_psn = attr->sq_psn & rdi->dparms.psn_modify_mask;
		qp->s_psn = qp->s_next_psn;
		qp->s_sending_psn = qp->s_next_psn;
		qp->s_last_psn = qp->s_next_psn - 1;
		qp->s_sending_hpsn = qp->s_last_psn;
	}

	if (attr_mask & IB_QP_RQ_PSN)
		qp->r_psn = attr->rq_psn & rdi->dparms.psn_modify_mask;

	if (attr_mask & IB_QP_ACCESS_FLAGS)
		qp->qp_access_flags = attr->qp_access_flags;

	if (attr_mask & IB_QP_AV) {
		rdma_replace_ah_attr(&qp->remote_ah_attr, &attr->ah_attr);
		qp->s_srate = rdma_ah_get_static_rate(&attr->ah_attr);
		qp->srate_mbps = ib_rate_to_mbps(qp->s_srate);
	}

	if (attr_mask & IB_QP_ALT_PATH) {
		rdma_replace_ah_attr(&qp->alt_ah_attr, &attr->alt_ah_attr);
		qp->s_alt_pkey_index = attr->alt_pkey_index;
	}

	if (attr_mask & IB_QP_PATH_MIG_STATE) {
		qp->s_mig_state = attr->path_mig_state;
		if (mig) {
			qp->remote_ah_attr = qp->alt_ah_attr;
			qp->port_num = rdma_ah_get_port_num(&qp->alt_ah_attr);
			qp->s_pkey_index = qp->s_alt_pkey_index;
		}
	}

	if (attr_mask & IB_QP_PATH_MTU) {
		qp->pmtu = rdi->driver_f.mtu_from_qp(rdi, qp, pmtu);
		qp->log_pmtu = ilog2(qp->pmtu);
	}

	if (attr_mask & IB_QP_RETRY_CNT) {
		qp->s_retry_cnt = attr->retry_cnt;
		qp->s_retry = attr->retry_cnt;
	}

	if (attr_mask & IB_QP_RNR_RETRY) {
		qp->s_rnr_retry_cnt = attr->rnr_retry;
		qp->s_rnr_retry = attr->rnr_retry;
	}

	if (attr_mask & IB_QP_MIN_RNR_TIMER)
		qp->r_min_rnr_timer = attr->min_rnr_timer;

	if (attr_mask & IB_QP_TIMEOUT) {
		qp->timeout = attr->timeout;
		qp->timeout_jiffies = rvt_timeout_to_jiffies(qp->timeout);
	}

	if (attr_mask & IB_QP_QKEY)
		qp->qkey = attr->qkey;

	if (attr_mask & IB_QP_MAX_DEST_RD_ATOMIC)
		qp->r_max_rd_atomic = attr->max_dest_rd_atomic;

	if (attr_mask & IB_QP_MAX_QP_RD_ATOMIC)
		qp->s_max_rd_atomic = attr->max_rd_atomic;

	if (rdi->driver_f.modify_qp)
		rdi->driver_f.modify_qp(qp, attr, attr_mask, udata);

	spin_unlock(&qp->s_lock);
	spin_unlock(&qp->s_hlock);
	spin_unlock_irq(&qp->r_lock);

	if (cur_state == IB_QPS_RESET && new_state == IB_QPS_INIT)
		rvt_insert_qp(rdi, qp);

	if (lastwqe) {
		ev.device = qp->ibqp.device;
		ev.element.qp = &qp->ibqp;
		ev.event = IB_EVENT_QP_LAST_WQE_REACHED;
		qp->ibqp.event_handler(&ev, qp->ibqp.qp_context);
	}
	if (mig) {
		ev.device = qp->ibqp.device;
		ev.element.qp = &qp->ibqp;
		ev.event = IB_EVENT_PATH_MIG;
		qp->ibqp.event_handler(&ev, qp->ibqp.qp_context);
	}
	return 0;

inval:
	spin_unlock(&qp->s_lock);
	spin_unlock(&qp->s_hlock);
	spin_unlock_irq(&qp->r_lock);
	return -EINVAL;
}

 
int rvt_destroy_qp(struct ib_qp *ibqp, struct ib_udata *udata)
{
	struct rvt_qp *qp = ibqp_to_rvtqp(ibqp);
	struct rvt_dev_info *rdi = ib_to_rvt(ibqp->device);

	rvt_reset_qp(rdi, qp, ibqp->qp_type);

	wait_event(qp->wait, !atomic_read(&qp->refcount));
	 
	rvt_free_qpn(&rdi->qp_dev->qpn_table, qp->ibqp.qp_num);

	spin_lock(&rdi->n_qps_lock);
	rdi->n_qps_allocated--;
	if (qp->ibqp.qp_type == IB_QPT_RC) {
		rdi->n_rc_qps--;
		rdi->busy_jiffies = rdi->n_rc_qps / RC_QP_SCALING_INTERVAL;
	}
	spin_unlock(&rdi->n_qps_lock);

	if (qp->ip)
		kref_put(&qp->ip->ref, rvt_release_mmap_info);
	kvfree(qp->r_rq.kwq);
	rdi->driver_f.qp_priv_free(rdi, qp);
	kfree(qp->s_ack_queue);
	kfree(qp->r_sg_list);
	rdma_destroy_ah_attr(&qp->remote_ah_attr);
	rdma_destroy_ah_attr(&qp->alt_ah_attr);
	free_ud_wq_attr(qp);
	vfree(qp->s_wq);
	return 0;
}

 
int rvt_query_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr,
		 int attr_mask, struct ib_qp_init_attr *init_attr)
{
	struct rvt_qp *qp = ibqp_to_rvtqp(ibqp);
	struct rvt_dev_info *rdi = ib_to_rvt(ibqp->device);

	attr->qp_state = qp->state;
	attr->cur_qp_state = attr->qp_state;
	attr->path_mtu = rdi->driver_f.mtu_to_path_mtu(qp->pmtu);
	attr->path_mig_state = qp->s_mig_state;
	attr->qkey = qp->qkey;
	attr->rq_psn = qp->r_psn & rdi->dparms.psn_mask;
	attr->sq_psn = qp->s_next_psn & rdi->dparms.psn_mask;
	attr->dest_qp_num = qp->remote_qpn;
	attr->qp_access_flags = qp->qp_access_flags;
	attr->cap.max_send_wr = qp->s_size - 1 -
		rdi->dparms.reserved_operations;
	attr->cap.max_recv_wr = qp->ibqp.srq ? 0 : qp->r_rq.size - 1;
	attr->cap.max_send_sge = qp->s_max_sge;
	attr->cap.max_recv_sge = qp->r_rq.max_sge;
	attr->cap.max_inline_data = 0;
	attr->ah_attr = qp->remote_ah_attr;
	attr->alt_ah_attr = qp->alt_ah_attr;
	attr->pkey_index = qp->s_pkey_index;
	attr->alt_pkey_index = qp->s_alt_pkey_index;
	attr->en_sqd_async_notify = 0;
	attr->sq_draining = qp->s_draining;
	attr->max_rd_atomic = qp->s_max_rd_atomic;
	attr->max_dest_rd_atomic = qp->r_max_rd_atomic;
	attr->min_rnr_timer = qp->r_min_rnr_timer;
	attr->port_num = qp->port_num;
	attr->timeout = qp->timeout;
	attr->retry_cnt = qp->s_retry_cnt;
	attr->rnr_retry = qp->s_rnr_retry_cnt;
	attr->alt_port_num =
		rdma_ah_get_port_num(&qp->alt_ah_attr);
	attr->alt_timeout = qp->alt_timeout;

	init_attr->event_handler = qp->ibqp.event_handler;
	init_attr->qp_context = qp->ibqp.qp_context;
	init_attr->send_cq = qp->ibqp.send_cq;
	init_attr->recv_cq = qp->ibqp.recv_cq;
	init_attr->srq = qp->ibqp.srq;
	init_attr->cap = attr->cap;
	if (qp->s_flags & RVT_S_SIGNAL_REQ_WR)
		init_attr->sq_sig_type = IB_SIGNAL_REQ_WR;
	else
		init_attr->sq_sig_type = IB_SIGNAL_ALL_WR;
	init_attr->qp_type = qp->ibqp.qp_type;
	init_attr->port_num = qp->port_num;
	return 0;
}

 
int rvt_post_recv(struct ib_qp *ibqp, const struct ib_recv_wr *wr,
		  const struct ib_recv_wr **bad_wr)
{
	struct rvt_qp *qp = ibqp_to_rvtqp(ibqp);
	struct rvt_krwq *wq = qp->r_rq.kwq;
	unsigned long flags;
	int qp_err_flush = (ib_rvt_state_ops[qp->state] & RVT_FLUSH_RECV) &&
				!qp->ibqp.srq;

	 
	if (!(ib_rvt_state_ops[qp->state] & RVT_POST_RECV_OK) || !wq) {
		*bad_wr = wr;
		return -EINVAL;
	}

	for (; wr; wr = wr->next) {
		struct rvt_rwqe *wqe;
		u32 next;
		int i;

		if ((unsigned)wr->num_sge > qp->r_rq.max_sge) {
			*bad_wr = wr;
			return -EINVAL;
		}

		spin_lock_irqsave(&qp->r_rq.kwq->p_lock, flags);
		next = wq->head + 1;
		if (next >= qp->r_rq.size)
			next = 0;
		if (next == READ_ONCE(wq->tail)) {
			spin_unlock_irqrestore(&qp->r_rq.kwq->p_lock, flags);
			*bad_wr = wr;
			return -ENOMEM;
		}
		if (unlikely(qp_err_flush)) {
			struct ib_wc wc;

			memset(&wc, 0, sizeof(wc));
			wc.qp = &qp->ibqp;
			wc.opcode = IB_WC_RECV;
			wc.wr_id = wr->wr_id;
			wc.status = IB_WC_WR_FLUSH_ERR;
			rvt_cq_enter(ibcq_to_rvtcq(qp->ibqp.recv_cq), &wc, 1);
		} else {
			wqe = rvt_get_rwqe_ptr(&qp->r_rq, wq->head);
			wqe->wr_id = wr->wr_id;
			wqe->num_sge = wr->num_sge;
			for (i = 0; i < wr->num_sge; i++) {
				wqe->sg_list[i].addr = wr->sg_list[i].addr;
				wqe->sg_list[i].length = wr->sg_list[i].length;
				wqe->sg_list[i].lkey = wr->sg_list[i].lkey;
			}
			 
			smp_store_release(&wq->head, next);
		}
		spin_unlock_irqrestore(&qp->r_rq.kwq->p_lock, flags);
	}
	return 0;
}

 
static inline int rvt_qp_valid_operation(
	struct rvt_qp *qp,
	const struct rvt_operation_params *post_parms,
	const struct ib_send_wr *wr)
{
	int len;

	if (wr->opcode >= RVT_OPERATION_MAX || !post_parms[wr->opcode].length)
		return -EINVAL;
	if (!(post_parms[wr->opcode].qpt_support & BIT(qp->ibqp.qp_type)))
		return -EINVAL;
	if ((post_parms[wr->opcode].flags & RVT_OPERATION_PRIV) &&
	    ibpd_to_rvtpd(qp->ibqp.pd)->user)
		return -EINVAL;
	if (post_parms[wr->opcode].flags & RVT_OPERATION_ATOMIC_SGE &&
	    (wr->num_sge == 0 ||
	     wr->sg_list[0].length < sizeof(u64) ||
	     wr->sg_list[0].addr & (sizeof(u64) - 1)))
		return -EINVAL;
	if (post_parms[wr->opcode].flags & RVT_OPERATION_ATOMIC &&
	    !qp->s_max_rd_atomic)
		return -EINVAL;
	len = post_parms[wr->opcode].length;
	 
	if (qp->ibqp.qp_type != IB_QPT_UC &&
	    qp->ibqp.qp_type != IB_QPT_RC) {
		if (qp->ibqp.pd != ud_wr(wr)->ah->pd)
			return -EINVAL;
		len = sizeof(struct ib_ud_wr);
	}
	return len;
}

 
static inline int rvt_qp_is_avail(
	struct rvt_qp *qp,
	struct rvt_dev_info *rdi,
	bool reserved_op)
{
	u32 slast;
	u32 avail;
	u32 reserved_used;

	 
	smp_mb__before_atomic();
	if (unlikely(reserved_op)) {
		 
		reserved_used = atomic_read(&qp->s_reserved_used);
		if (reserved_used >= rdi->dparms.reserved_operations)
			return -ENOMEM;
		return 0;
	}
	 
	if (likely(qp->s_avail))
		return 0;
	 
	slast = smp_load_acquire(&qp->s_last);
	if (qp->s_head >= slast)
		avail = qp->s_size - (qp->s_head - slast);
	else
		avail = slast - qp->s_head;

	reserved_used = atomic_read(&qp->s_reserved_used);
	avail =  avail - 1 -
		(rdi->dparms.reserved_operations - reserved_used);
	 
	if ((s32)avail <= 0)
		return -ENOMEM;
	qp->s_avail = avail;
	if (WARN_ON(qp->s_avail >
		    (qp->s_size - 1 - rdi->dparms.reserved_operations)))
		rvt_pr_err(rdi,
			   "More avail entries than QP RB size.\nQP: %u, size: %u, avail: %u\nhead: %u, tail: %u, cur: %u, acked: %u, last: %u",
			   qp->ibqp.qp_num, qp->s_size, qp->s_avail,
			   qp->s_head, qp->s_tail, qp->s_cur,
			   qp->s_acked, qp->s_last);
	return 0;
}

 
static int rvt_post_one_wr(struct rvt_qp *qp,
			   const struct ib_send_wr *wr,
			   bool *call_send)
{
	struct rvt_swqe *wqe;
	u32 next;
	int i;
	int j;
	int acc;
	struct rvt_lkey_table *rkt;
	struct rvt_pd *pd;
	struct rvt_dev_info *rdi = ib_to_rvt(qp->ibqp.device);
	u8 log_pmtu;
	int ret;
	size_t cplen;
	bool reserved_op;
	int local_ops_delayed = 0;

	BUILD_BUG_ON(IB_QPT_MAX >= (sizeof(u32) * BITS_PER_BYTE));

	 
	if (unlikely(wr->num_sge > qp->s_max_sge))
		return -EINVAL;

	ret = rvt_qp_valid_operation(qp, rdi->post_parms, wr);
	if (ret < 0)
		return ret;
	cplen = ret;

	 
	if ((rdi->post_parms[wr->opcode].flags & RVT_OPERATION_LOCAL)) {
		switch (wr->opcode) {
		case IB_WR_REG_MR:
			ret = rvt_fast_reg_mr(qp,
					      reg_wr(wr)->mr,
					      reg_wr(wr)->key,
					      reg_wr(wr)->access);
			if (ret || !(wr->send_flags & IB_SEND_SIGNALED))
				return ret;
			break;
		case IB_WR_LOCAL_INV:
			if ((wr->send_flags & IB_SEND_FENCE) ||
			    atomic_read(&qp->local_ops_pending)) {
				local_ops_delayed = 1;
			} else {
				ret = rvt_invalidate_rkey(
					qp, wr->ex.invalidate_rkey);
				if (ret || !(wr->send_flags & IB_SEND_SIGNALED))
					return ret;
			}
			break;
		default:
			return -EINVAL;
		}
	}

	reserved_op = rdi->post_parms[wr->opcode].flags &
			RVT_OPERATION_USE_RESERVE;
	 
	ret = rvt_qp_is_avail(qp, rdi, reserved_op);
	if (ret)
		return ret;
	next = qp->s_head + 1;
	if (next >= qp->s_size)
		next = 0;

	rkt = &rdi->lkey_table;
	pd = ibpd_to_rvtpd(qp->ibqp.pd);
	wqe = rvt_get_swqe_ptr(qp, qp->s_head);

	 
	memcpy(&wqe->ud_wr, wr, cplen);

	wqe->length = 0;
	j = 0;
	if (wr->num_sge) {
		struct rvt_sge *last_sge = NULL;

		acc = wr->opcode >= IB_WR_RDMA_READ ?
			IB_ACCESS_LOCAL_WRITE : 0;
		for (i = 0; i < wr->num_sge; i++) {
			u32 length = wr->sg_list[i].length;

			if (length == 0)
				continue;
			ret = rvt_lkey_ok(rkt, pd, &wqe->sg_list[j], last_sge,
					  &wr->sg_list[i], acc);
			if (unlikely(ret < 0))
				goto bail_inval_free;
			wqe->length += length;
			if (ret)
				last_sge = &wqe->sg_list[j];
			j += ret;
		}
		wqe->wr.num_sge = j;
	}

	 
	log_pmtu = qp->log_pmtu;
	if (qp->allowed_ops == IB_OPCODE_UD) {
		struct rvt_ah *ah = rvt_get_swqe_ah(wqe);

		log_pmtu = ah->log_pmtu;
		rdma_copy_ah_attr(wqe->ud_wr.attr, &ah->attr);
	}

	if (rdi->post_parms[wr->opcode].flags & RVT_OPERATION_LOCAL) {
		if (local_ops_delayed)
			atomic_inc(&qp->local_ops_pending);
		else
			wqe->wr.send_flags |= RVT_SEND_COMPLETION_ONLY;
		wqe->ssn = 0;
		wqe->psn = 0;
		wqe->lpsn = 0;
	} else {
		wqe->ssn = qp->s_ssn++;
		wqe->psn = qp->s_next_psn;
		wqe->lpsn = wqe->psn +
				(wqe->length ?
					((wqe->length - 1) >> log_pmtu) :
					0);
	}

	 
	if (rdi->driver_f.setup_wqe) {
		ret = rdi->driver_f.setup_wqe(qp, wqe, call_send);
		if (ret < 0)
			goto bail_inval_free_ref;
	}

	if (!(rdi->post_parms[wr->opcode].flags & RVT_OPERATION_LOCAL))
		qp->s_next_psn = wqe->lpsn + 1;

	if (unlikely(reserved_op)) {
		wqe->wr.send_flags |= RVT_SEND_RESERVE_USED;
		rvt_qp_wqe_reserve(qp, wqe);
	} else {
		wqe->wr.send_flags &= ~RVT_SEND_RESERVE_USED;
		qp->s_avail--;
	}
	trace_rvt_post_one_wr(qp, wqe, wr->num_sge);
	smp_wmb();  
	qp->s_head = next;

	return 0;

bail_inval_free_ref:
	if (qp->allowed_ops == IB_OPCODE_UD)
		rdma_destroy_ah_attr(wqe->ud_wr.attr);
bail_inval_free:
	 
	while (j) {
		struct rvt_sge *sge = &wqe->sg_list[--j];

		rvt_put_mr(sge->mr);
	}
	return ret;
}

 
int rvt_post_send(struct ib_qp *ibqp, const struct ib_send_wr *wr,
		  const struct ib_send_wr **bad_wr)
{
	struct rvt_qp *qp = ibqp_to_rvtqp(ibqp);
	struct rvt_dev_info *rdi = ib_to_rvt(ibqp->device);
	unsigned long flags = 0;
	bool call_send;
	unsigned nreq = 0;
	int err = 0;

	spin_lock_irqsave(&qp->s_hlock, flags);

	 
	if (unlikely(!(ib_rvt_state_ops[qp->state] & RVT_POST_SEND_OK))) {
		spin_unlock_irqrestore(&qp->s_hlock, flags);
		return -EINVAL;
	}

	 
	call_send = qp->s_head == READ_ONCE(qp->s_last) && !wr->next;

	for (; wr; wr = wr->next) {
		err = rvt_post_one_wr(qp, wr, &call_send);
		if (unlikely(err)) {
			*bad_wr = wr;
			goto bail;
		}
		nreq++;
	}
bail:
	spin_unlock_irqrestore(&qp->s_hlock, flags);
	if (nreq) {
		 
		if (nreq == 1 && call_send)
			rdi->driver_f.do_send(qp);
		else
			rdi->driver_f.schedule_send_no_lock(qp);
	}
	return err;
}

 
int rvt_post_srq_recv(struct ib_srq *ibsrq, const struct ib_recv_wr *wr,
		      const struct ib_recv_wr **bad_wr)
{
	struct rvt_srq *srq = ibsrq_to_rvtsrq(ibsrq);
	struct rvt_krwq *wq;
	unsigned long flags;

	for (; wr; wr = wr->next) {
		struct rvt_rwqe *wqe;
		u32 next;
		int i;

		if ((unsigned)wr->num_sge > srq->rq.max_sge) {
			*bad_wr = wr;
			return -EINVAL;
		}

		spin_lock_irqsave(&srq->rq.kwq->p_lock, flags);
		wq = srq->rq.kwq;
		next = wq->head + 1;
		if (next >= srq->rq.size)
			next = 0;
		if (next == READ_ONCE(wq->tail)) {
			spin_unlock_irqrestore(&srq->rq.kwq->p_lock, flags);
			*bad_wr = wr;
			return -ENOMEM;
		}

		wqe = rvt_get_rwqe_ptr(&srq->rq, wq->head);
		wqe->wr_id = wr->wr_id;
		wqe->num_sge = wr->num_sge;
		for (i = 0; i < wr->num_sge; i++) {
			wqe->sg_list[i].addr = wr->sg_list[i].addr;
			wqe->sg_list[i].length = wr->sg_list[i].length;
			wqe->sg_list[i].lkey = wr->sg_list[i].lkey;
		}
		 
		smp_store_release(&wq->head, next);
		spin_unlock_irqrestore(&srq->rq.kwq->p_lock, flags);
	}
	return 0;
}

 
static struct ib_sge *rvt_cast_sge(struct rvt_wqe_sge *sge)
{
	BUILD_BUG_ON(offsetof(struct ib_sge, addr) !=
		     offsetof(struct rvt_wqe_sge, addr));
	BUILD_BUG_ON(offsetof(struct ib_sge, length) !=
		     offsetof(struct rvt_wqe_sge, length));
	BUILD_BUG_ON(offsetof(struct ib_sge, lkey) !=
		     offsetof(struct rvt_wqe_sge, lkey));
	return (struct ib_sge *)sge;
}

 
static int init_sge(struct rvt_qp *qp, struct rvt_rwqe *wqe)
{
	int i, j, ret;
	struct ib_wc wc;
	struct rvt_lkey_table *rkt;
	struct rvt_pd *pd;
	struct rvt_sge_state *ss;
	struct rvt_dev_info *rdi = ib_to_rvt(qp->ibqp.device);

	rkt = &rdi->lkey_table;
	pd = ibpd_to_rvtpd(qp->ibqp.srq ? qp->ibqp.srq->pd : qp->ibqp.pd);
	ss = &qp->r_sge;
	ss->sg_list = qp->r_sg_list;
	qp->r_len = 0;
	for (i = j = 0; i < wqe->num_sge; i++) {
		if (wqe->sg_list[i].length == 0)
			continue;
		 
		ret = rvt_lkey_ok(rkt, pd, j ? &ss->sg_list[j - 1] : &ss->sge,
				  NULL, rvt_cast_sge(&wqe->sg_list[i]),
				  IB_ACCESS_LOCAL_WRITE);
		if (unlikely(ret <= 0))
			goto bad_lkey;
		qp->r_len += wqe->sg_list[i].length;
		j++;
	}
	ss->num_sge = j;
	ss->total_len = qp->r_len;
	return 1;

bad_lkey:
	while (j) {
		struct rvt_sge *sge = --j ? &ss->sg_list[j - 1] : &ss->sge;

		rvt_put_mr(sge->mr);
	}
	ss->num_sge = 0;
	memset(&wc, 0, sizeof(wc));
	wc.wr_id = wqe->wr_id;
	wc.status = IB_WC_LOC_PROT_ERR;
	wc.opcode = IB_WC_RECV;
	wc.qp = &qp->ibqp;
	 
	rvt_cq_enter(ibcq_to_rvtcq(qp->ibqp.recv_cq), &wc, 1);
	return 0;
}

 
static inline u32 get_rvt_head(struct rvt_rq *rq, void *ip)
{
	u32 head;

	if (ip)
		head = RDMA_READ_UAPI_ATOMIC(rq->wq->head);
	else
		head = rq->kwq->head;

	return head;
}

 
int rvt_get_rwqe(struct rvt_qp *qp, bool wr_id_only)
{
	unsigned long flags;
	struct rvt_rq *rq;
	struct rvt_krwq *kwq = NULL;
	struct rvt_rwq *wq;
	struct rvt_srq *srq;
	struct rvt_rwqe *wqe;
	void (*handler)(struct ib_event *, void *);
	u32 tail;
	u32 head;
	int ret;
	void *ip = NULL;

	if (qp->ibqp.srq) {
		srq = ibsrq_to_rvtsrq(qp->ibqp.srq);
		handler = srq->ibsrq.event_handler;
		rq = &srq->rq;
		ip = srq->ip;
	} else {
		srq = NULL;
		handler = NULL;
		rq = &qp->r_rq;
		ip = qp->ip;
	}

	spin_lock_irqsave(&rq->kwq->c_lock, flags);
	if (!(ib_rvt_state_ops[qp->state] & RVT_PROCESS_RECV_OK)) {
		ret = 0;
		goto unlock;
	}
	kwq = rq->kwq;
	if (ip) {
		wq = rq->wq;
		tail = RDMA_READ_UAPI_ATOMIC(wq->tail);
	} else {
		tail = kwq->tail;
	}

	 
	if (tail >= rq->size)
		tail = 0;

	if (kwq->count < RVT_RWQ_COUNT_THRESHOLD) {
		head = get_rvt_head(rq, ip);
		kwq->count = rvt_get_rq_count(rq, head, tail);
	}
	if (unlikely(kwq->count == 0)) {
		ret = 0;
		goto unlock;
	}
	 
	smp_rmb();
	wqe = rvt_get_rwqe_ptr(rq, tail);
	 
	if (++tail >= rq->size)
		tail = 0;
	if (ip)
		RDMA_WRITE_UAPI_ATOMIC(wq->tail, tail);
	else
		kwq->tail = tail;
	if (!wr_id_only && !init_sge(qp, wqe)) {
		ret = -1;
		goto unlock;
	}
	qp->r_wr_id = wqe->wr_id;

	kwq->count--;
	ret = 1;
	set_bit(RVT_R_WRID_VALID, &qp->r_aflags);
	if (handler) {
		 
		if (kwq->count < srq->limit) {
			kwq->count =
				rvt_get_rq_count(rq,
						 get_rvt_head(rq, ip), tail);
			if (kwq->count < srq->limit) {
				struct ib_event ev;

				srq->limit = 0;
				spin_unlock_irqrestore(&rq->kwq->c_lock, flags);
				ev.device = qp->ibqp.device;
				ev.element.srq = qp->ibqp.srq;
				ev.event = IB_EVENT_SRQ_LIMIT_REACHED;
				handler(&ev, srq->ibsrq.srq_context);
				goto bail;
			}
		}
	}
unlock:
	spin_unlock_irqrestore(&rq->kwq->c_lock, flags);
bail:
	return ret;
}
EXPORT_SYMBOL(rvt_get_rwqe);

 
void rvt_comm_est(struct rvt_qp *qp)
{
	qp->r_flags |= RVT_R_COMM_EST;
	if (qp->ibqp.event_handler) {
		struct ib_event ev;

		ev.device = qp->ibqp.device;
		ev.element.qp = &qp->ibqp;
		ev.event = IB_EVENT_COMM_EST;
		qp->ibqp.event_handler(&ev, qp->ibqp.qp_context);
	}
}
EXPORT_SYMBOL(rvt_comm_est);

void rvt_rc_error(struct rvt_qp *qp, enum ib_wc_status err)
{
	unsigned long flags;
	int lastwqe;

	spin_lock_irqsave(&qp->s_lock, flags);
	lastwqe = rvt_error_qp(qp, err);
	spin_unlock_irqrestore(&qp->s_lock, flags);

	if (lastwqe) {
		struct ib_event ev;

		ev.device = qp->ibqp.device;
		ev.element.qp = &qp->ibqp;
		ev.event = IB_EVENT_QP_LAST_WQE_REACHED;
		qp->ibqp.event_handler(&ev, qp->ibqp.qp_context);
	}
}
EXPORT_SYMBOL(rvt_rc_error);

 
unsigned long rvt_rnr_tbl_to_usec(u32 index)
{
	return ib_rvt_rnr_table[(index & IB_AETH_CREDIT_MASK)];
}
EXPORT_SYMBOL(rvt_rnr_tbl_to_usec);

static inline unsigned long rvt_aeth_to_usec(u32 aeth)
{
	return ib_rvt_rnr_table[(aeth >> IB_AETH_CREDIT_SHIFT) &
				  IB_AETH_CREDIT_MASK];
}

 
void rvt_add_retry_timer_ext(struct rvt_qp *qp, u8 shift)
{
	struct ib_qp *ibqp = &qp->ibqp;
	struct rvt_dev_info *rdi = ib_to_rvt(ibqp->device);

	lockdep_assert_held(&qp->s_lock);
	qp->s_flags |= RVT_S_TIMER;
        
	qp->s_timer.expires = jiffies + rdi->busy_jiffies +
			      (qp->timeout_jiffies << shift);
	add_timer(&qp->s_timer);
}
EXPORT_SYMBOL(rvt_add_retry_timer_ext);

 
void rvt_add_rnr_timer(struct rvt_qp *qp, u32 aeth)
{
	u32 to;

	lockdep_assert_held(&qp->s_lock);
	qp->s_flags |= RVT_S_WAIT_RNR;
	to = rvt_aeth_to_usec(aeth);
	trace_rvt_rnrnak_add(qp, to);
	hrtimer_start(&qp->s_rnr_timer,
		      ns_to_ktime(1000 * to), HRTIMER_MODE_REL_PINNED);
}
EXPORT_SYMBOL(rvt_add_rnr_timer);

 
void rvt_stop_rc_timers(struct rvt_qp *qp)
{
	lockdep_assert_held(&qp->s_lock);
	 
	if (qp->s_flags & (RVT_S_TIMER | RVT_S_WAIT_RNR)) {
		qp->s_flags &= ~(RVT_S_TIMER | RVT_S_WAIT_RNR);
		del_timer(&qp->s_timer);
		hrtimer_try_to_cancel(&qp->s_rnr_timer);
	}
}
EXPORT_SYMBOL(rvt_stop_rc_timers);

 
static void rvt_stop_rnr_timer(struct rvt_qp *qp)
{
	lockdep_assert_held(&qp->s_lock);
	 
	if (qp->s_flags & RVT_S_WAIT_RNR) {
		qp->s_flags &= ~RVT_S_WAIT_RNR;
		trace_rvt_rnrnak_stop(qp, 0);
	}
}

 
void rvt_del_timers_sync(struct rvt_qp *qp)
{
	del_timer_sync(&qp->s_timer);
	hrtimer_cancel(&qp->s_rnr_timer);
}
EXPORT_SYMBOL(rvt_del_timers_sync);

 
static void rvt_rc_timeout(struct timer_list *t)
{
	struct rvt_qp *qp = from_timer(qp, t, s_timer);
	struct rvt_dev_info *rdi = ib_to_rvt(qp->ibqp.device);
	unsigned long flags;

	spin_lock_irqsave(&qp->r_lock, flags);
	spin_lock(&qp->s_lock);
	if (qp->s_flags & RVT_S_TIMER) {
		struct rvt_ibport *rvp = rdi->ports[qp->port_num - 1];

		qp->s_flags &= ~RVT_S_TIMER;
		rvp->n_rc_timeouts++;
		del_timer(&qp->s_timer);
		trace_rvt_rc_timeout(qp, qp->s_last_psn + 1);
		if (rdi->driver_f.notify_restart_rc)
			rdi->driver_f.notify_restart_rc(qp,
							qp->s_last_psn + 1,
							1);
		rdi->driver_f.schedule_send(qp);
	}
	spin_unlock(&qp->s_lock);
	spin_unlock_irqrestore(&qp->r_lock, flags);
}

 
enum hrtimer_restart rvt_rc_rnr_retry(struct hrtimer *t)
{
	struct rvt_qp *qp = container_of(t, struct rvt_qp, s_rnr_timer);
	struct rvt_dev_info *rdi = ib_to_rvt(qp->ibqp.device);
	unsigned long flags;

	spin_lock_irqsave(&qp->s_lock, flags);
	rvt_stop_rnr_timer(qp);
	trace_rvt_rnrnak_timeout(qp, 0);
	rdi->driver_f.schedule_send(qp);
	spin_unlock_irqrestore(&qp->s_lock, flags);
	return HRTIMER_NORESTART;
}
EXPORT_SYMBOL(rvt_rc_rnr_retry);

 
struct rvt_qp_iter *rvt_qp_iter_init(struct rvt_dev_info *rdi,
				     u64 v,
				     void (*cb)(struct rvt_qp *qp, u64 v))
{
	struct rvt_qp_iter *i;

	i = kzalloc(sizeof(*i), GFP_KERNEL);
	if (!i)
		return NULL;

	i->rdi = rdi;
	 
	i->specials = rdi->ibdev.phys_port_cnt * 2;
	i->v = v;
	i->cb = cb;

	return i;
}
EXPORT_SYMBOL(rvt_qp_iter_init);

 
int rvt_qp_iter_next(struct rvt_qp_iter *iter)
	__must_hold(RCU)
{
	int n = iter->n;
	int ret = 1;
	struct rvt_qp *pqp = iter->qp;
	struct rvt_qp *qp;
	struct rvt_dev_info *rdi = iter->rdi;

	 
	for (; n <  rdi->qp_dev->qp_table_size + iter->specials; n++) {
		if (pqp) {
			qp = rcu_dereference(pqp->next);
		} else {
			if (n < iter->specials) {
				struct rvt_ibport *rvp;
				int pidx;

				pidx = n % rdi->ibdev.phys_port_cnt;
				rvp = rdi->ports[pidx];
				qp = rcu_dereference(rvp->qp[n & 1]);
			} else {
				qp = rcu_dereference(
					rdi->qp_dev->qp_table[
						(n - iter->specials)]);
			}
		}
		pqp = qp;
		if (qp) {
			iter->qp = qp;
			iter->n = n;
			return 0;
		}
	}
	return ret;
}
EXPORT_SYMBOL(rvt_qp_iter_next);

 
void rvt_qp_iter(struct rvt_dev_info *rdi,
		 u64 v,
		 void (*cb)(struct rvt_qp *qp, u64 v))
{
	int ret;
	struct rvt_qp_iter i = {
		.rdi = rdi,
		.specials = rdi->ibdev.phys_port_cnt * 2,
		.v = v,
		.cb = cb
	};

	rcu_read_lock();
	do {
		ret = rvt_qp_iter_next(&i);
		if (!ret) {
			rvt_get_qp(i.qp);
			rcu_read_unlock();
			i.cb(i.qp, i.v);
			rcu_read_lock();
			rvt_put_qp(i.qp);
		}
	} while (!ret);
	rcu_read_unlock();
}
EXPORT_SYMBOL(rvt_qp_iter);

 
void rvt_send_complete(struct rvt_qp *qp, struct rvt_swqe *wqe,
		       enum ib_wc_status status)
{
	u32 old_last, last;
	struct rvt_dev_info *rdi;

	if (!(ib_rvt_state_ops[qp->state] & RVT_PROCESS_OR_FLUSH_SEND))
		return;
	rdi = ib_to_rvt(qp->ibqp.device);

	old_last = qp->s_last;
	trace_rvt_qp_send_completion(qp, wqe, old_last);
	last = rvt_qp_complete_swqe(qp, wqe, rdi->wc_opcode[wqe->wr.opcode],
				    status);
	if (qp->s_acked == old_last)
		qp->s_acked = last;
	if (qp->s_cur == old_last)
		qp->s_cur = last;
	if (qp->s_tail == old_last)
		qp->s_tail = last;
	if (qp->state == IB_QPS_SQD && last == qp->s_cur)
		qp->s_draining = 0;
}
EXPORT_SYMBOL(rvt_send_complete);

 
void rvt_copy_sge(struct rvt_qp *qp, struct rvt_sge_state *ss,
		  void *data, u32 length,
		  bool release, bool copy_last)
{
	struct rvt_sge *sge = &ss->sge;
	int i;
	bool in_last = false;
	bool cacheless_copy = false;
	struct rvt_dev_info *rdi = ib_to_rvt(qp->ibqp.device);
	struct rvt_wss *wss = rdi->wss;
	unsigned int sge_copy_mode = rdi->dparms.sge_copy_mode;

	if (sge_copy_mode == RVT_SGE_COPY_CACHELESS) {
		cacheless_copy = length >= PAGE_SIZE;
	} else if (sge_copy_mode == RVT_SGE_COPY_ADAPTIVE) {
		if (length >= PAGE_SIZE) {
			 
			wss_insert(wss, sge->vaddr);
			if (length >= (2 * PAGE_SIZE))
				wss_insert(wss, (sge->vaddr + PAGE_SIZE));

			cacheless_copy = wss_exceeds_threshold(wss);
		} else {
			wss_advance_clean_counter(wss);
		}
	}

	if (copy_last) {
		if (length > 8) {
			length -= 8;
		} else {
			copy_last = false;
			in_last = true;
		}
	}

again:
	while (length) {
		u32 len = rvt_get_sge_length(sge, length);

		WARN_ON_ONCE(len == 0);
		if (unlikely(in_last)) {
			 
			for (i = 0; i < len; i++)
				((u8 *)sge->vaddr)[i] = ((u8 *)data)[i];
		} else if (cacheless_copy) {
			cacheless_memcpy(sge->vaddr, data, len);
		} else {
			memcpy(sge->vaddr, data, len);
		}
		rvt_update_sge(ss, len, release);
		data += len;
		length -= len;
	}

	if (copy_last) {
		copy_last = false;
		in_last = true;
		length = 8;
		goto again;
	}
}
EXPORT_SYMBOL(rvt_copy_sge);

static enum ib_wc_status loopback_qp_drop(struct rvt_ibport *rvp,
					  struct rvt_qp *sqp)
{
	rvp->n_pkt_drops++;
	 
	return sqp->ibqp.qp_type == IB_QPT_RC ?
		IB_WC_RETRY_EXC_ERR : IB_WC_SUCCESS;
}

 
void rvt_ruc_loopback(struct rvt_qp *sqp)
{
	struct rvt_ibport *rvp =  NULL;
	struct rvt_dev_info *rdi = ib_to_rvt(sqp->ibqp.device);
	struct rvt_qp *qp;
	struct rvt_swqe *wqe;
	struct rvt_sge *sge;
	unsigned long flags;
	struct ib_wc wc;
	u64 sdata;
	atomic64_t *maddr;
	enum ib_wc_status send_status;
	bool release;
	int ret;
	bool copy_last = false;
	int local_ops = 0;

	rcu_read_lock();
	rvp = rdi->ports[sqp->port_num - 1];

	 

	qp = rvt_lookup_qpn(ib_to_rvt(sqp->ibqp.device), rvp,
			    sqp->remote_qpn);

	spin_lock_irqsave(&sqp->s_lock, flags);

	 
	if ((sqp->s_flags & (RVT_S_BUSY | RVT_S_ANY_WAIT)) ||
	    !(ib_rvt_state_ops[sqp->state] & RVT_PROCESS_OR_FLUSH_SEND))
		goto unlock;

	sqp->s_flags |= RVT_S_BUSY;

again:
	if (sqp->s_last == READ_ONCE(sqp->s_head))
		goto clr_busy;
	wqe = rvt_get_swqe_ptr(sqp, sqp->s_last);

	 
	if (!(ib_rvt_state_ops[sqp->state] & RVT_PROCESS_NEXT_SEND_OK)) {
		if (!(ib_rvt_state_ops[sqp->state] & RVT_FLUSH_SEND))
			goto clr_busy;
		 
		send_status = IB_WC_WR_FLUSH_ERR;
		goto flush_send;
	}

	 
	if (sqp->s_last == sqp->s_cur) {
		if (++sqp->s_cur >= sqp->s_size)
			sqp->s_cur = 0;
	}
	spin_unlock_irqrestore(&sqp->s_lock, flags);

	if (!qp) {
		send_status = loopback_qp_drop(rvp, sqp);
		goto serr_no_r_lock;
	}
	spin_lock_irqsave(&qp->r_lock, flags);
	if (!(ib_rvt_state_ops[qp->state] & RVT_PROCESS_RECV_OK) ||
	    qp->ibqp.qp_type != sqp->ibqp.qp_type) {
		send_status = loopback_qp_drop(rvp, sqp);
		goto serr;
	}

	memset(&wc, 0, sizeof(wc));
	send_status = IB_WC_SUCCESS;

	release = true;
	sqp->s_sge.sge = wqe->sg_list[0];
	sqp->s_sge.sg_list = wqe->sg_list + 1;
	sqp->s_sge.num_sge = wqe->wr.num_sge;
	sqp->s_len = wqe->length;
	switch (wqe->wr.opcode) {
	case IB_WR_REG_MR:
		goto send_comp;

	case IB_WR_LOCAL_INV:
		if (!(wqe->wr.send_flags & RVT_SEND_COMPLETION_ONLY)) {
			if (rvt_invalidate_rkey(sqp,
						wqe->wr.ex.invalidate_rkey))
				send_status = IB_WC_LOC_PROT_ERR;
			local_ops = 1;
		}
		goto send_comp;

	case IB_WR_SEND_WITH_INV:
	case IB_WR_SEND_WITH_IMM:
	case IB_WR_SEND:
		ret = rvt_get_rwqe(qp, false);
		if (ret < 0)
			goto op_err;
		if (!ret)
			goto rnr_nak;
		if (wqe->length > qp->r_len)
			goto inv_err;
		switch (wqe->wr.opcode) {
		case IB_WR_SEND_WITH_INV:
			if (!rvt_invalidate_rkey(qp,
						 wqe->wr.ex.invalidate_rkey)) {
				wc.wc_flags = IB_WC_WITH_INVALIDATE;
				wc.ex.invalidate_rkey =
					wqe->wr.ex.invalidate_rkey;
			}
			break;
		case IB_WR_SEND_WITH_IMM:
			wc.wc_flags = IB_WC_WITH_IMM;
			wc.ex.imm_data = wqe->wr.ex.imm_data;
			break;
		default:
			break;
		}
		break;

	case IB_WR_RDMA_WRITE_WITH_IMM:
		if (unlikely(!(qp->qp_access_flags & IB_ACCESS_REMOTE_WRITE)))
			goto inv_err;
		wc.wc_flags = IB_WC_WITH_IMM;
		wc.ex.imm_data = wqe->wr.ex.imm_data;
		ret = rvt_get_rwqe(qp, true);
		if (ret < 0)
			goto op_err;
		if (!ret)
			goto rnr_nak;
		 
		goto do_write;
	case IB_WR_RDMA_WRITE:
		copy_last = rvt_is_user_qp(qp);
		if (unlikely(!(qp->qp_access_flags & IB_ACCESS_REMOTE_WRITE)))
			goto inv_err;
do_write:
		if (wqe->length == 0)
			break;
		if (unlikely(!rvt_rkey_ok(qp, &qp->r_sge.sge, wqe->length,
					  wqe->rdma_wr.remote_addr,
					  wqe->rdma_wr.rkey,
					  IB_ACCESS_REMOTE_WRITE)))
			goto acc_err;
		qp->r_sge.sg_list = NULL;
		qp->r_sge.num_sge = 1;
		qp->r_sge.total_len = wqe->length;
		break;

	case IB_WR_RDMA_READ:
		if (unlikely(!(qp->qp_access_flags & IB_ACCESS_REMOTE_READ)))
			goto inv_err;
		if (unlikely(!rvt_rkey_ok(qp, &sqp->s_sge.sge, wqe->length,
					  wqe->rdma_wr.remote_addr,
					  wqe->rdma_wr.rkey,
					  IB_ACCESS_REMOTE_READ)))
			goto acc_err;
		release = false;
		sqp->s_sge.sg_list = NULL;
		sqp->s_sge.num_sge = 1;
		qp->r_sge.sge = wqe->sg_list[0];
		qp->r_sge.sg_list = wqe->sg_list + 1;
		qp->r_sge.num_sge = wqe->wr.num_sge;
		qp->r_sge.total_len = wqe->length;
		break;

	case IB_WR_ATOMIC_CMP_AND_SWP:
	case IB_WR_ATOMIC_FETCH_AND_ADD:
		if (unlikely(!(qp->qp_access_flags & IB_ACCESS_REMOTE_ATOMIC)))
			goto inv_err;
		if (unlikely(wqe->atomic_wr.remote_addr & (sizeof(u64) - 1)))
			goto inv_err;
		if (unlikely(!rvt_rkey_ok(qp, &qp->r_sge.sge, sizeof(u64),
					  wqe->atomic_wr.remote_addr,
					  wqe->atomic_wr.rkey,
					  IB_ACCESS_REMOTE_ATOMIC)))
			goto acc_err;
		 
		maddr = (atomic64_t *)qp->r_sge.sge.vaddr;
		sdata = wqe->atomic_wr.compare_add;
		*(u64 *)sqp->s_sge.sge.vaddr =
			(wqe->wr.opcode == IB_WR_ATOMIC_FETCH_AND_ADD) ?
			(u64)atomic64_add_return(sdata, maddr) - sdata :
			(u64)cmpxchg((u64 *)qp->r_sge.sge.vaddr,
				      sdata, wqe->atomic_wr.swap);
		rvt_put_mr(qp->r_sge.sge.mr);
		qp->r_sge.num_sge = 0;
		goto send_comp;

	default:
		send_status = IB_WC_LOC_QP_OP_ERR;
		goto serr;
	}

	sge = &sqp->s_sge.sge;
	while (sqp->s_len) {
		u32 len = rvt_get_sge_length(sge, sqp->s_len);

		WARN_ON_ONCE(len == 0);
		rvt_copy_sge(qp, &qp->r_sge, sge->vaddr,
			     len, release, copy_last);
		rvt_update_sge(&sqp->s_sge, len, !release);
		sqp->s_len -= len;
	}
	if (release)
		rvt_put_ss(&qp->r_sge);

	if (!test_and_clear_bit(RVT_R_WRID_VALID, &qp->r_aflags))
		goto send_comp;

	if (wqe->wr.opcode == IB_WR_RDMA_WRITE_WITH_IMM)
		wc.opcode = IB_WC_RECV_RDMA_WITH_IMM;
	else
		wc.opcode = IB_WC_RECV;
	wc.wr_id = qp->r_wr_id;
	wc.status = IB_WC_SUCCESS;
	wc.byte_len = wqe->length;
	wc.qp = &qp->ibqp;
	wc.src_qp = qp->remote_qpn;
	wc.slid = rdma_ah_get_dlid(&qp->remote_ah_attr) & U16_MAX;
	wc.sl = rdma_ah_get_sl(&qp->remote_ah_attr);
	wc.port_num = 1;
	 
	rvt_recv_cq(qp, &wc, wqe->wr.send_flags & IB_SEND_SOLICITED);

send_comp:
	spin_unlock_irqrestore(&qp->r_lock, flags);
	spin_lock_irqsave(&sqp->s_lock, flags);
	rvp->n_loop_pkts++;
flush_send:
	sqp->s_rnr_retry = sqp->s_rnr_retry_cnt;
	spin_lock(&sqp->r_lock);
	rvt_send_complete(sqp, wqe, send_status);
	spin_unlock(&sqp->r_lock);
	if (local_ops) {
		atomic_dec(&sqp->local_ops_pending);
		local_ops = 0;
	}
	goto again;

rnr_nak:
	 
	if (qp->ibqp.qp_type == IB_QPT_UC)
		goto send_comp;
	rvp->n_rnr_naks++;
	 
	if (sqp->s_rnr_retry == 0) {
		send_status = IB_WC_RNR_RETRY_EXC_ERR;
		goto serr;
	}
	if (sqp->s_rnr_retry_cnt < 7)
		sqp->s_rnr_retry--;
	spin_unlock_irqrestore(&qp->r_lock, flags);
	spin_lock_irqsave(&sqp->s_lock, flags);
	if (!(ib_rvt_state_ops[sqp->state] & RVT_PROCESS_RECV_OK))
		goto clr_busy;
	rvt_add_rnr_timer(sqp, qp->r_min_rnr_timer <<
				IB_AETH_CREDIT_SHIFT);
	goto clr_busy;

op_err:
	send_status = IB_WC_REM_OP_ERR;
	wc.status = IB_WC_LOC_QP_OP_ERR;
	goto err;

inv_err:
	send_status =
		sqp->ibqp.qp_type == IB_QPT_RC ?
			IB_WC_REM_INV_REQ_ERR :
			IB_WC_SUCCESS;
	wc.status = IB_WC_LOC_QP_OP_ERR;
	goto err;

acc_err:
	send_status = IB_WC_REM_ACCESS_ERR;
	wc.status = IB_WC_LOC_PROT_ERR;
err:
	 
	rvt_rc_error(qp, wc.status);

serr:
	spin_unlock_irqrestore(&qp->r_lock, flags);
serr_no_r_lock:
	spin_lock_irqsave(&sqp->s_lock, flags);
	spin_lock(&sqp->r_lock);
	rvt_send_complete(sqp, wqe, send_status);
	spin_unlock(&sqp->r_lock);
	if (sqp->ibqp.qp_type == IB_QPT_RC) {
		int lastwqe;

		spin_lock(&sqp->r_lock);
		lastwqe = rvt_error_qp(sqp, IB_WC_WR_FLUSH_ERR);
		spin_unlock(&sqp->r_lock);

		sqp->s_flags &= ~RVT_S_BUSY;
		spin_unlock_irqrestore(&sqp->s_lock, flags);
		if (lastwqe) {
			struct ib_event ev;

			ev.device = sqp->ibqp.device;
			ev.element.qp = &sqp->ibqp;
			ev.event = IB_EVENT_QP_LAST_WQE_REACHED;
			sqp->ibqp.event_handler(&ev, sqp->ibqp.qp_context);
		}
		goto done;
	}
clr_busy:
	sqp->s_flags &= ~RVT_S_BUSY;
unlock:
	spin_unlock_irqrestore(&sqp->s_lock, flags);
done:
	rcu_read_unlock();
}
EXPORT_SYMBOL(rvt_ruc_loopback);
