 
 
#ifndef HFI1_TID_RDMA_H
#define HFI1_TID_RDMA_H

#include <linux/circ_buf.h>
#include "common.h"

 
#define CIRC_ADD(val, add, size) (((val) + (add)) & ((size) - 1))
#define CIRC_NEXT(val, size) CIRC_ADD(val, 1, size)
#define CIRC_PREV(val, size) CIRC_ADD(val, -1, size)

#define TID_RDMA_MIN_SEGMENT_SIZE       BIT(18)    
#define TID_RDMA_MAX_SEGMENT_SIZE       BIT(18)    
#define TID_RDMA_MAX_PAGES              (BIT(18) >> PAGE_SHIFT)
#define TID_RDMA_SEGMENT_SHIFT		18

 
#define HFI1_S_TID_BUSY_SET       BIT(0)
 
#define HFI1_R_TID_RSC_TIMER      BIT(2)
 
 
#define HFI1_S_TID_WAIT_INTERLCK  BIT(5)
#define HFI1_R_TID_WAIT_INTERLCK  BIT(6)
 
 
#define HFI1_S_TID_RETRY_TIMER    BIT(17)
 
#define HFI1_R_TID_SW_PSN         BIT(19)
 
 
 

 
#define HFI1_TID_RDMA_WRITE_CNT 8

struct tid_rdma_params {
	struct rcu_head rcu_head;
	u32 qp;
	u32 max_len;
	u16 jkey;
	u8 max_read;
	u8 max_write;
	u8 timeout;
	u8 urg;
	u8 version;
};

struct tid_rdma_qp_params {
	struct work_struct trigger_work;
	struct tid_rdma_params local;
	struct tid_rdma_params __rcu *remote;
};

 
struct tid_flow_state {
	u32 generation;
	u32 psn;
	u8 index;
	u8 last_index;
};

enum tid_rdma_req_state {
	TID_REQUEST_INACTIVE = 0,
	TID_REQUEST_INIT,
	TID_REQUEST_INIT_RESEND,
	TID_REQUEST_ACTIVE,
	TID_REQUEST_RESEND,
	TID_REQUEST_RESEND_ACTIVE,
	TID_REQUEST_QUEUED,
	TID_REQUEST_SYNC,
	TID_REQUEST_RNR_NAK,
	TID_REQUEST_COMPLETE,
};

struct tid_rdma_request {
	struct rvt_qp *qp;
	struct hfi1_ctxtdata *rcd;
	union {
		struct rvt_swqe *swqe;
		struct rvt_ack_entry *ack;
	} e;

	struct tid_rdma_flow *flows;	 
	struct rvt_sge_state ss;  
	u16 n_flows;		 
	u16 setup_head;		 
	u16 clear_tail;		 
	u16 flow_idx;		 
	u16 acked_tail;

	u32 seg_len;
	u32 total_len;
	u32 r_ack_psn;           
	u32 r_flow_psn;          
	u32 r_last_acked;        
	u32 s_next_psn;		 

	u32 total_segs;		 
	u32 cur_seg;		 
	u32 comp_seg;            
	u32 ack_seg;             
	u32 alloc_seg;           
	u32 isge;		 
	u32 ack_pending;         

	enum tid_rdma_req_state state;
};

 
struct flow_state {
	u32 flags;
	u32 resp_ib_psn;      
	u32 generation;       
	u32 spsn;             
	u32 lpsn;             
	u32 r_next_psn;       

	 
	u32 ib_spsn;          
	u32 ib_lpsn;          
};

struct tid_rdma_pageset {
	dma_addr_t addr : 48;  
	u8 idx: 8;
	u8 count : 7;
	u8 mapped: 1;
};

 
struct kern_tid_node {
	struct tid_group *grp;
	u8 map;
	u8 cnt;
};

 
struct tid_rdma_flow {
	 
	struct flow_state flow_state;
	struct tid_rdma_request *req;
	u32 tid_qpn;
	u32 tid_offset;
	u32 length;
	u32 sent;
	u8 tnode_cnt;
	u8 tidcnt;
	u8 tid_idx;
	u8 idx;
	u8 npagesets;
	u8 npkts;
	u8 pkt;
	u8 resync_npkts;
	struct kern_tid_node tnode[TID_RDMA_MAX_PAGES];
	struct tid_rdma_pageset pagesets[TID_RDMA_MAX_PAGES];
	u32 tid_entry[TID_RDMA_MAX_PAGES];
};

enum tid_rnr_nak_state {
	TID_RNR_NAK_INIT = 0,
	TID_RNR_NAK_SEND,
	TID_RNR_NAK_SENT,
};

bool tid_rdma_conn_req(struct rvt_qp *qp, u64 *data);
bool tid_rdma_conn_reply(struct rvt_qp *qp, u64 data);
bool tid_rdma_conn_resp(struct rvt_qp *qp, u64 *data);
void tid_rdma_conn_error(struct rvt_qp *qp);
void tid_rdma_opfn_init(struct rvt_qp *qp, struct tid_rdma_params *p);

int hfi1_kern_exp_rcv_init(struct hfi1_ctxtdata *rcd, int reinit);
int hfi1_kern_exp_rcv_setup(struct tid_rdma_request *req,
			    struct rvt_sge_state *ss, bool *last);
