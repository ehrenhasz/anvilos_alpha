
 

#include "multicast.h"
#include "main.h"

#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/byteorder/generic.h>
#include <linux/container_of.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/gfp.h>
#include <linux/icmpv6.h>
#include <linux/if_bridge.h>
#include <linux/if_ether.h>
#include <linux/igmp.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/inetdevice.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/lockdep.h>
#include <linux/netdevice.h>
#include <linux/netlink.h>
#include <linux/printk.h>
#include <linux/rculist.h>
#include <linux/rcupdate.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <net/addrconf.h>
#include <net/genetlink.h>
#include <net/if_inet6.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/netlink.h>
#include <net/sock.h>
#include <uapi/linux/batadv_packet.h>
#include <uapi/linux/batman_adv.h>

#include "bridge_loop_avoidance.h"
#include "hard-interface.h"
#include "hash.h"
#include "log.h"
#include "netlink.h"
#include "send.h"
#include "soft-interface.h"
#include "translation-table.h"
#include "tvlv.h"

static void batadv_mcast_mla_update(struct work_struct *work);

 
static void batadv_mcast_start_timer(struct batadv_priv *bat_priv)
{
	queue_delayed_work(batadv_event_workqueue, &bat_priv->mcast.work,
			   msecs_to_jiffies(BATADV_MCAST_WORK_PERIOD));
}

 
static struct net_device *batadv_mcast_get_bridge(struct net_device *soft_iface)
{
	struct net_device *upper = soft_iface;

	rcu_read_lock();
	do {
		upper = netdev_master_upper_dev_get_rcu(upper);
	} while (upper && !netif_is_bridge_master(upper));

	dev_hold(upper);
	rcu_read_unlock();

	return upper;
}

 
static u8 batadv_mcast_mla_rtr_flags_softif_get_ipv4(struct net_device *dev)
{
	struct in_device *in_dev = __in_dev_get_rcu(dev);

	if (in_dev && IN_DEV_MFORWARD(in_dev))
		return BATADV_NO_FLAGS;
	else
		return BATADV_MCAST_WANT_NO_RTR4;
}

 
#if IS_ENABLED(CONFIG_IPV6_MROUTE)
static u8 batadv_mcast_mla_rtr_flags_softif_get_ipv6(struct net_device *dev)
{
	struct inet6_dev *in6_dev = __in6_dev_get(dev);

	if (in6_dev && atomic_read(&in6_dev->cnf.mc_forwarding))
		return BATADV_NO_FLAGS;
	else
		return BATADV_MCAST_WANT_NO_RTR6;
}
#else
static inline u8
batadv_mcast_mla_rtr_flags_softif_get_ipv6(struct net_device *dev)
{
	return BATADV_MCAST_WANT_NO_RTR6;
}
#endif

 
static u8 batadv_mcast_mla_rtr_flags_softif_get(struct batadv_priv *bat_priv,
						struct net_device *bridge)
{
	struct net_device *dev = bridge ? bridge : bat_priv->soft_iface;
	u8 flags = BATADV_NO_FLAGS;

	rcu_read_lock();

	flags |= batadv_mcast_mla_rtr_flags_softif_get_ipv4(dev);
	flags |= batadv_mcast_mla_rtr_flags_softif_get_ipv6(dev);

	rcu_read_unlock();

	return flags;
}

 
static u8 batadv_mcast_mla_rtr_flags_bridge_get(struct batadv_priv *bat_priv,
						struct net_device *bridge)
{
	struct net_device *dev = bat_priv->soft_iface;
	u8 flags = BATADV_NO_FLAGS;

	if (!bridge)
		return BATADV_MCAST_WANT_NO_RTR4 | BATADV_MCAST_WANT_NO_RTR6;

	if (!br_multicast_has_router_adjacent(dev, ETH_P_IP))
		flags |= BATADV_MCAST_WANT_NO_RTR4;
	if (!br_multicast_has_router_adjacent(dev, ETH_P_IPV6))
		flags |= BATADV_MCAST_WANT_NO_RTR6;

	return flags;
}

 
static u8 batadv_mcast_mla_rtr_flags_get(struct batadv_priv *bat_priv,
					 struct net_device *bridge)
{
	u8 flags = BATADV_MCAST_WANT_NO_RTR4 | BATADV_MCAST_WANT_NO_RTR6;

	flags &= batadv_mcast_mla_rtr_flags_softif_get(bat_priv, bridge);
	flags &= batadv_mcast_mla_rtr_flags_bridge_get(bat_priv, bridge);

	return flags;
}

 
static struct batadv_mcast_mla_flags
batadv_mcast_mla_flags_get(struct batadv_priv *bat_priv)
{
	struct net_device *dev = bat_priv->soft_iface;
	struct batadv_mcast_querier_state *qr4, *qr6;
	struct batadv_mcast_mla_flags mla_flags;
	struct net_device *bridge;

	bridge = batadv_mcast_get_bridge(dev);

	memset(&mla_flags, 0, sizeof(mla_flags));
	mla_flags.enabled = 1;
	mla_flags.tvlv_flags |= batadv_mcast_mla_rtr_flags_get(bat_priv,
							       bridge);

	if (!bridge)
		return mla_flags;

	dev_put(bridge);

	mla_flags.bridged = 1;
	qr4 = &mla_flags.querier_ipv4;
	qr6 = &mla_flags.querier_ipv6;

	if (!IS_ENABLED(CONFIG_BRIDGE_IGMP_SNOOPING))
		pr_warn_once("No bridge IGMP snooping compiled - multicast optimizations disabled\n");

	qr4->exists = br_multicast_has_querier_anywhere(dev, ETH_P_IP);
	qr4->shadowing = br_multicast_has_querier_adjacent(dev, ETH_P_IP);

	qr6->exists = br_multicast_has_querier_anywhere(dev, ETH_P_IPV6);
	qr6->shadowing = br_multicast_has_querier_adjacent(dev, ETH_P_IPV6);

	mla_flags.tvlv_flags |= BATADV_MCAST_WANT_ALL_UNSNOOPABLES;

	 
	if (!qr4->exists || qr4->shadowing) {
		mla_flags.tvlv_flags |= BATADV_MCAST_WANT_ALL_IPV4;
		mla_flags.tvlv_flags &= ~BATADV_MCAST_WANT_NO_RTR4;
	}

	if (!qr6->exists || qr6->shadowing) {
		mla_flags.tvlv_flags |= BATADV_MCAST_WANT_ALL_IPV6;
		mla_flags.tvlv_flags &= ~BATADV_MCAST_WANT_NO_RTR6;
	}

	return mla_flags;
}

 
static bool batadv_mcast_mla_is_duplicate(u8 *mcast_addr,
					  struct hlist_head *mcast_list)
{
	struct batadv_hw_addr *mcast_entry;

