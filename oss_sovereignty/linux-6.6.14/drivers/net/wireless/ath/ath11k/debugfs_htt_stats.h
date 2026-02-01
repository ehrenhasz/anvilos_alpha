 
 

#ifndef DEBUG_HTT_STATS_H
#define DEBUG_HTT_STATS_H

#define HTT_STATS_COOKIE_LSB    GENMASK_ULL(31, 0)
#define HTT_STATS_COOKIE_MSB    GENMASK_ULL(63, 32)
#define HTT_STATS_MAGIC_VALUE   0xF0F0F0F0

enum htt_tlv_tag_t {
	HTT_STATS_TX_PDEV_CMN_TAG                           = 0,
	HTT_STATS_TX_PDEV_UNDERRUN_TAG                      = 1,
	HTT_STATS_TX_PDEV_SIFS_TAG                          = 2,
	HTT_STATS_TX_PDEV_FLUSH_TAG                         = 3,
	HTT_STATS_TX_PDEV_PHY_ERR_TAG                       = 4,
	HTT_STATS_STRING_TAG                                = 5,
	HTT_STATS_TX_HWQ_CMN_TAG                            = 6,
	HTT_STATS_TX_HWQ_DIFS_LATENCY_TAG                   = 7,
	HTT_STATS_TX_HWQ_CMD_RESULT_TAG                     = 8,
	HTT_STATS_TX_HWQ_CMD_STALL_TAG                      = 9,
	HTT_STATS_TX_HWQ_FES_STATUS_TAG                     = 10,
	HTT_STATS_TX_TQM_GEN_MPDU_TAG                       = 11,
	HTT_STATS_TX_TQM_LIST_MPDU_TAG                      = 12,
	HTT_STATS_TX_TQM_LIST_MPDU_CNT_TAG                  = 13,
	HTT_STATS_TX_TQM_CMN_TAG                            = 14,
	HTT_STATS_TX_TQM_PDEV_TAG                           = 15,
	HTT_STATS_TX_TQM_CMDQ_STATUS_TAG                    = 16,
	HTT_STATS_TX_DE_EAPOL_PACKETS_TAG                   = 17,
	HTT_STATS_TX_DE_CLASSIFY_FAILED_TAG                 = 18,
	HTT_STATS_TX_DE_CLASSIFY_STATS_TAG                  = 19,
	HTT_STATS_TX_DE_CLASSIFY_STATUS_TAG                 = 20,
	HTT_STATS_TX_DE_ENQUEUE_PACKETS_TAG                 = 21,
	HTT_STATS_TX_DE_ENQUEUE_DISCARD_TAG                 = 22,
	HTT_STATS_TX_DE_CMN_TAG                             = 23,
	HTT_STATS_RING_IF_TAG                               = 24,
	HTT_STATS_TX_PDEV_MU_MIMO_STATS_TAG                 = 25,
	HTT_STATS_SFM_CMN_TAG                               = 26,
	HTT_STATS_SRING_STATS_TAG                           = 27,
	HTT_STATS_RX_PDEV_FW_STATS_TAG                      = 28,
	HTT_STATS_RX_PDEV_FW_RING_MPDU_ERR_TAG              = 29,
	HTT_STATS_RX_PDEV_FW_MPDU_DROP_TAG                  = 30,
	HTT_STATS_RX_SOC_FW_STATS_TAG                       = 31,
	HTT_STATS_RX_SOC_FW_REFILL_RING_EMPTY_TAG           = 32,
	HTT_STATS_RX_SOC_FW_REFILL_RING_NUM_REFILL_TAG      = 33,
	HTT_STATS_TX_PDEV_RATE_STATS_TAG                    = 34,
	HTT_STATS_RX_PDEV_RATE_STATS_TAG                    = 35,
	HTT_STATS_TX_PDEV_SCHEDULER_TXQ_STATS_TAG           = 36,
	HTT_STATS_TX_SCHED_CMN_TAG                          = 37,
	HTT_STATS_TX_PDEV_MUMIMO_MPDU_STATS_TAG             = 38,
	HTT_STATS_SCHED_TXQ_CMD_POSTED_TAG                  = 39,
	HTT_STATS_RING_IF_CMN_TAG                           = 40,
	HTT_STATS_SFM_CLIENT_USER_TAG                       = 41,
	HTT_STATS_SFM_CLIENT_TAG                            = 42,
	HTT_STATS_TX_TQM_ERROR_STATS_TAG                    = 43,
	HTT_STATS_SCHED_TXQ_CMD_REAPED_TAG                  = 44,
	HTT_STATS_SRING_CMN_TAG                             = 45,
	HTT_STATS_TX_SELFGEN_AC_ERR_STATS_TAG               = 46,
	HTT_STATS_TX_SELFGEN_CMN_STATS_TAG                  = 47,
	HTT_STATS_TX_SELFGEN_AC_STATS_TAG                   = 48,
	HTT_STATS_TX_SELFGEN_AX_STATS_TAG                   = 49,
	HTT_STATS_TX_SELFGEN_AX_ERR_STATS_TAG               = 50,
	HTT_STATS_TX_HWQ_MUMIMO_SCH_STATS_TAG               = 51,
	HTT_STATS_TX_HWQ_MUMIMO_MPDU_STATS_TAG              = 52,
	HTT_STATS_TX_HWQ_MUMIMO_CMN_STATS_TAG               = 53,
	HTT_STATS_HW_INTR_MISC_TAG                          = 54,
	HTT_STATS_HW_WD_TIMEOUT_TAG                         = 55,
	HTT_STATS_HW_PDEV_ERRS_TAG                          = 56,
	HTT_STATS_COUNTER_NAME_TAG                          = 57,
	HTT_STATS_TX_TID_DETAILS_TAG                        = 58,
	HTT_STATS_RX_TID_DETAILS_TAG                        = 59,
	HTT_STATS_PEER_STATS_CMN_TAG                        = 60,
	HTT_STATS_PEER_DETAILS_TAG                          = 61,
	HTT_STATS_PEER_TX_RATE_STATS_TAG                    = 62,
	HTT_STATS_PEER_RX_RATE_STATS_TAG                    = 63,
	HTT_STATS_PEER_MSDU_FLOWQ_TAG                       = 64,
	HTT_STATS_TX_DE_COMPL_STATS_TAG                     = 65,
	HTT_STATS_WHAL_TX_TAG                               = 66,
	HTT_STATS_TX_PDEV_SIFS_HIST_TAG                     = 67,
	HTT_STATS_RX_PDEV_FW_STATS_PHY_ERR_TAG              = 68,
	HTT_STATS_TX_TID_DETAILS_V1_TAG                     = 69,
	HTT_STATS_PDEV_CCA_1SEC_HIST_TAG                    = 70,
	HTT_STATS_PDEV_CCA_100MSEC_HIST_TAG                 = 71,
	HTT_STATS_PDEV_CCA_STAT_CUMULATIVE_TAG              = 72,
	HTT_STATS_PDEV_CCA_COUNTERS_TAG                     = 73,
	HTT_STATS_TX_PDEV_MPDU_STATS_TAG                    = 74,
	HTT_STATS_PDEV_TWT_SESSIONS_TAG                     = 75,
	HTT_STATS_PDEV_TWT_SESSION_TAG                      = 76,
	HTT_STATS_RX_REFILL_RXDMA_ERR_TAG                   = 77,
	HTT_STATS_RX_REFILL_REO_ERR_TAG                     = 78,
	HTT_STATS_RX_REO_RESOURCE_STATS_TAG                 = 79,
	HTT_STATS_TX_SOUNDING_STATS_TAG                     = 80,
	HTT_STATS_TX_PDEV_TX_PPDU_STATS_TAG                 = 81,
	HTT_STATS_TX_PDEV_TRIED_MPDU_CNT_HIST_TAG           = 82,
	HTT_STATS_TX_HWQ_TRIED_MPDU_CNT_HIST_TAG            = 83,
	HTT_STATS_TX_HWQ_TXOP_USED_CNT_HIST_TAG             = 84,
	HTT_STATS_TX_DE_FW2WBM_RING_FULL_HIST_TAG           = 85,
	HTT_STATS_SCHED_TXQ_SCHED_ORDER_SU_TAG              = 86,
	HTT_STATS_SCHED_TXQ_SCHED_INELIGIBILITY_TAG         = 87,
	HTT_STATS_PDEV_OBSS_PD_TAG                          = 88,
	HTT_STATS_HW_WAR_TAG				    = 89,
	HTT_STATS_RING_BACKPRESSURE_STATS_TAG		    = 90,
	HTT_STATS_PEER_CTRL_PATH_TXRX_STATS_TAG		    = 101,
	HTT_STATS_PDEV_TX_RATE_TXBF_STATS_TAG		    = 108,
	HTT_STATS_TXBF_OFDMA_NDPA_STATS_TAG		    = 113,
	HTT_STATS_TXBF_OFDMA_NDP_STATS_TAG		    = 114,
	HTT_STATS_TXBF_OFDMA_BRP_STATS_TAG		    = 115,
	HTT_STATS_TXBF_OFDMA_STEER_STATS_TAG		    = 116,
	HTT_STATS_PHY_COUNTERS_TAG			    = 121,
	HTT_STATS_PHY_STATS_TAG				    = 122,
	HTT_STATS_PHY_RESET_COUNTERS_TAG		    = 123,
	HTT_STATS_PHY_RESET_STATS_TAG			    = 124,

	HTT_STATS_MAX_TAG,
};

