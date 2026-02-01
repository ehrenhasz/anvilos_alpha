 

#ifndef __DSA_TAG_H
#define __DSA_TAG_H

#include <linux/if_vlan.h>
#include <linux/list.h>
#include <linux/types.h>
#include <net/dsa.h>

#include "port.h"
#include "slave.h"

struct dsa_tag_driver {
	const struct dsa_device_ops *ops;
	struct list_head list;
	struct module *owner;
};

extern struct packet_type dsa_pack_type;

const struct dsa_device_ops *dsa_tag_driver_get_by_id(int tag_protocol);
const struct dsa_device_ops *dsa_tag_driver_get_by_name(const char *name);
void dsa_tag_driver_put(const struct dsa_device_ops *ops);
const char *dsa_tag_protocol_to_str(const struct dsa_device_ops *ops);

static inline int dsa_tag_protocol_overhead(const struct dsa_device_ops *ops)
{
	return ops->needed_headroom + ops->needed_tailroom;
}

static inline struct net_device *dsa_master_find_slave(struct net_device *dev,
						       int device, int port)
{
	struct dsa_port *cpu_dp = dev->dsa_ptr;
	struct dsa_switch_tree *dst = cpu_dp->dst;
	struct dsa_port *dp;

	list_for_each_entry(dp, &dst->ports, list)
		if (dp->ds->index == device && dp->index == port &&
		    dp->type == DSA_PORT_TYPE_USER)
			return dp->slave;

	return NULL;
}

 
static inline struct sk_buff *dsa_untag_bridge_pvid(struct sk_buff *skb)
{
	struct dsa_port *dp = dsa_slave_to_port(skb->dev);
	struct net_device *br = dsa_port_bridge_dev_get(dp);
	struct net_device *dev = skb->dev;
	struct net_device *upper_dev;
	u16 vid, pvid, proto;
	int err;

	if (!br || br_vlan_enabled(br))
		return skb;

	err = br_vlan_get_proto(br, &proto);
	if (err)
		return skb;

	 
	if (!skb_vlan_tag_present(skb) && skb->protocol == htons(proto)) {
		skb = skb_vlan_untag(skb);
		if (!skb)
			return NULL;
	}

	if (!skb_vlan_tag_present(skb))
		return skb;

	vid = skb_vlan_tag_get_id(skb);

	 
	err = br_vlan_get_pvid_rcu(dev, &pvid);
	if (err)
		return skb;

	if (vid != pvid)
		return skb;

	 
	upper_dev = __vlan_find_dev_deep_rcu(br, htons(proto), vid);
	if (upper_dev)
		return skb;

	__vlan_hwaccel_clear_tag(skb);

	return skb;
}

 
static inline struct net_device *
dsa_find_designated_bridge_port_by_vid(struct net_device *master, u16 vid)
{
	struct dsa_port *cpu_dp = master->dsa_ptr;
	struct dsa_switch_tree *dst = cpu_dp->dst;
	struct bridge_vlan_info vinfo;
	struct net_device *slave;
	struct dsa_port *dp;
	int err;

	list_for_each_entry(dp, &dst->ports, list) {
		if (dp->type != DSA_PORT_TYPE_USER)
			continue;

		if (!dp->bridge)
			continue;

		if (dp->stp_state != BR_STATE_LEARNING &&
		    dp->stp_state != BR_STATE_FORWARDING)
			continue;

		 
		if (dp->cpu_dp != cpu_dp)
			continue;

		slave = dp->slave;

		err = br_vlan_get_info_rcu(slave, vid, &vinfo);
		if (err)
			continue;

		return slave;
	}

	return NULL;
}

 
static inline void dsa_default_offload_fwd_mark(struct sk_buff *skb)
{
	struct dsa_port *dp = dsa_slave_to_port(skb->dev);

	skb->offload_fwd_mark = !!(dp->bridge);
}

 
static inline void dsa_strip_etype_header(struct sk_buff *skb, int len)
{
	memmove(skb->data - ETH_HLEN, skb->data - ETH_HLEN - len, 2 * ETH_ALEN);
}

 
static inline void dsa_alloc_etype_header(struct sk_buff *skb, int len)
{
	memmove(skb->data, skb->data + len, 2 * ETH_ALEN);
}

 
static inline void *dsa_etype_header_pos_rx(struct sk_buff *skb)
{
	return skb->data - 2;
}

 
static inline void *dsa_etype_header_pos_tx(struct sk_buff *skb)
{
	return skb->data + 2 * ETH_ALEN;
}

 
#define DSA_TAG_DRIVER_ALIAS "dsa_tag:"
#define MODULE_ALIAS_DSA_TAG_DRIVER(__proto, __name) \
	MODULE_ALIAS(DSA_TAG_DRIVER_ALIAS __name); \
	MODULE_ALIAS(DSA_TAG_DRIVER_ALIAS "id-" \
		     __stringify(__proto##_VALUE))

void dsa_tag_drivers_register(struct dsa_tag_driver *dsa_tag_driver_array[],
			      unsigned int count,
			      struct module *owner);
void dsa_tag_drivers_unregister(struct dsa_tag_driver *dsa_tag_driver_array[],
				unsigned int count);

#define dsa_tag_driver_module_drivers(__dsa_tag_drivers_array, __count)	\
static int __init dsa_tag_driver_module_init(void)			\
{									\
	dsa_tag_drivers_register(__dsa_tag_drivers_array, __count,	\
				 THIS_MODULE);				\
	return 0;							\
}									\
module_init(dsa_tag_driver_module_init);				\
									\
static void __exit dsa_tag_driver_module_exit(void)			\
{									\
	dsa_tag_drivers_unregister(__dsa_tag_drivers_array, __count);	\
}									\
module_exit(dsa_tag_driver_module_exit)

 
#define module_dsa_tag_drivers(__ops_array)				\
dsa_tag_driver_module_drivers(__ops_array, ARRAY_SIZE(__ops_array))

#define DSA_TAG_DRIVER_NAME(__ops) dsa_tag_driver ## _ ## __ops

 
#define DSA_TAG_DRIVER(__ops)						\
static struct dsa_tag_driver DSA_TAG_DRIVER_NAME(__ops) = {		\
	.ops = &__ops,							\
}

 
#define module_dsa_tag_driver(__ops)					\
DSA_TAG_DRIVER(__ops);							\
									\
static struct dsa_tag_driver *dsa_tag_driver_array[] =	{		\
	&DSA_TAG_DRIVER_NAME(__ops)					\
};									\
module_dsa_tag_drivers(dsa_tag_driver_array)

#endif
