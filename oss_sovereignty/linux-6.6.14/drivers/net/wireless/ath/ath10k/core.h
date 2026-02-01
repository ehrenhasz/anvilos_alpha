 
 

#ifndef _CORE_H_
#define _CORE_H_

#include <linux/completion.h>
#include <linux/if_ether.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/uuid.h>
#include <linux/time.h>

#include "htt.h"
#include "htc.h"
#include "hw.h"
#include "targaddrs.h"
#include "wmi.h"
#include "../ath.h"
#include "../regd.h"
#include "../dfs_pattern_detector.h"
#include "spectral.h"
#include "thermal.h"
#include "wow.h"
#include "swap.h"

#define MS(_v, _f) (((_v) & _f##_MASK) >> _f##_LSB)
#define SM(_v, _f) (((_v) << _f##_LSB) & _f##_MASK)
#define WO(_f)      ((_f##_OFFSET) >> 2)

#define ATH10K_SCAN_ID 0
#define ATH10K_SCAN_CHANNEL_SWITCH_WMI_EVT_OVERHEAD 10  
#define WMI_READY_TIMEOUT (5 * HZ)
#define ATH10K_FLUSH_TIMEOUT_HZ (5 * HZ)
#define ATH10K_CONNECTION_LOSS_HZ (3 * HZ)
#define ATH10K_NUM_CHANS 41
#define ATH10K_MAX_5G_CHAN 173

 
#define ATH10K_DEFAULT_NOISE_FLOOR -95

#define ATH10K_INVALID_RSSI 128

#define ATH10K_MAX_NUM_MGMT_PENDING 128

 
#define ATH10K_KICKOUT_THRESHOLD (20 * 16)

 
#define ATH10K_KEEPALIVE_MIN_IDLE 3747
#define ATH10K_KEEPALIVE_MAX_IDLE 3895
#define ATH10K_KEEPALIVE_MAX_UNRESPONSIVE 3900

 
#define ATH10K_SMBIOS_BDF_EXT_TYPE 0xF8

 
#define ATH10K_SMBIOS_BDF_EXT_LENGTH 0x9

 
#define ATH10K_SMBIOS_BDF_EXT_OFFSET 0x8

 
#define ATH10K_SMBIOS_BDF_EXT_STR_LENGTH 0x20

 
#define ATH10K_SMBIOS_BDF_EXT_MAGIC "BDF_"

 
#define ATH10K_AIRTIME_WEIGHT_MULTIPLIER  4

#define ATH10K_MAX_RETRY_COUNT 30

#define ATH10K_ITER_NORMAL_FLAGS (IEEE80211_IFACE_ITER_NORMAL | \
				  IEEE80211_IFACE_SKIP_SDATA_NOT_IN_DRIVER)
#define ATH10K_ITER_RESUME_FLAGS (IEEE80211_IFACE_ITER_RESUME_ALL |\
				  IEEE80211_IFACE_SKIP_SDATA_NOT_IN_DRIVER)

struct ath10k;

static inline const char *ath10k_bus_str(enum ath10k_bus bus)
{
	switch (bus) {
	case ATH10K_BUS_PCI:
		return "pci";
	case ATH10K_BUS_AHB:
		return "ahb";
	case ATH10K_BUS_SDIO:
		return "sdio";
	case ATH10K_BUS_USB:
		return "usb";
	case ATH10K_BUS_SNOC:
		return "snoc";
	}

	return "unknown";
}

enum ath10k_skb_flags {
	ATH10K_SKB_F_NO_HWCRYPT = BIT(0),
	ATH10K_SKB_F_DTIM_ZERO = BIT(1),
	ATH10K_SKB_F_DELIVER_CAB = BIT(2),
	ATH10K_SKB_F_MGMT = BIT(3),
	ATH10K_SKB_F_QOS = BIT(4),
	ATH10K_SKB_F_RAW_TX = BIT(5),
	ATH10K_SKB_F_NOACK_TID = BIT(6),
};

struct ath10k_skb_cb {
	dma_addr_t paddr;
	u8 flags;
	u8 eid;
	u16 msdu_id;
	u16 airtime_est;
	struct ieee80211_vif *vif;
	struct ieee80211_txq *txq;
	u32 ucast_cipher;
} __packed;

struct ath10k_skb_rxcb {
	dma_addr_t paddr;
	struct hlist_node hlist;
	u8 eid;
};

static inline struct ath10k_skb_cb *ATH10K_SKB_CB(struct sk_buff *skb)
{
	BUILD_BUG_ON(sizeof(struct ath10k_skb_cb) >
		     IEEE80211_TX_INFO_DRIVER_DATA_SIZE);
	return (struct ath10k_skb_cb *)&IEEE80211_SKB_CB(skb)->driver_data;
}

static inline struct ath10k_skb_rxcb *ATH10K_SKB_RXCB(struct sk_buff *skb)
{
	BUILD_BUG_ON(sizeof(struct ath10k_skb_rxcb) > sizeof(skb->cb));
	return (struct ath10k_skb_rxcb *)skb->cb;
}