#define HTT_STATS_MAX_STRING_SZ32            4
#define HTT_STATS_MACID_INVALID              0xff
#define HTT_TX_HWQ_MAX_DIFS_LATENCY_BINS     10
#define HTT_TX_HWQ_MAX_CMD_RESULT_STATS      13
#define HTT_TX_HWQ_MAX_CMD_STALL_STATS       5
#define HTT_TX_HWQ_MAX_FES_RESULT_STATS      10

enum htt_tx_pdev_underrun_enum {
	HTT_STATS_TX_PDEV_NO_DATA_UNDERRUN           = 0,
	HTT_STATS_TX_PDEV_DATA_UNDERRUN_BETWEEN_MPDU = 1,
	HTT_STATS_TX_PDEV_DATA_UNDERRUN_WITHIN_MPDU  = 2,
	HTT_TX_PDEV_MAX_URRN_STATS                   = 3,
};

#define HTT_TX_PDEV_MAX_FLUSH_REASON_STATS     71
#define HTT_TX_PDEV_MAX_SIFS_BURST_STATS       9
#define HTT_TX_PDEV_MAX_SIFS_BURST_HIST_STATS  10
#define HTT_TX_PDEV_MAX_PHY_ERR_STATS          18
#define HTT_TX_PDEV_SCHED_TX_MODE_MAX          4
#define HTT_TX_PDEV_NUM_SCHED_ORDER_LOG        20

#define HTT_RX_STATS_REFILL_MAX_RING         4
#define HTT_RX_STATS_RXDMA_MAX_ERR           16
#define HTT_RX_STATS_FW_DROP_REASON_MAX      16

 
 
struct htt_stats_string_tlv {
	  
	DECLARE_FLEX_ARRAY(u32, data);
} __packed;

#define HTT_STATS_MAC_ID	GENMASK(7, 0)

 
struct htt_tx_pdev_stats_cmn_tlv {
	u32 mac_id__word;
	u32 hw_queued;
	u32 hw_reaped;
	u32 underrun;
	u32 hw_paused;
	u32 hw_flush;
	u32 hw_filt;
	u32 tx_abort;
	u32 mpdu_requeued;
	u32 tx_xretry;
	u32 data_rc;
	u32 mpdu_dropped_xretry;
	u32 illgl_rate_phy_err;
	u32 cont_xretry;
	u32 tx_timeout;
	u32 pdev_resets;
	u32 phy_underrun;
	u32 txop_ovf;
	u32 seq_posted;
	u32 seq_failed_queueing;
	u32 seq_completed;
	u32 seq_restarted;
	u32 mu_seq_posted;
	u32 seq_switch_hw_paused;
	u32 next_seq_posted_dsr;
	u32 seq_posted_isr;
	u32 seq_ctrl_cached;
	u32 mpdu_count_tqm;
	u32 msdu_count_tqm;
	u32 mpdu_removed_tqm;
	u32 msdu_removed_tqm;
	u32 mpdus_sw_flush;
	u32 mpdus_hw_filter;
	u32 mpdus_truncated;
	u32 mpdus_ack_failed;
	u32 mpdus_expired;
	u32 mpdus_seq_hw_retry;
	u32 ack_tlv_proc;
	u32 coex_abort_mpdu_cnt_valid;
	u32 coex_abort_mpdu_cnt;
	u32 num_total_ppdus_tried_ota;
	u32 num_data_ppdus_tried_ota;
	u32 local_ctrl_mgmt_enqued;
	u32 local_ctrl_mgmt_freed;
	u32 local_data_enqued;
	u32 local_data_freed;
	u32 mpdu_tried;
	u32 isr_wait_seq_posted;

	u32 tx_active_dur_us_low;
	u32 tx_active_dur_us_high;
};

 
struct htt_tx_pdev_stats_urrn_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, urrn_stats);
};

 
struct htt_tx_pdev_stats_flush_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, flush_errs);
};

 
struct htt_tx_pdev_stats_sifs_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, sifs_status);
};

 
struct htt_tx_pdev_stats_phy_err_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, phy_errs);
};

 
struct htt_tx_pdev_stats_sifs_hist_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, sifs_hist_status);
};

struct htt_tx_pdev_stats_tx_ppdu_stats_tlv_v {
	u32 num_data_ppdus_legacy_su;
	u32 num_data_ppdus_ac_su;
	u32 num_data_ppdus_ax_su;
	u32 num_data_ppdus_ac_su_txbf;
	u32 num_data_ppdus_ax_su_txbf;
};

 
struct htt_tx_pdev_stats_tried_mpdu_cnt_hist_tlv_v {
	u32 hist_bin_size;
	u32 tried_mpdu_cnt_hist[];  
};

 

 
#define HTT_STATS_MAX_HW_INTR_NAME_LEN 8
struct htt_hw_stats_intr_misc_tlv {
	 
	u8 hw_intr_name[HTT_STATS_MAX_HW_INTR_NAME_LEN];
	u32 mask;
	u32 count;
};

#define HTT_STATS_MAX_HW_MODULE_NAME_LEN 8
struct htt_hw_stats_wd_timeout_tlv {
	 
	u8 hw_module_name[HTT_STATS_MAX_HW_MODULE_NAME_LEN];
	u32 count;
};

struct htt_hw_stats_pdev_errs_tlv {
	u32    mac_id__word;  
	u32    tx_abort;
	u32    tx_abort_fail_count;
	u32    rx_abort;
	u32    rx_abort_fail_count;
	u32    warm_reset;
	u32    cold_reset;
	u32    tx_flush;
	u32    tx_glb_reset;
	u32    tx_txq_reset;
	u32    rx_timeout_reset;
};

struct htt_hw_stats_whal_tx_tlv {
	u32 mac_id__word;
	u32 last_unpause_ppdu_id;
	u32 hwsch_unpause_wait_tqm_write;
	u32 hwsch_dummy_tlv_skipped;
	u32 hwsch_misaligned_offset_received;
	u32 hwsch_reset_count;
	u32 hwsch_dev_reset_war;
	u32 hwsch_delayed_pause;
	u32 hwsch_long_delayed_pause;
	u32 sch_rx_ppdu_no_response;
	u32 sch_selfgen_response;
	u32 sch_rx_sifs_resp_trigger;
};

 
#define	HTT_MSDU_FLOW_STATS_TX_FLOW_NO	GENMASK(15, 0)
#define	HTT_MSDU_FLOW_STATS_TID_NUM	GENMASK(19, 16)
#define	HTT_MSDU_FLOW_STATS_DROP_RULE	BIT(20)

struct htt_msdu_flow_stats_tlv {
	u32 last_update_timestamp;
	u32 last_add_timestamp;
	u32 last_remove_timestamp;
	u32 total_processed_msdu_count;
	u32 cur_msdu_count_in_flowq;
	u32 sw_peer_id;
	u32 tx_flow_no__tid_num__drop_rule;
	u32 last_cycle_enqueue_count;
	u32 last_cycle_dequeue_count;
	u32 last_cycle_drop_count;
	u32 current_drop_th;
};

#define MAX_HTT_TID_NAME 8

#define	HTT_TX_TID_STATS_SW_PEER_ID		GENMASK(15, 0)
#define	HTT_TX_TID_STATS_TID_NUM		GENMASK(31, 16)
#define	HTT_TX_TID_STATS_NUM_SCHED_PENDING	GENMASK(7, 0)
#define	HTT_TX_TID_STATS_NUM_PPDU_IN_HWQ	GENMASK(15, 8)

 
struct htt_tx_tid_stats_tlv {
	 
	u8     tid_name[MAX_HTT_TID_NAME];
	u32 sw_peer_id__tid_num;
	u32 num_sched_pending__num_ppdu_in_hwq;
	u32 tid_flags;
	u32 hw_queued;
	u32 hw_reaped;
	u32 mpdus_hw_filter;

	u32 qdepth_bytes;
	u32 qdepth_num_msdu;
	u32 qdepth_num_mpdu;
	u32 last_scheduled_tsmp;
	u32 pause_module_id;
	u32 block_module_id;
	u32 tid_tx_airtime;
};

#define	HTT_TX_TID_STATS_V1_SW_PEER_ID		GENMASK(15, 0)
#define	HTT_TX_TID_STATS_V1_TID_NUM		GENMASK(31, 16)
#define	HTT_TX_TID_STATS_V1_NUM_SCHED_PENDING	GENMASK(7, 0)
#define	HTT_TX_TID_STATS_V1_NUM_PPDU_IN_HWQ	GENMASK(15, 8)

 
struct htt_tx_tid_stats_v1_tlv {
	 
	u8 tid_name[MAX_HTT_TID_NAME];
	u32 sw_peer_id__tid_num;
	u32 num_sched_pending__num_ppdu_in_hwq;
	u32 tid_flags;
	u32 max_qdepth_bytes;
	u32 max_qdepth_n_msdus;
	u32 rsvd;

	u32 qdepth_bytes;
	u32 qdepth_num_msdu;
	u32 qdepth_num_mpdu;
	u32 last_scheduled_tsmp;
	u32 pause_module_id;
	u32 block_module_id;
	u32 tid_tx_airtime;
	u32 allow_n_flags;
	u32 sendn_frms_allowed;
};

#define	HTT_RX_TID_STATS_SW_PEER_ID	GENMASK(15, 0)
#define	HTT_RX_TID_STATS_TID_NUM	GENMASK(31, 16)

struct htt_rx_tid_stats_tlv {
	u32 sw_peer_id__tid_num;
	u8 tid_name[MAX_HTT_TID_NAME];
	u32 dup_in_reorder;
	u32 dup_past_outside_window;
	u32 dup_past_within_window;
	u32 rxdesc_err_decrypt;
	u32 tid_rx_airtime;
};

#define HTT_MAX_COUNTER_NAME 8
struct htt_counter_tlv {
	u8 counter_name[HTT_MAX_COUNTER_NAME];
	u32 count;
};

