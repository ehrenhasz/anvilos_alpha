 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/ethtool.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/can.h>
#include <linux/can/can-ml.h>
#include <linux/can/dev.h>
#include <linux/can/skb.h>
#include <linux/slab.h>
#include <net/rtnetlink.h>

#define DRV_NAME "vcan"

MODULE_DESCRIPTION("virtual CAN interface");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Urs Thuermann <urs.thuermann@volkswagen.de>");
MODULE_ALIAS_RTNL_LINK(DRV_NAME);

 

static bool echo;  
module_param(echo, bool, 0444);
MODULE_PARM_DESC(echo, "Echo sent frames (for testing). Default: 0 (Off)");

static void vcan_rx(struct sk_buff *skb, struct net_device *dev)
{
	struct net_device_stats *stats = &dev->stats;

	stats->rx_packets++;
	stats->rx_bytes += can_skb_get_data_len(skb);

	skb->pkt_type  = PACKET_BROADCAST;
	skb->dev       = dev;
	skb->ip_summed = CHECKSUM_UNNECESSARY;

	netif_rx(skb);
}

static netdev_tx_t vcan_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct net_device_stats *stats = &dev->stats;
	unsigned int len;
	int loop;

	if (can_dropped_invalid_skb(dev, skb))
		return NETDEV_TX_OK;

	len = can_skb_get_data_len(skb);
	stats->tx_packets++;
	stats->tx_bytes += len;

	 
	loop = skb->pkt_type == PACKET_LOOPBACK;

	skb_tx_timestamp(skb);

	if (!echo) {
		 
		if (loop) {
			 
			stats->rx_packets++;
			stats->rx_bytes += len;
		}
		consume_skb(skb);
		return NETDEV_TX_OK;
	}

	 

	if (loop) {
		skb = can_create_echo_skb(skb);
		if (!skb)
			return NETDEV_TX_OK;

		 
		vcan_rx(skb, dev);
	} else {
		 
		consume_skb(skb);
	}
	return NETDEV_TX_OK;
}

static int vcan_change_mtu(struct net_device *dev, int new_mtu)
{
	 
	if (dev->flags & IFF_UP)
		return -EBUSY;

	if (new_mtu != CAN_MTU && new_mtu != CANFD_MTU &&
	    !can_is_canxl_dev_mtu(new_mtu))
		return -EINVAL;

	dev->mtu = new_mtu;
	return 0;
}

static const struct net_device_ops vcan_netdev_ops = {
	.ndo_start_xmit = vcan_tx,
	.ndo_change_mtu = vcan_change_mtu,
};

static const struct ethtool_ops vcan_ethtool_ops = {
	.get_ts_info = ethtool_op_get_ts_info,
};

static void vcan_setup(struct net_device *dev)
{
	dev->type		= ARPHRD_CAN;
	dev->mtu		= CANFD_MTU;
	dev->hard_header_len	= 0;
	dev->addr_len		= 0;
	dev->tx_queue_len	= 0;
	dev->flags		= IFF_NOARP;
	can_set_ml_priv(dev, netdev_priv(dev));

	 
	if (echo)
		dev->flags |= IFF_ECHO;

	dev->netdev_ops		= &vcan_netdev_ops;
	dev->ethtool_ops	= &vcan_ethtool_ops;
	dev->needs_free_netdev	= true;
}

static struct rtnl_link_ops vcan_link_ops __read_mostly = {
	.kind = DRV_NAME,
	.priv_size = sizeof(struct can_ml_priv),
	.setup = vcan_setup,
};

static __init int vcan_init_module(void)
{
	pr_info("Virtual CAN interface driver\n");

	if (echo)
		pr_info("enabled echo on driver level.\n");

	return rtnl_link_register(&vcan_link_ops);
}

static __exit void vcan_cleanup_module(void)
{
	rtnl_link_unregister(&vcan_link_ops);
}

module_init(vcan_init_module);
module_exit(vcan_cleanup_module);
