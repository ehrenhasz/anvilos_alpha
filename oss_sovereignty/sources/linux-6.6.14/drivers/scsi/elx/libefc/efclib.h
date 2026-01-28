


#ifndef __EFCLIB_H__
#define __EFCLIB_H__

#include "scsi/fc/fc_els.h"
#include "scsi/fc/fc_fs.h"
#include "scsi/fc/fc_ns.h"
#include "scsi/fc/fc_gs.h"
#include "scsi/fc_frame.h"
#include "../include/efc_common.h"
#include "../libefc_sli/sli4.h"

#define EFC_SERVICE_PARMS_LENGTH	120
#define EFC_NAME_LENGTH			32
#define EFC_SM_NAME_LENGTH		64
#define EFC_DISPLAY_BUS_INFO_LENGTH	16

#define EFC_WWN_LENGTH			32

#define EFC_FC_ELS_DEFAULT_RETRIES	3


#define EFC_FC_ELS_SEND_DEFAULT_TIMEOUT	0
#define EFC_FC_FLOGI_TIMEOUT_SEC	5
#define EFC_SHUTDOWN_TIMEOUT_USEC	30000000


#define EFC_SCSI_CALL_COMPLETE		0
#define EFC_SCSI_CALL_ASYNC		1


enum efc_nport_topology {
	EFC_NPORT_TOPO_UNKNOWN = 0,
	EFC_NPORT_TOPO_FABRIC,
	EFC_NPORT_TOPO_P2P,
	EFC_NPORT_TOPO_FC_AL,
};

#define enable_target_rscn(efc)		1

enum efc_node_shutd_rsn {
	EFC_NODE_SHUTDOWN_DEFAULT = 0,
	EFC_NODE_SHUTDOWN_EXPLICIT_LOGO,
	EFC_NODE_SHUTDOWN_IMPLICIT_LOGO,
};

enum efc_node_send_ls_acc {
	EFC_NODE_SEND_LS_ACC_NONE = 0,
	EFC_NODE_SEND_LS_ACC_PLOGI,
	EFC_NODE_SEND_LS_ACC_PRLI,
};

#define EFC_LINK_STATUS_UP		0
#define EFC_LINK_STATUS_DOWN		1

enum efc_sm_event;


struct efc_sm_ctx {
	void (*current_state)(struct efc_sm_ctx *ctx,
			      enum efc_sm_event evt, void *arg);

	const char	*description;
	void		*app;
};


struct efc_domain_record {
	u32		index;
	u32		priority;
	u8		address[6];
	u8		wwn[8];
	union {
		u8	vlan[512];
		u8	loop[128];
	} map;
	u32		speed;
	u32		fc_id;
	bool		is_loop;
	bool		is_nport;
};


enum efc_hw_domain_event {
	EFC_HW_DOMAIN_ALLOC_OK,
	EFC_HW_DOMAIN_ALLOC_FAIL,
	EFC_HW_DOMAIN_ATTACH_OK,
	EFC_HW_DOMAIN_ATTACH_FAIL,
	EFC_HW_DOMAIN_FREE_OK,
	EFC_HW_DOMAIN_FREE_FAIL,
	EFC_HW_DOMAIN_LOST,
	EFC_HW_DOMAIN_FOUND,
	EFC_HW_DOMAIN_CHANGED,
};



struct efc_nport {
	struct list_head	list_entry;
	struct kref		ref;
	void			(*release)(struct kref *arg);
	struct efc		*efc;
	u32			instance_index;
	char			display_name[EFC_NAME_LENGTH];
	bool			is_vport;
	bool			free_req_pending;
	bool			attached;
	bool			attaching;
	bool			p2p_winner;
	struct efc_domain	*domain;
	u64			wwpn;
	u64			wwnn;
	void			*tgt_data;
	void			*ini_data;

	u32			indicator;
	u32			fc_id;
	struct efc_dma		dma;

	u8			wwnn_str[EFC_WWN_LENGTH];
	__be64			sli_wwpn;
	__be64			sli_wwnn;

	struct efc_sm_ctx	sm;
	struct xarray		lookup;
	bool			enable_ini;
	bool			enable_tgt;
	bool			enable_rscn;
	bool			shutting_down;
	u32			p2p_port_id;
	enum efc_nport_topology topology;
	u8			service_params[EFC_SERVICE_PARMS_LENGTH];
	u32			p2p_remote_port_id;
};


