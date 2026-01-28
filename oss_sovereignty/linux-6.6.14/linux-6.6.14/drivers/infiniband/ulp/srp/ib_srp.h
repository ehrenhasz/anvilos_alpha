#ifndef IB_SRP_H
#define IB_SRP_H
#include <linux/types.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <rdma/ib_verbs.h>
#include <rdma/ib_sa.h>
#include <rdma/ib_cm.h>
#include <rdma/rdma_cm.h>
enum {
	SRP_PATH_REC_TIMEOUT_MS	= 1000,
	SRP_ABORT_TIMEOUT_MS	= 5000,
	SRP_PORT_REDIRECT	= 1,
	SRP_DLID_REDIRECT	= 2,
	SRP_STALE_CONN		= 3,
	SRP_DEF_SG_TABLESIZE	= 12,
	SRP_DEFAULT_QUEUE_SIZE	= 1 << 6,
	SRP_RSP_SQ_SIZE		= 1,
	SRP_TSK_MGMT_SQ_SIZE	= 1,
	SRP_DEFAULT_CMD_SQ_SIZE = SRP_DEFAULT_QUEUE_SIZE - SRP_RSP_SQ_SIZE -
				  SRP_TSK_MGMT_SQ_SIZE,
	SRP_MAX_PAGES_PER_MR	= 512,
	SRP_MAX_ADD_CDB_LEN	= 16,
	SRP_MAX_IMM_SGE		= 2,
	SRP_MAX_SGE		= SRP_MAX_IMM_SGE + 1,
	SRP_IMM_DATA_OFFSET	= sizeof(struct srp_cmd) +
				  SRP_MAX_ADD_CDB_LEN +
				  sizeof(struct srp_imm_buf),
};
enum {
	SRP_TAG_NO_REQ		= ~0U,
	SRP_TAG_TSK_MGMT	= BIT(31),
};
enum srp_target_state {
	SRP_TARGET_SCANNING,
	SRP_TARGET_LIVE,
	SRP_TARGET_REMOVED,
};
enum srp_iu_type {
	SRP_IU_CMD,
	SRP_IU_TSK_MGMT,
	SRP_IU_RSP,
};
struct srp_device {
	struct list_head	dev_list;
	struct ib_device       *dev;
	struct ib_pd	       *pd;
	u32			global_rkey;
	u64			mr_page_mask;
	int			mr_page_size;
	int			mr_max_size;
	int			max_pages_per_mr;
	bool			has_fr;
	bool			use_fast_reg;
};
struct srp_host {
	struct srp_device      *srp_dev;
	u32			port;
	struct device		dev;
	struct list_head	target_list;
	spinlock_t		target_lock;
	struct list_head	list;
	struct mutex		add_target_mutex;
};
struct srp_request {
	struct scsi_cmnd       *scmnd;
	struct srp_iu	       *cmd;
	struct srp_fr_desc     **fr_list;
	struct srp_direct_buf  *indirect_desc;
	dma_addr_t		indirect_dma_addr;
	short			nmdesc;
	struct ib_cqe		reg_cqe;
};
struct srp_rdma_ch {
	struct list_head	free_tx;
	spinlock_t		lock;
	s32			req_lim;
	struct srp_target_port *target ____cacheline_aligned_in_smp;
	struct ib_cq	       *send_cq;
	struct ib_cq	       *recv_cq;
	struct ib_qp	       *qp;
	struct srp_fr_pool     *fr_pool;
	uint32_t		max_it_iu_len;
	uint32_t		max_ti_iu_len;
	u8			max_imm_sge;
	bool			use_imm_data;
	struct completion	done;
	int			status;
	union {
		struct ib_cm {
			struct sa_path_rec	path;
			struct ib_sa_query	*path_query;
			int			path_query_id;
			struct ib_cm_id		*cm_id;
		} ib_cm;
		struct rdma_cm {
			struct rdma_cm_id	*cm_id;
		} rdma_cm;
	};
	struct srp_iu	      **tx_ring;
	struct srp_iu	      **rx_ring;
	int			comp_vector;
	u64			tsk_mgmt_tag;
	struct completion	tsk_mgmt_done;
	u8			tsk_mgmt_status;
	bool			connected;
};
struct srp_target_port {
	spinlock_t		lock;
	u32			global_rkey;
	struct srp_rdma_ch	*ch;
	struct net		*net;
	u32			ch_count;
	u32			lkey;
	enum srp_target_state	state;
	uint32_t		max_it_iu_size;
	unsigned int		cmd_sg_cnt;
	unsigned int		indirect_size;
	bool			allow_ext_sg;
	union ib_gid		sgid;
	__be64			id_ext;
	__be64			ioc_guid;
	__be64			initiator_ext;
	u16			io_class;
	struct srp_host	       *srp_host;
	struct Scsi_Host       *scsi_host;
	struct srp_rport       *rport;
	char			target_name[32];
	unsigned int		scsi_id;
	unsigned int		sg_tablesize;
	unsigned int		target_can_queue;
	int			mr_pool_size;
	int			mr_per_cmd;
	int			queue_size;
	int			comp_vector;
	int			tl_retry_count;
	bool			using_rdma_cm;
	union {
		struct {
			__be64			service_id;
			union ib_gid		orig_dgid;
			__be16			pkey;
		} ib_cm;
		struct {
			union {
				struct sockaddr_in	ip4;
				struct sockaddr_in6	ip6;
				struct sockaddr		sa;
				struct sockaddr_storage ss;
			} src;
			union {
				struct sockaddr_in	ip4;
				struct sockaddr_in6	ip6;
				struct sockaddr		sa;
				struct sockaddr_storage ss;
			} dst;
			bool src_specified;
		} rdma_cm;
	};
	u32			rq_tmo_jiffies;
	int			zero_req_lim;
	struct work_struct	tl_err_work;
	struct work_struct	remove_work;
	struct list_head	list;
	bool			qp_in_error;
};
struct srp_iu {
	struct list_head	list;
	u64			dma;
	void		       *buf;
	size_t			size;
	enum dma_data_direction	direction;
	u32			num_sge;
	struct ib_sge		sge[SRP_MAX_SGE];
	struct ib_cqe		cqe;
};
struct srp_fr_desc {
	struct list_head		entry;
	struct ib_mr			*mr;
};
struct srp_fr_pool {
	int			size;
	int			max_page_list_len;
	spinlock_t		lock;
	struct list_head	free_list;
	struct srp_fr_desc	desc[];
};
struct srp_map_state {
	union {
		struct {
			struct srp_fr_desc **next;
			struct srp_fr_desc **end;
		} fr;
		struct {
			void		   **next;
			void		   **end;
		} gen;
	};
	struct srp_direct_buf  *desc;
	union {
		u64			*pages;
		struct scatterlist	*sg;
	};
	dma_addr_t		base_dma_addr;
	u32			dma_len;
	u32			total_len;
	unsigned int		npages;
	unsigned int		nmdesc;
	unsigned int		ndesc;
};
#endif  