	hlist_for_each_entry(mcast_entry, mcast_list, list)
		if (batadv_compare_eth(mcast_entry->addr, mcast_addr))
			return true;

	return false;
}

 
static int
batadv_mcast_mla_softif_get_ipv4(struct net_device *dev,
				 struct hlist_head *mcast_list,
				 struct batadv_mcast_mla_flags *flags)
{
	struct batadv_hw_addr *new;
	struct in_device *in_dev;
	u8 mcast_addr[ETH_ALEN];
	struct ip_mc_list *pmc;
	int ret = 0;

	if (flags->tvlv_flags & BATADV_MCAST_WANT_ALL_IPV4)
		return 0;

	rcu_read_lock();

	in_dev = __in_dev_get_rcu(dev);
	if (!in_dev) {
		rcu_read_unlock();
		return 0;
	}

	for (pmc = rcu_dereference(in_dev->mc_list); pmc;
	     pmc = rcu_dereference(pmc->next_rcu)) {
		if (flags->tvlv_flags & BATADV_MCAST_WANT_ALL_UNSNOOPABLES &&
		    ipv4_is_local_multicast(pmc->multiaddr))
			continue;

		if (!(flags->tvlv_flags & BATADV_MCAST_WANT_NO_RTR4) &&
		    !ipv4_is_local_multicast(pmc->multiaddr))
			continue;

		ip_eth_mc_map(pmc->multiaddr, mcast_addr);

		if (batadv_mcast_mla_is_duplicate(mcast_addr, mcast_list))
			continue;

		new = kmalloc(sizeof(*new), GFP_ATOMIC);
		if (!new) {
			ret = -ENOMEM;
			break;
		}

		ether_addr_copy(new->addr, mcast_addr);
		hlist_add_head(&new->list, mcast_list);
		ret++;
	}
	rcu_read_unlock();

	return ret;
}

 
#if IS_ENABLED(CONFIG_IPV6)
static int
batadv_mcast_mla_softif_get_ipv6(struct net_device *dev,
				 struct hlist_head *mcast_list,
				 struct batadv_mcast_mla_flags *flags)
{
	struct batadv_hw_addr *new;
	struct inet6_dev *in6_dev;
	u8 mcast_addr[ETH_ALEN];
	struct ifmcaddr6 *pmc6;
	int ret = 0;

	if (flags->tvlv_flags & BATADV_MCAST_WANT_ALL_IPV6)
		return 0;

	rcu_read_lock();

	in6_dev = __in6_dev_get(dev);
	if (!in6_dev) {
		rcu_read_unlock();
		return 0;
	}

	for (pmc6 = rcu_dereference(in6_dev->mc_list);
	     pmc6;
	     pmc6 = rcu_dereference(pmc6->next)) {
		if (IPV6_ADDR_MC_SCOPE(&pmc6->mca_addr) <
		    IPV6_ADDR_SCOPE_LINKLOCAL)
			continue;

		if (flags->tvlv_flags & BATADV_MCAST_WANT_ALL_UNSNOOPABLES &&
		    ipv6_addr_is_ll_all_nodes(&pmc6->mca_addr))
			continue;

		if (!(flags->tvlv_flags & BATADV_MCAST_WANT_NO_RTR6) &&
		    IPV6_ADDR_MC_SCOPE(&pmc6->mca_addr) >
		    IPV6_ADDR_SCOPE_LINKLOCAL)
			continue;

		ipv6_eth_mc_map(&pmc6->mca_addr, mcast_addr);

		if (batadv_mcast_mla_is_duplicate(mcast_addr, mcast_list))
			continue;

		new = kmalloc(sizeof(*new), GFP_ATOMIC);
		if (!new) {
			ret = -ENOMEM;
			break;
		}

		ether_addr_copy(new->addr, mcast_addr);
		hlist_add_head(&new->list, mcast_list);
		ret++;
	}
	rcu_read_unlock();

	return ret;
}
#else
static inline int
batadv_mcast_mla_softif_get_ipv6(struct net_device *dev,
				 struct hlist_head *mcast_list,
				 struct batadv_mcast_mla_flags *flags)
{
	return 0;
}
#endif

 
static int
batadv_mcast_mla_softif_get(struct net_device *dev,
			    struct hlist_head *mcast_list,
			    struct batadv_mcast_mla_flags *flags)
{
	struct net_device *bridge = batadv_mcast_get_bridge(dev);
	int ret4, ret6 = 0;

	if (bridge)
		dev = bridge;

	ret4 = batadv_mcast_mla_softif_get_ipv4(dev, mcast_list, flags);
	if (ret4 < 0)
		goto out;

	ret6 = batadv_mcast_mla_softif_get_ipv6(dev, mcast_list, flags);
	if (ret6 < 0) {
		ret4 = 0;
		goto out;
	}

out:
	dev_put(bridge);

	return ret4 + ret6;
}

 
static void batadv_mcast_mla_br_addr_cpy(char *dst, const struct br_ip *src)
{
	if (src->proto == htons(ETH_P_IP))
		ip_eth_mc_map(src->dst.ip4, dst);
#if IS_ENABLED(CONFIG_IPV6)
	else if (src->proto == htons(ETH_P_IPV6))
		ipv6_eth_mc_map(&src->dst.ip6, dst);
#endif
	else
		eth_zero_addr(dst);
}

 
static int batadv_mcast_mla_bridge_get(struct net_device *dev,
				       struct hlist_head *mcast_list,
				       struct batadv_mcast_mla_flags *flags)
{
	struct list_head bridge_mcast_list = LIST_HEAD_INIT(bridge_mcast_list);
	struct br_ip_list *br_ip_entry, *tmp;
	u8 tvlv_flags = flags->tvlv_flags;
	struct batadv_hw_addr *new;
	u8 mcast_addr[ETH_ALEN];
	int ret;

	 
	ret = br_multicast_list_adjacent(dev, &bridge_mcast_list);
	if (ret < 0)
		goto out;

