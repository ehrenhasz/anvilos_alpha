
 

#include <linux/skbuff.h>
#include <net/dsa.h>

#include "tag.h"

#define HELLCREEK_NAME		"hellcreek"

#define HELLCREEK_TAG_LEN	1

static struct sk_buff *hellcreek_xmit(struct sk_buff *skb,
				      struct net_device *dev)
{
	struct dsa_port *dp = dsa_slave_to_port(dev);
	u8 *tag;

	 
	if (skb->ip_summed == CHECKSUM_PARTIAL &&
	    skb_checksum_help(skb))
		return NULL;

	 
	tag  = skb_put(skb, HELLCREEK_TAG_LEN);
	*tag = BIT(dp->index);

	return skb;
}

static struct sk_buff *hellcreek_rcv(struct sk_buff *skb,
				     struct net_device *dev)
{
	 
	u8 *tag = skb_tail_pointer(skb) - HELLCREEK_TAG_LEN;
	unsigned int port = tag[0] & 0x03;

	skb->dev = dsa_master_find_slave(dev, 0, port);
	if (!skb->dev) {
		netdev_warn_once(dev, "Failed to get source port: %d\n", port);
		return NULL;
	}

	if (pskb_trim_rcsum(skb, skb->len - HELLCREEK_TAG_LEN))
		return NULL;

	dsa_default_offload_fwd_mark(skb);

	return skb;
}

static const struct dsa_device_ops hellcreek_netdev_ops = {
	.name	  = HELLCREEK_NAME,
	.proto	  = DSA_TAG_PROTO_HELLCREEK,
	.xmit	  = hellcreek_xmit,
	.rcv	  = hellcreek_rcv,
	.needed_tailroom = HELLCREEK_TAG_LEN,
};

MODULE_LICENSE("Dual MIT/GPL");
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_HELLCREEK, HELLCREEK_NAME);

module_dsa_tag_driver(hellcreek_netdev_ops);