struct efc_domain {
	struct efc		*efc;
	char			display_name[EFC_NAME_LENGTH];
	struct list_head	nport_list;
	struct kref		ref;
	void			(*release)(struct kref *arg);
	void			*ini_domain;
	void			*tgt_domain;

	
	u32			fcf;
	u32			fcf_indicator;
	u32			indicator;
	u32			nport_count;
	struct efc_dma		dma;

	
	u64			fcf_wwn;
	struct efc_sm_ctx	drvsm;
	bool			attached;
	bool			is_fc;
	bool			is_loop;
	bool			is_nlport;
	bool			domain_found_pending;
	bool			req_domain_free;
	bool			req_accept_frames;
	bool			domain_notify_pend;

	struct efc_domain_record pending_drec;
	u8			service_params[EFC_SERVICE_PARMS_LENGTH];
	u8			flogi_service_params[EFC_SERVICE_PARMS_LENGTH];

	struct xarray		lookup;

	struct efc_nport	*nport;
};


struct efc_remote_node {
	u32			indicator;
	u32			index;
	u32			fc_id;

	bool			attached;

	struct efc_nport	*nport;
	void			*node;
};


struct efc_node {
	struct efc		*efc;
	char			display_name[EFC_NAME_LENGTH];
	struct efc_nport	*nport;
	struct kref		ref;
	void			(*release)(struct kref *arg);
	bool			hold_frames;
	bool			els_io_enabled;
	bool			send_plogi_acc;
	bool			send_plogi;
	bool			rscn_pending;
	bool			fcp_enabled;
	bool			attached;
	bool			req_free;

	spinlock_t		els_ios_lock;
	struct list_head	els_ios_list;
	void			*ini_node;
	void			*tgt_node;

	struct efc_remote_node	rnode;
	
	struct efc_sm_ctx	sm;
	u32			evtdepth;

	enum efc_node_send_ls_acc send_ls_acc;
	void			*ls_acc_io;
	u32			ls_acc_oxid;
	u32			ls_acc_did;
	enum efc_node_shutd_rsn	shutdown_reason;
	bool			targ;
	bool			init;
	bool			refound;
	struct efc_dma		sparm_dma_buf;
	u8			service_params[EFC_SERVICE_PARMS_LENGTH];
	spinlock_t		pend_frames_lock;
	struct list_head	pend_frames;
	u32			pend_frames_processed;
	u32			ox_id_in_use;
	u32			els_retries_remaining;
	u32			els_req_cnt;
	u32			els_cmpl_cnt;
	u32			abort_cnt;

	char			current_state_name[EFC_SM_NAME_LENGTH];
	char			prev_state_name[EFC_SM_NAME_LENGTH];
	int			current_evt;
	int			prev_evt;

	void (*nodedb_state)(struct efc_sm_ctx *ctx,
			     enum efc_sm_event evt, void *arg);
	struct timer_list	gidpt_delay_timer;
	u64			time_last_gidpt_msec;

	char			wwnn[EFC_WWN_LENGTH];
	char			wwpn[EFC_WWN_LENGTH];
};



struct efc_vport {
	struct list_head	list_entry;
	u64			wwnn;
	u64			wwpn;
	u32			fc_id;
	bool			enable_tgt;
	bool			enable_ini;
	void			*tgt_data;
	void			*ini_data;
	struct efc_nport	*nport;
};

