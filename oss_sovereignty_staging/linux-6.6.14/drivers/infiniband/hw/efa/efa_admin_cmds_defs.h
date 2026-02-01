 
 

#ifndef _EFA_ADMIN_CMDS_H_
#define _EFA_ADMIN_CMDS_H_

#define EFA_ADMIN_API_VERSION_MAJOR          0
#define EFA_ADMIN_API_VERSION_MINOR          1

 
enum efa_admin_aq_opcode {
	EFA_ADMIN_CREATE_QP                         = 1,
	EFA_ADMIN_MODIFY_QP                         = 2,
	EFA_ADMIN_QUERY_QP                          = 3,
	EFA_ADMIN_DESTROY_QP                        = 4,
	EFA_ADMIN_CREATE_AH                         = 5,
	EFA_ADMIN_DESTROY_AH                        = 6,
	EFA_ADMIN_REG_MR                            = 7,
	EFA_ADMIN_DEREG_MR                          = 8,
	EFA_ADMIN_CREATE_CQ                         = 9,
	EFA_ADMIN_DESTROY_CQ                        = 10,
	EFA_ADMIN_GET_FEATURE                       = 11,
	EFA_ADMIN_SET_FEATURE                       = 12,
	EFA_ADMIN_GET_STATS                         = 13,
	EFA_ADMIN_ALLOC_PD                          = 14,
	EFA_ADMIN_DEALLOC_PD                        = 15,
	EFA_ADMIN_ALLOC_UAR                         = 16,
	EFA_ADMIN_DEALLOC_UAR                       = 17,
	EFA_ADMIN_CREATE_EQ                         = 18,
	EFA_ADMIN_DESTROY_EQ                        = 19,
	EFA_ADMIN_MAX_OPCODE                        = 19,
};

enum efa_admin_aq_feature_id {
	EFA_ADMIN_DEVICE_ATTR                       = 1,
	EFA_ADMIN_AENQ_CONFIG                       = 2,
	EFA_ADMIN_NETWORK_ATTR                      = 3,
	EFA_ADMIN_QUEUE_ATTR                        = 4,
	EFA_ADMIN_HW_HINTS                          = 5,
	EFA_ADMIN_HOST_INFO                         = 6,
	EFA_ADMIN_EVENT_QUEUE_ATTR                  = 7,
};

 
enum efa_admin_qp_type {
	 
	EFA_ADMIN_QP_TYPE_UD                        = 1,
	 
	EFA_ADMIN_QP_TYPE_SRD                       = 2,
};

 
enum efa_admin_qp_state {
	EFA_ADMIN_QP_STATE_RESET                    = 0,
	EFA_ADMIN_QP_STATE_INIT                     = 1,
	EFA_ADMIN_QP_STATE_RTR                      = 2,
	EFA_ADMIN_QP_STATE_RTS                      = 3,
	EFA_ADMIN_QP_STATE_SQD                      = 4,
	EFA_ADMIN_QP_STATE_SQE                      = 5,
	EFA_ADMIN_QP_STATE_ERR                      = 6,
};

enum efa_admin_get_stats_type {
	EFA_ADMIN_GET_STATS_TYPE_BASIC              = 0,
	EFA_ADMIN_GET_STATS_TYPE_MESSAGES           = 1,
	EFA_ADMIN_GET_STATS_TYPE_RDMA_READ          = 2,
	EFA_ADMIN_GET_STATS_TYPE_RDMA_WRITE         = 3,
};

enum efa_admin_get_stats_scope {
	EFA_ADMIN_GET_STATS_SCOPE_ALL               = 0,
	EFA_ADMIN_GET_STATS_SCOPE_QUEUE             = 1,
};

 
struct efa_admin_qp_alloc_size {
	 
	u32 send_queue_ring_size;

	 
	u32 send_queue_depth;

	 
	u32 recv_queue_ring_size;

	 
	u32 recv_queue_depth;
};

struct efa_admin_create_qp_cmd {
	 
