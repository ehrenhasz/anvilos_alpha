#ifndef __iwl_fw_api_rx_h__
#define __iwl_fw_api_rx_h__
#define IWL_RX_INFO_PHY_CNT 8
#define IWL_RX_INFO_ENERGY_ANT_ABC_IDX 1
#define IWL_RX_INFO_ENERGY_ANT_A_MSK 0x000000ff
#define IWL_RX_INFO_ENERGY_ANT_B_MSK 0x0000ff00
#define IWL_RX_INFO_ENERGY_ANT_A_POS 0
#define IWL_RX_INFO_ENERGY_ANT_B_POS 8
#define IWL_RX_INFO_ENERGY_ANT_C_POS 16
enum iwl_mac_context_info {
	MAC_CONTEXT_INFO_NONE,
	MAC_CONTEXT_INFO_GSCAN,
};
struct iwl_rx_phy_info {
	u8 non_cfg_phy_cnt;
	u8 cfg_phy_cnt;
	u8 stat_id;
	u8 reserved1;
	__le32 system_timestamp;
	__le64 timestamp;
	__le32 beacon_time_stamp;
	__le16 phy_flags;
	__le16 channel;
	__le32 non_cfg_phy[IWL_RX_INFO_PHY_CNT];
	__le32 rate_n_flags;
	__le32 byte_count;
	u8 mac_active_msk;
	u8 mac_context_info;
	__le16 frame_time;
} __packed;
enum iwl_csum_rx_assist_info {
	CSUM_RXA_RESERVED_MASK	= 0x000f,
	CSUM_RXA_MICSIZE_MASK	= 0x00f0,
	CSUM_RXA_HEADERLEN_MASK	= 0x1f00,
	CSUM_RXA_PADD		= BIT(13),
	CSUM_RXA_AMSDU		= BIT(14),
	CSUM_RXA_ENA		= BIT(15)
};
struct iwl_rx_mpdu_res_start {
	__le16 byte_count;
	__le16 assist;
} __packed;  
enum iwl_rx_phy_flags {
	RX_RES_PHY_FLAGS_BAND_24	= BIT(0),
	RX_RES_PHY_FLAGS_MOD_CCK	= BIT(1),
	RX_RES_PHY_FLAGS_SHORT_PREAMBLE	= BIT(2),
	RX_RES_PHY_FLAGS_NARROW_BAND	= BIT(3),
	RX_RES_PHY_FLAGS_ANTENNA	= (0x7 << 4),
	RX_RES_PHY_FLAGS_ANTENNA_POS	= 4,
	RX_RES_PHY_FLAGS_AGG		= BIT(7),
	RX_RES_PHY_FLAGS_OFDM_HT	= BIT(8),
	RX_RES_PHY_FLAGS_OFDM_GF	= BIT(9),
	RX_RES_PHY_FLAGS_OFDM_VHT	= BIT(10),
};
enum iwl_mvm_rx_status {
	RX_MPDU_RES_STATUS_CRC_OK			= BIT(0),
	RX_MPDU_RES_STATUS_OVERRUN_OK			= BIT(1),
	RX_MPDU_RES_STATUS_SRC_STA_FOUND		= BIT(2),
	RX_MPDU_RES_STATUS_KEY_VALID			= BIT(3),
	RX_MPDU_RES_STATUS_ICV_OK			= BIT(5),
	RX_MPDU_RES_STATUS_MIC_OK			= BIT(6),
	RX_MPDU_RES_STATUS_TTAK_OK			= BIT(7),
	RX_MPDU_RES_STATUS_MNG_FRAME_REPLAY_ERR		= BIT(7),
	RX_MPDU_RES_STATUS_SEC_NO_ENC			= (0 << 8),
	RX_MPDU_RES_STATUS_SEC_WEP_ENC			= (1 << 8),
	RX_MPDU_RES_STATUS_SEC_CCM_ENC			= (2 << 8),
	RX_MPDU_RES_STATUS_SEC_TKIP_ENC			= (3 << 8),
	RX_MPDU_RES_STATUS_SEC_EXT_ENC			= (4 << 8),
	RX_MPDU_RES_STATUS_SEC_CMAC_GMAC_ENC		= (6 << 8),
	RX_MPDU_RES_STATUS_SEC_ENC_ERR			= (7 << 8),
	RX_MPDU_RES_STATUS_SEC_ENC_MSK			= (7 << 8),
	RX_MPDU_RES_STATUS_DEC_DONE			= BIT(11),
	RX_MPDU_RES_STATUS_CSUM_DONE			= BIT(16),
	RX_MPDU_RES_STATUS_CSUM_OK			= BIT(17),
	RX_MDPU_RES_STATUS_STA_ID_SHIFT			= 24,
	RX_MPDU_RES_STATUS_STA_ID_MSK			= 0x1f << RX_MDPU_RES_STATUS_STA_ID_SHIFT,
};
enum iwl_rx_mpdu_mac_flags1 {
	IWL_RX_MDPU_MFLG1_ADDRTYPE_MASK		= 0x03,
	IWL_RX_MPDU_MFLG1_MIC_CRC_LEN_MASK	= 0xf0,
	IWL_RX_MPDU_MFLG1_MIC_CRC_LEN_SHIFT	= 3,
};
enum iwl_rx_mpdu_mac_flags2 {
	IWL_RX_MPDU_MFLG2_HDR_LEN_MASK		= 0x1f,
	IWL_RX_MPDU_MFLG2_PAD			= 0x20,
	IWL_RX_MPDU_MFLG2_AMSDU			= 0x40,
};
enum iwl_rx_mpdu_amsdu_info {
	IWL_RX_MPDU_AMSDU_SUBFRAME_IDX_MASK	= 0x7f,
	IWL_RX_MPDU_AMSDU_LAST_SUBFRAME		= 0x80,
};
#define RX_MPDU_BAND_POS 6
#define RX_MPDU_BAND_MASK 0xC0
#define BAND_IN_RX_STATUS(_val) \
	(((_val) & RX_MPDU_BAND_MASK) >> RX_MPDU_BAND_POS)
