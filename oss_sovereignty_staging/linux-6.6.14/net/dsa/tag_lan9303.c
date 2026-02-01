
 
#include <linux/dsa/lan9303.h>
#include <linux/etherdevice.h>
#include <linux/list.h>
#include <linux/slab.h>

#include "tag.h"

 

#define LAN9303_NAME "lan9303"

#define LAN9303_TAG_LEN 4
# define LAN9303_TAG_TX_USE_ALR BIT(3)
# define LAN9303_TAG_TX_STP_OVERRIDE BIT(4)
# define LAN9303_TAG_RX_IGMP BIT(3)
# define LAN9303_TAG_RX_STP BIT(4)
# define LAN9303_TAG_RX_TRAPPED_TO_CPU (LAN9303_TAG_RX_IGMP | \
					LAN9303_TAG_RX_STP)

 
static int lan9303_xmit_use_arl(struct dsa_port *dp, u8 *dest_addr)
{
	struct lan9303 *chip = dp->ds->priv;

	return chip->is_bridged && !is_multicast_ether_addr(dest_addr);
}

static struct sk_buff *lan9303_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct dsa_port *dp = dsa_slave_to_port(dev);
	__be16 *lan9303_tag;
	u16 tag;

	 
	skb_push(skb, LAN9303_TAG_LEN);

	 
	dsa_alloc_etype_header(skb, LAN9303_TAG_LEN);

	lan9303_tag = dsa_etype_header_pos_tx(skb);

	tag = lan9303_xmit_use_arl(dp, skb->data) ?
		LAN9303_TAG_TX_USE_ALR :
		dp->index | LAN9303_TAG_TX_STP_OVERRIDE;
	lan9303_tag[0] = htons(ETH_P_8021Q);
	lan9303_tag[1] = htons(tag);

	return skb;
}

static struct sk_buff *lan9303_rcv(struct sk_buff *skb, struct net_device *dev)
{
	u16 lan9303_tag1;
	unsigned int source_port;

	if (unlikely(!pskb_may_pull(skb, LAN9303_TAG_LEN))) {
		dev_warn_ratelimited(&dev->dev,
				     "Dropping packet, cannot pull\n");
		return NULL;
	}

	if (skb_vlan_tag_present(skb)) {
		lan9303_tag1 = skb_vlan_tag_get(skb);
		__vlan_hwaccel_clear_tag(skb);
	} else {
		skb_push_rcsum(skb, ETH_HLEN);
		__skb_vlan_pop(skb, &lan9303_tag1);
		skb_pull_rcsum(skb, ETH_HLEN);
	}

	source_port = lan9303_tag1 & 0x3;

	skb->dev = dsa_master_find_slave(dev, 0, source_port);
	if (!skb->dev) {
		dev_warn_ratelimited(&dev->dev, "Dropping packet due to invalid source port\n");
		return NULL;
	}

	if (!(lan9303_tag1 & LAN9303_TAG_RX_TRAPPED_TO_CPU))
		dsa_default_offload_fwd_mark(skb);

	return skb;
}

static const struct dsa_device_ops lan9303_netdev_ops = {
	.name = LAN9303_NAME,
	.proto	= DSA_TAG_PROTO_LAN9303,
	.xmit = lan9303_xmit,
	.rcv = lan9303_rcv,
	.needed_headroom = LAN9303_TAG_LEN,
};

MODULE_LICENSE("GPL");
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_LAN9303, LAN9303_NAME);

module_dsa_tag_driver(lan9303_netdev_ops);
