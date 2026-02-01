 
 

#ifndef __RTW_WIFI_REGD_H__
#define __RTW_WIFI_REGD_H__

void rtw_regd_init(struct wiphy *wiphy,
		   void (*reg_notifier)(struct wiphy *wiphy,
					struct regulatory_request *request));
void rtw_reg_notifier(struct wiphy *wiphy, struct regulatory_request *request);


#endif
