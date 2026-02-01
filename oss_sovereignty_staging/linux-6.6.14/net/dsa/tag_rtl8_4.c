
 

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/etherdevice.h>

#include "tag.h"

 

#define RTL8_4_NAME			"rtl8_4"
#define RTL8_4T_NAME			"rtl8_4t"

#define RTL8_4_TAG_LEN			8

#define RTL8_4_PROTOCOL			GENMASK(15, 8)
#define   RTL8_4_PROTOCOL_RTL8365MB	0x04
#define RTL8_4_REASON			GENMASK(7, 0)
#define   RTL8_4_REASON_FORWARD		0
#define   RTL8_4_REASON_TRAP		80

#define RTL8_4_LEARN_DIS		BIT(5)

#define RTL8_4_TX			GENMASK(3, 0)
#define RTL8_4_RX			GENMASK(10, 0)

static void rtl8_4_write_tag(struct sk_buff *skb, struct net_device *dev,
			     void *tag)
{
	struct dsa_port *dp = dsa_slave_to_port(dev);
	__be16 tag16[RTL8_4_TAG_LEN / 2];

	 
	tag16[0] = htons(ETH_P_REALTEK);

	 
	tag16[1] = htons(FIELD_PREP(RTL8_4_PROTOCOL, RTL8_4_PROTOCOL_RTL8365MB));

	 
	tag16[2] = htons(FIELD_PREP(RTL8_4_LEARN_DIS, 1));

	 
	tag16[3] = htons(FIELD_PREP(RTL8_4_RX, BIT(dp->index)));

	memcpy(tag, tag16, RTL8_4_TAG_LEN);
}

static struct sk_buff *rtl8_4_tag_xmit(struct sk_buff *skb,
				       struct net_device *dev)
{
	skb_push(skb, RTL8_4_TAG_LEN);

	dsa_alloc_etype_header(skb, RTL8_4_TAG_LEN);

	rtl8_4_write_tag(skb, dev, dsa_etype_header_pos_tx(skb));

	return skb;
}

static struct sk_buff *rtl8_4t_tag_xmit(struct sk_buff *skb,
					struct net_device *dev)
{
	 
	if (skb->ip_summed == CHECKSUM_PARTIAL && skb_checksum_help(skb))
		return NULL;

	rtl8_4_write_tag(skb, dev, skb_put(skb, RTL8_4_TAG_LEN));

	return skb;
}

static int rtl8_4_read_tag(struct sk_buff *skb, struct net_device *dev,
			   void *tag)
{
	__be16 tag16[RTL8_4_TAG_LEN / 2];
	u16 etype;
	u8 reason;
	u8 proto;
	u8 port;

	memcpy(tag16, tag, RTL8_4_TAG_LEN);

	 
	etype = ntohs(tag16[0]);
	if (unlikely(etype != ETH_P_REALTEK)) {
		dev_warn_ratelimited(&dev->dev,
				     "non-realtek ethertype 0x%04x\n", etype);
		return -EPROTO;
	}

	 
	proto = FIELD_GET(RTL8_4_PROTOCOL, ntohs(tag16[1]));
	if (unlikely(proto != RTL8_4_PROTOCOL_RTL8365MB)) {
		dev_warn_ratelimited(&dev->dev,
				     "unknown realtek protocol 0x%02x\n",
				     proto);
		return -EPROTO;
	}

	 
	reason = FIELD_GET(RTL8_4_REASON, ntohs(tag16[1]));

	 
	port = FIELD_GET(RTL8_4_TX, ntohs(tag16[3]));
	skb->dev = dsa_master_find_slave(dev, 0, port);
	if (!skb->dev) {
		dev_warn_ratelimited(&dev->dev,
				     "could not find slave for port %d\n",
				     port);
		return -ENOENT;
	}

	if (reason != RTL8_4_REASON_TRAP)
		dsa_default_offload_fwd_mark(skb);

	return 0;
}

static struct sk_buff *rtl8_4_tag_rcv(struct sk_buff *skb,
				      struct net_device *dev)
{
	if (unlikely(!pskb_may_pull(skb, RTL8_4_TAG_LEN)))
		return NULL;

	if (unlikely(rtl8_4_read_tag(skb, dev, dsa_etype_header_pos_rx(skb))))
		return NULL;

	 
	skb_pull_rcsum(skb, RTL8_4_TAG_LEN);

	dsa_strip_etype_header(skb, RTL8_4_TAG_LEN);

	return skb;
}

static struct sk_buff *rtl8_4t_tag_rcv(struct sk_buff *skb,
				       struct net_device *dev)
{
	if (skb_linearize(skb))
		return NULL;

	if (unlikely(rtl8_4_read_tag(skb, dev, skb_tail_pointer(skb) - RTL8_4_TAG_LEN)))
		return NULL;

	if (pskb_trim_rcsum(skb, skb->len - RTL8_4_TAG_LEN))
		return NULL;

	return skb;
}

 
static const struct dsa_device_ops rtl8_4_netdev_ops = {
	.name = "rtl8_4",
	.proto = DSA_TAG_PROTO_RTL8_4,
	.xmit = rtl8_4_tag_xmit,
	.rcv = rtl8_4_tag_rcv,
	.needed_headroom = RTL8_4_TAG_LEN,
};

DSA_TAG_DRIVER(rtl8_4_netdev_ops);

MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_RTL8_4, RTL8_4_NAME);

 
static const struct dsa_device_ops rtl8_4t_netdev_ops = {
	.name = "rtl8_4t",
	.proto = DSA_TAG_PROTO_RTL8_4T,
	.xmit = rtl8_4t_tag_xmit,
	.rcv = rtl8_4t_tag_rcv,
	.needed_tailroom = RTL8_4_TAG_LEN,
};

DSA_TAG_DRIVER(rtl8_4t_netdev_ops);

MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_RTL8_4T, RTL8_4T_NAME);

static struct dsa_tag_driver *dsa_tag_drivers[] = {
	&DSA_TAG_DRIVER_NAME(rtl8_4_netdev_ops),
	&DSA_TAG_DRIVER_NAME(rtl8_4t_netdev_ops),
};
module_dsa_tag_drivers(dsa_tag_drivers);

MODULE_LICENSE("GPL");
