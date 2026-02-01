 

#ifndef COMMON_H
#define COMMON_H

#include <linux/netdevice.h>

#define ATH6KL_MAX_IE			256

__printf(2, 3) void ath6kl_printk(const char *level, const char *fmt, ...);

 
#define ATH6KL_ABI_VERSION        1

#define SIGNAL_QUALITY_METRICS_NUM_MAX    2

enum {
	SIGNAL_QUALITY_METRICS_SNR = 0,
	SIGNAL_QUALITY_METRICS_RSSI,
	SIGNAL_QUALITY_METRICS_ALL,
};

 

#define WMI_MAX_TX_DATA_FRAME_LENGTH	      \
	(1500 + sizeof(struct wmi_data_hdr) + \
	 sizeof(struct ethhdr) +      \
	 sizeof(struct ath6kl_llc_snap_hdr))

   
#define WMI_MAX_AMSDU_RX_DATA_FRAME_LENGTH    \
	(3840 + sizeof(struct wmi_data_hdr) + \
	 sizeof(struct ethhdr) +      \
	 sizeof(struct ath6kl_llc_snap_hdr))

#define EPPING_ALIGNMENT_PAD			       \
	(((sizeof(struct htc_frame_hdr) + 3) & (~0x3)) \
	 - sizeof(struct htc_frame_hdr))

struct ath6kl_llc_snap_hdr {
	u8 dsap;
	u8 ssap;
	u8 cntl;
	u8 org_code[3];
	__be16 eth_type;
} __packed;

enum ath6kl_crypto_type {
	NONE_CRYPT          = 0x01,
	WEP_CRYPT           = 0x02,
	TKIP_CRYPT          = 0x04,
	AES_CRYPT           = 0x08,
	WAPI_CRYPT          = 0x10,
};

struct htc_endpoint_credit_dist;
struct ath6kl;
struct ath6kl_htcap;
enum htc_credit_dist_reason;
struct ath6kl_htc_credit_info;

struct sk_buff *ath6kl_buf_alloc(int size);
#endif  