	list_for_each_entry(br_ip_entry, &bridge_mcast_list, list) {
		if (br_ip_entry->addr.proto == htons(ETH_P_IP)) {
			if (tvlv_flags & BATADV_MCAST_WANT_ALL_IPV4)
				continue;

			if (tvlv_flags & BATADV_MCAST_WANT_ALL_UNSNOOPABLES &&
			    ipv4_is_local_multicast(br_ip_entry->addr.dst.ip4))
				continue;

			if (!(tvlv_flags & BATADV_MCAST_WANT_NO_RTR4) &&
			    !ipv4_is_local_multicast(br_ip_entry->addr.dst.ip4))
				continue;
		}

#if IS_ENABLED(CONFIG_IPV6)
		if (br_ip_entry->addr.proto == htons(ETH_P_IPV6)) {
			if (tvlv_flags & BATADV_MCAST_WANT_ALL_IPV6)
				continue;

			if (tvlv_flags & BATADV_MCAST_WANT_ALL_UNSNOOPABLES &&
			    ipv6_addr_is_ll_all_nodes(&br_ip_entry->addr.dst.ip6))
				continue;

			if (!(tvlv_flags & BATADV_MCAST_WANT_NO_RTR6) &&
			    IPV6_ADDR_MC_SCOPE(&br_ip_entry->addr.dst.ip6) >
			    IPV6_ADDR_SCOPE_LINKLOCAL)
				continue;
		}
#endif

		batadv_mcast_mla_br_addr_cpy(mcast_addr, &br_ip_entry->addr);
		if (batadv_mcast_mla_is_duplicate(mcast_addr, mcast_list))
			continue;

		new = kmalloc(sizeof(*new), GFP_ATOMIC);
		if (!new) {
			ret = -ENOMEM;
			break;
		}

		ether_addr_copy(new->addr, mcast_addr);
		hlist_add_head(&new->list, mcast_list);
	}

out:
	list_for_each_entry_safe(br_ip_entry, tmp, &bridge_mcast_list, list) {
		list_del(&br_ip_entry->list);
		kfree(br_ip_entry);
	}

	return ret;
}

 
static void batadv_mcast_mla_list_free(struct hlist_head *mcast_list)
{
	struct batadv_hw_addr *mcast_entry;
	struct hlist_node *tmp;

	hlist_for_each_entry_safe(mcast_entry, tmp, mcast_list, list) {
		hlist_del(&mcast_entry->list);
		kfree(mcast_entry);
	}
}

 
static void batadv_mcast_mla_tt_retract(struct batadv_priv *bat_priv,
					struct hlist_head *mcast_list)
{
	struct batadv_hw_addr *mcast_entry;
	struct hlist_node *tmp;

	hlist_for_each_entry_safe(mcast_entry, tmp, &bat_priv->mcast.mla_list,
				  list) {
		if (mcast_list &&
		    batadv_mcast_mla_is_duplicate(mcast_entry->addr,
						  mcast_list))
			continue;

		batadv_tt_local_remove(bat_priv, mcast_entry->addr,
				       BATADV_NO_FLAGS,
				       "mcast TT outdated", false);

		hlist_del(&mcast_entry->list);
		kfree(mcast_entry);
	}
}

 
static void batadv_mcast_mla_tt_add(struct batadv_priv *bat_priv,
				    struct hlist_head *mcast_list)
{
	struct batadv_hw_addr *mcast_entry;
	struct hlist_node *tmp;

	if (!mcast_list)
		return;

	hlist_for_each_entry_safe(mcast_entry, tmp, mcast_list, list) {
		if (batadv_mcast_mla_is_duplicate(mcast_entry->addr,
						  &bat_priv->mcast.mla_list))
			continue;

		if (!batadv_tt_local_add(bat_priv->soft_iface,
					 mcast_entry->addr, BATADV_NO_FLAGS,
					 BATADV_NULL_IFINDEX, BATADV_NO_MARK))
			continue;

		hlist_del(&mcast_entry->list);
		hlist_add_head(&mcast_entry->list, &bat_priv->mcast.mla_list);
	}
}

 
static void
batadv_mcast_querier_log(struct batadv_priv *bat_priv, char *str_proto,
			 struct batadv_mcast_querier_state *old_state,
			 struct batadv_mcast_querier_state *new_state)
{
	if (!old_state->exists && new_state->exists)
		batadv_info(bat_priv->soft_iface, "%s Querier appeared\n",
			    str_proto);
	else if (old_state->exists && !new_state->exists)
		batadv_info(bat_priv->soft_iface,
			    "%s Querier disappeared - multicast optimizations disabled\n",
			    str_proto);
	else if (!bat_priv->mcast.mla_flags.bridged && !new_state->exists)
		batadv_info(bat_priv->soft_iface,
			    "No %s Querier present - multicast optimizations disabled\n",
			    str_proto);

	if (new_state->exists) {
		if ((!old_state->shadowing && new_state->shadowing) ||
		    (!old_state->exists && new_state->shadowing))
			batadv_dbg(BATADV_DBG_MCAST, bat_priv,
				   "%s Querier is behind our bridged segment: Might shadow listeners\n",
				   str_proto);
		else if (old_state->shadowing && !new_state->shadowing)
			batadv_dbg(BATADV_DBG_MCAST, bat_priv,
				   "%s Querier is not behind our bridged segment\n",
				   str_proto);
	}
}

 
static void
batadv_mcast_bridge_log(struct batadv_priv *bat_priv,
			struct batadv_mcast_mla_flags *new_flags)
{
	struct batadv_mcast_mla_flags *old_flags = &bat_priv->mcast.mla_flags;

	if (!old_flags->bridged && new_flags->bridged)
		batadv_dbg(BATADV_DBG_MCAST, bat_priv,
			   "Bridge added: Setting Unsnoopables(U)-flag\n");
	else if (old_flags->bridged && !new_flags->bridged)
		batadv_dbg(BATADV_DBG_MCAST, bat_priv,
			   "Bridge removed: Unsetting Unsnoopables(U)-flag\n");

	if (new_flags->bridged) {
		batadv_mcast_querier_log(bat_priv, "IGMP",
					 &old_flags->querier_ipv4,
					 &new_flags->querier_ipv4);
		batadv_mcast_querier_log(bat_priv, "MLD",
					 &old_flags->querier_ipv6,
					 &new_flags->querier_ipv6);
	}
}

 
static void batadv_mcast_flags_log(struct batadv_priv *bat_priv, u8 flags)
{
	bool old_enabled = bat_priv->mcast.mla_flags.enabled;
	u8 old_flags = bat_priv->mcast.mla_flags.tvlv_flags;
	char str_old_flags[] = "[.... . ]";

	sprintf(str_old_flags, "[%c%c%c%s%s]",
		(old_flags & BATADV_MCAST_WANT_ALL_UNSNOOPABLES) ? 'U' : '.',
		(old_flags & BATADV_MCAST_WANT_ALL_IPV4) ? '4' : '.',
		(old_flags & BATADV_MCAST_WANT_ALL_IPV6) ? '6' : '.',
		!(old_flags & BATADV_MCAST_WANT_NO_RTR4) ? "R4" : ". ",
		!(old_flags & BATADV_MCAST_WANT_NO_RTR6) ? "R6" : ". ");

	batadv_dbg(BATADV_DBG_MCAST, bat_priv,
		   "Changing multicast flags from '%s' to '[%c%c%c%s%s]'\n",
		   old_enabled ? str_old_flags : "<undefined>",
		   (flags & BATADV_MCAST_WANT_ALL_UNSNOOPABLES) ? 'U' : '.',
		   (flags & BATADV_MCAST_WANT_ALL_IPV4) ? '4' : '.',
		   (flags & BATADV_MCAST_WANT_ALL_IPV6) ? '6' : '.',
		   !(flags & BATADV_MCAST_WANT_NO_RTR4) ? "R4" : ". ",
		   !(flags & BATADV_MCAST_WANT_NO_RTR6) ? "R6" : ". ");
}

 
static void
batadv_mcast_mla_flags_update(struct batadv_priv *bat_priv,
			      struct batadv_mcast_mla_flags *flags)
{
	struct batadv_tvlv_mcast_data mcast_data;

