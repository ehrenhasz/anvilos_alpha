
 

#include <linux/etherdevice.h>
#include <linux/bits.h>

#include "tag.h"

#define RTL4_A_NAME		"rtl4a"

#define RTL4_A_HDR_LEN		4
#define RTL4_A_ETHERTYPE	0x8899
#define RTL4_A_PROTOCOL_SHIFT	12
 
#define RTL4_A_PROTOCOL_RTL8366RB	0xa

static struct sk_buff *rtl4a_tag_xmit(struct sk_buff *skb,
				      struct net_device *dev)
{
	struct dsa_port *dp = dsa_slave_to_port(dev);
	__be16 *p;
	u8 *tag;
	u16 out;

	 
	if (unlikely(__skb_put_padto(skb, ETH_ZLEN, false)))
		return NULL;

	netdev_dbg(dev, "add realtek tag to package to port %d\n",
		   dp->index);
	skb_push(skb, RTL4_A_HDR_LEN);

	dsa_alloc_etype_header(skb, RTL4_A_HDR_LEN);
	tag = dsa_etype_header_pos_tx(skb);

	 
	p = (__be16 *)tag;
	*p = htons(RTL4_A_ETHERTYPE);

	out = (RTL4_A_PROTOCOL_RTL8366RB << RTL4_A_PROTOCOL_SHIFT);
	 
	out |= BIT(dp->index);

	p = (__be16 *)(tag + 2);
	*p = htons(out);

	return skb;
}

static struct sk_buff *rtl4a_tag_rcv(struct sk_buff *skb,
				     struct net_device *dev)
{
	u16 protport;
	__be16 *p;
	u16 etype;
	u8 *tag;
	u8 prot;
	u8 port;

	if (unlikely(!pskb_may_pull(skb, RTL4_A_HDR_LEN)))
		return NULL;

	tag = dsa_etype_header_pos_rx(skb);
	p = (__be16 *)tag;
	etype = ntohs(*p);
	if (etype != RTL4_A_ETHERTYPE) {
		 
		netdev_dbg(dev, "non-realtek ethertype 0x%04x\n", etype);
		return skb;
	}
	p = (__be16 *)(tag + 2);
	protport = ntohs(*p);
	 
	prot = (protport >> RTL4_A_PROTOCOL_SHIFT) & 0x0f;
	if (prot != RTL4_A_PROTOCOL_RTL8366RB) {
		netdev_err(dev, "unknown realtek protocol 0x%01x\n", prot);
		return NULL;
	}
	port = protport & 0xff;

	skb->dev = dsa_master_find_slave(dev, 0, port);
	if (!skb->dev) {
		netdev_dbg(dev, "could not find slave for port %d\n", port);
		return NULL;
	}

	 
	skb_pull_rcsum(skb, RTL4_A_HDR_LEN);

	dsa_strip_etype_header(skb, RTL4_A_HDR_LEN);

	dsa_default_offload_fwd_mark(skb);

	return skb;
}

static const struct dsa_device_ops rtl4a_netdev_ops = {
	.name	= RTL4_A_NAME,
	.proto	= DSA_TAG_PROTO_RTL4_A,
	.xmit	= rtl4a_tag_xmit,
	.rcv	= rtl4a_tag_rcv,
	.needed_headroom = RTL4_A_HDR_LEN,
};
module_dsa_tag_driver(rtl4a_netdev_ops);

MODULE_LICENSE("GPL");
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_RTL4_A, RTL4_A_NAME);
