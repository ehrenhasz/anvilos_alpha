 
 

#ifndef _RX_DESC_H_
#define _RX_DESC_H_

#include <linux/bitops.h>

enum rx_attention_flags {
	RX_ATTENTION_FLAGS_FIRST_MPDU          = BIT(0),
	RX_ATTENTION_FLAGS_LAST_MPDU           = BIT(1),
	RX_ATTENTION_FLAGS_MCAST_BCAST         = BIT(2),
	RX_ATTENTION_FLAGS_PEER_IDX_INVALID    = BIT(3),
	RX_ATTENTION_FLAGS_PEER_IDX_TIMEOUT    = BIT(4),
	RX_ATTENTION_FLAGS_POWER_MGMT          = BIT(5),
	RX_ATTENTION_FLAGS_NON_QOS             = BIT(6),
	RX_ATTENTION_FLAGS_NULL_DATA           = BIT(7),
	RX_ATTENTION_FLAGS_MGMT_TYPE           = BIT(8),
	RX_ATTENTION_FLAGS_CTRL_TYPE           = BIT(9),
	RX_ATTENTION_FLAGS_MORE_DATA           = BIT(10),
	RX_ATTENTION_FLAGS_EOSP                = BIT(11),
	RX_ATTENTION_FLAGS_U_APSD_TRIGGER      = BIT(12),
	RX_ATTENTION_FLAGS_FRAGMENT            = BIT(13),
	RX_ATTENTION_FLAGS_ORDER               = BIT(14),
	RX_ATTENTION_FLAGS_CLASSIFICATION      = BIT(15),
	RX_ATTENTION_FLAGS_OVERFLOW_ERR        = BIT(16),
	RX_ATTENTION_FLAGS_MSDU_LENGTH_ERR     = BIT(17),
	RX_ATTENTION_FLAGS_TCP_UDP_CHKSUM_FAIL = BIT(18),
	RX_ATTENTION_FLAGS_IP_CHKSUM_FAIL      = BIT(19),
	RX_ATTENTION_FLAGS_SA_IDX_INVALID      = BIT(20),
	RX_ATTENTION_FLAGS_DA_IDX_INVALID      = BIT(21),
	RX_ATTENTION_FLAGS_SA_IDX_TIMEOUT      = BIT(22),
	RX_ATTENTION_FLAGS_DA_IDX_TIMEOUT      = BIT(23),
	RX_ATTENTION_FLAGS_ENCRYPT_REQUIRED    = BIT(24),
	RX_ATTENTION_FLAGS_DIRECTED            = BIT(25),
	RX_ATTENTION_FLAGS_BUFFER_FRAGMENT     = BIT(26),
	RX_ATTENTION_FLAGS_MPDU_LENGTH_ERR     = BIT(27),
	RX_ATTENTION_FLAGS_TKIP_MIC_ERR        = BIT(28),
	RX_ATTENTION_FLAGS_DECRYPT_ERR         = BIT(29),
	RX_ATTENTION_FLAGS_FCS_ERR             = BIT(30),
	RX_ATTENTION_FLAGS_MSDU_DONE           = BIT(31),
};

struct rx_attention {
	__le32 flags;  
} __packed;

 

struct rx_frag_info_common {
	u8 ring0_more_count;
	u8 ring1_more_count;
	u8 ring2_more_count;
	u8 ring3_more_count;
} __packed;

struct rx_frag_info_wcn3990 {
	u8 ring4_more_count;
	u8 ring5_more_count;
	u8 ring6_more_count;
	u8 ring7_more_count;
} __packed;

struct rx_frag_info {
	struct rx_frag_info_common common;
	union {
		struct rx_frag_info_wcn3990 wcn3990;
	} __packed;
} __packed;

struct rx_frag_info_v1 {
	struct rx_frag_info_common common;
} __packed;

 

