
 

#include <linux/dsa/mv88e6xxx.h>
#include <linux/etherdevice.h>
#include <linux/list.h>
#include <linux/slab.h>

#include "tag.h"

#define DSA_NAME	"dsa"
#define EDSA_NAME	"edsa"

#define DSA_HLEN	4

 
enum dsa_cmd {
	DSA_CMD_TO_CPU     = 0,
	DSA_CMD_FROM_CPU   = 1,
	DSA_CMD_TO_SNIFFER = 2,
	DSA_CMD_FORWARD    = 3
};

 
enum dsa_code {
	DSA_CODE_MGMT_TRAP     = 0,
	DSA_CODE_FRAME2REG     = 1,
	DSA_CODE_IGMP_MLD_TRAP = 2,
	DSA_CODE_POLICY_TRAP   = 3,
	DSA_CODE_ARP_MIRROR    = 4,
	DSA_CODE_POLICY_MIRROR = 5,
	DSA_CODE_RESERVED_6    = 6,
	DSA_CODE_RESERVED_7    = 7
};

static struct sk_buff *dsa_xmit_ll(struct sk_buff *skb, struct net_device *dev,
				   u8 extra)
{
	struct dsa_port *dp = dsa_slave_to_port(dev);
	struct net_device *br_dev;
	u8 tag_dev, tag_port;
	enum dsa_cmd cmd;
	u8 *dsa_header;

	if (skb->offload_fwd_mark) {
		unsigned int bridge_num = dsa_port_bridge_num_get(dp);
		struct dsa_switch_tree *dst = dp->ds->dst;

		cmd = DSA_CMD_FORWARD;

		 
		tag_dev = dst->last_switch + bridge_num;
		tag_port = 0;
	} else {
		cmd = DSA_CMD_FROM_CPU;
		tag_dev = dp->ds->index;
		tag_port = dp->index;
	}

	br_dev = dsa_port_bridge_dev_get(dp);

	 
	if (skb->protocol == htons(ETH_P_8021Q) &&
	    (!br_dev || br_vlan_enabled(br_dev))) {
		if (extra) {
			skb_push(skb, extra);
			dsa_alloc_etype_header(skb, extra);
		}

		 
		dsa_header = dsa_etype_header_pos_tx(skb) + extra;
		dsa_header[0] = (cmd << 6) | 0x20 | tag_dev;
		dsa_header[1] = tag_port << 3;

		 
		if (dsa_header[2] & 0x10) {
			dsa_header[1] |= 0x01;
			dsa_header[2] &= ~0x10;
		}
	} else {
		u16 vid;

		vid = br_dev ? MV88E6XXX_VID_BRIDGED : MV88E6XXX_VID_STANDALONE;

		skb_push(skb, DSA_HLEN + extra);
		dsa_alloc_etype_header(skb, DSA_HLEN + extra);

		 
		dsa_header = dsa_etype_header_pos_tx(skb) + extra;

		dsa_header[0] = (cmd << 6) | tag_dev;
		dsa_header[1] = tag_port << 3;
		dsa_header[2] = vid >> 8;
		dsa_header[3] = vid & 0xff;
	}

	return skb;
}

