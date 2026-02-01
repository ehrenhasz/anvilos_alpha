 
 
#ifndef _HFI1_OPFN_H
#define _HFI1_OPFN_H

 

#include <linux/workqueue.h>
#include <rdma/ib_verbs.h>
#include <rdma/rdmavt_qp.h>

 
#define IB_BTHE_E_SHIFT           24
#define HFI1_VERBS_E_ATOMIC_VADDR U64_MAX

enum hfi1_opfn_codes {
	STL_VERBS_EXTD_NONE = 0,
	STL_VERBS_EXTD_TID_RDMA,
	STL_VERBS_EXTD_MAX
};

struct hfi1_opfn_data {
	u8 extended;
	u16 requested;
	u16 completed;
	enum hfi1_opfn_codes curr;
	 
	spinlock_t lock;
	struct work_struct opfn_work;
};

 
#define IB_WR_OPFN IB_WR_RESERVED3

void opfn_send_conn_request(struct work_struct *work);
void opfn_conn_response(struct rvt_qp *qp, struct rvt_ack_entry *e,
			struct ib_atomic_eth *ateth);
void opfn_conn_reply(struct rvt_qp *qp, u64 data);
void opfn_conn_error(struct rvt_qp *qp);
void opfn_qp_init(struct rvt_qp *qp, struct ib_qp_attr *attr, int attr_mask);
void opfn_trigger_conn_request(struct rvt_qp *qp, u32 bth1);
int opfn_init(void);
void opfn_exit(void);

#endif  
