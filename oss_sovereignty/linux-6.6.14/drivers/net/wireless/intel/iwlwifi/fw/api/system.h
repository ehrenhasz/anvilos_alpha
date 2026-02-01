 
 
#ifndef __iwl_fw_api_system_h__
#define __iwl_fw_api_system_h__

#define SOC_CONFIG_CMD_FLAGS_DISCRETE		BIT(0)
#define SOC_CONFIG_CMD_FLAGS_LOW_LATENCY	BIT(1)

#define SOC_FLAGS_LTR_APPLY_DELAY_MASK		0xc
#define SOC_FLAGS_LTR_APPLY_DELAY_NONE		0
#define SOC_FLAGS_LTR_APPLY_DELAY_200		1
#define SOC_FLAGS_LTR_APPLY_DELAY_2500		2
#define SOC_FLAGS_LTR_APPLY_DELAY_1820		3

 
struct iwl_soc_configuration_cmd {
	__le32 flags;
	__le32 latency;
} __packed;  

 
struct iwl_system_features_control_cmd {
	__le32 features[4];
} __packed;  

#endif  
