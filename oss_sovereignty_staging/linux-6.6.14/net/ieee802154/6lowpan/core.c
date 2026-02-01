 

 

 

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/ieee802154.h>
#include <linux/if_arp.h>

#include <net/ipv6.h>

#include "6lowpan_i.h"

static int open_count;

static const struct header_ops lowpan_header_ops = {
	.create	= lowpan_header_create,
};

static int lowpan_dev_init(struct net_device *ldev)
{
	netdev_lockdep_set_classes(ldev);

	return 0;
}

static int lowpan_open(struct net_device *dev)
{
	if (!open_count)
		lowpan_rx_init();
	open_count++;
	return 0;
}

static int lowpan_stop(struct net_device *dev)
{
	open_count--;
	if (!open_count)
		lowpan_rx_exit();
	return 0;
}

static int lowpan_neigh_construct(struct net_device *dev, struct neighbour *n)
{
	struct lowpan_802154_neigh *neigh = lowpan_802154_neigh(neighbour_priv(n));

	 
	neigh->short_addr = cpu_to_le16(IEEE802154_ADDR_SHORT_UNSPEC);
	return 0;
}

static int lowpan_get_iflink(const struct net_device *dev)
{
	return lowpan_802154_dev(dev)->wdev->ifindex;
}

static const struct net_device_ops lowpan_netdev_ops = {
	.ndo_init		= lowpan_dev_init,
	.ndo_start_xmit		= lowpan_xmit,
	.ndo_open		= lowpan_open,
	.ndo_stop		= lowpan_stop,
	.ndo_neigh_construct    = lowpan_neigh_construct,
	.ndo_get_iflink         = lowpan_get_iflink,
};

static void lowpan_setup(struct net_device *ldev)
{
	memset(ldev->broadcast, 0xff, IEEE802154_ADDR_LEN);
	 
	ldev->hard_header_len	= sizeof(struct ipv6hdr);
	ldev->flags		= IFF_BROADCAST | IFF_MULTICAST;
	ldev->priv_flags	|= IFF_NO_QUEUE;

	ldev->netdev_ops	= &lowpan_netdev_ops;
	ldev->header_ops	= &lowpan_header_ops;
	ldev->needs_free_netdev	= true;
	ldev->features		|= NETIF_F_NETNS_LOCAL;
}

static int lowpan_validate(struct nlattr *tb[], struct nlattr *data[],
			   struct netlink_ext_ack *extack)
{
	if (tb[IFLA_ADDRESS]) {
		if (nla_len(tb[IFLA_ADDRESS]) != IEEE802154_ADDR_LEN)
			return -EINVAL;
	}
	return 0;
}

static int lowpan_newlink(struct net *src_net, struct net_device *ldev,
			  struct nlattr *tb[], struct nlattr *data[],
			  struct netlink_ext_ack *extack)
{
	struct net_device *wdev;
	int ret;

	ASSERT_RTNL();

	pr_debug("adding new link\n");

	if (!tb[IFLA_LINK])
		return -EINVAL;
	 
	wdev = dev_get_by_index(dev_net(ldev), nla_get_u32(tb[IFLA_LINK]));
	if (!wdev)
		return -ENODEV;
	if (wdev->type != ARPHRD_IEEE802154) {
		dev_put(wdev);
		return -EINVAL;
	}

	if (wdev->ieee802154_ptr->lowpan_dev) {
		dev_put(wdev);
		return -EBUSY;
	}

	lowpan_802154_dev(ldev)->wdev = wdev;
	 
	__dev_addr_set(ldev, wdev->dev_addr, IEEE802154_ADDR_LEN);
	 
	ldev->needed_headroom = LOWPAN_IPHC_MAX_HEADER_LEN +
				wdev->needed_headroom;
	ldev->needed_tailroom = wdev->needed_tailroom;

	ldev->neigh_priv_len = sizeof(struct lowpan_802154_neigh);

	ret = lowpan_register_netdevice(ldev, LOWPAN_LLTYPE_IEEE802154);
	if (ret < 0) {
		dev_put(wdev);
		return ret;
	}

	wdev->ieee802154_ptr->lowpan_dev = ldev;
	return 0;
}

static void lowpan_dellink(struct net_device *ldev, struct list_head *head)
{
	struct net_device *wdev = lowpan_802154_dev(ldev)->wdev;

	ASSERT_RTNL();

	wdev->ieee802154_ptr->lowpan_dev = NULL;
	lowpan_unregister_netdevice(ldev);
	dev_put(wdev);
}

static struct rtnl_link_ops lowpan_link_ops __read_mostly = {
	.kind		= "lowpan",
	.priv_size	= LOWPAN_PRIV_SIZE(sizeof(struct lowpan_802154_dev)),
	.setup		= lowpan_setup,
	.newlink	= lowpan_newlink,
	.dellink	= lowpan_dellink,
	.validate	= lowpan_validate,
};

static inline int __init lowpan_netlink_init(void)
{
	return rtnl_link_register(&lowpan_link_ops);
}

static inline void lowpan_netlink_fini(void)
{
	rtnl_link_unregister(&lowpan_link_ops);
}

static int lowpan_device_event(struct notifier_block *unused,
			       unsigned long event, void *ptr)
{
	struct net_device *ndev = netdev_notifier_info_to_dev(ptr);
	struct wpan_dev *wpan_dev;

	if (ndev->type != ARPHRD_IEEE802154)
		return NOTIFY_DONE;
	wpan_dev = ndev->ieee802154_ptr;
	if (!wpan_dev)
		return NOTIFY_DONE;

	switch (event) {
	case NETDEV_UNREGISTER:
		 
		if (wpan_dev->lowpan_dev)
			lowpan_dellink(wpan_dev->lowpan_dev, NULL);
		break;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block lowpan_dev_notifier = {
	.notifier_call = lowpan_device_event,
};

static int __init lowpan_init_module(void)
{
	int err = 0;

	err = lowpan_net_frag_init();
	if (err < 0)
		goto out;

	err = lowpan_netlink_init();
	if (err < 0)
		goto out_frag;

	err = register_netdevice_notifier(&lowpan_dev_notifier);
	if (err < 0)
		goto out_pack;

	return 0;

out_pack:
	lowpan_netlink_fini();
out_frag:
	lowpan_net_frag_exit();
out:
	return err;
}

static void __exit lowpan_cleanup_module(void)
{
	lowpan_netlink_fini();

	lowpan_net_frag_exit();

	unregister_netdevice_notifier(&lowpan_dev_notifier);
}

module_init(lowpan_init_module);
module_exit(lowpan_cleanup_module);
MODULE_LICENSE("GPL");
MODULE_ALIAS_RTNL_LINK("lowpan");
