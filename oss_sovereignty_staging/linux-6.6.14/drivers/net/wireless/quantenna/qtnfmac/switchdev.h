 
 

#ifndef QTNFMAC_SWITCHDEV_H_
#define QTNFMAC_SWITCHDEV_H_

#include <linux/skbuff.h>

#ifdef CONFIG_NET_SWITCHDEV

static inline void qtnfmac_switch_mark_skb_flooded(struct sk_buff *skb)
{
	skb->offload_fwd_mark = 1;
}

#else

static inline void qtnfmac_switch_mark_skb_flooded(struct sk_buff *skb)
{
}

#endif

#endif  
