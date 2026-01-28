


#ifndef _QED_SP_H
#define _QED_SP_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/qed/qed_chain.h>
#include "qed.h"
#include "qed_hsi.h"

enum spq_mode {
	QED_SPQ_MODE_BLOCK,     
	QED_SPQ_MODE_CB,        
	QED_SPQ_MODE_EBLOCK,    
};

struct qed_spq_comp_cb {
	void	(*function)(struct qed_hwfn *p_hwfn,
			    void *cookie,
			    union event_ring_data *data,
			    u8 fw_return_code);
	void	*cookie;
};


int qed_eth_cqe_completion(struct qed_hwfn *p_hwfn,
			   struct eth_slow_path_rx_cqe *cqe);

 
union ramrod_data {
	struct pf_start_ramrod_data pf_start;
	struct pf_update_ramrod_data pf_update;
	struct rx_queue_start_ramrod_data rx_queue_start;
	struct rx_queue_update_ramrod_data rx_queue_update;
	struct rx_queue_stop_ramrod_data rx_queue_stop;
	struct tx_queue_start_ramrod_data tx_queue_start;
	struct tx_queue_stop_ramrod_data tx_queue_stop;
	struct vport_start_ramrod_data vport_start;
	struct vport_stop_ramrod_data vport_stop;
	struct rx_update_gft_filter_ramrod_data rx_update_gft;
	struct vport_update_ramrod_data vport_update;
	struct core_rx_start_ramrod_data core_rx_queue_start;
	struct core_rx_stop_ramrod_data core_rx_queue_stop;
	struct core_tx_start_ramrod_data core_tx_queue_start;
	struct core_tx_stop_ramrod_data core_tx_queue_stop;
	struct vport_filter_update_ramrod_data vport_filter_update;

	struct rdma_init_func_ramrod_data rdma_init_func;
	struct rdma_close_func_ramrod_data rdma_close_func;
	struct rdma_register_tid_ramrod_data rdma_register_tid;
	struct rdma_deregister_tid_ramrod_data rdma_deregister_tid;
	struct roce_create_qp_resp_ramrod_data roce_create_qp_resp;
	struct roce_create_qp_req_ramrod_data roce_create_qp_req;
	struct roce_modify_qp_resp_ramrod_data roce_modify_qp_resp;
	struct roce_modify_qp_req_ramrod_data roce_modify_qp_req;
	struct roce_query_qp_resp_ramrod_data roce_query_qp_resp;
	struct roce_query_qp_req_ramrod_data roce_query_qp_req;
	struct roce_destroy_qp_resp_ramrod_data roce_destroy_qp_resp;
	struct roce_destroy_qp_req_ramrod_data roce_destroy_qp_req;
	struct roce_init_func_ramrod_data roce_init_func;
	struct rdma_create_cq_ramrod_data rdma_create_cq;
	struct rdma_destroy_cq_ramrod_data rdma_destroy_cq;
	struct rdma_srq_create_ramrod_data rdma_create_srq;
	struct rdma_srq_destroy_ramrod_data rdma_destroy_srq;
	struct rdma_srq_modify_ramrod_data rdma_modify_srq;
	struct iwarp_create_qp_ramrod_data iwarp_create_qp;
	struct iwarp_tcp_offload_ramrod_data iwarp_tcp_offload;
	struct iwarp_mpa_offload_ramrod_data iwarp_mpa_offload;
	struct iwarp_modify_qp_ramrod_data iwarp_modify_qp;
	struct iwarp_init_func_ramrod_data iwarp_init_func;
	struct fcoe_init_ramrod_params fcoe_init;
	struct fcoe_conn_offload_ramrod_params fcoe_conn_ofld;
	struct fcoe_conn_terminate_ramrod_params fcoe_conn_terminate;
	struct fcoe_stat_ramrod_params fcoe_stat;

	struct iscsi_init_ramrod_params iscsi_init;
	struct iscsi_spe_conn_offload iscsi_conn_offload;
	struct iscsi_conn_update_ramrod_params iscsi_conn_update;
	struct iscsi_spe_conn_mac_update iscsi_conn_mac_update;
	struct iscsi_spe_conn_termination iscsi_conn_terminate;

	struct nvmetcp_init_ramrod_params nvmetcp_init;
	struct nvmetcp_spe_conn_offload nvmetcp_conn_offload;
	struct nvmetcp_conn_update_ramrod_params nvmetcp_conn_update;
	struct nvmetcp_spe_conn_termination nvmetcp_conn_terminate;

	struct vf_start_ramrod_data vf_start;
	struct vf_stop_ramrod_data vf_stop;
};

#define EQ_MAX_CREDIT   0xffffffff

enum spq_priority {
	QED_SPQ_PRIORITY_NORMAL,
	QED_SPQ_PRIORITY_HIGH,
};

union qed_spq_req_comp {
	struct qed_spq_comp_cb	cb;
	u64			*done_addr;
};

struct qed_spq_comp_done {
	unsigned int	done;
	u8		fw_return_code;
};