enum htt_rx_mpdu_encrypt_type {
	HTT_RX_MPDU_ENCRYPT_WEP40            = 0,
	HTT_RX_MPDU_ENCRYPT_WEP104           = 1,
	HTT_RX_MPDU_ENCRYPT_TKIP_WITHOUT_MIC = 2,
	HTT_RX_MPDU_ENCRYPT_WEP128           = 3,
	HTT_RX_MPDU_ENCRYPT_TKIP_WPA         = 4,
	HTT_RX_MPDU_ENCRYPT_WAPI             = 5,
	HTT_RX_MPDU_ENCRYPT_AES_CCM_WPA2     = 6,
	HTT_RX_MPDU_ENCRYPT_NONE             = 7,
	HTT_RX_MPDU_ENCRYPT_AES_CCM256_WPA2  = 8,
	HTT_RX_MPDU_ENCRYPT_AES_GCMP_WPA2    = 9,
	HTT_RX_MPDU_ENCRYPT_AES_GCMP256_WPA2 = 10,
};

#define RX_MPDU_START_INFO0_PEER_IDX_MASK     0x000007ff
#define RX_MPDU_START_INFO0_PEER_IDX_LSB      0
#define RX_MPDU_START_INFO0_SEQ_NUM_MASK      0x0fff0000
#define RX_MPDU_START_INFO0_SEQ_NUM_LSB       16
#define RX_MPDU_START_INFO0_ENCRYPT_TYPE_MASK 0xf0000000
#define RX_MPDU_START_INFO0_ENCRYPT_TYPE_LSB  28
#define RX_MPDU_START_INFO0_FROM_DS           BIT(11)
#define RX_MPDU_START_INFO0_TO_DS             BIT(12)
#define RX_MPDU_START_INFO0_ENCRYPTED         BIT(13)
#define RX_MPDU_START_INFO0_RETRY             BIT(14)
#define RX_MPDU_START_INFO0_TXBF_H_INFO       BIT(15)

#define RX_MPDU_START_INFO1_TID_MASK 0xf0000000
#define RX_MPDU_START_INFO1_TID_LSB  28
#define RX_MPDU_START_INFO1_DIRECTED BIT(16)

struct rx_mpdu_start {
	__le32 info0;
	union {
		struct {
			__le32 pn31_0;
			__le32 info1;  
		} __packed;
		struct {
			u8 pn[6];
		} __packed;
	} __packed;
} __packed;

 

#define RX_MPDU_END_INFO0_RESERVED_0_MASK     0x00001fff
#define RX_MPDU_END_INFO0_RESERVED_0_LSB      0
#define RX_MPDU_END_INFO0_POST_DELIM_CNT_MASK 0x0fff0000
#define RX_MPDU_END_INFO0_POST_DELIM_CNT_LSB  16
#define RX_MPDU_END_INFO0_OVERFLOW_ERR        BIT(13)
#define RX_MPDU_END_INFO0_LAST_MPDU           BIT(14)
#define RX_MPDU_END_INFO0_POST_DELIM_ERR      BIT(15)
#define RX_MPDU_END_INFO0_MPDU_LENGTH_ERR     BIT(28)
#define RX_MPDU_END_INFO0_TKIP_MIC_ERR        BIT(29)
#define RX_MPDU_END_INFO0_DECRYPT_ERR         BIT(30)
#define RX_MPDU_END_INFO0_FCS_ERR             BIT(31)

struct rx_mpdu_end {
	__le32 info0;
} __packed;

 

#define RX_MSDU_START_INFO0_MSDU_LENGTH_MASK    0x00003fff
#define RX_MSDU_START_INFO0_MSDU_LENGTH_LSB     0
#define RX_MSDU_START_INFO0_IP_OFFSET_MASK      0x000fc000
#define RX_MSDU_START_INFO0_IP_OFFSET_LSB       14
#define RX_MSDU_START_INFO0_RING_MASK_MASK      0x00f00000
#define RX_MSDU_START_INFO0_RING_MASK_LSB       20
#define RX_MSDU_START_INFO0_TCP_UDP_OFFSET_MASK 0x7f000000
#define RX_MSDU_START_INFO0_TCP_UDP_OFFSET_LSB  24