	if (!memcmp(flags, &bat_priv->mcast.mla_flags, sizeof(*flags)))
		return;

	batadv_mcast_bridge_log(bat_priv, flags);
	batadv_mcast_flags_log(bat_priv, flags->tvlv_flags);

	mcast_data.flags = flags->tvlv_flags;
	memset(mcast_data.reserved, 0, sizeof(mcast_data.reserved));

	batadv_tvlv_container_register(bat_priv, BATADV_TVLV_MCAST, 2,
				       &mcast_data, sizeof(mcast_data));

	bat_priv->mcast.mla_flags = *flags;
}

 
static void __batadv_mcast_mla_update(struct batadv_priv *bat_priv)
{
	struct net_device *soft_iface = bat_priv->soft_iface;
	struct hlist_head mcast_list = HLIST_HEAD_INIT;
	struct batadv_mcast_mla_flags flags;
	int ret;

	flags = batadv_mcast_mla_flags_get(bat_priv);

	ret = batadv_mcast_mla_softif_get(soft_iface, &mcast_list, &flags);
	if (ret < 0)
		goto out;

	ret = batadv_mcast_mla_bridge_get(soft_iface, &mcast_list, &flags);
	if (ret < 0)
		goto out;

	spin_lock(&bat_priv->mcast.mla_lock);
	batadv_mcast_mla_tt_retract(bat_priv, &mcast_list);
	batadv_mcast_mla_tt_add(bat_priv, &mcast_list);
	batadv_mcast_mla_flags_update(bat_priv, &flags);
	spin_unlock(&bat_priv->mcast.mla_lock);

out:
	batadv_mcast_mla_list_free(&mcast_list);
}

 
static void batadv_mcast_mla_update(struct work_struct *work)
{
	struct delayed_work *delayed_work;
	struct batadv_priv_mcast *priv_mcast;
	struct batadv_priv *bat_priv;

	delayed_work = to_delayed_work(work);
	priv_mcast = container_of(delayed_work, struct batadv_priv_mcast, work);
	bat_priv = container_of(priv_mcast, struct batadv_priv, mcast);

	__batadv_mcast_mla_update(bat_priv);
	batadv_mcast_start_timer(bat_priv);
}

 
static bool batadv_mcast_is_report_ipv4(struct sk_buff *skb)
{
	if (ip_mc_check_igmp(skb) < 0)
		return false;

	switch (igmp_hdr(skb)->type) {
	case IGMP_HOST_MEMBERSHIP_REPORT:
	case IGMPV2_HOST_MEMBERSHIP_REPORT:
	case IGMPV3_HOST_MEMBERSHIP_REPORT:
		return true;
	}

	return false;
}

 
static int batadv_mcast_forw_mode_check_ipv4(struct batadv_priv *bat_priv,
					     struct sk_buff *skb,
					     bool *is_unsnoopable,
					     int *is_routable)
{
	struct iphdr *iphdr;

	 
	if (!pskb_may_pull(skb, sizeof(struct ethhdr) + sizeof(*iphdr)))
		return -ENOMEM;

	if (batadv_mcast_is_report_ipv4(skb))
		return -EINVAL;

	iphdr = ip_hdr(skb);

	 
	if (ipv4_is_local_multicast(iphdr->daddr))
		*is_unsnoopable = true;
	else
		*is_routable = ETH_P_IP;

	return 0;
}

 
static bool batadv_mcast_is_report_ipv6(struct sk_buff *skb)
{
	if (ipv6_mc_check_mld(skb) < 0)
		return false;

	switch (icmp6_hdr(skb)->icmp6_type) {
	case ICMPV6_MGM_REPORT:
	case ICMPV6_MLD2_REPORT:
		return true;
	}

	return false;
}

 
static int batadv_mcast_forw_mode_check_ipv6(struct batadv_priv *bat_priv,
					     struct sk_buff *skb,
					     bool *is_unsnoopable,
					     int *is_routable)
{
	struct ipv6hdr *ip6hdr;

	 
	if (!pskb_may_pull(skb, sizeof(struct ethhdr) + sizeof(*ip6hdr)))
		return -ENOMEM;

	if (batadv_mcast_is_report_ipv6(skb))
		return -EINVAL;

	ip6hdr = ipv6_hdr(skb);

	if (IPV6_ADDR_MC_SCOPE(&ip6hdr->daddr) < IPV6_ADDR_SCOPE_LINKLOCAL)
		return -EINVAL;

	 
	if (ipv6_addr_is_ll_all_nodes(&ip6hdr->daddr))
		*is_unsnoopable = true;
	else if (IPV6_ADDR_MC_SCOPE(&ip6hdr->daddr) > IPV6_ADDR_SCOPE_LINKLOCAL)
		*is_routable = ETH_P_IPV6;

	return 0;
}

 
static int batadv_mcast_forw_mode_check(struct batadv_priv *bat_priv,
					struct sk_buff *skb,
					bool *is_unsnoopable,
					int *is_routable)
{
	struct ethhdr *ethhdr = eth_hdr(skb);

	if (!atomic_read(&bat_priv->multicast_mode))
		return -EINVAL;

	switch (ntohs(ethhdr->h_proto)) {
	case ETH_P_IP:
		return batadv_mcast_forw_mode_check_ipv4(bat_priv, skb,
							 is_unsnoopable,
							 is_routable);
	case ETH_P_IPV6:
		if (!IS_ENABLED(CONFIG_IPV6))
			return -EINVAL;

		return batadv_mcast_forw_mode_check_ipv6(bat_priv, skb,
							 is_unsnoopable,
							 is_routable);
	default:
		return -EINVAL;
	}
}

 
static int batadv_mcast_forw_want_all_ip_count(struct batadv_priv *bat_priv,
					       struct ethhdr *ethhdr)
{
	switch (ntohs(ethhdr->h_proto)) {
	case ETH_P_IP:
		return atomic_read(&bat_priv->mcast.num_want_all_ipv4);
	case ETH_P_IPV6:
		return atomic_read(&bat_priv->mcast.num_want_all_ipv6);
	default:
		 
		return 0;
	}
}

 

