 
 
#ifndef WFX_HIF_RX_H
#define WFX_HIF_RX_H

struct wfx_dev;
struct sk_buff;

void wfx_handle_rx(struct wfx_dev *wdev, struct sk_buff *skb);

#endif
