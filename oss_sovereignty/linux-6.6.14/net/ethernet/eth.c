
 
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/ip.h>
#include <linux/netdevice.h>
#include <linux/nvmem-consumer.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/if_ether.h>
#include <linux/of_net.h>
#include <linux/pci.h>
#include <linux/property.h>
#include <net/dst.h>
#include <net/arp.h>
#include <net/sock.h>
#include <net/ipv6.h>
#include <net/ip.h>
#include <net/dsa.h>
#include <net/flow_dissector.h>
#include <net/gro.h>
#include <linux/uaccess.h>
#include <net/pkt_sched.h>

 
int eth_header(struct sk_buff *skb, struct net_device *dev,
	       unsigned short type,
	       const void *daddr, const void *saddr, unsigned int len)
{
	struct ethhdr *eth = skb_push(skb, ETH_HLEN);

	if (type != ETH_P_802_3 && type != ETH_P_802_2)
		eth->h_proto = htons(type);
	else
		eth->h_proto = htons(len);

	 

	if (!saddr)
		saddr = dev->dev_addr;
	memcpy(eth->h_source, saddr, ETH_ALEN);

	if (daddr) {
		memcpy(eth->h_dest, daddr, ETH_ALEN);
		return ETH_HLEN;
	}

	 

	if (dev->flags & (IFF_LOOPBACK | IFF_NOARP)) {
		eth_zero_addr(eth->h_dest);
		return ETH_HLEN;
	}

	return -ETH_HLEN;
}
EXPORT_SYMBOL(eth_header);

 
u32 eth_get_headlen(const struct net_device *dev, const void *data, u32 len)
{
	const unsigned int flags = FLOW_DISSECTOR_F_PARSE_1ST_FRAG;
	const struct ethhdr *eth = (const struct ethhdr *)data;
	struct flow_keys_basic keys;

	 
	if (unlikely(len < sizeof(*eth)))
		return len;

	 
	if (!skb_flow_dissect_flow_keys_basic(dev_net(dev), NULL, &keys, data,
					      eth->h_proto, sizeof(*eth),
					      len, flags))
		return max_t(u32, keys.control.thoff, sizeof(*eth));

	 
	return min_t(u32, __skb_get_poff(NULL, data, &keys, len), len);
}
EXPORT_SYMBOL(eth_get_headlen);

 
__be16 eth_type_trans(struct sk_buff *skb, struct net_device *dev)
{
	unsigned short _service_access_point;
	const unsigned short *sap;
	const struct ethhdr *eth;

	skb->dev = dev;
	skb_reset_mac_header(skb);

	eth = (struct ethhdr *)skb->data;
	skb_pull_inline(skb, ETH_HLEN);

	if (unlikely(!ether_addr_equal_64bits(eth->h_dest,
					      dev->dev_addr))) {
		if (unlikely(is_multicast_ether_addr_64bits(eth->h_dest))) {
			if (ether_addr_equal_64bits(eth->h_dest, dev->broadcast))
				skb->pkt_type = PACKET_BROADCAST;
			else
				skb->pkt_type = PACKET_MULTICAST;
		} else {
			skb->pkt_type = PACKET_OTHERHOST;
		}
	}

	 
	if (unlikely(netdev_uses_dsa(dev)))
		return htons(ETH_P_XDSA);

	if (likely(eth_proto_is_802_3(eth->h_proto)))
		return eth->h_proto;

	 
	sap = skb_header_pointer(skb, 0, sizeof(*sap), &_service_access_point);
	if (sap && *sap == 0xFFFF)
		return htons(ETH_P_802_3);

	 
	return htons(ETH_P_802_2);
}
EXPORT_SYMBOL(eth_type_trans);

 
int eth_header_parse(const struct sk_buff *skb, unsigned char *haddr)
{
	const struct ethhdr *eth = eth_hdr(skb);
	memcpy(haddr, eth->h_source, ETH_ALEN);
	return ETH_ALEN;
}
EXPORT_SYMBOL(eth_header_parse);

 
int eth_header_cache(const struct neighbour *neigh, struct hh_cache *hh, __be16 type)
{
	struct ethhdr *eth;
	const struct net_device *dev = neigh->dev;

	eth = (struct ethhdr *)
	    (((u8 *) hh->hh_data) + (HH_DATA_OFF(sizeof(*eth))));

	if (type == htons(ETH_P_802_3))
		return -1;

	eth->h_proto = type;
	memcpy(eth->h_source, dev->dev_addr, ETH_ALEN);
	memcpy(eth->h_dest, neigh->ha, ETH_ALEN);

	 
	smp_store_release(&hh->hh_len, ETH_HLEN);

	return 0;
}
EXPORT_SYMBOL(eth_header_cache);

 
void eth_header_cache_update(struct hh_cache *hh,
			     const struct net_device *dev,
			     const unsigned char *haddr)
{
	memcpy(((u8 *) hh->hh_data) + HH_DATA_OFF(sizeof(struct ethhdr)),
	       haddr, ETH_ALEN);
}
EXPORT_SYMBOL(eth_header_cache_update);

 
__be16 eth_header_parse_protocol(const struct sk_buff *skb)
{
	const struct ethhdr *eth = eth_hdr(skb);

	return eth->h_proto;
}
EXPORT_SYMBOL(eth_header_parse_protocol);

 
int eth_prepare_mac_addr_change(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;

	if (!(dev->priv_flags & IFF_LIVE_ADDR_CHANGE) && netif_running(dev))
		return -EBUSY;
	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;
	return 0;
}
EXPORT_SYMBOL(eth_prepare_mac_addr_change);

 
void eth_commit_mac_addr_change(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;

	eth_hw_addr_set(dev, addr->sa_data);
}
EXPORT_SYMBOL(eth_commit_mac_addr_change);

 
int eth_mac_addr(struct net_device *dev, void *p)
{
	int ret;

	ret = eth_prepare_mac_addr_change(dev, p);
	if (ret < 0)
		return ret;
	eth_commit_mac_addr_change(dev, p);
	return 0;
}
EXPORT_SYMBOL(eth_mac_addr);

