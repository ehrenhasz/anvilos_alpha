 
 

#ifndef CW1200_WSM_H_INCLUDED
#define CW1200_WSM_H_INCLUDED

#include <linux/spinlock.h>

struct cw1200_common;

 
 
#define WSM_PHY_BAND_2_4G		(0)

 
#define WSM_PHY_BAND_5G			(1)

 
 
#define WSM_TRANSMIT_RATE_1		(0)

 
#define WSM_TRANSMIT_RATE_2		(1)

 
#define WSM_TRANSMIT_RATE_5		(2)

 
#define WSM_TRANSMIT_RATE_11		(3)

 
 

 
 

 
#define WSM_TRANSMIT_RATE_6		(6)

 
#define WSM_TRANSMIT_RATE_9		(7)

 
#define WSM_TRANSMIT_RATE_12		(8)

 
#define WSM_TRANSMIT_RATE_18		(9)

 
#define WSM_TRANSMIT_RATE_24		(10)

 
#define WSM_TRANSMIT_RATE_36		(11)

 
#define WSM_TRANSMIT_RATE_48		(12)

 
#define WSM_TRANSMIT_RATE_54		(13)

 
#define WSM_TRANSMIT_RATE_HT_6		(14)

 
#define WSM_TRANSMIT_RATE_HT_13		(15)

 
#define WSM_TRANSMIT_RATE_HT_19		(16)

 
#define WSM_TRANSMIT_RATE_HT_26		(17)

 
#define WSM_TRANSMIT_RATE_HT_39		(18)

 
#define WSM_TRANSMIT_RATE_HT_52		(19)

 
#define WSM_TRANSMIT_RATE_HT_58		(20)

 
#define WSM_TRANSMIT_RATE_HT_65		(21)

 
 
#define WSM_SCAN_TYPE_FOREGROUND	(0)

 
#define WSM_SCAN_TYPE_BACKGROUND	(1)

 
#define WSM_SCAN_TYPE_AUTO		(2)

 
 
 
 
 
#define WSM_SCAN_FLAG_FORCE_BACKGROUND	(BIT(0))

 
 
#define WSM_SCAN_FLAG_SPLIT_METHOD	(BIT(1))

 
#define WSM_SCAN_FLAG_SHORT_PREAMBLE	(BIT(2))

 
#define WSM_SCAN_FLAG_11N_GREENFIELD	(BIT(3))

 
 
#define WSM_SCAN_MAX_NUM_OF_CHANNELS	(48)

 
#define WSM_SCAN_MAX_NUM_OF_SSIDS	(2)

 
 
#define WSM_PSM_ACTIVE			(0)

 
#define WSM_PSM_PS			BIT(0)

 
#define WSM_PSM_FAST_PS_FLAG		BIT(7)

 
#define WSM_PSM_FAST_PS			(BIT(0) | BIT(7))

 
 
 
 
#define WSM_PSM_UNKNOWN			BIT(1)

 
 
#define WSM_QUEUE_BEST_EFFORT		(0)

 
#define WSM_QUEUE_BACKGROUND		(1)

 
#define WSM_QUEUE_VIDEO			(2)

 
#define WSM_QUEUE_VOICE			(3)

 
 
#define WSM_HT_TX_NON_HT		(0)

 
#define WSM_HT_TX_MIXED			(1)

 
#define WSM_HT_TX_GREENFIELD		(2)

 
#define WSM_HT_TX_STBC			(BIT(7))

 
 
#define WSM_EPTA_PRIORITY_DEFAULT	4
 
#define WSM_EPTA_PRIORITY_DATA		4
 
#define WSM_EPTA_PRIORITY_MGT		5
 
#define WSM_EPTA_PRIORITY_ACTION	5
 
#define WSM_EPTA_PRIORITY_VIDEO		5
 
#define WSM_EPTA_PRIORITY_VOICE		6
 
#define WSM_EPTA_PRIORITY_EAPOL		7

 
 
 
#define WSM_TX_STATUS_AGGREGATION	(BIT(0))

 
 
#define WSM_TX_STATUS_REQUEUE		(BIT(1))

 
#define WSM_TX_STATUS_NORMAL_ACK	(0<<2)

 
#define WSM_TX_STATUS_NO_ACK		(1<<2)

 
#define WSM_TX_STATUS_NO_EXPLICIT_ACK	(2<<2)

 
 
#define WSM_TX_STATUS_BLOCK_ACK		(3<<2)

 
 
#define WSM_RX_STATUS_UNENCRYPTED	(0<<0)

 
#define WSM_RX_STATUS_WEP		(1<<0)

 
#define WSM_RX_STATUS_TKIP		(2<<0)

 
#define WSM_RX_STATUS_AES		(3<<0)

 
#define WSM_RX_STATUS_WAPI		(4<<0)

 
#define WSM_RX_STATUS_ENCRYPTION(status) ((status) & 0x07)

 
#define WSM_RX_STATUS_AGGREGATE		(BIT(3))

 
#define WSM_RX_STATUS_AGGREGATE_FIRST	(BIT(4))

 
#define WSM_RX_STATUS_AGGREGATE_LAST	(BIT(5))

 
#define WSM_RX_STATUS_DEFRAGMENTED	(BIT(6))

 
#define WSM_RX_STATUS_BEACON		(BIT(7))

 
#define WSM_RX_STATUS_TIM		(BIT(8))

 
#define WSM_RX_STATUS_MULTICAST		(BIT(9))

 
#define WSM_RX_STATUS_MATCHING_SSID	(BIT(10))

 
#define WSM_RX_STATUS_MATCHING_BSSI	(BIT(11))

 
#define WSM_RX_STATUS_MORE_DATA		(BIT(12))

 
#define WSM_RX_STATUS_MEASUREMENT	(BIT(13))

 
#define WSM_RX_STATUS_HT		(BIT(14))

 
#define WSM_RX_STATUS_STBC		(BIT(15))

 
#define WSM_RX_STATUS_ADDRESS1		(BIT(16))

 
#define WSM_RX_STATUS_GROUP		(BIT(17))

 
#define WSM_RX_STATUS_BROADCAST		(BIT(18))

 
#define WSM_RX_STATUS_GROUP_KEY		(BIT(19))

 
#define WSM_RX_STATUS_KEY_IDX(status)	(((status >> 20)) & 0x0F)

 
#define WSM_RX_STATUS_TSF_INCLUDED	(BIT(24))

 
#define WSM_TX_2BYTES_SHIFT		(BIT(7))

 
 
