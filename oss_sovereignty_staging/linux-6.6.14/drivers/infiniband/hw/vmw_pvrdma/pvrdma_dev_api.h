 

#ifndef __PVRDMA_DEV_API_H__
#define __PVRDMA_DEV_API_H__

#include <linux/types.h>

#include "pvrdma_verbs.h"

 

#define PVRDMA_ROCEV1_VERSION		17
#define PVRDMA_ROCEV2_VERSION		18
#define PVRDMA_PPN64_VERSION		19
#define PVRDMA_QPHANDLE_VERSION		20
#define PVRDMA_VERSION			PVRDMA_QPHANDLE_VERSION

#define PVRDMA_BOARD_ID			1
#define PVRDMA_REV_ID			1

 

#define PVRDMA_PDIR_SHIFT		18
#define PVRDMA_PTABLE_SHIFT		9
#define PVRDMA_PAGE_DIR_DIR(x)		(((x) >> PVRDMA_PDIR_SHIFT) & 0x1)
#define PVRDMA_PAGE_DIR_TABLE(x)	(((x) >> PVRDMA_PTABLE_SHIFT) & 0x1ff)
#define PVRDMA_PAGE_DIR_PAGE(x)		((x) & 0x1ff)
#define PVRDMA_PAGE_DIR_MAX_PAGES	(1 * 512 * 512)
#define PVRDMA_MAX_FAST_REG_PAGES	128

 

#define PVRDMA_MAX_INTERRUPTS	3

 
#define PVRDMA_REG_VERSION	0x00	 
#define PVRDMA_REG_DSRLOW	0x04	 
#define PVRDMA_REG_DSRHIGH	0x08	 
#define PVRDMA_REG_CTL		0x0c	 
#define PVRDMA_REG_REQUEST	0x10	 
#define PVRDMA_REG_ERR		0x14	 
#define PVRDMA_REG_ICR		0x18	 
#define PVRDMA_REG_IMR		0x1c	 
#define PVRDMA_REG_MACL		0x20	 
#define PVRDMA_REG_MACH		0x24	 

 
#define PVRDMA_CQ_FLAG_ARMED_SOL	BIT(0)	 
#define PVRDMA_CQ_FLAG_ARMED		BIT(1)	 
#define PVRDMA_MR_FLAG_DMA		BIT(0)	 
#define PVRDMA_MR_FLAG_FRMR		BIT(1)	 

 

#define PVRDMA_ATOMIC_OP_COMP_SWAP	BIT(0)	 
#define PVRDMA_ATOMIC_OP_FETCH_ADD	BIT(1)	 
#define PVRDMA_ATOMIC_OP_MASK_COMP_SWAP	BIT(2)	 
#define PVRDMA_ATOMIC_OP_MASK_FETCH_ADD	BIT(3)	 

 

#define PVRDMA_BMME_FLAG_LOCAL_INV	BIT(0)	 
#define PVRDMA_BMME_FLAG_REMOTE_INV	BIT(1)	 
#define PVRDMA_BMME_FLAG_FAST_REG_WR	BIT(2)	 

 

#define PVRDMA_GID_TYPE_FLAG_ROCE_V1	BIT(0)
#define PVRDMA_GID_TYPE_FLAG_ROCE_V2	BIT(1)

 

#define PVRDMA_IS_VERSION17(_dev)					\
	(_dev->dsr_version == PVRDMA_ROCEV1_VERSION &&			\
	 _dev->dsr->caps.gid_types == PVRDMA_GID_TYPE_FLAG_ROCE_V1)

#define PVRDMA_IS_VERSION18(_dev)					\
	(_dev->dsr_version >= PVRDMA_ROCEV2_VERSION &&			\
	 (_dev->dsr->caps.gid_types == PVRDMA_GID_TYPE_FLAG_ROCE_V1 ||  \
	  _dev->dsr->caps.gid_types == PVRDMA_GID_TYPE_FLAG_ROCE_V2))	\

#define PVRDMA_SUPPORTED(_dev)						\
	((_dev->dsr->caps.mode == PVRDMA_DEVICE_MODE_ROCE) &&		\
	 (PVRDMA_IS_VERSION17(_dev) || PVRDMA_IS_VERSION18(_dev)))

 

#define PVRDMA_GET_CAP(_dev, _old_val, _val) \
	((PVRDMA_IS_VERSION18(_dev)) ? _val : _old_val)

enum pvrdma_pci_resource {
	PVRDMA_PCI_RESOURCE_MSIX,	 
	PVRDMA_PCI_RESOURCE_REG,	 
	PVRDMA_PCI_RESOURCE_UAR,	 
	PVRDMA_PCI_RESOURCE_LAST,	 
};

