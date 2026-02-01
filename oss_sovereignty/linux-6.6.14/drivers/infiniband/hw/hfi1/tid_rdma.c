
 

#include "hfi.h"
#include "qp.h"
#include "rc.h"
#include "verbs.h"
#include "tid_rdma.h"
#include "exp_rcv.h"
#include "trace.h"

 

#define RCV_TID_FLOW_TABLE_CTRL_FLOW_VALID_SMASK BIT_ULL(32)
#define RCV_TID_FLOW_TABLE_CTRL_HDR_SUPP_EN_SMASK BIT_ULL(33)
#define RCV_TID_FLOW_TABLE_CTRL_KEEP_AFTER_SEQ_ERR_SMASK BIT_ULL(34)
#define RCV_TID_FLOW_TABLE_CTRL_KEEP_ON_GEN_ERR_SMASK BIT_ULL(35)
#define RCV_TID_FLOW_TABLE_STATUS_SEQ_MISMATCH_SMASK BIT_ULL(37)
#define RCV_TID_FLOW_TABLE_STATUS_GEN_MISMATCH_SMASK BIT_ULL(38)

 
#define MAX_TID_FLOW_PSN BIT(HFI1_KDETH_BTH_SEQ_SHIFT)

#define GENERATION_MASK 0xFFFFF

static u32 mask_generation(u32 a)
{
	return a & GENERATION_MASK;
}

 
#define KERN_GENERATION_RESERVED mask_generation(U32_MAX)

 
#define TID_RDMA_JKEY                   32
#define HFI1_KERNEL_MIN_JKEY HFI1_ADMIN_JKEY_RANGE
#define HFI1_KERNEL_MAX_JKEY (2 * HFI1_ADMIN_JKEY_RANGE - 1)

 
#define TID_RDMA_MAX_READ_SEGS_PER_REQ  6
#define TID_RDMA_MAX_WRITE_SEGS_PER_REQ 4
#define MAX_REQ max_t(u16, TID_RDMA_MAX_READ_SEGS_PER_REQ, \
			TID_RDMA_MAX_WRITE_SEGS_PER_REQ)
#define MAX_FLOWS roundup_pow_of_two(MAX_REQ + 1)

#define MAX_EXPECTED_PAGES     (MAX_EXPECTED_BUFFER / PAGE_SIZE)

#define TID_RDMA_DESTQP_FLOW_SHIFT      11
#define TID_RDMA_DESTQP_FLOW_MASK       0x1f

#define TID_OPFN_QP_CTXT_MASK 0xff
#define TID_OPFN_QP_CTXT_SHIFT 56
#define TID_OPFN_QP_KDETH_MASK 0xff
#define TID_OPFN_QP_KDETH_SHIFT 48
#define TID_OPFN_MAX_LEN_MASK 0x7ff
#define TID_OPFN_MAX_LEN_SHIFT 37
#define TID_OPFN_TIMEOUT_MASK 0x1f
#define TID_OPFN_TIMEOUT_SHIFT 32
#define TID_OPFN_RESERVED_MASK 0x3f
#define TID_OPFN_RESERVED_SHIFT 26
#define TID_OPFN_URG_MASK 0x1
#define TID_OPFN_URG_SHIFT 25
#define TID_OPFN_VER_MASK 0x7
#define TID_OPFN_VER_SHIFT 22
#define TID_OPFN_JKEY_MASK 0x3f
#define TID_OPFN_JKEY_SHIFT 16
#define TID_OPFN_MAX_READ_MASK 0x3f
#define TID_OPFN_MAX_READ_SHIFT 10
#define TID_OPFN_MAX_WRITE_MASK 0x3f
#define TID_OPFN_MAX_WRITE_SHIFT 4

 

static void tid_rdma_trigger_resume(struct work_struct *work);
static void hfi1_kern_exp_rcv_free_flows(struct tid_rdma_request *req);
static int hfi1_kern_exp_rcv_alloc_flows(struct tid_rdma_request *req,
					 gfp_t gfp);
static void hfi1_init_trdma_req(struct rvt_qp *qp,
				struct tid_rdma_request *req);
static void hfi1_tid_write_alloc_resources(struct rvt_qp *qp, bool intr_ctx);
static void hfi1_tid_timeout(struct timer_list *t);
static void hfi1_add_tid_reap_timer(struct rvt_qp *qp);
static void hfi1_mod_tid_reap_timer(struct rvt_qp *qp);
static void hfi1_mod_tid_retry_timer(struct rvt_qp *qp);
static int hfi1_stop_tid_retry_timer(struct rvt_qp *qp);
static void hfi1_tid_retry_timeout(struct timer_list *t);
static int make_tid_rdma_ack(struct rvt_qp *qp,
			     struct ib_other_headers *ohdr,
			     struct hfi1_pkt_state *ps);
static void hfi1_do_tid_send(struct rvt_qp *qp);
static u32 read_r_next_psn(struct hfi1_devdata *dd, u8 ctxt, u8 fidx);
static void tid_rdma_rcv_err(struct hfi1_packet *packet,
			     struct ib_other_headers *ohdr,
			     struct rvt_qp *qp, u32 psn, int diff, bool fecn);
static void update_r_next_psn_fecn(struct hfi1_packet *packet,
				   struct hfi1_qp_priv *priv,
				   struct hfi1_ctxtdata *rcd,
				   struct tid_rdma_flow *flow,
				   bool fecn);

static void validate_r_tid_ack(struct hfi1_qp_priv *priv)
{
	if (priv->r_tid_ack == HFI1_QP_WQE_INVALID)
		priv->r_tid_ack = priv->r_tid_tail;
}

static void tid_rdma_schedule_ack(struct rvt_qp *qp)
{
	struct hfi1_qp_priv *priv = qp->priv;

	priv->s_flags |= RVT_S_ACK_PENDING;
	hfi1_schedule_tid_send(qp);
}

static void tid_rdma_trigger_ack(struct rvt_qp *qp)
{
	validate_r_tid_ack(qp->priv);
	tid_rdma_schedule_ack(qp);
}

static u64 tid_rdma_opfn_encode(struct tid_rdma_params *p)
{
	return
		(((u64)p->qp & TID_OPFN_QP_CTXT_MASK) <<
			TID_OPFN_QP_CTXT_SHIFT) |
		((((u64)p->qp >> 16) & TID_OPFN_QP_KDETH_MASK) <<
			TID_OPFN_QP_KDETH_SHIFT) |
		(((u64)((p->max_len >> PAGE_SHIFT) - 1) &
			TID_OPFN_MAX_LEN_MASK) << TID_OPFN_MAX_LEN_SHIFT) |
		(((u64)p->timeout & TID_OPFN_TIMEOUT_MASK) <<
			TID_OPFN_TIMEOUT_SHIFT) |
		(((u64)p->urg & TID_OPFN_URG_MASK) << TID_OPFN_URG_SHIFT) |
		(((u64)p->jkey & TID_OPFN_JKEY_MASK) << TID_OPFN_JKEY_SHIFT) |
		(((u64)p->max_read & TID_OPFN_MAX_READ_MASK) <<
			TID_OPFN_MAX_READ_SHIFT) |
		(((u64)p->max_write & TID_OPFN_MAX_WRITE_MASK) <<
			TID_OPFN_MAX_WRITE_SHIFT);
}

static void tid_rdma_opfn_decode(struct tid_rdma_params *p, u64 data)
{
	p->max_len = (((data >> TID_OPFN_MAX_LEN_SHIFT) &
		TID_OPFN_MAX_LEN_MASK) + 1) << PAGE_SHIFT;
	p->jkey = (data >> TID_OPFN_JKEY_SHIFT) & TID_OPFN_JKEY_MASK;
	p->max_write = (data >> TID_OPFN_MAX_WRITE_SHIFT) &
		TID_OPFN_MAX_WRITE_MASK;
	p->max_read = (data >> TID_OPFN_MAX_READ_SHIFT) &
		TID_OPFN_MAX_READ_MASK;
	p->qp =
		((((data >> TID_OPFN_QP_KDETH_SHIFT) & TID_OPFN_QP_KDETH_MASK)
			<< 16) |
		((data >> TID_OPFN_QP_CTXT_SHIFT) & TID_OPFN_QP_CTXT_MASK));
	p->urg = (data >> TID_OPFN_URG_SHIFT) & TID_OPFN_URG_MASK;
	p->timeout = (data >> TID_OPFN_TIMEOUT_SHIFT) & TID_OPFN_TIMEOUT_MASK;
}

void tid_rdma_opfn_init(struct rvt_qp *qp, struct tid_rdma_params *p)
{
	struct hfi1_qp_priv *priv = qp->priv;

	p->qp = (RVT_KDETH_QP_PREFIX << 16) | priv->rcd->ctxt;
	p->max_len = TID_RDMA_MAX_SEGMENT_SIZE;
	p->jkey = priv->rcd->jkey;
	p->max_read = TID_RDMA_MAX_READ_SEGS_PER_REQ;
	p->max_write = TID_RDMA_MAX_WRITE_SEGS_PER_REQ;
	p->timeout = qp->timeout;
	p->urg = is_urg_masked(priv->rcd);
}

bool tid_rdma_conn_req(struct rvt_qp *qp, u64 *data)
{
	struct hfi1_qp_priv *priv = qp->priv;

	*data = tid_rdma_opfn_encode(&priv->tid_rdma.local);
	return true;
}

bool tid_rdma_conn_reply(struct rvt_qp *qp, u64 data)
{
	struct hfi1_qp_priv *priv = qp->priv;
	struct tid_rdma_params *remote, *old;
	bool ret = true;

	old = rcu_dereference_protected(priv->tid_rdma.remote,
					lockdep_is_held(&priv->opfn.lock));
	data &= ~0xfULL;
	 
	if (!data || !HFI1_CAP_IS_KSET(TID_RDMA))
		goto null;
	 
	remote = kzalloc(sizeof(*remote), GFP_ATOMIC);
	if (!remote) {
		ret = false;
		goto null;
	}

	tid_rdma_opfn_decode(remote, data);
	priv->tid_timer_timeout_jiffies =
		usecs_to_jiffies((((4096UL * (1UL << remote->timeout)) /
				   1000UL) << 3) * 7);
	trace_hfi1_opfn_param(qp, 0, &priv->tid_rdma.local);
	trace_hfi1_opfn_param(qp, 1, remote);
	rcu_assign_pointer(priv->tid_rdma.remote, remote);
	 
	priv->pkts_ps = (u16)rvt_div_mtu(qp, remote->max_len);
	priv->timeout_shift = ilog2(priv->pkts_ps - 1) + 1;
	goto free;
null:
	RCU_INIT_POINTER(priv->tid_rdma.remote, NULL);
	priv->timeout_shift = 0;
free:
	if (old)
		kfree_rcu(old, rcu_head);
	return ret;
}

bool tid_rdma_conn_resp(struct rvt_qp *qp, u64 *data)
{
	bool ret;

	ret = tid_rdma_conn_reply(qp, *data);
	*data = 0;
	 
	if (ret)
		(void)tid_rdma_conn_req(qp, data);
	return ret;
}

void tid_rdma_conn_error(struct rvt_qp *qp)
{
	struct hfi1_qp_priv *priv = qp->priv;
	struct tid_rdma_params *old;

	old = rcu_dereference_protected(priv->tid_rdma.remote,
					lockdep_is_held(&priv->opfn.lock));
	RCU_INIT_POINTER(priv->tid_rdma.remote, NULL);
	if (old)
		kfree_rcu(old, rcu_head);
}

 
int hfi1_kern_exp_rcv_init(struct hfi1_ctxtdata *rcd, int reinit)
{
	if (reinit)
		return 0;

	BUILD_BUG_ON(TID_RDMA_JKEY < HFI1_KERNEL_MIN_JKEY);
	BUILD_BUG_ON(TID_RDMA_JKEY > HFI1_KERNEL_MAX_JKEY);
	rcd->jkey = TID_RDMA_JKEY;
	hfi1_set_ctxt_jkey(rcd->dd, rcd, rcd->jkey);
	return hfi1_alloc_ctxt_rcv_groups(rcd);
}

 
static struct hfi1_ctxtdata *qp_to_rcd(struct rvt_dev_info *rdi,
				       struct rvt_qp *qp)
{
	struct hfi1_ibdev *verbs_dev = container_of(rdi,
						    struct hfi1_ibdev,
						    rdi);
	struct hfi1_devdata *dd = container_of(verbs_dev,
					       struct hfi1_devdata,
					       verbs_dev);
	unsigned int ctxt;

	if (qp->ibqp.qp_num == 0)
		ctxt = 0;
	else
		ctxt = hfi1_get_qp_map(dd, qp->ibqp.qp_num >> dd->qos_shift);
	return dd->rcd[ctxt];
}

int hfi1_qp_priv_init(struct rvt_dev_info *rdi, struct rvt_qp *qp,
		      struct ib_qp_init_attr *init_attr)
{
	struct hfi1_qp_priv *qpriv = qp->priv;
	int i, ret;

	qpriv->rcd = qp_to_rcd(rdi, qp);

	spin_lock_init(&qpriv->opfn.lock);
	INIT_WORK(&qpriv->opfn.opfn_work, opfn_send_conn_request);
	INIT_WORK(&qpriv->tid_rdma.trigger_work, tid_rdma_trigger_resume);
	qpriv->flow_state.psn = 0;
	qpriv->flow_state.index = RXE_NUM_TID_FLOWS;
	qpriv->flow_state.last_index = RXE_NUM_TID_FLOWS;
	qpriv->flow_state.generation = KERN_GENERATION_RESERVED;
	qpriv->s_state = TID_OP(WRITE_RESP);
	qpriv->s_tid_cur = HFI1_QP_WQE_INVALID;
	qpriv->s_tid_head = HFI1_QP_WQE_INVALID;
	qpriv->s_tid_tail = HFI1_QP_WQE_INVALID;
	qpriv->rnr_nak_state = TID_RNR_NAK_INIT;
	qpriv->r_tid_head = HFI1_QP_WQE_INVALID;
	qpriv->r_tid_tail = HFI1_QP_WQE_INVALID;
	qpriv->r_tid_ack = HFI1_QP_WQE_INVALID;
	qpriv->r_tid_alloc = HFI1_QP_WQE_INVALID;
	atomic_set(&qpriv->n_requests, 0);
	atomic_set(&qpriv->n_tid_requests, 0);
	timer_setup(&qpriv->s_tid_timer, hfi1_tid_timeout, 0);
	timer_setup(&qpriv->s_tid_retry_timer, hfi1_tid_retry_timeout, 0);
	INIT_LIST_HEAD(&qpriv->tid_wait);

	if (init_attr->qp_type == IB_QPT_RC && HFI1_CAP_IS_KSET(TID_RDMA)) {
		struct hfi1_devdata *dd = qpriv->rcd->dd;

		qpriv->pages = kzalloc_node(TID_RDMA_MAX_PAGES *
						sizeof(*qpriv->pages),
					    GFP_KERNEL, dd->node);
		if (!qpriv->pages)
			return -ENOMEM;
		for (i = 0; i < qp->s_size; i++) {
			struct hfi1_swqe_priv *priv;
			struct rvt_swqe *wqe = rvt_get_swqe_ptr(qp, i);

			priv = kzalloc_node(sizeof(*priv), GFP_KERNEL,
					    dd->node);
			if (!priv)
				return -ENOMEM;

			hfi1_init_trdma_req(qp, &priv->tid_req);
			priv->tid_req.e.swqe = wqe;
			wqe->priv = priv;
		}
		for (i = 0; i < rvt_max_atomic(rdi); i++) {
			struct hfi1_ack_priv *priv;

			priv = kzalloc_node(sizeof(*priv), GFP_KERNEL,
					    dd->node);
			if (!priv)
				return -ENOMEM;

			hfi1_init_trdma_req(qp, &priv->tid_req);
			priv->tid_req.e.ack = &qp->s_ack_queue[i];

			ret = hfi1_kern_exp_rcv_alloc_flows(&priv->tid_req,
							    GFP_KERNEL);
			if (ret) {
				kfree(priv);
				return ret;
			}
			qp->s_ack_queue[i].priv = priv;
		}
	}

	return 0;
}

void hfi1_qp_priv_tid_free(struct rvt_dev_info *rdi, struct rvt_qp *qp)
{
	struct hfi1_qp_priv *qpriv = qp->priv;
	struct rvt_swqe *wqe;
	u32 i;

	if (qp->ibqp.qp_type == IB_QPT_RC && HFI1_CAP_IS_KSET(TID_RDMA)) {
		for (i = 0; i < qp->s_size; i++) {
			wqe = rvt_get_swqe_ptr(qp, i);
			kfree(wqe->priv);
			wqe->priv = NULL;
		}
		for (i = 0; i < rvt_max_atomic(rdi); i++) {
			struct hfi1_ack_priv *priv = qp->s_ack_queue[i].priv;

			if (priv)
				hfi1_kern_exp_rcv_free_flows(&priv->tid_req);
			kfree(priv);
			qp->s_ack_queue[i].priv = NULL;
		}
		cancel_work_sync(&qpriv->opfn.opfn_work);
		kfree(qpriv->pages);
		qpriv->pages = NULL;
	}
}

 
 

 
static struct rvt_qp *first_qp(struct hfi1_ctxtdata *rcd,
			       struct tid_queue *queue)
	__must_hold(&rcd->exp_lock)
{
	struct hfi1_qp_priv *priv;

	lockdep_assert_held(&rcd->exp_lock);
	priv = list_first_entry_or_null(&queue->queue_head,
					struct hfi1_qp_priv,
					tid_wait);
	if (!priv)
		return NULL;
	rvt_get_qp(priv->owner);
	return priv->owner;
}

 
static bool kernel_tid_waiters(struct hfi1_ctxtdata *rcd,
			       struct tid_queue *queue, struct rvt_qp *qp)
	__must_hold(&rcd->exp_lock) __must_hold(&qp->s_lock)
{
	struct rvt_qp *fqp;
	bool ret = true;

	lockdep_assert_held(&qp->s_lock);
	lockdep_assert_held(&rcd->exp_lock);
	fqp = first_qp(rcd, queue);
	if (!fqp || (fqp == qp && (qp->s_flags & HFI1_S_WAIT_TID_SPACE)))
		ret = false;
	rvt_put_qp(fqp);
	return ret;
}

 
static void dequeue_tid_waiter(struct hfi1_ctxtdata *rcd,
			       struct tid_queue *queue, struct rvt_qp *qp)
	__must_hold(&rcd->exp_lock) __must_hold(&qp->s_lock)
{
	struct hfi1_qp_priv *priv = qp->priv;

	lockdep_assert_held(&qp->s_lock);
	lockdep_assert_held(&rcd->exp_lock);
	if (list_empty(&priv->tid_wait))
		return;
	list_del_init(&priv->tid_wait);
	qp->s_flags &= ~HFI1_S_WAIT_TID_SPACE;
	queue->dequeue++;
	rvt_put_qp(qp);
}

 
static void queue_qp_for_tid_wait(struct hfi1_ctxtdata *rcd,
				  struct tid_queue *queue, struct rvt_qp *qp)
	__must_hold(&rcd->exp_lock) __must_hold(&qp->s_lock)
{
	struct hfi1_qp_priv *priv = qp->priv;

	lockdep_assert_held(&qp->s_lock);
	lockdep_assert_held(&rcd->exp_lock);
	if (list_empty(&priv->tid_wait)) {
		qp->s_flags |= HFI1_S_WAIT_TID_SPACE;
		list_add_tail(&priv->tid_wait, &queue->queue_head);
		priv->tid_enqueue = ++queue->enqueue;
		rcd->dd->verbs_dev.n_tidwait++;
		trace_hfi1_qpsleep(qp, HFI1_S_WAIT_TID_SPACE);
		rvt_get_qp(qp);
	}
}

 
static void __trigger_tid_waiter(struct rvt_qp *qp)
	__must_hold(&qp->s_lock)
{
	lockdep_assert_held(&qp->s_lock);
	if (!(qp->s_flags & HFI1_S_WAIT_TID_SPACE))
		return;
	trace_hfi1_qpwakeup(qp, HFI1_S_WAIT_TID_SPACE);
	hfi1_schedule_send(qp);
}

 
static void tid_rdma_schedule_tid_wakeup(struct rvt_qp *qp)
{
	struct hfi1_qp_priv *priv;
	struct hfi1_ibport *ibp;
	struct hfi1_pportdata *ppd;
	struct hfi1_devdata *dd;
	bool rval;

	if (!qp)
		return;

	priv = qp->priv;
	ibp = to_iport(qp->ibqp.device, qp->port_num);
	ppd = ppd_from_ibp(ibp);
	dd = dd_from_ibdev(qp->ibqp.device);

	rval = queue_work_on(priv->s_sde ?
			     priv->s_sde->cpu :
			     cpumask_first(cpumask_of_node(dd->node)),
			     ppd->hfi1_wq,
			     &priv->tid_rdma.trigger_work);
	if (!rval)
		rvt_put_qp(qp);
}

 
static void tid_rdma_trigger_resume(struct work_struct *work)
{
	struct tid_rdma_qp_params *tr;
	struct hfi1_qp_priv *priv;
	struct rvt_qp *qp;

	tr = container_of(work, struct tid_rdma_qp_params, trigger_work);
	priv = container_of(tr, struct hfi1_qp_priv, tid_rdma);
	qp = priv->owner;
	spin_lock_irq(&qp->s_lock);
	if (qp->s_flags & HFI1_S_WAIT_TID_SPACE) {
		spin_unlock_irq(&qp->s_lock);
		hfi1_do_send(priv->owner, true);
	} else {
		spin_unlock_irq(&qp->s_lock);
	}
	rvt_put_qp(qp);
}

 
static void _tid_rdma_flush_wait(struct rvt_qp *qp, struct tid_queue *queue)
	__must_hold(&qp->s_lock)
{
	struct hfi1_qp_priv *priv;

	if (!qp)
		return;
	lockdep_assert_held(&qp->s_lock);
	priv = qp->priv;
	qp->s_flags &= ~HFI1_S_WAIT_TID_SPACE;
	spin_lock(&priv->rcd->exp_lock);
	if (!list_empty(&priv->tid_wait)) {
		list_del_init(&priv->tid_wait);
		qp->s_flags &= ~HFI1_S_WAIT_TID_SPACE;
		queue->dequeue++;
		rvt_put_qp(qp);
	}
	spin_unlock(&priv->rcd->exp_lock);
}

