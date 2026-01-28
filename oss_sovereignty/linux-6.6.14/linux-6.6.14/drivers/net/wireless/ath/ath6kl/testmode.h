#include "core.h"
#ifdef CONFIG_NL80211_TESTMODE
void ath6kl_tm_rx_event(struct ath6kl *ar, void *buf, size_t buf_len);
int ath6kl_tm_cmd(struct wiphy *wiphy, struct wireless_dev *wdev,
		  void *data, int len);
#else
static inline void ath6kl_tm_rx_event(struct ath6kl *ar, void *buf,
				      size_t buf_len)
{
}
static inline int ath6kl_tm_cmd(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				void *data, int len)
{
	return 0;
}
#endif