struct htt_peer_stats_cmn_tlv {
	u32 ppdu_cnt;
	u32 mpdu_cnt;
	u32 msdu_cnt;
	u32 pause_bitmap;
	u32 block_bitmap;
	u32 current_timestamp;
	u32 peer_tx_airtime;
	u32 peer_rx_airtime;
	s32 rssi;
	u32 peer_enqueued_count_low;
	u32 peer_enqueued_count_high;
	u32 peer_dequeued_count_low;
	u32 peer_dequeued_count_high;
	u32 peer_dropped_count_low;
	u32 peer_dropped_count_high;
	u32 ppdu_transmitted_bytes_low;
	u32 ppdu_transmitted_bytes_high;
	u32 peer_ttl_removed_count;
	u32 inactive_time;
};

#define HTT_PEER_DETAILS_VDEV_ID	GENMASK(7, 0)
#define HTT_PEER_DETAILS_PDEV_ID	GENMASK(15, 8)
#define HTT_PEER_DETAILS_AST_IDX	GENMASK(31, 16)

struct htt_peer_details_tlv {
	u32 peer_type;
	u32 sw_peer_id;
	u32 vdev_pdev_ast_idx;
	struct htt_mac_addr mac_addr;
	u32 peer_flags;
	u32 qpeer_flags;
};

enum htt_stats_param_type {
	HTT_STATS_PREAM_OFDM,
	HTT_STATS_PREAM_CCK,
	HTT_STATS_PREAM_HT,
	HTT_STATS_PREAM_VHT,
	HTT_STATS_PREAM_HE,
	HTT_STATS_PREAM_RSVD,
	HTT_STATS_PREAM_RSVD1,

	HTT_STATS_PREAM_COUNT,
};

#define HTT_TX_PEER_STATS_NUM_MCS_COUNTERS        12
#define HTT_TX_PEER_STATS_NUM_GI_COUNTERS          4
#define HTT_TX_PEER_STATS_NUM_DCM_COUNTERS         5
#define HTT_TX_PEER_STATS_NUM_BW_COUNTERS          4
#define HTT_TX_PEER_STATS_NUM_SPATIAL_STREAMS      8
#define HTT_TX_PEER_STATS_NUM_PREAMBLE_TYPES       HTT_STATS_PREAM_COUNT

struct htt_tx_peer_rate_stats_tlv {
	u32 tx_ldpc;
	u32 rts_cnt;
	u32 ack_rssi;

	u32 tx_mcs[HTT_TX_PEER_STATS_NUM_MCS_COUNTERS];
	u32 tx_su_mcs[HTT_TX_PEER_STATS_NUM_MCS_COUNTERS];
	u32 tx_mu_mcs[HTT_TX_PEER_STATS_NUM_MCS_COUNTERS];
	 
	u32 tx_nss[HTT_TX_PEER_STATS_NUM_SPATIAL_STREAMS];
	 
	u32 tx_bw[HTT_TX_PEER_STATS_NUM_BW_COUNTERS];
	u32 tx_stbc[HTT_TX_PEER_STATS_NUM_MCS_COUNTERS];
	u32 tx_pream[HTT_TX_PEER_STATS_NUM_PREAMBLE_TYPES];

	 
	u32 tx_gi[HTT_TX_PEER_STATS_NUM_GI_COUNTERS][HTT_TX_PEER_STATS_NUM_MCS_COUNTERS];

	 
	u32 tx_dcm[HTT_TX_PEER_STATS_NUM_DCM_COUNTERS];

};

#define HTT_RX_PEER_STATS_NUM_MCS_COUNTERS        12
#define HTT_RX_PEER_STATS_NUM_GI_COUNTERS          4
#define HTT_RX_PEER_STATS_NUM_DCM_COUNTERS         5
#define HTT_RX_PEER_STATS_NUM_BW_COUNTERS          4
#define HTT_RX_PEER_STATS_NUM_SPATIAL_STREAMS      8
#define HTT_RX_PEER_STATS_NUM_PREAMBLE_TYPES       HTT_STATS_PREAM_COUNT

struct htt_rx_peer_rate_stats_tlv {
	u32 nsts;

	 
	u32 rx_ldpc;
	 
	u32 rts_cnt;

	u32 rssi_mgmt;  
	u32 rssi_data;  
	u32 rssi_comb;  
	u32 rx_mcs[HTT_RX_PEER_STATS_NUM_MCS_COUNTERS];
	 
	u32 rx_nss[HTT_RX_PEER_STATS_NUM_SPATIAL_STREAMS];
	u32 rx_dcm[HTT_RX_PEER_STATS_NUM_DCM_COUNTERS];
	u32 rx_stbc[HTT_RX_PEER_STATS_NUM_MCS_COUNTERS];
	 
	u32 rx_bw[HTT_RX_PEER_STATS_NUM_BW_COUNTERS];
	u32 rx_pream[HTT_RX_PEER_STATS_NUM_PREAMBLE_TYPES];
	 
	u8 rssi_chain[HTT_RX_PEER_STATS_NUM_SPATIAL_STREAMS]
		     [HTT_RX_PEER_STATS_NUM_BW_COUNTERS];

	 
	u32 rx_gi[HTT_RX_PEER_STATS_NUM_GI_COUNTERS]
		 [HTT_RX_PEER_STATS_NUM_MCS_COUNTERS];
};

enum htt_peer_stats_req_mode {
	HTT_PEER_STATS_REQ_MODE_NO_QUERY,
	HTT_PEER_STATS_REQ_MODE_QUERY_TQM,
	HTT_PEER_STATS_REQ_MODE_FLUSH_TQM,
};

enum htt_peer_stats_tlv_enum {
	HTT_PEER_STATS_CMN_TLV       = 0,
	HTT_PEER_DETAILS_TLV         = 1,
	HTT_TX_PEER_RATE_STATS_TLV   = 2,
	HTT_RX_PEER_RATE_STATS_TLV   = 3,
	HTT_TX_TID_STATS_TLV         = 4,
	HTT_RX_TID_STATS_TLV         = 5,
	HTT_MSDU_FLOW_STATS_TLV      = 6,

	HTT_PEER_STATS_MAX_TLV       = 31,
};

 
 
struct htt_tx_hwq_mu_mimo_sch_stats_tlv {
	u32 mu_mimo_sch_posted;
	u32 mu_mimo_sch_failed;
	u32 mu_mimo_ppdu_posted;
};

struct htt_tx_hwq_mu_mimo_mpdu_stats_tlv {
	u32 mu_mimo_mpdus_queued_usr;
	u32 mu_mimo_mpdus_tried_usr;
	u32 mu_mimo_mpdus_failed_usr;
	u32 mu_mimo_mpdus_requeued_usr;
	u32 mu_mimo_err_no_ba_usr;
	u32 mu_mimo_mpdu_underrun_usr;
	u32 mu_mimo_ampdu_underrun_usr;
};

#define	HTT_TX_HWQ_STATS_MAC_ID	GENMASK(7, 0)
#define	HTT_TX_HWQ_STATS_HWQ_ID	GENMASK(15, 8)

struct htt_tx_hwq_mu_mimo_cmn_stats_tlv {
	u32 mac_id__hwq_id__word;
};

 
struct htt_tx_hwq_stats_cmn_tlv {
	u32 mac_id__hwq_id__word;

	 
	u32 xretry;
	u32 underrun_cnt;
	u32 flush_cnt;
	u32 filt_cnt;
	u32 null_mpdu_bmap;
	u32 user_ack_failure;
	u32 ack_tlv_proc;
	u32 sched_id_proc;
	u32 null_mpdu_tx_count;
	u32 mpdu_bmap_not_recvd;

	 
	u32 num_bar;
	u32 rts;
	u32 cts2self;
	u32 qos_null;

	 
	u32 mpdu_tried_cnt;
	u32 mpdu_queued_cnt;
	u32 mpdu_ack_fail_cnt;
	u32 mpdu_filt_cnt;
	u32 false_mpdu_ack_count;

	u32 txq_timeout;
};

 
struct htt_tx_hwq_difs_latency_stats_tlv_v {
	u32 hist_intvl;
	 
	u32 difs_latency_hist[];  
};

 
struct htt_tx_hwq_cmd_result_stats_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, cmd_result);
};

 
struct htt_tx_hwq_cmd_stall_stats_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, cmd_stall_status);
};

 
struct htt_tx_hwq_fes_result_stats_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, fes_result);
};

 
struct htt_tx_hwq_tried_mpdu_cnt_hist_tlv_v {
	u32 hist_bin_size;
	 
	u32 tried_mpdu_cnt_hist[];  
};

 
struct htt_tx_hwq_txop_used_cnt_hist_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, txop_used_cnt_hist);
};

 
struct htt_tx_selfgen_cmn_stats_tlv {
	u32 mac_id__word;
	u32 su_bar;
	u32 rts;
	u32 cts2self;
	u32 qos_null;
	u32 delayed_bar_1;  
	u32 delayed_bar_2;  
	u32 delayed_bar_3;  
	u32 delayed_bar_4;  
	u32 delayed_bar_5;  
	u32 delayed_bar_6;  
	u32 delayed_bar_7;  
};

struct htt_tx_selfgen_ac_stats_tlv {
	 
	u32 ac_su_ndpa;
	u32 ac_su_ndp;
	u32 ac_mu_mimo_ndpa;
	u32 ac_mu_mimo_ndp;
	u32 ac_mu_mimo_brpoll_1;  
	u32 ac_mu_mimo_brpoll_2;  
	u32 ac_mu_mimo_brpoll_3;  
};

struct htt_tx_selfgen_ax_stats_tlv {
	 
