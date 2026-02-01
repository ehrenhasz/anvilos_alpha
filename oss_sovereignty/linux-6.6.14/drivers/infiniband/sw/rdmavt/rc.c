
 

#include <rdma/rdmavt_qp.h>
#include <rdma/ib_hdrs.h>

 
static const u16 credit_table[31] = {
	0,                       
	1,                       
	2,                       
	3,                       
	4,                       
	6,                       
	8,                       
	12,                      
	16,                      
	24,                      
	32,                      
	48,                      
	64,                      
	96,                      
	128,                     
	192,                     
	256,                     
	384,                     
	512,                     
	768,                     
	1024,                    
	1536,                    
	2048,                    
	3072,                    
	4096,                    
	6144,                    
	8192,                    
	12288,                   
	16384,                   
	24576,                   
	32768                    
};

 
__be32 rvt_compute_aeth(struct rvt_qp *qp)
{
	u32 aeth = qp->r_msn & IB_MSN_MASK;

	if (qp->ibqp.srq) {
		 
		aeth |= IB_AETH_CREDIT_INVAL << IB_AETH_CREDIT_SHIFT;
	} else {
		u32 min, max, x;
		u32 credits;
		u32 head;
		u32 tail;

		credits = READ_ONCE(qp->r_rq.kwq->count);
		if (credits == 0) {
			 
			if (qp->ip) {
				head = RDMA_READ_UAPI_ATOMIC(qp->r_rq.wq->head);
				tail = RDMA_READ_UAPI_ATOMIC(qp->r_rq.wq->tail);
			} else {
				head = READ_ONCE(qp->r_rq.kwq->head);
				tail = READ_ONCE(qp->r_rq.kwq->tail);
			}
			if (head >= qp->r_rq.size)
				head = 0;
			if (tail >= qp->r_rq.size)
				tail = 0;
			 
			credits = rvt_get_rq_count(&qp->r_rq, head, tail);
		}
		 
		min = 0;
		max = 31;
		for (;;) {
			x = (min + max) / 2;
			if (credit_table[x] == credits)
				break;
			if (credit_table[x] > credits) {
				max = x;
			} else {
				if (min == x)
					break;
				min = x;
			}
		}
		aeth |= x << IB_AETH_CREDIT_SHIFT;
	}
	return cpu_to_be32(aeth);
}
EXPORT_SYMBOL(rvt_compute_aeth);

 
void rvt_get_credit(struct rvt_qp *qp, u32 aeth)
{
	struct rvt_dev_info *rdi = ib_to_rvt(qp->ibqp.device);
	u32 credit = (aeth >> IB_AETH_CREDIT_SHIFT) & IB_AETH_CREDIT_MASK;

	lockdep_assert_held(&qp->s_lock);
	 
	if (credit == IB_AETH_CREDIT_INVAL) {
		if (!(qp->s_flags & RVT_S_UNLIMITED_CREDIT)) {
			qp->s_flags |= RVT_S_UNLIMITED_CREDIT;
			if (qp->s_flags & RVT_S_WAIT_SSN_CREDIT) {
				qp->s_flags &= ~RVT_S_WAIT_SSN_CREDIT;
				rdi->driver_f.schedule_send(qp);
			}
		}
	} else if (!(qp->s_flags & RVT_S_UNLIMITED_CREDIT)) {
		 
		credit = (aeth + credit_table[credit]) & IB_MSN_MASK;
		if (rvt_cmp_msn(credit, qp->s_lsn) > 0) {
			qp->s_lsn = credit;
			if (qp->s_flags & RVT_S_WAIT_SSN_CREDIT) {
				qp->s_flags &= ~RVT_S_WAIT_SSN_CREDIT;
				rdi->driver_f.schedule_send(qp);
			}
		}
	}
}
EXPORT_SYMBOL(rvt_get_credit);

 
u32 rvt_restart_sge(struct rvt_sge_state *ss, struct rvt_swqe *wqe, u32 len)
{
	ss->sge = wqe->sg_list[0];
	ss->sg_list = wqe->sg_list + 1;
	ss->num_sge = wqe->wr.num_sge;
	ss->total_len = wqe->length;
	rvt_skip_sge(ss, len, false);
	return wqe->length - len;
}
EXPORT_SYMBOL(rvt_restart_sge);