#define ATH10K_RXCB_SKB(rxcb) \
		container_of((void *)rxcb, struct sk_buff, cb)

static inline u32 host_interest_item_address(u32 item_offset)
{
	return QCA988X_HOST_INTEREST_ADDRESS + item_offset;
}

enum ath10k_phy_mode {
	ATH10K_PHY_MODE_LEGACY = 0,
	ATH10K_PHY_MODE_HT = 1,
	ATH10K_PHY_MODE_VHT = 2,
};

 
struct ath10k_index_ht_data_rate_type {
	u8   beacon_rate_index;
	u16  supported_rate[4];
};

 
struct ath10k_index_vht_data_rate_type {
	u8   beacon_rate_index;
	u16  supported_VHT80_rate[2];
	u16  supported_VHT40_rate[2];
	u16  supported_VHT20_rate[2];
};

struct ath10k_bmi {
	bool done_sent;
};

struct ath10k_mem_chunk {
	void *vaddr;
	dma_addr_t paddr;
	u32 len;
	u32 req_id;
};

struct ath10k_wmi {
	enum ath10k_htc_ep_id eid;
	struct completion service_ready;
	struct completion unified_ready;
	struct completion barrier;
	struct completion radar_confirm;
	wait_queue_head_t tx_credits_wq;
	DECLARE_BITMAP(svc_map, WMI_SERVICE_MAX);
	struct wmi_cmd_map *cmd;
	struct wmi_vdev_param_map *vdev_param;
	struct wmi_pdev_param_map *pdev_param;
	struct wmi_peer_param_map *peer_param;
	const struct wmi_ops *ops;
	const struct wmi_peer_flags_map *peer_flags;

	u32 mgmt_max_num_pending_tx;

	 
	struct idr mgmt_pending_tx;

	u32 num_mem_chunks;
	u32 rx_decap_mode;
	struct ath10k_mem_chunk mem_chunks[WMI_MAX_MEM_REQS];
};

struct ath10k_fw_stats_peer {
	struct list_head list;

	u8 peer_macaddr[ETH_ALEN];
	u32 peer_rssi;
	u32 peer_tx_rate;
	u32 peer_rx_rate;  
	u64 rx_duration;
};

struct ath10k_fw_extd_stats_peer {
	struct list_head list;

	u8 peer_macaddr[ETH_ALEN];
	u64 rx_duration;
};

struct ath10k_fw_stats_vdev {
	struct list_head list;

	u32 vdev_id;
	u32 beacon_snr;
	u32 data_snr;
	u32 num_tx_frames[4];
	u32 num_rx_frames;
	u32 num_tx_frames_retries[4];
	u32 num_tx_frames_failures[4];
	u32 num_rts_fail;
	u32 num_rts_success;
	u32 num_rx_err;
	u32 num_rx_discard;
	u32 num_tx_not_acked;
	u32 tx_rate_history[10];
	u32 beacon_rssi_history[10];
};

struct ath10k_fw_stats_vdev_extd {
	struct list_head list;

	u32 vdev_id;
	u32 ppdu_aggr_cnt;
	u32 ppdu_noack;
	u32 mpdu_queued;
	u32 ppdu_nonaggr_cnt;
	u32 mpdu_sw_requeued;
	u32 mpdu_suc_retry;
	u32 mpdu_suc_multitry;
	u32 mpdu_fail_retry;
	u32 tx_ftm_suc;
	u32 tx_ftm_suc_retry;
	u32 tx_ftm_fail;
	u32 rx_ftmr_cnt;
	u32 rx_ftmr_dup_cnt;
	u32 rx_iftmr_cnt;
	u32 rx_iftmr_dup_cnt;
};

struct ath10k_fw_stats_pdev {
	struct list_head list;

	 
	s32 ch_noise_floor;
	u32 tx_frame_count;  
	u32 rx_frame_count;  
	u32 rx_clear_count;  
	u32 cycle_count;  
	u32 phy_err_count;
	u32 chan_tx_power;
	u32 ack_rx_bad;
	u32 rts_bad;
	u32 rts_good;
	u32 fcs_bad;
	u32 no_beacons;
	u32 mib_int_count;

	 
	s32 comp_queued;
	s32 comp_delivered;
	s32 msdu_enqued;
	s32 mpdu_enqued;
	s32 wmm_drop;
	s32 local_enqued;
	s32 local_freed;
	s32 hw_queued;
	s32 hw_reaped;
	s32 underrun;
	u32 hw_paused;
	s32 tx_abort;
	s32 mpdus_requeued;
	u32 tx_ko;
	u32 data_rc;
	u32 self_triggers;
	u32 sw_retry_failure;
	u32 illgl_rate_phy_err;
	u32 pdev_cont_xretry;
	u32 pdev_tx_timeout;
	u32 pdev_resets;
	u32 phy_underrun;
	u32 txop_ovf;
	u32 seq_posted;
	u32 seq_failed_queueing;
	u32 seq_completed;
	u32 seq_restarted;
	u32 mu_seq_posted;
	u32 mpdus_sw_flush;
	u32 mpdus_hw_filter;
	u32 mpdus_truncated;
	u32 mpdus_ack_failed;
	u32 mpdus_expired;

	 
	s32 mid_ppdu_route_change;
	s32 status_rcvd;
	s32 r0_frags;
	s32 r1_frags;
	s32 r2_frags;
	s32 r3_frags;
	s32 htt_msdus;
	s32 htt_mpdus;
	s32 loc_msdus;
	s32 loc_mpdus;
	s32 oversize_amsdu;
	s32 phy_errs;
	s32 phy_err_drop;
	s32 mpdu_errs;
	s32 rx_ovfl_errs;
};

