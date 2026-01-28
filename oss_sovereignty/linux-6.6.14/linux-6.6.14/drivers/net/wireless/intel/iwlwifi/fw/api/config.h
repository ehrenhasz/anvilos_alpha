#ifndef __iwl_fw_api_config_h__
#define __iwl_fw_api_config_h__
struct iwl_dqa_enable_cmd {
	__le32 cmd_queue;
} __packed;  
struct iwl_tx_ant_cfg_cmd {
	__le32 valid;
} __packed;
struct iwl_calib_ctrl {
	__le32 flow_trigger;
	__le32 event_trigger;
} __packed;
enum iwl_calib_cfg {
	IWL_CALIB_CFG_XTAL_IDX			= BIT(0),
	IWL_CALIB_CFG_TEMPERATURE_IDX		= BIT(1),
	IWL_CALIB_CFG_VOLTAGE_READ_IDX		= BIT(2),
	IWL_CALIB_CFG_PAPD_IDX			= BIT(3),
	IWL_CALIB_CFG_TX_PWR_IDX		= BIT(4),
	IWL_CALIB_CFG_DC_IDX			= BIT(5),
	IWL_CALIB_CFG_BB_FILTER_IDX		= BIT(6),
	IWL_CALIB_CFG_LO_LEAKAGE_IDX		= BIT(7),
	IWL_CALIB_CFG_TX_IQ_IDX			= BIT(8),
	IWL_CALIB_CFG_TX_IQ_SKEW_IDX		= BIT(9),
	IWL_CALIB_CFG_RX_IQ_IDX			= BIT(10),
	IWL_CALIB_CFG_RX_IQ_SKEW_IDX		= BIT(11),
	IWL_CALIB_CFG_SENSITIVITY_IDX		= BIT(12),
	IWL_CALIB_CFG_CHAIN_NOISE_IDX		= BIT(13),
	IWL_CALIB_CFG_DISCONNECTED_ANT_IDX	= BIT(14),
	IWL_CALIB_CFG_ANT_COUPLING_IDX		= BIT(15),
	IWL_CALIB_CFG_DAC_IDX			= BIT(16),
	IWL_CALIB_CFG_ABS_IDX			= BIT(17),
	IWL_CALIB_CFG_AGC_IDX			= BIT(18),
};
struct iwl_phy_specific_cfg {
	__le32 filter_cfg_chains[4];
} __packed;  
struct iwl_phy_cfg_cmd_v1 {
	__le32	phy_cfg;
	struct iwl_calib_ctrl calib_control;
} __packed;
struct iwl_phy_cfg_cmd_v3 {
	__le32	phy_cfg;
	struct iwl_calib_ctrl calib_control;
	struct iwl_phy_specific_cfg phy_specific_cfg;
} __packed;  
enum iwl_dc2dc_config_id {
	DCDC_LOW_POWER_MODE_MSK_SET  = 0x1,  
	DCDC_FREQ_TUNE_SET = 0x2,
};  
#endif  