	struct efa_admin_aq_common_desc aq_common_desc;

	 
	u16 pd;

	 
	u8 qp_type;

	 
	u8 flags;

	 
	u64 sq_base_addr;

	 
	u64 rq_base_addr;

	 
	u32 send_cq_idx;

	 
	u32 recv_cq_idx;

	 
	u32 sq_l_key;

	 
	u32 rq_l_key;

	 
	struct efa_admin_qp_alloc_size qp_alloc_size;

	 
	u16 uar;

	 
	u16 reserved;

	 
	u32 reserved2;
};

struct efa_admin_create_qp_resp {
	 
	struct efa_admin_acq_common_desc acq_common_desc;

	 
	u32 qp_handle;

	 
	u16 qp_num;

	 
	u16 reserved;

	 
	u16 send_sub_cq_idx;

	 
	u16 recv_sub_cq_idx;

	 
	u32 sq_db_offset;

	 
	u32 rq_db_offset;

	 
	u32 llq_descriptors_offset;
};

struct efa_admin_modify_qp_cmd {
	 
	struct efa_admin_aq_common_desc aq_common_desc;

	 
	u32 modify_mask;

	 
	u32 qp_handle;

	 
	u32 qp_state;

	 
	u32 cur_qp_state;

	 
	u32 qkey;

	 
	u32 sq_psn;

	 
	u8 sq_drained_async_notify;

	 
	u8 rnr_retry;

	 
	u16 reserved2;
};

struct efa_admin_modify_qp_resp {
	 
	struct efa_admin_acq_common_desc acq_common_desc;
};

struct efa_admin_query_qp_cmd {
	 
	struct efa_admin_aq_common_desc aq_common_desc;

	 
	u32 qp_handle;
};

struct efa_admin_query_qp_resp {
	 
	struct efa_admin_acq_common_desc acq_common_desc;

	 
	u32 qp_state;

	 
	u32 qkey;

	 
	u32 sq_psn;

	 
	u8 sq_draining;

	 
	u8 rnr_retry;

	 
	u16 reserved2;
};

struct efa_admin_destroy_qp_cmd {
	 
	struct efa_admin_aq_common_desc aq_common_desc;

	 
	u32 qp_handle;
};

struct efa_admin_destroy_qp_resp {
	 
	struct efa_admin_acq_common_desc acq_common_desc;
};

 
struct efa_admin_create_ah_cmd {
	 
	struct efa_admin_aq_common_desc aq_common_desc;

	 
	u8 dest_addr[16];

	 
	u16 pd;

	 
	u16 reserved;
};

struct efa_admin_create_ah_resp {
	 
	struct efa_admin_acq_common_desc acq_common_desc;

	 
	u16 ah;

	 
	u16 reserved;
};

struct efa_admin_destroy_ah_cmd {
	 
	struct efa_admin_aq_common_desc aq_common_desc;

	 
	u16 ah;

	 
	u16 pd;
};

struct efa_admin_destroy_ah_resp {
	 
	struct efa_admin_acq_common_desc acq_common_desc;
};

 
struct efa_admin_reg_mr_cmd {
	 
	struct efa_admin_aq_common_desc aq_common_desc;

	 
	u16 pd;

	 
	u16 reserved16_w1;

	 
	union {
		 
		u64 inline_pbl_array[4];

		 
		struct efa_admin_ctrl_buff_info pbl;
	} pbl;

	 
	u64 mr_length;

	 
	u8 flags;

	 
	u8 permissions;

	 
	u16 reserved16_w5;

	 
	u32 page_num;

	 
	u64 iova;
};

struct efa_admin_reg_mr_resp {
	 
	struct efa_admin_acq_common_desc acq_common_desc;

	 
	u32 l_key;

	 
	u32 r_key;
};

struct efa_admin_dereg_mr_cmd {
	 
	struct efa_admin_aq_common_desc aq_common_desc;

	 
	u32 l_key;
};

struct efa_admin_dereg_mr_resp {
	 
	struct efa_admin_acq_common_desc acq_common_desc;
};

struct efa_admin_create_cq_cmd {
	struct efa_admin_aq_common_desc aq_common_desc;

	 
	u8 cq_caps_1;

	 
	u8 cq_caps_2;

	 
	u16 cq_depth;

	 
	u16 eqn;

	 
	u16 reserved;

	 
	struct efa_common_mem_addr cq_ba;

	 
	u32 l_key;

	 
	u16 num_sub_cqs;

	 
	u16 uar;
};