#define RX_MSDU_START_INFO1_MSDU_NUMBER_MASK    0x000000ff
#define RX_MSDU_START_INFO1_MSDU_NUMBER_LSB     0
#define RX_MSDU_START_INFO1_DECAP_FORMAT_MASK   0x00000300
#define RX_MSDU_START_INFO1_DECAP_FORMAT_LSB    8
#define RX_MSDU_START_INFO1_SA_IDX_MASK         0x07ff0000
#define RX_MSDU_START_INFO1_SA_IDX_LSB          16
#define RX_MSDU_START_INFO1_IPV4_PROTO          BIT(10)
#define RX_MSDU_START_INFO1_IPV6_PROTO          BIT(11)
#define RX_MSDU_START_INFO1_TCP_PROTO           BIT(12)
#define RX_MSDU_START_INFO1_UDP_PROTO           BIT(13)
#define RX_MSDU_START_INFO1_IP_FRAG             BIT(14)
#define RX_MSDU_START_INFO1_TCP_ONLY_ACK        BIT(15)

#define RX_MSDU_START_INFO2_DA_IDX_MASK         0x000007ff
#define RX_MSDU_START_INFO2_DA_IDX_LSB          0
#define RX_MSDU_START_INFO2_IP_PROTO_FIELD_MASK 0x00ff0000
#define RX_MSDU_START_INFO2_IP_PROTO_FIELD_LSB  16
#define RX_MSDU_START_INFO2_DA_BCAST_MCAST      BIT(11)

 
enum rx_msdu_decap_format {
	RX_MSDU_DECAP_RAW = 0,

	 
	RX_MSDU_DECAP_NATIVE_WIFI = 1,

	 
	RX_MSDU_DECAP_ETHERNET2_DIX = 2,

	 
	RX_MSDU_DECAP_8023_SNAP_LLC = 3
};

struct rx_msdu_start_common {
	__le32 info0;  
	__le32 flow_id_crc;
	__le32 info1;  
} __packed;

struct rx_msdu_start_qca99x0 {
	__le32 info2;  
} __packed;

struct rx_msdu_start_wcn3990 {
	__le32 info2;  
	__le32 info3;  
} __packed;

struct rx_msdu_start {
	struct rx_msdu_start_common common;
	union {
		struct rx_msdu_start_wcn3990 wcn3990;
	} __packed;
} __packed;

struct rx_msdu_start_v1 {
	struct rx_msdu_start_common common;
	union {
		struct rx_msdu_start_qca99x0 qca99x0;
	} __packed;
} __packed;

 

#define RX_MSDU_END_INFO0_REPORTED_MPDU_LENGTH_MASK 0x00003fff
#define RX_MSDU_END_INFO0_REPORTED_MPDU_LENGTH_LSB  0
#define RX_MSDU_END_INFO0_FIRST_MSDU                BIT(14)
#define RX_MSDU_END_INFO0_LAST_MSDU                 BIT(15)
#define RX_MSDU_END_INFO0_MSDU_LIMIT_ERR            BIT(18)
#define RX_MSDU_END_INFO0_PRE_DELIM_ERR             BIT(30)
#define RX_MSDU_END_INFO0_RESERVED_3B               BIT(31)

struct rx_msdu_end_common {
	__le16 ip_hdr_cksum;
	__le16 tcp_hdr_cksum;
	u8 key_id_octet;
	u8 classification_filter;
	u8 wapi_pn[10];
	__le32 info0;
} __packed;

#define RX_MSDU_END_INFO1_TCP_FLAG_MASK     0x000001ff
#define RX_MSDU_END_INFO1_TCP_FLAG_LSB      0
#define RX_MSDU_END_INFO1_L3_HDR_PAD_MASK   0x00001c00
#define RX_MSDU_END_INFO1_L3_HDR_PAD_LSB    10
#define RX_MSDU_END_INFO1_WINDOW_SIZE_MASK  0xffff0000
#define RX_MSDU_END_INFO1_WINDOW_SIZE_LSB   16
#define RX_MSDU_END_INFO1_IRO_ELIGIBLE      BIT(9)

#define RX_MSDU_END_INFO2_DA_OFFSET_MASK    0x0000003f
#define RX_MSDU_END_INFO2_DA_OFFSET_LSB     0
#define RX_MSDU_END_INFO2_SA_OFFSET_MASK    0x00000fc0
#define RX_MSDU_END_INFO2_SA_OFFSET_LSB     6
#define RX_MSDU_END_INFO2_TYPE_OFFSET_MASK  0x0003f000
#define RX_MSDU_END_INFO2_TYPE_OFFSET_LSB   12

