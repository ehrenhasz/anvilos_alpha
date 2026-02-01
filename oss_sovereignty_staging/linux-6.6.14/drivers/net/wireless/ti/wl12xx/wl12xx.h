 
 

#ifndef __WL12XX_PRIV_H__
#define __WL12XX_PRIV_H__

#include "conf.h"

 
#define CHIP_ID_127X_PG10              (0x04030101)
#define CHIP_ID_127X_PG20              (0x04030111)
#define CHIP_ID_128X_PG10              (0x05030101)
#define CHIP_ID_128X_PG20              (0x05030111)

 
#define WL127X_CHIP_VER		6
 
#define WL127X_IFTYPE_SR_VER	3
#define WL127X_MAJOR_SR_VER	10
#define WL127X_SUBTYPE_SR_VER	WLCORE_FW_VER_IGNORE
#define WL127X_MINOR_SR_VER	133
 
#define WL127X_IFTYPE_MR_VER	5
#define WL127X_MAJOR_MR_VER	7
#define WL127X_SUBTYPE_MR_VER	WLCORE_FW_VER_IGNORE
#define WL127X_MINOR_MR_VER	42

 
#define WL128X_CHIP_VER		7
 
#define WL128X_IFTYPE_SR_VER	3
#define WL128X_MAJOR_SR_VER	10
#define WL128X_SUBTYPE_SR_VER	WLCORE_FW_VER_IGNORE
#define WL128X_MINOR_SR_VER	133
 
#define WL128X_IFTYPE_MR_VER	5
#define WL128X_MAJOR_MR_VER	7
#define WL128X_SUBTYPE_MR_VER	WLCORE_FW_VER_IGNORE
#define WL128X_MINOR_MR_VER	42

#define WL12XX_AGGR_BUFFER_SIZE	(4 * PAGE_SIZE)

#define WL12XX_NUM_TX_DESCRIPTORS 16
#define WL12XX_NUM_RX_DESCRIPTORS 8

#define WL12XX_NUM_MAC_ADDRESSES 2

#define WL12XX_RX_BA_MAX_SESSIONS 3

#define WL12XX_MAX_AP_STATIONS 8
#define WL12XX_MAX_LINKS 12

struct wl127x_rx_mem_pool_addr {
	u32 addr;
	u32 addr_extra;
};

struct wl12xx_priv {
	struct wl12xx_priv_conf conf;

	int ref_clock;
	int tcxo_clock;

	struct wl127x_rx_mem_pool_addr *rx_mem_addr;
};

 
enum {
	WL12XX_REFCLOCK_19	= 0,  
	WL12XX_REFCLOCK_26	= 1,  
	WL12XX_REFCLOCK_38	= 2,  
	WL12XX_REFCLOCK_52	= 3,  
	WL12XX_REFCLOCK_38_XTAL = 4,  
	WL12XX_REFCLOCK_26_XTAL = 5,  
};

 
enum {
	WL12XX_TCXOCLOCK_19_2	= 0,  
	WL12XX_TCXOCLOCK_26	= 1,  
	WL12XX_TCXOCLOCK_38_4	= 2,  
	WL12XX_TCXOCLOCK_52	= 3,  
	WL12XX_TCXOCLOCK_16_368	= 4,  
	WL12XX_TCXOCLOCK_32_736	= 5,  
	WL12XX_TCXOCLOCK_16_8	= 6,  
	WL12XX_TCXOCLOCK_33_6	= 7,  
};

struct wl12xx_clock {
	u32	freq;
	bool	xtal;
	u8	hw_idx;
};

struct wl12xx_fw_packet_counters {
	 
	u8 tx_released_pkts[NUM_TX_QUEUES];

	 
	u8 tx_lnk_free_pkts[WL12XX_MAX_LINKS];

	 
	u8 tx_voice_released_blks;

	 
	u8 tx_last_rate;

	u8 padding[2];
} __packed;

 
struct wl12xx_fw_status {
	__le32 intr;
	u8  fw_rx_counter;
	u8  drv_rx_counter;
	u8  reserved;
	u8  tx_results_counter;
	__le32 rx_pkt_descs[WL12XX_NUM_RX_DESCRIPTORS];

	__le32 fw_localtime;

	 
	__le32 link_ps_bitmap;

	 
	__le32 link_fast_bitmap;

	 
	__le32 total_released_blks;

	 
	__le32 tx_total;

	struct wl12xx_fw_packet_counters counters;

	__le32 log_start_addr;
} __packed;

#endif  