struct ath10k_fw_stats {
	bool extended;
	struct list_head pdevs;
	struct list_head vdevs;
	struct list_head peers;
	struct list_head peers_extd;
};

#define ATH10K_TPC_TABLE_TYPE_FLAG	1
#define ATH10K_TPC_PREAM_TABLE_END	0xFFFF

struct ath10k_tpc_table {
	u32 pream_idx[WMI_TPC_RATE_MAX];
	u8 rate_code[WMI_TPC_RATE_MAX];
	char tpc_value[WMI_TPC_RATE_MAX][WMI_TPC_TX_N_CHAIN * WMI_TPC_BUF_SIZE];
};

struct ath10k_tpc_stats {
	u32 reg_domain;
	u32 chan_freq;
	u32 phy_mode;
	u32 twice_antenna_reduction;
	u32 twice_max_rd_power;
	s32 twice_antenna_gain;
	u32 power_limit;
	u32 num_tx_chain;
	u32 ctl;
	u32 rate_max;
	u8 flag[WMI_TPC_FLAG];
	struct ath10k_tpc_table tpc_table[WMI_TPC_FLAG];
};

struct ath10k_tpc_table_final {
	u32 pream_idx[WMI_TPC_FINAL_RATE_MAX];
	u8 rate_code[WMI_TPC_FINAL_RATE_MAX];
	char tpc_value[WMI_TPC_FINAL_RATE_MAX][WMI_TPC_TX_N_CHAIN * WMI_TPC_BUF_SIZE];
};

struct ath10k_tpc_stats_final {
	u32 reg_domain;
	u32 chan_freq;
	u32 phy_mode;
	u32 twice_antenna_reduction;
	u32 twice_max_rd_power;
	s32 twice_antenna_gain;
	u32 power_limit;
	u32 num_tx_chain;
	u32 ctl;
	u32 rate_max;
	u8 flag[WMI_TPC_FLAG];
	struct ath10k_tpc_table_final tpc_table_final[WMI_TPC_FLAG];
};

struct ath10k_dfs_stats {
	u32 phy_errors;
	u32 pulses_total;
	u32 pulses_detected;
	u32 pulses_discarded;
	u32 radar_detected;
};

enum ath10k_radar_confirmation_state {
	ATH10K_RADAR_CONFIRMATION_IDLE = 0,
	ATH10K_RADAR_CONFIRMATION_INPROGRESS,
	ATH10K_RADAR_CONFIRMATION_STOPPED,
};

struct ath10k_radar_found_info {
	u32 pri_min;
	u32 pri_max;
	u32 width_min;
	u32 width_max;
	u32 sidx_min;
	u32 sidx_max;
};

#define ATH10K_MAX_NUM_PEER_IDS (1 << 11)  

struct ath10k_peer {
	struct list_head list;
	struct ieee80211_vif *vif;
	struct ieee80211_sta *sta;

	bool removed;
	int vdev_id;
	u8 addr[ETH_ALEN];
	DECLARE_BITMAP(peer_ids, ATH10K_MAX_NUM_PEER_IDS);

	 
	struct ieee80211_key_conf *keys[WMI_MAX_KEY_INDEX + 1];
	union htt_rx_pn_t tids_last_pn[ATH10K_TXRX_NUM_EXT_TIDS];
	bool tids_last_pn_valid[ATH10K_TXRX_NUM_EXT_TIDS];
	union htt_rx_pn_t frag_tids_last_pn[ATH10K_TXRX_NUM_EXT_TIDS];
	u32 frag_tids_seq[ATH10K_TXRX_NUM_EXT_TIDS];
	struct {
		enum htt_security_types sec_type;
		int pn_len;
	} rx_pn[ATH10K_HTT_TXRX_PEER_SECURITY_MAX];
};

struct ath10k_txq {
	struct list_head list;
	unsigned long num_fw_queued;
	unsigned long num_push_allowed;
};

enum ath10k_pkt_rx_err {
	ATH10K_PKT_RX_ERR_FCS,
	ATH10K_PKT_RX_ERR_TKIP,
	ATH10K_PKT_RX_ERR_CRYPT,
	ATH10K_PKT_RX_ERR_PEER_IDX_INVAL,
	ATH10K_PKT_RX_ERR_MAX,
};