struct rx_msdu_end_qca99x0 {
	__le32 ipv6_crc;
	__le32 tcp_seq_no;
	__le32 tcp_ack_no;
	__le32 info1;
	__le32 info2;
} __packed;

struct rx_msdu_end_wcn3990 {
	__le32 ipv6_crc;
	__le32 tcp_seq_no;
	__le32 tcp_ack_no;
	__le32 info1;
	__le32 info2;
	__le32 rule_indication_0;
	__le32 rule_indication_1;
	__le32 rule_indication_2;
	__le32 rule_indication_3;
} __packed;

struct rx_msdu_end {
	struct rx_msdu_end_common common;
	union {
		struct rx_msdu_end_wcn3990 wcn3990;
	} __packed;
} __packed;

struct rx_msdu_end_v1 {
	struct rx_msdu_end_common common;
	union {
		struct rx_msdu_end_qca99x0 qca99x0;
	} __packed;
} __packed;

 

#define HTT_RX_PPDU_START_PREAMBLE_LEGACY        0x04
#define HTT_RX_PPDU_START_PREAMBLE_HT            0x08
#define HTT_RX_PPDU_START_PREAMBLE_HT_WITH_TXBF  0x09
#define HTT_RX_PPDU_START_PREAMBLE_VHT           0x0C
#define HTT_RX_PPDU_START_PREAMBLE_VHT_WITH_TXBF 0x0D

#define RX_PPDU_START_INFO0_IS_GREENFIELD BIT(0)

#define RX_PPDU_START_INFO1_L_SIG_RATE_MASK    0x0000000f
#define RX_PPDU_START_INFO1_L_SIG_RATE_LSB     0
#define RX_PPDU_START_INFO1_L_SIG_LENGTH_MASK  0x0001ffe0
#define RX_PPDU_START_INFO1_L_SIG_LENGTH_LSB   5
#define RX_PPDU_START_INFO1_L_SIG_TAIL_MASK    0x00fc0000
#define RX_PPDU_START_INFO1_L_SIG_TAIL_LSB     18
#define RX_PPDU_START_INFO1_PREAMBLE_TYPE_MASK 0xff000000
#define RX_PPDU_START_INFO1_PREAMBLE_TYPE_LSB  24
#define RX_PPDU_START_INFO1_L_SIG_RATE_SELECT  BIT(4)
#define RX_PPDU_START_INFO1_L_SIG_PARITY       BIT(17)

#define RX_PPDU_START_INFO2_HT_SIG_VHT_SIG_A_1_MASK 0x00ffffff
#define RX_PPDU_START_INFO2_HT_SIG_VHT_SIG_A_1_LSB  0

#define RX_PPDU_START_INFO3_HT_SIG_VHT_SIG_A_2_MASK 0x00ffffff
#define RX_PPDU_START_INFO3_HT_SIG_VHT_SIG_A_2_LSB  0
#define RX_PPDU_START_INFO3_TXBF_H_INFO             BIT(24)

#define RX_PPDU_START_INFO4_VHT_SIG_B_MASK 0x1fffffff
#define RX_PPDU_START_INFO4_VHT_SIG_B_LSB  0

#define RX_PPDU_START_INFO5_SERVICE_MASK 0x0000ffff
#define RX_PPDU_START_INFO5_SERVICE_LSB  0

 
#define RX_PPDU_START_RATE_FLAG BIT(3)

struct rx_ppdu_start {
	struct {
		u8 pri20_mhz;
		u8 ext20_mhz;
		u8 ext40_mhz;
		u8 ext80_mhz;
	} rssi_chains[4];
	u8 rssi_comb;
	__le16 rsvd0;
	u8 info0;  
	__le32 info1;  
	__le32 info2;  
	__le32 info3;  
	__le32 info4;  
	__le32 info5;  
} __packed;

 

#define RX_PPDU_END_FLAGS_PHY_ERR             BIT(0)
#define RX_PPDU_END_FLAGS_RX_LOCATION         BIT(1)
#define RX_PPDU_END_FLAGS_TXBF_H_INFO         BIT(2)

