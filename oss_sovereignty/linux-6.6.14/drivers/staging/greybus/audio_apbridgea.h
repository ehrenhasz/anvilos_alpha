 
 
 

#ifndef __AUDIO_APBRIDGEA_H
#define __AUDIO_APBRIDGEA_H

#define AUDIO_APBRIDGEA_TYPE_SET_CONFIG			0x01
#define AUDIO_APBRIDGEA_TYPE_REGISTER_CPORT		0x02
#define AUDIO_APBRIDGEA_TYPE_UNREGISTER_CPORT		0x03
#define AUDIO_APBRIDGEA_TYPE_SET_TX_DATA_SIZE		0x04
							 
#define AUDIO_APBRIDGEA_TYPE_PREPARE_TX			0x06
#define AUDIO_APBRIDGEA_TYPE_START_TX			0x07
#define AUDIO_APBRIDGEA_TYPE_STOP_TX			0x08
#define AUDIO_APBRIDGEA_TYPE_SHUTDOWN_TX		0x09
#define AUDIO_APBRIDGEA_TYPE_SET_RX_DATA_SIZE		0x0a
							 
#define AUDIO_APBRIDGEA_TYPE_PREPARE_RX			0x0c
#define AUDIO_APBRIDGEA_TYPE_START_RX			0x0d
#define AUDIO_APBRIDGEA_TYPE_STOP_RX			0x0e
#define AUDIO_APBRIDGEA_TYPE_SHUTDOWN_RX		0x0f

#define AUDIO_APBRIDGEA_PCM_FMT_8			BIT(0)
#define AUDIO_APBRIDGEA_PCM_FMT_16			BIT(1)
#define AUDIO_APBRIDGEA_PCM_FMT_24			BIT(2)
#define AUDIO_APBRIDGEA_PCM_FMT_32			BIT(3)
#define AUDIO_APBRIDGEA_PCM_FMT_64			BIT(4)

#define AUDIO_APBRIDGEA_PCM_RATE_5512			BIT(0)
#define AUDIO_APBRIDGEA_PCM_RATE_8000			BIT(1)
#define AUDIO_APBRIDGEA_PCM_RATE_11025			BIT(2)
#define AUDIO_APBRIDGEA_PCM_RATE_16000			BIT(3)
#define AUDIO_APBRIDGEA_PCM_RATE_22050			BIT(4)
#define AUDIO_APBRIDGEA_PCM_RATE_32000			BIT(5)
#define AUDIO_APBRIDGEA_PCM_RATE_44100			BIT(6)
#define AUDIO_APBRIDGEA_PCM_RATE_48000			BIT(7)
#define AUDIO_APBRIDGEA_PCM_RATE_64000			BIT(8)
#define AUDIO_APBRIDGEA_PCM_RATE_88200			BIT(9)
#define AUDIO_APBRIDGEA_PCM_RATE_96000			BIT(10)
#define AUDIO_APBRIDGEA_PCM_RATE_176400			BIT(11)
#define AUDIO_APBRIDGEA_PCM_RATE_192000			BIT(12)

#define AUDIO_APBRIDGEA_DIRECTION_TX			BIT(0)
#define AUDIO_APBRIDGEA_DIRECTION_RX			BIT(1)

 
 

struct audio_apbridgea_hdr {
	__u8	type;
	__le16	i2s_port;
	__u8	data[];
} __packed;

struct audio_apbridgea_set_config_request {
	struct audio_apbridgea_hdr	hdr;
	__le32				format;	 
	__le32				rate;	 
	__le32				mclk_freq;  
} __packed;

struct audio_apbridgea_register_cport_request {
	struct audio_apbridgea_hdr	hdr;
	__le16				cport;
	__u8				direction;
} __packed;

struct audio_apbridgea_unregister_cport_request {
	struct audio_apbridgea_hdr	hdr;
	__le16				cport;
	__u8				direction;
} __packed;

struct audio_apbridgea_set_tx_data_size_request {
	struct audio_apbridgea_hdr	hdr;
	__le16				size;
} __packed;

struct audio_apbridgea_prepare_tx_request {
	struct audio_apbridgea_hdr	hdr;
} __packed;

struct audio_apbridgea_start_tx_request {
	struct audio_apbridgea_hdr	hdr;
	__le64				timestamp;
} __packed;

struct audio_apbridgea_stop_tx_request {
	struct audio_apbridgea_hdr	hdr;
} __packed;

struct audio_apbridgea_shutdown_tx_request {
	struct audio_apbridgea_hdr	hdr;
} __packed;

struct audio_apbridgea_set_rx_data_size_request {
	struct audio_apbridgea_hdr	hdr;
	__le16				size;
} __packed;

struct audio_apbridgea_prepare_rx_request {
	struct audio_apbridgea_hdr	hdr;
} __packed;

struct audio_apbridgea_start_rx_request {
	struct audio_apbridgea_hdr	hdr;
} __packed;

struct audio_apbridgea_stop_rx_request {
	struct audio_apbridgea_hdr	hdr;
} __packed;

struct audio_apbridgea_shutdown_rx_request {
	struct audio_apbridgea_hdr	hdr;
} __packed;

#endif  
