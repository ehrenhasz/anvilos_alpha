#ifndef __iwl_fw_api_stats_h__
#define __iwl_fw_api_stats_h__
#include "mac.h"
struct mvm_statistics_dbg {
	__le32 burst_check;
	__le32 burst_count;
	__le32 wait_for_silence_timeout_cnt;
	u8 reserved[12];
} __packed;  
struct mvm_statistics_div {
	__le32 tx_on_a;
	__le32 tx_on_b;
	__le32 exec_time;
	__le32 probe_time;
	__le32 rssi_ant;
	__le32 reserved2;
} __packed;  
struct mvm_statistics_rx_non_phy {
	__le32 bogus_cts;
	__le32 bogus_ack;
	__le32 non_channel_beacons;
	__le32 channel_beacons;
	__le32 num_missed_bcon;
	__le32 adc_rx_saturation_time;
	__le32 ina_detection_search_time;
	__le32 beacon_silence_rssi_a;
	__le32 beacon_silence_rssi_b;
	__le32 beacon_silence_rssi_c;
	__le32 interference_data_flag;
	__le32 channel_load;
	__le32 beacon_rssi_a;
	__le32 beacon_rssi_b;
	__le32 beacon_rssi_c;
	__le32 beacon_energy_a;
	__le32 beacon_energy_b;
	__le32 beacon_energy_c;
	__le32 num_bt_kills;
	__le32 mac_id;
} __packed;  
struct mvm_statistics_rx_non_phy_v3 {
	__le32 bogus_cts;	 
	__le32 bogus_ack;	 
	__le32 non_bssid_frames;	 
	__le32 filtered_frames;	 
	__le32 non_channel_beacons;	 
	__le32 channel_beacons;	 
	__le32 num_missed_bcon;	 
	__le32 adc_rx_saturation_time;	 
	__le32 ina_detection_search_time; 
	__le32 beacon_silence_rssi_a;	 
	__le32 beacon_silence_rssi_b;	 
	__le32 beacon_silence_rssi_c;	 
	__le32 interference_data_flag;	 
	__le32 channel_load;		 
	__le32 dsp_false_alarms;	 
	__le32 beacon_rssi_a;
	__le32 beacon_rssi_b;
	__le32 beacon_rssi_c;
	__le32 beacon_energy_a;
	__le32 beacon_energy_b;
	__le32 beacon_energy_c;
	__le32 num_bt_kills;
	__le32 mac_id;
	__le32 directed_data_mpdu;
} __packed;  
struct mvm_statistics_rx_phy {
	__le32 unresponded_rts;
	__le32 rxe_frame_lmt_overrun;
	__le32 sent_ba_rsp_cnt;
	__le32 dsp_self_kill;
	__le32 reserved;
} __packed;  
struct mvm_statistics_rx_phy_v2 {
	__le32 ina_cnt;
	__le32 fina_cnt;
	__le32 plcp_err;
	__le32 crc32_err;
	__le32 overrun_err;
	__le32 early_overrun_err;
	__le32 crc32_good;
	__le32 false_alarm_cnt;
	__le32 fina_sync_err_cnt;
	__le32 sfd_timeout;
	__le32 fina_timeout;
	__le32 unresponded_rts;
	__le32 rxe_frame_lmt_overrun;
	__le32 sent_ack_cnt;
	__le32 sent_cts_cnt;
	__le32 sent_ba_rsp_cnt;
	__le32 dsp_self_kill;
	__le32 mh_format_err;
	__le32 re_acq_main_rssi_sum;
	__le32 reserved;
} __packed;  
struct mvm_statistics_rx_ht_phy_v1 {
	__le32 plcp_err;
	__le32 overrun_err;
	__le32 early_overrun_err;
	__le32 crc32_good;
	__le32 crc32_err;
	__le32 mh_format_err;
	__le32 agg_crc32_good;
	__le32 agg_mpdu_cnt;
	__le32 agg_cnt;
	__le32 unsupport_mcs;
} __packed;   
struct mvm_statistics_rx_ht_phy {
	__le32 mh_format_err;
	__le32 agg_mpdu_cnt;
	__le32 agg_cnt;
	__le32 unsupport_mcs;
} __packed;   
struct mvm_statistics_tx_non_phy_v3 {
	__le32 preamble_cnt;
	__le32 rx_detected_cnt;
	__le32 bt_prio_defer_cnt;
	__le32 bt_prio_kill_cnt;
	__le32 few_bytes_cnt;
	__le32 cts_timeout;
	__le32 ack_timeout;
	__le32 expected_ack_cnt;
	__le32 actual_ack_cnt;
	__le32 dump_msdu_cnt;
	__le32 burst_abort_next_frame_mismatch_cnt;
	__le32 burst_abort_missing_next_frame_cnt;
	__le32 cts_timeout_collision;
	__le32 ack_or_ba_timeout_collision;
} __packed;  
struct mvm_statistics_tx_non_phy {
	__le32 bt_prio_defer_cnt;
	__le32 bt_prio_kill_cnt;
	__le32 few_bytes_cnt;
	__le32 cts_timeout;
	__le32 ack_timeout;
	__le32 dump_msdu_cnt;
	__le32 burst_abort_next_frame_mismatch_cnt;
	__le32 burst_abort_missing_next_frame_cnt;
	__le32 cts_timeout_collision;
	__le32 ack_or_ba_timeout_collision;
} __packed;  
#define MAX_CHAINS 3
struct mvm_statistics_tx_non_phy_agg {
	__le32 ba_timeout;
	__le32 ba_reschedule_frames;
	__le32 scd_query_agg_frame_cnt;
	__le32 scd_query_no_agg;
	__le32 scd_query_agg;
	__le32 scd_query_mismatch;
	__le32 frame_not_ready;
	__le32 underrun;
	__le32 bt_prio_kill;
	__le32 rx_ba_rsp_cnt;
	__s8 txpower[MAX_CHAINS];
	__s8 reserved;
	__le32 reserved2;
} __packed;  
struct mvm_statistics_tx_channel_width {
	__le32 ext_cca_narrow_ch20[1];
	__le32 ext_cca_narrow_ch40[2];
	__le32 ext_cca_narrow_ch80[3];
	__le32 ext_cca_narrow_ch160[4];
	__le32 last_tx_ch_width_indx;
	__le32 rx_detected_per_ch_width[4];
	__le32 success_per_ch_width[4];
	__le32 fail_per_ch_width[4];
};  
struct mvm_statistics_tx_v4 {
	struct mvm_statistics_tx_non_phy_v3 general;
	struct mvm_statistics_tx_non_phy_agg agg;
	struct mvm_statistics_tx_channel_width channel_width;
} __packed;  
struct mvm_statistics_tx {
	struct mvm_statistics_tx_non_phy general;
	struct mvm_statistics_tx_non_phy_agg agg;
	struct mvm_statistics_tx_channel_width channel_width;
} __packed;  
struct mvm_statistics_bt_activity {
	__le32 hi_priority_tx_req_cnt;
	__le32 hi_priority_tx_denied_cnt;
	__le32 lo_priority_tx_req_cnt;
	__le32 lo_priority_tx_denied_cnt;
	__le32 hi_priority_rx_req_cnt;
	__le32 hi_priority_rx_denied_cnt;
	__le32 lo_priority_rx_req_cnt;
	__le32 lo_priority_rx_denied_cnt;
} __packed;   
struct mvm_statistics_general_common_v19 {
	__le32 radio_temperature;
	__le32 radio_voltage;
	struct mvm_statistics_dbg dbg;
	__le32 sleep_time;
	__le32 slots_out;
	__le32 slots_idle;
	__le32 ttl_timestamp;
	struct mvm_statistics_div slow_div;
	__le32 rx_enable_counter;
	__le32 num_of_sos_states;
	__le32 beacon_filtered;
	__le32 missed_beacons;
	u8 beacon_filter_average_energy;
	u8 beacon_filter_reason;
	u8 beacon_filter_current_energy;
	u8 beacon_filter_reserved;
	__le32 beacon_filter_delta_time;
	struct mvm_statistics_bt_activity bt_activity;
	__le64 rx_time;
	__le64 on_time_rf;
	__le64 on_time_scan;
	__le64 tx_time;
} __packed;
struct mvm_statistics_general_common {
	__le32 radio_temperature;
	struct mvm_statistics_dbg dbg;
	__le32 sleep_time;
	__le32 slots_out;
	__le32 slots_idle;
	__le32 ttl_timestamp;
	struct mvm_statistics_div slow_div;
	__le32 rx_enable_counter;
	__le32 num_of_sos_states;
	__le32 beacon_filtered;
	__le32 missed_beacons;
	u8 beacon_filter_average_energy;
	u8 beacon_filter_reason;
	u8 beacon_filter_current_energy;
	u8 beacon_filter_reserved;
	__le32 beacon_filter_delta_time;
	struct mvm_statistics_bt_activity bt_activity;
	__le64 rx_time;
	__le64 on_time_rf;
	__le64 on_time_scan;
	__le64 tx_time;
} __packed;  
struct mvm_statistics_general_v8 {
	struct mvm_statistics_general_common_v19 common;
	__le32 beacon_counter[NUM_MAC_INDEX];
	u8 beacon_average_energy[NUM_MAC_INDEX];
	u8 reserved[4 - (NUM_MAC_INDEX % 4)];
} __packed;  
struct mvm_statistics_general {
	struct mvm_statistics_general_common common;
	__le32 beacon_counter[MAC_INDEX_AUX];
	u8 beacon_average_energy[MAC_INDEX_AUX];
	u8 reserved[8 - MAC_INDEX_AUX];
} __packed;  
struct mvm_statistics_load {
	__le32 air_time[MAC_INDEX_AUX];
	__le32 byte_count[MAC_INDEX_AUX];
	__le32 pkt_count[MAC_INDEX_AUX];
	u8 avg_energy[IWL_MVM_STATION_COUNT_MAX];
} __packed;  
struct mvm_statistics_load_v1 {
	__le32 air_time[NUM_MAC_INDEX];
	__le32 byte_count[NUM_MAC_INDEX];
	__le32 pkt_count[NUM_MAC_INDEX];
	u8 avg_energy[IWL_MVM_STATION_COUNT_MAX];
} __packed;  
struct mvm_statistics_rx {
	struct mvm_statistics_rx_phy ofdm;
	struct mvm_statistics_rx_phy cck;
	struct mvm_statistics_rx_non_phy general;
	struct mvm_statistics_rx_ht_phy ofdm_ht;
} __packed;  
struct mvm_statistics_rx_v3 {
	struct mvm_statistics_rx_phy_v2 ofdm;
	struct mvm_statistics_rx_phy_v2 cck;
	struct mvm_statistics_rx_non_phy_v3 general;
	struct mvm_statistics_rx_ht_phy_v1 ofdm_ht;
} __packed;  
struct iwl_notif_statistics_v10 {
	__le32 flag;
	struct mvm_statistics_rx_v3 rx;
	struct mvm_statistics_tx_v4 tx;
	struct mvm_statistics_general_v8 general;
} __packed;  
struct iwl_notif_statistics_v11 {
	__le32 flag;
	struct mvm_statistics_rx_v3 rx;
	struct mvm_statistics_tx_v4 tx;
	struct mvm_statistics_general_v8 general;
	struct mvm_statistics_load_v1 load_stats;
} __packed;  
struct iwl_notif_statistics {
	__le32 flag;
	struct mvm_statistics_rx rx;
	struct mvm_statistics_tx tx;
	struct mvm_statistics_general general;
	struct mvm_statistics_load load_stats;
} __packed;  
enum iwl_statistics_notif_flags {
	IWL_STATISTICS_REPLY_FLG_CLEAR		= 0x1,
};
enum iwl_statistics_cmd_flags {
	IWL_STATISTICS_FLG_CLEAR		= 0x1,
	IWL_STATISTICS_FLG_DISABLE_NOTIF	= 0x2,
};
struct iwl_statistics_cmd {
	__le32 flags;
} __packed;  
#define MAX_BCAST_FILTER_NUM		8
enum iwl_fw_statistics_type {
	FW_STATISTICS_OPERATIONAL,
	FW_STATISTICS_PHY,
	FW_STATISTICS_MAC,
	FW_STATISTICS_RX,
	FW_STATISTICS_TX,
	FW_STATISTICS_DURATION,
	FW_STATISTICS_HE,
};  
#define IWL_STATISTICS_TYPE_MSK 0x7f
struct iwl_statistics_ntfy_hdr {
	u8 type;
	u8 version;
	__le16 size;
};  
struct iwl_statistics_ntfy_per_mac {
	__le32 beacon_filter_average_energy;
	__le32 air_time;
	__le32 beacon_counter;
	__le32 beacon_average_energy;
	__le32 beacon_rssi_a;
	__le32 beacon_rssi_b;
	__le32 rx_bytes;
} __packed;  
#define IWL_STATS_MAX_BW_INDEX 5
struct iwl_statistics_ntfy_per_phy {
	__le32 channel_load;
	__le32 channel_load_by_us;
	__le32 channel_load_not_by_us;
	__le32 clt;
	__le32 act;
	__le32 elp;
	__le32 rx_detected_per_ch_width[IWL_STATS_MAX_BW_INDEX];
	__le32 success_per_ch_width[IWL_STATS_MAX_BW_INDEX];
	__le32 fail_per_ch_width[IWL_STATS_MAX_BW_INDEX];
	__le32 last_tx_ch_width_indx;
} __packed;  
struct iwl_statistics_ntfy_per_sta {
	__le32 average_energy;
} __packed;  
#define IWL_STATS_MAX_PHY_OPERTINAL 3
struct iwl_statistics_operational_ntfy {
	struct iwl_statistics_ntfy_hdr hdr;
	__le32 flags;
	struct iwl_statistics_ntfy_per_mac per_mac_stats[MAC_INDEX_AUX];
	struct iwl_statistics_ntfy_per_phy per_phy_stats[IWL_STATS_MAX_PHY_OPERTINAL];
	struct iwl_statistics_ntfy_per_sta per_sta_stats[IWL_MVM_STATION_COUNT_MAX];
	__le64 rx_time;
	__le64 tx_time;
	__le64 on_time_rf;
	__le64 on_time_scan;
} __packed;  
struct iwl_statistics_operational_ntfy_ver_14 {
	struct iwl_statistics_ntfy_hdr hdr;
	__le32 flags;
	__le32 mac_id;
	__le32 beacon_filter_average_energy;
	__le32 beacon_filter_reason;
	__le32 radio_temperature;
	__le32 air_time[MAC_INDEX_AUX];
	__le32 beacon_counter[MAC_INDEX_AUX];
	__le32 beacon_average_energy[MAC_INDEX_AUX];
	__le32 beacon_rssi_a;
	__le32 beacon_rssi_b;
	__le32 rx_bytes[MAC_INDEX_AUX];
	__le64 rx_time;
	__le64 tx_time;
	__le64 on_time_rf;
	__le64 on_time_scan;
	__le32 average_energy[IWL_MVM_STATION_COUNT_MAX];
	__le32 reserved;
} __packed;  
struct iwl_statistics_phy_ntfy {
	struct iwl_statistics_ntfy_hdr hdr;
	__le32 energy_and_config;
	__le32 rssi_band;
	__le32 agc_word;
	__le32 agc_gain;
	__le32 dfe_gain;
	__le32 snr_calc_main;
	__le32 energy_calc_main;
	__le32 snr_calc_aux;
	__le32 dsp_dc_estim_a;
	__le32 dsp_dc_estim_b;
	__le32 ina_detec_type_and_ofdm_corr_comb;
	__le32 cw_corr_comb;
	__le32 rssi_comb;
	__le32 auto_corr_cck;
	__le32 ofdm_fine_freq_and_pina_freq_err;
	__le32 snrm_evm_main;
	__le32 snrm_evm_aux;
	__le32 rx_rate;
	__le32 per_chain_enums_and_dsp_atten_a;
	__le32 target_power_and_power_meas_a;
	__le32 tx_config_as_i_and_ac_a;
	__le32 predist_dcq_and_dci_a;
	__le32 per_chain_enums_and_dsp_atten_b;
	__le32 target_power_and_power_meas_b;
	__le32 tx_config_as_i_and_ac_b;
	__le32 predist_dcq_and_dci_b;
	__le32 tx_rate;
	__le32 tlc_backoff;
	__le32 mpapd_calib_mode_mpapd_calib_type_a;
	__le32 psat_and_phy_power_limit_a;
	__le32 sar_and_regulatory_power_limit_a;
	__le32 mpapd_calib_mode_mpapd_calib_type_b;
	__le32 psat_and_phy_power_limit_b;
	__le32 sar_and_regulatory_power_limit_b;
	__le32 srd_and_driver_power_limits;
	__le32 reserved;
} __packed;  
struct iwl_statistics_mac_ntfy {
	struct iwl_statistics_ntfy_hdr hdr;
	__le32 bcast_filter_passed_per_mac[NUM_MAC_INDEX_CDB];
	__le32 bcast_filter_dropped_per_mac[NUM_MAC_INDEX_CDB];
	__le32 bcast_filter_passed_per_filter[MAX_BCAST_FILTER_NUM];
	__le32 bcast_filter_dropped_per_filter[MAX_BCAST_FILTER_NUM];
	__le32 reserved;
} __packed;  
struct iwl_statistics_rx_ntfy {
	struct iwl_statistics_ntfy_hdr hdr;
	__le32 rx_agg_mpdu_cnt;
	__le32 rx_agg_cnt;
	__le32 unsupported_mcs;
	__le32 bogus_cts;
	__le32 bogus_ack;
	__le32 rx_byte_count[MAC_INDEX_AUX];
	__le32 rx_packet_count[MAC_INDEX_AUX];
	__le32 missed_beacons;
	__le32 unresponded_rts;
	__le32 rxe_frame_limit_overrun;
	__le32 sent_ba_rsp_cnt;
	__le32 late_rx_handle;
	__le32 num_bt_kills;
	__le32 reserved;
} __packed;  
struct iwl_statistics_tx_ntfy {
	struct iwl_statistics_ntfy_hdr hdr;
	__le32 cts_timeout;
	__le32 ack_timeout;
	__le32 dump_msdu_cnt;
	__le32 burst_abort_missing_next_frame_cnt;
	__le32 cts_timeout_collision;
	__le32 ack_or_ba_timeout_collision;
	__le32 ba_timeout;
	__le32 ba_reschedule_frames;
	__le32 scd_query_agg_frame_cnt;
	__le32 scd_query_no_agg;
	__le32 scd_query_agg;
	__le32 scd_query_mismatch;
	__le32 agg_terminated_underrun;
	__le32 agg_terminated_bt_prio_kill;
	__le32 tx_kill_on_long_retry;
	__le32 tx_kill_on_short_retry;
	__le32 tx_deffer_counter;
	__le32 tx_deffer_base_time;
	__le32 tx_underrun;
	__le32 bt_defer;
	__le32 tx_kill_on_dsp_timeout;
	__le32 tx_kill_on_immediate_quiet;
	__le32 kill_ba_cnt;
	__le32 kill_ack_cnt;
	__le32 kill_cts_cnt;
	__le32 burst_terminated;
	__le32 late_tx_vec_wr_cnt;
	__le32 late_rx2_tx_cnt;
	__le32 scd_query_cnt;
	__le32 tx_frames_acked_in_agg;
	__le32 last_tx_ch_width_indx;
	__le32 rx_detected_per_ch_width[4];
	__le32 success_per_ch_width[4];
	__le32 fail_per_ch_width[4];
	__le32 reserved;
} __packed;  
struct iwl_statistics_duration_ntfy {
	struct iwl_statistics_ntfy_hdr hdr;
	__le32 cont_burst_chk_cnt;
	__le32 cont_burst_cnt;
	__le32 wait_for_silence_timeout_cnt;
	__le32 reserved;
} __packed;  
struct iwl_statistics_he_ntfy {
	struct iwl_statistics_ntfy_hdr hdr;
	__le32 rx_siga_valid_cnt;
	__le32 rx_siga_invalid_cnt;
	__le32 rx_trig_based_frame_cnt;
	__le32 rx_su_frame_cnt;
	__le32 rx_sigb_invalid_cnt;
	__le32 rx_our_bss_color_cnt;
	__le32 rx_other_bss_color_cnt;
	__le32 rx_zero_bss_color_cnt;
	__le32 rx_mu_for_us_cnt;
	__le32 rx_mu_not_for_us_cnt;
	__le32 rx_mu_nss_ar[2];
	__le32 rx_mu_mimo_cnt;
	__le32 rx_mu_ru_bw_ar[7];
	__le32 rx_trig_for_us_cnt;
	__le32 rx_trig_not_for_us_cnt;
	__le32 rx_trig_with_cs_req_cnt;
	__le32 rx_trig_type_ar[8 + 1];
	__le32 rx_trig_in_agg_cnt;
	__le32 rx_basic_trig_alloc_nss_ar[2];
	__le32 rx_basic_trig_alloc_mu_mimo_cnt;
	__le32 rx_basic_trig_alloc_ru_bw_ar[7];
	__le32 rx_basic_trig_total_byte_cnt;
	__le32 tx_trig_based_cs_req_fail_cnt;
	__le32 tx_trig_based_sifs_ok_cnt;
	__le32 tx_trig_based_sifs_fail_cnt;
	__le32 tx_trig_based_byte_cnt;
	__le32 tx_trig_based_pad_byte_cnt;
	__le32 tx_trig_based_frame_cnt;
	__le32 tx_trig_based_acked_frame_cnt;
	__le32 tx_trig_based_ack_timeout_cnt;
	__le32 tx_su_frame_cnt;
	__le32 tx_edca_to_mu_edca_cnt;
	__le32 tx_mu_edca_to_edca_by_timeout_cnt;
	__le32 tx_mu_edca_to_edca_by_ack_fail_cnt;
	__le32 tx_mu_edca_to_edca_by_small_alloc_cnt;
	__le32 reserved;
} __packed;  
#endif  