enum pvrdma_device_ctl {
	PVRDMA_DEVICE_CTL_ACTIVATE,	 
	PVRDMA_DEVICE_CTL_UNQUIESCE,	 
	PVRDMA_DEVICE_CTL_RESET,	 
};

enum pvrdma_intr_vector {
	PVRDMA_INTR_VECTOR_RESPONSE,	 
	PVRDMA_INTR_VECTOR_ASYNC,	 
	PVRDMA_INTR_VECTOR_CQ,		 
	 
};

enum pvrdma_intr_cause {
	PVRDMA_INTR_CAUSE_RESPONSE	= (1 << PVRDMA_INTR_VECTOR_RESPONSE),
	PVRDMA_INTR_CAUSE_ASYNC		= (1 << PVRDMA_INTR_VECTOR_ASYNC),
	PVRDMA_INTR_CAUSE_CQ		= (1 << PVRDMA_INTR_VECTOR_CQ),
};

enum pvrdma_gos_bits {
	PVRDMA_GOS_BITS_UNK,		 
	PVRDMA_GOS_BITS_32,		 
	PVRDMA_GOS_BITS_64,		 
};

enum pvrdma_gos_type {
	PVRDMA_GOS_TYPE_UNK,		 
	PVRDMA_GOS_TYPE_LINUX,		 
};

enum pvrdma_device_mode {
	PVRDMA_DEVICE_MODE_ROCE,	 
	PVRDMA_DEVICE_MODE_IWARP,	 
	PVRDMA_DEVICE_MODE_IB,		 
};

struct pvrdma_gos_info {
	u32 gos_bits:2;			 
	u32 gos_type:4;			 
	u32 gos_ver:16;			 
	u32 gos_misc:10;		 
	u32 pad;			 
};

struct pvrdma_device_caps {
	u64 fw_ver;				 
	__be64 node_guid;
	__be64 sys_image_guid;
	u64 max_mr_size;
	u64 page_size_cap;
	u64 atomic_arg_sizes;			 
	u32 ex_comp_mask;			 
	u32 device_cap_flags2;			 
	u32 max_fa_bit_boundary;		 
	u32 log_max_atomic_inline_arg;		 
	u32 vendor_id;
	u32 vendor_part_id;
	u32 hw_ver;
	u32 max_qp;
	u32 max_qp_wr;
	u32 device_cap_flags;
	u32 max_sge;
	u32 max_sge_rd;
	u32 max_cq;
	u32 max_cqe;
	u32 max_mr;
	u32 max_pd;
	u32 max_qp_rd_atom;
	u32 max_ee_rd_atom;
	u32 max_res_rd_atom;
	u32 max_qp_init_rd_atom;
	u32 max_ee_init_rd_atom;
	u32 max_ee;
	u32 max_rdd;
	u32 max_mw;
	u32 max_raw_ipv6_qp;
	u32 max_raw_ethy_qp;
	u32 max_mcast_grp;
	u32 max_mcast_qp_attach;
	u32 max_total_mcast_qp_attach;
	u32 max_ah;
	u32 max_fmr;
	u32 max_map_per_fmr;
	u32 max_srq;
	u32 max_srq_wr;
	u32 max_srq_sge;
	u32 max_uar;
	u32 gid_tbl_len;
	u16 max_pkeys;
	u8  local_ca_ack_delay;
	u8  phys_port_cnt;
	u8  mode;				 
	u8  atomic_ops;				 
	u8  bmme_flags;				 
	u8  gid_types;				 
	u32 max_fast_reg_page_list_len;
};

struct pvrdma_ring_page_info {
	u32 num_pages;				 
	u32 reserved;				 
	u64 pdir_dma;				 
};

#pragma pack(push, 1)

struct pvrdma_device_shared_region {
	u32 driver_version;			 
	u32 pad;				 
	struct pvrdma_gos_info gos_info;	 
	u64 cmd_slot_dma;			 
	u64 resp_slot_dma;			 
	struct pvrdma_ring_page_info async_ring_pages;
						 
	struct pvrdma_ring_page_info cq_ring_pages;
						 
	union {
		u32 uar_pfn;			 
		u64 uar_pfn64;			 
	};
	struct pvrdma_device_caps caps;		 
};