#define WSM_JOIN_MODE_IBSS		(0)

 
#define WSM_JOIN_MODE_BSS		(1)

 
 
#define WSM_JOIN_PREAMBLE_LONG		(0)

 
#define WSM_JOIN_PREAMBLE_SHORT		(1)

 
#define WSM_JOIN_PREAMBLE_SHORT_2	(2)

 
 
#define WSM_JOIN_FLAGS_UNSYNCRONIZED	BIT(0)
 
#define WSM_JOIN_FLAGS_P2P_GO		BIT(1)
 
#define WSM_JOIN_FLAGS_FORCE		BIT(2)
 
#define WSM_JOIN_FLAGS_PRIO		BIT(3)
 
#define WSM_JOIN_FLAGS_FORCE_WITH_COMPLETE_IND BIT(5)

 
#define WSM_KEY_TYPE_WEP_DEFAULT	(0)
#define WSM_KEY_TYPE_WEP_PAIRWISE	(1)
#define WSM_KEY_TYPE_TKIP_GROUP		(2)
#define WSM_KEY_TYPE_TKIP_PAIRWISE	(3)
#define WSM_KEY_TYPE_AES_GROUP		(4)
#define WSM_KEY_TYPE_AES_PAIRWISE	(5)
#define WSM_KEY_TYPE_WAPI_GROUP		(6)
#define WSM_KEY_TYPE_WAPI_PAIRWISE	(7)

 
#define WSM_KEY_MAX_INDEX		(10)

 
#define WSM_ACK_POLICY_NORMAL		(0)
#define WSM_ACK_POLICY_NO_ACK		(1)

 
#define WSM_START_MODE_AP		(0)	 
#define WSM_START_MODE_P2P_GO		(1)	 
#define WSM_START_MODE_P2P_DEV		(2)	 

 
#define WSM_ASSOCIATION_MODE_USE_PREAMBLE_TYPE		(BIT(0))
#define WSM_ASSOCIATION_MODE_USE_HT_MODE		(BIT(1))
#define WSM_ASSOCIATION_MODE_USE_BASIC_RATE_SET		(BIT(2))
#define WSM_ASSOCIATION_MODE_USE_MPDU_START_SPACING	(BIT(3))
#define WSM_ASSOCIATION_MODE_SNOOP_ASSOC_FRAMES		(BIT(4))

 
#define WSM_RCPI_RSSI_THRESHOLD_ENABLE	(BIT(0))
#define WSM_RCPI_RSSI_USE_RSSI		(BIT(1))
#define WSM_RCPI_RSSI_DONT_USE_UPPER	(BIT(2))
#define WSM_RCPI_RSSI_DONT_USE_LOWER	(BIT(3))

 
#define WSM_UPDATE_IE_BEACON		(BIT(0))
#define WSM_UPDATE_IE_PROBE_RESP	(BIT(1))
#define WSM_UPDATE_IE_PROBE_REQ		(BIT(2))

 
 
#define WSM_EVENT_ERROR			(0)

 
#define WSM_EVENT_BSS_LOST		(1)

 
#define WSM_EVENT_BSS_REGAINED		(2)

 
#define WSM_EVENT_RADAR_DETECTED	(3)

 
#define WSM_EVENT_RCPI_RSSI		(4)

 
#define WSM_EVENT_BT_INACTIVE		(5)

 
#define WSM_EVENT_BT_ACTIVE		(6)

 
 