static int batadv_mcast_forw_rtr_count(struct batadv_priv *bat_priv,
				       int protocol)
{
	switch (protocol) {
	case ETH_P_IP:
		return atomic_read(&bat_priv->mcast.num_want_all_rtr4);
	case ETH_P_IPV6:
		return atomic_read(&bat_priv->mcast.num_want_all_rtr6);
	default:
		return 0;
	}
}

 
enum batadv_forw_mode
batadv_mcast_forw_mode(struct batadv_priv *bat_priv, struct sk_buff *skb,
		       int *is_routable)
{
	int ret, tt_count, ip_count, unsnoop_count, total_count;
	bool is_unsnoopable = false;
	struct ethhdr *ethhdr;
	int rtr_count = 0;

	ret = batadv_mcast_forw_mode_check(bat_priv, skb, &is_unsnoopable,
					   is_routable);
	if (ret == -ENOMEM)
		return BATADV_FORW_NONE;
	else if (ret < 0)
		return BATADV_FORW_BCAST;

	ethhdr = eth_hdr(skb);

	tt_count = batadv_tt_global_hash_count(bat_priv, ethhdr->h_dest,
					       BATADV_NO_FLAGS);
	ip_count = batadv_mcast_forw_want_all_ip_count(bat_priv, ethhdr);
	unsnoop_count = !is_unsnoopable ? 0 :
			atomic_read(&bat_priv->mcast.num_want_all_unsnoopables);
	rtr_count = batadv_mcast_forw_rtr_count(bat_priv, *is_routable);

	total_count = tt_count + ip_count + unsnoop_count + rtr_count;

	if (!total_count)
		return BATADV_FORW_NONE;
	else if (unsnoop_count)
		return BATADV_FORW_BCAST;

	if (total_count <= atomic_read(&bat_priv->multicast_fanout))
		return BATADV_FORW_UCASTS;

	return BATADV_FORW_BCAST;
}

 
static int batadv_mcast_forw_send_orig(struct batadv_priv *bat_priv,
				       struct sk_buff *skb,
				       unsigned short vid,
				       struct batadv_orig_node *orig_node)
{
	 
	if (batadv_bla_is_backbone_gw_orig(bat_priv, orig_node->orig, vid)) {
		dev_kfree_skb(skb);
		return NET_XMIT_SUCCESS;
	}

	return batadv_send_skb_unicast(bat_priv, skb, BATADV_UNICAST, 0,
				       orig_node, vid);
}

 
static int
batadv_mcast_forw_tt(struct batadv_priv *bat_priv, struct sk_buff *skb,
		     unsigned short vid)
{
	int ret = NET_XMIT_SUCCESS;
	struct sk_buff *newskb;

	struct batadv_tt_orig_list_entry *orig_entry;

	struct batadv_tt_global_entry *tt_global;
	const u8 *addr = eth_hdr(skb)->h_dest;

	tt_global = batadv_tt_global_hash_find(bat_priv, addr, vid);
	if (!tt_global)
		goto out;

	rcu_read_lock();
	hlist_for_each_entry_rcu(orig_entry, &tt_global->orig_list, list) {
		newskb = skb_copy(skb, GFP_ATOMIC);
		if (!newskb) {
			ret = NET_XMIT_DROP;
			break;
		}

		batadv_mcast_forw_send_orig(bat_priv, newskb, vid,
					    orig_entry->orig_node);
	}
	rcu_read_unlock();

	batadv_tt_global_entry_put(tt_global);

out:
	return ret;
}

 
static int
batadv_mcast_forw_want_all_ipv4(struct batadv_priv *bat_priv,
				struct sk_buff *skb, unsigned short vid)
{
	struct batadv_orig_node *orig_node;
	int ret = NET_XMIT_SUCCESS;
	struct sk_buff *newskb;

	rcu_read_lock();
	hlist_for_each_entry_rcu(orig_node,
				 &bat_priv->mcast.want_all_ipv4_list,
				 mcast_want_all_ipv4_node) {
		newskb = skb_copy(skb, GFP_ATOMIC);
		if (!newskb) {
			ret = NET_XMIT_DROP;
			break;
		}

		batadv_mcast_forw_send_orig(bat_priv, newskb, vid, orig_node);
	}
	rcu_read_unlock();
	return ret;
}

 
static int
batadv_mcast_forw_want_all_ipv6(struct batadv_priv *bat_priv,
				struct sk_buff *skb, unsigned short vid)
{
	struct batadv_orig_node *orig_node;
	int ret = NET_XMIT_SUCCESS;
	struct sk_buff *newskb;

	rcu_read_lock();
	hlist_for_each_entry_rcu(orig_node,
				 &bat_priv->mcast.want_all_ipv6_list,
				 mcast_want_all_ipv6_node) {
		newskb = skb_copy(skb, GFP_ATOMIC);
		if (!newskb) {
			ret = NET_XMIT_DROP;
			break;
		}

		batadv_mcast_forw_send_orig(bat_priv, newskb, vid, orig_node);
	}
	rcu_read_unlock();
	return ret;
}

 
static int
batadv_mcast_forw_want_all(struct batadv_priv *bat_priv,
			   struct sk_buff *skb, unsigned short vid)
{
	switch (ntohs(eth_hdr(skb)->h_proto)) {
	case ETH_P_IP:
		return batadv_mcast_forw_want_all_ipv4(bat_priv, skb, vid);
	case ETH_P_IPV6:
		return batadv_mcast_forw_want_all_ipv6(bat_priv, skb, vid);
	default:
		 
		return NET_XMIT_DROP;
	}
}

 
static int
batadv_mcast_forw_want_all_rtr4(struct batadv_priv *bat_priv,
				struct sk_buff *skb, unsigned short vid)
{
	struct batadv_orig_node *orig_node;
	int ret = NET_XMIT_SUCCESS;
	struct sk_buff *newskb;

	rcu_read_lock();
	hlist_for_each_entry_rcu(orig_node,
				 &bat_priv->mcast.want_all_rtr4_list,
				 mcast_want_all_rtr4_node) {
		newskb = skb_copy(skb, GFP_ATOMIC);
		if (!newskb) {
			ret = NET_XMIT_DROP;
			break;
		}

		batadv_mcast_forw_send_orig(bat_priv, newskb, vid, orig_node);
	}
	rcu_read_unlock();
	return ret;
}

 
static int
batadv_mcast_forw_want_all_rtr6(struct batadv_priv *bat_priv,
				struct sk_buff *skb, unsigned short vid)
{
	struct batadv_orig_node *orig_node;
	int ret = NET_XMIT_SUCCESS;
	struct sk_buff *newskb;

