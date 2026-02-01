 
 
#ifndef _RTL871X_HT_H_
#define _RTL871X_HT_H_

#include "osdep_service.h"
#include "wifi.h"

struct ht_priv {
	unsigned int	ht_option;
	unsigned int	ampdu_enable; 
	unsigned char	baddbareq_issued[16];
	unsigned int	tx_amsdu_enable; 
	unsigned int	tx_amdsu_maxlen;  
	unsigned int	rx_ampdu_maxlen;  
	struct ieee80211_ht_cap ht_cap;
};

#endif	 

