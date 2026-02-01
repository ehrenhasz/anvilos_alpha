 
 
#ifndef __iwl_fw_api_tx_h__
#define __iwl_fw_api_tx_h__
#include <linux/ieee80211.h>

 
enum iwl_tx_flags {
	TX_CMD_FLG_PROT_REQUIRE		= BIT(0),
	TX_CMD_FLG_WRITE_TX_POWER	= BIT(1),
	TX_CMD_FLG_ACK			= BIT(3),
	TX_CMD_FLG_STA_RATE		= BIT(4),
	TX_CMD_FLG_BAR			= BIT(6),
	TX_CMD_FLG_TXOP_PROT		= BIT(7),
	TX_CMD_FLG_VHT_NDPA		= BIT(8),
	TX_CMD_FLG_HT_NDPA		= BIT(9),
	TX_CMD_FLG_CSI_FDBK2HOST	= BIT(10),
	TX_CMD_FLG_BT_PRIO_POS		= 11,
	TX_CMD_FLG_BT_PRIO_MASK		= BIT(11) | BIT(12),
	TX_CMD_FLG_BT_DIS		= BIT(12),
	TX_CMD_FLG_SEQ_CTL		= BIT(13),
	TX_CMD_FLG_MORE_FRAG		= BIT(14),
	TX_CMD_FLG_TSF			= BIT(16),
	TX_CMD_FLG_CALIB		= BIT(17),
	TX_CMD_FLG_KEEP_SEQ_CTL		= BIT(18),
	TX_CMD_FLG_MH_PAD		= BIT(20),
	TX_CMD_FLG_RESP_TO_DRV		= BIT(21),
	TX_CMD_FLG_TKIP_MIC_DONE	= BIT(23),
	TX_CMD_FLG_DUR			= BIT(25),
	TX_CMD_FLG_FW_DROP		= BIT(26),
	TX_CMD_FLG_EXEC_PAPD		= BIT(27),
	TX_CMD_FLG_PAPD_TYPE		= BIT(28),
	TX_CMD_FLG_HCCA_CHUNK		= BIT(31)
};  

 
enum iwl_tx_cmd_flags {
	IWL_TX_FLAGS_CMD_RATE		= BIT(0),
	IWL_TX_FLAGS_ENCRYPT_DIS	= BIT(1),
	IWL_TX_FLAGS_HIGH_PRI		= BIT(2),
	 
	IWL_TX_FLAGS_RTS		= BIT(3),
	IWL_TX_FLAGS_CTS		= BIT(4),
};  

 
enum iwl_tx_pm_timeouts {
	PM_FRAME_NONE		= 0,
	PM_FRAME_MGMT		= 2,
	PM_FRAME_ASSOC		= 3,
};

#define TX_CMD_SEC_MSK			0x07
#define TX_CMD_SEC_WEP_KEY_IDX_POS	6
#define TX_CMD_SEC_WEP_KEY_IDX_MSK	0xc0

 
enum iwl_tx_cmd_sec_ctrl {
	TX_CMD_SEC_WEP			= 0x01,
	TX_CMD_SEC_CCM			= 0x02,
	TX_CMD_SEC_TKIP			= 0x03,
	TX_CMD_SEC_EXT			= 0x04,
	TX_CMD_SEC_GCMP			= 0x05,
	TX_CMD_SEC_KEY128		= 0x08,
	TX_CMD_SEC_KEY_FROM_TABLE	= 0x10,
};

 
#define TX_CMD_LIFE_TIME_INFINITE	0xFFFFFFFF
#define TX_CMD_LIFE_TIME_DEFAULT	2000000  
#define TX_CMD_LIFE_TIME_PROBE_RESP	40000  
#define TX_CMD_LIFE_TIME_EXPIRED_FRAME	0

 
#define IWL_TID_NON_QOS	0

 
#define IWL_DEFAULT_TX_RETRY			15
#define IWL_MGMT_DFAULT_RETRY_LIMIT		3
#define IWL_RTS_DFAULT_RETRY_LIMIT		60
#define IWL_BAR_DFAULT_RETRY_LIMIT		60
#define IWL_LOW_RETRY_LIMIT			7

 
enum iwl_tx_offload_assist_flags_pos {
	TX_CMD_OFFLD_IP_HDR =		0,
	TX_CMD_OFFLD_L4_EN =		6,
	TX_CMD_OFFLD_L3_EN =		7,
	TX_CMD_OFFLD_MH_SIZE =		8,
	TX_CMD_OFFLD_PAD =		13,
	TX_CMD_OFFLD_AMSDU =		14,
};

