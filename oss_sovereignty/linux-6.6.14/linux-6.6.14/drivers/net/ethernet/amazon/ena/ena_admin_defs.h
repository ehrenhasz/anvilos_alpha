#ifndef _ENA_ADMIN_H_
#define _ENA_ADMIN_H_
#define ENA_ADMIN_RSS_KEY_PARTS              10
enum ena_admin_aq_opcode {
	ENA_ADMIN_CREATE_SQ                         = 1,
	ENA_ADMIN_DESTROY_SQ                        = 2,
	ENA_ADMIN_CREATE_CQ                         = 3,
	ENA_ADMIN_DESTROY_CQ                        = 4,
	ENA_ADMIN_GET_FEATURE                       = 8,
	ENA_ADMIN_SET_FEATURE                       = 9,
	ENA_ADMIN_GET_STATS                         = 11,
};
enum ena_admin_aq_completion_status {
	ENA_ADMIN_SUCCESS                           = 0,
	ENA_ADMIN_RESOURCE_ALLOCATION_FAILURE       = 1,
	ENA_ADMIN_BAD_OPCODE                        = 2,
	ENA_ADMIN_UNSUPPORTED_OPCODE                = 3,
	ENA_ADMIN_MALFORMED_REQUEST                 = 4,
	ENA_ADMIN_ILLEGAL_PARAMETER                 = 5,
	ENA_ADMIN_UNKNOWN_ERROR                     = 6,
	ENA_ADMIN_RESOURCE_BUSY                     = 7,
};
enum ena_admin_aq_feature_id {
	ENA_ADMIN_DEVICE_ATTRIBUTES                 = 1,
	ENA_ADMIN_MAX_QUEUES_NUM                    = 2,
	ENA_ADMIN_HW_HINTS                          = 3,
	ENA_ADMIN_LLQ                               = 4,
	ENA_ADMIN_MAX_QUEUES_EXT                    = 7,
	ENA_ADMIN_RSS_HASH_FUNCTION                 = 10,
	ENA_ADMIN_STATELESS_OFFLOAD_CONFIG          = 11,
	ENA_ADMIN_RSS_INDIRECTION_TABLE_CONFIG      = 12,
	ENA_ADMIN_MTU                               = 14,
	ENA_ADMIN_RSS_HASH_INPUT                    = 18,
	ENA_ADMIN_INTERRUPT_MODERATION              = 20,
	ENA_ADMIN_AENQ_CONFIG                       = 26,
	ENA_ADMIN_LINK_CONFIG                       = 27,
	ENA_ADMIN_HOST_ATTR_CONFIG                  = 28,
	ENA_ADMIN_FEATURES_OPCODE_NUM               = 32,
};
enum ena_admin_aq_caps_id {
	ENA_ADMIN_ENI_STATS                         = 0,
};
enum ena_admin_placement_policy_type {
	ENA_ADMIN_PLACEMENT_POLICY_HOST             = 1,
	ENA_ADMIN_PLACEMENT_POLICY_DEV              = 3,
};
enum ena_admin_link_types {
	ENA_ADMIN_LINK_SPEED_1G                     = 0x1,
	ENA_ADMIN_LINK_SPEED_2_HALF_G               = 0x2,
	ENA_ADMIN_LINK_SPEED_5G                     = 0x4,
	ENA_ADMIN_LINK_SPEED_10G                    = 0x8,
	ENA_ADMIN_LINK_SPEED_25G                    = 0x10,
	ENA_ADMIN_LINK_SPEED_40G                    = 0x20,
	ENA_ADMIN_LINK_SPEED_50G                    = 0x40,
	ENA_ADMIN_LINK_SPEED_100G                   = 0x80,
	ENA_ADMIN_LINK_SPEED_200G                   = 0x100,
	ENA_ADMIN_LINK_SPEED_400G                   = 0x200,
};
enum ena_admin_completion_policy_type {
	ENA_ADMIN_COMPLETION_POLICY_DESC            = 0,
	ENA_ADMIN_COMPLETION_POLICY_DESC_ON_DEMAND  = 1,
	ENA_ADMIN_COMPLETION_POLICY_HEAD_ON_DEMAND  = 2,
	ENA_ADMIN_COMPLETION_POLICY_HEAD            = 3,
};
enum ena_admin_get_stats_type {
	ENA_ADMIN_GET_STATS_TYPE_BASIC              = 0,
	ENA_ADMIN_GET_STATS_TYPE_EXTENDED           = 1,
	ENA_ADMIN_GET_STATS_TYPE_ENI                = 2,
};
enum ena_admin_get_stats_scope {
	ENA_ADMIN_SPECIFIC_QUEUE                    = 0,
	ENA_ADMIN_ETH_TRAFFIC                       = 1,
};
struct ena_admin_aq_common_desc {
	u16 command_id;
	u8 opcode;
	u8 flags;
};
struct ena_admin_ctrl_buff_info {
	u32 length;
	struct ena_common_mem_addr address;
};
struct ena_admin_sq {
	u16 sq_idx;
	u8 sq_identity;
	u8 reserved1;
};
struct ena_admin_aq_entry {
	struct ena_admin_aq_common_desc aq_common_descriptor;
	union {
		u32 inline_data_w1[3];
		struct ena_admin_ctrl_buff_info control_buffer;
	} u;
	u32 inline_data_w4[12];
};
struct ena_admin_acq_common_desc {
	u16 command;
	u8 status;
	u8 flags;
	u16 extended_status;
	u16 sq_head_indx;
};
struct ena_admin_acq_entry {
	struct ena_admin_acq_common_desc acq_common_descriptor;
	u32 response_specific_data[14];
};
struct ena_admin_aq_create_sq_cmd {
	struct ena_admin_aq_common_desc aq_common_descriptor;
	u8 sq_identity;
	u8 reserved8_w1;
	u8 sq_caps_2;
	u8 sq_caps_3;
	u16 cq_idx;
	u16 sq_depth;
	struct ena_common_mem_addr sq_ba;
	struct ena_common_mem_addr sq_head_writeback;
	u32 reserved0_w7;
	u32 reserved0_w8;
};
enum ena_admin_sq_direction {
	ENA_ADMIN_SQ_DIRECTION_TX                   = 1,
	ENA_ADMIN_SQ_DIRECTION_RX                   = 2,
};
struct ena_admin_acq_create_sq_resp_desc {
	struct ena_admin_acq_common_desc acq_common_desc;
	u16 sq_idx;
	u16 reserved;
	u32 sq_doorbell_offset;
	u32 llq_descriptors_offset;
	u32 llq_headers_offset;
};
struct ena_admin_aq_destroy_sq_cmd {
	struct ena_admin_aq_common_desc aq_common_descriptor;
	struct ena_admin_sq sq;
};
struct ena_admin_acq_destroy_sq_resp_desc {
	struct ena_admin_acq_common_desc acq_common_desc;
};
struct ena_admin_aq_create_cq_cmd {
	struct ena_admin_aq_common_desc aq_common_descriptor;
	u8 cq_caps_1;
	u8 cq_caps_2;
	u16 cq_depth;
	u32 msix_vector;
	struct ena_common_mem_addr cq_ba;
};
struct ena_admin_acq_create_cq_resp_desc {
	struct ena_admin_acq_common_desc acq_common_desc;
	u16 cq_idx;
	u16 cq_actual_depth;
	u32 numa_node_register_offset;
	u32 cq_head_db_register_offset;
	u32 cq_interrupt_unmask_register_offset;
};
struct ena_admin_aq_destroy_cq_cmd {
	struct ena_admin_aq_common_desc aq_common_descriptor;
	u16 cq_idx;
	u16 reserved1;
};
struct ena_admin_acq_destroy_cq_resp_desc {
	struct ena_admin_acq_common_desc acq_common_desc;
};
struct ena_admin_aq_get_stats_cmd {
	struct ena_admin_aq_common_desc aq_common_descriptor;
	union {
		u32 inline_data_w1[3];
		struct ena_admin_ctrl_buff_info control_buffer;
	} u;
	u8 type;
	u8 scope;
	u16 reserved3;
	u16 queue_idx;
	u16 device_id;
};
struct ena_admin_basic_stats {
	u32 tx_bytes_low;
	u32 tx_bytes_high;
	u32 tx_pkts_low;
	u32 tx_pkts_high;
	u32 rx_bytes_low;
	u32 rx_bytes_high;
	u32 rx_pkts_low;
	u32 rx_pkts_high;
	u32 rx_drops_low;
	u32 rx_drops_high;
	u32 tx_drops_low;
	u32 tx_drops_high;
};
struct ena_admin_eni_stats {
	u64 bw_in_allowance_exceeded;
	u64 bw_out_allowance_exceeded;
	u64 pps_allowance_exceeded;
	u64 conntrack_allowance_exceeded;
	u64 linklocal_allowance_exceeded;
};
struct ena_admin_acq_get_stats_resp {
	struct ena_admin_acq_common_desc acq_common_desc;
	union {
		u64 raw[7];
		struct ena_admin_basic_stats basic_stats;
		struct ena_admin_eni_stats eni_stats;
	} u;
};
struct ena_admin_get_set_feature_common_desc {
	u8 flags;
	u8 feature_id;
	u8 feature_version;
	u8 reserved8;
};
struct ena_admin_device_attr_feature_desc {
	u32 impl_id;
	u32 device_version;
	u32 supported_features;
	u32 capabilities;
	u32 phys_addr_width;
	u32 virt_addr_width;
	u8 mac_addr[6];
	u8 reserved7[2];
	u32 max_mtu;
};
enum ena_admin_llq_header_location {
	ENA_ADMIN_INLINE_HEADER                     = 1,
	ENA_ADMIN_HEADER_RING                       = 2,
};
enum ena_admin_llq_ring_entry_size {
	ENA_ADMIN_LIST_ENTRY_SIZE_128B              = 1,
	ENA_ADMIN_LIST_ENTRY_SIZE_192B              = 2,
	ENA_ADMIN_LIST_ENTRY_SIZE_256B              = 4,
};
enum ena_admin_llq_num_descs_before_header {
	ENA_ADMIN_LLQ_NUM_DESCS_BEFORE_HEADER_0     = 0,
	ENA_ADMIN_LLQ_NUM_DESCS_BEFORE_HEADER_1     = 1,
	ENA_ADMIN_LLQ_NUM_DESCS_BEFORE_HEADER_2     = 2,
	ENA_ADMIN_LLQ_NUM_DESCS_BEFORE_HEADER_4     = 4,
	ENA_ADMIN_LLQ_NUM_DESCS_BEFORE_HEADER_8     = 8,
};
enum ena_admin_llq_stride_ctrl {
	ENA_ADMIN_SINGLE_DESC_PER_ENTRY             = 1,
	ENA_ADMIN_MULTIPLE_DESCS_PER_ENTRY          = 2,
};
enum ena_admin_accel_mode_feat {
	ENA_ADMIN_DISABLE_META_CACHING              = 0,
	ENA_ADMIN_LIMIT_TX_BURST                    = 1,
};
struct ena_admin_accel_mode_get {
	u16 supported_flags;
	u16 max_tx_burst_size;
};
struct ena_admin_accel_mode_set {
	u16 enabled_flags;
	u16 reserved;
};
struct ena_admin_accel_mode_req {
	union {
		u32 raw[2];
		struct ena_admin_accel_mode_get get;
		struct ena_admin_accel_mode_set set;
	} u;
};
struct ena_admin_feature_llq_desc {
	u32 max_llq_num;
	u32 max_llq_depth;
	u16 header_location_ctrl_supported;
	u16 header_location_ctrl_enabled;
	u16 entry_size_ctrl_supported;
	u16 entry_size_ctrl_enabled;
	u16 desc_num_before_header_supported;
	u16 desc_num_before_header_enabled;
	u16 descriptors_stride_ctrl_supported;
	u16 descriptors_stride_ctrl_enabled;
	u32 reserved1;
	struct ena_admin_accel_mode_req accel_mode;
};
struct ena_admin_queue_ext_feature_fields {
	u32 max_tx_sq_num;
	u32 max_tx_cq_num;
	u32 max_rx_sq_num;
	u32 max_rx_cq_num;
	u32 max_tx_sq_depth;
	u32 max_tx_cq_depth;
	u32 max_rx_sq_depth;
	u32 max_rx_cq_depth;
	u32 max_tx_header_size;
	u16 max_per_packet_tx_descs;
	u16 max_per_packet_rx_descs;
};
struct ena_admin_queue_feature_desc {
	u32 max_sq_num;
	u32 max_sq_depth;
	u32 max_cq_num;
	u32 max_cq_depth;
	u32 max_legacy_llq_num;
	u32 max_legacy_llq_depth;
	u32 max_header_size;
	u16 max_packet_tx_descs;
	u16 max_packet_rx_descs;
};
struct ena_admin_set_feature_mtu_desc {
	u32 mtu;
};
struct ena_admin_set_feature_host_attr_desc {
	struct ena_common_mem_addr os_info_ba;
	struct ena_common_mem_addr debug_ba;
	u32 debug_area_size;
};
struct ena_admin_feature_intr_moder_desc {
	u16 intr_delay_resolution;
	u16 reserved;
};
struct ena_admin_get_feature_link_desc {
	u32 speed;
	u32 supported;
	u32 flags;
};
struct ena_admin_feature_aenq_desc {
	u32 supported_groups;
	u32 enabled_groups;
};
struct ena_admin_feature_offload_desc {
	u32 tx;
	u32 rx_supported;
	u32 rx_enabled;
};
enum ena_admin_hash_functions {
	ENA_ADMIN_TOEPLITZ                          = 1,
	ENA_ADMIN_CRC32                             = 2,
};
struct ena_admin_feature_rss_flow_hash_control {
	u32 key_parts;
	u32 reserved;
	u32 key[ENA_ADMIN_RSS_KEY_PARTS];
};
struct ena_admin_feature_rss_flow_hash_function {
	u32 supported_func;
	u32 selected_func;
	u32 init_val;
};
enum ena_admin_flow_hash_proto {
	ENA_ADMIN_RSS_TCP4                          = 0,
	ENA_ADMIN_RSS_UDP4                          = 1,
	ENA_ADMIN_RSS_TCP6                          = 2,
	ENA_ADMIN_RSS_UDP6                          = 3,
	ENA_ADMIN_RSS_IP4                           = 4,
	ENA_ADMIN_RSS_IP6                           = 5,
	ENA_ADMIN_RSS_IP4_FRAG                      = 6,
	ENA_ADMIN_RSS_NOT_IP                        = 7,
	ENA_ADMIN_RSS_TCP6_EX                       = 8,
	ENA_ADMIN_RSS_IP6_EX                        = 9,
	ENA_ADMIN_RSS_PROTO_NUM                     = 16,
};
enum ena_admin_flow_hash_fields {
	ENA_ADMIN_RSS_L2_DA                         = BIT(0),
	ENA_ADMIN_RSS_L2_SA                         = BIT(1),
	ENA_ADMIN_RSS_L3_DA                         = BIT(2),
	ENA_ADMIN_RSS_L3_SA                         = BIT(3),
	ENA_ADMIN_RSS_L4_DP                         = BIT(4),
	ENA_ADMIN_RSS_L4_SP                         = BIT(5),
};
struct ena_admin_proto_input {
	u16 fields;
	u16 reserved2;
};
struct ena_admin_feature_rss_hash_control {
	struct ena_admin_proto_input supported_fields[ENA_ADMIN_RSS_PROTO_NUM];
	struct ena_admin_proto_input selected_fields[ENA_ADMIN_RSS_PROTO_NUM];
	struct ena_admin_proto_input reserved2[ENA_ADMIN_RSS_PROTO_NUM];
	struct ena_admin_proto_input reserved3[ENA_ADMIN_RSS_PROTO_NUM];
};
struct ena_admin_feature_rss_flow_hash_input {
	u16 supported_input_sort;
	u16 enabled_input_sort;
};
enum ena_admin_os_type {
	ENA_ADMIN_OS_LINUX                          = 1,
	ENA_ADMIN_OS_WIN                            = 2,
	ENA_ADMIN_OS_DPDK                           = 3,
	ENA_ADMIN_OS_FREEBSD                        = 4,
	ENA_ADMIN_OS_IPXE                           = 5,
	ENA_ADMIN_OS_ESXI                           = 6,
	ENA_ADMIN_OS_GROUPS_NUM                     = 6,
};
struct ena_admin_host_info {
	u32 os_type;
	u8 os_dist_str[128];
	u32 os_dist;
	u8 kernel_ver_str[32];
	u32 kernel_ver;
	u32 driver_version;
	u32 supported_network_features[2];
	u16 ena_spec_version;
	u16 bdf;
	u16 num_cpus;
	u16 reserved;
	u32 driver_supported_features;
};
struct ena_admin_rss_ind_table_entry {
	u16 cq_idx;
	u16 reserved;
};
struct ena_admin_feature_rss_ind_table {
	u16 min_size;
	u16 max_size;
	u16 size;
	u16 reserved;
	u32 inline_index;
	struct ena_admin_rss_ind_table_entry inline_entry;
};
struct ena_admin_ena_hw_hints {
	u16 mmio_read_timeout;
	u16 driver_watchdog_timeout;
	u16 missing_tx_completion_timeout;
	u16 missed_tx_completion_count_threshold_to_reset;
	u16 admin_completion_tx_timeout;
	u16 netdev_wd_timeout;
	u16 max_tx_sgl_size;
	u16 max_rx_sgl_size;
	u16 reserved[8];
};
struct ena_admin_get_feat_cmd {
	struct ena_admin_aq_common_desc aq_common_descriptor;
	struct ena_admin_ctrl_buff_info control_buffer;
	struct ena_admin_get_set_feature_common_desc feat_common;
	u32 raw[11];
};
struct ena_admin_queue_ext_feature_desc {
	u8 version;
	u8 reserved1[3];
	union {
		struct ena_admin_queue_ext_feature_fields max_queue_ext;
		u32 raw[10];
	};
};
struct ena_admin_get_feat_resp {
	struct ena_admin_acq_common_desc acq_common_desc;
	union {
		u32 raw[14];
		struct ena_admin_device_attr_feature_desc dev_attr;
		struct ena_admin_feature_llq_desc llq;
		struct ena_admin_queue_feature_desc max_queue;
		struct ena_admin_queue_ext_feature_desc max_queue_ext;
		struct ena_admin_feature_aenq_desc aenq;
		struct ena_admin_get_feature_link_desc link;
		struct ena_admin_feature_offload_desc offload;
		struct ena_admin_feature_rss_flow_hash_function flow_hash_func;
		struct ena_admin_feature_rss_flow_hash_input flow_hash_input;
		struct ena_admin_feature_rss_ind_table ind_table;
		struct ena_admin_feature_intr_moder_desc intr_moderation;
		struct ena_admin_ena_hw_hints hw_hints;
	} u;
};
struct ena_admin_set_feat_cmd {
	struct ena_admin_aq_common_desc aq_common_descriptor;
	struct ena_admin_ctrl_buff_info control_buffer;
	struct ena_admin_get_set_feature_common_desc feat_common;
	union {
		u32 raw[11];
		struct ena_admin_set_feature_mtu_desc mtu;
		struct ena_admin_set_feature_host_attr_desc host_attr;
		struct ena_admin_feature_aenq_desc aenq;
		struct ena_admin_feature_rss_flow_hash_function flow_hash_func;
		struct ena_admin_feature_rss_flow_hash_input flow_hash_input;
		struct ena_admin_feature_rss_ind_table ind_table;
		struct ena_admin_feature_llq_desc llq;
	} u;
};
struct ena_admin_set_feat_resp {
	struct ena_admin_acq_common_desc acq_common_desc;
	union {
		u32 raw[14];
	} u;
};
struct ena_admin_aenq_common_desc {
	u16 group;
	u16 syndrome;
	u8 flags;
	u8 reserved1[3];
	u32 timestamp_low;
	u32 timestamp_high;
};
enum ena_admin_aenq_group {
	ENA_ADMIN_LINK_CHANGE                       = 0,
	ENA_ADMIN_FATAL_ERROR                       = 1,
	ENA_ADMIN_WARNING                           = 2,
	ENA_ADMIN_NOTIFICATION                      = 3,
	ENA_ADMIN_KEEP_ALIVE                        = 4,
	ENA_ADMIN_AENQ_GROUPS_NUM                   = 5,
};
enum ena_admin_aenq_notification_syndrome {
	ENA_ADMIN_UPDATE_HINTS                      = 2,
};
struct ena_admin_aenq_entry {
	struct ena_admin_aenq_common_desc aenq_common_desc;
	u32 inline_data_w4[12];
};
struct ena_admin_aenq_link_change_desc {
	struct ena_admin_aenq_common_desc aenq_common_desc;
	u32 flags;
};
struct ena_admin_aenq_keep_alive_desc {
	struct ena_admin_aenq_common_desc aenq_common_desc;
	u32 rx_drops_low;
	u32 rx_drops_high;
	u32 tx_drops_low;
	u32 tx_drops_high;
};
struct ena_admin_ena_mmio_req_read_less_resp {
	u16 req_id;
	u16 reg_off;
	u32 reg_val;
};
#define ENA_ADMIN_AQ_COMMON_DESC_COMMAND_ID_MASK            GENMASK(11, 0)
#define ENA_ADMIN_AQ_COMMON_DESC_PHASE_MASK                 BIT(0)
#define ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_SHIFT            1
#define ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_MASK             BIT(1)
#define ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_INDIRECT_SHIFT   2
#define ENA_ADMIN_AQ_COMMON_DESC_CTRL_DATA_INDIRECT_MASK    BIT(2)
#define ENA_ADMIN_SQ_SQ_DIRECTION_SHIFT                     5
#define ENA_ADMIN_SQ_SQ_DIRECTION_MASK                      GENMASK(7, 5)
#define ENA_ADMIN_ACQ_COMMON_DESC_COMMAND_ID_MASK           GENMASK(11, 0)
#define ENA_ADMIN_ACQ_COMMON_DESC_PHASE_MASK                BIT(0)
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_SQ_DIRECTION_SHIFT       5
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_SQ_DIRECTION_MASK        GENMASK(7, 5)
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_PLACEMENT_POLICY_MASK    GENMASK(3, 0)
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_COMPLETION_POLICY_SHIFT  4
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_COMPLETION_POLICY_MASK   GENMASK(6, 4)
#define ENA_ADMIN_AQ_CREATE_SQ_CMD_IS_PHYSICALLY_CONTIGUOUS_MASK BIT(0)
#define ENA_ADMIN_AQ_CREATE_CQ_CMD_INTERRUPT_MODE_ENABLED_SHIFT 5
#define ENA_ADMIN_AQ_CREATE_CQ_CMD_INTERRUPT_MODE_ENABLED_MASK BIT(5)
#define ENA_ADMIN_AQ_CREATE_CQ_CMD_CQ_ENTRY_SIZE_WORDS_MASK GENMASK(4, 0)
#define ENA_ADMIN_GET_SET_FEATURE_COMMON_DESC_SELECT_MASK   GENMASK(1, 0)
#define ENA_ADMIN_GET_FEATURE_LINK_DESC_AUTONEG_MASK        BIT(0)
#define ENA_ADMIN_GET_FEATURE_LINK_DESC_DUPLEX_SHIFT        1
#define ENA_ADMIN_GET_FEATURE_LINK_DESC_DUPLEX_MASK         BIT(1)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L3_CSUM_IPV4_MASK BIT(0)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_SHIFT 1
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_MASK BIT(1)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_FULL_SHIFT 2
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_FULL_MASK BIT(2)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_PART_SHIFT 3
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_PART_MASK BIT(3)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_FULL_SHIFT 4
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_FULL_MASK BIT(4)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_SHIFT       5
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_MASK        BIT(5)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV6_SHIFT       6
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV6_MASK        BIT(6)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_ECN_SHIFT        7
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_ECN_MASK         BIT(7)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L3_CSUM_IPV4_MASK BIT(0)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV4_CSUM_SHIFT 1
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV4_CSUM_MASK BIT(1)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV6_CSUM_SHIFT 2
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV6_CSUM_MASK BIT(2)
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_HASH_SHIFT        3
#define ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_HASH_MASK         BIT(3)
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_FUNCTION_FUNCS_MASK GENMASK(7, 0)
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_FUNCTION_SELECTED_FUNC_MASK GENMASK(7, 0)
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L3_SORT_SHIFT 1
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L3_SORT_MASK  BIT(1)
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L4_SORT_SHIFT 2
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_L4_SORT_MASK  BIT(2)
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L3_SORT_SHIFT 1
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L3_SORT_MASK BIT(1)
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L4_SORT_SHIFT 2
#define ENA_ADMIN_FEATURE_RSS_FLOW_HASH_INPUT_ENABLE_L4_SORT_MASK BIT(2)
#define ENA_ADMIN_HOST_INFO_MAJOR_MASK                      GENMASK(7, 0)
#define ENA_ADMIN_HOST_INFO_MINOR_SHIFT                     8
#define ENA_ADMIN_HOST_INFO_MINOR_MASK                      GENMASK(15, 8)
#define ENA_ADMIN_HOST_INFO_SUB_MINOR_SHIFT                 16
#define ENA_ADMIN_HOST_INFO_SUB_MINOR_MASK                  GENMASK(23, 16)
#define ENA_ADMIN_HOST_INFO_MODULE_TYPE_SHIFT               24
#define ENA_ADMIN_HOST_INFO_MODULE_TYPE_MASK                GENMASK(31, 24)
#define ENA_ADMIN_HOST_INFO_FUNCTION_MASK                   GENMASK(2, 0)
#define ENA_ADMIN_HOST_INFO_DEVICE_SHIFT                    3
#define ENA_ADMIN_HOST_INFO_DEVICE_MASK                     GENMASK(7, 3)
#define ENA_ADMIN_HOST_INFO_BUS_SHIFT                       8
#define ENA_ADMIN_HOST_INFO_BUS_MASK                        GENMASK(15, 8)
#define ENA_ADMIN_HOST_INFO_RX_OFFSET_SHIFT                 1
#define ENA_ADMIN_HOST_INFO_RX_OFFSET_MASK                  BIT(1)
#define ENA_ADMIN_HOST_INFO_INTERRUPT_MODERATION_SHIFT      2
#define ENA_ADMIN_HOST_INFO_INTERRUPT_MODERATION_MASK       BIT(2)
#define ENA_ADMIN_HOST_INFO_RX_BUF_MIRRORING_SHIFT          3
#define ENA_ADMIN_HOST_INFO_RX_BUF_MIRRORING_MASK           BIT(3)
#define ENA_ADMIN_HOST_INFO_RSS_CONFIGURABLE_FUNCTION_KEY_SHIFT 4
#define ENA_ADMIN_HOST_INFO_RSS_CONFIGURABLE_FUNCTION_KEY_MASK BIT(4)
#define ENA_ADMIN_HOST_INFO_RX_PAGE_REUSE_SHIFT             6
#define ENA_ADMIN_HOST_INFO_RX_PAGE_REUSE_MASK              BIT(6)
#define ENA_ADMIN_AENQ_COMMON_DESC_PHASE_MASK               BIT(0)
#define ENA_ADMIN_AENQ_LINK_CHANGE_DESC_LINK_STATUS_MASK    BIT(0)
#endif  