static struct sk_buff *dsa_rcv_ll(struct sk_buff *skb, struct net_device *dev,
				  u8 extra)
{
	bool trap = false, trunk = false;
	int source_device, source_port;
	enum dsa_code code;
	enum dsa_cmd cmd;
	u8 *dsa_header;

	 
	dsa_header = dsa_etype_header_pos_rx(skb);

	cmd = dsa_header[0] >> 6;
	switch (cmd) {
	case DSA_CMD_FORWARD:
		trunk = !!(dsa_header[1] & 4);
		break;

	case DSA_CMD_TO_CPU:
		code = (dsa_header[1] & 0x6) | ((dsa_header[2] >> 4) & 1);

		switch (code) {
		case DSA_CODE_FRAME2REG:
			 
			return NULL;
		case DSA_CODE_ARP_MIRROR:
		case DSA_CODE_POLICY_MIRROR:
			 
			break;
		case DSA_CODE_MGMT_TRAP:
		case DSA_CODE_IGMP_MLD_TRAP:
		case DSA_CODE_POLICY_TRAP:
			 
			trap = true;
			break;
		default:
			 
			return NULL;
		}

		break;

	default:
		return NULL;
	}

	source_device = dsa_header[0] & 0x1f;
	source_port = (dsa_header[1] >> 3) & 0x1f;

	if (trunk) {
		struct dsa_port *cpu_dp = dev->dsa_ptr;
		struct dsa_lag *lag;

		 
		lag = dsa_lag_by_id(cpu_dp->dst, source_port + 1);
		skb->dev = lag ? lag->dev : NULL;
	} else {
		skb->dev = dsa_master_find_slave(dev, source_device,
						 source_port);
	}

	if (!skb->dev)
		return NULL;

	 
	if (trunk)
		skb->offload_fwd_mark = true;
	else if (!trap)
		dsa_default_offload_fwd_mark(skb);

	 
	if (dsa_header[0] & 0x20) {
		u8 new_header[4];

		 
		new_header[0] = (ETH_P_8021Q >> 8) & 0xff;
		new_header[1] = ETH_P_8021Q & 0xff;
		new_header[2] = dsa_header[2] & ~0x10;
		new_header[3] = dsa_header[3];

		 
		if (dsa_header[1] & 0x01)
			new_header[2] |= 0x10;

		 
		if (skb->ip_summed == CHECKSUM_COMPLETE) {
			__wsum c = skb->csum;
			c = csum_add(c, csum_partial(new_header + 2, 2, 0));
			c = csum_sub(c, csum_partial(dsa_header + 2, 2, 0));
			skb->csum = c;
		}

		memcpy(dsa_header, new_header, DSA_HLEN);

		if (extra)
			dsa_strip_etype_header(skb, extra);
	} else {
		skb_pull_rcsum(skb, DSA_HLEN);
		dsa_strip_etype_header(skb, DSA_HLEN + extra);
	}

	return skb;
}

#if IS_ENABLED(CONFIG_NET_DSA_TAG_DSA)

static struct sk_buff *dsa_xmit(struct sk_buff *skb, struct net_device *dev)
{
	return dsa_xmit_ll(skb, dev, 0);
}

static struct sk_buff *dsa_rcv(struct sk_buff *skb, struct net_device *dev)
{
	if (unlikely(!pskb_may_pull(skb, DSA_HLEN)))
		return NULL;

	return dsa_rcv_ll(skb, dev, 0);
}

static const struct dsa_device_ops dsa_netdev_ops = {
	.name	  = DSA_NAME,
	.proto	  = DSA_TAG_PROTO_DSA,
	.xmit	  = dsa_xmit,
	.rcv	  = dsa_rcv,
	.needed_headroom = DSA_HLEN,
};

DSA_TAG_DRIVER(dsa_netdev_ops);
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_DSA, DSA_NAME);
#endif	 

#if IS_ENABLED(CONFIG_NET_DSA_TAG_EDSA)

#define EDSA_HLEN 8

static struct sk_buff *edsa_xmit(struct sk_buff *skb, struct net_device *dev)
{
	u8 *edsa_header;

	skb = dsa_xmit_ll(skb, dev, EDSA_HLEN - DSA_HLEN);
	if (!skb)
		return NULL;

	edsa_header = dsa_etype_header_pos_tx(skb);
	edsa_header[0] = (ETH_P_EDSA >> 8) & 0xff;
	edsa_header[1] = ETH_P_EDSA & 0xff;
	edsa_header[2] = 0x00;
	edsa_header[3] = 0x00;
	return skb;
}

static struct sk_buff *edsa_rcv(struct sk_buff *skb, struct net_device *dev)
{
	if (unlikely(!pskb_may_pull(skb, EDSA_HLEN)))
		return NULL;

	skb_pull_rcsum(skb, EDSA_HLEN - DSA_HLEN);

	return dsa_rcv_ll(skb, dev, EDSA_HLEN - DSA_HLEN);
}

static const struct dsa_device_ops edsa_netdev_ops = {
	.name	  = EDSA_NAME,
	.proto	  = DSA_TAG_PROTO_EDSA,
	.xmit	  = edsa_xmit,
	.rcv	  = edsa_rcv,
	.needed_headroom = EDSA_HLEN,
};

DSA_TAG_DRIVER(edsa_netdev_ops);
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_EDSA, EDSA_NAME);
#endif	 

static struct dsa_tag_driver *dsa_tag_drivers[] = {
#if IS_ENABLED(CONFIG_NET_DSA_TAG_DSA)
	&DSA_TAG_DRIVER_NAME(dsa_netdev_ops),
#endif
#if IS_ENABLED(CONFIG_NET_DSA_TAG_EDSA)
	&DSA_TAG_DRIVER_NAME(edsa_netdev_ops),
#endif
};

module_dsa_tag_drivers(dsa_tag_drivers);

MODULE_LICENSE("GPL");