	rcu_read_lock();
	hlist_for_each_entry_rcu(orig_node,
				 &bat_priv->mcast.want_all_rtr6_list,
				 mcast_want_all_rtr6_node) {
		newskb = skb_copy(skb, GFP_ATOMIC);
		if (!newskb) {
			ret = NET_XMIT_DROP;
			break;
		}

		batadv_mcast_forw_send_orig(bat_priv, newskb, vid, orig_node);
	}
	rcu_read_unlock();
	return ret;
}

 
static int
batadv_mcast_forw_want_rtr(struct batadv_priv *bat_priv,
			   struct sk_buff *skb, unsigned short vid)
{
	switch (ntohs(eth_hdr(skb)->h_proto)) {
	case ETH_P_IP:
		return batadv_mcast_forw_want_all_rtr4(bat_priv, skb, vid);
	case ETH_P_IPV6:
		return batadv_mcast_forw_want_all_rtr6(bat_priv, skb, vid);
	default:
		 
		return NET_XMIT_DROP;
	}
}

 
int batadv_mcast_forw_send(struct batadv_priv *bat_priv, struct sk_buff *skb,
			   unsigned short vid, int is_routable)
{
	int ret;

	ret = batadv_mcast_forw_tt(bat_priv, skb, vid);
	if (ret != NET_XMIT_SUCCESS) {
		kfree_skb(skb);
		return ret;
	}

	ret = batadv_mcast_forw_want_all(bat_priv, skb, vid);
	if (ret != NET_XMIT_SUCCESS) {
		kfree_skb(skb);
		return ret;
	}

	if (!is_routable)
		goto skip_mc_router;

	ret = batadv_mcast_forw_want_rtr(bat_priv, skb, vid);
	if (ret != NET_XMIT_SUCCESS) {
		kfree_skb(skb);
		return ret;
	}

skip_mc_router:
	consume_skb(skb);
	return ret;
}

 
static void batadv_mcast_want_unsnoop_update(struct batadv_priv *bat_priv,
					     struct batadv_orig_node *orig,
					     u8 mcast_flags)
{
	struct hlist_node *node = &orig->mcast_want_all_unsnoopables_node;
	struct hlist_head *head = &bat_priv->mcast.want_all_unsnoopables_list;

	lockdep_assert_held(&orig->mcast_handler_lock);

	 
	if (mcast_flags & BATADV_MCAST_WANT_ALL_UNSNOOPABLES &&
	    !(orig->mcast_flags & BATADV_MCAST_WANT_ALL_UNSNOOPABLES)) {
		atomic_inc(&bat_priv->mcast.num_want_all_unsnoopables);

		spin_lock_bh(&bat_priv->mcast.want_lists_lock);
		 
		WARN_ON(!hlist_unhashed(node));

		hlist_add_head_rcu(node, head);
		spin_unlock_bh(&bat_priv->mcast.want_lists_lock);
	 
	} else if (!(mcast_flags & BATADV_MCAST_WANT_ALL_UNSNOOPABLES) &&
		   orig->mcast_flags & BATADV_MCAST_WANT_ALL_UNSNOOPABLES) {
		atomic_dec(&bat_priv->mcast.num_want_all_unsnoopables);

		spin_lock_bh(&bat_priv->mcast.want_lists_lock);
		 
		WARN_ON(hlist_unhashed(node));

		hlist_del_init_rcu(node);
		spin_unlock_bh(&bat_priv->mcast.want_lists_lock);
	}
}

 
static void batadv_mcast_want_ipv4_update(struct batadv_priv *bat_priv,
					  struct batadv_orig_node *orig,
					  u8 mcast_flags)
{
	struct hlist_node *node = &orig->mcast_want_all_ipv4_node;
	struct hlist_head *head = &bat_priv->mcast.want_all_ipv4_list;

	lockdep_assert_held(&orig->mcast_handler_lock);

	 
	if (mcast_flags & BATADV_MCAST_WANT_ALL_IPV4 &&
	    !(orig->mcast_flags & BATADV_MCAST_WANT_ALL_IPV4)) {
		atomic_inc(&bat_priv->mcast.num_want_all_ipv4);

		spin_lock_bh(&bat_priv->mcast.want_lists_lock);
		 
		WARN_ON(!hlist_unhashed(node));

		hlist_add_head_rcu(node, head);
		spin_unlock_bh(&bat_priv->mcast.want_lists_lock);
	 
	} else if (!(mcast_flags & BATADV_MCAST_WANT_ALL_IPV4) &&
		   orig->mcast_flags & BATADV_MCAST_WANT_ALL_IPV4) {
		atomic_dec(&bat_priv->mcast.num_want_all_ipv4);

		spin_lock_bh(&bat_priv->mcast.want_lists_lock);
		 
		WARN_ON(hlist_unhashed(node));

		hlist_del_init_rcu(node);
		spin_unlock_bh(&bat_priv->mcast.want_lists_lock);
	}
}

 
static void batadv_mcast_want_ipv6_update(struct batadv_priv *bat_priv,
					  struct batadv_orig_node *orig,
					  u8 mcast_flags)
{
	struct hlist_node *node = &orig->mcast_want_all_ipv6_node;
	struct hlist_head *head = &bat_priv->mcast.want_all_ipv6_list;

	lockdep_assert_held(&orig->mcast_handler_lock);

	 
	if (mcast_flags & BATADV_MCAST_WANT_ALL_IPV6 &&
	    !(orig->mcast_flags & BATADV_MCAST_WANT_ALL_IPV6)) {
		atomic_inc(&bat_priv->mcast.num_want_all_ipv6);

		spin_lock_bh(&bat_priv->mcast.want_lists_lock);
		 
		WARN_ON(!hlist_unhashed(node));

		hlist_add_head_rcu(node, head);
		spin_unlock_bh(&bat_priv->mcast.want_lists_lock);
	 
	} else if (!(mcast_flags & BATADV_MCAST_WANT_ALL_IPV6) &&
		   orig->mcast_flags & BATADV_MCAST_WANT_ALL_IPV6) {
		atomic_dec(&bat_priv->mcast.num_want_all_ipv6);

		spin_lock_bh(&bat_priv->mcast.want_lists_lock);
		 
		WARN_ON(hlist_unhashed(node));

		hlist_del_init_rcu(node);
		spin_unlock_bh(&bat_priv->mcast.want_lists_lock);
	}
}

 
static void batadv_mcast_want_rtr4_update(struct batadv_priv *bat_priv,
					  struct batadv_orig_node *orig,
					  u8 mcast_flags)
{
	struct hlist_node *node = &orig->mcast_want_all_rtr4_node;
	struct hlist_head *head = &bat_priv->mcast.want_all_rtr4_list;

	lockdep_assert_held(&orig->mcast_handler_lock);

	 
	if (!(mcast_flags & BATADV_MCAST_WANT_NO_RTR4) &&
	    orig->mcast_flags & BATADV_MCAST_WANT_NO_RTR4) {
		atomic_inc(&bat_priv->mcast.num_want_all_rtr4);

		spin_lock_bh(&bat_priv->mcast.want_lists_lock);
		 
		WARN_ON(!hlist_unhashed(node));

		hlist_add_head_rcu(node, head);
		spin_unlock_bh(&bat_priv->mcast.want_lists_lock);
	 
	} else if (mcast_flags & BATADV_MCAST_WANT_NO_RTR4 &&
		   !(orig->mcast_flags & BATADV_MCAST_WANT_NO_RTR4)) {
		atomic_dec(&bat_priv->mcast.num_want_all_rtr4);

		spin_lock_bh(&bat_priv->mcast.want_lists_lock);
		 
		WARN_ON(hlist_unhashed(node));

		hlist_del_init_rcu(node);
		spin_unlock_bh(&bat_priv->mcast.want_lists_lock);
	}
}

 
static void batadv_mcast_want_rtr6_update(struct batadv_priv *bat_priv,
					  struct batadv_orig_node *orig,
					  u8 mcast_flags)
{
	struct hlist_node *node = &orig->mcast_want_all_rtr6_node;
	struct hlist_head *head = &bat_priv->mcast.want_all_rtr6_list;

	lockdep_assert_held(&orig->mcast_handler_lock);

	 
	if (!(mcast_flags & BATADV_MCAST_WANT_NO_RTR6) &&
	    orig->mcast_flags & BATADV_MCAST_WANT_NO_RTR6) {
		atomic_inc(&bat_priv->mcast.num_want_all_rtr6);

		spin_lock_bh(&bat_priv->mcast.want_lists_lock);
		 
		WARN_ON(!hlist_unhashed(node));

		hlist_add_head_rcu(node, head);
		spin_unlock_bh(&bat_priv->mcast.want_lists_lock);
	 
	} else if (mcast_flags & BATADV_MCAST_WANT_NO_RTR6 &&
		   !(orig->mcast_flags & BATADV_MCAST_WANT_NO_RTR6)) {
		atomic_dec(&bat_priv->mcast.num_want_all_rtr6);

		spin_lock_bh(&bat_priv->mcast.want_lists_lock);
		 
		WARN_ON(hlist_unhashed(node));

		hlist_del_init_rcu(node);
		spin_unlock_bh(&bat_priv->mcast.want_lists_lock);
	}
}

 
static u8
batadv_mcast_tvlv_flags_get(bool enabled, void *tvlv_value, u16 tvlv_value_len)
{
	u8 mcast_flags = BATADV_NO_FLAGS;

	if (enabled && tvlv_value && tvlv_value_len >= sizeof(mcast_flags))
		mcast_flags = *(u8 *)tvlv_value;

	if (!enabled) {
		mcast_flags |= BATADV_MCAST_WANT_ALL_IPV4;
		mcast_flags |= BATADV_MCAST_WANT_ALL_IPV6;
	}

	 
	if (mcast_flags & BATADV_MCAST_WANT_ALL_IPV4)
		mcast_flags |= BATADV_MCAST_WANT_NO_RTR4;

	if (mcast_flags & BATADV_MCAST_WANT_ALL_IPV6)
		mcast_flags |= BATADV_MCAST_WANT_NO_RTR6;

	return mcast_flags;
}

 
static void batadv_mcast_tvlv_ogm_handler(struct batadv_priv *bat_priv,
					  struct batadv_orig_node *orig,
					  u8 flags,
					  void *tvlv_value,
					  u16 tvlv_value_len)
{
	bool orig_mcast_enabled = !(flags & BATADV_TVLV_HANDLER_OGM_CIFNOTFND);
	u8 mcast_flags;

	mcast_flags = batadv_mcast_tvlv_flags_get(orig_mcast_enabled,
						  tvlv_value, tvlv_value_len);

	spin_lock_bh(&orig->mcast_handler_lock);

	if (orig_mcast_enabled &&
	    !test_bit(BATADV_ORIG_CAPA_HAS_MCAST, &orig->capabilities)) {
		set_bit(BATADV_ORIG_CAPA_HAS_MCAST, &orig->capabilities);
	} else if (!orig_mcast_enabled &&
		   test_bit(BATADV_ORIG_CAPA_HAS_MCAST, &orig->capabilities)) {
		clear_bit(BATADV_ORIG_CAPA_HAS_MCAST, &orig->capabilities);
	}

	set_bit(BATADV_ORIG_CAPA_HAS_MCAST, &orig->capa_initialized);

	batadv_mcast_want_unsnoop_update(bat_priv, orig, mcast_flags);
	batadv_mcast_want_ipv4_update(bat_priv, orig, mcast_flags);
	batadv_mcast_want_ipv6_update(bat_priv, orig, mcast_flags);
	batadv_mcast_want_rtr4_update(bat_priv, orig, mcast_flags);
	batadv_mcast_want_rtr6_update(bat_priv, orig, mcast_flags);

	orig->mcast_flags = mcast_flags;
	spin_unlock_bh(&orig->mcast_handler_lock);
}

 
void batadv_mcast_init(struct batadv_priv *bat_priv)
{
	batadv_tvlv_handler_register(bat_priv, batadv_mcast_tvlv_ogm_handler,
				     NULL, NULL, BATADV_TVLV_MCAST, 2,
				     BATADV_TVLV_HANDLER_OGM_CIFNOTFND);

	INIT_DELAYED_WORK(&bat_priv->mcast.work, batadv_mcast_mla_update);
	batadv_mcast_start_timer(bat_priv);
}

 
int batadv_mcast_mesh_info_put(struct sk_buff *msg,
			       struct batadv_priv *bat_priv)
{
	u32 flags = bat_priv->mcast.mla_flags.tvlv_flags;
	u32 flags_priv = BATADV_NO_FLAGS;