struct efa_admin_create_cq_resp {
	struct efa_admin_acq_common_desc acq_common_desc;

	u16 cq_idx;

	 
	u16 cq_actual_depth;

	 
	u32 db_offset;

	 
	u32 flags;
};

struct efa_admin_destroy_cq_cmd {
	struct efa_admin_aq_common_desc aq_common_desc;

	u16 cq_idx;

	 
	u16 reserved1;
};

struct efa_admin_destroy_cq_resp {
	struct efa_admin_acq_common_desc acq_common_desc;
};

 
struct efa_admin_aq_get_stats_cmd {
	struct efa_admin_aq_common_desc aq_common_descriptor;

	union {
		 
		u32 inline_data_w1[3];

		struct efa_admin_ctrl_buff_info control_buffer;
	} u;

	 
	u8 type;

	 
	u8 scope;

	u16 scope_modifier;
};

struct efa_admin_basic_stats {
	u64 tx_bytes;

	u64 tx_pkts;

	u64 rx_bytes;

	u64 rx_pkts;

	u64 rx_drops;
};

struct efa_admin_messages_stats {
	u64 send_bytes;

	u64 send_wrs;

	u64 recv_bytes;

	u64 recv_wrs;
};

struct efa_admin_rdma_read_stats {
	u64 read_wrs;

	u64 read_bytes;

	u64 read_wr_err;

	u64 read_resp_bytes;
};

struct efa_admin_rdma_write_stats {
	u64 write_wrs;

	u64 write_bytes;

	u64 write_wr_err;

	u64 write_recv_bytes;
};

struct efa_admin_acq_get_stats_resp {
	struct efa_admin_acq_common_desc acq_common_desc;

	union {
		struct efa_admin_basic_stats basic_stats;

		struct efa_admin_messages_stats messages_stats;

		struct efa_admin_rdma_read_stats rdma_read_stats;

		struct efa_admin_rdma_write_stats rdma_write_stats;
	} u;
};

struct efa_admin_get_set_feature_common_desc {
	 
	u8 reserved0;

	 
	u8 feature_id;

	 
	u16 reserved16;
};

struct efa_admin_feature_device_attr_desc {
	 
	u64 supported_features;

	 
	u64 page_size_cap;

	u32 fw_version;

	u32 admin_api_version;

	u32 device_version;

	 
	u16 db_bar;

	 
	u8 phys_addr_width;

	 
	u8 virt_addr_width;

	 
	u32 device_caps;

	 
	u32 max_rdma_size;
};

struct efa_admin_feature_queue_attr_desc {
	 
	u32 max_qp;

	 
	u32 max_sq_depth;

	 
	u32 inline_buf_size;

	 
	u32 max_rq_depth;

	 
	u32 max_cq;

	 
	u32 max_cq_depth;

	 
	u16 sub_cqs_per_cq;

	 
	u16 min_sq_depth;

	 
	u16 max_wr_send_sges;

	 
	u16 max_wr_recv_sges;

	 
	u32 max_mr;

	 
	u32 max_mr_pages;

	 
	u32 max_pd;

	 
	u32 max_ah;

	 
	u32 max_llq_size;

	 
	u16 max_wr_rdma_sges;

	 
	u16 max_tx_batch;
};

struct efa_admin_event_queue_attr_desc {
	 
	u32 max_eq;

	 
	u32 max_eq_depth;

	 
	u32 event_bitmask;
};

struct efa_admin_feature_aenq_desc {
	 
	u32 supported_groups;

	 
	u32 enabled_groups;
};

struct efa_admin_feature_network_attr_desc {
	 
	u8 addr[16];

	 
	u32 mtu;
};

 
struct efa_admin_hw_hints {
	 
	u16 mmio_read_timeout;

	 
	u16 driver_watchdog_timeout;

	 
	u16 admin_completion_timeout;

	 
	u16 poll_interval;
};

struct efa_admin_get_feature_cmd {
	struct efa_admin_aq_common_desc aq_common_descriptor;

	struct efa_admin_ctrl_buff_info control_buffer;

	struct efa_admin_get_set_feature_common_desc feature_common;

	u32 raw[11];
};