#define WSM_MIB_ID_DOT11_STATION_ID		0x0000

 
#define WSM_MIB_ID_DOT11_MAX_TRANSMIT_LIFTIME	0x0001

 
#define WSM_MIB_ID_DOT11_MAX_RECEIVE_LIFETIME	0x0002

 
#define WSM_MIB_ID_DOT11_SLOT_TIME		0x0003

 
#define WSM_MIB_ID_DOT11_GROUP_ADDRESSES_TABLE	0x0004
#define WSM_MAX_GRP_ADDRTABLE_ENTRIES		8

 
#define WSM_MIB_ID_DOT11_WEP_DEFAULT_KEY_ID	0x0005

 
#define WSM_MIB_ID_DOT11_CURRENT_TX_POWER_LEVEL	0x0006

 
#define WSM_MIB_ID_DOT11_RTS_THRESHOLD		0x0007

 
#define WSM_MIB_ID_NON_ERP_PROTECTION		0x1000

 
#define WSM_MIB_ID_ARP_IP_ADDRESSES_TABLE	0x1001
#define WSM_MAX_ARP_IP_ADDRTABLE_ENTRIES	1

 
#define WSM_MIB_ID_TEMPLATE_FRAME		0x1002

 
#define WSM_MIB_ID_RX_FILTER			0x1003

 
#define WSM_MIB_ID_BEACON_FILTER_TABLE		0x1004

 
#define WSM_MIB_ID_BEACON_FILTER_ENABLE		0x1005

 
#define WSM_MIB_ID_OPERATIONAL_POWER_MODE	0x1006

 
#define WSM_MIB_ID_BEACON_WAKEUP_PERIOD		0x1007

 
#define WSM_MIB_ID_RCPI_RSSI_THRESHOLD		0x1009

 
#define WSM_MIB_ID_STATISTICS_TABLE		0x100A

 
#define WSM_MIB_ID_IBSS_PS_CONFIG		0x100B

 
#define WSM_MIB_ID_COUNTERS_TABLE		0x100C

 
#define WSM_MIB_ID_BLOCK_ACK_POLICY		0x100E

 
#define WSM_MIB_ID_OVERRIDE_INTERNAL_TX_RATE	0x100F

 
#define WSM_MIB_ID_SET_ASSOCIATION_MODE		0x1010

 
#define WSM_MIB_ID_UPDATE_EPTA_CONFIG_DATA	0x1011

 
#define WSM_MIB_ID_SELECT_CCA_METHOD		0x1012

 
#define WSM_MIB_ID_SET_UAPSD_INFORMATION	0x1013

 
#define WSM_MIB_ID_SET_AUTO_CALIBRATION_MODE	0x1015

 
#define WSM_MIB_ID_SET_TX_RATE_RETRY_POLICY	0x1016

 
#define WSM_MIB_ID_SET_HOST_MSG_TYPE_FILTER	0x1017

 
#define WSM_MIB_ID_P2P_FIND_INFO		0x1018

 
#define WSM_MIB_ID_P2P_PS_MODE_INFO		0x1019

 
#define WSM_MIB_ID_SET_ETHERTYPE_DATAFRAME_FILTER 0x101A

 
#define WSM_MIB_ID_SET_UDPPORT_DATAFRAME_FILTER	0x101B

 
#define WSM_MIB_ID_SET_MAGIC_DATAFRAME_FILTER	0x101C

 
#define WSM_MIB_ID_P2P_DEVICE_INFO		0x101D

 
#define WSM_MIB_ID_SET_WCDMA_BAND		0x101E

 
#define WSM_MIB_ID_GRP_SEQ_COUNTER		0x101F

 
#define WSM_MIB_ID_PROTECTED_MGMT_POLICY	0x1020

 
#define WSM_MIB_ID_SET_HT_PROTECTION		0x1021

 
#define WSM_MIB_ID_GPIO_COMMAND			0x1022

 
#define WSM_MIB_ID_TSF_COUNTER			0x1023

 
#define WSM_MIB_ID_BLOCK_ACK_INFO		0x100D

 
#define WSM_MIB_USE_MULTI_TX_CONF		0x1024

 
#define WSM_MIB_ID_KEEP_ALIVE_PERIOD		0x1025

 
#define WSM_MIB_ID_DISABLE_BSSID_FILTER		0x1026

 
#define WSM_FRAME_TYPE_PROBE_REQUEST	(0)
#define WSM_FRAME_TYPE_BEACON		(1)
#define WSM_FRAME_TYPE_NULL		(2)
#define WSM_FRAME_TYPE_QOS_NULL		(3)
#define WSM_FRAME_TYPE_PS_POLL		(4)
#define WSM_FRAME_TYPE_PROBE_RESPONSE	(5)

#define WSM_FRAME_GREENFIELD		(0x80)	 

 
 
 
#define WSM_STATUS_SUCCESS              (0)

 
 
#define WSM_STATUS_FAILURE              (1)

 
#define WSM_INVALID_PARAMETER           (2)

 
 
#define WSM_ACCESS_DENIED               (3)

 
#define WSM_STATUS_DECRYPTFAILURE       (4)

 
#define WSM_STATUS_MICFAILURE           (5)

 
 
#define WSM_STATUS_RETRY_EXCEEDED       (6)

 
 
#define WSM_STATUS_TX_LIFETIME_EXCEEDED (7)

 
#define WSM_STATUS_LINK_LOST            (8)

 
#define WSM_STATUS_NO_KEY_FOUND         (9)

 
#define WSM_STATUS_JAMMER_DETECTED      (10)

 
 
#define WSM_REQUEUE                     (11)

 
#define WSM_MAX_FILTER_ELEMENTS		(4)

#define WSM_FILTER_ACTION_IGNORE	(0)
#define WSM_FILTER_ACTION_FILTER_IN	(1)
#define WSM_FILTER_ACTION_FILTER_OUT	(2)

#define WSM_FILTER_PORT_TYPE_DST	(0)
#define WSM_FILTER_PORT_TYPE_SRC	(1)

 
struct wsm_hdr {
	__le16 len;
	__le16 id;
};

#define WSM_TX_SEQ_MAX			(7)
#define WSM_TX_SEQ(seq)			\
		((seq & WSM_TX_SEQ_MAX) << 13)
#define WSM_TX_LINK_ID_MAX		(0x0F)
#define WSM_TX_LINK_ID(link_id)		\
		((link_id & WSM_TX_LINK_ID_MAX) << 6)

#define MAX_BEACON_SKIP_TIME_MS 1000

#define WSM_CMD_LAST_CHANCE_TIMEOUT (HZ * 3 / 2)

 
 

#define WSM_STARTUP_IND_ID 0x0801

struct wsm_startup_ind {
	u16 input_buffers;
	u16 input_buffer_size;
	u16 status;
	u16 hw_id;
	u16 hw_subid;
	u16 fw_cap;
	u16 fw_type;
	u16 fw_api;
	u16 fw_build;
	u16 fw_ver;
	char fw_label[128];
	u32 config[4];
};

 
 

 
#define WSM_CONFIGURATION_REQ_ID 0x0009
#define WSM_CONFIGURATION_RESP_ID 0x0409

struct wsm_tx_power_range {
	int min_power_level;
	int max_power_level;
	u32 stepping;
};

struct wsm_configuration {
	  u32 dot11MaxTransmitMsduLifeTime;
	  u32 dot11MaxReceiveLifeTime;
	  u32 dot11RtsThreshold;
	  u8 *dot11StationId;
	  const void *dpdData;
	  size_t dpdData_size;
	  u8 dot11FrequencyBandsSupported;
	  u32 supportedRateMask;
	  struct wsm_tx_power_range txPowerRange[2];
};

int wsm_configuration(struct cw1200_common *priv,
		      struct wsm_configuration *arg);

 
#define WSM_RESET_REQ_ID 0x000A
#define WSM_RESET_RESP_ID 0x040A
struct wsm_reset {
	  int link_id;
	  bool reset_statistics;
};

int wsm_reset(struct cw1200_common *priv, const struct wsm_reset *arg);

 
#define WSM_READ_MIB_REQ_ID 0x0005
#define WSM_READ_MIB_RESP_ID 0x0405
int wsm_read_mib(struct cw1200_common *priv, u16 mib_id, void *buf,
		 size_t buf_size);

 