#pragma pack(pop)

 
enum pvrdma_eqe_type {
	PVRDMA_EVENT_CQ_ERR,
	PVRDMA_EVENT_QP_FATAL,
	PVRDMA_EVENT_QP_REQ_ERR,
	PVRDMA_EVENT_QP_ACCESS_ERR,
	PVRDMA_EVENT_COMM_EST,
	PVRDMA_EVENT_SQ_DRAINED,
	PVRDMA_EVENT_PATH_MIG,
	PVRDMA_EVENT_PATH_MIG_ERR,
	PVRDMA_EVENT_DEVICE_FATAL,
	PVRDMA_EVENT_PORT_ACTIVE,
	PVRDMA_EVENT_PORT_ERR,
	PVRDMA_EVENT_LID_CHANGE,
	PVRDMA_EVENT_PKEY_CHANGE,
	PVRDMA_EVENT_SM_CHANGE,
	PVRDMA_EVENT_SRQ_ERR,
	PVRDMA_EVENT_SRQ_LIMIT_REACHED,
	PVRDMA_EVENT_QP_LAST_WQE_REACHED,
	PVRDMA_EVENT_CLIENT_REREGISTER,
	PVRDMA_EVENT_GID_CHANGE,
};

 
struct pvrdma_eqe {
	u32 type;	 
	u32 info;	 
};

 
struct pvrdma_cqne {
	u32 info;	 
};

enum {
	PVRDMA_CMD_FIRST,
	PVRDMA_CMD_QUERY_PORT = PVRDMA_CMD_FIRST,
	PVRDMA_CMD_QUERY_PKEY,
	PVRDMA_CMD_CREATE_PD,
	PVRDMA_CMD_DESTROY_PD,
	PVRDMA_CMD_CREATE_MR,
	PVRDMA_CMD_DESTROY_MR,
	PVRDMA_CMD_CREATE_CQ,
	PVRDMA_CMD_RESIZE_CQ,
	PVRDMA_CMD_DESTROY_CQ,
	PVRDMA_CMD_CREATE_QP,
	PVRDMA_CMD_MODIFY_QP,
	PVRDMA_CMD_QUERY_QP,
	PVRDMA_CMD_DESTROY_QP,
	PVRDMA_CMD_CREATE_UC,
	PVRDMA_CMD_DESTROY_UC,
	PVRDMA_CMD_CREATE_BIND,
	PVRDMA_CMD_DESTROY_BIND,
	PVRDMA_CMD_CREATE_SRQ,
	PVRDMA_CMD_MODIFY_SRQ,
	PVRDMA_CMD_QUERY_SRQ,
	PVRDMA_CMD_DESTROY_SRQ,
	PVRDMA_CMD_MAX,
};

enum {
	PVRDMA_CMD_FIRST_RESP = (1 << 31),
	PVRDMA_CMD_QUERY_PORT_RESP = PVRDMA_CMD_FIRST_RESP,
	PVRDMA_CMD_QUERY_PKEY_RESP,
	PVRDMA_CMD_CREATE_PD_RESP,
	PVRDMA_CMD_DESTROY_PD_RESP_NOOP,
	PVRDMA_CMD_CREATE_MR_RESP,
	PVRDMA_CMD_DESTROY_MR_RESP_NOOP,
	PVRDMA_CMD_CREATE_CQ_RESP,
	PVRDMA_CMD_RESIZE_CQ_RESP,
	PVRDMA_CMD_DESTROY_CQ_RESP_NOOP,
	PVRDMA_CMD_CREATE_QP_RESP,
	PVRDMA_CMD_MODIFY_QP_RESP,
	PVRDMA_CMD_QUERY_QP_RESP,
	PVRDMA_CMD_DESTROY_QP_RESP,
	PVRDMA_CMD_CREATE_UC_RESP,
	PVRDMA_CMD_DESTROY_UC_RESP_NOOP,
	PVRDMA_CMD_CREATE_BIND_RESP_NOOP,
	PVRDMA_CMD_DESTROY_BIND_RESP_NOOP,
	PVRDMA_CMD_CREATE_SRQ_RESP,
	PVRDMA_CMD_MODIFY_SRQ_RESP,
	PVRDMA_CMD_QUERY_SRQ_RESP,
	PVRDMA_CMD_DESTROY_SRQ_RESP,
	PVRDMA_CMD_MAX_RESP,
};

struct pvrdma_cmd_hdr {
	u64 response;		 
	u32 cmd;		 
	u32 reserved;		 
};

struct pvrdma_cmd_resp_hdr {
	u64 response;		 
	u32 ack;		 
	u8 err;			 
	u8 reserved[3];		 
};

struct pvrdma_cmd_query_port {
	struct pvrdma_cmd_hdr hdr;
	u8 port_num;
	u8 reserved[7];
};

struct pvrdma_cmd_query_port_resp {
	struct pvrdma_cmd_resp_hdr hdr;
	struct pvrdma_port_attr attrs;
};