#define IWL_TX_CMD_OFFLD_MH_MASK	0x1f
#define IWL_TX_CMD_OFFLD_IP_HDR_MASK	0x3f

 
 
struct iwl_tx_cmd {
	__le16 len;
	__le16 offload_assist;
	__le32 tx_flags;
	struct {
		u8 try_cnt;
		u8 btkill_cnt;
		__le16 reserved;
	} scratch;  
	__le32 rate_n_flags;
	u8 sta_id;
	u8 sec_ctl;
	u8 initial_rate_index;
	u8 reserved2;
	u8 key[16];
	__le32 reserved3;
	__le32 life_time;
	__le32 dram_lsb_ptr;
	u8 dram_msb_ptr;
	u8 rts_retry_limit;
	u8 data_retry_limit;
	u8 tid_tspec;
	__le16 pm_frame_timeout;
	__le16 reserved4;
	union {
		DECLARE_FLEX_ARRAY(u8, payload);
		DECLARE_FLEX_ARRAY(struct ieee80211_hdr, hdr);
	};
} __packed;  

struct iwl_dram_sec_info {
	__le32 pn_low;
	__le16 pn_high;
	__le16 aux_info;
} __packed;  

 
struct iwl_tx_cmd_gen2 {
	__le16 len;
	__le16 offload_assist;
	__le32 flags;
	struct iwl_dram_sec_info dram_info;
	__le32 rate_n_flags;
	struct ieee80211_hdr hdr[];
} __packed;  

 
struct iwl_tx_cmd_gen3 {
	__le16 len;
	__le16 flags;
	__le32 offload_assist;
	struct iwl_dram_sec_info dram_info;
	__le32 rate_n_flags;
	u8 reserved[8];
	struct ieee80211_hdr hdr[];
} __packed;  

 

 
enum iwl_tx_status {
	TX_STATUS_MSK = 0x000000ff,
	TX_STATUS_SUCCESS = 0x01,
	TX_STATUS_DIRECT_DONE = 0x02,
	 
	TX_STATUS_POSTPONE_DELAY = 0x40,
	TX_STATUS_POSTPONE_FEW_BYTES = 0x41,
	TX_STATUS_POSTPONE_BT_PRIO = 0x42,
	TX_STATUS_POSTPONE_QUIET_PERIOD = 0x43,
	TX_STATUS_POSTPONE_CALC_TTAK = 0x44,
	 
	TX_STATUS_FAIL_INTERNAL_CROSSED_RETRY = 0x81,
	TX_STATUS_FAIL_SHORT_LIMIT = 0x82,
	TX_STATUS_FAIL_LONG_LIMIT = 0x83,
	TX_STATUS_FAIL_UNDERRUN = 0x84,
	TX_STATUS_FAIL_DRAIN_FLOW = 0x85,
	TX_STATUS_FAIL_RFKILL_FLUSH = 0x86,
	TX_STATUS_FAIL_LIFE_EXPIRE = 0x87,
	TX_STATUS_FAIL_DEST_PS = 0x88,
	TX_STATUS_FAIL_HOST_ABORTED = 0x89,
	TX_STATUS_FAIL_BT_RETRY = 0x8a,
	TX_STATUS_FAIL_STA_INVALID = 0x8b,
	TX_STATUS_FAIL_FRAG_DROPPED = 0x8c,
	TX_STATUS_FAIL_TID_DISABLE = 0x8d,
	TX_STATUS_FAIL_FIFO_FLUSHED = 0x8e,
	TX_STATUS_FAIL_SMALL_CF_POLL = 0x8f,
	TX_STATUS_FAIL_FW_DROP = 0x90,
	TX_STATUS_FAIL_STA_COLOR_MISMATCH = 0x91,
	TX_STATUS_INTERNAL_ABORT = 0x92,
	TX_MODE_MSK = 0x00000f00,
	TX_MODE_NO_BURST = 0x00000000,
	TX_MODE_IN_BURST_SEQ = 0x00000100,
	TX_MODE_FIRST_IN_BURST = 0x00000200,
	TX_QUEUE_NUM_MSK = 0x0001f000,
	TX_NARROW_BW_MSK = 0x00060000,
	TX_NARROW_BW_1DIV2 = 0x00020000,
	TX_NARROW_BW_1DIV4 = 0x00040000,
	TX_NARROW_BW_1DIV8 = 0x00060000,
};

 
enum iwl_tx_agg_status {
	AGG_TX_STATE_STATUS_MSK = 0x00fff,
	AGG_TX_STATE_TRANSMITTED = 0x000,
	AGG_TX_STATE_UNDERRUN = 0x001,
	AGG_TX_STATE_BT_PRIO = 0x002,
	AGG_TX_STATE_FEW_BYTES = 0x004,
	AGG_TX_STATE_ABORT = 0x008,
	AGG_TX_STATE_TX_ON_AIR_DROP = 0x010,
	AGG_TX_STATE_LAST_SENT_TRY_CNT = 0x020,
	AGG_TX_STATE_LAST_SENT_BT_KILL = 0x040,
	AGG_TX_STATE_SCD_QUERY = 0x080,
	AGG_TX_STATE_TEST_BAD_CRC32 = 0x0100,
	AGG_TX_STATE_RESPONSE = 0x1ff,
	AGG_TX_STATE_DUMP_TX = 0x200,
	AGG_TX_STATE_DELAY_TX = 0x400,
	AGG_TX_STATE_TRY_CNT_POS = 12,
	AGG_TX_STATE_TRY_CNT_MSK = 0xf << AGG_TX_STATE_TRY_CNT_POS,
};

 
#define AGG_TX_STAT_FRAME_NOT_SENT (AGG_TX_STATE_FEW_BYTES | \
				    AGG_TX_STATE_ABORT | \
				    AGG_TX_STATE_SCD_QUERY)

 

 
