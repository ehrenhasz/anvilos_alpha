#ifndef _WCN36XX_PMC_H_
#define _WCN36XX_PMC_H_
struct wcn36xx;
enum wcn36xx_power_state {
	WCN36XX_FULL_POWER,
	WCN36XX_BMPS
};
int wcn36xx_pmc_enter_bmps_state(struct wcn36xx *wcn,
				 struct ieee80211_vif *vif);
int wcn36xx_pmc_exit_bmps_state(struct wcn36xx *wcn,
				struct ieee80211_vif *vif);
int wcn36xx_enable_keep_alive_null_packet(struct wcn36xx *wcn,
					  struct ieee80211_vif *vif);
#endif	 
