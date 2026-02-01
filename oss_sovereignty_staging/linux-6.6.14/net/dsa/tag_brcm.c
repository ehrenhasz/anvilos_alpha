
 

#include <linux/dsa/brcm.h>
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>
#include <linux/list.h>
#include <linux/slab.h>

#include "tag.h"

#define BRCM_NAME		"brcm"
#define BRCM_LEGACY_NAME	"brcm-legacy"
#define BRCM_PREPEND_NAME	"brcm-prepend"

 
#define BRCM_LEG_TAG_LEN	6

 
 
#define BRCM_LEG_TYPE_HI	0x88
 
#define BRCM_LEG_TYPE_LO	0x74

 
 
#define BRCM_LEG_UNICAST	(0 << 5)
#define BRCM_LEG_MULTICAST	(1 << 5)
#define BRCM_LEG_EGRESS		(2 << 5)
#define BRCM_LEG_INGRESS	(3 << 5)

 
#define BRCM_LEG_PORT_ID	(0xf)

 
#define BRCM_TAG_LEN	4

 

 
#define BRCM_OPCODE_SHIFT	5
#define BRCM_OPCODE_MASK	0x7

 
 
#define BRCM_IG_TC_SHIFT	2
#define BRCM_IG_TC_MASK		0x7
 
#define BRCM_IG_TE_MASK		0x3
#define BRCM_IG_TS_SHIFT	7
 
#define BRCM_IG_DSTMAP2_MASK	1
#define BRCM_IG_DSTMAP1_MASK	0xff

 

 
#define BRCM_EG_CID_MASK	0xff

 
#define BRCM_EG_RC_MASK		0xff
#define  BRCM_EG_RC_RSVD	(3 << 6)
#define  BRCM_EG_RC_EXCEPTION	(1 << 5)
#define  BRCM_EG_RC_PROT_SNOOP	(1 << 4)
#define  BRCM_EG_RC_PROT_TERM	(1 << 3)
#define  BRCM_EG_RC_SWITCH	(1 << 2)
#define  BRCM_EG_RC_MAC_LEARN	(1 << 1)
#define  BRCM_EG_RC_MIRROR	(1 << 0)
#define BRCM_EG_TC_SHIFT	5
#define BRCM_EG_TC_MASK		0x7
#define BRCM_EG_PID_MASK	0x1f

#if IS_ENABLED(CONFIG_NET_DSA_TAG_BRCM) || \
	IS_ENABLED(CONFIG_NET_DSA_TAG_BRCM_PREPEND)

static struct sk_buff *brcm_tag_xmit_ll(struct sk_buff *skb,
					struct net_device *dev,
					unsigned int offset)
{
	struct dsa_port *dp = dsa_slave_to_port(dev);
	u16 queue = skb_get_queue_mapping(skb);
	u8 *brcm_tag;

	 
	if (__skb_put_padto(skb, ETH_ZLEN + BRCM_TAG_LEN, false))
		return NULL;

	skb_push(skb, BRCM_TAG_LEN);

	if (offset)
		dsa_alloc_etype_header(skb, BRCM_TAG_LEN);

	brcm_tag = skb->data + offset;

	 
	brcm_tag[0] = (1 << BRCM_OPCODE_SHIFT) |
		       ((queue & BRCM_IG_TC_MASK) << BRCM_IG_TC_SHIFT);
	brcm_tag[1] = 0;
	brcm_tag[2] = 0;
	if (dp->index == 8)
		brcm_tag[2] = BRCM_IG_DSTMAP2_MASK;
	brcm_tag[3] = (1 << dp->index) & BRCM_IG_DSTMAP1_MASK;

	 
	skb_set_queue_mapping(skb, BRCM_TAG_SET_PORT_QUEUE(dp->index, queue));

	return skb;
}

 
static struct sk_buff *brcm_tag_rcv_ll(struct sk_buff *skb,
				       struct net_device *dev,
				       unsigned int offset)
{
	int source_port;
	u8 *brcm_tag;

	if (unlikely(!pskb_may_pull(skb, BRCM_TAG_LEN)))
		return NULL;

	brcm_tag = skb->data - offset;

	 
	if (unlikely((brcm_tag[0] >> BRCM_OPCODE_SHIFT) & BRCM_OPCODE_MASK))
		return NULL;

	 
	if (unlikely(brcm_tag[2] & BRCM_EG_RC_RSVD))
		return NULL;

	 
	source_port = brcm_tag[3] & BRCM_EG_PID_MASK;

	skb->dev = dsa_master_find_slave(dev, 0, source_port);
	if (!skb->dev)
		return NULL;

	 
	skb_pull_rcsum(skb, BRCM_TAG_LEN);

	dsa_default_offload_fwd_mark(skb);

	return skb;
}
#endif

#if IS_ENABLED(CONFIG_NET_DSA_TAG_BRCM)
static struct sk_buff *brcm_tag_xmit(struct sk_buff *skb,
				     struct net_device *dev)
{
	 
	return brcm_tag_xmit_ll(skb, dev, 2 * ETH_ALEN);
}