#define RX_PPDU_END_INFO0_RX_ANTENNA_MASK     0x00ffffff
#define RX_PPDU_END_INFO0_RX_ANTENNA_LSB      0
#define RX_PPDU_END_INFO0_FLAGS_TX_HT_VHT_ACK BIT(24)
#define RX_PPDU_END_INFO0_BB_CAPTURED_CHANNEL BIT(25)

#define RX_PPDU_END_INFO1_PEER_IDX_MASK       0x1ffc
#define RX_PPDU_END_INFO1_PEER_IDX_LSB        2
#define RX_PPDU_END_INFO1_BB_DATA             BIT(0)
#define RX_PPDU_END_INFO1_PEER_IDX_VALID      BIT(1)
#define RX_PPDU_END_INFO1_PPDU_DONE           BIT(15)

struct rx_ppdu_end_common {
	__le32 evm_p0;
	__le32 evm_p1;
	__le32 evm_p2;
	__le32 evm_p3;
	__le32 evm_p4;
	__le32 evm_p5;
	__le32 evm_p6;
	__le32 evm_p7;
	__le32 evm_p8;
	__le32 evm_p9;
	__le32 evm_p10;
	__le32 evm_p11;
	__le32 evm_p12;
	__le32 evm_p13;
	__le32 evm_p14;
	__le32 evm_p15;
	__le32 tsf_timestamp;
	__le32 wb_timestamp;
} __packed;

struct rx_ppdu_end_qca988x {
	u8 locationing_timestamp;
	u8 phy_err_code;
	__le16 flags;  
	__le32 info0;  
	__le16 bb_length;
	__le16 info1;  
} __packed;

#define RX_PPDU_END_RTT_CORRELATION_VALUE_MASK 0x00ffffff
#define RX_PPDU_END_RTT_CORRELATION_VALUE_LSB  0
#define RX_PPDU_END_RTT_UNUSED_MASK            0x7f000000
#define RX_PPDU_END_RTT_UNUSED_LSB             24
#define RX_PPDU_END_RTT_NORMAL_MODE            BIT(31)

struct rx_ppdu_end_qca6174 {
	u8 locationing_timestamp;
	u8 phy_err_code;
	__le16 flags;  
	__le32 info0;  
	__le32 rtt;  
	__le16 bb_length;
	__le16 info1;  
} __packed;

#define RX_PKT_END_INFO0_RX_SUCCESS              BIT(0)
#define RX_PKT_END_INFO0_ERR_TX_INTERRUPT_RX     BIT(3)
#define RX_PKT_END_INFO0_ERR_OFDM_POWER_DROP     BIT(4)
#define RX_PKT_END_INFO0_ERR_OFDM_RESTART        BIT(5)
#define RX_PKT_END_INFO0_ERR_CCK_POWER_DROP      BIT(6)
#define RX_PKT_END_INFO0_ERR_CCK_RESTART         BIT(7)

#define RX_LOCATION_INFO_RTT_CORR_VAL_MASK       0x0001ffff
#define RX_LOCATION_INFO_RTT_CORR_VAL_LSB        0
#define RX_LOCATION_INFO_FAC_STATUS_MASK         0x000c0000
#define RX_LOCATION_INFO_FAC_STATUS_LSB          18
#define RX_LOCATION_INFO_PKT_BW_MASK             0x00700000
#define RX_LOCATION_INFO_PKT_BW_LSB              20
#define RX_LOCATION_INFO_RTT_TX_FRAME_PHASE_MASK 0x01800000
#define RX_LOCATION_INFO_RTT_TX_FRAME_PHASE_LSB  23
#define RX_LOCATION_INFO_CIR_STATUS              BIT(17)
#define RX_LOCATION_INFO_RTT_MAC_PHY_PHASE       BIT(25)
#define RX_LOCATION_INFO_RTT_TX_DATA_START_X     BIT(26)
#define RX_LOCATION_INFO_HW_IFFT_MODE            BIT(30)
#define RX_LOCATION_INFO_RX_LOCATION_VALID       BIT(31)