struct qed_spq_entry {
	struct list_head		list;

	u8				flags;

	
	struct slow_path_element	elem;

	union ramrod_data		ramrod;

	enum spq_priority		priority;

	
	struct list_head		*queue;

	enum spq_mode			comp_mode;
	struct qed_spq_comp_cb		comp_cb;
	struct qed_spq_comp_done	comp_done; 

	
	struct qed_spq_entry		*post_ent;
};

struct qed_eq {
	struct qed_chain	chain;
	u8			eq_sb_index;    
	__le16			*p_fw_cons;     
};

struct qed_consq {
	struct qed_chain chain;
};

typedef int (*qed_spq_async_comp_cb)(struct qed_hwfn *p_hwfn, u8 opcode,
				     __le16 echo, union event_ring_data *data,
				     u8 fw_return_code);

int
qed_spq_register_async_cb(struct qed_hwfn *p_hwfn,
			  enum protocol_type protocol_id,
			  qed_spq_async_comp_cb cb);

void
qed_spq_unregister_async_cb(struct qed_hwfn *p_hwfn,
			    enum protocol_type protocol_id);

struct qed_spq {
	spinlock_t		lock; 

	struct list_head	unlimited_pending;
	struct list_head	pending;
	struct list_head	completion_pending;
	struct list_head	free_pool;

	struct qed_chain	chain;

	
	dma_addr_t		p_phys;
	struct qed_spq_entry	*p_virt;

#define SPQ_RING_SIZE \
	(CORE_SPQE_PAGE_SIZE_BYTES / sizeof(struct slow_path_element))

	
	DECLARE_BITMAP(p_comp_bitmap, SPQ_RING_SIZE);
	u8			comp_bitmap_idx;

	
	u32			unlimited_pending_count;
	u32			normal_count;
	u32			high_count;
	u32			comp_sent_count;
	u32			comp_count;

	u32			cid;
	u32			db_addr_offset;
	struct core_db_data	db_data;
	qed_spq_async_comp_cb	async_comp_cb[MAX_PROTOCOL_TYPE];
};


int qed_spq_post(struct qed_hwfn *p_hwfn,
		 struct qed_spq_entry *p_ent,
		 u8 *fw_return_code);


int qed_spq_alloc(struct qed_hwfn *p_hwfn);


void qed_spq_setup(struct qed_hwfn *p_hwfn);


void qed_spq_free(struct qed_hwfn *p_hwfn);


int
qed_spq_get_entry(struct qed_hwfn *p_hwfn,
		  struct qed_spq_entry **pp_ent);


void qed_spq_return_entry(struct qed_hwfn *p_hwfn,
			  struct qed_spq_entry *p_ent);

int qed_eq_alloc(struct qed_hwfn *p_hwfn, u16 num_elem);


void qed_eq_setup(struct qed_hwfn *p_hwfn);


void qed_eq_free(struct qed_hwfn *p_hwfn);


void qed_eq_prod_update(struct qed_hwfn *p_hwfn,
			u16 prod);


int qed_eq_completion(struct qed_hwfn *p_hwfn,
		      void *cookie);


int qed_spq_completion(struct qed_hwfn *p_hwfn,
		       __le16 echo,
		       u8 fw_return_code,
		       union event_ring_data *p_data);


u32 qed_spq_get_cid(struct qed_hwfn *p_hwfn);


int qed_consq_alloc(struct qed_hwfn *p_hwfn);


void qed_consq_setup(struct qed_hwfn *p_hwfn);


void qed_consq_free(struct qed_hwfn *p_hwfn);
int qed_spq_pend_post(struct qed_hwfn *p_hwfn);



#define QED_SP_EQ_COMPLETION  0x01
#define QED_SP_CQE_COMPLETION 0x02

struct qed_sp_init_data {
	u32			cid;
	u16			opaque_fid;

	
	enum spq_mode		comp_mode;
	struct qed_spq_comp_cb *p_comp_data;
};


void qed_sp_destroy_request(struct qed_hwfn *p_hwfn,
			    struct qed_spq_entry *p_ent);

int qed_sp_init_request(struct qed_hwfn *p_hwfn,
			struct qed_spq_entry **pp_ent,
			u8 cmd,
			u8 protocol,
			struct qed_sp_init_data *p_data);



int qed_sp_pf_start(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    struct qed_tunnel_info *p_tunn,
		    bool allow_npar_tx_switch);



int qed_sp_pf_update(struct qed_hwfn *p_hwfn);


int qed_sp_pf_update_stag(struct qed_hwfn *p_hwfn);


int qed_sp_pf_update_ufp(struct qed_hwfn *p_hwfn);

int qed_sp_pf_stop(struct qed_hwfn *p_hwfn);

int qed_sp_pf_update_tunn_cfg(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt,
			      struct qed_tunnel_info *p_tunn,
			      enum spq_mode comp_mode,
			      struct qed_spq_comp_cb *p_comp_data);


int qed_sp_heartbeat_ramrod(struct qed_hwfn *p_hwfn);

#endif