#define WSM_WRITE_MIB_REQ_ID 0x0006
#define WSM_WRITE_MIB_RESP_ID 0x0406
int wsm_write_mib(struct cw1200_common *priv, u16 mib_id, void *buf,
		  size_t buf_size);

 
#define WSM_START_SCAN_REQ_ID 0x0007
#define WSM_START_SCAN_RESP_ID 0x0407

struct wsm_ssid {
	u8 ssid[32];
	u32 length;
};

struct wsm_scan_ch {
	u16 number;
	u32 min_chan_time;
	u32 max_chan_time;
	u32 tx_power_level;
};

struct wsm_scan {
	 
	u8 band;

	 
	u8 type;

	 
	u8 flags;

	 
	u8 max_tx_rate;

	 
	 
	 
	u32 auto_scan_interval;

	 
	 
	 
	 
	 
	u32 num_probes;

	 
	 
	u8 num_channels;

	 
	 
	 
	u8 num_ssids;

	 
	 
	u8 probe_delay;

	 
	struct wsm_ssid *ssids;

	 
	struct wsm_scan_ch *ch;
};

int wsm_scan(struct cw1200_common *priv, const struct wsm_scan *arg);

 
#define WSM_STOP_SCAN_REQ_ID 0x0008
#define WSM_STOP_SCAN_RESP_ID 0x0408
int wsm_stop_scan(struct cw1200_common *priv);

 
#define WSM_SCAN_COMPLETE_IND_ID 0x0806
struct wsm_scan_complete {
	 
	u32 status;

	 
	u8 psm;

	 
	u8 num_channels;
};

 
#define WSM_TX_CONFIRM_IND_ID 0x0404
#define WSM_MULTI_TX_CONFIRM_ID 0x041E

struct wsm_tx_confirm {
	 
	u32 packet_id;

	 
	u32 status;

	 
	u8 tx_rate;

	 
	 
	u8 ack_failures;

	 
	u16 flags;

	 
	 
	u32 media_delay;

	 
	 
	u32 tx_queue_delay;
};

 

 
struct wsm_tx {
	 
	struct wsm_hdr hdr;

	 
	u32 packet_id;   

	 
	u8 max_tx_rate;

	 
	u8 queue_id;

	 
	u8 more;

	 
	 
	 
	 
	 
	u8 flags;

	 
	u32 reserved;

	 
	 
	 
	 
	 
	__le32 expire_time;

	 
	__le32 ht_tx_parameters;
} __packed;

 
#define WSM_TX_EXTRA_HEADROOM (28)

 
#define WSM_RECEIVE_IND_ID 0x0804

struct wsm_rx {
	 
	u32 status;

	 
	u16 channel_number;

	 
	u8 rx_rate;

	 
	 
	u8 rcpi_rssi;

	 
	u32 flags;
};

 
#define WSM_RX_EXTRA_HEADROOM (16)

 
struct wsm_event {
	 
	  u32 id;

	 
	 
	 
	 
	  u32 data;
};

struct cw1200_wsm_event {
	struct list_head link;
	struct wsm_event evt;
};

 
 

typedef void (*wsm_event_cb) (struct cw1200_common *priv,
			      struct wsm_event *arg);

 
#define WSM_JOIN_REQ_ID 0x000B
#define WSM_JOIN_RESP_ID 0x040B

struct wsm_join {
	 
	u8 mode;

	 
	u8 band;

	 
	 
	 
	u16 channel_number;

	 
	 
	u8 bssid[6];

	 
	 
	 
	u16 atim_window;

	 
	u8 preamble_type;

	 
	 
	u8 probe_for_join;

	 
	u8 dtim_period;

	 
	u8 flags;

	 
	u32 ssid_len;

	 
	u8 ssid[32];

	 
	u32 beacon_interval;

	 
	u32 basic_rate_set;
};

struct wsm_join_cnf {
	u32 status;

	 
	u32 min_power_level;

	 
	u32 max_power_level;
};

int wsm_join(struct cw1200_common *priv, struct wsm_join *arg);

 
struct wsm_join_complete {
	 
	u32 status;
};

 
#define WSM_SET_PM_REQ_ID 0x0010
#define WSM_SET_PM_RESP_ID 0x0410
struct wsm_set_pm {
	 
	u8 mode;

	 
	u8 fast_psm_idle_period;

	 
	u8 ap_psm_change_period;

	 
	u8 min_auto_pspoll_period;
};

int wsm_set_pm(struct cw1200_common *priv, const struct wsm_set_pm *arg);

 
struct wsm_set_pm_complete {
	u8 psm;			 
};

 
#define WSM_SET_BSS_PARAMS_REQ_ID 0x0011
#define WSM_SET_BSS_PARAMS_RESP_ID 0x0411
struct wsm_set_bss_params {
	 
	u8 reset_beacon_loss;

	 
	 
	 
	u8 beacon_lost_count;

	 
	u16 aid;

	 
	u32 operational_rate_set;
};

int wsm_set_bss_params(struct cw1200_common *priv,
		       const struct wsm_set_bss_params *arg);

 
#define WSM_ADD_KEY_REQ_ID         0x000C
#define WSM_ADD_KEY_RESP_ID        0x040C
struct wsm_add_key {
	u8 type;		 
	u8 index;		 
	u16 reserved;
	union {
		struct {
			u8 peer[6];	 
			u8 reserved;
			u8 keylen;		 
			u8 keydata[16];		 
		} __packed wep_pairwise;
		struct {
			u8 keyid;	 
			u8 keylen;		 
			u16 reserved;
			u8 keydata[16];		 
		} __packed wep_group;
		struct {
			u8 peer[6];	 
			u16 reserved;
			u8 keydata[16];	 
			u8 rx_mic_key[8];		 
			u8 tx_mic_key[8];		 
		} __packed tkip_pairwise;
		struct {
			u8 keydata[16];	 
			u8 rx_mic_key[8];		 
			u8 keyid;		 
			u8 reserved[3];
			u8 rx_seqnum[8];	 
		} __packed tkip_group;
		struct {
			u8 peer[6];	 
			u16 reserved;
			u8 keydata[16];	 
		} __packed aes_pairwise;
		struct {
			u8 keydata[16];	 
			u8 keyid;		 
			u8 reserved[3];
			u8 rx_seqnum[8];	 
		} __packed aes_group;
		struct {
			u8 peer[6];	 
			u8 keyid;		 
			u8 reserved;
			u8 keydata[16];	 
			u8 mic_key[16];	 
		} __packed wapi_pairwise;
		struct {
			u8 keydata[16];	 
			u8 mic_key[16];	 
			u8 keyid;		 
			u8 reserved[3];
		} __packed wapi_group;
	} __packed;
} __packed;