struct rx_pkt_end {
	__le32 info0;  
	__le32 phy_timestamp_1;
	__le32 phy_timestamp_2;
} __packed;

struct rx_pkt_end_wcn3990 {
	__le32 info0;  
	__le64 phy_timestamp_1;
	__le64 phy_timestamp_2;
} __packed;

#define RX_LOCATION_INFO0_RTT_FAC_LEGACY_MASK		0x00003fff
#define RX_LOCATION_INFO0_RTT_FAC_LEGACY_LSB		0
#define RX_LOCATION_INFO0_RTT_FAC_VHT_MASK		0x1fff8000
#define RX_LOCATION_INFO0_RTT_FAC_VHT_LSB		15
#define RX_LOCATION_INFO0_RTT_STRONGEST_CHAIN_MASK	0xc0000000
#define RX_LOCATION_INFO0_RTT_STRONGEST_CHAIN_LSB	30
#define RX_LOCATION_INFO0_RTT_FAC_LEGACY_STATUS		BIT(14)
#define RX_LOCATION_INFO0_RTT_FAC_VHT_STATUS		BIT(29)

#define RX_LOCATION_INFO1_RTT_PREAMBLE_TYPE_MASK	0x0000000c
#define RX_LOCATION_INFO1_RTT_PREAMBLE_TYPE_LSB		2
#define RX_LOCATION_INFO1_PKT_BW_MASK			0x00000030
#define RX_LOCATION_INFO1_PKT_BW_LSB			4
#define RX_LOCATION_INFO1_SKIP_P_SKIP_BTCF_MASK		0x0000ff00
#define RX_LOCATION_INFO1_SKIP_P_SKIP_BTCF_LSB		8
#define RX_LOCATION_INFO1_RTT_MSC_RATE_MASK		0x000f0000
#define RX_LOCATION_INFO1_RTT_MSC_RATE_LSB		16
#define RX_LOCATION_INFO1_RTT_PBD_LEG_BW_MASK		0x00300000
#define RX_LOCATION_INFO1_RTT_PBD_LEG_BW_LSB		20
#define RX_LOCATION_INFO1_TIMING_BACKOFF_MASK		0x07c00000
#define RX_LOCATION_INFO1_TIMING_BACKOFF_LSB		22
#define RX_LOCATION_INFO1_RTT_TX_FRAME_PHASE_MASK	0x18000000
#define RX_LOCATION_INFO1_RTT_TX_FRAME_PHASE_LSB	27
#define RX_LOCATION_INFO1_RTT_CFR_STATUS		BIT(0)
#define RX_LOCATION_INFO1_RTT_CIR_STATUS		BIT(1)
#define RX_LOCATION_INFO1_RTT_GI_TYPE			BIT(7)
#define RX_LOCATION_INFO1_RTT_MAC_PHY_PHASE		BIT(29)
#define RX_LOCATION_INFO1_RTT_TX_DATA_START_X_PHASE	BIT(30)
#define RX_LOCATION_INFO1_RX_LOCATION_VALID		BIT(31)

struct rx_location_info {
	__le32 rx_location_info0;  
	__le32 rx_location_info1;  
} __packed;

struct rx_location_info_wcn3990 {
	__le32 rx_location_info0;  
	__le32 rx_location_info1;  
	__le32 rx_location_info2;  
} __packed;