enum ath10k_ampdu_subfrm_num {
	ATH10K_AMPDU_SUBFRM_NUM_10,
	ATH10K_AMPDU_SUBFRM_NUM_20,
	ATH10K_AMPDU_SUBFRM_NUM_30,
	ATH10K_AMPDU_SUBFRM_NUM_40,
	ATH10K_AMPDU_SUBFRM_NUM_50,
	ATH10K_AMPDU_SUBFRM_NUM_60,
	ATH10K_AMPDU_SUBFRM_NUM_MORE,
	ATH10K_AMPDU_SUBFRM_NUM_MAX,
};

enum ath10k_amsdu_subfrm_num {
	ATH10K_AMSDU_SUBFRM_NUM_1,
	ATH10K_AMSDU_SUBFRM_NUM_2,
	ATH10K_AMSDU_SUBFRM_NUM_3,
	ATH10K_AMSDU_SUBFRM_NUM_4,
	ATH10K_AMSDU_SUBFRM_NUM_MORE,
	ATH10K_AMSDU_SUBFRM_NUM_MAX,
};

struct ath10k_sta_tid_stats {
	unsigned long rx_pkt_from_fw;
	unsigned long rx_pkt_unchained;
	unsigned long rx_pkt_drop_chained;
	unsigned long rx_pkt_drop_filter;
	unsigned long rx_pkt_err[ATH10K_PKT_RX_ERR_MAX];
	unsigned long rx_pkt_queued_for_mac;
	unsigned long rx_pkt_ampdu[ATH10K_AMPDU_SUBFRM_NUM_MAX];
	unsigned long rx_pkt_amsdu[ATH10K_AMSDU_SUBFRM_NUM_MAX];
};

enum ath10k_counter_type {
	ATH10K_COUNTER_TYPE_BYTES,
	ATH10K_COUNTER_TYPE_PKTS,
	ATH10K_COUNTER_TYPE_MAX,
};

enum ath10k_stats_type {
	ATH10K_STATS_TYPE_SUCC,
	ATH10K_STATS_TYPE_FAIL,
	ATH10K_STATS_TYPE_RETRY,
	ATH10K_STATS_TYPE_AMPDU,
	ATH10K_STATS_TYPE_MAX,
};

struct ath10k_htt_data_stats {
	u64 legacy[ATH10K_COUNTER_TYPE_MAX][ATH10K_LEGACY_NUM];
	u64 ht[ATH10K_COUNTER_TYPE_MAX][ATH10K_HT_MCS_NUM];
	u64 vht[ATH10K_COUNTER_TYPE_MAX][ATH10K_VHT_MCS_NUM];
	u64 bw[ATH10K_COUNTER_TYPE_MAX][ATH10K_BW_NUM];
	u64 nss[ATH10K_COUNTER_TYPE_MAX][ATH10K_NSS_NUM];
	u64 gi[ATH10K_COUNTER_TYPE_MAX][ATH10K_GI_NUM];
	u64 rate_table[ATH10K_COUNTER_TYPE_MAX][ATH10K_RATE_TABLE_NUM];
};

struct ath10k_htt_tx_stats {
	struct ath10k_htt_data_stats stats[ATH10K_STATS_TYPE_MAX];
	u64 tx_duration;
	u64 ba_fails;
	u64 ack_fails;
};

#define ATH10K_TID_MAX	8

struct ath10k_sta {
	struct ath10k_vif *arvif;

	 
	u32 changed;  
	u32 bw;
	u32 nss;
	u32 smps;
	u16 peer_id;
	struct rate_info txrate;
	struct ieee80211_tx_info tx_info;
	u32 tx_retries;
	u32 tx_failed;
	u32 last_tx_bitrate;

	u32 rx_rate_code;
	u32 rx_bitrate_kbps;
	u32 tx_rate_code;
	u32 tx_bitrate_kbps;
	struct work_struct update_wk;
	u64 rx_duration;
	struct ath10k_htt_tx_stats *tx_stats;
	u32 ucast_cipher;

#ifdef CONFIG_MAC80211_DEBUGFS
	 
	bool aggr_mode;

	 
	struct ath10k_sta_tid_stats tid_stats[IEEE80211_NUM_TIDS + 1];
#endif
	 
	u32 peer_ps_state;
	struct work_struct tid_config_wk;
	int noack[ATH10K_TID_MAX];
	int retry_long[ATH10K_TID_MAX];
	int ampdu[ATH10K_TID_MAX];
	u8 rate_ctrl[ATH10K_TID_MAX];
	u32 rate_code[ATH10K_TID_MAX];
	int rtscts[ATH10K_TID_MAX];
};

#define ATH10K_VDEV_SETUP_TIMEOUT_HZ	(5 * HZ)
#define ATH10K_VDEV_DELETE_TIMEOUT_HZ	(5 * HZ)

enum ath10k_beacon_state {
	ATH10K_BEACON_SCHEDULED = 0,
	ATH10K_BEACON_SENDING,
	ATH10K_BEACON_SENT,
};

struct ath10k_vif {
	struct list_head list;