void hfi1_tid_rdma_flush_wait(struct rvt_qp *qp)
	__must_hold(&qp->s_lock)
{
	struct hfi1_qp_priv *priv = qp->priv;

	_tid_rdma_flush_wait(qp, &priv->rcd->flow_queue);
	_tid_rdma_flush_wait(qp, &priv->rcd->rarr_queue);
}

 
 
static int kern_reserve_flow(struct hfi1_ctxtdata *rcd, int last)
	__must_hold(&rcd->exp_lock)
{
	int nr;

	 
	if (last >= 0 && last < RXE_NUM_TID_FLOWS &&
	    !test_and_set_bit(last, &rcd->flow_mask))
		return last;

	nr = ffz(rcd->flow_mask);
	BUILD_BUG_ON(RXE_NUM_TID_FLOWS >=
		     (sizeof(rcd->flow_mask) * BITS_PER_BYTE));
	if (nr > (RXE_NUM_TID_FLOWS - 1))
		return -EAGAIN;
	set_bit(nr, &rcd->flow_mask);
	return nr;
}

static void kern_set_hw_flow(struct hfi1_ctxtdata *rcd, u32 generation,
			     u32 flow_idx)
{
	u64 reg;

	reg = ((u64)generation << HFI1_KDETH_BTH_SEQ_SHIFT) |
		RCV_TID_FLOW_TABLE_CTRL_FLOW_VALID_SMASK |
		RCV_TID_FLOW_TABLE_CTRL_KEEP_AFTER_SEQ_ERR_SMASK |
		RCV_TID_FLOW_TABLE_CTRL_KEEP_ON_GEN_ERR_SMASK |
		RCV_TID_FLOW_TABLE_STATUS_SEQ_MISMATCH_SMASK |
		RCV_TID_FLOW_TABLE_STATUS_GEN_MISMATCH_SMASK;

	if (generation != KERN_GENERATION_RESERVED)
		reg |= RCV_TID_FLOW_TABLE_CTRL_HDR_SUPP_EN_SMASK;

	write_uctxt_csr(rcd->dd, rcd->ctxt,
			RCV_TID_FLOW_TABLE + 8 * flow_idx, reg);
}

static u32 kern_setup_hw_flow(struct hfi1_ctxtdata *rcd, u32 flow_idx)
	__must_hold(&rcd->exp_lock)
{
	u32 generation = rcd->flows[flow_idx].generation;

	kern_set_hw_flow(rcd, generation, flow_idx);
	return generation;
}

static u32 kern_flow_generation_next(u32 gen)
{
	u32 generation = mask_generation(gen + 1);

	if (generation == KERN_GENERATION_RESERVED)
		generation = mask_generation(generation + 1);
	return generation;
}

static void kern_clear_hw_flow(struct hfi1_ctxtdata *rcd, u32 flow_idx)
	__must_hold(&rcd->exp_lock)
{
	rcd->flows[flow_idx].generation =
		kern_flow_generation_next(rcd->flows[flow_idx].generation);
	kern_set_hw_flow(rcd, KERN_GENERATION_RESERVED, flow_idx);
}

int hfi1_kern_setup_hw_flow(struct hfi1_ctxtdata *rcd, struct rvt_qp *qp)
{
	struct hfi1_qp_priv *qpriv = (struct hfi1_qp_priv *)qp->priv;
	struct tid_flow_state *fs = &qpriv->flow_state;
	struct rvt_qp *fqp;
	unsigned long flags;
	int ret = 0;

	 
	if (fs->index != RXE_NUM_TID_FLOWS)
		return ret;

	spin_lock_irqsave(&rcd->exp_lock, flags);
	if (kernel_tid_waiters(rcd, &rcd->flow_queue, qp))
		goto queue;

	ret = kern_reserve_flow(rcd, fs->last_index);
	if (ret < 0)
		goto queue;
	fs->index = ret;
	fs->last_index = fs->index;

	 
	if (fs->generation != KERN_GENERATION_RESERVED)
		rcd->flows[fs->index].generation = fs->generation;
	fs->generation = kern_setup_hw_flow(rcd, fs->index);
	fs->psn = 0;
	dequeue_tid_waiter(rcd, &rcd->flow_queue, qp);
	 
	fqp = first_qp(rcd, &rcd->flow_queue);
	spin_unlock_irqrestore(&rcd->exp_lock, flags);

	tid_rdma_schedule_tid_wakeup(fqp);
	return 0;
queue:
	queue_qp_for_tid_wait(rcd, &rcd->flow_queue, qp);
	spin_unlock_irqrestore(&rcd->exp_lock, flags);
	return -EAGAIN;
}

void hfi1_kern_clear_hw_flow(struct hfi1_ctxtdata *rcd, struct rvt_qp *qp)
{
	struct hfi1_qp_priv *qpriv = (struct hfi1_qp_priv *)qp->priv;
	struct tid_flow_state *fs = &qpriv->flow_state;
	struct rvt_qp *fqp;
	unsigned long flags;

	if (fs->index >= RXE_NUM_TID_FLOWS)
		return;
	spin_lock_irqsave(&rcd->exp_lock, flags);
	kern_clear_hw_flow(rcd, fs->index);
	clear_bit(fs->index, &rcd->flow_mask);
	fs->index = RXE_NUM_TID_FLOWS;
	fs->psn = 0;
	fs->generation = KERN_GENERATION_RESERVED;

	 
	fqp = first_qp(rcd, &rcd->flow_queue);
	spin_unlock_irqrestore(&rcd->exp_lock, flags);

	if (fqp == qp) {
		__trigger_tid_waiter(fqp);
		rvt_put_qp(fqp);
	} else {
		tid_rdma_schedule_tid_wakeup(fqp);
	}
}

void hfi1_kern_init_ctxt_generations(struct hfi1_ctxtdata *rcd)
{
	int i;

	for (i = 0; i < RXE_NUM_TID_FLOWS; i++) {
		rcd->flows[i].generation = mask_generation(get_random_u32());
		kern_set_hw_flow(rcd, KERN_GENERATION_RESERVED, i);
	}
}

 
static u8 trdma_pset_order(struct tid_rdma_pageset *s)
{
	u8 count = s->count;

	return ilog2(count) + 1;
}

 
static u32 tid_rdma_find_phys_blocks_4k(struct tid_rdma_flow *flow,
					struct page **pages,
					u32 npages,
					struct tid_rdma_pageset *list)
{
	u32 pagecount, pageidx, setcount = 0, i;
	void *vaddr, *this_vaddr;

	if (!npages)
		return 0;

	 
	vaddr = page_address(pages[0]);
	trace_hfi1_tid_flow_page(flow->req->qp, flow, 0, 0, 0, vaddr);
	for (pageidx = 0, pagecount = 1, i = 1; i <= npages; i++) {
		this_vaddr = i < npages ? page_address(pages[i]) : NULL;
		trace_hfi1_tid_flow_page(flow->req->qp, flow, i, 0, 0,
					 this_vaddr);
		 
		if (this_vaddr != (vaddr + PAGE_SIZE)) {
			 
			while (pagecount) {
				int maxpages = pagecount;
				u32 bufsize = pagecount * PAGE_SIZE;

				if (bufsize > MAX_EXPECTED_BUFFER)
					maxpages =
						MAX_EXPECTED_BUFFER >>
						PAGE_SHIFT;
				else if (!is_power_of_2(bufsize))
					maxpages =
						rounddown_pow_of_two(bufsize) >>
						PAGE_SHIFT;

				list[setcount].idx = pageidx;
				list[setcount].count = maxpages;
				trace_hfi1_tid_pageset(flow->req->qp, setcount,
						       list[setcount].idx,
						       list[setcount].count);
				pagecount -= maxpages;
				pageidx += maxpages;
				setcount++;
			}
			pageidx = i;
			pagecount = 1;
			vaddr = this_vaddr;
		} else {
			vaddr += PAGE_SIZE;
			pagecount++;
		}
	}
	 
	if (setcount & 1)
		list[setcount++].count = 0;
	return setcount;
}

 

static u32 tid_flush_pages(struct tid_rdma_pageset *list,
			   u32 *idx, u32 pages, u32 sets)
{
	while (pages) {
		u32 maxpages = pages;

		if (maxpages > MAX_EXPECTED_PAGES)
			maxpages = MAX_EXPECTED_PAGES;
		else if (!is_power_of_2(maxpages))
			maxpages = rounddown_pow_of_two(maxpages);
		list[sets].idx = *idx;
		list[sets++].count = maxpages;
		*idx += maxpages;
		pages -= maxpages;
	}
	 
	if (sets & 1)
		list[sets++].count = 0;
	return sets;
}

 
static u32 tid_rdma_find_phys_blocks_8k(struct tid_rdma_flow *flow,
					struct page **pages,
					u32 npages,
					struct tid_rdma_pageset *list)
{
	u32 idx, sets = 0, i;
	u32 pagecnt = 0;
	void *v0, *v1, *vm1;

	if (!npages)
		return 0;
	for (idx = 0, i = 0, vm1 = NULL; i < npages; i += 2) {
		 
		v0 = page_address(pages[i]);
		trace_hfi1_tid_flow_page(flow->req->qp, flow, i, 1, 0, v0);
		v1 = i + 1 < npages ?
				page_address(pages[i + 1]) : NULL;
		trace_hfi1_tid_flow_page(flow->req->qp, flow, i, 1, 1, v1);
		 
		if (v1 != (v0 + PAGE_SIZE)) {
			 
			sets = tid_flush_pages(list, &idx, pagecnt, sets);
			 
			list[sets].idx = idx++;
			list[sets++].count = 1;
			if (v1) {
				list[sets].count = 1;
				list[sets++].idx = idx++;
			} else {
				list[sets++].count = 0;
			}
			vm1 = NULL;
			pagecnt = 0;
			continue;
		}
		 
		if (vm1 && v0 != (vm1 + PAGE_SIZE)) {
			 
			sets = tid_flush_pages(list, &idx, pagecnt, sets);
			pagecnt = 0;
		}
		 
		pagecnt += 2;
		 
		vm1 = v1;
		 
	}
	 
	sets = tid_flush_pages(list, &idx, npages - idx, sets);
	 
	WARN_ON(sets & 1);
	return sets;
}

 
static u32 kern_find_pages(struct tid_rdma_flow *flow,
			   struct page **pages,
			   struct rvt_sge_state *ss, bool *last)
{
	struct tid_rdma_request *req = flow->req;
	struct rvt_sge *sge = &ss->sge;
	u32 length = flow->req->seg_len;
	u32 len = PAGE_SIZE;
	u32 i = 0;

	while (length && req->isge < ss->num_sge) {
		pages[i++] = virt_to_page(sge->vaddr);

		sge->vaddr += len;
		sge->length -= len;
		sge->sge_length -= len;
		if (!sge->sge_length) {
			if (++req->isge < ss->num_sge)
				*sge = ss->sg_list[req->isge - 1];
		} else if (sge->length == 0 && sge->mr->lkey) {
			if (++sge->n >= RVT_SEGSZ) {
				++sge->m;
				sge->n = 0;
			}
			sge->vaddr = sge->mr->map[sge->m]->segs[sge->n].vaddr;
			sge->length = sge->mr->map[sge->m]->segs[sge->n].length;
		}
		length -= len;
	}

	flow->length = flow->req->seg_len - length;
	*last = req->isge != ss->num_sge;
	return i;
}

static void dma_unmap_flow(struct tid_rdma_flow *flow)
{
	struct hfi1_devdata *dd;
	int i;
	struct tid_rdma_pageset *pset;

	dd = flow->req->rcd->dd;
	for (i = 0, pset = &flow->pagesets[0]; i < flow->npagesets;
			i++, pset++) {
		if (pset->count && pset->addr) {
			dma_unmap_page(&dd->pcidev->dev,
				       pset->addr,
				       PAGE_SIZE * pset->count,
				       DMA_FROM_DEVICE);
			pset->mapped = 0;
		}
	}
}

static int dma_map_flow(struct tid_rdma_flow *flow, struct page **pages)
{
	int i;
	struct hfi1_devdata *dd = flow->req->rcd->dd;
	struct tid_rdma_pageset *pset;

	for (i = 0, pset = &flow->pagesets[0]; i < flow->npagesets;
			i++, pset++) {
		if (pset->count) {
			pset->addr = dma_map_page(&dd->pcidev->dev,
						  pages[pset->idx],
						  0,
						  PAGE_SIZE * pset->count,
						  DMA_FROM_DEVICE);

			if (dma_mapping_error(&dd->pcidev->dev, pset->addr)) {
				dma_unmap_flow(flow);
				return -ENOMEM;
			}
			pset->mapped = 1;
		}
	}
	return 0;
}

static inline bool dma_mapped(struct tid_rdma_flow *flow)
{
	return !!flow->pagesets[0].mapped;
}

 
static int kern_get_phys_blocks(struct tid_rdma_flow *flow,
				struct page **pages,
				struct rvt_sge_state *ss, bool *last)
{
	u8 npages;

	 
	if (flow->npagesets) {
		trace_hfi1_tid_flow_alloc(flow->req->qp, flow->req->setup_head,
					  flow);
		if (!dma_mapped(flow))
			return dma_map_flow(flow, pages);
		return 0;
	}

	npages = kern_find_pages(flow, pages, ss, last);

	if (flow->req->qp->pmtu == enum_to_mtu(OPA_MTU_4096))
		flow->npagesets =
			tid_rdma_find_phys_blocks_4k(flow, pages, npages,
						     flow->pagesets);
	else
		flow->npagesets =
			tid_rdma_find_phys_blocks_8k(flow, pages, npages,
						     flow->pagesets);

	return dma_map_flow(flow, pages);
}

static inline void kern_add_tid_node(struct tid_rdma_flow *flow,
				     struct hfi1_ctxtdata *rcd, char *s,
				     struct tid_group *grp, u8 cnt)
{
	struct kern_tid_node *node = &flow->tnode[flow->tnode_cnt++];

	WARN_ON_ONCE(flow->tnode_cnt >=
		     (TID_RDMA_MAX_SEGMENT_SIZE >> PAGE_SHIFT));
	if (WARN_ON_ONCE(cnt & 1))
		dd_dev_err(rcd->dd,
			   "unexpected odd allocation cnt %u map 0x%x used %u",
			   cnt, grp->map, grp->used);

	node->grp = grp;
	node->map = grp->map;
	node->cnt = cnt;
	trace_hfi1_tid_node_add(flow->req->qp, s, flow->tnode_cnt - 1,
				grp->base, grp->map, grp->used, cnt);
}

 
static int kern_alloc_tids(struct tid_rdma_flow *flow)
{
	struct hfi1_ctxtdata *rcd = flow->req->rcd;
	struct hfi1_devdata *dd = rcd->dd;
	u32 ngroups, pageidx = 0;
	struct tid_group *group = NULL, *used;
	u8 use;

	flow->tnode_cnt = 0;
	ngroups = flow->npagesets / dd->rcv_entries.group_size;
	if (!ngroups)
		goto used_list;

	 
	list_for_each_entry(group,  &rcd->tid_group_list.list, list) {
		kern_add_tid_node(flow, rcd, "complete groups", group,
				  group->size);

		pageidx += group->size;
		if (!--ngroups)
			break;
	}

	if (pageidx >= flow->npagesets)
		goto ok;

used_list:
	 
	list_for_each_entry(used, &rcd->tid_used_list.list, list) {
		use = min_t(u32, flow->npagesets - pageidx,
			    used->size - used->used);
		kern_add_tid_node(flow, rcd, "used groups", used, use);

		pageidx += use;
		if (pageidx >= flow->npagesets)
			goto ok;
	}

	 
	if (group && &group->list == &rcd->tid_group_list.list)
		goto bail_eagain;
	group = list_prepare_entry(group, &rcd->tid_group_list.list,
				   list);
	if (list_is_last(&group->list, &rcd->tid_group_list.list))
		goto bail_eagain;
	group = list_next_entry(group, list);
	use = min_t(u32, flow->npagesets - pageidx, group->size);
	kern_add_tid_node(flow, rcd, "complete continue", group, use);
	pageidx += use;
	if (pageidx >= flow->npagesets)
		goto ok;
bail_eagain:
	trace_hfi1_msg_alloc_tids(flow->req->qp, " insufficient tids: needed ",
				  (u64)flow->npagesets);
	return -EAGAIN;
ok:
	return 0;
}

static void kern_program_rcv_group(struct tid_rdma_flow *flow, int grp_num,
				   u32 *pset_idx)
{
	struct hfi1_ctxtdata *rcd = flow->req->rcd;
	struct hfi1_devdata *dd = rcd->dd;
	struct kern_tid_node *node = &flow->tnode[grp_num];
	struct tid_group *grp = node->grp;
	struct tid_rdma_pageset *pset;
	u32 pmtu_pg = flow->req->qp->pmtu >> PAGE_SHIFT;
	u32 rcventry, npages = 0, pair = 0, tidctrl;
	u8 i, cnt = 0;

	for (i = 0; i < grp->size; i++) {
		rcventry = grp->base + i;

		if (node->map & BIT(i) || cnt >= node->cnt) {
			rcv_array_wc_fill(dd, rcventry);
			continue;
		}
		pset = &flow->pagesets[(*pset_idx)++];
		if (pset->count) {
			hfi1_put_tid(dd, rcventry, PT_EXPECTED,
				     pset->addr, trdma_pset_order(pset));
		} else {
			hfi1_put_tid(dd, rcventry, PT_INVALID, 0, 0);
		}
		npages += pset->count;

		rcventry -= rcd->expected_base;
		tidctrl = pair ? 0x3 : rcventry & 0x1 ? 0x2 : 0x1;
		 
		pair = !(i & 0x1) && !((node->map >> i) & 0x3) &&
			node->cnt >= cnt + 2;
		if (!pair) {
			if (!pset->count)
				tidctrl = 0x1;
			flow->tid_entry[flow->tidcnt++] =
				EXP_TID_SET(IDX, rcventry >> 1) |
				EXP_TID_SET(CTRL, tidctrl) |
				EXP_TID_SET(LEN, npages);
			trace_hfi1_tid_entry_alloc( 
			   flow->req->qp, flow->tidcnt - 1,
			   flow->tid_entry[flow->tidcnt - 1]);

			 
			flow->npkts += (npages + pmtu_pg - 1) >> ilog2(pmtu_pg);
			npages = 0;
		}

		if (grp->used == grp->size - 1)
			tid_group_move(grp, &rcd->tid_used_list,
				       &rcd->tid_full_list);
		else if (!grp->used)
			tid_group_move(grp, &rcd->tid_group_list,
				       &rcd->tid_used_list);

		grp->used++;
		grp->map |= BIT(i);
		cnt++;
	}
}

static void kern_unprogram_rcv_group(struct tid_rdma_flow *flow, int grp_num)
{
	struct hfi1_ctxtdata *rcd = flow->req->rcd;
	struct hfi1_devdata *dd = rcd->dd;
	struct kern_tid_node *node = &flow->tnode[grp_num];
	struct tid_group *grp = node->grp;
	u32 rcventry;
	u8 i, cnt = 0;

	for (i = 0; i < grp->size; i++) {
		rcventry = grp->base + i;

		if (node->map & BIT(i) || cnt >= node->cnt) {
			rcv_array_wc_fill(dd, rcventry);
			continue;
		}

		hfi1_put_tid(dd, rcventry, PT_INVALID, 0, 0);

		grp->used--;
		grp->map &= ~BIT(i);
		cnt++;

		if (grp->used == grp->size - 1)
			tid_group_move(grp, &rcd->tid_full_list,
				       &rcd->tid_used_list);
		else if (!grp->used)
			tid_group_move(grp, &rcd->tid_used_list,
				       &rcd->tid_group_list);
	}
	if (WARN_ON_ONCE(cnt & 1)) {
		struct hfi1_ctxtdata *rcd = flow->req->rcd;
		struct hfi1_devdata *dd = rcd->dd;

		dd_dev_err(dd, "unexpected odd free cnt %u map 0x%x used %u",
			   cnt, grp->map, grp->used);
	}
}

static void kern_program_rcvarray(struct tid_rdma_flow *flow)
{
	u32 pset_idx = 0;
	int i;

	flow->npkts = 0;
	flow->tidcnt = 0;
	for (i = 0; i < flow->tnode_cnt; i++)
		kern_program_rcv_group(flow, i, &pset_idx);
	trace_hfi1_tid_flow_alloc(flow->req->qp, flow->req->setup_head, flow);
}

 
int hfi1_kern_exp_rcv_setup(struct tid_rdma_request *req,
			    struct rvt_sge_state *ss, bool *last)
	__must_hold(&req->qp->s_lock)
{
	struct tid_rdma_flow *flow = &req->flows[req->setup_head];
	struct hfi1_ctxtdata *rcd = req->rcd;
	struct hfi1_qp_priv *qpriv = req->qp->priv;
	unsigned long flags;
	struct rvt_qp *fqp;
	u16 clear_tail = req->clear_tail;

	lockdep_assert_held(&req->qp->s_lock);
	 
	if (!CIRC_SPACE(req->setup_head, clear_tail, MAX_FLOWS) ||
	    CIRC_CNT(req->setup_head, clear_tail, MAX_FLOWS) >=
	    req->n_flows)
		return -EINVAL;

	 
	if (kern_get_phys_blocks(flow, qpriv->pages, ss, last)) {
		hfi1_wait_kmem(flow->req->qp);
		return -ENOMEM;
	}

	spin_lock_irqsave(&rcd->exp_lock, flags);
	if (kernel_tid_waiters(rcd, &rcd->rarr_queue, flow->req->qp))
		goto queue;

	 
	if (kern_alloc_tids(flow))
		goto queue;
	 
	kern_program_rcvarray(flow);

	 
	memset(&flow->flow_state, 0x0, sizeof(flow->flow_state));
	flow->idx = qpriv->flow_state.index;
	flow->flow_state.generation = qpriv->flow_state.generation;
	flow->flow_state.spsn = qpriv->flow_state.psn;
	flow->flow_state.lpsn = flow->flow_state.spsn + flow->npkts - 1;
	flow->flow_state.r_next_psn =
		full_flow_psn(flow, flow->flow_state.spsn);
	qpriv->flow_state.psn += flow->npkts;

	dequeue_tid_waiter(rcd, &rcd->rarr_queue, flow->req->qp);
	 
	fqp = first_qp(rcd, &rcd->rarr_queue);
	spin_unlock_irqrestore(&rcd->exp_lock, flags);
	tid_rdma_schedule_tid_wakeup(fqp);

	req->setup_head = (req->setup_head + 1) & (MAX_FLOWS - 1);
	return 0;
queue:
	queue_qp_for_tid_wait(rcd, &rcd->rarr_queue, flow->req->qp);
	spin_unlock_irqrestore(&rcd->exp_lock, flags);
	return -EAGAIN;
}