int eth_validate_addr(struct net_device *dev)
{
	if (!is_valid_ether_addr(dev->dev_addr))
		return -EADDRNOTAVAIL;

	return 0;
}
EXPORT_SYMBOL(eth_validate_addr);

const struct header_ops eth_header_ops ____cacheline_aligned = {
	.create		= eth_header,
	.parse		= eth_header_parse,
	.cache		= eth_header_cache,
	.cache_update	= eth_header_cache_update,
	.parse_protocol	= eth_header_parse_protocol,
};

 
void ether_setup(struct net_device *dev)
{
	dev->header_ops		= &eth_header_ops;
	dev->type		= ARPHRD_ETHER;
	dev->hard_header_len 	= ETH_HLEN;
	dev->min_header_len	= ETH_HLEN;
	dev->mtu		= ETH_DATA_LEN;
	dev->min_mtu		= ETH_MIN_MTU;
	dev->max_mtu		= ETH_DATA_LEN;
	dev->addr_len		= ETH_ALEN;
	dev->tx_queue_len	= DEFAULT_TX_QUEUE_LEN;
	dev->flags		= IFF_BROADCAST|IFF_MULTICAST;
	dev->priv_flags		|= IFF_TX_SKB_SHARING;

	eth_broadcast_addr(dev->broadcast);

}
EXPORT_SYMBOL(ether_setup);

 

struct net_device *alloc_etherdev_mqs(int sizeof_priv, unsigned int txqs,
				      unsigned int rxqs)
{
	return alloc_netdev_mqs(sizeof_priv, "eth%d", NET_NAME_ENUM,
				ether_setup, txqs, rxqs);
}
EXPORT_SYMBOL(alloc_etherdev_mqs);

ssize_t sysfs_format_mac(char *buf, const unsigned char *addr, int len)
{
	return sysfs_emit(buf, "%*phC\n", len, addr);
}
EXPORT_SYMBOL(sysfs_format_mac);

struct sk_buff *eth_gro_receive(struct list_head *head, struct sk_buff *skb)
{
	const struct packet_offload *ptype;
	unsigned int hlen, off_eth;
	struct sk_buff *pp = NULL;
	struct ethhdr *eh, *eh2;
	struct sk_buff *p;
	__be16 type;
	int flush = 1;

	off_eth = skb_gro_offset(skb);
	hlen = off_eth + sizeof(*eh);
	eh = skb_gro_header(skb, hlen, off_eth);
	if (unlikely(!eh))
		goto out;

	flush = 0;

	list_for_each_entry(p, head, list) {
		if (!NAPI_GRO_CB(p)->same_flow)
			continue;

		eh2 = (struct ethhdr *)(p->data + off_eth);
		if (compare_ether_header(eh, eh2)) {
			NAPI_GRO_CB(p)->same_flow = 0;
			continue;
		}
	}

	type = eh->h_proto;