	u32 vdev_id;
	u16 peer_id;
	enum wmi_vdev_type vdev_type;
	enum wmi_vdev_subtype vdev_subtype;
	u32 beacon_interval;
	u32 dtim_period;
	struct sk_buff *beacon;
	 
	enum ath10k_beacon_state beacon_state;
	void *beacon_buf;
	dma_addr_t beacon_paddr;
	unsigned long tx_paused;  

	struct ath10k *ar;
	struct ieee80211_vif *vif;

	bool is_started;
	bool is_up;
	bool spectral_enabled;
	bool ps;
	u32 aid;
	u8 bssid[ETH_ALEN];

	struct ieee80211_key_conf *wep_keys[WMI_MAX_KEY_INDEX + 1];
	s8 def_wep_key_idx;

	u16 tx_seq_no;

	union {
		struct {
			u32 uapsd;
		} sta;
		struct {
			 
			u8 tim_bitmap[64];
			u8 tim_len;
			u32 ssid_len;
			u8 ssid[IEEE80211_MAX_SSID_LEN];
			bool hidden_ssid;
			 
			u32 noa_len;
			u8 *noa_data;
		} ap;
	} u;

	bool use_cts_prot;
	bool nohwcrypt;
	int num_legacy_stations;
	int txpower;
	bool ftm_responder;
	struct wmi_wmm_params_all_arg wmm_params;
	struct work_struct ap_csa_work;
	struct delayed_work connection_loss_work;
	struct cfg80211_bitrate_mask bitrate_mask;

	 
	int vht_num_rates;
	u8 vht_pfr;
	u32 tid_conf_changed[ATH10K_TID_MAX];
	int noack[ATH10K_TID_MAX];
	int retry_long[ATH10K_TID_MAX];
	int ampdu[ATH10K_TID_MAX];
	u8 rate_ctrl[ATH10K_TID_MAX];
	u32 rate_code[ATH10K_TID_MAX];
	int rtscts[ATH10K_TID_MAX];
	u32 tids_rst;
};

struct ath10k_vif_iter {
	u32 vdev_id;
	struct ath10k_vif *arvif;
};

 
struct ath10k_ce_crash_data {
	__le32 base_addr;
	__le32 src_wr_idx;
	__le32 src_r_idx;
	__le32 dst_wr_idx;
	__le32 dst_r_idx;
};

struct ath10k_ce_crash_hdr {
	__le32 ce_count;
	__le32 reserved[3];  
	struct ath10k_ce_crash_data entries[];
};

#define MAX_MEM_DUMP_TYPE	5

 
struct ath10k_fw_crash_data {
	guid_t guid;
	struct timespec64 timestamp;
	__le32 registers[REG_DUMP_COUNT_QCA988X];
	struct ath10k_ce_crash_data ce_crash_data[CE_COUNT_MAX];

	u8 *ramdump_buf;
	size_t ramdump_buf_len;
};

struct ath10k_debug {
	struct dentry *debugfs_phy;

	struct ath10k_fw_stats fw_stats;
	struct completion fw_stats_complete;
	bool fw_stats_done;

	unsigned long htt_stats_mask;
	unsigned long reset_htt_stats;
	struct delayed_work htt_stats_dwork;
	struct ath10k_dfs_stats dfs_stats;
	struct ath_dfs_pool_stats dfs_pool_stats;

	 
	struct ath10k_tpc_stats *tpc_stats;
	struct ath10k_tpc_stats_final *tpc_stats_final;

	struct completion tpc_complete;

	 
	u64 fw_dbglog_mask;
	u32 fw_dbglog_level;
	u32 reg_addr;
	u32 nf_cal_period;
	void *cal_data;
	u32 enable_extd_tx_stats;
	u8 fw_dbglog_mode;
};

enum ath10k_state {
	ATH10K_STATE_OFF = 0,
	ATH10K_STATE_ON,

	 
	ATH10K_STATE_RESTARTING,
	ATH10K_STATE_RESTARTED,

	 
	ATH10K_STATE_WEDGED,

	 
	ATH10K_STATE_UTF,
};

enum ath10k_firmware_mode {
	 
	ATH10K_FIRMWARE_MODE_NORMAL,

	 
	ATH10K_FIRMWARE_MODE_UTF,
};

enum ath10k_fw_features {
	 