static void hfi1_tid_rdma_reset_flow(struct tid_rdma_flow *flow)
{
	flow->npagesets = 0;
}

 
int hfi1_kern_exp_rcv_clear(struct tid_rdma_request *req)
	__must_hold(&req->qp->s_lock)
{
	struct tid_rdma_flow *flow = &req->flows[req->clear_tail];
	struct hfi1_ctxtdata *rcd = req->rcd;
	unsigned long flags;
	int i;
	struct rvt_qp *fqp;

	lockdep_assert_held(&req->qp->s_lock);
	 
	if (!CIRC_CNT(req->setup_head, req->clear_tail, MAX_FLOWS))
		return -EINVAL;

	spin_lock_irqsave(&rcd->exp_lock, flags);

	for (i = 0; i < flow->tnode_cnt; i++)
		kern_unprogram_rcv_group(flow, i);
	 
	flow->tnode_cnt = 0;
	 
	fqp = first_qp(rcd, &rcd->rarr_queue);
	spin_unlock_irqrestore(&rcd->exp_lock, flags);

	dma_unmap_flow(flow);

	hfi1_tid_rdma_reset_flow(flow);
	req->clear_tail = (req->clear_tail + 1) & (MAX_FLOWS - 1);

	if (fqp == req->qp) {
		__trigger_tid_waiter(fqp);
		rvt_put_qp(fqp);
	} else {
		tid_rdma_schedule_tid_wakeup(fqp);
	}

	return 0;
}

 
void hfi1_kern_exp_rcv_clear_all(struct tid_rdma_request *req)
	__must_hold(&req->qp->s_lock)
{
	 
	while (CIRC_CNT(req->setup_head, req->clear_tail, MAX_FLOWS)) {
		if (hfi1_kern_exp_rcv_clear(req))
			break;
	}
}

 
static void hfi1_kern_exp_rcv_free_flows(struct tid_rdma_request *req)
{
	kfree(req->flows);
	req->flows = NULL;
}

 
void __trdma_clean_swqe(struct rvt_qp *qp, struct rvt_swqe *wqe)
{
	struct hfi1_swqe_priv *p = wqe->priv;

	hfi1_kern_exp_rcv_free_flows(&p->tid_req);
}

 
static int hfi1_kern_exp_rcv_alloc_flows(struct tid_rdma_request *req,
					 gfp_t gfp)
{
	struct tid_rdma_flow *flows;
	int i;

	if (likely(req->flows))
		return 0;
	flows = kmalloc_node(MAX_FLOWS * sizeof(*flows), gfp,
			     req->rcd->numa_id);
	if (!flows)
		return -ENOMEM;
	 
	for (i = 0; i < MAX_FLOWS; i++) {
		flows[i].req = req;
		flows[i].npagesets = 0;
		flows[i].pagesets[0].mapped =  0;
		flows[i].resync_npkts = 0;
	}
	req->flows = flows;
	return 0;
}

static void hfi1_init_trdma_req(struct rvt_qp *qp,
				struct tid_rdma_request *req)
{
	struct hfi1_qp_priv *qpriv = qp->priv;

	 
	req->qp = qp;
	req->rcd = qpriv->rcd;
}

u64 hfi1_access_sw_tid_wait(const struct cntr_entry *entry,
			    void *context, int vl, int mode, u64 data)
{
	struct hfi1_devdata *dd = context;

	return dd->verbs_dev.n_tidwait;
}

static struct tid_rdma_flow *find_flow_ib(struct tid_rdma_request *req,
					  u32 psn, u16 *fidx)
{
	u16 head, tail;
	struct tid_rdma_flow *flow;

	head = req->setup_head;
	tail = req->clear_tail;
	for ( ; CIRC_CNT(head, tail, MAX_FLOWS);
	     tail = CIRC_NEXT(tail, MAX_FLOWS)) {
		flow = &req->flows[tail];
		if (cmp_psn(psn, flow->flow_state.ib_spsn) >= 0 &&
		    cmp_psn(psn, flow->flow_state.ib_lpsn) <= 0) {
			if (fidx)
				*fidx = tail;
			return flow;
		}
	}
	return NULL;
}

 
u32 hfi1_build_tid_rdma_read_packet(struct rvt_swqe *wqe,
				    struct ib_other_headers *ohdr, u32 *bth1,
				    u32 *bth2, u32 *len)
{
	struct tid_rdma_request *req = wqe_to_tid_req(wqe);
	struct tid_rdma_flow *flow = &req->flows[req->flow_idx];
	struct rvt_qp *qp = req->qp;
	struct hfi1_qp_priv *qpriv = qp->priv;
	struct hfi1_swqe_priv *wpriv = wqe->priv;
	struct tid_rdma_read_req *rreq = &ohdr->u.tid_rdma.r_req;
	struct tid_rdma_params *remote;
	u32 req_len = 0;
	void *req_addr = NULL;

	 
	*bth2 = mask_psn(flow->flow_state.ib_spsn + flow->pkt);
	trace_hfi1_tid_flow_build_read_pkt(qp, req->flow_idx, flow);

	 
	req_addr = &flow->tid_entry[flow->tid_idx];
	req_len = sizeof(*flow->tid_entry) *
			(flow->tidcnt - flow->tid_idx);

	memset(&ohdr->u.tid_rdma.r_req, 0, sizeof(ohdr->u.tid_rdma.r_req));
	wpriv->ss.sge.vaddr = req_addr;
	wpriv->ss.sge.sge_length = req_len;
	wpriv->ss.sge.length = wpriv->ss.sge.sge_length;
	 
	wpriv->ss.sge.mr = NULL;
	wpriv->ss.sge.m = 0;
	wpriv->ss.sge.n = 0;

	wpriv->ss.sg_list = NULL;
	wpriv->ss.total_len = wpriv->ss.sge.sge_length;
	wpriv->ss.num_sge = 1;

	 
	rcu_read_lock();
	remote = rcu_dereference(qpriv->tid_rdma.remote);

	KDETH_RESET(rreq->kdeth0, KVER, 0x1);
	KDETH_RESET(rreq->kdeth1, JKEY, remote->jkey);
	rreq->reth.vaddr = cpu_to_be64(wqe->rdma_wr.remote_addr +
			   req->cur_seg * req->seg_len + flow->sent);
	rreq->reth.rkey = cpu_to_be32(wqe->rdma_wr.rkey);
	rreq->reth.length = cpu_to_be32(*len);
	rreq->tid_flow_psn =
		cpu_to_be32((flow->flow_state.generation <<
			     HFI1_KDETH_BTH_SEQ_SHIFT) |
			    ((flow->flow_state.spsn + flow->pkt) &
			     HFI1_KDETH_BTH_SEQ_MASK));
	rreq->tid_flow_qp =
		cpu_to_be32(qpriv->tid_rdma.local.qp |
			    ((flow->idx & TID_RDMA_DESTQP_FLOW_MASK) <<
			     TID_RDMA_DESTQP_FLOW_SHIFT) |
			    qpriv->rcd->ctxt);
	rreq->verbs_qp = cpu_to_be32(qp->remote_qpn);
	*bth1 &= ~RVT_QPN_MASK;
	*bth1 |= remote->qp;
	*bth2 |= IB_BTH_REQ_ACK;
	rcu_read_unlock();

	 
	flow->sent += *len;
	req->cur_seg++;
	qp->s_state = TID_OP(READ_REQ);
	req->ack_pending++;
	req->flow_idx = (req->flow_idx + 1) & (MAX_FLOWS - 1);
	qpriv->pending_tid_r_segs++;
	qp->s_num_rd_atomic++;

	 
	*len = req_len;

	return sizeof(ohdr->u.tid_rdma.r_req) / sizeof(u32);
}

 
u32 hfi1_build_tid_rdma_read_req(struct rvt_qp *qp, struct rvt_swqe *wqe,
				 struct ib_other_headers *ohdr, u32 *bth1,
				 u32 *bth2, u32 *len)
	__must_hold(&qp->s_lock)
{
	struct hfi1_qp_priv *qpriv = qp->priv;
	struct tid_rdma_request *req = wqe_to_tid_req(wqe);
	struct tid_rdma_flow *flow = NULL;
	u32 hdwords = 0;
	bool last;
	bool retry = true;
	u32 npkts = rvt_div_round_up_mtu(qp, *len);

	trace_hfi1_tid_req_build_read_req(qp, 0, wqe->wr.opcode, wqe->psn,
					  wqe->lpsn, req);
	 
sync_check:
	if (req->state == TID_REQUEST_SYNC) {
		if (qpriv->pending_tid_r_segs)
			goto done;

		hfi1_kern_clear_hw_flow(req->rcd, qp);
		qpriv->s_flags &= ~HFI1_R_TID_SW_PSN;
		req->state = TID_REQUEST_ACTIVE;
	}

	 
	if (req->flow_idx == req->setup_head) {
		retry = false;
		if (req->state == TID_REQUEST_RESEND) {
			 
			restart_sge(&qp->s_sge, wqe, req->s_next_psn,
				    qp->pmtu);
			req->isge = 0;
			req->state = TID_REQUEST_ACTIVE;
		}

		 
		if ((qpriv->flow_state.psn + npkts) > MAX_TID_FLOW_PSN - 1) {
			req->state = TID_REQUEST_SYNC;
			goto sync_check;
		}

		 
		if (hfi1_kern_setup_hw_flow(qpriv->rcd, qp))
			goto done;

		 
		if (hfi1_kern_exp_rcv_setup(req, &qp->s_sge, &last)) {
			req->state = TID_REQUEST_QUEUED;

			 
			goto done;
		}
	}

	 
	flow = &req->flows[req->flow_idx];
	flow->pkt = 0;
	flow->tid_idx = 0;
	flow->sent = 0;
	if (!retry) {
		 
		flow->flow_state.ib_spsn = req->s_next_psn;
		flow->flow_state.ib_lpsn =
			flow->flow_state.ib_spsn + flow->npkts - 1;
	}

	 
	req->s_next_psn += flow->npkts;

	 
	hdwords = hfi1_build_tid_rdma_read_packet(wqe, ohdr, bth1, bth2, len);
done:
	return hdwords;
}

 
static int tid_rdma_rcv_read_request(struct rvt_qp *qp,
				     struct rvt_ack_entry *e,
				     struct hfi1_packet *packet,
				     struct ib_other_headers *ohdr,
				     u32 bth0, u32 psn, u64 vaddr, u32 len)
{
	struct hfi1_qp_priv *qpriv = qp->priv;
	struct tid_rdma_request *req;
	struct tid_rdma_flow *flow;
	u32 flow_psn, i, tidlen = 0, pktlen, tlen;

	req = ack_to_tid_req(e);

	 
	flow = &req->flows[req->setup_head];

	 
	pktlen = packet->tlen - (packet->hlen + 4);
	if (pktlen > sizeof(flow->tid_entry))
		return 1;
	memcpy(flow->tid_entry, packet->ebuf, pktlen);
	flow->tidcnt = pktlen / sizeof(*flow->tid_entry);

	 
	flow->npkts = rvt_div_round_up_mtu(qp, len);
	for (i = 0; i < flow->tidcnt; i++) {
		trace_hfi1_tid_entry_rcv_read_req(qp, i,
						  flow->tid_entry[i]);
		tlen = EXP_TID_GET(flow->tid_entry[i], LEN);
		if (!tlen)
			return 1;

		 
		tidlen += tlen;
	}
	if (tidlen * PAGE_SIZE < len)
		return 1;

	 
	req->clear_tail = req->setup_head;
	flow->pkt = 0;
	flow->tid_idx = 0;
	flow->tid_offset = 0;
	flow->sent = 0;
	flow->tid_qpn = be32_to_cpu(ohdr->u.tid_rdma.r_req.tid_flow_qp);
	flow->idx = (flow->tid_qpn >> TID_RDMA_DESTQP_FLOW_SHIFT) &
		    TID_RDMA_DESTQP_FLOW_MASK;
	flow_psn = mask_psn(be32_to_cpu(ohdr->u.tid_rdma.r_req.tid_flow_psn));
	flow->flow_state.generation = flow_psn >> HFI1_KDETH_BTH_SEQ_SHIFT;
	flow->flow_state.spsn = flow_psn & HFI1_KDETH_BTH_SEQ_MASK;
	flow->length = len;

	flow->flow_state.lpsn = flow->flow_state.spsn +
		flow->npkts - 1;
	flow->flow_state.ib_spsn = psn;
	flow->flow_state.ib_lpsn = flow->flow_state.ib_spsn + flow->npkts - 1;

	trace_hfi1_tid_flow_rcv_read_req(qp, req->setup_head, flow);
	 
	req->flow_idx = req->setup_head;

	 
	req->setup_head = (req->setup_head + 1) & (MAX_FLOWS - 1);

	 
	e->opcode = (bth0 >> 24) & 0xff;
	e->psn = psn;
	e->lpsn = psn + flow->npkts - 1;
	e->sent = 0;

	req->n_flows = qpriv->tid_rdma.local.max_read;
	req->state = TID_REQUEST_ACTIVE;
	req->cur_seg = 0;
	req->comp_seg = 0;
	req->ack_seg = 0;
	req->isge = 0;
	req->seg_len = qpriv->tid_rdma.local.max_len;
	req->total_len = len;
	req->total_segs = 1;
	req->r_flow_psn = e->psn;

	trace_hfi1_tid_req_rcv_read_req(qp, 0, e->opcode, e->psn, e->lpsn,
					req);
	return 0;
}

static int tid_rdma_rcv_error(struct hfi1_packet *packet,
			      struct ib_other_headers *ohdr,
			      struct rvt_qp *qp, u32 psn, int diff)
{
	struct hfi1_ibport *ibp = to_iport(qp->ibqp.device, qp->port_num);
	struct hfi1_ctxtdata *rcd = ((struct hfi1_qp_priv *)qp->priv)->rcd;
	struct hfi1_ibdev *dev = to_idev(qp->ibqp.device);
	struct hfi1_qp_priv *qpriv = qp->priv;
	struct rvt_ack_entry *e;
	struct tid_rdma_request *req;
	unsigned long flags;
	u8 prev;
	bool old_req;

	trace_hfi1_rsp_tid_rcv_error(qp, psn);
	trace_hfi1_tid_rdma_rcv_err(qp, 0, psn, diff);
	if (diff > 0) {
		 
		if (!qp->r_nak_state) {
			ibp->rvp.n_rc_seqnak++;
			qp->r_nak_state = IB_NAK_PSN_ERROR;
			qp->r_ack_psn = qp->r_psn;
			rc_defered_ack(rcd, qp);
		}
		goto done;
	}

	ibp->rvp.n_rc_dupreq++;

	spin_lock_irqsave(&qp->s_lock, flags);
	e = find_prev_entry(qp, psn, &prev, NULL, &old_req);
	if (!e || (e->opcode != TID_OP(READ_REQ) &&
		   e->opcode != TID_OP(WRITE_REQ)))
		goto unlock;

	req = ack_to_tid_req(e);
	req->r_flow_psn = psn;
	trace_hfi1_tid_req_rcv_err(qp, 0, e->opcode, e->psn, e->lpsn, req);
	if (e->opcode == TID_OP(READ_REQ)) {
		struct ib_reth *reth;
		u32 len;
		u32 rkey;
		u64 vaddr;
		int ok;
		u32 bth0;

		reth = &ohdr->u.tid_rdma.r_req.reth;
		 
		len = be32_to_cpu(reth->length);
		if (psn != e->psn || len != req->total_len)
			goto unlock;

		release_rdma_sge_mr(e);

		rkey = be32_to_cpu(reth->rkey);
		vaddr = get_ib_reth_vaddr(reth);

		qp->r_len = len;
		ok = rvt_rkey_ok(qp, &e->rdma_sge, len, vaddr, rkey,
				 IB_ACCESS_REMOTE_READ);
		if (unlikely(!ok))
			goto unlock;

		 
		bth0 = be32_to_cpu(ohdr->bth[0]);
		if (tid_rdma_rcv_read_request(qp, e, packet, ohdr, bth0, psn,
					      vaddr, len))
			goto unlock;

		 
		if (old_req)
			goto unlock;
	} else {
		struct flow_state *fstate;
		bool schedule = false;
		u8 i;

		if (req->state == TID_REQUEST_RESEND) {
			req->state = TID_REQUEST_RESEND_ACTIVE;
		} else if (req->state == TID_REQUEST_INIT_RESEND) {
			req->state = TID_REQUEST_INIT;
			schedule = true;
		}

		 
		if (old_req || req->state == TID_REQUEST_INIT ||
		    (req->state == TID_REQUEST_SYNC && !req->cur_seg)) {
			for (i = prev + 1; ; i++) {
				if (i > rvt_size_atomic(&dev->rdi))
					i = 0;
				if (i == qp->r_head_ack_queue)
					break;
				e = &qp->s_ack_queue[i];
				req = ack_to_tid_req(e);
				if (e->opcode == TID_OP(WRITE_REQ) &&
				    req->state == TID_REQUEST_INIT)
					req->state = TID_REQUEST_INIT_RESEND;
			}
			 
			if (!schedule)
				goto unlock;
		}

		 
		if (req->clear_tail == req->setup_head)
			goto schedule;
		 
		if (CIRC_CNT(req->flow_idx, req->clear_tail, MAX_FLOWS)) {
			fstate = &req->flows[req->clear_tail].flow_state;
			qpriv->pending_tid_w_segs -=
				CIRC_CNT(req->flow_idx, req->clear_tail,
					 MAX_FLOWS);
			req->flow_idx =
				CIRC_ADD(req->clear_tail,
					 delta_psn(psn, fstate->resp_ib_psn),
					 MAX_FLOWS);
			qpriv->pending_tid_w_segs +=
				delta_psn(psn, fstate->resp_ib_psn);
			 
			if (CIRC_CNT(req->setup_head, req->flow_idx,
				     MAX_FLOWS)) {
				req->cur_seg = delta_psn(psn, e->psn);
				req->state = TID_REQUEST_RESEND_ACTIVE;
			}
		}

		for (i = prev + 1; ; i++) {
			 
			if (i > rvt_size_atomic(&dev->rdi))
				i = 0;
			if (i == qp->r_head_ack_queue)
				break;
			e = &qp->s_ack_queue[i];
			req = ack_to_tid_req(e);
			trace_hfi1_tid_req_rcv_err(qp, 0, e->opcode, e->psn,
						   e->lpsn, req);
			if (e->opcode != TID_OP(WRITE_REQ) ||
			    req->cur_seg == req->comp_seg ||
			    req->state == TID_REQUEST_INIT ||
			    req->state == TID_REQUEST_INIT_RESEND) {
				if (req->state == TID_REQUEST_INIT)
					req->state = TID_REQUEST_INIT_RESEND;
				continue;
			}
			qpriv->pending_tid_w_segs -=
				CIRC_CNT(req->flow_idx,
					 req->clear_tail,
					 MAX_FLOWS);
			req->flow_idx = req->clear_tail;
			req->state = TID_REQUEST_RESEND;
			req->cur_seg = req->comp_seg;
		}
		qpriv->s_flags &= ~HFI1_R_TID_WAIT_INTERLCK;
	}
	 
	if (qp->s_acked_ack_queue == qp->s_tail_ack_queue)
		qp->s_acked_ack_queue = prev;
	qp->s_tail_ack_queue = prev;
	 
	qp->s_ack_state = OP(ACKNOWLEDGE);
schedule:
	 
	if (qpriv->rnr_nak_state) {
		qp->s_nak_state = 0;
		qpriv->rnr_nak_state = TID_RNR_NAK_INIT;
		qp->r_psn = e->lpsn + 1;
		hfi1_tid_write_alloc_resources(qp, true);
	}

	qp->r_state = e->opcode;
	qp->r_nak_state = 0;
	qp->s_flags |= RVT_S_RESP_PENDING;
	hfi1_schedule_send(qp);
unlock:
	spin_unlock_irqrestore(&qp->s_lock, flags);
done:
	return 1;
}