int hfi1_kern_exp_rcv_clear(struct tid_rdma_request *req);
void hfi1_kern_exp_rcv_clear_all(struct tid_rdma_request *req);
void __trdma_clean_swqe(struct rvt_qp *qp, struct rvt_swqe *wqe);

 
static inline void trdma_clean_swqe(struct rvt_qp *qp, struct rvt_swqe *wqe)
{
	if (!wqe->priv)
		return;
	__trdma_clean_swqe(qp, wqe);
}

void hfi1_kern_read_tid_flow_free(struct rvt_qp *qp);

int hfi1_qp_priv_init(struct rvt_dev_info *rdi, struct rvt_qp *qp,
		      struct ib_qp_init_attr *init_attr);
void hfi1_qp_priv_tid_free(struct rvt_dev_info *rdi, struct rvt_qp *qp);

void hfi1_tid_rdma_flush_wait(struct rvt_qp *qp);

int hfi1_kern_setup_hw_flow(struct hfi1_ctxtdata *rcd, struct rvt_qp *qp);
void hfi1_kern_clear_hw_flow(struct hfi1_ctxtdata *rcd, struct rvt_qp *qp);
void hfi1_kern_init_ctxt_generations(struct hfi1_ctxtdata *rcd);

struct cntr_entry;
u64 hfi1_access_sw_tid_wait(const struct cntr_entry *entry,
			    void *context, int vl, int mode, u64 data);

u32 hfi1_build_tid_rdma_read_packet(struct rvt_swqe *wqe,
				    struct ib_other_headers *ohdr,
				    u32 *bth1, u32 *bth2, u32 *len);
u32 hfi1_build_tid_rdma_read_req(struct rvt_qp *qp, struct rvt_swqe *wqe,
				 struct ib_other_headers *ohdr, u32 *bth1,
				 u32 *bth2, u32 *len);
void hfi1_rc_rcv_tid_rdma_read_req(struct hfi1_packet *packet);
u32 hfi1_build_tid_rdma_read_resp(struct rvt_qp *qp, struct rvt_ack_entry *e,
				  struct ib_other_headers *ohdr, u32 *bth0,
				  u32 *bth1, u32 *bth2, u32 *len, bool *last);
void hfi1_rc_rcv_tid_rdma_read_resp(struct hfi1_packet *packet);
bool hfi1_handle_kdeth_eflags(struct hfi1_ctxtdata *rcd,
			      struct hfi1_pportdata *ppd,
			      struct hfi1_packet *packet);
void hfi1_tid_rdma_restart_req(struct rvt_qp *qp, struct rvt_swqe *wqe,
			       u32 *bth2);
void hfi1_qp_kern_exp_rcv_clear_all(struct rvt_qp *qp);
bool hfi1_tid_rdma_wqe_interlock(struct rvt_qp *qp, struct rvt_swqe *wqe);

void setup_tid_rdma_wqe(struct rvt_qp *qp, struct rvt_swqe *wqe);
static inline void hfi1_setup_tid_rdma_wqe(struct rvt_qp *qp,
					   struct rvt_swqe *wqe)
{
	if (wqe->priv &&
	    (wqe->wr.opcode == IB_WR_RDMA_READ ||
	     wqe->wr.opcode == IB_WR_RDMA_WRITE) &&
	    wqe->length >= TID_RDMA_MIN_SEGMENT_SIZE)
		setup_tid_rdma_wqe(qp, wqe);
}

u32 hfi1_build_tid_rdma_write_req(struct rvt_qp *qp, struct rvt_swqe *wqe,
				  struct ib_other_headers *ohdr,
				  u32 *bth1, u32 *bth2, u32 *len);

void hfi1_rc_rcv_tid_rdma_write_req(struct hfi1_packet *packet);

u32 hfi1_build_tid_rdma_write_resp(struct rvt_qp *qp, struct rvt_ack_entry *e,
				   struct ib_other_headers *ohdr, u32 *bth1,
				   u32 bth2, u32 *len,
				   struct rvt_sge_state **ss);

void hfi1_del_tid_reap_timer(struct rvt_qp *qp);

void hfi1_rc_rcv_tid_rdma_write_resp(struct hfi1_packet *packet);

bool hfi1_build_tid_rdma_packet(struct rvt_swqe *wqe,
				struct ib_other_headers *ohdr,
				u32 *bth1, u32 *bth2, u32 *len);

void hfi1_rc_rcv_tid_rdma_write_data(struct hfi1_packet *packet);

u32 hfi1_build_tid_rdma_write_ack(struct rvt_qp *qp, struct rvt_ack_entry *e,
				  struct ib_other_headers *ohdr, u16 iflow,
				  u32 *bth1, u32 *bth2);

void hfi1_rc_rcv_tid_rdma_ack(struct hfi1_packet *packet);

void hfi1_add_tid_retry_timer(struct rvt_qp *qp);
void hfi1_del_tid_retry_timer(struct rvt_qp *qp);

u32 hfi1_build_tid_rdma_resync(struct rvt_qp *qp, struct rvt_swqe *wqe,
			       struct ib_other_headers *ohdr, u32 *bth1,
			       u32 *bth2, u16 fidx);

void hfi1_rc_rcv_tid_rdma_resync(struct hfi1_packet *packet);

struct hfi1_pkt_state;
int hfi1_make_tid_rdma_pkt(struct rvt_qp *qp, struct hfi1_pkt_state *ps);

void _hfi1_do_tid_send(struct work_struct *work);

bool hfi1_schedule_tid_send(struct rvt_qp *qp);

bool hfi1_tid_rdma_ack_interlock(struct rvt_qp *qp, struct rvt_ack_entry *e);

#endif  