	ATH10K_FW_FEATURE_EXT_WMI_MGMT_RX = 0,

	 
	ATH10K_FW_FEATURE_WMI_10X = 1,

	 
	ATH10K_FW_FEATURE_HAS_WMI_MGMT_TX = 2,

	 
	ATH10K_FW_FEATURE_NO_P2P = 3,

	 
	ATH10K_FW_FEATURE_WMI_10_2 = 4,

	 
	ATH10K_FW_FEATURE_MULTI_VIF_PS_SUPPORT = 5,

	 
	ATH10K_FW_FEATURE_WOWLAN_SUPPORT = 6,

	 
	ATH10K_FW_FEATURE_IGNORE_OTP_RESULT = 7,

	 
	ATH10K_FW_FEATURE_NO_NWIFI_DECAP_4ADDR_PADDING = 8,

	 
	ATH10K_FW_FEATURE_SUPPORTS_SKIP_CLOCK_INIT = 9,

	 
	ATH10K_FW_FEATURE_RAW_MODE_SUPPORT = 10,

	 
	ATH10K_FW_FEATURE_SUPPORTS_ADAPTIVE_CCA = 11,

	 
	ATH10K_FW_FEATURE_MFP_SUPPORT = 12,

	 
	ATH10K_FW_FEATURE_PEER_FLOW_CONTROL = 13,

	 
	ATH10K_FW_FEATURE_BTCOEX_PARAM = 14,

	 
	ATH10K_FW_FEATURE_SKIP_NULL_FUNC_WAR = 15,

	 
	ATH10K_FW_FEATURE_ALLOWS_MESH_BCAST = 16,

	 
	ATH10K_FW_FEATURE_NO_PS = 17,

	 
	ATH10K_FW_FEATURE_MGMT_TX_BY_REF = 18,

	 
	ATH10K_FW_FEATURE_NON_BMI = 19,

	 
	ATH10K_FW_FEATURE_SINGLE_CHAN_INFO_PER_CHANNEL = 20,

	 
	ATH10K_FW_FEATURE_PEER_FIXED_RATE = 21,

	 
	ATH10K_FW_FEATURE_IRAM_RECOVERY = 22,

	 
	ATH10K_FW_FEATURE_COUNT,
};

enum ath10k_dev_flags {
	 
	ATH10K_CAC_RUNNING,
	ATH10K_FLAG_CORE_REGISTERED,

	 
	ATH10K_FLAG_CRASH_FLUSH,

	 
	ATH10K_FLAG_RAW_MODE,

	 
	ATH10K_FLAG_HW_CRYPTO_DISABLED,

	 
	ATH10K_FLAG_BTCOEX,

	 
	ATH10K_FLAG_PEER_STATS,

	 
	ATH10K_FLAG_RESTARTING,

	 
	ATH10K_FLAG_NAPI_ENABLED,
};

enum ath10k_cal_mode {
	ATH10K_CAL_MODE_FILE,
	ATH10K_CAL_MODE_OTP,
	ATH10K_CAL_MODE_DT,
	ATH10K_CAL_MODE_NVMEM,
	ATH10K_PRE_CAL_MODE_FILE,
	ATH10K_PRE_CAL_MODE_DT,
	ATH10K_PRE_CAL_MODE_NVMEM,
	ATH10K_CAL_MODE_EEPROM,
};

enum ath10k_crypt_mode {
	 
	ATH10K_CRYPT_MODE_HW,
	 
	ATH10K_CRYPT_MODE_SW,
};

static inline const char *ath10k_cal_mode_str(enum ath10k_cal_mode mode)
{
	switch (mode) {
	case ATH10K_CAL_MODE_FILE:
		return "file";
	case ATH10K_CAL_MODE_OTP:
		return "otp";
	case ATH10K_CAL_MODE_DT:
		return "dt";
	case ATH10K_CAL_MODE_NVMEM:
		return "nvmem";
	case ATH10K_PRE_CAL_MODE_FILE:
		return "pre-cal-file";
	case ATH10K_PRE_CAL_MODE_DT:
		return "pre-cal-dt";
	case ATH10K_PRE_CAL_MODE_NVMEM:
		return "pre-cal-nvmem";
	case ATH10K_CAL_MODE_EEPROM:
		return "eeprom";
	}

	return "unknown";
}

enum ath10k_scan_state {
	ATH10K_SCAN_IDLE,
	ATH10K_SCAN_STARTING,
	ATH10K_SCAN_RUNNING,
	ATH10K_SCAN_ABORTING,
};

static inline const char *ath10k_scan_state_str(enum ath10k_scan_state state)
{
	switch (state) {
	case ATH10K_SCAN_IDLE:
		return "idle";
	case ATH10K_SCAN_STARTING:
		return "starting";
	case ATH10K_SCAN_RUNNING:
		return "running";
	case ATH10K_SCAN_ABORTING:
		return "aborting";
	}

	return "unknown";
}

enum ath10k_tx_pause_reason {
	ATH10K_TX_PAUSE_Q_FULL,
	ATH10K_TX_PAUSE_MAX,
};

struct ath10k_fw_file {
	const struct firmware *firmware;

	char fw_version[ETHTOOL_FWVERS_LEN];

	DECLARE_BITMAP(fw_features, ATH10K_FW_FEATURE_COUNT);

	enum ath10k_fw_wmi_op_version wmi_op_version;
	enum ath10k_fw_htt_op_version htt_op_version;

	const void *firmware_data;
	size_t firmware_len;

	const void *otp_data;
	size_t otp_len;

	const void *codeswap_data;
	size_t codeswap_len;

	 
	struct ath10k_swap_code_seg_info *firmware_swap_code_seg_info;
};

struct ath10k_fw_components {
	const struct firmware *board;
	const void *board_data;
	size_t board_len;
	const struct firmware *ext_board;
	const void *ext_board_data;
	size_t ext_board_len;