	u32 ax_su_ndpa;
	u32 ax_su_ndp;
	u32 ax_mu_mimo_ndpa;
	u32 ax_mu_mimo_ndp;
	u32 ax_mu_mimo_brpoll_1;  
	u32 ax_mu_mimo_brpoll_2;  
	u32 ax_mu_mimo_brpoll_3;  
	u32 ax_mu_mimo_brpoll_4;  
	u32 ax_mu_mimo_brpoll_5;  
	u32 ax_mu_mimo_brpoll_6;  
	u32 ax_mu_mimo_brpoll_7;  
	u32 ax_basic_trigger;
	u32 ax_bsr_trigger;
	u32 ax_mu_bar_trigger;
	u32 ax_mu_rts_trigger;
	u32 ax_ulmumimo_trigger;
};

struct htt_tx_selfgen_ac_err_stats_tlv {
	 
	u32 ac_su_ndp_err;
	u32 ac_su_ndpa_err;
	u32 ac_mu_mimo_ndpa_err;
	u32 ac_mu_mimo_ndp_err;
	u32 ac_mu_mimo_brp1_err;
	u32 ac_mu_mimo_brp2_err;
	u32 ac_mu_mimo_brp3_err;
};

struct htt_tx_selfgen_ax_err_stats_tlv {
	 
	u32 ax_su_ndp_err;
	u32 ax_su_ndpa_err;
	u32 ax_mu_mimo_ndpa_err;
	u32 ax_mu_mimo_ndp_err;
	u32 ax_mu_mimo_brp1_err;
	u32 ax_mu_mimo_brp2_err;
	u32 ax_mu_mimo_brp3_err;
	u32 ax_mu_mimo_brp4_err;
	u32 ax_mu_mimo_brp5_err;
	u32 ax_mu_mimo_brp6_err;
	u32 ax_mu_mimo_brp7_err;
	u32 ax_basic_trigger_err;
	u32 ax_bsr_trigger_err;
	u32 ax_mu_bar_trigger_err;
	u32 ax_mu_rts_trigger_err;
	u32 ax_ulmumimo_trigger_err;
};

 
#define HTT_TX_PDEV_STATS_NUM_AC_MUMIMO_USER_STATS 4
#define HTT_TX_PDEV_STATS_NUM_AX_MUMIMO_USER_STATS 8
#define HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS    74
#define HTT_TX_PDEV_STATS_NUM_UL_MUMIMO_USER_STATS 8

struct htt_tx_pdev_mu_mimo_sch_stats_tlv {
	 
	u32 mu_mimo_sch_posted;
	u32 mu_mimo_sch_failed;
	 
	u32 mu_mimo_ppdu_posted;
	 
	u32 ac_mu_mimo_sch_nusers[HTT_TX_PDEV_STATS_NUM_AC_MUMIMO_USER_STATS];
	u32 ax_mu_mimo_sch_nusers[HTT_TX_PDEV_STATS_NUM_AX_MUMIMO_USER_STATS];
	u32 ax_ofdma_sch_nusers[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	u32 ax_ul_ofdma_basic_sch_nusers[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	u32 ax_ul_ofdma_bsr_sch_nusers[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	u32 ax_ul_ofdma_bar_sch_nusers[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	u32 ax_ul_ofdma_brp_sch_nusers[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];

	 
	 
	u32 ax_ul_mumimo_basic_sch_nusers[HTT_TX_PDEV_STATS_NUM_UL_MUMIMO_USER_STATS];

	 
	u32 ax_ul_mumimo_brp_sch_nusers[HTT_TX_PDEV_STATS_NUM_UL_MUMIMO_USER_STATS];

	u32 ac_mu_mimo_sch_posted_per_grp_sz[HTT_TX_PDEV_STATS_NUM_AC_MUMIMO_USER_STATS];
	u32 ax_mu_mimo_sch_posted_per_grp_sz[HTT_TX_PDEV_STATS_NUM_AX_MUMIMO_USER_STATS];
};

struct htt_tx_pdev_mu_mimo_mpdu_stats_tlv {
	u32 mu_mimo_mpdus_queued_usr;
	u32 mu_mimo_mpdus_tried_usr;
	u32 mu_mimo_mpdus_failed_usr;
	u32 mu_mimo_mpdus_requeued_usr;
	u32 mu_mimo_err_no_ba_usr;
	u32 mu_mimo_mpdu_underrun_usr;
	u32 mu_mimo_ampdu_underrun_usr;

	u32 ax_mu_mimo_mpdus_queued_usr;
	u32 ax_mu_mimo_mpdus_tried_usr;
	u32 ax_mu_mimo_mpdus_failed_usr;
	u32 ax_mu_mimo_mpdus_requeued_usr;
	u32 ax_mu_mimo_err_no_ba_usr;
	u32 ax_mu_mimo_mpdu_underrun_usr;
	u32 ax_mu_mimo_ampdu_underrun_usr;

	u32 ax_ofdma_mpdus_queued_usr;
	u32 ax_ofdma_mpdus_tried_usr;
	u32 ax_ofdma_mpdus_failed_usr;
	u32 ax_ofdma_mpdus_requeued_usr;
	u32 ax_ofdma_err_no_ba_usr;
	u32 ax_ofdma_mpdu_underrun_usr;
	u32 ax_ofdma_ampdu_underrun_usr;
};

#define HTT_STATS_TX_SCHED_MODE_MU_MIMO_AC  1
#define HTT_STATS_TX_SCHED_MODE_MU_MIMO_AX  2
#define HTT_STATS_TX_SCHED_MODE_MU_OFDMA_AX 3

struct htt_tx_pdev_mpdu_stats_tlv {
	 
	u32 mpdus_queued_usr;
	u32 mpdus_tried_usr;
	u32 mpdus_failed_usr;
	u32 mpdus_requeued_usr;
	u32 err_no_ba_usr;
	u32 mpdu_underrun_usr;
	u32 ampdu_underrun_usr;
	u32 user_index;
	u32 tx_sched_mode;  
};

 
 
struct htt_sched_txq_cmd_posted_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, sched_cmd_posted);
};

 
struct htt_sched_txq_cmd_reaped_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, sched_cmd_reaped);
};

 
struct htt_sched_txq_sched_order_su_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, sched_order_su);
};

enum htt_sched_txq_sched_ineligibility_tlv_enum {
	HTT_SCHED_TID_SKIP_SCHED_MASK_DISABLED = 0,
	HTT_SCHED_TID_SKIP_NOTIFY_MPDU,
	HTT_SCHED_TID_SKIP_MPDU_STATE_INVALID,
	HTT_SCHED_TID_SKIP_SCHED_DISABLED,
	HTT_SCHED_TID_SKIP_TQM_BYPASS_CMD_PENDING,
	HTT_SCHED_TID_SKIP_SECOND_SU_SCHEDULE,

	HTT_SCHED_TID_SKIP_CMD_SLOT_NOT_AVAIL,
	HTT_SCHED_TID_SKIP_NO_ENQ,
	HTT_SCHED_TID_SKIP_LOW_ENQ,
	HTT_SCHED_TID_SKIP_PAUSED,
	HTT_SCHED_TID_SKIP_UL,
	HTT_SCHED_TID_REMOVE_PAUSED,
	HTT_SCHED_TID_REMOVE_NO_ENQ,
	HTT_SCHED_TID_REMOVE_UL,
	HTT_SCHED_TID_QUERY,
	HTT_SCHED_TID_SU_ONLY,
	HTT_SCHED_TID_ELIGIBLE,
	HTT_SCHED_INELIGIBILITY_MAX,
};

 
struct htt_sched_txq_sched_ineligibility_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, sched_ineligibility);
};

#define	HTT_TX_PDEV_STATS_SCHED_PER_TXQ_MAC_ID	GENMASK(7, 0)
#define	HTT_TX_PDEV_STATS_SCHED_PER_TXQ_ID	GENMASK(15, 8)

struct htt_tx_pdev_stats_sched_per_txq_tlv {
	u32 mac_id__txq_id__word;
	u32 sched_policy;
	u32 last_sched_cmd_posted_timestamp;
	u32 last_sched_cmd_compl_timestamp;
	u32 sched_2_tac_lwm_count;
	u32 sched_2_tac_ring_full;
	u32 sched_cmd_post_failure;
	u32 num_active_tids;
	u32 num_ps_schedules;
	u32 sched_cmds_pending;
	u32 num_tid_register;
	u32 num_tid_unregister;
	u32 num_qstats_queried;
	u32 qstats_update_pending;
	u32 last_qstats_query_timestamp;
	u32 num_tqm_cmdq_full;
	u32 num_de_sched_algo_trigger;
	u32 num_rt_sched_algo_trigger;
	u32 num_tqm_sched_algo_trigger;
	u32 notify_sched;
	u32 dur_based_sendn_term;
};

struct htt_stats_tx_sched_cmn_tlv {
	 
	u32 mac_id__word;
	 
	u32 current_timestamp;
};

 
#define HTT_TX_TQM_MAX_GEN_MPDU_END_REASON          16
#define HTT_TX_TQM_MAX_LIST_MPDU_END_REASON         16
#define HTT_TX_TQM_MAX_LIST_MPDU_CNT_HISTOGRAM_BINS 16

 
struct htt_tx_tqm_gen_mpdu_stats_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, gen_mpdu_end_reason);
};

 
struct htt_tx_tqm_list_mpdu_stats_tlv_v {
	  
	DECLARE_FLEX_ARRAY(u32, list_mpdu_end_reason);
};

 
struct htt_tx_tqm_list_mpdu_cnt_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, list_mpdu_cnt_hist);
};

