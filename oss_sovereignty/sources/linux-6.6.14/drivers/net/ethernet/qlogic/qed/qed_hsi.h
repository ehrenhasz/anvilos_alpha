


#ifndef _QED_HSI_H
#define _QED_HSI_H

#include <linux/types.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/qed/common_hsi.h>
#include <linux/qed/storage_common.h>
#include <linux/qed/tcp_common.h>
#include <linux/qed/fcoe_common.h>
#include <linux/qed/eth_common.h>
#include <linux/qed/iscsi_common.h>
#include <linux/qed/nvmetcp_common.h>
#include <linux/qed/iwarp_common.h>
#include <linux/qed/rdma_common.h>
#include <linux/qed/roce_common.h>
#include <linux/qed/qed_fcoe_if.h>

struct qed_hwfn;
struct qed_ptt;


enum common_event_opcode {
	COMMON_EVENT_PF_START,
	COMMON_EVENT_PF_STOP,
	COMMON_EVENT_VF_START,
	COMMON_EVENT_VF_STOP,
	COMMON_EVENT_VF_PF_CHANNEL,
	COMMON_EVENT_VF_FLR,
	COMMON_EVENT_PF_UPDATE,
	COMMON_EVENT_FW_ERROR,
	COMMON_EVENT_RL_UPDATE,
	COMMON_EVENT_EMPTY,
	MAX_COMMON_EVENT_OPCODE
};


enum common_ramrod_cmd_id {
	COMMON_RAMROD_UNUSED,
	COMMON_RAMROD_PF_START,
	COMMON_RAMROD_PF_STOP,
	COMMON_RAMROD_VF_START,
	COMMON_RAMROD_VF_STOP,
	COMMON_RAMROD_PF_UPDATE,
	COMMON_RAMROD_RL_UPDATE,
	COMMON_RAMROD_EMPTY,
	MAX_COMMON_RAMROD_CMD_ID
};


enum core_error_handle {
	LL2_DROP_PACKET,
	LL2_DO_NOTHING,
	LL2_ASSERT,
	MAX_CORE_ERROR_HANDLE
};


enum core_event_opcode {
	CORE_EVENT_TX_QUEUE_START,
	CORE_EVENT_TX_QUEUE_STOP,
	CORE_EVENT_RX_QUEUE_START,
	CORE_EVENT_RX_QUEUE_STOP,
	CORE_EVENT_RX_QUEUE_FLUSH,
	CORE_EVENT_TX_QUEUE_UPDATE,
	CORE_EVENT_QUEUE_STATS_QUERY,
	MAX_CORE_EVENT_OPCODE
};


enum core_l4_pseudo_checksum_mode {
	CORE_L4_PSEUDO_CSUM_CORRECT_LENGTH,
	CORE_L4_PSEUDO_CSUM_ZERO_LENGTH,
	MAX_CORE_L4_PSEUDO_CHECKSUM_MODE
};


enum core_ll2_error_code {
	LL2_OK = 0,
	LL2_ERROR,
	MAX_CORE_LL2_ERROR_CODE
};


struct core_ll2_port_stats {
	struct regpair gsi_invalid_hdr;
	struct regpair gsi_invalid_pkt_length;
	struct regpair gsi_unsupported_pkt_typ;
	struct regpair gsi_crcchksm_error;
};


struct core_ll2_pstorm_per_queue_stat {
	struct regpair sent_ucast_bytes;
	struct regpair sent_mcast_bytes;
	struct regpair sent_bcast_bytes;
	struct regpair sent_ucast_pkts;
	struct regpair sent_mcast_pkts;
	struct regpair sent_bcast_pkts;
	struct regpair error_drop_pkts;
};


struct core_ll2_rx_prod {
	__le16 bd_prod;
	__le16 cqe_prod;
};

struct core_ll2_tstorm_per_queue_stat {
	struct regpair packet_too_big_discard;
	struct regpair no_buff_discard;
};

struct core_ll2_ustorm_per_queue_stat {
	struct regpair rcv_ucast_bytes;
	struct regpair rcv_mcast_bytes;
	struct regpair rcv_bcast_bytes;
	struct regpair rcv_ucast_pkts;
	struct regpair rcv_mcast_pkts;
	struct regpair rcv_bcast_pkts;
};

struct core_ll2_rx_per_queue_stat {
	struct core_ll2_tstorm_per_queue_stat tstorm_stat;
	struct core_ll2_ustorm_per_queue_stat ustorm_stat;
};

struct core_ll2_tx_per_queue_stat {
	struct core_ll2_pstorm_per_queue_stat pstorm_stat;
};


struct core_pwm_prod_update_data {
	__le16 icid; 
	u8 reserved0;
	u8 params;
#define CORE_PWM_PROD_UPDATE_DATA_AGG_CMD_MASK	  0x3
#define CORE_PWM_PROD_UPDATE_DATA_AGG_CMD_SHIFT   0
#define CORE_PWM_PROD_UPDATE_DATA_RESERVED1_MASK  0x3F	
#define CORE_PWM_PROD_UPDATE_DATA_RESERVED1_SHIFT 2
	struct core_ll2_rx_prod prod; 
};


struct core_queue_stats_query_ramrod_data {
	u8 rx_stat;
	u8 tx_stat;
	__le16 reserved[3];
	struct regpair rx_stat_addr;
	struct regpair tx_stat_addr;
};


enum core_ramrod_cmd_id {
	CORE_RAMROD_UNUSED,
	CORE_RAMROD_RX_QUEUE_START,
	CORE_RAMROD_TX_QUEUE_START,
	CORE_RAMROD_RX_QUEUE_STOP,
	CORE_RAMROD_TX_QUEUE_STOP,
	CORE_RAMROD_RX_QUEUE_FLUSH,
	CORE_RAMROD_TX_QUEUE_UPDATE,
	CORE_RAMROD_QUEUE_STATS_QUERY,
	MAX_CORE_RAMROD_CMD_ID
};


enum core_roce_flavor_type {
	CORE_ROCE,
	CORE_RROCE,
	MAX_CORE_ROCE_FLAVOR_TYPE
};


struct core_rx_action_on_error {
	u8 error_type;
#define CORE_RX_ACTION_ON_ERROR_PACKET_TOO_BIG_MASK	0x3
#define CORE_RX_ACTION_ON_ERROR_PACKET_TOO_BIG_SHIFT	0
#define CORE_RX_ACTION_ON_ERROR_NO_BUFF_MASK		0x3
#define CORE_RX_ACTION_ON_ERROR_NO_BUFF_SHIFT		2
#define CORE_RX_ACTION_ON_ERROR_RESERVED_MASK		0xF
#define CORE_RX_ACTION_ON_ERROR_RESERVED_SHIFT		4
};


struct core_rx_bd {
	struct regpair addr;
	__le16 reserved[4];
};


struct core_rx_bd_with_buff_len {
	struct regpair addr;
	__le16 buff_length;
	__le16 reserved[3];
};


union core_rx_bd_union {
	struct core_rx_bd rx_bd;
	struct core_rx_bd_with_buff_len rx_bd_with_len;
};


struct core_rx_cqe_opaque_data {
	__le32 data[2];
};


enum core_rx_cqe_type {
	CORE_RX_CQE_ILLEGAL_TYPE,
	CORE_RX_CQE_TYPE_REGULAR,
	CORE_RX_CQE_TYPE_GSI_OFFLOAD,
	CORE_RX_CQE_TYPE_SLOW_PATH,
	MAX_CORE_RX_CQE_TYPE
};


struct core_rx_fast_path_cqe {
	u8 type;
	u8 placement_offset;
	struct parsing_and_err_flags parse_flags;
	__le16 packet_length;
	__le16 vlan;
	struct core_rx_cqe_opaque_data opaque_data;
	struct parsing_err_flags err_flags;
	u8 packet_source;
	u8 reserved0;
	__le32 reserved1[3];
};


struct core_rx_gsi_offload_cqe {
	u8 type;
	u8 data_length_error;
	struct parsing_and_err_flags parse_flags;
	__le16 data_length;
	__le16 vlan;
	__le32 src_mac_addrhi;
	__le16 src_mac_addrlo;
	__le16 qp_id;
	__le32 src_qp;
	struct core_rx_cqe_opaque_data opaque_data;
	u8 packet_source;
	u8 reserved[3];
};


struct core_rx_slow_path_cqe {
	u8 type;
	u8 ramrod_cmd_id;
	__le16 echo;
	struct core_rx_cqe_opaque_data opaque_data;
	__le32 reserved1[5];
};


union core_rx_cqe_union {
	struct core_rx_fast_path_cqe rx_cqe_fp;
	struct core_rx_gsi_offload_cqe rx_cqe_gsi;
	struct core_rx_slow_path_cqe rx_cqe_sp;
};


enum core_rx_pkt_source {
	CORE_RX_PKT_SOURCE_NETWORK = 0,
	CORE_RX_PKT_SOURCE_LB,
	CORE_RX_PKT_SOURCE_TX,
	CORE_RX_PKT_SOURCE_LL2_TX,
	MAX_CORE_RX_PKT_SOURCE
};


struct core_rx_start_ramrod_data {
	struct regpair bd_base;
	struct regpair cqe_pbl_addr;
	__le16 mtu;
	__le16 sb_id;
	u8 sb_index;
	u8 complete_cqe_flg;
	u8 complete_event_flg;
	u8 drop_ttl0_flg;
	__le16 num_of_pbl_pages;
	u8 inner_vlan_stripping_en;
	u8 report_outer_vlan;
	u8 queue_id;
	u8 main_func_queue;
	u8 mf_si_bcast_accept_all;
	u8 mf_si_mcast_accept_all;
	struct core_rx_action_on_error action_on_error;
	u8 gsi_offload_flag;
	u8 vport_id_valid;
	u8 vport_id;
	u8 zero_prod_flg;
	u8 wipe_inner_vlan_pri_en;
	u8 reserved[2];
};


struct core_rx_stop_ramrod_data {
	u8 complete_cqe_flg;
	u8 complete_event_flg;
	u8 queue_id;
	u8 reserved1;
	__le16 reserved2[2];
};


struct core_tx_bd_data {
	__le16 as_bitfield;
#define CORE_TX_BD_DATA_FORCE_VLAN_MODE_MASK		0x1
#define CORE_TX_BD_DATA_FORCE_VLAN_MODE_SHIFT		0
#define CORE_TX_BD_DATA_VLAN_INSERTION_MASK		0x1
#define CORE_TX_BD_DATA_VLAN_INSERTION_SHIFT		1
#define CORE_TX_BD_DATA_START_BD_MASK			0x1
#define CORE_TX_BD_DATA_START_BD_SHIFT			2
#define CORE_TX_BD_DATA_IP_CSUM_MASK			0x1
#define CORE_TX_BD_DATA_IP_CSUM_SHIFT			3
#define CORE_TX_BD_DATA_L4_CSUM_MASK			0x1
#define CORE_TX_BD_DATA_L4_CSUM_SHIFT			4
#define CORE_TX_BD_DATA_IPV6_EXT_MASK			0x1
#define CORE_TX_BD_DATA_IPV6_EXT_SHIFT			5
#define CORE_TX_BD_DATA_L4_PROTOCOL_MASK		0x1
#define CORE_TX_BD_DATA_L4_PROTOCOL_SHIFT		6
#define CORE_TX_BD_DATA_L4_PSEUDO_CSUM_MODE_MASK	0x1
#define CORE_TX_BD_DATA_L4_PSEUDO_CSUM_MODE_SHIFT	7
#define CORE_TX_BD_DATA_NBDS_MASK			0xF
#define CORE_TX_BD_DATA_NBDS_SHIFT			8
#define CORE_TX_BD_DATA_ROCE_FLAV_MASK			0x1
#define CORE_TX_BD_DATA_ROCE_FLAV_SHIFT			12
#define CORE_TX_BD_DATA_IP_LEN_MASK			0x1
#define CORE_TX_BD_DATA_IP_LEN_SHIFT			13
#define CORE_TX_BD_DATA_DISABLE_STAG_INSERTION_MASK	0x1
#define CORE_TX_BD_DATA_DISABLE_STAG_INSERTION_SHIFT	14
#define CORE_TX_BD_DATA_RESERVED0_MASK			0x1
#define CORE_TX_BD_DATA_RESERVED0_SHIFT			15
};


struct core_tx_bd {
	struct regpair addr;
	__le16 nbytes;
	__le16 nw_vlan_or_lb_echo;
	struct core_tx_bd_data bd_data;
	__le16 bitfield1;
#define CORE_TX_BD_L4_HDR_OFFSET_W_MASK		0x3FFF
#define CORE_TX_BD_L4_HDR_OFFSET_W_SHIFT	0
#define CORE_TX_BD_TX_DST_MASK			0x3
#define CORE_TX_BD_TX_DST_SHIFT			14
};


enum core_tx_dest {
	CORE_TX_DEST_NW,
	CORE_TX_DEST_LB,
	CORE_TX_DEST_RESERVED,
	CORE_TX_DEST_DROP,
	MAX_CORE_TX_DEST
};


struct core_tx_start_ramrod_data {
	struct regpair pbl_base_addr;
	__le16 mtu;
	__le16 sb_id;
	u8 sb_index;
	u8 stats_en;
	u8 stats_id;
	u8 conn_type;
	__le16 pbl_size;
	__le16 qm_pq_id;
	u8 gsi_offload_flag;
	u8 ctx_stats_en;
	u8 vport_id_valid;
	u8 vport_id;
	u8 enforce_security_flag;
	u8 reserved[7];
};


struct core_tx_stop_ramrod_data {
	__le32 reserved0[2];
};


struct core_tx_update_ramrod_data {
	u8 update_qm_pq_id_flg;
	u8 reserved0;
	__le16 qm_pq_id;
	__le32 reserved1[1];
};


enum dcb_dscp_update_mode {
	DONT_UPDATE_DCB_DSCP,
	UPDATE_DCB,
	UPDATE_DSCP,
	UPDATE_DCB_DSCP,
	MAX_DCB_DSCP_UPDATE_MODE
};


struct ystorm_core_conn_st_ctx {
	__le32 reserved[4];
};


struct pstorm_core_conn_st_ctx {
	__le32 reserved[20];
};


struct xstorm_core_conn_st_ctx {
	struct regpair spq_base_addr;
	__le32 reserved0[2];
	__le16 spq_cons;
	__le16 reserved1[111];
};

struct xstorm_core_conn_ag_ctx {
	u8 reserved0;
	u8 state;
	u8 flags0;
#define XSTORM_CORE_CONN_AG_CTX_EXIST_IN_QM0_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT	0
#define XSTORM_CORE_CONN_AG_CTX_RESERVED1_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED1_SHIFT	1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED2_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED2_SHIFT	2
#define XSTORM_CORE_CONN_AG_CTX_EXIST_IN_QM3_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_EXIST_IN_QM3_SHIFT	3
#define XSTORM_CORE_CONN_AG_CTX_RESERVED3_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED3_SHIFT	4
#define XSTORM_CORE_CONN_AG_CTX_RESERVED4_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED4_SHIFT	5
#define XSTORM_CORE_CONN_AG_CTX_RESERVED5_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED5_SHIFT	6
#define XSTORM_CORE_CONN_AG_CTX_RESERVED6_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED6_SHIFT	7
	u8 flags1;
#define XSTORM_CORE_CONN_AG_CTX_RESERVED7_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED7_SHIFT	0
#define XSTORM_CORE_CONN_AG_CTX_RESERVED8_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED8_SHIFT	1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED9_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED9_SHIFT	2
#define XSTORM_CORE_CONN_AG_CTX_BIT11_MASK		0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT11_SHIFT		3
#define XSTORM_CORE_CONN_AG_CTX_BIT12_MASK		0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT12_SHIFT		4
#define XSTORM_CORE_CONN_AG_CTX_BIT13_MASK		0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT13_SHIFT		5
#define XSTORM_CORE_CONN_AG_CTX_TX_RULE_ACTIVE_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_TX_RULE_ACTIVE_SHIFT	6
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_ACTIVE_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_ACTIVE_SHIFT	7
	u8 flags2;
#define XSTORM_CORE_CONN_AG_CTX_CF0_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CF0_SHIFT	0
#define XSTORM_CORE_CONN_AG_CTX_CF1_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CF1_SHIFT	2
#define XSTORM_CORE_CONN_AG_CTX_CF2_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CF2_SHIFT	4
#define XSTORM_CORE_CONN_AG_CTX_CF3_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CF3_SHIFT	6
	u8 flags3;
#define XSTORM_CORE_CONN_AG_CTX_CF4_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CF4_SHIFT	0
#define XSTORM_CORE_CONN_AG_CTX_CF5_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CF5_SHIFT	2
#define XSTORM_CORE_CONN_AG_CTX_CF6_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CF6_SHIFT	4
#define XSTORM_CORE_CONN_AG_CTX_CF7_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CF7_SHIFT	6
	u8 flags4;
#define XSTORM_CORE_CONN_AG_CTX_CF8_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CF8_SHIFT	0
#define XSTORM_CORE_CONN_AG_CTX_CF9_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CF9_SHIFT	2
#define XSTORM_CORE_CONN_AG_CTX_CF10_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CF10_SHIFT	4
#define XSTORM_CORE_CONN_AG_CTX_CF11_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CF11_SHIFT	6
	u8 flags5;
#define XSTORM_CORE_CONN_AG_CTX_CF12_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CF12_SHIFT	0
#define XSTORM_CORE_CONN_AG_CTX_CF13_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CF13_SHIFT	2
#define XSTORM_CORE_CONN_AG_CTX_CF14_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CF14_SHIFT	4
#define XSTORM_CORE_CONN_AG_CTX_CF15_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CF15_SHIFT	6
	u8 flags6;
#define XSTORM_CORE_CONN_AG_CTX_CONSOLID_PROD_CF_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CONSOLID_PROD_CF_SHIFT	0
#define XSTORM_CORE_CONN_AG_CTX_CF17_MASK			0x3
#define XSTORM_CORE_CONN_AG_CTX_CF17_SHIFT			2
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_MASK			0x3
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_SHIFT			4
#define XSTORM_CORE_CONN_AG_CTX_TERMINATE_CF_MASK		0x3
#define XSTORM_CORE_CONN_AG_CTX_TERMINATE_CF_SHIFT		6
	u8 flags7;
#define XSTORM_CORE_CONN_AG_CTX_FLUSH_Q0_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_FLUSH_Q0_SHIFT	0
#define XSTORM_CORE_CONN_AG_CTX_RESERVED10_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_RESERVED10_SHIFT	2
#define XSTORM_CORE_CONN_AG_CTX_SLOW_PATH_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_SLOW_PATH_SHIFT	4
#define XSTORM_CORE_CONN_AG_CTX_CF0EN_MASK		0x1
#define XSTORM_CORE_CONN_AG_CTX_CF0EN_SHIFT		6
#define XSTORM_CORE_CONN_AG_CTX_CF1EN_MASK		0x1
#define XSTORM_CORE_CONN_AG_CTX_CF1EN_SHIFT		7
	u8 flags8;
#define XSTORM_CORE_CONN_AG_CTX_CF2EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_CF2EN_SHIFT	0
#define XSTORM_CORE_CONN_AG_CTX_CF3EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_CF3EN_SHIFT	1
#define XSTORM_CORE_CONN_AG_CTX_CF4EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_CF4EN_SHIFT	2
#define XSTORM_CORE_CONN_AG_CTX_CF5EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_CF5EN_SHIFT	3
#define XSTORM_CORE_CONN_AG_CTX_CF6EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_CF6EN_SHIFT	4
#define XSTORM_CORE_CONN_AG_CTX_CF7EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_CF7EN_SHIFT	5
#define XSTORM_CORE_CONN_AG_CTX_CF8EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_CF8EN_SHIFT	6
#define XSTORM_CORE_CONN_AG_CTX_CF9EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_CF9EN_SHIFT	7
	u8 flags9;
#define XSTORM_CORE_CONN_AG_CTX_CF10EN_MASK			0x1
#define XSTORM_CORE_CONN_AG_CTX_CF10EN_SHIFT			0
#define XSTORM_CORE_CONN_AG_CTX_CF11EN_MASK			0x1
#define XSTORM_CORE_CONN_AG_CTX_CF11EN_SHIFT			1
#define XSTORM_CORE_CONN_AG_CTX_CF12EN_MASK			0x1
#define XSTORM_CORE_CONN_AG_CTX_CF12EN_SHIFT			2
#define XSTORM_CORE_CONN_AG_CTX_CF13EN_MASK			0x1
#define XSTORM_CORE_CONN_AG_CTX_CF13EN_SHIFT			3
#define XSTORM_CORE_CONN_AG_CTX_CF14EN_MASK			0x1
#define XSTORM_CORE_CONN_AG_CTX_CF14EN_SHIFT			4
#define XSTORM_CORE_CONN_AG_CTX_CF15EN_MASK			0x1
#define XSTORM_CORE_CONN_AG_CTX_CF15EN_SHIFT			5
#define XSTORM_CORE_CONN_AG_CTX_CONSOLID_PROD_CF_EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_CONSOLID_PROD_CF_EN_SHIFT	6
#define XSTORM_CORE_CONN_AG_CTX_CF17EN_MASK			0x1
#define XSTORM_CORE_CONN_AG_CTX_CF17EN_SHIFT			7
	u8 flags10;
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_EN_MASK		0x1
#define XSTORM_CORE_CONN_AG_CTX_DQ_CF_EN_SHIFT		0
#define XSTORM_CORE_CONN_AG_CTX_TERMINATE_CF_EN_MASK		0x1
#define XSTORM_CORE_CONN_AG_CTX_TERMINATE_CF_EN_SHIFT	1
#define XSTORM_CORE_CONN_AG_CTX_FLUSH_Q0_EN_MASK		0x1
#define XSTORM_CORE_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT		2
#define XSTORM_CORE_CONN_AG_CTX_RESERVED11_MASK		0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED11_SHIFT		3
#define XSTORM_CORE_CONN_AG_CTX_SLOW_PATH_EN_MASK		0x1
#define XSTORM_CORE_CONN_AG_CTX_SLOW_PATH_EN_SHIFT		4
#define XSTORM_CORE_CONN_AG_CTX_CF23EN_MASK			0x1
#define XSTORM_CORE_CONN_AG_CTX_CF23EN_SHIFT			5
#define XSTORM_CORE_CONN_AG_CTX_RESERVED12_MASK		0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED12_SHIFT		6
#define XSTORM_CORE_CONN_AG_CTX_RESERVED13_MASK		0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED13_SHIFT		7
	u8 flags11;
#define XSTORM_CORE_CONN_AG_CTX_RESERVED14_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED14_SHIFT	0
#define XSTORM_CORE_CONN_AG_CTX_RESERVED15_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RESERVED15_SHIFT	1
#define XSTORM_CORE_CONN_AG_CTX_TX_DEC_RULE_EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_TX_DEC_RULE_EN_SHIFT	2
#define XSTORM_CORE_CONN_AG_CTX_RULE5EN_MASK		0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE5EN_SHIFT	3
#define XSTORM_CORE_CONN_AG_CTX_RULE6EN_MASK		0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE6EN_SHIFT	4
#define XSTORM_CORE_CONN_AG_CTX_RULE7EN_MASK		0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE7EN_SHIFT	5
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED1_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED1_SHIFT	6
#define XSTORM_CORE_CONN_AG_CTX_RULE9EN_MASK		0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE9EN_SHIFT	7
	u8 flags12;
#define XSTORM_CORE_CONN_AG_CTX_RULE10EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE10EN_SHIFT	0
#define XSTORM_CORE_CONN_AG_CTX_RULE11EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE11EN_SHIFT	1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED2_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED2_SHIFT	2
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED3_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED3_SHIFT	3
#define XSTORM_CORE_CONN_AG_CTX_RULE14EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE14EN_SHIFT	4
#define XSTORM_CORE_CONN_AG_CTX_RULE15EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE15EN_SHIFT	5
#define XSTORM_CORE_CONN_AG_CTX_RULE16EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE16EN_SHIFT	6
#define XSTORM_CORE_CONN_AG_CTX_RULE17EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE17EN_SHIFT	7
	u8 flags13;
#define XSTORM_CORE_CONN_AG_CTX_RULE18EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE18EN_SHIFT	0
#define XSTORM_CORE_CONN_AG_CTX_RULE19EN_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_RULE19EN_SHIFT	1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED4_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED4_SHIFT	2
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED5_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED5_SHIFT	3
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED6_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED6_SHIFT	4
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED7_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED7_SHIFT	5
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED8_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED8_SHIFT	6
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED9_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_A0_RESERVED9_SHIFT	7
	u8 flags14;
#define XSTORM_CORE_CONN_AG_CTX_BIT16_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT16_SHIFT	0
#define XSTORM_CORE_CONN_AG_CTX_BIT17_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT17_SHIFT	1
#define XSTORM_CORE_CONN_AG_CTX_BIT18_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT18_SHIFT	2
#define XSTORM_CORE_CONN_AG_CTX_BIT19_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT19_SHIFT	3
#define XSTORM_CORE_CONN_AG_CTX_BIT20_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT20_SHIFT	4
#define XSTORM_CORE_CONN_AG_CTX_BIT21_MASK	0x1
#define XSTORM_CORE_CONN_AG_CTX_BIT21_SHIFT	5
#define XSTORM_CORE_CONN_AG_CTX_CF23_MASK	0x3
#define XSTORM_CORE_CONN_AG_CTX_CF23_SHIFT	6
	u8 byte2;
	__le16 physical_q0;
	__le16 consolid_prod;
	__le16 reserved16;
	__le16 tx_bd_cons;
	__le16 tx_bd_or_spq_prod;
	__le16 updated_qm_pq_id;
	__le16 conn_dpi;
	u8 byte3;
	u8 byte4;
	u8 byte5;
	u8 byte6;
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 reg3;
	__le32 reg4;
	__le32 reg5;
	__le32 reg6;
	__le16 word7;
	__le16 word8;
	__le16 word9;
	__le16 word10;
	__le32 reg7;
	__le32 reg8;
	__le32 reg9;
	u8 byte7;
	u8 byte8;
	u8 byte9;
	u8 byte10;
	u8 byte11;
	u8 byte12;
	u8 byte13;
	u8 byte14;
	u8 byte15;
	u8 e5_reserved;
	__le16 word11;
	__le32 reg10;
	__le32 reg11;
	__le32 reg12;
	__le32 reg13;
	__le32 reg14;
	__le32 reg15;
	__le32 reg16;
	__le32 reg17;
	__le32 reg18;
	__le32 reg19;
	__le16 word12;
	__le16 word13;
	__le16 word14;
	__le16 word15;
};

struct tstorm_core_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define TSTORM_CORE_CONN_AG_CTX_BIT0_MASK	0x1
#define TSTORM_CORE_CONN_AG_CTX_BIT0_SHIFT	0
#define TSTORM_CORE_CONN_AG_CTX_BIT1_MASK	0x1
#define TSTORM_CORE_CONN_AG_CTX_BIT1_SHIFT	1
#define TSTORM_CORE_CONN_AG_CTX_BIT2_MASK	0x1
#define TSTORM_CORE_CONN_AG_CTX_BIT2_SHIFT	2
#define TSTORM_CORE_CONN_AG_CTX_BIT3_MASK	0x1
#define TSTORM_CORE_CONN_AG_CTX_BIT3_SHIFT	3
#define TSTORM_CORE_CONN_AG_CTX_BIT4_MASK	0x1
#define TSTORM_CORE_CONN_AG_CTX_BIT4_SHIFT	4
#define TSTORM_CORE_CONN_AG_CTX_BIT5_MASK	0x1
#define TSTORM_CORE_CONN_AG_CTX_BIT5_SHIFT	5
#define TSTORM_CORE_CONN_AG_CTX_CF0_MASK	0x3
#define TSTORM_CORE_CONN_AG_CTX_CF0_SHIFT	6
	u8 flags1;
#define TSTORM_CORE_CONN_AG_CTX_CF1_MASK	0x3
#define TSTORM_CORE_CONN_AG_CTX_CF1_SHIFT	0
#define TSTORM_CORE_CONN_AG_CTX_CF2_MASK	0x3
#define TSTORM_CORE_CONN_AG_CTX_CF2_SHIFT	2
#define TSTORM_CORE_CONN_AG_CTX_CF3_MASK	0x3
#define TSTORM_CORE_CONN_AG_CTX_CF3_SHIFT	4
#define TSTORM_CORE_CONN_AG_CTX_CF4_MASK	0x3
#define TSTORM_CORE_CONN_AG_CTX_CF4_SHIFT	6
	u8 flags2;
#define TSTORM_CORE_CONN_AG_CTX_CF5_MASK	0x3
#define TSTORM_CORE_CONN_AG_CTX_CF5_SHIFT	0
#define TSTORM_CORE_CONN_AG_CTX_CF6_MASK	0x3
#define TSTORM_CORE_CONN_AG_CTX_CF6_SHIFT	2
#define TSTORM_CORE_CONN_AG_CTX_CF7_MASK	0x3
#define TSTORM_CORE_CONN_AG_CTX_CF7_SHIFT	4
#define TSTORM_CORE_CONN_AG_CTX_CF8_MASK	0x3
#define TSTORM_CORE_CONN_AG_CTX_CF8_SHIFT	6
	u8 flags3;
#define TSTORM_CORE_CONN_AG_CTX_CF9_MASK	0x3
#define TSTORM_CORE_CONN_AG_CTX_CF9_SHIFT	0
#define TSTORM_CORE_CONN_AG_CTX_CF10_MASK	0x3
#define TSTORM_CORE_CONN_AG_CTX_CF10_SHIFT	2
#define TSTORM_CORE_CONN_AG_CTX_CF0EN_MASK	0x1
#define TSTORM_CORE_CONN_AG_CTX_CF0EN_SHIFT	4
#define TSTORM_CORE_CONN_AG_CTX_CF1EN_MASK	0x1
#define TSTORM_CORE_CONN_AG_CTX_CF1EN_SHIFT	5
#define TSTORM_CORE_CONN_AG_CTX_CF2EN_MASK	0x1
#define TSTORM_CORE_CONN_AG_CTX_CF2EN_SHIFT	6
#define TSTORM_CORE_CONN_AG_CTX_CF3EN_MASK	0x1
#define TSTORM_CORE_CONN_AG_CTX_CF3EN_SHIFT	7
	u8 flags4;
#define TSTORM_CORE_CONN_AG_CTX_CF4EN_MASK		0x1
#define TSTORM_CORE_CONN_AG_CTX_CF4EN_SHIFT		0
#define TSTORM_CORE_CONN_AG_CTX_CF5EN_MASK		0x1
#define TSTORM_CORE_CONN_AG_CTX_CF5EN_SHIFT		1
#define TSTORM_CORE_CONN_AG_CTX_CF6EN_MASK		0x1
#define TSTORM_CORE_CONN_AG_CTX_CF6EN_SHIFT		2
#define TSTORM_CORE_CONN_AG_CTX_CF7EN_MASK		0x1
#define TSTORM_CORE_CONN_AG_CTX_CF7EN_SHIFT		3
#define TSTORM_CORE_CONN_AG_CTX_CF8EN_MASK		0x1
#define TSTORM_CORE_CONN_AG_CTX_CF8EN_SHIFT		4
#define TSTORM_CORE_CONN_AG_CTX_CF9EN_MASK		0x1
#define TSTORM_CORE_CONN_AG_CTX_CF9EN_SHIFT		5
#define TSTORM_CORE_CONN_AG_CTX_CF10EN_MASK		0x1
#define TSTORM_CORE_CONN_AG_CTX_CF10EN_SHIFT		6
#define TSTORM_CORE_CONN_AG_CTX_RULE0EN_MASK		0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE0EN_SHIFT	7
	u8 flags5;
#define TSTORM_CORE_CONN_AG_CTX_RULE1EN_MASK		0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE1EN_SHIFT	0
#define TSTORM_CORE_CONN_AG_CTX_RULE2EN_MASK		0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE2EN_SHIFT	1
#define TSTORM_CORE_CONN_AG_CTX_RULE3EN_MASK		0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE3EN_SHIFT	2
#define TSTORM_CORE_CONN_AG_CTX_RULE4EN_MASK		0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE4EN_SHIFT	3
#define TSTORM_CORE_CONN_AG_CTX_RULE5EN_MASK		0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE5EN_SHIFT	4
#define TSTORM_CORE_CONN_AG_CTX_RULE6EN_MASK		0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE6EN_SHIFT	5
#define TSTORM_CORE_CONN_AG_CTX_RULE7EN_MASK		0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE7EN_SHIFT	6
#define TSTORM_CORE_CONN_AG_CTX_RULE8EN_MASK		0x1
#define TSTORM_CORE_CONN_AG_CTX_RULE8EN_SHIFT	7
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 reg3;
	__le32 reg4;
	__le32 reg5;
	__le32 reg6;
	__le32 reg7;
	__le32 reg8;
	u8 byte2;
	u8 byte3;
	__le16 word0;
	u8 byte4;
	u8 byte5;
	__le16 word1;
	__le16 word2;
	__le16 word3;
	__le32 ll2_rx_prod;
	__le32 reg10;
};

struct ustorm_core_conn_ag_ctx {
	u8 reserved;
	u8 byte1;
	u8 flags0;
#define USTORM_CORE_CONN_AG_CTX_BIT0_MASK	0x1
#define USTORM_CORE_CONN_AG_CTX_BIT0_SHIFT	0
#define USTORM_CORE_CONN_AG_CTX_BIT1_MASK	0x1
#define USTORM_CORE_CONN_AG_CTX_BIT1_SHIFT	1
#define USTORM_CORE_CONN_AG_CTX_CF0_MASK	0x3
#define USTORM_CORE_CONN_AG_CTX_CF0_SHIFT	2
#define USTORM_CORE_CONN_AG_CTX_CF1_MASK	0x3
#define USTORM_CORE_CONN_AG_CTX_CF1_SHIFT	4
#define USTORM_CORE_CONN_AG_CTX_CF2_MASK	0x3
#define USTORM_CORE_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define USTORM_CORE_CONN_AG_CTX_CF3_MASK	0x3
#define USTORM_CORE_CONN_AG_CTX_CF3_SHIFT	0
#define USTORM_CORE_CONN_AG_CTX_CF4_MASK	0x3
#define USTORM_CORE_CONN_AG_CTX_CF4_SHIFT	2
#define USTORM_CORE_CONN_AG_CTX_CF5_MASK	0x3
#define USTORM_CORE_CONN_AG_CTX_CF5_SHIFT	4
#define USTORM_CORE_CONN_AG_CTX_CF6_MASK	0x3
#define USTORM_CORE_CONN_AG_CTX_CF6_SHIFT	6
	u8 flags2;
#define USTORM_CORE_CONN_AG_CTX_CF0EN_MASK		0x1
#define USTORM_CORE_CONN_AG_CTX_CF0EN_SHIFT		0
#define USTORM_CORE_CONN_AG_CTX_CF1EN_MASK		0x1
#define USTORM_CORE_CONN_AG_CTX_CF1EN_SHIFT		1
#define USTORM_CORE_CONN_AG_CTX_CF2EN_MASK		0x1
#define USTORM_CORE_CONN_AG_CTX_CF2EN_SHIFT		2
#define USTORM_CORE_CONN_AG_CTX_CF3EN_MASK		0x1
#define USTORM_CORE_CONN_AG_CTX_CF3EN_SHIFT		3
#define USTORM_CORE_CONN_AG_CTX_CF4EN_MASK		0x1
#define USTORM_CORE_CONN_AG_CTX_CF4EN_SHIFT		4
#define USTORM_CORE_CONN_AG_CTX_CF5EN_MASK		0x1
#define USTORM_CORE_CONN_AG_CTX_CF5EN_SHIFT		5
#define USTORM_CORE_CONN_AG_CTX_CF6EN_MASK		0x1
#define USTORM_CORE_CONN_AG_CTX_CF6EN_SHIFT		6
#define USTORM_CORE_CONN_AG_CTX_RULE0EN_MASK		0x1
#define USTORM_CORE_CONN_AG_CTX_RULE0EN_SHIFT	7
	u8 flags3;
#define USTORM_CORE_CONN_AG_CTX_RULE1EN_MASK		0x1
#define USTORM_CORE_CONN_AG_CTX_RULE1EN_SHIFT	0
#define USTORM_CORE_CONN_AG_CTX_RULE2EN_MASK		0x1
#define USTORM_CORE_CONN_AG_CTX_RULE2EN_SHIFT	1
#define USTORM_CORE_CONN_AG_CTX_RULE3EN_MASK		0x1
#define USTORM_CORE_CONN_AG_CTX_RULE3EN_SHIFT	2
#define USTORM_CORE_CONN_AG_CTX_RULE4EN_MASK		0x1
#define USTORM_CORE_CONN_AG_CTX_RULE4EN_SHIFT	3
#define USTORM_CORE_CONN_AG_CTX_RULE5EN_MASK		0x1
#define USTORM_CORE_CONN_AG_CTX_RULE5EN_SHIFT	4
#define USTORM_CORE_CONN_AG_CTX_RULE6EN_MASK		0x1
#define USTORM_CORE_CONN_AG_CTX_RULE6EN_SHIFT	5
#define USTORM_CORE_CONN_AG_CTX_RULE7EN_MASK		0x1
#define USTORM_CORE_CONN_AG_CTX_RULE7EN_SHIFT	6
#define USTORM_CORE_CONN_AG_CTX_RULE8EN_MASK		0x1
#define USTORM_CORE_CONN_AG_CTX_RULE8EN_SHIFT	7
	u8 byte2;
	u8 byte3;
	__le16 word0;
	__le16 word1;
	__le32 rx_producers;
	__le32 reg1;
	__le32 reg2;
	__le32 reg3;
	__le16 word2;
	__le16 word3;
};


struct mstorm_core_conn_st_ctx {
	__le32 reserved[40];
};


struct ustorm_core_conn_st_ctx {
	__le32 reserved[20];
};


struct tstorm_core_conn_st_ctx {
	__le32 reserved[4];
};


struct core_conn_context {
	struct ystorm_core_conn_st_ctx ystorm_st_context;
	struct regpair ystorm_st_padding[2];
	struct pstorm_core_conn_st_ctx pstorm_st_context;
	struct regpair pstorm_st_padding[2];
	struct xstorm_core_conn_st_ctx xstorm_st_context;
	struct xstorm_core_conn_ag_ctx xstorm_ag_context;
	struct tstorm_core_conn_ag_ctx tstorm_ag_context;
	struct ustorm_core_conn_ag_ctx ustorm_ag_context;
	struct mstorm_core_conn_st_ctx mstorm_st_context;
	struct ustorm_core_conn_st_ctx ustorm_st_context;
	struct regpair ustorm_st_padding[2];
	struct tstorm_core_conn_st_ctx tstorm_st_context;
	struct regpair tstorm_st_padding[2];
};

struct eth_mstorm_per_pf_stat {
	struct regpair gre_discard_pkts;
	struct regpair vxlan_discard_pkts;
	struct regpair geneve_discard_pkts;
	struct regpair lb_discard_pkts;
};

struct eth_mstorm_per_queue_stat {
	struct regpair ttl0_discard;
	struct regpair packet_too_big_discard;
	struct regpair no_buff_discard;
	struct regpair not_active_discard;
	struct regpair tpa_coalesced_pkts;
	struct regpair tpa_coalesced_events;
	struct regpair tpa_aborts_num;
	struct regpair tpa_coalesced_bytes;
};


struct eth_pstorm_per_pf_stat {
	struct regpair sent_lb_ucast_bytes;
	struct regpair sent_lb_mcast_bytes;
	struct regpair sent_lb_bcast_bytes;
	struct regpair sent_lb_ucast_pkts;
	struct regpair sent_lb_mcast_pkts;
	struct regpair sent_lb_bcast_pkts;
	struct regpair sent_gre_bytes;
	struct regpair sent_vxlan_bytes;
	struct regpair sent_geneve_bytes;
	struct regpair sent_mpls_bytes;
	struct regpair sent_gre_mpls_bytes;
	struct regpair sent_udp_mpls_bytes;
	struct regpair sent_gre_pkts;
	struct regpair sent_vxlan_pkts;
	struct regpair sent_geneve_pkts;
	struct regpair sent_mpls_pkts;
	struct regpair sent_gre_mpls_pkts;
	struct regpair sent_udp_mpls_pkts;
	struct regpair gre_drop_pkts;
	struct regpair vxlan_drop_pkts;
	struct regpair geneve_drop_pkts;
	struct regpair mpls_drop_pkts;
	struct regpair gre_mpls_drop_pkts;
	struct regpair udp_mpls_drop_pkts;
};


struct eth_pstorm_per_queue_stat {
	struct regpair sent_ucast_bytes;
	struct regpair sent_mcast_bytes;
	struct regpair sent_bcast_bytes;
	struct regpair sent_ucast_pkts;
	struct regpair sent_mcast_pkts;
	struct regpair sent_bcast_pkts;
	struct regpair error_drop_pkts;
};


struct eth_rx_rate_limit {
	__le16 mult;
	__le16 cnst;
	u8 add_sub_cnst;
	u8 reserved0;
	__le16 reserved1;
};


struct eth_tstorm_rss_update_data {
	u8 vport_id;
	u8 ind_table_index;
	__le16 ind_table_value;
	__le16 reserved1;
	u8 reserved;
	u8 valid;
};

struct eth_ustorm_per_pf_stat {
	struct regpair rcv_lb_ucast_bytes;
	struct regpair rcv_lb_mcast_bytes;
	struct regpair rcv_lb_bcast_bytes;
	struct regpair rcv_lb_ucast_pkts;
	struct regpair rcv_lb_mcast_pkts;
	struct regpair rcv_lb_bcast_pkts;
	struct regpair rcv_gre_bytes;
	struct regpair rcv_vxlan_bytes;
	struct regpair rcv_geneve_bytes;
	struct regpair rcv_gre_pkts;
	struct regpair rcv_vxlan_pkts;
	struct regpair rcv_geneve_pkts;
};

struct eth_ustorm_per_queue_stat {
	struct regpair rcv_ucast_bytes;
	struct regpair rcv_mcast_bytes;
	struct regpair rcv_bcast_bytes;
	struct regpair rcv_ucast_pkts;
	struct regpair rcv_mcast_pkts;
	struct regpair rcv_bcast_pkts;
};


struct vf_pf_channel_eqe_data {
	struct regpair msg_addr;
};


struct initial_cleanup_eqe_data {
	u8 vf_id;
	u8 reserved[7];
};


struct fw_err_data {
	u8 recovery_scope;
	u8 err_id;
	__le16 entity_id;
	u8 reserved[4];
};


union event_ring_data {
	u8 bytes[8];
	struct vf_pf_channel_eqe_data vf_pf_channel;
	struct iscsi_eqe_data iscsi_info;
	struct iscsi_connect_done_results iscsi_conn_done_info;
	union rdma_eqe_data rdma_data;
	struct initial_cleanup_eqe_data vf_init_cleanup;
	struct fw_err_data err_data;
};


struct event_ring_entry {
	u8 protocol_id;
	u8 opcode;
	u8 reserved0;
	u8 vf_id;
	__le16 echo;
	u8 fw_return_code;
	u8 flags;
#define EVENT_RING_ENTRY_ASYNC_MASK		0x1
#define EVENT_RING_ENTRY_ASYNC_SHIFT		0
#define EVENT_RING_ENTRY_RESERVED1_MASK		0x7F
#define EVENT_RING_ENTRY_RESERVED1_SHIFT	1
	union event_ring_data data;
};


struct event_ring_next_addr {
	struct regpair addr;
	__le32 reserved[2];
};


union event_ring_element {
	struct event_ring_entry entry;
	struct event_ring_next_addr next_addr;
};


enum fw_flow_ctrl_mode {
	flow_ctrl_pause,
	flow_ctrl_pfc,
	MAX_FW_FLOW_CTRL_MODE
};


enum gft_profile_type {
	GFT_PROFILE_TYPE_4_TUPLE,
	GFT_PROFILE_TYPE_L4_DST_PORT,
	GFT_PROFILE_TYPE_IP_DST_ADDR,
	GFT_PROFILE_TYPE_IP_SRC_ADDR,
	GFT_PROFILE_TYPE_TUNNEL_TYPE,
	MAX_GFT_PROFILE_TYPE
};


struct hsi_fp_ver_struct {
	u8 minor_ver_arr[2];
	u8 major_ver_arr[2];
};


enum integ_phase {
	INTEG_PHASE_BB_A0_LATEST = 3,
	INTEG_PHASE_BB_B0_NO_MCP = 10,
	INTEG_PHASE_BB_B0_WITH_MCP = 11,
	MAX_INTEG_PHASE
};


enum iwarp_ll2_tx_queues {
	IWARP_LL2_IN_ORDER_TX_QUEUE = 1,
	IWARP_LL2_ALIGNED_TX_QUEUE,
	IWARP_LL2_ALIGNED_RIGHT_TRIMMED_TX_QUEUE,
	IWARP_LL2_ERROR,
	MAX_IWARP_LL2_TX_QUEUES
};


enum func_err_id {
	FUNC_NO_ERROR,
	VF_PF_CHANNEL_NOT_READY,
	VF_ZONE_MSG_NOT_VALID,
	VF_ZONE_FUNC_NOT_ENABLED,
	ETH_PACKET_TOO_SMALL,
	ETH_ILLEGAL_VLAN_MODE,
	ETH_MTU_VIOLATION,
	ETH_ILLEGAL_INBAND_TAGS,
	ETH_VLAN_INSERT_AND_INBAND_VLAN,
	ETH_ILLEGAL_NBDS,
	ETH_FIRST_BD_WO_SOP,
	ETH_INSUFFICIENT_BDS,
	ETH_ILLEGAL_LSO_HDR_NBDS,
	ETH_ILLEGAL_LSO_MSS,
	ETH_ZERO_SIZE_BD,
	ETH_ILLEGAL_LSO_HDR_LEN,
	ETH_INSUFFICIENT_PAYLOAD,
	ETH_EDPM_OUT_OF_SYNC,
	ETH_TUNN_IPV6_EXT_NBD_ERR,
	ETH_CONTROL_PACKET_VIOLATION,
	ETH_ANTI_SPOOFING_ERR,
	ETH_PACKET_SIZE_TOO_LARGE,
	CORE_ILLEGAL_VLAN_MODE,
	CORE_ILLEGAL_NBDS,
	CORE_FIRST_BD_WO_SOP,
	CORE_INSUFFICIENT_BDS,
	CORE_PACKET_TOO_SMALL,
	CORE_ILLEGAL_INBAND_TAGS,
	CORE_VLAN_INSERT_AND_INBAND_VLAN,
	CORE_MTU_VIOLATION,
	CORE_CONTROL_PACKET_VIOLATION,
	CORE_ANTI_SPOOFING_ERR,
	CORE_PACKET_SIZE_TOO_LARGE,
	CORE_ILLEGAL_BD_FLAGS,
	CORE_GSI_PACKET_VIOLATION,
	MAX_FUNC_ERR_ID
};


enum fw_err_mode {
	FW_ERR_FATAL_ASSERT,
	FW_ERR_DRV_REPORT,
	MAX_FW_ERR_MODE
};


enum fw_err_recovery_scope {
	ERR_SCOPE_INVALID,
	ERR_SCOPE_TX_Q,
	ERR_SCOPE_RX_Q,
	ERR_SCOPE_QP,
	ERR_SCOPE_VPORT,
	ERR_SCOPE_FUNC,
	ERR_SCOPE_PORT,
	ERR_SCOPE_ENGINE,
	MAX_FW_ERR_RECOVERY_SCOPE
};


struct mstorm_non_trigger_vf_zone {
	struct eth_mstorm_per_queue_stat eth_queue_stat;
	struct eth_rx_prod_data eth_rx_queue_producers[ETH_MAX_RXQ_VF_QUAD];
};


struct mstorm_vf_zone {
	struct mstorm_non_trigger_vf_zone non_trigger;
};


struct vlan_header {
	__le16 tpid;
	__le16 tci;
};


struct outer_tag_config_struct {
	u8 enable_stag_pri_change;
	u8 pri_map_valid;
	u8 reserved[2];
	struct vlan_header outer_tag;
	u8 inner_to_outer_pri_map[8];
};


enum personality_type {
	BAD_PERSONALITY_TYP,
	PERSONALITY_TCP_ULP,
	PERSONALITY_FCOE,
	PERSONALITY_RDMA_AND_ETH,
	PERSONALITY_RDMA,
	PERSONALITY_CORE,
	PERSONALITY_ETH,
	PERSONALITY_RESERVED,
	MAX_PERSONALITY_TYPE
};


struct pf_start_tunnel_config {
	u8 set_vxlan_udp_port_flg;
	u8 set_geneve_udp_port_flg;
	u8 set_no_inner_l2_vxlan_udp_port_flg;
	u8 tunnel_clss_vxlan;
	u8 tunnel_clss_l2geneve;
	u8 tunnel_clss_ipgeneve;
	u8 tunnel_clss_l2gre;
	u8 tunnel_clss_ipgre;
	__le16 vxlan_udp_port;
	__le16 geneve_udp_port;
	__le16 no_inner_l2_vxlan_udp_port;
	__le16 reserved[3];
};


struct pf_start_ramrod_data {
	struct regpair event_ring_pbl_addr;
	struct regpair consolid_q_pbl_base_addr;
	struct pf_start_tunnel_config tunnel_config;
	__le16 event_ring_sb_id;
	u8 base_vf_id;
	u8 num_vfs;
	u8 event_ring_num_pages;
	u8 event_ring_sb_index;
	u8 path_id;
	u8 warning_as_error;
	u8 dont_log_ramrods;
	u8 personality;
	__le16 log_type_mask;
	u8 mf_mode;
	u8 integ_phase;
	u8 allow_npar_tx_switching;
	u8 reserved0;
	struct hsi_fp_ver_struct hsi_fp_ver;
	struct outer_tag_config_struct outer_tag_config;
	u8 pf_fp_err_mode;
	u8 consolid_q_num_pages;
	u8 reserved[6];
};


struct protocol_dcb_data {
	u8 dcb_enable_flag;
	u8 dscp_enable_flag;
	u8 dcb_priority;
	u8 dcb_tc;
	u8 dscp_val;
	u8 dcb_dont_add_vlan0;
};


struct pf_update_tunnel_config {
	u8 update_rx_pf_clss;
	u8 update_rx_def_ucast_clss;
	u8 update_rx_def_non_ucast_clss;
	u8 set_vxlan_udp_port_flg;
	u8 set_geneve_udp_port_flg;
	u8 set_no_inner_l2_vxlan_udp_port_flg;
	u8 tunnel_clss_vxlan;
	u8 tunnel_clss_l2geneve;
	u8 tunnel_clss_ipgeneve;
	u8 tunnel_clss_l2gre;
	u8 tunnel_clss_ipgre;
	u8 reserved;
	__le16 vxlan_udp_port;
	__le16 geneve_udp_port;
	__le16 no_inner_l2_vxlan_udp_port;
	__le16 reserved1[3];
};


struct pf_update_ramrod_data {
	u8 update_eth_dcb_data_mode;
	u8 update_fcoe_dcb_data_mode;
	u8 update_iscsi_dcb_data_mode;
	u8 update_roce_dcb_data_mode;
	u8 update_rroce_dcb_data_mode;
	u8 update_iwarp_dcb_data_mode;
	u8 update_mf_vlan_flag;
	u8 update_enable_stag_pri_change;
	struct protocol_dcb_data eth_dcb_data;
	struct protocol_dcb_data fcoe_dcb_data;
	struct protocol_dcb_data iscsi_dcb_data;
	struct protocol_dcb_data roce_dcb_data;
	struct protocol_dcb_data rroce_dcb_data;
	struct protocol_dcb_data iwarp_dcb_data;
	__le16 mf_vlan;
	u8 enable_stag_pri_change;
	u8 reserved;
	struct pf_update_tunnel_config tunnel_config;
};


enum ports_mode {
	ENGX2_PORTX1,
	ENGX2_PORTX2,
	ENGX1_PORTX1,
	ENGX1_PORTX2,
	ENGX1_PORTX4,
	MAX_PORTS_MODE
};


enum protocol_common_error_code {
	COMMON_ERR_CODE_OK = 0,
	COMMON_ERR_CODE_ERROR,
	MAX_PROTOCOL_COMMON_ERROR_CODE
};


enum protocol_version_array_key {
	ETH_VER_KEY = 0,
	ROCE_VER_KEY,
	MAX_PROTOCOL_VERSION_ARRAY_KEY
};


struct rdma_sent_stats {
	struct regpair sent_bytes;
	struct regpair sent_pkts;
};


struct pstorm_non_trigger_vf_zone {
	struct eth_pstorm_per_queue_stat eth_queue_stat;
	struct rdma_sent_stats rdma_stats;
};


struct pstorm_vf_zone {
	struct pstorm_non_trigger_vf_zone non_trigger;
	struct regpair reserved[7];
};


struct ramrod_header {
	__le32 cid;
	u8 cmd_id;
	u8 protocol_id;
	__le16 echo;
};


struct rdma_rcv_stats {
	struct regpair rcv_bytes;
	struct regpair rcv_pkts;
};


struct rl_update_ramrod_data {
	u8 qcn_update_param_flg;
	u8 dcqcn_update_param_flg;
	u8 rl_init_flg;
	u8 rl_start_flg;
	u8 rl_stop_flg;
	u8 rl_id_first;
	u8 rl_id_last;
	u8 rl_dc_qcn_flg;
	u8 dcqcn_reset_alpha_on_idle;
	u8 rl_bc_stage_th;
	u8 rl_timer_stage_th;
	u8 reserved1;
	__le32 rl_bc_rate;
	__le16 rl_max_rate;
	__le16 rl_r_ai;
	__le16 rl_r_hai;
	__le16 dcqcn_g;
	__le32 dcqcn_k_us;
	__le32 dcqcn_timeuot_us;
	__le32 qcn_timeuot_us;
	__le32 reserved2;
};


struct slow_path_element {
	struct ramrod_header hdr;
	struct regpair data_ptr;
};


struct tstorm_non_trigger_vf_zone {
	struct rdma_rcv_stats rdma_stats;
};

struct tstorm_per_port_stat {
	struct regpair trunc_error_discard;
	struct regpair mac_error_discard;
	struct regpair mftag_filter_discard;
	struct regpair eth_mac_filter_discard;
	struct regpair ll2_mac_filter_discard;
	struct regpair ll2_conn_disabled_discard;
	struct regpair iscsi_irregular_pkt;
	struct regpair fcoe_irregular_pkt;
	struct regpair roce_irregular_pkt;
	struct regpair iwarp_irregular_pkt;
	struct regpair eth_irregular_pkt;
	struct regpair toe_irregular_pkt;
	struct regpair preroce_irregular_pkt;
	struct regpair eth_gre_tunn_filter_discard;
	struct regpair eth_vxlan_tunn_filter_discard;
	struct regpair eth_geneve_tunn_filter_discard;
	struct regpair eth_gft_drop_pkt;
};


struct tstorm_vf_zone {
	struct tstorm_non_trigger_vf_zone non_trigger;
};


enum tunnel_clss {
	TUNNEL_CLSS_MAC_VLAN = 0,
	TUNNEL_CLSS_MAC_VNI,
	TUNNEL_CLSS_INNER_MAC_VLAN,
	TUNNEL_CLSS_INNER_MAC_VNI,
	TUNNEL_CLSS_MAC_VLAN_DUAL_STAGE,
	MAX_TUNNEL_CLSS
};


struct ustorm_non_trigger_vf_zone {
	struct eth_ustorm_per_queue_stat eth_queue_stat;
	struct regpair vf_pf_msg_addr;
};


struct ustorm_trigger_vf_zone {
	u8 vf_pf_msg_valid;
	u8 reserved[7];
};


struct ustorm_vf_zone {
	struct ustorm_non_trigger_vf_zone non_trigger;
	struct ustorm_trigger_vf_zone trigger;
};


struct vf_pf_channel_data {
	__le32 ready;
	u8 valid;
	u8 reserved0;
	__le16 reserved1;
};


struct vf_start_ramrod_data {
	u8 vf_id;
	u8 enable_flr_ack;
	__le16 opaque_fid;
	u8 personality;
	u8 reserved[7];
	struct hsi_fp_ver_struct hsi_fp_ver;

};


struct vf_stop_ramrod_data {
	u8 vf_id;
	u8 reserved0;
	__le16 reserved1;
	__le32 reserved2;
};


enum vf_zone_size_mode {
	VF_ZONE_SIZE_MODE_DEFAULT,
	VF_ZONE_SIZE_MODE_DOUBLE,
	VF_ZONE_SIZE_MODE_QUAD,
	MAX_VF_ZONE_SIZE_MODE
};


struct xstorm_non_trigger_vf_zone {
	struct regpair non_edpm_ack_pkts;
};


struct xstorm_vf_zone {
	struct xstorm_non_trigger_vf_zone non_trigger;
};


struct atten_status_block {
	__le32 atten_bits;
	__le32 atten_ack;
	__le16 reserved0;
	__le16 sb_index;
	__le32 reserved1;
};


struct dmae_cmd {
	__le32 opcode;
#define DMAE_CMD_SRC_MASK		0x1
#define DMAE_CMD_SRC_SHIFT		0
#define DMAE_CMD_DST_MASK		0x3
#define DMAE_CMD_DST_SHIFT		1
#define DMAE_CMD_C_DST_MASK		0x1
#define DMAE_CMD_C_DST_SHIFT		3
#define DMAE_CMD_CRC_RESET_MASK		0x1
#define DMAE_CMD_CRC_RESET_SHIFT	4
#define DMAE_CMD_SRC_ADDR_RESET_MASK	0x1
#define DMAE_CMD_SRC_ADDR_RESET_SHIFT	5
#define DMAE_CMD_DST_ADDR_RESET_MASK	0x1
#define DMAE_CMD_DST_ADDR_RESET_SHIFT	6
#define DMAE_CMD_COMP_FUNC_MASK		0x1
#define DMAE_CMD_COMP_FUNC_SHIFT	7
#define DMAE_CMD_COMP_WORD_EN_MASK	0x1
#define DMAE_CMD_COMP_WORD_EN_SHIFT	8
#define DMAE_CMD_COMP_CRC_EN_MASK	0x1
#define DMAE_CMD_COMP_CRC_EN_SHIFT	9
#define DMAE_CMD_COMP_CRC_OFFSET_MASK	0x7
#define DMAE_CMD_COMP_CRC_OFFSET_SHIFT 10
#define DMAE_CMD_RESERVED1_MASK		0x1
#define DMAE_CMD_RESERVED1_SHIFT	13
#define DMAE_CMD_ENDIANITY_MODE_MASK	0x3
#define DMAE_CMD_ENDIANITY_MODE_SHIFT	14
#define DMAE_CMD_ERR_HANDLING_MASK	0x3
#define DMAE_CMD_ERR_HANDLING_SHIFT	16
#define DMAE_CMD_PORT_ID_MASK		0x3
#define DMAE_CMD_PORT_ID_SHIFT		18
#define DMAE_CMD_SRC_PF_ID_MASK		0xF
#define DMAE_CMD_SRC_PF_ID_SHIFT	20
#define DMAE_CMD_DST_PF_ID_MASK		0xF
#define DMAE_CMD_DST_PF_ID_SHIFT	24
#define DMAE_CMD_SRC_VF_ID_VALID_MASK	0x1
#define DMAE_CMD_SRC_VF_ID_VALID_SHIFT 28
#define DMAE_CMD_DST_VF_ID_VALID_MASK	0x1
#define DMAE_CMD_DST_VF_ID_VALID_SHIFT 29
#define DMAE_CMD_RESERVED2_MASK		0x3
#define DMAE_CMD_RESERVED2_SHIFT	30
	__le32 src_addr_lo;
	__le32 src_addr_hi;
	__le32 dst_addr_lo;
	__le32 dst_addr_hi;
	__le16 length_dw;
	__le16 opcode_b;
#define DMAE_CMD_SRC_VF_ID_MASK		0xFF
#define DMAE_CMD_SRC_VF_ID_SHIFT	0
#define DMAE_CMD_DST_VF_ID_MASK		0xFF
#define DMAE_CMD_DST_VF_ID_SHIFT	8
	__le32 comp_addr_lo;
	__le32 comp_addr_hi;
	__le32 comp_val;
	__le32 crc32;
	__le32 crc_32_c;
	__le16 crc16;
	__le16 crc16_c;
	__le16 crc10;
	__le16 error_bit_reserved;
#define DMAE_CMD_ERROR_BIT_MASK        0x1
#define DMAE_CMD_ERROR_BIT_SHIFT       0
#define DMAE_CMD_RESERVED_MASK	       0x7FFF
#define DMAE_CMD_RESERVED_SHIFT        1
	__le16 xsum16;
	__le16 xsum8;
};

enum dmae_cmd_comp_crc_en_enum {
	dmae_cmd_comp_crc_disabled,
	dmae_cmd_comp_crc_enabled,
	MAX_DMAE_CMD_COMP_CRC_EN_ENUM
};

enum dmae_cmd_comp_func_enum {
	dmae_cmd_comp_func_to_src,
	dmae_cmd_comp_func_to_dst,
	MAX_DMAE_CMD_COMP_FUNC_ENUM
};

enum dmae_cmd_comp_word_en_enum {
	dmae_cmd_comp_word_disabled,
	dmae_cmd_comp_word_enabled,
	MAX_DMAE_CMD_COMP_WORD_EN_ENUM
};

enum dmae_cmd_c_dst_enum {
	dmae_cmd_c_dst_pcie,
	dmae_cmd_c_dst_grc,
	MAX_DMAE_CMD_C_DST_ENUM
};

enum dmae_cmd_dst_enum {
	dmae_cmd_dst_none_0,
	dmae_cmd_dst_pcie,
	dmae_cmd_dst_grc,
	dmae_cmd_dst_none_3,
	MAX_DMAE_CMD_DST_ENUM
};

enum dmae_cmd_error_handling_enum {
	dmae_cmd_error_handling_send_regular_comp,
	dmae_cmd_error_handling_send_comp_with_err,
	dmae_cmd_error_handling_dont_send_comp,
	MAX_DMAE_CMD_ERROR_HANDLING_ENUM
};

enum dmae_cmd_src_enum {
	dmae_cmd_src_pcie,
	dmae_cmd_src_grc,
	MAX_DMAE_CMD_SRC_ENUM
};

struct mstorm_core_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define MSTORM_CORE_CONN_AG_CTX_BIT0_MASK	0x1
#define MSTORM_CORE_CONN_AG_CTX_BIT0_SHIFT	0
#define MSTORM_CORE_CONN_AG_CTX_BIT1_MASK	0x1
#define MSTORM_CORE_CONN_AG_CTX_BIT1_SHIFT	1
#define MSTORM_CORE_CONN_AG_CTX_CF0_MASK	0x3
#define MSTORM_CORE_CONN_AG_CTX_CF0_SHIFT	2
#define MSTORM_CORE_CONN_AG_CTX_CF1_MASK	0x3
#define MSTORM_CORE_CONN_AG_CTX_CF1_SHIFT	4
#define MSTORM_CORE_CONN_AG_CTX_CF2_MASK	0x3
#define MSTORM_CORE_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define MSTORM_CORE_CONN_AG_CTX_CF0EN_MASK		0x1
#define MSTORM_CORE_CONN_AG_CTX_CF0EN_SHIFT		0
#define MSTORM_CORE_CONN_AG_CTX_CF1EN_MASK		0x1
#define MSTORM_CORE_CONN_AG_CTX_CF1EN_SHIFT		1
#define MSTORM_CORE_CONN_AG_CTX_CF2EN_MASK		0x1
#define MSTORM_CORE_CONN_AG_CTX_CF2EN_SHIFT		2
#define MSTORM_CORE_CONN_AG_CTX_RULE0EN_MASK		0x1
#define MSTORM_CORE_CONN_AG_CTX_RULE0EN_SHIFT	3
#define MSTORM_CORE_CONN_AG_CTX_RULE1EN_MASK		0x1
#define MSTORM_CORE_CONN_AG_CTX_RULE1EN_SHIFT	4
#define MSTORM_CORE_CONN_AG_CTX_RULE2EN_MASK		0x1
#define MSTORM_CORE_CONN_AG_CTX_RULE2EN_SHIFT	5
#define MSTORM_CORE_CONN_AG_CTX_RULE3EN_MASK		0x1
#define MSTORM_CORE_CONN_AG_CTX_RULE3EN_SHIFT	6
#define MSTORM_CORE_CONN_AG_CTX_RULE4EN_MASK		0x1
#define MSTORM_CORE_CONN_AG_CTX_RULE4EN_SHIFT	7
	__le16 word0;
	__le16 word1;
	__le32 reg0;
	__le32 reg1;
};

struct ystorm_core_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define YSTORM_CORE_CONN_AG_CTX_BIT0_MASK	0x1
#define YSTORM_CORE_CONN_AG_CTX_BIT0_SHIFT	0
#define YSTORM_CORE_CONN_AG_CTX_BIT1_MASK	0x1
#define YSTORM_CORE_CONN_AG_CTX_BIT1_SHIFT	1
#define YSTORM_CORE_CONN_AG_CTX_CF0_MASK	0x3
#define YSTORM_CORE_CONN_AG_CTX_CF0_SHIFT	2
#define YSTORM_CORE_CONN_AG_CTX_CF1_MASK	0x3
#define YSTORM_CORE_CONN_AG_CTX_CF1_SHIFT	4
#define YSTORM_CORE_CONN_AG_CTX_CF2_MASK	0x3
#define YSTORM_CORE_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define YSTORM_CORE_CONN_AG_CTX_CF0EN_MASK		0x1
#define YSTORM_CORE_CONN_AG_CTX_CF0EN_SHIFT		0
#define YSTORM_CORE_CONN_AG_CTX_CF1EN_MASK		0x1
#define YSTORM_CORE_CONN_AG_CTX_CF1EN_SHIFT		1
#define YSTORM_CORE_CONN_AG_CTX_CF2EN_MASK		0x1
#define YSTORM_CORE_CONN_AG_CTX_CF2EN_SHIFT		2
#define YSTORM_CORE_CONN_AG_CTX_RULE0EN_MASK		0x1
#define YSTORM_CORE_CONN_AG_CTX_RULE0EN_SHIFT	3
#define YSTORM_CORE_CONN_AG_CTX_RULE1EN_MASK		0x1
#define YSTORM_CORE_CONN_AG_CTX_RULE1EN_SHIFT	4
#define YSTORM_CORE_CONN_AG_CTX_RULE2EN_MASK		0x1
#define YSTORM_CORE_CONN_AG_CTX_RULE2EN_SHIFT	5
#define YSTORM_CORE_CONN_AG_CTX_RULE3EN_MASK		0x1
#define YSTORM_CORE_CONN_AG_CTX_RULE3EN_SHIFT	6
#define YSTORM_CORE_CONN_AG_CTX_RULE4EN_MASK		0x1
#define YSTORM_CORE_CONN_AG_CTX_RULE4EN_SHIFT	7
	u8 byte2;
	u8 byte3;
	__le16 word0;
	__le32 reg0;
	__le32 reg1;
	__le16 word1;
	__le16 word2;
	__le16 word3;
	__le16 word4;
	__le32 reg2;
	__le32 reg3;
};


struct qed_dmae_params {
	u32 flags;

#define QED_DMAE_PARAMS_RW_REPL_SRC_MASK	0x1
#define QED_DMAE_PARAMS_RW_REPL_SRC_SHIFT	0
#define QED_DMAE_PARAMS_SRC_VF_VALID_MASK	0x1
#define QED_DMAE_PARAMS_SRC_VF_VALID_SHIFT	1
#define QED_DMAE_PARAMS_DST_VF_VALID_MASK	0x1
#define QED_DMAE_PARAMS_DST_VF_VALID_SHIFT	2
#define QED_DMAE_PARAMS_COMPLETION_DST_MASK	0x1
#define QED_DMAE_PARAMS_COMPLETION_DST_SHIFT	3
#define QED_DMAE_PARAMS_PORT_VALID_MASK		0x1
#define QED_DMAE_PARAMS_PORT_VALID_SHIFT	4
#define QED_DMAE_PARAMS_SRC_PF_VALID_MASK	0x1
#define QED_DMAE_PARAMS_SRC_PF_VALID_SHIFT	5
#define QED_DMAE_PARAMS_DST_PF_VALID_MASK	0x1
#define QED_DMAE_PARAMS_DST_PF_VALID_SHIFT	6
#define QED_DMAE_PARAMS_RESERVED_MASK		0x1FFFFFF
#define QED_DMAE_PARAMS_RESERVED_SHIFT		7
	u8 src_vfid;
	u8 dst_vfid;
	u8 port_id;
	u8 src_pfid;
	u8 dst_pfid;
	u8 reserved1;
	__le16 reserved2;
};


struct igu_cleanup {
	__le32 sb_id_and_flags;
#define IGU_CLEANUP_RESERVED0_MASK	0x7FFFFFF
#define IGU_CLEANUP_RESERVED0_SHIFT	0
#define IGU_CLEANUP_CLEANUP_SET_MASK	0x1
#define IGU_CLEANUP_CLEANUP_SET_SHIFT	27
#define IGU_CLEANUP_CLEANUP_TYPE_MASK	0x7
#define IGU_CLEANUP_CLEANUP_TYPE_SHIFT	28
#define IGU_CLEANUP_COMMAND_TYPE_MASK	0x1
#define IGU_CLEANUP_COMMAND_TYPE_SHIFT	31
	__le32 reserved1;
};


union igu_command {
	struct igu_prod_cons_update prod_cons_update;
	struct igu_cleanup cleanup;
};


struct igu_command_reg_ctrl {
	__le16 opaque_fid;
	__le16 igu_command_reg_ctrl_fields;
#define IGU_COMMAND_REG_CTRL_PXP_BAR_ADDR_MASK	0xFFF
#define IGU_COMMAND_REG_CTRL_PXP_BAR_ADDR_SHIFT	0
#define IGU_COMMAND_REG_CTRL_RESERVED_MASK	0x7
#define IGU_COMMAND_REG_CTRL_RESERVED_SHIFT	12
#define IGU_COMMAND_REG_CTRL_COMMAND_TYPE_MASK	0x1
#define IGU_COMMAND_REG_CTRL_COMMAND_TYPE_SHIFT	15
};


struct igu_mapping_line {
	__le32 igu_mapping_line_fields;
#define IGU_MAPPING_LINE_VALID_MASK		0x1
#define IGU_MAPPING_LINE_VALID_SHIFT		0
#define IGU_MAPPING_LINE_VECTOR_NUMBER_MASK	0xFF
#define IGU_MAPPING_LINE_VECTOR_NUMBER_SHIFT	1
#define IGU_MAPPING_LINE_FUNCTION_NUMBER_MASK	0xFF
#define IGU_MAPPING_LINE_FUNCTION_NUMBER_SHIFT	9
#define IGU_MAPPING_LINE_PF_VALID_MASK		0x1
#define IGU_MAPPING_LINE_PF_VALID_SHIFT		17
#define IGU_MAPPING_LINE_IPS_GROUP_MASK		0x3F
#define IGU_MAPPING_LINE_IPS_GROUP_SHIFT	18
#define IGU_MAPPING_LINE_RESERVED_MASK		0xFF
#define IGU_MAPPING_LINE_RESERVED_SHIFT		24
};


struct igu_msix_vector {
	struct regpair address;
	__le32 data;
	__le32 msix_vector_fields;
#define IGU_MSIX_VECTOR_MASK_BIT_MASK		0x1
#define IGU_MSIX_VECTOR_MASK_BIT_SHIFT		0
#define IGU_MSIX_VECTOR_RESERVED0_MASK		0x7FFF
#define IGU_MSIX_VECTOR_RESERVED0_SHIFT		1
#define IGU_MSIX_VECTOR_STEERING_TAG_MASK	0xFF
#define IGU_MSIX_VECTOR_STEERING_TAG_SHIFT	16
#define IGU_MSIX_VECTOR_RESERVED1_MASK		0xFF
#define IGU_MSIX_VECTOR_RESERVED1_SHIFT		24
};


struct prs_reg_encapsulation_type_en {
	u8 flags;
#define PRS_REG_ENCAPSULATION_TYPE_EN_ETH_OVER_GRE_ENABLE_MASK		0x1
#define PRS_REG_ENCAPSULATION_TYPE_EN_ETH_OVER_GRE_ENABLE_SHIFT		0
#define PRS_REG_ENCAPSULATION_TYPE_EN_IP_OVER_GRE_ENABLE_MASK		0x1
#define PRS_REG_ENCAPSULATION_TYPE_EN_IP_OVER_GRE_ENABLE_SHIFT		1
#define PRS_REG_ENCAPSULATION_TYPE_EN_VXLAN_ENABLE_MASK			0x1
#define PRS_REG_ENCAPSULATION_TYPE_EN_VXLAN_ENABLE_SHIFT		2
#define PRS_REG_ENCAPSULATION_TYPE_EN_T_TAG_ENABLE_MASK			0x1
#define PRS_REG_ENCAPSULATION_TYPE_EN_T_TAG_ENABLE_SHIFT		3
#define PRS_REG_ENCAPSULATION_TYPE_EN_ETH_OVER_GENEVE_ENABLE_MASK	0x1
#define PRS_REG_ENCAPSULATION_TYPE_EN_ETH_OVER_GENEVE_ENABLE_SHIFT	4
#define PRS_REG_ENCAPSULATION_TYPE_EN_IP_OVER_GENEVE_ENABLE_MASK	0x1
#define PRS_REG_ENCAPSULATION_TYPE_EN_IP_OVER_GENEVE_ENABLE_SHIFT	5
#define PRS_REG_ENCAPSULATION_TYPE_EN_RESERVED_MASK			0x3
#define PRS_REG_ENCAPSULATION_TYPE_EN_RESERVED_SHIFT			6
};

enum pxp_tph_st_hint {
	TPH_ST_HINT_BIDIR,
	TPH_ST_HINT_REQUESTER,
	TPH_ST_HINT_TARGET,
	TPH_ST_HINT_TARGET_PRIO,
	MAX_PXP_TPH_ST_HINT
};


struct qm_rf_bypass_mask {
	u8 flags;
#define QM_RF_BYPASS_MASK_LINEVOQ_MASK		0x1
#define QM_RF_BYPASS_MASK_LINEVOQ_SHIFT		0
#define QM_RF_BYPASS_MASK_RESERVED0_MASK	0x1
#define QM_RF_BYPASS_MASK_RESERVED0_SHIFT	1
#define QM_RF_BYPASS_MASK_PFWFQ_MASK		0x1
#define QM_RF_BYPASS_MASK_PFWFQ_SHIFT		2
#define QM_RF_BYPASS_MASK_VPWFQ_MASK		0x1
#define QM_RF_BYPASS_MASK_VPWFQ_SHIFT		3
#define QM_RF_BYPASS_MASK_PFRL_MASK		0x1
#define QM_RF_BYPASS_MASK_PFRL_SHIFT		4
#define QM_RF_BYPASS_MASK_VPQCNRL_MASK		0x1
#define QM_RF_BYPASS_MASK_VPQCNRL_SHIFT		5
#define QM_RF_BYPASS_MASK_FWPAUSE_MASK		0x1
#define QM_RF_BYPASS_MASK_FWPAUSE_SHIFT		6
#define QM_RF_BYPASS_MASK_RESERVED1_MASK	0x1
#define QM_RF_BYPASS_MASK_RESERVED1_SHIFT	7
};


struct qm_rf_opportunistic_mask {
	__le16 flags;
#define QM_RF_OPPORTUNISTIC_MASK_LINEVOQ_MASK		0x1
#define QM_RF_OPPORTUNISTIC_MASK_LINEVOQ_SHIFT		0
#define QM_RF_OPPORTUNISTIC_MASK_BYTEVOQ_MASK		0x1
#define QM_RF_OPPORTUNISTIC_MASK_BYTEVOQ_SHIFT		1
#define QM_RF_OPPORTUNISTIC_MASK_PFWFQ_MASK		0x1
#define QM_RF_OPPORTUNISTIC_MASK_PFWFQ_SHIFT		2
#define QM_RF_OPPORTUNISTIC_MASK_VPWFQ_MASK		0x1
#define QM_RF_OPPORTUNISTIC_MASK_VPWFQ_SHIFT		3
#define QM_RF_OPPORTUNISTIC_MASK_PFRL_MASK		0x1
#define QM_RF_OPPORTUNISTIC_MASK_PFRL_SHIFT		4
#define QM_RF_OPPORTUNISTIC_MASK_VPQCNRL_MASK		0x1
#define QM_RF_OPPORTUNISTIC_MASK_VPQCNRL_SHIFT		5
#define QM_RF_OPPORTUNISTIC_MASK_FWPAUSE_MASK		0x1
#define QM_RF_OPPORTUNISTIC_MASK_FWPAUSE_SHIFT		6
#define QM_RF_OPPORTUNISTIC_MASK_RESERVED0_MASK		0x1
#define QM_RF_OPPORTUNISTIC_MASK_RESERVED0_SHIFT	7
#define QM_RF_OPPORTUNISTIC_MASK_QUEUEEMPTY_MASK	0x1
#define QM_RF_OPPORTUNISTIC_MASK_QUEUEEMPTY_SHIFT	8
#define QM_RF_OPPORTUNISTIC_MASK_RESERVED1_MASK		0x7F
#define QM_RF_OPPORTUNISTIC_MASK_RESERVED1_SHIFT	9
};


struct qm_rf_pq_map {
	__le32 reg;
#define QM_RF_PQ_MAP_PQ_VALID_MASK		0x1
#define QM_RF_PQ_MAP_PQ_VALID_SHIFT		0
#define QM_RF_PQ_MAP_RL_ID_MASK		0xFF
#define QM_RF_PQ_MAP_RL_ID_SHIFT		1
#define QM_RF_PQ_MAP_VP_PQ_ID_MASK		0x1FF
#define QM_RF_PQ_MAP_VP_PQ_ID_SHIFT		9
#define QM_RF_PQ_MAP_VOQ_MASK		0x1F
#define QM_RF_PQ_MAP_VOQ_SHIFT		18
#define QM_RF_PQ_MAP_WRR_WEIGHT_GROUP_MASK	0x3
#define QM_RF_PQ_MAP_WRR_WEIGHT_GROUP_SHIFT	23
#define QM_RF_PQ_MAP_RL_VALID_MASK		0x1
#define QM_RF_PQ_MAP_RL_VALID_SHIFT		25
#define QM_RF_PQ_MAP_RESERVED_MASK		0x3F
#define QM_RF_PQ_MAP_RESERVED_SHIFT		26
};


struct sdm_agg_int_comp_params {
	__le16 params;
#define SDM_AGG_INT_COMP_PARAMS_AGG_INT_INDEX_MASK	0x3F
#define SDM_AGG_INT_COMP_PARAMS_AGG_INT_INDEX_SHIFT	0
#define SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_ENABLE_MASK	0x1
#define SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_ENABLE_SHIFT	6
#define SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_BIT_MASK	0x1FF
#define SDM_AGG_INT_COMP_PARAMS_AGG_VECTOR_BIT_SHIFT	7
};


struct sdm_op_gen {
	__le32 command;
#define SDM_OP_GEN_COMP_PARAM_MASK	0xFFFF
#define SDM_OP_GEN_COMP_PARAM_SHIFT	0
#define SDM_OP_GEN_COMP_TYPE_MASK	0xF
#define SDM_OP_GEN_COMP_TYPE_SHIFT	16
#define SDM_OP_GEN_RESERVED_MASK	0xFFF
#define SDM_OP_GEN_RESERVED_SHIFT	20
};


struct phys_mem_desc {
	dma_addr_t phys_addr;
	void *virt_addr;
	u32 size;		
};


struct virt_mem_desc {
	void *ptr;
	u32 size;		
};






#define NUM_OF_VLAN_PRIORITIES	8


struct init_brb_ram_req {
	u32 guranteed_per_tc;
	u32 headroom_per_tc;
	u32 min_pkt_size;
	u32 max_ports_per_engine;
	u8 num_active_tcs[MAX_NUM_PORTS];
};


struct init_ets_tc_req {
	u8 use_sp;
	u8 use_wfq;
	u16 weight;
};


struct init_ets_req {
	u32 mtu;
	struct init_ets_tc_req tc_req[NUM_OF_TCS];
};


struct init_nig_lb_rl_req {
	u16 lb_mac_rate;
	u16 lb_rate;
	u32 mtu;
	u16 tc_rate[NUM_OF_PHYS_TCS];
};


struct init_nig_pri_tc_map_entry {
	u8 tc_id;
	u8 valid;
};


struct init_nig_pri_tc_map_req {
	struct init_nig_pri_tc_map_entry pri[NUM_OF_VLAN_PRIORITIES];
};


struct init_qm_global_rl_params {
	u8 type;
	u8 reserved0;
	u16 reserved1;
	u32 rate_limit;
};


struct init_qm_port_params {
	u16 active_phys_tcs;
	u16 num_pbf_cmd_lines;
	u16 num_btb_blocks;
	u8 active;
	u8 reserved;
};


struct init_qm_pq_params {
	u16 vport_id;
	u16 rl_id;
	u8 rl_valid;
	u8 tc_id;
	u8 wrr_group;
	u8 port_id;
};


struct init_qm_rl_params {
	u32 vport_rl;
	u8 vport_rl_type;
	u8 reserved[3];
};


enum init_qm_rl_type {
	QM_RL_TYPE_NORMAL,
	QM_RL_TYPE_QCN,
	MAX_INIT_QM_RL_TYPE
};


struct init_qm_vport_params {
	u16 wfq;
	u16 reserved;
	u16 tc_wfq[NUM_OF_TCS];
	u16 first_tx_pq_id[NUM_OF_TCS];
};






#define GRC_ADDR_BITS	23
#define MAX_GRC_ADDR	(BIT(GRC_ADDR_BITS) - 1)


#define ANY_PHASE_ID	0xffff


#define MAX_ZIPPED_SIZE	8192
enum chip_ids {
	CHIP_BB,
	CHIP_K2,
	MAX_CHIP_IDS
};

struct fw_asserts_ram_section {
	__le16 section_ram_line_offset;
	__le16 section_ram_line_size;
	u8 list_dword_offset;
	u8 list_element_dword_size;
	u8 list_num_elements;
	u8 list_next_index_dword_offset;
};

struct fw_ver_num {
	u8 major;
	u8 minor;
	u8 rev;
	u8 eng;
};

struct fw_ver_info {
	__le16 tools_ver;
	u8 image_id;
	u8 reserved1;
	struct fw_ver_num num;
	__le32 timestamp;
	__le32 reserved2;
};

struct fw_info {
	struct fw_ver_info ver;
	struct fw_asserts_ram_section fw_asserts_section;
};

struct fw_info_location {
	__le32 grc_addr;
	__le32 size;
};

enum init_modes {
	MODE_BB_A0_DEPRECATED,
	MODE_BB,
	MODE_K2,
	MODE_ASIC,
	MODE_EMUL_REDUCED,
	MODE_EMUL_FULL,
	MODE_FPGA,
	MODE_CHIPSIM,
	MODE_SF,
	MODE_MF_SD,
	MODE_MF_SI,
	MODE_PORTS_PER_ENG_1,
	MODE_PORTS_PER_ENG_2,
	MODE_PORTS_PER_ENG_4,
	MODE_100G,
	MODE_SKIP_PRAM_INIT,
	MODE_EMUL_MAC,
	MAX_INIT_MODES
};

enum init_phases {
	PHASE_ENGINE,
	PHASE_PORT,
	PHASE_PF,
	PHASE_VF,
	PHASE_QM_PF,
	MAX_INIT_PHASES
};

enum init_split_types {
	SPLIT_TYPE_NONE,
	SPLIT_TYPE_PORT,
	SPLIT_TYPE_PF,
	SPLIT_TYPE_PORT_PF,
	SPLIT_TYPE_VF,
	MAX_INIT_SPLIT_TYPES
};


struct bin_buffer_hdr {
	u32 offset;
	u32 length;
};


enum bin_init_buffer_type {
	BIN_BUF_INIT_FW_VER_INFO,
	BIN_BUF_INIT_CMD,
	BIN_BUF_INIT_VAL,
	BIN_BUF_INIT_MODE_TREE,
	BIN_BUF_INIT_IRO,
	BIN_BUF_INIT_OVERLAYS,
	MAX_BIN_INIT_BUFFER_TYPE
};


struct fw_overlay_buf_hdr {
	u32 data;
#define FW_OVERLAY_BUF_HDR_STORM_ID_MASK  0xFF
#define FW_OVERLAY_BUF_HDR_STORM_ID_SHIFT 0
#define FW_OVERLAY_BUF_HDR_BUF_SIZE_MASK  0xFFFFFF
#define FW_OVERLAY_BUF_HDR_BUF_SIZE_SHIFT 8
};


struct init_array_raw_hdr {
	__le32						data;
#define INIT_ARRAY_RAW_HDR_TYPE_MASK			0xF
#define INIT_ARRAY_RAW_HDR_TYPE_SHIFT			0
#define INIT_ARRAY_RAW_HDR_PARAMS_MASK			0xFFFFFFF
#define INIT_ARRAY_RAW_HDR_PARAMS_SHIFT			4
};


struct init_array_standard_hdr {
	__le32						data;
#define INIT_ARRAY_STANDARD_HDR_TYPE_MASK		0xF
#define INIT_ARRAY_STANDARD_HDR_TYPE_SHIFT		0
#define INIT_ARRAY_STANDARD_HDR_SIZE_MASK		0xFFFFFFF
#define INIT_ARRAY_STANDARD_HDR_SIZE_SHIFT		4
};


struct init_array_zipped_hdr {
	__le32						data;
#define INIT_ARRAY_ZIPPED_HDR_TYPE_MASK			0xF
#define INIT_ARRAY_ZIPPED_HDR_TYPE_SHIFT		0
#define INIT_ARRAY_ZIPPED_HDR_ZIPPED_SIZE_MASK		0xFFFFFFF
#define INIT_ARRAY_ZIPPED_HDR_ZIPPED_SIZE_SHIFT		4
};


struct init_array_pattern_hdr {
	__le32						data;
#define INIT_ARRAY_PATTERN_HDR_TYPE_MASK		0xF
#define INIT_ARRAY_PATTERN_HDR_TYPE_SHIFT		0
#define INIT_ARRAY_PATTERN_HDR_PATTERN_SIZE_MASK	0xF
#define INIT_ARRAY_PATTERN_HDR_PATTERN_SIZE_SHIFT	4
#define INIT_ARRAY_PATTERN_HDR_REPETITIONS_MASK		0xFFFFFF
#define INIT_ARRAY_PATTERN_HDR_REPETITIONS_SHIFT	8
};


union init_array_hdr {
	struct init_array_raw_hdr			raw;
	struct init_array_standard_hdr			standard;
	struct init_array_zipped_hdr			zipped;
	struct init_array_pattern_hdr			pattern;
};


enum init_array_types {
	INIT_ARR_STANDARD,
	INIT_ARR_ZIPPED,
	INIT_ARR_PATTERN,
	MAX_INIT_ARRAY_TYPES
};


struct init_callback_op {
	__le32						op_data;
#define INIT_CALLBACK_OP_OP_MASK			0xF
#define INIT_CALLBACK_OP_OP_SHIFT			0
#define INIT_CALLBACK_OP_RESERVED_MASK			0xFFFFFFF
#define INIT_CALLBACK_OP_RESERVED_SHIFT			4
	__le16						callback_id;
	__le16						block_id;
};


struct init_delay_op {
	__le32						op_data;
#define INIT_DELAY_OP_OP_MASK				0xF
#define INIT_DELAY_OP_OP_SHIFT				0
#define INIT_DELAY_OP_RESERVED_MASK			0xFFFFFFF
#define INIT_DELAY_OP_RESERVED_SHIFT			4
	__le32						delay;
};


struct init_if_mode_op {
	__le32						op_data;
#define INIT_IF_MODE_OP_OP_MASK				0xF
#define INIT_IF_MODE_OP_OP_SHIFT			0
#define INIT_IF_MODE_OP_RESERVED1_MASK			0xFFF
#define INIT_IF_MODE_OP_RESERVED1_SHIFT			4
#define INIT_IF_MODE_OP_CMD_OFFSET_MASK			0xFFFF
#define INIT_IF_MODE_OP_CMD_OFFSET_SHIFT		16
	__le16						reserved2;
	__le16						modes_buf_offset;
};


struct init_if_phase_op {
	__le32						op_data;
#define INIT_IF_PHASE_OP_OP_MASK			0xF
#define INIT_IF_PHASE_OP_OP_SHIFT			0
#define INIT_IF_PHASE_OP_RESERVED1_MASK			0xFFF
#define INIT_IF_PHASE_OP_RESERVED1_SHIFT		4
#define INIT_IF_PHASE_OP_CMD_OFFSET_MASK		0xFFFF
#define INIT_IF_PHASE_OP_CMD_OFFSET_SHIFT		16
	__le32						phase_data;
#define INIT_IF_PHASE_OP_PHASE_MASK			0xFF
#define INIT_IF_PHASE_OP_PHASE_SHIFT			0
#define INIT_IF_PHASE_OP_RESERVED2_MASK			0xFF
#define INIT_IF_PHASE_OP_RESERVED2_SHIFT		8
#define INIT_IF_PHASE_OP_PHASE_ID_MASK			0xFFFF
#define INIT_IF_PHASE_OP_PHASE_ID_SHIFT			16
};


enum init_mode_ops {
	INIT_MODE_OP_NOT,
	INIT_MODE_OP_OR,
	INIT_MODE_OP_AND,
	MAX_INIT_MODE_OPS
};


struct init_raw_op {
	__le32						op_data;
#define INIT_RAW_OP_OP_MASK				0xF
#define INIT_RAW_OP_OP_SHIFT				0
#define INIT_RAW_OP_PARAM1_MASK				0xFFFFFFF
#define INIT_RAW_OP_PARAM1_SHIFT			4
	__le32						param2;
};


struct init_op_array_params {
	__le16						size;
	__le16						offset;
};


union init_write_args {
	__le32						inline_val;
	__le32						zeros_count;
	__le32						array_offset;
	struct init_op_array_params			runtime;
};


struct init_write_op {
	__le32						data;
#define INIT_WRITE_OP_OP_MASK				0xF
#define INIT_WRITE_OP_OP_SHIFT				0
#define INIT_WRITE_OP_SOURCE_MASK			0x7
#define INIT_WRITE_OP_SOURCE_SHIFT			4
#define INIT_WRITE_OP_RESERVED_MASK			0x1
#define INIT_WRITE_OP_RESERVED_SHIFT			7
#define INIT_WRITE_OP_WIDE_BUS_MASK			0x1
#define INIT_WRITE_OP_WIDE_BUS_SHIFT			8
#define INIT_WRITE_OP_ADDRESS_MASK			0x7FFFFF
#define INIT_WRITE_OP_ADDRESS_SHIFT			9
	union init_write_args				args;
};


struct init_read_op {
	__le32						op_data;
#define INIT_READ_OP_OP_MASK				0xF
#define INIT_READ_OP_OP_SHIFT				0
#define INIT_READ_OP_POLL_TYPE_MASK			0xF
#define INIT_READ_OP_POLL_TYPE_SHIFT			4
#define INIT_READ_OP_RESERVED_MASK			0x1
#define INIT_READ_OP_RESERVED_SHIFT			8
#define INIT_READ_OP_ADDRESS_MASK			0x7FFFFF
#define INIT_READ_OP_ADDRESS_SHIFT			9
	__le32						expected_val;
};


union init_op {
	struct init_raw_op				raw;
	struct init_write_op				write;
	struct init_read_op				read;
	struct init_if_mode_op				if_mode;
	struct init_if_phase_op				if_phase;
	struct init_callback_op				callback;
	struct init_delay_op				delay;
};


enum init_op_types {
	INIT_OP_READ,
	INIT_OP_WRITE,
	INIT_OP_IF_MODE,
	INIT_OP_IF_PHASE,
	INIT_OP_DELAY,
	INIT_OP_CALLBACK,
	MAX_INIT_OP_TYPES
};


enum init_poll_types {
	INIT_POLL_NONE,
	INIT_POLL_EQ,
	INIT_POLL_OR,
	INIT_POLL_AND,
	MAX_INIT_POLL_TYPES
};


enum init_source_types {
	INIT_SRC_INLINE,
	INIT_SRC_ZEROS,
	INIT_SRC_ARRAY,
	INIT_SRC_RUNTIME,
	MAX_INIT_SOURCE_TYPES
};


struct iro {
	u32 base;
	u16 m1;
	u16 m2;
	u16 m3;
	u16 size;
};


#define GTT_BAR0_MAP_REG_IGU_CMD	0x00f000UL


#define GTT_BAR0_MAP_REG_TSDM_RAM	0x010000UL


#define GTT_BAR0_MAP_REG_MSDM_RAM	0x011000UL


#define GTT_BAR0_MAP_REG_MSDM_RAM_1024	0x012000UL


#define GTT_BAR0_MAP_REG_MSDM_RAM_2048	0x013000UL


#define GTT_BAR0_MAP_REG_USDM_RAM	0x014000UL


#define GTT_BAR0_MAP_REG_USDM_RAM_1024	0x015000UL


#define GTT_BAR0_MAP_REG_USDM_RAM_2048	0x016000UL


#define GTT_BAR0_MAP_REG_XSDM_RAM	0x017000UL


#define GTT_BAR0_MAP_REG_XSDM_RAM_1024	0x018000UL


#define GTT_BAR0_MAP_REG_YSDM_RAM	0x019000UL


#define GTT_BAR0_MAP_REG_PSDM_RAM	0x01a000UL


#define VOQ(port, tc, max_phys_tcs_per_port)   ((tc) ==                       \
						PURE_LB_TC ? NUM_OF_PHYS_TCS *\
						MAX_NUM_PORTS_BB +            \
						(port) : (port) *             \
						(max_phys_tcs_per_port) + (tc))

struct init_qm_pq_params;


u32 qed_qm_pf_mem_size(u32 num_pf_cids,
		       u32 num_vf_cids,
		       u32 num_tids, u16 num_pf_pqs, u16 num_vf_pqs);

struct qed_qm_common_rt_init_params {
	u8 max_ports_per_engine;
	u8 max_phys_tcs_per_port;
	bool pf_rl_en;
	bool pf_wfq_en;
	bool global_rl_en;
	bool vport_wfq_en;
	struct init_qm_port_params *port_params;
	struct init_qm_global_rl_params
	global_rl_params[COMMON_MAX_QM_GLOBAL_RLS];
};


int qed_qm_common_rt_init(struct qed_hwfn *p_hwfn,
			  struct qed_qm_common_rt_init_params *p_params);

struct qed_qm_pf_rt_init_params {
	u8 port_id;
	u8 pf_id;
	u8 max_phys_tcs_per_port;
	bool is_pf_loading;
	u32 num_pf_cids;
	u32 num_vf_cids;
	u32 num_tids;
	u16 start_pq;
	u16 num_pf_pqs;
	u16 num_vf_pqs;
	u16 start_vport;
	u16 num_vports;
	u16 start_rl;
	u16 num_rls;
	u16 pf_wfq;
	u32 pf_rl;
	u32 link_speed;
	struct init_qm_pq_params *pq_params;
	struct init_qm_vport_params *vport_params;
	struct init_qm_rl_params *rl_params;
};


int qed_qm_pf_rt_init(struct qed_hwfn *p_hwfn,
		      struct qed_ptt *p_ptt,
		      struct qed_qm_pf_rt_init_params *p_params);


int qed_init_pf_wfq(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt, u8 pf_id, u16 pf_wfq);


int qed_init_pf_rl(struct qed_hwfn *p_hwfn,
		   struct qed_ptt *p_ptt, u8 pf_id, u32 pf_rl);


int qed_init_vport_wfq(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       u16 first_tx_pq_id[NUM_OF_TCS], u16 wfq);


int qed_init_vport_tc_wfq(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt,
			  u16 first_tx_pq_id, u16 weight);


int qed_init_global_rl(struct qed_hwfn *p_hwfn,
		       struct qed_ptt *p_ptt,
		       u16 rl_id, u32 rate_limit,
		       enum init_qm_rl_type vport_rl_type);


bool qed_send_qm_stop_cmd(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt,
			  bool is_release_cmd,
			  bool is_tx_pq, u16 start_pq, u16 num_pqs);


void qed_set_vxlan_dest_port(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt, u16 dest_port);


void qed_set_vxlan_enable(struct qed_hwfn *p_hwfn,
			  struct qed_ptt *p_ptt, bool vxlan_enable);


void qed_set_gre_enable(struct qed_hwfn *p_hwfn,
			struct qed_ptt *p_ptt,
			bool eth_gre_enable, bool ip_gre_enable);


void qed_set_geneve_dest_port(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt, u16 dest_port);


void qed_set_geneve_enable(struct qed_hwfn *p_hwfn,
			   struct qed_ptt *p_ptt,
			   bool eth_geneve_enable, bool ip_geneve_enable);

void qed_set_vxlan_no_l2_enable(struct qed_hwfn *p_hwfn,
				struct qed_ptt *p_ptt, bool enable);


void qed_gft_disable(struct qed_hwfn *p_hwfn, struct qed_ptt *p_ptt, u16 pf_id);


void qed_gft_config(struct qed_hwfn *p_hwfn,
		    struct qed_ptt *p_ptt,
		    u16 pf_id,
		    bool tcp,
		    bool udp,
		    bool ipv4, bool ipv6, enum gft_profile_type profile_type);


void qed_enable_context_validation(struct qed_hwfn *p_hwfn,
				   struct qed_ptt *p_ptt);


void qed_calc_session_ctx_validation(void *p_ctx_mem,
				     u16 ctx_size, u8 ctx_type, u32 cid);


void qed_calc_task_ctx_validation(void *p_ctx_mem,
				  u16 ctx_size, u8 ctx_type, u32 tid);


void qed_memset_session_ctx(void *p_ctx_mem, u32 ctx_size, u8 ctx_type);


void qed_memset_task_ctx(void *p_ctx_mem, u32 ctx_size, u8 ctx_type);

#define NUM_STORMS 6


const char *qed_get_protocol_type_str(u32 protocol_type);


const char *qed_get_ramrod_cmd_id_str(u32 protocol_type, u32 ramrod_cmd_id);


void qed_set_rdma_error_level(struct qed_hwfn *p_hwfn,
			      struct qed_ptt *p_ptt,
			      u8 assert_level[NUM_STORMS]);

struct phys_mem_desc *
qed_fw_overlay_mem_alloc(struct qed_hwfn *p_hwfn,
			 const u32 *const fw_overlay_in_buf,
			 u32 buf_size_in_bytes);


void qed_fw_overlay_init_ram(struct qed_hwfn *p_hwfn,
			     struct qed_ptt *p_ptt,
			     struct phys_mem_desc *fw_overlay_mem);


void qed_fw_overlay_mem_free(struct qed_hwfn *p_hwfn,
			     struct phys_mem_desc **fw_overlay_mem);

#define PCICFG_OFFSET					0x2000
#define GRC_CONFIG_REG_PF_INIT_VF			0x624


#define GRC_CR_PF_INIT_VF_PF_FIRST_VF_NUM_MASK		0xff


#define DORQ_REG_PF_MAX_ICID_0_RT_OFFSET				0
#define DORQ_REG_PF_MAX_ICID_1_RT_OFFSET				1
#define DORQ_REG_PF_MAX_ICID_2_RT_OFFSET				2
#define DORQ_REG_PF_MAX_ICID_3_RT_OFFSET				3
#define DORQ_REG_PF_MAX_ICID_4_RT_OFFSET				4
#define DORQ_REG_PF_MAX_ICID_5_RT_OFFSET				5
#define DORQ_REG_PF_MAX_ICID_6_RT_OFFSET				6
#define DORQ_REG_PF_MAX_ICID_7_RT_OFFSET				7
#define DORQ_REG_VF_MAX_ICID_0_RT_OFFSET				8
#define DORQ_REG_VF_MAX_ICID_1_RT_OFFSET				9
#define DORQ_REG_VF_MAX_ICID_2_RT_OFFSET				10
#define DORQ_REG_VF_MAX_ICID_3_RT_OFFSET				11
#define DORQ_REG_VF_MAX_ICID_4_RT_OFFSET				12
#define DORQ_REG_VF_MAX_ICID_5_RT_OFFSET				13
#define DORQ_REG_VF_MAX_ICID_6_RT_OFFSET				14
#define DORQ_REG_VF_MAX_ICID_7_RT_OFFSET				15
#define DORQ_REG_VF_ICID_BIT_SHIFT_NORM_RT_OFFSET			16
#define DORQ_REG_PF_WAKE_ALL_RT_OFFSET					17
#define DORQ_REG_TAG1_ETHERTYPE_RT_OFFSET				18
#define IGU_REG_PF_CONFIGURATION_RT_OFFSET				19
#define IGU_REG_VF_CONFIGURATION_RT_OFFSET				20
#define IGU_REG_ATTN_MSG_ADDR_L_RT_OFFSET				21
#define IGU_REG_ATTN_MSG_ADDR_H_RT_OFFSET				22
#define IGU_REG_LEADING_EDGE_LATCH_RT_OFFSET				23
#define IGU_REG_TRAILING_EDGE_LATCH_RT_OFFSET				24
#define CAU_REG_CQE_AGG_UNIT_SIZE_RT_OFFSET				25
#define CAU_REG_SB_VAR_MEMORY_RT_OFFSET					26
#define CAU_REG_SB_VAR_MEMORY_RT_SIZE					736
#define CAU_REG_SB_ADDR_MEMORY_RT_OFFSET				762
#define CAU_REG_SB_ADDR_MEMORY_RT_SIZE					736
#define CAU_REG_PI_MEMORY_RT_OFFSET					1498
#define CAU_REG_PI_MEMORY_RT_SIZE					4416
#define PRS_REG_SEARCH_RESP_INITIATOR_TYPE_RT_OFFSET			5914
#define PRS_REG_TASK_ID_MAX_INITIATOR_PF_RT_OFFSET			5915
#define PRS_REG_TASK_ID_MAX_INITIATOR_VF_RT_OFFSET			5916
#define PRS_REG_TASK_ID_MAX_TARGET_PF_RT_OFFSET				5917
#define PRS_REG_TASK_ID_MAX_TARGET_VF_RT_OFFSET				5918
#define PRS_REG_SEARCH_TCP_RT_OFFSET					5919
#define PRS_REG_SEARCH_FCOE_RT_OFFSET					5920
#define PRS_REG_SEARCH_ROCE_RT_OFFSET					5921
#define PRS_REG_ROCE_DEST_QP_MAX_VF_RT_OFFSET				5922
#define PRS_REG_ROCE_DEST_QP_MAX_PF_RT_OFFSET				5923
#define PRS_REG_SEARCH_OPENFLOW_RT_OFFSET				5924
#define PRS_REG_SEARCH_NON_IP_AS_OPENFLOW_RT_OFFSET			5925
#define PRS_REG_OPENFLOW_SUPPORT_ONLY_KNOWN_OVER_IP_RT_OFFSET		5926
#define PRS_REG_OPENFLOW_SEARCH_KEY_MASK_RT_OFFSET			5927
#define PRS_REG_TAG_ETHERTYPE_0_RT_OFFSET				5928
#define PRS_REG_LIGHT_L2_ETHERTYPE_EN_RT_OFFSET				5929
#define SRC_REG_FIRSTFREE_RT_OFFSET					5930
#define SRC_REG_FIRSTFREE_RT_SIZE					2
#define SRC_REG_LASTFREE_RT_OFFSET					5932
#define SRC_REG_LASTFREE_RT_SIZE					2
#define SRC_REG_COUNTFREE_RT_OFFSET					5934
#define SRC_REG_NUMBER_HASH_BITS_RT_OFFSET				5935
#define PSWRQ2_REG_CDUT_P_SIZE_RT_OFFSET				5936
#define PSWRQ2_REG_CDUC_P_SIZE_RT_OFFSET				5937
#define PSWRQ2_REG_TM_P_SIZE_RT_OFFSET					5938
#define PSWRQ2_REG_QM_P_SIZE_RT_OFFSET					5939
#define PSWRQ2_REG_SRC_P_SIZE_RT_OFFSET					5940
#define PSWRQ2_REG_TSDM_P_SIZE_RT_OFFSET				5941
#define PSWRQ2_REG_TM_FIRST_ILT_RT_OFFSET				5942
#define PSWRQ2_REG_TM_LAST_ILT_RT_OFFSET				5943
#define PSWRQ2_REG_QM_FIRST_ILT_RT_OFFSET				5944
#define PSWRQ2_REG_QM_LAST_ILT_RT_OFFSET				5945
#define PSWRQ2_REG_SRC_FIRST_ILT_RT_OFFSET				5946
#define PSWRQ2_REG_SRC_LAST_ILT_RT_OFFSET				5947
#define PSWRQ2_REG_CDUC_FIRST_ILT_RT_OFFSET				5948
#define PSWRQ2_REG_CDUC_LAST_ILT_RT_OFFSET				5949
#define PSWRQ2_REG_CDUT_FIRST_ILT_RT_OFFSET				5950
#define PSWRQ2_REG_CDUT_LAST_ILT_RT_OFFSET				5951
#define PSWRQ2_REG_TSDM_FIRST_ILT_RT_OFFSET				5952
#define PSWRQ2_REG_TSDM_LAST_ILT_RT_OFFSET				5953
#define PSWRQ2_REG_TM_NUMBER_OF_PF_BLOCKS_RT_OFFSET			5954
#define PSWRQ2_REG_CDUT_NUMBER_OF_PF_BLOCKS_RT_OFFSET			5955
#define PSWRQ2_REG_CDUC_NUMBER_OF_PF_BLOCKS_RT_OFFSET			5956
#define PSWRQ2_REG_TM_VF_BLOCKS_RT_OFFSET				5957
#define PSWRQ2_REG_CDUT_VF_BLOCKS_RT_OFFSET				5958
#define PSWRQ2_REG_CDUC_VF_BLOCKS_RT_OFFSET				5959
#define PSWRQ2_REG_TM_BLOCKS_FACTOR_RT_OFFSET				5960
#define PSWRQ2_REG_CDUT_BLOCKS_FACTOR_RT_OFFSET				5961
#define PSWRQ2_REG_CDUC_BLOCKS_FACTOR_RT_OFFSET				5962
#define PSWRQ2_REG_VF_BASE_RT_OFFSET					5963
#define PSWRQ2_REG_VF_LAST_ILT_RT_OFFSET				5964
#define PSWRQ2_REG_DRAM_ALIGN_WR_RT_OFFSET				5965
#define PSWRQ2_REG_DRAM_ALIGN_RD_RT_OFFSET				5966
#define PSWRQ2_REG_ILT_MEMORY_RT_OFFSET					5967
#define PSWRQ2_REG_ILT_MEMORY_RT_SIZE					22000
#define PGLUE_REG_B_VF_BASE_RT_OFFSET					27967
#define PGLUE_REG_B_MSDM_OFFSET_MASK_B_RT_OFFSET			27968
#define PGLUE_REG_B_MSDM_VF_SHIFT_B_RT_OFFSET				27969
#define PGLUE_REG_B_CACHE_LINE_SIZE_RT_OFFSET				27970
#define PGLUE_REG_B_PF_BAR0_SIZE_RT_OFFSET				27971
#define PGLUE_REG_B_PF_BAR1_SIZE_RT_OFFSET				27972
#define PGLUE_REG_B_VF_BAR1_SIZE_RT_OFFSET				27973
#define TM_REG_VF_ENABLE_CONN_RT_OFFSET					27974
#define TM_REG_PF_ENABLE_CONN_RT_OFFSET					27975
#define TM_REG_PF_ENABLE_TASK_RT_OFFSET					27976
#define TM_REG_GROUP_SIZE_RESOLUTION_CONN_RT_OFFSET			27977
#define TM_REG_GROUP_SIZE_RESOLUTION_TASK_RT_OFFSET			27978
#define TM_REG_CONFIG_CONN_MEM_RT_OFFSET				27979
#define TM_REG_CONFIG_CONN_MEM_RT_SIZE					416
#define TM_REG_CONFIG_TASK_MEM_RT_OFFSET				28395
#define TM_REG_CONFIG_TASK_MEM_RT_SIZE					512
#define QM_REG_MAXPQSIZE_0_RT_OFFSET					28907
#define QM_REG_MAXPQSIZE_1_RT_OFFSET					28908
#define QM_REG_MAXPQSIZE_2_RT_OFFSET					28909
#define QM_REG_MAXPQSIZETXSEL_0_RT_OFFSET				28910
#define QM_REG_MAXPQSIZETXSEL_1_RT_OFFSET				28911
#define QM_REG_MAXPQSIZETXSEL_2_RT_OFFSET				28912
#define QM_REG_MAXPQSIZETXSEL_3_RT_OFFSET				28913
#define QM_REG_MAXPQSIZETXSEL_4_RT_OFFSET				28914
#define QM_REG_MAXPQSIZETXSEL_5_RT_OFFSET				28915
#define QM_REG_MAXPQSIZETXSEL_6_RT_OFFSET				28916
#define QM_REG_MAXPQSIZETXSEL_7_RT_OFFSET				28917
#define QM_REG_MAXPQSIZETXSEL_8_RT_OFFSET				28918
#define QM_REG_MAXPQSIZETXSEL_9_RT_OFFSET				28919
#define QM_REG_MAXPQSIZETXSEL_10_RT_OFFSET				28920
#define QM_REG_MAXPQSIZETXSEL_11_RT_OFFSET				28921
#define QM_REG_MAXPQSIZETXSEL_12_RT_OFFSET				28922
#define QM_REG_MAXPQSIZETXSEL_13_RT_OFFSET				28923
#define QM_REG_MAXPQSIZETXSEL_14_RT_OFFSET				28924
#define QM_REG_MAXPQSIZETXSEL_15_RT_OFFSET				28925
#define QM_REG_MAXPQSIZETXSEL_16_RT_OFFSET				28926
#define QM_REG_MAXPQSIZETXSEL_17_RT_OFFSET				28927
#define QM_REG_MAXPQSIZETXSEL_18_RT_OFFSET				28928
#define QM_REG_MAXPQSIZETXSEL_19_RT_OFFSET				28929
#define QM_REG_MAXPQSIZETXSEL_20_RT_OFFSET				28930
#define QM_REG_MAXPQSIZETXSEL_21_RT_OFFSET				28931
#define QM_REG_MAXPQSIZETXSEL_22_RT_OFFSET				28932
#define QM_REG_MAXPQSIZETXSEL_23_RT_OFFSET				28933
#define QM_REG_MAXPQSIZETXSEL_24_RT_OFFSET				28934
#define QM_REG_MAXPQSIZETXSEL_25_RT_OFFSET				28935
#define QM_REG_MAXPQSIZETXSEL_26_RT_OFFSET				28936
#define QM_REG_MAXPQSIZETXSEL_27_RT_OFFSET				28937
#define QM_REG_MAXPQSIZETXSEL_28_RT_OFFSET				28938
#define QM_REG_MAXPQSIZETXSEL_29_RT_OFFSET				28939
#define QM_REG_MAXPQSIZETXSEL_30_RT_OFFSET				28940
#define QM_REG_MAXPQSIZETXSEL_31_RT_OFFSET				28941
#define QM_REG_MAXPQSIZETXSEL_32_RT_OFFSET				28942
#define QM_REG_MAXPQSIZETXSEL_33_RT_OFFSET				28943
#define QM_REG_MAXPQSIZETXSEL_34_RT_OFFSET				28944
#define QM_REG_MAXPQSIZETXSEL_35_RT_OFFSET				28945
#define QM_REG_MAXPQSIZETXSEL_36_RT_OFFSET				28946
#define QM_REG_MAXPQSIZETXSEL_37_RT_OFFSET				28947
#define QM_REG_MAXPQSIZETXSEL_38_RT_OFFSET				28948
#define QM_REG_MAXPQSIZETXSEL_39_RT_OFFSET				28949
#define QM_REG_MAXPQSIZETXSEL_40_RT_OFFSET				28950
#define QM_REG_MAXPQSIZETXSEL_41_RT_OFFSET				28951
#define QM_REG_MAXPQSIZETXSEL_42_RT_OFFSET				28952
#define QM_REG_MAXPQSIZETXSEL_43_RT_OFFSET				28953
#define QM_REG_MAXPQSIZETXSEL_44_RT_OFFSET				28954
#define QM_REG_MAXPQSIZETXSEL_45_RT_OFFSET				28955
#define QM_REG_MAXPQSIZETXSEL_46_RT_OFFSET				28956
#define QM_REG_MAXPQSIZETXSEL_47_RT_OFFSET				28957
#define QM_REG_MAXPQSIZETXSEL_48_RT_OFFSET				28958
#define QM_REG_MAXPQSIZETXSEL_49_RT_OFFSET				28959
#define QM_REG_MAXPQSIZETXSEL_50_RT_OFFSET				28960
#define QM_REG_MAXPQSIZETXSEL_51_RT_OFFSET				28961
#define QM_REG_MAXPQSIZETXSEL_52_RT_OFFSET				28962
#define QM_REG_MAXPQSIZETXSEL_53_RT_OFFSET				28963
#define QM_REG_MAXPQSIZETXSEL_54_RT_OFFSET				28964
#define QM_REG_MAXPQSIZETXSEL_55_RT_OFFSET				28965
#define QM_REG_MAXPQSIZETXSEL_56_RT_OFFSET				28966
#define QM_REG_MAXPQSIZETXSEL_57_RT_OFFSET				28967
#define QM_REG_MAXPQSIZETXSEL_58_RT_OFFSET				28968
#define QM_REG_MAXPQSIZETXSEL_59_RT_OFFSET				28969
#define QM_REG_MAXPQSIZETXSEL_60_RT_OFFSET				28970
#define QM_REG_MAXPQSIZETXSEL_61_RT_OFFSET				28971
#define QM_REG_MAXPQSIZETXSEL_62_RT_OFFSET				28972
#define QM_REG_MAXPQSIZETXSEL_63_RT_OFFSET				28973
#define QM_REG_BASEADDROTHERPQ_RT_OFFSET				28974
#define QM_REG_BASEADDROTHERPQ_RT_SIZE					128
#define QM_REG_PTRTBLOTHER_RT_OFFSET					29102
#define QM_REG_PTRTBLOTHER_RT_SIZE					256
#define QM_REG_VOQCRDLINE_RT_OFFSET					29358
#define QM_REG_VOQCRDLINE_RT_SIZE					20
#define QM_REG_VOQINITCRDLINE_RT_OFFSET					29378
#define QM_REG_VOQINITCRDLINE_RT_SIZE					20
#define QM_REG_AFULLQMBYPTHRPFWFQ_RT_OFFSET				29398
#define QM_REG_AFULLQMBYPTHRVPWFQ_RT_OFFSET				29399
#define QM_REG_AFULLQMBYPTHRPFRL_RT_OFFSET				29400
#define QM_REG_AFULLQMBYPTHRGLBLRL_RT_OFFSET				29401
#define QM_REG_AFULLOPRTNSTCCRDMASK_RT_OFFSET				29402
#define QM_REG_WRROTHERPQGRP_0_RT_OFFSET				29403
#define QM_REG_WRROTHERPQGRP_1_RT_OFFSET				29404
#define QM_REG_WRROTHERPQGRP_2_RT_OFFSET				29405
#define QM_REG_WRROTHERPQGRP_3_RT_OFFSET				29406
#define QM_REG_WRROTHERPQGRP_4_RT_OFFSET				29407
#define QM_REG_WRROTHERPQGRP_5_RT_OFFSET				29408
#define QM_REG_WRROTHERPQGRP_6_RT_OFFSET				29409
#define QM_REG_WRROTHERPQGRP_7_RT_OFFSET				29410
#define QM_REG_WRROTHERPQGRP_8_RT_OFFSET				29411
#define QM_REG_WRROTHERPQGRP_9_RT_OFFSET				29412
#define QM_REG_WRROTHERPQGRP_10_RT_OFFSET				29413
#define QM_REG_WRROTHERPQGRP_11_RT_OFFSET				29414
#define QM_REG_WRROTHERPQGRP_12_RT_OFFSET				29415
#define QM_REG_WRROTHERPQGRP_13_RT_OFFSET				29416
#define QM_REG_WRROTHERPQGRP_14_RT_OFFSET				29417
#define QM_REG_WRROTHERPQGRP_15_RT_OFFSET				29418
#define QM_REG_WRROTHERGRPWEIGHT_0_RT_OFFSET				29419
#define QM_REG_WRROTHERGRPWEIGHT_1_RT_OFFSET				29420
#define QM_REG_WRROTHERGRPWEIGHT_2_RT_OFFSET				29421
#define QM_REG_WRROTHERGRPWEIGHT_3_RT_OFFSET				29422
#define QM_REG_WRRTXGRPWEIGHT_0_RT_OFFSET				29423
#define QM_REG_WRRTXGRPWEIGHT_1_RT_OFFSET				29424
#define QM_REG_PQTX2PF_0_RT_OFFSET					29425
#define QM_REG_PQTX2PF_1_RT_OFFSET					29426
#define QM_REG_PQTX2PF_2_RT_OFFSET					29427
#define QM_REG_PQTX2PF_3_RT_OFFSET					29428
#define QM_REG_PQTX2PF_4_RT_OFFSET					29429
#define QM_REG_PQTX2PF_5_RT_OFFSET					29430
#define QM_REG_PQTX2PF_6_RT_OFFSET					29431
#define QM_REG_PQTX2PF_7_RT_OFFSET					29432
#define QM_REG_PQTX2PF_8_RT_OFFSET					29433
#define QM_REG_PQTX2PF_9_RT_OFFSET					29434
#define QM_REG_PQTX2PF_10_RT_OFFSET					29435
#define QM_REG_PQTX2PF_11_RT_OFFSET					29436
#define QM_REG_PQTX2PF_12_RT_OFFSET					29437
#define QM_REG_PQTX2PF_13_RT_OFFSET					29438
#define QM_REG_PQTX2PF_14_RT_OFFSET					29439
#define QM_REG_PQTX2PF_15_RT_OFFSET					29440
#define QM_REG_PQTX2PF_16_RT_OFFSET					29441
#define QM_REG_PQTX2PF_17_RT_OFFSET					29442
#define QM_REG_PQTX2PF_18_RT_OFFSET					29443
#define QM_REG_PQTX2PF_19_RT_OFFSET					29444
#define QM_REG_PQTX2PF_20_RT_OFFSET					29445
#define QM_REG_PQTX2PF_21_RT_OFFSET					29446
#define QM_REG_PQTX2PF_22_RT_OFFSET					29447
#define QM_REG_PQTX2PF_23_RT_OFFSET					29448
#define QM_REG_PQTX2PF_24_RT_OFFSET					29449
#define QM_REG_PQTX2PF_25_RT_OFFSET					29450
#define QM_REG_PQTX2PF_26_RT_OFFSET					29451
#define QM_REG_PQTX2PF_27_RT_OFFSET					29452
#define QM_REG_PQTX2PF_28_RT_OFFSET					29453
#define QM_REG_PQTX2PF_29_RT_OFFSET					29454
#define QM_REG_PQTX2PF_30_RT_OFFSET					29455
#define QM_REG_PQTX2PF_31_RT_OFFSET					29456
#define QM_REG_PQTX2PF_32_RT_OFFSET					29457
#define QM_REG_PQTX2PF_33_RT_OFFSET					29458
#define QM_REG_PQTX2PF_34_RT_OFFSET					29459
#define QM_REG_PQTX2PF_35_RT_OFFSET					29460
#define QM_REG_PQTX2PF_36_RT_OFFSET					29461
#define QM_REG_PQTX2PF_37_RT_OFFSET					29462
#define QM_REG_PQTX2PF_38_RT_OFFSET					29463
#define QM_REG_PQTX2PF_39_RT_OFFSET					29464
#define QM_REG_PQTX2PF_40_RT_OFFSET					29465
#define QM_REG_PQTX2PF_41_RT_OFFSET					29466
#define QM_REG_PQTX2PF_42_RT_OFFSET					29467
#define QM_REG_PQTX2PF_43_RT_OFFSET					29468
#define QM_REG_PQTX2PF_44_RT_OFFSET					29469
#define QM_REG_PQTX2PF_45_RT_OFFSET					29470
#define QM_REG_PQTX2PF_46_RT_OFFSET					29471
#define QM_REG_PQTX2PF_47_RT_OFFSET					29472
#define QM_REG_PQTX2PF_48_RT_OFFSET					29473
#define QM_REG_PQTX2PF_49_RT_OFFSET					29474
#define QM_REG_PQTX2PF_50_RT_OFFSET					29475
#define QM_REG_PQTX2PF_51_RT_OFFSET					29476
#define QM_REG_PQTX2PF_52_RT_OFFSET					29477
#define QM_REG_PQTX2PF_53_RT_OFFSET					29478
#define QM_REG_PQTX2PF_54_RT_OFFSET					29479
#define QM_REG_PQTX2PF_55_RT_OFFSET					29480
#define QM_REG_PQTX2PF_56_RT_OFFSET					29481
#define QM_REG_PQTX2PF_57_RT_OFFSET					29482
#define QM_REG_PQTX2PF_58_RT_OFFSET					29483
#define QM_REG_PQTX2PF_59_RT_OFFSET					29484
#define QM_REG_PQTX2PF_60_RT_OFFSET					29485
#define QM_REG_PQTX2PF_61_RT_OFFSET					29486
#define QM_REG_PQTX2PF_62_RT_OFFSET					29487
#define QM_REG_PQTX2PF_63_RT_OFFSET					29488
#define QM_REG_PQOTHER2PF_0_RT_OFFSET					29489
#define QM_REG_PQOTHER2PF_1_RT_OFFSET					29490
#define QM_REG_PQOTHER2PF_2_RT_OFFSET					29491
#define QM_REG_PQOTHER2PF_3_RT_OFFSET					29492
#define QM_REG_PQOTHER2PF_4_RT_OFFSET					29493
#define QM_REG_PQOTHER2PF_5_RT_OFFSET					29494
#define QM_REG_PQOTHER2PF_6_RT_OFFSET					29495
#define QM_REG_PQOTHER2PF_7_RT_OFFSET					29496
#define QM_REG_PQOTHER2PF_8_RT_OFFSET					29497
#define QM_REG_PQOTHER2PF_9_RT_OFFSET					29498
#define QM_REG_PQOTHER2PF_10_RT_OFFSET					29499
#define QM_REG_PQOTHER2PF_11_RT_OFFSET					29500
#define QM_REG_PQOTHER2PF_12_RT_OFFSET					29501
#define QM_REG_PQOTHER2PF_13_RT_OFFSET					29502
#define QM_REG_PQOTHER2PF_14_RT_OFFSET					29503
#define QM_REG_PQOTHER2PF_15_RT_OFFSET					29504
#define QM_REG_RLGLBLPERIOD_0_RT_OFFSET					29505
#define QM_REG_RLGLBLPERIOD_1_RT_OFFSET					29506
#define QM_REG_RLGLBLPERIODTIMER_0_RT_OFFSET				29507
#define QM_REG_RLGLBLPERIODTIMER_1_RT_OFFSET				29508
#define QM_REG_RLGLBLPERIODSEL_0_RT_OFFSET				29509
#define QM_REG_RLGLBLPERIODSEL_1_RT_OFFSET				29510
#define QM_REG_RLGLBLPERIODSEL_2_RT_OFFSET				29511
#define QM_REG_RLGLBLPERIODSEL_3_RT_OFFSET				29512
#define QM_REG_RLGLBLPERIODSEL_4_RT_OFFSET				29513
#define QM_REG_RLGLBLPERIODSEL_5_RT_OFFSET				29514
#define QM_REG_RLGLBLPERIODSEL_6_RT_OFFSET				29515
#define QM_REG_RLGLBLPERIODSEL_7_RT_OFFSET				29516
#define QM_REG_RLGLBLINCVAL_RT_OFFSET					29517
#define QM_REG_RLGLBLINCVAL_RT_SIZE					256
#define QM_REG_RLGLBLUPPERBOUND_RT_OFFSET				29773
#define QM_REG_RLGLBLUPPERBOUND_RT_SIZE					256
#define QM_REG_RLGLBLCRD_RT_OFFSET					30029
#define QM_REG_RLGLBLCRD_RT_SIZE					256
#define QM_REG_RLGLBLENABLE_RT_OFFSET					30285
#define QM_REG_RLPFPERIOD_RT_OFFSET					30286
#define QM_REG_RLPFPERIODTIMER_RT_OFFSET				30287
#define QM_REG_RLPFINCVAL_RT_OFFSET					30288
#define QM_REG_RLPFINCVAL_RT_SIZE					16
#define QM_REG_RLPFUPPERBOUND_RT_OFFSET					30304
#define QM_REG_RLPFUPPERBOUND_RT_SIZE					16
#define QM_REG_RLPFCRD_RT_OFFSET					30320
#define QM_REG_RLPFCRD_RT_SIZE						16
#define QM_REG_RLPFENABLE_RT_OFFSET					30336
#define QM_REG_RLPFVOQENABLE_RT_OFFSET					30337
#define QM_REG_WFQPFWEIGHT_RT_OFFSET					30338
#define QM_REG_WFQPFWEIGHT_RT_SIZE					16
#define QM_REG_WFQPFUPPERBOUND_RT_OFFSET				30354
#define QM_REG_WFQPFUPPERBOUND_RT_SIZE					16
#define QM_REG_WFQPFCRD_RT_OFFSET					30370
#define QM_REG_WFQPFCRD_RT_SIZE						160
#define QM_REG_WFQPFENABLE_RT_OFFSET					30530
#define QM_REG_WFQVPENABLE_RT_OFFSET					30531
#define QM_REG_BASEADDRTXPQ_RT_OFFSET					30532
#define QM_REG_BASEADDRTXPQ_RT_SIZE					512
#define QM_REG_TXPQMAP_RT_OFFSET					31044
#define QM_REG_TXPQMAP_RT_SIZE						512
#define QM_REG_WFQVPWEIGHT_RT_OFFSET					31556
#define QM_REG_WFQVPWEIGHT_RT_SIZE					512
#define QM_REG_WFQVPUPPERBOUND_RT_OFFSET				32068
#define QM_REG_WFQVPUPPERBOUND_RT_SIZE					512
#define QM_REG_WFQVPCRD_RT_OFFSET					32580
#define QM_REG_WFQVPCRD_RT_SIZE						512
#define QM_REG_WFQVPMAP_RT_OFFSET					33092
#define QM_REG_WFQVPMAP_RT_SIZE						512
#define QM_REG_PTRTBLTX_RT_OFFSET					33604
#define QM_REG_PTRTBLTX_RT_SIZE						1024
#define QM_REG_WFQPFCRD_MSB_RT_OFFSET					34628
#define QM_REG_WFQPFCRD_MSB_RT_SIZE					160
#define NIG_REG_TAG_ETHERTYPE_0_RT_OFFSET				34788
#define NIG_REG_BRB_GATE_DNTFWD_PORT_RT_OFFSET				34789
#define NIG_REG_OUTER_TAG_VALUE_LIST0_RT_OFFSET				34790
#define NIG_REG_OUTER_TAG_VALUE_LIST1_RT_OFFSET				34791
#define NIG_REG_OUTER_TAG_VALUE_LIST2_RT_OFFSET				34792
#define NIG_REG_OUTER_TAG_VALUE_LIST3_RT_OFFSET				34793
#define NIG_REG_LLH_FUNC_TAGMAC_CLS_TYPE_RT_OFFSET			34794
#define NIG_REG_LLH_FUNC_TAG_EN_RT_OFFSET				34795
#define NIG_REG_LLH_FUNC_TAG_EN_RT_SIZE					4
#define NIG_REG_LLH_FUNC_TAG_VALUE_RT_OFFSET				34799
#define NIG_REG_LLH_FUNC_TAG_VALUE_RT_SIZE				4
#define NIG_REG_LLH_FUNC_FILTER_VALUE_RT_OFFSET				34803
#define NIG_REG_LLH_FUNC_FILTER_VALUE_RT_SIZE				32
#define NIG_REG_LLH_FUNC_FILTER_EN_RT_OFFSET				34835
#define NIG_REG_LLH_FUNC_FILTER_EN_RT_SIZE				16
#define NIG_REG_LLH_FUNC_FILTER_MODE_RT_OFFSET				34851
#define NIG_REG_LLH_FUNC_FILTER_MODE_RT_SIZE				16
#define NIG_REG_LLH_FUNC_FILTER_PROTOCOL_TYPE_RT_OFFSET			34867
#define NIG_REG_LLH_FUNC_FILTER_PROTOCOL_TYPE_RT_SIZE			16
#define NIG_REG_LLH_FUNC_FILTER_HDR_SEL_RT_OFFSET			34883
#define NIG_REG_LLH_FUNC_FILTER_HDR_SEL_RT_SIZE				16
#define NIG_REG_TX_EDPM_CTRL_RT_OFFSET					34899
#define NIG_REG_PPF_TO_ENGINE_SEL_RT_OFFSET				34900
#define NIG_REG_PPF_TO_ENGINE_SEL_RT_SIZE				8
#define CDU_REG_CID_ADDR_PARAMS_RT_OFFSET				34908
#define CDU_REG_SEGMENT0_PARAMS_RT_OFFSET				34909
#define CDU_REG_SEGMENT1_PARAMS_RT_OFFSET				34910
#define CDU_REG_PF_SEG0_TYPE_OFFSET_RT_OFFSET				34911
#define CDU_REG_PF_SEG1_TYPE_OFFSET_RT_OFFSET				34912
#define CDU_REG_PF_SEG2_TYPE_OFFSET_RT_OFFSET				34913
#define CDU_REG_PF_SEG3_TYPE_OFFSET_RT_OFFSET				34914
#define CDU_REG_PF_FL_SEG0_TYPE_OFFSET_RT_OFFSET			34915
#define CDU_REG_PF_FL_SEG1_TYPE_OFFSET_RT_OFFSET			34916
#define CDU_REG_PF_FL_SEG2_TYPE_OFFSET_RT_OFFSET			34917
#define CDU_REG_PF_FL_SEG3_TYPE_OFFSET_RT_OFFSET			34918
#define CDU_REG_VF_SEG_TYPE_OFFSET_RT_OFFSET				34919
#define CDU_REG_VF_FL_SEG_TYPE_OFFSET_RT_OFFSET				34920
#define PBF_REG_TAG_ETHERTYPE_0_RT_OFFSET				34921
#define PBF_REG_BTB_SHARED_AREA_SIZE_RT_OFFSET				34922
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ0_RT_OFFSET			34923
#define PBF_REG_BTB_GUARANTEED_VOQ0_RT_OFFSET				34924
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ0_RT_OFFSET			34925
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ1_RT_OFFSET			34926
#define PBF_REG_BTB_GUARANTEED_VOQ1_RT_OFFSET				34927
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ1_RT_OFFSET			34928
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ2_RT_OFFSET			34929
#define PBF_REG_BTB_GUARANTEED_VOQ2_RT_OFFSET				34930
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ2_RT_OFFSET			34931
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ3_RT_OFFSET			34932
#define PBF_REG_BTB_GUARANTEED_VOQ3_RT_OFFSET				34933
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ3_RT_OFFSET			34934
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ4_RT_OFFSET			34935
#define PBF_REG_BTB_GUARANTEED_VOQ4_RT_OFFSET				34936
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ4_RT_OFFSET			34937
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ5_RT_OFFSET			34938
#define PBF_REG_BTB_GUARANTEED_VOQ5_RT_OFFSET				34939
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ5_RT_OFFSET			34940
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ6_RT_OFFSET			34941
#define PBF_REG_BTB_GUARANTEED_VOQ6_RT_OFFSET				34942
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ6_RT_OFFSET			34943
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ7_RT_OFFSET			34944
#define PBF_REG_BTB_GUARANTEED_VOQ7_RT_OFFSET				34945
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ7_RT_OFFSET			34946
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ8_RT_OFFSET			34947
#define PBF_REG_BTB_GUARANTEED_VOQ8_RT_OFFSET				34948
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ8_RT_OFFSET			34949
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ9_RT_OFFSET			34950
#define PBF_REG_BTB_GUARANTEED_VOQ9_RT_OFFSET				34951
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ9_RT_OFFSET			34952
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ10_RT_OFFSET			34953
#define PBF_REG_BTB_GUARANTEED_VOQ10_RT_OFFSET				34954
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ10_RT_OFFSET			34955
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ11_RT_OFFSET			34956
#define PBF_REG_BTB_GUARANTEED_VOQ11_RT_OFFSET				34957
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ11_RT_OFFSET			34958
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ12_RT_OFFSET			34959
#define PBF_REG_BTB_GUARANTEED_VOQ12_RT_OFFSET				34960
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ12_RT_OFFSET			34961
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ13_RT_OFFSET			34962
#define PBF_REG_BTB_GUARANTEED_VOQ13_RT_OFFSET				34963
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ13_RT_OFFSET			34964
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ14_RT_OFFSET			34965
#define PBF_REG_BTB_GUARANTEED_VOQ14_RT_OFFSET				34966
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ14_RT_OFFSET			34967
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ15_RT_OFFSET			34968
#define PBF_REG_BTB_GUARANTEED_VOQ15_RT_OFFSET				34969
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ15_RT_OFFSET			34970
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ16_RT_OFFSET			34971
#define PBF_REG_BTB_GUARANTEED_VOQ16_RT_OFFSET				34972
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ16_RT_OFFSET			34973
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ17_RT_OFFSET			34974
#define PBF_REG_BTB_GUARANTEED_VOQ17_RT_OFFSET				34975
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ17_RT_OFFSET			34976
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ18_RT_OFFSET			34977
#define PBF_REG_BTB_GUARANTEED_VOQ18_RT_OFFSET				34978
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ18_RT_OFFSET			34979
#define PBF_REG_YCMD_QS_NUM_LINES_VOQ19_RT_OFFSET			34980
#define PBF_REG_BTB_GUARANTEED_VOQ19_RT_OFFSET				34981
#define PBF_REG_BTB_SHARED_AREA_SETUP_VOQ19_RT_OFFSET			34982
#define XCM_REG_CON_PHY_Q3_RT_OFFSET					34983

#define RUNTIME_ARRAY_SIZE						34984


#define DMAE_READY_CB	0


struct tstorm_eth_conn_st_ctx {
	__le32 reserved[4];
};


struct pstorm_eth_conn_st_ctx {
	__le32 reserved[8];
};


struct xstorm_eth_conn_st_ctx {
	__le32 reserved[60];
};

struct xstorm_eth_conn_ag_ctx {
	u8 reserved0;
	u8 state;
	u8 flags0;
#define XSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM0_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM0_SHIFT	0
#define XSTORM_ETH_CONN_AG_CTX_RESERVED1_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED1_SHIFT	1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED2_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED2_SHIFT	2
#define XSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM3_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM3_SHIFT	3
#define XSTORM_ETH_CONN_AG_CTX_RESERVED3_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED3_SHIFT	4
#define XSTORM_ETH_CONN_AG_CTX_RESERVED4_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED4_SHIFT	5
#define XSTORM_ETH_CONN_AG_CTX_RESERVED5_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED5_SHIFT	6
#define XSTORM_ETH_CONN_AG_CTX_RESERVED6_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED6_SHIFT	7
		u8 flags1;
#define XSTORM_ETH_CONN_AG_CTX_RESERVED7_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED7_SHIFT	0
#define XSTORM_ETH_CONN_AG_CTX_RESERVED8_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED8_SHIFT	1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED9_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED9_SHIFT	2
#define XSTORM_ETH_CONN_AG_CTX_BIT11_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_BIT11_SHIFT		3
#define XSTORM_ETH_CONN_AG_CTX_E5_RESERVED2_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_E5_RESERVED2_SHIFT	4
#define XSTORM_ETH_CONN_AG_CTX_E5_RESERVED3_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_E5_RESERVED3_SHIFT	5
#define XSTORM_ETH_CONN_AG_CTX_TX_RULE_ACTIVE_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_TX_RULE_ACTIVE_SHIFT	6
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_ACTIVE_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_ACTIVE_SHIFT	7
	u8 flags2;
#define XSTORM_ETH_CONN_AG_CTX_CF0_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_CF0_SHIFT	0
#define XSTORM_ETH_CONN_AG_CTX_CF1_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_CF1_SHIFT	2
#define XSTORM_ETH_CONN_AG_CTX_CF2_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_CF2_SHIFT	4
#define XSTORM_ETH_CONN_AG_CTX_CF3_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_CF3_SHIFT	6
	u8 flags3;
#define XSTORM_ETH_CONN_AG_CTX_CF4_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_CF4_SHIFT	0
#define XSTORM_ETH_CONN_AG_CTX_CF5_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_CF5_SHIFT	2
#define XSTORM_ETH_CONN_AG_CTX_CF6_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_CF6_SHIFT	4
#define XSTORM_ETH_CONN_AG_CTX_CF7_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_CF7_SHIFT	6
		u8 flags4;
#define XSTORM_ETH_CONN_AG_CTX_CF8_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_CF8_SHIFT	0
#define XSTORM_ETH_CONN_AG_CTX_CF9_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_CF9_SHIFT	2
#define XSTORM_ETH_CONN_AG_CTX_CF10_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_CF10_SHIFT	4
#define XSTORM_ETH_CONN_AG_CTX_CF11_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_CF11_SHIFT	6
	u8 flags5;
#define XSTORM_ETH_CONN_AG_CTX_CF12_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_CF12_SHIFT	0
#define XSTORM_ETH_CONN_AG_CTX_CF13_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_CF13_SHIFT	2
#define XSTORM_ETH_CONN_AG_CTX_CF14_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_CF14_SHIFT	4
#define XSTORM_ETH_CONN_AG_CTX_CF15_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_CF15_SHIFT	6
	u8 flags6;
#define XSTORM_ETH_CONN_AG_CTX_GO_TO_BD_CONS_CF_MASK		0x3
#define XSTORM_ETH_CONN_AG_CTX_GO_TO_BD_CONS_CF_SHIFT	0
#define XSTORM_ETH_CONN_AG_CTX_MULTI_UNICAST_CF_MASK		0x3
#define XSTORM_ETH_CONN_AG_CTX_MULTI_UNICAST_CF_SHIFT	2
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_MASK			0x3
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_SHIFT			4
#define XSTORM_ETH_CONN_AG_CTX_TERMINATE_CF_MASK		0x3
#define XSTORM_ETH_CONN_AG_CTX_TERMINATE_CF_SHIFT		6
	u8 flags7;
#define XSTORM_ETH_CONN_AG_CTX_FLUSH_Q0_MASK		0x3
#define XSTORM_ETH_CONN_AG_CTX_FLUSH_Q0_SHIFT	0
#define XSTORM_ETH_CONN_AG_CTX_RESERVED10_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_RESERVED10_SHIFT	2
#define XSTORM_ETH_CONN_AG_CTX_SLOW_PATH_MASK	0x3
#define XSTORM_ETH_CONN_AG_CTX_SLOW_PATH_SHIFT	4
#define XSTORM_ETH_CONN_AG_CTX_CF0EN_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_CF0EN_SHIFT		6
#define XSTORM_ETH_CONN_AG_CTX_CF1EN_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_CF1EN_SHIFT		7
	u8 flags8;
#define XSTORM_ETH_CONN_AG_CTX_CF2EN_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_CF2EN_SHIFT	0
#define XSTORM_ETH_CONN_AG_CTX_CF3EN_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_CF3EN_SHIFT	1
#define XSTORM_ETH_CONN_AG_CTX_CF4EN_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_CF4EN_SHIFT	2
#define XSTORM_ETH_CONN_AG_CTX_CF5EN_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_CF5EN_SHIFT	3
#define XSTORM_ETH_CONN_AG_CTX_CF6EN_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_CF6EN_SHIFT	4
#define XSTORM_ETH_CONN_AG_CTX_CF7EN_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_CF7EN_SHIFT	5
#define XSTORM_ETH_CONN_AG_CTX_CF8EN_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_CF8EN_SHIFT	6
#define XSTORM_ETH_CONN_AG_CTX_CF9EN_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_CF9EN_SHIFT	7
	u8 flags9;
#define XSTORM_ETH_CONN_AG_CTX_CF10EN_MASK			0x1
#define XSTORM_ETH_CONN_AG_CTX_CF10EN_SHIFT			0
#define XSTORM_ETH_CONN_AG_CTX_CF11EN_MASK			0x1
#define XSTORM_ETH_CONN_AG_CTX_CF11EN_SHIFT			1
#define XSTORM_ETH_CONN_AG_CTX_CF12EN_MASK			0x1
#define XSTORM_ETH_CONN_AG_CTX_CF12EN_SHIFT			2
#define XSTORM_ETH_CONN_AG_CTX_CF13EN_MASK			0x1
#define XSTORM_ETH_CONN_AG_CTX_CF13EN_SHIFT			3
#define XSTORM_ETH_CONN_AG_CTX_CF14EN_MASK			0x1
#define XSTORM_ETH_CONN_AG_CTX_CF14EN_SHIFT			4
#define XSTORM_ETH_CONN_AG_CTX_CF15EN_MASK			0x1
#define XSTORM_ETH_CONN_AG_CTX_CF15EN_SHIFT			5
#define XSTORM_ETH_CONN_AG_CTX_GO_TO_BD_CONS_CF_EN_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_GO_TO_BD_CONS_CF_EN_SHIFT	6
#define XSTORM_ETH_CONN_AG_CTX_MULTI_UNICAST_CF_EN_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_MULTI_UNICAST_CF_EN_SHIFT	7
	u8 flags10;
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_EN_MASK			0x1
#define XSTORM_ETH_CONN_AG_CTX_DQ_CF_EN_SHIFT		0
#define XSTORM_ETH_CONN_AG_CTX_TERMINATE_CF_EN_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_TERMINATE_CF_EN_SHIFT		1
#define XSTORM_ETH_CONN_AG_CTX_FLUSH_Q0_EN_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT		2
#define XSTORM_ETH_CONN_AG_CTX_RESERVED11_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED11_SHIFT		3
#define XSTORM_ETH_CONN_AG_CTX_SLOW_PATH_EN_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_SLOW_PATH_EN_SHIFT		4
#define XSTORM_ETH_CONN_AG_CTX_TPH_ENABLE_EN_RESERVED_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_TPH_ENABLE_EN_RESERVED_SHIFT	5
#define XSTORM_ETH_CONN_AG_CTX_RESERVED12_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED12_SHIFT		6
#define XSTORM_ETH_CONN_AG_CTX_RESERVED13_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED13_SHIFT		7
	u8 flags11;
#define XSTORM_ETH_CONN_AG_CTX_RESERVED14_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED14_SHIFT	0
#define XSTORM_ETH_CONN_AG_CTX_RESERVED15_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_RESERVED15_SHIFT	1
#define XSTORM_ETH_CONN_AG_CTX_TX_DEC_RULE_EN_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_TX_DEC_RULE_EN_SHIFT	2
#define XSTORM_ETH_CONN_AG_CTX_RULE5EN_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE5EN_SHIFT		3
#define XSTORM_ETH_CONN_AG_CTX_RULE6EN_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE6EN_SHIFT		4
#define XSTORM_ETH_CONN_AG_CTX_RULE7EN_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE7EN_SHIFT		5
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED1_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED1_SHIFT	6
#define XSTORM_ETH_CONN_AG_CTX_RULE9EN_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE9EN_SHIFT		7
	u8 flags12;
#define XSTORM_ETH_CONN_AG_CTX_RULE10EN_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE10EN_SHIFT	0
#define XSTORM_ETH_CONN_AG_CTX_RULE11EN_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE11EN_SHIFT	1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED2_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED2_SHIFT	2
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED3_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED3_SHIFT	3
#define XSTORM_ETH_CONN_AG_CTX_RULE14EN_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE14EN_SHIFT	4
#define XSTORM_ETH_CONN_AG_CTX_RULE15EN_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE15EN_SHIFT	5
#define XSTORM_ETH_CONN_AG_CTX_RULE16EN_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE16EN_SHIFT	6
#define XSTORM_ETH_CONN_AG_CTX_RULE17EN_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE17EN_SHIFT	7
	u8 flags13;
#define XSTORM_ETH_CONN_AG_CTX_RULE18EN_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE18EN_SHIFT	0
#define XSTORM_ETH_CONN_AG_CTX_RULE19EN_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_RULE19EN_SHIFT	1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED4_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED4_SHIFT	2
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED5_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED5_SHIFT	3
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED6_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED6_SHIFT	4
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED7_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED7_SHIFT	5
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED8_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED8_SHIFT	6
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED9_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_A0_RESERVED9_SHIFT	7
	u8 flags14;
#define XSTORM_ETH_CONN_AG_CTX_EDPM_USE_EXT_HDR_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_EDPM_USE_EXT_HDR_SHIFT	0
#define XSTORM_ETH_CONN_AG_CTX_EDPM_SEND_RAW_L3L4_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_EDPM_SEND_RAW_L3L4_SHIFT	1
#define XSTORM_ETH_CONN_AG_CTX_EDPM_INBAND_PROP_HDR_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_EDPM_INBAND_PROP_HDR_SHIFT	2
#define XSTORM_ETH_CONN_AG_CTX_EDPM_SEND_EXT_TUNNEL_MASK	0x1
#define XSTORM_ETH_CONN_AG_CTX_EDPM_SEND_EXT_TUNNEL_SHIFT	3
#define XSTORM_ETH_CONN_AG_CTX_L2_EDPM_ENABLE_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_L2_EDPM_ENABLE_SHIFT		4
#define XSTORM_ETH_CONN_AG_CTX_ROCE_EDPM_ENABLE_MASK		0x1
#define XSTORM_ETH_CONN_AG_CTX_ROCE_EDPM_ENABLE_SHIFT	5
#define XSTORM_ETH_CONN_AG_CTX_TPH_ENABLE_MASK		0x3
#define XSTORM_ETH_CONN_AG_CTX_TPH_ENABLE_SHIFT		6
	u8 edpm_event_id;
	__le16 physical_q0;
	__le16 e5_reserved1;
	__le16 edpm_num_bds;
	__le16 tx_bd_cons;
	__le16 tx_bd_prod;
	__le16 updated_qm_pq_id;
	__le16 conn_dpi;
	u8 byte3;
	u8 byte4;
	u8 byte5;
	u8 byte6;
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 reg3;
	__le32 reg4;
	__le32 reg5;
	__le32 reg6;
	__le16 word7;
	__le16 word8;
	__le16 word9;
	__le16 word10;
	__le32 reg7;
	__le32 reg8;
	__le32 reg9;
	u8 byte7;
	u8 byte8;
	u8 byte9;
	u8 byte10;
	u8 byte11;
	u8 byte12;
	u8 byte13;
	u8 byte14;
	u8 byte15;
	u8 e5_reserved;
	__le16 word11;
	__le32 reg10;
	__le32 reg11;
	__le32 reg12;
	__le32 reg13;
	__le32 reg14;
	__le32 reg15;
	__le32 reg16;
	__le32 reg17;
	__le32 reg18;
	__le32 reg19;
	__le16 word12;
	__le16 word13;
	__le16 word14;
	__le16 word15;
};


struct ystorm_eth_conn_st_ctx {
	__le32 reserved[8];
};

struct ystorm_eth_conn_ag_ctx {
	u8 byte0;
	u8 state;
	u8 flags0;
#define YSTORM_ETH_CONN_AG_CTX_BIT0_MASK			0x1
#define YSTORM_ETH_CONN_AG_CTX_BIT0_SHIFT			0
#define YSTORM_ETH_CONN_AG_CTX_BIT1_MASK			0x1
#define YSTORM_ETH_CONN_AG_CTX_BIT1_SHIFT			1
#define YSTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_MASK	0x3
#define YSTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_SHIFT	2
#define YSTORM_ETH_CONN_AG_CTX_PMD_TERMINATE_CF_MASK		0x3
#define YSTORM_ETH_CONN_AG_CTX_PMD_TERMINATE_CF_SHIFT	4
#define YSTORM_ETH_CONN_AG_CTX_CF2_MASK			0x3
#define YSTORM_ETH_CONN_AG_CTX_CF2_SHIFT			6
	u8 flags1;
#define YSTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_EN_MASK	0x1
#define YSTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_EN_SHIFT	0
#define YSTORM_ETH_CONN_AG_CTX_PMD_TERMINATE_CF_EN_MASK	0x1
#define YSTORM_ETH_CONN_AG_CTX_PMD_TERMINATE_CF_EN_SHIFT	1
#define YSTORM_ETH_CONN_AG_CTX_CF2EN_MASK			0x1
#define YSTORM_ETH_CONN_AG_CTX_CF2EN_SHIFT			2
#define YSTORM_ETH_CONN_AG_CTX_RULE0EN_MASK			0x1
#define YSTORM_ETH_CONN_AG_CTX_RULE0EN_SHIFT			3
#define YSTORM_ETH_CONN_AG_CTX_RULE1EN_MASK			0x1
#define YSTORM_ETH_CONN_AG_CTX_RULE1EN_SHIFT			4
#define YSTORM_ETH_CONN_AG_CTX_RULE2EN_MASK			0x1
#define YSTORM_ETH_CONN_AG_CTX_RULE2EN_SHIFT			5
#define YSTORM_ETH_CONN_AG_CTX_RULE3EN_MASK			0x1
#define YSTORM_ETH_CONN_AG_CTX_RULE3EN_SHIFT			6
#define YSTORM_ETH_CONN_AG_CTX_RULE4EN_MASK			0x1
#define YSTORM_ETH_CONN_AG_CTX_RULE4EN_SHIFT			7
	u8 tx_q0_int_coallecing_timeset;
	u8 byte3;
	__le16 word0;
	__le32 terminate_spqe;
	__le32 reg1;
	__le16 tx_bd_cons_upd;
	__le16 word2;
	__le16 word3;
	__le16 word4;
	__le32 reg2;
	__le32 reg3;
};

struct tstorm_eth_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define TSTORM_ETH_CONN_AG_CTX_BIT0_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_BIT0_SHIFT	0
#define TSTORM_ETH_CONN_AG_CTX_BIT1_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_BIT1_SHIFT	1
#define TSTORM_ETH_CONN_AG_CTX_BIT2_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_BIT2_SHIFT	2
#define TSTORM_ETH_CONN_AG_CTX_BIT3_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_BIT3_SHIFT	3
#define TSTORM_ETH_CONN_AG_CTX_BIT4_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_BIT4_SHIFT	4
#define TSTORM_ETH_CONN_AG_CTX_BIT5_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_BIT5_SHIFT	5
#define TSTORM_ETH_CONN_AG_CTX_CF0_MASK	0x3
#define TSTORM_ETH_CONN_AG_CTX_CF0_SHIFT	6
	u8 flags1;
#define TSTORM_ETH_CONN_AG_CTX_CF1_MASK	0x3
#define TSTORM_ETH_CONN_AG_CTX_CF1_SHIFT	0
#define TSTORM_ETH_CONN_AG_CTX_CF2_MASK	0x3
#define TSTORM_ETH_CONN_AG_CTX_CF2_SHIFT	2
#define TSTORM_ETH_CONN_AG_CTX_CF3_MASK	0x3
#define TSTORM_ETH_CONN_AG_CTX_CF3_SHIFT	4
#define TSTORM_ETH_CONN_AG_CTX_CF4_MASK	0x3
#define TSTORM_ETH_CONN_AG_CTX_CF4_SHIFT	6
	u8 flags2;
#define TSTORM_ETH_CONN_AG_CTX_CF5_MASK	0x3
#define TSTORM_ETH_CONN_AG_CTX_CF5_SHIFT	0
#define TSTORM_ETH_CONN_AG_CTX_CF6_MASK	0x3
#define TSTORM_ETH_CONN_AG_CTX_CF6_SHIFT	2
#define TSTORM_ETH_CONN_AG_CTX_CF7_MASK	0x3
#define TSTORM_ETH_CONN_AG_CTX_CF7_SHIFT	4
#define TSTORM_ETH_CONN_AG_CTX_CF8_MASK	0x3
#define TSTORM_ETH_CONN_AG_CTX_CF8_SHIFT	6
	u8 flags3;
#define TSTORM_ETH_CONN_AG_CTX_CF9_MASK	0x3
#define TSTORM_ETH_CONN_AG_CTX_CF9_SHIFT	0
#define TSTORM_ETH_CONN_AG_CTX_CF10_MASK	0x3
#define TSTORM_ETH_CONN_AG_CTX_CF10_SHIFT	2
#define TSTORM_ETH_CONN_AG_CTX_CF0EN_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_CF0EN_SHIFT	4
#define TSTORM_ETH_CONN_AG_CTX_CF1EN_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_CF1EN_SHIFT	5
#define TSTORM_ETH_CONN_AG_CTX_CF2EN_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_CF2EN_SHIFT	6
#define TSTORM_ETH_CONN_AG_CTX_CF3EN_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_CF3EN_SHIFT	7
	u8 flags4;
#define TSTORM_ETH_CONN_AG_CTX_CF4EN_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_CF4EN_SHIFT	0
#define TSTORM_ETH_CONN_AG_CTX_CF5EN_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_CF5EN_SHIFT	1
#define TSTORM_ETH_CONN_AG_CTX_CF6EN_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_CF6EN_SHIFT	2
#define TSTORM_ETH_CONN_AG_CTX_CF7EN_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_CF7EN_SHIFT	3
#define TSTORM_ETH_CONN_AG_CTX_CF8EN_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_CF8EN_SHIFT	4
#define TSTORM_ETH_CONN_AG_CTX_CF9EN_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_CF9EN_SHIFT	5
#define TSTORM_ETH_CONN_AG_CTX_CF10EN_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_CF10EN_SHIFT	6
#define TSTORM_ETH_CONN_AG_CTX_RULE0EN_MASK	0x1
#define TSTORM_ETH_CONN_AG_CTX_RULE0EN_SHIFT	7
	u8 flags5;
#define TSTORM_ETH_CONN_AG_CTX_RULE1EN_MASK		0x1
#define TSTORM_ETH_CONN_AG_CTX_RULE1EN_SHIFT		0
#define TSTORM_ETH_CONN_AG_CTX_RULE2EN_MASK		0x1
#define TSTORM_ETH_CONN_AG_CTX_RULE2EN_SHIFT		1
#define TSTORM_ETH_CONN_AG_CTX_RULE3EN_MASK		0x1
#define TSTORM_ETH_CONN_AG_CTX_RULE3EN_SHIFT		2
#define TSTORM_ETH_CONN_AG_CTX_RULE4EN_MASK		0x1
#define TSTORM_ETH_CONN_AG_CTX_RULE4EN_SHIFT		3
#define TSTORM_ETH_CONN_AG_CTX_RULE5EN_MASK		0x1
#define TSTORM_ETH_CONN_AG_CTX_RULE5EN_SHIFT		4
#define TSTORM_ETH_CONN_AG_CTX_RX_BD_EN_MASK		0x1
#define TSTORM_ETH_CONN_AG_CTX_RX_BD_EN_SHIFT	5
#define TSTORM_ETH_CONN_AG_CTX_RULE7EN_MASK		0x1
#define TSTORM_ETH_CONN_AG_CTX_RULE7EN_SHIFT		6
#define TSTORM_ETH_CONN_AG_CTX_RULE8EN_MASK		0x1
#define TSTORM_ETH_CONN_AG_CTX_RULE8EN_SHIFT		7
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 reg3;
	__le32 reg4;
	__le32 reg5;
	__le32 reg6;
	__le32 reg7;
	__le32 reg8;
	u8 byte2;
	u8 byte3;
	__le16 rx_bd_cons;
	u8 byte4;
	u8 byte5;
	__le16 rx_bd_prod;
	__le16 word2;
	__le16 word3;
	__le32 reg9;
	__le32 reg10;
};

struct ustorm_eth_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define USTORM_ETH_CONN_AG_CTX_BIT0_MASK			0x1
#define USTORM_ETH_CONN_AG_CTX_BIT0_SHIFT			0
#define USTORM_ETH_CONN_AG_CTX_BIT1_MASK			0x1
#define USTORM_ETH_CONN_AG_CTX_BIT1_SHIFT			1
#define USTORM_ETH_CONN_AG_CTX_TX_PMD_TERMINATE_CF_MASK	0x3
#define USTORM_ETH_CONN_AG_CTX_TX_PMD_TERMINATE_CF_SHIFT	2
#define USTORM_ETH_CONN_AG_CTX_RX_PMD_TERMINATE_CF_MASK	0x3
#define USTORM_ETH_CONN_AG_CTX_RX_PMD_TERMINATE_CF_SHIFT	4
#define USTORM_ETH_CONN_AG_CTX_CF2_MASK			0x3
#define USTORM_ETH_CONN_AG_CTX_CF2_SHIFT			6
	u8 flags1;
#define USTORM_ETH_CONN_AG_CTX_CF3_MASK			0x3
#define USTORM_ETH_CONN_AG_CTX_CF3_SHIFT			0
#define USTORM_ETH_CONN_AG_CTX_TX_ARM_CF_MASK		0x3
#define USTORM_ETH_CONN_AG_CTX_TX_ARM_CF_SHIFT		2
#define USTORM_ETH_CONN_AG_CTX_RX_ARM_CF_MASK		0x3
#define USTORM_ETH_CONN_AG_CTX_RX_ARM_CF_SHIFT		4
#define USTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_MASK	0x3
#define USTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_SHIFT	6
	u8 flags2;
#define USTORM_ETH_CONN_AG_CTX_TX_PMD_TERMINATE_CF_EN_MASK	0x1
#define USTORM_ETH_CONN_AG_CTX_TX_PMD_TERMINATE_CF_EN_SHIFT	0
#define USTORM_ETH_CONN_AG_CTX_RX_PMD_TERMINATE_CF_EN_MASK	0x1
#define USTORM_ETH_CONN_AG_CTX_RX_PMD_TERMINATE_CF_EN_SHIFT	1
#define USTORM_ETH_CONN_AG_CTX_CF2EN_MASK			0x1
#define USTORM_ETH_CONN_AG_CTX_CF2EN_SHIFT			2
#define USTORM_ETH_CONN_AG_CTX_CF3EN_MASK			0x1
#define USTORM_ETH_CONN_AG_CTX_CF3EN_SHIFT			3
#define USTORM_ETH_CONN_AG_CTX_TX_ARM_CF_EN_MASK		0x1
#define USTORM_ETH_CONN_AG_CTX_TX_ARM_CF_EN_SHIFT		4
#define USTORM_ETH_CONN_AG_CTX_RX_ARM_CF_EN_MASK		0x1
#define USTORM_ETH_CONN_AG_CTX_RX_ARM_CF_EN_SHIFT		5
#define USTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_EN_MASK	0x1
#define USTORM_ETH_CONN_AG_CTX_TX_BD_CONS_UPD_CF_EN_SHIFT	6
#define USTORM_ETH_CONN_AG_CTX_RULE0EN_MASK			0x1
#define USTORM_ETH_CONN_AG_CTX_RULE0EN_SHIFT			7
	u8 flags3;
#define USTORM_ETH_CONN_AG_CTX_RULE1EN_MASK	0x1
#define USTORM_ETH_CONN_AG_CTX_RULE1EN_SHIFT	0
#define USTORM_ETH_CONN_AG_CTX_RULE2EN_MASK	0x1
#define USTORM_ETH_CONN_AG_CTX_RULE2EN_SHIFT	1
#define USTORM_ETH_CONN_AG_CTX_RULE3EN_MASK	0x1
#define USTORM_ETH_CONN_AG_CTX_RULE3EN_SHIFT	2
#define USTORM_ETH_CONN_AG_CTX_RULE4EN_MASK	0x1
#define USTORM_ETH_CONN_AG_CTX_RULE4EN_SHIFT	3
#define USTORM_ETH_CONN_AG_CTX_RULE5EN_MASK	0x1
#define USTORM_ETH_CONN_AG_CTX_RULE5EN_SHIFT	4
#define USTORM_ETH_CONN_AG_CTX_RULE6EN_MASK	0x1
#define USTORM_ETH_CONN_AG_CTX_RULE6EN_SHIFT	5
#define USTORM_ETH_CONN_AG_CTX_RULE7EN_MASK	0x1
#define USTORM_ETH_CONN_AG_CTX_RULE7EN_SHIFT	6
#define USTORM_ETH_CONN_AG_CTX_RULE8EN_MASK	0x1
#define USTORM_ETH_CONN_AG_CTX_RULE8EN_SHIFT	7
	u8 byte2;
	u8 byte3;
	__le16 word0;
	__le16 tx_bd_cons;
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 tx_int_coallecing_timeset;
	__le16 tx_drv_bd_cons;
	__le16 rx_drv_cqe_cons;
};


struct ustorm_eth_conn_st_ctx {
	__le32 reserved[40];
};


struct mstorm_eth_conn_st_ctx {
	__le32 reserved[8];
};


struct eth_conn_context {
	struct tstorm_eth_conn_st_ctx tstorm_st_context;
	struct regpair tstorm_st_padding[2];
	struct pstorm_eth_conn_st_ctx pstorm_st_context;
	struct xstorm_eth_conn_st_ctx xstorm_st_context;
	struct xstorm_eth_conn_ag_ctx xstorm_ag_context;
	struct tstorm_eth_conn_ag_ctx tstorm_ag_context;
	struct ystorm_eth_conn_st_ctx ystorm_st_context;
	struct ystorm_eth_conn_ag_ctx ystorm_ag_context;
	struct ustorm_eth_conn_ag_ctx ustorm_ag_context;
	struct ustorm_eth_conn_st_ctx ustorm_st_context;
	struct mstorm_eth_conn_st_ctx mstorm_st_context;
};


enum eth_error_code {
	ETH_OK = 0x00,
	ETH_FILTERS_MAC_ADD_FAIL_FULL,
	ETH_FILTERS_MAC_ADD_FAIL_FULL_MTT2,
	ETH_FILTERS_MAC_ADD_FAIL_DUP_MTT2,
	ETH_FILTERS_MAC_ADD_FAIL_DUP_STT2,
	ETH_FILTERS_MAC_DEL_FAIL_NOF,
	ETH_FILTERS_MAC_DEL_FAIL_NOF_MTT2,
	ETH_FILTERS_MAC_DEL_FAIL_NOF_STT2,
	ETH_FILTERS_MAC_ADD_FAIL_ZERO_MAC,
	ETH_FILTERS_VLAN_ADD_FAIL_FULL,
	ETH_FILTERS_VLAN_ADD_FAIL_DUP,
	ETH_FILTERS_VLAN_DEL_FAIL_NOF,
	ETH_FILTERS_VLAN_DEL_FAIL_NOF_TT1,
	ETH_FILTERS_PAIR_ADD_FAIL_DUP,
	ETH_FILTERS_PAIR_ADD_FAIL_FULL,
	ETH_FILTERS_PAIR_ADD_FAIL_FULL_MAC,
	ETH_FILTERS_PAIR_DEL_FAIL_NOF,
	ETH_FILTERS_PAIR_DEL_FAIL_NOF_TT1,
	ETH_FILTERS_PAIR_ADD_FAIL_ZERO_MAC,
	ETH_FILTERS_VNI_ADD_FAIL_FULL,
	ETH_FILTERS_VNI_ADD_FAIL_DUP,
	ETH_FILTERS_GFT_UPDATE_FAIL,
	ETH_RX_QUEUE_FAIL_LOAD_VF_DATA,
	ETH_FILTERS_GFS_ADD_FILTER_FAIL_MAX_HOPS,
	ETH_FILTERS_GFS_ADD_FILTER_FAIL_NO_FREE_ENRTY,
	ETH_FILTERS_GFS_ADD_FILTER_FAIL_ALREADY_EXISTS,
	ETH_FILTERS_GFS_ADD_FILTER_FAIL_PCI_ERROR,
	ETH_FILTERS_GFS_ADD_FINLER_FAIL_MAGIC_NUM_ERROR,
	ETH_FILTERS_GFS_DEL_FILTER_FAIL_MAX_HOPS,
	ETH_FILTERS_GFS_DEL_FILTER_FAIL_NO_MATCH_ENRTY,
	ETH_FILTERS_GFS_DEL_FILTER_FAIL_PCI_ERROR,
	ETH_FILTERS_GFS_DEL_FILTER_FAIL_MAGIC_NUM_ERROR,
	MAX_ETH_ERROR_CODE
};


enum eth_event_opcode {
	ETH_EVENT_UNUSED,
	ETH_EVENT_VPORT_START,
	ETH_EVENT_VPORT_UPDATE,
	ETH_EVENT_VPORT_STOP,
	ETH_EVENT_TX_QUEUE_START,
	ETH_EVENT_TX_QUEUE_STOP,
	ETH_EVENT_RX_QUEUE_START,
	ETH_EVENT_RX_QUEUE_UPDATE,
	ETH_EVENT_RX_QUEUE_STOP,
	ETH_EVENT_FILTERS_UPDATE,
	ETH_EVENT_RX_ADD_OPENFLOW_FILTER,
	ETH_EVENT_RX_DELETE_OPENFLOW_FILTER,
	ETH_EVENT_RX_CREATE_OPENFLOW_ACTION,
	ETH_EVENT_RX_ADD_UDP_FILTER,
	ETH_EVENT_RX_DELETE_UDP_FILTER,
	ETH_EVENT_RX_CREATE_GFT_ACTION,
	ETH_EVENT_RX_GFT_UPDATE_FILTER,
	ETH_EVENT_TX_QUEUE_UPDATE,
	ETH_EVENT_RGFS_ADD_FILTER,
	ETH_EVENT_RGFS_DEL_FILTER,
	ETH_EVENT_TGFS_ADD_FILTER,
	ETH_EVENT_TGFS_DEL_FILTER,
	ETH_EVENT_GFS_COUNTERS_REPORT_REQUEST,
	MAX_ETH_EVENT_OPCODE
};


enum eth_filter_action {
	ETH_FILTER_ACTION_UNUSED,
	ETH_FILTER_ACTION_REMOVE,
	ETH_FILTER_ACTION_ADD,
	ETH_FILTER_ACTION_REMOVE_ALL,
	MAX_ETH_FILTER_ACTION
};


struct eth_filter_cmd {
	u8 type;
	u8 vport_id;
	u8 action;
	u8 reserved0;
	__le32 vni;
	__le16 mac_lsb;
	__le16 mac_mid;
	__le16 mac_msb;
	__le16 vlan_id;
};


struct eth_filter_cmd_header {
	u8 rx;
	u8 tx;
	u8 cmd_cnt;
	u8 assert_on_error;
	u8 reserved1[4];
};


enum eth_filter_type {
	ETH_FILTER_TYPE_UNUSED,
	ETH_FILTER_TYPE_MAC,
	ETH_FILTER_TYPE_VLAN,
	ETH_FILTER_TYPE_PAIR,
	ETH_FILTER_TYPE_INNER_MAC,
	ETH_FILTER_TYPE_INNER_VLAN,
	ETH_FILTER_TYPE_INNER_PAIR,
	ETH_FILTER_TYPE_INNER_MAC_VNI_PAIR,
	ETH_FILTER_TYPE_MAC_VNI_PAIR,
	ETH_FILTER_TYPE_VNI,
	MAX_ETH_FILTER_TYPE
};


struct eth_in_to_in_pri_map_cfg {
	u8 inner_vlan_pri_remap_en;
	u8 reserved[7];
	u8 non_rdma_in_to_in_pri_map[8];
	u8 rdma_in_to_in_pri_map[8];
};


enum eth_ipv4_frag_type {
	ETH_IPV4_NOT_FRAG,
	ETH_IPV4_FIRST_FRAG,
	ETH_IPV4_NON_FIRST_FRAG,
	MAX_ETH_IPV4_FRAG_TYPE
};


enum eth_ip_type {
	ETH_IPV4,
	ETH_IPV6,
	MAX_ETH_IP_TYPE
};


enum eth_ramrod_cmd_id {
	ETH_RAMROD_UNUSED,
	ETH_RAMROD_VPORT_START,
	ETH_RAMROD_VPORT_UPDATE,
	ETH_RAMROD_VPORT_STOP,
	ETH_RAMROD_RX_QUEUE_START,
	ETH_RAMROD_RX_QUEUE_STOP,
	ETH_RAMROD_TX_QUEUE_START,
	ETH_RAMROD_TX_QUEUE_STOP,
	ETH_RAMROD_FILTERS_UPDATE,
	ETH_RAMROD_RX_QUEUE_UPDATE,
	ETH_RAMROD_RX_CREATE_OPENFLOW_ACTION,
	ETH_RAMROD_RX_ADD_OPENFLOW_FILTER,
	ETH_RAMROD_RX_DELETE_OPENFLOW_FILTER,
	ETH_RAMROD_RX_ADD_UDP_FILTER,
	ETH_RAMROD_RX_DELETE_UDP_FILTER,
	ETH_RAMROD_RX_CREATE_GFT_ACTION,
	ETH_RAMROD_RX_UPDATE_GFT_FILTER,
	ETH_RAMROD_TX_QUEUE_UPDATE,
	ETH_RAMROD_RGFS_FILTER_ADD,
	ETH_RAMROD_RGFS_FILTER_DEL,
	ETH_RAMROD_TGFS_FILTER_ADD,
	ETH_RAMROD_TGFS_FILTER_DEL,
	ETH_RAMROD_GFS_COUNTERS_REPORT_REQUEST,
	MAX_ETH_RAMROD_CMD_ID
};


struct eth_return_code {
	u8 value;
#define ETH_RETURN_CODE_ERR_CODE_MASK  0x3F
#define ETH_RETURN_CODE_ERR_CODE_SHIFT 0
#define ETH_RETURN_CODE_RESERVED_MASK  0x1
#define ETH_RETURN_CODE_RESERVED_SHIFT 6
#define ETH_RETURN_CODE_RX_TX_MASK     0x1
#define ETH_RETURN_CODE_RX_TX_SHIFT    7
};


enum eth_tx_dst_mode_config_enum {
	ETH_TX_DST_MODE_CONFIG_DISABLE,
	ETH_TX_DST_MODE_CONFIG_FORWARD_DATA_IN_BD,
	ETH_TX_DST_MODE_CONFIG_FORWARD_DATA_IN_VPORT,
	MAX_ETH_TX_DST_MODE_CONFIG_ENUM
};


enum eth_tx_err {
	ETH_TX_ERR_DROP,
	ETH_TX_ERR_ASSERT_MALICIOUS,
	MAX_ETH_TX_ERR
};


struct eth_tx_err_vals {
	__le16 values;
#define ETH_TX_ERR_VALS_ILLEGAL_VLAN_MODE_MASK			0x1
#define ETH_TX_ERR_VALS_ILLEGAL_VLAN_MODE_SHIFT			0
#define ETH_TX_ERR_VALS_PACKET_TOO_SMALL_MASK			0x1
#define ETH_TX_ERR_VALS_PACKET_TOO_SMALL_SHIFT			1
#define ETH_TX_ERR_VALS_ANTI_SPOOFING_ERR_MASK			0x1
#define ETH_TX_ERR_VALS_ANTI_SPOOFING_ERR_SHIFT			2
#define ETH_TX_ERR_VALS_ILLEGAL_INBAND_TAGS_MASK		0x1
#define ETH_TX_ERR_VALS_ILLEGAL_INBAND_TAGS_SHIFT		3
#define ETH_TX_ERR_VALS_VLAN_INSERTION_W_INBAND_TAG_MASK	0x1
#define ETH_TX_ERR_VALS_VLAN_INSERTION_W_INBAND_TAG_SHIFT	4
#define ETH_TX_ERR_VALS_MTU_VIOLATION_MASK			0x1
#define ETH_TX_ERR_VALS_MTU_VIOLATION_SHIFT			5
#define ETH_TX_ERR_VALS_ILLEGAL_CONTROL_FRAME_MASK		0x1
#define ETH_TX_ERR_VALS_ILLEGAL_CONTROL_FRAME_SHIFT		6
#define ETH_TX_ERR_VALS_ILLEGAL_BD_FLAGS_MASK			0x1
#define ETH_TX_ERR_VALS_ILLEGAL_BD_FLAGS_SHIFT			7
#define ETH_TX_ERR_VALS_RESERVED_MASK				0xFF
#define ETH_TX_ERR_VALS_RESERVED_SHIFT				8
};


struct eth_vport_rss_config {
	__le16 capabilities;
#define ETH_VPORT_RSS_CONFIG_IPV4_CAPABILITY_MASK		0x1
#define ETH_VPORT_RSS_CONFIG_IPV4_CAPABILITY_SHIFT		0
#define ETH_VPORT_RSS_CONFIG_IPV6_CAPABILITY_MASK		0x1
#define ETH_VPORT_RSS_CONFIG_IPV6_CAPABILITY_SHIFT		1
#define ETH_VPORT_RSS_CONFIG_IPV4_TCP_CAPABILITY_MASK		0x1
#define ETH_VPORT_RSS_CONFIG_IPV4_TCP_CAPABILITY_SHIFT		2
#define ETH_VPORT_RSS_CONFIG_IPV6_TCP_CAPABILITY_MASK		0x1
#define ETH_VPORT_RSS_CONFIG_IPV6_TCP_CAPABILITY_SHIFT		3
#define ETH_VPORT_RSS_CONFIG_IPV4_UDP_CAPABILITY_MASK		0x1
#define ETH_VPORT_RSS_CONFIG_IPV4_UDP_CAPABILITY_SHIFT		4
#define ETH_VPORT_RSS_CONFIG_IPV6_UDP_CAPABILITY_MASK		0x1
#define ETH_VPORT_RSS_CONFIG_IPV6_UDP_CAPABILITY_SHIFT		5
#define ETH_VPORT_RSS_CONFIG_EN_5_TUPLE_CAPABILITY_MASK		0x1
#define ETH_VPORT_RSS_CONFIG_EN_5_TUPLE_CAPABILITY_SHIFT	6
#define ETH_VPORT_RSS_CONFIG_RESERVED0_MASK			0x1FF
#define ETH_VPORT_RSS_CONFIG_RESERVED0_SHIFT			7
	u8 rss_id;
	u8 rss_mode;
	u8 update_rss_key;
	u8 update_rss_ind_table;
	u8 update_rss_capabilities;
	u8 tbl_size;
	u8 ind_table_mask_valid;
	u8 reserved2[3];
	__le16 indirection_table[ETH_RSS_IND_TABLE_ENTRIES_NUM];
	__le32 ind_table_mask[ETH_RSS_IND_TABLE_MASK_SIZE_REGS];
	__le32 rss_key[ETH_RSS_KEY_SIZE_REGS];
	__le32 reserved3;
};


enum eth_vport_rss_mode {
	ETH_VPORT_RSS_MODE_DISABLED,
	ETH_VPORT_RSS_MODE_REGULAR,
	MAX_ETH_VPORT_RSS_MODE
};


struct eth_vport_rx_mode {
	__le16 state;
#define ETH_VPORT_RX_MODE_UCAST_DROP_ALL_MASK		0x1
#define ETH_VPORT_RX_MODE_UCAST_DROP_ALL_SHIFT		0
#define ETH_VPORT_RX_MODE_UCAST_ACCEPT_ALL_MASK		0x1
#define ETH_VPORT_RX_MODE_UCAST_ACCEPT_ALL_SHIFT	1
#define ETH_VPORT_RX_MODE_UCAST_ACCEPT_UNMATCHED_MASK	0x1
#define ETH_VPORT_RX_MODE_UCAST_ACCEPT_UNMATCHED_SHIFT	2
#define ETH_VPORT_RX_MODE_MCAST_DROP_ALL_MASK		0x1
#define ETH_VPORT_RX_MODE_MCAST_DROP_ALL_SHIFT		3
#define ETH_VPORT_RX_MODE_MCAST_ACCEPT_ALL_MASK		0x1
#define ETH_VPORT_RX_MODE_MCAST_ACCEPT_ALL_SHIFT	4
#define ETH_VPORT_RX_MODE_BCAST_ACCEPT_ALL_MASK		0x1
#define ETH_VPORT_RX_MODE_BCAST_ACCEPT_ALL_SHIFT	5
#define ETH_VPORT_RX_MODE_ACCEPT_ANY_VNI_MASK		0x1
#define ETH_VPORT_RX_MODE_ACCEPT_ANY_VNI_SHIFT		6
#define ETH_VPORT_RX_MODE_RESERVED1_MASK		0x1FF
#define ETH_VPORT_RX_MODE_RESERVED1_SHIFT		7
};


struct eth_vport_tpa_param {
	u8 tpa_ipv4_en_flg;
	u8 tpa_ipv6_en_flg;
	u8 tpa_ipv4_tunn_en_flg;
	u8 tpa_ipv6_tunn_en_flg;
	u8 tpa_pkt_split_flg;
	u8 tpa_hdr_data_split_flg;
	u8 tpa_gro_consistent_flg;

	u8 tpa_max_aggs_num;

	__le16 tpa_max_size;
	__le16 tpa_min_size_to_start;

	__le16 tpa_min_size_to_cont;
	u8 max_buff_num;
	u8 reserved;
};


struct eth_vport_tx_mode {
	__le16 state;
#define ETH_VPORT_TX_MODE_UCAST_DROP_ALL_MASK		0x1
#define ETH_VPORT_TX_MODE_UCAST_DROP_ALL_SHIFT		0
#define ETH_VPORT_TX_MODE_UCAST_ACCEPT_ALL_MASK		0x1
#define ETH_VPORT_TX_MODE_UCAST_ACCEPT_ALL_SHIFT	1
#define ETH_VPORT_TX_MODE_MCAST_DROP_ALL_MASK		0x1
#define ETH_VPORT_TX_MODE_MCAST_DROP_ALL_SHIFT		2
#define ETH_VPORT_TX_MODE_MCAST_ACCEPT_ALL_MASK		0x1
#define ETH_VPORT_TX_MODE_MCAST_ACCEPT_ALL_SHIFT	3
#define ETH_VPORT_TX_MODE_BCAST_ACCEPT_ALL_MASK		0x1
#define ETH_VPORT_TX_MODE_BCAST_ACCEPT_ALL_SHIFT	4
#define ETH_VPORT_TX_MODE_RESERVED1_MASK		0x7FF
#define ETH_VPORT_TX_MODE_RESERVED1_SHIFT		5
};


enum gft_filter_update_action {
	GFT_ADD_FILTER,
	GFT_DELETE_FILTER,
	MAX_GFT_FILTER_UPDATE_ACTION
};


struct rx_create_gft_action_ramrod_data {
	u8 vport_id;
	u8 reserved[7];
};


struct rx_create_openflow_action_ramrod_data {
	u8 vport_id;
	u8 reserved[7];
};


struct rx_openflow_filter_ramrod_data {
	__le16 action_icid;
	u8 priority;
	u8 reserved0;
	__le32 tenant_id;
	__le16 dst_mac_hi;
	__le16 dst_mac_mid;
	__le16 dst_mac_lo;
	__le16 src_mac_hi;
	__le16 src_mac_mid;
	__le16 src_mac_lo;
	__le16 vlan_id;
	__le16 l2_eth_type;
	u8 ipv4_dscp;
	u8 ipv4_frag_type;
	u8 ipv4_over_ip;
	u8 tenant_id_exists;
	__le32 ipv4_dst_addr;
	__le32 ipv4_src_addr;
	__le16 l4_dst_port;
	__le16 l4_src_port;
};


struct rx_queue_start_ramrod_data {
	__le16 rx_queue_id;
	__le16 num_of_pbl_pages;
	__le16 bd_max_bytes;
	__le16 sb_id;
	u8 sb_index;
	u8 vport_id;
	u8 default_rss_queue_flg;
	u8 complete_cqe_flg;
	u8 complete_event_flg;
	u8 stats_counter_id;
	u8 pin_context;
	u8 pxp_tph_valid_bd;
	u8 pxp_tph_valid_pkt;
	u8 pxp_st_hint;

	__le16 pxp_st_index;
	u8 pmd_mode;

	u8 notify_en;
	u8 toggle_val;

	u8 vf_rx_prod_index;
	u8 vf_rx_prod_use_zone_a;
	u8 reserved[5];
	__le16 reserved1;
	struct regpair cqe_pbl_addr;
	struct regpair bd_base;
	struct regpair reserved2;
};


struct rx_queue_stop_ramrod_data {
	__le16 rx_queue_id;
	u8 complete_cqe_flg;
	u8 complete_event_flg;
	u8 vport_id;
	u8 reserved[3];
};


struct rx_queue_update_ramrod_data {
	__le16 rx_queue_id;
	u8 complete_cqe_flg;
	u8 complete_event_flg;
	u8 vport_id;
	u8 set_default_rss_queue;
	u8 reserved[3];
	u8 reserved1;
	u8 reserved2;
	u8 reserved3;
	__le16 reserved4;
	__le16 reserved5;
	struct regpair reserved6;
};


struct rx_udp_filter_ramrod_data {
	__le16 action_icid;
	__le16 vlan_id;
	u8 ip_type;
	u8 tenant_id_exists;
	__le16 reserved1;
	__le32 ip_dst_addr[4];
	__le32 ip_src_addr[4];
	__le16 udp_dst_port;
	__le16 udp_src_port;
	__le32 tenant_id;
};


struct rx_update_gft_filter_ramrod_data {
	struct regpair pkt_hdr_addr;
	__le16 pkt_hdr_length;
	__le16 action_icid;
	__le16 rx_qid;
	__le16 flow_id;
	__le16 vport_id;
	u8 action_icid_valid;
	u8 rx_qid_valid;
	u8 flow_id_valid;
	u8 filter_action;
	u8 assert_on_error;
	u8 inner_vlan_removal_en;
};


struct tx_queue_start_ramrod_data {
	__le16 sb_id;
	u8 sb_index;
	u8 vport_id;
	u8 reserved0;
	u8 stats_counter_id;
	__le16 qm_pq_id;
	u8 flags;
#define TX_QUEUE_START_RAMROD_DATA_DISABLE_OPPORTUNISTIC_MASK	0x1
#define TX_QUEUE_START_RAMROD_DATA_DISABLE_OPPORTUNISTIC_SHIFT	0
#define TX_QUEUE_START_RAMROD_DATA_TEST_MODE_PKT_DUP_MASK	0x1
#define TX_QUEUE_START_RAMROD_DATA_TEST_MODE_PKT_DUP_SHIFT	1
#define TX_QUEUE_START_RAMROD_DATA_PMD_MODE_MASK		0x1
#define TX_QUEUE_START_RAMROD_DATA_PMD_MODE_SHIFT		2
#define TX_QUEUE_START_RAMROD_DATA_NOTIFY_EN_MASK		0x1
#define TX_QUEUE_START_RAMROD_DATA_NOTIFY_EN_SHIFT		3
#define TX_QUEUE_START_RAMROD_DATA_PIN_CONTEXT_MASK		0x1
#define TX_QUEUE_START_RAMROD_DATA_PIN_CONTEXT_SHIFT		4
#define TX_QUEUE_START_RAMROD_DATA_RESERVED1_MASK		0x7
#define TX_QUEUE_START_RAMROD_DATA_RESERVED1_SHIFT		5
	u8 pxp_st_hint;
	u8 pxp_tph_valid_bd;
	u8 pxp_tph_valid_pkt;
	__le16 pxp_st_index;
	u8 comp_agg_size;
	u8 reserved3;
	__le16 queue_zone_id;
	__le16 reserved2;
	__le16 pbl_size;
	__le16 tx_queue_id;
	__le16 same_as_last_id;
	__le16 reserved[3];
	struct regpair pbl_base_addr;
	struct regpair bd_cons_address;
};


struct tx_queue_stop_ramrod_data {
	__le16 reserved[4];
};


struct tx_queue_update_ramrod_data {
	__le16 update_qm_pq_id_flg;
	__le16 qm_pq_id;
	__le32 reserved0;
	struct regpair reserved1[5];
};


enum update_in_to_in_pri_map_mode_enum {
	ETH_IN_TO_IN_PRI_MAP_UPDATE_DISABLED,
	ETH_IN_TO_IN_PRI_MAP_UPDATE_NON_RDMA_TBL,
	ETH_IN_TO_IN_PRI_MAP_UPDATE_RDMA_TBL,
	MAX_UPDATE_IN_TO_IN_PRI_MAP_MODE_ENUM
};


struct vport_filter_update_ramrod_data {
	struct eth_filter_cmd_header filter_cmd_hdr;
	struct eth_filter_cmd filter_cmds[ETH_FILTER_RULES_COUNT];
};


struct vport_start_ramrod_data {
	u8 vport_id;
	u8 sw_fid;
	__le16 mtu;
	u8 drop_ttl0_en;
	u8 inner_vlan_removal_en;
	struct eth_vport_rx_mode rx_mode;
	struct eth_vport_tx_mode tx_mode;
	struct eth_vport_tpa_param tpa_param;
	__le16 default_vlan;
	u8 tx_switching_en;
	u8 anti_spoofing_en;
	u8 default_vlan_en;
	u8 handle_ptp_pkts;
	u8 silent_vlan_removal_en;
	u8 untagged;
	struct eth_tx_err_vals tx_err_behav;
	u8 zero_placement_offset;
	u8 ctl_frame_mac_check_en;
	u8 ctl_frame_ethtype_check_en;
	u8 reserved0;
	u8 reserved1;
	u8 tx_dst_port_mode_config;
	u8 dst_vport_id;
	u8 tx_dst_port_mode;
	u8 dst_vport_id_valid;
	u8 wipe_inner_vlan_pri_en;
	u8 reserved2[2];
	struct eth_in_to_in_pri_map_cfg in_to_in_vlan_pri_map_cfg;
};


struct vport_stop_ramrod_data {
	u8 vport_id;
	u8 reserved[7];
};


struct vport_update_ramrod_data_cmn {
	u8 vport_id;
	u8 update_rx_active_flg;
	u8 rx_active_flg;
	u8 update_tx_active_flg;
	u8 tx_active_flg;
	u8 update_rx_mode_flg;
	u8 update_tx_mode_flg;
	u8 update_approx_mcast_flg;

	u8 update_rss_flg;
	u8 update_inner_vlan_removal_en_flg;

	u8 inner_vlan_removal_en;
	u8 update_tpa_param_flg;
	u8 update_tpa_en_flg;
	u8 update_tx_switching_en_flg;

	u8 tx_switching_en;
	u8 update_anti_spoofing_en_flg;

	u8 anti_spoofing_en;
	u8 update_handle_ptp_pkts;

	u8 handle_ptp_pkts;
	u8 update_default_vlan_en_flg;

	u8 default_vlan_en;

	u8 update_default_vlan_flg;

	__le16 default_vlan;
	u8 update_accept_any_vlan_flg;

	u8 accept_any_vlan;
	u8 silent_vlan_removal_en;
	u8 update_mtu_flg;

	__le16 mtu;
	u8 update_ctl_frame_checks_en_flg;
	u8 ctl_frame_mac_check_en;
	u8 ctl_frame_ethtype_check_en;
	u8 update_in_to_in_pri_map_mode;
	u8 in_to_in_pri_map[8];
	u8 update_tx_dst_port_mode_flg;
	u8 tx_dst_port_mode_config;
	u8 dst_vport_id;
	u8 tx_dst_port_mode;
	u8 dst_vport_id_valid;
	u8 reserved[1];
};

struct vport_update_ramrod_mcast {
	__le32 bins[ETH_MULTICAST_MAC_BINS_IN_REGS];
};


struct vport_update_ramrod_data {
	struct vport_update_ramrod_data_cmn common;

	struct eth_vport_rx_mode rx_mode;
	struct eth_vport_tx_mode tx_mode;
	__le32 reserved[3];
	struct eth_vport_tpa_param tpa_param;
	struct vport_update_ramrod_mcast approx_mcast;
	struct eth_vport_rss_config rss_config;
};

struct xstorm_eth_conn_ag_ctx_dq_ext_ldpart {
	u8 reserved0;
	u8 state;
	u8 flags0;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EXIST_IN_QM0_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EXIST_IN_QM0_SHIFT	0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED1_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED1_SHIFT		1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED2_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED2_SHIFT		2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EXIST_IN_QM3_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EXIST_IN_QM3_SHIFT	3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED3_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED3_SHIFT		4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED4_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED4_SHIFT		5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED5_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED5_SHIFT		6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED6_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED6_SHIFT		7
	u8 flags1;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED7_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED7_SHIFT		0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED8_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED8_SHIFT		1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED9_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED9_SHIFT		2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_BIT11_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_BIT11_SHIFT		3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_E5_RESERVED2_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_E5_RESERVED2_SHIFT	4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_E5_RESERVED3_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_E5_RESERVED3_SHIFT	5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TX_RULE_ACTIVE_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TX_RULE_ACTIVE_SHIFT	6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_ACTIVE_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_ACTIVE_SHIFT	7
	u8 flags2;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF0_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF0_SHIFT	0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF1_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF1_SHIFT	2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF2_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF2_SHIFT	4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF3_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF3_SHIFT	6
	u8 flags3;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF4_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF4_SHIFT	0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF5_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF5_SHIFT	2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF6_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF6_SHIFT	4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF7_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF7_SHIFT	6
	u8 flags4;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF8_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF8_SHIFT	0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF9_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF9_SHIFT	2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF10_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF10_SHIFT	4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF11_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF11_SHIFT	6
	u8 flags5;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF12_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF12_SHIFT	0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF13_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF13_SHIFT	2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF14_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF14_SHIFT	4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF15_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF15_SHIFT	6
	u8 flags6;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_GO_TO_BD_CONS_CF_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_GO_TO_BD_CONS_CF_SHIFT	0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_MULTI_UNICAST_CF_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_MULTI_UNICAST_CF_SHIFT	2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_MASK		0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_SHIFT		4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TERMINATE_CF_MASK	0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TERMINATE_CF_SHIFT	6
	u8 flags7;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_FLUSH_Q0_MASK		0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_FLUSH_Q0_SHIFT		0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED10_MASK		0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED10_SHIFT	2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_SLOW_PATH_MASK		0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_SLOW_PATH_SHIFT		4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF0EN_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF0EN_SHIFT		6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF1EN_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF1EN_SHIFT		7
	u8 flags8;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF2EN_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF2EN_SHIFT	0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF3EN_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF3EN_SHIFT	1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF4EN_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF4EN_SHIFT	2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF5EN_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF5EN_SHIFT	3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF6EN_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF6EN_SHIFT	4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF7EN_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF7EN_SHIFT	5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF8EN_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF8EN_SHIFT	6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF9EN_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF9EN_SHIFT	7
	u8 flags9;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF10EN_MASK			0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF10EN_SHIFT			0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF11EN_MASK			0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF11EN_SHIFT			1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF12EN_MASK			0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF12EN_SHIFT			2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF13EN_MASK			0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF13EN_SHIFT			3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF14EN_MASK			0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF14EN_SHIFT			4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF15EN_MASK			0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_CF15EN_SHIFT			5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_GO_TO_BD_CONS_CF_EN_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_GO_TO_BD_CONS_CF_EN_SHIFT	6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_MULTI_UNICAST_CF_EN_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_MULTI_UNICAST_CF_EN_SHIFT	7
	u8 flags10;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_EN_MASK			0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_DQ_CF_EN_SHIFT			0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TERMINATE_CF_EN_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TERMINATE_CF_EN_SHIFT		1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_FLUSH_Q0_EN_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_FLUSH_Q0_EN_SHIFT		2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED11_MASK			0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED11_SHIFT		3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_SLOW_PATH_EN_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_SLOW_PATH_EN_SHIFT		4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TPH_ENABLE_EN_RESERVED_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TPH_ENABLE_EN_RESERVED_SHIFT	5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED12_MASK			0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED12_SHIFT		6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED13_MASK			0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED13_SHIFT		7
	u8 flags11;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED14_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED14_SHIFT	0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED15_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RESERVED15_SHIFT	1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TX_DEC_RULE_EN_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TX_DEC_RULE_EN_SHIFT	2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE5EN_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE5EN_SHIFT		3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE6EN_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE6EN_SHIFT		4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE7EN_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE7EN_SHIFT		5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED1_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED1_SHIFT	6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE9EN_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE9EN_SHIFT		7
	u8 flags12;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE10EN_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE10EN_SHIFT		0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE11EN_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE11EN_SHIFT		1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED2_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED2_SHIFT	2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED3_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED3_SHIFT	3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE14EN_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE14EN_SHIFT		4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE15EN_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE15EN_SHIFT		5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE16EN_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE16EN_SHIFT		6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE17EN_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE17EN_SHIFT		7
	u8 flags13;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE18EN_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE18EN_SHIFT		0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE19EN_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_RULE19EN_SHIFT		1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED4_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED4_SHIFT	2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED5_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED5_SHIFT	3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED6_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED6_SHIFT	4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED7_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED7_SHIFT	5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED8_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED8_SHIFT	6
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED9_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_A0_RESERVED9_SHIFT	7
	u8 flags14;
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_USE_EXT_HDR_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_USE_EXT_HDR_SHIFT		0
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_SEND_RAW_L3L4_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_SEND_RAW_L3L4_SHIFT	1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_INBAND_PROP_HDR_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_INBAND_PROP_HDR_SHIFT	2
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_SEND_EXT_TUNNEL_MASK	0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_EDPM_SEND_EXT_TUNNEL_SHIFT	3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_L2_EDPM_ENABLE_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_L2_EDPM_ENABLE_SHIFT		4
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_ROCE_EDPM_ENABLE_MASK		0x1
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_ROCE_EDPM_ENABLE_SHIFT		5
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TPH_ENABLE_MASK			0x3
#define E4XSTORMETHCONNAGCTXDQEXTLDPART_TPH_ENABLE_SHIFT		6
	u8 edpm_event_id;
	__le16 physical_q0;
	__le16 e5_reserved1;
	__le16 edpm_num_bds;
	__le16 tx_bd_cons;
	__le16 tx_bd_prod;
	__le16 updated_qm_pq_id;
	__le16 conn_dpi;
	u8 byte3;
	u8 byte4;
	u8 byte5;
	u8 byte6;
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 reg3;
	__le32 reg4;
};

struct mstorm_eth_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define MSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM0_MASK	0x1
#define MSTORM_ETH_CONN_AG_CTX_EXIST_IN_QM0_SHIFT	 0
#define MSTORM_ETH_CONN_AG_CTX_BIT1_MASK		0x1
#define MSTORM_ETH_CONN_AG_CTX_BIT1_SHIFT		1
#define MSTORM_ETH_CONN_AG_CTX_CF0_MASK		0x3
#define MSTORM_ETH_CONN_AG_CTX_CF0_SHIFT		2
#define MSTORM_ETH_CONN_AG_CTX_CF1_MASK		0x3
#define MSTORM_ETH_CONN_AG_CTX_CF1_SHIFT		4
#define MSTORM_ETH_CONN_AG_CTX_CF2_MASK		0x3
#define MSTORM_ETH_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define MSTORM_ETH_CONN_AG_CTX_CF0EN_MASK	0x1
#define MSTORM_ETH_CONN_AG_CTX_CF0EN_SHIFT	0
#define MSTORM_ETH_CONN_AG_CTX_CF1EN_MASK	0x1
#define MSTORM_ETH_CONN_AG_CTX_CF1EN_SHIFT	1
#define MSTORM_ETH_CONN_AG_CTX_CF2EN_MASK	0x1
#define MSTORM_ETH_CONN_AG_CTX_CF2EN_SHIFT	2
#define MSTORM_ETH_CONN_AG_CTX_RULE0EN_MASK	0x1
#define MSTORM_ETH_CONN_AG_CTX_RULE0EN_SHIFT	3
#define MSTORM_ETH_CONN_AG_CTX_RULE1EN_MASK	0x1
#define MSTORM_ETH_CONN_AG_CTX_RULE1EN_SHIFT	4
#define MSTORM_ETH_CONN_AG_CTX_RULE2EN_MASK	0x1
#define MSTORM_ETH_CONN_AG_CTX_RULE2EN_SHIFT	5
#define MSTORM_ETH_CONN_AG_CTX_RULE3EN_MASK	0x1
#define MSTORM_ETH_CONN_AG_CTX_RULE3EN_SHIFT	6
#define MSTORM_ETH_CONN_AG_CTX_RULE4EN_MASK	0x1
#define MSTORM_ETH_CONN_AG_CTX_RULE4EN_SHIFT	7
	__le16 word0;
	__le16 word1;
	__le32 reg0;
	__le32 reg1;
};

struct xstorm_eth_hw_conn_ag_ctx {
	u8 reserved0;
	u8 state;
	u8 flags0;
#define XSTORM_ETH_HW_CONN_AG_CTX_EXIST_IN_QM0_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_EXIST_IN_QM0_SHIFT	0
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED1_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED1_SHIFT	1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED2_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED2_SHIFT	2
#define XSTORM_ETH_HW_CONN_AG_CTX_EXIST_IN_QM3_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_EXIST_IN_QM3_SHIFT	3
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED3_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED3_SHIFT	4
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED4_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED4_SHIFT	5
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED5_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED5_SHIFT	6
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED6_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED6_SHIFT	7
	u8 flags1;
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED7_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED7_SHIFT		0
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED8_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED8_SHIFT		1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED9_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED9_SHIFT		2
#define XSTORM_ETH_HW_CONN_AG_CTX_BIT11_MASK			0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_BIT11_SHIFT		3
#define XSTORM_ETH_HW_CONN_AG_CTX_E5_RESERVED2_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_E5_RESERVED2_SHIFT		4
#define XSTORM_ETH_HW_CONN_AG_CTX_E5_RESERVED3_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_E5_RESERVED3_SHIFT		5
#define XSTORM_ETH_HW_CONN_AG_CTX_TX_RULE_ACTIVE_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_TX_RULE_ACTIVE_SHIFT	6
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_ACTIVE_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_ACTIVE_SHIFT		7
	u8 flags2;
#define XSTORM_ETH_HW_CONN_AG_CTX_CF0_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF0_SHIFT	0
#define XSTORM_ETH_HW_CONN_AG_CTX_CF1_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF1_SHIFT	2
#define XSTORM_ETH_HW_CONN_AG_CTX_CF2_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF2_SHIFT	4
#define XSTORM_ETH_HW_CONN_AG_CTX_CF3_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF3_SHIFT	6
	u8 flags3;
#define XSTORM_ETH_HW_CONN_AG_CTX_CF4_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF4_SHIFT	0
#define XSTORM_ETH_HW_CONN_AG_CTX_CF5_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF5_SHIFT	2
#define XSTORM_ETH_HW_CONN_AG_CTX_CF6_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF6_SHIFT	4
#define XSTORM_ETH_HW_CONN_AG_CTX_CF7_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF7_SHIFT	6
	u8 flags4;
#define XSTORM_ETH_HW_CONN_AG_CTX_CF8_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF8_SHIFT	0
#define XSTORM_ETH_HW_CONN_AG_CTX_CF9_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF9_SHIFT	2
#define XSTORM_ETH_HW_CONN_AG_CTX_CF10_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF10_SHIFT	4
#define XSTORM_ETH_HW_CONN_AG_CTX_CF11_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF11_SHIFT	6
	u8 flags5;
#define XSTORM_ETH_HW_CONN_AG_CTX_CF12_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF12_SHIFT	0
#define XSTORM_ETH_HW_CONN_AG_CTX_CF13_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF13_SHIFT	2
#define XSTORM_ETH_HW_CONN_AG_CTX_CF14_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF14_SHIFT	4
#define XSTORM_ETH_HW_CONN_AG_CTX_CF15_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF15_SHIFT	6
	u8 flags6;
#define XSTORM_ETH_HW_CONN_AG_CTX_GO_TO_BD_CONS_CF_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_GO_TO_BD_CONS_CF_SHIFT	0
#define XSTORM_ETH_HW_CONN_AG_CTX_MULTI_UNICAST_CF_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_MULTI_UNICAST_CF_SHIFT	2
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_MASK			0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_SHIFT		4
#define XSTORM_ETH_HW_CONN_AG_CTX_TERMINATE_CF_MASK		0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_TERMINATE_CF_SHIFT		6
	u8 flags7;
#define XSTORM_ETH_HW_CONN_AG_CTX_FLUSH_Q0_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_FLUSH_Q0_SHIFT	0
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED10_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED10_SHIFT	2
#define XSTORM_ETH_HW_CONN_AG_CTX_SLOW_PATH_MASK	0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_SLOW_PATH_SHIFT	4
#define XSTORM_ETH_HW_CONN_AG_CTX_CF0EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF0EN_SHIFT	6
#define XSTORM_ETH_HW_CONN_AG_CTX_CF1EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF1EN_SHIFT	7
	u8 flags8;
#define XSTORM_ETH_HW_CONN_AG_CTX_CF2EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF2EN_SHIFT	0
#define XSTORM_ETH_HW_CONN_AG_CTX_CF3EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF3EN_SHIFT	1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF4EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF4EN_SHIFT	2
#define XSTORM_ETH_HW_CONN_AG_CTX_CF5EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF5EN_SHIFT	3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF6EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF6EN_SHIFT	4
#define XSTORM_ETH_HW_CONN_AG_CTX_CF7EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF7EN_SHIFT	5
#define XSTORM_ETH_HW_CONN_AG_CTX_CF8EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF8EN_SHIFT	6
#define XSTORM_ETH_HW_CONN_AG_CTX_CF9EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF9EN_SHIFT	7
	u8 flags9;
#define XSTORM_ETH_HW_CONN_AG_CTX_CF10EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF10EN_SHIFT		0
#define XSTORM_ETH_HW_CONN_AG_CTX_CF11EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF11EN_SHIFT		1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF12EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF12EN_SHIFT		2
#define XSTORM_ETH_HW_CONN_AG_CTX_CF13EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF13EN_SHIFT		3
#define XSTORM_ETH_HW_CONN_AG_CTX_CF14EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF14EN_SHIFT		4
#define XSTORM_ETH_HW_CONN_AG_CTX_CF15EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_CF15EN_SHIFT		5
#define XSTORM_ETH_HW_CONN_AG_CTX_GO_TO_BD_CONS_CF_EN_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_GO_TO_BD_CONS_CF_EN_SHIFT	6
#define XSTORM_ETH_HW_CONN_AG_CTX_MULTI_UNICAST_CF_EN_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_MULTI_UNICAST_CF_EN_SHIFT	7
	u8 flags10;
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_EN_MASK			0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_DQ_CF_EN_SHIFT			0
#define XSTORM_ETH_HW_CONN_AG_CTX_TERMINATE_CF_EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_TERMINATE_CF_EN_SHIFT		1
#define XSTORM_ETH_HW_CONN_AG_CTX_FLUSH_Q0_EN_MASK			0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT			2
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED11_MASK			0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED11_SHIFT			3
#define XSTORM_ETH_HW_CONN_AG_CTX_SLOW_PATH_EN_MASK			0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_SLOW_PATH_EN_SHIFT			4
#define XSTORM_ETH_HW_CONN_AG_CTX_TPH_ENABLE_EN_RESERVED_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_TPH_ENABLE_EN_RESERVED_SHIFT	5
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED12_MASK			0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED12_SHIFT			6
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED13_MASK			0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED13_SHIFT			7
	u8 flags11;
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED14_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED14_SHIFT		0
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED15_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RESERVED15_SHIFT		1
#define XSTORM_ETH_HW_CONN_AG_CTX_TX_DEC_RULE_EN_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_TX_DEC_RULE_EN_SHIFT	2
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE5EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE5EN_SHIFT		3
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE6EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE6EN_SHIFT		4
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE7EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE7EN_SHIFT		5
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED1_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED1_SHIFT		6
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE9EN_MASK		0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE9EN_SHIFT		7
	u8 flags12;
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE10EN_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE10EN_SHIFT	0
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE11EN_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE11EN_SHIFT	1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED2_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED2_SHIFT	2
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED3_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED3_SHIFT	3
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE14EN_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE14EN_SHIFT	4
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE15EN_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE15EN_SHIFT	5
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE16EN_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE16EN_SHIFT	6
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE17EN_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE17EN_SHIFT	7
	u8 flags13;
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE18EN_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE18EN_SHIFT	0
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE19EN_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_RULE19EN_SHIFT	1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED4_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED4_SHIFT	2
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED5_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED5_SHIFT	3
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED6_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED6_SHIFT	4
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED7_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED7_SHIFT	5
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED8_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED8_SHIFT	6
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED9_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_A0_RESERVED9_SHIFT	7
	u8 flags14;
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_USE_EXT_HDR_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_USE_EXT_HDR_SHIFT	0
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_SEND_RAW_L3L4_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_SEND_RAW_L3L4_SHIFT	1
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_INBAND_PROP_HDR_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_INBAND_PROP_HDR_SHIFT	2
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_SEND_EXT_TUNNEL_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_EDPM_SEND_EXT_TUNNEL_SHIFT	3
#define XSTORM_ETH_HW_CONN_AG_CTX_L2_EDPM_ENABLE_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_L2_EDPM_ENABLE_SHIFT	4
#define XSTORM_ETH_HW_CONN_AG_CTX_ROCE_EDPM_ENABLE_MASK	0x1
#define XSTORM_ETH_HW_CONN_AG_CTX_ROCE_EDPM_ENABLE_SHIFT	5
#define XSTORM_ETH_HW_CONN_AG_CTX_TPH_ENABLE_MASK		0x3
#define XSTORM_ETH_HW_CONN_AG_CTX_TPH_ENABLE_SHIFT		6
	u8 edpm_event_id;
	__le16 physical_q0;
	__le16 e5_reserved1;
	__le16 edpm_num_bds;
	__le16 tx_bd_cons;
	__le16 tx_bd_prod;
	__le16 updated_qm_pq_id;
	__le16 conn_dpi;
};


struct gft_cam_line_mapped {
	__le32 camline;
#define GFT_CAM_LINE_MAPPED_VALID_MASK				0x1
#define GFT_CAM_LINE_MAPPED_VALID_SHIFT				0
#define GFT_CAM_LINE_MAPPED_IP_VERSION_MASK			0x1
#define GFT_CAM_LINE_MAPPED_IP_VERSION_SHIFT			1
#define GFT_CAM_LINE_MAPPED_TUNNEL_IP_VERSION_MASK		0x1
#define GFT_CAM_LINE_MAPPED_TUNNEL_IP_VERSION_SHIFT		2
#define GFT_CAM_LINE_MAPPED_UPPER_PROTOCOL_TYPE_MASK		0xF
#define GFT_CAM_LINE_MAPPED_UPPER_PROTOCOL_TYPE_SHIFT		3
#define GFT_CAM_LINE_MAPPED_TUNNEL_TYPE_MASK			0xF
#define GFT_CAM_LINE_MAPPED_TUNNEL_TYPE_SHIFT			7
#define GFT_CAM_LINE_MAPPED_PF_ID_MASK				0xF
#define GFT_CAM_LINE_MAPPED_PF_ID_SHIFT				11
#define GFT_CAM_LINE_MAPPED_IP_VERSION_MASK_MASK		0x1
#define GFT_CAM_LINE_MAPPED_IP_VERSION_MASK_SHIFT		15
#define GFT_CAM_LINE_MAPPED_TUNNEL_IP_VERSION_MASK_MASK		0x1
#define GFT_CAM_LINE_MAPPED_TUNNEL_IP_VERSION_MASK_SHIFT	16
#define GFT_CAM_LINE_MAPPED_UPPER_PROTOCOL_TYPE_MASK_MASK	0xF
#define GFT_CAM_LINE_MAPPED_UPPER_PROTOCOL_TYPE_MASK_SHIFT	17
#define GFT_CAM_LINE_MAPPED_TUNNEL_TYPE_MASK_MASK		0xF
#define GFT_CAM_LINE_MAPPED_TUNNEL_TYPE_MASK_SHIFT		21
#define GFT_CAM_LINE_MAPPED_PF_ID_MASK_MASK			0xF
#define GFT_CAM_LINE_MAPPED_PF_ID_MASK_SHIFT			25
#define GFT_CAM_LINE_MAPPED_RESERVED1_MASK			0x7
#define GFT_CAM_LINE_MAPPED_RESERVED1_SHIFT			29
};


enum gft_profile_ip_version {
	GFT_PROFILE_IPV4 = 0,
	GFT_PROFILE_IPV6 = 1,
	MAX_GFT_PROFILE_IP_VERSION
};


struct gft_profile_key {
	__le16 profile_key;
#define GFT_PROFILE_KEY_IP_VERSION_MASK			0x1
#define GFT_PROFILE_KEY_IP_VERSION_SHIFT		0
#define GFT_PROFILE_KEY_TUNNEL_IP_VERSION_MASK		0x1
#define GFT_PROFILE_KEY_TUNNEL_IP_VERSION_SHIFT		1
#define GFT_PROFILE_KEY_UPPER_PROTOCOL_TYPE_MASK	0xF
#define GFT_PROFILE_KEY_UPPER_PROTOCOL_TYPE_SHIFT	2
#define GFT_PROFILE_KEY_TUNNEL_TYPE_MASK		0xF
#define GFT_PROFILE_KEY_TUNNEL_TYPE_SHIFT		6
#define GFT_PROFILE_KEY_PF_ID_MASK			0xF
#define GFT_PROFILE_KEY_PF_ID_SHIFT			10
#define GFT_PROFILE_KEY_RESERVED0_MASK			0x3
#define GFT_PROFILE_KEY_RESERVED0_SHIFT			14
};


enum gft_profile_tunnel_type {
	GFT_PROFILE_NO_TUNNEL = 0,
	GFT_PROFILE_VXLAN_TUNNEL = 1,
	GFT_PROFILE_GRE_MAC_OR_NVGRE_TUNNEL = 2,
	GFT_PROFILE_GRE_IP_TUNNEL = 3,
	GFT_PROFILE_GENEVE_MAC_TUNNEL = 4,
	GFT_PROFILE_GENEVE_IP_TUNNEL = 5,
	MAX_GFT_PROFILE_TUNNEL_TYPE
};


enum gft_profile_upper_protocol_type {
	GFT_PROFILE_ROCE_PROTOCOL = 0,
	GFT_PROFILE_RROCE_PROTOCOL = 1,
	GFT_PROFILE_FCOE_PROTOCOL = 2,
	GFT_PROFILE_ICMP_PROTOCOL = 3,
	GFT_PROFILE_ARP_PROTOCOL = 4,
	GFT_PROFILE_USER_TCP_SRC_PORT_1_INNER = 5,
	GFT_PROFILE_USER_TCP_DST_PORT_1_INNER = 6,
	GFT_PROFILE_TCP_PROTOCOL = 7,
	GFT_PROFILE_USER_UDP_DST_PORT_1_INNER = 8,
	GFT_PROFILE_USER_UDP_DST_PORT_2_OUTER = 9,
	GFT_PROFILE_UDP_PROTOCOL = 10,
	GFT_PROFILE_USER_IP_1_INNER = 11,
	GFT_PROFILE_USER_IP_2_OUTER = 12,
	GFT_PROFILE_USER_ETH_1_INNER = 13,
	GFT_PROFILE_USER_ETH_2_OUTER = 14,
	GFT_PROFILE_RAW = 15,
	MAX_GFT_PROFILE_UPPER_PROTOCOL_TYPE
};


struct gft_ram_line {
	__le32 lo;
#define GFT_RAM_LINE_VLAN_SELECT_MASK			0x3
#define GFT_RAM_LINE_VLAN_SELECT_SHIFT			0
#define GFT_RAM_LINE_TUNNEL_ENTROPHY_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_ENTROPHY_SHIFT		2
#define GFT_RAM_LINE_TUNNEL_TTL_EQUAL_ONE_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_TTL_EQUAL_ONE_SHIFT		3
#define GFT_RAM_LINE_TUNNEL_TTL_MASK			0x1
#define GFT_RAM_LINE_TUNNEL_TTL_SHIFT			4
#define GFT_RAM_LINE_TUNNEL_ETHERTYPE_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_ETHERTYPE_SHIFT		5
#define GFT_RAM_LINE_TUNNEL_DST_PORT_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_DST_PORT_SHIFT		6
#define GFT_RAM_LINE_TUNNEL_SRC_PORT_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_SRC_PORT_SHIFT		7
#define GFT_RAM_LINE_TUNNEL_DSCP_MASK			0x1
#define GFT_RAM_LINE_TUNNEL_DSCP_SHIFT			8
#define GFT_RAM_LINE_TUNNEL_OVER_IP_PROTOCOL_MASK	0x1
#define GFT_RAM_LINE_TUNNEL_OVER_IP_PROTOCOL_SHIFT	9
#define GFT_RAM_LINE_TUNNEL_DST_IP_MASK			0x1
#define GFT_RAM_LINE_TUNNEL_DST_IP_SHIFT		10
#define GFT_RAM_LINE_TUNNEL_SRC_IP_MASK			0x1
#define GFT_RAM_LINE_TUNNEL_SRC_IP_SHIFT		11
#define GFT_RAM_LINE_TUNNEL_PRIORITY_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_PRIORITY_SHIFT		12
#define GFT_RAM_LINE_TUNNEL_PROVIDER_VLAN_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_PROVIDER_VLAN_SHIFT		13
#define GFT_RAM_LINE_TUNNEL_VLAN_MASK			0x1
#define GFT_RAM_LINE_TUNNEL_VLAN_SHIFT			14
#define GFT_RAM_LINE_TUNNEL_DST_MAC_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_DST_MAC_SHIFT		15
#define GFT_RAM_LINE_TUNNEL_SRC_MAC_MASK		0x1
#define GFT_RAM_LINE_TUNNEL_SRC_MAC_SHIFT		16
#define GFT_RAM_LINE_TTL_EQUAL_ONE_MASK			0x1
#define GFT_RAM_LINE_TTL_EQUAL_ONE_SHIFT		17
#define GFT_RAM_LINE_TTL_MASK				0x1
#define GFT_RAM_LINE_TTL_SHIFT				18
#define GFT_RAM_LINE_ETHERTYPE_MASK			0x1
#define GFT_RAM_LINE_ETHERTYPE_SHIFT			19
#define GFT_RAM_LINE_RESERVED0_MASK			0x1
#define GFT_RAM_LINE_RESERVED0_SHIFT			20
#define GFT_RAM_LINE_TCP_FLAG_FIN_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_FIN_SHIFT			21
#define GFT_RAM_LINE_TCP_FLAG_SYN_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_SYN_SHIFT			22
#define GFT_RAM_LINE_TCP_FLAG_RST_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_RST_SHIFT			23
#define GFT_RAM_LINE_TCP_FLAG_PSH_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_PSH_SHIFT			24
#define GFT_RAM_LINE_TCP_FLAG_ACK_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_ACK_SHIFT			25
#define GFT_RAM_LINE_TCP_FLAG_URG_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_URG_SHIFT			26
#define GFT_RAM_LINE_TCP_FLAG_ECE_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_ECE_SHIFT			27
#define GFT_RAM_LINE_TCP_FLAG_CWR_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_CWR_SHIFT			28
#define GFT_RAM_LINE_TCP_FLAG_NS_MASK			0x1
#define GFT_RAM_LINE_TCP_FLAG_NS_SHIFT			29
#define GFT_RAM_LINE_DST_PORT_MASK			0x1
#define GFT_RAM_LINE_DST_PORT_SHIFT			30
#define GFT_RAM_LINE_SRC_PORT_MASK			0x1
#define GFT_RAM_LINE_SRC_PORT_SHIFT			31
	__le32 hi;
#define GFT_RAM_LINE_DSCP_MASK				0x1
#define GFT_RAM_LINE_DSCP_SHIFT				0
#define GFT_RAM_LINE_OVER_IP_PROTOCOL_MASK		0x1
#define GFT_RAM_LINE_OVER_IP_PROTOCOL_SHIFT		1
#define GFT_RAM_LINE_DST_IP_MASK			0x1
#define GFT_RAM_LINE_DST_IP_SHIFT			2
#define GFT_RAM_LINE_SRC_IP_MASK			0x1
#define GFT_RAM_LINE_SRC_IP_SHIFT			3
#define GFT_RAM_LINE_PRIORITY_MASK			0x1
#define GFT_RAM_LINE_PRIORITY_SHIFT			4
#define GFT_RAM_LINE_PROVIDER_VLAN_MASK			0x1
#define GFT_RAM_LINE_PROVIDER_VLAN_SHIFT		5
#define GFT_RAM_LINE_VLAN_MASK				0x1
#define GFT_RAM_LINE_VLAN_SHIFT				6
#define GFT_RAM_LINE_DST_MAC_MASK			0x1
#define GFT_RAM_LINE_DST_MAC_SHIFT			7
#define GFT_RAM_LINE_SRC_MAC_MASK			0x1
#define GFT_RAM_LINE_SRC_MAC_SHIFT			8
#define GFT_RAM_LINE_TENANT_ID_MASK			0x1
#define GFT_RAM_LINE_TENANT_ID_SHIFT			9
#define GFT_RAM_LINE_RESERVED1_MASK			0x3FFFFF
#define GFT_RAM_LINE_RESERVED1_SHIFT			10
};


enum gft_vlan_select {
	INNER_PROVIDER_VLAN = 0,
	INNER_VLAN = 1,
	OUTER_PROVIDER_VLAN = 2,
	OUTER_VLAN = 3,
	MAX_GFT_VLAN_SELECT
};


struct ystorm_rdma_task_st_ctx {
	struct regpair temp[4];
};

struct ystorm_rdma_task_ag_ctx {
	u8 reserved;
	u8 byte1;
	__le16 msem_ctx_upd_seq;
	u8 flags0;
#define YSTORM_RDMA_TASK_AG_CTX_CONNECTION_TYPE_MASK		0xF
#define YSTORM_RDMA_TASK_AG_CTX_CONNECTION_TYPE_SHIFT	0
#define YSTORM_RDMA_TASK_AG_CTX_EXIST_IN_QM0_MASK		0x1
#define YSTORM_RDMA_TASK_AG_CTX_EXIST_IN_QM0_SHIFT		4
#define YSTORM_RDMA_TASK_AG_CTX_BIT1_MASK			0x1
#define YSTORM_RDMA_TASK_AG_CTX_BIT1_SHIFT			5
#define YSTORM_RDMA_TASK_AG_CTX_VALID_MASK			0x1
#define YSTORM_RDMA_TASK_AG_CTX_VALID_SHIFT			6
#define YSTORM_RDMA_TASK_AG_CTX_DIF_FIRST_IO_MASK		0x1
#define YSTORM_RDMA_TASK_AG_CTX_DIF_FIRST_IO_SHIFT		7
	u8 flags1;
#define YSTORM_RDMA_TASK_AG_CTX_CF0_MASK		0x3
#define YSTORM_RDMA_TASK_AG_CTX_CF0_SHIFT		0
#define YSTORM_RDMA_TASK_AG_CTX_CF1_MASK		0x3
#define YSTORM_RDMA_TASK_AG_CTX_CF1_SHIFT		2
#define YSTORM_RDMA_TASK_AG_CTX_CF2SPECIAL_MASK	0x3
#define YSTORM_RDMA_TASK_AG_CTX_CF2SPECIAL_SHIFT	4
#define YSTORM_RDMA_TASK_AG_CTX_CF0EN_MASK		0x1
#define YSTORM_RDMA_TASK_AG_CTX_CF0EN_SHIFT		6
#define YSTORM_RDMA_TASK_AG_CTX_CF1EN_MASK		0x1
#define YSTORM_RDMA_TASK_AG_CTX_CF1EN_SHIFT		7
	u8 flags2;
#define YSTORM_RDMA_TASK_AG_CTX_BIT4_MASK		0x1
#define YSTORM_RDMA_TASK_AG_CTX_BIT4_SHIFT		0
#define YSTORM_RDMA_TASK_AG_CTX_RULE0EN_MASK		0x1
#define YSTORM_RDMA_TASK_AG_CTX_RULE0EN_SHIFT	1
#define YSTORM_RDMA_TASK_AG_CTX_RULE1EN_MASK		0x1
#define YSTORM_RDMA_TASK_AG_CTX_RULE1EN_SHIFT	2
#define YSTORM_RDMA_TASK_AG_CTX_RULE2EN_MASK		0x1
#define YSTORM_RDMA_TASK_AG_CTX_RULE2EN_SHIFT	3
#define YSTORM_RDMA_TASK_AG_CTX_RULE3EN_MASK		0x1
#define YSTORM_RDMA_TASK_AG_CTX_RULE3EN_SHIFT	4
#define YSTORM_RDMA_TASK_AG_CTX_RULE4EN_MASK		0x1
#define YSTORM_RDMA_TASK_AG_CTX_RULE4EN_SHIFT	5
#define YSTORM_RDMA_TASK_AG_CTX_RULE5EN_MASK		0x1
#define YSTORM_RDMA_TASK_AG_CTX_RULE5EN_SHIFT	6
#define YSTORM_RDMA_TASK_AG_CTX_RULE6EN_MASK		0x1
#define YSTORM_RDMA_TASK_AG_CTX_RULE6EN_SHIFT	7
	u8 key;
	__le32 mw_cnt_or_qp_id;
	u8 ref_cnt_seq;
	u8 ctx_upd_seq;
	__le16 dif_flags;
	__le16 tx_ref_count;
	__le16 last_used_ltid;
	__le16 parent_mr_lo;
	__le16 parent_mr_hi;
	__le32 fbo_lo;
	__le32 fbo_hi;
};

struct mstorm_rdma_task_ag_ctx {
	u8 reserved;
	u8 byte1;
	__le16 icid;
	u8 flags0;
#define MSTORM_RDMA_TASK_AG_CTX_CONNECTION_TYPE_MASK		0xF
#define MSTORM_RDMA_TASK_AG_CTX_CONNECTION_TYPE_SHIFT	0
#define MSTORM_RDMA_TASK_AG_CTX_EXIST_IN_QM0_MASK		0x1
#define MSTORM_RDMA_TASK_AG_CTX_EXIST_IN_QM0_SHIFT		4
#define MSTORM_RDMA_TASK_AG_CTX_BIT1_MASK			0x1
#define MSTORM_RDMA_TASK_AG_CTX_BIT1_SHIFT			5
#define MSTORM_RDMA_TASK_AG_CTX_BIT2_MASK			0x1
#define MSTORM_RDMA_TASK_AG_CTX_BIT2_SHIFT			6
#define MSTORM_RDMA_TASK_AG_CTX_DIF_FIRST_IO_MASK		0x1
#define MSTORM_RDMA_TASK_AG_CTX_DIF_FIRST_IO_SHIFT		7
	u8 flags1;
#define MSTORM_RDMA_TASK_AG_CTX_CF0_MASK	0x3
#define MSTORM_RDMA_TASK_AG_CTX_CF0_SHIFT	0
#define MSTORM_RDMA_TASK_AG_CTX_CF1_MASK	0x3
#define MSTORM_RDMA_TASK_AG_CTX_CF1_SHIFT	2
#define MSTORM_RDMA_TASK_AG_CTX_CF2_MASK	0x3
#define MSTORM_RDMA_TASK_AG_CTX_CF2_SHIFT	4
#define MSTORM_RDMA_TASK_AG_CTX_CF0EN_MASK	0x1
#define MSTORM_RDMA_TASK_AG_CTX_CF0EN_SHIFT	6
#define MSTORM_RDMA_TASK_AG_CTX_CF1EN_MASK	0x1
#define MSTORM_RDMA_TASK_AG_CTX_CF1EN_SHIFT	7
	u8 flags2;
#define MSTORM_RDMA_TASK_AG_CTX_CF2EN_MASK		0x1
#define MSTORM_RDMA_TASK_AG_CTX_CF2EN_SHIFT		0
#define MSTORM_RDMA_TASK_AG_CTX_RULE0EN_MASK		0x1
#define MSTORM_RDMA_TASK_AG_CTX_RULE0EN_SHIFT	1
#define MSTORM_RDMA_TASK_AG_CTX_RULE1EN_MASK		0x1
#define MSTORM_RDMA_TASK_AG_CTX_RULE1EN_SHIFT	2
#define MSTORM_RDMA_TASK_AG_CTX_RULE2EN_MASK		0x1
#define MSTORM_RDMA_TASK_AG_CTX_RULE2EN_SHIFT	3
#define MSTORM_RDMA_TASK_AG_CTX_RULE3EN_MASK		0x1
#define MSTORM_RDMA_TASK_AG_CTX_RULE3EN_SHIFT	4
#define MSTORM_RDMA_TASK_AG_CTX_RULE4EN_MASK		0x1
#define MSTORM_RDMA_TASK_AG_CTX_RULE4EN_SHIFT	5
#define MSTORM_RDMA_TASK_AG_CTX_RULE5EN_MASK		0x1
#define MSTORM_RDMA_TASK_AG_CTX_RULE5EN_SHIFT	6
#define MSTORM_RDMA_TASK_AG_CTX_RULE6EN_MASK		0x1
#define MSTORM_RDMA_TASK_AG_CTX_RULE6EN_SHIFT	7
	u8 key;
	__le32 mw_cnt_or_qp_id;
	u8 ref_cnt_seq;
	u8 ctx_upd_seq;
	__le16 dif_flags;
	__le16 tx_ref_count;
	__le16 last_used_ltid;
	__le16 parent_mr_lo;
	__le16 parent_mr_hi;
	__le32 fbo_lo;
	__le32 fbo_hi;
};


struct mstorm_rdma_task_st_ctx {
	struct regpair temp[4];
};


struct ustorm_rdma_task_st_ctx {
	struct regpair temp[6];
};

struct ustorm_rdma_task_ag_ctx {
	u8 reserved;
	u8 state;
	__le16 icid;
	u8 flags0;
#define USTORM_RDMA_TASK_AG_CTX_CONNECTION_TYPE_MASK		0xF
#define USTORM_RDMA_TASK_AG_CTX_CONNECTION_TYPE_SHIFT	0
#define USTORM_RDMA_TASK_AG_CTX_EXIST_IN_QM0_MASK		0x1
#define USTORM_RDMA_TASK_AG_CTX_EXIST_IN_QM0_SHIFT		4
#define USTORM_RDMA_TASK_AG_CTX_BIT1_MASK			0x1
#define USTORM_RDMA_TASK_AG_CTX_BIT1_SHIFT			5
#define USTORM_RDMA_TASK_AG_CTX_DIF_WRITE_RESULT_CF_MASK	0x3
#define USTORM_RDMA_TASK_AG_CTX_DIF_WRITE_RESULT_CF_SHIFT	6
	u8 flags1;
#define USTORM_RDMA_TASK_AG_CTX_DIF_RESULT_TOGGLE_BIT_MASK	0x3
#define USTORM_RDMA_TASK_AG_CTX_DIF_RESULT_TOGGLE_BIT_SHIFT	0
#define USTORM_RDMA_TASK_AG_CTX_DIF_TX_IO_FLG_MASK		0x3
#define USTORM_RDMA_TASK_AG_CTX_DIF_TX_IO_FLG_SHIFT		2
#define USTORM_RDMA_TASK_AG_CTX_DIF_BLOCK_SIZE_MASK          0x3
#define USTORM_RDMA_TASK_AG_CTX_DIF_BLOCK_SIZE_SHIFT         4
#define USTORM_RDMA_TASK_AG_CTX_DIF_ERROR_CF_MASK		0x3
#define USTORM_RDMA_TASK_AG_CTX_DIF_ERROR_CF_SHIFT		6
	u8 flags2;
#define USTORM_RDMA_TASK_AG_CTX_DIF_WRITE_RESULT_CF_EN_MASK	0x1
#define USTORM_RDMA_TASK_AG_CTX_DIF_WRITE_RESULT_CF_EN_SHIFT	0
#define USTORM_RDMA_TASK_AG_CTX_RESERVED2_MASK		0x1
#define USTORM_RDMA_TASK_AG_CTX_RESERVED2_SHIFT		1
#define USTORM_RDMA_TASK_AG_CTX_RESERVED3_MASK		0x1
#define USTORM_RDMA_TASK_AG_CTX_RESERVED3_SHIFT		2
#define USTORM_RDMA_TASK_AG_CTX_RESERVED4_MASK               0x1
#define USTORM_RDMA_TASK_AG_CTX_RESERVED4_SHIFT              3
#define USTORM_RDMA_TASK_AG_CTX_DIF_ERROR_CF_EN_MASK		0x1
#define USTORM_RDMA_TASK_AG_CTX_DIF_ERROR_CF_EN_SHIFT	4
#define USTORM_RDMA_TASK_AG_CTX_RULE0EN_MASK			0x1
#define USTORM_RDMA_TASK_AG_CTX_RULE0EN_SHIFT		5
#define USTORM_RDMA_TASK_AG_CTX_RULE1EN_MASK			0x1
#define USTORM_RDMA_TASK_AG_CTX_RULE1EN_SHIFT		6
#define USTORM_RDMA_TASK_AG_CTX_RULE2EN_MASK			0x1
#define USTORM_RDMA_TASK_AG_CTX_RULE2EN_SHIFT		7
	u8 flags3;
#define USTORM_RDMA_TASK_AG_CTX_DIF_RXMIT_PROD_CONS_EN_MASK	0x1
#define USTORM_RDMA_TASK_AG_CTX_DIF_RXMIT_PROD_CONS_EN_SHIFT	0
#define USTORM_RDMA_TASK_AG_CTX_RULE4EN_MASK			0x1
#define USTORM_RDMA_TASK_AG_CTX_RULE4EN_SHIFT		1
#define USTORM_RDMA_TASK_AG_CTX_DIF_WRITE_PROD_CONS_EN_MASK	0x1
#define USTORM_RDMA_TASK_AG_CTX_DIF_WRITE_PROD_CONS_EN_SHIFT	2
#define USTORM_RDMA_TASK_AG_CTX_RULE6EN_MASK			0x1
#define USTORM_RDMA_TASK_AG_CTX_RULE6EN_SHIFT		3
#define USTORM_RDMA_TASK_AG_CTX_DIF_ERROR_TYPE_MASK		0xF
#define USTORM_RDMA_TASK_AG_CTX_DIF_ERROR_TYPE_SHIFT		4
	__le32 dif_err_intervals;
	__le32 dif_error_1st_interval;
	__le32 dif_rxmit_cons;
	__le32 dif_rxmit_prod;
	__le32 sge_index;
	__le32 sq_cons;
	u8 byte2;
	u8 byte3;
	__le16 dif_write_cons;
	__le16 dif_write_prod;
	__le16 word3;
	__le32 dif_error_buffer_address_lo;
	__le32 dif_error_buffer_address_hi;
};


struct rdma_task_context {
	struct ystorm_rdma_task_st_ctx ystorm_st_context;
	struct ystorm_rdma_task_ag_ctx ystorm_ag_context;
	struct tdif_task_context tdif_context;
	struct mstorm_rdma_task_ag_ctx mstorm_ag_context;
	struct mstorm_rdma_task_st_ctx mstorm_st_context;
	struct rdif_task_context rdif_context;
	struct ustorm_rdma_task_st_ctx ustorm_st_context;
	struct regpair ustorm_st_padding[2];
	struct ustorm_rdma_task_ag_ctx ustorm_ag_context;
};

#define TOE_MAX_RAMROD_PER_PF			8
#define TOE_TX_PAGE_SIZE_BYTES			4096
#define TOE_GRQ_PAGE_SIZE_BYTES			4096
#define TOE_RX_CQ_PAGE_SIZE_BYTES		4096

#define TOE_RX_MAX_RSS_CHAINS			64
#define TOE_TX_MAX_TSS_CHAINS			64
#define TOE_RSS_INDIRECTION_TABLE_SIZE		128


struct mstorm_toe_conn_st_ctx {
	__le32 reserved[24];
};


struct pstorm_toe_conn_st_ctx {
	__le32 reserved[36];
};


struct ystorm_toe_conn_st_ctx {
	__le32 reserved[8];
};


struct xstorm_toe_conn_st_ctx {
	__le32 reserved[44];
};

struct ystorm_toe_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define YSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1
#define YSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define YSTORM_TOE_CONN_AG_CTX_BIT1_MASK			0x1
#define YSTORM_TOE_CONN_AG_CTX_BIT1_SHIFT			1
#define YSTORM_TOE_CONN_AG_CTX_SLOW_PATH_CF_MASK		0x3
#define YSTORM_TOE_CONN_AG_CTX_SLOW_PATH_CF_SHIFT		2
#define YSTORM_TOE_CONN_AG_CTX_RESET_RECEIVED_CF_MASK		0x3
#define YSTORM_TOE_CONN_AG_CTX_RESET_RECEIVED_CF_SHIFT		4
#define YSTORM_TOE_CONN_AG_CTX_CF2_MASK				0x3
#define YSTORM_TOE_CONN_AG_CTX_CF2_SHIFT			6
	u8 flags1;
#define YSTORM_TOE_CONN_AG_CTX_SLOW_PATH_CF_EN_MASK		0x1
#define YSTORM_TOE_CONN_AG_CTX_SLOW_PATH_CF_EN_SHIFT		0
#define YSTORM_TOE_CONN_AG_CTX_RESET_RECEIVED_CF_EN_MASK	0x1
#define YSTORM_TOE_CONN_AG_CTX_RESET_RECEIVED_CF_EN_SHIFT	1
#define YSTORM_TOE_CONN_AG_CTX_CF2EN_MASK			0x1
#define YSTORM_TOE_CONN_AG_CTX_CF2EN_SHIFT			2
#define YSTORM_TOE_CONN_AG_CTX_REL_SEQ_EN_MASK			0x1
#define YSTORM_TOE_CONN_AG_CTX_REL_SEQ_EN_SHIFT			3
#define YSTORM_TOE_CONN_AG_CTX_RULE1EN_MASK			0x1
#define YSTORM_TOE_CONN_AG_CTX_RULE1EN_SHIFT			4
#define YSTORM_TOE_CONN_AG_CTX_RULE2EN_MASK			0x1
#define YSTORM_TOE_CONN_AG_CTX_RULE2EN_SHIFT			5
#define YSTORM_TOE_CONN_AG_CTX_RULE3EN_MASK			0x1
#define YSTORM_TOE_CONN_AG_CTX_RULE3EN_SHIFT			6
#define YSTORM_TOE_CONN_AG_CTX_CONS_PROD_EN_MASK		0x1
#define YSTORM_TOE_CONN_AG_CTX_CONS_PROD_EN_SHIFT		7
	u8 completion_opcode;
	u8 byte3;
	__le16 word0;
	__le32 rel_seq;
	__le32 rel_seq_threshold;
	__le16 app_prod;
	__le16 app_cons;
	__le16 word3;
	__le16 word4;
	__le32 reg2;
	__le32 reg3;
};

struct xstorm_toe_conn_ag_ctx {
	u8 reserved0;
	u8 state;
	u8 flags0;
#define XSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1
#define XSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define XSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM1_MASK		0x1
#define XSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM1_SHIFT		1
#define XSTORM_TOE_CONN_AG_CTX_RESERVED1_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_RESERVED1_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM3_MASK		0x1
#define XSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM3_SHIFT		3
#define XSTORM_TOE_CONN_AG_CTX_TX_DEC_RULE_RES_MASK		0x1
#define XSTORM_TOE_CONN_AG_CTX_TX_DEC_RULE_RES_SHIFT		4
#define XSTORM_TOE_CONN_AG_CTX_RESERVED2_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_RESERVED2_SHIFT			5
#define XSTORM_TOE_CONN_AG_CTX_BIT6_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_BIT6_SHIFT			6
#define XSTORM_TOE_CONN_AG_CTX_BIT7_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_BIT7_SHIFT			7
	u8 flags1;
#define XSTORM_TOE_CONN_AG_CTX_BIT8_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_BIT8_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_BIT9_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_BIT9_SHIFT			1
#define XSTORM_TOE_CONN_AG_CTX_BIT10_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_BIT10_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_BIT11_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_BIT11_SHIFT			3
#define XSTORM_TOE_CONN_AG_CTX_BIT12_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_BIT12_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_BIT13_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_BIT13_SHIFT			5
#define XSTORM_TOE_CONN_AG_CTX_BIT14_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_BIT14_SHIFT			6
#define XSTORM_TOE_CONN_AG_CTX_BIT15_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_BIT15_SHIFT			7
	u8 flags2;
#define XSTORM_TOE_CONN_AG_CTX_CF0_MASK				0x3
#define XSTORM_TOE_CONN_AG_CTX_CF0_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_CF1_MASK				0x3
#define XSTORM_TOE_CONN_AG_CTX_CF1_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_CF2_MASK				0x3
#define XSTORM_TOE_CONN_AG_CTX_CF2_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_MASK		0x3
#define XSTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_SHIFT		6
	u8 flags3;
#define XSTORM_TOE_CONN_AG_CTX_CF4_MASK				0x3
#define XSTORM_TOE_CONN_AG_CTX_CF4_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_CF5_MASK				0x3
#define XSTORM_TOE_CONN_AG_CTX_CF5_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_CF6_MASK				0x3
#define XSTORM_TOE_CONN_AG_CTX_CF6_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_CF7_MASK				0x3
#define XSTORM_TOE_CONN_AG_CTX_CF7_SHIFT			6
	u8 flags4;
#define XSTORM_TOE_CONN_AG_CTX_CF8_MASK				0x3
#define XSTORM_TOE_CONN_AG_CTX_CF8_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_CF9_MASK				0x3
#define XSTORM_TOE_CONN_AG_CTX_CF9_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_CF10_MASK			0x3
#define XSTORM_TOE_CONN_AG_CTX_CF10_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_CF11_MASK			0x3
#define XSTORM_TOE_CONN_AG_CTX_CF11_SHIFT			6
	u8 flags5;
#define XSTORM_TOE_CONN_AG_CTX_CF12_MASK			0x3
#define XSTORM_TOE_CONN_AG_CTX_CF12_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_CF13_MASK			0x3
#define XSTORM_TOE_CONN_AG_CTX_CF13_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_CF14_MASK			0x3
#define XSTORM_TOE_CONN_AG_CTX_CF14_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_CF15_MASK			0x3
#define XSTORM_TOE_CONN_AG_CTX_CF15_SHIFT			6
	u8 flags6;
#define XSTORM_TOE_CONN_AG_CTX_CF16_MASK			0x3
#define XSTORM_TOE_CONN_AG_CTX_CF16_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_CF17_MASK			0x3
#define XSTORM_TOE_CONN_AG_CTX_CF17_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_CF18_MASK			0x3
#define XSTORM_TOE_CONN_AG_CTX_CF18_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_DQ_FLUSH_MASK			0x3
#define XSTORM_TOE_CONN_AG_CTX_DQ_FLUSH_SHIFT			6
	u8 flags7;
#define XSTORM_TOE_CONN_AG_CTX_FLUSH_Q0_MASK			0x3
#define XSTORM_TOE_CONN_AG_CTX_FLUSH_Q0_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_FLUSH_Q1_MASK			0x3
#define XSTORM_TOE_CONN_AG_CTX_FLUSH_Q1_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_SLOW_PATH_MASK			0x3
#define XSTORM_TOE_CONN_AG_CTX_SLOW_PATH_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_CF0EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF0EN_SHIFT			6
#define XSTORM_TOE_CONN_AG_CTX_CF1EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF1EN_SHIFT			7
	u8 flags8;
#define XSTORM_TOE_CONN_AG_CTX_CF2EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF2EN_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_EN_MASK		0x1
#define XSTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_EN_SHIFT		1
#define XSTORM_TOE_CONN_AG_CTX_CF4EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF4EN_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_CF5EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF5EN_SHIFT			3
#define XSTORM_TOE_CONN_AG_CTX_CF6EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF6EN_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_CF7EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF7EN_SHIFT			5
#define XSTORM_TOE_CONN_AG_CTX_CF8EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF8EN_SHIFT			6
#define XSTORM_TOE_CONN_AG_CTX_CF9EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF9EN_SHIFT			7
	u8 flags9;
#define XSTORM_TOE_CONN_AG_CTX_CF10EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF10EN_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_CF11EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF11EN_SHIFT			1
#define XSTORM_TOE_CONN_AG_CTX_CF12EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF12EN_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_CF13EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF13EN_SHIFT			3
#define XSTORM_TOE_CONN_AG_CTX_CF14EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF14EN_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_CF15EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF15EN_SHIFT			5
#define XSTORM_TOE_CONN_AG_CTX_CF16EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF16EN_SHIFT			6
#define XSTORM_TOE_CONN_AG_CTX_CF17EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF17EN_SHIFT			7
	u8 flags10;
#define XSTORM_TOE_CONN_AG_CTX_CF18EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF18EN_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_DQ_FLUSH_EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_DQ_FLUSH_EN_SHIFT		1
#define XSTORM_TOE_CONN_AG_CTX_FLUSH_Q0_EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT		2
#define XSTORM_TOE_CONN_AG_CTX_FLUSH_Q1_EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_FLUSH_Q1_EN_SHIFT		3
#define XSTORM_TOE_CONN_AG_CTX_SLOW_PATH_EN_MASK		0x1
#define XSTORM_TOE_CONN_AG_CTX_SLOW_PATH_EN_SHIFT		4
#define XSTORM_TOE_CONN_AG_CTX_CF23EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_CF23EN_SHIFT			5
#define XSTORM_TOE_CONN_AG_CTX_RULE0EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_RULE0EN_SHIFT			6
#define XSTORM_TOE_CONN_AG_CTX_MORE_TO_SEND_RULE_EN_MASK	0x1
#define XSTORM_TOE_CONN_AG_CTX_MORE_TO_SEND_RULE_EN_SHIFT	7
	u8 flags11;
#define XSTORM_TOE_CONN_AG_CTX_TX_BLOCKED_EN_MASK		0x1
#define XSTORM_TOE_CONN_AG_CTX_TX_BLOCKED_EN_SHIFT		0
#define XSTORM_TOE_CONN_AG_CTX_RULE3EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_RULE3EN_SHIFT			1
#define XSTORM_TOE_CONN_AG_CTX_RESERVED3_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_RESERVED3_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_RULE5EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_RULE5EN_SHIFT			3
#define XSTORM_TOE_CONN_AG_CTX_RULE6EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_RULE6EN_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_RULE7EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_RULE7EN_SHIFT			5
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED1_MASK		0x1
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED1_SHIFT		6
#define XSTORM_TOE_CONN_AG_CTX_RULE9EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_RULE9EN_SHIFT			7
	u8 flags12;
#define XSTORM_TOE_CONN_AG_CTX_RULE10EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_RULE10EN_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_RULE11EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_RULE11EN_SHIFT			1
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED2_MASK		0x1
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED2_SHIFT		2
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED3_MASK		0x1
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED3_SHIFT		3
#define XSTORM_TOE_CONN_AG_CTX_RULE14EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_RULE14EN_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_RULE15EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_RULE15EN_SHIFT			5
#define XSTORM_TOE_CONN_AG_CTX_RULE16EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_RULE16EN_SHIFT			6
#define XSTORM_TOE_CONN_AG_CTX_RULE17EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_RULE17EN_SHIFT			7
	u8 flags13;
#define XSTORM_TOE_CONN_AG_CTX_RULE18EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_RULE18EN_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_RULE19EN_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_RULE19EN_SHIFT			1
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED4_MASK		0x1
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED4_SHIFT		2
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED5_MASK		0x1
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED5_SHIFT		3
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED6_MASK		0x1
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED6_SHIFT		4
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED7_MASK		0x1
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED7_SHIFT		5
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED8_MASK		0x1
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED8_SHIFT		6
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED9_MASK		0x1
#define XSTORM_TOE_CONN_AG_CTX_A0_RESERVED9_SHIFT		7
	u8 flags14;
#define XSTORM_TOE_CONN_AG_CTX_BIT16_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_BIT16_SHIFT			0
#define XSTORM_TOE_CONN_AG_CTX_BIT17_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_BIT17_SHIFT			1
#define XSTORM_TOE_CONN_AG_CTX_BIT18_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_BIT18_SHIFT			2
#define XSTORM_TOE_CONN_AG_CTX_BIT19_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_BIT19_SHIFT			3
#define XSTORM_TOE_CONN_AG_CTX_BIT20_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_BIT20_SHIFT			4
#define XSTORM_TOE_CONN_AG_CTX_BIT21_MASK			0x1
#define XSTORM_TOE_CONN_AG_CTX_BIT21_SHIFT			5
#define XSTORM_TOE_CONN_AG_CTX_CF23_MASK			0x3
#define XSTORM_TOE_CONN_AG_CTX_CF23_SHIFT			6
	u8 byte2;
	__le16 physical_q0;
	__le16 physical_q1;
	__le16 word2;
	__le16 word3;
	__le16 bd_prod;
	__le16 word5;
	__le16 word6;
	u8 byte3;
	u8 byte4;
	u8 byte5;
	u8 byte6;
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 more_to_send_seq;
	__le32 local_adv_wnd_seq;
	__le32 reg5;
	__le32 reg6;
	__le16 word7;
	__le16 word8;
	__le16 word9;
	__le16 word10;
	__le32 reg7;
	__le32 reg8;
	__le32 reg9;
	u8 byte7;
	u8 byte8;
	u8 byte9;
	u8 byte10;
	u8 byte11;
	u8 byte12;
	u8 byte13;
	u8 byte14;
	u8 byte15;
	u8 e5_reserved;
	__le16 word11;
	__le32 reg10;
	__le32 reg11;
	__le32 reg12;
	__le32 reg13;
	__le32 reg14;
	__le32 reg15;
	__le32 reg16;
	__le32 reg17;
};

struct tstorm_toe_conn_ag_ctx {
	u8 reserved0;
	u8 byte1;
	u8 flags0;
#define TSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1
#define TSTORM_TOE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define TSTORM_TOE_CONN_AG_CTX_BIT1_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_BIT1_SHIFT			1
#define TSTORM_TOE_CONN_AG_CTX_BIT2_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_BIT2_SHIFT			2
#define TSTORM_TOE_CONN_AG_CTX_BIT3_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_BIT3_SHIFT			3
#define TSTORM_TOE_CONN_AG_CTX_BIT4_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_BIT4_SHIFT			4
#define TSTORM_TOE_CONN_AG_CTX_BIT5_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_BIT5_SHIFT			5
#define TSTORM_TOE_CONN_AG_CTX_TIMEOUT_CF_MASK			0x3
#define TSTORM_TOE_CONN_AG_CTX_TIMEOUT_CF_SHIFT			6
	u8 flags1;
#define TSTORM_TOE_CONN_AG_CTX_CF1_MASK				0x3
#define TSTORM_TOE_CONN_AG_CTX_CF1_SHIFT			0
#define TSTORM_TOE_CONN_AG_CTX_CF2_MASK				0x3
#define TSTORM_TOE_CONN_AG_CTX_CF2_SHIFT			2
#define TSTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_MASK		0x3
#define TSTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_SHIFT		4
#define TSTORM_TOE_CONN_AG_CTX_CF4_MASK				0x3
#define TSTORM_TOE_CONN_AG_CTX_CF4_SHIFT			6
	u8 flags2;
#define TSTORM_TOE_CONN_AG_CTX_CF5_MASK				0x3
#define TSTORM_TOE_CONN_AG_CTX_CF5_SHIFT			0
#define TSTORM_TOE_CONN_AG_CTX_CF6_MASK				0x3
#define TSTORM_TOE_CONN_AG_CTX_CF6_SHIFT			2
#define TSTORM_TOE_CONN_AG_CTX_CF7_MASK				0x3
#define TSTORM_TOE_CONN_AG_CTX_CF7_SHIFT			4
#define TSTORM_TOE_CONN_AG_CTX_CF8_MASK				0x3
#define TSTORM_TOE_CONN_AG_CTX_CF8_SHIFT			6
	u8 flags3;
#define TSTORM_TOE_CONN_AG_CTX_FLUSH_Q0_MASK			0x3
#define TSTORM_TOE_CONN_AG_CTX_FLUSH_Q0_SHIFT			0
#define TSTORM_TOE_CONN_AG_CTX_CF10_MASK			0x3
#define TSTORM_TOE_CONN_AG_CTX_CF10_SHIFT			2
#define TSTORM_TOE_CONN_AG_CTX_TIMEOUT_CF_EN_MASK		0x1
#define TSTORM_TOE_CONN_AG_CTX_TIMEOUT_CF_EN_SHIFT		4
#define TSTORM_TOE_CONN_AG_CTX_CF1EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_CF1EN_SHIFT			5
#define TSTORM_TOE_CONN_AG_CTX_CF2EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_CF2EN_SHIFT			6
#define TSTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_EN_MASK		0x1
#define TSTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_EN_SHIFT		7
	u8 flags4;
#define TSTORM_TOE_CONN_AG_CTX_CF4EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_CF4EN_SHIFT			0
#define TSTORM_TOE_CONN_AG_CTX_CF5EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_CF5EN_SHIFT			1
#define TSTORM_TOE_CONN_AG_CTX_CF6EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_CF6EN_SHIFT			2
#define TSTORM_TOE_CONN_AG_CTX_CF7EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_CF7EN_SHIFT			3
#define TSTORM_TOE_CONN_AG_CTX_CF8EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_CF8EN_SHIFT			4
#define TSTORM_TOE_CONN_AG_CTX_FLUSH_Q0_EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT		5
#define TSTORM_TOE_CONN_AG_CTX_CF10EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_CF10EN_SHIFT			6
#define TSTORM_TOE_CONN_AG_CTX_RULE0EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_RULE0EN_SHIFT			7
	u8 flags5;
#define TSTORM_TOE_CONN_AG_CTX_RULE1EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_RULE1EN_SHIFT			0
#define TSTORM_TOE_CONN_AG_CTX_RULE2EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_RULE2EN_SHIFT			1
#define TSTORM_TOE_CONN_AG_CTX_RULE3EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_RULE3EN_SHIFT			2
#define TSTORM_TOE_CONN_AG_CTX_RULE4EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_RULE4EN_SHIFT			3
#define TSTORM_TOE_CONN_AG_CTX_RULE5EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_RULE5EN_SHIFT			4
#define TSTORM_TOE_CONN_AG_CTX_RULE6EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_RULE6EN_SHIFT			5
#define TSTORM_TOE_CONN_AG_CTX_RULE7EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_RULE7EN_SHIFT			6
#define TSTORM_TOE_CONN_AG_CTX_RULE8EN_MASK			0x1
#define TSTORM_TOE_CONN_AG_CTX_RULE8EN_SHIFT			7
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 reg3;
	__le32 reg4;
	__le32 reg5;
	__le32 reg6;
	__le32 reg7;
	__le32 reg8;
	u8 byte2;
	u8 byte3;
	__le16 word0;
};

struct ustorm_toe_conn_ag_ctx {
	u8 reserved;
	u8 byte1;
	u8 flags0;
#define USTORM_TOE_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1
#define USTORM_TOE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define USTORM_TOE_CONN_AG_CTX_BIT1_MASK			0x1
#define USTORM_TOE_CONN_AG_CTX_BIT1_SHIFT			1
#define USTORM_TOE_CONN_AG_CTX_CF0_MASK				0x3
#define USTORM_TOE_CONN_AG_CTX_CF0_SHIFT			2
#define USTORM_TOE_CONN_AG_CTX_CF1_MASK				0x3
#define USTORM_TOE_CONN_AG_CTX_CF1_SHIFT			4
#define USTORM_TOE_CONN_AG_CTX_PUSH_TIMER_CF_MASK		0x3
#define USTORM_TOE_CONN_AG_CTX_PUSH_TIMER_CF_SHIFT		6
	u8 flags1;
#define USTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_MASK		0x3
#define USTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_SHIFT		0
#define USTORM_TOE_CONN_AG_CTX_SLOW_PATH_CF_MASK		0x3
#define USTORM_TOE_CONN_AG_CTX_SLOW_PATH_CF_SHIFT		2
#define USTORM_TOE_CONN_AG_CTX_DQ_CF_MASK			0x3
#define USTORM_TOE_CONN_AG_CTX_DQ_CF_SHIFT			4
#define USTORM_TOE_CONN_AG_CTX_CF6_MASK				0x3
#define USTORM_TOE_CONN_AG_CTX_CF6_SHIFT			6
	u8 flags2;
#define USTORM_TOE_CONN_AG_CTX_CF0EN_MASK			0x1
#define USTORM_TOE_CONN_AG_CTX_CF0EN_SHIFT			0
#define USTORM_TOE_CONN_AG_CTX_CF1EN_MASK			0x1
#define USTORM_TOE_CONN_AG_CTX_CF1EN_SHIFT			1
#define USTORM_TOE_CONN_AG_CTX_PUSH_TIMER_CF_EN_MASK		0x1
#define USTORM_TOE_CONN_AG_CTX_PUSH_TIMER_CF_EN_SHIFT		2
#define USTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_EN_MASK		0x1
#define USTORM_TOE_CONN_AG_CTX_TIMER_STOP_ALL_EN_SHIFT		3
#define USTORM_TOE_CONN_AG_CTX_SLOW_PATH_CF_EN_MASK		0x1
#define USTORM_TOE_CONN_AG_CTX_SLOW_PATH_CF_EN_SHIFT		4
#define USTORM_TOE_CONN_AG_CTX_DQ_CF_EN_MASK			0x1
#define USTORM_TOE_CONN_AG_CTX_DQ_CF_EN_SHIFT			5
#define USTORM_TOE_CONN_AG_CTX_CF6EN_MASK			0x1
#define USTORM_TOE_CONN_AG_CTX_CF6EN_SHIFT			6
#define USTORM_TOE_CONN_AG_CTX_RULE0EN_MASK			0x1
#define USTORM_TOE_CONN_AG_CTX_RULE0EN_SHIFT			7
	u8 flags3;
#define USTORM_TOE_CONN_AG_CTX_RULE1EN_MASK			0x1
#define USTORM_TOE_CONN_AG_CTX_RULE1EN_SHIFT			0
#define USTORM_TOE_CONN_AG_CTX_RULE2EN_MASK			0x1
#define USTORM_TOE_CONN_AG_CTX_RULE2EN_SHIFT			1
#define USTORM_TOE_CONN_AG_CTX_RULE3EN_MASK			0x1
#define USTORM_TOE_CONN_AG_CTX_RULE3EN_SHIFT			2
#define USTORM_TOE_CONN_AG_CTX_RULE4EN_MASK			0x1
#define USTORM_TOE_CONN_AG_CTX_RULE4EN_SHIFT			3
#define USTORM_TOE_CONN_AG_CTX_RULE5EN_MASK			0x1
#define USTORM_TOE_CONN_AG_CTX_RULE5EN_SHIFT			4
#define USTORM_TOE_CONN_AG_CTX_RULE6EN_MASK			0x1
#define USTORM_TOE_CONN_AG_CTX_RULE6EN_SHIFT			5
#define USTORM_TOE_CONN_AG_CTX_RULE7EN_MASK			0x1
#define USTORM_TOE_CONN_AG_CTX_RULE7EN_SHIFT			6
#define USTORM_TOE_CONN_AG_CTX_RULE8EN_MASK			0x1
#define USTORM_TOE_CONN_AG_CTX_RULE8EN_SHIFT			7
	u8 byte2;
	u8 byte3;
	__le16 word0;
	__le16 word1;
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 reg3;
	__le16 word2;
	__le16 word3;
};


struct tstorm_toe_conn_st_ctx {
	__le32 reserved[16];
};


struct ustorm_toe_conn_st_ctx {
	__le32 reserved[52];
};


struct toe_conn_context {
	struct ystorm_toe_conn_st_ctx ystorm_st_context;
	struct pstorm_toe_conn_st_ctx pstorm_st_context;
	struct regpair pstorm_st_padding[2];
	struct xstorm_toe_conn_st_ctx xstorm_st_context;
	struct regpair xstorm_st_padding[2];
	struct ystorm_toe_conn_ag_ctx ystorm_ag_context;
	struct xstorm_toe_conn_ag_ctx xstorm_ag_context;
	struct tstorm_toe_conn_ag_ctx tstorm_ag_context;
	struct regpair tstorm_ag_padding[2];
	struct timers_context timer_context;
	struct ustorm_toe_conn_ag_ctx ustorm_ag_context;
	struct tstorm_toe_conn_st_ctx tstorm_st_context;
	struct mstorm_toe_conn_st_ctx mstorm_st_context;
	struct ustorm_toe_conn_st_ctx ustorm_st_context;
};


struct toe_init_ramrod_header {
	u8 first_rss;
	u8 num_rss;
	u8 reserved[6];
};


struct toe_pf_init_params {
	__le32 push_timeout;
	__le16 grq_buffer_size;
	__le16 grq_sb_id;
	u8 grq_sb_index;
	u8 max_seg_retransmit;
	u8 doubt_reachability;
	u8 ll2_rx_queue_id;
	__le16 grq_fetch_threshold;
	u8 reserved1[2];
	struct regpair grq_page_addr;
};


struct toe_tss_params {
	struct regpair curr_page_addr;
	struct regpair next_page_addr;
	u8 reserved0;
	u8 status_block_index;
	__le16 status_block_id;
	__le16 reserved1[2];
};


struct toe_rss_params {
	struct regpair curr_page_addr;
	struct regpair next_page_addr;
	u8 reserved0;
	u8 status_block_index;
	__le16 status_block_id;
	__le16 reserved1[2];
};


struct toe_init_ramrod_data {
	struct toe_init_ramrod_header hdr;
	struct tcp_init_params tcp_params;
	struct toe_pf_init_params pf_params;
	struct toe_tss_params tss_params[TOE_TX_MAX_TSS_CHAINS];
	struct toe_rss_params rss_params[TOE_RX_MAX_RSS_CHAINS];
};


struct toe_offload_params {
	struct regpair tx_bd_page_addr;
	struct regpair tx_app_page_addr;
	__le32 more_to_send_seq;
	__le16 rcv_indication_size;
	u8 rss_tss_id;
	u8 ignore_grq_push;
	struct regpair rx_db_data_ptr;
};


struct toe_offload_ramrod_data {
	struct tcp_offload_params tcp_ofld_params;
	struct toe_offload_params toe_ofld_params;
};


enum toe_ramrod_cmd_id {
	TOE_RAMROD_UNUSED,
	TOE_RAMROD_FUNC_INIT,
	TOE_RAMROD_INITATE_OFFLOAD,
	TOE_RAMROD_FUNC_CLOSE,
	TOE_RAMROD_SEARCHER_DELETE,
	TOE_RAMROD_TERMINATE,
	TOE_RAMROD_QUERY,
	TOE_RAMROD_UPDATE,
	TOE_RAMROD_EMPTY,
	TOE_RAMROD_RESET_SEND,
	TOE_RAMROD_INVALIDATE,
	MAX_TOE_RAMROD_CMD_ID
};


struct toe_rx_bd {
	struct regpair addr;
	__le16 size;
	__le16 flags;
#define TOE_RX_BD_START_MASK		0x1
#define TOE_RX_BD_START_SHIFT		0
#define TOE_RX_BD_END_MASK		0x1
#define TOE_RX_BD_END_SHIFT		1
#define TOE_RX_BD_NO_PUSH_MASK		0x1
#define TOE_RX_BD_NO_PUSH_SHIFT		2
#define TOE_RX_BD_SPLIT_MASK		0x1
#define TOE_RX_BD_SPLIT_SHIFT		3
#define TOE_RX_BD_RESERVED0_MASK	0xFFF
#define TOE_RX_BD_RESERVED0_SHIFT	4
	__le32 reserved1;
};


enum toe_rx_cmp_opcode {
	TOE_RX_CMP_OPCODE_GA = 1,
	TOE_RX_CMP_OPCODE_GR = 2,
	TOE_RX_CMP_OPCODE_GNI = 3,
	TOE_RX_CMP_OPCODE_GAIR = 4,
	TOE_RX_CMP_OPCODE_GAIL = 5,
	TOE_RX_CMP_OPCODE_GRI = 6,
	TOE_RX_CMP_OPCODE_GJ = 7,
	TOE_RX_CMP_OPCODE_DGI = 8,
	TOE_RX_CMP_OPCODE_CMP = 9,
	TOE_RX_CMP_OPCODE_REL = 10,
	TOE_RX_CMP_OPCODE_SKP = 11,
	TOE_RX_CMP_OPCODE_URG = 12,
	TOE_RX_CMP_OPCODE_RT_TO = 13,
	TOE_RX_CMP_OPCODE_KA_TO = 14,
	TOE_RX_CMP_OPCODE_MAX_RT = 15,
	TOE_RX_CMP_OPCODE_DBT_RE = 16,
	TOE_RX_CMP_OPCODE_SYN = 17,
	TOE_RX_CMP_OPCODE_OPT_ERR = 18,
	TOE_RX_CMP_OPCODE_FW2_TO = 19,
	TOE_RX_CMP_OPCODE_2WY_CLS = 20,
	TOE_RX_CMP_OPCODE_RST_RCV = 21,
	TOE_RX_CMP_OPCODE_FIN_RCV = 22,
	TOE_RX_CMP_OPCODE_FIN_UPL = 23,
	TOE_RX_CMP_OPCODE_INIT = 32,
	TOE_RX_CMP_OPCODE_RSS_UPDATE = 33,
	TOE_RX_CMP_OPCODE_CLOSE = 34,
	TOE_RX_CMP_OPCODE_INITIATE_OFFLOAD = 80,
	TOE_RX_CMP_OPCODE_SEARCHER_DELETE = 81,
	TOE_RX_CMP_OPCODE_TERMINATE = 82,
	TOE_RX_CMP_OPCODE_QUERY = 83,
	TOE_RX_CMP_OPCODE_RESET_SEND = 84,
	TOE_RX_CMP_OPCODE_INVALIDATE = 85,
	TOE_RX_CMP_OPCODE_EMPTY = 86,
	TOE_RX_CMP_OPCODE_UPDATE = 87,
	MAX_TOE_RX_CMP_OPCODE
};


struct toe_rx_cqe_ooo_params {
	__le32 nbytes;
	__le16 grq_buff_id;
	u8 isle_num;
	u8 reserved0;
};


struct toe_rx_cqe_in_order_params {
	__le32 nbytes;
	__le16 grq_buff_id;
	__le16 reserved1;
};


union toe_rx_cqe_data_union {
	struct toe_rx_cqe_ooo_params ooo_params;
	struct toe_rx_cqe_in_order_params in_order_params;
	struct regpair raw_data;
};


struct toe_rx_cqe {
	__le16 icid;
	u8 completion_opcode;
	u8 reserved0;
	__le32 reserved1;
	union toe_rx_cqe_data_union data;
};


struct toe_rx_db_data {
	__le32 local_adv_wnd_seq;
	__le32 reserved[3];
};


struct toe_rx_grq_bd {
	struct regpair addr;
	__le16 buff_id;
	__le16 reserved0;
	__le32 reserved1;
};


struct toe_tx_app_buff_desc {
	__le32 next_buffer_start_seq;
	__le32 reserved;
};


struct toe_tx_app_buff_page_pointer {
	struct regpair next_page_addr;
};


struct toe_tx_bd {
	struct regpair addr;
	__le16 size;
	__le16 flags;
#define TOE_TX_BD_PUSH_MASK		0x1
#define TOE_TX_BD_PUSH_SHIFT		0
#define TOE_TX_BD_NOTIFY_MASK		0x1
#define TOE_TX_BD_NOTIFY_SHIFT		1
#define TOE_TX_BD_LARGE_IO_MASK		0x1
#define TOE_TX_BD_LARGE_IO_SHIFT	2
#define TOE_TX_BD_BD_CONS_MASK		0x1FFF
#define TOE_TX_BD_BD_CONS_SHIFT		3
	__le32 next_bd_start_seq;
};


enum toe_tx_cmp_opcode {
	TOE_TX_CMP_OPCODE_DATA,
	TOE_TX_CMP_OPCODE_TERMINATE,
	TOE_TX_CMP_OPCODE_EMPTY,
	TOE_TX_CMP_OPCODE_RESET_SEND,
	TOE_TX_CMP_OPCODE_INVALIDATE,
	TOE_TX_CMP_OPCODE_RST_RCV,
	MAX_TOE_TX_CMP_OPCODE
};


struct toe_tx_cqe {
	__le16 icid;
	u8 opcode;
	u8 reserved;
	__le32 size;
};


struct toe_tx_page_pointer_bd {
	struct regpair next_page_addr;
	struct regpair prev_page_addr;
};


struct toe_tx_page_pointer_cqe {
	struct regpair next_page_addr;
};


struct toe_update_params {
	__le16 flags;
#define TOE_UPDATE_PARAMS_RCV_INDICATION_SIZE_CHANGED_MASK	0x1
#define TOE_UPDATE_PARAMS_RCV_INDICATION_SIZE_CHANGED_SHIFT	0
#define TOE_UPDATE_PARAMS_RESERVED_MASK				0x7FFF
#define TOE_UPDATE_PARAMS_RESERVED_SHIFT			1
	__le16 rcv_indication_size;
	__le16 reserved1[2];
};


struct toe_update_ramrod_data {
	struct tcp_update_params tcp_upd_params;
	struct toe_update_params toe_upd_params;
};

struct mstorm_toe_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define MSTORM_TOE_CONN_AG_CTX_BIT0_MASK	0x1
#define MSTORM_TOE_CONN_AG_CTX_BIT0_SHIFT	0
#define MSTORM_TOE_CONN_AG_CTX_BIT1_MASK	0x1
#define MSTORM_TOE_CONN_AG_CTX_BIT1_SHIFT	1
#define MSTORM_TOE_CONN_AG_CTX_CF0_MASK		0x3
#define MSTORM_TOE_CONN_AG_CTX_CF0_SHIFT	2
#define MSTORM_TOE_CONN_AG_CTX_CF1_MASK		0x3
#define MSTORM_TOE_CONN_AG_CTX_CF1_SHIFT	4
#define MSTORM_TOE_CONN_AG_CTX_CF2_MASK		0x3
#define MSTORM_TOE_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define MSTORM_TOE_CONN_AG_CTX_CF0EN_MASK	0x1
#define MSTORM_TOE_CONN_AG_CTX_CF0EN_SHIFT	0
#define MSTORM_TOE_CONN_AG_CTX_CF1EN_MASK	0x1
#define MSTORM_TOE_CONN_AG_CTX_CF1EN_SHIFT	1
#define MSTORM_TOE_CONN_AG_CTX_CF2EN_MASK	0x1
#define MSTORM_TOE_CONN_AG_CTX_CF2EN_SHIFT	2
#define MSTORM_TOE_CONN_AG_CTX_RULE0EN_MASK	0x1
#define MSTORM_TOE_CONN_AG_CTX_RULE0EN_SHIFT	3
#define MSTORM_TOE_CONN_AG_CTX_RULE1EN_MASK	0x1
#define MSTORM_TOE_CONN_AG_CTX_RULE1EN_SHIFT	4
#define MSTORM_TOE_CONN_AG_CTX_RULE2EN_MASK	0x1
#define MSTORM_TOE_CONN_AG_CTX_RULE2EN_SHIFT	5
#define MSTORM_TOE_CONN_AG_CTX_RULE3EN_MASK	0x1
#define MSTORM_TOE_CONN_AG_CTX_RULE3EN_SHIFT	6
#define MSTORM_TOE_CONN_AG_CTX_RULE4EN_MASK	0x1
#define MSTORM_TOE_CONN_AG_CTX_RULE4EN_SHIFT	7
	__le16 word0;
	__le16 word1;
	__le32 reg0;
	__le32 reg1;
};


struct toe_db_data {
	u8 params;
#define TOE_DB_DATA_DEST_MASK			0x3
#define TOE_DB_DATA_DEST_SHIFT			0
#define TOE_DB_DATA_AGG_CMD_MASK		0x3
#define TOE_DB_DATA_AGG_CMD_SHIFT		2
#define TOE_DB_DATA_BYPASS_EN_MASK		0x1
#define TOE_DB_DATA_BYPASS_EN_SHIFT		4
#define TOE_DB_DATA_RESERVED_MASK		0x1
#define TOE_DB_DATA_RESERVED_SHIFT		5
#define TOE_DB_DATA_AGG_VAL_SEL_MASK		0x3
#define TOE_DB_DATA_AGG_VAL_SEL_SHIFT		6
	u8 agg_flags;
	__le16 bd_prod;
};


struct rdma_close_func_ramrod_data {
	u8 cnq_start_offset;
	u8 num_cnqs;
	u8 vf_id;
	u8 vf_valid;
	u8 reserved[4];
};


struct rdma_cnq_params {
	__le16 sb_num;
	u8 sb_index;
	u8 num_pbl_pages;
	__le32 reserved;
	struct regpair pbl_base_addr;
	__le16 queue_zone_num;
	u8 reserved1[6];
};


struct rdma_create_cq_ramrod_data {
	struct regpair cq_handle;
	struct regpair pbl_addr;
	__le32 max_cqes;
	__le16 pbl_num_pages;
	__le16 dpi;
	u8 is_two_level_pbl;
	u8 cnq_id;
	u8 pbl_log_page_size;
	u8 toggle_bit;
	__le16 int_timeout;
	u8 vf_id;
	u8 flags;
#define RDMA_CREATE_CQ_RAMROD_DATA_VF_ID_VALID_MASK  0x1
#define RDMA_CREATE_CQ_RAMROD_DATA_VF_ID_VALID_SHIFT 0
#define RDMA_CREATE_CQ_RAMROD_DATA_RESERVED1_MASK    0x7F
#define RDMA_CREATE_CQ_RAMROD_DATA_RESERVED1_SHIFT   1
};


struct rdma_deregister_tid_ramrod_data {
	__le32 itid;
	__le32 reserved;
};


struct rdma_destroy_cq_output_params {
	__le16 cnq_num;
	__le16 reserved0;
	__le32 reserved1;
};


struct rdma_destroy_cq_ramrod_data {
	struct regpair output_params_addr;
};


enum rdma_event_opcode {
	RDMA_EVENT_UNUSED,
	RDMA_EVENT_FUNC_INIT,
	RDMA_EVENT_FUNC_CLOSE,
	RDMA_EVENT_REGISTER_MR,
	RDMA_EVENT_DEREGISTER_MR,
	RDMA_EVENT_CREATE_CQ,
	RDMA_EVENT_RESIZE_CQ,
	RDMA_EVENT_DESTROY_CQ,
	RDMA_EVENT_CREATE_SRQ,
	RDMA_EVENT_MODIFY_SRQ,
	RDMA_EVENT_DESTROY_SRQ,
	RDMA_EVENT_START_NAMESPACE_TRACKING,
	RDMA_EVENT_STOP_NAMESPACE_TRACKING,
	MAX_RDMA_EVENT_OPCODE
};


enum rdma_fw_return_code {
	RDMA_RETURN_OK = 0,
	RDMA_RETURN_REGISTER_MR_BAD_STATE_ERR,
	RDMA_RETURN_DEREGISTER_MR_BAD_STATE_ERR,
	RDMA_RETURN_RESIZE_CQ_ERR,
	RDMA_RETURN_NIG_DRAIN_REQ,
	RDMA_RETURN_GENERAL_ERR,
	MAX_RDMA_FW_RETURN_CODE
};


struct rdma_init_func_hdr {
	u8 cnq_start_offset;
	u8 num_cnqs;
	u8 cq_ring_mode;
	u8 vf_id;
	u8 vf_valid;
	u8 relaxed_ordering;
	__le16 first_reg_srq_id;
	__le32 reg_srq_base_addr;
	u8 flags;
#define RDMA_INIT_FUNC_HDR_SEARCHER_MODE_MASK		0x1
#define RDMA_INIT_FUNC_HDR_SEARCHER_MODE_SHIFT		0
#define RDMA_INIT_FUNC_HDR_PVRDMA_MODE_MASK		0x1
#define RDMA_INIT_FUNC_HDR_PVRDMA_MODE_SHIFT		1
#define RDMA_INIT_FUNC_HDR_DPT_MODE_MASK		0x1
#define RDMA_INIT_FUNC_HDR_DPT_MODE_SHIFT		2
#define RDMA_INIT_FUNC_HDR_RESERVED0_MASK		0x1F
#define RDMA_INIT_FUNC_HDR_RESERVED0_SHIFT		3
	u8 dpt_byte_threshold_log;
	u8 dpt_common_queue_id;
	u8 max_num_ns_log;
};


struct rdma_init_func_ramrod_data {
	struct rdma_init_func_hdr params_header;
	struct rdma_cnq_params dptq_params;
	struct rdma_cnq_params cnq_params[NUM_OF_GLOBAL_QUEUES];
};


struct rdma_namespace_tracking_ramrod_data {
	u8 name_space;
	u8 reserved[7];
};


enum rdma_ramrod_cmd_id {
	RDMA_RAMROD_UNUSED,
	RDMA_RAMROD_FUNC_INIT,
	RDMA_RAMROD_FUNC_CLOSE,
	RDMA_RAMROD_REGISTER_MR,
	RDMA_RAMROD_DEREGISTER_MR,
	RDMA_RAMROD_CREATE_CQ,
	RDMA_RAMROD_RESIZE_CQ,
	RDMA_RAMROD_DESTROY_CQ,
	RDMA_RAMROD_CREATE_SRQ,
	RDMA_RAMROD_MODIFY_SRQ,
	RDMA_RAMROD_DESTROY_SRQ,
	RDMA_RAMROD_START_NS_TRACKING,
	RDMA_RAMROD_STOP_NS_TRACKING,
	MAX_RDMA_RAMROD_CMD_ID
};


struct rdma_register_tid_ramrod_data {
	__le16 flags;
#define RDMA_REGISTER_TID_RAMROD_DATA_PAGE_SIZE_LOG_MASK	0x1F
#define RDMA_REGISTER_TID_RAMROD_DATA_PAGE_SIZE_LOG_SHIFT	0
#define RDMA_REGISTER_TID_RAMROD_DATA_TWO_LEVEL_PBL_MASK	0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_TWO_LEVEL_PBL_SHIFT	5
#define RDMA_REGISTER_TID_RAMROD_DATA_ZERO_BASED_MASK		0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_ZERO_BASED_SHIFT		6
#define RDMA_REGISTER_TID_RAMROD_DATA_PHY_MR_MASK		0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_PHY_MR_SHIFT		7
#define RDMA_REGISTER_TID_RAMROD_DATA_REMOTE_READ_MASK		0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_REMOTE_READ_SHIFT		8
#define RDMA_REGISTER_TID_RAMROD_DATA_REMOTE_WRITE_MASK		0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_REMOTE_WRITE_SHIFT	9
#define RDMA_REGISTER_TID_RAMROD_DATA_REMOTE_ATOMIC_MASK	0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_REMOTE_ATOMIC_SHIFT	10
#define RDMA_REGISTER_TID_RAMROD_DATA_LOCAL_WRITE_MASK		0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_LOCAL_WRITE_SHIFT		11
#define RDMA_REGISTER_TID_RAMROD_DATA_LOCAL_READ_MASK		0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_LOCAL_READ_SHIFT		12
#define RDMA_REGISTER_TID_RAMROD_DATA_ENABLE_MW_BIND_MASK	0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_ENABLE_MW_BIND_SHIFT	13
#define RDMA_REGISTER_TID_RAMROD_DATA_RESERVED_MASK		0x3
#define RDMA_REGISTER_TID_RAMROD_DATA_RESERVED_SHIFT		14
	u8 flags1;
#define RDMA_REGISTER_TID_RAMROD_DATA_PBL_PAGE_SIZE_LOG_MASK	0x1F
#define RDMA_REGISTER_TID_RAMROD_DATA_PBL_PAGE_SIZE_LOG_SHIFT	0
#define RDMA_REGISTER_TID_RAMROD_DATA_TID_TYPE_MASK		0x7
#define RDMA_REGISTER_TID_RAMROD_DATA_TID_TYPE_SHIFT		5
	u8 flags2;
#define RDMA_REGISTER_TID_RAMROD_DATA_DMA_MR_MASK		0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_DMA_MR_SHIFT		0
#define RDMA_REGISTER_TID_RAMROD_DATA_DIF_ON_HOST_FLG_MASK	0x1
#define RDMA_REGISTER_TID_RAMROD_DATA_DIF_ON_HOST_FLG_SHIFT	1
#define RDMA_REGISTER_TID_RAMROD_DATA_RESERVED1_MASK		0x3F
#define RDMA_REGISTER_TID_RAMROD_DATA_RESERVED1_SHIFT		2
	u8 key;
	u8 length_hi;
	u8 vf_id;
	u8 vf_valid;
	__le16 pd;
	__le16 reserved2;
	__le32 length_lo;
	__le32 itid;
	__le32 reserved3;
	struct regpair va;
	struct regpair pbl_base;
	struct regpair dif_error_addr;
	__le32 reserved4[4];
};


struct rdma_resize_cq_output_params {
	__le32 old_cq_cons;
	__le32 old_cq_prod;
};


struct rdma_resize_cq_ramrod_data {
	u8 flags;
#define RDMA_RESIZE_CQ_RAMROD_DATA_TOGGLE_BIT_MASK		0x1
#define RDMA_RESIZE_CQ_RAMROD_DATA_TOGGLE_BIT_SHIFT		0
#define RDMA_RESIZE_CQ_RAMROD_DATA_IS_TWO_LEVEL_PBL_MASK	0x1
#define RDMA_RESIZE_CQ_RAMROD_DATA_IS_TWO_LEVEL_PBL_SHIFT	1
#define RDMA_RESIZE_CQ_RAMROD_DATA_VF_ID_VALID_MASK		0x1
#define RDMA_RESIZE_CQ_RAMROD_DATA_VF_ID_VALID_SHIFT		2
#define RDMA_RESIZE_CQ_RAMROD_DATA_RESERVED_MASK		0x1F
#define RDMA_RESIZE_CQ_RAMROD_DATA_RESERVED_SHIFT		3
	u8 pbl_log_page_size;
	__le16 pbl_num_pages;
	__le32 max_cqes;
	struct regpair pbl_addr;
	struct regpair output_params_addr;
	u8 vf_id;
	u8 reserved1[7];
};


struct rdma_srq_context {
	struct regpair temp[8];
};


struct rdma_srq_create_ramrod_data {
	u8 flags;
#define RDMA_SRQ_CREATE_RAMROD_DATA_XRC_FLAG_MASK         0x1
#define RDMA_SRQ_CREATE_RAMROD_DATA_XRC_FLAG_SHIFT        0
#define RDMA_SRQ_CREATE_RAMROD_DATA_RESERVED_KEY_EN_MASK  0x1
#define RDMA_SRQ_CREATE_RAMROD_DATA_RESERVED_KEY_EN_SHIFT 1
#define RDMA_SRQ_CREATE_RAMROD_DATA_RESERVED1_MASK        0x3F
#define RDMA_SRQ_CREATE_RAMROD_DATA_RESERVED1_SHIFT       2
	u8 reserved2;
	__le16 xrc_domain;
	__le32 xrc_srq_cq_cid;
	struct regpair pbl_base_addr;
	__le16 pages_in_srq_pbl;
	__le16 pd_id;
	struct rdma_srq_id srq_id;
	__le16 page_size;
	__le16 reserved3;
	__le32 reserved4;
	struct regpair producers_addr;
};


struct rdma_srq_destroy_ramrod_data {
	struct rdma_srq_id srq_id;
	__le32 reserved;
};


struct rdma_srq_modify_ramrod_data {
	struct rdma_srq_id srq_id;
	__le32 wqe_limit;
};


enum rdma_tid_type {
	RDMA_TID_REGISTERED_MR,
	RDMA_TID_FMR,
	RDMA_TID_MW,
	MAX_RDMA_TID_TYPE
};


struct rdma_xrc_srq_context {
	struct regpair temp[9];
};

struct tstorm_rdma_task_ag_ctx {
	u8 byte0;
	u8 byte1;
	__le16 word0;
	u8 flags0;
#define TSTORM_RDMA_TASK_AG_CTX_NIBBLE0_MASK		0xF
#define TSTORM_RDMA_TASK_AG_CTX_NIBBLE0_SHIFT	0
#define TSTORM_RDMA_TASK_AG_CTX_BIT0_MASK		0x1
#define TSTORM_RDMA_TASK_AG_CTX_BIT0_SHIFT		4
#define TSTORM_RDMA_TASK_AG_CTX_BIT1_MASK		0x1
#define TSTORM_RDMA_TASK_AG_CTX_BIT1_SHIFT		5
#define TSTORM_RDMA_TASK_AG_CTX_BIT2_MASK		0x1
#define TSTORM_RDMA_TASK_AG_CTX_BIT2_SHIFT		6
#define TSTORM_RDMA_TASK_AG_CTX_BIT3_MASK		0x1
#define TSTORM_RDMA_TASK_AG_CTX_BIT3_SHIFT		7
	u8 flags1;
#define TSTORM_RDMA_TASK_AG_CTX_BIT4_MASK	0x1
#define TSTORM_RDMA_TASK_AG_CTX_BIT4_SHIFT	0
#define TSTORM_RDMA_TASK_AG_CTX_BIT5_MASK	0x1
#define TSTORM_RDMA_TASK_AG_CTX_BIT5_SHIFT	1
#define TSTORM_RDMA_TASK_AG_CTX_CF0_MASK	0x3
#define TSTORM_RDMA_TASK_AG_CTX_CF0_SHIFT	2
#define TSTORM_RDMA_TASK_AG_CTX_CF1_MASK	0x3
#define TSTORM_RDMA_TASK_AG_CTX_CF1_SHIFT	4
#define TSTORM_RDMA_TASK_AG_CTX_CF2_MASK	0x3
#define TSTORM_RDMA_TASK_AG_CTX_CF2_SHIFT	6
	u8 flags2;
#define TSTORM_RDMA_TASK_AG_CTX_CF3_MASK	0x3
#define TSTORM_RDMA_TASK_AG_CTX_CF3_SHIFT	0
#define TSTORM_RDMA_TASK_AG_CTX_CF4_MASK	0x3
#define TSTORM_RDMA_TASK_AG_CTX_CF4_SHIFT	2
#define TSTORM_RDMA_TASK_AG_CTX_CF5_MASK	0x3
#define TSTORM_RDMA_TASK_AG_CTX_CF5_SHIFT	4
#define TSTORM_RDMA_TASK_AG_CTX_CF6_MASK	0x3
#define TSTORM_RDMA_TASK_AG_CTX_CF6_SHIFT	6
	u8 flags3;
#define TSTORM_RDMA_TASK_AG_CTX_CF7_MASK	0x3
#define TSTORM_RDMA_TASK_AG_CTX_CF7_SHIFT	0
#define TSTORM_RDMA_TASK_AG_CTX_CF0EN_MASK	0x1
#define TSTORM_RDMA_TASK_AG_CTX_CF0EN_SHIFT	2
#define TSTORM_RDMA_TASK_AG_CTX_CF1EN_MASK	0x1
#define TSTORM_RDMA_TASK_AG_CTX_CF1EN_SHIFT	3
#define TSTORM_RDMA_TASK_AG_CTX_CF2EN_MASK	0x1
#define TSTORM_RDMA_TASK_AG_CTX_CF2EN_SHIFT	4
#define TSTORM_RDMA_TASK_AG_CTX_CF3EN_MASK	0x1
#define TSTORM_RDMA_TASK_AG_CTX_CF3EN_SHIFT	5
#define TSTORM_RDMA_TASK_AG_CTX_CF4EN_MASK	0x1
#define TSTORM_RDMA_TASK_AG_CTX_CF4EN_SHIFT	6
#define TSTORM_RDMA_TASK_AG_CTX_CF5EN_MASK	0x1
#define TSTORM_RDMA_TASK_AG_CTX_CF5EN_SHIFT	7
	u8 flags4;
#define TSTORM_RDMA_TASK_AG_CTX_CF6EN_MASK		0x1
#define TSTORM_RDMA_TASK_AG_CTX_CF6EN_SHIFT		0
#define TSTORM_RDMA_TASK_AG_CTX_CF7EN_MASK		0x1
#define TSTORM_RDMA_TASK_AG_CTX_CF7EN_SHIFT		1
#define TSTORM_RDMA_TASK_AG_CTX_RULE0EN_MASK		0x1
#define TSTORM_RDMA_TASK_AG_CTX_RULE0EN_SHIFT	2
#define TSTORM_RDMA_TASK_AG_CTX_RULE1EN_MASK		0x1
#define TSTORM_RDMA_TASK_AG_CTX_RULE1EN_SHIFT	3
#define TSTORM_RDMA_TASK_AG_CTX_RULE2EN_MASK		0x1
#define TSTORM_RDMA_TASK_AG_CTX_RULE2EN_SHIFT	4
#define TSTORM_RDMA_TASK_AG_CTX_RULE3EN_MASK		0x1
#define TSTORM_RDMA_TASK_AG_CTX_RULE3EN_SHIFT	5
#define TSTORM_RDMA_TASK_AG_CTX_RULE4EN_MASK		0x1
#define TSTORM_RDMA_TASK_AG_CTX_RULE4EN_SHIFT	6
#define TSTORM_RDMA_TASK_AG_CTX_RULE5EN_MASK		0x1
#define TSTORM_RDMA_TASK_AG_CTX_RULE5EN_SHIFT	7
	u8 byte2;
	__le16 word1;
	__le32 reg0;
	u8 byte3;
	u8 byte4;
	__le16 word2;
	__le16 word3;
	__le16 word4;
	__le32 reg1;
	__le32 reg2;
};

struct ustorm_rdma_conn_ag_ctx {
	u8 reserved;
	u8 byte1;
	u8 flags0;
#define USTORM_RDMA_CONN_AG_CTX_EXIST_IN_QM0_MASK	0x1
#define USTORM_RDMA_CONN_AG_CTX_EXIST_IN_QM0_SHIFT	0
#define USTORM_RDMA_CONN_AG_CTX_DIF_ERROR_REPORTED_MASK  0x1
#define USTORM_RDMA_CONN_AG_CTX_DIF_ERROR_REPORTED_SHIFT 1
#define USTORM_RDMA_CONN_AG_CTX_FLUSH_Q0_CF_MASK	0x3
#define USTORM_RDMA_CONN_AG_CTX_FLUSH_Q0_CF_SHIFT	2
#define USTORM_RDMA_CONN_AG_CTX_CF1_MASK		0x3
#define USTORM_RDMA_CONN_AG_CTX_CF1_SHIFT		4
#define USTORM_RDMA_CONN_AG_CTX_CF2_MASK		0x3
#define USTORM_RDMA_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define USTORM_RDMA_CONN_AG_CTX_CF3_MASK		0x3
#define USTORM_RDMA_CONN_AG_CTX_CF3_SHIFT		0
#define USTORM_RDMA_CONN_AG_CTX_CQ_ARM_SE_CF_MASK	0x3
#define USTORM_RDMA_CONN_AG_CTX_CQ_ARM_SE_CF_SHIFT	2
#define USTORM_RDMA_CONN_AG_CTX_CQ_ARM_CF_MASK	0x3
#define USTORM_RDMA_CONN_AG_CTX_CQ_ARM_CF_SHIFT	4
#define USTORM_RDMA_CONN_AG_CTX_CF6_MASK		0x3
#define USTORM_RDMA_CONN_AG_CTX_CF6_SHIFT		6
	u8 flags2;
#define USTORM_RDMA_CONN_AG_CTX_FLUSH_Q0_CF_EN_MASK		0x1
#define USTORM_RDMA_CONN_AG_CTX_FLUSH_Q0_CF_EN_SHIFT		0
#define USTORM_RDMA_CONN_AG_CTX_CF1EN_MASK			0x1
#define USTORM_RDMA_CONN_AG_CTX_CF1EN_SHIFT			1
#define USTORM_RDMA_CONN_AG_CTX_CF2EN_MASK			0x1
#define USTORM_RDMA_CONN_AG_CTX_CF2EN_SHIFT			2
#define USTORM_RDMA_CONN_AG_CTX_CF3EN_MASK			0x1
#define USTORM_RDMA_CONN_AG_CTX_CF3EN_SHIFT			3
#define USTORM_RDMA_CONN_AG_CTX_CQ_ARM_SE_CF_EN_MASK		0x1
#define USTORM_RDMA_CONN_AG_CTX_CQ_ARM_SE_CF_EN_SHIFT	4
#define USTORM_RDMA_CONN_AG_CTX_CQ_ARM_CF_EN_MASK		0x1
#define USTORM_RDMA_CONN_AG_CTX_CQ_ARM_CF_EN_SHIFT		5
#define USTORM_RDMA_CONN_AG_CTX_CF6EN_MASK			0x1
#define USTORM_RDMA_CONN_AG_CTX_CF6EN_SHIFT			6
#define USTORM_RDMA_CONN_AG_CTX_CQ_SE_EN_MASK		0x1
#define USTORM_RDMA_CONN_AG_CTX_CQ_SE_EN_SHIFT		7
	u8 flags3;
#define USTORM_RDMA_CONN_AG_CTX_CQ_EN_MASK		0x1
#define USTORM_RDMA_CONN_AG_CTX_CQ_EN_SHIFT		0
#define USTORM_RDMA_CONN_AG_CTX_RULE2EN_MASK		0x1
#define USTORM_RDMA_CONN_AG_CTX_RULE2EN_SHIFT	1
#define USTORM_RDMA_CONN_AG_CTX_RULE3EN_MASK		0x1
#define USTORM_RDMA_CONN_AG_CTX_RULE3EN_SHIFT	2
#define USTORM_RDMA_CONN_AG_CTX_RULE4EN_MASK		0x1
#define USTORM_RDMA_CONN_AG_CTX_RULE4EN_SHIFT	3
#define USTORM_RDMA_CONN_AG_CTX_RULE5EN_MASK		0x1
#define USTORM_RDMA_CONN_AG_CTX_RULE5EN_SHIFT	4
#define USTORM_RDMA_CONN_AG_CTX_RULE6EN_MASK		0x1
#define USTORM_RDMA_CONN_AG_CTX_RULE6EN_SHIFT	5
#define USTORM_RDMA_CONN_AG_CTX_RULE7EN_MASK		0x1
#define USTORM_RDMA_CONN_AG_CTX_RULE7EN_SHIFT	6
#define USTORM_RDMA_CONN_AG_CTX_RULE8EN_MASK		0x1
#define USTORM_RDMA_CONN_AG_CTX_RULE8EN_SHIFT	7
	u8 byte2;
	u8 nvmf_only;
	__le16 conn_dpi;
	__le16 word1;
	__le32 cq_cons;
	__le32 cq_se_prod;
	__le32 cq_prod;
	__le32 reg3;
	__le16 int_timeout;
	__le16 word3;
};

struct xstorm_roce_conn_ag_ctx {
	u8 reserved0;
	u8 state;
	u8 flags0;
#define XSTORM_ROCE_CONN_AG_CTX_EXIST_IN_QM0_MASK      0x1
#define XSTORM_ROCE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT     0
#define XSTORM_ROCE_CONN_AG_CTX_BIT1_MASK              0x1
#define XSTORM_ROCE_CONN_AG_CTX_BIT1_SHIFT             1
#define XSTORM_ROCE_CONN_AG_CTX_BIT2_MASK              0x1
#define XSTORM_ROCE_CONN_AG_CTX_BIT2_SHIFT             2
#define XSTORM_ROCE_CONN_AG_CTX_EXIST_IN_QM3_MASK      0x1
#define XSTORM_ROCE_CONN_AG_CTX_EXIST_IN_QM3_SHIFT     3
#define XSTORM_ROCE_CONN_AG_CTX_BIT4_MASK              0x1
#define XSTORM_ROCE_CONN_AG_CTX_BIT4_SHIFT             4
#define XSTORM_ROCE_CONN_AG_CTX_BIT5_MASK              0x1
#define XSTORM_ROCE_CONN_AG_CTX_BIT5_SHIFT             5
#define XSTORM_ROCE_CONN_AG_CTX_BIT6_MASK              0x1
#define XSTORM_ROCE_CONN_AG_CTX_BIT6_SHIFT             6
#define XSTORM_ROCE_CONN_AG_CTX_BIT7_MASK              0x1
#define XSTORM_ROCE_CONN_AG_CTX_BIT7_SHIFT             7
	u8 flags1;
#define XSTORM_ROCE_CONN_AG_CTX_BIT8_MASK              0x1
#define XSTORM_ROCE_CONN_AG_CTX_BIT8_SHIFT             0
#define XSTORM_ROCE_CONN_AG_CTX_BIT9_MASK              0x1
#define XSTORM_ROCE_CONN_AG_CTX_BIT9_SHIFT             1
#define XSTORM_ROCE_CONN_AG_CTX_BIT10_MASK             0x1
#define XSTORM_ROCE_CONN_AG_CTX_BIT10_SHIFT            2
#define XSTORM_ROCE_CONN_AG_CTX_BIT11_MASK             0x1
#define XSTORM_ROCE_CONN_AG_CTX_BIT11_SHIFT            3
#define XSTORM_ROCE_CONN_AG_CTX_MSDM_FLUSH_MASK        0x1
#define XSTORM_ROCE_CONN_AG_CTX_MSDM_FLUSH_SHIFT       4
#define XSTORM_ROCE_CONN_AG_CTX_MSEM_FLUSH_MASK        0x1
#define XSTORM_ROCE_CONN_AG_CTX_MSEM_FLUSH_SHIFT       5
#define XSTORM_ROCE_CONN_AG_CTX_BIT14_MASK	       0x1
#define XSTORM_ROCE_CONN_AG_CTX_BIT14_SHIFT	       6
#define XSTORM_ROCE_CONN_AG_CTX_YSTORM_FLUSH_MASK      0x1
#define XSTORM_ROCE_CONN_AG_CTX_YSTORM_FLUSH_SHIFT     7
	u8 flags2;
#define XSTORM_ROCE_CONN_AG_CTX_CF0_MASK               0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF0_SHIFT              0
#define XSTORM_ROCE_CONN_AG_CTX_CF1_MASK               0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF1_SHIFT              2
#define XSTORM_ROCE_CONN_AG_CTX_CF2_MASK               0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF2_SHIFT              4
#define XSTORM_ROCE_CONN_AG_CTX_CF3_MASK               0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF3_SHIFT              6
	u8 flags3;
#define XSTORM_ROCE_CONN_AG_CTX_CF4_MASK               0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF4_SHIFT              0
#define XSTORM_ROCE_CONN_AG_CTX_CF5_MASK               0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF5_SHIFT              2
#define XSTORM_ROCE_CONN_AG_CTX_CF6_MASK               0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF6_SHIFT              4
#define XSTORM_ROCE_CONN_AG_CTX_FLUSH_Q0_CF_MASK       0x3
#define XSTORM_ROCE_CONN_AG_CTX_FLUSH_Q0_CF_SHIFT      6
	u8 flags4;
#define XSTORM_ROCE_CONN_AG_CTX_CF8_MASK               0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF8_SHIFT              0
#define XSTORM_ROCE_CONN_AG_CTX_CF9_MASK               0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF9_SHIFT              2
#define XSTORM_ROCE_CONN_AG_CTX_CF10_MASK              0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF10_SHIFT             4
#define XSTORM_ROCE_CONN_AG_CTX_CF11_MASK              0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF11_SHIFT             6
	u8 flags5;
#define XSTORM_ROCE_CONN_AG_CTX_CF12_MASK              0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF12_SHIFT             0
#define XSTORM_ROCE_CONN_AG_CTX_CF13_MASK              0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF13_SHIFT             2
#define XSTORM_ROCE_CONN_AG_CTX_CF14_MASK              0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF14_SHIFT             4
#define XSTORM_ROCE_CONN_AG_CTX_CF15_MASK              0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF15_SHIFT             6
	u8 flags6;
#define XSTORM_ROCE_CONN_AG_CTX_CF16_MASK              0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF16_SHIFT             0
#define XSTORM_ROCE_CONN_AG_CTX_CF17_MASK              0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF17_SHIFT             2
#define XSTORM_ROCE_CONN_AG_CTX_CF18_MASK              0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF18_SHIFT             4
#define XSTORM_ROCE_CONN_AG_CTX_CF19_MASK              0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF19_SHIFT             6
	u8 flags7;
#define XSTORM_ROCE_CONN_AG_CTX_CF20_MASK              0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF20_SHIFT             0
#define XSTORM_ROCE_CONN_AG_CTX_CF21_MASK              0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF21_SHIFT             2
#define XSTORM_ROCE_CONN_AG_CTX_SLOW_PATH_MASK         0x3
#define XSTORM_ROCE_CONN_AG_CTX_SLOW_PATH_SHIFT        4
#define XSTORM_ROCE_CONN_AG_CTX_CF0EN_MASK             0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF0EN_SHIFT            6
#define XSTORM_ROCE_CONN_AG_CTX_CF1EN_MASK             0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF1EN_SHIFT            7
	u8 flags8;
#define XSTORM_ROCE_CONN_AG_CTX_CF2EN_MASK             0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF2EN_SHIFT            0
#define XSTORM_ROCE_CONN_AG_CTX_CF3EN_MASK             0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF3EN_SHIFT            1
#define XSTORM_ROCE_CONN_AG_CTX_CF4EN_MASK             0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF4EN_SHIFT            2
#define XSTORM_ROCE_CONN_AG_CTX_CF5EN_MASK             0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF5EN_SHIFT            3
#define XSTORM_ROCE_CONN_AG_CTX_CF6EN_MASK             0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF6EN_SHIFT            4
#define XSTORM_ROCE_CONN_AG_CTX_FLUSH_Q0_CF_EN_MASK    0x1
#define XSTORM_ROCE_CONN_AG_CTX_FLUSH_Q0_CF_EN_SHIFT   5
#define XSTORM_ROCE_CONN_AG_CTX_CF8EN_MASK             0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF8EN_SHIFT            6
#define XSTORM_ROCE_CONN_AG_CTX_CF9EN_MASK             0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF9EN_SHIFT            7
	u8 flags9;
#define XSTORM_ROCE_CONN_AG_CTX_CF10EN_MASK            0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF10EN_SHIFT           0
#define XSTORM_ROCE_CONN_AG_CTX_CF11EN_MASK            0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF11EN_SHIFT           1
#define XSTORM_ROCE_CONN_AG_CTX_CF12EN_MASK            0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF12EN_SHIFT           2
#define XSTORM_ROCE_CONN_AG_CTX_CF13EN_MASK            0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF13EN_SHIFT           3
#define XSTORM_ROCE_CONN_AG_CTX_CF14EN_MASK            0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF14EN_SHIFT           4
#define XSTORM_ROCE_CONN_AG_CTX_CF15EN_MASK            0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF15EN_SHIFT           5
#define XSTORM_ROCE_CONN_AG_CTX_CF16EN_MASK            0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF16EN_SHIFT           6
#define XSTORM_ROCE_CONN_AG_CTX_CF17EN_MASK            0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF17EN_SHIFT           7
	u8 flags10;
#define XSTORM_ROCE_CONN_AG_CTX_CF18EN_MASK            0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF18EN_SHIFT           0
#define XSTORM_ROCE_CONN_AG_CTX_CF19EN_MASK            0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF19EN_SHIFT           1
#define XSTORM_ROCE_CONN_AG_CTX_CF20EN_MASK            0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF20EN_SHIFT           2
#define XSTORM_ROCE_CONN_AG_CTX_CF21EN_MASK            0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF21EN_SHIFT           3
#define XSTORM_ROCE_CONN_AG_CTX_SLOW_PATH_EN_MASK      0x1
#define XSTORM_ROCE_CONN_AG_CTX_SLOW_PATH_EN_SHIFT     4
#define XSTORM_ROCE_CONN_AG_CTX_CF23EN_MASK            0x1
#define XSTORM_ROCE_CONN_AG_CTX_CF23EN_SHIFT           5
#define XSTORM_ROCE_CONN_AG_CTX_RULE0EN_MASK           0x1
#define XSTORM_ROCE_CONN_AG_CTX_RULE0EN_SHIFT          6
#define XSTORM_ROCE_CONN_AG_CTX_RULE1EN_MASK           0x1
#define XSTORM_ROCE_CONN_AG_CTX_RULE1EN_SHIFT          7
	u8 flags11;
#define XSTORM_ROCE_CONN_AG_CTX_RULE2EN_MASK           0x1
#define XSTORM_ROCE_CONN_AG_CTX_RULE2EN_SHIFT          0
#define XSTORM_ROCE_CONN_AG_CTX_RULE3EN_MASK           0x1
#define XSTORM_ROCE_CONN_AG_CTX_RULE3EN_SHIFT          1
#define XSTORM_ROCE_CONN_AG_CTX_RULE4EN_MASK           0x1
#define XSTORM_ROCE_CONN_AG_CTX_RULE4EN_SHIFT          2
#define XSTORM_ROCE_CONN_AG_CTX_RULE5EN_MASK           0x1
#define XSTORM_ROCE_CONN_AG_CTX_RULE5EN_SHIFT          3
#define XSTORM_ROCE_CONN_AG_CTX_RULE6EN_MASK           0x1
#define XSTORM_ROCE_CONN_AG_CTX_RULE6EN_SHIFT          4
#define XSTORM_ROCE_CONN_AG_CTX_RULE7EN_MASK           0x1
#define XSTORM_ROCE_CONN_AG_CTX_RULE7EN_SHIFT          5
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED1_MASK      0x1
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED1_SHIFT     6
#define XSTORM_ROCE_CONN_AG_CTX_RULE9EN_MASK           0x1
#define XSTORM_ROCE_CONN_AG_CTX_RULE9EN_SHIFT          7
	u8 flags12;
#define XSTORM_ROCE_CONN_AG_CTX_RULE10EN_MASK          0x1
#define XSTORM_ROCE_CONN_AG_CTX_RULE10EN_SHIFT         0
#define XSTORM_ROCE_CONN_AG_CTX_RULE11EN_MASK          0x1
#define XSTORM_ROCE_CONN_AG_CTX_RULE11EN_SHIFT         1
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED2_MASK      0x1
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED2_SHIFT     2
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED3_MASK      0x1
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED3_SHIFT     3
#define XSTORM_ROCE_CONN_AG_CTX_RULE14EN_MASK          0x1
#define XSTORM_ROCE_CONN_AG_CTX_RULE14EN_SHIFT         4
#define XSTORM_ROCE_CONN_AG_CTX_RULE15EN_MASK          0x1
#define XSTORM_ROCE_CONN_AG_CTX_RULE15EN_SHIFT         5
#define XSTORM_ROCE_CONN_AG_CTX_RULE16EN_MASK          0x1
#define XSTORM_ROCE_CONN_AG_CTX_RULE16EN_SHIFT         6
#define XSTORM_ROCE_CONN_AG_CTX_RULE17EN_MASK          0x1
#define XSTORM_ROCE_CONN_AG_CTX_RULE17EN_SHIFT         7
	u8 flags13;
#define XSTORM_ROCE_CONN_AG_CTX_RULE18EN_MASK          0x1
#define XSTORM_ROCE_CONN_AG_CTX_RULE18EN_SHIFT         0
#define XSTORM_ROCE_CONN_AG_CTX_RULE19EN_MASK          0x1
#define XSTORM_ROCE_CONN_AG_CTX_RULE19EN_SHIFT         1
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED4_MASK      0x1
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED4_SHIFT     2
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED5_MASK      0x1
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED5_SHIFT     3
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED6_MASK      0x1
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED6_SHIFT     4
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED7_MASK      0x1
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED7_SHIFT     5
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED8_MASK      0x1
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED8_SHIFT     6
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED9_MASK      0x1
#define XSTORM_ROCE_CONN_AG_CTX_A0_RESERVED9_SHIFT     7
	u8 flags14;
#define XSTORM_ROCE_CONN_AG_CTX_MIGRATION_MASK         0x1
#define XSTORM_ROCE_CONN_AG_CTX_MIGRATION_SHIFT        0
#define XSTORM_ROCE_CONN_AG_CTX_BIT17_MASK             0x1
#define XSTORM_ROCE_CONN_AG_CTX_BIT17_SHIFT            1
#define XSTORM_ROCE_CONN_AG_CTX_DPM_PORT_NUM_MASK      0x3
#define XSTORM_ROCE_CONN_AG_CTX_DPM_PORT_NUM_SHIFT     2
#define XSTORM_ROCE_CONN_AG_CTX_RESERVED_MASK          0x1
#define XSTORM_ROCE_CONN_AG_CTX_RESERVED_SHIFT         4
#define XSTORM_ROCE_CONN_AG_CTX_ROCE_EDPM_ENABLE_MASK  0x1
#define XSTORM_ROCE_CONN_AG_CTX_ROCE_EDPM_ENABLE_SHIFT 5
#define XSTORM_ROCE_CONN_AG_CTX_CF23_MASK              0x3
#define XSTORM_ROCE_CONN_AG_CTX_CF23_SHIFT             6
	u8 byte2;
	__le16 physical_q0;
	__le16 word1;
	__le16 word2;
	__le16 word3;
	__le16 word4;
	__le16 word5;
	__le16 conn_dpi;
	u8 byte3;
	u8 byte4;
	u8 byte5;
	u8 byte6;
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 snd_nxt_psn;
	__le32 reg4;
	__le32 reg5;
	__le32 reg6;
};

struct tstorm_roce_conn_ag_ctx {
	u8 reserved0;
	u8 byte1;
	u8 flags0;
#define TSTORM_ROCE_CONN_AG_CTX_EXIST_IN_QM0_MASK          0x1
#define TSTORM_ROCE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT         0
#define TSTORM_ROCE_CONN_AG_CTX_BIT1_MASK                  0x1
#define TSTORM_ROCE_CONN_AG_CTX_BIT1_SHIFT                 1
#define TSTORM_ROCE_CONN_AG_CTX_BIT2_MASK                  0x1
#define TSTORM_ROCE_CONN_AG_CTX_BIT2_SHIFT                 2
#define TSTORM_ROCE_CONN_AG_CTX_BIT3_MASK                  0x1
#define TSTORM_ROCE_CONN_AG_CTX_BIT3_SHIFT                 3
#define TSTORM_ROCE_CONN_AG_CTX_BIT4_MASK                  0x1
#define TSTORM_ROCE_CONN_AG_CTX_BIT4_SHIFT                 4
#define TSTORM_ROCE_CONN_AG_CTX_BIT5_MASK                  0x1
#define TSTORM_ROCE_CONN_AG_CTX_BIT5_SHIFT                 5
#define TSTORM_ROCE_CONN_AG_CTX_CF0_MASK                   0x3
#define TSTORM_ROCE_CONN_AG_CTX_CF0_SHIFT                  6
	u8 flags1;
#define TSTORM_ROCE_CONN_AG_CTX_MSTORM_FLUSH_CF_MASK       0x3
#define TSTORM_ROCE_CONN_AG_CTX_MSTORM_FLUSH_CF_SHIFT      0
#define TSTORM_ROCE_CONN_AG_CTX_CF2_MASK                   0x3
#define TSTORM_ROCE_CONN_AG_CTX_CF2_SHIFT                  2
#define TSTORM_ROCE_CONN_AG_CTX_TIMER_STOP_ALL_CF_MASK     0x3
#define TSTORM_ROCE_CONN_AG_CTX_TIMER_STOP_ALL_CF_SHIFT    4
#define TSTORM_ROCE_CONN_AG_CTX_FLUSH_Q0_CF_MASK           0x3
#define TSTORM_ROCE_CONN_AG_CTX_FLUSH_Q0_CF_SHIFT          6
	u8 flags2;
#define TSTORM_ROCE_CONN_AG_CTX_CF5_MASK                   0x3
#define TSTORM_ROCE_CONN_AG_CTX_CF5_SHIFT                  0
#define TSTORM_ROCE_CONN_AG_CTX_CF6_MASK                   0x3
#define TSTORM_ROCE_CONN_AG_CTX_CF6_SHIFT                  2
#define TSTORM_ROCE_CONN_AG_CTX_CF7_MASK                   0x3
#define TSTORM_ROCE_CONN_AG_CTX_CF7_SHIFT                  4
#define TSTORM_ROCE_CONN_AG_CTX_CF8_MASK                   0x3
#define TSTORM_ROCE_CONN_AG_CTX_CF8_SHIFT                  6
	u8 flags3;
#define TSTORM_ROCE_CONN_AG_CTX_CF9_MASK                   0x3
#define TSTORM_ROCE_CONN_AG_CTX_CF9_SHIFT                  0
#define TSTORM_ROCE_CONN_AG_CTX_CF10_MASK                  0x3
#define TSTORM_ROCE_CONN_AG_CTX_CF10_SHIFT                 2
#define TSTORM_ROCE_CONN_AG_CTX_CF0EN_MASK                 0x1
#define TSTORM_ROCE_CONN_AG_CTX_CF0EN_SHIFT                4
#define TSTORM_ROCE_CONN_AG_CTX_MSTORM_FLUSH_CF_EN_MASK    0x1
#define TSTORM_ROCE_CONN_AG_CTX_MSTORM_FLUSH_CF_EN_SHIFT   5
#define TSTORM_ROCE_CONN_AG_CTX_CF2EN_MASK                 0x1
#define TSTORM_ROCE_CONN_AG_CTX_CF2EN_SHIFT                6
#define TSTORM_ROCE_CONN_AG_CTX_TIMER_STOP_ALL_CF_EN_MASK  0x1
#define TSTORM_ROCE_CONN_AG_CTX_TIMER_STOP_ALL_CF_EN_SHIFT 7
	u8 flags4;
#define TSTORM_ROCE_CONN_AG_CTX_FLUSH_Q0_CF_EN_MASK        0x1
#define TSTORM_ROCE_CONN_AG_CTX_FLUSH_Q0_CF_EN_SHIFT       0
#define TSTORM_ROCE_CONN_AG_CTX_CF5EN_MASK                 0x1
#define TSTORM_ROCE_CONN_AG_CTX_CF5EN_SHIFT                1
#define TSTORM_ROCE_CONN_AG_CTX_CF6EN_MASK                 0x1
#define TSTORM_ROCE_CONN_AG_CTX_CF6EN_SHIFT                2
#define TSTORM_ROCE_CONN_AG_CTX_CF7EN_MASK                 0x1
#define TSTORM_ROCE_CONN_AG_CTX_CF7EN_SHIFT                3
#define TSTORM_ROCE_CONN_AG_CTX_CF8EN_MASK                 0x1
#define TSTORM_ROCE_CONN_AG_CTX_CF8EN_SHIFT                4
#define TSTORM_ROCE_CONN_AG_CTX_CF9EN_MASK                 0x1
#define TSTORM_ROCE_CONN_AG_CTX_CF9EN_SHIFT                5
#define TSTORM_ROCE_CONN_AG_CTX_CF10EN_MASK                0x1
#define TSTORM_ROCE_CONN_AG_CTX_CF10EN_SHIFT               6
#define TSTORM_ROCE_CONN_AG_CTX_RULE0EN_MASK               0x1
#define TSTORM_ROCE_CONN_AG_CTX_RULE0EN_SHIFT              7
	u8 flags5;
#define TSTORM_ROCE_CONN_AG_CTX_RULE1EN_MASK               0x1
#define TSTORM_ROCE_CONN_AG_CTX_RULE1EN_SHIFT              0
#define TSTORM_ROCE_CONN_AG_CTX_RULE2EN_MASK               0x1
#define TSTORM_ROCE_CONN_AG_CTX_RULE2EN_SHIFT              1
#define TSTORM_ROCE_CONN_AG_CTX_RULE3EN_MASK               0x1
#define TSTORM_ROCE_CONN_AG_CTX_RULE3EN_SHIFT              2
#define TSTORM_ROCE_CONN_AG_CTX_RULE4EN_MASK               0x1
#define TSTORM_ROCE_CONN_AG_CTX_RULE4EN_SHIFT              3
#define TSTORM_ROCE_CONN_AG_CTX_RULE5EN_MASK               0x1
#define TSTORM_ROCE_CONN_AG_CTX_RULE5EN_SHIFT              4
#define TSTORM_ROCE_CONN_AG_CTX_RULE6EN_MASK               0x1
#define TSTORM_ROCE_CONN_AG_CTX_RULE6EN_SHIFT              5
#define TSTORM_ROCE_CONN_AG_CTX_RULE7EN_MASK               0x1
#define TSTORM_ROCE_CONN_AG_CTX_RULE7EN_SHIFT              6
#define TSTORM_ROCE_CONN_AG_CTX_RULE8EN_MASK               0x1
#define TSTORM_ROCE_CONN_AG_CTX_RULE8EN_SHIFT              7
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 reg3;
	__le32 reg4;
	__le32 reg5;
	__le32 reg6;
	__le32 reg7;
	__le32 reg8;
	u8 byte2;
	u8 byte3;
	__le16 word0;
	u8 byte4;
	u8 byte5;
	__le16 word1;
	__le16 word2;
	__le16 word3;
	__le32 reg9;
	__le32 reg10;
};


struct ystorm_roce_conn_st_ctx {
	struct regpair temp[2];
};


struct pstorm_roce_conn_st_ctx {
	struct regpair temp[16];
};


struct xstorm_roce_conn_st_ctx {
	struct regpair temp[24];
};


struct tstorm_roce_conn_st_ctx {
	struct regpair temp[30];
};


struct mstorm_roce_conn_st_ctx {
	struct regpair temp[6];
};


struct ustorm_roce_conn_st_ctx {
	struct regpair temp[14];
};


struct roce_conn_context {
	struct ystorm_roce_conn_st_ctx ystorm_st_context;
	struct regpair ystorm_st_padding[2];
	struct pstorm_roce_conn_st_ctx pstorm_st_context;
	struct xstorm_roce_conn_st_ctx xstorm_st_context;
	struct xstorm_roce_conn_ag_ctx xstorm_ag_context;
	struct tstorm_roce_conn_ag_ctx tstorm_ag_context;
	struct timers_context timer_context;
	struct ustorm_rdma_conn_ag_ctx ustorm_ag_context;
	struct tstorm_roce_conn_st_ctx tstorm_st_context;
	struct regpair tstorm_st_padding[2];
	struct mstorm_roce_conn_st_ctx mstorm_st_context;
	struct regpair mstorm_st_padding[2];
	struct ustorm_roce_conn_st_ctx ustorm_st_context;
	struct regpair ustorm_st_padding[2];
};


struct roce_cqe_stats {
	__le32 req_cqe_error;
	__le32 req_remote_access_errors;
	__le32 req_remote_invalid_request;
	__le32 resp_cqe_error;
	__le32 resp_local_length_error;
	__le32 reserved;
};


struct roce_create_qp_req_ramrod_data {
	__le16 flags;
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_ROCE_FLAVOR_MASK			0x3
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_ROCE_FLAVOR_SHIFT		0
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_FMR_AND_RESERVED_EN_MASK		0x1
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_FMR_AND_RESERVED_EN_SHIFT	2
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_SIGNALED_COMP_MASK		0x1
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_SIGNALED_COMP_SHIFT		3
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_PRI_MASK				0x7
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_PRI_SHIFT			4
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_XRC_FLAG_MASK			0x1
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_XRC_FLAG_SHIFT			7
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_ERR_RETRY_CNT_MASK		0xF
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_ERR_RETRY_CNT_SHIFT		8
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_RNR_NAK_CNT_MASK			0xF
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_RNR_NAK_CNT_SHIFT		12
	u8 max_ord;
	u8 traffic_class;
	u8 hop_limit;
	u8 orq_num_pages;
	__le16 p_key;
	__le32 flow_label;
	__le32 dst_qp_id;
	__le32 ack_timeout_val;
	__le32 initial_psn;
	__le16 mtu;
	__le16 pd;
	__le16 sq_num_pages;
	__le16 low_latency_phy_queue;
	struct regpair sq_pbl_addr;
	struct regpair orq_pbl_addr;
	__le16 local_mac_addr[3];
	__le16 remote_mac_addr[3];
	__le16 vlan_id;
	__le16 udp_src_port;
	__le32 src_gid[4];
	__le32 dst_gid[4];
	__le32 cq_cid;
	struct regpair qp_handle_for_cqe;
	struct regpair qp_handle_for_async;
	u8 stats_counter_id;
	u8 vf_id;
	u8 vport_id;
	u8 flags2;
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_EDPM_MODE_MASK			0x1
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_EDPM_MODE_SHIFT			0
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_VF_ID_VALID_MASK			0x1
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_VF_ID_VALID_SHIFT		1
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_FORCE_LB_MASK			0x1
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_FORCE_LB_SHIFT			2
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_RESERVED_MASK			0x1F
#define ROCE_CREATE_QP_REQ_RAMROD_DATA_RESERVED_SHIFT			3
	u8 name_space;
	u8 reserved3[3];
	__le16 regular_latency_phy_queue;
	__le16 dpi;
};


struct roce_create_qp_resp_ramrod_data {
	__le32 flags;
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_ROCE_FLAVOR_MASK		0x3
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_ROCE_FLAVOR_SHIFT		0
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_RDMA_RD_EN_MASK			0x1
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_RDMA_RD_EN_SHIFT		2
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_RDMA_WR_EN_MASK			0x1
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_RDMA_WR_EN_SHIFT		3
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_ATOMIC_EN_MASK			0x1
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_ATOMIC_EN_SHIFT			4
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_SRQ_FLG_MASK			0x1
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_SRQ_FLG_SHIFT			5
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_E2E_FLOW_CONTROL_EN_MASK	0x1
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_E2E_FLOW_CONTROL_EN_SHIFT	6
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_RESERVED_KEY_EN_MASK		0x1
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_RESERVED_KEY_EN_SHIFT		7
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_PRI_MASK			0x7
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_PRI_SHIFT			8
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_MIN_RNR_NAK_TIMER_MASK		0x1F
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_MIN_RNR_NAK_TIMER_SHIFT		11
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_XRC_FLAG_MASK             0x1
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_XRC_FLAG_SHIFT            16
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_VF_ID_VALID_MASK	0x1
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_VF_ID_VALID_SHIFT	17
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_FORCE_LB_MASK			0x1
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_FORCE_LB_SHIFT			18
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_RESERVED_MASK			0x1FFF
#define ROCE_CREATE_QP_RESP_RAMROD_DATA_RESERVED_SHIFT			19
	__le16 xrc_domain;
	u8 max_ird;
	u8 traffic_class;
	u8 hop_limit;
	u8 irq_num_pages;
	__le16 p_key;
	__le32 flow_label;
	__le32 dst_qp_id;
	u8 stats_counter_id;
	u8 reserved1;
	__le16 mtu;
	__le32 initial_psn;
	__le16 pd;
	__le16 rq_num_pages;
	struct rdma_srq_id srq_id;
	struct regpair rq_pbl_addr;
	struct regpair irq_pbl_addr;
	__le16 local_mac_addr[3];
	__le16 remote_mac_addr[3];
	__le16 vlan_id;
	__le16 udp_src_port;
	__le32 src_gid[4];
	__le32 dst_gid[4];
	struct regpair qp_handle_for_cqe;
	struct regpair qp_handle_for_async;
	__le16 low_latency_phy_queue;
	u8 vf_id;
	u8 vport_id;
	__le32 cq_cid;
	__le16 regular_latency_phy_queue;
	__le16 dpi;
	__le32 src_qp_id;
	u8 name_space;
	u8 reserved3[3];
};


struct roce_create_suspended_qp_req_runtime_ramrod_data {
	__le32 flags;
#define ROCE_CREATE_SUSPENDED_QP_REQ_RUNTIME_RAMROD_DATA_ERR_FLG_MASK 0x1
#define ROCE_CREATE_SUSPENDED_QP_REQ_RUNTIME_RAMROD_DATA_ERR_FLG_SHIFT 0
#define ROCE_CREATE_SUSPENDED_QP_REQ_RUNTIME_RAMROD_DATA_RESERVED0_MASK \
								 0x7FFFFFFF
#define ROCE_CREATE_SUSPENDED_QP_REQ_RUNTIME_RAMROD_DATA_RESERVED0_SHIFT 1
	__le32 send_msg_psn;
	__le32 inflight_sends;
	__le32 ssn;
};


struct roce_create_suspended_qp_req_ramrod_data {
	struct roce_create_qp_req_ramrod_data qp_params;
	struct roce_create_suspended_qp_req_runtime_ramrod_data
	 qp_runtime_params;
};


struct roce_create_suspended_qp_resp_runtime_params {
	__le32 flags;
#define ROCE_CREATE_SUSPENDED_QP_RESP_RUNTIME_PARAMS_ERR_FLG_MASK 0x1
#define ROCE_CREATE_SUSPENDED_QP_RESP_RUNTIME_PARAMS_ERR_FLG_SHIFT 0
#define ROCE_CREATE_SUSPENDED_QP_RESP_RUNTIME_PARAMS_RDMA_ACTIVE_MASK 0x1
#define ROCE_CREATE_SUSPENDED_QP_RESP_RUNTIME_PARAMS_RDMA_ACTIVE_SHIFT 1
#define ROCE_CREATE_SUSPENDED_QP_RESP_RUNTIME_PARAMS_RESERVED0_MASK 0x3FFFFFFF
#define ROCE_CREATE_SUSPENDED_QP_RESP_RUNTIME_PARAMS_RESERVED0_SHIFT 2
	__le32 receive_msg_psn;
	__le32 inflight_receives;
	__le32 rmsn;
	__le32 rdma_key;
	struct regpair rdma_va;
	__le32 rdma_length;
	__le32 num_rdb_entries;
	__le32 resreved;
};


struct roce_resp_qp_rdb_entry {
	struct regpair atomic_data;
	struct regpair va;
	__le32 psn;
	__le32 rkey;
	__le32 byte_count;
	u8 op_type;
	u8 reserved[3];
};


struct roce_create_suspended_qp_resp_runtime_ramrod_data {
	struct roce_create_suspended_qp_resp_runtime_params params;
	struct roce_resp_qp_rdb_entry
	 rdb_array_entries[RDMA_MAX_IRQ_ELEMS_IN_PAGE];
};


struct roce_create_suspended_qp_resp_ramrod_data {
	struct roce_create_qp_resp_ramrod_data
	 qp_params;
	struct roce_create_suspended_qp_resp_runtime_ramrod_data
	 qp_runtime_params;
};


struct roce_create_ud_qp_ramrod_data {
	__le16 local_mac_addr[3];
	__le16 vlan_id;
	__le32 src_qp_id;
	u8 name_space;
	u8 reserved[3];
};


struct roce_dcqcn_received_stats {
	struct regpair ecn_pkt_rcv;
	struct regpair cnp_pkt_rcv;
	struct regpair cnp_pkt_reject;
};


struct roce_dcqcn_sent_stats {
	struct regpair cnp_pkt_sent;
};


struct roce_destroy_qp_req_output_params {
	__le32 cq_prod;
	__le32 reserved;
};


struct roce_destroy_qp_req_ramrod_data {
	struct regpair output_params_addr;
};


struct roce_destroy_qp_resp_output_params {
	__le32 cq_prod;
	__le32 reserved;
};


struct roce_destroy_qp_resp_ramrod_data {
	struct regpair output_params_addr;
	__le32 src_qp_id;
	__le32 reserved;
};


struct roce_destroy_ud_qp_ramrod_data {
	__le32 src_qp_id;
	__le32 reserved;
};


struct roce_error_stats {
	__le32 resp_remote_access_errors;
	__le32 reserved;
};


struct roce_events_stats {
	__le32 silent_drops;
	__le32 rnr_naks_sent;
	__le32 retransmit_count;
	__le32 icrc_error_count;
	__le32 implied_nak_seq_err;
	__le32 duplicate_request;
	__le32 local_ack_timeout_err;
	__le32 out_of_sequence;
	__le32 packet_seq_err;
	__le32 rnr_nak_retry_err;
};


enum roce_event_opcode {
	ROCE_EVENT_CREATE_QP = 13,
	ROCE_EVENT_MODIFY_QP,
	ROCE_EVENT_QUERY_QP,
	ROCE_EVENT_DESTROY_QP,
	ROCE_EVENT_CREATE_UD_QP,
	ROCE_EVENT_DESTROY_UD_QP,
	ROCE_EVENT_FUNC_UPDATE,
	ROCE_EVENT_SUSPEND_QP,
	ROCE_EVENT_QUERY_SUSPENDED_QP,
	ROCE_EVENT_CREATE_SUSPENDED_QP,
	ROCE_EVENT_RESUME_QP,
	ROCE_EVENT_SUSPEND_UD_QP,
	ROCE_EVENT_RESUME_UD_QP,
	ROCE_EVENT_CREATE_SUSPENDED_UD_QP,
	ROCE_EVENT_FLUSH_DPT_QP,
	MAX_ROCE_EVENT_OPCODE
};


struct roce_init_func_params {
	u8 ll2_queue_id;
	u8 cnp_vlan_priority;
	u8 cnp_dscp;
	u8 flags;
#define ROCE_INIT_FUNC_PARAMS_DCQCN_NP_EN_MASK		0x1
#define ROCE_INIT_FUNC_PARAMS_DCQCN_NP_EN_SHIFT		0
#define ROCE_INIT_FUNC_PARAMS_DCQCN_RP_EN_MASK		0x1
#define ROCE_INIT_FUNC_PARAMS_DCQCN_RP_EN_SHIFT		1
#define ROCE_INIT_FUNC_PARAMS_RESERVED0_MASK		0x3F
#define ROCE_INIT_FUNC_PARAMS_RESERVED0_SHIFT		2
	__le32 cnp_send_timeout;
	__le16 rl_offset;
	u8 rl_count_log;
	u8 reserved1[5];
};


struct roce_init_func_ramrod_data {
	struct rdma_init_func_ramrod_data rdma;
	struct roce_init_func_params roce;
};


struct roce_ll2_cqe_data {
	u8 name_space;
	u8 flags;
#define ROCE_LL2_CQE_DATA_QP_SUSPENDED_MASK	0x1
#define ROCE_LL2_CQE_DATA_QP_SUSPENDED_SHIFT	0
#define ROCE_LL2_CQE_DATA_RESERVED0_MASK	0x7F
#define ROCE_LL2_CQE_DATA_RESERVED0_SHIFT	1
	u8 reserved1[2];
	__le32 cid;
};


struct roce_modify_qp_req_ramrod_data {
	__le16 flags;
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_MOVE_TO_ERR_FLG_MASK		0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_MOVE_TO_ERR_FLG_SHIFT		0
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_MOVE_TO_SQD_FLG_MASK		0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_MOVE_TO_SQD_FLG_SHIFT		1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_EN_SQD_ASYNC_NOTIFY_MASK		0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_EN_SQD_ASYNC_NOTIFY_SHIFT	2
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_P_KEY_FLG_MASK			0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_P_KEY_FLG_SHIFT			3
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_ADDRESS_VECTOR_FLG_MASK		0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_ADDRESS_VECTOR_FLG_SHIFT		4
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_MAX_ORD_FLG_MASK			0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_MAX_ORD_FLG_SHIFT		5
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_RNR_NAK_CNT_FLG_MASK		0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_RNR_NAK_CNT_FLG_SHIFT		6
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_ERR_RETRY_CNT_FLG_MASK		0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_ERR_RETRY_CNT_FLG_SHIFT		7
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_ACK_TIMEOUT_FLG_MASK		0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_ACK_TIMEOUT_FLG_SHIFT		8
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_PRI_FLG_MASK			0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_PRI_FLG_SHIFT			9
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_PRI_MASK				0x7
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_PRI_SHIFT			10
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_PHYSICAL_QUEUE_FLG_MASK		0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_PHYSICAL_QUEUE_FLG_SHIFT		13
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_FORCE_LB_MASK			0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_FORCE_LB_SHIFT			14
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_RESERVED1_MASK			0x1
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_RESERVED1_SHIFT			15
	u8 fields;
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_ERR_RETRY_CNT_MASK	0xF
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_ERR_RETRY_CNT_SHIFT	0
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_RNR_NAK_CNT_MASK		0xF
#define ROCE_MODIFY_QP_REQ_RAMROD_DATA_RNR_NAK_CNT_SHIFT	4
	u8 max_ord;
	u8 traffic_class;
	u8 hop_limit;
	__le16 p_key;
	__le32 flow_label;
	__le32 ack_timeout_val;
	__le16 mtu;
	__le16 reserved2;
	__le32 reserved3[2];
	__le16 low_latency_phy_queue;
	__le16 regular_latency_phy_queue;
	__le32 src_gid[4];
	__le32 dst_gid[4];
};


struct roce_modify_qp_resp_ramrod_data {
	__le16 flags;
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_MOVE_TO_ERR_FLG_MASK		0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_MOVE_TO_ERR_FLG_SHIFT		0
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_RDMA_RD_EN_MASK			0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_RDMA_RD_EN_SHIFT		1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_RDMA_WR_EN_MASK			0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_RDMA_WR_EN_SHIFT		2
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_ATOMIC_EN_MASK			0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_ATOMIC_EN_SHIFT			3
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_P_KEY_FLG_MASK			0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_P_KEY_FLG_SHIFT			4
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_ADDRESS_VECTOR_FLG_MASK		0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_ADDRESS_VECTOR_FLG_SHIFT	5
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_MAX_IRD_FLG_MASK		0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_MAX_IRD_FLG_SHIFT		6
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_PRI_FLG_MASK			0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_PRI_FLG_SHIFT			7
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_MIN_RNR_NAK_TIMER_FLG_MASK	0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_MIN_RNR_NAK_TIMER_FLG_SHIFT	8
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_RDMA_OPS_EN_FLG_MASK		0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_RDMA_OPS_EN_FLG_SHIFT		9
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_PHYSICAL_QUEUE_FLG_MASK		0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_PHYSICAL_QUEUE_FLG_SHIFT	10
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_FORCE_LB_MASK			0x1
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_FORCE_LB_SHIFT			11
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_RESERVED1_MASK			0xF
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_RESERVED1_SHIFT			12
	u8 fields;
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_PRI_MASK		0x7
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_PRI_SHIFT		0
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_MIN_RNR_NAK_TIMER_MASK	0x1F
#define ROCE_MODIFY_QP_RESP_RAMROD_DATA_MIN_RNR_NAK_TIMER_SHIFT	3
	u8 max_ird;
	u8 traffic_class;
	u8 hop_limit;
	__le16 p_key;
	__le32 flow_label;
	__le16 mtu;
	__le16 low_latency_phy_queue;
	__le16 regular_latency_phy_queue;
	u8 reserved2[6];
	__le32 src_gid[4];
	__le32 dst_gid[4];
};


struct roce_query_qp_req_output_params {
	__le32 psn;
	__le32 flags;
#define ROCE_QUERY_QP_REQ_OUTPUT_PARAMS_ERR_FLG_MASK		0x1
#define ROCE_QUERY_QP_REQ_OUTPUT_PARAMS_ERR_FLG_SHIFT		0
#define ROCE_QUERY_QP_REQ_OUTPUT_PARAMS_SQ_DRAINING_FLG_MASK	0x1
#define ROCE_QUERY_QP_REQ_OUTPUT_PARAMS_SQ_DRAINING_FLG_SHIFT	1
#define ROCE_QUERY_QP_REQ_OUTPUT_PARAMS_RESERVED0_MASK		0x3FFFFFFF
#define ROCE_QUERY_QP_REQ_OUTPUT_PARAMS_RESERVED0_SHIFT		2
};


struct roce_query_qp_req_ramrod_data {
	struct regpair output_params_addr;
};


struct roce_query_qp_resp_output_params {
	__le32 psn;
	__le32 flags;
#define ROCE_QUERY_QP_RESP_OUTPUT_PARAMS_ERROR_FLG_MASK  0x1
#define ROCE_QUERY_QP_RESP_OUTPUT_PARAMS_ERROR_FLG_SHIFT 0
#define ROCE_QUERY_QP_RESP_OUTPUT_PARAMS_RESERVED0_MASK  0x7FFFFFFF
#define ROCE_QUERY_QP_RESP_OUTPUT_PARAMS_RESERVED0_SHIFT 1
};


struct roce_query_qp_resp_ramrod_data {
	struct regpair output_params_addr;
};


struct roce_query_suspended_qp_req_output_params {
	__le32 psn;
	__le32 flags;
#define ROCE_QUERY_SUSPENDED_QP_REQ_OUTPUT_PARAMS_ERR_FLG_MASK		0x1
#define ROCE_QUERY_SUSPENDED_QP_REQ_OUTPUT_PARAMS_ERR_FLG_SHIFT		0
#define ROCE_QUERY_SUSPENDED_QP_REQ_OUTPUT_PARAMS_RESERVED0_MASK 0x7FFFFFFF
#define ROCE_QUERY_SUSPENDED_QP_REQ_OUTPUT_PARAMS_RESERVED0_SHIFT	1
	__le32 send_msg_psn;
	__le32 inflight_sends;
	__le32 ssn;
	__le32 reserved;
};


struct roce_query_suspended_qp_req_ramrod_data {
	struct regpair output_params_addr;
};


struct roce_query_suspended_qp_resp_runtime_params {
	__le32 psn;
	__le32 flags;
#define ROCE_QUERY_SUSPENDED_QP_RESP_RUNTIME_PARAMS_ERR_FLG_MASK 0x1
#define ROCE_QUERY_SUSPENDED_QP_RESP_RUNTIME_PARAMS_ERR_FLG_SHIFT 0
#define ROCE_QUERY_SUSPENDED_QP_RESP_RUNTIME_PARAMS_RDMA_ACTIVE_MASK 0x1
#define ROCE_QUERY_SUSPENDED_QP_RESP_RUNTIME_PARAMS_RDMA_ACTIVE_SHIFT 1
#define ROCE_QUERY_SUSPENDED_QP_RESP_RUNTIME_PARAMS_RESERVED0_MASK 0x3FFFFFFF
#define ROCE_QUERY_SUSPENDED_QP_RESP_RUNTIME_PARAMS_RESERVED0_SHIFT 2
	__le32 receive_msg_psn;
	__le32 inflight_receives;
	__le32 rmsn;
	__le32 rdma_key;
	struct regpair rdma_va;
	__le32 rdma_length;
	__le32 num_rdb_entries;
};


struct roce_query_suspended_qp_resp_output_params {
	struct roce_query_suspended_qp_resp_runtime_params runtime_params;
	struct roce_resp_qp_rdb_entry
	 rdb_array_entries[RDMA_MAX_IRQ_ELEMS_IN_PAGE];
};


struct roce_query_suspended_qp_resp_ramrod_data {
	struct regpair output_params_addr;
};


enum roce_ramrod_cmd_id {
	ROCE_RAMROD_CREATE_QP = 13,
	ROCE_RAMROD_MODIFY_QP,
	ROCE_RAMROD_QUERY_QP,
	ROCE_RAMROD_DESTROY_QP,
	ROCE_RAMROD_CREATE_UD_QP,
	ROCE_RAMROD_DESTROY_UD_QP,
	ROCE_RAMROD_FUNC_UPDATE,
	ROCE_RAMROD_SUSPEND_QP,
	ROCE_RAMROD_QUERY_SUSPENDED_QP,
	ROCE_RAMROD_CREATE_SUSPENDED_QP,
	ROCE_RAMROD_RESUME_QP,
	ROCE_RAMROD_SUSPEND_UD_QP,
	ROCE_RAMROD_RESUME_UD_QP,
	ROCE_RAMROD_CREATE_SUSPENDED_UD_QP,
	ROCE_RAMROD_FLUSH_DPT_QP,
	MAX_ROCE_RAMROD_CMD_ID
};


enum roce_resp_qp_rdb_entry_type {
	ROCE_QP_RDB_ENTRY_RDMA_RESPONSE = 0,
	ROCE_QP_RDB_ENTRY_ATOMIC_RESPONSE = 1,
	ROCE_QP_RDB_ENTRY_INVALID = 2,
	MAX_ROCE_RESP_QP_RDB_ENTRY_TYPE
};


struct roce_update_func_params {
	u8 cnp_vlan_priority;
	u8 cnp_dscp;
	__le16 flags;
#define ROCE_UPDATE_FUNC_PARAMS_DCQCN_NP_EN_MASK	0x1
#define ROCE_UPDATE_FUNC_PARAMS_DCQCN_NP_EN_SHIFT	0
#define ROCE_UPDATE_FUNC_PARAMS_DCQCN_RP_EN_MASK	0x1
#define ROCE_UPDATE_FUNC_PARAMS_DCQCN_RP_EN_SHIFT	1
#define ROCE_UPDATE_FUNC_PARAMS_RESERVED0_MASK		0x3FFF
#define ROCE_UPDATE_FUNC_PARAMS_RESERVED0_SHIFT		2
	__le32 cnp_send_timeout;
};

struct xstorm_roce_conn_ag_ctx_dq_ext_ld_part {
	u8 reserved0;
	u8 state;
	u8 flags0;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_EXIST_IN_QM0_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_EXIST_IN_QM0_SHIFT	0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT1_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT1_SHIFT		1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT2_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT2_SHIFT		2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_EXIST_IN_QM3_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_EXIST_IN_QM3_SHIFT	3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT4_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT4_SHIFT		4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT5_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT5_SHIFT		5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT6_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT6_SHIFT		6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT7_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT7_SHIFT		7
	u8 flags1;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT8_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT8_SHIFT		0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT9_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT9_SHIFT		1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT10_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT10_SHIFT		2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT11_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT11_SHIFT		3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_MSDM_FLUSH_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_MSDM_FLUSH_SHIFT	4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_MSEM_FLUSH_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_MSEM_FLUSH_SHIFT	5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT14_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT14_SHIFT		6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_YSTORM_FLUSH_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_YSTORM_FLUSH_SHIFT	7
	u8 flags2;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF0_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF0_SHIFT	0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF1_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF1_SHIFT	2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF2_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF2_SHIFT	4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF3_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF3_SHIFT	6
	u8 flags3;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF4_MASK		0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF4_SHIFT		0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF5_MASK		0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF5_SHIFT		2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF6_MASK		0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF6_SHIFT		4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_FLUSH_Q0_CF_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_FLUSH_Q0_CF_SHIFT	6
	u8 flags4;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF8_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF8_SHIFT	0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF9_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF9_SHIFT	2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF10_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF10_SHIFT	4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF11_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF11_SHIFT	6
	u8 flags5;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF12_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF12_SHIFT	0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF13_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF13_SHIFT	2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF14_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF14_SHIFT	4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF15_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF15_SHIFT	6
	u8 flags6;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF16_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF16_SHIFT	0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF17_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF17_SHIFT	2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF18_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF18_SHIFT	4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF19_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF19_SHIFT	6
	u8 flags7;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF20_MASK		0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF20_SHIFT		0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF21_MASK		0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF21_SHIFT		2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_SLOW_PATH_MASK		0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_SLOW_PATH_SHIFT	4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF0EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF0EN_SHIFT		6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF1EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF1EN_SHIFT		7
	u8 flags8;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF2EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF2EN_SHIFT		0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF3EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF3EN_SHIFT		1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF4EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF4EN_SHIFT		2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF5EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF5EN_SHIFT		3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF6EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF6EN_SHIFT		4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_FLUSH_Q0_CF_EN_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_FLUSH_Q0_CF_EN_SHIFT	5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF8EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF8EN_SHIFT		6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF9EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF9EN_SHIFT		7
	u8 flags9;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF10EN_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF10EN_SHIFT	0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF11EN_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF11EN_SHIFT	1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF12EN_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF12EN_SHIFT	2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF13EN_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF13EN_SHIFT	3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF14EN_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF14EN_SHIFT	4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF15EN_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF15EN_SHIFT	5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF16EN_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF16EN_SHIFT	6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF17EN_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF17EN_SHIFT	7
	u8 flags10;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF18EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF18EN_SHIFT		0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF19EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF19EN_SHIFT		1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF20EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF20EN_SHIFT		2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF21EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF21EN_SHIFT		3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_SLOW_PATH_EN_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_SLOW_PATH_EN_SHIFT	4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF23EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF23EN_SHIFT		5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE0EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE0EN_SHIFT		6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE1EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE1EN_SHIFT		7
	u8 flags11;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE2EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE2EN_SHIFT		0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE3EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE3EN_SHIFT		1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE4EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE4EN_SHIFT		2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE5EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE5EN_SHIFT		3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE6EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE6EN_SHIFT		4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE7EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE7EN_SHIFT		5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED1_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED1_SHIFT	6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE9EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE9EN_SHIFT		7
	u8 flags12;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE10EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE10EN_SHIFT		0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE11EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE11EN_SHIFT		1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED2_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED2_SHIFT	2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED3_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED3_SHIFT	3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE14EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE14EN_SHIFT		4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE15EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE15EN_SHIFT		5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE16EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE16EN_SHIFT		6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE17EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE17EN_SHIFT		7
	u8 flags13;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE18EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE18EN_SHIFT		0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE19EN_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RULE19EN_SHIFT		1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED4_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED4_SHIFT	2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED5_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED5_SHIFT	3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED6_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED6_SHIFT	4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED7_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED7_SHIFT	5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED8_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED8_SHIFT	6
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED9_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_A0_RESERVED9_SHIFT	7
	u8 flags14;
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_MIGRATION_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_MIGRATION_SHIFT	0
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT17_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_BIT17_SHIFT		1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_DPM_PORT_NUM_MASK	0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_DPM_PORT_NUM_SHIFT	2
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RESERVED_MASK		0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_RESERVED_SHIFT		4
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_ROCE_EDPM_ENABLE_MASK	0x1
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_ROCE_EDPM_ENABLE_SHIFT	5
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF23_MASK		0x3
#define E4XSTORMROCECONNAGCTXDQEXTLDPART_CF23_SHIFT		6
	u8 byte2;
	__le16 physical_q0;
	__le16 word1;
	__le16 word2;
	__le16 word3;
	__le16 word4;
	__le16 word5;
	__le16 conn_dpi;
	u8 byte3;
	u8 byte4;
	u8 byte5;
	u8 byte6;
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 snd_nxt_psn;
	__le32 reg4;
};

struct mstorm_roce_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define MSTORM_ROCE_CONN_AG_CTX_BIT0_MASK     0x1
#define MSTORM_ROCE_CONN_AG_CTX_BIT0_SHIFT    0
#define MSTORM_ROCE_CONN_AG_CTX_BIT1_MASK     0x1
#define MSTORM_ROCE_CONN_AG_CTX_BIT1_SHIFT    1
#define MSTORM_ROCE_CONN_AG_CTX_CF0_MASK      0x3
#define MSTORM_ROCE_CONN_AG_CTX_CF0_SHIFT     2
#define MSTORM_ROCE_CONN_AG_CTX_CF1_MASK      0x3
#define MSTORM_ROCE_CONN_AG_CTX_CF1_SHIFT     4
#define MSTORM_ROCE_CONN_AG_CTX_CF2_MASK      0x3
#define MSTORM_ROCE_CONN_AG_CTX_CF2_SHIFT     6
	u8 flags1;
#define MSTORM_ROCE_CONN_AG_CTX_CF0EN_MASK    0x1
#define MSTORM_ROCE_CONN_AG_CTX_CF0EN_SHIFT   0
#define MSTORM_ROCE_CONN_AG_CTX_CF1EN_MASK    0x1
#define MSTORM_ROCE_CONN_AG_CTX_CF1EN_SHIFT   1
#define MSTORM_ROCE_CONN_AG_CTX_CF2EN_MASK    0x1
#define MSTORM_ROCE_CONN_AG_CTX_CF2EN_SHIFT   2
#define MSTORM_ROCE_CONN_AG_CTX_RULE0EN_MASK  0x1
#define MSTORM_ROCE_CONN_AG_CTX_RULE0EN_SHIFT 3
#define MSTORM_ROCE_CONN_AG_CTX_RULE1EN_MASK  0x1
#define MSTORM_ROCE_CONN_AG_CTX_RULE1EN_SHIFT 4
#define MSTORM_ROCE_CONN_AG_CTX_RULE2EN_MASK  0x1
#define MSTORM_ROCE_CONN_AG_CTX_RULE2EN_SHIFT 5
#define MSTORM_ROCE_CONN_AG_CTX_RULE3EN_MASK  0x1
#define MSTORM_ROCE_CONN_AG_CTX_RULE3EN_SHIFT 6
#define MSTORM_ROCE_CONN_AG_CTX_RULE4EN_MASK  0x1
#define MSTORM_ROCE_CONN_AG_CTX_RULE4EN_SHIFT 7
	__le16 word0;
	__le16 word1;
	__le32 reg0;
	__le32 reg1;
};

struct mstorm_roce_req_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define MSTORM_ROCE_REQ_CONN_AG_CTX_BIT0_MASK	0x1
#define MSTORM_ROCE_REQ_CONN_AG_CTX_BIT0_SHIFT	0
#define MSTORM_ROCE_REQ_CONN_AG_CTX_BIT1_MASK	0x1
#define MSTORM_ROCE_REQ_CONN_AG_CTX_BIT1_SHIFT	1
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF0_MASK		0x3
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF0_SHIFT	2
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF1_MASK		0x3
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF1_SHIFT	4
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF2_MASK		0x3
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF0EN_MASK	0x1
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF0EN_SHIFT	0
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF1EN_MASK	0x1
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF1EN_SHIFT	1
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF2EN_MASK	0x1
#define MSTORM_ROCE_REQ_CONN_AG_CTX_CF2EN_SHIFT	2
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_MASK	0x1
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_SHIFT	3
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_MASK	0x1
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_SHIFT	4
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE2EN_MASK	0x1
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE2EN_SHIFT	5
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_MASK	0x1
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_SHIFT	6
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_MASK	0x1
#define MSTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_SHIFT	7
	__le16 word0;
	__le16 word1;
	__le32 reg0;
	__le32 reg1;
};

struct mstorm_roce_resp_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define MSTORM_ROCE_RESP_CONN_AG_CTX_BIT0_MASK	0x1
#define MSTORM_ROCE_RESP_CONN_AG_CTX_BIT0_SHIFT	0
#define MSTORM_ROCE_RESP_CONN_AG_CTX_BIT1_MASK	0x1
#define MSTORM_ROCE_RESP_CONN_AG_CTX_BIT1_SHIFT	1
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF0_MASK	0x3
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF0_SHIFT	2
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF1_MASK	0x3
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF1_SHIFT	4
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF2_MASK	0x3
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_MASK	0x1
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_SHIFT	0
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF1EN_MASK	0x1
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF1EN_SHIFT	1
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF2EN_MASK	0x1
#define MSTORM_ROCE_RESP_CONN_AG_CTX_CF2EN_SHIFT	2
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_MASK	0x1
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_SHIFT	3
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_MASK	0x1
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_SHIFT	4
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_MASK	0x1
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_SHIFT	5
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_MASK	0x1
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_SHIFT	6
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_MASK	0x1
#define MSTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_SHIFT	7
	__le16 word0;
	__le16 word1;
	__le32 reg0;
	__le32 reg1;
};

struct tstorm_roce_req_conn_ag_ctx {
	u8 reserved0;
	u8 state;
	u8 flags0;
#define TSTORM_ROCE_REQ_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RX_ERROR_OCCURRED_MASK		0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RX_ERROR_OCCURRED_SHIFT		1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TX_CQE_ERROR_OCCURRED_MASK	0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TX_CQE_ERROR_OCCURRED_SHIFT	2
#define TSTORM_ROCE_REQ_CONN_AG_CTX_BIT3_MASK			0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_BIT3_SHIFT			3
#define TSTORM_ROCE_REQ_CONN_AG_CTX_MSTORM_FLUSH_MASK		0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_MSTORM_FLUSH_SHIFT		4
#define TSTORM_ROCE_REQ_CONN_AG_CTX_CACHED_ORQ_MASK			0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_CACHED_ORQ_SHIFT			5
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TIMER_CF_MASK			0x3
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TIMER_CF_SHIFT			6
	u8 flags1;
#define TSTORM_ROCE_REQ_CONN_AG_CTX_MSTORM_FLUSH_CF_MASK             0x3
#define TSTORM_ROCE_REQ_CONN_AG_CTX_MSTORM_FLUSH_CF_SHIFT            0
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_SQ_CF_MASK			0x3
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_SQ_CF_SHIFT		2
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TIMER_STOP_ALL_CF_MASK		0x3
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TIMER_STOP_ALL_CF_SHIFT		4
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_Q0_CF_MASK			0x3
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_Q0_CF_SHIFT		6
	u8 flags2;
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FORCE_COMP_CF_MASK               0x3
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FORCE_COMP_CF_SHIFT              0
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SET_TIMER_CF_MASK	0x3
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SET_TIMER_CF_SHIFT	2
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TX_ASYNC_ERROR_CF_MASK	0x3
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TX_ASYNC_ERROR_CF_SHIFT	4
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RXMIT_DONE_CF_MASK	0x3
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RXMIT_DONE_CF_SHIFT	6
	u8 flags3;
#define TSTORM_ROCE_REQ_CONN_AG_CTX_ERROR_SCAN_COMPLETED_CF_MASK	0x3
#define TSTORM_ROCE_REQ_CONN_AG_CTX_ERROR_SCAN_COMPLETED_CF_SHIFT	0
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SQ_DRAIN_COMPLETED_CF_MASK	0x3
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SQ_DRAIN_COMPLETED_CF_SHIFT	2
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TIMER_CF_EN_MASK			0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TIMER_CF_EN_SHIFT		4
#define TSTORM_ROCE_REQ_CONN_AG_CTX_MSTORM_FLUSH_CF_EN_MASK          0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_MSTORM_FLUSH_CF_EN_SHIFT         5
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_SQ_CF_EN_MASK		0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_SQ_CF_EN_SHIFT		6
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TIMER_STOP_ALL_CF_EN_MASK	0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TIMER_STOP_ALL_CF_EN_SHIFT	7
	u8 flags4;
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_Q0_CF_EN_MASK		0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_Q0_CF_EN_SHIFT		0
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FORCE_COMP_CF_EN_MASK            0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_FORCE_COMP_CF_EN_SHIFT           1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SET_TIMER_CF_EN_MASK		0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SET_TIMER_CF_EN_SHIFT		2
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TX_ASYNC_ERROR_CF_EN_MASK	0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_TX_ASYNC_ERROR_CF_EN_SHIFT	3
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RXMIT_DONE_CF_EN_MASK		0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RXMIT_DONE_CF_EN_SHIFT		4
#define TSTORM_ROCE_REQ_CONN_AG_CTX_ERROR_SCAN_COMPLETED_CF_EN_MASK	0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_ERROR_SCAN_COMPLETED_CF_EN_SHIFT	5
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SQ_DRAIN_COMPLETED_CF_EN_MASK	0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SQ_DRAIN_COMPLETED_CF_EN_SHIFT	6
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_MASK			0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_SHIFT			7
	u8 flags5;
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_MASK		0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_SHIFT		0
#define TSTORM_ROCE_REQ_CONN_AG_CTX_DIF_CNT_EN_MASK		0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_DIF_CNT_EN_SHIFT		1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_MASK		0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_SHIFT		2
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_MASK		0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_SHIFT		3
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE5EN_MASK		0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE5EN_SHIFT		4
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SND_SQ_CONS_EN_MASK	0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_SND_SQ_CONS_EN_SHIFT	5
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE7EN_MASK		0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE7EN_SHIFT		6
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE8EN_MASK		0x1
#define TSTORM_ROCE_REQ_CONN_AG_CTX_RULE8EN_SHIFT		7
	__le32 dif_rxmit_cnt;
	__le32 snd_nxt_psn;
	__le32 snd_max_psn;
	__le32 orq_prod;
	__le32 reg4;
	__le32 dif_acked_cnt;
	__le32 dif_cnt;
	__le32 reg7;
	__le32 reg8;
	u8 tx_cqe_error_type;
	u8 orq_cache_idx;
	__le16 snd_sq_cons_th;
	u8 byte4;
	u8 byte5;
	__le16 snd_sq_cons;
	__le16 conn_dpi;
	__le16 force_comp_cons;
	__le32 dif_rxmit_acked_cnt;
	__le32 reg10;
};

struct tstorm_roce_resp_conn_ag_ctx {
	u8 byte0;
	u8 state;
	u8 flags0;
#define TSTORM_ROCE_RESP_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_NOTIFY_REQUESTER_MASK	0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_NOTIFY_REQUESTER_SHIFT	1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_BIT2_MASK			0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_BIT2_SHIFT			2
#define TSTORM_ROCE_RESP_CONN_AG_CTX_BIT3_MASK			0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_BIT3_SHIFT			3
#define TSTORM_ROCE_RESP_CONN_AG_CTX_MSTORM_FLUSH_MASK		0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_MSTORM_FLUSH_SHIFT		4
#define TSTORM_ROCE_RESP_CONN_AG_CTX_BIT5_MASK			0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_BIT5_SHIFT			5
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF0_MASK			0x3
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF0_SHIFT			6
	u8 flags1;
#define TSTORM_ROCE_RESP_CONN_AG_CTX_MSTORM_FLUSH_CF_MASK            0x3
#define TSTORM_ROCE_RESP_CONN_AG_CTX_MSTORM_FLUSH_CF_SHIFT           0
#define TSTORM_ROCE_RESP_CONN_AG_CTX_TX_ERROR_CF_MASK	0x3
#define TSTORM_ROCE_RESP_CONN_AG_CTX_TX_ERROR_CF_SHIFT	2
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF3_MASK		0x3
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF3_SHIFT		4
#define TSTORM_ROCE_RESP_CONN_AG_CTX_FLUSH_Q0_CF_MASK	0x3
#define TSTORM_ROCE_RESP_CONN_AG_CTX_FLUSH_Q0_CF_SHIFT	6
	u8 flags2;
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_CF_MASK                0x3
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_CF_SHIFT               0
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF6_MASK		0x3
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF6_SHIFT		2
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF7_MASK		0x3
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF7_SHIFT		4
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF8_MASK		0x3
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF8_SHIFT		6
	u8 flags3;
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF9_MASK		0x3
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF9_SHIFT		0
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF10_MASK		0x3
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF10_SHIFT		2
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_MASK		0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_SHIFT		4
#define TSTORM_ROCE_RESP_CONN_AG_CTX_MSTORM_FLUSH_CF_EN_MASK         0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_MSTORM_FLUSH_CF_EN_SHIFT        5
#define TSTORM_ROCE_RESP_CONN_AG_CTX_TX_ERROR_CF_EN_MASK	0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_TX_ERROR_CF_EN_SHIFT	6
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF3EN_MASK		0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF3EN_SHIFT		7
	u8 flags4;
#define TSTORM_ROCE_RESP_CONN_AG_CTX_FLUSH_Q0_CF_EN_MASK		0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_FLUSH_Q0_CF_EN_SHIFT		0
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_CF_EN_MASK             0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_CF_EN_SHIFT            1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF6EN_MASK			0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF6EN_SHIFT			2
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF7EN_MASK			0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF7EN_SHIFT			3
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF8EN_MASK			0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF8EN_SHIFT			4
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF9EN_MASK			0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF9EN_SHIFT			5
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF10EN_MASK			0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_CF10EN_SHIFT			6
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_MASK			0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_SHIFT			7
	u8 flags5;
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_MASK		0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_SHIFT		0
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_MASK		0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_SHIFT		1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_MASK		0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_SHIFT		2
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_MASK		0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_SHIFT		3
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE5EN_MASK		0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE5EN_SHIFT		4
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RQ_RULE_EN_MASK		0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RQ_RULE_EN_SHIFT	5
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE7EN_MASK		0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE7EN_SHIFT		6
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE8EN_MASK		0x1
#define TSTORM_ROCE_RESP_CONN_AG_CTX_RULE8EN_SHIFT		7
	__le32 psn_and_rxmit_id_echo;
	__le32 reg1;
	__le32 reg2;
	__le32 reg3;
	__le32 reg4;
	__le32 reg5;
	__le32 reg6;
	__le32 reg7;
	__le32 reg8;
	u8 tx_async_error_type;
	u8 byte3;
	__le16 rq_cons;
	u8 byte4;
	u8 byte5;
	__le16 rq_prod;
	__le16 conn_dpi;
	__le16 irq_cons;
	__le32 reg9;
	__le32 reg10;
};

struct ustorm_roce_req_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define USTORM_ROCE_REQ_CONN_AG_CTX_BIT0_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_BIT0_SHIFT	0
#define USTORM_ROCE_REQ_CONN_AG_CTX_BIT1_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_BIT1_SHIFT	1
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF0_MASK		0x3
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF0_SHIFT	2
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF1_MASK		0x3
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF1_SHIFT	4
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF2_MASK		0x3
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF3_MASK		0x3
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF3_SHIFT	0
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF4_MASK		0x3
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF4_SHIFT	2
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF5_MASK		0x3
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF5_SHIFT	4
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF6_MASK		0x3
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF6_SHIFT	6
	u8 flags2;
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF0EN_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF0EN_SHIFT	0
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF1EN_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF1EN_SHIFT	1
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF2EN_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF2EN_SHIFT	2
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF3EN_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF3EN_SHIFT	3
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF4EN_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF4EN_SHIFT	4
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF5EN_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF5EN_SHIFT	5
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF6EN_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_CF6EN_SHIFT	6
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_SHIFT	7
	u8 flags3;
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_SHIFT	0
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE2EN_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE2EN_SHIFT	1
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_SHIFT	2
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_SHIFT	3
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE5EN_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE5EN_SHIFT	4
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE6EN_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE6EN_SHIFT	5
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE7EN_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE7EN_SHIFT	6
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE8EN_MASK	0x1
#define USTORM_ROCE_REQ_CONN_AG_CTX_RULE8EN_SHIFT	7
	u8 byte2;
	u8 byte3;
	__le16 word0;
	__le16 word1;
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 reg3;
	__le16 word2;
	__le16 word3;
};

struct ustorm_roce_resp_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define USTORM_ROCE_RESP_CONN_AG_CTX_BIT0_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_BIT0_SHIFT	0
#define USTORM_ROCE_RESP_CONN_AG_CTX_BIT1_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_BIT1_SHIFT	1
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF0_MASK	0x3
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF0_SHIFT	2
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF1_MASK	0x3
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF1_SHIFT	4
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF2_MASK	0x3
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF3_MASK	0x3
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF3_SHIFT	0
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF4_MASK	0x3
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF4_SHIFT	2
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF5_MASK	0x3
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF5_SHIFT	4
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF6_MASK	0x3
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF6_SHIFT	6
	u8 flags2;
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_SHIFT	0
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF1EN_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF1EN_SHIFT	1
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF2EN_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF2EN_SHIFT	2
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF3EN_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF3EN_SHIFT	3
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF4EN_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF4EN_SHIFT	4
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF5EN_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF5EN_SHIFT	5
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF6EN_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_CF6EN_SHIFT	6
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_SHIFT	7
	u8 flags3;
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_SHIFT	0
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_SHIFT	1
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_SHIFT	2
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_SHIFT	3
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE5EN_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE5EN_SHIFT	4
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE6EN_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE6EN_SHIFT	5
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE7EN_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE7EN_SHIFT	6
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE8EN_MASK	0x1
#define USTORM_ROCE_RESP_CONN_AG_CTX_RULE8EN_SHIFT	7
	u8 byte2;
	u8 byte3;
	__le16 word0;
	__le16 word1;
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 reg3;
	__le16 word2;
	__le16 word3;
};

struct xstorm_roce_req_conn_ag_ctx {
	u8 reserved0;
	u8 state;
	u8 flags0;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_EXIST_IN_QM0_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_EXIST_IN_QM0_SHIFT	0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED1_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED1_SHIFT		1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED2_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED2_SHIFT		2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_EXIST_IN_QM3_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_EXIST_IN_QM3_SHIFT	3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED3_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED3_SHIFT		4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED4_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED4_SHIFT		5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED5_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED5_SHIFT		6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED6_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED6_SHIFT		7
	u8 flags1;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED7_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED7_SHIFT		0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED8_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED8_SHIFT		1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_BIT10_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_BIT10_SHIFT		2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_BIT11_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_BIT11_SHIFT		3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_MSDM_FLUSH_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_MSDM_FLUSH_SHIFT		4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_MSEM_FLUSH_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_MSEM_FLUSH_SHIFT		5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_ERROR_STATE_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_ERROR_STATE_SHIFT	6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_YSTORM_FLUSH_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_YSTORM_FLUSH_SHIFT	7
	u8 flags2;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF0_MASK		0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF0_SHIFT	0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF1_MASK		0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF1_SHIFT	2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF2_MASK		0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF2_SHIFT	4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF3_MASK		0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF3_SHIFT	6
	u8 flags3;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SQ_FLUSH_CF_MASK		0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SQ_FLUSH_CF_SHIFT	0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RX_ERROR_CF_MASK		0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RX_ERROR_CF_SHIFT	2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SND_RXMIT_CF_MASK	0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SND_RXMIT_CF_SHIFT	4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_Q0_CF_MASK		0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_Q0_CF_SHIFT	6
	u8 flags4;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_DIF_ERROR_CF_MASK        0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_DIF_ERROR_CF_SHIFT       0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SCAN_SQ_FOR_COMP_CF_MASK     0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SCAN_SQ_FOR_COMP_CF_SHIFT    2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF10_MASK	0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF10_SHIFT	4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF11_MASK	0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF11_SHIFT	6
	u8 flags5;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF12_MASK		0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF12_SHIFT		0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF13_MASK		0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF13_SHIFT		2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_FMR_ENDED_CF_MASK	0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_FMR_ENDED_CF_SHIFT	4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF15_MASK		0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF15_SHIFT		6
	u8 flags6;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF16_MASK	0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF16_SHIFT	0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF17_MASK	0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF17_SHIFT	2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF18_MASK	0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF18_SHIFT	4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF19_MASK	0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF19_SHIFT	6
	u8 flags7;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF20_MASK	0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF20_SHIFT	0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF21_MASK	0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF21_SHIFT	2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SLOW_PATH_MASK	0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SLOW_PATH_SHIFT	4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF0EN_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF0EN_SHIFT	6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF1EN_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF1EN_SHIFT	7
	u8 flags8;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF2EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF2EN_SHIFT		0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF3EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF3EN_SHIFT		1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SQ_FLUSH_CF_EN_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SQ_FLUSH_CF_EN_SHIFT	2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RX_ERROR_CF_EN_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RX_ERROR_CF_EN_SHIFT	3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SND_RXMIT_CF_EN_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SND_RXMIT_CF_EN_SHIFT	4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_Q0_CF_EN_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_FLUSH_Q0_CF_EN_SHIFT	5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_DIF_ERROR_CF_EN_MASK     0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_DIF_ERROR_CF_EN_SHIFT    6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SCAN_SQ_FOR_COMP_CF_EN_MASK  0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SCAN_SQ_FOR_COMP_CF_EN_SHIFT 7
	u8 flags9;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF10EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF10EN_SHIFT		0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF11EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF11EN_SHIFT		1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF12EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF12EN_SHIFT		2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF13EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF13EN_SHIFT		3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_FME_ENDED_CF_EN_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_FME_ENDED_CF_EN_SHIFT	4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF15EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF15EN_SHIFT		5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF16EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF16EN_SHIFT		6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF17EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF17EN_SHIFT		7
	u8 flags10;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF18EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF18EN_SHIFT		0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF19EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF19EN_SHIFT		1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF20EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF20EN_SHIFT		2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF21EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF21EN_SHIFT		3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SLOW_PATH_EN_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SLOW_PATH_EN_SHIFT	4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF23EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF23EN_SHIFT		5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_SHIFT		6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_SHIFT		7
	u8 flags11;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE2EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE2EN_SHIFT		0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_SHIFT		1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_SHIFT		2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE5EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE5EN_SHIFT		3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE6EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE6EN_SHIFT		4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_E2E_CREDIT_RULE_EN_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_E2E_CREDIT_RULE_EN_SHIFT	5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED1_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED1_SHIFT	6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE9EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE9EN_SHIFT		7
	u8 flags12;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SQ_PROD_EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_SQ_PROD_EN_SHIFT		0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE11EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE11EN_SHIFT		1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED2_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED2_SHIFT	2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED3_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED3_SHIFT	3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_INV_FENCE_RULE_EN_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_INV_FENCE_RULE_EN_SHIFT	4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE15EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE15EN_SHIFT		5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_ORQ_FENCE_RULE_EN_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_ORQ_FENCE_RULE_EN_SHIFT	6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_MAX_ORD_RULE_EN_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_MAX_ORD_RULE_EN_SHIFT	7
	u8 flags13;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE18EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE18EN_SHIFT		0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE19EN_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RULE19EN_SHIFT		1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED4_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED4_SHIFT	2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED5_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED5_SHIFT	3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED6_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED6_SHIFT	4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED7_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED7_SHIFT	5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED8_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED8_SHIFT	6
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED9_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_A0_RESERVED9_SHIFT	7
	u8 flags14;
#define XSTORM_ROCE_REQ_CONN_AG_CTX_MIGRATION_FLAG_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_MIGRATION_FLAG_SHIFT	0
#define XSTORM_ROCE_REQ_CONN_AG_CTX_BIT17_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_BIT17_SHIFT		1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_DPM_PORT_NUM_MASK	0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_DPM_PORT_NUM_SHIFT	2
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED_MASK		0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_RESERVED_SHIFT		4
#define XSTORM_ROCE_REQ_CONN_AG_CTX_ROCE_EDPM_ENABLE_MASK	0x1
#define XSTORM_ROCE_REQ_CONN_AG_CTX_ROCE_EDPM_ENABLE_SHIFT	5
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF23_MASK		0x3
#define XSTORM_ROCE_REQ_CONN_AG_CTX_CF23_SHIFT		6
	u8 byte2;
	__le16 physical_q0;
	__le16 word1;
	__le16 sq_cmp_cons;
	__le16 sq_cons;
	__le16 sq_prod;
	__le16 dif_error_first_sq_cons;
	__le16 conn_dpi;
	u8 dif_error_sge_index;
	u8 byte4;
	u8 byte5;
	u8 byte6;
	__le32 lsn;
	__le32 ssn;
	__le32 snd_una_psn;
	__le32 snd_nxt_psn;
	__le32 dif_error_offset;
	__le32 orq_cons_th;
	__le32 orq_cons;
};

struct xstorm_roce_resp_conn_ag_ctx {
	u8 reserved0;
	u8 state;
	u8 flags0;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_EXIST_IN_QM0_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_EXIST_IN_QM0_SHIFT	0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED1_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED1_SHIFT		1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED2_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED2_SHIFT		2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_EXIST_IN_QM3_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_EXIST_IN_QM3_SHIFT	3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED3_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED3_SHIFT		4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED4_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED4_SHIFT		5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED5_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED5_SHIFT		6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED6_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED6_SHIFT		7
	u8 flags1;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED7_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED7_SHIFT		0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED8_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RESERVED8_SHIFT		1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT10_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT10_SHIFT		2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT11_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT11_SHIFT		3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_MSDM_FLUSH_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_MSDM_FLUSH_SHIFT	4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_MSEM_FLUSH_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_MSEM_FLUSH_SHIFT	5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_ERROR_STATE_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_ERROR_STATE_SHIFT	6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_YSTORM_FLUSH_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_YSTORM_FLUSH_SHIFT	7
	u8 flags2;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF0_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF0_SHIFT	0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF1_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF1_SHIFT	2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF2_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF2_SHIFT	4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF3_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF3_SHIFT	6
	u8 flags3;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RXMIT_CF_MASK		0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RXMIT_CF_SHIFT		0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_CF_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_CF_SHIFT	2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_FORCE_ACK_CF_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_FORCE_ACK_CF_SHIFT	4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_FLUSH_Q0_CF_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_FLUSH_Q0_CF_SHIFT	6
	u8 flags4;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF8_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF8_SHIFT	0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF9_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF9_SHIFT	2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF10_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF10_SHIFT	4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF11_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF11_SHIFT	6
	u8 flags5;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF12_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF12_SHIFT	0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF13_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF13_SHIFT	2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF14_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF14_SHIFT	4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF15_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF15_SHIFT	6
	u8 flags6;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF16_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF16_SHIFT	0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF17_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF17_SHIFT	2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF18_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF18_SHIFT	4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF19_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF19_SHIFT	6
	u8 flags7;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF20_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF20_SHIFT	0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF21_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF21_SHIFT	2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_SLOW_PATH_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_SLOW_PATH_SHIFT	4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_SHIFT	6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF1EN_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF1EN_SHIFT	7
	u8 flags8;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF2EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF2EN_SHIFT		0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF3EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF3EN_SHIFT		1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RXMIT_CF_EN_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RXMIT_CF_EN_SHIFT	2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_CF_EN_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RX_ERROR_CF_EN_SHIFT	3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_FORCE_ACK_CF_EN_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_FORCE_ACK_CF_EN_SHIFT	4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_FLUSH_Q0_CF_EN_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_FLUSH_Q0_CF_EN_SHIFT	5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF8EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF8EN_SHIFT		6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF9EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF9EN_SHIFT		7
	u8 flags9;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF10EN_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF10EN_SHIFT	0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF11EN_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF11EN_SHIFT	1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF12EN_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF12EN_SHIFT	2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF13EN_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF13EN_SHIFT	3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF14EN_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF14EN_SHIFT	4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF15EN_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF15EN_SHIFT	5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF16EN_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF16EN_SHIFT	6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF17EN_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF17EN_SHIFT	7
	u8 flags10;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF18EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF18EN_SHIFT		0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF19EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF19EN_SHIFT		1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF20EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF20EN_SHIFT		2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF21EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF21EN_SHIFT		3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_SLOW_PATH_EN_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_SLOW_PATH_EN_SHIFT	4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF23EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF23EN_SHIFT		5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_SHIFT		6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_SHIFT		7
	u8 flags11;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_SHIFT		0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_SHIFT		1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_SHIFT		2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE5EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE5EN_SHIFT		3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE6EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE6EN_SHIFT		4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE7EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE7EN_SHIFT		5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED1_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED1_SHIFT	6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE9EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE9EN_SHIFT		7
	u8 flags12;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_IRQ_PROD_RULE_EN_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_IRQ_PROD_RULE_EN_SHIFT	0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE11EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE11EN_SHIFT		1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED2_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED2_SHIFT	2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED3_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED3_SHIFT	3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE14EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE14EN_SHIFT		4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE15EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE15EN_SHIFT		5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE16EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE16EN_SHIFT		6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE17EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE17EN_SHIFT		7
	u8 flags13;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE18EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE18EN_SHIFT		0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE19EN_MASK		0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_RULE19EN_SHIFT		1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED4_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED4_SHIFT	2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED5_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED5_SHIFT	3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED6_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED6_SHIFT	4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED7_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED7_SHIFT	5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED8_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED8_SHIFT	6
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED9_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_A0_RESERVED9_SHIFT	7
	u8 flags14;
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT16_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT16_SHIFT	0
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT17_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT17_SHIFT	1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT18_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT18_SHIFT	2
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT19_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT19_SHIFT	3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT20_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT20_SHIFT	4
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT21_MASK	0x1
#define XSTORM_ROCE_RESP_CONN_AG_CTX_BIT21_SHIFT	5
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF23_MASK	0x3
#define XSTORM_ROCE_RESP_CONN_AG_CTX_CF23_SHIFT	6
	u8 byte2;
	__le16 physical_q0;
	__le16 irq_prod_shadow;
	__le16 word2;
	__le16 irq_cons;
	__le16 irq_prod;
	__le16 e5_reserved1;
	__le16 conn_dpi;
	u8 rxmit_opcode;
	u8 byte4;
	u8 byte5;
	u8 byte6;
	__le32 rxmit_psn_and_id;
	__le32 rxmit_bytes_length;
	__le32 psn;
	__le32 reg3;
	__le32 reg4;
	__le32 reg5;
	__le32 msn_and_syndrome;
};

struct ystorm_roce_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define YSTORM_ROCE_CONN_AG_CTX_BIT0_MASK     0x1
#define YSTORM_ROCE_CONN_AG_CTX_BIT0_SHIFT    0
#define YSTORM_ROCE_CONN_AG_CTX_BIT1_MASK     0x1
#define YSTORM_ROCE_CONN_AG_CTX_BIT1_SHIFT    1
#define YSTORM_ROCE_CONN_AG_CTX_CF0_MASK      0x3
#define YSTORM_ROCE_CONN_AG_CTX_CF0_SHIFT     2
#define YSTORM_ROCE_CONN_AG_CTX_CF1_MASK      0x3
#define YSTORM_ROCE_CONN_AG_CTX_CF1_SHIFT     4
#define YSTORM_ROCE_CONN_AG_CTX_CF2_MASK      0x3
#define YSTORM_ROCE_CONN_AG_CTX_CF2_SHIFT     6
	u8 flags1;
#define YSTORM_ROCE_CONN_AG_CTX_CF0EN_MASK    0x1
#define YSTORM_ROCE_CONN_AG_CTX_CF0EN_SHIFT   0
#define YSTORM_ROCE_CONN_AG_CTX_CF1EN_MASK    0x1
#define YSTORM_ROCE_CONN_AG_CTX_CF1EN_SHIFT   1
#define YSTORM_ROCE_CONN_AG_CTX_CF2EN_MASK    0x1
#define YSTORM_ROCE_CONN_AG_CTX_CF2EN_SHIFT   2
#define YSTORM_ROCE_CONN_AG_CTX_RULE0EN_MASK  0x1
#define YSTORM_ROCE_CONN_AG_CTX_RULE0EN_SHIFT 3
#define YSTORM_ROCE_CONN_AG_CTX_RULE1EN_MASK  0x1
#define YSTORM_ROCE_CONN_AG_CTX_RULE1EN_SHIFT 4
#define YSTORM_ROCE_CONN_AG_CTX_RULE2EN_MASK  0x1
#define YSTORM_ROCE_CONN_AG_CTX_RULE2EN_SHIFT 5
#define YSTORM_ROCE_CONN_AG_CTX_RULE3EN_MASK  0x1
#define YSTORM_ROCE_CONN_AG_CTX_RULE3EN_SHIFT 6
#define YSTORM_ROCE_CONN_AG_CTX_RULE4EN_MASK  0x1
#define YSTORM_ROCE_CONN_AG_CTX_RULE4EN_SHIFT 7
	u8 byte2;
	u8 byte3;
	__le16 word0;
	__le32 reg0;
	__le32 reg1;
	__le16 word1;
	__le16 word2;
	__le16 word3;
	__le16 word4;
	__le32 reg2;
	__le32 reg3;
};

struct ystorm_roce_req_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define YSTORM_ROCE_REQ_CONN_AG_CTX_BIT0_MASK	0x1
#define YSTORM_ROCE_REQ_CONN_AG_CTX_BIT0_SHIFT	0
#define YSTORM_ROCE_REQ_CONN_AG_CTX_BIT1_MASK	0x1
#define YSTORM_ROCE_REQ_CONN_AG_CTX_BIT1_SHIFT	1
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF0_MASK		0x3
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF0_SHIFT	2
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF1_MASK		0x3
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF1_SHIFT	4
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF2_MASK		0x3
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF0EN_MASK	0x1
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF0EN_SHIFT	0
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF1EN_MASK	0x1
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF1EN_SHIFT	1
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF2EN_MASK	0x1
#define YSTORM_ROCE_REQ_CONN_AG_CTX_CF2EN_SHIFT	2
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_MASK	0x1
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE0EN_SHIFT	3
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_MASK	0x1
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE1EN_SHIFT	4
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE2EN_MASK	0x1
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE2EN_SHIFT	5
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_MASK	0x1
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE3EN_SHIFT	6
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_MASK	0x1
#define YSTORM_ROCE_REQ_CONN_AG_CTX_RULE4EN_SHIFT	7
	u8 byte2;
	u8 byte3;
	__le16 word0;
	__le32 reg0;
	__le32 reg1;
	__le16 word1;
	__le16 word2;
	__le16 word3;
	__le16 word4;
	__le32 reg2;
	__le32 reg3;
};

struct ystorm_roce_resp_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define YSTORM_ROCE_RESP_CONN_AG_CTX_BIT0_MASK	0x1
#define YSTORM_ROCE_RESP_CONN_AG_CTX_BIT0_SHIFT	0
#define YSTORM_ROCE_RESP_CONN_AG_CTX_BIT1_MASK	0x1
#define YSTORM_ROCE_RESP_CONN_AG_CTX_BIT1_SHIFT	1
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF0_MASK	0x3
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF0_SHIFT	2
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF1_MASK	0x3
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF1_SHIFT	4
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF2_MASK	0x3
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_MASK	0x1
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF0EN_SHIFT	0
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF1EN_MASK	0x1
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF1EN_SHIFT	1
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF2EN_MASK	0x1
#define YSTORM_ROCE_RESP_CONN_AG_CTX_CF2EN_SHIFT	2
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_MASK	0x1
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE0EN_SHIFT	3
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_MASK	0x1
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE1EN_SHIFT	4
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_MASK	0x1
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE2EN_SHIFT	5
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_MASK	0x1
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE3EN_SHIFT	6
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_MASK	0x1
#define YSTORM_ROCE_RESP_CONN_AG_CTX_RULE4EN_SHIFT	7
	u8 byte2;
	u8 byte3;
	__le16 word0;
	__le32 reg0;
	__le32 reg1;
	__le16 word1;
	__le16 word2;
	__le16 word3;
	__le16 word4;
	__le32 reg2;
	__le32 reg3;
};


enum roce_flavor {
	PLAIN_ROCE,
	RROCE_IPV4,
	RROCE_IPV6,
	MAX_ROCE_FLAVOR
};


struct ystorm_iwarp_conn_st_ctx {
	__le32 reserved[4];
};


struct pstorm_iwarp_conn_st_ctx {
	__le32 reserved[36];
};


struct xstorm_iwarp_conn_st_ctx {
	__le32 reserved[48];
};

struct xstorm_iwarp_conn_ag_ctx {
	u8 reserved0;
	u8 state;
	u8 flags0;
#define XSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM0_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM0_SHIFT	0
#define XSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM1_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM1_SHIFT	1
#define XSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM2_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM2_SHIFT	2
#define XSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM3_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM3_SHIFT	3
#define XSTORM_IWARP_CONN_AG_CTX_BIT4_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_BIT4_SHIFT		4
#define XSTORM_IWARP_CONN_AG_CTX_RESERVED2_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_RESERVED2_SHIFT	5
#define XSTORM_IWARP_CONN_AG_CTX_BIT6_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_BIT6_SHIFT		6
#define XSTORM_IWARP_CONN_AG_CTX_BIT7_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_BIT7_SHIFT		7
	u8 flags1;
#define XSTORM_IWARP_CONN_AG_CTX_BIT8_MASK				0x1
#define XSTORM_IWARP_CONN_AG_CTX_BIT8_SHIFT				0
#define XSTORM_IWARP_CONN_AG_CTX_BIT9_MASK				0x1
#define XSTORM_IWARP_CONN_AG_CTX_BIT9_SHIFT				1
#define XSTORM_IWARP_CONN_AG_CTX_BIT10_MASK				0x1
#define XSTORM_IWARP_CONN_AG_CTX_BIT10_SHIFT				2
#define XSTORM_IWARP_CONN_AG_CTX_BIT11_MASK				0x1
#define XSTORM_IWARP_CONN_AG_CTX_BIT11_SHIFT				3
#define XSTORM_IWARP_CONN_AG_CTX_BIT12_MASK				0x1
#define XSTORM_IWARP_CONN_AG_CTX_BIT12_SHIFT				4
#define XSTORM_IWARP_CONN_AG_CTX_BIT13_MASK				0x1
#define XSTORM_IWARP_CONN_AG_CTX_BIT13_SHIFT				5
#define XSTORM_IWARP_CONN_AG_CTX_BIT14_MASK				0x1
#define XSTORM_IWARP_CONN_AG_CTX_BIT14_SHIFT				6
#define XSTORM_IWARP_CONN_AG_CTX_YSTORM_FLUSH_OR_REWIND_SND_MAX_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_YSTORM_FLUSH_OR_REWIND_SND_MAX_SHIFT 7
	u8 flags2;
#define XSTORM_IWARP_CONN_AG_CTX_CF0_MASK			0x3
#define XSTORM_IWARP_CONN_AG_CTX_CF0_SHIFT			0
#define XSTORM_IWARP_CONN_AG_CTX_CF1_MASK			0x3
#define XSTORM_IWARP_CONN_AG_CTX_CF1_SHIFT			2
#define XSTORM_IWARP_CONN_AG_CTX_CF2_MASK			0x3
#define XSTORM_IWARP_CONN_AG_CTX_CF2_SHIFT			4
#define XSTORM_IWARP_CONN_AG_CTX_TIMER_STOP_ALL_MASK		0x3
#define XSTORM_IWARP_CONN_AG_CTX_TIMER_STOP_ALL_SHIFT	6
	u8 flags3;
#define XSTORM_IWARP_CONN_AG_CTX_CF4_MASK	0x3
#define XSTORM_IWARP_CONN_AG_CTX_CF4_SHIFT	0
#define XSTORM_IWARP_CONN_AG_CTX_CF5_MASK	0x3
#define XSTORM_IWARP_CONN_AG_CTX_CF5_SHIFT	2
#define XSTORM_IWARP_CONN_AG_CTX_CF6_MASK	0x3
#define XSTORM_IWARP_CONN_AG_CTX_CF6_SHIFT	4
#define XSTORM_IWARP_CONN_AG_CTX_CF7_MASK	0x3
#define XSTORM_IWARP_CONN_AG_CTX_CF7_SHIFT	6
	u8 flags4;
#define XSTORM_IWARP_CONN_AG_CTX_CF8_MASK	0x3
#define XSTORM_IWARP_CONN_AG_CTX_CF8_SHIFT	0
#define XSTORM_IWARP_CONN_AG_CTX_CF9_MASK	0x3
#define XSTORM_IWARP_CONN_AG_CTX_CF9_SHIFT	2
#define XSTORM_IWARP_CONN_AG_CTX_CF10_MASK	0x3
#define XSTORM_IWARP_CONN_AG_CTX_CF10_SHIFT	4
#define XSTORM_IWARP_CONN_AG_CTX_CF11_MASK	0x3
#define XSTORM_IWARP_CONN_AG_CTX_CF11_SHIFT	6
	u8 flags5;
#define XSTORM_IWARP_CONN_AG_CTX_CF12_MASK		0x3
#define XSTORM_IWARP_CONN_AG_CTX_CF12_SHIFT		0
#define XSTORM_IWARP_CONN_AG_CTX_CF13_MASK		0x3
#define XSTORM_IWARP_CONN_AG_CTX_CF13_SHIFT		2
#define XSTORM_IWARP_CONN_AG_CTX_SQ_FLUSH_CF_MASK	0x3
#define XSTORM_IWARP_CONN_AG_CTX_SQ_FLUSH_CF_SHIFT	4
#define XSTORM_IWARP_CONN_AG_CTX_CF15_MASK		0x3
#define XSTORM_IWARP_CONN_AG_CTX_CF15_SHIFT		6
	u8 flags6;
#define XSTORM_IWARP_CONN_AG_CTX_MPA_OR_ERROR_WAKEUP_TRIGGER_CF_MASK	0x3
#define XSTORM_IWARP_CONN_AG_CTX_MPA_OR_ERROR_WAKEUP_TRIGGER_CF_SHIFT 0
#define XSTORM_IWARP_CONN_AG_CTX_CF17_MASK				0x3
#define XSTORM_IWARP_CONN_AG_CTX_CF17_SHIFT				2
#define XSTORM_IWARP_CONN_AG_CTX_CF18_MASK				0x3
#define XSTORM_IWARP_CONN_AG_CTX_CF18_SHIFT				4
#define XSTORM_IWARP_CONN_AG_CTX_DQ_FLUSH_MASK			0x3
#define XSTORM_IWARP_CONN_AG_CTX_DQ_FLUSH_SHIFT			6
	u8 flags7;
#define XSTORM_IWARP_CONN_AG_CTX_FLUSH_Q0_MASK	0x3
#define XSTORM_IWARP_CONN_AG_CTX_FLUSH_Q0_SHIFT	0
#define XSTORM_IWARP_CONN_AG_CTX_FLUSH_Q1_MASK	0x3
#define XSTORM_IWARP_CONN_AG_CTX_FLUSH_Q1_SHIFT	2
#define XSTORM_IWARP_CONN_AG_CTX_SLOW_PATH_MASK	0x3
#define XSTORM_IWARP_CONN_AG_CTX_SLOW_PATH_SHIFT	4
#define XSTORM_IWARP_CONN_AG_CTX_CF0EN_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_CF0EN_SHIFT		6
#define XSTORM_IWARP_CONN_AG_CTX_CF1EN_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_CF1EN_SHIFT		7
	u8 flags8;
#define XSTORM_IWARP_CONN_AG_CTX_CF2EN_MASK			0x1
#define XSTORM_IWARP_CONN_AG_CTX_CF2EN_SHIFT			0
#define XSTORM_IWARP_CONN_AG_CTX_TIMER_STOP_ALL_EN_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_TIMER_STOP_ALL_EN_SHIFT	1
#define XSTORM_IWARP_CONN_AG_CTX_CF4EN_MASK			0x1
#define XSTORM_IWARP_CONN_AG_CTX_CF4EN_SHIFT			2
#define XSTORM_IWARP_CONN_AG_CTX_CF5EN_MASK			0x1
#define XSTORM_IWARP_CONN_AG_CTX_CF5EN_SHIFT			3
#define XSTORM_IWARP_CONN_AG_CTX_CF6EN_MASK			0x1
#define XSTORM_IWARP_CONN_AG_CTX_CF6EN_SHIFT			4
#define XSTORM_IWARP_CONN_AG_CTX_CF7EN_MASK			0x1
#define XSTORM_IWARP_CONN_AG_CTX_CF7EN_SHIFT			5
#define XSTORM_IWARP_CONN_AG_CTX_CF8EN_MASK			0x1
#define XSTORM_IWARP_CONN_AG_CTX_CF8EN_SHIFT			6
#define XSTORM_IWARP_CONN_AG_CTX_CF9EN_MASK			0x1
#define XSTORM_IWARP_CONN_AG_CTX_CF9EN_SHIFT			7
	u8 flags9;
#define XSTORM_IWARP_CONN_AG_CTX_CF10EN_MASK				0x1
#define XSTORM_IWARP_CONN_AG_CTX_CF10EN_SHIFT			0
#define XSTORM_IWARP_CONN_AG_CTX_CF11EN_MASK				0x1
#define XSTORM_IWARP_CONN_AG_CTX_CF11EN_SHIFT			1
#define XSTORM_IWARP_CONN_AG_CTX_CF12EN_MASK				0x1
#define XSTORM_IWARP_CONN_AG_CTX_CF12EN_SHIFT			2
#define XSTORM_IWARP_CONN_AG_CTX_CF13EN_MASK				0x1
#define XSTORM_IWARP_CONN_AG_CTX_CF13EN_SHIFT			3
#define XSTORM_IWARP_CONN_AG_CTX_SQ_FLUSH_CF_EN_MASK			0x1
#define XSTORM_IWARP_CONN_AG_CTX_SQ_FLUSH_CF_EN_SHIFT		4
#define XSTORM_IWARP_CONN_AG_CTX_CF15EN_MASK				0x1
#define XSTORM_IWARP_CONN_AG_CTX_CF15EN_SHIFT			5
#define XSTORM_IWARP_CONN_AG_CTX_MPA_OR_ERROR_WAKEUP_TRIGGER_CF_EN_MASK 0x1
#define XSTORM_IWARP_CONN_AG_CTX_MPA_OR_ERROR_WAKEUP_TRIGGER_CF_EN_SHIFT 6
#define XSTORM_IWARP_CONN_AG_CTX_CF17EN_MASK				0x1
#define XSTORM_IWARP_CONN_AG_CTX_CF17EN_SHIFT			7
	u8 flags10;
#define XSTORM_IWARP_CONN_AG_CTX_CF18EN_MASK			0x1
#define XSTORM_IWARP_CONN_AG_CTX_CF18EN_SHIFT		0
#define XSTORM_IWARP_CONN_AG_CTX_DQ_FLUSH_EN_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_DQ_FLUSH_EN_SHIFT		1
#define XSTORM_IWARP_CONN_AG_CTX_FLUSH_Q0_EN_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT		2
#define XSTORM_IWARP_CONN_AG_CTX_FLUSH_Q1_EN_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_FLUSH_Q1_EN_SHIFT		3
#define XSTORM_IWARP_CONN_AG_CTX_SLOW_PATH_EN_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_SLOW_PATH_EN_SHIFT		4
#define XSTORM_IWARP_CONN_AG_CTX_SEND_TERMINATE_CF_EN_MASK               0x1
#define XSTORM_IWARP_CONN_AG_CTX_SEND_TERMINATE_CF_EN_SHIFT              5
#define XSTORM_IWARP_CONN_AG_CTX_RULE0EN_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_RULE0EN_SHIFT		6
#define XSTORM_IWARP_CONN_AG_CTX_MORE_TO_SEND_RULE_EN_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_MORE_TO_SEND_RULE_EN_SHIFT	7
	u8 flags11;
#define XSTORM_IWARP_CONN_AG_CTX_TX_BLOCKED_EN_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_TX_BLOCKED_EN_SHIFT	0
#define XSTORM_IWARP_CONN_AG_CTX_RULE3EN_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_RULE3EN_SHIFT	1
#define XSTORM_IWARP_CONN_AG_CTX_RESERVED3_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_RESERVED3_SHIFT	2
#define XSTORM_IWARP_CONN_AG_CTX_RULE5EN_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_RULE5EN_SHIFT	3
#define XSTORM_IWARP_CONN_AG_CTX_RULE6EN_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_RULE6EN_SHIFT	4
#define XSTORM_IWARP_CONN_AG_CTX_RULE7EN_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_RULE7EN_SHIFT	5
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED1_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED1_SHIFT	6
#define XSTORM_IWARP_CONN_AG_CTX_RULE9EN_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_RULE9EN_SHIFT	7
	u8 flags12;
#define XSTORM_IWARP_CONN_AG_CTX_SQ_NOT_EMPTY_RULE_EN_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_SQ_NOT_EMPTY_RULE_EN_SHIFT	0
#define XSTORM_IWARP_CONN_AG_CTX_RULE11EN_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_RULE11EN_SHIFT		1
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED2_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED2_SHIFT		2
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED3_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED3_SHIFT		3
#define XSTORM_IWARP_CONN_AG_CTX_SQ_FENCE_RULE_EN_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_SQ_FENCE_RULE_EN_SHIFT	4
#define XSTORM_IWARP_CONN_AG_CTX_RULE15EN_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_RULE15EN_SHIFT		5
#define XSTORM_IWARP_CONN_AG_CTX_RULE16EN_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_RULE16EN_SHIFT		6
#define XSTORM_IWARP_CONN_AG_CTX_RULE17EN_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_RULE17EN_SHIFT		7
	u8 flags13;
#define XSTORM_IWARP_CONN_AG_CTX_IRQ_NOT_EMPTY_RULE_EN_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_IRQ_NOT_EMPTY_RULE_EN_SHIFT	0
#define XSTORM_IWARP_CONN_AG_CTX_HQ_NOT_FULL_RULE_EN_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_HQ_NOT_FULL_RULE_EN_SHIFT	1
#define XSTORM_IWARP_CONN_AG_CTX_ORQ_RD_FENCE_RULE_EN_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_ORQ_RD_FENCE_RULE_EN_SHIFT	2
#define XSTORM_IWARP_CONN_AG_CTX_RULE21EN_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_RULE21EN_SHIFT		3
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED6_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED6_SHIFT		4
#define XSTORM_IWARP_CONN_AG_CTX_ORQ_NOT_FULL_RULE_EN_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_ORQ_NOT_FULL_RULE_EN_SHIFT	5
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED8_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED8_SHIFT		6
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED9_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_A0_RESERVED9_SHIFT		7
	u8 flags14;
#define XSTORM_IWARP_CONN_AG_CTX_BIT16_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_BIT16_SHIFT		0
#define XSTORM_IWARP_CONN_AG_CTX_BIT17_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_BIT17_SHIFT		1
#define XSTORM_IWARP_CONN_AG_CTX_BIT18_MASK		0x1
#define XSTORM_IWARP_CONN_AG_CTX_BIT18_SHIFT		2
#define XSTORM_IWARP_CONN_AG_CTX_E5_RESERVED1_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_E5_RESERVED1_SHIFT	3
#define XSTORM_IWARP_CONN_AG_CTX_E5_RESERVED2_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_E5_RESERVED2_SHIFT	4
#define XSTORM_IWARP_CONN_AG_CTX_E5_RESERVED3_MASK	0x1
#define XSTORM_IWARP_CONN_AG_CTX_E5_RESERVED3_SHIFT	5
#define XSTORM_IWARP_CONN_AG_CTX_SEND_TERMINATE_CF_MASK	0x3
#define XSTORM_IWARP_CONN_AG_CTX_SEND_TERMINATE_CF_SHIFT	6
	u8 byte2;
	__le16 physical_q0;
	__le16 physical_q1;
	__le16 sq_comp_cons;
	__le16 sq_tx_cons;
	__le16 sq_prod;
	__le16 word5;
	__le16 conn_dpi;
	u8 byte3;
	u8 byte4;
	u8 byte5;
	u8 byte6;
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 more_to_send_seq;
	__le32 reg4;
	__le32 rewinded_snd_max_or_term_opcode;
	__le32 rd_msn;
	__le16 irq_prod_via_msdm;
	__le16 irq_cons;
	__le16 hq_cons_th_or_mpa_data;
	__le16 hq_cons;
	__le32 atom_msn;
	__le32 orq_cons;
	__le32 orq_cons_th;
	u8 byte7;
	u8 wqe_data_pad_bytes;
	u8 max_ord;
	u8 former_hq_prod;
	u8 irq_prod_via_msem;
	u8 byte12;
	u8 max_pkt_pdu_size_lo;
	u8 max_pkt_pdu_size_hi;
	u8 byte15;
	u8 e5_reserved;
	__le16 e5_reserved4;
	__le32 reg10;
	__le32 reg11;
	__le32 shared_queue_page_addr_lo;
	__le32 shared_queue_page_addr_hi;
	__le32 reg14;
	__le32 reg15;
	__le32 reg16;
	__le32 reg17;
};

struct tstorm_iwarp_conn_ag_ctx {
	u8 reserved0;
	u8 state;
	u8 flags0;
#define TSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM0_MASK	0x1
#define TSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM0_SHIFT	0
#define TSTORM_IWARP_CONN_AG_CTX_BIT1_MASK		0x1
#define TSTORM_IWARP_CONN_AG_CTX_BIT1_SHIFT		1
#define TSTORM_IWARP_CONN_AG_CTX_BIT2_MASK		0x1
#define TSTORM_IWARP_CONN_AG_CTX_BIT2_SHIFT		2
#define TSTORM_IWARP_CONN_AG_CTX_MSTORM_FLUSH_OR_TERMINATE_SENT_MASK  0x1
#define TSTORM_IWARP_CONN_AG_CTX_MSTORM_FLUSH_OR_TERMINATE_SENT_SHIFT 3
#define TSTORM_IWARP_CONN_AG_CTX_BIT4_MASK		0x1
#define TSTORM_IWARP_CONN_AG_CTX_BIT4_SHIFT		4
#define TSTORM_IWARP_CONN_AG_CTX_CACHED_ORQ_MASK	0x1
#define TSTORM_IWARP_CONN_AG_CTX_CACHED_ORQ_SHIFT	5
#define TSTORM_IWARP_CONN_AG_CTX_CF0_MASK		0x3
#define TSTORM_IWARP_CONN_AG_CTX_CF0_SHIFT		6
	u8 flags1;
#define TSTORM_IWARP_CONN_AG_CTX_RQ_POST_CF_MASK		0x3
#define TSTORM_IWARP_CONN_AG_CTX_RQ_POST_CF_SHIFT		0
#define TSTORM_IWARP_CONN_AG_CTX_MPA_TIMEOUT_CF_MASK		0x3
#define TSTORM_IWARP_CONN_AG_CTX_MPA_TIMEOUT_CF_SHIFT	2
#define TSTORM_IWARP_CONN_AG_CTX_TIMER_STOP_ALL_MASK		0x3
#define TSTORM_IWARP_CONN_AG_CTX_TIMER_STOP_ALL_SHIFT	4
#define TSTORM_IWARP_CONN_AG_CTX_CF4_MASK			0x3
#define TSTORM_IWARP_CONN_AG_CTX_CF4_SHIFT			6
	u8 flags2;
#define TSTORM_IWARP_CONN_AG_CTX_CF5_MASK	0x3
#define TSTORM_IWARP_CONN_AG_CTX_CF5_SHIFT	0
#define TSTORM_IWARP_CONN_AG_CTX_CF6_MASK	0x3
#define TSTORM_IWARP_CONN_AG_CTX_CF6_SHIFT	2
#define TSTORM_IWARP_CONN_AG_CTX_CF7_MASK	0x3
#define TSTORM_IWARP_CONN_AG_CTX_CF7_SHIFT	4
#define TSTORM_IWARP_CONN_AG_CTX_CF8_MASK	0x3
#define TSTORM_IWARP_CONN_AG_CTX_CF8_SHIFT	6
	u8 flags3;
#define TSTORM_IWARP_CONN_AG_CTX_FLUSH_Q0_AND_TCP_HANDSHAKE_COMPLETE_MASK 0x3
#define TSTORM_IWARP_CONN_AG_CTX_FLUSH_Q0_AND_TCP_HANDSHAKE_COMPLETE_SHIFT 0
#define TSTORM_IWARP_CONN_AG_CTX_FLUSH_OR_ERROR_DETECTED_MASK	0x3
#define TSTORM_IWARP_CONN_AG_CTX_FLUSH_OR_ERROR_DETECTED_SHIFT	2
#define TSTORM_IWARP_CONN_AG_CTX_CF0EN_MASK				0x1
#define TSTORM_IWARP_CONN_AG_CTX_CF0EN_SHIFT				4
#define TSTORM_IWARP_CONN_AG_CTX_RQ_POST_CF_EN_MASK			0x1
#define TSTORM_IWARP_CONN_AG_CTX_RQ_POST_CF_EN_SHIFT			5
#define TSTORM_IWARP_CONN_AG_CTX_MPA_TIMEOUT_CF_EN_MASK		0x1
#define TSTORM_IWARP_CONN_AG_CTX_MPA_TIMEOUT_CF_EN_SHIFT		6
#define TSTORM_IWARP_CONN_AG_CTX_TIMER_STOP_ALL_EN_MASK		0x1
#define TSTORM_IWARP_CONN_AG_CTX_TIMER_STOP_ALL_EN_SHIFT		7
	u8 flags4;
#define TSTORM_IWARP_CONN_AG_CTX_CF4EN_MASK				0x1
#define TSTORM_IWARP_CONN_AG_CTX_CF4EN_SHIFT				0
#define TSTORM_IWARP_CONN_AG_CTX_CF5EN_MASK				0x1
#define TSTORM_IWARP_CONN_AG_CTX_CF5EN_SHIFT				1
#define TSTORM_IWARP_CONN_AG_CTX_CF6EN_MASK				0x1
#define TSTORM_IWARP_CONN_AG_CTX_CF6EN_SHIFT				2
#define TSTORM_IWARP_CONN_AG_CTX_CF7EN_MASK				0x1
#define TSTORM_IWARP_CONN_AG_CTX_CF7EN_SHIFT				3
#define TSTORM_IWARP_CONN_AG_CTX_CF8EN_MASK				0x1
#define TSTORM_IWARP_CONN_AG_CTX_CF8EN_SHIFT				4
#define TSTORM_IWARP_CONN_AG_CTX_FLUSH_Q0_AND_TCP_HANDSHAKE_COMPL_EN_MASK 0x1
#define	TSTORM_IWARP_CONN_AG_CTX_FLUSH_Q0_AND_TCP_HANDSHAKE_COMPL_EN_SHIFT 5
#define TSTORM_IWARP_CONN_AG_CTX_FLUSH_OR_ERROR_DETECTED_EN_MASK	0x1
#define TSTORM_IWARP_CONN_AG_CTX_FLUSH_OR_ERROR_DETECTED_EN_SHIFT	6
#define TSTORM_IWARP_CONN_AG_CTX_RULE0EN_MASK			0x1
#define TSTORM_IWARP_CONN_AG_CTX_RULE0EN_SHIFT			7
	u8 flags5;
#define TSTORM_IWARP_CONN_AG_CTX_RULE1EN_MASK		0x1
#define TSTORM_IWARP_CONN_AG_CTX_RULE1EN_SHIFT		0
#define TSTORM_IWARP_CONN_AG_CTX_RULE2EN_MASK		0x1
#define TSTORM_IWARP_CONN_AG_CTX_RULE2EN_SHIFT		1
#define TSTORM_IWARP_CONN_AG_CTX_RULE3EN_MASK		0x1
#define TSTORM_IWARP_CONN_AG_CTX_RULE3EN_SHIFT		2
#define TSTORM_IWARP_CONN_AG_CTX_RULE4EN_MASK		0x1
#define TSTORM_IWARP_CONN_AG_CTX_RULE4EN_SHIFT		3
#define TSTORM_IWARP_CONN_AG_CTX_RULE5EN_MASK		0x1
#define TSTORM_IWARP_CONN_AG_CTX_RULE5EN_SHIFT		4
#define TSTORM_IWARP_CONN_AG_CTX_SND_SQ_CONS_RULE_MASK	0x1
#define TSTORM_IWARP_CONN_AG_CTX_SND_SQ_CONS_RULE_SHIFT	5
#define TSTORM_IWARP_CONN_AG_CTX_RULE7EN_MASK		0x1
#define TSTORM_IWARP_CONN_AG_CTX_RULE7EN_SHIFT		6
#define TSTORM_IWARP_CONN_AG_CTX_RULE8EN_MASK		0x1
#define TSTORM_IWARP_CONN_AG_CTX_RULE8EN_SHIFT		7
	__le32 reg0;
	__le32 reg1;
	__le32 unaligned_nxt_seq;
	__le32 reg3;
	__le32 reg4;
	__le32 reg5;
	__le32 reg6;
	__le32 reg7;
	__le32 reg8;
	u8 orq_cache_idx;
	u8 hq_prod;
	__le16 sq_tx_cons_th;
	u8 orq_prod;
	u8 irq_cons;
	__le16 sq_tx_cons;
	__le16 conn_dpi;
	__le16 rq_prod;
	__le32 snd_seq;
	__le32 last_hq_sequence;
};


struct tstorm_iwarp_conn_st_ctx {
	__le32 reserved[60];
};


struct mstorm_iwarp_conn_st_ctx {
	__le32 reserved[32];
};


struct ustorm_iwarp_conn_st_ctx {
	struct regpair reserved[14];
};


struct iwarp_conn_context {
	struct ystorm_iwarp_conn_st_ctx ystorm_st_context;
	struct regpair ystorm_st_padding[2];
	struct pstorm_iwarp_conn_st_ctx pstorm_st_context;
	struct regpair pstorm_st_padding[2];
	struct xstorm_iwarp_conn_st_ctx xstorm_st_context;
	struct xstorm_iwarp_conn_ag_ctx xstorm_ag_context;
	struct tstorm_iwarp_conn_ag_ctx tstorm_ag_context;
	struct timers_context timer_context;
	struct ustorm_rdma_conn_ag_ctx ustorm_ag_context;
	struct tstorm_iwarp_conn_st_ctx tstorm_st_context;
	struct regpair tstorm_st_padding[2];
	struct mstorm_iwarp_conn_st_ctx mstorm_st_context;
	struct ustorm_iwarp_conn_st_ctx ustorm_st_context;
	struct regpair ustorm_st_padding[2];
};


struct iwarp_create_qp_ramrod_data {
	u8 flags;
#define IWARP_CREATE_QP_RAMROD_DATA_FMR_AND_RESERVED_EN_MASK	0x1
#define IWARP_CREATE_QP_RAMROD_DATA_FMR_AND_RESERVED_EN_SHIFT	0
#define IWARP_CREATE_QP_RAMROD_DATA_SIGNALED_COMP_MASK		0x1
#define IWARP_CREATE_QP_RAMROD_DATA_SIGNALED_COMP_SHIFT		1
#define IWARP_CREATE_QP_RAMROD_DATA_RDMA_RD_EN_MASK		0x1
#define IWARP_CREATE_QP_RAMROD_DATA_RDMA_RD_EN_SHIFT		2
#define IWARP_CREATE_QP_RAMROD_DATA_RDMA_WR_EN_MASK		0x1
#define IWARP_CREATE_QP_RAMROD_DATA_RDMA_WR_EN_SHIFT		3
#define IWARP_CREATE_QP_RAMROD_DATA_ATOMIC_EN_MASK		0x1
#define IWARP_CREATE_QP_RAMROD_DATA_ATOMIC_EN_SHIFT		4
#define IWARP_CREATE_QP_RAMROD_DATA_SRQ_FLG_MASK		0x1
#define IWARP_CREATE_QP_RAMROD_DATA_SRQ_FLG_SHIFT		5
#define IWARP_CREATE_QP_RAMROD_DATA_LOW_LATENCY_QUEUE_EN_MASK	0x1
#define IWARP_CREATE_QP_RAMROD_DATA_LOW_LATENCY_QUEUE_EN_SHIFT	6
#define IWARP_CREATE_QP_RAMROD_DATA_RESERVED0_MASK		0x1
#define IWARP_CREATE_QP_RAMROD_DATA_RESERVED0_SHIFT		7
	u8 reserved1;
	__le16 pd;
	__le16 sq_num_pages;
	__le16 rq_num_pages;
	__le32 reserved3[2];
	struct regpair qp_handle_for_cqe;
	struct rdma_srq_id srq_id;
	__le32 cq_cid_for_sq;
	__le32 cq_cid_for_rq;
	__le16 dpi;
	__le16 physical_q0;
	__le16 physical_q1;
	u8 reserved2[6];
};


enum iwarp_eqe_async_opcode {
	IWARP_EVENT_TYPE_ASYNC_CONNECT_COMPLETE,
	IWARP_EVENT_TYPE_ASYNC_ENHANCED_MPA_REPLY_ARRIVED,
	IWARP_EVENT_TYPE_ASYNC_MPA_HANDSHAKE_COMPLETE,
	IWARP_EVENT_TYPE_ASYNC_CID_CLEANED,
	IWARP_EVENT_TYPE_ASYNC_EXCEPTION_DETECTED,
	IWARP_EVENT_TYPE_ASYNC_QP_IN_ERROR_STATE,
	IWARP_EVENT_TYPE_ASYNC_CQ_OVERFLOW,
	IWARP_EVENT_TYPE_ASYNC_SRQ_LIMIT,
	IWARP_EVENT_TYPE_ASYNC_SRQ_EMPTY,
	MAX_IWARP_EQE_ASYNC_OPCODE
};

struct iwarp_eqe_data_mpa_async_completion {
	__le16 ulp_data_len;
	u8 rtr_type_sent;
	u8 reserved[5];
};

struct iwarp_eqe_data_tcp_async_completion {
	__le16 ulp_data_len;
	u8 mpa_handshake_mode;
	u8 reserved[5];
};


enum iwarp_eqe_sync_opcode {
	IWARP_EVENT_TYPE_TCP_OFFLOAD = 13,
	IWARP_EVENT_TYPE_MPA_OFFLOAD,
	IWARP_EVENT_TYPE_MPA_OFFLOAD_SEND_RTR,
	IWARP_EVENT_TYPE_CREATE_QP,
	IWARP_EVENT_TYPE_QUERY_QP,
	IWARP_EVENT_TYPE_MODIFY_QP,
	IWARP_EVENT_TYPE_DESTROY_QP,
	IWARP_EVENT_TYPE_ABORT_TCP_OFFLOAD,
	MAX_IWARP_EQE_SYNC_OPCODE
};


enum iwarp_fw_return_code {
	IWARP_CONN_ERROR_TCP_CONNECT_INVALID_PACKET = 6,
	IWARP_CONN_ERROR_TCP_CONNECTION_RST,
	IWARP_CONN_ERROR_TCP_CONNECT_TIMEOUT,
	IWARP_CONN_ERROR_MPA_ERROR_REJECT,
	IWARP_CONN_ERROR_MPA_NOT_SUPPORTED_VER,
	IWARP_CONN_ERROR_MPA_RST,
	IWARP_CONN_ERROR_MPA_FIN,
	IWARP_CONN_ERROR_MPA_RTR_MISMATCH,
	IWARP_CONN_ERROR_MPA_INSUF_IRD,
	IWARP_CONN_ERROR_MPA_INVALID_PACKET,
	IWARP_CONN_ERROR_MPA_LOCAL_ERROR,
	IWARP_CONN_ERROR_MPA_TIMEOUT,
	IWARP_CONN_ERROR_MPA_TERMINATE,
	IWARP_QP_IN_ERROR_GOOD_CLOSE,
	IWARP_QP_IN_ERROR_BAD_CLOSE,
	IWARP_EXCEPTION_DETECTED_LLP_CLOSED,
	IWARP_EXCEPTION_DETECTED_LLP_RESET,
	IWARP_EXCEPTION_DETECTED_IRQ_FULL,
	IWARP_EXCEPTION_DETECTED_RQ_EMPTY,
	IWARP_EXCEPTION_DETECTED_LLP_TIMEOUT,
	IWARP_EXCEPTION_DETECTED_REMOTE_PROTECTION_ERROR,
	IWARP_EXCEPTION_DETECTED_CQ_OVERFLOW,
	IWARP_EXCEPTION_DETECTED_LOCAL_CATASTROPHIC,
	IWARP_EXCEPTION_DETECTED_LOCAL_ACCESS_ERROR,
	IWARP_EXCEPTION_DETECTED_REMOTE_OPERATION_ERROR,
	IWARP_EXCEPTION_DETECTED_TERMINATE_RECEIVED,
	MAX_IWARP_FW_RETURN_CODE
};


struct iwarp_init_func_params {
	u8 ll2_ooo_q_index;
	u8 reserved1[7];
};


struct iwarp_init_func_ramrod_data {
	struct rdma_init_func_ramrod_data rdma;
	struct tcp_init_params tcp;
	struct iwarp_init_func_params iwarp;
};


enum iwarp_modify_qp_new_state_type {
	IWARP_MODIFY_QP_STATE_CLOSING = 1,
	IWARP_MODIFY_QP_STATE_ERROR = 2,
	MAX_IWARP_MODIFY_QP_NEW_STATE_TYPE
};


struct iwarp_modify_qp_ramrod_data {
	__le16 transition_to_state;
	__le16 flags;
#define IWARP_MODIFY_QP_RAMROD_DATA_RDMA_RD_EN_MASK		0x1
#define IWARP_MODIFY_QP_RAMROD_DATA_RDMA_RD_EN_SHIFT		0
#define IWARP_MODIFY_QP_RAMROD_DATA_RDMA_WR_EN_MASK		0x1
#define IWARP_MODIFY_QP_RAMROD_DATA_RDMA_WR_EN_SHIFT		1
#define IWARP_MODIFY_QP_RAMROD_DATA_ATOMIC_EN_MASK		0x1
#define IWARP_MODIFY_QP_RAMROD_DATA_ATOMIC_EN_SHIFT		2
#define IWARP_MODIFY_QP_RAMROD_DATA_STATE_TRANS_EN_MASK		0x1
#define IWARP_MODIFY_QP_RAMROD_DATA_STATE_TRANS_EN_SHIFT	3
#define IWARP_MODIFY_QP_RAMROD_DATA_RDMA_OPS_EN_FLG_MASK	0x1
#define IWARP_MODIFY_QP_RAMROD_DATA_RDMA_OPS_EN_FLG_SHIFT	4
#define IWARP_MODIFY_QP_RAMROD_DATA_PHYSICAL_QUEUE_FLG_MASK	0x1
#define IWARP_MODIFY_QP_RAMROD_DATA_PHYSICAL_QUEUE_FLG_SHIFT	5
#define IWARP_MODIFY_QP_RAMROD_DATA_RESERVED_MASK		0x3FF
#define IWARP_MODIFY_QP_RAMROD_DATA_RESERVED_SHIFT		6
	__le16 physical_q0;
	__le16 physical_q1;
	__le32 reserved1[10];
};


struct mpa_rq_params {
	__le32 ird;
	__le32 ord;
};


struct mpa_ulp_buffer {
	struct regpair addr;
	__le16 len;
	__le16 reserved[3];
};


struct mpa_outgoing_params {
	u8 crc_needed;
	u8 reject;
	u8 reserved[6];
	struct mpa_rq_params out_rq;
	struct mpa_ulp_buffer outgoing_ulp_buffer;
};


struct iwarp_mpa_offload_ramrod_data {
	struct mpa_outgoing_params common;
	__le32 tcp_cid;
	u8 mode;
	u8 tcp_connect_side;
	u8 rtr_pref;
#define IWARP_MPA_OFFLOAD_RAMROD_DATA_RTR_SUPPORTED_MASK	0x7
#define IWARP_MPA_OFFLOAD_RAMROD_DATA_RTR_SUPPORTED_SHIFT	0
#define IWARP_MPA_OFFLOAD_RAMROD_DATA_RESERVED1_MASK		0x1F
#define IWARP_MPA_OFFLOAD_RAMROD_DATA_RESERVED1_SHIFT		3
	u8 reserved2;
	struct mpa_ulp_buffer incoming_ulp_buffer;
	struct regpair async_eqe_output_buf;
	struct regpair handle_for_async;
	struct regpair shared_queue_addr;
	__le32 additional_setup_time;
	__le16 rcv_wnd;
	u8 stats_counter_id;
	u8 reserved3[9];
};


struct iwarp_offload_params {
	struct mpa_ulp_buffer incoming_ulp_buffer;
	struct regpair async_eqe_output_buf;
	struct regpair handle_for_async;
	__le32 additional_setup_time;
	__le16 physical_q0;
	__le16 physical_q1;
	u8 stats_counter_id;
	u8 mpa_mode;
	u8 src_vport_id;
	u8 reserved[5];
};


struct iwarp_query_qp_output_params {
	__le32 flags;
#define IWARP_QUERY_QP_OUTPUT_PARAMS_ERROR_FLG_MASK	0x1
#define IWARP_QUERY_QP_OUTPUT_PARAMS_ERROR_FLG_SHIFT	0
#define IWARP_QUERY_QP_OUTPUT_PARAMS_RESERVED0_MASK	0x7FFFFFFF
#define IWARP_QUERY_QP_OUTPUT_PARAMS_RESERVED0_SHIFT	1
	u8 reserved1[4];
};


struct iwarp_query_qp_ramrod_data {
	struct regpair output_params_addr;
};


enum iwarp_ramrod_cmd_id {
	IWARP_RAMROD_CMD_ID_TCP_OFFLOAD = 13,
	IWARP_RAMROD_CMD_ID_MPA_OFFLOAD,
	IWARP_RAMROD_CMD_ID_MPA_OFFLOAD_SEND_RTR,
	IWARP_RAMROD_CMD_ID_CREATE_QP,
	IWARP_RAMROD_CMD_ID_QUERY_QP,
	IWARP_RAMROD_CMD_ID_MODIFY_QP,
	IWARP_RAMROD_CMD_ID_DESTROY_QP,
	IWARP_RAMROD_CMD_ID_ABORT_TCP_OFFLOAD,
	MAX_IWARP_RAMROD_CMD_ID
};


struct iwarp_rxmit_stats_drv {
	struct regpair tx_go_to_slow_start_event_cnt;
	struct regpair tx_fast_retransmit_event_cnt;
};


struct iwarp_tcp_offload_ramrod_data {
	struct tcp_offload_params_opt2 tcp;
	struct iwarp_offload_params iwarp;
};


enum mpa_negotiation_mode {
	MPA_NEGOTIATION_TYPE_BASIC = 1,
	MPA_NEGOTIATION_TYPE_ENHANCED = 2,
	MAX_MPA_NEGOTIATION_MODE
};


enum mpa_rtr_type {
	MPA_RTR_TYPE_NONE = 0,
	MPA_RTR_TYPE_ZERO_SEND = 1,
	MPA_RTR_TYPE_ZERO_WRITE = 2,
	MPA_RTR_TYPE_ZERO_SEND_AND_WRITE = 3,
	MPA_RTR_TYPE_ZERO_READ = 4,
	MPA_RTR_TYPE_ZERO_SEND_AND_READ = 5,
	MPA_RTR_TYPE_ZERO_WRITE_AND_READ = 6,
	MPA_RTR_TYPE_ZERO_SEND_AND_WRITE_AND_READ = 7,
	MAX_MPA_RTR_TYPE
};


struct unaligned_opaque_data {
	__le16 first_mpa_offset;
	u8 tcp_payload_offset;
	u8 flags;
#define UNALIGNED_OPAQUE_DATA_PKT_REACHED_WIN_RIGHT_EDGE_MASK	0x1
#define UNALIGNED_OPAQUE_DATA_PKT_REACHED_WIN_RIGHT_EDGE_SHIFT	0
#define UNALIGNED_OPAQUE_DATA_CONNECTION_CLOSED_MASK		0x1
#define UNALIGNED_OPAQUE_DATA_CONNECTION_CLOSED_SHIFT		1
#define UNALIGNED_OPAQUE_DATA_RESERVED_MASK			0x3F
#define UNALIGNED_OPAQUE_DATA_RESERVED_SHIFT			2
	__le32 cid;
};

struct mstorm_iwarp_conn_ag_ctx {
	u8 reserved;
	u8 state;
	u8 flags0;
#define MSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM0_MASK		0x1
#define MSTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM0_SHIFT		0
#define MSTORM_IWARP_CONN_AG_CTX_BIT1_MASK			0x1
#define MSTORM_IWARP_CONN_AG_CTX_BIT1_SHIFT			1
#define MSTORM_IWARP_CONN_AG_CTX_INV_STAG_DONE_CF_MASK	0x3
#define MSTORM_IWARP_CONN_AG_CTX_INV_STAG_DONE_CF_SHIFT	2
#define MSTORM_IWARP_CONN_AG_CTX_CF1_MASK			0x3
#define MSTORM_IWARP_CONN_AG_CTX_CF1_SHIFT			4
#define MSTORM_IWARP_CONN_AG_CTX_CF2_MASK			0x3
#define MSTORM_IWARP_CONN_AG_CTX_CF2_SHIFT			6
	u8 flags1;
#define MSTORM_IWARP_CONN_AG_CTX_INV_STAG_DONE_CF_EN_MASK	0x1
#define MSTORM_IWARP_CONN_AG_CTX_INV_STAG_DONE_CF_EN_SHIFT	0
#define MSTORM_IWARP_CONN_AG_CTX_CF1EN_MASK			0x1
#define MSTORM_IWARP_CONN_AG_CTX_CF1EN_SHIFT			1
#define MSTORM_IWARP_CONN_AG_CTX_CF2EN_MASK			0x1
#define MSTORM_IWARP_CONN_AG_CTX_CF2EN_SHIFT			2
#define MSTORM_IWARP_CONN_AG_CTX_RULE0EN_MASK		0x1
#define MSTORM_IWARP_CONN_AG_CTX_RULE0EN_SHIFT		3
#define MSTORM_IWARP_CONN_AG_CTX_RULE1EN_MASK		0x1
#define MSTORM_IWARP_CONN_AG_CTX_RULE1EN_SHIFT		4
#define MSTORM_IWARP_CONN_AG_CTX_RULE2EN_MASK		0x1
#define MSTORM_IWARP_CONN_AG_CTX_RULE2EN_SHIFT		5
#define MSTORM_IWARP_CONN_AG_CTX_RCQ_CONS_EN_MASK		0x1
#define MSTORM_IWARP_CONN_AG_CTX_RCQ_CONS_EN_SHIFT		6
#define MSTORM_IWARP_CONN_AG_CTX_RULE4EN_MASK		0x1
#define MSTORM_IWARP_CONN_AG_CTX_RULE4EN_SHIFT		7
	__le16 rcq_cons;
	__le16 rcq_cons_th;
	__le32 reg0;
	__le32 reg1;
};

struct ustorm_iwarp_conn_ag_ctx {
	u8 reserved;
	u8 byte1;
	u8 flags0;
#define USTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM0_MASK	0x1
#define USTORM_IWARP_CONN_AG_CTX_EXIST_IN_QM0_SHIFT	0
#define USTORM_IWARP_CONN_AG_CTX_BIT1_MASK		0x1
#define USTORM_IWARP_CONN_AG_CTX_BIT1_SHIFT		1
#define USTORM_IWARP_CONN_AG_CTX_CF0_MASK		0x3
#define USTORM_IWARP_CONN_AG_CTX_CF0_SHIFT		2
#define USTORM_IWARP_CONN_AG_CTX_CF1_MASK		0x3
#define USTORM_IWARP_CONN_AG_CTX_CF1_SHIFT		4
#define USTORM_IWARP_CONN_AG_CTX_CF2_MASK		0x3
#define USTORM_IWARP_CONN_AG_CTX_CF2_SHIFT		6
	u8 flags1;
#define USTORM_IWARP_CONN_AG_CTX_CF3_MASK		0x3
#define USTORM_IWARP_CONN_AG_CTX_CF3_SHIFT		0
#define USTORM_IWARP_CONN_AG_CTX_CQ_ARM_SE_CF_MASK	0x3
#define USTORM_IWARP_CONN_AG_CTX_CQ_ARM_SE_CF_SHIFT	2
#define USTORM_IWARP_CONN_AG_CTX_CQ_ARM_CF_MASK	0x3
#define USTORM_IWARP_CONN_AG_CTX_CQ_ARM_CF_SHIFT	4
#define USTORM_IWARP_CONN_AG_CTX_CF6_MASK		0x3
#define USTORM_IWARP_CONN_AG_CTX_CF6_SHIFT		6
	u8 flags2;
#define USTORM_IWARP_CONN_AG_CTX_CF0EN_MASK			0x1
#define USTORM_IWARP_CONN_AG_CTX_CF0EN_SHIFT			0
#define USTORM_IWARP_CONN_AG_CTX_CF1EN_MASK			0x1
#define USTORM_IWARP_CONN_AG_CTX_CF1EN_SHIFT			1
#define USTORM_IWARP_CONN_AG_CTX_CF2EN_MASK			0x1
#define USTORM_IWARP_CONN_AG_CTX_CF2EN_SHIFT			2
#define USTORM_IWARP_CONN_AG_CTX_CF3EN_MASK			0x1
#define USTORM_IWARP_CONN_AG_CTX_CF3EN_SHIFT			3
#define USTORM_IWARP_CONN_AG_CTX_CQ_ARM_SE_CF_EN_MASK	0x1
#define USTORM_IWARP_CONN_AG_CTX_CQ_ARM_SE_CF_EN_SHIFT	4
#define USTORM_IWARP_CONN_AG_CTX_CQ_ARM_CF_EN_MASK		0x1
#define USTORM_IWARP_CONN_AG_CTX_CQ_ARM_CF_EN_SHIFT		5
#define USTORM_IWARP_CONN_AG_CTX_CF6EN_MASK			0x1
#define USTORM_IWARP_CONN_AG_CTX_CF6EN_SHIFT			6
#define USTORM_IWARP_CONN_AG_CTX_CQ_SE_EN_MASK		0x1
#define USTORM_IWARP_CONN_AG_CTX_CQ_SE_EN_SHIFT		7
	u8 flags3;
#define USTORM_IWARP_CONN_AG_CTX_CQ_EN_MASK		0x1
#define USTORM_IWARP_CONN_AG_CTX_CQ_EN_SHIFT		0
#define USTORM_IWARP_CONN_AG_CTX_RULE2EN_MASK	0x1
#define USTORM_IWARP_CONN_AG_CTX_RULE2EN_SHIFT	1
#define USTORM_IWARP_CONN_AG_CTX_RULE3EN_MASK	0x1
#define USTORM_IWARP_CONN_AG_CTX_RULE3EN_SHIFT	2
#define USTORM_IWARP_CONN_AG_CTX_RULE4EN_MASK	0x1
#define USTORM_IWARP_CONN_AG_CTX_RULE4EN_SHIFT	3
#define USTORM_IWARP_CONN_AG_CTX_RULE5EN_MASK	0x1
#define USTORM_IWARP_CONN_AG_CTX_RULE5EN_SHIFT	4
#define USTORM_IWARP_CONN_AG_CTX_RULE6EN_MASK	0x1
#define USTORM_IWARP_CONN_AG_CTX_RULE6EN_SHIFT	5
#define USTORM_IWARP_CONN_AG_CTX_RULE7EN_MASK	0x1
#define USTORM_IWARP_CONN_AG_CTX_RULE7EN_SHIFT	6
#define USTORM_IWARP_CONN_AG_CTX_RULE8EN_MASK	0x1
#define USTORM_IWARP_CONN_AG_CTX_RULE8EN_SHIFT	7
	u8 byte2;
	u8 byte3;
	__le16 word0;
	__le16 word1;
	__le32 cq_cons;
	__le32 cq_se_prod;
	__le32 cq_prod;
	__le32 reg3;
	__le16 word2;
	__le16 word3;
};

struct ystorm_iwarp_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define YSTORM_IWARP_CONN_AG_CTX_BIT0_MASK	0x1
#define YSTORM_IWARP_CONN_AG_CTX_BIT0_SHIFT	0
#define YSTORM_IWARP_CONN_AG_CTX_BIT1_MASK	0x1
#define YSTORM_IWARP_CONN_AG_CTX_BIT1_SHIFT	1
#define YSTORM_IWARP_CONN_AG_CTX_CF0_MASK	0x3
#define YSTORM_IWARP_CONN_AG_CTX_CF0_SHIFT	2
#define YSTORM_IWARP_CONN_AG_CTX_CF1_MASK	0x3
#define YSTORM_IWARP_CONN_AG_CTX_CF1_SHIFT	4
#define YSTORM_IWARP_CONN_AG_CTX_CF2_MASK	0x3
#define YSTORM_IWARP_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define YSTORM_IWARP_CONN_AG_CTX_CF0EN_MASK		0x1
#define YSTORM_IWARP_CONN_AG_CTX_CF0EN_SHIFT		0
#define YSTORM_IWARP_CONN_AG_CTX_CF1EN_MASK		0x1
#define YSTORM_IWARP_CONN_AG_CTX_CF1EN_SHIFT		1
#define YSTORM_IWARP_CONN_AG_CTX_CF2EN_MASK		0x1
#define YSTORM_IWARP_CONN_AG_CTX_CF2EN_SHIFT		2
#define YSTORM_IWARP_CONN_AG_CTX_RULE0EN_MASK	0x1
#define YSTORM_IWARP_CONN_AG_CTX_RULE0EN_SHIFT	3
#define YSTORM_IWARP_CONN_AG_CTX_RULE1EN_MASK	0x1
#define YSTORM_IWARP_CONN_AG_CTX_RULE1EN_SHIFT	4
#define YSTORM_IWARP_CONN_AG_CTX_RULE2EN_MASK	0x1
#define YSTORM_IWARP_CONN_AG_CTX_RULE2EN_SHIFT	5
#define YSTORM_IWARP_CONN_AG_CTX_RULE3EN_MASK	0x1
#define YSTORM_IWARP_CONN_AG_CTX_RULE3EN_SHIFT	6
#define YSTORM_IWARP_CONN_AG_CTX_RULE4EN_MASK	0x1
#define YSTORM_IWARP_CONN_AG_CTX_RULE4EN_SHIFT	7
	u8 byte2;
	u8 byte3;
	__le16 word0;
	__le32 reg0;
	__le32 reg1;
	__le16 word1;
	__le16 word2;
	__le16 word3;
	__le16 word4;
	__le32 reg2;
	__le32 reg3;
};


struct ystorm_fcoe_conn_st_ctx {
	u8 func_mode;
	u8 cos;
	u8 conf_version;
	u8 eth_hdr_size;
	__le16 stat_ram_addr;
	__le16 mtu;
	__le16 max_fc_payload_len;
	__le16 tx_max_fc_pay_len;
	u8 fcp_cmd_size;
	u8 fcp_rsp_size;
	__le16 mss;
	struct regpair reserved;
	__le16 min_frame_size;
	u8 protection_info_flags;
#define YSTORM_FCOE_CONN_ST_CTX_SUPPORT_PROTECTION_MASK		0x1
#define YSTORM_FCOE_CONN_ST_CTX_SUPPORT_PROTECTION_SHIFT	0
#define YSTORM_FCOE_CONN_ST_CTX_VALID_MASK			0x1
#define YSTORM_FCOE_CONN_ST_CTX_VALID_SHIFT			1
#define YSTORM_FCOE_CONN_ST_CTX_RESERVED1_MASK			0x3F
#define YSTORM_FCOE_CONN_ST_CTX_RESERVED1_SHIFT			2
	u8 dst_protection_per_mss;
	u8 src_protection_per_mss;
	u8 ptu_log_page_size;
	u8 flags;
#define YSTORM_FCOE_CONN_ST_CTX_INNER_VLAN_FLAG_MASK	0x1
#define YSTORM_FCOE_CONN_ST_CTX_INNER_VLAN_FLAG_SHIFT	0
#define YSTORM_FCOE_CONN_ST_CTX_OUTER_VLAN_FLAG_MASK	0x1
#define YSTORM_FCOE_CONN_ST_CTX_OUTER_VLAN_FLAG_SHIFT	1
#define YSTORM_FCOE_CONN_ST_CTX_RSRV_MASK		0x3F
#define YSTORM_FCOE_CONN_ST_CTX_RSRV_SHIFT		2
	u8 fcp_xfer_size;
};


struct fcoe_vlan_fields {
	__le16 fields;
#define FCOE_VLAN_FIELDS_VID_MASK	0xFFF
#define FCOE_VLAN_FIELDS_VID_SHIFT	0
#define FCOE_VLAN_FIELDS_CLI_MASK	0x1
#define FCOE_VLAN_FIELDS_CLI_SHIFT	12
#define FCOE_VLAN_FIELDS_PRI_MASK	0x7
#define FCOE_VLAN_FIELDS_PRI_SHIFT	13
};


union fcoe_vlan_field_union {
	struct fcoe_vlan_fields fields;
	__le16 val;
};


union fcoe_vlan_vif_field_union {
	union fcoe_vlan_field_union vlan;
	__le16 vif;
};


struct pstorm_fcoe_eth_context_section {
	u8 remote_addr_3;
	u8 remote_addr_2;
	u8 remote_addr_1;
	u8 remote_addr_0;
	u8 local_addr_1;
	u8 local_addr_0;
	u8 remote_addr_5;
	u8 remote_addr_4;
	u8 local_addr_5;
	u8 local_addr_4;
	u8 local_addr_3;
	u8 local_addr_2;
	union fcoe_vlan_vif_field_union vif_outer_vlan;
	__le16 vif_outer_eth_type;
	union fcoe_vlan_vif_field_union inner_vlan;
	__le16 inner_eth_type;
};


struct pstorm_fcoe_conn_st_ctx {
	u8 func_mode;
	u8 cos;
	u8 conf_version;
	u8 rsrv;
	__le16 stat_ram_addr;
	__le16 mss;
	struct regpair abts_cleanup_addr;
	struct pstorm_fcoe_eth_context_section eth;
	u8 sid_2;
	u8 sid_1;
	u8 sid_0;
	u8 flags;
#define PSTORM_FCOE_CONN_ST_CTX_VNTAG_VLAN_MASK			0x1
#define PSTORM_FCOE_CONN_ST_CTX_VNTAG_VLAN_SHIFT		0
#define PSTORM_FCOE_CONN_ST_CTX_SUPPORT_REC_RR_TOV_MASK		0x1
#define PSTORM_FCOE_CONN_ST_CTX_SUPPORT_REC_RR_TOV_SHIFT	1
#define PSTORM_FCOE_CONN_ST_CTX_INNER_VLAN_FLAG_MASK		0x1
#define PSTORM_FCOE_CONN_ST_CTX_INNER_VLAN_FLAG_SHIFT		2
#define PSTORM_FCOE_CONN_ST_CTX_OUTER_VLAN_FLAG_MASK		0x1
#define PSTORM_FCOE_CONN_ST_CTX_OUTER_VLAN_FLAG_SHIFT		3
#define PSTORM_FCOE_CONN_ST_CTX_SINGLE_VLAN_FLAG_MASK		0x1
#define PSTORM_FCOE_CONN_ST_CTX_SINGLE_VLAN_FLAG_SHIFT		4
#define PSTORM_FCOE_CONN_ST_CTX_RESERVED_MASK			0x7
#define PSTORM_FCOE_CONN_ST_CTX_RESERVED_SHIFT			5
	u8 did_2;
	u8 did_1;
	u8 did_0;
	u8 src_mac_index;
	__le16 rec_rr_tov_val;
	u8 q_relative_offset;
	u8 reserved1;
};


struct xstorm_fcoe_conn_st_ctx {
	u8 func_mode;
	u8 src_mac_index;
	u8 conf_version;
	u8 cached_wqes_avail;
	__le16 stat_ram_addr;
	u8 flags;
#define XSTORM_FCOE_CONN_ST_CTX_SQ_DEFERRED_MASK		0x1
#define XSTORM_FCOE_CONN_ST_CTX_SQ_DEFERRED_SHIFT		0
#define XSTORM_FCOE_CONN_ST_CTX_INNER_VLAN_FLAG_MASK		0x1
#define XSTORM_FCOE_CONN_ST_CTX_INNER_VLAN_FLAG_SHIFT		1
#define XSTORM_FCOE_CONN_ST_CTX_INNER_VLAN_FLAG_ORIG_MASK	0x1
#define XSTORM_FCOE_CONN_ST_CTX_INNER_VLAN_FLAG_ORIG_SHIFT	2
#define XSTORM_FCOE_CONN_ST_CTX_LAST_QUEUE_HANDLED_MASK		0x3
#define XSTORM_FCOE_CONN_ST_CTX_LAST_QUEUE_HANDLED_SHIFT	3
#define XSTORM_FCOE_CONN_ST_CTX_RSRV_MASK			0x7
#define XSTORM_FCOE_CONN_ST_CTX_RSRV_SHIFT			5
	u8 cached_wqes_offset;
	u8 reserved2;
	u8 eth_hdr_size;
	u8 seq_id;
	u8 max_conc_seqs;
	__le16 num_pages_in_pbl;
	__le16 reserved;
	struct regpair sq_pbl_addr;
	struct regpair sq_curr_page_addr;
	struct regpair sq_next_page_addr;
	struct regpair xferq_pbl_addr;
	struct regpair xferq_curr_page_addr;
	struct regpair xferq_next_page_addr;
	struct regpair respq_pbl_addr;
	struct regpair respq_curr_page_addr;
	struct regpair respq_next_page_addr;
	__le16 mtu;
	__le16 tx_max_fc_pay_len;
	__le16 max_fc_payload_len;
	__le16 min_frame_size;
	__le16 sq_pbl_next_index;
	__le16 respq_pbl_next_index;
	u8 fcp_cmd_byte_credit;
	u8 fcp_rsp_byte_credit;
	__le16 protection_info;
#define XSTORM_FCOE_CONN_ST_CTX_PROTECTION_PERF_MASK		0x1
#define XSTORM_FCOE_CONN_ST_CTX_PROTECTION_PERF_SHIFT		0
#define XSTORM_FCOE_CONN_ST_CTX_SUPPORT_PROTECTION_MASK		0x1
#define XSTORM_FCOE_CONN_ST_CTX_SUPPORT_PROTECTION_SHIFT	1
#define XSTORM_FCOE_CONN_ST_CTX_VALID_MASK			0x1
#define XSTORM_FCOE_CONN_ST_CTX_VALID_SHIFT			2
#define XSTORM_FCOE_CONN_ST_CTX_FRAME_PROT_ALIGNED_MASK		0x1
#define XSTORM_FCOE_CONN_ST_CTX_FRAME_PROT_ALIGNED_SHIFT	3
#define XSTORM_FCOE_CONN_ST_CTX_RESERVED3_MASK			0xF
#define XSTORM_FCOE_CONN_ST_CTX_RESERVED3_SHIFT			4
#define XSTORM_FCOE_CONN_ST_CTX_DST_PROTECTION_PER_MSS_MASK	0xFF
#define XSTORM_FCOE_CONN_ST_CTX_DST_PROTECTION_PER_MSS_SHIFT	8
	__le16 xferq_pbl_next_index;
	__le16 page_size;
	u8 mid_seq;
	u8 fcp_xfer_byte_credit;
	u8 reserved1[2];
	struct fcoe_wqe cached_wqes[16];
};

struct xstorm_fcoe_conn_ag_ctx {
	u8 reserved0;
	u8 state;
	u8 flags0;
#define XSTORM_FCOE_CONN_AG_CTX_EXIST_IN_QM0_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT	0
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED1_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED1_SHIFT	1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED2_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED2_SHIFT	2
#define XSTORM_FCOE_CONN_AG_CTX_EXIST_IN_QM3_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_EXIST_IN_QM3_SHIFT	3
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED3_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED3_SHIFT	4
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED4_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED4_SHIFT	5
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED5_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED5_SHIFT	6
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED6_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED6_SHIFT	7
	u8 flags1;
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED7_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED7_SHIFT	0
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED8_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED8_SHIFT	1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED9_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED9_SHIFT	2
#define XSTORM_FCOE_CONN_AG_CTX_BIT11_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_BIT11_SHIFT		3
#define XSTORM_FCOE_CONN_AG_CTX_BIT12_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_BIT12_SHIFT		4
#define XSTORM_FCOE_CONN_AG_CTX_BIT13_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_BIT13_SHIFT		5
#define XSTORM_FCOE_CONN_AG_CTX_BIT14_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_BIT14_SHIFT		6
#define XSTORM_FCOE_CONN_AG_CTX_BIT15_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_BIT15_SHIFT		7
	u8 flags2;
#define XSTORM_FCOE_CONN_AG_CTX_CF0_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF0_SHIFT	0
#define XSTORM_FCOE_CONN_AG_CTX_CF1_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF1_SHIFT	2
#define XSTORM_FCOE_CONN_AG_CTX_CF2_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF2_SHIFT	4
#define XSTORM_FCOE_CONN_AG_CTX_CF3_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF3_SHIFT	6
	u8 flags3;
#define XSTORM_FCOE_CONN_AG_CTX_CF4_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF4_SHIFT	0
#define XSTORM_FCOE_CONN_AG_CTX_CF5_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF5_SHIFT	2
#define XSTORM_FCOE_CONN_AG_CTX_CF6_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF6_SHIFT	4
#define XSTORM_FCOE_CONN_AG_CTX_CF7_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF7_SHIFT	6
	u8 flags4;
#define XSTORM_FCOE_CONN_AG_CTX_CF8_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF8_SHIFT	0
#define XSTORM_FCOE_CONN_AG_CTX_CF9_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF9_SHIFT	2
#define XSTORM_FCOE_CONN_AG_CTX_CF10_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF10_SHIFT	4
#define XSTORM_FCOE_CONN_AG_CTX_CF11_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF11_SHIFT	6
	u8 flags5;
#define XSTORM_FCOE_CONN_AG_CTX_CF12_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF12_SHIFT	0
#define XSTORM_FCOE_CONN_AG_CTX_CF13_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF13_SHIFT	2
#define XSTORM_FCOE_CONN_AG_CTX_CF14_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF14_SHIFT	4
#define XSTORM_FCOE_CONN_AG_CTX_CF15_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF15_SHIFT	6
	u8 flags6;
#define XSTORM_FCOE_CONN_AG_CTX_CF16_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF16_SHIFT	0
#define XSTORM_FCOE_CONN_AG_CTX_CF17_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF17_SHIFT	2
#define XSTORM_FCOE_CONN_AG_CTX_CF18_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF18_SHIFT	4
#define XSTORM_FCOE_CONN_AG_CTX_DQ_CF_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_DQ_CF_SHIFT	6
	u8 flags7;
#define XSTORM_FCOE_CONN_AG_CTX_FLUSH_Q0_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_FLUSH_Q0_SHIFT	0
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED10_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED10_SHIFT	2
#define XSTORM_FCOE_CONN_AG_CTX_SLOW_PATH_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_SLOW_PATH_SHIFT	4
#define XSTORM_FCOE_CONN_AG_CTX_CF0EN_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF0EN_SHIFT		6
#define XSTORM_FCOE_CONN_AG_CTX_CF1EN_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF1EN_SHIFT		7
	u8 flags8;
#define XSTORM_FCOE_CONN_AG_CTX_CF2EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF2EN_SHIFT	0
#define XSTORM_FCOE_CONN_AG_CTX_CF3EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF3EN_SHIFT	1
#define XSTORM_FCOE_CONN_AG_CTX_CF4EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF4EN_SHIFT	2
#define XSTORM_FCOE_CONN_AG_CTX_CF5EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF5EN_SHIFT	3
#define XSTORM_FCOE_CONN_AG_CTX_CF6EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF6EN_SHIFT	4
#define XSTORM_FCOE_CONN_AG_CTX_CF7EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF7EN_SHIFT	5
#define XSTORM_FCOE_CONN_AG_CTX_CF8EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF8EN_SHIFT	6
#define XSTORM_FCOE_CONN_AG_CTX_CF9EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF9EN_SHIFT	7
	u8 flags9;
#define XSTORM_FCOE_CONN_AG_CTX_CF10EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF10EN_SHIFT	0
#define XSTORM_FCOE_CONN_AG_CTX_CF11EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF11EN_SHIFT	1
#define XSTORM_FCOE_CONN_AG_CTX_CF12EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF12EN_SHIFT	2
#define XSTORM_FCOE_CONN_AG_CTX_CF13EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF13EN_SHIFT	3
#define XSTORM_FCOE_CONN_AG_CTX_CF14EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF14EN_SHIFT	4
#define XSTORM_FCOE_CONN_AG_CTX_CF15EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF15EN_SHIFT	5
#define XSTORM_FCOE_CONN_AG_CTX_CF16EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF16EN_SHIFT	6
#define XSTORM_FCOE_CONN_AG_CTX_CF17EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF17EN_SHIFT	7
	u8 flags10;
#define XSTORM_FCOE_CONN_AG_CTX_CF18EN_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF18EN_SHIFT		0
#define XSTORM_FCOE_CONN_AG_CTX_DQ_CF_EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_DQ_CF_EN_SHIFT	1
#define XSTORM_FCOE_CONN_AG_CTX_FLUSH_Q0_EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT	2
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED11_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED11_SHIFT	3
#define XSTORM_FCOE_CONN_AG_CTX_SLOW_PATH_EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_SLOW_PATH_EN_SHIFT	4
#define XSTORM_FCOE_CONN_AG_CTX_CF23EN_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_CF23EN_SHIFT		5
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED12_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED12_SHIFT	6
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED13_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED13_SHIFT	7
	u8 flags11;
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED14_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED14_SHIFT		0
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED15_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED15_SHIFT		1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED16_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_RESERVED16_SHIFT		2
#define XSTORM_FCOE_CONN_AG_CTX_RULE5EN_MASK			0x1
#define XSTORM_FCOE_CONN_AG_CTX_RULE5EN_SHIFT		3
#define XSTORM_FCOE_CONN_AG_CTX_RULE6EN_MASK			0x1
#define XSTORM_FCOE_CONN_AG_CTX_RULE6EN_SHIFT		4
#define XSTORM_FCOE_CONN_AG_CTX_RULE7EN_MASK			0x1
#define XSTORM_FCOE_CONN_AG_CTX_RULE7EN_SHIFT		5
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED1_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED1_SHIFT		6
#define XSTORM_FCOE_CONN_AG_CTX_XFERQ_DECISION_EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_XFERQ_DECISION_EN_SHIFT	7
	u8 flags12;
#define XSTORM_FCOE_CONN_AG_CTX_SQ_DECISION_EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_SQ_DECISION_EN_SHIFT	0
#define XSTORM_FCOE_CONN_AG_CTX_RULE11EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RULE11EN_SHIFT	1
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED2_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED2_SHIFT	2
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED3_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED3_SHIFT	3
#define XSTORM_FCOE_CONN_AG_CTX_RULE14EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RULE14EN_SHIFT	4
#define XSTORM_FCOE_CONN_AG_CTX_RULE15EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RULE15EN_SHIFT	5
#define XSTORM_FCOE_CONN_AG_CTX_RULE16EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RULE16EN_SHIFT	6
#define XSTORM_FCOE_CONN_AG_CTX_RULE17EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RULE17EN_SHIFT	7
	u8 flags13;
#define XSTORM_FCOE_CONN_AG_CTX_RESPQ_DECISION_EN_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_RESPQ_DECISION_EN_SHIFT	0
#define XSTORM_FCOE_CONN_AG_CTX_RULE19EN_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_RULE19EN_SHIFT		1
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED4_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED4_SHIFT		2
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED5_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED5_SHIFT		3
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED6_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED6_SHIFT		4
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED7_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED7_SHIFT		5
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED8_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED8_SHIFT		6
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED9_MASK		0x1
#define XSTORM_FCOE_CONN_AG_CTX_A0_RESERVED9_SHIFT		7
	u8 flags14;
#define XSTORM_FCOE_CONN_AG_CTX_BIT16_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_BIT16_SHIFT	0
#define XSTORM_FCOE_CONN_AG_CTX_BIT17_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_BIT17_SHIFT	1
#define XSTORM_FCOE_CONN_AG_CTX_BIT18_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_BIT18_SHIFT	2
#define XSTORM_FCOE_CONN_AG_CTX_BIT19_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_BIT19_SHIFT	3
#define XSTORM_FCOE_CONN_AG_CTX_BIT20_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_BIT20_SHIFT	4
#define XSTORM_FCOE_CONN_AG_CTX_BIT21_MASK	0x1
#define XSTORM_FCOE_CONN_AG_CTX_BIT21_SHIFT	5
#define XSTORM_FCOE_CONN_AG_CTX_CF23_MASK	0x3
#define XSTORM_FCOE_CONN_AG_CTX_CF23_SHIFT	6
	u8 byte2;
	__le16 physical_q0;
	__le16 word1;
	__le16 word2;
	__le16 sq_cons;
	__le16 sq_prod;
	__le16 xferq_prod;
	__le16 xferq_cons;
	u8 byte3;
	u8 byte4;
	u8 byte5;
	u8 byte6;
	__le32 remain_io;
	__le32 reg1;
	__le32 reg2;
	__le32 reg3;
	__le32 reg4;
	__le32 reg5;
	__le32 reg6;
	__le16 respq_prod;
	__le16 respq_cons;
	__le16 word9;
	__le16 word10;
	__le32 reg7;
	__le32 reg8;
};


struct ustorm_fcoe_conn_st_ctx {
	struct regpair respq_pbl_addr;
	__le16 num_pages_in_pbl;
	u8 ptu_log_page_size;
	u8 log_page_size;
	__le16 respq_prod;
	u8 reserved[2];
};

struct tstorm_fcoe_conn_ag_ctx {
	u8 reserved0;
	u8 state;
	u8 flags0;
#define TSTORM_FCOE_CONN_AG_CTX_EXIST_IN_QM0_MASK	0x1
#define TSTORM_FCOE_CONN_AG_CTX_EXIST_IN_QM0_SHIFT	0
#define TSTORM_FCOE_CONN_AG_CTX_BIT1_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_BIT1_SHIFT		1
#define TSTORM_FCOE_CONN_AG_CTX_BIT2_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_BIT2_SHIFT		2
#define TSTORM_FCOE_CONN_AG_CTX_BIT3_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_BIT3_SHIFT		3
#define TSTORM_FCOE_CONN_AG_CTX_BIT4_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_BIT4_SHIFT		4
#define TSTORM_FCOE_CONN_AG_CTX_BIT5_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_BIT5_SHIFT		5
#define TSTORM_FCOE_CONN_AG_CTX_DUMMY_TIMER_CF_MASK	0x3
#define TSTORM_FCOE_CONN_AG_CTX_DUMMY_TIMER_CF_SHIFT	6
	u8 flags1;
#define TSTORM_FCOE_CONN_AG_CTX_FLUSH_Q0_CF_MASK		0x3
#define TSTORM_FCOE_CONN_AG_CTX_FLUSH_Q0_CF_SHIFT		0
#define TSTORM_FCOE_CONN_AG_CTX_CF2_MASK			0x3
#define TSTORM_FCOE_CONN_AG_CTX_CF2_SHIFT			2
#define TSTORM_FCOE_CONN_AG_CTX_TIMER_STOP_ALL_CF_MASK	0x3
#define TSTORM_FCOE_CONN_AG_CTX_TIMER_STOP_ALL_CF_SHIFT	4
#define TSTORM_FCOE_CONN_AG_CTX_CF4_MASK			0x3
#define TSTORM_FCOE_CONN_AG_CTX_CF4_SHIFT			6
	u8 flags2;
#define TSTORM_FCOE_CONN_AG_CTX_CF5_MASK	0x3
#define TSTORM_FCOE_CONN_AG_CTX_CF5_SHIFT	0
#define TSTORM_FCOE_CONN_AG_CTX_CF6_MASK	0x3
#define TSTORM_FCOE_CONN_AG_CTX_CF6_SHIFT	2
#define TSTORM_FCOE_CONN_AG_CTX_CF7_MASK	0x3
#define TSTORM_FCOE_CONN_AG_CTX_CF7_SHIFT	4
#define TSTORM_FCOE_CONN_AG_CTX_CF8_MASK	0x3
#define TSTORM_FCOE_CONN_AG_CTX_CF8_SHIFT	6
	u8 flags3;
#define TSTORM_FCOE_CONN_AG_CTX_CF9_MASK			0x3
#define TSTORM_FCOE_CONN_AG_CTX_CF9_SHIFT			0
#define TSTORM_FCOE_CONN_AG_CTX_CF10_MASK			0x3
#define TSTORM_FCOE_CONN_AG_CTX_CF10_SHIFT			2
#define TSTORM_FCOE_CONN_AG_CTX_DUMMY_TIMER_CF_EN_MASK	0x1
#define TSTORM_FCOE_CONN_AG_CTX_DUMMY_TIMER_CF_EN_SHIFT	4
#define TSTORM_FCOE_CONN_AG_CTX_FLUSH_Q0_CF_EN_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_FLUSH_Q0_CF_EN_SHIFT		5
#define TSTORM_FCOE_CONN_AG_CTX_CF2EN_MASK			0x1
#define TSTORM_FCOE_CONN_AG_CTX_CF2EN_SHIFT			6
#define TSTORM_FCOE_CONN_AG_CTX_TIMER_STOP_ALL_CF_EN_MASK	0x1
#define TSTORM_FCOE_CONN_AG_CTX_TIMER_STOP_ALL_CF_EN_SHIFT	7
	u8 flags4;
#define TSTORM_FCOE_CONN_AG_CTX_CF4EN_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_CF4EN_SHIFT		0
#define TSTORM_FCOE_CONN_AG_CTX_CF5EN_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_CF5EN_SHIFT		1
#define TSTORM_FCOE_CONN_AG_CTX_CF6EN_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_CF6EN_SHIFT		2
#define TSTORM_FCOE_CONN_AG_CTX_CF7EN_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_CF7EN_SHIFT		3
#define TSTORM_FCOE_CONN_AG_CTX_CF8EN_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_CF8EN_SHIFT		4
#define TSTORM_FCOE_CONN_AG_CTX_CF9EN_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_CF9EN_SHIFT		5
#define TSTORM_FCOE_CONN_AG_CTX_CF10EN_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_CF10EN_SHIFT		6
#define TSTORM_FCOE_CONN_AG_CTX_RULE0EN_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_RULE0EN_SHIFT	7
	u8 flags5;
#define TSTORM_FCOE_CONN_AG_CTX_RULE1EN_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_RULE1EN_SHIFT	0
#define TSTORM_FCOE_CONN_AG_CTX_RULE2EN_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_RULE2EN_SHIFT	1
#define TSTORM_FCOE_CONN_AG_CTX_RULE3EN_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_RULE3EN_SHIFT	2
#define TSTORM_FCOE_CONN_AG_CTX_RULE4EN_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_RULE4EN_SHIFT	3
#define TSTORM_FCOE_CONN_AG_CTX_RULE5EN_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_RULE5EN_SHIFT	4
#define TSTORM_FCOE_CONN_AG_CTX_RULE6EN_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_RULE6EN_SHIFT	5
#define TSTORM_FCOE_CONN_AG_CTX_RULE7EN_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_RULE7EN_SHIFT	6
#define TSTORM_FCOE_CONN_AG_CTX_RULE8EN_MASK		0x1
#define TSTORM_FCOE_CONN_AG_CTX_RULE8EN_SHIFT	7
	__le32 reg0;
	__le32 reg1;
};

struct ustorm_fcoe_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define USTORM_FCOE_CONN_AG_CTX_BIT0_MASK	0x1
#define USTORM_FCOE_CONN_AG_CTX_BIT0_SHIFT	0
#define USTORM_FCOE_CONN_AG_CTX_BIT1_MASK	0x1
#define USTORM_FCOE_CONN_AG_CTX_BIT1_SHIFT	1
#define USTORM_FCOE_CONN_AG_CTX_CF0_MASK	0x3
#define USTORM_FCOE_CONN_AG_CTX_CF0_SHIFT	2
#define USTORM_FCOE_CONN_AG_CTX_CF1_MASK	0x3
#define USTORM_FCOE_CONN_AG_CTX_CF1_SHIFT	4
#define USTORM_FCOE_CONN_AG_CTX_CF2_MASK	0x3
#define USTORM_FCOE_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define USTORM_FCOE_CONN_AG_CTX_CF3_MASK	0x3
#define USTORM_FCOE_CONN_AG_CTX_CF3_SHIFT	0
#define USTORM_FCOE_CONN_AG_CTX_CF4_MASK	0x3
#define USTORM_FCOE_CONN_AG_CTX_CF4_SHIFT	2
#define USTORM_FCOE_CONN_AG_CTX_CF5_MASK	0x3
#define USTORM_FCOE_CONN_AG_CTX_CF5_SHIFT	4
#define USTORM_FCOE_CONN_AG_CTX_CF6_MASK	0x3
#define USTORM_FCOE_CONN_AG_CTX_CF6_SHIFT	6
	u8 flags2;
#define USTORM_FCOE_CONN_AG_CTX_CF0EN_MASK		0x1
#define USTORM_FCOE_CONN_AG_CTX_CF0EN_SHIFT		0
#define USTORM_FCOE_CONN_AG_CTX_CF1EN_MASK		0x1
#define USTORM_FCOE_CONN_AG_CTX_CF1EN_SHIFT		1
#define USTORM_FCOE_CONN_AG_CTX_CF2EN_MASK		0x1
#define USTORM_FCOE_CONN_AG_CTX_CF2EN_SHIFT		2
#define USTORM_FCOE_CONN_AG_CTX_CF3EN_MASK		0x1
#define USTORM_FCOE_CONN_AG_CTX_CF3EN_SHIFT		3
#define USTORM_FCOE_CONN_AG_CTX_CF4EN_MASK		0x1
#define USTORM_FCOE_CONN_AG_CTX_CF4EN_SHIFT		4
#define USTORM_FCOE_CONN_AG_CTX_CF5EN_MASK		0x1
#define USTORM_FCOE_CONN_AG_CTX_CF5EN_SHIFT		5
#define USTORM_FCOE_CONN_AG_CTX_CF6EN_MASK		0x1
#define USTORM_FCOE_CONN_AG_CTX_CF6EN_SHIFT		6
#define USTORM_FCOE_CONN_AG_CTX_RULE0EN_MASK		0x1
#define USTORM_FCOE_CONN_AG_CTX_RULE0EN_SHIFT	7
	u8 flags3;
#define USTORM_FCOE_CONN_AG_CTX_RULE1EN_MASK		0x1
#define USTORM_FCOE_CONN_AG_CTX_RULE1EN_SHIFT	0
#define USTORM_FCOE_CONN_AG_CTX_RULE2EN_MASK		0x1
#define USTORM_FCOE_CONN_AG_CTX_RULE2EN_SHIFT	1
#define USTORM_FCOE_CONN_AG_CTX_RULE3EN_MASK		0x1
#define USTORM_FCOE_CONN_AG_CTX_RULE3EN_SHIFT	2
#define USTORM_FCOE_CONN_AG_CTX_RULE4EN_MASK		0x1
#define USTORM_FCOE_CONN_AG_CTX_RULE4EN_SHIFT	3
#define USTORM_FCOE_CONN_AG_CTX_RULE5EN_MASK		0x1
#define USTORM_FCOE_CONN_AG_CTX_RULE5EN_SHIFT	4
#define USTORM_FCOE_CONN_AG_CTX_RULE6EN_MASK		0x1
#define USTORM_FCOE_CONN_AG_CTX_RULE6EN_SHIFT	5
#define USTORM_FCOE_CONN_AG_CTX_RULE7EN_MASK		0x1
#define USTORM_FCOE_CONN_AG_CTX_RULE7EN_SHIFT	6
#define USTORM_FCOE_CONN_AG_CTX_RULE8EN_MASK		0x1
#define USTORM_FCOE_CONN_AG_CTX_RULE8EN_SHIFT	7
	u8 byte2;
	u8 byte3;
	__le16 word0;
	__le16 word1;
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 reg3;
	__le16 word2;
	__le16 word3;
};


struct tstorm_fcoe_conn_st_ctx {
	__le16 stat_ram_addr;
	__le16 rx_max_fc_payload_len;
	__le16 e_d_tov_val;
	u8 flags;
#define TSTORM_FCOE_CONN_ST_CTX_INC_SEQ_CNT_MASK	0x1
#define TSTORM_FCOE_CONN_ST_CTX_INC_SEQ_CNT_SHIFT	0
#define TSTORM_FCOE_CONN_ST_CTX_SUPPORT_CONF_MASK	0x1
#define TSTORM_FCOE_CONN_ST_CTX_SUPPORT_CONF_SHIFT	1
#define TSTORM_FCOE_CONN_ST_CTX_DEF_Q_IDX_MASK		0x3F
#define TSTORM_FCOE_CONN_ST_CTX_DEF_Q_IDX_SHIFT		2
	u8 timers_cleanup_invocation_cnt;
	__le32 reserved1[2];
	__le32 dst_mac_address_bytes_0_to_3;
	__le16 dst_mac_address_bytes_4_to_5;
	__le16 ramrod_echo;
	u8 flags1;
#define TSTORM_FCOE_CONN_ST_CTX_MODE_MASK	0x3
#define TSTORM_FCOE_CONN_ST_CTX_MODE_SHIFT	0
#define TSTORM_FCOE_CONN_ST_CTX_RESERVED_MASK	0x3F
#define TSTORM_FCOE_CONN_ST_CTX_RESERVED_SHIFT	2
	u8 cq_relative_offset;
	u8 cmdq_relative_offset;
	u8 bdq_resource_id;
	u8 reserved0[4];
};

struct mstorm_fcoe_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define MSTORM_FCOE_CONN_AG_CTX_BIT0_MASK	0x1
#define MSTORM_FCOE_CONN_AG_CTX_BIT0_SHIFT	0
#define MSTORM_FCOE_CONN_AG_CTX_BIT1_MASK	0x1
#define MSTORM_FCOE_CONN_AG_CTX_BIT1_SHIFT	1
#define MSTORM_FCOE_CONN_AG_CTX_CF0_MASK	0x3
#define MSTORM_FCOE_CONN_AG_CTX_CF0_SHIFT	2
#define MSTORM_FCOE_CONN_AG_CTX_CF1_MASK	0x3
#define MSTORM_FCOE_CONN_AG_CTX_CF1_SHIFT	4
#define MSTORM_FCOE_CONN_AG_CTX_CF2_MASK	0x3
#define MSTORM_FCOE_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define MSTORM_FCOE_CONN_AG_CTX_CF0EN_MASK		0x1
#define MSTORM_FCOE_CONN_AG_CTX_CF0EN_SHIFT		0
#define MSTORM_FCOE_CONN_AG_CTX_CF1EN_MASK		0x1
#define MSTORM_FCOE_CONN_AG_CTX_CF1EN_SHIFT		1
#define MSTORM_FCOE_CONN_AG_CTX_CF2EN_MASK		0x1
#define MSTORM_FCOE_CONN_AG_CTX_CF2EN_SHIFT		2
#define MSTORM_FCOE_CONN_AG_CTX_RULE0EN_MASK		0x1
#define MSTORM_FCOE_CONN_AG_CTX_RULE0EN_SHIFT	3
#define MSTORM_FCOE_CONN_AG_CTX_RULE1EN_MASK		0x1
#define MSTORM_FCOE_CONN_AG_CTX_RULE1EN_SHIFT	4
#define MSTORM_FCOE_CONN_AG_CTX_RULE2EN_MASK		0x1
#define MSTORM_FCOE_CONN_AG_CTX_RULE2EN_SHIFT	5
#define MSTORM_FCOE_CONN_AG_CTX_RULE3EN_MASK		0x1
#define MSTORM_FCOE_CONN_AG_CTX_RULE3EN_SHIFT	6
#define MSTORM_FCOE_CONN_AG_CTX_RULE4EN_MASK		0x1
#define MSTORM_FCOE_CONN_AG_CTX_RULE4EN_SHIFT	7
	__le16 word0;
	__le16 word1;
	__le32 reg0;
	__le32 reg1;
};


struct fcoe_mstorm_fcoe_conn_st_ctx_fp {
	__le16 xfer_prod;
	u8 num_cqs;
	u8 reserved1;
	u8 protection_info;
#define FCOE_MSTORM_FCOE_CONN_ST_CTX_FP_SUPPORT_PROTECTION_MASK  0x1
#define FCOE_MSTORM_FCOE_CONN_ST_CTX_FP_SUPPORT_PROTECTION_SHIFT 0
#define FCOE_MSTORM_FCOE_CONN_ST_CTX_FP_VALID_MASK               0x1
#define FCOE_MSTORM_FCOE_CONN_ST_CTX_FP_VALID_SHIFT              1
#define FCOE_MSTORM_FCOE_CONN_ST_CTX_FP_RESERVED0_MASK           0x3F
#define FCOE_MSTORM_FCOE_CONN_ST_CTX_FP_RESERVED0_SHIFT          2
	u8 q_relative_offset;
	u8 reserved2[2];
};


struct fcoe_mstorm_fcoe_conn_st_ctx_non_fp {
	__le16 conn_id;
	__le16 stat_ram_addr;
	__le16 num_pages_in_pbl;
	u8 ptu_log_page_size;
	u8 log_page_size;
	__le16 unsolicited_cq_count;
	__le16 cmdq_count;
	u8 bdq_resource_id;
	u8 reserved0[3];
	struct regpair xferq_pbl_addr;
	struct regpair reserved1;
	struct regpair reserved2[3];
};


struct mstorm_fcoe_conn_st_ctx {
	struct fcoe_mstorm_fcoe_conn_st_ctx_fp fp;
	struct fcoe_mstorm_fcoe_conn_st_ctx_non_fp non_fp;
};


struct fcoe_conn_context {
	struct ystorm_fcoe_conn_st_ctx ystorm_st_context;
	struct pstorm_fcoe_conn_st_ctx pstorm_st_context;
	struct regpair pstorm_st_padding[2];
	struct xstorm_fcoe_conn_st_ctx xstorm_st_context;
	struct xstorm_fcoe_conn_ag_ctx xstorm_ag_context;
	struct regpair xstorm_ag_padding[6];
	struct ustorm_fcoe_conn_st_ctx ustorm_st_context;
	struct regpair ustorm_st_padding[2];
	struct tstorm_fcoe_conn_ag_ctx tstorm_ag_context;
	struct regpair tstorm_ag_padding[2];
	struct timers_context timer_context;
	struct ustorm_fcoe_conn_ag_ctx ustorm_ag_context;
	struct tstorm_fcoe_conn_st_ctx tstorm_st_context;
	struct mstorm_fcoe_conn_ag_ctx mstorm_ag_context;
	struct mstorm_fcoe_conn_st_ctx mstorm_st_context;
};


struct fcoe_conn_offload_ramrod_params {
	struct fcoe_conn_offload_ramrod_data offload_ramrod_data;
};


struct fcoe_conn_terminate_ramrod_params {
	struct fcoe_conn_terminate_ramrod_data terminate_ramrod_data;
};


enum fcoe_event_type {
	FCOE_EVENT_INIT_FUNC,
	FCOE_EVENT_DESTROY_FUNC,
	FCOE_EVENT_STAT_FUNC,
	FCOE_EVENT_OFFLOAD_CONN,
	FCOE_EVENT_TERMINATE_CONN,
	FCOE_EVENT_ERROR,
	MAX_FCOE_EVENT_TYPE
};


struct fcoe_init_ramrod_params {
	struct fcoe_init_func_ramrod_data init_ramrod_data;
};


enum fcoe_ramrod_cmd_id {
	FCOE_RAMROD_CMD_ID_INIT_FUNC,
	FCOE_RAMROD_CMD_ID_DESTROY_FUNC,
	FCOE_RAMROD_CMD_ID_STAT_FUNC,
	FCOE_RAMROD_CMD_ID_OFFLOAD_CONN,
	FCOE_RAMROD_CMD_ID_TERMINATE_CONN,
	MAX_FCOE_RAMROD_CMD_ID
};


struct fcoe_stat_ramrod_params {
	struct fcoe_stat_ramrod_data stat_ramrod_data;
};

struct ystorm_fcoe_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define YSTORM_FCOE_CONN_AG_CTX_BIT0_MASK	0x1
#define YSTORM_FCOE_CONN_AG_CTX_BIT0_SHIFT	0
#define YSTORM_FCOE_CONN_AG_CTX_BIT1_MASK	0x1
#define YSTORM_FCOE_CONN_AG_CTX_BIT1_SHIFT	1
#define YSTORM_FCOE_CONN_AG_CTX_CF0_MASK	0x3
#define YSTORM_FCOE_CONN_AG_CTX_CF0_SHIFT	2
#define YSTORM_FCOE_CONN_AG_CTX_CF1_MASK	0x3
#define YSTORM_FCOE_CONN_AG_CTX_CF1_SHIFT	4
#define YSTORM_FCOE_CONN_AG_CTX_CF2_MASK	0x3
#define YSTORM_FCOE_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define YSTORM_FCOE_CONN_AG_CTX_CF0EN_MASK		0x1
#define YSTORM_FCOE_CONN_AG_CTX_CF0EN_SHIFT		0
#define YSTORM_FCOE_CONN_AG_CTX_CF1EN_MASK		0x1
#define YSTORM_FCOE_CONN_AG_CTX_CF1EN_SHIFT		1
#define YSTORM_FCOE_CONN_AG_CTX_CF2EN_MASK		0x1
#define YSTORM_FCOE_CONN_AG_CTX_CF2EN_SHIFT		2
#define YSTORM_FCOE_CONN_AG_CTX_RULE0EN_MASK		0x1
#define YSTORM_FCOE_CONN_AG_CTX_RULE0EN_SHIFT	3
#define YSTORM_FCOE_CONN_AG_CTX_RULE1EN_MASK		0x1
#define YSTORM_FCOE_CONN_AG_CTX_RULE1EN_SHIFT	4
#define YSTORM_FCOE_CONN_AG_CTX_RULE2EN_MASK		0x1
#define YSTORM_FCOE_CONN_AG_CTX_RULE2EN_SHIFT	5
#define YSTORM_FCOE_CONN_AG_CTX_RULE3EN_MASK		0x1
#define YSTORM_FCOE_CONN_AG_CTX_RULE3EN_SHIFT	6
#define YSTORM_FCOE_CONN_AG_CTX_RULE4EN_MASK		0x1
#define YSTORM_FCOE_CONN_AG_CTX_RULE4EN_SHIFT	7
	u8 byte2;
	u8 byte3;
	__le16 word0;
	__le32 reg0;
	__le32 reg1;
	__le16 word1;
	__le16 word2;
	__le16 word3;
	__le16 word4;
	__le32 reg2;
	__le32 reg3;
};


struct ystorm_iscsi_conn_st_ctx {
	__le32 reserved[8];
};


struct pstorm_iscsi_tcp_conn_st_ctx {
	__le32 tcp[32];
	__le32 iscsi[4];
};


struct xstorm_iscsi_tcp_conn_st_ctx {
	__le32 reserved_tcp[4];
	__le32 reserved_iscsi[44];
};

struct xstorm_iscsi_conn_ag_ctx {
	u8 cdu_validation;
	u8 state;
	u8 flags0;
#define XSTORM_ISCSI_CONN_AG_CTX_EXIST_IN_QM0_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_EXIST_IN_QM0_SHIFT	0
#define XSTORM_ISCSI_CONN_AG_CTX_EXIST_IN_QM1_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_EXIST_IN_QM1_SHIFT	1
#define XSTORM_ISCSI_CONN_AG_CTX_RESERVED1_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_RESERVED1_SHIFT	2
#define XSTORM_ISCSI_CONN_AG_CTX_EXIST_IN_QM3_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_EXIST_IN_QM3_SHIFT	3
#define XSTORM_ISCSI_CONN_AG_CTX_BIT4_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT4_SHIFT		4
#define XSTORM_ISCSI_CONN_AG_CTX_RESERVED2_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_RESERVED2_SHIFT	5
#define XSTORM_ISCSI_CONN_AG_CTX_BIT6_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT6_SHIFT		6
#define XSTORM_ISCSI_CONN_AG_CTX_BIT7_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT7_SHIFT		7
	u8 flags1;
#define XSTORM_ISCSI_CONN_AG_CTX_BIT8_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT8_SHIFT		0
#define XSTORM_ISCSI_CONN_AG_CTX_BIT9_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT9_SHIFT		1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT10_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT10_SHIFT		2
#define XSTORM_ISCSI_CONN_AG_CTX_BIT11_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT11_SHIFT		3
#define XSTORM_ISCSI_CONN_AG_CTX_BIT12_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT12_SHIFT		4
#define XSTORM_ISCSI_CONN_AG_CTX_BIT13_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT13_SHIFT		5
#define XSTORM_ISCSI_CONN_AG_CTX_BIT14_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT14_SHIFT		6
#define XSTORM_ISCSI_CONN_AG_CTX_TX_TRUNCATE_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_TX_TRUNCATE_SHIFT	7
	u8 flags2;
#define XSTORM_ISCSI_CONN_AG_CTX_CF0_MASK			0x3
#define XSTORM_ISCSI_CONN_AG_CTX_CF0_SHIFT			0
#define XSTORM_ISCSI_CONN_AG_CTX_CF1_MASK			0x3
#define XSTORM_ISCSI_CONN_AG_CTX_CF1_SHIFT			2
#define XSTORM_ISCSI_CONN_AG_CTX_CF2_MASK			0x3
#define XSTORM_ISCSI_CONN_AG_CTX_CF2_SHIFT			4
#define XSTORM_ISCSI_CONN_AG_CTX_TIMER_STOP_ALL_MASK		0x3
#define XSTORM_ISCSI_CONN_AG_CTX_TIMER_STOP_ALL_SHIFT	6
	u8 flags3;
#define XSTORM_ISCSI_CONN_AG_CTX_CF4_MASK	0x3
#define XSTORM_ISCSI_CONN_AG_CTX_CF4_SHIFT	0
#define XSTORM_ISCSI_CONN_AG_CTX_CF5_MASK	0x3
#define XSTORM_ISCSI_CONN_AG_CTX_CF5_SHIFT	2
#define XSTORM_ISCSI_CONN_AG_CTX_CF6_MASK	0x3
#define XSTORM_ISCSI_CONN_AG_CTX_CF6_SHIFT	4
#define XSTORM_ISCSI_CONN_AG_CTX_CF7_MASK	0x3
#define XSTORM_ISCSI_CONN_AG_CTX_CF7_SHIFT	6
	u8 flags4;
#define XSTORM_ISCSI_CONN_AG_CTX_CF8_MASK	0x3
#define XSTORM_ISCSI_CONN_AG_CTX_CF8_SHIFT	0
#define XSTORM_ISCSI_CONN_AG_CTX_CF9_MASK	0x3
#define XSTORM_ISCSI_CONN_AG_CTX_CF9_SHIFT	2
#define XSTORM_ISCSI_CONN_AG_CTX_CF10_MASK	0x3
#define XSTORM_ISCSI_CONN_AG_CTX_CF10_SHIFT	4
#define XSTORM_ISCSI_CONN_AG_CTX_CF11_MASK	0x3
#define XSTORM_ISCSI_CONN_AG_CTX_CF11_SHIFT	6
	u8 flags5;
#define XSTORM_ISCSI_CONN_AG_CTX_CF12_MASK				0x3
#define XSTORM_ISCSI_CONN_AG_CTX_CF12_SHIFT				0
#define XSTORM_ISCSI_CONN_AG_CTX_CF13_MASK				0x3
#define XSTORM_ISCSI_CONN_AG_CTX_CF13_SHIFT				2
#define XSTORM_ISCSI_CONN_AG_CTX_CF14_MASK				0x3
#define XSTORM_ISCSI_CONN_AG_CTX_CF14_SHIFT				4
#define XSTORM_ISCSI_CONN_AG_CTX_UPDATE_STATE_TO_BASE_CF_MASK	0x3
#define XSTORM_ISCSI_CONN_AG_CTX_UPDATE_STATE_TO_BASE_CF_SHIFT	6
	u8 flags6;
#define XSTORM_ISCSI_CONN_AG_CTX_CF16_MASK		0x3
#define XSTORM_ISCSI_CONN_AG_CTX_CF16_SHIFT		0
#define XSTORM_ISCSI_CONN_AG_CTX_CF17_MASK		0x3
#define XSTORM_ISCSI_CONN_AG_CTX_CF17_SHIFT		2
#define XSTORM_ISCSI_CONN_AG_CTX_CF18_MASK		0x3
#define XSTORM_ISCSI_CONN_AG_CTX_CF18_SHIFT		4
#define XSTORM_ISCSI_CONN_AG_CTX_DQ_FLUSH_MASK	0x3
#define XSTORM_ISCSI_CONN_AG_CTX_DQ_FLUSH_SHIFT	6
	u8 flags7;
#define XSTORM_ISCSI_CONN_AG_CTX_MST_XCM_Q0_FLUSH_CF_MASK	0x3
#define XSTORM_ISCSI_CONN_AG_CTX_MST_XCM_Q0_FLUSH_CF_SHIFT	0
#define XSTORM_ISCSI_CONN_AG_CTX_UST_XCM_Q1_FLUSH_CF_MASK	0x3
#define XSTORM_ISCSI_CONN_AG_CTX_UST_XCM_Q1_FLUSH_CF_SHIFT	2
#define XSTORM_ISCSI_CONN_AG_CTX_SLOW_PATH_MASK		0x3
#define XSTORM_ISCSI_CONN_AG_CTX_SLOW_PATH_SHIFT		4
#define XSTORM_ISCSI_CONN_AG_CTX_CF0EN_MASK			0x1
#define XSTORM_ISCSI_CONN_AG_CTX_CF0EN_SHIFT			6
#define XSTORM_ISCSI_CONN_AG_CTX_CF1EN_MASK			0x1
#define XSTORM_ISCSI_CONN_AG_CTX_CF1EN_SHIFT			7
	u8 flags8;
#define XSTORM_ISCSI_CONN_AG_CTX_CF2EN_MASK			0x1
#define XSTORM_ISCSI_CONN_AG_CTX_CF2EN_SHIFT			0
#define XSTORM_ISCSI_CONN_AG_CTX_TIMER_STOP_ALL_EN_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_TIMER_STOP_ALL_EN_SHIFT	1
#define XSTORM_ISCSI_CONN_AG_CTX_CF4EN_MASK			0x1
#define XSTORM_ISCSI_CONN_AG_CTX_CF4EN_SHIFT			2
#define XSTORM_ISCSI_CONN_AG_CTX_CF5EN_MASK			0x1
#define XSTORM_ISCSI_CONN_AG_CTX_CF5EN_SHIFT			3
#define XSTORM_ISCSI_CONN_AG_CTX_CF6EN_MASK			0x1
#define XSTORM_ISCSI_CONN_AG_CTX_CF6EN_SHIFT			4
#define XSTORM_ISCSI_CONN_AG_CTX_CF7EN_MASK			0x1
#define XSTORM_ISCSI_CONN_AG_CTX_CF7EN_SHIFT			5
#define XSTORM_ISCSI_CONN_AG_CTX_CF8EN_MASK			0x1
#define XSTORM_ISCSI_CONN_AG_CTX_CF8EN_SHIFT			6
#define XSTORM_ISCSI_CONN_AG_CTX_CF9EN_MASK			0x1
#define XSTORM_ISCSI_CONN_AG_CTX_CF9EN_SHIFT			7
	u8 flags9;
#define XSTORM_ISCSI_CONN_AG_CTX_CF10EN_MASK				0x1
#define XSTORM_ISCSI_CONN_AG_CTX_CF10EN_SHIFT			0
#define XSTORM_ISCSI_CONN_AG_CTX_CF11EN_MASK				0x1
#define XSTORM_ISCSI_CONN_AG_CTX_CF11EN_SHIFT			1
#define XSTORM_ISCSI_CONN_AG_CTX_CF12EN_MASK				0x1
#define XSTORM_ISCSI_CONN_AG_CTX_CF12EN_SHIFT			2
#define XSTORM_ISCSI_CONN_AG_CTX_CF13EN_MASK				0x1
#define XSTORM_ISCSI_CONN_AG_CTX_CF13EN_SHIFT			3
#define XSTORM_ISCSI_CONN_AG_CTX_CF14EN_MASK				0x1
#define XSTORM_ISCSI_CONN_AG_CTX_CF14EN_SHIFT			4
#define XSTORM_ISCSI_CONN_AG_CTX_UPDATE_STATE_TO_BASE_CF_EN_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_UPDATE_STATE_TO_BASE_CF_EN_SHIFT	5
#define XSTORM_ISCSI_CONN_AG_CTX_CF16EN_MASK				0x1
#define XSTORM_ISCSI_CONN_AG_CTX_CF16EN_SHIFT			6
#define XSTORM_ISCSI_CONN_AG_CTX_CF17EN_MASK				0x1
#define XSTORM_ISCSI_CONN_AG_CTX_CF17EN_SHIFT			7
	u8 flags10;
#define XSTORM_ISCSI_CONN_AG_CTX_CF18EN_MASK				0x1
#define XSTORM_ISCSI_CONN_AG_CTX_CF18EN_SHIFT			0
#define XSTORM_ISCSI_CONN_AG_CTX_DQ_FLUSH_EN_MASK			0x1
#define XSTORM_ISCSI_CONN_AG_CTX_DQ_FLUSH_EN_SHIFT			1
#define XSTORM_ISCSI_CONN_AG_CTX_MST_XCM_Q0_FLUSH_CF_EN_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_MST_XCM_Q0_FLUSH_CF_EN_SHIFT	2
#define XSTORM_ISCSI_CONN_AG_CTX_UST_XCM_Q1_FLUSH_CF_EN_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_UST_XCM_Q1_FLUSH_CF_EN_SHIFT	3
#define XSTORM_ISCSI_CONN_AG_CTX_SLOW_PATH_EN_MASK			0x1
#define XSTORM_ISCSI_CONN_AG_CTX_SLOW_PATH_EN_SHIFT			4
#define XSTORM_ISCSI_CONN_AG_CTX_PROC_ONLY_CLEANUP_EN_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_PROC_ONLY_CLEANUP_EN_SHIFT		5
#define XSTORM_ISCSI_CONN_AG_CTX_RULE0EN_MASK			0x1
#define XSTORM_ISCSI_CONN_AG_CTX_RULE0EN_SHIFT			6
#define XSTORM_ISCSI_CONN_AG_CTX_MORE_TO_SEND_DEC_RULE_EN_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_MORE_TO_SEND_DEC_RULE_EN_SHIFT	7
	u8 flags11;
#define XSTORM_ISCSI_CONN_AG_CTX_TX_BLOCKED_EN_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_TX_BLOCKED_EN_SHIFT	0
#define XSTORM_ISCSI_CONN_AG_CTX_RULE3EN_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_RULE3EN_SHIFT	1
#define XSTORM_ISCSI_CONN_AG_CTX_RESERVED3_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_RESERVED3_SHIFT	2
#define XSTORM_ISCSI_CONN_AG_CTX_RULE5EN_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_RULE5EN_SHIFT	3
#define XSTORM_ISCSI_CONN_AG_CTX_RULE6EN_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_RULE6EN_SHIFT	4
#define XSTORM_ISCSI_CONN_AG_CTX_RULE7EN_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_RULE7EN_SHIFT	5
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED1_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED1_SHIFT	6
#define XSTORM_ISCSI_CONN_AG_CTX_RULE9EN_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_RULE9EN_SHIFT	7
	u8 flags12;
#define XSTORM_ISCSI_CONN_AG_CTX_SQ_DEC_RULE_EN_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_SQ_DEC_RULE_EN_SHIFT	0
#define XSTORM_ISCSI_CONN_AG_CTX_RULE11EN_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_RULE11EN_SHIFT		1
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED2_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED2_SHIFT		2
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED3_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED3_SHIFT		3
#define XSTORM_ISCSI_CONN_AG_CTX_RULE14EN_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_RULE14EN_SHIFT		4
#define XSTORM_ISCSI_CONN_AG_CTX_RULE15EN_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_RULE15EN_SHIFT		5
#define XSTORM_ISCSI_CONN_AG_CTX_RULE16EN_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_RULE16EN_SHIFT		6
#define XSTORM_ISCSI_CONN_AG_CTX_RULE17EN_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_RULE17EN_SHIFT		7
	u8 flags13;
#define XSTORM_ISCSI_CONN_AG_CTX_R2TQ_DEC_RULE_EN_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_R2TQ_DEC_RULE_EN_SHIFT	0
#define XSTORM_ISCSI_CONN_AG_CTX_HQ_DEC_RULE_EN_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_HQ_DEC_RULE_EN_SHIFT	1
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED4_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED4_SHIFT		2
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED5_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED5_SHIFT		3
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED6_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED6_SHIFT		4
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED7_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED7_SHIFT		5
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED8_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED8_SHIFT		6
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED9_MASK		0x1
#define XSTORM_ISCSI_CONN_AG_CTX_A0_RESERVED9_SHIFT		7
	u8 flags14;
#define XSTORM_ISCSI_CONN_AG_CTX_BIT16_MASK			0x1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT16_SHIFT			0
#define XSTORM_ISCSI_CONN_AG_CTX_BIT17_MASK			0x1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT17_SHIFT			1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT18_MASK			0x1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT18_SHIFT			2
#define XSTORM_ISCSI_CONN_AG_CTX_BIT19_MASK			0x1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT19_SHIFT			3
#define XSTORM_ISCSI_CONN_AG_CTX_BIT20_MASK			0x1
#define XSTORM_ISCSI_CONN_AG_CTX_BIT20_SHIFT			4
#define XSTORM_ISCSI_CONN_AG_CTX_DUMMY_READ_DONE_MASK	0x1
#define XSTORM_ISCSI_CONN_AG_CTX_DUMMY_READ_DONE_SHIFT	5
#define XSTORM_ISCSI_CONN_AG_CTX_PROC_ONLY_CLEANUP_MASK	0x3
#define XSTORM_ISCSI_CONN_AG_CTX_PROC_ONLY_CLEANUP_SHIFT	6
	u8 byte2;
	__le16 physical_q0;
	__le16 physical_q1;
	__le16 dummy_dorq_var;
	__le16 sq_cons;
	__le16 sq_prod;
	__le16 word5;
	__le16 slow_io_total_data_tx_update;
	u8 byte3;
	u8 byte4;
	u8 byte5;
	u8 byte6;
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 more_to_send_seq;
	__le32 reg4;
	__le32 reg5;
	__le32 hq_scan_next_relevant_ack;
	__le16 r2tq_prod;
	__le16 r2tq_cons;
	__le16 hq_prod;
	__le16 hq_cons;
	__le32 remain_seq;
	__le32 bytes_to_next_pdu;
	__le32 hq_tcp_seq;
	u8 byte7;
	u8 byte8;
	u8 byte9;
	u8 byte10;
	u8 byte11;
	u8 byte12;
	u8 byte13;
	u8 byte14;
	u8 byte15;
	u8 e5_reserved;
	__le16 word11;
	__le32 reg10;
	__le32 reg11;
	__le32 exp_stat_sn;
	__le32 ongoing_fast_rxmit_seq;
	__le32 reg14;
	__le32 reg15;
	__le32 reg16;
	__le32 reg17;
};

struct tstorm_iscsi_conn_ag_ctx {
	u8 reserved0;
	u8 state;
	u8 flags0;
#define TSTORM_ISCSI_CONN_AG_CTX_EXIST_IN_QM0_MASK	0x1
#define TSTORM_ISCSI_CONN_AG_CTX_EXIST_IN_QM0_SHIFT	0
#define TSTORM_ISCSI_CONN_AG_CTX_BIT1_MASK		0x1
#define TSTORM_ISCSI_CONN_AG_CTX_BIT1_SHIFT		1
#define TSTORM_ISCSI_CONN_AG_CTX_BIT2_MASK		0x1
#define TSTORM_ISCSI_CONN_AG_CTX_BIT2_SHIFT		2
#define TSTORM_ISCSI_CONN_AG_CTX_BIT3_MASK		0x1
#define TSTORM_ISCSI_CONN_AG_CTX_BIT3_SHIFT		3
#define TSTORM_ISCSI_CONN_AG_CTX_BIT4_MASK		0x1
#define TSTORM_ISCSI_CONN_AG_CTX_BIT4_SHIFT		4
#define TSTORM_ISCSI_CONN_AG_CTX_BIT5_MASK		0x1
#define TSTORM_ISCSI_CONN_AG_CTX_BIT5_SHIFT		5
#define TSTORM_ISCSI_CONN_AG_CTX_CF0_MASK		0x3
#define TSTORM_ISCSI_CONN_AG_CTX_CF0_SHIFT		6
	u8 flags1;
#define TSTORM_ISCSI_CONN_AG_CTX_P2T_FLUSH_CF_MASK		0x3
#define TSTORM_ISCSI_CONN_AG_CTX_P2T_FLUSH_CF_SHIFT		0
#define TSTORM_ISCSI_CONN_AG_CTX_M2T_FLUSH_CF_MASK		0x3
#define TSTORM_ISCSI_CONN_AG_CTX_M2T_FLUSH_CF_SHIFT		2
#define TSTORM_ISCSI_CONN_AG_CTX_TIMER_STOP_ALL_MASK		0x3
#define TSTORM_ISCSI_CONN_AG_CTX_TIMER_STOP_ALL_SHIFT	4
#define TSTORM_ISCSI_CONN_AG_CTX_CF4_MASK			0x3
#define TSTORM_ISCSI_CONN_AG_CTX_CF4_SHIFT			6
	u8 flags2;
#define TSTORM_ISCSI_CONN_AG_CTX_CF5_MASK	0x3
#define TSTORM_ISCSI_CONN_AG_CTX_CF5_SHIFT	0
#define TSTORM_ISCSI_CONN_AG_CTX_CF6_MASK	0x3
#define TSTORM_ISCSI_CONN_AG_CTX_CF6_SHIFT	2
#define TSTORM_ISCSI_CONN_AG_CTX_CF7_MASK	0x3
#define TSTORM_ISCSI_CONN_AG_CTX_CF7_SHIFT	4
#define TSTORM_ISCSI_CONN_AG_CTX_CF8_MASK	0x3
#define TSTORM_ISCSI_CONN_AG_CTX_CF8_SHIFT	6
	u8 flags3;
#define TSTORM_ISCSI_CONN_AG_CTX_FLUSH_Q0_MASK		0x3
#define TSTORM_ISCSI_CONN_AG_CTX_FLUSH_Q0_SHIFT		0
#define TSTORM_ISCSI_CONN_AG_CTX_FLUSH_OOO_ISLES_CF_MASK	0x3
#define TSTORM_ISCSI_CONN_AG_CTX_FLUSH_OOO_ISLES_CF_SHIFT	2
#define TSTORM_ISCSI_CONN_AG_CTX_CF0EN_MASK			0x1
#define TSTORM_ISCSI_CONN_AG_CTX_CF0EN_SHIFT			4
#define TSTORM_ISCSI_CONN_AG_CTX_P2T_FLUSH_CF_EN_MASK	0x1
#define TSTORM_ISCSI_CONN_AG_CTX_P2T_FLUSH_CF_EN_SHIFT	5
#define TSTORM_ISCSI_CONN_AG_CTX_M2T_FLUSH_CF_EN_MASK	0x1
#define TSTORM_ISCSI_CONN_AG_CTX_M2T_FLUSH_CF_EN_SHIFT	6
#define TSTORM_ISCSI_CONN_AG_CTX_TIMER_STOP_ALL_EN_MASK	0x1
#define TSTORM_ISCSI_CONN_AG_CTX_TIMER_STOP_ALL_EN_SHIFT	7
	u8 flags4;
#define TSTORM_ISCSI_CONN_AG_CTX_CF4EN_MASK		0x1
#define TSTORM_ISCSI_CONN_AG_CTX_CF4EN_SHIFT		0
#define TSTORM_ISCSI_CONN_AG_CTX_CF5EN_MASK		0x1
#define TSTORM_ISCSI_CONN_AG_CTX_CF5EN_SHIFT		1
#define TSTORM_ISCSI_CONN_AG_CTX_CF6EN_MASK		0x1
#define TSTORM_ISCSI_CONN_AG_CTX_CF6EN_SHIFT		2
#define TSTORM_ISCSI_CONN_AG_CTX_CF7EN_MASK		0x1
#define TSTORM_ISCSI_CONN_AG_CTX_CF7EN_SHIFT		3
#define TSTORM_ISCSI_CONN_AG_CTX_CF8EN_MASK		0x1
#define TSTORM_ISCSI_CONN_AG_CTX_CF8EN_SHIFT		4
#define TSTORM_ISCSI_CONN_AG_CTX_FLUSH_Q0_EN_MASK	0x1
#define TSTORM_ISCSI_CONN_AG_CTX_FLUSH_Q0_EN_SHIFT	5
#define TSTORM_ISCSI_CONN_AG_CTX_FLUSH_OOO_ISLES_CF_EN_MASK	0x1
#define TSTORM_ISCSI_CONN_AG_CTX_FLUSH_OOO_ISLES_CF_EN_SHIFT	6
#define TSTORM_ISCSI_CONN_AG_CTX_RULE0EN_MASK	0x1
#define TSTORM_ISCSI_CONN_AG_CTX_RULE0EN_SHIFT	7
	u8 flags5;
#define TSTORM_ISCSI_CONN_AG_CTX_RULE1EN_MASK	0x1
#define TSTORM_ISCSI_CONN_AG_CTX_RULE1EN_SHIFT	0
#define TSTORM_ISCSI_CONN_AG_CTX_RULE2EN_MASK	0x1
#define TSTORM_ISCSI_CONN_AG_CTX_RULE2EN_SHIFT	1
#define TSTORM_ISCSI_CONN_AG_CTX_RULE3EN_MASK	0x1
#define TSTORM_ISCSI_CONN_AG_CTX_RULE3EN_SHIFT	2
#define TSTORM_ISCSI_CONN_AG_CTX_RULE4EN_MASK	0x1
#define TSTORM_ISCSI_CONN_AG_CTX_RULE4EN_SHIFT	3
#define TSTORM_ISCSI_CONN_AG_CTX_RULE5EN_MASK	0x1
#define TSTORM_ISCSI_CONN_AG_CTX_RULE5EN_SHIFT	4
#define TSTORM_ISCSI_CONN_AG_CTX_RULE6EN_MASK	0x1
#define TSTORM_ISCSI_CONN_AG_CTX_RULE6EN_SHIFT	5
#define TSTORM_ISCSI_CONN_AG_CTX_RULE7EN_MASK	0x1
#define TSTORM_ISCSI_CONN_AG_CTX_RULE7EN_SHIFT	6
#define TSTORM_ISCSI_CONN_AG_CTX_RULE8EN_MASK	0x1
#define TSTORM_ISCSI_CONN_AG_CTX_RULE8EN_SHIFT	7
	__le32 reg0;
	__le32 reg1;
	__le32 rx_tcp_checksum_err_cnt;
	__le32 reg3;
	__le32 reg4;
	__le32 reg5;
	__le32 reg6;
	__le32 reg7;
	__le32 reg8;
	u8 cid_offload_cnt;
	u8 byte3;
	__le16 word0;
};

struct ustorm_iscsi_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define USTORM_ISCSI_CONN_AG_CTX_BIT0_MASK	0x1
#define USTORM_ISCSI_CONN_AG_CTX_BIT0_SHIFT	0
#define USTORM_ISCSI_CONN_AG_CTX_BIT1_MASK	0x1
#define USTORM_ISCSI_CONN_AG_CTX_BIT1_SHIFT	1
#define USTORM_ISCSI_CONN_AG_CTX_CF0_MASK	0x3
#define USTORM_ISCSI_CONN_AG_CTX_CF0_SHIFT	2
#define USTORM_ISCSI_CONN_AG_CTX_CF1_MASK	0x3
#define USTORM_ISCSI_CONN_AG_CTX_CF1_SHIFT	4
#define USTORM_ISCSI_CONN_AG_CTX_CF2_MASK	0x3
#define USTORM_ISCSI_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define USTORM_ISCSI_CONN_AG_CTX_CF3_MASK	0x3
#define USTORM_ISCSI_CONN_AG_CTX_CF3_SHIFT	0
#define USTORM_ISCSI_CONN_AG_CTX_CF4_MASK	0x3
#define USTORM_ISCSI_CONN_AG_CTX_CF4_SHIFT	2
#define USTORM_ISCSI_CONN_AG_CTX_CF5_MASK	0x3
#define USTORM_ISCSI_CONN_AG_CTX_CF5_SHIFT	4
#define USTORM_ISCSI_CONN_AG_CTX_CF6_MASK	0x3
#define USTORM_ISCSI_CONN_AG_CTX_CF6_SHIFT	6
	u8 flags2;
#define USTORM_ISCSI_CONN_AG_CTX_CF0EN_MASK		0x1
#define USTORM_ISCSI_CONN_AG_CTX_CF0EN_SHIFT		0
#define USTORM_ISCSI_CONN_AG_CTX_CF1EN_MASK		0x1
#define USTORM_ISCSI_CONN_AG_CTX_CF1EN_SHIFT		1
#define USTORM_ISCSI_CONN_AG_CTX_CF2EN_MASK		0x1
#define USTORM_ISCSI_CONN_AG_CTX_CF2EN_SHIFT		2
#define USTORM_ISCSI_CONN_AG_CTX_CF3EN_MASK		0x1
#define USTORM_ISCSI_CONN_AG_CTX_CF3EN_SHIFT		3
#define USTORM_ISCSI_CONN_AG_CTX_CF4EN_MASK		0x1
#define USTORM_ISCSI_CONN_AG_CTX_CF4EN_SHIFT		4
#define USTORM_ISCSI_CONN_AG_CTX_CF5EN_MASK		0x1
#define USTORM_ISCSI_CONN_AG_CTX_CF5EN_SHIFT		5
#define USTORM_ISCSI_CONN_AG_CTX_CF6EN_MASK		0x1
#define USTORM_ISCSI_CONN_AG_CTX_CF6EN_SHIFT		6
#define USTORM_ISCSI_CONN_AG_CTX_RULE0EN_MASK	0x1
#define USTORM_ISCSI_CONN_AG_CTX_RULE0EN_SHIFT	7
	u8 flags3;
#define USTORM_ISCSI_CONN_AG_CTX_RULE1EN_MASK	0x1
#define USTORM_ISCSI_CONN_AG_CTX_RULE1EN_SHIFT	0
#define USTORM_ISCSI_CONN_AG_CTX_RULE2EN_MASK	0x1
#define USTORM_ISCSI_CONN_AG_CTX_RULE2EN_SHIFT	1
#define USTORM_ISCSI_CONN_AG_CTX_RULE3EN_MASK	0x1
#define USTORM_ISCSI_CONN_AG_CTX_RULE3EN_SHIFT	2
#define USTORM_ISCSI_CONN_AG_CTX_RULE4EN_MASK	0x1
#define USTORM_ISCSI_CONN_AG_CTX_RULE4EN_SHIFT	3
#define USTORM_ISCSI_CONN_AG_CTX_RULE5EN_MASK	0x1
#define USTORM_ISCSI_CONN_AG_CTX_RULE5EN_SHIFT	4
#define USTORM_ISCSI_CONN_AG_CTX_RULE6EN_MASK	0x1
#define USTORM_ISCSI_CONN_AG_CTX_RULE6EN_SHIFT	5
#define USTORM_ISCSI_CONN_AG_CTX_RULE7EN_MASK	0x1
#define USTORM_ISCSI_CONN_AG_CTX_RULE7EN_SHIFT	6
#define USTORM_ISCSI_CONN_AG_CTX_RULE8EN_MASK	0x1
#define USTORM_ISCSI_CONN_AG_CTX_RULE8EN_SHIFT	7
	u8 byte2;
	u8 byte3;
	__le16 word0;
	__le16 word1;
	__le32 reg0;
	__le32 reg1;
	__le32 reg2;
	__le32 reg3;
	__le16 word2;
	__le16 word3;
};


struct tstorm_iscsi_conn_st_ctx {
	__le32 reserved[44];
};

struct mstorm_iscsi_conn_ag_ctx {
	u8 reserved;
	u8 state;
	u8 flags0;
#define MSTORM_ISCSI_CONN_AG_CTX_BIT0_MASK	0x1
#define MSTORM_ISCSI_CONN_AG_CTX_BIT0_SHIFT	0
#define MSTORM_ISCSI_CONN_AG_CTX_BIT1_MASK	0x1
#define MSTORM_ISCSI_CONN_AG_CTX_BIT1_SHIFT	1
#define MSTORM_ISCSI_CONN_AG_CTX_CF0_MASK	0x3
#define MSTORM_ISCSI_CONN_AG_CTX_CF0_SHIFT	2
#define MSTORM_ISCSI_CONN_AG_CTX_CF1_MASK	0x3
#define MSTORM_ISCSI_CONN_AG_CTX_CF1_SHIFT	4
#define MSTORM_ISCSI_CONN_AG_CTX_CF2_MASK	0x3
#define MSTORM_ISCSI_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define MSTORM_ISCSI_CONN_AG_CTX_CF0EN_MASK		0x1
#define MSTORM_ISCSI_CONN_AG_CTX_CF0EN_SHIFT		0
#define MSTORM_ISCSI_CONN_AG_CTX_CF1EN_MASK		0x1
#define MSTORM_ISCSI_CONN_AG_CTX_CF1EN_SHIFT		1
#define MSTORM_ISCSI_CONN_AG_CTX_CF2EN_MASK		0x1
#define MSTORM_ISCSI_CONN_AG_CTX_CF2EN_SHIFT		2
#define MSTORM_ISCSI_CONN_AG_CTX_RULE0EN_MASK	0x1
#define MSTORM_ISCSI_CONN_AG_CTX_RULE0EN_SHIFT	3
#define MSTORM_ISCSI_CONN_AG_CTX_RULE1EN_MASK	0x1
#define MSTORM_ISCSI_CONN_AG_CTX_RULE1EN_SHIFT	4
#define MSTORM_ISCSI_CONN_AG_CTX_RULE2EN_MASK	0x1
#define MSTORM_ISCSI_CONN_AG_CTX_RULE2EN_SHIFT	5
#define MSTORM_ISCSI_CONN_AG_CTX_RULE3EN_MASK	0x1
#define MSTORM_ISCSI_CONN_AG_CTX_RULE3EN_SHIFT	6
#define MSTORM_ISCSI_CONN_AG_CTX_RULE4EN_MASK	0x1
#define MSTORM_ISCSI_CONN_AG_CTX_RULE4EN_SHIFT	7
	__le16 word0;
	__le16 word1;
	__le32 reg0;
	__le32 reg1;
};


struct mstorm_iscsi_tcp_conn_st_ctx {
	__le32 reserved_tcp[20];
	__le32 reserved_iscsi[12];
};


struct ustorm_iscsi_conn_st_ctx {
	__le32 reserved[52];
};


struct iscsi_conn_context {
	struct ystorm_iscsi_conn_st_ctx ystorm_st_context;
	struct pstorm_iscsi_tcp_conn_st_ctx pstorm_st_context;
	struct regpair pstorm_st_padding[2];
	struct pb_context xpb2_context;
	struct xstorm_iscsi_tcp_conn_st_ctx xstorm_st_context;
	struct regpair xstorm_st_padding[2];
	struct xstorm_iscsi_conn_ag_ctx xstorm_ag_context;
	struct tstorm_iscsi_conn_ag_ctx tstorm_ag_context;
	struct regpair tstorm_ag_padding[2];
	struct timers_context timer_context;
	struct ustorm_iscsi_conn_ag_ctx ustorm_ag_context;
	struct pb_context upb_context;
	struct tstorm_iscsi_conn_st_ctx tstorm_st_context;
	struct regpair tstorm_st_padding[2];
	struct mstorm_iscsi_conn_ag_ctx mstorm_ag_context;
	struct mstorm_iscsi_tcp_conn_st_ctx mstorm_st_context;
	struct ustorm_iscsi_conn_st_ctx ustorm_st_context;
};


struct iscsi_init_ramrod_params {
	struct iscsi_spe_func_init iscsi_init_spe;
	struct tcp_init_params tcp_init;
};

struct ystorm_iscsi_conn_ag_ctx {
	u8 byte0;
	u8 byte1;
	u8 flags0;
#define YSTORM_ISCSI_CONN_AG_CTX_BIT0_MASK	0x1
#define YSTORM_ISCSI_CONN_AG_CTX_BIT0_SHIFT	0
#define YSTORM_ISCSI_CONN_AG_CTX_BIT1_MASK	0x1
#define YSTORM_ISCSI_CONN_AG_CTX_BIT1_SHIFT	1
#define YSTORM_ISCSI_CONN_AG_CTX_CF0_MASK	0x3
#define YSTORM_ISCSI_CONN_AG_CTX_CF0_SHIFT	2
#define YSTORM_ISCSI_CONN_AG_CTX_CF1_MASK	0x3
#define YSTORM_ISCSI_CONN_AG_CTX_CF1_SHIFT	4
#define YSTORM_ISCSI_CONN_AG_CTX_CF2_MASK	0x3
#define YSTORM_ISCSI_CONN_AG_CTX_CF2_SHIFT	6
	u8 flags1;
#define YSTORM_ISCSI_CONN_AG_CTX_CF0EN_MASK		0x1
#define YSTORM_ISCSI_CONN_AG_CTX_CF0EN_SHIFT		0
#define YSTORM_ISCSI_CONN_AG_CTX_CF1EN_MASK		0x1
#define YSTORM_ISCSI_CONN_AG_CTX_CF1EN_SHIFT		1
#define YSTORM_ISCSI_CONN_AG_CTX_CF2EN_MASK		0x1
#define YSTORM_ISCSI_CONN_AG_CTX_CF2EN_SHIFT		2
#define YSTORM_ISCSI_CONN_AG_CTX_RULE0EN_MASK	0x1
#define YSTORM_ISCSI_CONN_AG_CTX_RULE0EN_SHIFT	3
#define YSTORM_ISCSI_CONN_AG_CTX_RULE1EN_MASK	0x1
#define YSTORM_ISCSI_CONN_AG_CTX_RULE1EN_SHIFT	4
#define YSTORM_ISCSI_CONN_AG_CTX_RULE2EN_MASK	0x1
#define YSTORM_ISCSI_CONN_AG_CTX_RULE2EN_SHIFT	5
#define YSTORM_ISCSI_CONN_AG_CTX_RULE3EN_MASK	0x1
#define YSTORM_ISCSI_CONN_AG_CTX_RULE3EN_SHIFT	6
#define YSTORM_ISCSI_CONN_AG_CTX_RULE4EN_MASK	0x1
#define YSTORM_ISCSI_CONN_AG_CTX_RULE4EN_SHIFT	7
	u8 byte2;
	u8 byte3;
	__le16 word0;
	__le32 reg0;
	__le32 reg1;
	__le16 word1;
	__le16 word2;
	__le16 word3;
	__le16 word4;
	__le32 reg2;
	__le32 reg3;
};

#endif