	struct ath10k_fw_file fw_file;
};

struct ath10k_per_peer_tx_stats {
	u32	succ_bytes;
	u32	retry_bytes;
	u32	failed_bytes;
	u8	ratecode;
	u8	flags;
	u16	peer_id;
	u16	succ_pkts;
	u16	retry_pkts;
	u16	failed_pkts;
	u16	duration;
	u32	reserved1;
	u32	reserved2;
};

enum ath10k_dev_type {
	ATH10K_DEV_TYPE_LL,
	ATH10K_DEV_TYPE_HL,
};

struct ath10k_bus_params {
	u32 chip_id;
	enum ath10k_dev_type dev_type;
	bool link_can_suspend;
	bool hl_msdu_ids;
};

struct ath10k {
	struct ath_common ath_common;
	struct ieee80211_hw *hw;
	struct ieee80211_ops *ops;
	struct device *dev;
	struct msa_region {
		dma_addr_t paddr;
		u32 mem_size;
		void *vaddr;
	} msa;
	u8 mac_addr[ETH_ALEN];

	enum ath10k_hw_rev hw_rev;
	u16 dev_id;
	u32 chip_id;
	u32 target_version;
	u8 fw_version_major;
	u32 fw_version_minor;
	u16 fw_version_release;
	u16 fw_version_build;
	u32 fw_stats_req_mask;
	u32 phy_capability;
	u32 hw_min_tx_power;
	u32 hw_max_tx_power;
	u32 hw_eeprom_rd;
	u32 ht_cap_info;
	u32 vht_cap_info;
	u32 vht_supp_mcs;
	u32 num_rf_chains;
	u32 max_spatial_stream;
	 
	u32 low_2ghz_chan;
	u32 high_2ghz_chan;
	u32 low_5ghz_chan;
	u32 high_5ghz_chan;
	bool ani_enabled;
	u32 sys_cap_info;

	 
	bool hw_rfkill_on;

	 
	u8 ps_state_enable;

	bool nlo_enabled;
	bool p2p;

	struct {
		enum ath10k_bus bus;
		const struct ath10k_hif_ops *ops;
	} hif;

	struct completion target_suspend;
	struct completion driver_recovery;

	const struct ath10k_hw_regs *regs;
	const struct ath10k_hw_ce_regs *hw_ce_regs;
	const struct ath10k_hw_values *hw_values;
	struct ath10k_bmi bmi;
	struct ath10k_wmi wmi;
	struct ath10k_htc htc;
	struct ath10k_htt htt;

	struct ath10k_hw_params hw_params;

	 
	struct ath10k_fw_components normal_mode_fw;

	 
	const struct ath10k_fw_components *running_fw;

	const struct firmware *pre_cal_file;
	const struct firmware *cal_file;

	struct {
		u32 vendor;
		u32 device;
		u32 subsystem_vendor;
		u32 subsystem_device;

		bool bmi_ids_valid;
		bool qmi_ids_valid;
		u32 qmi_board_id;
		u32 qmi_chip_id;
		u8 bmi_board_id;
		u8 bmi_eboard_id;
		u8 bmi_chip_id;
		bool ext_bid_supported;

		char bdf_ext[ATH10K_SMBIOS_BDF_EXT_STR_LENGTH];
	} id;

	int fw_api;
	int bd_api;
	enum ath10k_cal_mode cal_mode;

	struct {
		struct completion started;
		struct completion completed;
		struct completion on_channel;
		struct delayed_work timeout;
		enum ath10k_scan_state state;
		bool is_roc;
		int vdev_id;
		int roc_freq;
		bool roc_notify;
	} scan;

	struct {
		struct ieee80211_supported_band sbands[NUM_NL80211_BANDS];
	} mac;

	 
	struct ieee80211_channel *rx_channel;

	 
	struct ieee80211_channel *scan_channel;

	 
	struct cfg80211_chan_def chandef;

	 
	struct ieee80211_channel *tgt_oper_chan;

	unsigned long long free_vdev_map;
	struct ath10k_vif *monitor_arvif;
	bool monitor;
	int monitor_vdev_id;
	bool monitor_started;
	unsigned int filter_flags;
	unsigned long dev_flags;
	bool dfs_block_radar_events;

	 
	bool radar_enabled;
	int num_started_vdevs;

	 
	u8 cfg_tx_chainmask;
	u8 cfg_rx_chainmask;

	struct completion install_key_done;

	int last_wmi_vdev_start_status;
	struct completion vdev_setup_done;
	struct completion vdev_delete_done;
	struct completion peer_stats_info_complete;

	struct workqueue_struct *workqueue;
	 
	struct workqueue_struct *workqueue_aux;
	struct workqueue_struct *workqueue_tx_complete;
	 
	struct mutex conf_mutex;

	 
	struct mutex dump_mutex;

	 
	spinlock_t data_lock;

	 
	spinlock_t queue_lock[IEEE80211_NUM_ACS];

