#ifndef	__R8192UDM_H__
#define __R8192UDM_H__
#define         DM_DIG_THRESH_HIGH                      40
#define         DM_DIG_THRESH_LOW                       35
#define         DM_DIG_HIGH_PWR_THRESH_HIGH             75
#define         DM_DIG_HIGH_PWR_THRESH_LOW              70
#define         BW_AUTO_SWITCH_HIGH_LOW                 25
#define         BW_AUTO_SWITCH_LOW_HIGH                 30
#define         DM_DIG_BACKOFF                          12
#define         DM_DIG_MAX                            0x36
#define         DM_DIG_MIN                            0x1c
#define         DM_DIG_MIN_NETCORE                    0x12
#define         RX_PATH_SELECTION_SS_TH_LOW             30
#define         RX_PATH_SELECTION_DIFF_TH               18
#define         RATE_ADAPTIVE_TH_HIGH                   50
#define         RATE_ADAPTIVE_TH_LOW_20M                30
#define         RATE_ADAPTIVE_TH_LOW_40M                10
#define         VERY_LOW_RSSI                           15
#define         CTS_TO_SELF_TH_VAL                      30
#define         E_FOR_TX_POWER_TRACK                   300
#define         TX_POWER_NEAR_FIELD_THRESH_HIGH         68
#define         TX_POWER_NEAR_FIELD_THRESH_LOW          62
#define         TX_POWER_ATHEROAP_THRESH_HIGH           78
#define         TX_POWER_ATHEROAP_THRESH_LOW            72
#define         CURRENT_TX_RATE_REG                  0x1b8
#define         INITIAL_TX_RATE_REG                  0x1b9
#define         TX_RETRY_COUNT_REG                   0x1ac
#define         REG_C38_TH                              20
enum dig_algorithm {
	DIG_ALGO_BY_FALSE_ALARM = 0,
	DIG_ALGO_BY_RSSI	= 1,
};
enum dynamic_init_gain_state {
	DM_STA_DIG_OFF = 0,
	DM_STA_DIG_ON,
	DM_STA_DIG_MAX
};
enum dig_connect {
	DIG_DISCONNECT = 0,
	DIG_CONNECT    = 1,
};
enum dig_pkt_detection_threshold {
	DIG_PD_AT_LOW_POWER    = 0,
	DIG_PD_AT_NORMAL_POWER = 1,
	DIG_PD_AT_HIGH_POWER   = 2,
};
enum dig_cck_cs_ratio_state {
	DIG_CS_RATIO_LOWER  = 0,
	DIG_CS_RATIO_HIGHER = 1,
};
struct dig {
	u8                                 dig_enable_flag;
	enum dig_algorithm                 dig_algorithm;
	u8                                 dig_algorithm_switch;
	long                               rssi_low_thresh;
	long                               rssi_high_thresh;
	long                               rssi_high_power_lowthresh;
	long                               rssi_high_power_highthresh;
	enum dynamic_init_gain_state       dig_state;
	enum dynamic_init_gain_state       dig_highpwr_state;
	enum dig_connect                   cur_connect_state;
	enum dig_connect                   pre_connect_state;
	enum dig_pkt_detection_threshold   curpd_thstate;
	enum dig_pkt_detection_threshold   prepd_thstate;
	enum dig_cck_cs_ratio_state        curcs_ratio_state;
	enum dig_cck_cs_ratio_state        precs_ratio_state;
	u32                                pre_ig_value;
	u32                                cur_ig_value;
	u8                                 backoff_val;
	u8                                 rx_gain_range_min;
	long                               rssi_val;
};
enum cck_rx_path_method {
	CCK_RX_VERSION_1 = 0,
	CCK_RX_VERSION_2 = 1,
};
struct dynamic_rx_path_sel {
	enum cck_rx_path_method            cck_method;
	u8                                 cck_rx_path;
	u8                                 disabled_rf;
	u8                                 rf_rssi[4];
	u8                                 rf_enable_rssi_th[4];
	long                               cck_pwdb_sta[4];
};
struct tx_config_cmd {
	u32     cmd_op;         
	u32     cmd_length;     
	u32     cmd_value;
};
extern struct dig dm_digtable;
extern u8 dm_shadow[16][256];
void init_hal_dm(struct net_device *dev);
void deinit_hal_dm(struct net_device *dev);
void hal_dm_watchdog(struct net_device *dev);
void init_rate_adaptive(struct net_device *dev);
void dm_txpower_trackingcallback(struct work_struct *work);
void dm_restore_dynamic_mechanism_state(struct net_device *dev);
void dm_force_tx_fw_info(struct net_device *dev,
			 u32 force_type, u32 force_value);
void dm_init_edca_turbo(struct net_device *dev);
void dm_rf_operation_test_callback(unsigned long data);
void dm_rf_pathcheck_workitemcallback(struct work_struct *work);
void dm_fsync_work_callback(struct work_struct *work);
void dm_cck_txpower_adjust(struct net_device *dev, bool  binch14);
void dm_initialize_txpower_tracking(struct net_device *dev);
#endif	 