void hfi1_rc_rcv_tid_rdma_read_req(struct hfi1_packet *packet)
{
	 

	 
	struct hfi1_ctxtdata *rcd = packet->rcd;
	struct rvt_qp *qp = packet->qp;
	struct hfi1_ibport *ibp = to_iport(qp->ibqp.device, qp->port_num);
	struct ib_other_headers *ohdr = packet->ohdr;
	struct rvt_ack_entry *e;
	unsigned long flags;
	struct ib_reth *reth;
	struct hfi1_qp_priv *qpriv = qp->priv;
	u32 bth0, psn, len, rkey;
	bool fecn;
	u8 next;
	u64 vaddr;
	int diff;
	u8 nack_state = IB_NAK_INVALID_REQUEST;

	bth0 = be32_to_cpu(ohdr->bth[0]);
	if (hfi1_ruc_check_hdr(ibp, packet))
		return;

	fecn = process_ecn(qp, packet);
	psn = mask_psn(be32_to_cpu(ohdr->bth[2]));
	trace_hfi1_rsp_rcv_tid_read_req(qp, psn);

	if (qp->state == IB_QPS_RTR && !(qp->r_flags & RVT_R_COMM_EST))
		rvt_comm_est(qp);

	if (unlikely(!(qp->qp_access_flags & IB_ACCESS_REMOTE_READ)))
		goto nack_inv;

	reth = &ohdr->u.tid_rdma.r_req.reth;
	vaddr = be64_to_cpu(reth->vaddr);
	len = be32_to_cpu(reth->length);
	 
	if (!len || len & ~PAGE_MASK || len > qpriv->tid_rdma.local.max_len)
		goto nack_inv;

	diff = delta_psn(psn, qp->r_psn);
	if (unlikely(diff)) {
		tid_rdma_rcv_err(packet, ohdr, qp, psn, diff, fecn);
		return;
	}

	 
	next = qp->r_head_ack_queue + 1;
	if (next > rvt_size_atomic(ib_to_rvt(qp->ibqp.device)))
		next = 0;
	spin_lock_irqsave(&qp->s_lock, flags);
	if (unlikely(next == qp->s_tail_ack_queue)) {
		if (!qp->s_ack_queue[next].sent) {
			nack_state = IB_NAK_REMOTE_OPERATIONAL_ERROR;
			goto nack_inv_unlock;
		}
		update_ack_queue(qp, next);
	}
	e = &qp->s_ack_queue[qp->r_head_ack_queue];
	release_rdma_sge_mr(e);

	rkey = be32_to_cpu(reth->rkey);
	qp->r_len = len;

	if (unlikely(!rvt_rkey_ok(qp, &e->rdma_sge, qp->r_len, vaddr,
				  rkey, IB_ACCESS_REMOTE_READ)))
		goto nack_acc;

	 
	if (tid_rdma_rcv_read_request(qp, e, packet, ohdr, bth0, psn, vaddr,
				      len))
		goto nack_inv_unlock;

	qp->r_state = e->opcode;
	qp->r_nak_state = 0;
	 
	qp->r_msn++;
	qp->r_psn += e->lpsn - e->psn + 1;

	qp->r_head_ack_queue = next;

	 
	qpriv->r_tid_alloc = qp->r_head_ack_queue;

	 
	qp->s_flags |= RVT_S_RESP_PENDING;
	if (fecn)
		qp->s_flags |= RVT_S_ECN;
	hfi1_schedule_send(qp);

	spin_unlock_irqrestore(&qp->s_lock, flags);
	return;

nack_inv_unlock:
	spin_unlock_irqrestore(&qp->s_lock, flags);
nack_inv:
	rvt_rc_error(qp, IB_WC_LOC_QP_OP_ERR);
	qp->r_nak_state = nack_state;
	qp->r_ack_psn = qp->r_psn;
	 
	rc_defered_ack(rcd, qp);
	return;
nack_acc:
	spin_unlock_irqrestore(&qp->s_lock, flags);
	rvt_rc_error(qp, IB_WC_LOC_PROT_ERR);
	qp->r_nak_state = IB_NAK_REMOTE_ACCESS_ERROR;
	qp->r_ack_psn = qp->r_psn;
}

u32 hfi1_build_tid_rdma_read_resp(struct rvt_qp *qp, struct rvt_ack_entry *e,
				  struct ib_other_headers *ohdr, u32 *bth0,
				  u32 *bth1, u32 *bth2, u32 *len, bool *last)
{
	struct hfi1_ack_priv *epriv = e->priv;
	struct tid_rdma_request *req = &epriv->tid_req;
	struct hfi1_qp_priv *qpriv = qp->priv;
	struct tid_rdma_flow *flow = &req->flows[req->clear_tail];
	u32 tidentry = flow->tid_entry[flow->tid_idx];
	u32 tidlen = EXP_TID_GET(tidentry, LEN) << PAGE_SHIFT;
	struct tid_rdma_read_resp *resp = &ohdr->u.tid_rdma.r_rsp;
	u32 next_offset, om = KDETH_OM_LARGE;
	bool last_pkt;
	u32 hdwords = 0;
	struct tid_rdma_params *remote;

	*len = min_t(u32, qp->pmtu, tidlen - flow->tid_offset);
	flow->sent += *len;
	next_offset = flow->tid_offset + *len;
	last_pkt = (flow->sent >= flow->length);

	trace_hfi1_tid_entry_build_read_resp(qp, flow->tid_idx, tidentry);
	trace_hfi1_tid_flow_build_read_resp(qp, req->clear_tail, flow);

	rcu_read_lock();
	remote = rcu_dereference(qpriv->tid_rdma.remote);
	if (!remote) {
		rcu_read_unlock();
		goto done;
	}
	KDETH_RESET(resp->kdeth0, KVER, 0x1);
	KDETH_SET(resp->kdeth0, SH, !last_pkt);
	KDETH_SET(resp->kdeth0, INTR, !!(!last_pkt && remote->urg));
	KDETH_SET(resp->kdeth0, TIDCTRL, EXP_TID_GET(tidentry, CTRL));
	KDETH_SET(resp->kdeth0, TID, EXP_TID_GET(tidentry, IDX));
	KDETH_SET(resp->kdeth0, OM, om == KDETH_OM_LARGE);
	KDETH_SET(resp->kdeth0, OFFSET, flow->tid_offset / om);
	KDETH_RESET(resp->kdeth1, JKEY, remote->jkey);
	resp->verbs_qp = cpu_to_be32(qp->remote_qpn);
	rcu_read_unlock();

	resp->aeth = rvt_compute_aeth(qp);
	resp->verbs_psn = cpu_to_be32(mask_psn(flow->flow_state.ib_spsn +
					       flow->pkt));

	*bth0 = TID_OP(READ_RESP) << 24;
	*bth1 = flow->tid_qpn;
	*bth2 = mask_psn(((flow->flow_state.spsn + flow->pkt++) &
			  HFI1_KDETH_BTH_SEQ_MASK) |
			 (flow->flow_state.generation <<
			  HFI1_KDETH_BTH_SEQ_SHIFT));
	*last = last_pkt;
	if (last_pkt)
		 
		req->clear_tail = (req->clear_tail + 1) &
				  (MAX_FLOWS - 1);

	if (next_offset >= tidlen) {
		flow->tid_offset = 0;
		flow->tid_idx++;
	} else {
		flow->tid_offset = next_offset;
	}

	hdwords = sizeof(ohdr->u.tid_rdma.r_rsp) / sizeof(u32);

done:
	return hdwords;
}

static inline struct tid_rdma_request *
find_tid_request(struct rvt_qp *qp, u32 psn, enum ib_wr_opcode opcode)
	__must_hold(&qp->s_lock)
{
	struct rvt_swqe *wqe;
	struct tid_rdma_request *req = NULL;
	u32 i, end;

	end = qp->s_cur + 1;
	if (end == qp->s_size)
		end = 0;
	for (i = qp->s_acked; i != end;) {
		wqe = rvt_get_swqe_ptr(qp, i);
		if (cmp_psn(psn, wqe->psn) >= 0 &&
		    cmp_psn(psn, wqe->lpsn) <= 0) {
			if (wqe->wr.opcode == opcode)
				req = wqe_to_tid_req(wqe);
			break;
		}
		if (++i == qp->s_size)
			i = 0;
	}

	return req;
}

void hfi1_rc_rcv_tid_rdma_read_resp(struct hfi1_packet *packet)
{
	 

	 
	struct ib_other_headers *ohdr = packet->ohdr;
	struct rvt_qp *qp = packet->qp;
	struct hfi1_qp_priv *priv = qp->priv;
	struct hfi1_ctxtdata *rcd = packet->rcd;
	struct tid_rdma_request *req;
	struct tid_rdma_flow *flow;
	u32 opcode, aeth;
	bool fecn;
	unsigned long flags;
	u32 kpsn, ipsn;

	trace_hfi1_sender_rcv_tid_read_resp(qp);
	fecn = process_ecn(qp, packet);
	kpsn = mask_psn(be32_to_cpu(ohdr->bth[2]));
	aeth = be32_to_cpu(ohdr->u.tid_rdma.r_rsp.aeth);
	opcode = (be32_to_cpu(ohdr->bth[0]) >> 24) & 0xff;

	spin_lock_irqsave(&qp->s_lock, flags);
	ipsn = mask_psn(be32_to_cpu(ohdr->u.tid_rdma.r_rsp.verbs_psn));
	req = find_tid_request(qp, ipsn, IB_WR_TID_RDMA_READ);
	if (unlikely(!req))
		goto ack_op_err;

	flow = &req->flows[req->clear_tail];
	 
	if (cmp_psn(ipsn, flow->flow_state.ib_lpsn)) {
		update_r_next_psn_fecn(packet, priv, rcd, flow, fecn);

		if (cmp_psn(kpsn, flow->flow_state.r_next_psn))
			goto ack_done;
		flow->flow_state.r_next_psn = mask_psn(kpsn + 1);
		 
		if (fecn && packet->etype == RHF_RCV_TYPE_EAGER) {
			struct rvt_sge_state ss;
			u32 len;
			u32 tlen = packet->tlen;
			u16 hdrsize = packet->hlen;
			u8 pad = packet->pad;
			u8 extra_bytes = pad + packet->extra_byte +
				(SIZE_OF_CRC << 2);
			u32 pmtu = qp->pmtu;

			if (unlikely(tlen != (hdrsize + pmtu + extra_bytes)))
				goto ack_op_err;
			len = restart_sge(&ss, req->e.swqe, ipsn, pmtu);
			if (unlikely(len < pmtu))
				goto ack_op_err;
			rvt_copy_sge(qp, &ss, packet->payload, pmtu, false,
				     false);
			 
			priv->s_flags |= HFI1_R_TID_SW_PSN;
		}

		goto ack_done;
	}
	flow->flow_state.r_next_psn = mask_psn(kpsn + 1);
	req->ack_pending--;
	priv->pending_tid_r_segs--;
	qp->s_num_rd_atomic--;
	if ((qp->s_flags & RVT_S_WAIT_FENCE) &&
	    !qp->s_num_rd_atomic) {
		qp->s_flags &= ~(RVT_S_WAIT_FENCE |
				 RVT_S_WAIT_ACK);
		hfi1_schedule_send(qp);
	}
	if (qp->s_flags & RVT_S_WAIT_RDMAR) {
		qp->s_flags &= ~(RVT_S_WAIT_RDMAR | RVT_S_WAIT_ACK);
		hfi1_schedule_send(qp);
	}

	trace_hfi1_ack(qp, ipsn);
	trace_hfi1_tid_req_rcv_read_resp(qp, 0, req->e.swqe->wr.opcode,
					 req->e.swqe->psn, req->e.swqe->lpsn,
					 req);
	trace_hfi1_tid_flow_rcv_read_resp(qp, req->clear_tail, flow);

	 
	hfi1_kern_exp_rcv_clear(req);

	if (!do_rc_ack(qp, aeth, ipsn, opcode, 0, rcd))
		goto ack_done;

	 
	if (++req->comp_seg >= req->total_segs) {
		priv->tid_r_comp++;
		req->state = TID_REQUEST_COMPLETE;
	}

	 
	if ((req->state == TID_REQUEST_SYNC &&
	     req->comp_seg == req->cur_seg) ||
	    priv->tid_r_comp == priv->tid_r_reqs) {
		hfi1_kern_clear_hw_flow(priv->rcd, qp);
		priv->s_flags &= ~HFI1_R_TID_SW_PSN;
		if (req->state == TID_REQUEST_SYNC)
			req->state = TID_REQUEST_ACTIVE;
	}

	hfi1_schedule_send(qp);
	goto ack_done;

ack_op_err:
	 
	if (qp->s_last == qp->s_acked)
		rvt_error_qp(qp, IB_WC_WR_FLUSH_ERR);

ack_done:
	spin_unlock_irqrestore(&qp->s_lock, flags);
}

void hfi1_kern_read_tid_flow_free(struct rvt_qp *qp)
	__must_hold(&qp->s_lock)
{
	u32 n = qp->s_acked;
	struct rvt_swqe *wqe;
	struct tid_rdma_request *req;
	struct hfi1_qp_priv *priv = qp->priv;

	lockdep_assert_held(&qp->s_lock);
	 
	while (n != qp->s_tail) {
		wqe = rvt_get_swqe_ptr(qp, n);
		if (wqe->wr.opcode == IB_WR_TID_RDMA_READ) {
			req = wqe_to_tid_req(wqe);
			hfi1_kern_exp_rcv_clear_all(req);
		}

		if (++n == qp->s_size)
			n = 0;
	}
	 
	hfi1_kern_clear_hw_flow(priv->rcd, qp);
}

static bool tid_rdma_tid_err(struct hfi1_packet *packet, u8 rcv_type)
{
	struct rvt_qp *qp = packet->qp;

	if (rcv_type >= RHF_RCV_TYPE_IB)
		goto done;

	spin_lock(&qp->s_lock);

	 
	if (rcv_type == RHF_RCV_TYPE_EAGER) {
		hfi1_restart_rc(qp, qp->s_last_psn + 1, 1);
		hfi1_schedule_send(qp);
	}

	 
	spin_unlock(&qp->s_lock);
done:
	return true;
}

static void restart_tid_rdma_read_req(struct hfi1_ctxtdata *rcd,
				      struct rvt_qp *qp, struct rvt_swqe *wqe)
{
	struct tid_rdma_request *req;
	struct tid_rdma_flow *flow;

	 
	qp->r_flags |= RVT_R_RDMAR_SEQ;
	req = wqe_to_tid_req(wqe);
	flow = &req->flows[req->clear_tail];
	hfi1_restart_rc(qp, flow->flow_state.ib_spsn, 0);
	if (list_empty(&qp->rspwait)) {
		qp->r_flags |= RVT_R_RSP_SEND;
		rvt_get_qp(qp);
		list_add_tail(&qp->rspwait, &rcd->qp_wait_list);
	}
}

 
static bool handle_read_kdeth_eflags(struct hfi1_ctxtdata *rcd,
				     struct hfi1_packet *packet, u8 rcv_type,
				     u8 rte, u32 psn, u32 ibpsn)
	__must_hold(&packet->qp->r_lock) __must_hold(RCU)
{
	struct hfi1_pportdata *ppd = rcd->ppd;
	struct hfi1_devdata *dd = ppd->dd;
	struct hfi1_ibport *ibp;
	struct rvt_swqe *wqe;
	struct tid_rdma_request *req;
	struct tid_rdma_flow *flow;
	u32 ack_psn;
	struct rvt_qp *qp = packet->qp;
	struct hfi1_qp_priv *priv = qp->priv;
	bool ret = true;
	int diff = 0;
	u32 fpsn;

	lockdep_assert_held(&qp->r_lock);
	trace_hfi1_rsp_read_kdeth_eflags(qp, ibpsn);
	trace_hfi1_sender_read_kdeth_eflags(qp);
	trace_hfi1_tid_read_sender_kdeth_eflags(qp, 0);
	spin_lock(&qp->s_lock);
	 
	if (cmp_psn(ibpsn, qp->s_last_psn) < 0 ||
	    cmp_psn(ibpsn, qp->s_psn) > 0)
		goto s_unlock;

	 
	ack_psn = ibpsn - 1;
	wqe = rvt_get_swqe_ptr(qp, qp->s_acked);
	ibp = to_iport(qp->ibqp.device, qp->port_num);

	 
	while ((int)delta_psn(ack_psn, wqe->lpsn) >= 0) {
		 
		if (wqe->wr.opcode == IB_WR_RDMA_READ ||
		    wqe->wr.opcode == IB_WR_TID_RDMA_READ ||
		    wqe->wr.opcode == IB_WR_ATOMIC_CMP_AND_SWP ||
		    wqe->wr.opcode == IB_WR_ATOMIC_FETCH_AND_ADD) {
			 
			if (!(qp->r_flags & RVT_R_RDMAR_SEQ)) {
				qp->r_flags |= RVT_R_RDMAR_SEQ;
				if (wqe->wr.opcode == IB_WR_TID_RDMA_READ) {
					restart_tid_rdma_read_req(rcd, qp,
								  wqe);
				} else {
					hfi1_restart_rc(qp, qp->s_last_psn + 1,
							0);
					if (list_empty(&qp->rspwait)) {
						qp->r_flags |= RVT_R_RSP_SEND;
						rvt_get_qp(qp);
						list_add_tail( 
						   &qp->rspwait,
						   &rcd->qp_wait_list);
					}
				}
			}
			 
			break;
		}

		wqe = do_rc_completion(qp, wqe, ibp);
		if (qp->s_acked == qp->s_tail)
			goto s_unlock;
	}

	if (qp->s_acked == qp->s_tail)
		goto s_unlock;

	 
	if (wqe->wr.opcode != IB_WR_TID_RDMA_READ)
		goto s_unlock;

	req = wqe_to_tid_req(wqe);
	trace_hfi1_tid_req_read_kdeth_eflags(qp, 0, wqe->wr.opcode, wqe->psn,
					     wqe->lpsn, req);
	switch (rcv_type) {
	case RHF_RCV_TYPE_EXPECTED:
		switch (rte) {
		case RHF_RTE_EXPECTED_FLOW_SEQ_ERR:
			 
			flow = &req->flows[req->clear_tail];
			trace_hfi1_tid_flow_read_kdeth_eflags(qp,
							      req->clear_tail,
							      flow);
			if (priv->s_flags & HFI1_R_TID_SW_PSN) {
				diff = cmp_psn(psn,
					       flow->flow_state.r_next_psn);
				if (diff > 0) {
					 
					goto s_unlock;
				} else if (diff < 0) {
					 
					if (qp->r_flags & RVT_R_RDMAR_SEQ)
						qp->r_flags &=
							~RVT_R_RDMAR_SEQ;

					 
					goto s_unlock;
				}

				 
				fpsn = full_flow_psn(flow,
						     flow->flow_state.lpsn);
				if (cmp_psn(fpsn, psn) == 0) {
					ret = false;
					if (qp->r_flags & RVT_R_RDMAR_SEQ)
						qp->r_flags &=
							~RVT_R_RDMAR_SEQ;
				}
				flow->flow_state.r_next_psn =
					mask_psn(psn + 1);
			} else {
				u32 last_psn;

				last_psn = read_r_next_psn(dd, rcd->ctxt,
							   flow->idx);
				flow->flow_state.r_next_psn = last_psn;
				priv->s_flags |= HFI1_R_TID_SW_PSN;
				 
				if (!(qp->r_flags & RVT_R_RDMAR_SEQ))
					restart_tid_rdma_read_req(rcd, qp,
								  wqe);
			}

			break;

		case RHF_RTE_EXPECTED_FLOW_GEN_ERR:
			 
			break;

		default:
			break;
		}
		break;

	case RHF_RCV_TYPE_ERROR:
		switch (rte) {
		case RHF_RTE_ERROR_OP_CODE_ERR:
		case RHF_RTE_ERROR_KHDR_MIN_LEN_ERR:
		case RHF_RTE_ERROR_KHDR_HCRC_ERR:
		case RHF_RTE_ERROR_KHDR_KVER_ERR:
		case RHF_RTE_ERROR_CONTEXT_ERR:
		case RHF_RTE_ERROR_KHDR_TID_ERR:
		default:
			break;
		}
		break;
	default:
		break;
	}
s_unlock:
	spin_unlock(&qp->s_lock);
	return ret;
}

bool hfi1_handle_kdeth_eflags(struct hfi1_ctxtdata *rcd,
			      struct hfi1_pportdata *ppd,
			      struct hfi1_packet *packet)
{
	struct hfi1_ibport *ibp = &ppd->ibport_data;
	struct hfi1_devdata *dd = ppd->dd;
	struct rvt_dev_info *rdi = &dd->verbs_dev.rdi;
	u8 rcv_type = rhf_rcv_type(packet->rhf);
	u8 rte = rhf_rcv_type_err(packet->rhf);
	struct ib_header *hdr = packet->hdr;
	struct ib_other_headers *ohdr = NULL;
	int lnh = be16_to_cpu(hdr->lrh[0]) & 3;
	u16 lid  = be16_to_cpu(hdr->lrh[1]);
	u8 opcode;
	u32 qp_num, psn, ibpsn;
	struct rvt_qp *qp;
	struct hfi1_qp_priv *qpriv;
	unsigned long flags;
	bool ret = true;
	struct rvt_ack_entry *e;
	struct tid_rdma_request *req;
	struct tid_rdma_flow *flow;
	int diff = 0;

	trace_hfi1_msg_handle_kdeth_eflags(NULL, "Kdeth error: rhf ",
					   packet->rhf);
	if (packet->rhf & RHF_ICRC_ERR)
		return ret;

	packet->ohdr = &hdr->u.oth;
	ohdr = packet->ohdr;
	trace_input_ibhdr(rcd->dd, packet, !!(rhf_dc_info(packet->rhf)));

	 
	qp_num = be32_to_cpu(ohdr->u.tid_rdma.r_rsp.verbs_qp) &
		RVT_QPN_MASK;
	if (lid >= be16_to_cpu(IB_MULTICAST_LID_BASE))
		goto drop;

	psn = mask_psn(be32_to_cpu(ohdr->bth[2]));
	opcode = (be32_to_cpu(ohdr->bth[0]) >> 24) & 0xff;

	rcu_read_lock();
	qp = rvt_lookup_qpn(rdi, &ibp->rvp, qp_num);
	if (!qp)
		goto rcu_unlock;

	packet->qp = qp;

	 
	spin_lock_irqsave(&qp->r_lock, flags);
	if (!(ib_rvt_state_ops[qp->state] & RVT_PROCESS_RECV_OK)) {
		ibp->rvp.n_pkt_drops++;
		goto r_unlock;
	}

	if (packet->rhf & RHF_TID_ERR) {
		 
		u32 tlen = rhf_pkt_len(packet->rhf);  

		 
		if (tlen < 24)
			goto r_unlock;

		 
		if (lnh == HFI1_LRH_GRH)
			goto r_unlock;

		if (tid_rdma_tid_err(packet, rcv_type))
			goto r_unlock;
	}

	 
	if (opcode == TID_OP(READ_RESP)) {
		ibpsn = be32_to_cpu(ohdr->u.tid_rdma.r_rsp.verbs_psn);
		ibpsn = mask_psn(ibpsn);
		ret = handle_read_kdeth_eflags(rcd, packet, rcv_type, rte, psn,
					       ibpsn);
		goto r_unlock;
	}

	 
	spin_lock(&qp->s_lock);
	qpriv = qp->priv;
	if (qpriv->r_tid_tail == HFI1_QP_WQE_INVALID ||
	    qpriv->r_tid_tail == qpriv->r_tid_head)
		goto unlock;
	e = &qp->s_ack_queue[qpriv->r_tid_tail];
	if (e->opcode != TID_OP(WRITE_REQ))
		goto unlock;
	req = ack_to_tid_req(e);
	if (req->comp_seg == req->cur_seg)
		goto unlock;
	flow = &req->flows[req->clear_tail];
	trace_hfi1_eflags_err_write(qp, rcv_type, rte, psn);
	trace_hfi1_rsp_handle_kdeth_eflags(qp, psn);
	trace_hfi1_tid_write_rsp_handle_kdeth_eflags(qp);
	trace_hfi1_tid_req_handle_kdeth_eflags(qp, 0, e->opcode, e->psn,
					       e->lpsn, req);
	trace_hfi1_tid_flow_handle_kdeth_eflags(qp, req->clear_tail, flow);