enum rx_phy_ppdu_end_info0 {
	RX_PHY_PPDU_END_INFO0_ERR_RADAR           = BIT(2),
	RX_PHY_PPDU_END_INFO0_ERR_RX_ABORT        = BIT(3),
	RX_PHY_PPDU_END_INFO0_ERR_RX_NAP          = BIT(4),
	RX_PHY_PPDU_END_INFO0_ERR_OFDM_TIMING     = BIT(5),
	RX_PHY_PPDU_END_INFO0_ERR_OFDM_PARITY     = BIT(6),
	RX_PHY_PPDU_END_INFO0_ERR_OFDM_RATE       = BIT(7),
	RX_PHY_PPDU_END_INFO0_ERR_OFDM_LENGTH     = BIT(8),
	RX_PHY_PPDU_END_INFO0_ERR_OFDM_RESTART    = BIT(9),
	RX_PHY_PPDU_END_INFO0_ERR_OFDM_SERVICE    = BIT(10),
	RX_PHY_PPDU_END_INFO0_ERR_OFDM_POWER_DROP = BIT(11),
	RX_PHY_PPDU_END_INFO0_ERR_CCK_BLOCKER     = BIT(12),
	RX_PHY_PPDU_END_INFO0_ERR_CCK_TIMING      = BIT(13),
	RX_PHY_PPDU_END_INFO0_ERR_CCK_HEADER_CRC  = BIT(14),
	RX_PHY_PPDU_END_INFO0_ERR_CCK_RATE        = BIT(15),
	RX_PHY_PPDU_END_INFO0_ERR_CCK_LENGTH      = BIT(16),
	RX_PHY_PPDU_END_INFO0_ERR_CCK_RESTART     = BIT(17),
	RX_PHY_PPDU_END_INFO0_ERR_CCK_SERVICE     = BIT(18),
	RX_PHY_PPDU_END_INFO0_ERR_CCK_POWER_DROP  = BIT(19),
	RX_PHY_PPDU_END_INFO0_ERR_HT_CRC          = BIT(20),
	RX_PHY_PPDU_END_INFO0_ERR_HT_LENGTH       = BIT(21),
	RX_PHY_PPDU_END_INFO0_ERR_HT_RATE         = BIT(22),
	RX_PHY_PPDU_END_INFO0_ERR_HT_ZLF          = BIT(23),
	RX_PHY_PPDU_END_INFO0_ERR_FALSE_RADAR_EXT = BIT(24),
	RX_PHY_PPDU_END_INFO0_ERR_GREEN_FIELD     = BIT(25),
	RX_PHY_PPDU_END_INFO0_ERR_SPECTRAL_SCAN   = BIT(26),
	RX_PHY_PPDU_END_INFO0_ERR_RX_DYN_BW       = BIT(27),
	RX_PHY_PPDU_END_INFO0_ERR_LEG_HT_MISMATCH = BIT(28),
	RX_PHY_PPDU_END_INFO0_ERR_VHT_CRC         = BIT(29),
	RX_PHY_PPDU_END_INFO0_ERR_VHT_SIGA        = BIT(30),
	RX_PHY_PPDU_END_INFO0_ERR_VHT_LSIG        = BIT(31),
};

enum rx_phy_ppdu_end_info1 {
	RX_PHY_PPDU_END_INFO1_ERR_VHT_NDP            = BIT(0),
	RX_PHY_PPDU_END_INFO1_ERR_VHT_NSYM           = BIT(1),
	RX_PHY_PPDU_END_INFO1_ERR_VHT_RX_EXT_SYM     = BIT(2),
	RX_PHY_PPDU_END_INFO1_ERR_VHT_RX_SKIP_ID0    = BIT(3),
	RX_PHY_PPDU_END_INFO1_ERR_VHT_RX_SKIP_ID1_62 = BIT(4),
	RX_PHY_PPDU_END_INFO1_ERR_VHT_RX_SKIP_ID63   = BIT(5),
	RX_PHY_PPDU_END_INFO1_ERR_OFDM_LDPC_DECODER  = BIT(6),
	RX_PHY_PPDU_END_INFO1_ERR_DEFER_NAP          = BIT(7),
	RX_PHY_PPDU_END_INFO1_ERR_FDOMAIN_TIMEOUT    = BIT(8),
	RX_PHY_PPDU_END_INFO1_ERR_LSIG_REL_CHECK     = BIT(9),
	RX_PHY_PPDU_END_INFO1_ERR_BT_COLLISION       = BIT(10),
	RX_PHY_PPDU_END_INFO1_ERR_MU_FEEDBACK        = BIT(11),
	RX_PHY_PPDU_END_INFO1_ERR_TX_INTERRUPT_RX    = BIT(12),
	RX_PHY_PPDU_END_INFO1_ERR_RX_CBF             = BIT(13),
};

struct rx_phy_ppdu_end {
	__le32 info0;  
	__le32 info1;  
} __packed;

