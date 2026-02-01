 

#ifndef IB_SRPT_H
#define IB_SRPT_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/wait.h>

#include <rdma/ib_verbs.h>
#include <rdma/ib_sa.h>
#include <rdma/ib_cm.h>
#include <rdma/rdma_cm.h>
#include <rdma/rw.h>

#include <scsi/srp.h>

#include "ib_dm_mad.h"

 
#define SRP_SERVICE_NAME_PREFIX		"SRP.T10:"

struct srpt_nexus;

enum {
	 
	SRP_PROTOCOL = 0x0108,
	SRP_PROTOCOL_VERSION = 0x0001,
	SRP_IO_SUBCLASS = 0x609e,
	SRP_SEND_TO_IOC = 0x01,
	SRP_SEND_FROM_IOC = 0x02,
	SRP_RDMA_READ_FROM_IOC = 0x08,
	SRP_RDMA_WRITE_FROM_IOC = 0x20,

	 
	SRP_MTCH_ACTION = 0x03,  
	SRP_LOSOLNT = 0x10,  
	SRP_CRSOLNT = 0x20,  
	SRP_AESOLNT = 0x40,  

	 
	SRP_SCSOLNT = 0x02,  
	SRP_UCSOLNT = 0x04,  

	 
	SRP_SOLNT = 0x01,  

	 
	SRP_TSK_MGMT_SUCCESS = 0x00,
	SRP_TSK_MGMT_FUNC_NOT_SUPP = 0x04,
	SRP_TSK_MGMT_FAILED = 0x05,

	 
	SRP_CMD_SIMPLE_Q = 0x0,
	SRP_CMD_HEAD_OF_Q = 0x1,
	SRP_CMD_ORDERED_Q = 0x2,
	SRP_CMD_ACA = 0x4,

	SRPT_DEF_SG_TABLESIZE = 128,

	MIN_SRPT_SQ_SIZE = 16,
	DEF_SRPT_SQ_SIZE = 4096,
	MAX_SRPT_RQ_SIZE = 128,
	MIN_SRPT_SRQ_SIZE = 4,
	DEFAULT_SRPT_SRQ_SIZE = 4095,
	MAX_SRPT_SRQ_SIZE = 65535,
	MAX_SRPT_RDMA_SIZE = 1U << 24,
	MAX_SRPT_RSP_SIZE = 1024,

	SRP_MAX_ADD_CDB_LEN = 16,
	SRP_MAX_IMM_DATA_OFFSET = 80,
	SRP_MAX_IMM_DATA = 8 * 1024,
	MIN_MAX_REQ_SIZE = 996,
	DEFAULT_MAX_REQ_SIZE_1 = sizeof(struct srp_cmd)  +
				 SRP_MAX_ADD_CDB_LEN +
				 sizeof(struct srp_indirect_buf)  +
				 128 * sizeof(struct srp_direct_buf) ,
	DEFAULT_MAX_REQ_SIZE_2 = SRP_MAX_IMM_DATA_OFFSET +
				 sizeof(struct srp_imm_buf) + SRP_MAX_IMM_DATA,
	DEFAULT_MAX_REQ_SIZE = DEFAULT_MAX_REQ_SIZE_1 > DEFAULT_MAX_REQ_SIZE_2 ?
			       DEFAULT_MAX_REQ_SIZE_1 : DEFAULT_MAX_REQ_SIZE_2,

	MIN_MAX_RSP_SIZE = sizeof(struct srp_rsp)  + 4,
	DEFAULT_MAX_RSP_SIZE = 256,  

	DEFAULT_MAX_RDMA_SIZE = 65536,
};

 
enum srpt_command_state {
	SRPT_STATE_NEW		 = 0,
	SRPT_STATE_NEED_DATA	 = 1,
	SRPT_STATE_DATA_IN	 = 2,
	SRPT_STATE_CMD_RSP_SENT	 = 3,
	SRPT_STATE_MGMT		 = 4,
	SRPT_STATE_MGMT_RSP_SENT = 5,
	SRPT_STATE_DONE		 = 6,
};

 
struct srpt_ioctx {
	struct ib_cqe		cqe;
	void			*buf;
	dma_addr_t		dma;
	uint32_t		offset;
	uint32_t		index;
};

 
struct srpt_recv_ioctx {
	struct srpt_ioctx	ioctx;
	struct list_head	wait_list;
	int			byte_len;
};