	switch (rcv_type) {
	case RHF_RCV_TYPE_EXPECTED:
		switch (rte) {
		case RHF_RTE_EXPECTED_FLOW_SEQ_ERR:
			if (!(qpriv->s_flags & HFI1_R_TID_SW_PSN)) {
				qpriv->s_flags |= HFI1_R_TID_SW_PSN;
				flow->flow_state.r_next_psn =
					read_r_next_psn(dd, rcd->ctxt,
							flow->idx);
				qpriv->r_next_psn_kdeth =
					flow->flow_state.r_next_psn;
				goto nak_psn;
			} else {
				 
				diff = cmp_psn(psn,
					       flow->flow_state.r_next_psn);
				if (diff > 0)
					goto nak_psn;
				else if (diff < 0)
					break;

				qpriv->s_nak_state = 0;
				 
				if (psn == full_flow_psn(flow,
							 flow->flow_state.lpsn))
					ret = false;
				flow->flow_state.r_next_psn =
					mask_psn(psn + 1);
				qpriv->r_next_psn_kdeth =
					flow->flow_state.r_next_psn;
			}
			break;

		case RHF_RTE_EXPECTED_FLOW_GEN_ERR:
			goto nak_psn;

		default:
			break;
		}
		break;

	case RHF_RCV_TYPE_ERROR:
		switch (rte) {
		case RHF_RTE_ERROR_OP_CODE_ERR:
		case RHF_RTE_ERROR_KHDR_MIN_LEN_ERR:
		case RHF_RTE_ERROR_KHDR_HCRC_ERR:
		case RHF_RTE_ERROR_KHDR_KVER_ERR:
		case RHF_RTE_ERROR_CONTEXT_ERR:
		case RHF_RTE_ERROR_KHDR_TID_ERR:
		default:
			break;
		}
		break;
	default:
		break;
	}

unlock:
	spin_unlock(&qp->s_lock);
r_unlock:
	spin_unlock_irqrestore(&qp->r_lock, flags);
rcu_unlock:
	rcu_read_unlock();
drop:
	return ret;
nak_psn:
	ibp->rvp.n_rc_seqnak++;
	if (!qpriv->s_nak_state) {
		qpriv->s_nak_state = IB_NAK_PSN_ERROR;
		 
		qpriv->s_nak_psn = mask_psn(flow->flow_state.r_next_psn);
		tid_rdma_trigger_ack(qp);
	}
	goto unlock;
}

 
void hfi1_tid_rdma_restart_req(struct rvt_qp *qp, struct rvt_swqe *wqe,
			       u32 *bth2)
{
	struct tid_rdma_request *req = wqe_to_tid_req(wqe);
	struct tid_rdma_flow *flow;
	struct hfi1_qp_priv *qpriv = qp->priv;
	int diff, delta_pkts;
	u32 tididx = 0, i;
	u16 fidx;

	if (wqe->wr.opcode == IB_WR_TID_RDMA_READ) {
		*bth2 = mask_psn(qp->s_psn);
		flow = find_flow_ib(req, *bth2, &fidx);
		if (!flow) {
			trace_hfi1_msg_tid_restart_req( 
			   qp, "!!!!!! Could not find flow to restart: bth2 ",
			   (u64)*bth2);
			trace_hfi1_tid_req_restart_req(qp, 0, wqe->wr.opcode,
						       wqe->psn, wqe->lpsn,
						       req);
			return;
		}
	} else {
		fidx = req->acked_tail;
		flow = &req->flows[fidx];
		*bth2 = mask_psn(req->r_ack_psn);
	}

	if (wqe->wr.opcode == IB_WR_TID_RDMA_READ)
		delta_pkts = delta_psn(*bth2, flow->flow_state.ib_spsn);
	else
		delta_pkts = delta_psn(*bth2,
				       full_flow_psn(flow,
						     flow->flow_state.spsn));

	trace_hfi1_tid_flow_restart_req(qp, fidx, flow);
	diff = delta_pkts + flow->resync_npkts;

	flow->sent = 0;
	flow->pkt = 0;
	flow->tid_idx = 0;
	flow->tid_offset = 0;
	if (diff) {
		for (tididx = 0; tididx < flow->tidcnt; tididx++) {
			u32 tidentry = flow->tid_entry[tididx], tidlen,
				tidnpkts, npkts;

			flow->tid_offset = 0;
			tidlen = EXP_TID_GET(tidentry, LEN) * PAGE_SIZE;
			tidnpkts = rvt_div_round_up_mtu(qp, tidlen);
			npkts = min_t(u32, diff, tidnpkts);
			flow->pkt += npkts;
			flow->sent += (npkts == tidnpkts ? tidlen :
				       npkts * qp->pmtu);
			flow->tid_offset += npkts * qp->pmtu;
			diff -= npkts;
			if (!diff)
				break;
		}
	}
	if (wqe->wr.opcode == IB_WR_TID_RDMA_WRITE) {
		rvt_skip_sge(&qpriv->tid_ss, (req->cur_seg * req->seg_len) +
			     flow->sent, 0);
		 
		flow->pkt -= flow->resync_npkts;
	}

	if (flow->tid_offset ==
	    EXP_TID_GET(flow->tid_entry[tididx], LEN) * PAGE_SIZE) {
		tididx++;
		flow->tid_offset = 0;
	}
	flow->tid_idx = tididx;
	if (wqe->wr.opcode == IB_WR_TID_RDMA_READ)
		 
		req->flow_idx = fidx;
	else
		req->clear_tail = fidx;

	trace_hfi1_tid_flow_restart_req(qp, fidx, flow);
	trace_hfi1_tid_req_restart_req(qp, 0, wqe->wr.opcode, wqe->psn,
				       wqe->lpsn, req);
	req->state = TID_REQUEST_ACTIVE;
	if (wqe->wr.opcode == IB_WR_TID_RDMA_WRITE) {
		 
		fidx = CIRC_NEXT(fidx, MAX_FLOWS);
		i = qpriv->s_tid_tail;
		do {
			for (; CIRC_CNT(req->setup_head, fidx, MAX_FLOWS);
			      fidx = CIRC_NEXT(fidx, MAX_FLOWS)) {
				req->flows[fidx].sent = 0;
				req->flows[fidx].pkt = 0;
				req->flows[fidx].tid_idx = 0;
				req->flows[fidx].tid_offset = 0;
				req->flows[fidx].resync_npkts = 0;
			}
			if (i == qpriv->s_tid_cur)
				break;
			do {
				i = (++i == qp->s_size ? 0 : i);
				wqe = rvt_get_swqe_ptr(qp, i);
			} while (wqe->wr.opcode != IB_WR_TID_RDMA_WRITE);
			req = wqe_to_tid_req(wqe);
			req->cur_seg = req->ack_seg;
			fidx = req->acked_tail;
			 
			req->clear_tail = fidx;
		} while (1);
	}
}

void hfi1_qp_kern_exp_rcv_clear_all(struct rvt_qp *qp)
{
	int i, ret;
	struct hfi1_qp_priv *qpriv = qp->priv;
	struct tid_flow_state *fs;

	if (qp->ibqp.qp_type != IB_QPT_RC || !HFI1_CAP_IS_KSET(TID_RDMA))
		return;

	 
	fs = &qpriv->flow_state;
	if (fs->index != RXE_NUM_TID_FLOWS)
		hfi1_kern_clear_hw_flow(qpriv->rcd, qp);

	for (i = qp->s_acked; i != qp->s_head;) {
		struct rvt_swqe *wqe = rvt_get_swqe_ptr(qp, i);

		if (++i == qp->s_size)
			i = 0;
		 
		if (wqe->wr.opcode != IB_WR_TID_RDMA_READ)
			continue;
		do {
			struct hfi1_swqe_priv *priv = wqe->priv;

			ret = hfi1_kern_exp_rcv_clear(&priv->tid_req);
		} while (!ret);
	}
	for (i = qp->s_acked_ack_queue; i != qp->r_head_ack_queue;) {
		struct rvt_ack_entry *e = &qp->s_ack_queue[i];

		if (++i == rvt_max_atomic(ib_to_rvt(qp->ibqp.device)))
			i = 0;
		 
		if (e->opcode != TID_OP(WRITE_REQ))
			continue;
		do {
			struct hfi1_ack_priv *priv = e->priv;

			ret = hfi1_kern_exp_rcv_clear(&priv->tid_req);
		} while (!ret);
	}
}