#define RX_PPDU_END_RX_TIMING_OFFSET_MASK          0x00000fff
#define RX_PPDU_END_RX_TIMING_OFFSET_LSB           0

#define RX_PPDU_END_RX_INFO_RX_ANTENNA_MASK        0x00ffffff
#define RX_PPDU_END_RX_INFO_RX_ANTENNA_LSB         0
#define RX_PPDU_END_RX_INFO_TX_HT_VHT_ACK          BIT(24)
#define RX_PPDU_END_RX_INFO_RX_PKT_END_VALID       BIT(25)
#define RX_PPDU_END_RX_INFO_RX_PHY_PPDU_END_VALID  BIT(26)
#define RX_PPDU_END_RX_INFO_RX_TIMING_OFFSET_VALID BIT(27)
#define RX_PPDU_END_RX_INFO_BB_CAPTURED_CHANNEL    BIT(28)
#define RX_PPDU_END_RX_INFO_UNSUPPORTED_MU_NC      BIT(29)
#define RX_PPDU_END_RX_INFO_OTP_TXBF_DISABLE       BIT(30)

struct rx_ppdu_end_qca99x0 {
	struct rx_pkt_end rx_pkt_end;
	__le32 rx_location_info;  
	struct rx_phy_ppdu_end rx_phy_ppdu_end;
	__le32 rx_timing_offset;  
	__le32 rx_info;  
	__le16 bb_length;
	__le16 info1;  
} __packed;

struct rx_ppdu_end_qca9984 {
	struct rx_pkt_end rx_pkt_end;
	struct rx_location_info rx_location_info;
	struct rx_phy_ppdu_end rx_phy_ppdu_end;
	__le32 rx_timing_offset;  
	__le32 rx_info;  
	__le16 bb_length;
	__le16 info1;  
} __packed;

struct rx_ppdu_end_wcn3990 {
	struct rx_pkt_end_wcn3990 rx_pkt_end;
	struct rx_location_info_wcn3990 rx_location_info;
	struct rx_phy_ppdu_end rx_phy_ppdu_end;
	__le32 rx_timing_offset;
	__le32 reserved_info_0;
	__le32 reserved_info_1;
	__le32 rx_antenna_info;
	__le32 rx_coex_info;
	__le32 rx_mpdu_cnt_info;
	__le64 phy_timestamp_tx;
	__le32 rx_bb_length;
} __packed;

struct rx_ppdu_end {
	struct rx_ppdu_end_common common;
	union {
		struct rx_ppdu_end_wcn3990 wcn3990;
	} __packed;
} __packed;

struct rx_ppdu_end_v1 {
	struct rx_ppdu_end_common common;
	union {
		struct rx_ppdu_end_qca988x qca988x;
		struct rx_ppdu_end_qca6174 qca6174;
		struct rx_ppdu_end_qca99x0 qca99x0;
		struct rx_ppdu_end_qca9984 qca9984;
	} __packed;
} __packed;

 

#define FW_RX_DESC_INFO0_DISCARD  BIT(0)
#define FW_RX_DESC_INFO0_FORWARD  BIT(1)
#define FW_RX_DESC_INFO0_INSPECT  BIT(5)
#define FW_RX_DESC_INFO0_EXT_MASK 0xC0
#define FW_RX_DESC_INFO0_EXT_LSB  6

struct fw_rx_desc_base {
	u8 info0;
} __packed;

#define FW_RX_DESC_FLAGS_FIRST_MSDU (1 << 0)
#define FW_RX_DESC_FLAGS_LAST_MSDU  (1 << 1)
#define FW_RX_DESC_C3_FAILED        (1 << 2)
#define FW_RX_DESC_C4_FAILED        (1 << 3)
#define FW_RX_DESC_IPV6             (1 << 4)
#define FW_RX_DESC_TCP              (1 << 5)
#define FW_RX_DESC_UDP              (1 << 6)

struct fw_rx_desc_hl {
	union {
		struct {
		u8 discard:1,
		   forward:1,
		   any_err:1,
		   dup_err:1,
		   reserved:1,
		   inspect:1,
		   extension:2;
		} bits;
		u8 info0;
	} u;

	u8 version;
	u8 len;
	u8 flags;
} __packed;

#endif  
