#ifndef __iwl_fw_api_phy_ctxt_h__
#define __iwl_fw_api_phy_ctxt_h__
#define PHY_BAND_5  (0)
#define PHY_BAND_24 (1)
#define PHY_BAND_6 (2)
#define IWL_PHY_CHANNEL_MODE20	0x0
#define IWL_PHY_CHANNEL_MODE40	0x1
#define IWL_PHY_CHANNEL_MODE80	0x2
#define IWL_PHY_CHANNEL_MODE160	0x3
#define IWL_PHY_CHANNEL_MODE320	0x4
#define IWL_PHY_CTRL_POS_ABOVE		0x4
#define IWL_PHY_CTRL_POS_OFFS_EXT	0x8
#define IWL_PHY_CTRL_POS_OFFS_MSK	0x3
struct iwl_fw_channel_info_v1 {
	u8 band;
	u8 channel;
	u8 width;
	u8 ctrl_pos;
} __packed;  
struct iwl_fw_channel_info {
	__le32 channel;
	u8 band;
	u8 width;
	u8 ctrl_pos;
	u8 reserved;
} __packed;  
#define PHY_RX_CHAIN_DRIVER_FORCE_POS	(0)
#define PHY_RX_CHAIN_DRIVER_FORCE_MSK \
	(0x1 << PHY_RX_CHAIN_DRIVER_FORCE_POS)
#define PHY_RX_CHAIN_VALID_POS		(1)
#define PHY_RX_CHAIN_VALID_MSK \
	(0x7 << PHY_RX_CHAIN_VALID_POS)
#define PHY_RX_CHAIN_FORCE_SEL_POS	(4)
#define PHY_RX_CHAIN_FORCE_SEL_MSK \
	(0x7 << PHY_RX_CHAIN_FORCE_SEL_POS)
#define PHY_RX_CHAIN_FORCE_MIMO_SEL_POS	(7)
#define PHY_RX_CHAIN_FORCE_MIMO_SEL_MSK \
	(0x7 << PHY_RX_CHAIN_FORCE_MIMO_SEL_POS)
#define PHY_RX_CHAIN_CNT_POS		(10)
#define PHY_RX_CHAIN_CNT_MSK \
	(0x3 << PHY_RX_CHAIN_CNT_POS)
#define PHY_RX_CHAIN_MIMO_CNT_POS	(12)
#define PHY_RX_CHAIN_MIMO_CNT_MSK \
	(0x3 << PHY_RX_CHAIN_MIMO_CNT_POS)
#define PHY_RX_CHAIN_MIMO_FORCE_POS	(14)
#define PHY_RX_CHAIN_MIMO_FORCE_MSK \
	(0x1 << PHY_RX_CHAIN_MIMO_FORCE_POS)
#define NUM_PHY_CTX	3
struct iwl_phy_context_cmd_tail {
	__le32 txchain_info;
	__le32 rxchain_info;
	__le32 acquisition_data;
	__le32 dsp_cfg_flags;
} __packed;
struct iwl_phy_context_cmd_v1 {
	__le32 id_and_color;
	__le32 action;
	__le32 apply_time;
	__le32 tx_param_color;
	struct iwl_fw_channel_info ci;
	struct iwl_phy_context_cmd_tail tail;
} __packed;  
struct iwl_phy_context_cmd {
	__le32 id_and_color;
	__le32 action;
	struct iwl_fw_channel_info ci;
	__le32 lmac_id;
	__le32 rxchain_info;  
	__le32 dsp_cfg_flags;
	__le32 reserved;
} __packed;  
#endif  