	if (bat_priv->mcast.mla_flags.bridged) {
		flags_priv |= BATADV_MCAST_FLAGS_BRIDGED;

		if (bat_priv->mcast.mla_flags.querier_ipv4.exists)
			flags_priv |= BATADV_MCAST_FLAGS_QUERIER_IPV4_EXISTS;
		if (bat_priv->mcast.mla_flags.querier_ipv6.exists)
			flags_priv |= BATADV_MCAST_FLAGS_QUERIER_IPV6_EXISTS;
		if (bat_priv->mcast.mla_flags.querier_ipv4.shadowing)
			flags_priv |= BATADV_MCAST_FLAGS_QUERIER_IPV4_SHADOWING;
		if (bat_priv->mcast.mla_flags.querier_ipv6.shadowing)
			flags_priv |= BATADV_MCAST_FLAGS_QUERIER_IPV6_SHADOWING;
	}

	if (nla_put_u32(msg, BATADV_ATTR_MCAST_FLAGS, flags) ||
	    nla_put_u32(msg, BATADV_ATTR_MCAST_FLAGS_PRIV, flags_priv))
		return -EMSGSIZE;

	return 0;
}

 
static int
batadv_mcast_flags_dump_entry(struct sk_buff *msg, u32 portid,
			      struct netlink_callback *cb,
			      struct batadv_orig_node *orig_node)
{
	void *hdr;

