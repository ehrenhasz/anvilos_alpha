
 

#include "../wlcore/wlcore.h"
#include "../wlcore/cmd.h"
#include "../wlcore/debug.h"
#include "../wlcore/acx.h"
#include "../wlcore/tx.h"

#include "wl18xx.h"
#include "tx.h"

static
void wl18xx_get_last_tx_rate(struct wl1271 *wl, struct ieee80211_vif *vif,
			     u8 band, struct ieee80211_tx_rate *rate, u8 hlid)
{
	u8 fw_rate = wl->links[hlid].fw_rate_idx;

	if (fw_rate > CONF_HW_RATE_INDEX_MAX) {
		wl1271_error("last Tx rate invalid: %d", fw_rate);
		rate->idx = 0;
		rate->flags = 0;
		return;
	}

	if (fw_rate <= CONF_HW_RATE_INDEX_54MBPS) {
		rate->idx = fw_rate;
		if (band == NL80211_BAND_5GHZ)
			rate->idx -= CONF_HW_RATE_INDEX_6MBPS;
		rate->flags = 0;
	} else {
		rate->flags = IEEE80211_TX_RC_MCS;
		rate->idx = fw_rate - CONF_HW_RATE_INDEX_MCS0;

		 
		if (fw_rate >= CONF_HW_RATE_INDEX_MCS7_SGI)
			(rate->idx)--;
		if (fw_rate == CONF_HW_RATE_INDEX_MCS15_SGI)
			(rate->idx)--;

		 
		if (fw_rate == CONF_HW_RATE_INDEX_MCS7_SGI ||
		    fw_rate == CONF_HW_RATE_INDEX_MCS15_SGI)
			rate->flags |= IEEE80211_TX_RC_SHORT_GI;

		if (fw_rate > CONF_HW_RATE_INDEX_MCS7_SGI && vif) {
			struct wl12xx_vif *wlvif = wl12xx_vif_to_data(vif);
			if (wlvif->channel_type == NL80211_CHAN_HT40MINUS ||
			    wlvif->channel_type == NL80211_CHAN_HT40PLUS) {
				 
				rate->idx -= 8;
				rate->flags |= IEEE80211_TX_RC_40_MHZ_WIDTH;
			}
		}
	}
}

static void wl18xx_tx_complete_packet(struct wl1271 *wl, u8 tx_stat_byte)
{
	struct ieee80211_tx_info *info;
	struct sk_buff *skb;
	int id = tx_stat_byte & WL18XX_TX_STATUS_DESC_ID_MASK;
	bool tx_success;
	struct wl1271_tx_hw_descr *tx_desc;

	 
	if (unlikely(id >= wl->num_tx_desc || wl->tx_frames[id] == NULL)) {
		wl1271_warning("illegal id in tx completion: %d", id);
		return;
	}

	 
	tx_success = !(tx_stat_byte & BIT(WL18XX_TX_STATUS_STAT_BIT_IDX));

	skb = wl->tx_frames[id];
	info = IEEE80211_SKB_CB(skb);
	tx_desc = (struct wl1271_tx_hw_descr *)skb->data;

	if (wl12xx_is_dummy_packet(wl, skb)) {
		wl1271_free_tx_id(wl, id);
		return;
	}

	 
	if (tx_success && !(info->flags & IEEE80211_TX_CTL_NO_ACK))
		info->flags |= IEEE80211_TX_STAT_ACK;
	 
	wl18xx_get_last_tx_rate(wl, info->control.vif,
				info->band,
				&info->status.rates[0],
				tx_desc->hlid);

	info->status.rates[0].count = 1;  
	info->status.ack_signal = -1;

	if (!tx_success)
		wl->stats.retry_count++;

	 

	 
	skb_pull(skb, sizeof(struct wl1271_tx_hw_descr));

	 
	if ((wl->quirks & WLCORE_QUIRK_TKIP_HEADER_SPACE) &&
	    info->control.hw_key &&
	    info->control.hw_key->cipher == WLAN_CIPHER_SUITE_TKIP) {
		int hdrlen = ieee80211_get_hdrlen_from_skb(skb);
		memmove(skb->data + WL1271_EXTRA_SPACE_TKIP, skb->data, hdrlen);
		skb_pull(skb, WL1271_EXTRA_SPACE_TKIP);
	}

	wl1271_debug(DEBUG_TX, "tx status id %u skb 0x%p success %d",
		     id, skb, tx_success);

	 
	skb_queue_tail(&wl->deferred_tx_queue, skb);
	queue_work(wl->freezable_wq, &wl->netstack_work);
	wl1271_free_tx_id(wl, id);
}

void wl18xx_tx_immediate_complete(struct wl1271 *wl)
{
	struct wl18xx_fw_status_priv *status_priv =
		(struct wl18xx_fw_status_priv *)wl->fw_status->priv;
	struct wl18xx_priv *priv = wl->priv;
	u8 i, hlid;

	 
	if (priv->last_fw_rls_idx == status_priv->fw_release_idx)
		return;

	 
	hlid = wl->fw_status->counters.hlid;

	if (hlid < WLCORE_MAX_LINKS) {
		wl->links[hlid].fw_rate_idx =
				wl->fw_status->counters.tx_last_rate;
		wl->links[hlid].fw_rate_mbps =
				wl->fw_status->counters.tx_last_rate_mbps;
	}

	 
	wl1271_debug(DEBUG_TX, "last released desc = %d, current idx = %d",
		     priv->last_fw_rls_idx, status_priv->fw_release_idx);

	if (status_priv->fw_release_idx >= WL18XX_FW_MAX_TX_STATUS_DESC) {
		wl1271_error("invalid desc release index %d",
			     status_priv->fw_release_idx);
		WARN_ON(1);
		return;
	}

	for (i = priv->last_fw_rls_idx;
	     i != status_priv->fw_release_idx;
	     i = (i + 1) % WL18XX_FW_MAX_TX_STATUS_DESC) {
		wl18xx_tx_complete_packet(wl,
			status_priv->released_tx_desc[i]);

		wl->tx_results_count++;
	}

	priv->last_fw_rls_idx = status_priv->fw_release_idx;
}