static struct sk_buff *brcm_tag_rcv(struct sk_buff *skb, struct net_device *dev)
{
	struct sk_buff *nskb;

	 
	nskb = brcm_tag_rcv_ll(skb, dev, 2);
	if (!nskb)
		return nskb;

	dsa_strip_etype_header(skb, BRCM_TAG_LEN);

	return nskb;
}

static const struct dsa_device_ops brcm_netdev_ops = {
	.name	= BRCM_NAME,
	.proto	= DSA_TAG_PROTO_BRCM,
	.xmit	= brcm_tag_xmit,
	.rcv	= brcm_tag_rcv,
	.needed_headroom = BRCM_TAG_LEN,
};

DSA_TAG_DRIVER(brcm_netdev_ops);
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_BRCM, BRCM_NAME);
#endif

#if IS_ENABLED(CONFIG_NET_DSA_TAG_BRCM_LEGACY)
static struct sk_buff *brcm_leg_tag_xmit(struct sk_buff *skb,
					 struct net_device *dev)
{
	struct dsa_port *dp = dsa_slave_to_port(dev);
	u8 *brcm_tag;

	 
	if (__skb_put_padto(skb, ETH_ZLEN + BRCM_LEG_TAG_LEN, false))
		return NULL;

	skb_push(skb, BRCM_LEG_TAG_LEN);

	dsa_alloc_etype_header(skb, BRCM_LEG_TAG_LEN);

	brcm_tag = skb->data + 2 * ETH_ALEN;

	 
	brcm_tag[0] = BRCM_LEG_TYPE_HI;
	brcm_tag[1] = BRCM_LEG_TYPE_LO;

	 
	brcm_tag[2] = BRCM_LEG_EGRESS;
	brcm_tag[3] = 0;
	brcm_tag[4] = 0;
	brcm_tag[5] = dp->index & BRCM_LEG_PORT_ID;

	return skb;
}

static struct sk_buff *brcm_leg_tag_rcv(struct sk_buff *skb,
					struct net_device *dev)
{
	int len = BRCM_LEG_TAG_LEN;
	int source_port;
	u8 *brcm_tag;

	if (unlikely(!pskb_may_pull(skb, BRCM_LEG_PORT_ID)))
		return NULL;

	brcm_tag = dsa_etype_header_pos_rx(skb);

	source_port = brcm_tag[5] & BRCM_LEG_PORT_ID;

	skb->dev = dsa_master_find_slave(dev, 0, source_port);
	if (!skb->dev)
		return NULL;

	 
	if (netdev_uses_dsa(skb->dev))
		len += VLAN_HLEN;

	 
	skb_pull_rcsum(skb, len);

	dsa_default_offload_fwd_mark(skb);

	dsa_strip_etype_header(skb, len);

	return skb;
}

static const struct dsa_device_ops brcm_legacy_netdev_ops = {
	.name = BRCM_LEGACY_NAME,
	.proto = DSA_TAG_PROTO_BRCM_LEGACY,
	.xmit = brcm_leg_tag_xmit,
	.rcv = brcm_leg_tag_rcv,
	.needed_headroom = BRCM_LEG_TAG_LEN,
};

DSA_TAG_DRIVER(brcm_legacy_netdev_ops);
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_BRCM_LEGACY, BRCM_LEGACY_NAME);
#endif  

#if IS_ENABLED(CONFIG_NET_DSA_TAG_BRCM_PREPEND)
static struct sk_buff *brcm_tag_xmit_prepend(struct sk_buff *skb,
					     struct net_device *dev)
{
	 
	return brcm_tag_xmit_ll(skb, dev, 0);
}

static struct sk_buff *brcm_tag_rcv_prepend(struct sk_buff *skb,
					    struct net_device *dev)
{
	 
	return brcm_tag_rcv_ll(skb, dev, ETH_HLEN);
}

static const struct dsa_device_ops brcm_prepend_netdev_ops = {
	.name	= BRCM_PREPEND_NAME,
	.proto	= DSA_TAG_PROTO_BRCM_PREPEND,
	.xmit	= brcm_tag_xmit_prepend,
	.rcv	= brcm_tag_rcv_prepend,
	.needed_headroom = BRCM_TAG_LEN,
};

DSA_TAG_DRIVER(brcm_prepend_netdev_ops);
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_BRCM_PREPEND, BRCM_PREPEND_NAME);
#endif

static struct dsa_tag_driver *dsa_tag_driver_array[] =	{
#if IS_ENABLED(CONFIG_NET_DSA_TAG_BRCM)
	&DSA_TAG_DRIVER_NAME(brcm_netdev_ops),
#endif
#if IS_ENABLED(CONFIG_NET_DSA_TAG_BRCM_LEGACY)
	&DSA_TAG_DRIVER_NAME(brcm_legacy_netdev_ops),
#endif
#if IS_ENABLED(CONFIG_NET_DSA_TAG_BRCM_PREPEND)
	&DSA_TAG_DRIVER_NAME(brcm_prepend_netdev_ops),
#endif
};

module_dsa_tag_drivers(dsa_tag_driver_array);

MODULE_LICENSE("GPL");