struct agg_tx_status {
	__le16 status;
	__le16 sequence;
} __packed;

 
#define TX_RES_INIT_RATE_INDEX_MSK 0x0f
#define TX_RES_RATE_TABLE_COLOR_POS 4
#define TX_RES_RATE_TABLE_COLOR_MSK 0x70
#define TX_RES_INV_RATE_INDEX_MSK 0x80
#define TX_RES_RATE_TABLE_COL_GET(_f) (((_f) & TX_RES_RATE_TABLE_COLOR_MSK) >>\
				       TX_RES_RATE_TABLE_COLOR_POS)

#define IWL_MVM_TX_RES_GET_TID(_ra_tid) ((_ra_tid) & 0x0f)
#define IWL_MVM_TX_RES_GET_RA(_ra_tid) ((_ra_tid) >> 4)

 
struct iwl_mvm_tx_resp_v3 {
	u8 frame_count;
	u8 bt_kill_count;
	u8 failure_rts;
	u8 failure_frame;
	__le32 initial_rate;
	__le16 wireless_media_time;

	u8 pa_status;
	u8 pa_integ_res_a[3];
	u8 pa_integ_res_b[3];
	u8 pa_integ_res_c[3];
	__le16 measurement_req_id;
	u8 reduced_tpc;
	u8 reserved;

	__le32 tfd_info;
	__le16 seq_ctl;
	__le16 byte_cnt;
	u8 tlc_info;
	u8 ra_tid;
	__le16 frame_ctrl;
	struct agg_tx_status status[];
} __packed;  

 
struct iwl_mvm_tx_resp {
	u8 frame_count;
	u8 bt_kill_count;
	u8 failure_rts;
	u8 failure_frame;
	__le32 initial_rate;
	__le16 wireless_media_time;

	u8 pa_status;
	u8 pa_integ_res_a[3];
	u8 pa_integ_res_b[3];
	u8 pa_integ_res_c[3];
	__le16 measurement_req_id;
	u8 reduced_tpc;
	u8 reserved;

	__le32 tfd_info;
	__le16 seq_ctl;
	__le16 byte_cnt;
	u8 tlc_info;
	u8 ra_tid;
	__le16 frame_ctrl;
	__le16 tx_queue;
	__le16 reserved2;
	struct agg_tx_status status;
} __packed;  

 
struct iwl_mvm_ba_notif {
	u8 sta_addr[ETH_ALEN];
	__le16 reserved;