enum iwl_rx_l3_proto_values {
	IWL_RX_L3_TYPE_NONE,
	IWL_RX_L3_TYPE_IPV4,
	IWL_RX_L3_TYPE_IPV4_FRAG,
	IWL_RX_L3_TYPE_IPV6_FRAG,
	IWL_RX_L3_TYPE_IPV6,
	IWL_RX_L3_TYPE_IPV6_IN_IPV4,
	IWL_RX_L3_TYPE_ARP,
	IWL_RX_L3_TYPE_EAPOL,
};
#define IWL_RX_L3_PROTO_POS 4
enum iwl_rx_l3l4_flags {
	IWL_RX_L3L4_IP_HDR_CSUM_OK		= BIT(0),
	IWL_RX_L3L4_TCP_UDP_CSUM_OK		= BIT(1),
	IWL_RX_L3L4_TCP_FIN_SYN_RST_PSH		= BIT(2),
	IWL_RX_L3L4_TCP_ACK			= BIT(3),
	IWL_RX_L3L4_L3_PROTO_MASK		= 0xf << IWL_RX_L3_PROTO_POS,
	IWL_RX_L3L4_L4_PROTO_MASK		= 0xf << 8,
	IWL_RX_L3L4_RSS_HASH_MASK		= 0xf << 12,
};
enum iwl_rx_mpdu_status {
	IWL_RX_MPDU_STATUS_CRC_OK		= BIT(0),
	IWL_RX_MPDU_STATUS_OVERRUN_OK		= BIT(1),
	IWL_RX_MPDU_STATUS_SRC_STA_FOUND	= BIT(2),
	IWL_RX_MPDU_STATUS_KEY_VALID		= BIT(3),
	IWL_RX_MPDU_STATUS_ICV_OK		= BIT(5),
	IWL_RX_MPDU_STATUS_MIC_OK		= BIT(6),
	IWL_RX_MPDU_RES_STATUS_TTAK_OK		= BIT(7),
	IWL_RX_MPDU_STATUS_REPLAY_ERROR		= BIT(7),
	IWL_RX_MPDU_STATUS_SEC_MASK		= 0x7 << 8,
	IWL_RX_MPDU_STATUS_SEC_UNKNOWN		= IWL_RX_MPDU_STATUS_SEC_MASK,
	IWL_RX_MPDU_STATUS_SEC_NONE		= 0x0 << 8,
	IWL_RX_MPDU_STATUS_SEC_WEP		= 0x1 << 8,
	IWL_RX_MPDU_STATUS_SEC_CCM		= 0x2 << 8,
	IWL_RX_MPDU_STATUS_SEC_TKIP		= 0x3 << 8,
	IWL_RX_MPDU_STATUS_SEC_EXT_ENC		= 0x4 << 8,
	IWL_RX_MPDU_STATUS_SEC_GCM		= 0x5 << 8,
	IWL_RX_MPDU_STATUS_DECRYPTED		= BIT(11),
	IWL_RX_MPDU_STATUS_ROBUST_MNG_FRAME	= BIT(15),
	IWL_RX_MPDU_STATUS_DUPLICATE		= BIT(22),
	IWL_RX_MPDU_STATUS_STA_ID		= 0x1f000000,
};
#define IWL_RX_REORDER_DATA_INVALID_BAID 0x7f
enum iwl_rx_mpdu_reorder_data {
	IWL_RX_MPDU_REORDER_NSSN_MASK		= 0x00000fff,
	IWL_RX_MPDU_REORDER_SN_MASK		= 0x00fff000,
	IWL_RX_MPDU_REORDER_SN_SHIFT		= 12,
	IWL_RX_MPDU_REORDER_BAID_MASK		= 0x7f000000,
	IWL_RX_MPDU_REORDER_BAID_SHIFT		= 24,
	IWL_RX_MPDU_REORDER_BA_OLD_SN		= 0x80000000,
};
enum iwl_rx_mpdu_phy_info {
	IWL_RX_MPDU_PHY_AMPDU		= BIT(5),
	IWL_RX_MPDU_PHY_AMPDU_TOGGLE	= BIT(6),
	IWL_RX_MPDU_PHY_SHORT_PREAMBLE	= BIT(7),
	IWL_RX_MPDU_PHY_NCCK_ADDTL_NTFY	= BIT(7),
	IWL_RX_MPDU_PHY_TSF_OVERLOAD	= BIT(8),
};
enum iwl_rx_mpdu_mac_info {
	IWL_RX_MPDU_PHY_MAC_INDEX_MASK		= 0x0f,
	IWL_RX_MPDU_PHY_PHY_INDEX_MASK		= 0xf0,
};
enum iwl_rx_phy_he_data0 {
	IWL_RX_PHY_DATA0_HE_BEAM_CHNG				= 0x00000001,
	IWL_RX_PHY_DATA0_HE_UPLINK				= 0x00000002,
	IWL_RX_PHY_DATA0_HE_BSS_COLOR_MASK			= 0x000000fc,
	IWL_RX_PHY_DATA0_HE_SPATIAL_REUSE_MASK			= 0x00000f00,
	IWL_RX_PHY_DATA0_HE_TXOP_DUR_MASK			= 0x000fe000,
	IWL_RX_PHY_DATA0_HE_LDPC_EXT_SYM			= 0x00100000,
	IWL_RX_PHY_DATA0_HE_PRE_FEC_PAD_MASK			= 0x00600000,
	IWL_RX_PHY_DATA0_HE_PE_DISAMBIG				= 0x00800000,
	IWL_RX_PHY_DATA0_HE_DOPPLER				= 0x01000000,
	IWL_RX_PHY_DATA0_HE_DELIM_EOF				= 0x80000000,
};
enum iwl_rx_phy_eht_data0 {
	IWL_RX_PHY_DATA0_EHT_VALIDATE				= BIT(0),
	IWL_RX_PHY_DATA0_EHT_UPLINK				= BIT(1),
	IWL_RX_PHY_DATA0_EHT_BSS_COLOR_MASK			= 0x000000fc,
	IWL_RX_PHY_DATA0_ETH_SPATIAL_REUSE_MASK			= 0x00000f00,
	IWL_RX_PHY_DATA0_EHT_PS160				= BIT(12),
	IWL_RX_PHY_DATA0_EHT_TXOP_DUR_MASK			= 0x000fe000,
	IWL_RX_PHY_DATA0_EHT_LDPC_EXT_SYM			= BIT(20),
	IWL_RX_PHY_DATA0_EHT_PRE_FEC_PAD_MASK			= 0x00600000,
	IWL_RX_PHY_DATA0_EHT_PE_DISAMBIG			= BIT(23),
	IWL_RX_PHY_DATA0_EHT_BW320_SLOT				= BIT(24),
	IWL_RX_PHY_DATA0_EHT_SIGA_CRC_OK			= BIT(25),
	IWL_RX_PHY_DATA0_EHT_PHY_VER				= 0x1c000000,
	IWL_RX_PHY_DATA0_EHT_DELIM_EOF				= BIT(31),
};
enum iwl_rx_phy_info_type {
	IWL_RX_PHY_INFO_TYPE_NONE				= 0,
	IWL_RX_PHY_INFO_TYPE_CCK				= 1,
	IWL_RX_PHY_INFO_TYPE_OFDM_LGCY				= 2,
	IWL_RX_PHY_INFO_TYPE_HT					= 3,
	IWL_RX_PHY_INFO_TYPE_VHT_SU				= 4,
	IWL_RX_PHY_INFO_TYPE_VHT_MU				= 5,
	IWL_RX_PHY_INFO_TYPE_HE_SU				= 6,
	IWL_RX_PHY_INFO_TYPE_HE_MU				= 7,
	IWL_RX_PHY_INFO_TYPE_HE_TB				= 8,
	IWL_RX_PHY_INFO_TYPE_HE_MU_EXT				= 9,
	IWL_RX_PHY_INFO_TYPE_HE_TB_EXT				= 10,
	IWL_RX_PHY_INFO_TYPE_EHT_MU				= 11,
	IWL_RX_PHY_INFO_TYPE_EHT_TB				= 12,
	IWL_RX_PHY_INFO_TYPE_EHT_MU_EXT				= 13,
	IWL_RX_PHY_INFO_TYPE_EHT_TB_EXT				= 14,
};
enum iwl_rx_phy_common_data1 {
	IWL_RX_PHY_DATA1_INFO_TYPE_MASK				= 0xf0000000,
	IWL_RX_PHY_DATA1_LSIG_LEN_MASK				= 0x0fff0000,
};
enum iwl_rx_phy_he_data1 {
	IWL_RX_PHY_DATA1_HE_MU_SIGB_COMPRESSION			= 0x00000001,
	IWL_RX_PHY_DATA1_HE_MU_SIBG_SYM_OR_USER_NUM_MASK	= 0x0000001e,
	IWL_RX_PHY_DATA1_HE_LTF_NUM_MASK			= 0x000000e0,
	IWL_RX_PHY_DATA1_HE_RU_ALLOC_SEC80			= 0x00000100,
	IWL_RX_PHY_DATA1_HE_RU_ALLOC_MASK			= 0x0000fe00,
	IWL_RX_PHY_DATA1_HE_TB_PILOT_TYPE			= 0x00000001,
	IWL_RX_PHY_DATA1_HE_TB_LOW_SS_MASK			= 0x0000000e,
};
enum iwl_rx_phy_eht_data1 {
	IWL_RX_PHY_DATA1_EHT_MU_NUM_SIG_SYM_USIGA2	= 0x0000001f,
	IWL_RX_PHY_DATA1_EHT_TB_PILOT_TYPE		= BIT(0),
	IWL_RX_PHY_DATA1_EHT_TB_LOW_SS			= 0x0000001e,
	IWL_RX_PHY_DATA1_EHT_SIG_LTF_NUM		= 0x000000e0,
	IWL_RX_PHY_DATA1_EHT_RU_ALLOC_B0		= 0x00000100,
	IWL_RX_PHY_DATA1_EHT_RU_ALLOC_B1_B7		= 0x0000fe00,
};
enum iwl_rx_phy_he_data2 {
	IWL_RX_PHY_DATA2_HE_MU_EXT_CH1_RU0		= 0x000000ff,  
	IWL_RX_PHY_DATA2_HE_MU_EXT_CH1_RU2		= 0x0000ff00,  
	IWL_RX_PHY_DATA2_HE_MU_EXT_CH2_RU0		= 0x00ff0000,  
	IWL_RX_PHY_DATA2_HE_MU_EXT_CH2_RU2		= 0xff000000,  
	IWL_RX_PHY_DATA2_HE_TB_EXT_SPTL_REUSE1		= 0x0000000f,
	IWL_RX_PHY_DATA2_HE_TB_EXT_SPTL_REUSE2		= 0x000000f0,
	IWL_RX_PHY_DATA2_HE_TB_EXT_SPTL_REUSE3		= 0x00000f00,
	IWL_RX_PHY_DATA2_HE_TB_EXT_SPTL_REUSE4		= 0x0000f000,
};
enum iwl_rx_phy_he_data3 {
	IWL_RX_PHY_DATA3_HE_MU_EXT_CH1_RU1		= 0x000000ff,  
	IWL_RX_PHY_DATA3_HE_MU_EXT_CH1_RU3		= 0x0000ff00,  
	IWL_RX_PHY_DATA3_HE_MU_EXT_CH2_RU1		= 0x00ff0000,  
	IWL_RX_PHY_DATA3_HE_MU_EXT_CH2_RU3		= 0xff000000,  
};
enum iwl_rx_phy_he_he_data4 {
	IWL_RX_PHY_DATA4_HE_MU_EXT_CH1_CTR_RU			= 0x0001,
	IWL_RX_PHY_DATA4_HE_MU_EXT_CH2_CTR_RU			= 0x0002,
	IWL_RX_PHY_DATA4_HE_MU_EXT_CH1_CRC_OK			= 0x0004,
	IWL_RX_PHY_DATA4_HE_MU_EXT_CH2_CRC_OK			= 0x0008,
	IWL_RX_PHY_DATA4_HE_MU_EXT_SIGB_MCS_MASK		= 0x00f0,
	IWL_RX_PHY_DATA4_HE_MU_EXT_SIGB_DCM			= 0x0100,
	IWL_RX_PHY_DATA4_HE_MU_EXT_PREAMBLE_PUNC_TYPE_MASK	= 0x0600,
};
enum iwl_rx_phy_eht_data2 {
	IWL_RX_PHY_DATA2_EHT_MU_EXT_RU_ALLOC_A1	= 0x000001ff,
	IWL_RX_PHY_DATA2_EHT_MU_EXT_RU_ALLOC_A2	= 0x0003fe00,
	IWL_RX_PHY_DATA2_EHT_MU_EXT_RU_ALLOC_B1	= 0x07fc0000,
	IWL_RX_PHY_DATA2_EHT_TB_EXT_TRIG_SIGA1	= 0xffffffff,
};
enum iwl_rx_phy_eht_data3 {
	IWL_RX_PHY_DATA3_EHT_MU_EXT_RU_ALLOC_B2	= 0x000001ff,
	IWL_RX_PHY_DATA3_EHT_MU_EXT_RU_ALLOC_C1	= 0x0003fe00,
	IWL_RX_PHY_DATA3_EHT_MU_EXT_RU_ALLOC_C2	= 0x07fc0000,
};
enum iwl_rx_phy_eht_data4 {
	IWL_RX_PHY_DATA4_EHT_MU_EXT_RU_ALLOC_D1	= 0x000001ff,
	IWL_RX_PHY_DATA4_EHT_MU_EXT_RU_ALLOC_D2	= 0x0003fe00,
	IWL_RX_PHY_DATA4_EHT_MU_EXT_SIGB_MCS	= 0x000c0000,
};
enum iwl_rx_phy_data5 {
	IWL_RX_PHY_DATA5_EHT_TYPE_AND_COMP		= 0x00000003,
	IWL_RX_PHY_DATA5_EHT_TB_SPATIAL_REUSE1		= 0x0000003c,
	IWL_RX_PHY_DATA5_EHT_TB_SPATIAL_REUSE2		= 0x000003c0,
	IWL_RX_PHY_DATA5_EHT_MU_PUNC_CH_CODE		= 0x0000007c,
	IWL_RX_PHY_DATA5_EHT_MU_STA_ID_USR		= 0x0003ff80,
	IWL_RX_PHY_DATA5_EHT_MU_NUM_USR_NON_OFDMA	= 0x001c0000,
	IWL_RX_PHY_DATA5_EHT_MU_SPATIAL_CONF_USR_FIELD	= 0x0fe00000,
};
struct iwl_rx_mpdu_desc_v1 {
	union {
		__le32 rss_hash;
		__le32 phy_data2;
	};
	union {
		__le32 filter_match;
		__le32 phy_data3;
	};
	__le32 rate_n_flags;
	u8 energy_a;
	u8 energy_b;
	u8 channel;
	u8 mac_context;
	__le32 gp2_on_air_rise;
	union {
		__le64 tsf_on_air_rise;
		struct {
			__le32 phy_data0;
			__le32 phy_data1;
		};
	};
} __packed;  
struct iwl_rx_mpdu_desc_v3 {
	union {
		__le32 filter_match;
		__le32 phy_data3;
	};
	union {
		__le32 rss_hash;
		__le32 phy_data2;
	};
	__le32 partial_hash;
	__be16 raw_xsum;
	__le16 reserved_xsum;
	__le32 rate_n_flags;
	u8 energy_a;
	u8 energy_b;
	u8 channel;
	u8 mac_context;
	__le32 gp2_on_air_rise;
	union {
		__le64 tsf_on_air_rise;
		struct {
			__le32 phy_data0;
			__le32 phy_data1;
		};
	};
	__le32 phy_data5;
	__le32 reserved[1];
} __packed;  
struct iwl_rx_mpdu_desc {
	__le16 mpdu_len;
	u8 mac_flags1;
	u8 mac_flags2;
	u8 amsdu_info;
	__le16 phy_info;
	u8 mac_phy_idx;
	union {
		struct {
			__le16 raw_csum;
			union {
				__le16 l3l4_flags;
				__le16 phy_data4;
			};
		};
		__le32 phy_eht_data4;
	};
	__le32 status;
	__le32 reorder_data;
	union {
		struct iwl_rx_mpdu_desc_v1 v1;
		struct iwl_rx_mpdu_desc_v3 v3;
	};
} __packed;  
#define IWL_RX_DESC_SIZE_V1 offsetofend(struct iwl_rx_mpdu_desc, v1)
#define RX_NO_DATA_CHAIN_A_POS		0
#define RX_NO_DATA_CHAIN_A_MSK		(0xff << RX_NO_DATA_CHAIN_A_POS)
#define RX_NO_DATA_CHAIN_B_POS		8
#define RX_NO_DATA_CHAIN_B_MSK		(0xff << RX_NO_DATA_CHAIN_B_POS)
#define RX_NO_DATA_CHANNEL_POS		16
#define RX_NO_DATA_CHANNEL_MSK		(0xff << RX_NO_DATA_CHANNEL_POS)
#define RX_NO_DATA_INFO_TYPE_POS	0
#define RX_NO_DATA_INFO_TYPE_MSK	(0xff << RX_NO_DATA_INFO_TYPE_POS)
#define RX_NO_DATA_INFO_TYPE_NONE	0
#define RX_NO_DATA_INFO_TYPE_RX_ERR	1
#define RX_NO_DATA_INFO_TYPE_NDP	2
#define RX_NO_DATA_INFO_TYPE_MU_UNMATCHED	3
#define RX_NO_DATA_INFO_TYPE_TB_UNMATCHED	4
#define RX_NO_DATA_INFO_ERR_POS		8
#define RX_NO_DATA_INFO_ERR_MSK		(0xff << RX_NO_DATA_INFO_ERR_POS)
#define RX_NO_DATA_INFO_ERR_NONE	0
#define RX_NO_DATA_INFO_ERR_BAD_PLCP	1
#define RX_NO_DATA_INFO_ERR_UNSUPPORTED_RATE	2
#define RX_NO_DATA_INFO_ERR_NO_DELIM		3
#define RX_NO_DATA_INFO_ERR_BAD_MAC_HDR	4
#define RX_NO_DATA_INFO_LOW_ENERGY		5
#define RX_NO_DATA_FRAME_TIME_POS	0
#define RX_NO_DATA_FRAME_TIME_MSK	(0xfffff << RX_NO_DATA_FRAME_TIME_POS)
#define RX_NO_DATA_RX_VEC0_HE_NSTS_MSK	0x03800000
#define RX_NO_DATA_RX_VEC0_VHT_NSTS_MSK	0x38000000
#define RX_NO_DATA_RX_VEC2_EHT_NSTS_MSK	0x00f00000
enum iwl_rx_usig_a1 {
	IWL_RX_USIG_A1_ENHANCED_WIFI_VER_ID	= 0x00000007,
	IWL_RX_USIG_A1_BANDWIDTH		= 0x00000038,
	IWL_RX_USIG_A1_UL_FLAG			= 0x00000040,
	IWL_RX_USIG_A1_BSS_COLOR		= 0x00001f80,
	IWL_RX_USIG_A1_TXOP_DURATION		= 0x000fe000,
	IWL_RX_USIG_A1_DISREGARD		= 0x01f00000,
	IWL_RX_USIG_A1_VALIDATE			= 0x02000000,
	IWL_RX_USIG_A1_EHT_BW320_SLOT		= 0x04000000,
	IWL_RX_USIG_A1_EHT_TYPE			= 0x18000000,
	IWL_RX_USIG_A1_RDY			= 0x80000000,
};
enum iwl_rx_usig_a2_eht {
	IWL_RX_USIG_A2_EHT_PPDU_TYPE		= 0x00000003,
	IWL_RX_USIG_A2_EHT_USIG2_VALIDATE_B2	= 0x00000004,
	IWL_RX_USIG_A2_EHT_PUNC_CHANNEL		= 0x000000f8,
	IWL_RX_USIG_A2_EHT_USIG2_VALIDATE_B8	= 0x00000100,
	IWL_RX_USIG_A2_EHT_SIG_MCS		= 0x00000600,
	IWL_RX_USIG_A2_EHT_SIG_SYM_NUM		= 0x0000f800,
	IWL_RX_USIG_A2_EHT_TRIG_SPATIAL_REUSE_1 = 0x000f0000,
	IWL_RX_USIG_A2_EHT_TRIG_SPATIAL_REUSE_2 = 0x00f00000,
	IWL_RX_USIG_A2_EHT_TRIG_USIG2_DISREGARD	= 0x1f000000,
	IWL_RX_USIG_A2_EHT_CRC_OK		= 0x40000000,
	IWL_RX_USIG_A2_EHT_RDY			= 0x80000000,
};
struct iwl_rx_no_data {
	__le32 info;
	__le32 rssi;
	__le32 on_air_rise_time;
	__le32 fr_time;
	__le32 rate;
	__le32 phy_info[2];
	__le32 rx_vec[2];
} __packed;  
struct iwl_rx_no_data_ver_3 {
	__le32 info;
	__le32 rssi;
	__le32 on_air_rise_time;
	__le32 fr_time;
	__le32 rate;
	__le32 phy_info[2];
	__le32 rx_vec[4];
} __packed;  
struct iwl_frame_release {
	u8 baid;
	u8 reserved;
	__le16 nssn;
};
enum iwl_bar_frame_release_sta_tid {
	IWL_BAR_FRAME_RELEASE_TID_MASK = 0x0000000f,
	IWL_BAR_FRAME_RELEASE_STA_MASK = 0x000001f0,
};
enum iwl_bar_frame_release_ba_info {
	IWL_BAR_FRAME_RELEASE_NSSN_MASK	= 0x00000fff,
	IWL_BAR_FRAME_RELEASE_SN_MASK	= 0x00fff000,
	IWL_BAR_FRAME_RELEASE_BAID_MASK	= 0x3f000000,
};
struct iwl_bar_frame_release {
	__le32 sta_tid;
	__le32 ba_info;
} __packed;  
enum iwl_rss_hash_func_en {
	IWL_RSS_HASH_TYPE_IPV4_TCP,
	IWL_RSS_HASH_TYPE_IPV4_UDP,
	IWL_RSS_HASH_TYPE_IPV4_PAYLOAD,
	IWL_RSS_HASH_TYPE_IPV6_TCP,
	IWL_RSS_HASH_TYPE_IPV6_UDP,
	IWL_RSS_HASH_TYPE_IPV6_PAYLOAD,
};
#define IWL_RSS_HASH_KEY_CNT 10
#define IWL_RSS_INDIRECTION_TABLE_SIZE 128
#define IWL_RSS_ENABLE 1
struct iwl_rss_config_cmd {
	__le32 flags;
	u8 hash_mask;
	u8 reserved[3];
	__le32 secret_key[IWL_RSS_HASH_KEY_CNT];
	u8 indirection_table[IWL_RSS_INDIRECTION_TABLE_SIZE];
} __packed;  
#define IWL_MULTI_QUEUE_SYNC_SENDER_POS 0
#define IWL_MULTI_QUEUE_SYNC_SENDER_MSK 0xf
struct iwl_rxq_sync_cmd {
	__le32 flags;
	__le32 rxq_mask;
	__le32 count;
	u8 payload[];
} __packed;  
struct iwl_rxq_sync_notification {
	__le32 count;
	u8 payload[];
} __packed;  
enum iwl_mvm_pm_event {
	IWL_MVM_PM_EVENT_AWAKE,
	IWL_MVM_PM_EVENT_ASLEEP,
	IWL_MVM_PM_EVENT_UAPSD,
	IWL_MVM_PM_EVENT_PS_POLL,
};  
struct iwl_mvm_pm_state_notification {
	u8 sta_id;
	u8 type;
	__le16 reserved;
} __packed;  
#define BA_WINDOW_STREAMS_MAX		16
#define BA_WINDOW_STATUS_TID_MSK	0x000F
#define BA_WINDOW_STATUS_STA_ID_POS	4
#define BA_WINDOW_STATUS_STA_ID_MSK	0x01F0
#define BA_WINDOW_STATUS_VALID_MSK	BIT(9)
struct iwl_ba_window_status_notif {
	__le64 bitmap[BA_WINDOW_STREAMS_MAX];
	__le16 ra_tid[BA_WINDOW_STREAMS_MAX];
	__le32 start_seq_num[BA_WINDOW_STREAMS_MAX];
	__le16 mpdu_rx_count[BA_WINDOW_STREAMS_MAX];
} __packed;  
struct iwl_rfh_queue_data {
	u8 q_num;
	u8 enable;
	__le16 reserved;
	__le64 urbd_stts_wrptr;
	__le64 fr_bd_cb;
	__le64 ur_bd_cb;
	__le32 fr_bd_wid;
} __packed;  
struct iwl_rfh_queue_config {
	u8 num_queues;
	u8 reserved[3];
	struct iwl_rfh_queue_data data[];
} __packed;  
#endif  