	struct list_head arvifs;
	struct list_head peers;
	struct ath10k_peer *peer_map[ATH10K_MAX_NUM_PEER_IDS];
	wait_queue_head_t peer_mapping_wq;

	 
	int num_peers;
	int num_stations;

	int max_num_peers;
	int max_num_stations;
	int max_num_vdevs;
	int max_num_tdls_vdevs;
	int num_active_peers;
	int num_tids;

	struct work_struct svc_rdy_work;
	struct sk_buff *svc_rdy_skb;

	struct work_struct offchan_tx_work;
	struct sk_buff_head offchan_tx_queue;
	struct completion offchan_tx_completed;
	struct sk_buff *offchan_tx_skb;

	struct work_struct wmi_mgmt_tx_work;
	struct sk_buff_head wmi_mgmt_tx_queue;

	enum ath10k_state state;

	struct work_struct register_work;
	struct work_struct restart_work;
	struct work_struct bundle_tx_work;
	struct work_struct tx_complete_work;

	 
	u32 survey_last_rx_clear_count;
	u32 survey_last_cycle_count;
	struct survey_info survey[ATH10K_NUM_CHANS];

	 
	bool ch_info_can_report_survey;
	struct completion bss_survey_done;

	struct dfs_pattern_detector *dfs_detector;

	unsigned long tx_paused;  

#ifdef CONFIG_ATH10K_DEBUGFS
	struct ath10k_debug debug;
	struct {
		 
		struct rchan *rfs_chan_spec_scan;

		 
		enum ath10k_spectral_mode mode;
		struct ath10k_spec_scan config;
	} spectral;
#endif

	u32 pktlog_filter;

#ifdef CONFIG_DEV_COREDUMP
	struct {
		struct ath10k_fw_crash_data *fw_crash_data;
	} coredump;
#endif

	struct {
		 
		struct ath10k_fw_components utf_mode_fw;

		 
		bool utf_monitor;
	} testmode;

	struct {
		 
		u32 rx_crc_err_drop;
		u32 fw_crash_counter;
		u32 fw_warm_reset_counter;
		u32 fw_cold_reset_counter;
	} stats;

	struct ath10k_thermal thermal;
	struct ath10k_wow wow;
	struct ath10k_per_peer_tx_stats peer_tx_stats;

	 
	struct net_device napi_dev;
	struct napi_struct napi;

	struct work_struct set_coverage_class_work;
	 
	struct {
		 
		s16 coverage_class;

		u32 reg_phyclk;
		u32 reg_slottime_conf;
		u32 reg_slottime_orig;
		u32 reg_ack_cts_timeout_conf;
		u32 reg_ack_cts_timeout_orig;
	} fw_coverage;

	u32 ampdu_reference;

	const u8 *wmi_key_cipher;
	void *ce_priv;

	u32 sta_tid_stats_mask;

	 
	enum ath10k_radar_confirmation_state radar_conf_state;
	struct ath10k_radar_found_info last_radar_info;
	struct work_struct radar_confirmation_work;
	struct ath10k_bus_params bus_param;
	struct completion peer_delete_done;

	bool coex_support;
	int coex_gpio_pin;

	s32 tx_power_2g_limit;
	s32 tx_power_5g_limit;

	 
	u8 drv_priv[] __aligned(sizeof(void *));
};

static inline bool ath10k_peer_stats_enabled(struct ath10k *ar)
{
	if (test_bit(ATH10K_FLAG_PEER_STATS, &ar->dev_flags) &&
	    test_bit(WMI_SERVICE_PEER_STATS, ar->wmi.svc_map))
		return true;

	return false;
}

extern unsigned int ath10k_frame_mode;
extern unsigned long ath10k_coredump_mask;

void ath10k_core_napi_sync_disable(struct ath10k *ar);
void ath10k_core_napi_enable(struct ath10k *ar);
struct ath10k *ath10k_core_create(size_t priv_size, struct device *dev,
				  enum ath10k_bus bus,
				  enum ath10k_hw_rev hw_rev,
				  const struct ath10k_hif_ops *hif_ops);
void ath10k_core_destroy(struct ath10k *ar);
void ath10k_core_get_fw_features_str(struct ath10k *ar,
				     char *buf,
				     size_t max_len);
int ath10k_core_fetch_firmware_api_n(struct ath10k *ar, const char *name,
				     struct ath10k_fw_file *fw_file);

int ath10k_core_start(struct ath10k *ar, enum ath10k_firmware_mode mode,
		      const struct ath10k_fw_components *fw_components);
int ath10k_wait_for_suspend(struct ath10k *ar, u32 suspend_opt);
void ath10k_core_stop(struct ath10k *ar);
void ath10k_core_start_recovery(struct ath10k *ar);
int ath10k_core_register(struct ath10k *ar,
			 const struct ath10k_bus_params *bus_params);
void ath10k_core_unregister(struct ath10k *ar);
int ath10k_core_fetch_board_file(struct ath10k *ar, int bd_ie_type);
int ath10k_core_check_dt(struct ath10k *ar);
void ath10k_core_free_board_files(struct ath10k *ar);

#endif  