	u8 sta_id;
	u8 tid;
	__le16 seq_ctl;
	__le64 bitmap;
	__le16 scd_flow;
	__le16 scd_ssn;
	u8 txed;
	u8 txed_2_done;
	u8 reduced_txp;
	u8 reserved1;
} __packed;

 
struct iwl_mvm_compressed_ba_tfd {
	__le16 q_num;
	__le16 tfd_index;
	u8 scd_queue;
	u8 tid;
	u8 reserved[2];
} __packed;  

 
struct iwl_mvm_compressed_ba_ratid {
	u8 q_num;
	u8 tid;
	__le16 ssn;
} __packed;  

 
enum iwl_mvm_ba_resp_flags {
	IWL_MVM_BA_RESP_TX_AGG,
	IWL_MVM_BA_RESP_TX_BAR,
	IWL_MVM_BA_RESP_TX_AGG_FAIL,
	IWL_MVM_BA_RESP_TX_UNDERRUN,
	IWL_MVM_BA_RESP_TX_BT_KILL,
	IWL_MVM_BA_RESP_TX_DSP_TIMEOUT
};

 
struct iwl_mvm_compressed_ba_notif {
	__le32 flags;
	u8 sta_id;
	u8 reduced_txp;
	u8 tlc_rate_info;
	u8 retry_cnt;
	__le32 query_byte_cnt;
	__le16 query_frame_cnt;
	__le16 txed;
	__le16 done;
	__le16 reserved;
	__le32 wireless_time;
	__le32 tx_rate;
	__le16 tfd_cnt;
	__le16 ra_tid_cnt;
	union {
		DECLARE_FLEX_ARRAY(struct iwl_mvm_compressed_ba_ratid, ra_tid);
		DECLARE_FLEX_ARRAY(struct iwl_mvm_compressed_ba_tfd, tfd);
	};
} __packed;  

 
struct iwl_mac_beacon_cmd_v6 {
	struct iwl_tx_cmd tx;
	__le32 template_id;
	__le32 tim_idx;
	__le32 tim_size;
	struct ieee80211_hdr frame[];
} __packed;  

 
struct iwl_mac_beacon_cmd_v7 {
	struct iwl_tx_cmd tx;
	__le32 template_id;
	__le32 tim_idx;
	__le32 tim_size;
	__le32 ecsa_offset;
	__le32 csa_offset;
	struct ieee80211_hdr frame[];
} __packed;  

 
enum iwl_mac_beacon_flags_v1 {
	IWL_MAC_BEACON_CCK_V1	= BIT(8),
	IWL_MAC_BEACON_ANT_A_V1 = BIT(9),
	IWL_MAC_BEACON_ANT_B_V1 = BIT(10),
	IWL_MAC_BEACON_FILS_V1	= BIT(12),
};

 
enum iwl_mac_beacon_flags {
	IWL_MAC_BEACON_CCK	= BIT(5),
	IWL_MAC_BEACON_ANT_A	= BIT(6),
	IWL_MAC_BEACON_ANT_B	= BIT(7),
	IWL_MAC_BEACON_FILS	= BIT(8),
};

 
struct iwl_mac_beacon_cmd {
	__le16 byte_cnt;
	__le16 flags;
	__le32 short_ssid;
	__le32 reserved;
	__le32 link_id;
	__le32 tim_idx;
	__le32 tim_size;
	__le32 ecsa_offset;
	__le32 csa_offset;
	struct ieee80211_hdr frame[];
} __packed;  

struct iwl_beacon_notif {
	struct iwl_mvm_tx_resp beacon_notify_hdr;
	__le64 tsf;
	__le32 ibss_mgr_status;
} __packed;

 
struct iwl_extended_beacon_notif_v5 {
	struct iwl_mvm_tx_resp beacon_notify_hdr;
	__le64 tsf;
	__le32 ibss_mgr_status;
	__le32 gp2;
} __packed;  

 
struct iwl_extended_beacon_notif {
	__le32 status;
	__le64 tsf;
	__le32 ibss_mgr_status;
	__le32 gp2;
} __packed;  

 
enum iwl_dump_control {
	DUMP_TX_FIFO_FLUSH	= BIT(1),
};

 
struct iwl_tx_path_flush_cmd_v1 {
	__le32 queues_ctl;
	__le16 flush_ctl;
	__le16 reserved;
} __packed;  

 
struct iwl_tx_path_flush_cmd {
	__le32 sta_id;
	__le16 tid_mask;
	__le16 reserved;
} __packed;  

#define IWL_TX_FLUSH_QUEUE_RSP 16

 
struct iwl_flush_queue_info {
	__le16 tid;
	__le16 queue_num;
	__le16 read_before_flush;
	__le16 read_after_flush;
} __packed;  

 
struct iwl_tx_path_flush_cmd_rsp {
	__le16 sta_id;
	__le16 num_flushed_queues;
	struct iwl_flush_queue_info queues[IWL_TX_FLUSH_QUEUE_RSP];
} __packed;  

 
enum iwl_scd_cfg_actions {
	SCD_CFG_DISABLE_QUEUE		= 0x0,
	SCD_CFG_ENABLE_QUEUE		= 0x1,
	SCD_CFG_UPDATE_QUEUE_TID	= 0x2,
};

 
struct iwl_scd_txq_cfg_cmd {
	u8 token;
	u8 sta_id;
	u8 tid;
	u8 scd_queue;
	u8 action;
	u8 aggregate;
	u8 tx_fifo;
	u8 window;
	__le16 ssn;
	__le16 reserved;
} __packed;  

 
struct iwl_scd_txq_cfg_rsp {
	u8 token;
	u8 sta_id;
	u8 tid;
	u8 scd_queue;
} __packed;  

#endif  
