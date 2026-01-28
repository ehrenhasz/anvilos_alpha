#ifndef WFX_DATA_RX_H
#define WFX_DATA_RX_H
struct wfx_vif;
struct sk_buff;
struct wfx_hif_ind_rx;
void wfx_rx_cb(struct wfx_vif *wvif, const struct wfx_hif_ind_rx *arg, struct sk_buff *skb);
#endif