int wsm_add_key(struct cw1200_common *priv, const struct wsm_add_key *arg);

 
#define WSM_REMOVE_KEY_REQ_ID         0x000D
#define WSM_REMOVE_KEY_RESP_ID        0x040D
struct wsm_remove_key {
	u8 index;  
};

int wsm_remove_key(struct cw1200_common *priv,
		   const struct wsm_remove_key *arg);

 
struct wsm_set_tx_queue_params {
	 
	u8 ackPolicy;

	 
	 
	u16 allowedMediumTime;

	 
	 
	u32 maxTransmitLifetime;
};

struct wsm_tx_queue_params {
	 
	struct wsm_set_tx_queue_params params[4];
};


#define WSM_TX_QUEUE_SET(queue_params, queue, ack_policy, allowed_time,\
		max_life_time)	\
do {							\
	struct wsm_set_tx_queue_params *p = &(queue_params)->params[queue]; \
	p->ackPolicy = (ack_policy);				\
	p->allowedMediumTime = (allowed_time);				\
	p->maxTransmitLifetime = (max_life_time);			\
} while (0)

int wsm_set_tx_queue_params(struct cw1200_common *priv,
			    const struct wsm_set_tx_queue_params *arg, u8 id);

 
#define WSM_EDCA_PARAMS_REQ_ID 0x0013
#define WSM_EDCA_PARAMS_RESP_ID 0x0413
struct wsm_edca_queue_params {
	 
	u16 cwmin;

	 
	u16 cwmax;

	 
	u16 aifns;

	 
	u16 txop_limit;

	 
	 
	 
	u32 max_rx_lifetime;
};

struct wsm_edca_params {
	 
	struct wsm_edca_queue_params params[4];
	bool uapsd_enable[4];
};

#define TXOP_UNIT 32
#define WSM_EDCA_SET(__edca, __queue, __aifs, __cw_min, __cw_max, __txop, __lifetime,\
		     __uapsd) \
	do {							\
		struct wsm_edca_queue_params *p = &(__edca)->params[__queue]; \
		p->cwmin = __cw_min;					\
		p->cwmax = __cw_max;					\
		p->aifns = __aifs;					\
		p->txop_limit = ((__txop) * TXOP_UNIT);			\
		p->max_rx_lifetime = __lifetime;			\
		(__edca)->uapsd_enable[__queue] = (__uapsd);		\
	} while (0)

int wsm_set_edca_params(struct cw1200_common *priv,
			const struct wsm_edca_params *arg);

int wsm_set_uapsd_param(struct cw1200_common *priv,
			const struct wsm_edca_params *arg);

 
 

 
#define WSM_SWITCH_CHANNEL_REQ_ID 0x0016
#define WSM_SWITCH_CHANNEL_RESP_ID 0x0416

struct wsm_switch_channel {
	 
	 
	u8 mode;

	 
	 
	 
	u8 switch_count;

	 
	 
	u16 channel_number;
};

int wsm_switch_channel(struct cw1200_common *priv,
		       const struct wsm_switch_channel *arg);

#define WSM_START_REQ_ID 0x0017
#define WSM_START_RESP_ID 0x0417

struct wsm_start {
	 
	  u8 mode;

	 
	  u8 band;

	 
	  u16 channel_number;

	 
	 
	  u32 ct_window;

	 
	 
	  u32 beacon_interval;

	 
	  u8 dtim_period;

	 
	  u8 preamble;

	 
	 
	  u8 probe_delay;

	 
	  u8 ssid_len;

	 
	  u8 ssid[32];

	 
	  u32 basic_rate_set;
};

int wsm_start(struct cw1200_common *priv, const struct wsm_start *arg);

#define WSM_BEACON_TRANSMIT_REQ_ID 0x0018
#define WSM_BEACON_TRANSMIT_RESP_ID 0x0418

struct wsm_beacon_transmit {
	 
	  u8 enable_beaconing;
};

int wsm_beacon_transmit(struct cw1200_common *priv,
			const struct wsm_beacon_transmit *arg);

int wsm_start_find(struct cw1200_common *priv);

int wsm_stop_find(struct cw1200_common *priv);

struct wsm_suspend_resume {
	 
	 
	  int link_id;
	 
	  bool stop;
	 
	  bool multicast;
	 
	 
	 
	  int queue;
};

 
struct wsm_update_ie {
	 
	  u16 what;
	  u16 count;
	  u8 *ies;
	  size_t length;
};

int wsm_update_ie(struct cw1200_common *priv,
		  const struct wsm_update_ie *arg);

 
struct wsm_map_link {
	 
	  u8 mac_addr[6];
	  u8 link_id;
};

int wsm_map_link(struct cw1200_common *priv, const struct wsm_map_link *arg);

 
 

static inline int wsm_set_output_power(struct cw1200_common *priv,
				       int power_level)
{
	__le32 val = __cpu_to_le32(power_level);
	return wsm_write_mib(priv, WSM_MIB_ID_DOT11_CURRENT_TX_POWER_LEVEL,
			     &val, sizeof(val));
}