	ptype = gro_find_receive_by_type(type);
	if (ptype == NULL) {
		flush = 1;
		goto out;
	}

	skb_gro_pull(skb, sizeof(*eh));
	skb_gro_postpull_rcsum(skb, eh, sizeof(*eh));

	pp = indirect_call_gro_receive_inet(ptype->callbacks.gro_receive,
					    ipv6_gro_receive, inet_gro_receive,
					    head, skb);

out:
	skb_gro_flush_final(skb, pp, flush);

	return pp;
}
EXPORT_SYMBOL(eth_gro_receive);

int eth_gro_complete(struct sk_buff *skb, int nhoff)
{
	struct ethhdr *eh = (struct ethhdr *)(skb->data + nhoff);
	__be16 type = eh->h_proto;
	struct packet_offload *ptype;
	int err = -ENOSYS;

	if (skb->encapsulation)
		skb_set_inner_mac_header(skb, nhoff);

	ptype = gro_find_complete_by_type(type);
	if (ptype != NULL)
		err = INDIRECT_CALL_INET(ptype->callbacks.gro_complete,
					 ipv6_gro_complete, inet_gro_complete,
					 skb, nhoff + sizeof(*eh));

	return err;
}
EXPORT_SYMBOL(eth_gro_complete);

static struct packet_offload eth_packet_offload __read_mostly = {
	.type = cpu_to_be16(ETH_P_TEB),
	.priority = 10,
	.callbacks = {
		.gro_receive = eth_gro_receive,
		.gro_complete = eth_gro_complete,
	},
};

static int __init eth_offload_init(void)
{
	dev_add_offload(&eth_packet_offload);

	return 0;
}

fs_initcall(eth_offload_init);

unsigned char * __weak arch_get_platform_mac_address(void)
{
	return NULL;
}

int eth_platform_get_mac_address(struct device *dev, u8 *mac_addr)
{
	unsigned char *addr;
	int ret;

	ret = of_get_mac_address(dev->of_node, mac_addr);
	if (!ret)
		return 0;

	addr = arch_get_platform_mac_address();
	if (!addr)
		return -ENODEV;

	ether_addr_copy(mac_addr, addr);

	return 0;
}
EXPORT_SYMBOL(eth_platform_get_mac_address);

 
int platform_get_ethdev_address(struct device *dev, struct net_device *netdev)
{
	u8 addr[ETH_ALEN] __aligned(2);
	int ret;

	ret = eth_platform_get_mac_address(dev, addr);
	if (!ret)
		eth_hw_addr_set(netdev, addr);
	return ret;
}
EXPORT_SYMBOL(platform_get_ethdev_address);

 
int nvmem_get_mac_address(struct device *dev, void *addrbuf)
{
	struct nvmem_cell *cell;
	const void *mac;
	size_t len;

	cell = nvmem_cell_get(dev, "mac-address");
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	mac = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(mac))
		return PTR_ERR(mac);

	if (len != ETH_ALEN || !is_valid_ether_addr(mac)) {
		kfree(mac);
		return -EINVAL;
	}

	ether_addr_copy(addrbuf, mac);
	kfree(mac);

	return 0;
}

static int fwnode_get_mac_addr(struct fwnode_handle *fwnode,
			       const char *name, char *addr)
{
	int ret;

	ret = fwnode_property_read_u8_array(fwnode, name, addr, ETH_ALEN);
	if (ret)
		return ret;

	if (!is_valid_ether_addr(addr))
		return -EINVAL;
	return 0;
}

 
int fwnode_get_mac_address(struct fwnode_handle *fwnode, char *addr)
{
	if (!fwnode_get_mac_addr(fwnode, "mac-address", addr) ||
	    !fwnode_get_mac_addr(fwnode, "local-mac-address", addr) ||
	    !fwnode_get_mac_addr(fwnode, "address", addr))
		return 0;

	return -ENOENT;
}
EXPORT_SYMBOL(fwnode_get_mac_address);

 
int device_get_mac_address(struct device *dev, char *addr)
{
	return fwnode_get_mac_address(dev_fwnode(dev), addr);
}
EXPORT_SYMBOL(device_get_mac_address);

 
int device_get_ethdev_address(struct device *dev, struct net_device *netdev)
{
	u8 addr[ETH_ALEN];
	int ret;

	ret = device_get_mac_address(dev, addr);
	if (!ret)
		eth_hw_addr_set(netdev, addr);
	return ret;
}
EXPORT_SYMBOL(device_get_ethdev_address);
