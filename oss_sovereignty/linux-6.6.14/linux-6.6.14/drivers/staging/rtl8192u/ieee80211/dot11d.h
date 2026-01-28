#ifndef __INC_DOT11D_H
#define __INC_DOT11D_H
#include "ieee80211.h"
struct chnl_txpower_triple {
	u8  first_channel;
	u8  num_channels;
	u8  max_tx_pwr_dbm;
};
enum dot11d_state {
	DOT11D_STATE_NONE = 0,
	DOT11D_STATE_LEARNED,
	DOT11D_STATE_DONE,
};
struct rt_dot11d_info {
	u16 country_ie_len;  
	u8  country_ie_src_addr[ETH_ALEN];  
	u8  country_ie_buf[MAX_IE_LEN];
	u8  country_ie_watchdog;
	u8  channel_map[MAX_CHANNEL_NUMBER + 1];   
	u8  max_tx_pwr_dbm_list[MAX_CHANNEL_NUMBER + 1];
	enum dot11d_state state;
	u8  dot11d_enabled;  
};
#define GET_DOT11D_INFO(ieee_dev) ((struct rt_dot11d_info *)((ieee_dev)->dot11d_info))
#define IS_DOT11D_ENABLE(ieee_dev) (GET_DOT11D_INFO(ieee_dev)->dot11d_enabled)
#define IS_COUNTRY_IE_VALID(ieee_dev) (GET_DOT11D_INFO(ieee_dev)->country_ie_len > 0)
#define IS_EQUAL_CIE_SRC(ieee_dev, addr) ether_addr_equal(GET_DOT11D_INFO(ieee_dev)->country_ie_src_addr, addr)
#define UPDATE_CIE_SRC(ieee_dev, addr) ether_addr_copy(GET_DOT11D_INFO(ieee_dev)->country_ie_src_addr, addr)
#define GET_CIE_WATCHDOG(ieee_dev) (GET_DOT11D_INFO(ieee_dev)->country_ie_watchdog)
#define RESET_CIE_WATCHDOG(ieee_dev) (GET_CIE_WATCHDOG(ieee_dev) = 0)
#define UPDATE_CIE_WATCHDOG(ieee_dev) (++GET_CIE_WATCHDOG(ieee_dev))
void rtl8192u_dot11d_init(struct ieee80211_device *dev);
void dot11d_reset(struct ieee80211_device *dev);
void dot11d_update_country_ie(struct ieee80211_device *dev,
			      u8 *addr,
			      u16 coutry_ie_len,
			      u8 *coutry_ie);
u8 dot11d_get_max_tx_pwr_in_dbm(struct ieee80211_device *dev, u8 channel);
void dot11d_scan_complete(struct ieee80211_device *dev);
int is_legal_channel(struct ieee80211_device *dev, u8 channel);
int to_legal_channel(struct ieee80211_device *dev, u8 channel);
#endif  