bool hfi1_tid_rdma_wqe_interlock(struct rvt_qp *qp, struct rvt_swqe *wqe)
{
	struct rvt_swqe *prev;
	struct hfi1_qp_priv *priv = qp->priv;
	u32 s_prev;
	struct tid_rdma_request *req;

	s_prev = (qp->s_cur == 0 ? qp->s_size : qp->s_cur) - 1;
	prev = rvt_get_swqe_ptr(qp, s_prev);

	switch (wqe->wr.opcode) {
	case IB_WR_SEND:
	case IB_WR_SEND_WITH_IMM:
	case IB_WR_SEND_WITH_INV:
	case IB_WR_ATOMIC_CMP_AND_SWP:
	case IB_WR_ATOMIC_FETCH_AND_ADD:
	case IB_WR_RDMA_WRITE:
	case IB_WR_RDMA_WRITE_WITH_IMM:
		switch (prev->wr.opcode) {
		case IB_WR_TID_RDMA_WRITE:
			req = wqe_to_tid_req(prev);
			if (req->ack_seg != req->total_segs)
				goto interlock;
			break;
		default:
			break;
		}
		break;
	case IB_WR_RDMA_READ:
		if (prev->wr.opcode != IB_WR_TID_RDMA_WRITE)
			break;
		fallthrough;
	case IB_WR_TID_RDMA_READ:
		switch (prev->wr.opcode) {
		case IB_WR_RDMA_READ:
			if (qp->s_acked != qp->s_cur)
				goto interlock;
			break;
		case IB_WR_TID_RDMA_WRITE:
			req = wqe_to_tid_req(prev);
			if (req->ack_seg != req->total_segs)
				goto interlock;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return false;

interlock:
	priv->s_flags |= HFI1_S_TID_WAIT_INTERLCK;
	return true;
}

 
static inline bool hfi1_check_sge_align(struct rvt_qp *qp,
					struct rvt_sge *sge, int num_sge)
{
	int i;

	for (i = 0; i < num_sge; i++, sge++) {
		trace_hfi1_sge_check_align(qp, i, sge);
		if ((u64)sge->vaddr & ~PAGE_MASK ||
		    sge->sge_length & ~PAGE_MASK)
			return false;
	}
	return true;
}

void setup_tid_rdma_wqe(struct rvt_qp *qp, struct rvt_swqe *wqe)
{
	struct hfi1_qp_priv *qpriv = (struct hfi1_qp_priv *)qp->priv;
	struct hfi1_swqe_priv *priv = wqe->priv;
	struct tid_rdma_params *remote;
	enum ib_wr_opcode new_opcode;
	bool do_tid_rdma = false;
	struct hfi1_pportdata *ppd = qpriv->rcd->ppd;

	if ((rdma_ah_get_dlid(&qp->remote_ah_attr) & ~((1 << ppd->lmc) - 1)) ==
				ppd->lid)
		return;
	if (qpriv->hdr_type != HFI1_PKT_TYPE_9B)
		return;

	rcu_read_lock();
	remote = rcu_dereference(qpriv->tid_rdma.remote);
	 
	if (!remote)
		goto exit;

	if (wqe->wr.opcode == IB_WR_RDMA_READ) {
		if (hfi1_check_sge_align(qp, &wqe->sg_list[0],
					 wqe->wr.num_sge)) {
			new_opcode = IB_WR_TID_RDMA_READ;
			do_tid_rdma = true;
		}
	} else if (wqe->wr.opcode == IB_WR_RDMA_WRITE) {
		 
		if (!(wqe->rdma_wr.remote_addr & ~PAGE_MASK) &&
		    !(wqe->length & ~PAGE_MASK)) {
			new_opcode = IB_WR_TID_RDMA_WRITE;
			do_tid_rdma = true;
		}
	}

	if (do_tid_rdma) {
		if (hfi1_kern_exp_rcv_alloc_flows(&priv->tid_req, GFP_ATOMIC))
			goto exit;
		wqe->wr.opcode = new_opcode;
		priv->tid_req.seg_len =
			min_t(u32, remote->max_len, wqe->length);
		priv->tid_req.total_segs =
			DIV_ROUND_UP(wqe->length, priv->tid_req.seg_len);
		 
		wqe->lpsn = wqe->psn;
		if (wqe->wr.opcode == IB_WR_TID_RDMA_READ) {
			priv->tid_req.n_flows = remote->max_read;
			qpriv->tid_r_reqs++;
			wqe->lpsn += rvt_div_round_up_mtu(qp, wqe->length) - 1;
		} else {
			wqe->lpsn += priv->tid_req.total_segs - 1;
			atomic_inc(&qpriv->n_requests);
		}

		priv->tid_req.cur_seg = 0;
		priv->tid_req.comp_seg = 0;
		priv->tid_req.ack_seg = 0;
		priv->tid_req.state = TID_REQUEST_INACTIVE;
		 
		priv->tid_req.acked_tail = priv->tid_req.setup_head;
		trace_hfi1_tid_req_setup_tid_wqe(qp, 1, wqe->wr.opcode,
						 wqe->psn, wqe->lpsn,
						 &priv->tid_req);
	}
exit:
	rcu_read_unlock();
}

 

u32 hfi1_build_tid_rdma_write_req(struct rvt_qp *qp, struct rvt_swqe *wqe,
				  struct ib_other_headers *ohdr,
				  u32 *bth1, u32 *bth2, u32 *len)
{
	struct hfi1_qp_priv *qpriv = qp->priv;
	struct tid_rdma_request *req = wqe_to_tid_req(wqe);
	struct tid_rdma_params *remote;

	rcu_read_lock();
	remote = rcu_dereference(qpriv->tid_rdma.remote);
	 
	req->n_flows = remote->max_write;
	req->state = TID_REQUEST_ACTIVE;

	KDETH_RESET(ohdr->u.tid_rdma.w_req.kdeth0, KVER, 0x1);
	KDETH_RESET(ohdr->u.tid_rdma.w_req.kdeth1, JKEY, remote->jkey);
	ohdr->u.tid_rdma.w_req.reth.vaddr =
		cpu_to_be64(wqe->rdma_wr.remote_addr + (wqe->length - *len));
	ohdr->u.tid_rdma.w_req.reth.rkey =
		cpu_to_be32(wqe->rdma_wr.rkey);
	ohdr->u.tid_rdma.w_req.reth.length = cpu_to_be32(*len);
	ohdr->u.tid_rdma.w_req.verbs_qp = cpu_to_be32(qp->remote_qpn);
	*bth1 &= ~RVT_QPN_MASK;
	*bth1 |= remote->qp;
	qp->s_state = TID_OP(WRITE_REQ);
	qp->s_flags |= HFI1_S_WAIT_TID_RESP;
	*bth2 |= IB_BTH_REQ_ACK;
	*len = 0;

	rcu_read_unlock();
	return sizeof(ohdr->u.tid_rdma.w_req) / sizeof(u32);
}

static u32 hfi1_compute_tid_rdma_flow_wt(struct rvt_qp *qp)
{
	 
	return (MAX_TID_FLOW_PSN * qp->pmtu) >> TID_RDMA_SEGMENT_SHIFT;
}

static u32 position_in_queue(struct hfi1_qp_priv *qpriv,
			     struct tid_queue *queue)
{
	return qpriv->tid_enqueue - queue->dequeue;
}

 
static u32 hfi1_compute_tid_rnr_timeout(struct rvt_qp *qp, u32 to_seg)
{
	struct hfi1_qp_priv *qpriv = qp->priv;
	u64 timeout;
	u32 bytes_per_us;
	u8 i;

	bytes_per_us = active_egress_rate(qpriv->rcd->ppd) / 8;
	timeout = (to_seg * TID_RDMA_MAX_SEGMENT_SIZE) / bytes_per_us;
	 
	for (i = 1; i <= IB_AETH_CREDIT_MASK; i++)
		if (rvt_rnr_tbl_to_usec(i) >= timeout)
			return i;
	return 0;
}

 
static void hfi1_tid_write_alloc_resources(struct rvt_qp *qp, bool intr_ctx)
{
	struct tid_rdma_request *req;
	struct hfi1_qp_priv *qpriv = qp->priv;
	struct hfi1_ctxtdata *rcd = qpriv->rcd;
	struct tid_rdma_params *local = &qpriv->tid_rdma.local;
	struct rvt_ack_entry *e;
	u32 npkts, to_seg;
	bool last;
	int ret = 0;

	lockdep_assert_held(&qp->s_lock);

	while (1) {
		trace_hfi1_rsp_tid_write_alloc_res(qp, 0);
		trace_hfi1_tid_write_rsp_alloc_res(qp);
		 
		if (qpriv->rnr_nak_state == TID_RNR_NAK_SEND)
			break;

		 
		if (qpriv->r_tid_alloc == qpriv->r_tid_head) {
			 
			if (qpriv->flow_state.index < RXE_NUM_TID_FLOWS &&
			    !qpriv->alloc_w_segs) {
				hfi1_kern_clear_hw_flow(rcd, qp);
				qpriv->s_flags &= ~HFI1_R_TID_SW_PSN;
			}
			break;
		}

		e = &qp->s_ack_queue[qpriv->r_tid_alloc];
		if (e->opcode != TID_OP(WRITE_REQ))
			goto next_req;
		req = ack_to_tid_req(e);
		trace_hfi1_tid_req_write_alloc_res(qp, 0, e->opcode, e->psn,
						   e->lpsn, req);
		 
		if (req->alloc_seg >= req->total_segs)
			goto next_req;

		 
		if (qpriv->alloc_w_segs >= local->max_write)
			break;

		 
		if (qpriv->sync_pt && qpriv->alloc_w_segs)
			break;

		 
		if (qpriv->sync_pt && !qpriv->alloc_w_segs) {
			hfi1_kern_clear_hw_flow(rcd, qp);
			qpriv->sync_pt = false;
			qpriv->s_flags &= ~HFI1_R_TID_SW_PSN;
		}

		 
		if (qpriv->flow_state.index >= RXE_NUM_TID_FLOWS) {
			ret = hfi1_kern_setup_hw_flow(qpriv->rcd, qp);
			if (ret) {
				to_seg = hfi1_compute_tid_rdma_flow_wt(qp) *
					position_in_queue(qpriv,
							  &rcd->flow_queue);
				break;
			}
		}

		npkts = rvt_div_round_up_mtu(qp, req->seg_len);

		 
		if (qpriv->flow_state.psn + npkts > MAX_TID_FLOW_PSN - 1) {
			qpriv->sync_pt = true;
			break;
		}

		 
		if (!CIRC_SPACE(req->setup_head, req->acked_tail,
				MAX_FLOWS)) {
			ret = -EAGAIN;
			to_seg = MAX_FLOWS >> 1;
			tid_rdma_trigger_ack(qp);
			break;
		}

		 
		ret = hfi1_kern_exp_rcv_setup(req, &req->ss, &last);
		if (ret == -EAGAIN)
			to_seg = position_in_queue(qpriv, &rcd->rarr_queue);
		if (ret)
			break;

		qpriv->alloc_w_segs++;
		req->alloc_seg++;
		continue;
next_req:
		 
		if (++qpriv->r_tid_alloc >
		    rvt_size_atomic(ib_to_rvt(qp->ibqp.device)))
			qpriv->r_tid_alloc = 0;
	}

	 
	if (ret == -EAGAIN && intr_ctx && !qp->r_nak_state)
		goto send_rnr_nak;

	return;

send_rnr_nak:
	lockdep_assert_held(&qp->r_lock);

	 
	qp->r_nak_state = hfi1_compute_tid_rnr_timeout(qp, to_seg) | IB_RNR_NAK;

	 
	qp->r_psn = e->psn + req->alloc_seg;
	qp->r_ack_psn = qp->r_psn;
	 
	qp->r_head_ack_queue = qpriv->r_tid_alloc + 1;
	if (qp->r_head_ack_queue > rvt_size_atomic(ib_to_rvt(qp->ibqp.device)))
		qp->r_head_ack_queue = 0;
	qpriv->r_tid_head = qp->r_head_ack_queue;
	 
	qp->s_nak_state = qp->r_nak_state;
	qp->s_ack_psn = qp->r_ack_psn;
	 
	qp->s_flags &= ~(RVT_S_ACK_PENDING);

	trace_hfi1_rsp_tid_write_alloc_res(qp, qp->r_psn);
	 
	qpriv->rnr_nak_state = TID_RNR_NAK_SEND;

	 
	rc_defered_ack(rcd, qp);
}

void hfi1_rc_rcv_tid_rdma_write_req(struct hfi1_packet *packet)
{
	 

	 
	struct hfi1_ctxtdata *rcd = packet->rcd;
	struct rvt_qp *qp = packet->qp;
	struct hfi1_ibport *ibp = to_iport(qp->ibqp.device, qp->port_num);
	struct ib_other_headers *ohdr = packet->ohdr;
	struct rvt_ack_entry *e;
	unsigned long flags;
	struct ib_reth *reth;
	struct hfi1_qp_priv *qpriv = qp->priv;
	struct tid_rdma_request *req;
	u32 bth0, psn, len, rkey, num_segs;
	bool fecn;
	u8 next;
	u64 vaddr;
	int diff;

	bth0 = be32_to_cpu(ohdr->bth[0]);
	if (hfi1_ruc_check_hdr(ibp, packet))
		return;

	fecn = process_ecn(qp, packet);
	psn = mask_psn(be32_to_cpu(ohdr->bth[2]));
	trace_hfi1_rsp_rcv_tid_write_req(qp, psn);

	if (qp->state == IB_QPS_RTR && !(qp->r_flags & RVT_R_COMM_EST))
		rvt_comm_est(qp);

	if (unlikely(!(qp->qp_access_flags & IB_ACCESS_REMOTE_WRITE)))
		goto nack_inv;

	reth = &ohdr->u.tid_rdma.w_req.reth;
	vaddr = be64_to_cpu(reth->vaddr);
	len = be32_to_cpu(reth->length);

	num_segs = DIV_ROUND_UP(len, qpriv->tid_rdma.local.max_len);
	diff = delta_psn(psn, qp->r_psn);
	if (unlikely(diff)) {
		tid_rdma_rcv_err(packet, ohdr, qp, psn, diff, fecn);
		return;
	}

	 
	if (qpriv->rnr_nak_state)
		qp->r_head_ack_queue = qp->r_head_ack_queue ?
			qp->r_head_ack_queue - 1 :
			rvt_size_atomic(ib_to_rvt(qp->ibqp.device));

	 
	next = qp->r_head_ack_queue + 1;
	if (next > rvt_size_atomic(ib_to_rvt(qp->ibqp.device)))
		next = 0;
	spin_lock_irqsave(&qp->s_lock, flags);
	if (unlikely(next == qp->s_acked_ack_queue)) {
		if (!qp->s_ack_queue[next].sent)
			goto nack_inv_unlock;
		update_ack_queue(qp, next);
	}
	e = &qp->s_ack_queue[qp->r_head_ack_queue];
	req = ack_to_tid_req(e);

	 
	if (qpriv->rnr_nak_state) {
		qp->r_nak_state = 0;
		qp->s_nak_state = 0;
		qpriv->rnr_nak_state = TID_RNR_NAK_INIT;
		qp->r_psn = e->lpsn + 1;
		req->state = TID_REQUEST_INIT;
		goto update_head;
	}

	release_rdma_sge_mr(e);

	 
	if (!len || len & ~PAGE_MASK)
		goto nack_inv_unlock;

	rkey = be32_to_cpu(reth->rkey);
	qp->r_len = len;

	if (e->opcode == TID_OP(WRITE_REQ) &&
	    (req->setup_head != req->clear_tail ||
	     req->clear_tail != req->acked_tail))
		goto nack_inv_unlock;

	if (unlikely(!rvt_rkey_ok(qp, &e->rdma_sge, qp->r_len, vaddr,
				  rkey, IB_ACCESS_REMOTE_WRITE)))
		goto nack_acc;

	qp->r_psn += num_segs - 1;

	e->opcode = (bth0 >> 24) & 0xff;
	e->psn = psn;
	e->lpsn = qp->r_psn;
	e->sent = 0;

	req->n_flows = min_t(u16, num_segs, qpriv->tid_rdma.local.max_write);
	req->state = TID_REQUEST_INIT;
	req->cur_seg = 0;
	req->comp_seg = 0;
	req->ack_seg = 0;
	req->alloc_seg = 0;
	req->isge = 0;
	req->seg_len = qpriv->tid_rdma.local.max_len;
	req->total_len = len;
	req->total_segs = num_segs;
	req->r_flow_psn = e->psn;
	req->ss.sge = e->rdma_sge;
	req->ss.num_sge = 1;

	req->flow_idx = req->setup_head;
	req->clear_tail = req->setup_head;
	req->acked_tail = req->setup_head;

	qp->r_state = e->opcode;
	qp->r_nak_state = 0;
	 
	qp->r_msn++;
	qp->r_psn++;

	trace_hfi1_tid_req_rcv_write_req(qp, 0, e->opcode, e->psn, e->lpsn,
					 req);

	if (qpriv->r_tid_tail == HFI1_QP_WQE_INVALID) {
		qpriv->r_tid_tail = qp->r_head_ack_queue;
	} else if (qpriv->r_tid_tail == qpriv->r_tid_head) {
		struct tid_rdma_request *ptr;

		e = &qp->s_ack_queue[qpriv->r_tid_tail];
		ptr = ack_to_tid_req(e);

		if (e->opcode != TID_OP(WRITE_REQ) ||
		    ptr->comp_seg == ptr->total_segs) {
			if (qpriv->r_tid_tail == qpriv->r_tid_ack)
				qpriv->r_tid_ack = qp->r_head_ack_queue;
			qpriv->r_tid_tail = qp->r_head_ack_queue;
		}
	}
update_head:
	qp->r_head_ack_queue = next;
	qpriv->r_tid_head = qp->r_head_ack_queue;

	hfi1_tid_write_alloc_resources(qp, true);
	trace_hfi1_tid_write_rsp_rcv_req(qp);

	 
	qp->s_flags |= RVT_S_RESP_PENDING;
	if (fecn)
		qp->s_flags |= RVT_S_ECN;
	hfi1_schedule_send(qp);

	spin_unlock_irqrestore(&qp->s_lock, flags);
	return;

nack_inv_unlock:
	spin_unlock_irqrestore(&qp->s_lock, flags);
nack_inv:
	rvt_rc_error(qp, IB_WC_LOC_QP_OP_ERR);
	qp->r_nak_state = IB_NAK_INVALID_REQUEST;
	qp->r_ack_psn = qp->r_psn;
	 
	rc_defered_ack(rcd, qp);
	return;
nack_acc:
	spin_unlock_irqrestore(&qp->s_lock, flags);
	rvt_rc_error(qp, IB_WC_LOC_PROT_ERR);
	qp->r_nak_state = IB_NAK_REMOTE_ACCESS_ERROR;
	qp->r_ack_psn = qp->r_psn;
}

u32 hfi1_build_tid_rdma_write_resp(struct rvt_qp *qp, struct rvt_ack_entry *e,
				   struct ib_other_headers *ohdr, u32 *bth1,
				   u32 bth2, u32 *len,
				   struct rvt_sge_state **ss)
{
	struct hfi1_ack_priv *epriv = e->priv;
	struct tid_rdma_request *req = &epriv->tid_req;
	struct hfi1_qp_priv *qpriv = qp->priv;
	struct tid_rdma_flow *flow = NULL;
	u32 resp_len = 0, hdwords = 0;
	void *resp_addr = NULL;
	struct tid_rdma_params *remote;

	trace_hfi1_tid_req_build_write_resp(qp, 0, e->opcode, e->psn, e->lpsn,
					    req);
	trace_hfi1_tid_write_rsp_build_resp(qp);
	trace_hfi1_rsp_build_tid_write_resp(qp, bth2);
	flow = &req->flows[req->flow_idx];
	switch (req->state) {
	default:
		 
		hfi1_tid_write_alloc_resources(qp, false);

		 
		if (req->cur_seg >= req->alloc_seg)
			goto done;

		 
		if (qpriv->rnr_nak_state == TID_RNR_NAK_SENT)
			goto done;

		req->state = TID_REQUEST_ACTIVE;
		trace_hfi1_tid_flow_build_write_resp(qp, req->flow_idx, flow);
		req->flow_idx = CIRC_NEXT(req->flow_idx, MAX_FLOWS);
		hfi1_add_tid_reap_timer(qp);
		break;

	case TID_REQUEST_RESEND_ACTIVE:
	case TID_REQUEST_RESEND:
		trace_hfi1_tid_flow_build_write_resp(qp, req->flow_idx, flow);
		req->flow_idx = CIRC_NEXT(req->flow_idx, MAX_FLOWS);
		if (!CIRC_CNT(req->setup_head, req->flow_idx, MAX_FLOWS))
			req->state = TID_REQUEST_ACTIVE;

		hfi1_mod_tid_reap_timer(qp);
		break;
	}
	flow->flow_state.resp_ib_psn = bth2;
	resp_addr = (void *)flow->tid_entry;
	resp_len = sizeof(*flow->tid_entry) * flow->tidcnt;
	req->cur_seg++;

	memset(&ohdr->u.tid_rdma.w_rsp, 0, sizeof(ohdr->u.tid_rdma.w_rsp));
	epriv->ss.sge.vaddr = resp_addr;
	epriv->ss.sge.sge_length = resp_len;
	epriv->ss.sge.length = epriv->ss.sge.sge_length;
	 
	epriv->ss.sge.mr = NULL;
	epriv->ss.sge.m = 0;
	epriv->ss.sge.n = 0;

	epriv->ss.sg_list = NULL;
	epriv->ss.total_len = epriv->ss.sge.sge_length;
	epriv->ss.num_sge = 1;

	*ss = &epriv->ss;
	*len = epriv->ss.total_len;

	 
	rcu_read_lock();
	remote = rcu_dereference(qpriv->tid_rdma.remote);

	KDETH_RESET(ohdr->u.tid_rdma.w_rsp.kdeth0, KVER, 0x1);
	KDETH_RESET(ohdr->u.tid_rdma.w_rsp.kdeth1, JKEY, remote->jkey);
	ohdr->u.tid_rdma.w_rsp.aeth = rvt_compute_aeth(qp);
	ohdr->u.tid_rdma.w_rsp.tid_flow_psn =
		cpu_to_be32((flow->flow_state.generation <<
			     HFI1_KDETH_BTH_SEQ_SHIFT) |
			    (flow->flow_state.spsn &
			     HFI1_KDETH_BTH_SEQ_MASK));
	ohdr->u.tid_rdma.w_rsp.tid_flow_qp =
		cpu_to_be32(qpriv->tid_rdma.local.qp |
			    ((flow->idx & TID_RDMA_DESTQP_FLOW_MASK) <<
			     TID_RDMA_DESTQP_FLOW_SHIFT) |
			    qpriv->rcd->ctxt);
	ohdr->u.tid_rdma.w_rsp.verbs_qp = cpu_to_be32(qp->remote_qpn);
	*bth1 = remote->qp;
	rcu_read_unlock();
	hdwords = sizeof(ohdr->u.tid_rdma.w_rsp) / sizeof(u32);
	qpriv->pending_tid_w_segs++;
done:
	return hdwords;
}

static void hfi1_add_tid_reap_timer(struct rvt_qp *qp)
{
	struct hfi1_qp_priv *qpriv = qp->priv;

	lockdep_assert_held(&qp->s_lock);
	if (!(qpriv->s_flags & HFI1_R_TID_RSC_TIMER)) {
		qpriv->s_flags |= HFI1_R_TID_RSC_TIMER;
		qpriv->s_tid_timer.expires = jiffies +
			qpriv->tid_timer_timeout_jiffies;
		add_timer(&qpriv->s_tid_timer);
	}
}

static void hfi1_mod_tid_reap_timer(struct rvt_qp *qp)
{
	struct hfi1_qp_priv *qpriv = qp->priv;

	lockdep_assert_held(&qp->s_lock);
	qpriv->s_flags |= HFI1_R_TID_RSC_TIMER;
	mod_timer(&qpriv->s_tid_timer, jiffies +
		  qpriv->tid_timer_timeout_jiffies);
}

static int hfi1_stop_tid_reap_timer(struct rvt_qp *qp)
{
	struct hfi1_qp_priv *qpriv = qp->priv;
	int rval = 0;

	lockdep_assert_held(&qp->s_lock);
	if (qpriv->s_flags & HFI1_R_TID_RSC_TIMER) {
		rval = del_timer(&qpriv->s_tid_timer);
		qpriv->s_flags &= ~HFI1_R_TID_RSC_TIMER;
	}
	return rval;
}

void hfi1_del_tid_reap_timer(struct rvt_qp *qp)
{
	struct hfi1_qp_priv *qpriv = qp->priv;

	del_timer_sync(&qpriv->s_tid_timer);
	qpriv->s_flags &= ~HFI1_R_TID_RSC_TIMER;
}

static void hfi1_tid_timeout(struct timer_list *t)
{
	struct hfi1_qp_priv *qpriv = from_timer(qpriv, t, s_tid_timer);
	struct rvt_qp *qp = qpriv->owner;
	struct rvt_dev_info *rdi = ib_to_rvt(qp->ibqp.device);
	unsigned long flags;
	u32 i;

	spin_lock_irqsave(&qp->r_lock, flags);
	spin_lock(&qp->s_lock);
	if (qpriv->s_flags & HFI1_R_TID_RSC_TIMER) {
		dd_dev_warn(dd_from_ibdev(qp->ibqp.device), "[QP%u] %s %d\n",
			    qp->ibqp.qp_num, __func__, __LINE__);
		trace_hfi1_msg_tid_timeout( 
			qp, "resource timeout = ",
			(u64)qpriv->tid_timer_timeout_jiffies);
		hfi1_stop_tid_reap_timer(qp);
		 
		hfi1_kern_clear_hw_flow(qpriv->rcd, qp);
		for (i = 0; i < rvt_max_atomic(rdi); i++) {
			struct tid_rdma_request *req =
				ack_to_tid_req(&qp->s_ack_queue[i]);

			hfi1_kern_exp_rcv_clear_all(req);
		}
		spin_unlock(&qp->s_lock);
		if (qp->ibqp.event_handler) {
			struct ib_event ev;

			ev.device = qp->ibqp.device;
			ev.element.qp = &qp->ibqp;
			ev.event = IB_EVENT_QP_FATAL;
			qp->ibqp.event_handler(&ev, qp->ibqp.qp_context);
		}
		rvt_rc_error(qp, IB_WC_RESP_TIMEOUT_ERR);
		goto unlock_r_lock;
	}
	spin_unlock(&qp->s_lock);
unlock_r_lock:
	spin_unlock_irqrestore(&qp->r_lock, flags);
}

void hfi1_rc_rcv_tid_rdma_write_resp(struct hfi1_packet *packet)
{
	 

	 
	struct ib_other_headers *ohdr = packet->ohdr;
	struct rvt_qp *qp = packet->qp;
	struct hfi1_qp_priv *qpriv = qp->priv;
	struct hfi1_ctxtdata *rcd = packet->rcd;
	struct rvt_swqe *wqe;
	struct tid_rdma_request *req;
	struct tid_rdma_flow *flow;
	enum ib_wc_status status;
	u32 opcode, aeth, psn, flow_psn, i, tidlen = 0, pktlen;
	bool fecn;
	unsigned long flags;

	fecn = process_ecn(qp, packet);
	psn = mask_psn(be32_to_cpu(ohdr->bth[2]));
	aeth = be32_to_cpu(ohdr->u.tid_rdma.w_rsp.aeth);
	opcode = (be32_to_cpu(ohdr->bth[0]) >> 24) & 0xff;

	spin_lock_irqsave(&qp->s_lock, flags);

	 
	if (cmp_psn(psn, qp->s_next_psn) >= 0)
		goto ack_done;

	 
	if (unlikely(cmp_psn(psn, qp->s_last_psn) <= 0))
		goto ack_done;

	if (unlikely(qp->s_acked == qp->s_tail))
		goto ack_done;

	 
	if (qp->r_flags & RVT_R_RDMAR_SEQ) {
		if (cmp_psn(psn, qp->s_last_psn + 1) != 0)
			goto ack_done;
		qp->r_flags &= ~RVT_R_RDMAR_SEQ;
	}

	wqe = rvt_get_swqe_ptr(qp, qpriv->s_tid_cur);
	if (unlikely(wqe->wr.opcode != IB_WR_TID_RDMA_WRITE))
		goto ack_op_err;

	req = wqe_to_tid_req(wqe);
	 
	if (!CIRC_SPACE(req->setup_head, req->acked_tail, MAX_FLOWS))
		goto ack_done;

	 
	if (!do_rc_ack(qp, aeth, psn, opcode, 0, rcd))
		goto ack_done;

	trace_hfi1_ack(qp, psn);

	flow = &req->flows[req->setup_head];
	flow->pkt = 0;
	flow->tid_idx = 0;
	flow->tid_offset = 0;
	flow->sent = 0;
	flow->resync_npkts = 0;
	flow->tid_qpn = be32_to_cpu(ohdr->u.tid_rdma.w_rsp.tid_flow_qp);
	flow->idx = (flow->tid_qpn >> TID_RDMA_DESTQP_FLOW_SHIFT) &
		TID_RDMA_DESTQP_FLOW_MASK;
	flow_psn = mask_psn(be32_to_cpu(ohdr->u.tid_rdma.w_rsp.tid_flow_psn));
	flow->flow_state.generation = flow_psn >> HFI1_KDETH_BTH_SEQ_SHIFT;
	flow->flow_state.spsn = flow_psn & HFI1_KDETH_BTH_SEQ_MASK;
	flow->flow_state.resp_ib_psn = psn;
	flow->length = min_t(u32, req->seg_len,
			     (wqe->length - (req->comp_seg * req->seg_len)));

	flow->npkts = rvt_div_round_up_mtu(qp, flow->length);
	flow->flow_state.lpsn = flow->flow_state.spsn +
		flow->npkts - 1;
	 
	pktlen = packet->tlen - (packet->hlen + 4);
	if (pktlen > sizeof(flow->tid_entry)) {
		status = IB_WC_LOC_LEN_ERR;
		goto ack_err;
	}
	memcpy(flow->tid_entry, packet->ebuf, pktlen);
	flow->tidcnt = pktlen / sizeof(*flow->tid_entry);
	trace_hfi1_tid_flow_rcv_write_resp(qp, req->setup_head, flow);

	req->comp_seg++;
	trace_hfi1_tid_write_sender_rcv_resp(qp, 0);
	 
	for (i = 0; i < flow->tidcnt; i++) {
		trace_hfi1_tid_entry_rcv_write_resp( 
			qp, i, flow->tid_entry[i]);
		if (!EXP_TID_GET(flow->tid_entry[i], LEN)) {
			status = IB_WC_LOC_LEN_ERR;
			goto ack_err;
		}
		tidlen += EXP_TID_GET(flow->tid_entry[i], LEN);
	}
	if (tidlen * PAGE_SIZE < flow->length) {
		status = IB_WC_LOC_LEN_ERR;
		goto ack_err;
	}

	trace_hfi1_tid_req_rcv_write_resp(qp, 0, wqe->wr.opcode, wqe->psn,
					  wqe->lpsn, req);
	 
	if (!cmp_psn(psn, wqe->psn)) {
		req->r_last_acked = mask_psn(wqe->psn - 1);
		 
		req->acked_tail = req->setup_head;
	}

	 
	req->setup_head = CIRC_NEXT(req->setup_head, MAX_FLOWS);
	req->state = TID_REQUEST_ACTIVE;

	 
	if (qpriv->s_tid_cur != qpriv->s_tid_head &&
	    req->comp_seg == req->total_segs) {
		for (i = qpriv->s_tid_cur + 1; ; i++) {
			if (i == qp->s_size)
				i = 0;
			wqe = rvt_get_swqe_ptr(qp, i);
			if (i == qpriv->s_tid_head)
				break;
			if (wqe->wr.opcode == IB_WR_TID_RDMA_WRITE)
				break;
		}
		qpriv->s_tid_cur = i;
	}
	qp->s_flags &= ~HFI1_S_WAIT_TID_RESP;
	hfi1_schedule_tid_send(qp);
	goto ack_done;

ack_op_err:
	status = IB_WC_LOC_QP_OP_ERR;
ack_err:
	rvt_error_qp(qp, status);
ack_done:
	if (fecn)
		qp->s_flags |= RVT_S_ECN;
	spin_unlock_irqrestore(&qp->s_lock, flags);
}

bool hfi1_build_tid_rdma_packet(struct rvt_swqe *wqe,
				struct ib_other_headers *ohdr,
				u32 *bth1, u32 *bth2, u32 *len)
{
	struct tid_rdma_request *req = wqe_to_tid_req(wqe);
	struct tid_rdma_flow *flow = &req->flows[req->clear_tail];
	struct tid_rdma_params *remote;
	struct rvt_qp *qp = req->qp;
	struct hfi1_qp_priv *qpriv = qp->priv;
	u32 tidentry = flow->tid_entry[flow->tid_idx];
	u32 tidlen = EXP_TID_GET(tidentry, LEN) << PAGE_SHIFT;
	struct tid_rdma_write_data *wd = &ohdr->u.tid_rdma.w_data;
	u32 next_offset, om = KDETH_OM_LARGE;
	bool last_pkt;

	if (!tidlen) {
		hfi1_trdma_send_complete(qp, wqe, IB_WC_REM_INV_RD_REQ_ERR);
		rvt_error_qp(qp, IB_WC_REM_INV_RD_REQ_ERR);
	}

	*len = min_t(u32, qp->pmtu, tidlen - flow->tid_offset);
	flow->sent += *len;
	next_offset = flow->tid_offset + *len;
	last_pkt = (flow->tid_idx == (flow->tidcnt - 1) &&
		    next_offset >= tidlen) || (flow->sent >= flow->length);
	trace_hfi1_tid_entry_build_write_data(qp, flow->tid_idx, tidentry);
	trace_hfi1_tid_flow_build_write_data(qp, req->clear_tail, flow);

	rcu_read_lock();
	remote = rcu_dereference(qpriv->tid_rdma.remote);
	KDETH_RESET(wd->kdeth0, KVER, 0x1);
	KDETH_SET(wd->kdeth0, SH, !last_pkt);
	KDETH_SET(wd->kdeth0, INTR, !!(!last_pkt && remote->urg));
	KDETH_SET(wd->kdeth0, TIDCTRL, EXP_TID_GET(tidentry, CTRL));
	KDETH_SET(wd->kdeth0, TID, EXP_TID_GET(tidentry, IDX));
	KDETH_SET(wd->kdeth0, OM, om == KDETH_OM_LARGE);
	KDETH_SET(wd->kdeth0, OFFSET, flow->tid_offset / om);
	KDETH_RESET(wd->kdeth1, JKEY, remote->jkey);
	wd->verbs_qp = cpu_to_be32(qp->remote_qpn);
	rcu_read_unlock();

	*bth1 = flow->tid_qpn;
	*bth2 = mask_psn(((flow->flow_state.spsn + flow->pkt++) &
			 HFI1_KDETH_BTH_SEQ_MASK) |
			 (flow->flow_state.generation <<
			  HFI1_KDETH_BTH_SEQ_SHIFT));
	if (last_pkt) {
		 
		if (flow->flow_state.lpsn + 1 +
		    rvt_div_round_up_mtu(qp, req->seg_len) >
		    MAX_TID_FLOW_PSN)
			req->state = TID_REQUEST_SYNC;
		*bth2 |= IB_BTH_REQ_ACK;
	}

	if (next_offset >= tidlen) {
		flow->tid_offset = 0;
		flow->tid_idx++;
	} else {
		flow->tid_offset = next_offset;
	}
	return last_pkt;
}

void hfi1_rc_rcv_tid_rdma_write_data(struct hfi1_packet *packet)
{
	struct rvt_qp *qp = packet->qp;
	struct hfi1_qp_priv *priv = qp->priv;
	struct hfi1_ctxtdata *rcd = priv->rcd;
	struct ib_other_headers *ohdr = packet->ohdr;
	struct rvt_ack_entry *e;
	struct tid_rdma_request *req;
	struct tid_rdma_flow *flow;
	struct hfi1_ibdev *dev = to_idev(qp->ibqp.device);
	unsigned long flags;
	u32 psn, next;
	u8 opcode;
	bool fecn;

	fecn = process_ecn(qp, packet);
	psn = mask_psn(be32_to_cpu(ohdr->bth[2]));
	opcode = (be32_to_cpu(ohdr->bth[0]) >> 24) & 0xff;

	 
	spin_lock_irqsave(&qp->s_lock, flags);
	e = &qp->s_ack_queue[priv->r_tid_tail];
	req = ack_to_tid_req(e);
	flow = &req->flows[req->clear_tail];
	if (cmp_psn(psn, full_flow_psn(flow, flow->flow_state.lpsn))) {
		update_r_next_psn_fecn(packet, priv, rcd, flow, fecn);

		if (cmp_psn(psn, flow->flow_state.r_next_psn))
			goto send_nak;

		flow->flow_state.r_next_psn = mask_psn(psn + 1);
		 
		if (fecn && packet->etype == RHF_RCV_TYPE_EAGER) {
			struct rvt_sge_state ss;
			u32 len;
			u32 tlen = packet->tlen;
			u16 hdrsize = packet->hlen;
			u8 pad = packet->pad;
			u8 extra_bytes = pad + packet->extra_byte +
				(SIZE_OF_CRC << 2);
			u32 pmtu = qp->pmtu;

			if (unlikely(tlen != (hdrsize + pmtu + extra_bytes)))
				goto send_nak;
			len = req->comp_seg * req->seg_len;
			len += delta_psn(psn,
				full_flow_psn(flow, flow->flow_state.spsn)) *
				pmtu;
			if (unlikely(req->total_len - len < pmtu))
				goto send_nak;

			 
			ss.sge = e->rdma_sge;
			ss.sg_list = NULL;
			ss.num_sge = 1;
			ss.total_len = req->total_len;
			rvt_skip_sge(&ss, len, false);
			rvt_copy_sge(qp, &ss, packet->payload, pmtu, false,
				     false);
			 
			priv->r_next_psn_kdeth = mask_psn(psn + 1);
			priv->s_flags |= HFI1_R_TID_SW_PSN;
		}
		goto exit;
	}
	flow->flow_state.r_next_psn = mask_psn(psn + 1);
	hfi1_kern_exp_rcv_clear(req);
	priv->alloc_w_segs--;
	rcd->flows[flow->idx].psn = psn & HFI1_KDETH_BTH_SEQ_MASK;
	req->comp_seg++;
	priv->s_nak_state = 0;

	 
	trace_hfi1_rsp_rcv_tid_write_data(qp, psn);
	trace_hfi1_tid_req_rcv_write_data(qp, 0, e->opcode, e->psn, e->lpsn,
					  req);
	trace_hfi1_tid_write_rsp_rcv_data(qp);
	validate_r_tid_ack(priv);

	if (opcode == TID_OP(WRITE_DATA_LAST)) {
		release_rdma_sge_mr(e);
		for (next = priv->r_tid_tail + 1; ; next++) {
			if (next > rvt_size_atomic(&dev->rdi))
				next = 0;
			if (next == priv->r_tid_head)
				break;
			e = &qp->s_ack_queue[next];
			if (e->opcode == TID_OP(WRITE_REQ))
				break;
		}
		priv->r_tid_tail = next;
		if (++qp->s_acked_ack_queue > rvt_size_atomic(&dev->rdi))
			qp->s_acked_ack_queue = 0;
	}

	hfi1_tid_write_alloc_resources(qp, true);

	 
	if (req->cur_seg < req->total_segs ||
	    qp->s_tail_ack_queue != qp->r_head_ack_queue) {
		qp->s_flags |= RVT_S_RESP_PENDING;
		hfi1_schedule_send(qp);
	}

	priv->pending_tid_w_segs--;
	if (priv->s_flags & HFI1_R_TID_RSC_TIMER) {
		if (priv->pending_tid_w_segs)
			hfi1_mod_tid_reap_timer(req->qp);
		else
			hfi1_stop_tid_reap_timer(req->qp);
	}

done:
	tid_rdma_schedule_ack(qp);
exit:
	priv->r_next_psn_kdeth = flow->flow_state.r_next_psn;
	if (fecn)
		qp->s_flags |= RVT_S_ECN;
	spin_unlock_irqrestore(&qp->s_lock, flags);
	return;

send_nak:
	if (!priv->s_nak_state) {
		priv->s_nak_state = IB_NAK_PSN_ERROR;
		priv->s_nak_psn = flow->flow_state.r_next_psn;
		tid_rdma_trigger_ack(qp);
	}
	goto done;
}

static bool hfi1_tid_rdma_is_resync_psn(u32 psn)
{
	return (bool)((psn & HFI1_KDETH_BTH_SEQ_MASK) ==
		      HFI1_KDETH_BTH_SEQ_MASK);
}

u32 hfi1_build_tid_rdma_write_ack(struct rvt_qp *qp, struct rvt_ack_entry *e,
				  struct ib_other_headers *ohdr, u16 iflow,
				  u32 *bth1, u32 *bth2)
{
	struct hfi1_qp_priv *qpriv = qp->priv;
	struct tid_flow_state *fs = &qpriv->flow_state;
	struct tid_rdma_request *req = ack_to_tid_req(e);
	struct tid_rdma_flow *flow = &req->flows[iflow];
	struct tid_rdma_params *remote;

	rcu_read_lock();
	remote = rcu_dereference(qpriv->tid_rdma.remote);
	KDETH_RESET(ohdr->u.tid_rdma.ack.kdeth1, JKEY, remote->jkey);
	ohdr->u.tid_rdma.ack.verbs_qp = cpu_to_be32(qp->remote_qpn);
	*bth1 = remote->qp;
	rcu_read_unlock();

	if (qpriv->resync) {
		*bth2 = mask_psn((fs->generation <<
				  HFI1_KDETH_BTH_SEQ_SHIFT) - 1);
		ohdr->u.tid_rdma.ack.aeth = rvt_compute_aeth(qp);
	} else if (qpriv->s_nak_state) {
		*bth2 = mask_psn(qpriv->s_nak_psn);
		ohdr->u.tid_rdma.ack.aeth =
			cpu_to_be32((qp->r_msn & IB_MSN_MASK) |
				    (qpriv->s_nak_state <<
				     IB_AETH_CREDIT_SHIFT));
	} else {
		*bth2 = full_flow_psn(flow, flow->flow_state.lpsn);
		ohdr->u.tid_rdma.ack.aeth = rvt_compute_aeth(qp);
	}
	KDETH_RESET(ohdr->u.tid_rdma.ack.kdeth0, KVER, 0x1);
	ohdr->u.tid_rdma.ack.tid_flow_qp =
		cpu_to_be32(qpriv->tid_rdma.local.qp |
			    ((flow->idx & TID_RDMA_DESTQP_FLOW_MASK) <<
			     TID_RDMA_DESTQP_FLOW_SHIFT) |
			    qpriv->rcd->ctxt);

	ohdr->u.tid_rdma.ack.tid_flow_psn = 0;
	ohdr->u.tid_rdma.ack.verbs_psn =
		cpu_to_be32(flow->flow_state.resp_ib_psn);

	if (qpriv->resync) {
		 
		if (hfi1_tid_rdma_is_resync_psn(qpriv->r_next_psn_kdeth - 1)) {
			ohdr->u.tid_rdma.ack.tid_flow_psn =
				cpu_to_be32(qpriv->r_next_psn_kdeth_save);
		} else {
			 
			qpriv->r_next_psn_kdeth_save =
				qpriv->r_next_psn_kdeth - 1;
			ohdr->u.tid_rdma.ack.tid_flow_psn =
				cpu_to_be32(qpriv->r_next_psn_kdeth_save);
			qpriv->r_next_psn_kdeth = mask_psn(*bth2 + 1);
		}
		qpriv->resync = false;
	}

	return sizeof(ohdr->u.tid_rdma.ack) / sizeof(u32);
}

void hfi1_rc_rcv_tid_rdma_ack(struct hfi1_packet *packet)
{
	struct ib_other_headers *ohdr = packet->ohdr;
	struct rvt_qp *qp = packet->qp;
	struct hfi1_qp_priv *qpriv = qp->priv;
	struct rvt_swqe *wqe;
	struct tid_rdma_request *req;
	struct tid_rdma_flow *flow;
	u32 aeth, psn, req_psn, ack_psn, flpsn, resync_psn, ack_kpsn;
	unsigned long flags;
	u16 fidx;

	trace_hfi1_tid_write_sender_rcv_tid_ack(qp, 0);
	process_ecn(qp, packet);
	psn = mask_psn(be32_to_cpu(ohdr->bth[2]));
	aeth = be32_to_cpu(ohdr->u.tid_rdma.ack.aeth);
	req_psn = mask_psn(be32_to_cpu(ohdr->u.tid_rdma.ack.verbs_psn));
	resync_psn = mask_psn(be32_to_cpu(ohdr->u.tid_rdma.ack.tid_flow_psn));

	spin_lock_irqsave(&qp->s_lock, flags);
	trace_hfi1_rcv_tid_ack(qp, aeth, psn, req_psn, resync_psn);

	 
	if ((qp->s_flags & HFI1_S_WAIT_HALT) &&
	    cmp_psn(psn, qpriv->s_resync_psn))
		goto ack_op_err;

	ack_psn = req_psn;
	if (hfi1_tid_rdma_is_resync_psn(psn))
		ack_kpsn = resync_psn;
	else
		ack_kpsn = psn;
	if (aeth >> 29) {
		ack_psn--;
		ack_kpsn--;
	}

	if (unlikely(qp->s_acked == qp->s_tail))
		goto ack_op_err;

	wqe = rvt_get_swqe_ptr(qp, qp->s_acked);

	if (wqe->wr.opcode != IB_WR_TID_RDMA_WRITE)
		goto ack_op_err;

	req = wqe_to_tid_req(wqe);
	trace_hfi1_tid_req_rcv_tid_ack(qp, 0, wqe->wr.opcode, wqe->psn,
				       wqe->lpsn, req);
	flow = &req->flows[req->acked_tail];
	trace_hfi1_tid_flow_rcv_tid_ack(qp, req->acked_tail, flow);

	 
	if (cmp_psn(psn, full_flow_psn(flow, flow->flow_state.spsn)) < 0 ||
	    cmp_psn(req_psn, flow->flow_state.resp_ib_psn) < 0)
		goto ack_op_err;

	while (cmp_psn(ack_kpsn,
		       full_flow_psn(flow, flow->flow_state.lpsn)) >= 0 &&
	       req->ack_seg < req->cur_seg) {
		req->ack_seg++;
		 
		req->acked_tail = CIRC_NEXT(req->acked_tail, MAX_FLOWS);
		req->r_last_acked = flow->flow_state.resp_ib_psn;
		trace_hfi1_tid_req_rcv_tid_ack(qp, 0, wqe->wr.opcode, wqe->psn,
					       wqe->lpsn, req);
		if (req->ack_seg == req->total_segs) {
			req->state = TID_REQUEST_COMPLETE;
			wqe = do_rc_completion(qp, wqe,
					       to_iport(qp->ibqp.device,
							qp->port_num));
			trace_hfi1_sender_rcv_tid_ack(qp);
			atomic_dec(&qpriv->n_tid_requests);
			if (qp->s_acked == qp->s_tail)
				break;
			if (wqe->wr.opcode != IB_WR_TID_RDMA_WRITE)
				break;
			req = wqe_to_tid_req(wqe);
		}
		flow = &req->flows[req->acked_tail];
		trace_hfi1_tid_flow_rcv_tid_ack(qp, req->acked_tail, flow);
	}

	trace_hfi1_tid_req_rcv_tid_ack(qp, 0, wqe->wr.opcode, wqe->psn,
				       wqe->lpsn, req);
	switch (aeth >> 29) {
	case 0:          
		if (qpriv->s_flags & RVT_S_WAIT_ACK)
			qpriv->s_flags &= ~RVT_S_WAIT_ACK;
		if (!hfi1_tid_rdma_is_resync_psn(psn)) {
			 
			if (wqe->wr.opcode == IB_WR_TID_RDMA_WRITE &&
			    req->ack_seg < req->cur_seg)
				hfi1_mod_tid_retry_timer(qp);
			else
				hfi1_stop_tid_retry_timer(qp);
			hfi1_schedule_send(qp);
		} else {
			u32 spsn, fpsn, last_acked, generation;
			struct tid_rdma_request *rptr;

			 
			hfi1_stop_tid_retry_timer(qp);
			 
			qp->s_flags &= ~HFI1_S_WAIT_HALT;
			 
			qpriv->s_flags &= ~RVT_S_SEND_ONE;
			hfi1_schedule_send(qp);

			if ((qp->s_acked == qpriv->s_tid_tail &&
			     req->ack_seg == req->total_segs) ||
			    qp->s_acked == qp->s_tail) {
				qpriv->s_state = TID_OP(WRITE_DATA_LAST);
				goto done;
			}

			if (req->ack_seg == req->comp_seg) {
				qpriv->s_state = TID_OP(WRITE_DATA);
				goto done;
			}

			 
			psn = mask_psn(psn + 1);
			generation = psn >> HFI1_KDETH_BTH_SEQ_SHIFT;
			spsn = 0;

			 
			if (delta_psn(ack_psn, wqe->lpsn))
				wqe = rvt_get_swqe_ptr(qp, qp->s_acked);
			req = wqe_to_tid_req(wqe);
			flow = &req->flows[req->acked_tail];
			 
			fpsn = full_flow_psn(flow, flow->flow_state.spsn);
			req->r_ack_psn = psn;
			 
			if (flow->flow_state.generation !=
			    (resync_psn >> HFI1_KDETH_BTH_SEQ_SHIFT))
				resync_psn = mask_psn(fpsn - 1);
			flow->resync_npkts +=
				delta_psn(mask_psn(resync_psn + 1), fpsn);
			 
			last_acked = qp->s_acked;
			rptr = req;
			while (1) {
				 
				for (fidx = rptr->acked_tail;
				     CIRC_CNT(rptr->setup_head, fidx,
					      MAX_FLOWS);
				     fidx = CIRC_NEXT(fidx, MAX_FLOWS)) {
					u32 lpsn;
					u32 gen;

					flow = &rptr->flows[fidx];
					gen = flow->flow_state.generation;
					if (WARN_ON(gen == generation &&
						    flow->flow_state.spsn !=
						     spsn))
						continue;
					lpsn = flow->flow_state.lpsn;
					lpsn = full_flow_psn(flow, lpsn);
					flow->npkts =
						delta_psn(lpsn,
							  mask_psn(resync_psn)
							  );
					flow->flow_state.generation =
						generation;
					flow->flow_state.spsn = spsn;
					flow->flow_state.lpsn =
						flow->flow_state.spsn +
						flow->npkts - 1;
					flow->pkt = 0;
					spsn += flow->npkts;
					resync_psn += flow->npkts;
					trace_hfi1_tid_flow_rcv_tid_ack(qp,
									fidx,
									flow);
				}
				if (++last_acked == qpriv->s_tid_cur + 1)
					break;
				if (last_acked == qp->s_size)
					last_acked = 0;
				wqe = rvt_get_swqe_ptr(qp, last_acked);
				rptr = wqe_to_tid_req(wqe);
			}
			req->cur_seg = req->ack_seg;
			qpriv->s_tid_tail = qp->s_acked;
			qpriv->s_state = TID_OP(WRITE_REQ);
			hfi1_schedule_tid_send(qp);
		}
done:
		qpriv->s_retry = qp->s_retry_cnt;
		break;

	case 3:          
		hfi1_stop_tid_retry_timer(qp);
		switch ((aeth >> IB_AETH_CREDIT_SHIFT) &
			IB_AETH_CREDIT_MASK) {
		case 0:  
			if (!req->flows)
				break;
			flow = &req->flows[req->acked_tail];
			flpsn = full_flow_psn(flow, flow->flow_state.lpsn);
			if (cmp_psn(psn, flpsn) > 0)
				break;
			trace_hfi1_tid_flow_rcv_tid_ack(qp, req->acked_tail,
							flow);
			req->r_ack_psn = mask_psn(be32_to_cpu(ohdr->bth[2]));
			req->cur_seg = req->ack_seg;
			qpriv->s_tid_tail = qp->s_acked;
			qpriv->s_state = TID_OP(WRITE_REQ);
			qpriv->s_retry = qp->s_retry_cnt;
			hfi1_schedule_tid_send(qp);
			break;

		default:
			break;
		}
		break;

	default:
		break;
	}

ack_op_err:
	spin_unlock_irqrestore(&qp->s_lock, flags);
}

void hfi1_add_tid_retry_timer(struct rvt_qp *qp)
{
	struct hfi1_qp_priv *priv = qp->priv;
	struct ib_qp *ibqp = &qp->ibqp;
	struct rvt_dev_info *rdi = ib_to_rvt(ibqp->device);

	lockdep_assert_held(&qp->s_lock);
	if (!(priv->s_flags & HFI1_S_TID_RETRY_TIMER)) {
		priv->s_flags |= HFI1_S_TID_RETRY_TIMER;
		priv->s_tid_retry_timer.expires = jiffies +
			priv->tid_retry_timeout_jiffies + rdi->busy_jiffies;
		add_timer(&priv->s_tid_retry_timer);
	}
}

static void hfi1_mod_tid_retry_timer(struct rvt_qp *qp)
{
	struct hfi1_qp_priv *priv = qp->priv;
	struct ib_qp *ibqp = &qp->ibqp;
	struct rvt_dev_info *rdi = ib_to_rvt(ibqp->device);

	lockdep_assert_held(&qp->s_lock);
	priv->s_flags |= HFI1_S_TID_RETRY_TIMER;
	mod_timer(&priv->s_tid_retry_timer, jiffies +
		  priv->tid_retry_timeout_jiffies + rdi->busy_jiffies);
}

static int hfi1_stop_tid_retry_timer(struct rvt_qp *qp)
{
	struct hfi1_qp_priv *priv = qp->priv;
	int rval = 0;

	lockdep_assert_held(&qp->s_lock);
	if (priv->s_flags & HFI1_S_TID_RETRY_TIMER) {
		rval = del_timer(&priv->s_tid_retry_timer);
		priv->s_flags &= ~HFI1_S_TID_RETRY_TIMER;
	}
	return rval;
}

void hfi1_del_tid_retry_timer(struct rvt_qp *qp)
{
	struct hfi1_qp_priv *priv = qp->priv;

	del_timer_sync(&priv->s_tid_retry_timer);
	priv->s_flags &= ~HFI1_S_TID_RETRY_TIMER;
}

static void hfi1_tid_retry_timeout(struct timer_list *t)
{
	struct hfi1_qp_priv *priv = from_timer(priv, t, s_tid_retry_timer);
	struct rvt_qp *qp = priv->owner;
	struct rvt_swqe *wqe;
	unsigned long flags;
	struct tid_rdma_request *req;

	spin_lock_irqsave(&qp->r_lock, flags);
	spin_lock(&qp->s_lock);
	trace_hfi1_tid_write_sender_retry_timeout(qp, 0);
	if (priv->s_flags & HFI1_S_TID_RETRY_TIMER) {
		hfi1_stop_tid_retry_timer(qp);
		if (!priv->s_retry) {
			trace_hfi1_msg_tid_retry_timeout( 
				qp,
				"Exhausted retries. Tid retry timeout = ",
				(u64)priv->tid_retry_timeout_jiffies);

			wqe = rvt_get_swqe_ptr(qp, qp->s_acked);
			hfi1_trdma_send_complete(qp, wqe, IB_WC_RETRY_EXC_ERR);
			rvt_error_qp(qp, IB_WC_WR_FLUSH_ERR);
		} else {
			wqe = rvt_get_swqe_ptr(qp, qp->s_acked);
			req = wqe_to_tid_req(wqe);
			trace_hfi1_tid_req_tid_retry_timeout( 
			   qp, 0, wqe->wr.opcode, wqe->psn, wqe->lpsn, req);

			priv->s_flags &= ~RVT_S_WAIT_ACK;
			 
			priv->s_flags |= RVT_S_SEND_ONE;
			 
			qp->s_flags |= HFI1_S_WAIT_HALT;
			priv->s_state = TID_OP(RESYNC);
			priv->s_retry--;
			hfi1_schedule_tid_send(qp);
		}
	}
	spin_unlock(&qp->s_lock);
	spin_unlock_irqrestore(&qp->r_lock, flags);
}

u32 hfi1_build_tid_rdma_resync(struct rvt_qp *qp, struct rvt_swqe *wqe,
			       struct ib_other_headers *ohdr, u32 *bth1,
			       u32 *bth2, u16 fidx)
{
	struct hfi1_qp_priv *qpriv = qp->priv;
	struct tid_rdma_params *remote;
	struct tid_rdma_request *req = wqe_to_tid_req(wqe);
	struct tid_rdma_flow *flow = &req->flows[fidx];
	u32 generation;

	rcu_read_lock();
	remote = rcu_dereference(qpriv->tid_rdma.remote);
	KDETH_RESET(ohdr->u.tid_rdma.ack.kdeth1, JKEY, remote->jkey);
	ohdr->u.tid_rdma.ack.verbs_qp = cpu_to_be32(qp->remote_qpn);
	*bth1 = remote->qp;
	rcu_read_unlock();

	generation = kern_flow_generation_next(flow->flow_state.generation);
	*bth2 = mask_psn((generation << HFI1_KDETH_BTH_SEQ_SHIFT) - 1);
	qpriv->s_resync_psn = *bth2;
	*bth2 |= IB_BTH_REQ_ACK;
	KDETH_RESET(ohdr->u.tid_rdma.ack.kdeth0, KVER, 0x1);

	return sizeof(ohdr->u.tid_rdma.resync) / sizeof(u32);
}

void hfi1_rc_rcv_tid_rdma_resync(struct hfi1_packet *packet)
{
	struct ib_other_headers *ohdr = packet->ohdr;
	struct rvt_qp *qp = packet->qp;
	struct hfi1_qp_priv *qpriv = qp->priv;
	struct hfi1_ctxtdata *rcd = qpriv->rcd;
	struct hfi1_ibdev *dev = to_idev(qp->ibqp.device);
	struct rvt_ack_entry *e;
	struct tid_rdma_request *req;
	struct tid_rdma_flow *flow;
	struct tid_flow_state *fs = &qpriv->flow_state;
	u32 psn, generation, idx, gen_next;
	bool fecn;
	unsigned long flags;

	fecn = process_ecn(qp, packet);
	psn = mask_psn(be32_to_cpu(ohdr->bth[2]));

	generation = mask_psn(psn + 1) >> HFI1_KDETH_BTH_SEQ_SHIFT;
	spin_lock_irqsave(&qp->s_lock, flags);

	gen_next = (fs->generation == KERN_GENERATION_RESERVED) ?
		generation : kern_flow_generation_next(fs->generation);
	 
	if (generation != mask_generation(gen_next - 1) &&
	    generation != gen_next)
		goto bail;
	 
	if (qpriv->resync)
		goto bail;

	spin_lock(&rcd->exp_lock);
	if (fs->index >= RXE_NUM_TID_FLOWS) {
		 
		fs->generation = generation;
	} else {
		 
		rcd->flows[fs->index].generation = generation;
		fs->generation = kern_setup_hw_flow(rcd, fs->index);
	}
	fs->psn = 0;
	 
	qpriv->s_flags &= ~HFI1_R_TID_SW_PSN;
	trace_hfi1_tid_write_rsp_rcv_resync(qp);

	 
	for (idx = qpriv->r_tid_tail; ; idx++) {
		u16 flow_idx;

		if (idx > rvt_size_atomic(&dev->rdi))
			idx = 0;
		e = &qp->s_ack_queue[idx];
		if (e->opcode == TID_OP(WRITE_REQ)) {
			req = ack_to_tid_req(e);
			trace_hfi1_tid_req_rcv_resync(qp, 0, e->opcode, e->psn,
						      e->lpsn, req);

			 
			for (flow_idx = req->clear_tail;
			     CIRC_CNT(req->setup_head, flow_idx,
				      MAX_FLOWS);
			     flow_idx = CIRC_NEXT(flow_idx, MAX_FLOWS)) {
				u32 lpsn;
				u32 next;

				flow = &req->flows[flow_idx];
				lpsn = full_flow_psn(flow,
						     flow->flow_state.lpsn);
				next = flow->flow_state.r_next_psn;
				flow->npkts = delta_psn(lpsn, next - 1);
				flow->flow_state.generation = fs->generation;
				flow->flow_state.spsn = fs->psn;
				flow->flow_state.lpsn =
					flow->flow_state.spsn + flow->npkts - 1;
				flow->flow_state.r_next_psn =
					full_flow_psn(flow,
						      flow->flow_state.spsn);
				fs->psn += flow->npkts;
				trace_hfi1_tid_flow_rcv_resync(qp, flow_idx,
							       flow);
			}
		}
		if (idx == qp->s_tail_ack_queue)
			break;
	}

	spin_unlock(&rcd->exp_lock);
	qpriv->resync = true;
	 
	qpriv->s_nak_state = 0;
	tid_rdma_trigger_ack(qp);
bail:
	if (fecn)
		qp->s_flags |= RVT_S_ECN;
	spin_unlock_irqrestore(&qp->s_lock, flags);
}

 
static void update_tid_tail(struct rvt_qp *qp)
	__must_hold(&qp->s_lock)
{
	struct hfi1_qp_priv *priv = qp->priv;
	u32 i;
	struct rvt_swqe *wqe;

	lockdep_assert_held(&qp->s_lock);
	 
	if (priv->s_tid_tail == priv->s_tid_cur)
		return;
	for (i = priv->s_tid_tail + 1; ; i++) {
		if (i == qp->s_size)
			i = 0;

		if (i == priv->s_tid_cur)
			break;
		wqe = rvt_get_swqe_ptr(qp, i);
		if (wqe->wr.opcode == IB_WR_TID_RDMA_WRITE)
			break;
	}
	priv->s_tid_tail = i;
	priv->s_state = TID_OP(WRITE_RESP);
}

int hfi1_make_tid_rdma_pkt(struct rvt_qp *qp, struct hfi1_pkt_state *ps)
	__must_hold(&qp->s_lock)
{
	struct hfi1_qp_priv *priv = qp->priv;
	struct rvt_swqe *wqe;
	u32 bth1 = 0, bth2 = 0, hwords = 5, len, middle = 0;
	struct ib_other_headers *ohdr;
	struct rvt_sge_state *ss = &qp->s_sge;
	struct rvt_ack_entry *e = &qp->s_ack_queue[qp->s_tail_ack_queue];
	struct tid_rdma_request *req = ack_to_tid_req(e);
	bool last = false;
	u8 opcode = TID_OP(WRITE_DATA);

	lockdep_assert_held(&qp->s_lock);
	trace_hfi1_tid_write_sender_make_tid_pkt(qp, 0);
	 
	if (((atomic_read(&priv->n_tid_requests) < HFI1_TID_RDMA_WRITE_CNT) &&
	     atomic_read(&priv->n_requests) &&
	     !(qp->s_flags & (RVT_S_BUSY | RVT_S_WAIT_ACK |
			     HFI1_S_ANY_WAIT_IO))) ||
	    (e->opcode == TID_OP(WRITE_REQ) && req->cur_seg < req->alloc_seg &&
	     !(qp->s_flags & (RVT_S_BUSY | HFI1_S_ANY_WAIT_IO)))) {
		struct iowait_work *iowork;

		iowork = iowait_get_ib_work(&priv->s_iowait);
		ps->s_txreq = get_waiting_verbs_txreq(iowork);
		if (ps->s_txreq || hfi1_make_rc_req(qp, ps)) {
			priv->s_flags |= HFI1_S_TID_BUSY_SET;
			return 1;
		}
	}

	ps->s_txreq = get_txreq(ps->dev, qp);
	if (!ps->s_txreq)
		goto bail_no_tx;

	ohdr = &ps->s_txreq->phdr.hdr.ibh.u.oth;

	if ((priv->s_flags & RVT_S_ACK_PENDING) &&
	    make_tid_rdma_ack(qp, ohdr, ps))
		return 1;

	 
	if (!(ib_rvt_state_ops[qp->state] & RVT_PROCESS_SEND_OK))
		goto bail;

	if (priv->s_flags & RVT_S_WAIT_ACK)
		goto bail;

	 
	if (priv->s_tid_tail == HFI1_QP_WQE_INVALID)
		goto bail;
	wqe = rvt_get_swqe_ptr(qp, priv->s_tid_tail);
	req = wqe_to_tid_req(wqe);
	trace_hfi1_tid_req_make_tid_pkt(qp, 0, wqe->wr.opcode, wqe->psn,
					wqe->lpsn, req);
	switch (priv->s_state) {
	case TID_OP(WRITE_REQ):
	case TID_OP(WRITE_RESP):
		priv->tid_ss.sge = wqe->sg_list[0];
		priv->tid_ss.sg_list = wqe->sg_list + 1;
		priv->tid_ss.num_sge = wqe->wr.num_sge;
		priv->tid_ss.total_len = wqe->length;

		if (priv->s_state == TID_OP(WRITE_REQ))
			hfi1_tid_rdma_restart_req(qp, wqe, &bth2);
		priv->s_state = TID_OP(WRITE_DATA);
		fallthrough;

	case TID_OP(WRITE_DATA):
		 
		trace_hfi1_sender_make_tid_pkt(qp);
		trace_hfi1_tid_write_sender_make_tid_pkt(qp, 0);
		wqe = rvt_get_swqe_ptr(qp, priv->s_tid_tail);
		req = wqe_to_tid_req(wqe);
		len = wqe->length;

		if (!req->comp_seg || req->cur_seg == req->comp_seg)
			goto bail;

		trace_hfi1_tid_req_make_tid_pkt(qp, 0, wqe->wr.opcode,
						wqe->psn, wqe->lpsn, req);
		last = hfi1_build_tid_rdma_packet(wqe, ohdr, &bth1, &bth2,
						  &len);

		if (last) {
			 
			req->clear_tail = CIRC_NEXT(req->clear_tail,
						    MAX_FLOWS);
			if (++req->cur_seg < req->total_segs) {
				if (!CIRC_CNT(req->setup_head, req->clear_tail,
					      MAX_FLOWS))
					qp->s_flags |= HFI1_S_WAIT_TID_RESP;
			} else {
				priv->s_state = TID_OP(WRITE_DATA_LAST);
				opcode = TID_OP(WRITE_DATA_LAST);

				 
				update_tid_tail(qp);
			}
		}
		hwords += sizeof(ohdr->u.tid_rdma.w_data) / sizeof(u32);
		ss = &priv->tid_ss;
		break;

	case TID_OP(RESYNC):
		trace_hfi1_sender_make_tid_pkt(qp);
		 
		wqe = rvt_get_swqe_ptr(qp, priv->s_tid_cur);
		req = wqe_to_tid_req(wqe);
		 
		if (!req->comp_seg) {
			wqe = rvt_get_swqe_ptr(qp,
					       (!priv->s_tid_cur ? qp->s_size :
						priv->s_tid_cur) - 1);
			req = wqe_to_tid_req(wqe);
		}
		hwords += hfi1_build_tid_rdma_resync(qp, wqe, ohdr, &bth1,
						     &bth2,
						     CIRC_PREV(req->setup_head,
							       MAX_FLOWS));
		ss = NULL;
		len = 0;
		opcode = TID_OP(RESYNC);
		break;

	default:
		goto bail;
	}
	if (priv->s_flags & RVT_S_SEND_ONE) {
		priv->s_flags &= ~RVT_S_SEND_ONE;
		priv->s_flags |= RVT_S_WAIT_ACK;
		bth2 |= IB_BTH_REQ_ACK;
	}
	qp->s_len -= len;
	ps->s_txreq->hdr_dwords = hwords;
	ps->s_txreq->sde = priv->s_sde;
	ps->s_txreq->ss = ss;
	ps->s_txreq->s_cur_size = len;
	hfi1_make_ruc_header(qp, ohdr, (opcode << 24), bth1, bth2,
			     middle, ps);
	return 1;
bail:
	hfi1_put_txreq(ps->s_txreq);
bail_no_tx:
	ps->s_txreq = NULL;
	priv->s_flags &= ~RVT_S_BUSY;
	 
	iowait_set_flag(&priv->s_iowait, IOWAIT_PENDING_TID);
	return 0;
}

static int make_tid_rdma_ack(struct rvt_qp *qp,
			     struct ib_other_headers *ohdr,
			     struct hfi1_pkt_state *ps)
{
	struct rvt_ack_entry *e;
	struct hfi1_qp_priv *qpriv = qp->priv;
	struct hfi1_ibdev *dev = to_idev(qp->ibqp.device);
	u32 hwords, next;
	u32 len = 0;
	u32 bth1 = 0, bth2 = 0;
	int middle = 0;
	u16 flow;
	struct tid_rdma_request *req, *nreq;

	trace_hfi1_tid_write_rsp_make_tid_ack(qp);
	 
	if (!(ib_rvt_state_ops[qp->state] & RVT_PROCESS_RECV_OK))
		goto bail;

	 
	hwords = 5;

	e = &qp->s_ack_queue[qpriv->r_tid_ack];
	req = ack_to_tid_req(e);
	 
	if (qpriv->resync) {
		if (!req->ack_seg || req->ack_seg == req->total_segs)
			qpriv->r_tid_ack = !qpriv->r_tid_ack ?
				rvt_size_atomic(&dev->rdi) :
				qpriv->r_tid_ack - 1;
		e = &qp->s_ack_queue[qpriv->r_tid_ack];
		req = ack_to_tid_req(e);
	}

	trace_hfi1_rsp_make_tid_ack(qp, e->psn);
	trace_hfi1_tid_req_make_tid_ack(qp, 0, e->opcode, e->psn, e->lpsn,
					req);
	 
	if (!qpriv->s_nak_state && !qpriv->resync &&
	    req->ack_seg == req->comp_seg)
		goto bail;

	do {
		 
		req->ack_seg +=
			 
			CIRC_CNT(req->clear_tail, req->acked_tail,
				 MAX_FLOWS);
		 
		req->acked_tail = req->clear_tail;

		 
		flow = CIRC_PREV(req->acked_tail, MAX_FLOWS);
		if (req->ack_seg != req->total_segs)
			break;
		req->state = TID_REQUEST_COMPLETE;

		next = qpriv->r_tid_ack + 1;
		if (next > rvt_size_atomic(&dev->rdi))
			next = 0;
		qpriv->r_tid_ack = next;
		if (qp->s_ack_queue[next].opcode != TID_OP(WRITE_REQ))
			break;
		nreq = ack_to_tid_req(&qp->s_ack_queue[next]);
		if (!nreq->comp_seg || nreq->ack_seg == nreq->comp_seg)
			break;

		 
		e = &qp->s_ack_queue[qpriv->r_tid_ack];
		req = ack_to_tid_req(e);
	} while (1);

	 
	if (qpriv->s_nak_state ||
	    (qpriv->resync &&
	     !hfi1_tid_rdma_is_resync_psn(qpriv->r_next_psn_kdeth - 1) &&
	     (cmp_psn(qpriv->r_next_psn_kdeth - 1,
		      full_flow_psn(&req->flows[flow],
				    req->flows[flow].flow_state.lpsn)) > 0))) {
		 
		e = &qp->s_ack_queue[qpriv->r_tid_ack];
		req = ack_to_tid_req(e);
		flow = req->acked_tail;
	} else if (req->ack_seg == req->total_segs &&
		   qpriv->s_flags & HFI1_R_TID_WAIT_INTERLCK)
		qpriv->s_flags &= ~HFI1_R_TID_WAIT_INTERLCK;

	trace_hfi1_tid_write_rsp_make_tid_ack(qp);
	trace_hfi1_tid_req_make_tid_ack(qp, 0, e->opcode, e->psn, e->lpsn,
					req);
	hwords += hfi1_build_tid_rdma_write_ack(qp, e, ohdr, flow, &bth1,
						&bth2);
	len = 0;
	qpriv->s_flags &= ~RVT_S_ACK_PENDING;
	ps->s_txreq->hdr_dwords = hwords;
	ps->s_txreq->sde = qpriv->s_sde;
	ps->s_txreq->s_cur_size = len;
	ps->s_txreq->ss = NULL;
	hfi1_make_ruc_header(qp, ohdr, (TID_OP(ACK) << 24), bth1, bth2, middle,
			     ps);
	ps->s_txreq->txreq.flags |= SDMA_TXREQ_F_VIP;
	return 1;
bail:
	 
	smp_wmb();
	qpriv->s_flags &= ~RVT_S_ACK_PENDING;
	return 0;
}

static int hfi1_send_tid_ok(struct rvt_qp *qp)
{
	struct hfi1_qp_priv *priv = qp->priv;

	return !(priv->s_flags & RVT_S_BUSY ||
		 qp->s_flags & HFI1_S_ANY_WAIT_IO) &&
		(verbs_txreq_queued(iowait_get_tid_work(&priv->s_iowait)) ||
		 (priv->s_flags & RVT_S_RESP_PENDING) ||
		 !(qp->s_flags & HFI1_S_ANY_TID_WAIT_SEND));
}

void _hfi1_do_tid_send(struct work_struct *work)
{
	struct iowait_work *w = container_of(work, struct iowait_work, iowork);
	struct rvt_qp *qp = iowait_to_qp(w->iow);

	hfi1_do_tid_send(qp);
}

static void hfi1_do_tid_send(struct rvt_qp *qp)
{
	struct hfi1_pkt_state ps;
	struct hfi1_qp_priv *priv = qp->priv;

	ps.dev = to_idev(qp->ibqp.device);
	ps.ibp = to_iport(qp->ibqp.device, qp->port_num);
	ps.ppd = ppd_from_ibp(ps.ibp);
	ps.wait = iowait_get_tid_work(&priv->s_iowait);
	ps.in_thread = false;
	ps.timeout_int = qp->timeout_jiffies / 8;

	trace_hfi1_rc_do_tid_send(qp, false);
	spin_lock_irqsave(&qp->s_lock, ps.flags);

	 
	if (!hfi1_send_tid_ok(qp)) {
		if (qp->s_flags & HFI1_S_ANY_WAIT_IO)
			iowait_set_flag(&priv->s_iowait, IOWAIT_PENDING_TID);
		spin_unlock_irqrestore(&qp->s_lock, ps.flags);
		return;
	}

	priv->s_flags |= RVT_S_BUSY;

	ps.timeout = jiffies + ps.timeout_int;
	ps.cpu = priv->s_sde ? priv->s_sde->cpu :
		cpumask_first(cpumask_of_node(ps.ppd->dd->node));
	ps.pkts_sent = false;

	 
	ps.s_txreq = get_waiting_verbs_txreq(ps.wait);
	do {
		 
		if (ps.s_txreq) {
			if (priv->s_flags & HFI1_S_TID_BUSY_SET) {
				qp->s_flags |= RVT_S_BUSY;
				ps.wait = iowait_get_ib_work(&priv->s_iowait);
			}
			spin_unlock_irqrestore(&qp->s_lock, ps.flags);

			 
			if (hfi1_verbs_send(qp, &ps))
				return;

			 
			if (hfi1_schedule_send_yield(qp, &ps, true))
				return;

			spin_lock_irqsave(&qp->s_lock, ps.flags);
			if (priv->s_flags & HFI1_S_TID_BUSY_SET) {
				qp->s_flags &= ~RVT_S_BUSY;
				priv->s_flags &= ~HFI1_S_TID_BUSY_SET;
				ps.wait = iowait_get_tid_work(&priv->s_iowait);
				if (iowait_flag_set(&priv->s_iowait,
						    IOWAIT_PENDING_IB))
					hfi1_schedule_send(qp);
			}
		}
	} while (hfi1_make_tid_rdma_pkt(qp, &ps));
	iowait_starve_clear(ps.pkts_sent, &priv->s_iowait);
	spin_unlock_irqrestore(&qp->s_lock, ps.flags);
}

static bool _hfi1_schedule_tid_send(struct rvt_qp *qp)
{
	struct hfi1_qp_priv *priv = qp->priv;
	struct hfi1_ibport *ibp =
		to_iport(qp->ibqp.device, qp->port_num);
	struct hfi1_pportdata *ppd = ppd_from_ibp(ibp);
	struct hfi1_devdata *dd = ppd->dd;

	if ((dd->flags & HFI1_SHUTDOWN))
		return true;

	return iowait_tid_schedule(&priv->s_iowait, ppd->hfi1_wq,
				   priv->s_sde ?
				   priv->s_sde->cpu :
				   cpumask_first(cpumask_of_node(dd->node)));
}

 
bool hfi1_schedule_tid_send(struct rvt_qp *qp)
{
	lockdep_assert_held(&qp->s_lock);
	if (hfi1_send_tid_ok(qp)) {
		 
		_hfi1_schedule_tid_send(qp);
		return true;
	}
	if (qp->s_flags & HFI1_S_ANY_WAIT_IO)
		iowait_set_flag(&((struct hfi1_qp_priv *)qp->priv)->s_iowait,
				IOWAIT_PENDING_TID);
	return false;
}

bool hfi1_tid_rdma_ack_interlock(struct rvt_qp *qp, struct rvt_ack_entry *e)
{
	struct rvt_ack_entry *prev;
	struct tid_rdma_request *req;
	struct hfi1_ibdev *dev = to_idev(qp->ibqp.device);
	struct hfi1_qp_priv *priv = qp->priv;
	u32 s_prev;

	s_prev = qp->s_tail_ack_queue == 0 ? rvt_size_atomic(&dev->rdi) :
		(qp->s_tail_ack_queue - 1);
	prev = &qp->s_ack_queue[s_prev];

	if ((e->opcode == TID_OP(READ_REQ) ||
	     e->opcode == OP(RDMA_READ_REQUEST)) &&
	    prev->opcode == TID_OP(WRITE_REQ)) {
		req = ack_to_tid_req(prev);
		if (req->ack_seg != req->total_segs) {
			priv->s_flags |= HFI1_R_TID_WAIT_INTERLCK;
			return true;
		}
	}
	return false;
}

static u32 read_r_next_psn(struct hfi1_devdata *dd, u8 ctxt, u8 fidx)
{
	u64 reg;

	 
	reg = read_uctxt_csr(dd, ctxt, RCV_TID_FLOW_TABLE + (8 * fidx));
	return mask_psn(reg);
}

static void tid_rdma_rcv_err(struct hfi1_packet *packet,
			     struct ib_other_headers *ohdr,
			     struct rvt_qp *qp, u32 psn, int diff, bool fecn)
{
	unsigned long flags;

	tid_rdma_rcv_error(packet, ohdr, qp, psn, diff);
	if (fecn) {
		spin_lock_irqsave(&qp->s_lock, flags);
		qp->s_flags |= RVT_S_ECN;
		spin_unlock_irqrestore(&qp->s_lock, flags);
	}
}

static void update_r_next_psn_fecn(struct hfi1_packet *packet,
				   struct hfi1_qp_priv *priv,
				   struct hfi1_ctxtdata *rcd,
				   struct tid_rdma_flow *flow,
				   bool fecn)
{
	 
	if (fecn && packet->etype == RHF_RCV_TYPE_EAGER &&
	    !(priv->s_flags & HFI1_R_TID_SW_PSN)) {
		struct hfi1_devdata *dd = rcd->dd;

		flow->flow_state.r_next_psn =
			read_r_next_psn(dd, rcd->ctxt, flow->idx);
	}
}