struct efa_admin_get_feature_resp {
	struct efa_admin_acq_common_desc acq_common_desc;

	union {
		u32 raw[14];

		struct efa_admin_feature_device_attr_desc device_attr;

		struct efa_admin_feature_aenq_desc aenq;

		struct efa_admin_feature_network_attr_desc network_attr;

		struct efa_admin_feature_queue_attr_desc queue_attr;

		struct efa_admin_event_queue_attr_desc event_queue_attr;

		struct efa_admin_hw_hints hw_hints;
	} u;
};

struct efa_admin_set_feature_cmd {
	struct efa_admin_aq_common_desc aq_common_descriptor;

	struct efa_admin_ctrl_buff_info control_buffer;

	struct efa_admin_get_set_feature_common_desc feature_common;

	union {
		u32 raw[11];

		 
		struct efa_admin_feature_aenq_desc aenq;
	} u;
};

struct efa_admin_set_feature_resp {
	struct efa_admin_acq_common_desc acq_common_desc;

	union {
		u32 raw[14];
	} u;
};

struct efa_admin_alloc_pd_cmd {
	struct efa_admin_aq_common_desc aq_common_descriptor;
};

struct efa_admin_alloc_pd_resp {
	struct efa_admin_acq_common_desc acq_common_desc;

	 
	u16 pd;

	 
	u16 reserved;
};

struct efa_admin_dealloc_pd_cmd {
	struct efa_admin_aq_common_desc aq_common_descriptor;

	 
	u16 pd;

	 
	u16 reserved;
};

struct efa_admin_dealloc_pd_resp {
	struct efa_admin_acq_common_desc acq_common_desc;
};

struct efa_admin_alloc_uar_cmd {
	struct efa_admin_aq_common_desc aq_common_descriptor;
};

struct efa_admin_alloc_uar_resp {
	struct efa_admin_acq_common_desc acq_common_desc;

	 
	u16 uar;

	 
	u16 reserved;
};

struct efa_admin_dealloc_uar_cmd {
	struct efa_admin_aq_common_desc aq_common_descriptor;

	 
	u16 uar;

	 
	u16 reserved;
};

struct efa_admin_dealloc_uar_resp {
	struct efa_admin_acq_common_desc acq_common_desc;
};

struct efa_admin_create_eq_cmd {
	struct efa_admin_aq_common_desc aq_common_descriptor;

	 
	u16 depth;

	 
	u8 msix_vec;

	 
	u8 caps;

	 
	struct efa_common_mem_addr ba;

	 
	u32 event_bitmask;

	 
	u32 reserved;
};

struct efa_admin_create_eq_resp {
	struct efa_admin_acq_common_desc acq_common_desc;

	 
	u16 eqn;

	 
	u16 reserved;
};

struct efa_admin_destroy_eq_cmd {
	struct efa_admin_aq_common_desc aq_common_descriptor;

	 
	u16 eqn;

	 
	u16 reserved;
};

struct efa_admin_destroy_eq_resp {
	struct efa_admin_acq_common_desc acq_common_desc;
};

 
enum efa_admin_aenq_group {
	EFA_ADMIN_FATAL_ERROR                       = 1,
	EFA_ADMIN_WARNING                           = 2,
	EFA_ADMIN_NOTIFICATION                      = 3,
	EFA_ADMIN_KEEP_ALIVE                        = 4,
	EFA_ADMIN_AENQ_GROUPS_NUM                   = 5,
};

struct efa_admin_mmio_req_read_less_resp {
	u16 req_id;

	u16 reg_off;

	 
	u32 reg_val;
};

enum efa_admin_os_type {
	EFA_ADMIN_OS_LINUX                          = 0,
};

struct efa_admin_host_info {
	 