static inline int wsm_set_beacon_wakeup_period(struct cw1200_common *priv,
					       unsigned dtim_interval,
					       unsigned listen_interval)
{
	struct {
		u8 numBeaconPeriods;
		u8 reserved;
		__le16 listenInterval;
	} val = {
		dtim_interval, 0, __cpu_to_le16(listen_interval)
	};

	if (dtim_interval > 0xFF || listen_interval > 0xFFFF)
		return -EINVAL;
	else
		return wsm_write_mib(priv, WSM_MIB_ID_BEACON_WAKEUP_PERIOD,
				     &val, sizeof(val));
}

struct wsm_rcpi_rssi_threshold {
	u8 rssiRcpiMode;	 
	u8 lowerThreshold;
	u8 upperThreshold;
	u8 rollingAverageCount;
};

static inline int wsm_set_rcpi_rssi_threshold(struct cw1200_common *priv,
					struct wsm_rcpi_rssi_threshold *arg)
{
	return wsm_write_mib(priv, WSM_MIB_ID_RCPI_RSSI_THRESHOLD, arg,
			     sizeof(*arg));
}

struct wsm_mib_counters_table {
	__le32 plcp_errors;
	__le32 fcs_errors;
	__le32 tx_packets;
	__le32 rx_packets;
	__le32 rx_packet_errors;
	__le32 rx_decryption_failures;
	__le32 rx_mic_failures;
	__le32 rx_no_key_failures;
	__le32 tx_multicast_frames;
	__le32 tx_frames_success;
	__le32 tx_frame_failures;
	__le32 tx_frames_retried;
	__le32 tx_frames_multi_retried;
	__le32 rx_frame_duplicates;
	__le32 rts_success;
	__le32 rts_failures;
	__le32 ack_failures;
	__le32 rx_multicast_frames;
	__le32 rx_frames_success;
	__le32 rx_cmac_icv_errors;
	__le32 rx_cmac_replays;
	__le32 rx_mgmt_ccmp_replays;
} __packed;

static inline int wsm_get_counters_table(struct cw1200_common *priv,
					 struct wsm_mib_counters_table *arg)
{
	return wsm_read_mib(priv, WSM_MIB_ID_COUNTERS_TABLE,
			    arg, sizeof(*arg));
}

static inline int wsm_get_station_id(struct cw1200_common *priv, u8 *mac)
{
	return wsm_read_mib(priv, WSM_MIB_ID_DOT11_STATION_ID, mac, ETH_ALEN);
}

struct wsm_rx_filter {
	bool promiscuous;
	bool bssid;
	bool fcs;
	bool probeResponder;
};

static inline int wsm_set_rx_filter(struct cw1200_common *priv,
				    const struct wsm_rx_filter *arg)
{
	__le32 val = 0;
	if (arg->promiscuous)
		val |= __cpu_to_le32(BIT(0));
	if (arg->bssid)
		val |= __cpu_to_le32(BIT(1));
	if (arg->fcs)
		val |= __cpu_to_le32(BIT(2));
	if (arg->probeResponder)
		val |= __cpu_to_le32(BIT(3));
	return wsm_write_mib(priv, WSM_MIB_ID_RX_FILTER, &val, sizeof(val));
}

int wsm_set_probe_responder(struct cw1200_common *priv, bool enable);

#define WSM_BEACON_FILTER_IE_HAS_CHANGED	BIT(0)
#define WSM_BEACON_FILTER_IE_NO_LONGER_PRESENT	BIT(1)
#define WSM_BEACON_FILTER_IE_HAS_APPEARED	BIT(2)

struct wsm_beacon_filter_table_entry {
	u8	ie_id;
	u8	flags;
	u8	oui[3];
	u8	match_data[3];
} __packed;

struct wsm_mib_beacon_filter_table {
	__le32 num;
	struct wsm_beacon_filter_table_entry entry[10];
} __packed;

static inline int wsm_set_beacon_filter_table(struct cw1200_common *priv,
					      struct wsm_mib_beacon_filter_table *ft)
{
	size_t size = __le32_to_cpu(ft->num) *
		     sizeof(struct wsm_beacon_filter_table_entry) +
		     sizeof(__le32);

	return wsm_write_mib(priv, WSM_MIB_ID_BEACON_FILTER_TABLE, ft, size);
}

#define WSM_BEACON_FILTER_ENABLE	BIT(0)  
#define WSM_BEACON_FILTER_AUTO_ERP	BIT(1)  

struct wsm_beacon_filter_control {
	int enabled;
	int bcn_count;
};

static inline int wsm_beacon_filter_control(struct cw1200_common *priv,
					struct wsm_beacon_filter_control *arg)
{
	struct {
		__le32 enabled;
		__le32 bcn_count;
	} val;
	val.enabled = __cpu_to_le32(arg->enabled);
	val.bcn_count = __cpu_to_le32(arg->bcn_count);
	return wsm_write_mib(priv, WSM_MIB_ID_BEACON_FILTER_ENABLE, &val,
			     sizeof(val));
}

enum wsm_power_mode {
	wsm_power_mode_active = 0,
	wsm_power_mode_doze = 1,
	wsm_power_mode_quiescent = 2,
};

struct wsm_operational_mode {
	enum wsm_power_mode power_mode;
	int disable_more_flag_usage;
	int perform_ant_diversity;
};

static inline int wsm_set_operational_mode(struct cw1200_common *priv,
					const struct wsm_operational_mode *arg)
{
	u8 val = arg->power_mode;
	if (arg->disable_more_flag_usage)
		val |= BIT(4);
	if (arg->perform_ant_diversity)
		val |= BIT(5);
	return wsm_write_mib(priv, WSM_MIB_ID_OPERATIONAL_POWER_MODE, &val,
			     sizeof(val));
}

struct wsm_template_frame {
	u8 frame_type;
	u8 rate;
	struct sk_buff *skb;
};

static inline int wsm_set_template_frame(struct cw1200_common *priv,
					 struct wsm_template_frame *arg)
{
	int ret;
	u8 *p = skb_push(arg->skb, 4);
	p[0] = arg->frame_type;
	p[1] = arg->rate;
	((__le16 *)p)[1] = __cpu_to_le16(arg->skb->len - 4);
	ret = wsm_write_mib(priv, WSM_MIB_ID_TEMPLATE_FRAME, p, arg->skb->len);
	skb_pull(arg->skb, 4);
	return ret;
}