struct pvrdma_cmd_query_pkey {
	struct pvrdma_cmd_hdr hdr;
	u8 port_num;
	u8 index;
	u8 reserved[6];
};

struct pvrdma_cmd_query_pkey_resp {
	struct pvrdma_cmd_resp_hdr hdr;
	u16 pkey;
	u8 reserved[6];
};

struct pvrdma_cmd_create_uc {
	struct pvrdma_cmd_hdr hdr;
	union {
		u32 pfn;  
		u64 pfn64;  
	};
};

struct pvrdma_cmd_create_uc_resp {
	struct pvrdma_cmd_resp_hdr hdr;
	u32 ctx_handle;
	u8 reserved[4];
};

struct pvrdma_cmd_destroy_uc {
	struct pvrdma_cmd_hdr hdr;
	u32 ctx_handle;
	u8 reserved[4];
};

struct pvrdma_cmd_create_pd {
	struct pvrdma_cmd_hdr hdr;
	u32 ctx_handle;
	u8 reserved[4];
};

struct pvrdma_cmd_create_pd_resp {
	struct pvrdma_cmd_resp_hdr hdr;
	u32 pd_handle;
	u8 reserved[4];
};

struct pvrdma_cmd_destroy_pd {
	struct pvrdma_cmd_hdr hdr;
	u32 pd_handle;
	u8 reserved[4];
};

struct pvrdma_cmd_create_mr {
	struct pvrdma_cmd_hdr hdr;
	u64 start;
	u64 length;
	u64 pdir_dma;
	u32 pd_handle;
	u32 access_flags;
	u32 flags;
	u32 nchunks;
};

struct pvrdma_cmd_create_mr_resp {
	struct pvrdma_cmd_resp_hdr hdr;
	u32 mr_handle;
	u32 lkey;
	u32 rkey;
	u8 reserved[4];
};

struct pvrdma_cmd_destroy_mr {
	struct pvrdma_cmd_hdr hdr;
	u32 mr_handle;
	u8 reserved[4];
};

struct pvrdma_cmd_create_cq {
	struct pvrdma_cmd_hdr hdr;
	u64 pdir_dma;
	u32 ctx_handle;
	u32 cqe;
	u32 nchunks;
	u8 reserved[4];
};

struct pvrdma_cmd_create_cq_resp {
	struct pvrdma_cmd_resp_hdr hdr;
	u32 cq_handle;
	u32 cqe;
};

struct pvrdma_cmd_resize_cq {
	struct pvrdma_cmd_hdr hdr;
	u32 cq_handle;
	u32 cqe;
};

struct pvrdma_cmd_resize_cq_resp {
	struct pvrdma_cmd_resp_hdr hdr;
	u32 cqe;
	u8 reserved[4];
};

struct pvrdma_cmd_destroy_cq {
	struct pvrdma_cmd_hdr hdr;
	u32 cq_handle;
	u8 reserved[4];
};

struct pvrdma_cmd_create_srq {
	struct pvrdma_cmd_hdr hdr;
	u64 pdir_dma;
	u32 pd_handle;
	u32 nchunks;
	struct pvrdma_srq_attr attrs;
	u8 srq_type;
	u8 reserved[7];
};

struct pvrdma_cmd_create_srq_resp {
	struct pvrdma_cmd_resp_hdr hdr;
	u32 srqn;
	u8 reserved[4];
};

struct pvrdma_cmd_modify_srq {
	struct pvrdma_cmd_hdr hdr;
	u32 srq_handle;
	u32 attr_mask;
	struct pvrdma_srq_attr attrs;
};

struct pvrdma_cmd_query_srq {
	struct pvrdma_cmd_hdr hdr;
	u32 srq_handle;
	u8 reserved[4];
};

struct pvrdma_cmd_query_srq_resp {
	struct pvrdma_cmd_resp_hdr hdr;
	struct pvrdma_srq_attr attrs;
};

struct pvrdma_cmd_destroy_srq {
	struct pvrdma_cmd_hdr hdr;
	u32 srq_handle;
	u8 reserved[4];
};

struct pvrdma_cmd_create_qp {
	struct pvrdma_cmd_hdr hdr;
	u64 pdir_dma;
	u32 pd_handle;
	u32 send_cq_handle;
	u32 recv_cq_handle;
	u32 srq_handle;
	u32 max_send_wr;
	u32 max_recv_wr;
	u32 max_send_sge;
	u32 max_recv_sge;
	u32 max_inline_data;
	u32 lkey;
	u32 access_flags;
	u16 total_chunks;
	u16 send_chunks;
	u16 max_atomic_arg;
	u8 sq_sig_all;
	u8 qp_type;
	u8 is_srq;
	u8 reserved[3];
};