	hdr = genlmsg_put(msg, portid, cb->nlh->nlmsg_seq,
			  &batadv_netlink_family, NLM_F_MULTI,
			  BATADV_CMD_GET_MCAST_FLAGS);
	if (!hdr)
		return -ENOBUFS;

	genl_dump_check_consistent(cb, hdr);

	if (nla_put(msg, BATADV_ATTR_ORIG_ADDRESS, ETH_ALEN,
		    orig_node->orig)) {
		genlmsg_cancel(msg, hdr);
		return -EMSGSIZE;
	}

	if (test_bit(BATADV_ORIG_CAPA_HAS_MCAST,
		     &orig_node->capabilities)) {
		if (nla_put_u32(msg, BATADV_ATTR_MCAST_FLAGS,
				orig_node->mcast_flags)) {
			genlmsg_cancel(msg, hdr);
			return -EMSGSIZE;
		}
	}

	genlmsg_end(msg, hdr);
	return 0;
}

 
static int
batadv_mcast_flags_dump_bucket(struct sk_buff *msg, u32 portid,
			       struct netlink_callback *cb,
			       struct batadv_hashtable *hash,
			       unsigned int bucket, long *idx_skip)
{
	struct batadv_orig_node *orig_node;
	long idx = 0;

	spin_lock_bh(&hash->list_locks[bucket]);
	cb->seq = atomic_read(&hash->generation) << 1 | 1;

	hlist_for_each_entry(orig_node, &hash->table[bucket], hash_entry) {
		if (!test_bit(BATADV_ORIG_CAPA_HAS_MCAST,
			      &orig_node->capa_initialized))
			continue;

		if (idx < *idx_skip)
			goto skip;

		if (batadv_mcast_flags_dump_entry(msg, portid, cb, orig_node)) {
			spin_unlock_bh(&hash->list_locks[bucket]);
			*idx_skip = idx;

			return -EMSGSIZE;
		}

skip:
		idx++;
	}
	spin_unlock_bh(&hash->list_locks[bucket]);

	return 0;
}

 
static int
__batadv_mcast_flags_dump(struct sk_buff *msg, u32 portid,
			  struct netlink_callback *cb,
			  struct batadv_priv *bat_priv, long *bucket, long *idx)
{
	struct batadv_hashtable *hash = bat_priv->orig_hash;
	long bucket_tmp = *bucket;
	long idx_tmp = *idx;

	while (bucket_tmp < hash->size) {
		if (batadv_mcast_flags_dump_bucket(msg, portid, cb, hash,
						   bucket_tmp, &idx_tmp))
			break;

		bucket_tmp++;
		idx_tmp = 0;
	}

	*bucket = bucket_tmp;
	*idx = idx_tmp;

	return msg->len;
}

 
static int
batadv_mcast_netlink_get_primary(struct netlink_callback *cb,
				 struct batadv_hard_iface **primary_if)
{
	struct batadv_hard_iface *hard_iface = NULL;
	struct net *net = sock_net(cb->skb->sk);
	struct net_device *soft_iface;
	struct batadv_priv *bat_priv;
	int ifindex;
	int ret = 0;

	ifindex = batadv_netlink_get_ifindex(cb->nlh, BATADV_ATTR_MESH_IFINDEX);
	if (!ifindex)
		return -EINVAL;

	soft_iface = dev_get_by_index(net, ifindex);
	if (!soft_iface || !batadv_softif_is_valid(soft_iface)) {
		ret = -ENODEV;
		goto out;
	}

	bat_priv = netdev_priv(soft_iface);

	hard_iface = batadv_primary_if_get_selected(bat_priv);
	if (!hard_iface || hard_iface->if_status != BATADV_IF_ACTIVE) {
		ret = -ENOENT;
		goto out;
	}

out:
	dev_put(soft_iface);

	if (!ret && primary_if)
		*primary_if = hard_iface;
	else
		batadv_hardif_put(hard_iface);

	return ret;
}

 
int batadv_mcast_flags_dump(struct sk_buff *msg, struct netlink_callback *cb)
{
	struct batadv_hard_iface *primary_if = NULL;
	int portid = NETLINK_CB(cb->skb).portid;
	struct batadv_priv *bat_priv;
	long *bucket = &cb->args[0];
	long *idx = &cb->args[1];
	int ret;

	ret = batadv_mcast_netlink_get_primary(cb, &primary_if);
	if (ret)
		return ret;

	bat_priv = netdev_priv(primary_if->soft_iface);
	ret = __batadv_mcast_flags_dump(msg, portid, cb, bat_priv, bucket, idx);

	batadv_hardif_put(primary_if);
	return ret;
}

 
void batadv_mcast_free(struct batadv_priv *bat_priv)
{
	cancel_delayed_work_sync(&bat_priv->mcast.work);

	batadv_tvlv_container_unregister(bat_priv, BATADV_TVLV_MCAST, 2);
	batadv_tvlv_handler_unregister(bat_priv, BATADV_TVLV_MCAST, 2);

	 
	batadv_mcast_mla_tt_retract(bat_priv, NULL);
}

 
void batadv_mcast_purge_orig(struct batadv_orig_node *orig)
{
	struct batadv_priv *bat_priv = orig->bat_priv;

	spin_lock_bh(&orig->mcast_handler_lock);

	batadv_mcast_want_unsnoop_update(bat_priv, orig, BATADV_NO_FLAGS);
	batadv_mcast_want_ipv4_update(bat_priv, orig, BATADV_NO_FLAGS);
	batadv_mcast_want_ipv6_update(bat_priv, orig, BATADV_NO_FLAGS);
	batadv_mcast_want_rtr4_update(bat_priv, orig,
				      BATADV_MCAST_WANT_NO_RTR4);
	batadv_mcast_want_rtr6_update(bat_priv, orig,
				      BATADV_MCAST_WANT_NO_RTR6);

	spin_unlock_bh(&orig->mcast_handler_lock);
}