struct wsm_protected_mgmt_policy {
	bool protectedMgmtEnable;
	bool unprotectedMgmtFramesAllowed;
	bool encryptionForAuthFrame;
};

static inline int wsm_set_protected_mgmt_policy(struct cw1200_common *priv,
		struct wsm_protected_mgmt_policy *arg)
{
	__le32 val = 0;
	int ret;
	if (arg->protectedMgmtEnable)
		val |= __cpu_to_le32(BIT(0));
	if (arg->unprotectedMgmtFramesAllowed)
		val |= __cpu_to_le32(BIT(1));
	if (arg->encryptionForAuthFrame)
		val |= __cpu_to_le32(BIT(2));
	ret = wsm_write_mib(priv, WSM_MIB_ID_PROTECTED_MGMT_POLICY,
			&val, sizeof(val));
	return ret;
}

struct wsm_mib_block_ack_policy {
	u8 tx_tid;
	u8 reserved1;
	u8 rx_tid;
	u8 reserved2;
} __packed;

static inline int wsm_set_block_ack_policy(struct cw1200_common *priv,
					   u8 tx_tid_policy,
					   u8 rx_tid_policy)
{
	struct wsm_mib_block_ack_policy val = {
		.tx_tid = tx_tid_policy,
		.rx_tid = rx_tid_policy,
	};
	return wsm_write_mib(priv, WSM_MIB_ID_BLOCK_ACK_POLICY, &val,
			     sizeof(val));
}

struct wsm_mib_association_mode {
	u8 flags;		 
	u8 preamble;	 
	u8 greenfield;	 
	u8 mpdu_start_spacing;
	__le32 basic_rate_set;
} __packed;

static inline int wsm_set_association_mode(struct cw1200_common *priv,
					   struct wsm_mib_association_mode *arg)
{
	return wsm_write_mib(priv, WSM_MIB_ID_SET_ASSOCIATION_MODE, arg,
			     sizeof(*arg));
}

#define WSM_TX_RATE_POLICY_FLAG_TERMINATE_WHEN_FINISHED BIT(2)
#define WSM_TX_RATE_POLICY_FLAG_COUNT_INITIAL_TRANSMIT BIT(3)
struct wsm_tx_rate_retry_policy {
	u8 index;
	u8 short_retries;
	u8 long_retries;
	 
	u8 flags;
	u8 rate_recoveries;
	u8 reserved[3];
	__le32 rate_count_indices[3];
} __packed;

struct wsm_set_tx_rate_retry_policy {
	u8 num;
	u8 reserved[3];
	struct wsm_tx_rate_retry_policy tbl[8];
} __packed;

static inline int wsm_set_tx_rate_retry_policy(struct cw1200_common *priv,
				struct wsm_set_tx_rate_retry_policy *arg)
{
	size_t size = 4 + arg->num * sizeof(struct wsm_tx_rate_retry_policy);
	return wsm_write_mib(priv, WSM_MIB_ID_SET_TX_RATE_RETRY_POLICY, arg,
			     size);
}

 
struct wsm_ether_type_filter_hdr {
	u8 num;		 
	u8 reserved[3];
} __packed;

struct wsm_ether_type_filter {
	u8 action;	 
	u8 reserved;
	__le16 type;	 
} __packed;

static inline int wsm_set_ether_type_filter(struct cw1200_common *priv,
				struct wsm_ether_type_filter_hdr *arg)
{
	size_t size = sizeof(struct wsm_ether_type_filter_hdr) +
		arg->num * sizeof(struct wsm_ether_type_filter);
	return wsm_write_mib(priv, WSM_MIB_ID_SET_ETHERTYPE_DATAFRAME_FILTER,
		arg, size);
}

 
struct wsm_udp_port_filter_hdr {
	u8 num;		 
	u8 reserved[3];
} __packed;

struct wsm_udp_port_filter {
	u8 action;	 
	u8 type;		 
	__le16 port;		 
} __packed;

static inline int wsm_set_udp_port_filter(struct cw1200_common *priv,
				struct wsm_udp_port_filter_hdr *arg)
{
	size_t size = sizeof(struct wsm_udp_port_filter_hdr) +
		arg->num * sizeof(struct wsm_udp_port_filter);
	return wsm_write_mib(priv, WSM_MIB_ID_SET_UDPPORT_DATAFRAME_FILTER,
		arg, size);
}

 
 
#define D11_MAX_SSID_LEN		(32)

struct wsm_p2p_device_type {
	__le16 category_id;
	u8 oui[4];
	__le16 subcategory_id;
} __packed;

struct wsm_p2p_device_info {
	struct wsm_p2p_device_type primaryDevice;
	u8 reserved1[3];
	u8 devname_size;
	u8 local_devname[D11_MAX_SSID_LEN];
	u8 reserved2[3];
	u8 num_secdev_supported;
	struct wsm_p2p_device_type secdevs[];
} __packed;

 
struct wsm_cdma_band {
	u8 wcdma_band;
	u8 reserved[3];
} __packed;

 
struct wsm_group_tx_seq {
	__le32 bits_47_16;
	__le16 bits_15_00;
	__le16 reserved;
} __packed;

 
#define WSM_DUAL_CTS_PROT_ENB		(1 << 0)
#define WSM_NON_GREENFIELD_STA_PRESENT  (1 << 1)
#define WSM_HT_PROT_MODE__NO_PROT	(0 << 2)
#define WSM_HT_PROT_MODE__NON_MEMBER	(1 << 2)
#define WSM_HT_PROT_MODE__20_MHZ	(2 << 2)
#define WSM_HT_PROT_MODE__NON_HT_MIXED	(3 << 2)
#define WSM_LSIG_TXOP_PROT_FULL		(1 << 4)
#define WSM_LARGE_L_LENGTH_PROT		(1 << 5)