struct pvrdma_cmd_create_qp_resp {
	struct pvrdma_cmd_resp_hdr hdr;
	u32 qpn;
	u32 max_send_wr;
	u32 max_recv_wr;
	u32 max_send_sge;
	u32 max_recv_sge;
	u32 max_inline_data;
};

struct pvrdma_cmd_create_qp_resp_v2 {
	struct pvrdma_cmd_resp_hdr hdr;
	u32 qpn;
	u32 qp_handle;
	u32 max_send_wr;
	u32 max_recv_wr;
	u32 max_send_sge;
	u32 max_recv_sge;
	u32 max_inline_data;
};

struct pvrdma_cmd_modify_qp {
	struct pvrdma_cmd_hdr hdr;
	u32 qp_handle;
	u32 attr_mask;
	struct pvrdma_qp_attr attrs;
};

struct pvrdma_cmd_query_qp {
	struct pvrdma_cmd_hdr hdr;
	u32 qp_handle;
	u32 attr_mask;
};

struct pvrdma_cmd_query_qp_resp {
	struct pvrdma_cmd_resp_hdr hdr;
	struct pvrdma_qp_attr attrs;
};

struct pvrdma_cmd_destroy_qp {
	struct pvrdma_cmd_hdr hdr;
	u32 qp_handle;
	u8 reserved[4];
};

struct pvrdma_cmd_destroy_qp_resp {
	struct pvrdma_cmd_resp_hdr hdr;
	u32 events_reported;
	u8 reserved[4];
};

struct pvrdma_cmd_create_bind {
	struct pvrdma_cmd_hdr hdr;
	u32 mtu;
	u32 vlan;
	u32 index;
	u8 new_gid[16];
	u8 gid_type;
	u8 reserved[3];
};

struct pvrdma_cmd_destroy_bind {
	struct pvrdma_cmd_hdr hdr;
	u32 index;
	u8 dest_gid[16];
	u8 reserved[4];
};

union pvrdma_cmd_req {
	struct pvrdma_cmd_hdr hdr;
	struct pvrdma_cmd_query_port query_port;
	struct pvrdma_cmd_query_pkey query_pkey;
	struct pvrdma_cmd_create_uc create_uc;
	struct pvrdma_cmd_destroy_uc destroy_uc;
	struct pvrdma_cmd_create_pd create_pd;
	struct pvrdma_cmd_destroy_pd destroy_pd;
	struct pvrdma_cmd_create_mr create_mr;
	struct pvrdma_cmd_destroy_mr destroy_mr;
	struct pvrdma_cmd_create_cq create_cq;
	struct pvrdma_cmd_resize_cq resize_cq;
	struct pvrdma_cmd_destroy_cq destroy_cq;
	struct pvrdma_cmd_create_qp create_qp;
	struct pvrdma_cmd_modify_qp modify_qp;
	struct pvrdma_cmd_query_qp query_qp;
	struct pvrdma_cmd_destroy_qp destroy_qp;
	struct pvrdma_cmd_create_bind create_bind;
	struct pvrdma_cmd_destroy_bind destroy_bind;
	struct pvrdma_cmd_create_srq create_srq;
	struct pvrdma_cmd_modify_srq modify_srq;
	struct pvrdma_cmd_query_srq query_srq;
	struct pvrdma_cmd_destroy_srq destroy_srq;
};

union pvrdma_cmd_resp {
	struct pvrdma_cmd_resp_hdr hdr;
	struct pvrdma_cmd_query_port_resp query_port_resp;
	struct pvrdma_cmd_query_pkey_resp query_pkey_resp;
	struct pvrdma_cmd_create_uc_resp create_uc_resp;
	struct pvrdma_cmd_create_pd_resp create_pd_resp;
	struct pvrdma_cmd_create_mr_resp create_mr_resp;
	struct pvrdma_cmd_create_cq_resp create_cq_resp;
	struct pvrdma_cmd_resize_cq_resp resize_cq_resp;
	struct pvrdma_cmd_create_qp_resp create_qp_resp;
	struct pvrdma_cmd_create_qp_resp_v2 create_qp_resp_v2;
	struct pvrdma_cmd_query_qp_resp query_qp_resp;
	struct pvrdma_cmd_destroy_qp_resp destroy_qp_resp;
	struct pvrdma_cmd_create_srq_resp create_srq_resp;
	struct pvrdma_cmd_query_srq_resp query_srq_resp;
};

#endif  