struct htt_tx_tqm_pdev_stats_tlv_v {
	u32 msdu_count;
	u32 mpdu_count;
	u32 remove_msdu;
	u32 remove_mpdu;
	u32 remove_msdu_ttl;
	u32 send_bar;
	u32 bar_sync;
	u32 notify_mpdu;
	u32 sync_cmd;
	u32 write_cmd;
	u32 hwsch_trigger;
	u32 ack_tlv_proc;
	u32 gen_mpdu_cmd;
	u32 gen_list_cmd;
	u32 remove_mpdu_cmd;
	u32 remove_mpdu_tried_cmd;
	u32 mpdu_queue_stats_cmd;
	u32 mpdu_head_info_cmd;
	u32 msdu_flow_stats_cmd;
	u32 remove_msdu_cmd;
	u32 remove_msdu_ttl_cmd;
	u32 flush_cache_cmd;
	u32 update_mpduq_cmd;
	u32 enqueue;
	u32 enqueue_notify;
	u32 notify_mpdu_at_head;
	u32 notify_mpdu_state_valid;
	 
	u32 sched_udp_notify1;
	u32 sched_udp_notify2;
	u32 sched_nonudp_notify1;
	u32 sched_nonudp_notify2;
};

struct htt_tx_tqm_cmn_stats_tlv {
	u32 mac_id__word;
	u32 max_cmdq_id;
	u32 list_mpdu_cnt_hist_intvl;

	 
	u32 add_msdu;
	u32 q_empty;
	u32 q_not_empty;
	u32 drop_notification;
	u32 desc_threshold;
};

struct htt_tx_tqm_error_stats_tlv {
	 
	u32 q_empty_failure;
	u32 q_not_empty_failure;
	u32 add_msdu_failure;
};

 
#define	HTT_TX_TQM_CMDQ_STATUS_MAC_ID	GENMASK(7, 0)
#define	HTT_TX_TQM_CMDQ_STATUS_CMDQ_ID	GENMASK(15, 8)

struct htt_tx_tqm_cmdq_status_tlv {
	u32 mac_id__cmdq_id__word;
	u32 sync_cmd;
	u32 write_cmd;
	u32 gen_mpdu_cmd;
	u32 mpdu_queue_stats_cmd;
	u32 mpdu_head_info_cmd;
	u32 msdu_flow_stats_cmd;
	u32 remove_mpdu_cmd;
	u32 remove_msdu_cmd;
	u32 flush_cache_cmd;
	u32 update_mpduq_cmd;
	u32 update_msduq_cmd;
};

 
 
struct htt_tx_de_eapol_packets_stats_tlv {
	u32 m1_packets;
	u32 m2_packets;
	u32 m3_packets;
	u32 m4_packets;
	u32 g1_packets;
	u32 g2_packets;
};

struct htt_tx_de_classify_failed_stats_tlv {
	u32 ap_bss_peer_not_found;
	u32 ap_bcast_mcast_no_peer;
	u32 sta_delete_in_progress;
	u32 ibss_no_bss_peer;
	u32 invalid_vdev_type;
	u32 invalid_ast_peer_entry;
	u32 peer_entry_invalid;
	u32 ethertype_not_ip;
	u32 eapol_lookup_failed;
	u32 qpeer_not_allow_data;
	u32 fse_tid_override;
	u32 ipv6_jumbogram_zero_length;
	u32 qos_to_non_qos_in_prog;
};

struct htt_tx_de_classify_stats_tlv {
	u32 arp_packets;
	u32 igmp_packets;
	u32 dhcp_packets;
	u32 host_inspected;
	u32 htt_included;
	u32 htt_valid_mcs;
	u32 htt_valid_nss;
	u32 htt_valid_preamble_type;
	u32 htt_valid_chainmask;
	u32 htt_valid_guard_interval;
	u32 htt_valid_retries;
	u32 htt_valid_bw_info;
	u32 htt_valid_power;
	u32 htt_valid_key_flags;
	u32 htt_valid_no_encryption;
	u32 fse_entry_count;
	u32 fse_priority_be;
	u32 fse_priority_high;
	u32 fse_priority_low;
	u32 fse_traffic_ptrn_be;
	u32 fse_traffic_ptrn_over_sub;
	u32 fse_traffic_ptrn_bursty;
	u32 fse_traffic_ptrn_interactive;
	u32 fse_traffic_ptrn_periodic;
	u32 fse_hwqueue_alloc;
	u32 fse_hwqueue_created;
	u32 fse_hwqueue_send_to_host;
	u32 mcast_entry;
	u32 bcast_entry;
	u32 htt_update_peer_cache;
	u32 htt_learning_frame;
	u32 fse_invalid_peer;
	 
	u32    mec_notify;
};

struct htt_tx_de_classify_status_stats_tlv {
	u32 eok;
	u32 classify_done;
	u32 lookup_failed;
	u32 send_host_dhcp;
	u32 send_host_mcast;
	u32 send_host_unknown_dest;
	u32 send_host;
	u32 status_invalid;
};

struct htt_tx_de_enqueue_packets_stats_tlv {
	u32 enqueued_pkts;
	u32 to_tqm;
	u32 to_tqm_bypass;
};

struct htt_tx_de_enqueue_discard_stats_tlv {
	u32 discarded_pkts;
	u32 local_frames;
	u32 is_ext_msdu;
};

struct htt_tx_de_compl_stats_tlv {
	u32 tcl_dummy_frame;
	u32 tqm_dummy_frame;
	u32 tqm_notify_frame;
	u32 fw2wbm_enq;
	u32 tqm_bypass_frame;
};

 
struct htt_tx_de_fw2wbm_ring_full_hist_tlv {
	DECLARE_FLEX_ARRAY(u32, fw2wbm_ring_full_hist);
};

struct htt_tx_de_cmn_stats_tlv {
	u32   mac_id__word;

	 
	u32   tcl2fw_entry_count;
	u32   not_to_fw;
	u32   invalid_pdev_vdev_peer;
	u32   tcl_res_invalid_addrx;
	u32   wbm2fw_entry_count;
	u32   invalid_pdev;
};

 
#define HTT_STATS_LOW_WM_BINS      5
#define HTT_STATS_HIGH_WM_BINS     5

#define HTT_RING_IF_STATS_NUM_ELEMS		GENMASK(15, 0)
#define	HTT_RING_IF_STATS_PREFETCH_TAIL_INDEX	GENMASK(31, 16)
#define HTT_RING_IF_STATS_HEAD_IDX		GENMASK(15, 0)
#define HTT_RING_IF_STATS_TAIL_IDX		GENMASK(31, 16)
#define HTT_RING_IF_STATS_SHADOW_HEAD_IDX	GENMASK(15, 0)
#define HTT_RING_IF_STATS_SHADOW_TAIL_IDX	GENMASK(31, 16)
#define HTT_RING_IF_STATS_LWM_THRESH		GENMASK(15, 0)
#define HTT_RING_IF_STATS_HWM_THRESH		GENMASK(31, 16)

struct htt_ring_if_stats_tlv {
	u32 base_addr;  
	u32 elem_size;
	u32 num_elems__prefetch_tail_idx;
	u32 head_idx__tail_idx;
	u32 shadow_head_idx__shadow_tail_idx;
	u32 num_tail_incr;
	u32 lwm_thresh__hwm_thresh;
	u32 overrun_hit_count;
	u32 underrun_hit_count;
	u32 prod_blockwait_count;
	u32 cons_blockwait_count;
	u32 low_wm_hit_count[HTT_STATS_LOW_WM_BINS];
	u32 high_wm_hit_count[HTT_STATS_HIGH_WM_BINS];
};

struct htt_ring_if_cmn_tlv {
	u32 mac_id__word;
	u32 num_records;
};

 
 
struct htt_sfm_client_user_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, dwords_used_by_user_n);
};

struct htt_sfm_client_tlv {
	 
	u32 client_id;
	 
	u32 buf_min;
	 
	u32 buf_max;
	 
	u32 buf_busy;
	 
	u32 buf_alloc;
	 
	u32 buf_avail;
	 
	u32 num_users;
};

struct htt_sfm_cmn_tlv {
	u32 mac_id__word;
	 
	u32 buf_total;
	 
	u32 mem_empty;
	 
	u32 deallocate_bufs;
	 
	u32 num_records;
};

 
#define	HTT_SRING_STATS_MAC_ID			GENMASK(7, 0)
#define HTT_SRING_STATS_RING_ID			GENMASK(15, 8)
#define HTT_SRING_STATS_ARENA			GENMASK(23, 16)
#define HTT_SRING_STATS_EP			BIT(24)
#define HTT_SRING_STATS_NUM_AVAIL_WORDS		GENMASK(15, 0)
#define HTT_SRING_STATS_NUM_VALID_WORDS		GENMASK(31, 16)
#define HTT_SRING_STATS_HEAD_PTR		GENMASK(15, 0)
#define HTT_SRING_STATS_TAIL_PTR		GENMASK(31, 16)
#define HTT_SRING_STATS_CONSUMER_EMPTY		GENMASK(15, 0)
#define HTT_SRING_STATS_PRODUCER_FULL		GENMASK(31, 16)
#define HTT_SRING_STATS_PREFETCH_COUNT		GENMASK(15, 0)
#define HTT_SRING_STATS_INTERNAL_TAIL_PTR	GENMASK(31, 16)

struct htt_sring_stats_tlv {
	u32 mac_id__ring_id__arena__ep;
	u32 base_addr_lsb;  
	u32 base_addr_msb;
	u32 ring_size;
	u32 elem_size;

	u32 num_avail_words__num_valid_words;
	u32 head_ptr__tail_ptr;
	u32 consumer_empty__producer_full;
	u32 prefetch_count__internal_tail_ptr;
};