struct wsm_ht_protection {
	__le32 flags;
} __packed;

 
#define WSM_GPIO_COMMAND_SETUP	0
#define WSM_GPIO_COMMAND_READ	1
#define WSM_GPIO_COMMAND_WRITE	2
#define WSM_GPIO_COMMAND_RESET	3
#define WSM_GPIO_ALL_PINS	0xFF

struct wsm_gpio_command {
	u8 command;
	u8 pin;
	__le16 config;
} __packed;

 
struct wsm_tsf_counter {
	__le64 tsf_counter;
} __packed;

 
struct wsm_keep_alive_period {
	__le16 period;
	u8 reserved[2];
} __packed;

static inline int wsm_keep_alive_period(struct cw1200_common *priv,
					int period)
{
	struct wsm_keep_alive_period arg = {
		.period = __cpu_to_le16(period),
	};
	return wsm_write_mib(priv, WSM_MIB_ID_KEEP_ALIVE_PERIOD,
			&arg, sizeof(arg));
};

 
struct wsm_set_bssid_filtering {
	u8 filter;
	u8 reserved[3];
} __packed;

static inline int wsm_set_bssid_filtering(struct cw1200_common *priv,
					  bool enabled)
{
	struct wsm_set_bssid_filtering arg = {
		.filter = !enabled,
	};
	return wsm_write_mib(priv, WSM_MIB_ID_DISABLE_BSSID_FILTER,
			&arg, sizeof(arg));
}

 
struct wsm_mib_multicast_filter {
	__le32 enable;
	__le32 num_addrs;
	u8 macaddrs[WSM_MAX_GRP_ADDRTABLE_ENTRIES][ETH_ALEN];
} __packed;

static inline int wsm_set_multicast_filter(struct cw1200_common *priv,
					   struct wsm_mib_multicast_filter *fp)
{
	return wsm_write_mib(priv, WSM_MIB_ID_DOT11_GROUP_ADDRESSES_TABLE,
			     fp, sizeof(*fp));
}

 
struct wsm_mib_arp_ipv4_filter {
	__le32 enable;
	__be32 ipv4addrs[WSM_MAX_ARP_IP_ADDRTABLE_ENTRIES];
} __packed;

static inline int wsm_set_arp_ipv4_filter(struct cw1200_common *priv,
					  struct wsm_mib_arp_ipv4_filter *fp)
{
	return wsm_write_mib(priv, WSM_MIB_ID_ARP_IP_ADDRESSES_TABLE,
			    fp, sizeof(*fp));
}

 
struct wsm_p2p_ps_modeinfo {
	u8	opp_ps_ct_window;
	u8	count;
	u8	reserved;
	u8	dtim_count;
	__le32	duration;
	__le32	interval;
	__le32	start_time;
} __packed;

static inline int wsm_set_p2p_ps_modeinfo(struct cw1200_common *priv,
					  struct wsm_p2p_ps_modeinfo *mi)
{
	return wsm_write_mib(priv, WSM_MIB_ID_P2P_PS_MODE_INFO,
			     mi, sizeof(*mi));
}

static inline int wsm_get_p2p_ps_modeinfo(struct cw1200_common *priv,
					  struct wsm_p2p_ps_modeinfo *mi)
{
	return wsm_read_mib(priv, WSM_MIB_ID_P2P_PS_MODE_INFO,
			    mi, sizeof(*mi));
}

 

static inline int wsm_use_multi_tx_conf(struct cw1200_common *priv,
					bool enabled)
{
	__le32 arg = enabled ? __cpu_to_le32(1) : 0;

	return wsm_write_mib(priv, WSM_MIB_USE_MULTI_TX_CONF,
			&arg, sizeof(arg));
}


 
struct wsm_uapsd_info {
	__le16 uapsd_flags;
	__le16 min_auto_trigger_interval;
	__le16 max_auto_trigger_interval;
	__le16 auto_trigger_step;
};

static inline int wsm_set_uapsd_info(struct cw1200_common *priv,
				     struct wsm_uapsd_info *arg)
{
	return wsm_write_mib(priv, WSM_MIB_ID_SET_UAPSD_INFORMATION,
				arg, sizeof(*arg));
}

 
struct wsm_override_internal_txrate {
	u8 internalTxRate;
	u8 nonErpInternalTxRate;
	u8 reserved[2];
} __packed;

static inline int wsm_set_override_internal_txrate(struct cw1200_common *priv,
				     struct wsm_override_internal_txrate *arg)
{
	return wsm_write_mib(priv, WSM_MIB_ID_OVERRIDE_INTERNAL_TX_RATE,
				arg, sizeof(*arg));
}

 
 

void wsm_lock_tx(struct cw1200_common *priv);
void wsm_lock_tx_async(struct cw1200_common *priv);
bool wsm_flush_tx(struct cw1200_common *priv);
void wsm_unlock_tx(struct cw1200_common *priv);

 
 

int wsm_handle_exception(struct cw1200_common *priv, u8 *data, size_t len);
int wsm_handle_rx(struct cw1200_common *priv, u16 id, struct wsm_hdr *wsm,
		  struct sk_buff **skb_p);

 
 

struct wsm_buf {
	u8 *begin;
	u8 *data;
	u8 *end;
};

void wsm_buf_init(struct wsm_buf *buf);
void wsm_buf_deinit(struct wsm_buf *buf);

 
 

struct wsm_cmd {
	spinlock_t lock;  
	int done;
	u8 *ptr;
	size_t len;
	void *arg;
	int ret;
	u16 cmd;
};

 
 

int wsm_get_tx(struct cw1200_common *priv, u8 **data,
	       size_t *tx_len, int *burst);
void wsm_txed(struct cw1200_common *priv, u8 *data);

 
 
 
 

static inline u8 wsm_queue_id_to_linux(u8 queue_id)
{
	static const u8 queue_mapping[] = {
		2, 3, 1, 0
	};
	return queue_mapping[queue_id];
}

static inline u8 wsm_queue_id_to_wsm(u8 queue_id)
{
	static const u8 queue_mapping[] = {
		3, 2, 0, 1
	};
	return queue_mapping[queue_id];
}

#endif  