	u8 os_dist_str[128];

	 
	u32 os_type;

	 
	u8 kernel_ver_str[32];

	 
	u32 kernel_ver;

	 
	u32 driver_ver;

	 
	u16 bdf;

	 
	u16 spec_ver;

	 
	u32 flags;
};

 
#define EFA_ADMIN_CREATE_QP_CMD_SQ_VIRT_MASK                BIT(0)
#define EFA_ADMIN_CREATE_QP_CMD_RQ_VIRT_MASK                BIT(1)

 
#define EFA_ADMIN_MODIFY_QP_CMD_QP_STATE_MASK               BIT(0)
#define EFA_ADMIN_MODIFY_QP_CMD_CUR_QP_STATE_MASK           BIT(1)
#define EFA_ADMIN_MODIFY_QP_CMD_QKEY_MASK                   BIT(2)
#define EFA_ADMIN_MODIFY_QP_CMD_SQ_PSN_MASK                 BIT(3)
#define EFA_ADMIN_MODIFY_QP_CMD_SQ_DRAINED_ASYNC_NOTIFY_MASK BIT(4)
#define EFA_ADMIN_MODIFY_QP_CMD_RNR_RETRY_MASK              BIT(5)

 
#define EFA_ADMIN_REG_MR_CMD_PHYS_PAGE_SIZE_SHIFT_MASK      GENMASK(4, 0)
#define EFA_ADMIN_REG_MR_CMD_MEM_ADDR_PHY_MODE_EN_MASK      BIT(7)
#define EFA_ADMIN_REG_MR_CMD_LOCAL_WRITE_ENABLE_MASK        BIT(0)
#define EFA_ADMIN_REG_MR_CMD_REMOTE_WRITE_ENABLE_MASK       BIT(1)
#define EFA_ADMIN_REG_MR_CMD_REMOTE_READ_ENABLE_MASK        BIT(2)

 
#define EFA_ADMIN_CREATE_CQ_CMD_INTERRUPT_MODE_ENABLED_MASK BIT(5)
#define EFA_ADMIN_CREATE_CQ_CMD_VIRT_MASK                   BIT(6)
#define EFA_ADMIN_CREATE_CQ_CMD_CQ_ENTRY_SIZE_WORDS_MASK    GENMASK(4, 0)
#define EFA_ADMIN_CREATE_CQ_CMD_SET_SRC_ADDR_MASK           BIT(5)

 
#define EFA_ADMIN_CREATE_CQ_RESP_DB_VALID_MASK              BIT(0)

 
#define EFA_ADMIN_FEATURE_DEVICE_ATTR_DESC_RDMA_READ_MASK   BIT(0)
#define EFA_ADMIN_FEATURE_DEVICE_ATTR_DESC_RNR_RETRY_MASK   BIT(1)
#define EFA_ADMIN_FEATURE_DEVICE_ATTR_DESC_DATA_POLLING_128_MASK BIT(2)
#define EFA_ADMIN_FEATURE_DEVICE_ATTR_DESC_RDMA_WRITE_MASK  BIT(3)

 
#define EFA_ADMIN_CREATE_EQ_CMD_ENTRY_SIZE_WORDS_MASK       GENMASK(4, 0)
#define EFA_ADMIN_CREATE_EQ_CMD_VIRT_MASK                   BIT(6)
#define EFA_ADMIN_CREATE_EQ_CMD_COMPLETION_EVENTS_MASK      BIT(0)

 
#define EFA_ADMIN_HOST_INFO_DRIVER_MODULE_TYPE_MASK         GENMASK(7, 0)
#define EFA_ADMIN_HOST_INFO_DRIVER_SUB_MINOR_MASK           GENMASK(15, 8)
#define EFA_ADMIN_HOST_INFO_DRIVER_MINOR_MASK               GENMASK(23, 16)
#define EFA_ADMIN_HOST_INFO_DRIVER_MAJOR_MASK               GENMASK(31, 24)
#define EFA_ADMIN_HOST_INFO_FUNCTION_MASK                   GENMASK(2, 0)
#define EFA_ADMIN_HOST_INFO_DEVICE_MASK                     GENMASK(7, 3)
#define EFA_ADMIN_HOST_INFO_BUS_MASK                        GENMASK(15, 8)
#define EFA_ADMIN_HOST_INFO_SPEC_MINOR_MASK                 GENMASK(7, 0)
#define EFA_ADMIN_HOST_INFO_SPEC_MAJOR_MASK                 GENMASK(15, 8)
#define EFA_ADMIN_HOST_INFO_INTREE_MASK                     BIT(0)
#define EFA_ADMIN_HOST_INFO_GDR_MASK                        BIT(1)

#endif  