struct htt_sring_cmn_tlv {
	u32 num_records;
};

 
#define HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS        12
#define HTT_TX_PDEV_STATS_NUM_GI_COUNTERS          4
#define HTT_TX_PDEV_STATS_NUM_DCM_COUNTERS         5
#define HTT_TX_PDEV_STATS_NUM_BW_COUNTERS          4
#define HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS      8
#define HTT_TX_PDEV_STATS_NUM_PREAMBLE_TYPES       HTT_STATS_PREAM_COUNT
#define HTT_TX_PDEV_STATS_NUM_LEGACY_CCK_STATS     4
#define HTT_TX_PDEV_STATS_NUM_LEGACY_OFDM_STATS    8
#define HTT_TX_PDEV_STATS_NUM_LTF                  4

#define HTT_TX_NUM_OF_SOUNDING_STATS_WORDS \
	(HTT_TX_PDEV_STATS_NUM_BW_COUNTERS * \
	 HTT_TX_PDEV_STATS_NUM_AX_MUMIMO_USER_STATS)

struct htt_tx_pdev_rate_stats_tlv {
	u32 mac_id__word;
	u32 tx_ldpc;
	u32 rts_cnt;
	 
	u32 ack_rssi;

	u32 tx_mcs[HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];

	u32 tx_su_mcs[HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
	u32 tx_mu_mcs[HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];

	 
	u32 tx_nss[HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	 
	u32 tx_bw[HTT_TX_PDEV_STATS_NUM_BW_COUNTERS];
	u32 tx_stbc[HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
	u32 tx_pream[HTT_TX_PDEV_STATS_NUM_PREAMBLE_TYPES];

	 
	u32 tx_gi[HTT_TX_PDEV_STATS_NUM_GI_COUNTERS][HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];

	 
	u32 tx_dcm[HTT_TX_PDEV_STATS_NUM_DCM_COUNTERS];
	 
	u32 rts_success;

	 
	u32 tx_legacy_cck_rate[HTT_TX_PDEV_STATS_NUM_LEGACY_CCK_STATS];
	u32 tx_legacy_ofdm_rate[HTT_TX_PDEV_STATS_NUM_LEGACY_OFDM_STATS];

	u32 ac_mu_mimo_tx_ldpc;
	u32 ax_mu_mimo_tx_ldpc;
	u32 ofdma_tx_ldpc;

	 
	u32 tx_he_ltf[HTT_TX_PDEV_STATS_NUM_LTF];

	u32 ac_mu_mimo_tx_mcs[HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
	u32 ax_mu_mimo_tx_mcs[HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
	u32 ofdma_tx_mcs[HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];

	u32 ac_mu_mimo_tx_nss[HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	u32 ax_mu_mimo_tx_nss[HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	u32 ofdma_tx_nss[HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];

	u32 ac_mu_mimo_tx_bw[HTT_TX_PDEV_STATS_NUM_BW_COUNTERS];
	u32 ax_mu_mimo_tx_bw[HTT_TX_PDEV_STATS_NUM_BW_COUNTERS];
	u32 ofdma_tx_bw[HTT_TX_PDEV_STATS_NUM_BW_COUNTERS];

	u32 ac_mu_mimo_tx_gi[HTT_TX_PDEV_STATS_NUM_GI_COUNTERS]
			    [HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
	u32 ax_mu_mimo_tx_gi[HTT_TX_PDEV_STATS_NUM_GI_COUNTERS]
			    [HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
	u32 ofdma_tx_gi[HTT_TX_PDEV_STATS_NUM_GI_COUNTERS]
		       [HTT_TX_PDEV_STATS_NUM_MCS_COUNTERS];
};

 
#define HTT_RX_PDEV_STATS_NUM_LEGACY_CCK_STATS     4
#define HTT_RX_PDEV_STATS_NUM_LEGACY_OFDM_STATS    8
#define HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS        12
#define HTT_RX_PDEV_STATS_NUM_GI_COUNTERS          4
#define HTT_RX_PDEV_STATS_NUM_DCM_COUNTERS         5
#define HTT_RX_PDEV_STATS_NUM_BW_COUNTERS          4
#define HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS      8
#define HTT_RX_PDEV_STATS_NUM_PREAMBLE_TYPES       HTT_STATS_PREAM_COUNT
#define HTT_RX_PDEV_MAX_OFDMA_NUM_USER             8
#define HTT_RX_PDEV_STATS_RXEVM_MAX_PILOTS_PER_NSS 16
#define HTT_RX_PDEV_STATS_NUM_RU_SIZE_COUNTERS     6
#define HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER          8

struct htt_rx_pdev_rate_stats_tlv {
	u32 mac_id__word;
	u32 nsts;

	u32 rx_ldpc;
	u32 rts_cnt;

	u32 rssi_mgmt;  
	u32 rssi_data;  
	u32 rssi_comb;  
	u32 rx_mcs[HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS];
	 
	u32 rx_nss[HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	u32 rx_dcm[HTT_RX_PDEV_STATS_NUM_DCM_COUNTERS];
	u32 rx_stbc[HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS];
	 
	u32 rx_bw[HTT_RX_PDEV_STATS_NUM_BW_COUNTERS];
	u32 rx_pream[HTT_RX_PDEV_STATS_NUM_PREAMBLE_TYPES];
	u8 rssi_chain[HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS]
		     [HTT_RX_PDEV_STATS_NUM_BW_COUNTERS];
					 

	 
	u32 rx_gi[HTT_RX_PDEV_STATS_NUM_GI_COUNTERS][HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS];
	s32 rssi_in_dbm;  

	u32 rx_11ax_su_ext;
	u32 rx_11ac_mumimo;
	u32 rx_11ax_mumimo;
	u32 rx_11ax_ofdma;
	u32 txbf;
	u32 rx_legacy_cck_rate[HTT_RX_PDEV_STATS_NUM_LEGACY_CCK_STATS];
	u32 rx_legacy_ofdm_rate[HTT_RX_PDEV_STATS_NUM_LEGACY_OFDM_STATS];
	u32 rx_active_dur_us_low;
	u32 rx_active_dur_us_high;

	u32 rx_11ax_ul_ofdma;

	u32 ul_ofdma_rx_mcs[HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS];
	u32 ul_ofdma_rx_gi[HTT_TX_PDEV_STATS_NUM_GI_COUNTERS]
			  [HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS];
	u32 ul_ofdma_rx_nss[HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	u32 ul_ofdma_rx_bw[HTT_TX_PDEV_STATS_NUM_BW_COUNTERS];
	u32 ul_ofdma_rx_stbc;
	u32 ul_ofdma_rx_ldpc;

	 
	u32 rx_ulofdma_non_data_ppdu[HTT_RX_PDEV_MAX_OFDMA_NUM_USER];  
	u32 rx_ulofdma_data_ppdu[HTT_RX_PDEV_MAX_OFDMA_NUM_USER];      
	u32 rx_ulofdma_mpdu_ok[HTT_RX_PDEV_MAX_OFDMA_NUM_USER];        
	u32 rx_ulofdma_mpdu_fail[HTT_RX_PDEV_MAX_OFDMA_NUM_USER];      

	u32 nss_count;
	u32 pilot_count;
	 
	s32 rx_pilot_evm_db[HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS]
			   [HTT_RX_PDEV_STATS_RXEVM_MAX_PILOTS_PER_NSS];
	 
	s32 rx_pilot_evm_db_mean[HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	s8 rx_ul_fd_rssi[HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS]
			[HTT_RX_PDEV_MAX_OFDMA_NUM_USER];  
	 
	u32 per_chain_rssi_pkt_type;
	s8 rx_per_chain_rssi_in_dbm[HTT_RX_PDEV_STATS_NUM_SPATIAL_STREAMS]
				   [HTT_RX_PDEV_STATS_NUM_BW_COUNTERS];

	u32 rx_su_ndpa;
	u32 rx_11ax_su_txbf_mcs[HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS];
	u32 rx_mu_ndpa;
	u32 rx_11ax_mu_txbf_mcs[HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS];
	u32 rx_br_poll;
	u32 rx_11ax_dl_ofdma_mcs[HTT_RX_PDEV_STATS_NUM_MCS_COUNTERS];
	u32 rx_11ax_dl_ofdma_ru[HTT_RX_PDEV_STATS_NUM_RU_SIZE_COUNTERS];

	u32 rx_ulmumimo_non_data_ppdu[HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER];
	u32 rx_ulmumimo_data_ppdu[HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER];
	u32 rx_ulmumimo_mpdu_ok[HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER];
	u32 rx_ulmumimo_mpdu_fail[HTT_RX_PDEV_MAX_ULMUMIMO_NUM_USER];
	u32 rx_ulofdma_non_data_nusers[HTT_RX_PDEV_MAX_OFDMA_NUM_USER];
	u32 rx_ulofdma_data_nusers[HTT_RX_PDEV_MAX_OFDMA_NUM_USER];
};

 
struct htt_rx_soc_fw_stats_tlv {
	u32 fw_reo_ring_data_msdu;
	u32 fw_to_host_data_msdu_bcmc;
	u32 fw_to_host_data_msdu_uc;
	u32 ofld_remote_data_buf_recycle_cnt;
	u32 ofld_remote_free_buf_indication_cnt;

	u32 ofld_buf_to_host_data_msdu_uc;
	u32 reo_fw_ring_to_host_data_msdu_uc;

	u32 wbm_sw_ring_reap;
	u32 wbm_forward_to_host_cnt;
	u32 wbm_target_recycle_cnt;

	u32 target_refill_ring_recycle_cnt;
};

 
struct htt_rx_soc_fw_refill_ring_empty_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, refill_ring_empty_cnt);
};

 
struct htt_rx_soc_fw_refill_ring_num_refill_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, refill_ring_num_refill);
};

 
enum htt_rx_rxdma_error_code_enum {
	HTT_RX_RXDMA_OVERFLOW_ERR                           = 0,
	HTT_RX_RXDMA_MPDU_LENGTH_ERR                        = 1,
	HTT_RX_RXDMA_FCS_ERR                                = 2,
	HTT_RX_RXDMA_DECRYPT_ERR                            = 3,
	HTT_RX_RXDMA_TKIP_MIC_ERR                           = 4,
	HTT_RX_RXDMA_UNECRYPTED_ERR                         = 5,
	HTT_RX_RXDMA_MSDU_LEN_ERR                           = 6,
	HTT_RX_RXDMA_MSDU_LIMIT_ERR                         = 7,
	HTT_RX_RXDMA_WIFI_PARSE_ERR                         = 8,
	HTT_RX_RXDMA_AMSDU_PARSE_ERR                        = 9,
	HTT_RX_RXDMA_SA_TIMEOUT_ERR                         = 10,
	HTT_RX_RXDMA_DA_TIMEOUT_ERR                         = 11,
	HTT_RX_RXDMA_FLOW_TIMEOUT_ERR                       = 12,
	HTT_RX_RXDMA_FLUSH_REQUEST                          = 13,
	HTT_RX_RXDMA_ERR_CODE_RVSD0                         = 14,
	HTT_RX_RXDMA_ERR_CODE_RVSD1                         = 15,

	 
	HTT_RX_RXDMA_MAX_ERR_CODE
};

 
struct htt_rx_soc_fw_refill_ring_num_rxdma_err_tlv_v {
	DECLARE_FLEX_ARRAY(u32, rxdma_err);  
};

 
enum htt_rx_reo_error_code_enum {
	HTT_RX_REO_QUEUE_DESC_ADDR_ZERO                     = 0,
	HTT_RX_REO_QUEUE_DESC_NOT_VALID                     = 1,
	HTT_RX_AMPDU_IN_NON_BA                              = 2,
	HTT_RX_NON_BA_DUPLICATE                             = 3,
	HTT_RX_BA_DUPLICATE                                 = 4,
	HTT_RX_REGULAR_FRAME_2K_JUMP                        = 5,
	HTT_RX_BAR_FRAME_2K_JUMP                            = 6,
	HTT_RX_REGULAR_FRAME_OOR                            = 7,
	HTT_RX_BAR_FRAME_OOR                                = 8,
	HTT_RX_BAR_FRAME_NO_BA_SESSION                      = 9,
	HTT_RX_BAR_FRAME_SN_EQUALS_SSN                      = 10,
	HTT_RX_PN_CHECK_FAILED                              = 11,
	HTT_RX_2K_ERROR_HANDLING_FLAG_SET                   = 12,
	HTT_RX_PN_ERROR_HANDLING_FLAG_SET                   = 13,
	HTT_RX_QUEUE_DESCRIPTOR_BLOCKED_SET                 = 14,
	HTT_RX_REO_ERR_CODE_RVSD                            = 15,

	 
	HTT_RX_REO_MAX_ERR_CODE
};

 
struct htt_rx_soc_fw_refill_ring_num_reo_err_tlv_v {
	DECLARE_FLEX_ARRAY(u32, reo_err);  
};

 
#define HTT_STATS_SUBTYPE_MAX     16

struct htt_rx_pdev_fw_stats_tlv {
	u32 mac_id__word;
	u32 ppdu_recvd;
	u32 mpdu_cnt_fcs_ok;
	u32 mpdu_cnt_fcs_err;
	u32 tcp_msdu_cnt;
	u32 tcp_ack_msdu_cnt;
	u32 udp_msdu_cnt;
	u32 other_msdu_cnt;
	u32 fw_ring_mpdu_ind;
	u32 fw_ring_mgmt_subtype[HTT_STATS_SUBTYPE_MAX];
	u32 fw_ring_ctrl_subtype[HTT_STATS_SUBTYPE_MAX];
	u32 fw_ring_mcast_data_msdu;
	u32 fw_ring_bcast_data_msdu;
	u32 fw_ring_ucast_data_msdu;
	u32 fw_ring_null_data_msdu;
	u32 fw_ring_mpdu_drop;
	u32 ofld_local_data_ind_cnt;
	u32 ofld_local_data_buf_recycle_cnt;
	u32 drx_local_data_ind_cnt;
	u32 drx_local_data_buf_recycle_cnt;
	u32 local_nondata_ind_cnt;
	u32 local_nondata_buf_recycle_cnt;

	u32 fw_status_buf_ring_refill_cnt;
	u32 fw_status_buf_ring_empty_cnt;
	u32 fw_pkt_buf_ring_refill_cnt;
	u32 fw_pkt_buf_ring_empty_cnt;
	u32 fw_link_buf_ring_refill_cnt;
	u32 fw_link_buf_ring_empty_cnt;

	u32 host_pkt_buf_ring_refill_cnt;
	u32 host_pkt_buf_ring_empty_cnt;
	u32 mon_pkt_buf_ring_refill_cnt;
	u32 mon_pkt_buf_ring_empty_cnt;
	u32 mon_status_buf_ring_refill_cnt;
	u32 mon_status_buf_ring_empty_cnt;
	u32 mon_desc_buf_ring_refill_cnt;
	u32 mon_desc_buf_ring_empty_cnt;
	u32 mon_dest_ring_update_cnt;
	u32 mon_dest_ring_full_cnt;

	u32 rx_suspend_cnt;
	u32 rx_suspend_fail_cnt;
	u32 rx_resume_cnt;
	u32 rx_resume_fail_cnt;
	u32 rx_ring_switch_cnt;
	u32 rx_ring_restore_cnt;
	u32 rx_flush_cnt;
	u32 rx_recovery_reset_cnt;
};

#define HTT_STATS_PHY_ERR_MAX 43

struct htt_rx_pdev_fw_stats_phy_err_tlv {
	u32 mac_id__word;
	u32 total_phy_err_cnt;
	 
	u32 phy_err[HTT_STATS_PHY_ERR_MAX];
};

 
struct htt_rx_pdev_fw_ring_mpdu_err_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, fw_ring_mpdu_err);  
};

 
struct htt_rx_pdev_fw_mpdu_drop_tlv_v {
	 
	DECLARE_FLEX_ARRAY(u32, fw_mpdu_drop);  
};

#define HTT_PDEV_CCA_STATS_TX_FRAME_INFO_PRESENT               (0x1)
#define HTT_PDEV_CCA_STATS_RX_FRAME_INFO_PRESENT               (0x2)
#define HTT_PDEV_CCA_STATS_RX_CLEAR_INFO_PRESENT               (0x4)
#define HTT_PDEV_CCA_STATS_MY_RX_FRAME_INFO_PRESENT            (0x8)
#define HTT_PDEV_CCA_STATS_USEC_CNT_INFO_PRESENT              (0x10)
#define HTT_PDEV_CCA_STATS_MED_RX_IDLE_INFO_PRESENT           (0x20)
#define HTT_PDEV_CCA_STATS_MED_TX_IDLE_GLOBAL_INFO_PRESENT    (0x40)
#define HTT_PDEV_CCA_STATS_CCA_OBBS_USEC_INFO_PRESENT         (0x80)

struct htt_pdev_stats_cca_counters_tlv {
	 
	u32 tx_frame_usec;
	u32 rx_frame_usec;
	u32 rx_clear_usec;
	u32 my_rx_frame_usec;
	u32 usec_cnt;
	u32 med_rx_idle_usec;
	u32 med_tx_idle_global_usec;
	u32 cca_obss_usec;
};

struct htt_pdev_cca_stats_hist_v1_tlv {
	u32    chan_num;
	 
	u32    num_records;
	u32    valid_cca_counters_bitmap;
	u32    collection_interval;

	 
};

struct htt_pdev_stats_twt_session_tlv {
	u32 vdev_id;
	struct htt_mac_addr peer_mac;
	u32 flow_id_flags;

	 
	u32 dialog_id;
	u32 wake_dura_us;
	u32 wake_intvl_us;
	u32 sp_offset_us;
};

struct htt_pdev_stats_twt_sessions_tlv {
	u32 pdev_id;
	u32 num_sessions;
	struct htt_pdev_stats_twt_session_tlv twt_session[];
};

enum htt_rx_reo_resource_sample_id_enum {
	 
	HTT_RX_REO_RESOURCE_GLOBAL_LINK_DESC_COUNT_0           = 0,
	HTT_RX_REO_RESOURCE_GLOBAL_LINK_DESC_COUNT_1           = 1,
	HTT_RX_REO_RESOURCE_GLOBAL_LINK_DESC_COUNT_2           = 2,
	 
	HTT_RX_REO_RESOURCE_BUFFERS_USED_AC0                   = 3,
	HTT_RX_REO_RESOURCE_BUFFERS_USED_AC1                   = 4,
	HTT_RX_REO_RESOURCE_BUFFERS_USED_AC2                   = 5,
	HTT_RX_REO_RESOURCE_BUFFERS_USED_AC3                   = 6,
	 
	HTT_RX_REO_RESOURCE_AGING_NUM_QUEUES_AC0               = 7,
	HTT_RX_REO_RESOURCE_AGING_NUM_QUEUES_AC1               = 8,
	HTT_RX_REO_RESOURCE_AGING_NUM_QUEUES_AC2               = 9,
	HTT_RX_REO_RESOURCE_AGING_NUM_QUEUES_AC3               = 10,

	HTT_RX_REO_RESOURCE_STATS_MAX                          = 16
};

struct htt_rx_reo_resource_stats_tlv_v {
	 
	u32 sample_id;
	u32 total_max;
	u32 total_avg;
	u32 total_sample;
	u32 non_zeros_avg;
	u32 non_zeros_sample;
	u32 last_non_zeros_max;
	u32 last_non_zeros_min;
	u32 last_non_zeros_avg;
	u32 last_non_zeros_sample;
};

 

enum htt_txbf_sound_steer_modes {
	HTT_IMPLICIT_TXBF_STEER_STATS                = 0,
	HTT_EXPLICIT_TXBF_SU_SIFS_STEER_STATS        = 1,
	HTT_EXPLICIT_TXBF_SU_RBO_STEER_STATS         = 2,
	HTT_EXPLICIT_TXBF_MU_SIFS_STEER_STATS        = 3,
	HTT_EXPLICIT_TXBF_MU_RBO_STEER_STATS         = 4,
	HTT_TXBF_MAX_NUM_OF_MODES                    = 5
};

enum htt_stats_sounding_tx_mode {
	HTT_TX_AC_SOUNDING_MODE                      = 0,
	HTT_TX_AX_SOUNDING_MODE                      = 1,
};

struct htt_tx_sounding_stats_tlv {
	u32 tx_sounding_mode;  
	 
	u32 cbf_20[HTT_TXBF_MAX_NUM_OF_MODES];
	u32 cbf_40[HTT_TXBF_MAX_NUM_OF_MODES];
	u32 cbf_80[HTT_TXBF_MAX_NUM_OF_MODES];
	u32 cbf_160[HTT_TXBF_MAX_NUM_OF_MODES];
	 
	u32 sounding[HTT_TX_NUM_OF_SOUNDING_STATS_WORDS];
};

struct htt_pdev_obss_pd_stats_tlv {
	u32 num_obss_tx_ppdu_success;
	u32 num_obss_tx_ppdu_failure;
	u32 num_sr_tx_transmissions;
	u32 num_spatial_reuse_opportunities;
	u32 num_non_srg_opportunities;
	u32 num_non_srg_ppdu_tried;
	u32 num_non_srg_ppdu_success;
	u32 num_srg_opportunities;
	u32 num_srg_ppdu_tried;
	u32 num_srg_ppdu_success;
	u32 num_psr_opportunities;
	u32 num_psr_ppdu_tried;
	u32 num_psr_ppdu_success;
};

struct htt_ring_backpressure_stats_tlv {
	u32 pdev_id;
	u32 current_head_idx;
	u32 current_tail_idx;
	u32 num_htt_msgs_sent;
	 
	u32 backpressure_time_ms;
	 
	u32 backpressure_hist[5];
};

#define HTT_TX_TXBF_RATE_STATS_NUM_MCS_COUNTERS 14
#define HTT_TX_TXBF_RATE_STATS_NUM_BW_COUNTERS 5
#define HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS 8

struct htt_pdev_txrate_txbf_stats_tlv {
	 
	u32 tx_su_txbf_mcs[HTT_TX_TXBF_RATE_STATS_NUM_MCS_COUNTERS];
	 
	u32 tx_su_ibf_mcs[HTT_TX_TXBF_RATE_STATS_NUM_MCS_COUNTERS];
	 
	u32 tx_su_ol_mcs[HTT_TX_TXBF_RATE_STATS_NUM_MCS_COUNTERS];
	 
	u32 tx_su_txbf_nss[HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	 
	u32 tx_su_ibf_nss[HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	 
	u32 tx_su_ol_nss[HTT_TX_PDEV_STATS_NUM_SPATIAL_STREAMS];
	 
	u32 tx_su_txbf_bw[HTT_TX_TXBF_RATE_STATS_NUM_BW_COUNTERS];
	 
	u32 tx_su_ibf_bw[HTT_TX_TXBF_RATE_STATS_NUM_BW_COUNTERS];
	 
	u32 tx_su_ol_bw[HTT_TX_TXBF_RATE_STATS_NUM_BW_COUNTERS];
};

struct htt_txbf_ofdma_ndpa_stats_tlv {
	 
	u32 ax_ofdma_ndpa_queued[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	 
	u32 ax_ofdma_ndpa_tried[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	 
	u32 ax_ofdma_ndpa_flushed[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	 
	u32 ax_ofdma_ndpa_err[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
};

struct htt_txbf_ofdma_ndp_stats_tlv {
	 
	u32 ax_ofdma_ndp_queued[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	 
	u32 ax_ofdma_ndp_tried[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	 
	u32 ax_ofdma_ndp_flushed[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	 
	u32 ax_ofdma_ndp_err[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
};

struct htt_txbf_ofdma_brp_stats_tlv {
	 
	u32 ax_ofdma_brpoll_queued[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	 
	u32 ax_ofdma_brpoll_tried[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	 
	u32 ax_ofdma_brpoll_flushed[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	 
	u32 ax_ofdma_brp_err[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	 
	u32 ax_ofdma_brp_err_num_cbf_rcvd[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS + 1];
};

struct htt_txbf_ofdma_steer_stats_tlv {
	 
	u32 ax_ofdma_num_ppdu_steer[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	 
	u32 ax_ofdma_num_ppdu_ol[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	 
	u32 ax_ofdma_num_usrs_prefetch[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	 
	u32 ax_ofdma_num_usrs_sound[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
	 
	u32 ax_ofdma_num_usrs_force_sound[HTT_TX_PDEV_STATS_NUM_OFDMA_USER_STATS];
};

#define HTT_MAX_RX_PKT_CNT 8
#define HTT_MAX_RX_PKT_CRC_PASS_CNT 8
#define HTT_MAX_PER_BLK_ERR_CNT 20
#define HTT_MAX_RX_OTA_ERR_CNT 14
#define HTT_STATS_MAX_CHAINS 8
#define ATH11K_STATS_MGMT_FRM_TYPE_MAX 16

struct htt_phy_counters_tlv {
	 
	u32 rx_ofdma_timing_err_cnt;
	 
	u32 rx_cck_fail_cnt;
	 
	u32 mactx_abort_cnt;
	 
	u32 macrx_abort_cnt;
	 
	u32 phytx_abort_cnt;
	 
	u32 phyrx_abort_cnt;
	 
	u32 phyrx_defer_abort_cnt;
	 
	u32 rx_gain_adj_lstf_event_cnt;
	 
	u32 rx_gain_adj_non_legacy_cnt;
	 
	u32 rx_pkt_cnt[HTT_MAX_RX_PKT_CNT];
	 
	u32 rx_pkt_crc_pass_cnt[HTT_MAX_RX_PKT_CRC_PASS_CNT];
	 
	u32 per_blk_err_cnt[HTT_MAX_PER_BLK_ERR_CNT];
	 
	u32 rx_ota_err_cnt[HTT_MAX_RX_OTA_ERR_CNT];
};

struct htt_phy_stats_tlv {
	 
	s32 nf_chain[HTT_STATS_MAX_CHAINS];
	 
	u32 false_radar_cnt;
	 
	u32 radar_cs_cnt;
	 
	s32 ani_level;
	 
	u32 fw_run_time;
};

struct htt_phy_reset_counters_tlv {
	u32 pdev_id;
	u32 cf_active_low_fail_cnt;
	u32 cf_active_low_pass_cnt;
	u32 phy_off_through_vreg_cnt;
	u32 force_calibration_cnt;
	u32 rf_mode_switch_phy_off_cnt;
};

struct htt_phy_reset_stats_tlv {
	u32 pdev_id;
	u32 chan_mhz;
	u32 chan_band_center_freq1;
	u32 chan_band_center_freq2;
	u32 chan_phy_mode;
	u32 chan_flags;
	u32 chan_num;
	u32 reset_cause;
	u32 prev_reset_cause;
	u32 phy_warm_reset_src;
	u32 rx_gain_tbl_mode;
	u32 xbar_val;
	u32 force_calibration;
	u32 phyrf_mode;
	u32 phy_homechan;
	u32 phy_tx_ch_mask;
	u32 phy_rx_ch_mask;
	u32 phybb_ini_mask;
	u32 phyrf_ini_mask;
	u32 phy_dfs_en_mask;
	u32 phy_sscan_en_mask;
	u32 phy_synth_sel_mask;
	u32 phy_adfs_freq;
	u32 cck_fir_settings;
	u32 phy_dyn_pri_chan;
	u32 cca_thresh;
	u32 dyn_cca_status;
	u32 rxdesense_thresh_hw;
	u32 rxdesense_thresh_sw;
};

struct htt_peer_ctrl_path_txrx_stats_tlv {
	 
	u8 peer_mac_addr[ETH_ALEN];
	u8 rsvd[2];
	 
	u32 peer_tx_mgmt_subtype[ATH11K_STATS_MGMT_FRM_TYPE_MAX];
	 
	u32 peer_rx_mgmt_subtype[ATH11K_STATS_MGMT_FRM_TYPE_MAX];
};

#ifdef CONFIG_ATH11K_DEBUGFS

void ath11k_debugfs_htt_stats_init(struct ath11k *ar);
void ath11k_debugfs_htt_ext_stats_handler(struct ath11k_base *ab,
					  struct sk_buff *skb);
int ath11k_debugfs_htt_stats_req(struct ath11k *ar);

#else  

static inline void ath11k_debugfs_htt_stats_init(struct ath11k *ar)
{
}

static inline void ath11k_debugfs_htt_ext_stats_handler(struct ath11k_base *ab,
							struct sk_buff *skb)
{
}

static inline int ath11k_debugfs_htt_stats_req(struct ath11k *ar)
{
	return 0;
}

#endif  

#endif