struct srpt_rw_ctx {
	struct rdma_rw_ctx	rw;
	struct scatterlist	*sg;
	unsigned int		nents;
};

 
struct srpt_send_ioctx {
	struct srpt_ioctx	ioctx;
	struct srpt_rdma_ch	*ch;
	struct srpt_recv_ioctx	*recv_ioctx;

	struct srpt_rw_ctx	s_rw_ctx;
	struct srpt_rw_ctx	*rw_ctxs;

	struct scatterlist	imm_sg;

	struct ib_cqe		rdma_cqe;
	enum srpt_command_state	state;
	struct se_cmd		cmd;
	u8			n_rdma;
	u8			n_rw_ctx;
	bool			queue_status_only;
	u8			sense_data[TRANSPORT_SENSE_BUFFER];
};

 
enum rdma_ch_state {
	CH_CONNECTING,
	CH_LIVE,
	CH_DISCONNECTING,
	CH_DRAINING,
	CH_DISCONNECTED,
};

 
struct srpt_rdma_ch {
	struct srpt_nexus	*nexus;
	struct ib_qp		*qp;
	union {
		struct {
			struct ib_cm_id		*cm_id;
		} ib_cm;
		struct {
			struct rdma_cm_id	*cm_id;
		} rdma_cm;
	};
	struct ib_cq		*cq;
	u32			cq_size;
	struct ib_cqe		zw_cqe;
	struct rcu_head		rcu;
	struct kref		kref;
	struct completion	*closed;
	int			rq_size;
	u32			max_rsp_size;
	atomic_t		sq_wr_avail;
	struct srpt_port	*sport;
	int			max_ti_iu_len;
	atomic_t		req_lim;
	atomic_t		req_lim_delta;
	u16			imm_data_offset;
	spinlock_t		spinlock;
	enum rdma_ch_state	state;
	struct kmem_cache	*rsp_buf_cache;
	struct srpt_send_ioctx	**ioctx_ring;
	struct kmem_cache	*req_buf_cache;
	struct srpt_recv_ioctx	**ioctx_recv_ring;
	struct list_head	list;
	struct list_head	cmd_wait_list;
	uint16_t		pkey;
	bool			using_rdma_cm;
	bool			processing_wait_list;
	struct se_session	*sess;
	u8			sess_name[40];
	struct work_struct	release_work;
};

 
struct srpt_nexus {
	struct rcu_head		rcu;
	struct list_head	entry;
	struct list_head	ch_list;
	u8			i_port_id[16];
	u8			t_port_id[16];
};

 
struct srpt_port_attrib {
	u32			srp_max_rdma_size;
	u32			srp_max_rsp_size;
	u32			srp_sq_size;
	bool			use_srq;
};

 
struct srpt_tpg {
	struct list_head	entry;
	struct srpt_port_id	*sport_id;
	struct se_portal_group	tpg;
};

 
struct srpt_port_id {
	struct mutex		mutex;
	struct list_head	tpg_list;
	struct se_wwn		wwn;
	char			name[64];
};

 
struct srpt_port {
	struct srpt_device	*sdev;
	struct ib_mad_agent	*mad_agent;
	bool			enabled;
	u8			port;
	u32			sm_lid;
	u32			lid;
	union ib_gid		gid;
	struct work_struct	work;
	char			guid_name[64];
	struct srpt_port_id	*guid_id;
	char			gid_name[64];
	struct srpt_port_id	*gid_id;
	struct srpt_port_attrib port_attrib;
	atomic_t		refcount;
	struct completion	*freed_channels;
	struct mutex		mutex;
	struct list_head	nexus_list;
};

 
struct srpt_device {
	struct kref		refcnt;
	struct ib_device	*device;
	struct ib_pd		*pd;
	u32			lkey;
	struct ib_srq		*srq;
	struct ib_cm_id		*cm_id;
	int			srq_size;
	struct mutex		sdev_mutex;
	bool			use_srq;
	struct kmem_cache	*req_buf_cache;
	struct srpt_recv_ioctx	**ioctx_ring;
	struct ib_event_handler	event_handler;
	struct list_head	list;
	struct srpt_port        port[];
};

#endif				 