#define node_printf(node, fmt, args...) \
	efc_log_info(node->efc, "[%s] " fmt, node->display_name, ##args)


struct efc_node_cb {
	int			status;
	int			ext_status;
	struct efc_hw_rq_buffer *header;
	struct efc_hw_rq_buffer *payload;
	struct efc_dma		els_rsp;

	
	int			rsp_len;
};

struct efc_hw_rq_buffer {
	u16			rqindex;
	struct efc_dma		dma;
};


struct efc_hw_sequence {
	struct list_head	list_entry;
	void			*hw;
	u8			fcfi;
	struct efc_hw_rq_buffer *header;
	struct efc_hw_rq_buffer *payload;
	void			*hw_priv;
};

enum efc_disc_io_type {
	EFC_DISC_IO_ELS_REQ,
	EFC_DISC_IO_ELS_RESP,
	EFC_DISC_IO_CT_REQ,
	EFC_DISC_IO_CT_RESP
};

struct efc_io_els_params {
	u32             s_id;
	u16             ox_id;
	u8              timeout;
};

struct efc_io_ct_params {
	u8              r_ctl;
	u8              type;
	u8              df_ctl;
	u8              timeout;
	u16             ox_id;
};

union efc_disc_io_param {
	struct efc_io_els_params els;
	struct efc_io_ct_params ct;
};

struct efc_disc_io {
	struct efc_dma		req;         
	struct efc_dma		rsp;         
	enum efc_disc_io_type	io_type;     
	u16			xmit_len;    
	u16			rsp_len;     
	u32			rpi;         
	u32			vpi;         
	u32			s_id;
	u32			d_id;
	bool			rpi_registered; 
	union efc_disc_io_param iparam;
};


#define EFC_HW_SEQ_HOLD		0

#define EFC_HW_SEQ_FREE		1

struct libefc_function_template {
	
	int (*new_nport)(struct efc *efc, struct efc_nport *sp);
	void (*del_nport)(struct efc *efc, struct efc_nport *sp);

	
	int (*scsi_new_node)(struct efc *efc, struct efc_node *n);
	int (*scsi_del_node)(struct efc *efc, struct efc_node *n, int reason);

	int (*issue_mbox_rqst)(void *efct, void *buf, void *cb, void *arg);
	
	int (*send_els)(struct efc *efc, struct efc_disc_io *io);
	
	int (*send_bls)(struct efc *efc, u32 type, struct sli_bls_params *bls);
	
	int (*hw_seq_free)(struct efc *efc, struct efc_hw_sequence *seq);
};

#define EFC_LOG_LIB		0x01
#define EFC_LOG_NODE		0x02
#define EFC_LOG_PORT		0x04
#define EFC_LOG_DOMAIN		0x08
#define EFC_LOG_ELS		0x10
#define EFC_LOG_DOMAIN_SM	0x20
#define EFC_LOG_SM		0x40


struct efc {
	void			*base;
	struct pci_dev		*pci;
	struct sli4		*sli;
	u32			fcfi;
	u64			req_wwpn;
	u64			req_wwnn;

	u64			def_wwpn;
	u64			def_wwnn;
	u64			max_xfer_size;
	mempool_t		*node_pool;
	struct dma_pool		*node_dma_pool;
	u32			nodes_count;

	u32			link_status;

	struct list_head	vport_list;
	
	spinlock_t		vport_lock;

	struct libefc_function_template tt;
	
	spinlock_t		lock;

	bool			enable_ini;
	bool			enable_tgt;

	u32			log_level;

	struct efc_domain	*domain;
	void (*domain_free_cb)(struct efc *efc, void *arg);
	void			*domain_free_cb_arg;

	u64			tgt_rscn_delay_msec;
	u64			tgt_rscn_period_msec;

	bool			external_loopback;
	u32			nodedb_mask;
	u32			logmask;
	mempool_t		*els_io_pool;
	atomic_t		els_io_alloc_failed_count;

	
	bool			hold_frames;
	
	spinlock_t		pend_frames_lock;
	struct list_head	pend_frames;
	
	u32			pend_frames_processed;

};


int efcport_init(struct efc *efc);
void efcport_destroy(struct efc *efc);

int efc_domain_cb(void *arg, int event, void *data);
void
efc_register_domain_free_cb(struct efc *efc,
			    void (*callback)(struct efc *efc, void *arg),
			    void *arg);


void efc_nport_cb(void *arg, int event, void *data);
struct efc_vport *
efc_vport_create_spec(struct efc *efc, u64 wwnn, u64 wwpn, u32 fc_id,
		      bool enable_ini, bool enable_tgt,
		      void *tgt_data, void *ini_data);
int efc_nport_vport_new(struct efc_domain *domain, u64 wwpn,
			u64 wwnn, u32 fc_id, bool ini, bool tgt,
			void *tgt_data, void *ini_data);
int efc_nport_vport_del(struct efc *efc, struct efc_domain *domain,
			u64 wwpn, u64 wwnn);

void efc_vport_del_all(struct efc *efc);


int efc_remote_node_cb(void *arg, int event, void *data);
void efc_node_fcid_display(u32 fc_id, char *buffer, u32 buf_len);
void efc_node_post_shutdown(struct efc_node *node, void *arg);
u64 efc_node_get_wwpn(struct efc_node *node);


void efc_dispatch_frame(struct efc *efc, struct efc_hw_sequence *seq);
void efc_disc_io_complete(struct efc_disc_io *io, u32 len, u32 status,
			  u32 ext_status);


void efc_scsi_sess_reg_complete(struct efc_node *node, u32 status);
void efc_scsi_del_initiator_complete(struct efc *efc, struct efc_node *node);
void efc_scsi_del_target_complete(struct efc *efc, struct efc_node *node);
void efc_scsi_io_list_empty(struct efc *efc, struct efc_node *node);

#endif 
