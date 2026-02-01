
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/filter.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <net/ip.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/icmpv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/socket.h>
#include <linux/ctype.h>
#include <linux/inet.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <asm/dma.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/igmp.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <linux/rtnetlink.h>
#include <linux/smp.h>
#include <linux/if_ether.h>
#include <net/arp.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/if_bonding.h>
#include <linux/phy.h>
#include <linux/jiffies.h>
#include <linux/preempt.h>
#include <net/route.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <net/pkt_sched.h>
#include <linux/rculist.h>
#include <net/flow_dissector.h>
#include <net/xfrm.h>
#include <net/bonding.h>
#include <net/bond_3ad.h>
#include <net/bond_alb.h>
#if IS_ENABLED(CONFIG_TLS_DEVICE)
#include <net/tls.h>
#endif
#include <net/ip6_route.h>
#include <net/xdp.h>

#include "bonding_priv.h"

 

 

static int max_bonds	= BOND_DEFAULT_MAX_BONDS;
static int tx_queues	= BOND_DEFAULT_TX_QUEUES;
static int num_peer_notif = 1;
static int miimon;
static int updelay;
static int downdelay;
static int use_carrier	= 1;
static char *mode;
static char *primary;
static char *primary_reselect;
static char *lacp_rate;
static int min_links;
static char *ad_select;
static char *xmit_hash_policy;
static int arp_interval;
static char *arp_ip_target[BOND_MAX_ARP_TARGETS];
static char *arp_validate;
static char *arp_all_targets;
static char *fail_over_mac;
static int all_slaves_active;
static struct bond_params bonding_defaults;
static int resend_igmp = BOND_DEFAULT_RESEND_IGMP;
static int packets_per_slave = 1;
static int lp_interval = BOND_ALB_DEFAULT_LP_INTERVAL;

module_param(max_bonds, int, 0);
MODULE_PARM_DESC(max_bonds, "Max number of bonded devices");
module_param(tx_queues, int, 0);
MODULE_PARM_DESC(tx_queues, "Max number of transmit queues (default = 16)");
module_param_named(num_grat_arp, num_peer_notif, int, 0644);
MODULE_PARM_DESC(num_grat_arp, "Number of peer notifications to send on "
			       "failover event (alias of num_unsol_na)");
module_param_named(num_unsol_na, num_peer_notif, int, 0644);
MODULE_PARM_DESC(num_unsol_na, "Number of peer notifications to send on "
			       "failover event (alias of num_grat_arp)");
module_param(miimon, int, 0);
MODULE_PARM_DESC(miimon, "Link check interval in milliseconds");
module_param(updelay, int, 0);
MODULE_PARM_DESC(updelay, "Delay before considering link up, in milliseconds");
module_param(downdelay, int, 0);
MODULE_PARM_DESC(downdelay, "Delay before considering link down, "
			    "in milliseconds");
module_param(use_carrier, int, 0);
MODULE_PARM_DESC(use_carrier, "Use netif_carrier_ok (vs MII ioctls) in miimon; "
			      "0 for off, 1 for on (default)");
module_param(mode, charp, 0);
MODULE_PARM_DESC(mode, "Mode of operation; 0 for balance-rr, "
		       "1 for active-backup, 2 for balance-xor, "
		       "3 for broadcast, 4 for 802.3ad, 5 for balance-tlb, "
		       "6 for balance-alb");
module_param(primary, charp, 0);
MODULE_PARM_DESC(primary, "Primary network device to use");
module_param(primary_reselect, charp, 0);
MODULE_PARM_DESC(primary_reselect, "Reselect primary slave "
				   "once it comes up; "
				   "0 for always (default), "
				   "1 for only if speed of primary is "
				   "better, "
				   "2 for only on active slave "
				   "failure");
module_param(lacp_rate, charp, 0);
MODULE_PARM_DESC(lacp_rate, "LACPDU tx rate to request from 802.3ad partner; "
			    "0 for slow, 1 for fast");
module_param(ad_select, charp, 0);
MODULE_PARM_DESC(ad_select, "802.3ad aggregation selection logic; "
			    "0 for stable (default), 1 for bandwidth, "
			    "2 for count");
module_param(min_links, int, 0);
MODULE_PARM_DESC(min_links, "Minimum number of available links before turning on carrier");

module_param(xmit_hash_policy, charp, 0);
MODULE_PARM_DESC(xmit_hash_policy, "balance-alb, balance-tlb, balance-xor, 802.3ad hashing method; "
				   "0 for layer 2 (default), 1 for layer 3+4, "
				   "2 for layer 2+3, 3 for encap layer 2+3, "
				   "4 for encap layer 3+4, 5 for vlan+srcmac");
module_param(arp_interval, int, 0);
MODULE_PARM_DESC(arp_interval, "arp interval in milliseconds");
module_param_array(arp_ip_target, charp, NULL, 0);
MODULE_PARM_DESC(arp_ip_target, "arp targets in n.n.n.n form");
module_param(arp_validate, charp, 0);
MODULE_PARM_DESC(arp_validate, "validate src/dst of ARP probes; "
			       "0 for none (default), 1 for active, "
			       "2 for backup, 3 for all");
module_param(arp_all_targets, charp, 0);
MODULE_PARM_DESC(arp_all_targets, "fail on any/all arp targets timeout; 0 for any (default), 1 for all");
module_param(fail_over_mac, charp, 0);
MODULE_PARM_DESC(fail_over_mac, "For active-backup, do not set all slaves to "
				"the same MAC; 0 for none (default), "
				"1 for active, 2 for follow");
module_param(all_slaves_active, int, 0);
MODULE_PARM_DESC(all_slaves_active, "Keep all frames received on an interface "
				     "by setting active flag for all slaves; "
				     "0 for never (default), 1 for always.");
module_param(resend_igmp, int, 0);
MODULE_PARM_DESC(resend_igmp, "Number of IGMP membership reports to send on "
			      "link failure");
module_param(packets_per_slave, int, 0);
MODULE_PARM_DESC(packets_per_slave, "Packets to send per slave in balance-rr "
				    "mode; 0 for a random slave, 1 packet per "
				    "slave (default), >1 packets per slave.");
module_param(lp_interval, uint, 0);
MODULE_PARM_DESC(lp_interval, "The number of seconds between instances where "
			      "the bonding driver sends learning packets to "
			      "each slaves peer switch. The default is 1.");

 

#ifdef CONFIG_NET_POLL_CONTROLLER
atomic_t netpoll_block_tx = ATOMIC_INIT(0);
#endif

unsigned int bond_net_id __read_mostly;

static const struct flow_dissector_key flow_keys_bonding_keys[] = {
	{
		.key_id = FLOW_DISSECTOR_KEY_CONTROL,
		.offset = offsetof(struct flow_keys, control),
	},
	{
		.key_id = FLOW_DISSECTOR_KEY_BASIC,
		.offset = offsetof(struct flow_keys, basic),
	},
	{
		.key_id = FLOW_DISSECTOR_KEY_IPV4_ADDRS,
		.offset = offsetof(struct flow_keys, addrs.v4addrs),
	},
	{
		.key_id = FLOW_DISSECTOR_KEY_IPV6_ADDRS,
		.offset = offsetof(struct flow_keys, addrs.v6addrs),
	},
	{
		.key_id = FLOW_DISSECTOR_KEY_TIPC,
		.offset = offsetof(struct flow_keys, addrs.tipckey),
	},
	{
		.key_id = FLOW_DISSECTOR_KEY_PORTS,
		.offset = offsetof(struct flow_keys, ports),
	},
	{
		.key_id = FLOW_DISSECTOR_KEY_ICMP,
		.offset = offsetof(struct flow_keys, icmp),
	},
	{
		.key_id = FLOW_DISSECTOR_KEY_VLAN,
		.offset = offsetof(struct flow_keys, vlan),
	},
	{
		.key_id = FLOW_DISSECTOR_KEY_FLOW_LABEL,
		.offset = offsetof(struct flow_keys, tags),
	},
	{
		.key_id = FLOW_DISSECTOR_KEY_GRE_KEYID,
		.offset = offsetof(struct flow_keys, keyid),
	},
};

static struct flow_dissector flow_keys_bonding __read_mostly;

 

static int bond_init(struct net_device *bond_dev);
static void bond_uninit(struct net_device *bond_dev);
static void bond_get_stats(struct net_device *bond_dev,
			   struct rtnl_link_stats64 *stats);
static void bond_slave_arr_handler(struct work_struct *work);
static bool bond_time_in_interval(struct bonding *bond, unsigned long last_act,
				  int mod);
static void bond_netdev_notify_work(struct work_struct *work);

 

const char *bond_mode_name(int mode)
{
	static const char *names[] = {
		[BOND_MODE_ROUNDROBIN] = "load balancing (round-robin)",
		[BOND_MODE_ACTIVEBACKUP] = "fault-tolerance (active-backup)",
		[BOND_MODE_XOR] = "load balancing (xor)",
		[BOND_MODE_BROADCAST] = "fault-tolerance (broadcast)",
		[BOND_MODE_8023AD] = "IEEE 802.3ad Dynamic link aggregation",
		[BOND_MODE_TLB] = "transmit load balancing",
		[BOND_MODE_ALB] = "adaptive load balancing",
	};

	if (mode < BOND_MODE_ROUNDROBIN || mode > BOND_MODE_ALB)
		return "unknown";

	return names[mode];
}

 
netdev_tx_t bond_dev_queue_xmit(struct bonding *bond, struct sk_buff *skb,
			struct net_device *slave_dev)
{
	skb->dev = slave_dev;

	BUILD_BUG_ON(sizeof(skb->queue_mapping) !=
		     sizeof(qdisc_skb_cb(skb)->slave_dev_queue_mapping));
	skb_set_queue_mapping(skb, qdisc_skb_cb(skb)->slave_dev_queue_mapping);

	if (unlikely(netpoll_tx_running(bond->dev)))
		return bond_netpoll_send_skb(bond_get_slave_by_dev(bond, slave_dev), skb);

	return dev_queue_xmit(skb);
}

static bool bond_sk_check(struct bonding *bond)
{
	switch (BOND_MODE(bond)) {
	case BOND_MODE_8023AD:
	case BOND_MODE_XOR:
		if (bond->params.xmit_policy == BOND_XMIT_POLICY_LAYER34)
			return true;
		fallthrough;
	default:
		return false;
	}
}

static bool bond_xdp_check(struct bonding *bond)
{
	switch (BOND_MODE(bond)) {
	case BOND_MODE_ROUNDROBIN:
	case BOND_MODE_ACTIVEBACKUP:
		return true;
	case BOND_MODE_8023AD:
	case BOND_MODE_XOR:
		 
		if (bond->params.xmit_policy != BOND_XMIT_POLICY_VLAN_SRCMAC)
			return true;
		fallthrough;
	default:
		return false;
	}
}

 

 

 
static int bond_vlan_rx_add_vid(struct net_device *bond_dev,
				__be16 proto, u16 vid)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct slave *slave, *rollback_slave;
	struct list_head *iter;
	int res;

	bond_for_each_slave(bond, slave, iter) {
		res = vlan_vid_add(slave->dev, proto, vid);
		if (res)
			goto unwind;
	}

	return 0;

unwind:
	 
	bond_for_each_slave(bond, rollback_slave, iter) {
		if (rollback_slave == slave)
			break;

		vlan_vid_del(rollback_slave->dev, proto, vid);
	}

	return res;
}

 
static int bond_vlan_rx_kill_vid(struct net_device *bond_dev,
				 __be16 proto, u16 vid)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct list_head *iter;
	struct slave *slave;

	bond_for_each_slave(bond, slave, iter)
		vlan_vid_del(slave->dev, proto, vid);

	if (bond_is_lb(bond))
		bond_alb_clear_vlan(bond, vid);

	return 0;
}

 

#ifdef CONFIG_XFRM_OFFLOAD
 
static int bond_ipsec_add_sa(struct xfrm_state *xs,
			     struct netlink_ext_ack *extack)
{
	struct net_device *bond_dev = xs->xso.dev;
	struct bond_ipsec *ipsec;
	struct bonding *bond;
	struct slave *slave;
	int err;

	if (!bond_dev)
		return -EINVAL;

	rcu_read_lock();
	bond = netdev_priv(bond_dev);
	slave = rcu_dereference(bond->curr_active_slave);
	if (!slave) {
		rcu_read_unlock();
		return -ENODEV;
	}

	if (!slave->dev->xfrmdev_ops ||
	    !slave->dev->xfrmdev_ops->xdo_dev_state_add ||
	    netif_is_bond_master(slave->dev)) {
		NL_SET_ERR_MSG_MOD(extack, "Slave does not support ipsec offload");
		rcu_read_unlock();
		return -EINVAL;
	}

	ipsec = kmalloc(sizeof(*ipsec), GFP_ATOMIC);
	if (!ipsec) {
		rcu_read_unlock();
		return -ENOMEM;
	}
	xs->xso.real_dev = slave->dev;

	err = slave->dev->xfrmdev_ops->xdo_dev_state_add(xs, extack);
	if (!err) {
		ipsec->xs = xs;
		INIT_LIST_HEAD(&ipsec->list);
		spin_lock_bh(&bond->ipsec_lock);
		list_add(&ipsec->list, &bond->ipsec_list);
		spin_unlock_bh(&bond->ipsec_lock);
	} else {
		kfree(ipsec);
	}
	rcu_read_unlock();
	return err;
}

static void bond_ipsec_add_sa_all(struct bonding *bond)
{
	struct net_device *bond_dev = bond->dev;
	struct bond_ipsec *ipsec;
	struct slave *slave;

	rcu_read_lock();
	slave = rcu_dereference(bond->curr_active_slave);
	if (!slave)
		goto out;

	if (!slave->dev->xfrmdev_ops ||
	    !slave->dev->xfrmdev_ops->xdo_dev_state_add ||
	    netif_is_bond_master(slave->dev)) {
		spin_lock_bh(&bond->ipsec_lock);
		if (!list_empty(&bond->ipsec_list))
			slave_warn(bond_dev, slave->dev,
				   "%s: no slave xdo_dev_state_add\n",
				   __func__);
		spin_unlock_bh(&bond->ipsec_lock);
		goto out;
	}

	spin_lock_bh(&bond->ipsec_lock);
	list_for_each_entry(ipsec, &bond->ipsec_list, list) {
		ipsec->xs->xso.real_dev = slave->dev;
		if (slave->dev->xfrmdev_ops->xdo_dev_state_add(ipsec->xs, NULL)) {
			slave_warn(bond_dev, slave->dev, "%s: failed to add SA\n", __func__);
			ipsec->xs->xso.real_dev = NULL;
		}
	}
	spin_unlock_bh(&bond->ipsec_lock);
out:
	rcu_read_unlock();
}

 
static void bond_ipsec_del_sa(struct xfrm_state *xs)
{
	struct net_device *bond_dev = xs->xso.dev;
	struct bond_ipsec *ipsec;
	struct bonding *bond;
	struct slave *slave;

	if (!bond_dev)
		return;

	rcu_read_lock();
	bond = netdev_priv(bond_dev);
	slave = rcu_dereference(bond->curr_active_slave);

	if (!slave)
		goto out;

	if (!xs->xso.real_dev)
		goto out;

	WARN_ON(xs->xso.real_dev != slave->dev);

	if (!slave->dev->xfrmdev_ops ||
	    !slave->dev->xfrmdev_ops->xdo_dev_state_delete ||
	    netif_is_bond_master(slave->dev)) {
		slave_warn(bond_dev, slave->dev, "%s: no slave xdo_dev_state_delete\n", __func__);
		goto out;
	}

	slave->dev->xfrmdev_ops->xdo_dev_state_delete(xs);
out:
	spin_lock_bh(&bond->ipsec_lock);
	list_for_each_entry(ipsec, &bond->ipsec_list, list) {
		if (ipsec->xs == xs) {
			list_del(&ipsec->list);
			kfree(ipsec);
			break;
		}
	}
	spin_unlock_bh(&bond->ipsec_lock);
	rcu_read_unlock();
}

static void bond_ipsec_del_sa_all(struct bonding *bond)
{
	struct net_device *bond_dev = bond->dev;
	struct bond_ipsec *ipsec;
	struct slave *slave;

	rcu_read_lock();
	slave = rcu_dereference(bond->curr_active_slave);
	if (!slave) {
		rcu_read_unlock();
		return;
	}

	spin_lock_bh(&bond->ipsec_lock);
	list_for_each_entry(ipsec, &bond->ipsec_list, list) {
		if (!ipsec->xs->xso.real_dev)
			continue;

		if (!slave->dev->xfrmdev_ops ||
		    !slave->dev->xfrmdev_ops->xdo_dev_state_delete ||
		    netif_is_bond_master(slave->dev)) {
			slave_warn(bond_dev, slave->dev,
				   "%s: no slave xdo_dev_state_delete\n",
				   __func__);
		} else {
			slave->dev->xfrmdev_ops->xdo_dev_state_delete(ipsec->xs);
		}
		ipsec->xs->xso.real_dev = NULL;
	}
	spin_unlock_bh(&bond->ipsec_lock);
	rcu_read_unlock();
}

 
static bool bond_ipsec_offload_ok(struct sk_buff *skb, struct xfrm_state *xs)
{
	struct net_device *bond_dev = xs->xso.dev;
	struct net_device *real_dev;
	struct slave *curr_active;
	struct bonding *bond;
	int err;

	bond = netdev_priv(bond_dev);
	rcu_read_lock();
	curr_active = rcu_dereference(bond->curr_active_slave);
	real_dev = curr_active->dev;

	if (BOND_MODE(bond) != BOND_MODE_ACTIVEBACKUP) {
		err = false;
		goto out;
	}

	if (!xs->xso.real_dev) {
		err = false;
		goto out;
	}

	if (!real_dev->xfrmdev_ops ||
	    !real_dev->xfrmdev_ops->xdo_dev_offload_ok ||
	    netif_is_bond_master(real_dev)) {
		err = false;
		goto out;
	}

	err = real_dev->xfrmdev_ops->xdo_dev_offload_ok(skb, xs);
out:
	rcu_read_unlock();
	return err;
}

static const struct xfrmdev_ops bond_xfrmdev_ops = {
	.xdo_dev_state_add = bond_ipsec_add_sa,
	.xdo_dev_state_delete = bond_ipsec_del_sa,
	.xdo_dev_offload_ok = bond_ipsec_offload_ok,
};
#endif  

 

 
int bond_set_carrier(struct bonding *bond)
{
	struct list_head *iter;
	struct slave *slave;

	if (!bond_has_slaves(bond))
		goto down;

	if (BOND_MODE(bond) == BOND_MODE_8023AD)
		return bond_3ad_set_carrier(bond);

	bond_for_each_slave(bond, slave, iter) {
		if (slave->link == BOND_LINK_UP) {
			if (!netif_carrier_ok(bond->dev)) {
				netif_carrier_on(bond->dev);
				return 1;
			}
			return 0;
		}
	}

down:
	if (netif_carrier_ok(bond->dev)) {
		netif_carrier_off(bond->dev);
		return 1;
	}
	return 0;
}

 
static int bond_update_speed_duplex(struct slave *slave)
{
	struct net_device *slave_dev = slave->dev;
	struct ethtool_link_ksettings ecmd;
	int res;

	slave->speed = SPEED_UNKNOWN;
	slave->duplex = DUPLEX_UNKNOWN;

	res = __ethtool_get_link_ksettings(slave_dev, &ecmd);
	if (res < 0)
		return 1;
	if (ecmd.base.speed == 0 || ecmd.base.speed == ((__u32)-1))
		return 1;
	switch (ecmd.base.duplex) {
	case DUPLEX_FULL:
	case DUPLEX_HALF:
		break;
	default:
		return 1;
	}

	slave->speed = ecmd.base.speed;
	slave->duplex = ecmd.base.duplex;

	return 0;
}

const char *bond_slave_link_status(s8 link)
{
	switch (link) {
	case BOND_LINK_UP:
		return "up";
	case BOND_LINK_FAIL:
		return "going down";
	case BOND_LINK_DOWN:
		return "down";
	case BOND_LINK_BACK:
		return "going back";
	default:
		return "unknown";
	}
}

 
static int bond_check_dev_link(struct bonding *bond,
			       struct net_device *slave_dev, int reporting)
{
	const struct net_device_ops *slave_ops = slave_dev->netdev_ops;
	int (*ioctl)(struct net_device *, struct ifreq *, int);
	struct ifreq ifr;
	struct mii_ioctl_data *mii;

	if (!reporting && !netif_running(slave_dev))
		return 0;

	if (bond->params.use_carrier)
		return netif_carrier_ok(slave_dev) ? BMSR_LSTATUS : 0;

	 
	if (slave_dev->ethtool_ops->get_link)
		return slave_dev->ethtool_ops->get_link(slave_dev) ?
			BMSR_LSTATUS : 0;

	 
	ioctl = slave_ops->ndo_eth_ioctl;
	if (ioctl) {
		 

		 

		 
		strscpy_pad(ifr.ifr_name, slave_dev->name, IFNAMSIZ);
		mii = if_mii(&ifr);
		if (ioctl(slave_dev, &ifr, SIOCGMIIPHY) == 0) {
			mii->reg_num = MII_BMSR;
			if (ioctl(slave_dev, &ifr, SIOCGMIIREG) == 0)
				return mii->val_out & BMSR_LSTATUS;
		}
	}

	 
	return reporting ? -1 : BMSR_LSTATUS;
}

 

 
static int bond_set_promiscuity(struct bonding *bond, int inc)
{
	struct list_head *iter;
	int err = 0;

	if (bond_uses_primary(bond)) {
		struct slave *curr_active = rtnl_dereference(bond->curr_active_slave);

		if (curr_active)
			err = dev_set_promiscuity(curr_active->dev, inc);
	} else {
		struct slave *slave;

		bond_for_each_slave(bond, slave, iter) {
			err = dev_set_promiscuity(slave->dev, inc);
			if (err)
				return err;
		}
	}
	return err;
}

 
static int bond_set_allmulti(struct bonding *bond, int inc)
{
	struct list_head *iter;
	int err = 0;

	if (bond_uses_primary(bond)) {
		struct slave *curr_active = rtnl_dereference(bond->curr_active_slave);

		if (curr_active)
			err = dev_set_allmulti(curr_active->dev, inc);
	} else {
		struct slave *slave;

		bond_for_each_slave(bond, slave, iter) {
			err = dev_set_allmulti(slave->dev, inc);
			if (err)
				return err;
		}
	}
	return err;
}

 
static void bond_resend_igmp_join_requests_delayed(struct work_struct *work)
{
	struct bonding *bond = container_of(work, struct bonding,
					    mcast_work.work);

	if (!rtnl_trylock()) {
		queue_delayed_work(bond->wq, &bond->mcast_work, 1);
		return;
	}
	call_netdevice_notifiers(NETDEV_RESEND_IGMP, bond->dev);

	if (bond->igmp_retrans > 1) {
		bond->igmp_retrans--;
		queue_delayed_work(bond->wq, &bond->mcast_work, HZ/5);
	}
	rtnl_unlock();
}

 
static void bond_hw_addr_flush(struct net_device *bond_dev,
			       struct net_device *slave_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);

	dev_uc_unsync(slave_dev, bond_dev);
	dev_mc_unsync(slave_dev, bond_dev);

	if (BOND_MODE(bond) == BOND_MODE_8023AD)
		dev_mc_del(slave_dev, lacpdu_mcast_addr);
}

 

 
static void bond_hw_addr_swap(struct bonding *bond, struct slave *new_active,
			      struct slave *old_active)
{
	if (old_active) {
		if (bond->dev->flags & IFF_PROMISC)
			dev_set_promiscuity(old_active->dev, -1);

		if (bond->dev->flags & IFF_ALLMULTI)
			dev_set_allmulti(old_active->dev, -1);

		if (bond->dev->flags & IFF_UP)
			bond_hw_addr_flush(bond->dev, old_active->dev);
	}

	if (new_active) {
		 
		if (bond->dev->flags & IFF_PROMISC)
			dev_set_promiscuity(new_active->dev, 1);

		if (bond->dev->flags & IFF_ALLMULTI)
			dev_set_allmulti(new_active->dev, 1);

		if (bond->dev->flags & IFF_UP) {
			netif_addr_lock_bh(bond->dev);
			dev_uc_sync(new_active->dev, bond->dev);
			dev_mc_sync(new_active->dev, bond->dev);
			netif_addr_unlock_bh(bond->dev);
		}
	}
}

 
static int bond_set_dev_addr(struct net_device *bond_dev,
			     struct net_device *slave_dev)
{
	int err;

	slave_dbg(bond_dev, slave_dev, "bond_dev=%p slave_dev=%p slave_dev->addr_len=%d\n",
		  bond_dev, slave_dev, slave_dev->addr_len);
	err = dev_pre_changeaddr_notify(bond_dev, slave_dev->dev_addr, NULL);
	if (err)
		return err;

	__dev_addr_set(bond_dev, slave_dev->dev_addr, slave_dev->addr_len);
	bond_dev->addr_assign_type = NET_ADDR_STOLEN;
	call_netdevice_notifiers(NETDEV_CHANGEADDR, bond_dev);
	return 0;
}

static struct slave *bond_get_old_active(struct bonding *bond,
					 struct slave *new_active)
{
	struct slave *slave;
	struct list_head *iter;

	bond_for_each_slave(bond, slave, iter) {
		if (slave == new_active)
			continue;

		if (ether_addr_equal(bond->dev->dev_addr, slave->dev->dev_addr))
			return slave;
	}

	return NULL;
}

 
static void bond_do_fail_over_mac(struct bonding *bond,
				  struct slave *new_active,
				  struct slave *old_active)
{
	u8 tmp_mac[MAX_ADDR_LEN];
	struct sockaddr_storage ss;
	int rv;

	switch (bond->params.fail_over_mac) {
	case BOND_FOM_ACTIVE:
		if (new_active) {
			rv = bond_set_dev_addr(bond->dev, new_active->dev);
			if (rv)
				slave_err(bond->dev, new_active->dev, "Error %d setting bond MAC from slave\n",
					  -rv);
		}
		break;
	case BOND_FOM_FOLLOW:
		 
		if (!new_active)
			return;

		if (!old_active)
			old_active = bond_get_old_active(bond, new_active);

		if (old_active) {
			bond_hw_addr_copy(tmp_mac, new_active->dev->dev_addr,
					  new_active->dev->addr_len);
			bond_hw_addr_copy(ss.__data,
					  old_active->dev->dev_addr,
					  old_active->dev->addr_len);
			ss.ss_family = new_active->dev->type;
		} else {
			bond_hw_addr_copy(ss.__data, bond->dev->dev_addr,
					  bond->dev->addr_len);
			ss.ss_family = bond->dev->type;
		}

		rv = dev_set_mac_address(new_active->dev,
					 (struct sockaddr *)&ss, NULL);
		if (rv) {
			slave_err(bond->dev, new_active->dev, "Error %d setting MAC of new active slave\n",
				  -rv);
			goto out;
		}

		if (!old_active)
			goto out;

		bond_hw_addr_copy(ss.__data, tmp_mac,
				  new_active->dev->addr_len);
		ss.ss_family = old_active->dev->type;

		rv = dev_set_mac_address(old_active->dev,
					 (struct sockaddr *)&ss, NULL);
		if (rv)
			slave_err(bond->dev, old_active->dev, "Error %d setting MAC of old active slave\n",
				  -rv);
out:
		break;
	default:
		netdev_err(bond->dev, "bond_do_fail_over_mac impossible: bad policy %d\n",
			   bond->params.fail_over_mac);
		break;
	}

}

 
static struct slave *bond_choose_primary_or_current(struct bonding *bond)
{
	struct slave *prim = rtnl_dereference(bond->primary_slave);
	struct slave *curr = rtnl_dereference(bond->curr_active_slave);
	struct slave *slave, *hprio = NULL;
	struct list_head *iter;

	if (!prim || prim->link != BOND_LINK_UP) {
		bond_for_each_slave(bond, slave, iter) {
			if (slave->link == BOND_LINK_UP) {
				hprio = hprio ?: slave;
				if (slave->prio > hprio->prio)
					hprio = slave;
			}
		}

		if (hprio && hprio != curr) {
			prim = hprio;
			goto link_reselect;
		}

		if (!curr || curr->link != BOND_LINK_UP)
			return NULL;
		return curr;
	}

	if (bond->force_primary) {
		bond->force_primary = false;
		return prim;
	}

link_reselect:
	if (!curr || curr->link != BOND_LINK_UP)
		return prim;

	 
	switch (bond->params.primary_reselect) {
	case BOND_PRI_RESELECT_ALWAYS:
		return prim;
	case BOND_PRI_RESELECT_BETTER:
		if (prim->speed < curr->speed)
			return curr;
		if (prim->speed == curr->speed && prim->duplex <= curr->duplex)
			return curr;
		return prim;
	case BOND_PRI_RESELECT_FAILURE:
		return curr;
	default:
		netdev_err(bond->dev, "impossible primary_reselect %d\n",
			   bond->params.primary_reselect);
		return curr;
	}
}

 
static struct slave *bond_find_best_slave(struct bonding *bond)
{
	struct slave *slave, *bestslave = NULL;
	struct list_head *iter;
	int mintime = bond->params.updelay;

	slave = bond_choose_primary_or_current(bond);
	if (slave)
		return slave;

	bond_for_each_slave(bond, slave, iter) {
		if (slave->link == BOND_LINK_UP)
			return slave;
		if (slave->link == BOND_LINK_BACK && bond_slave_is_up(slave) &&
		    slave->delay < mintime) {
			mintime = slave->delay;
			bestslave = slave;
		}
	}

	return bestslave;
}

static bool bond_should_notify_peers(struct bonding *bond)
{
	struct slave *slave;

	rcu_read_lock();
	slave = rcu_dereference(bond->curr_active_slave);
	rcu_read_unlock();

	if (!slave || !bond->send_peer_notif ||
	    bond->send_peer_notif %
	    max(1, bond->params.peer_notif_delay) != 0 ||
	    !netif_carrier_ok(bond->dev) ||
	    test_bit(__LINK_STATE_LINKWATCH_PENDING, &slave->dev->state))
		return false;

	netdev_dbg(bond->dev, "bond_should_notify_peers: slave %s\n",
		   slave ? slave->dev->name : "NULL");

	return true;
}

 
void bond_change_active_slave(struct bonding *bond, struct slave *new_active)
{
	struct slave *old_active;

	ASSERT_RTNL();

	old_active = rtnl_dereference(bond->curr_active_slave);

	if (old_active == new_active)
		return;

#ifdef CONFIG_XFRM_OFFLOAD
	bond_ipsec_del_sa_all(bond);
#endif  

	if (new_active) {
		new_active->last_link_up = jiffies;

		if (new_active->link == BOND_LINK_BACK) {
			if (bond_uses_primary(bond)) {
				slave_info(bond->dev, new_active->dev, "making interface the new active one %d ms earlier\n",
					   (bond->params.updelay - new_active->delay) * bond->params.miimon);
			}

			new_active->delay = 0;
			bond_set_slave_link_state(new_active, BOND_LINK_UP,
						  BOND_SLAVE_NOTIFY_NOW);

			if (BOND_MODE(bond) == BOND_MODE_8023AD)
				bond_3ad_handle_link_change(new_active, BOND_LINK_UP);

			if (bond_is_lb(bond))
				bond_alb_handle_link_change(bond, new_active, BOND_LINK_UP);
		} else {
			if (bond_uses_primary(bond))
				slave_info(bond->dev, new_active->dev, "making interface the new active one\n");
		}
	}

	if (bond_uses_primary(bond))
		bond_hw_addr_swap(bond, new_active, old_active);

	if (bond_is_lb(bond)) {
		bond_alb_handle_active_change(bond, new_active);
		if (old_active)
			bond_set_slave_inactive_flags(old_active,
						      BOND_SLAVE_NOTIFY_NOW);
		if (new_active)
			bond_set_slave_active_flags(new_active,
						    BOND_SLAVE_NOTIFY_NOW);
	} else {
		rcu_assign_pointer(bond->curr_active_slave, new_active);
	}

	if (BOND_MODE(bond) == BOND_MODE_ACTIVEBACKUP) {
		if (old_active)
			bond_set_slave_inactive_flags(old_active,
						      BOND_SLAVE_NOTIFY_NOW);

		if (new_active) {
			bool should_notify_peers = false;

			bond_set_slave_active_flags(new_active,
						    BOND_SLAVE_NOTIFY_NOW);

			if (bond->params.fail_over_mac)
				bond_do_fail_over_mac(bond, new_active,
						      old_active);

			if (netif_running(bond->dev)) {
				bond->send_peer_notif =
					bond->params.num_peer_notif *
					max(1, bond->params.peer_notif_delay);
				should_notify_peers =
					bond_should_notify_peers(bond);
			}

			call_netdevice_notifiers(NETDEV_BONDING_FAILOVER, bond->dev);
			if (should_notify_peers) {
				bond->send_peer_notif--;
				call_netdevice_notifiers(NETDEV_NOTIFY_PEERS,
							 bond->dev);
			}
		}
	}

#ifdef CONFIG_XFRM_OFFLOAD
	bond_ipsec_add_sa_all(bond);
#endif  

	 
	if (netif_running(bond->dev) && (bond->params.resend_igmp > 0) &&
	    ((bond_uses_primary(bond) && new_active) ||
	     BOND_MODE(bond) == BOND_MODE_ROUNDROBIN)) {
		bond->igmp_retrans = bond->params.resend_igmp;
		queue_delayed_work(bond->wq, &bond->mcast_work, 1);
	}
}

 
void bond_select_active_slave(struct bonding *bond)
{
	struct slave *best_slave;
	int rv;

	ASSERT_RTNL();

	best_slave = bond_find_best_slave(bond);
	if (best_slave != rtnl_dereference(bond->curr_active_slave)) {
		bond_change_active_slave(bond, best_slave);
		rv = bond_set_carrier(bond);
		if (!rv)
			return;

		if (netif_carrier_ok(bond->dev))
			netdev_info(bond->dev, "active interface up!\n");
		else
			netdev_info(bond->dev, "now running without any active interface!\n");
	}
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static inline int slave_enable_netpoll(struct slave *slave)
{
	struct netpoll *np;
	int err = 0;

	np = kzalloc(sizeof(*np), GFP_KERNEL);
	err = -ENOMEM;
	if (!np)
		goto out;

	err = __netpoll_setup(np, slave->dev);
	if (err) {
		kfree(np);
		goto out;
	}
	slave->np = np;
out:
	return err;
}
static inline void slave_disable_netpoll(struct slave *slave)
{
	struct netpoll *np = slave->np;

	if (!np)
		return;

	slave->np = NULL;

	__netpoll_free(np);
}

static void bond_poll_controller(struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct slave *slave = NULL;
	struct list_head *iter;
	struct ad_info ad_info;

	if (BOND_MODE(bond) == BOND_MODE_8023AD)
		if (bond_3ad_get_active_agg_info(bond, &ad_info))
			return;

	bond_for_each_slave_rcu(bond, slave, iter) {
		if (!bond_slave_is_up(slave))
			continue;

		if (BOND_MODE(bond) == BOND_MODE_8023AD) {
			struct aggregator *agg =
			    SLAVE_AD_INFO(slave)->port.aggregator;

			if (agg &&
			    agg->aggregator_identifier != ad_info.aggregator_id)
				continue;
		}

		netpoll_poll_dev(slave->dev);
	}
}

static void bond_netpoll_cleanup(struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct list_head *iter;
	struct slave *slave;

	bond_for_each_slave(bond, slave, iter)
		if (bond_slave_is_up(slave))
			slave_disable_netpoll(slave);
}

static int bond_netpoll_setup(struct net_device *dev, struct netpoll_info *ni)
{
	struct bonding *bond = netdev_priv(dev);
	struct list_head *iter;
	struct slave *slave;
	int err = 0;

	bond_for_each_slave(bond, slave, iter) {
		err = slave_enable_netpoll(slave);
		if (err) {
			bond_netpoll_cleanup(dev);
			break;
		}
	}
	return err;
}
#else
static inline int slave_enable_netpoll(struct slave *slave)
{
	return 0;
}
static inline void slave_disable_netpoll(struct slave *slave)
{
}
static void bond_netpoll_cleanup(struct net_device *bond_dev)
{
}
#endif

 

static netdev_features_t bond_fix_features(struct net_device *dev,
					   netdev_features_t features)
{
	struct bonding *bond = netdev_priv(dev);
	struct list_head *iter;
	netdev_features_t mask;
	struct slave *slave;

	mask = features;

	features &= ~NETIF_F_ONE_FOR_ALL;
	features |= NETIF_F_ALL_FOR_ALL;

	bond_for_each_slave(bond, slave, iter) {
		features = netdev_increment_features(features,
						     slave->dev->features,
						     mask);
	}
	features = netdev_add_tso_features(features, mask);

	return features;
}

#define BOND_VLAN_FEATURES	(NETIF_F_HW_CSUM | NETIF_F_SG | \
				 NETIF_F_FRAGLIST | NETIF_F_GSO_SOFTWARE | \
				 NETIF_F_HIGHDMA | NETIF_F_LRO)

#define BOND_ENC_FEATURES	(NETIF_F_HW_CSUM | NETIF_F_SG | \
				 NETIF_F_RXCSUM | NETIF_F_GSO_SOFTWARE)

#define BOND_MPLS_FEATURES	(NETIF_F_HW_CSUM | NETIF_F_SG | \
				 NETIF_F_GSO_SOFTWARE)


static void bond_compute_features(struct bonding *bond)
{
	unsigned int dst_release_flag = IFF_XMIT_DST_RELEASE |
					IFF_XMIT_DST_RELEASE_PERM;
	netdev_features_t vlan_features = BOND_VLAN_FEATURES;
	netdev_features_t enc_features  = BOND_ENC_FEATURES;
#ifdef CONFIG_XFRM_OFFLOAD
	netdev_features_t xfrm_features  = BOND_XFRM_FEATURES;
#endif  
	netdev_features_t mpls_features  = BOND_MPLS_FEATURES;
	struct net_device *bond_dev = bond->dev;
	struct list_head *iter;
	struct slave *slave;
	unsigned short max_hard_header_len = ETH_HLEN;
	unsigned int tso_max_size = TSO_MAX_SIZE;
	u16 tso_max_segs = TSO_MAX_SEGS;

	if (!bond_has_slaves(bond))
		goto done;
	vlan_features &= NETIF_F_ALL_FOR_ALL;
	mpls_features &= NETIF_F_ALL_FOR_ALL;

	bond_for_each_slave(bond, slave, iter) {
		vlan_features = netdev_increment_features(vlan_features,
			slave->dev->vlan_features, BOND_VLAN_FEATURES);

		enc_features = netdev_increment_features(enc_features,
							 slave->dev->hw_enc_features,
							 BOND_ENC_FEATURES);

#ifdef CONFIG_XFRM_OFFLOAD
		xfrm_features = netdev_increment_features(xfrm_features,
							  slave->dev->hw_enc_features,
							  BOND_XFRM_FEATURES);
#endif  

		mpls_features = netdev_increment_features(mpls_features,
							  slave->dev->mpls_features,
							  BOND_MPLS_FEATURES);

		dst_release_flag &= slave->dev->priv_flags;
		if (slave->dev->hard_header_len > max_hard_header_len)
			max_hard_header_len = slave->dev->hard_header_len;

		tso_max_size = min(tso_max_size, slave->dev->tso_max_size);
		tso_max_segs = min(tso_max_segs, slave->dev->tso_max_segs);
	}
	bond_dev->hard_header_len = max_hard_header_len;

done:
	bond_dev->vlan_features = vlan_features;
	bond_dev->hw_enc_features = enc_features | NETIF_F_GSO_ENCAP_ALL |
				    NETIF_F_HW_VLAN_CTAG_TX |
				    NETIF_F_HW_VLAN_STAG_TX;
#ifdef CONFIG_XFRM_OFFLOAD
	bond_dev->hw_enc_features |= xfrm_features;
#endif  
	bond_dev->mpls_features = mpls_features;
	netif_set_tso_max_segs(bond_dev, tso_max_segs);
	netif_set_tso_max_size(bond_dev, tso_max_size);

	bond_dev->priv_flags &= ~IFF_XMIT_DST_RELEASE;
	if ((bond_dev->priv_flags & IFF_XMIT_DST_RELEASE_PERM) &&
	    dst_release_flag == (IFF_XMIT_DST_RELEASE | IFF_XMIT_DST_RELEASE_PERM))
		bond_dev->priv_flags |= IFF_XMIT_DST_RELEASE;

	netdev_change_features(bond_dev);
}

static void bond_setup_by_slave(struct net_device *bond_dev,
				struct net_device *slave_dev)
{
	bool was_up = !!(bond_dev->flags & IFF_UP);

	dev_close(bond_dev);

	bond_dev->header_ops	    = slave_dev->header_ops;

	bond_dev->type		    = slave_dev->type;
	bond_dev->hard_header_len   = slave_dev->hard_header_len;
	bond_dev->needed_headroom   = slave_dev->needed_headroom;
	bond_dev->addr_len	    = slave_dev->addr_len;

	memcpy(bond_dev->broadcast, slave_dev->broadcast,
		slave_dev->addr_len);

	if (slave_dev->flags & IFF_POINTOPOINT) {
		bond_dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);
		bond_dev->flags |= (IFF_POINTOPOINT | IFF_NOARP);
	}
	if (was_up)
		dev_open(bond_dev, NULL);
}

 
static bool bond_should_deliver_exact_match(struct sk_buff *skb,
					    struct slave *slave,
					    struct bonding *bond)
{
	if (bond_is_slave_inactive(slave)) {
		if (BOND_MODE(bond) == BOND_MODE_ALB &&
		    skb->pkt_type != PACKET_BROADCAST &&
		    skb->pkt_type != PACKET_MULTICAST)
			return false;
		return true;
	}
	return false;
}

static rx_handler_result_t bond_handle_frame(struct sk_buff **pskb)
{
	struct sk_buff *skb = *pskb;
	struct slave *slave;
	struct bonding *bond;
	int (*recv_probe)(const struct sk_buff *, struct bonding *,
			  struct slave *);
	int ret = RX_HANDLER_ANOTHER;

	skb = skb_share_check(skb, GFP_ATOMIC);
	if (unlikely(!skb))
		return RX_HANDLER_CONSUMED;

	*pskb = skb;

	slave = bond_slave_get_rcu(skb->dev);
	bond = slave->bond;

	recv_probe = READ_ONCE(bond->recv_probe);
	if (recv_probe) {
		ret = recv_probe(skb, bond, slave);
		if (ret == RX_HANDLER_CONSUMED) {
			consume_skb(skb);
			return ret;
		}
	}

	 
	if (bond_should_deliver_exact_match(skb, slave, bond)) {
		if (is_link_local_ether_addr(eth_hdr(skb)->h_dest))
			return RX_HANDLER_PASS;
		return RX_HANDLER_EXACT;
	}

	skb->dev = bond->dev;

	if (BOND_MODE(bond) == BOND_MODE_ALB &&
	    netif_is_bridge_port(bond->dev) &&
	    skb->pkt_type == PACKET_HOST) {

		if (unlikely(skb_cow_head(skb,
					  skb->data - skb_mac_header(skb)))) {
			kfree_skb(skb);
			return RX_HANDLER_CONSUMED;
		}
		bond_hw_addr_copy(eth_hdr(skb)->h_dest, bond->dev->dev_addr,
				  bond->dev->addr_len);
	}

	return ret;
}

static enum netdev_lag_tx_type bond_lag_tx_type(struct bonding *bond)
{
	switch (BOND_MODE(bond)) {
	case BOND_MODE_ROUNDROBIN:
		return NETDEV_LAG_TX_TYPE_ROUNDROBIN;
	case BOND_MODE_ACTIVEBACKUP:
		return NETDEV_LAG_TX_TYPE_ACTIVEBACKUP;
	case BOND_MODE_BROADCAST:
		return NETDEV_LAG_TX_TYPE_BROADCAST;
	case BOND_MODE_XOR:
	case BOND_MODE_8023AD:
		return NETDEV_LAG_TX_TYPE_HASH;
	default:
		return NETDEV_LAG_TX_TYPE_UNKNOWN;
	}
}

static enum netdev_lag_hash bond_lag_hash_type(struct bonding *bond,
					       enum netdev_lag_tx_type type)
{
	if (type != NETDEV_LAG_TX_TYPE_HASH)
		return NETDEV_LAG_HASH_NONE;

	switch (bond->params.xmit_policy) {
	case BOND_XMIT_POLICY_LAYER2:
		return NETDEV_LAG_HASH_L2;
	case BOND_XMIT_POLICY_LAYER34:
		return NETDEV_LAG_HASH_L34;
	case BOND_XMIT_POLICY_LAYER23:
		return NETDEV_LAG_HASH_L23;
	case BOND_XMIT_POLICY_ENCAP23:
		return NETDEV_LAG_HASH_E23;
	case BOND_XMIT_POLICY_ENCAP34:
		return NETDEV_LAG_HASH_E34;
	case BOND_XMIT_POLICY_VLAN_SRCMAC:
		return NETDEV_LAG_HASH_VLAN_SRCMAC;
	default:
		return NETDEV_LAG_HASH_UNKNOWN;
	}
}

static int bond_master_upper_dev_link(struct bonding *bond, struct slave *slave,
				      struct netlink_ext_ack *extack)
{
	struct netdev_lag_upper_info lag_upper_info;
	enum netdev_lag_tx_type type;
	int err;

	type = bond_lag_tx_type(bond);
	lag_upper_info.tx_type = type;
	lag_upper_info.hash_type = bond_lag_hash_type(bond, type);

	err = netdev_master_upper_dev_link(slave->dev, bond->dev, slave,
					   &lag_upper_info, extack);
	if (err)
		return err;

	slave->dev->flags |= IFF_SLAVE;
	return 0;
}

static void bond_upper_dev_unlink(struct bonding *bond, struct slave *slave)
{
	netdev_upper_dev_unlink(slave->dev, bond->dev);
	slave->dev->flags &= ~IFF_SLAVE;
}

static void slave_kobj_release(struct kobject *kobj)
{
	struct slave *slave = to_slave(kobj);
	struct bonding *bond = bond_get_bond_by_slave(slave);

	cancel_delayed_work_sync(&slave->notify_work);
	if (BOND_MODE(bond) == BOND_MODE_8023AD)
		kfree(SLAVE_AD_INFO(slave));

	kfree(slave);
}

static struct kobj_type slave_ktype = {
	.release = slave_kobj_release,
#ifdef CONFIG_SYSFS
	.sysfs_ops = &slave_sysfs_ops,
#endif
};

static int bond_kobj_init(struct slave *slave)
{
	int err;

	err = kobject_init_and_add(&slave->kobj, &slave_ktype,
				   &(slave->dev->dev.kobj), "bonding_slave");
	if (err)
		kobject_put(&slave->kobj);

	return err;
}

static struct slave *bond_alloc_slave(struct bonding *bond,
				      struct net_device *slave_dev)
{
	struct slave *slave = NULL;

	slave = kzalloc(sizeof(*slave), GFP_KERNEL);
	if (!slave)
		return NULL;

	slave->bond = bond;
	slave->dev = slave_dev;
	INIT_DELAYED_WORK(&slave->notify_work, bond_netdev_notify_work);

	if (bond_kobj_init(slave))
		return NULL;

	if (BOND_MODE(bond) == BOND_MODE_8023AD) {
		SLAVE_AD_INFO(slave) = kzalloc(sizeof(struct ad_slave_info),
					       GFP_KERNEL);
		if (!SLAVE_AD_INFO(slave)) {
			kobject_put(&slave->kobj);
			return NULL;
		}
	}

	return slave;
}

static void bond_fill_ifbond(struct bonding *bond, struct ifbond *info)
{
	info->bond_mode = BOND_MODE(bond);
	info->miimon = bond->params.miimon;
	info->num_slaves = bond->slave_cnt;
}

static void bond_fill_ifslave(struct slave *slave, struct ifslave *info)
{
	strcpy(info->slave_name, slave->dev->name);
	info->link = slave->link;
	info->state = bond_slave_state(slave);
	info->link_failure_count = slave->link_failure_count;
}

static void bond_netdev_notify_work(struct work_struct *_work)
{
	struct slave *slave = container_of(_work, struct slave,
					   notify_work.work);

	if (rtnl_trylock()) {
		struct netdev_bonding_info binfo;

		bond_fill_ifslave(slave, &binfo.slave);
		bond_fill_ifbond(slave->bond, &binfo.master);
		netdev_bonding_info_change(slave->dev, &binfo);
		rtnl_unlock();
	} else {
		queue_delayed_work(slave->bond->wq, &slave->notify_work, 1);
	}
}

void bond_queue_slave_event(struct slave *slave)
{
	queue_delayed_work(slave->bond->wq, &slave->notify_work, 0);
}

void bond_lower_state_changed(struct slave *slave)
{
	struct netdev_lag_lower_state_info info;

	info.link_up = slave->link == BOND_LINK_UP ||
		       slave->link == BOND_LINK_FAIL;
	info.tx_enabled = bond_is_active_slave(slave);
	netdev_lower_state_changed(slave->dev, &info);
}

#define BOND_NL_ERR(bond_dev, extack, errmsg) do {		\
	if (extack)						\
		NL_SET_ERR_MSG(extack, errmsg);			\
	else							\
		netdev_err(bond_dev, "Error: %s\n", errmsg);	\
} while (0)

#define SLAVE_NL_ERR(bond_dev, slave_dev, extack, errmsg) do {		\
	if (extack)							\
		NL_SET_ERR_MSG(extack, errmsg);				\
	else								\
		slave_err(bond_dev, slave_dev, "Error: %s\n", errmsg);	\
} while (0)

 
static void bond_ether_setup(struct net_device *bond_dev)
{
	unsigned int flags = bond_dev->flags & (IFF_SLAVE | IFF_UP);

	ether_setup(bond_dev);
	bond_dev->flags |= IFF_MASTER | flags;
	bond_dev->priv_flags &= ~IFF_TX_SKB_SHARING;
}

void bond_xdp_set_features(struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	xdp_features_t val = NETDEV_XDP_ACT_MASK;
	struct list_head *iter;
	struct slave *slave;

	ASSERT_RTNL();

	if (!bond_xdp_check(bond)) {
		xdp_clear_features_flag(bond_dev);
		return;
	}

	bond_for_each_slave(bond, slave, iter)
		val &= slave->dev->xdp_features;

	xdp_set_features_flag(bond_dev, val);
}

 
int bond_enslave(struct net_device *bond_dev, struct net_device *slave_dev,
		 struct netlink_ext_ack *extack)
{
	struct bonding *bond = netdev_priv(bond_dev);
	const struct net_device_ops *slave_ops = slave_dev->netdev_ops;
	struct slave *new_slave = NULL, *prev_slave;
	struct sockaddr_storage ss;
	int link_reporting;
	int res = 0, i;

	if (slave_dev->flags & IFF_MASTER &&
	    !netif_is_bond_master(slave_dev)) {
		BOND_NL_ERR(bond_dev, extack,
			    "Device type (master device) cannot be enslaved");
		return -EPERM;
	}

	if (!bond->params.use_carrier &&
	    slave_dev->ethtool_ops->get_link == NULL &&
	    slave_ops->ndo_eth_ioctl == NULL) {
		slave_warn(bond_dev, slave_dev, "no link monitoring support\n");
	}

	 
	if (netdev_is_rx_handler_busy(slave_dev)) {
		SLAVE_NL_ERR(bond_dev, slave_dev, extack,
			     "Device is in use and cannot be enslaved");
		return -EBUSY;
	}

	if (bond_dev == slave_dev) {
		BOND_NL_ERR(bond_dev, extack, "Cannot enslave bond to itself.");
		return -EPERM;
	}

	 
	 
	if (slave_dev->features & NETIF_F_VLAN_CHALLENGED) {
		slave_dbg(bond_dev, slave_dev, "is NETIF_F_VLAN_CHALLENGED\n");
		if (vlan_uses_dev(bond_dev)) {
			SLAVE_NL_ERR(bond_dev, slave_dev, extack,
				     "Can not enslave VLAN challenged device to VLAN enabled bond");
			return -EPERM;
		} else {
			slave_warn(bond_dev, slave_dev, "enslaved VLAN challenged slave. Adding VLANs will be blocked as long as it is part of bond.\n");
		}
	} else {
		slave_dbg(bond_dev, slave_dev, "is !NETIF_F_VLAN_CHALLENGED\n");
	}

	if (slave_dev->features & NETIF_F_HW_ESP)
		slave_dbg(bond_dev, slave_dev, "is esp-hw-offload capable\n");

	 
	if (slave_dev->flags & IFF_UP) {
		SLAVE_NL_ERR(bond_dev, slave_dev, extack,
			     "Device can not be enslaved while up");
		return -EPERM;
	}

	 
	if (!bond_has_slaves(bond)) {
		if (bond_dev->type != slave_dev->type) {
			slave_dbg(bond_dev, slave_dev, "change device type from %d to %d\n",
				  bond_dev->type, slave_dev->type);

			res = call_netdevice_notifiers(NETDEV_PRE_TYPE_CHANGE,
						       bond_dev);
			res = notifier_to_errno(res);
			if (res) {
				slave_err(bond_dev, slave_dev, "refused to change device type\n");
				return -EBUSY;
			}

			 
			dev_uc_flush(bond_dev);
			dev_mc_flush(bond_dev);

			if (slave_dev->type != ARPHRD_ETHER)
				bond_setup_by_slave(bond_dev, slave_dev);
			else
				bond_ether_setup(bond_dev);

			call_netdevice_notifiers(NETDEV_POST_TYPE_CHANGE,
						 bond_dev);
		}
	} else if (bond_dev->type != slave_dev->type) {
		SLAVE_NL_ERR(bond_dev, slave_dev, extack,
			     "Device type is different from other slaves");
		return -EINVAL;
	}

	if (slave_dev->type == ARPHRD_INFINIBAND &&
	    BOND_MODE(bond) != BOND_MODE_ACTIVEBACKUP) {
		SLAVE_NL_ERR(bond_dev, slave_dev, extack,
			     "Only active-backup mode is supported for infiniband slaves");
		res = -EOPNOTSUPP;
		goto err_undo_flags;
	}

	if (!slave_ops->ndo_set_mac_address ||
	    slave_dev->type == ARPHRD_INFINIBAND) {
		slave_warn(bond_dev, slave_dev, "The slave device specified does not support setting the MAC address\n");
		if (BOND_MODE(bond) == BOND_MODE_ACTIVEBACKUP &&
		    bond->params.fail_over_mac != BOND_FOM_ACTIVE) {
			if (!bond_has_slaves(bond)) {
				bond->params.fail_over_mac = BOND_FOM_ACTIVE;
				slave_warn(bond_dev, slave_dev, "Setting fail_over_mac to active for active-backup mode\n");
			} else {
				SLAVE_NL_ERR(bond_dev, slave_dev, extack,
					     "Slave device does not support setting the MAC address, but fail_over_mac is not set to active");
				res = -EOPNOTSUPP;
				goto err_undo_flags;
			}
		}
	}

	call_netdevice_notifiers(NETDEV_JOIN, slave_dev);

	 
	if (!bond_has_slaves(bond) &&
	    bond->dev->addr_assign_type == NET_ADDR_RANDOM) {
		res = bond_set_dev_addr(bond->dev, slave_dev);
		if (res)
			goto err_undo_flags;
	}

	new_slave = bond_alloc_slave(bond, slave_dev);
	if (!new_slave) {
		res = -ENOMEM;
		goto err_undo_flags;
	}

	 
	new_slave->queue_id = 0;

	 
	new_slave->original_mtu = slave_dev->mtu;
	res = dev_set_mtu(slave_dev, bond->dev->mtu);
	if (res) {
		slave_err(bond_dev, slave_dev, "Error %d calling dev_set_mtu\n", res);
		goto err_free;
	}

	 
	bond_hw_addr_copy(new_slave->perm_hwaddr, slave_dev->dev_addr,
			  slave_dev->addr_len);

	if (!bond->params.fail_over_mac ||
	    BOND_MODE(bond) != BOND_MODE_ACTIVEBACKUP) {
		 
		memcpy(ss.__data, bond_dev->dev_addr, bond_dev->addr_len);
		ss.ss_family = slave_dev->type;
		res = dev_set_mac_address(slave_dev, (struct sockaddr *)&ss,
					  extack);
		if (res) {
			slave_err(bond_dev, slave_dev, "Error %d calling set_mac_address\n", res);
			goto err_restore_mtu;
		}
	}

	 
	slave_dev->priv_flags |= IFF_NO_ADDRCONF;

	 
	res = dev_open(slave_dev, extack);
	if (res) {
		slave_err(bond_dev, slave_dev, "Opening slave failed\n");
		goto err_restore_mac;
	}

	slave_dev->priv_flags |= IFF_BONDING;
	 
	dev_get_stats(new_slave->dev, &new_slave->slave_stats);

	if (bond_is_lb(bond)) {
		 
		res = bond_alb_init_slave(bond, new_slave);
		if (res)
			goto err_close;
	}

	res = vlan_vids_add_by_dev(slave_dev, bond_dev);
	if (res) {
		slave_err(bond_dev, slave_dev, "Couldn't add bond vlan ids\n");
		goto err_close;
	}

	prev_slave = bond_last_slave(bond);

	new_slave->delay = 0;
	new_slave->link_failure_count = 0;

	if (bond_update_speed_duplex(new_slave) &&
	    bond_needs_speed_duplex(bond))
		new_slave->link = BOND_LINK_DOWN;

	new_slave->last_rx = jiffies -
		(msecs_to_jiffies(bond->params.arp_interval) + 1);
	for (i = 0; i < BOND_MAX_ARP_TARGETS; i++)
		new_slave->target_last_arp_rx[i] = new_slave->last_rx;

	new_slave->last_tx = new_slave->last_rx;

	if (bond->params.miimon && !bond->params.use_carrier) {
		link_reporting = bond_check_dev_link(bond, slave_dev, 1);

		if ((link_reporting == -1) && !bond->params.arp_interval) {
			 
			slave_warn(bond_dev, slave_dev, "MII and ETHTOOL support not available for slave, and arp_interval/arp_ip_target module parameters not specified, thus bonding will not detect link failures! see bonding.txt for details\n");
		} else if (link_reporting == -1) {
			 
			slave_warn(bond_dev, slave_dev, "can't get link status from slave; the network driver associated with this interface does not support MII or ETHTOOL link status reporting, thus miimon has no effect on this interface\n");
		}
	}

	 
	new_slave->link = BOND_LINK_NOCHANGE;
	if (bond->params.miimon) {
		if (bond_check_dev_link(bond, slave_dev, 0) == BMSR_LSTATUS) {
			if (bond->params.updelay) {
				bond_set_slave_link_state(new_slave,
							  BOND_LINK_BACK,
							  BOND_SLAVE_NOTIFY_NOW);
				new_slave->delay = bond->params.updelay;
			} else {
				bond_set_slave_link_state(new_slave,
							  BOND_LINK_UP,
							  BOND_SLAVE_NOTIFY_NOW);
			}
		} else {
			bond_set_slave_link_state(new_slave, BOND_LINK_DOWN,
						  BOND_SLAVE_NOTIFY_NOW);
		}
	} else if (bond->params.arp_interval) {
		bond_set_slave_link_state(new_slave,
					  (netif_carrier_ok(slave_dev) ?
					  BOND_LINK_UP : BOND_LINK_DOWN),
					  BOND_SLAVE_NOTIFY_NOW);
	} else {
		bond_set_slave_link_state(new_slave, BOND_LINK_UP,
					  BOND_SLAVE_NOTIFY_NOW);
	}

	if (new_slave->link != BOND_LINK_DOWN)
		new_slave->last_link_up = jiffies;
	slave_dbg(bond_dev, slave_dev, "Initial state of slave is BOND_LINK_%s\n",
		  new_slave->link == BOND_LINK_DOWN ? "DOWN" :
		  (new_slave->link == BOND_LINK_UP ? "UP" : "BACK"));

	if (bond_uses_primary(bond) && bond->params.primary[0]) {
		 
		if (strcmp(bond->params.primary, new_slave->dev->name) == 0) {
			rcu_assign_pointer(bond->primary_slave, new_slave);
			bond->force_primary = true;
		}
	}

	switch (BOND_MODE(bond)) {
	case BOND_MODE_ACTIVEBACKUP:
		bond_set_slave_inactive_flags(new_slave,
					      BOND_SLAVE_NOTIFY_NOW);
		break;
	case BOND_MODE_8023AD:
		 
		bond_set_slave_inactive_flags(new_slave, BOND_SLAVE_NOTIFY_NOW);
		 
		if (!prev_slave) {
			SLAVE_AD_INFO(new_slave)->id = 1;
			 
			bond_3ad_initialize(bond);
		} else {
			SLAVE_AD_INFO(new_slave)->id =
				SLAVE_AD_INFO(prev_slave)->id + 1;
		}

		bond_3ad_bind_slave(new_slave);
		break;
	case BOND_MODE_TLB:
	case BOND_MODE_ALB:
		bond_set_active_slave(new_slave);
		bond_set_slave_inactive_flags(new_slave, BOND_SLAVE_NOTIFY_NOW);
		break;
	default:
		slave_dbg(bond_dev, slave_dev, "This slave is always active in trunk mode\n");

		 
		bond_set_active_slave(new_slave);

		 
		if (!rcu_access_pointer(bond->curr_active_slave) &&
		    new_slave->link == BOND_LINK_UP)
			rcu_assign_pointer(bond->curr_active_slave, new_slave);

		break;
	}  

#ifdef CONFIG_NET_POLL_CONTROLLER
	if (bond->dev->npinfo) {
		if (slave_enable_netpoll(new_slave)) {
			slave_info(bond_dev, slave_dev, "master_dev is using netpoll, but new slave device does not support netpoll\n");
			res = -EBUSY;
			goto err_detach;
		}
	}
#endif

	if (!(bond_dev->features & NETIF_F_LRO))
		dev_disable_lro(slave_dev);

	res = netdev_rx_handler_register(slave_dev, bond_handle_frame,
					 new_slave);
	if (res) {
		slave_dbg(bond_dev, slave_dev, "Error %d calling netdev_rx_handler_register\n", res);
		goto err_detach;
	}

	res = bond_master_upper_dev_link(bond, new_slave, extack);
	if (res) {
		slave_dbg(bond_dev, slave_dev, "Error %d calling bond_master_upper_dev_link\n", res);
		goto err_unregister;
	}

	bond_lower_state_changed(new_slave);

	res = bond_sysfs_slave_add(new_slave);
	if (res) {
		slave_dbg(bond_dev, slave_dev, "Error %d calling bond_sysfs_slave_add\n", res);
		goto err_upper_unlink;
	}

	 
	if (!bond_uses_primary(bond)) {
		 
		if (bond_dev->flags & IFF_PROMISC) {
			res = dev_set_promiscuity(slave_dev, 1);
			if (res)
				goto err_sysfs_del;
		}

		 
		if (bond_dev->flags & IFF_ALLMULTI) {
			res = dev_set_allmulti(slave_dev, 1);
			if (res) {
				if (bond_dev->flags & IFF_PROMISC)
					dev_set_promiscuity(slave_dev, -1);
				goto err_sysfs_del;
			}
		}

		if (bond_dev->flags & IFF_UP) {
			netif_addr_lock_bh(bond_dev);
			dev_mc_sync_multiple(slave_dev, bond_dev);
			dev_uc_sync_multiple(slave_dev, bond_dev);
			netif_addr_unlock_bh(bond_dev);

			if (BOND_MODE(bond) == BOND_MODE_8023AD)
				dev_mc_add(slave_dev, lacpdu_mcast_addr);
		}
	}

	bond->slave_cnt++;
	bond_compute_features(bond);
	bond_set_carrier(bond);

	if (bond_uses_primary(bond)) {
		block_netpoll_tx();
		bond_select_active_slave(bond);
		unblock_netpoll_tx();
	}

	if (bond_mode_can_use_xmit_hash(bond))
		bond_update_slave_arr(bond, NULL);


	if (!slave_dev->netdev_ops->ndo_bpf ||
	    !slave_dev->netdev_ops->ndo_xdp_xmit) {
		if (bond->xdp_prog) {
			SLAVE_NL_ERR(bond_dev, slave_dev, extack,
				     "Slave does not support XDP");
			res = -EOPNOTSUPP;
			goto err_sysfs_del;
		}
	} else if (bond->xdp_prog) {
		struct netdev_bpf xdp = {
			.command = XDP_SETUP_PROG,
			.flags   = 0,
			.prog    = bond->xdp_prog,
			.extack  = extack,
		};

		if (dev_xdp_prog_count(slave_dev) > 0) {
			SLAVE_NL_ERR(bond_dev, slave_dev, extack,
				     "Slave has XDP program loaded, please unload before enslaving");
			res = -EOPNOTSUPP;
			goto err_sysfs_del;
		}

		res = slave_dev->netdev_ops->ndo_bpf(slave_dev, &xdp);
		if (res < 0) {
			 
			slave_dbg(bond_dev, slave_dev, "Error %d calling ndo_bpf\n", res);
			goto err_sysfs_del;
		}
		if (bond->xdp_prog)
			bpf_prog_inc(bond->xdp_prog);
	}

	bond_xdp_set_features(bond_dev);

	slave_info(bond_dev, slave_dev, "Enslaving as %s interface with %s link\n",
		   bond_is_active_slave(new_slave) ? "an active" : "a backup",
		   new_slave->link != BOND_LINK_DOWN ? "an up" : "a down");

	 
	bond_queue_slave_event(new_slave);
	return 0;

 
err_sysfs_del:
	bond_sysfs_slave_del(new_slave);

err_upper_unlink:
	bond_upper_dev_unlink(bond, new_slave);

err_unregister:
	netdev_rx_handler_unregister(slave_dev);

err_detach:
	vlan_vids_del_by_dev(slave_dev, bond_dev);
	if (rcu_access_pointer(bond->primary_slave) == new_slave)
		RCU_INIT_POINTER(bond->primary_slave, NULL);
	if (rcu_access_pointer(bond->curr_active_slave) == new_slave) {
		block_netpoll_tx();
		bond_change_active_slave(bond, NULL);
		bond_select_active_slave(bond);
		unblock_netpoll_tx();
	}
	 
	synchronize_rcu();
	slave_disable_netpoll(new_slave);

err_close:
	if (!netif_is_bond_master(slave_dev))
		slave_dev->priv_flags &= ~IFF_BONDING;
	dev_close(slave_dev);

err_restore_mac:
	slave_dev->priv_flags &= ~IFF_NO_ADDRCONF;
	if (!bond->params.fail_over_mac ||
	    BOND_MODE(bond) != BOND_MODE_ACTIVEBACKUP) {
		 
		bond_hw_addr_copy(ss.__data, new_slave->perm_hwaddr,
				  new_slave->dev->addr_len);
		ss.ss_family = slave_dev->type;
		dev_set_mac_address(slave_dev, (struct sockaddr *)&ss, NULL);
	}

err_restore_mtu:
	dev_set_mtu(slave_dev, new_slave->original_mtu);

err_free:
	kobject_put(&new_slave->kobj);

err_undo_flags:
	 
	if (!bond_has_slaves(bond)) {
		if (ether_addr_equal_64bits(bond_dev->dev_addr,
					    slave_dev->dev_addr))
			eth_hw_addr_random(bond_dev);
		if (bond_dev->type != ARPHRD_ETHER) {
			dev_close(bond_dev);
			bond_ether_setup(bond_dev);
		}
	}

	return res;
}

 
static int __bond_release_one(struct net_device *bond_dev,
			      struct net_device *slave_dev,
			      bool all, bool unregister)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct slave *slave, *oldcurrent;
	struct sockaddr_storage ss;
	int old_flags = bond_dev->flags;
	netdev_features_t old_features = bond_dev->features;

	 
	if (!(slave_dev->flags & IFF_SLAVE) ||
	    !netdev_has_upper_dev(slave_dev, bond_dev)) {
		slave_dbg(bond_dev, slave_dev, "cannot release slave\n");
		return -EINVAL;
	}

	block_netpoll_tx();

	slave = bond_get_slave_by_dev(bond, slave_dev);
	if (!slave) {
		 
		slave_info(bond_dev, slave_dev, "interface not enslaved\n");
		unblock_netpoll_tx();
		return -EINVAL;
	}

	bond_set_slave_inactive_flags(slave, BOND_SLAVE_NOTIFY_NOW);

	bond_sysfs_slave_del(slave);

	 
	bond_get_stats(bond->dev, &bond->bond_stats);

	if (bond->xdp_prog) {
		struct netdev_bpf xdp = {
			.command = XDP_SETUP_PROG,
			.flags   = 0,
			.prog	 = NULL,
			.extack  = NULL,
		};
		if (slave_dev->netdev_ops->ndo_bpf(slave_dev, &xdp))
			slave_warn(bond_dev, slave_dev, "failed to unload XDP program\n");
	}

	 
	netdev_rx_handler_unregister(slave_dev);

	if (BOND_MODE(bond) == BOND_MODE_8023AD)
		bond_3ad_unbind_slave(slave);

	bond_upper_dev_unlink(bond, slave);

	if (bond_mode_can_use_xmit_hash(bond))
		bond_update_slave_arr(bond, slave);

	slave_info(bond_dev, slave_dev, "Releasing %s interface\n",
		    bond_is_active_slave(slave) ? "active" : "backup");

	oldcurrent = rcu_access_pointer(bond->curr_active_slave);

	RCU_INIT_POINTER(bond->current_arp_slave, NULL);

	if (!all && (!bond->params.fail_over_mac ||
		     BOND_MODE(bond) != BOND_MODE_ACTIVEBACKUP)) {
		if (ether_addr_equal_64bits(bond_dev->dev_addr, slave->perm_hwaddr) &&
		    bond_has_slaves(bond))
			slave_warn(bond_dev, slave_dev, "the permanent HWaddr of slave - %pM - is still in use by bond - set the HWaddr of slave to a different address to avoid conflicts\n",
				   slave->perm_hwaddr);
	}

	if (rtnl_dereference(bond->primary_slave) == slave)
		RCU_INIT_POINTER(bond->primary_slave, NULL);

	if (oldcurrent == slave)
		bond_change_active_slave(bond, NULL);

	if (bond_is_lb(bond)) {
		 
		bond_alb_deinit_slave(bond, slave);
	}

	if (all) {
		RCU_INIT_POINTER(bond->curr_active_slave, NULL);
	} else if (oldcurrent == slave) {
		 
		bond_select_active_slave(bond);
	}

	bond_set_carrier(bond);
	if (!bond_has_slaves(bond))
		eth_hw_addr_random(bond_dev);

	unblock_netpoll_tx();
	synchronize_rcu();
	bond->slave_cnt--;

	if (!bond_has_slaves(bond)) {
		call_netdevice_notifiers(NETDEV_CHANGEADDR, bond->dev);
		call_netdevice_notifiers(NETDEV_RELEASE, bond->dev);
	}

	bond_compute_features(bond);
	if (!(bond_dev->features & NETIF_F_VLAN_CHALLENGED) &&
	    (old_features & NETIF_F_VLAN_CHALLENGED))
		slave_info(bond_dev, slave_dev, "last VLAN challenged slave left bond - VLAN blocking is removed\n");

	vlan_vids_del_by_dev(slave_dev, bond_dev);

	 
	if (!bond_uses_primary(bond)) {
		 
		if (old_flags & IFF_PROMISC)
			dev_set_promiscuity(slave_dev, -1);

		 
		if (old_flags & IFF_ALLMULTI)
			dev_set_allmulti(slave_dev, -1);

		if (old_flags & IFF_UP)
			bond_hw_addr_flush(bond_dev, slave_dev);
	}

	slave_disable_netpoll(slave);

	 
	dev_close(slave_dev);

	slave_dev->priv_flags &= ~IFF_NO_ADDRCONF;

	if (bond->params.fail_over_mac != BOND_FOM_ACTIVE ||
	    BOND_MODE(bond) != BOND_MODE_ACTIVEBACKUP) {
		 
		bond_hw_addr_copy(ss.__data, slave->perm_hwaddr,
				  slave->dev->addr_len);
		ss.ss_family = slave_dev->type;
		dev_set_mac_address(slave_dev, (struct sockaddr *)&ss, NULL);
	}

	if (unregister)
		__dev_set_mtu(slave_dev, slave->original_mtu);
	else
		dev_set_mtu(slave_dev, slave->original_mtu);

	if (!netif_is_bond_master(slave_dev))
		slave_dev->priv_flags &= ~IFF_BONDING;

	bond_xdp_set_features(bond_dev);
	kobject_put(&slave->kobj);

	return 0;
}

 
int bond_release(struct net_device *bond_dev, struct net_device *slave_dev)
{
	return __bond_release_one(bond_dev, slave_dev, false, false);
}

 
static int bond_release_and_destroy(struct net_device *bond_dev,
				    struct net_device *slave_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	int ret;

	ret = __bond_release_one(bond_dev, slave_dev, false, true);
	if (ret == 0 && !bond_has_slaves(bond) &&
	    bond_dev->reg_state != NETREG_UNREGISTERING) {
		bond_dev->priv_flags |= IFF_DISABLE_NETPOLL;
		netdev_info(bond_dev, "Destroying bond\n");
		bond_remove_proc_entry(bond);
		unregister_netdevice(bond_dev);
	}
	return ret;
}

static void bond_info_query(struct net_device *bond_dev, struct ifbond *info)
{
	struct bonding *bond = netdev_priv(bond_dev);

	bond_fill_ifbond(bond, info);
}

static int bond_slave_info_query(struct net_device *bond_dev, struct ifslave *info)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct list_head *iter;
	int i = 0, res = -ENODEV;
	struct slave *slave;

	bond_for_each_slave(bond, slave, iter) {
		if (i++ == (int)info->slave_id) {
			res = 0;
			bond_fill_ifslave(slave, info);
			break;
		}
	}

	return res;
}

 

 
static int bond_miimon_inspect(struct bonding *bond)
{
	bool ignore_updelay = false;
	int link_state, commit = 0;
	struct list_head *iter;
	struct slave *slave;

	if (BOND_MODE(bond) == BOND_MODE_ACTIVEBACKUP) {
		ignore_updelay = !rcu_dereference(bond->curr_active_slave);
	} else {
		struct bond_up_slave *usable_slaves;

		usable_slaves = rcu_dereference(bond->usable_slaves);

		if (usable_slaves && usable_slaves->count == 0)
			ignore_updelay = true;
	}

	bond_for_each_slave_rcu(bond, slave, iter) {
		bond_propose_link_state(slave, BOND_LINK_NOCHANGE);

		link_state = bond_check_dev_link(bond, slave->dev, 0);

		switch (slave->link) {
		case BOND_LINK_UP:
			if (link_state)
				continue;

			bond_propose_link_state(slave, BOND_LINK_FAIL);
			commit++;
			slave->delay = bond->params.downdelay;
			if (slave->delay) {
				slave_info(bond->dev, slave->dev, "link status down for %sinterface, disabling it in %d ms\n",
					   (BOND_MODE(bond) ==
					    BOND_MODE_ACTIVEBACKUP) ?
					    (bond_is_active_slave(slave) ?
					     "active " : "backup ") : "",
					   bond->params.downdelay * bond->params.miimon);
			}
			fallthrough;
		case BOND_LINK_FAIL:
			if (link_state) {
				 
				bond_propose_link_state(slave, BOND_LINK_UP);
				slave->last_link_up = jiffies;
				slave_info(bond->dev, slave->dev, "link status up again after %d ms\n",
					   (bond->params.downdelay - slave->delay) *
					   bond->params.miimon);
				commit++;
				continue;
			}

			if (slave->delay <= 0) {
				bond_propose_link_state(slave, BOND_LINK_DOWN);
				commit++;
				continue;
			}

			slave->delay--;
			break;

		case BOND_LINK_DOWN:
			if (!link_state)
				continue;

			bond_propose_link_state(slave, BOND_LINK_BACK);
			commit++;
			slave->delay = bond->params.updelay;

			if (slave->delay) {
				slave_info(bond->dev, slave->dev, "link status up, enabling it in %d ms\n",
					   ignore_updelay ? 0 :
					   bond->params.updelay *
					   bond->params.miimon);
			}
			fallthrough;
		case BOND_LINK_BACK:
			if (!link_state) {
				bond_propose_link_state(slave, BOND_LINK_DOWN);
				slave_info(bond->dev, slave->dev, "link status down again after %d ms\n",
					   (bond->params.updelay - slave->delay) *
					   bond->params.miimon);
				commit++;
				continue;
			}

			if (ignore_updelay)
				slave->delay = 0;

			if (slave->delay <= 0) {
				bond_propose_link_state(slave, BOND_LINK_UP);
				commit++;
				ignore_updelay = false;
				continue;
			}

			slave->delay--;
			break;
		}
	}

	return commit;
}

static void bond_miimon_link_change(struct bonding *bond,
				    struct slave *slave,
				    char link)
{
	switch (BOND_MODE(bond)) {
	case BOND_MODE_8023AD:
		bond_3ad_handle_link_change(slave, link);
		break;
	case BOND_MODE_TLB:
	case BOND_MODE_ALB:
		bond_alb_handle_link_change(bond, slave, link);
		break;
	case BOND_MODE_XOR:
		bond_update_slave_arr(bond, NULL);
		break;
	}
}

static void bond_miimon_commit(struct bonding *bond)
{
	struct slave *slave, *primary, *active;
	bool do_failover = false;
	struct list_head *iter;

	ASSERT_RTNL();

	bond_for_each_slave(bond, slave, iter) {
		switch (slave->link_new_state) {
		case BOND_LINK_NOCHANGE:
			 
			if (BOND_MODE(bond) == BOND_MODE_8023AD &&
			    slave->link == BOND_LINK_UP)
				bond_3ad_adapter_speed_duplex_changed(slave);
			continue;

		case BOND_LINK_UP:
			if (bond_update_speed_duplex(slave) &&
			    bond_needs_speed_duplex(bond)) {
				slave->link = BOND_LINK_DOWN;
				if (net_ratelimit())
					slave_warn(bond->dev, slave->dev,
						   "failed to get link speed/duplex\n");
				continue;
			}
			bond_set_slave_link_state(slave, BOND_LINK_UP,
						  BOND_SLAVE_NOTIFY_NOW);
			slave->last_link_up = jiffies;

			primary = rtnl_dereference(bond->primary_slave);
			if (BOND_MODE(bond) == BOND_MODE_8023AD) {
				 
				bond_set_backup_slave(slave);
			} else if (BOND_MODE(bond) != BOND_MODE_ACTIVEBACKUP) {
				 
				bond_set_active_slave(slave);
			}

			slave_info(bond->dev, slave->dev, "link status definitely up, %u Mbps %s duplex\n",
				   slave->speed == SPEED_UNKNOWN ? 0 : slave->speed,
				   slave->duplex ? "full" : "half");

			bond_miimon_link_change(bond, slave, BOND_LINK_UP);

			active = rtnl_dereference(bond->curr_active_slave);
			if (!active || slave == primary || slave->prio > active->prio)
				do_failover = true;

			continue;

		case BOND_LINK_DOWN:
			if (slave->link_failure_count < UINT_MAX)
				slave->link_failure_count++;

			bond_set_slave_link_state(slave, BOND_LINK_DOWN,
						  BOND_SLAVE_NOTIFY_NOW);

			if (BOND_MODE(bond) == BOND_MODE_ACTIVEBACKUP ||
			    BOND_MODE(bond) == BOND_MODE_8023AD)
				bond_set_slave_inactive_flags(slave,
							      BOND_SLAVE_NOTIFY_NOW);

			slave_info(bond->dev, slave->dev, "link status definitely down, disabling slave\n");

			bond_miimon_link_change(bond, slave, BOND_LINK_DOWN);

			if (slave == rcu_access_pointer(bond->curr_active_slave))
				do_failover = true;

			continue;

		default:
			slave_err(bond->dev, slave->dev, "invalid new link %d on slave\n",
				  slave->link_new_state);
			bond_propose_link_state(slave, BOND_LINK_NOCHANGE);

			continue;
		}
	}

	if (do_failover) {
		block_netpoll_tx();
		bond_select_active_slave(bond);
		unblock_netpoll_tx();
	}

	bond_set_carrier(bond);
}

 
static void bond_mii_monitor(struct work_struct *work)
{
	struct bonding *bond = container_of(work, struct bonding,
					    mii_work.work);
	bool should_notify_peers = false;
	bool commit;
	unsigned long delay;
	struct slave *slave;
	struct list_head *iter;

	delay = msecs_to_jiffies(bond->params.miimon);

	if (!bond_has_slaves(bond))
		goto re_arm;

	rcu_read_lock();
	should_notify_peers = bond_should_notify_peers(bond);
	commit = !!bond_miimon_inspect(bond);
	if (bond->send_peer_notif) {
		rcu_read_unlock();
		if (rtnl_trylock()) {
			bond->send_peer_notif--;
			rtnl_unlock();
		}
	} else {
		rcu_read_unlock();
	}

	if (commit) {
		 
		if (!rtnl_trylock()) {
			delay = 1;
			should_notify_peers = false;
			goto re_arm;
		}

		bond_for_each_slave(bond, slave, iter) {
			bond_commit_link_state(slave, BOND_SLAVE_NOTIFY_LATER);
		}
		bond_miimon_commit(bond);

		rtnl_unlock();	 
	}

re_arm:
	if (bond->params.miimon)
		queue_delayed_work(bond->wq, &bond->mii_work, delay);

	if (should_notify_peers) {
		if (!rtnl_trylock())
			return;
		call_netdevice_notifiers(NETDEV_NOTIFY_PEERS, bond->dev);
		rtnl_unlock();
	}
}

static int bond_upper_dev_walk(struct net_device *upper,
			       struct netdev_nested_priv *priv)
{
	__be32 ip = *(__be32 *)priv->data;

	return ip == bond_confirm_addr(upper, 0, ip);
}

static bool bond_has_this_ip(struct bonding *bond, __be32 ip)
{
	struct netdev_nested_priv priv = {
		.data = (void *)&ip,
	};
	bool ret = false;

	if (ip == bond_confirm_addr(bond->dev, 0, ip))
		return true;

	rcu_read_lock();
	if (netdev_walk_all_upper_dev_rcu(bond->dev, bond_upper_dev_walk, &priv))
		ret = true;
	rcu_read_unlock();

	return ret;
}

#define BOND_VLAN_PROTO_NONE cpu_to_be16(0xffff)

static bool bond_handle_vlan(struct slave *slave, struct bond_vlan_tag *tags,
			     struct sk_buff *skb)
{
	struct net_device *bond_dev = slave->bond->dev;
	struct net_device *slave_dev = slave->dev;
	struct bond_vlan_tag *outer_tag = tags;

	if (!tags || tags->vlan_proto == BOND_VLAN_PROTO_NONE)
		return true;

	tags++;

	 
	while (tags->vlan_proto != BOND_VLAN_PROTO_NONE) {
		if (!tags->vlan_id) {
			tags++;
			continue;
		}

		slave_dbg(bond_dev, slave_dev, "inner tag: proto %X vid %X\n",
			  ntohs(outer_tag->vlan_proto), tags->vlan_id);
		skb = vlan_insert_tag_set_proto(skb, tags->vlan_proto,
						tags->vlan_id);
		if (!skb) {
			net_err_ratelimited("failed to insert inner VLAN tag\n");
			return false;
		}

		tags++;
	}
	 
	if (outer_tag->vlan_id) {
		slave_dbg(bond_dev, slave_dev, "outer tag: proto %X vid %X\n",
			  ntohs(outer_tag->vlan_proto), outer_tag->vlan_id);
		__vlan_hwaccel_put_tag(skb, outer_tag->vlan_proto,
				       outer_tag->vlan_id);
	}

	return true;
}

 
static void bond_arp_send(struct slave *slave, int arp_op, __be32 dest_ip,
			  __be32 src_ip, struct bond_vlan_tag *tags)
{
	struct net_device *bond_dev = slave->bond->dev;
	struct net_device *slave_dev = slave->dev;
	struct sk_buff *skb;

	slave_dbg(bond_dev, slave_dev, "arp %d on slave: dst %pI4 src %pI4\n",
		  arp_op, &dest_ip, &src_ip);

	skb = arp_create(arp_op, ETH_P_ARP, dest_ip, slave_dev, src_ip,
			 NULL, slave_dev->dev_addr, NULL);

	if (!skb) {
		net_err_ratelimited("ARP packet allocation failed\n");
		return;
	}

	if (bond_handle_vlan(slave, tags, skb)) {
		slave_update_last_tx(slave);
		arp_xmit(skb);
	}

	return;
}

 
struct bond_vlan_tag *bond_verify_device_path(struct net_device *start_dev,
					      struct net_device *end_dev,
					      int level)
{
	struct bond_vlan_tag *tags;
	struct net_device *upper;
	struct list_head  *iter;

	if (start_dev == end_dev) {
		tags = kcalloc(level + 1, sizeof(*tags), GFP_ATOMIC);
		if (!tags)
			return ERR_PTR(-ENOMEM);
		tags[level].vlan_proto = BOND_VLAN_PROTO_NONE;
		return tags;
	}

	netdev_for_each_upper_dev_rcu(start_dev, upper, iter) {
		tags = bond_verify_device_path(upper, end_dev, level + 1);
		if (IS_ERR_OR_NULL(tags)) {
			if (IS_ERR(tags))
				return tags;
			continue;
		}
		if (is_vlan_dev(upper)) {
			tags[level].vlan_proto = vlan_dev_vlan_proto(upper);
			tags[level].vlan_id = vlan_dev_vlan_id(upper);
		}

		return tags;
	}

	return NULL;
}

static void bond_arp_send_all(struct bonding *bond, struct slave *slave)
{
	struct rtable *rt;
	struct bond_vlan_tag *tags;
	__be32 *targets = bond->params.arp_targets, addr;
	int i;

	for (i = 0; i < BOND_MAX_ARP_TARGETS && targets[i]; i++) {
		slave_dbg(bond->dev, slave->dev, "%s: target %pI4\n",
			  __func__, &targets[i]);
		tags = NULL;

		 
		rt = ip_route_output(dev_net(bond->dev), targets[i], 0,
				     RTO_ONLINK, 0);
		if (IS_ERR(rt)) {
			 
			if (bond->params.arp_validate)
				pr_warn_once("%s: no route to arp_ip_target %pI4 and arp_validate is set\n",
					     bond->dev->name,
					     &targets[i]);
			bond_arp_send(slave, ARPOP_REQUEST, targets[i],
				      0, tags);
			continue;
		}

		 
		if (rt->dst.dev == bond->dev)
			goto found;

		rcu_read_lock();
		tags = bond_verify_device_path(bond->dev, rt->dst.dev, 0);
		rcu_read_unlock();

		if (!IS_ERR_OR_NULL(tags))
			goto found;

		 
		slave_dbg(bond->dev, slave->dev, "no path to arp_ip_target %pI4 via rt.dev %s\n",
			   &targets[i], rt->dst.dev ? rt->dst.dev->name : "NULL");

		ip_rt_put(rt);
		continue;

found:
		addr = bond_confirm_addr(rt->dst.dev, targets[i], 0);
		ip_rt_put(rt);
		bond_arp_send(slave, ARPOP_REQUEST, targets[i], addr, tags);
		kfree(tags);
	}
}

static void bond_validate_arp(struct bonding *bond, struct slave *slave, __be32 sip, __be32 tip)
{
	int i;

	if (!sip || !bond_has_this_ip(bond, tip)) {
		slave_dbg(bond->dev, slave->dev, "%s: sip %pI4 tip %pI4 not found\n",
			   __func__, &sip, &tip);
		return;
	}

	i = bond_get_targets_ip(bond->params.arp_targets, sip);
	if (i == -1) {
		slave_dbg(bond->dev, slave->dev, "%s: sip %pI4 not found in targets\n",
			   __func__, &sip);
		return;
	}
	slave->last_rx = jiffies;
	slave->target_last_arp_rx[i] = jiffies;
}

static int bond_arp_rcv(const struct sk_buff *skb, struct bonding *bond,
			struct slave *slave)
{
	struct arphdr *arp = (struct arphdr *)skb->data;
	struct slave *curr_active_slave, *curr_arp_slave;
	unsigned char *arp_ptr;
	__be32 sip, tip;
	unsigned int alen;

	alen = arp_hdr_len(bond->dev);

	if (alen > skb_headlen(skb)) {
		arp = kmalloc(alen, GFP_ATOMIC);
		if (!arp)
			goto out_unlock;
		if (skb_copy_bits(skb, 0, arp, alen) < 0)
			goto out_unlock;
	}

	if (arp->ar_hln != bond->dev->addr_len ||
	    skb->pkt_type == PACKET_OTHERHOST ||
	    skb->pkt_type == PACKET_LOOPBACK ||
	    arp->ar_hrd != htons(ARPHRD_ETHER) ||
	    arp->ar_pro != htons(ETH_P_IP) ||
	    arp->ar_pln != 4)
		goto out_unlock;

	arp_ptr = (unsigned char *)(arp + 1);
	arp_ptr += bond->dev->addr_len;
	memcpy(&sip, arp_ptr, 4);
	arp_ptr += 4 + bond->dev->addr_len;
	memcpy(&tip, arp_ptr, 4);

	slave_dbg(bond->dev, slave->dev, "%s: %s/%d av %d sv %d sip %pI4 tip %pI4\n",
		  __func__, slave->dev->name, bond_slave_state(slave),
		  bond->params.arp_validate, slave_do_arp_validate(bond, slave),
		  &sip, &tip);

	curr_active_slave = rcu_dereference(bond->curr_active_slave);
	curr_arp_slave = rcu_dereference(bond->current_arp_slave);

	 
	if (bond_is_active_slave(slave))
		bond_validate_arp(bond, slave, sip, tip);
	else if (curr_active_slave &&
		 time_after(slave_last_rx(bond, curr_active_slave),
			    curr_active_slave->last_link_up))
		bond_validate_arp(bond, slave, tip, sip);
	else if (curr_arp_slave && (arp->ar_op == htons(ARPOP_REPLY)) &&
		 bond_time_in_interval(bond, slave_last_tx(curr_arp_slave), 1))
		bond_validate_arp(bond, slave, sip, tip);

out_unlock:
	if (arp != (struct arphdr *)skb->data)
		kfree(arp);
	return RX_HANDLER_ANOTHER;
}

#if IS_ENABLED(CONFIG_IPV6)
static void bond_ns_send(struct slave *slave, const struct in6_addr *daddr,
			 const struct in6_addr *saddr, struct bond_vlan_tag *tags)
{
	struct net_device *bond_dev = slave->bond->dev;
	struct net_device *slave_dev = slave->dev;
	struct in6_addr mcaddr;
	struct sk_buff *skb;

	slave_dbg(bond_dev, slave_dev, "NS on slave: dst %pI6c src %pI6c\n",
		  daddr, saddr);

	skb = ndisc_ns_create(slave_dev, daddr, saddr, 0);
	if (!skb) {
		net_err_ratelimited("NS packet allocation failed\n");
		return;
	}

	addrconf_addr_solict_mult(daddr, &mcaddr);
	if (bond_handle_vlan(slave, tags, skb)) {
		slave_update_last_tx(slave);
		ndisc_send_skb(skb, &mcaddr, saddr);
	}
}

static void bond_ns_send_all(struct bonding *bond, struct slave *slave)
{
	struct in6_addr *targets = bond->params.ns_targets;
	struct bond_vlan_tag *tags;
	struct dst_entry *dst;
	struct in6_addr saddr;
	struct flowi6 fl6;
	int i;

	for (i = 0; i < BOND_MAX_NS_TARGETS && !ipv6_addr_any(&targets[i]); i++) {
		slave_dbg(bond->dev, slave->dev, "%s: target %pI6c\n",
			  __func__, &targets[i]);
		tags = NULL;

		 
		memset(&fl6, 0, sizeof(struct flowi6));
		fl6.daddr = targets[i];
		fl6.flowi6_oif = bond->dev->ifindex;

		dst = ip6_route_output(dev_net(bond->dev), NULL, &fl6);
		if (dst->error) {
			dst_release(dst);
			 
			if (bond->params.arp_validate)
				pr_warn_once("%s: no route to ns_ip6_target %pI6c and arp_validate is set\n",
					     bond->dev->name,
					     &targets[i]);
			bond_ns_send(slave, &targets[i], &in6addr_any, tags);
			continue;
		}

		 
		if (dst->dev == bond->dev)
			goto found;

		rcu_read_lock();
		tags = bond_verify_device_path(bond->dev, dst->dev, 0);
		rcu_read_unlock();

		if (!IS_ERR_OR_NULL(tags))
			goto found;

		 
		slave_dbg(bond->dev, slave->dev, "no path to ns_ip6_target %pI6c via dst->dev %s\n",
			  &targets[i], dst->dev ? dst->dev->name : "NULL");

		dst_release(dst);
		continue;

found:
		if (!ipv6_dev_get_saddr(dev_net(dst->dev), dst->dev, &targets[i], 0, &saddr))
			bond_ns_send(slave, &targets[i], &saddr, tags);
		else
			bond_ns_send(slave, &targets[i], &in6addr_any, tags);

		dst_release(dst);
		kfree(tags);
	}
}

static int bond_confirm_addr6(struct net_device *dev,
			      struct netdev_nested_priv *priv)
{
	struct in6_addr *addr = (struct in6_addr *)priv->data;

	return ipv6_chk_addr(dev_net(dev), addr, dev, 0);
}

static bool bond_has_this_ip6(struct bonding *bond, struct in6_addr *addr)
{
	struct netdev_nested_priv priv = {
		.data = addr,
	};
	int ret = false;

	if (bond_confirm_addr6(bond->dev, &priv))
		return true;

	rcu_read_lock();
	if (netdev_walk_all_upper_dev_rcu(bond->dev, bond_confirm_addr6, &priv))
		ret = true;
	rcu_read_unlock();

	return ret;
}

static void bond_validate_na(struct bonding *bond, struct slave *slave,
			     struct in6_addr *saddr, struct in6_addr *daddr)
{
	int i;

	 
	if (ipv6_addr_any(saddr) ||
	    (!ipv6_addr_equal(daddr, &in6addr_linklocal_allnodes) &&
	     !bond_has_this_ip6(bond, daddr))) {
		slave_dbg(bond->dev, slave->dev, "%s: sip %pI6c tip %pI6c not found\n",
			  __func__, saddr, daddr);
		return;
	}

	i = bond_get_targets_ip6(bond->params.ns_targets, saddr);
	if (i == -1) {
		slave_dbg(bond->dev, slave->dev, "%s: sip %pI6c not found in targets\n",
			  __func__, saddr);
		return;
	}
	slave->last_rx = jiffies;
	slave->target_last_arp_rx[i] = jiffies;
}

static int bond_na_rcv(const struct sk_buff *skb, struct bonding *bond,
		       struct slave *slave)
{
	struct slave *curr_active_slave, *curr_arp_slave;
	struct in6_addr *saddr, *daddr;
	struct {
		struct ipv6hdr ip6;
		struct icmp6hdr icmp6;
	} *combined, _combined;

	if (skb->pkt_type == PACKET_OTHERHOST ||
	    skb->pkt_type == PACKET_LOOPBACK)
		goto out;

	combined = skb_header_pointer(skb, 0, sizeof(_combined), &_combined);
	if (!combined || combined->ip6.nexthdr != NEXTHDR_ICMP ||
	    (combined->icmp6.icmp6_type != NDISC_NEIGHBOUR_SOLICITATION &&
	     combined->icmp6.icmp6_type != NDISC_NEIGHBOUR_ADVERTISEMENT))
		goto out;

	saddr = &combined->ip6.saddr;
	daddr = &combined->ip6.daddr;

	slave_dbg(bond->dev, slave->dev, "%s: %s/%d av %d sv %d sip %pI6c tip %pI6c\n",
		  __func__, slave->dev->name, bond_slave_state(slave),
		  bond->params.arp_validate, slave_do_arp_validate(bond, slave),
		  saddr, daddr);

	curr_active_slave = rcu_dereference(bond->curr_active_slave);
	curr_arp_slave = rcu_dereference(bond->current_arp_slave);

	 
	if (bond_is_active_slave(slave))
		bond_validate_na(bond, slave, saddr, daddr);
	else if (curr_active_slave &&
		 time_after(slave_last_rx(bond, curr_active_slave),
			    curr_active_slave->last_link_up))
		bond_validate_na(bond, slave, daddr, saddr);
	else if (curr_arp_slave &&
		 bond_time_in_interval(bond, slave_last_tx(curr_arp_slave), 1))
		bond_validate_na(bond, slave, saddr, daddr);

out:
	return RX_HANDLER_ANOTHER;
}
#endif

int bond_rcv_validate(const struct sk_buff *skb, struct bonding *bond,
		      struct slave *slave)
{
#if IS_ENABLED(CONFIG_IPV6)
	bool is_ipv6 = skb->protocol == __cpu_to_be16(ETH_P_IPV6);
#endif
	bool is_arp = skb->protocol == __cpu_to_be16(ETH_P_ARP);

	slave_dbg(bond->dev, slave->dev, "%s: skb->dev %s\n",
		  __func__, skb->dev->name);

	 
	if (!slave_do_arp_validate(bond, slave)) {
		if ((slave_do_arp_validate_only(bond) && is_arp) ||
#if IS_ENABLED(CONFIG_IPV6)
		    (slave_do_arp_validate_only(bond) && is_ipv6) ||
#endif
		    !slave_do_arp_validate_only(bond))
			slave->last_rx = jiffies;
		return RX_HANDLER_ANOTHER;
	} else if (is_arp) {
		return bond_arp_rcv(skb, bond, slave);
#if IS_ENABLED(CONFIG_IPV6)
	} else if (is_ipv6) {
		return bond_na_rcv(skb, bond, slave);
#endif
	} else {
		return RX_HANDLER_ANOTHER;
	}
}

static void bond_send_validate(struct bonding *bond, struct slave *slave)
{
	bond_arp_send_all(bond, slave);
#if IS_ENABLED(CONFIG_IPV6)
	bond_ns_send_all(bond, slave);
#endif
}

 
static bool bond_time_in_interval(struct bonding *bond, unsigned long last_act,
				  int mod)
{
	int delta_in_ticks = msecs_to_jiffies(bond->params.arp_interval);

	return time_in_range(jiffies,
			     last_act - delta_in_ticks,
			     last_act + mod * delta_in_ticks + delta_in_ticks/2);
}

 
static void bond_loadbalance_arp_mon(struct bonding *bond)
{
	struct slave *slave, *oldcurrent;
	struct list_head *iter;
	int do_failover = 0, slave_state_changed = 0;

	if (!bond_has_slaves(bond))
		goto re_arm;

	rcu_read_lock();

	oldcurrent = rcu_dereference(bond->curr_active_slave);
	 
	bond_for_each_slave_rcu(bond, slave, iter) {
		unsigned long last_tx = slave_last_tx(slave);

		bond_propose_link_state(slave, BOND_LINK_NOCHANGE);

		if (slave->link != BOND_LINK_UP) {
			if (bond_time_in_interval(bond, last_tx, 1) &&
			    bond_time_in_interval(bond, slave->last_rx, 1)) {

				bond_propose_link_state(slave, BOND_LINK_UP);
				slave_state_changed = 1;

				 
				if (!oldcurrent) {
					slave_info(bond->dev, slave->dev, "link status definitely up\n");
					do_failover = 1;
				} else {
					slave_info(bond->dev, slave->dev, "interface is now up\n");
				}
			}
		} else {
			 

			 
			if (!bond_time_in_interval(bond, last_tx, bond->params.missed_max) ||
			    !bond_time_in_interval(bond, slave->last_rx, bond->params.missed_max)) {

				bond_propose_link_state(slave, BOND_LINK_DOWN);
				slave_state_changed = 1;

				if (slave->link_failure_count < UINT_MAX)
					slave->link_failure_count++;

				slave_info(bond->dev, slave->dev, "interface is now down\n");

				if (slave == oldcurrent)
					do_failover = 1;
			}
		}

		 
		if (bond_slave_is_up(slave))
			bond_send_validate(bond, slave);
	}

	rcu_read_unlock();

	if (do_failover || slave_state_changed) {
		if (!rtnl_trylock())
			goto re_arm;

		bond_for_each_slave(bond, slave, iter) {
			if (slave->link_new_state != BOND_LINK_NOCHANGE)
				slave->link = slave->link_new_state;
		}

		if (slave_state_changed) {
			bond_slave_state_change(bond);
			if (BOND_MODE(bond) == BOND_MODE_XOR)
				bond_update_slave_arr(bond, NULL);
		}
		if (do_failover) {
			block_netpoll_tx();
			bond_select_active_slave(bond);
			unblock_netpoll_tx();
		}
		rtnl_unlock();
	}

re_arm:
	if (bond->params.arp_interval)
		queue_delayed_work(bond->wq, &bond->arp_work,
				   msecs_to_jiffies(bond->params.arp_interval));
}

 
static int bond_ab_arp_inspect(struct bonding *bond)
{
	unsigned long last_tx, last_rx;
	struct list_head *iter;
	struct slave *slave;
	int commit = 0;

	bond_for_each_slave_rcu(bond, slave, iter) {
		bond_propose_link_state(slave, BOND_LINK_NOCHANGE);
		last_rx = slave_last_rx(bond, slave);

		if (slave->link != BOND_LINK_UP) {
			if (bond_time_in_interval(bond, last_rx, 1)) {
				bond_propose_link_state(slave, BOND_LINK_UP);
				commit++;
			} else if (slave->link == BOND_LINK_BACK) {
				bond_propose_link_state(slave, BOND_LINK_FAIL);
				commit++;
			}
			continue;
		}

		 
		if (bond_time_in_interval(bond, slave->last_link_up, 2))
			continue;

		 
		if (!bond_is_active_slave(slave) &&
		    !rcu_access_pointer(bond->current_arp_slave) &&
		    !bond_time_in_interval(bond, last_rx, bond->params.missed_max + 1)) {
			bond_propose_link_state(slave, BOND_LINK_DOWN);
			commit++;
		}

		 
		last_tx = slave_last_tx(slave);
		if (bond_is_active_slave(slave) &&
		    (!bond_time_in_interval(bond, last_tx, bond->params.missed_max) ||
		     !bond_time_in_interval(bond, last_rx, bond->params.missed_max))) {
			bond_propose_link_state(slave, BOND_LINK_DOWN);
			commit++;
		}
	}

	return commit;
}

 
static void bond_ab_arp_commit(struct bonding *bond)
{
	bool do_failover = false;
	struct list_head *iter;
	unsigned long last_tx;
	struct slave *slave;

	bond_for_each_slave(bond, slave, iter) {
		switch (slave->link_new_state) {
		case BOND_LINK_NOCHANGE:
			continue;

		case BOND_LINK_UP:
			last_tx = slave_last_tx(slave);
			if (rtnl_dereference(bond->curr_active_slave) != slave ||
			    (!rtnl_dereference(bond->curr_active_slave) &&
			     bond_time_in_interval(bond, last_tx, 1))) {
				struct slave *current_arp_slave;

				current_arp_slave = rtnl_dereference(bond->current_arp_slave);
				bond_set_slave_link_state(slave, BOND_LINK_UP,
							  BOND_SLAVE_NOTIFY_NOW);
				if (current_arp_slave) {
					bond_set_slave_inactive_flags(
						current_arp_slave,
						BOND_SLAVE_NOTIFY_NOW);
					RCU_INIT_POINTER(bond->current_arp_slave, NULL);
				}

				slave_info(bond->dev, slave->dev, "link status definitely up\n");

				if (!rtnl_dereference(bond->curr_active_slave) ||
				    slave == rtnl_dereference(bond->primary_slave) ||
				    slave->prio > rtnl_dereference(bond->curr_active_slave)->prio)
					do_failover = true;

			}

			continue;

		case BOND_LINK_DOWN:
			if (slave->link_failure_count < UINT_MAX)
				slave->link_failure_count++;

			bond_set_slave_link_state(slave, BOND_LINK_DOWN,
						  BOND_SLAVE_NOTIFY_NOW);
			bond_set_slave_inactive_flags(slave,
						      BOND_SLAVE_NOTIFY_NOW);

			slave_info(bond->dev, slave->dev, "link status definitely down, disabling slave\n");

			if (slave == rtnl_dereference(bond->curr_active_slave)) {
				RCU_INIT_POINTER(bond->current_arp_slave, NULL);
				do_failover = true;
			}

			continue;

		case BOND_LINK_FAIL:
			bond_set_slave_link_state(slave, BOND_LINK_FAIL,
						  BOND_SLAVE_NOTIFY_NOW);
			bond_set_slave_inactive_flags(slave,
						      BOND_SLAVE_NOTIFY_NOW);

			 
			if (rtnl_dereference(bond->curr_active_slave))
				RCU_INIT_POINTER(bond->current_arp_slave, NULL);
			continue;

		default:
			slave_err(bond->dev, slave->dev,
				  "impossible: link_new_state %d on slave\n",
				  slave->link_new_state);
			continue;
		}
	}

	if (do_failover) {
		block_netpoll_tx();
		bond_select_active_slave(bond);
		unblock_netpoll_tx();
	}

	bond_set_carrier(bond);
}

 
static bool bond_ab_arp_probe(struct bonding *bond)
{
	struct slave *slave, *before = NULL, *new_slave = NULL,
		     *curr_arp_slave = rcu_dereference(bond->current_arp_slave),
		     *curr_active_slave = rcu_dereference(bond->curr_active_slave);
	struct list_head *iter;
	bool found = false;
	bool should_notify_rtnl = BOND_SLAVE_NOTIFY_LATER;

	if (curr_arp_slave && curr_active_slave)
		netdev_info(bond->dev, "PROBE: c_arp %s && cas %s BAD\n",
			    curr_arp_slave->dev->name,
			    curr_active_slave->dev->name);

	if (curr_active_slave) {
		bond_send_validate(bond, curr_active_slave);
		return should_notify_rtnl;
	}

	 

	if (!curr_arp_slave) {
		curr_arp_slave = bond_first_slave_rcu(bond);
		if (!curr_arp_slave)
			return should_notify_rtnl;
	}

	bond_for_each_slave_rcu(bond, slave, iter) {
		if (!found && !before && bond_slave_is_up(slave))
			before = slave;

		if (found && !new_slave && bond_slave_is_up(slave))
			new_slave = slave;
		 
		if (!bond_slave_is_up(slave) && slave->link == BOND_LINK_UP) {
			bond_set_slave_link_state(slave, BOND_LINK_DOWN,
						  BOND_SLAVE_NOTIFY_LATER);
			if (slave->link_failure_count < UINT_MAX)
				slave->link_failure_count++;

			bond_set_slave_inactive_flags(slave,
						      BOND_SLAVE_NOTIFY_LATER);

			slave_info(bond->dev, slave->dev, "backup interface is now down\n");
		}
		if (slave == curr_arp_slave)
			found = true;
	}

	if (!new_slave && before)
		new_slave = before;

	if (!new_slave)
		goto check_state;

	bond_set_slave_link_state(new_slave, BOND_LINK_BACK,
				  BOND_SLAVE_NOTIFY_LATER);
	bond_set_slave_active_flags(new_slave, BOND_SLAVE_NOTIFY_LATER);
	bond_send_validate(bond, new_slave);
	new_slave->last_link_up = jiffies;
	rcu_assign_pointer(bond->current_arp_slave, new_slave);

check_state:
	bond_for_each_slave_rcu(bond, slave, iter) {
		if (slave->should_notify || slave->should_notify_link) {
			should_notify_rtnl = BOND_SLAVE_NOTIFY_NOW;
			break;
		}
	}
	return should_notify_rtnl;
}

static void bond_activebackup_arp_mon(struct bonding *bond)
{
	bool should_notify_peers = false;
	bool should_notify_rtnl = false;
	int delta_in_ticks;

	delta_in_ticks = msecs_to_jiffies(bond->params.arp_interval);

	if (!bond_has_slaves(bond))
		goto re_arm;

	rcu_read_lock();

	should_notify_peers = bond_should_notify_peers(bond);

	if (bond_ab_arp_inspect(bond)) {
		rcu_read_unlock();

		 
		if (!rtnl_trylock()) {
			delta_in_ticks = 1;
			should_notify_peers = false;
			goto re_arm;
		}

		bond_ab_arp_commit(bond);

		rtnl_unlock();
		rcu_read_lock();
	}

	should_notify_rtnl = bond_ab_arp_probe(bond);
	rcu_read_unlock();

re_arm:
	if (bond->params.arp_interval)
		queue_delayed_work(bond->wq, &bond->arp_work, delta_in_ticks);

	if (should_notify_peers || should_notify_rtnl) {
		if (!rtnl_trylock())
			return;

		if (should_notify_peers) {
			bond->send_peer_notif--;
			call_netdevice_notifiers(NETDEV_NOTIFY_PEERS,
						 bond->dev);
		}
		if (should_notify_rtnl) {
			bond_slave_state_notify(bond);
			bond_slave_link_notify(bond);
		}

		rtnl_unlock();
	}
}

static void bond_arp_monitor(struct work_struct *work)
{
	struct bonding *bond = container_of(work, struct bonding,
					    arp_work.work);

	if (BOND_MODE(bond) == BOND_MODE_ACTIVEBACKUP)
		bond_activebackup_arp_mon(bond);
	else
		bond_loadbalance_arp_mon(bond);
}

 

 
static int bond_event_changename(struct bonding *bond)
{
	bond_remove_proc_entry(bond);
	bond_create_proc_entry(bond);

	bond_debug_reregister(bond);

	return NOTIFY_DONE;
}

static int bond_master_netdev_event(unsigned long event,
				    struct net_device *bond_dev)
{
	struct bonding *event_bond = netdev_priv(bond_dev);

	netdev_dbg(bond_dev, "%s called\n", __func__);

	switch (event) {
	case NETDEV_CHANGENAME:
		return bond_event_changename(event_bond);
	case NETDEV_UNREGISTER:
		bond_remove_proc_entry(event_bond);
#ifdef CONFIG_XFRM_OFFLOAD
		xfrm_dev_state_flush(dev_net(bond_dev), bond_dev, true);
#endif  
		break;
	case NETDEV_REGISTER:
		bond_create_proc_entry(event_bond);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static int bond_slave_netdev_event(unsigned long event,
				   struct net_device *slave_dev)
{
	struct slave *slave = bond_slave_get_rtnl(slave_dev), *primary;
	struct bonding *bond;
	struct net_device *bond_dev;

	 
	if (!slave) {
		netdev_dbg(slave_dev, "%s called on NULL slave\n", __func__);
		return NOTIFY_DONE;
	}

	bond_dev = slave->bond->dev;
	bond = slave->bond;
	primary = rtnl_dereference(bond->primary_slave);

	slave_dbg(bond_dev, slave_dev, "%s called\n", __func__);

	switch (event) {
	case NETDEV_UNREGISTER:
		if (bond_dev->type != ARPHRD_ETHER)
			bond_release_and_destroy(bond_dev, slave_dev);
		else
			__bond_release_one(bond_dev, slave_dev, false, true);
		break;
	case NETDEV_UP:
	case NETDEV_CHANGE:
		 
		if (bond_update_speed_duplex(slave) &&
		    BOND_MODE(bond) == BOND_MODE_8023AD) {
			if (slave->last_link_up)
				slave->link = BOND_LINK_FAIL;
			else
				slave->link = BOND_LINK_DOWN;
		}

		if (BOND_MODE(bond) == BOND_MODE_8023AD)
			bond_3ad_adapter_speed_duplex_changed(slave);
		fallthrough;
	case NETDEV_DOWN:
		 
		if (bond_mode_can_use_xmit_hash(bond))
			bond_update_slave_arr(bond, NULL);
		break;
	case NETDEV_CHANGEMTU:
		 
		break;
	case NETDEV_CHANGENAME:
		 
		if (!bond_uses_primary(bond) ||
		    !bond->params.primary[0])
			break;

		if (slave == primary) {
			 
			RCU_INIT_POINTER(bond->primary_slave, NULL);
		} else if (!strcmp(slave_dev->name, bond->params.primary)) {
			 
			rcu_assign_pointer(bond->primary_slave, slave);
		} else {  
			break;
		}

		netdev_info(bond->dev, "Primary slave changed to %s, reselecting active slave\n",
			    primary ? slave_dev->name : "none");

		block_netpoll_tx();
		bond_select_active_slave(bond);
		unblock_netpoll_tx();
		break;
	case NETDEV_FEAT_CHANGE:
		if (!bond->notifier_ctx) {
			bond->notifier_ctx = true;
			bond_compute_features(bond);
			bond->notifier_ctx = false;
		}
		break;
	case NETDEV_RESEND_IGMP:
		 
		call_netdevice_notifiers(event, slave->bond->dev);
		break;
	case NETDEV_XDP_FEAT_CHANGE:
		bond_xdp_set_features(bond_dev);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

 
static int bond_netdev_event(struct notifier_block *this,
			     unsigned long event, void *ptr)
{
	struct net_device *event_dev = netdev_notifier_info_to_dev(ptr);

	netdev_dbg(event_dev, "%s received %s\n",
		   __func__, netdev_cmd_to_name(event));

	if (!(event_dev->priv_flags & IFF_BONDING))
		return NOTIFY_DONE;

	if (event_dev->flags & IFF_MASTER) {
		int ret;

		ret = bond_master_netdev_event(event, event_dev);
		if (ret != NOTIFY_DONE)
			return ret;
	}

	if (event_dev->flags & IFF_SLAVE)
		return bond_slave_netdev_event(event, event_dev);

	return NOTIFY_DONE;
}

static struct notifier_block bond_netdev_notifier = {
	.notifier_call = bond_netdev_event,
};

 

 
static inline const void *bond_pull_data(struct sk_buff *skb,
					 const void *data, int hlen, int n)
{
	if (likely(n <= hlen))
		return data;
	else if (skb && likely(pskb_may_pull(skb, n)))
		return skb->data;

	return NULL;
}

 
static inline u32 bond_eth_hash(struct sk_buff *skb, const void *data, int mhoff, int hlen)
{
	struct ethhdr *ep;

	data = bond_pull_data(skb, data, hlen, mhoff + sizeof(struct ethhdr));
	if (!data)
		return 0;

	ep = (struct ethhdr *)(data + mhoff);
	return ep->h_dest[5] ^ ep->h_source[5] ^ be16_to_cpu(ep->h_proto);
}

static bool bond_flow_ip(struct sk_buff *skb, struct flow_keys *fk, const void *data,
			 int hlen, __be16 l2_proto, int *nhoff, int *ip_proto, bool l34)
{
	const struct ipv6hdr *iph6;
	const struct iphdr *iph;

	if (l2_proto == htons(ETH_P_IP)) {
		data = bond_pull_data(skb, data, hlen, *nhoff + sizeof(*iph));
		if (!data)
			return false;

		iph = (const struct iphdr *)(data + *nhoff);
		iph_to_flow_copy_v4addrs(fk, iph);
		*nhoff += iph->ihl << 2;
		if (!ip_is_fragment(iph))
			*ip_proto = iph->protocol;
	} else if (l2_proto == htons(ETH_P_IPV6)) {
		data = bond_pull_data(skb, data, hlen, *nhoff + sizeof(*iph6));
		if (!data)
			return false;

		iph6 = (const struct ipv6hdr *)(data + *nhoff);
		iph_to_flow_copy_v6addrs(fk, iph6);
		*nhoff += sizeof(*iph6);
		*ip_proto = iph6->nexthdr;
	} else {
		return false;
	}

	if (l34 && *ip_proto >= 0)
		fk->ports.ports = __skb_flow_get_ports(skb, *nhoff, *ip_proto, data, hlen);

	return true;
}

static u32 bond_vlan_srcmac_hash(struct sk_buff *skb, const void *data, int mhoff, int hlen)
{
	u32 srcmac_vendor = 0, srcmac_dev = 0;
	struct ethhdr *mac_hdr;
	u16 vlan = 0;
	int i;

	data = bond_pull_data(skb, data, hlen, mhoff + sizeof(struct ethhdr));
	if (!data)
		return 0;
	mac_hdr = (struct ethhdr *)(data + mhoff);

	for (i = 0; i < 3; i++)
		srcmac_vendor = (srcmac_vendor << 8) | mac_hdr->h_source[i];

	for (i = 3; i < ETH_ALEN; i++)
		srcmac_dev = (srcmac_dev << 8) | mac_hdr->h_source[i];

	if (skb && skb_vlan_tag_present(skb))
		vlan = skb_vlan_tag_get(skb);

	return vlan ^ srcmac_vendor ^ srcmac_dev;
}

 
static bool bond_flow_dissect(struct bonding *bond, struct sk_buff *skb, const void *data,
			      __be16 l2_proto, int nhoff, int hlen, struct flow_keys *fk)
{
	bool l34 = bond->params.xmit_policy == BOND_XMIT_POLICY_LAYER34;
	int ip_proto = -1;

	switch (bond->params.xmit_policy) {
	case BOND_XMIT_POLICY_ENCAP23:
	case BOND_XMIT_POLICY_ENCAP34:
		memset(fk, 0, sizeof(*fk));
		return __skb_flow_dissect(NULL, skb, &flow_keys_bonding,
					  fk, data, l2_proto, nhoff, hlen, 0);
	default:
		break;
	}

	fk->ports.ports = 0;
	memset(&fk->icmp, 0, sizeof(fk->icmp));
	if (!bond_flow_ip(skb, fk, data, hlen, l2_proto, &nhoff, &ip_proto, l34))
		return false;

	 
	if (ip_proto == IPPROTO_ICMP || ip_proto == IPPROTO_ICMPV6) {
		skb_flow_get_icmp_tci(skb, &fk->icmp, data, nhoff, hlen);
		if (ip_proto == IPPROTO_ICMP) {
			if (!icmp_is_err(fk->icmp.type))
				return true;

			nhoff += sizeof(struct icmphdr);
		} else if (ip_proto == IPPROTO_ICMPV6) {
			if (!icmpv6_is_err(fk->icmp.type))
				return true;

			nhoff += sizeof(struct icmp6hdr);
		}
		return bond_flow_ip(skb, fk, data, hlen, l2_proto, &nhoff, &ip_proto, l34);
	}

	return true;
}

static u32 bond_ip_hash(u32 hash, struct flow_keys *flow, int xmit_policy)
{
	hash ^= (__force u32)flow_get_u32_dst(flow) ^
		(__force u32)flow_get_u32_src(flow);
	hash ^= (hash >> 16);
	hash ^= (hash >> 8);

	 
	if (xmit_policy == BOND_XMIT_POLICY_LAYER34 ||
		xmit_policy == BOND_XMIT_POLICY_ENCAP34)
		return hash >> 1;

	return hash;
}

 
static u32 __bond_xmit_hash(struct bonding *bond, struct sk_buff *skb, const void *data,
			    __be16 l2_proto, int mhoff, int nhoff, int hlen)
{
	struct flow_keys flow;
	u32 hash;

	if (bond->params.xmit_policy == BOND_XMIT_POLICY_VLAN_SRCMAC)
		return bond_vlan_srcmac_hash(skb, data, mhoff, hlen);

	if (bond->params.xmit_policy == BOND_XMIT_POLICY_LAYER2 ||
	    !bond_flow_dissect(bond, skb, data, l2_proto, nhoff, hlen, &flow))
		return bond_eth_hash(skb, data, mhoff, hlen);

	if (bond->params.xmit_policy == BOND_XMIT_POLICY_LAYER23 ||
	    bond->params.xmit_policy == BOND_XMIT_POLICY_ENCAP23) {
		hash = bond_eth_hash(skb, data, mhoff, hlen);
	} else {
		if (flow.icmp.id)
			memcpy(&hash, &flow.icmp, sizeof(hash));
		else
			memcpy(&hash, &flow.ports.ports, sizeof(hash));
	}

	return bond_ip_hash(hash, &flow, bond->params.xmit_policy);
}

 
u32 bond_xmit_hash(struct bonding *bond, struct sk_buff *skb)
{
	if (bond->params.xmit_policy == BOND_XMIT_POLICY_ENCAP34 &&
	    skb->l4_hash)
		return skb->hash;

	return __bond_xmit_hash(bond, skb, skb->data, skb->protocol,
				0, skb_network_offset(skb),
				skb_headlen(skb));
}

 
static u32 bond_xmit_hash_xdp(struct bonding *bond, struct xdp_buff *xdp)
{
	struct ethhdr *eth;

	if (xdp->data + sizeof(struct ethhdr) > xdp->data_end)
		return 0;

	eth = (struct ethhdr *)xdp->data;

	return __bond_xmit_hash(bond, NULL, xdp->data, eth->h_proto, 0,
				sizeof(struct ethhdr), xdp->data_end - xdp->data);
}

 

void bond_work_init_all(struct bonding *bond)
{
	INIT_DELAYED_WORK(&bond->mcast_work,
			  bond_resend_igmp_join_requests_delayed);
	INIT_DELAYED_WORK(&bond->alb_work, bond_alb_monitor);
	INIT_DELAYED_WORK(&bond->mii_work, bond_mii_monitor);
	INIT_DELAYED_WORK(&bond->arp_work, bond_arp_monitor);
	INIT_DELAYED_WORK(&bond->ad_work, bond_3ad_state_machine_handler);
	INIT_DELAYED_WORK(&bond->slave_arr_work, bond_slave_arr_handler);
}

static void bond_work_cancel_all(struct bonding *bond)
{
	cancel_delayed_work_sync(&bond->mii_work);
	cancel_delayed_work_sync(&bond->arp_work);
	cancel_delayed_work_sync(&bond->alb_work);
	cancel_delayed_work_sync(&bond->ad_work);
	cancel_delayed_work_sync(&bond->mcast_work);
	cancel_delayed_work_sync(&bond->slave_arr_work);
}

static int bond_open(struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct list_head *iter;
	struct slave *slave;

	if (BOND_MODE(bond) == BOND_MODE_ROUNDROBIN && !bond->rr_tx_counter) {
		bond->rr_tx_counter = alloc_percpu(u32);
		if (!bond->rr_tx_counter)
			return -ENOMEM;
	}

	 
	if (bond_has_slaves(bond)) {
		bond_for_each_slave(bond, slave, iter) {
			if (bond_uses_primary(bond) &&
			    slave != rcu_access_pointer(bond->curr_active_slave)) {
				bond_set_slave_inactive_flags(slave,
							      BOND_SLAVE_NOTIFY_NOW);
			} else if (BOND_MODE(bond) != BOND_MODE_8023AD) {
				bond_set_slave_active_flags(slave,
							    BOND_SLAVE_NOTIFY_NOW);
			}
		}
	}

	if (bond_is_lb(bond)) {
		 
		if (bond_alb_initialize(bond, (BOND_MODE(bond) == BOND_MODE_ALB)))
			return -ENOMEM;
		if (bond->params.tlb_dynamic_lb || BOND_MODE(bond) == BOND_MODE_ALB)
			queue_delayed_work(bond->wq, &bond->alb_work, 0);
	}

	if (bond->params.miimon)   
		queue_delayed_work(bond->wq, &bond->mii_work, 0);

	if (bond->params.arp_interval) {   
		queue_delayed_work(bond->wq, &bond->arp_work, 0);
		bond->recv_probe = bond_rcv_validate;
	}

	if (BOND_MODE(bond) == BOND_MODE_8023AD) {
		queue_delayed_work(bond->wq, &bond->ad_work, 0);
		 
		bond->recv_probe = bond_3ad_lacpdu_recv;
		bond_3ad_initiate_agg_selection(bond, 1);

		bond_for_each_slave(bond, slave, iter)
			dev_mc_add(slave->dev, lacpdu_mcast_addr);
	}

	if (bond_mode_can_use_xmit_hash(bond))
		bond_update_slave_arr(bond, NULL);

	return 0;
}

static int bond_close(struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct slave *slave;

	bond_work_cancel_all(bond);
	bond->send_peer_notif = 0;
	if (bond_is_lb(bond))
		bond_alb_deinitialize(bond);
	bond->recv_probe = NULL;

	if (bond_uses_primary(bond)) {
		rcu_read_lock();
		slave = rcu_dereference(bond->curr_active_slave);
		if (slave)
			bond_hw_addr_flush(bond_dev, slave->dev);
		rcu_read_unlock();
	} else {
		struct list_head *iter;

		bond_for_each_slave(bond, slave, iter)
			bond_hw_addr_flush(bond_dev, slave->dev);
	}

	return 0;
}

 
static void bond_fold_stats(struct rtnl_link_stats64 *_res,
			    const struct rtnl_link_stats64 *_new,
			    const struct rtnl_link_stats64 *_old)
{
	const u64 *new = (const u64 *)_new;
	const u64 *old = (const u64 *)_old;
	u64 *res = (u64 *)_res;
	int i;

	for (i = 0; i < sizeof(*_res) / sizeof(u64); i++) {
		u64 nv = new[i];
		u64 ov = old[i];
		s64 delta = nv - ov;

		 
		if (((nv | ov) >> 32) == 0)
			delta = (s64)(s32)((u32)nv - (u32)ov);

		 
		if (delta > 0)
			res[i] += delta;
	}
}

#ifdef CONFIG_LOCKDEP
static int bond_get_lowest_level_rcu(struct net_device *dev)
{
	struct net_device *ldev, *next, *now, *dev_stack[MAX_NEST_DEV + 1];
	struct list_head *niter, *iter, *iter_stack[MAX_NEST_DEV + 1];
	int cur = 0, max = 0;

	now = dev;
	iter = &dev->adj_list.lower;

	while (1) {
		next = NULL;
		while (1) {
			ldev = netdev_next_lower_dev_rcu(now, &iter);
			if (!ldev)
				break;

			next = ldev;
			niter = &ldev->adj_list.lower;
			dev_stack[cur] = now;
			iter_stack[cur++] = iter;
			if (max <= cur)
				max = cur;
			break;
		}

		if (!next) {
			if (!cur)
				return max;
			next = dev_stack[--cur];
			niter = iter_stack[cur];
		}

		now = next;
		iter = niter;
	}

	return max;
}
#endif

static void bond_get_stats(struct net_device *bond_dev,
			   struct rtnl_link_stats64 *stats)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct rtnl_link_stats64 temp;
	struct list_head *iter;
	struct slave *slave;
	int nest_level = 0;


	rcu_read_lock();
#ifdef CONFIG_LOCKDEP
	nest_level = bond_get_lowest_level_rcu(bond_dev);
#endif

	spin_lock_nested(&bond->stats_lock, nest_level);
	memcpy(stats, &bond->bond_stats, sizeof(*stats));

	bond_for_each_slave_rcu(bond, slave, iter) {
		const struct rtnl_link_stats64 *new =
			dev_get_stats(slave->dev, &temp);

		bond_fold_stats(stats, new, &slave->slave_stats);

		 
		memcpy(&slave->slave_stats, new, sizeof(*new));
	}

	memcpy(&bond->bond_stats, stats, sizeof(*stats));
	spin_unlock(&bond->stats_lock);
	rcu_read_unlock();
}

static int bond_eth_ioctl(struct net_device *bond_dev, struct ifreq *ifr, int cmd)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct mii_ioctl_data *mii = NULL;

	netdev_dbg(bond_dev, "bond_eth_ioctl: cmd=%d\n", cmd);

	switch (cmd) {
	case SIOCGMIIPHY:
		mii = if_mii(ifr);
		if (!mii)
			return -EINVAL;

		mii->phy_id = 0;
		fallthrough;
	case SIOCGMIIREG:
		 
		mii = if_mii(ifr);
		if (!mii)
			return -EINVAL;

		if (mii->reg_num == 1) {
			mii->val_out = 0;
			if (netif_carrier_ok(bond->dev))
				mii->val_out = BMSR_LSTATUS;
		}

		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int bond_do_ioctl(struct net_device *bond_dev, struct ifreq *ifr, int cmd)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct net_device *slave_dev = NULL;
	struct ifbond k_binfo;
	struct ifbond __user *u_binfo = NULL;
	struct ifslave k_sinfo;
	struct ifslave __user *u_sinfo = NULL;
	struct bond_opt_value newval;
	struct net *net;
	int res = 0;

	netdev_dbg(bond_dev, "bond_ioctl: cmd=%d\n", cmd);

	switch (cmd) {
	case SIOCBONDINFOQUERY:
		u_binfo = (struct ifbond __user *)ifr->ifr_data;

		if (copy_from_user(&k_binfo, u_binfo, sizeof(ifbond)))
			return -EFAULT;

		bond_info_query(bond_dev, &k_binfo);
		if (copy_to_user(u_binfo, &k_binfo, sizeof(ifbond)))
			return -EFAULT;

		return 0;
	case SIOCBONDSLAVEINFOQUERY:
		u_sinfo = (struct ifslave __user *)ifr->ifr_data;

		if (copy_from_user(&k_sinfo, u_sinfo, sizeof(ifslave)))
			return -EFAULT;

		res = bond_slave_info_query(bond_dev, &k_sinfo);
		if (res == 0 &&
		    copy_to_user(u_sinfo, &k_sinfo, sizeof(ifslave)))
			return -EFAULT;

		return res;
	default:
		break;
	}

	net = dev_net(bond_dev);

	if (!ns_capable(net->user_ns, CAP_NET_ADMIN))
		return -EPERM;

	slave_dev = __dev_get_by_name(net, ifr->ifr_slave);

	slave_dbg(bond_dev, slave_dev, "slave_dev=%p:\n", slave_dev);

	if (!slave_dev)
		return -ENODEV;

	switch (cmd) {
	case SIOCBONDENSLAVE:
		res = bond_enslave(bond_dev, slave_dev, NULL);
		break;
	case SIOCBONDRELEASE:
		res = bond_release(bond_dev, slave_dev);
		break;
	case SIOCBONDSETHWADDR:
		res = bond_set_dev_addr(bond_dev, slave_dev);
		break;
	case SIOCBONDCHANGEACTIVE:
		bond_opt_initstr(&newval, slave_dev->name);
		res = __bond_opt_set_notify(bond, BOND_OPT_ACTIVE_SLAVE,
					    &newval);
		break;
	default:
		res = -EOPNOTSUPP;
	}

	return res;
}

static int bond_siocdevprivate(struct net_device *bond_dev, struct ifreq *ifr,
			       void __user *data, int cmd)
{
	struct ifreq ifrdata = { .ifr_data = data };

	switch (cmd) {
	case BOND_INFO_QUERY_OLD:
		return bond_do_ioctl(bond_dev, &ifrdata, SIOCBONDINFOQUERY);
	case BOND_SLAVE_INFO_QUERY_OLD:
		return bond_do_ioctl(bond_dev, &ifrdata, SIOCBONDSLAVEINFOQUERY);
	case BOND_ENSLAVE_OLD:
		return bond_do_ioctl(bond_dev, ifr, SIOCBONDENSLAVE);
	case BOND_RELEASE_OLD:
		return bond_do_ioctl(bond_dev, ifr, SIOCBONDRELEASE);
	case BOND_SETHWADDR_OLD:
		return bond_do_ioctl(bond_dev, ifr, SIOCBONDSETHWADDR);
	case BOND_CHANGE_ACTIVE_OLD:
		return bond_do_ioctl(bond_dev, ifr, SIOCBONDCHANGEACTIVE);
	}

	return -EOPNOTSUPP;
}

static void bond_change_rx_flags(struct net_device *bond_dev, int change)
{
	struct bonding *bond = netdev_priv(bond_dev);

	if (change & IFF_PROMISC)
		bond_set_promiscuity(bond,
				     bond_dev->flags & IFF_PROMISC ? 1 : -1);

	if (change & IFF_ALLMULTI)
		bond_set_allmulti(bond,
				  bond_dev->flags & IFF_ALLMULTI ? 1 : -1);
}

static void bond_set_rx_mode(struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct list_head *iter;
	struct slave *slave;

	rcu_read_lock();
	if (bond_uses_primary(bond)) {
		slave = rcu_dereference(bond->curr_active_slave);
		if (slave) {
			dev_uc_sync(slave->dev, bond_dev);
			dev_mc_sync(slave->dev, bond_dev);
		}
	} else {
		bond_for_each_slave_rcu(bond, slave, iter) {
			dev_uc_sync_multiple(slave->dev, bond_dev);
			dev_mc_sync_multiple(slave->dev, bond_dev);
		}
	}
	rcu_read_unlock();
}

static int bond_neigh_init(struct neighbour *n)
{
	struct bonding *bond = netdev_priv(n->dev);
	const struct net_device_ops *slave_ops;
	struct neigh_parms parms;
	struct slave *slave;
	int ret = 0;

	rcu_read_lock();
	slave = bond_first_slave_rcu(bond);
	if (!slave)
		goto out;
	slave_ops = slave->dev->netdev_ops;
	if (!slave_ops->ndo_neigh_setup)
		goto out;

	 
	memset(&parms, 0, sizeof(parms));
	ret = slave_ops->ndo_neigh_setup(slave->dev, &parms);

	if (ret)
		goto out;

	if (parms.neigh_setup)
		ret = parms.neigh_setup(n);
out:
	rcu_read_unlock();
	return ret;
}

 
static int bond_neigh_setup(struct net_device *dev,
			    struct neigh_parms *parms)
{
	 
	if (parms->dev == dev)
		parms->neigh_setup = bond_neigh_init;

	return 0;
}

 
static int bond_change_mtu(struct net_device *bond_dev, int new_mtu)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct slave *slave, *rollback_slave;
	struct list_head *iter;
	int res = 0;

	netdev_dbg(bond_dev, "bond=%p, new_mtu=%d\n", bond, new_mtu);

	bond_for_each_slave(bond, slave, iter) {
		slave_dbg(bond_dev, slave->dev, "s %p c_m %p\n",
			   slave, slave->dev->netdev_ops->ndo_change_mtu);

		res = dev_set_mtu(slave->dev, new_mtu);

		if (res) {
			 
			slave_dbg(bond_dev, slave->dev, "err %d setting mtu to %d\n",
				  res, new_mtu);
			goto unwind;
		}
	}

	bond_dev->mtu = new_mtu;

	return 0;

unwind:
	 
	bond_for_each_slave(bond, rollback_slave, iter) {
		int tmp_res;

		if (rollback_slave == slave)
			break;

		tmp_res = dev_set_mtu(rollback_slave->dev, bond_dev->mtu);
		if (tmp_res)
			slave_dbg(bond_dev, rollback_slave->dev, "unwind err %d\n",
				  tmp_res);
	}

	return res;
}

 
static int bond_set_mac_address(struct net_device *bond_dev, void *addr)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct slave *slave, *rollback_slave;
	struct sockaddr_storage *ss = addr, tmp_ss;
	struct list_head *iter;
	int res = 0;

	if (BOND_MODE(bond) == BOND_MODE_ALB)
		return bond_alb_set_mac_address(bond_dev, addr);


	netdev_dbg(bond_dev, "%s: bond=%p\n", __func__, bond);

	 
	if (bond->params.fail_over_mac &&
	    BOND_MODE(bond) == BOND_MODE_ACTIVEBACKUP)
		return 0;

	if (!is_valid_ether_addr(ss->__data))
		return -EADDRNOTAVAIL;

	bond_for_each_slave(bond, slave, iter) {
		slave_dbg(bond_dev, slave->dev, "%s: slave=%p\n",
			  __func__, slave);
		res = dev_set_mac_address(slave->dev, addr, NULL);
		if (res) {
			 
			slave_dbg(bond_dev, slave->dev, "%s: err %d\n",
				  __func__, res);
			goto unwind;
		}
	}

	 
	dev_addr_set(bond_dev, ss->__data);
	return 0;

unwind:
	memcpy(tmp_ss.__data, bond_dev->dev_addr, bond_dev->addr_len);
	tmp_ss.ss_family = bond_dev->type;

	 
	bond_for_each_slave(bond, rollback_slave, iter) {
		int tmp_res;

		if (rollback_slave == slave)
			break;

		tmp_res = dev_set_mac_address(rollback_slave->dev,
					      (struct sockaddr *)&tmp_ss, NULL);
		if (tmp_res) {
			slave_dbg(bond_dev, rollback_slave->dev, "%s: unwind err %d\n",
				   __func__, tmp_res);
		}
	}

	return res;
}

 
static struct slave *bond_get_slave_by_id(struct bonding *bond,
					  int slave_id)
{
	struct list_head *iter;
	struct slave *slave;
	int i = slave_id;

	 
	bond_for_each_slave_rcu(bond, slave, iter) {
		if (--i < 0) {
			if (bond_slave_can_tx(slave))
				return slave;
		}
	}

	 
	i = slave_id;
	bond_for_each_slave_rcu(bond, slave, iter) {
		if (--i < 0)
			break;
		if (bond_slave_can_tx(slave))
			return slave;
	}
	 
	return NULL;
}

 
static u32 bond_rr_gen_slave_id(struct bonding *bond)
{
	u32 slave_id;
	struct reciprocal_value reciprocal_packets_per_slave;
	int packets_per_slave = bond->params.packets_per_slave;

	switch (packets_per_slave) {
	case 0:
		slave_id = get_random_u32();
		break;
	case 1:
		slave_id = this_cpu_inc_return(*bond->rr_tx_counter);
		break;
	default:
		reciprocal_packets_per_slave =
			bond->params.reciprocal_packets_per_slave;
		slave_id = this_cpu_inc_return(*bond->rr_tx_counter);
		slave_id = reciprocal_divide(slave_id,
					     reciprocal_packets_per_slave);
		break;
	}

	return slave_id;
}

static struct slave *bond_xmit_roundrobin_slave_get(struct bonding *bond,
						    struct sk_buff *skb)
{
	struct slave *slave;
	int slave_cnt;
	u32 slave_id;

	 
	if (skb->protocol == htons(ETH_P_IP)) {
		int noff = skb_network_offset(skb);
		struct iphdr *iph;

		if (unlikely(!pskb_may_pull(skb, noff + sizeof(*iph))))
			goto non_igmp;

		iph = ip_hdr(skb);
		if (iph->protocol == IPPROTO_IGMP) {
			slave = rcu_dereference(bond->curr_active_slave);
			if (slave)
				return slave;
			return bond_get_slave_by_id(bond, 0);
		}
	}

non_igmp:
	slave_cnt = READ_ONCE(bond->slave_cnt);
	if (likely(slave_cnt)) {
		slave_id = bond_rr_gen_slave_id(bond) % slave_cnt;
		return bond_get_slave_by_id(bond, slave_id);
	}
	return NULL;
}

static struct slave *bond_xdp_xmit_roundrobin_slave_get(struct bonding *bond,
							struct xdp_buff *xdp)
{
	struct slave *slave;
	int slave_cnt;
	u32 slave_id;
	const struct ethhdr *eth;
	void *data = xdp->data;

	if (data + sizeof(struct ethhdr) > xdp->data_end)
		goto non_igmp;

	eth = (struct ethhdr *)data;
	data += sizeof(struct ethhdr);

	 
	if (eth->h_proto == htons(ETH_P_IP)) {
		const struct iphdr *iph;

		if (data + sizeof(struct iphdr) > xdp->data_end)
			goto non_igmp;

		iph = (struct iphdr *)data;

		if (iph->protocol == IPPROTO_IGMP) {
			slave = rcu_dereference(bond->curr_active_slave);
			if (slave)
				return slave;
			return bond_get_slave_by_id(bond, 0);
		}
	}

non_igmp:
	slave_cnt = READ_ONCE(bond->slave_cnt);
	if (likely(slave_cnt)) {
		slave_id = bond_rr_gen_slave_id(bond) % slave_cnt;
		return bond_get_slave_by_id(bond, slave_id);
	}
	return NULL;
}

static netdev_tx_t bond_xmit_roundrobin(struct sk_buff *skb,
					struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct slave *slave;

	slave = bond_xmit_roundrobin_slave_get(bond, skb);
	if (likely(slave))
		return bond_dev_queue_xmit(bond, skb, slave->dev);

	return bond_tx_drop(bond_dev, skb);
}

static struct slave *bond_xmit_activebackup_slave_get(struct bonding *bond)
{
	return rcu_dereference(bond->curr_active_slave);
}

 
static netdev_tx_t bond_xmit_activebackup(struct sk_buff *skb,
					  struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct slave *slave;

	slave = bond_xmit_activebackup_slave_get(bond);
	if (slave)
		return bond_dev_queue_xmit(bond, skb, slave->dev);

	return bond_tx_drop(bond_dev, skb);
}

 
void bond_slave_arr_work_rearm(struct bonding *bond, unsigned long delay)
{
	queue_delayed_work(bond->wq, &bond->slave_arr_work, delay);
}

 
static void bond_slave_arr_handler(struct work_struct *work)
{
	struct bonding *bond = container_of(work, struct bonding,
					    slave_arr_work.work);
	int ret;

	if (!rtnl_trylock())
		goto err;

	ret = bond_update_slave_arr(bond, NULL);
	rtnl_unlock();
	if (ret) {
		pr_warn_ratelimited("Failed to update slave array from WT\n");
		goto err;
	}
	return;

err:
	bond_slave_arr_work_rearm(bond, 1);
}

static void bond_skip_slave(struct bond_up_slave *slaves,
			    struct slave *skipslave)
{
	int idx;

	 
	for (idx = 0; slaves && idx < slaves->count; idx++) {
		if (skipslave == slaves->arr[idx]) {
			slaves->arr[idx] =
				slaves->arr[slaves->count - 1];
			slaves->count--;
			break;
		}
	}
}

static void bond_set_slave_arr(struct bonding *bond,
			       struct bond_up_slave *usable_slaves,
			       struct bond_up_slave *all_slaves)
{
	struct bond_up_slave *usable, *all;

	usable = rtnl_dereference(bond->usable_slaves);
	rcu_assign_pointer(bond->usable_slaves, usable_slaves);
	kfree_rcu(usable, rcu);

	all = rtnl_dereference(bond->all_slaves);
	rcu_assign_pointer(bond->all_slaves, all_slaves);
	kfree_rcu(all, rcu);
}

static void bond_reset_slave_arr(struct bonding *bond)
{
	bond_set_slave_arr(bond, NULL, NULL);
}

 
int bond_update_slave_arr(struct bonding *bond, struct slave *skipslave)
{
	struct bond_up_slave *usable_slaves = NULL, *all_slaves = NULL;
	struct slave *slave;
	struct list_head *iter;
	int agg_id = 0;
	int ret = 0;

	might_sleep();

	usable_slaves = kzalloc(struct_size(usable_slaves, arr,
					    bond->slave_cnt), GFP_KERNEL);
	all_slaves = kzalloc(struct_size(all_slaves, arr,
					 bond->slave_cnt), GFP_KERNEL);
	if (!usable_slaves || !all_slaves) {
		ret = -ENOMEM;
		goto out;
	}
	if (BOND_MODE(bond) == BOND_MODE_8023AD) {
		struct ad_info ad_info;

		spin_lock_bh(&bond->mode_lock);
		if (bond_3ad_get_active_agg_info(bond, &ad_info)) {
			spin_unlock_bh(&bond->mode_lock);
			pr_debug("bond_3ad_get_active_agg_info failed\n");
			 
			bond_reset_slave_arr(bond);
			goto out;
		}
		spin_unlock_bh(&bond->mode_lock);
		agg_id = ad_info.aggregator_id;
	}
	bond_for_each_slave(bond, slave, iter) {
		if (skipslave == slave)
			continue;

		all_slaves->arr[all_slaves->count++] = slave;
		if (BOND_MODE(bond) == BOND_MODE_8023AD) {
			struct aggregator *agg;

			agg = SLAVE_AD_INFO(slave)->port.aggregator;
			if (!agg || agg->aggregator_identifier != agg_id)
				continue;
		}
		if (!bond_slave_can_tx(slave))
			continue;

		slave_dbg(bond->dev, slave->dev, "Adding slave to tx hash array[%d]\n",
			  usable_slaves->count);

		usable_slaves->arr[usable_slaves->count++] = slave;
	}

	bond_set_slave_arr(bond, usable_slaves, all_slaves);
	return ret;
out:
	if (ret != 0 && skipslave) {
		bond_skip_slave(rtnl_dereference(bond->all_slaves),
				skipslave);
		bond_skip_slave(rtnl_dereference(bond->usable_slaves),
				skipslave);
	}
	kfree_rcu(all_slaves, rcu);
	kfree_rcu(usable_slaves, rcu);

	return ret;
}

static struct slave *bond_xmit_3ad_xor_slave_get(struct bonding *bond,
						 struct sk_buff *skb,
						 struct bond_up_slave *slaves)
{
	struct slave *slave;
	unsigned int count;
	u32 hash;

	hash = bond_xmit_hash(bond, skb);
	count = slaves ? READ_ONCE(slaves->count) : 0;
	if (unlikely(!count))
		return NULL;

	slave = slaves->arr[hash % count];
	return slave;
}

static struct slave *bond_xdp_xmit_3ad_xor_slave_get(struct bonding *bond,
						     struct xdp_buff *xdp)
{
	struct bond_up_slave *slaves;
	unsigned int count;
	u32 hash;

	hash = bond_xmit_hash_xdp(bond, xdp);
	slaves = rcu_dereference(bond->usable_slaves);
	count = slaves ? READ_ONCE(slaves->count) : 0;
	if (unlikely(!count))
		return NULL;

	return slaves->arr[hash % count];
}

 
static netdev_tx_t bond_3ad_xor_xmit(struct sk_buff *skb,
				     struct net_device *dev)
{
	struct bonding *bond = netdev_priv(dev);
	struct bond_up_slave *slaves;
	struct slave *slave;

	slaves = rcu_dereference(bond->usable_slaves);
	slave = bond_xmit_3ad_xor_slave_get(bond, skb, slaves);
	if (likely(slave))
		return bond_dev_queue_xmit(bond, skb, slave->dev);

	return bond_tx_drop(dev, skb);
}

 
static netdev_tx_t bond_xmit_broadcast(struct sk_buff *skb,
				       struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct slave *slave = NULL;
	struct list_head *iter;
	bool xmit_suc = false;
	bool skb_used = false;

	bond_for_each_slave_rcu(bond, slave, iter) {
		struct sk_buff *skb2;

		if (!(bond_slave_is_up(slave) && slave->link == BOND_LINK_UP))
			continue;

		if (bond_is_last_slave(bond, slave)) {
			skb2 = skb;
			skb_used = true;
		} else {
			skb2 = skb_clone(skb, GFP_ATOMIC);
			if (!skb2) {
				net_err_ratelimited("%s: Error: %s: skb_clone() failed\n",
						    bond_dev->name, __func__);
				continue;
			}
		}

		if (bond_dev_queue_xmit(bond, skb2, slave->dev) == NETDEV_TX_OK)
			xmit_suc = true;
	}

	if (!skb_used)
		dev_kfree_skb_any(skb);

	if (xmit_suc)
		return NETDEV_TX_OK;

	dev_core_stats_tx_dropped_inc(bond_dev);
	return NET_XMIT_DROP;
}

 

 
static inline int bond_slave_override(struct bonding *bond,
				      struct sk_buff *skb)
{
	struct slave *slave = NULL;
	struct list_head *iter;

	if (!skb_rx_queue_recorded(skb))
		return 1;

	 
	bond_for_each_slave_rcu(bond, slave, iter) {
		if (slave->queue_id == skb_get_queue_mapping(skb)) {
			if (bond_slave_is_up(slave) &&
			    slave->link == BOND_LINK_UP) {
				bond_dev_queue_xmit(bond, skb, slave->dev);
				return 0;
			}
			 
			break;
		}
	}

	return 1;
}


static u16 bond_select_queue(struct net_device *dev, struct sk_buff *skb,
			     struct net_device *sb_dev)
{
	 
	u16 txq = skb_rx_queue_recorded(skb) ? skb_get_rx_queue(skb) : 0;

	 
	qdisc_skb_cb(skb)->slave_dev_queue_mapping = skb_get_queue_mapping(skb);

	if (unlikely(txq >= dev->real_num_tx_queues)) {
		do {
			txq -= dev->real_num_tx_queues;
		} while (txq >= dev->real_num_tx_queues);
	}
	return txq;
}

static struct net_device *bond_xmit_get_slave(struct net_device *master_dev,
					      struct sk_buff *skb,
					      bool all_slaves)
{
	struct bonding *bond = netdev_priv(master_dev);
	struct bond_up_slave *slaves;
	struct slave *slave = NULL;

	switch (BOND_MODE(bond)) {
	case BOND_MODE_ROUNDROBIN:
		slave = bond_xmit_roundrobin_slave_get(bond, skb);
		break;
	case BOND_MODE_ACTIVEBACKUP:
		slave = bond_xmit_activebackup_slave_get(bond);
		break;
	case BOND_MODE_8023AD:
	case BOND_MODE_XOR:
		if (all_slaves)
			slaves = rcu_dereference(bond->all_slaves);
		else
			slaves = rcu_dereference(bond->usable_slaves);
		slave = bond_xmit_3ad_xor_slave_get(bond, skb, slaves);
		break;
	case BOND_MODE_BROADCAST:
		break;
	case BOND_MODE_ALB:
		slave = bond_xmit_alb_slave_get(bond, skb);
		break;
	case BOND_MODE_TLB:
		slave = bond_xmit_tlb_slave_get(bond, skb);
		break;
	default:
		 
		WARN_ONCE(true, "Unknown bonding mode");
		break;
	}

	if (slave)
		return slave->dev;
	return NULL;
}

static void bond_sk_to_flow(struct sock *sk, struct flow_keys *flow)
{
	switch (sk->sk_family) {
#if IS_ENABLED(CONFIG_IPV6)
	case AF_INET6:
		if (ipv6_only_sock(sk) ||
		    ipv6_addr_type(&sk->sk_v6_daddr) != IPV6_ADDR_MAPPED) {
			flow->control.addr_type = FLOW_DISSECTOR_KEY_IPV6_ADDRS;
			flow->addrs.v6addrs.src = inet6_sk(sk)->saddr;
			flow->addrs.v6addrs.dst = sk->sk_v6_daddr;
			break;
		}
		fallthrough;
#endif
	default:  
		flow->control.addr_type = FLOW_DISSECTOR_KEY_IPV4_ADDRS;
		flow->addrs.v4addrs.src = inet_sk(sk)->inet_rcv_saddr;
		flow->addrs.v4addrs.dst = inet_sk(sk)->inet_daddr;
		break;
	}

	flow->ports.src = inet_sk(sk)->inet_sport;
	flow->ports.dst = inet_sk(sk)->inet_dport;
}

 
static u32 bond_sk_hash_l34(struct sock *sk)
{
	struct flow_keys flow;
	u32 hash;

	bond_sk_to_flow(sk, &flow);

	 
	memcpy(&hash, &flow.ports.ports, sizeof(hash));
	 
	return bond_ip_hash(hash, &flow, BOND_XMIT_POLICY_LAYER34);
}

static struct net_device *__bond_sk_get_lower_dev(struct bonding *bond,
						  struct sock *sk)
{
	struct bond_up_slave *slaves;
	struct slave *slave;
	unsigned int count;
	u32 hash;

	slaves = rcu_dereference(bond->usable_slaves);
	count = slaves ? READ_ONCE(slaves->count) : 0;
	if (unlikely(!count))
		return NULL;

	hash = bond_sk_hash_l34(sk);
	slave = slaves->arr[hash % count];

	return slave->dev;
}

static struct net_device *bond_sk_get_lower_dev(struct net_device *dev,
						struct sock *sk)
{
	struct bonding *bond = netdev_priv(dev);
	struct net_device *lower = NULL;

	rcu_read_lock();
	if (bond_sk_check(bond))
		lower = __bond_sk_get_lower_dev(bond, sk);
	rcu_read_unlock();

	return lower;
}

#if IS_ENABLED(CONFIG_TLS_DEVICE)
static netdev_tx_t bond_tls_device_xmit(struct bonding *bond, struct sk_buff *skb,
					struct net_device *dev)
{
	struct net_device *tls_netdev = rcu_dereference(tls_get_ctx(skb->sk)->netdev);

	 
	if (likely(bond_get_slave_by_dev(bond, tls_netdev)))
		return bond_dev_queue_xmit(bond, skb, tls_netdev);
	return bond_tx_drop(dev, skb);
}
#endif

static netdev_tx_t __bond_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct bonding *bond = netdev_priv(dev);

	if (bond_should_override_tx_queue(bond) &&
	    !bond_slave_override(bond, skb))
		return NETDEV_TX_OK;

#if IS_ENABLED(CONFIG_TLS_DEVICE)
	if (tls_is_skb_tx_device_offloaded(skb))
		return bond_tls_device_xmit(bond, skb, dev);
#endif

	switch (BOND_MODE(bond)) {
	case BOND_MODE_ROUNDROBIN:
		return bond_xmit_roundrobin(skb, dev);
	case BOND_MODE_ACTIVEBACKUP:
		return bond_xmit_activebackup(skb, dev);
	case BOND_MODE_8023AD:
	case BOND_MODE_XOR:
		return bond_3ad_xor_xmit(skb, dev);
	case BOND_MODE_BROADCAST:
		return bond_xmit_broadcast(skb, dev);
	case BOND_MODE_ALB:
		return bond_alb_xmit(skb, dev);
	case BOND_MODE_TLB:
		return bond_tlb_xmit(skb, dev);
	default:
		 
		netdev_err(dev, "Unknown bonding mode %d\n", BOND_MODE(bond));
		WARN_ON_ONCE(1);
		return bond_tx_drop(dev, skb);
	}
}

static netdev_tx_t bond_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct bonding *bond = netdev_priv(dev);
	netdev_tx_t ret = NETDEV_TX_OK;

	 
	if (unlikely(is_netpoll_tx_blocked(dev)))
		return NETDEV_TX_BUSY;

	rcu_read_lock();
	if (bond_has_slaves(bond))
		ret = __bond_start_xmit(skb, dev);
	else
		ret = bond_tx_drop(dev, skb);
	rcu_read_unlock();

	return ret;
}

static struct net_device *
bond_xdp_get_xmit_slave(struct net_device *bond_dev, struct xdp_buff *xdp)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct slave *slave;

	 

	switch (BOND_MODE(bond)) {
	case BOND_MODE_ROUNDROBIN:
		slave = bond_xdp_xmit_roundrobin_slave_get(bond, xdp);
		break;

	case BOND_MODE_ACTIVEBACKUP:
		slave = bond_xmit_activebackup_slave_get(bond);
		break;

	case BOND_MODE_8023AD:
	case BOND_MODE_XOR:
		slave = bond_xdp_xmit_3ad_xor_slave_get(bond, xdp);
		break;

	default:
		 
		netdev_err(bond_dev, "Unknown bonding mode %d for xdp xmit\n", BOND_MODE(bond));
		WARN_ON_ONCE(1);
		return NULL;
	}

	if (slave)
		return slave->dev;

	return NULL;
}

static int bond_xdp_xmit(struct net_device *bond_dev,
			 int n, struct xdp_frame **frames, u32 flags)
{
	int nxmit, err = -ENXIO;

	rcu_read_lock();

	for (nxmit = 0; nxmit < n; nxmit++) {
		struct xdp_frame *frame = frames[nxmit];
		struct xdp_frame *frames1[] = {frame};
		struct net_device *slave_dev;
		struct xdp_buff xdp;

		xdp_convert_frame_to_buff(frame, &xdp);

		slave_dev = bond_xdp_get_xmit_slave(bond_dev, &xdp);
		if (!slave_dev) {
			err = -ENXIO;
			break;
		}

		err = slave_dev->netdev_ops->ndo_xdp_xmit(slave_dev, 1, frames1, flags);
		if (err < 1)
			break;
	}

	rcu_read_unlock();

	 
	if (err < 0)
		return (nxmit == 0 ? err : nxmit);

	return nxmit;
}

static int bond_xdp_set(struct net_device *dev, struct bpf_prog *prog,
			struct netlink_ext_ack *extack)
{
	struct bonding *bond = netdev_priv(dev);
	struct list_head *iter;
	struct slave *slave, *rollback_slave;
	struct bpf_prog *old_prog;
	struct netdev_bpf xdp = {
		.command = XDP_SETUP_PROG,
		.flags   = 0,
		.prog    = prog,
		.extack  = extack,
	};
	int err;

	ASSERT_RTNL();

	if (!bond_xdp_check(bond))
		return -EOPNOTSUPP;

	old_prog = bond->xdp_prog;
	bond->xdp_prog = prog;

	bond_for_each_slave(bond, slave, iter) {
		struct net_device *slave_dev = slave->dev;

		if (!slave_dev->netdev_ops->ndo_bpf ||
		    !slave_dev->netdev_ops->ndo_xdp_xmit) {
			SLAVE_NL_ERR(dev, slave_dev, extack,
				     "Slave device does not support XDP");
			err = -EOPNOTSUPP;
			goto err;
		}

		if (dev_xdp_prog_count(slave_dev) > 0) {
			SLAVE_NL_ERR(dev, slave_dev, extack,
				     "Slave has XDP program loaded, please unload before enslaving");
			err = -EOPNOTSUPP;
			goto err;
		}

		err = slave_dev->netdev_ops->ndo_bpf(slave_dev, &xdp);
		if (err < 0) {
			 
			slave_err(dev, slave_dev, "Error %d calling ndo_bpf\n", err);
			goto err;
		}
		if (prog)
			bpf_prog_inc(prog);
	}

	if (prog) {
		static_branch_inc(&bpf_master_redirect_enabled_key);
	} else if (old_prog) {
		bpf_prog_put(old_prog);
		static_branch_dec(&bpf_master_redirect_enabled_key);
	}

	return 0;

err:
	 
	bond->xdp_prog = old_prog;
	xdp.prog = old_prog;
	xdp.extack = NULL;  

	bond_for_each_slave(bond, rollback_slave, iter) {
		struct net_device *slave_dev = rollback_slave->dev;
		int err_unwind;

		if (slave == rollback_slave)
			break;

		err_unwind = slave_dev->netdev_ops->ndo_bpf(slave_dev, &xdp);
		if (err_unwind < 0)
			slave_err(dev, slave_dev,
				  "Error %d when unwinding XDP program change\n", err_unwind);
		else if (xdp.prog)
			bpf_prog_inc(xdp.prog);
	}
	return err;
}

static int bond_xdp(struct net_device *dev, struct netdev_bpf *xdp)
{
	switch (xdp->command) {
	case XDP_SETUP_PROG:
		return bond_xdp_set(dev, xdp->prog, xdp->extack);
	default:
		return -EINVAL;
	}
}

static u32 bond_mode_bcast_speed(struct slave *slave, u32 speed)
{
	if (speed == 0 || speed == SPEED_UNKNOWN)
		speed = slave->speed;
	else
		speed = min(speed, slave->speed);

	return speed;
}

 
static int bond_set_phc_index_flag(struct kernel_hwtstamp_config *kernel_cfg)
{
	struct ifreq *ifr = kernel_cfg->ifr;
	struct hwtstamp_config cfg;

	if (kernel_cfg->copied_to_user) {
		 
		if (copy_from_user(&cfg, ifr->ifr_data, sizeof(cfg)))
			return -EFAULT;

		cfg.flags |= HWTSTAMP_FLAG_BONDED_PHC_INDEX;
		if (copy_to_user(ifr->ifr_data, &cfg, sizeof(cfg)))
			return -EFAULT;
	} else {
		kernel_cfg->flags |= HWTSTAMP_FLAG_BONDED_PHC_INDEX;
	}

	return 0;
}

static int bond_hwtstamp_get(struct net_device *dev,
			     struct kernel_hwtstamp_config *cfg)
{
	struct bonding *bond = netdev_priv(dev);
	struct net_device *real_dev;
	int err;

	real_dev = bond_option_active_slave_get_rcu(bond);
	if (!real_dev)
		return -EOPNOTSUPP;

	err = generic_hwtstamp_get_lower(real_dev, cfg);
	if (err)
		return err;

	return bond_set_phc_index_flag(cfg);
}

static int bond_hwtstamp_set(struct net_device *dev,
			     struct kernel_hwtstamp_config *cfg,
			     struct netlink_ext_ack *extack)
{
	struct bonding *bond = netdev_priv(dev);
	struct net_device *real_dev;
	int err;

	if (!(cfg->flags & HWTSTAMP_FLAG_BONDED_PHC_INDEX))
		return -EOPNOTSUPP;

	real_dev = bond_option_active_slave_get_rcu(bond);
	if (!real_dev)
		return -EOPNOTSUPP;

	err = generic_hwtstamp_set_lower(real_dev, cfg, extack);
	if (err)
		return err;

	return bond_set_phc_index_flag(cfg);
}

static int bond_ethtool_get_link_ksettings(struct net_device *bond_dev,
					   struct ethtool_link_ksettings *cmd)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct list_head *iter;
	struct slave *slave;
	u32 speed = 0;

	cmd->base.duplex = DUPLEX_UNKNOWN;
	cmd->base.port = PORT_OTHER;

	 
	bond_for_each_slave(bond, slave, iter) {
		if (bond_slave_can_tx(slave)) {
			bond_update_speed_duplex(slave);
			if (slave->speed != SPEED_UNKNOWN) {
				if (BOND_MODE(bond) == BOND_MODE_BROADCAST)
					speed = bond_mode_bcast_speed(slave,
								      speed);
				else
					speed += slave->speed;
			}
			if (cmd->base.duplex == DUPLEX_UNKNOWN &&
			    slave->duplex != DUPLEX_UNKNOWN)
				cmd->base.duplex = slave->duplex;
		}
	}
	cmd->base.speed = speed ? : SPEED_UNKNOWN;

	return 0;
}

static void bond_ethtool_get_drvinfo(struct net_device *bond_dev,
				     struct ethtool_drvinfo *drvinfo)
{
	strscpy(drvinfo->driver, DRV_NAME, sizeof(drvinfo->driver));
	snprintf(drvinfo->fw_version, sizeof(drvinfo->fw_version), "%d",
		 BOND_ABI_VERSION);
}

static int bond_ethtool_get_ts_info(struct net_device *bond_dev,
				    struct ethtool_ts_info *info)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct ethtool_ts_info ts_info;
	const struct ethtool_ops *ops;
	struct net_device *real_dev;
	bool sw_tx_support = false;
	struct phy_device *phydev;
	struct list_head *iter;
	struct slave *slave;
	int ret = 0;

	rcu_read_lock();
	real_dev = bond_option_active_slave_get_rcu(bond);
	dev_hold(real_dev);
	rcu_read_unlock();

	if (real_dev) {
		ops = real_dev->ethtool_ops;
		phydev = real_dev->phydev;

		if (phy_has_tsinfo(phydev)) {
			ret = phy_ts_info(phydev, info);
			goto out;
		} else if (ops->get_ts_info) {
			ret = ops->get_ts_info(real_dev, info);
			goto out;
		}
	} else {
		 
		rcu_read_lock();
		bond_for_each_slave_rcu(bond, slave, iter) {
			ret = -1;
			ops = slave->dev->ethtool_ops;
			phydev = slave->dev->phydev;

			if (phy_has_tsinfo(phydev))
				ret = phy_ts_info(phydev, &ts_info);
			else if (ops->get_ts_info)
				ret = ops->get_ts_info(slave->dev, &ts_info);

			if (!ret && (ts_info.so_timestamping & SOF_TIMESTAMPING_TX_SOFTWARE)) {
				sw_tx_support = true;
				continue;
			}

			sw_tx_support = false;
			break;
		}
		rcu_read_unlock();
	}

	ret = 0;
	info->so_timestamping = SOF_TIMESTAMPING_RX_SOFTWARE |
				SOF_TIMESTAMPING_SOFTWARE;
	if (sw_tx_support)
		info->so_timestamping |= SOF_TIMESTAMPING_TX_SOFTWARE;

	info->phc_index = -1;

out:
	dev_put(real_dev);
	return ret;
}

static const struct ethtool_ops bond_ethtool_ops = {
	.get_drvinfo		= bond_ethtool_get_drvinfo,
	.get_link		= ethtool_op_get_link,
	.get_link_ksettings	= bond_ethtool_get_link_ksettings,
	.get_ts_info		= bond_ethtool_get_ts_info,
};

static const struct net_device_ops bond_netdev_ops = {
	.ndo_init		= bond_init,
	.ndo_uninit		= bond_uninit,
	.ndo_open		= bond_open,
	.ndo_stop		= bond_close,
	.ndo_start_xmit		= bond_start_xmit,
	.ndo_select_queue	= bond_select_queue,
	.ndo_get_stats64	= bond_get_stats,
	.ndo_eth_ioctl		= bond_eth_ioctl,
	.ndo_siocbond		= bond_do_ioctl,
	.ndo_siocdevprivate	= bond_siocdevprivate,
	.ndo_change_rx_flags	= bond_change_rx_flags,
	.ndo_set_rx_mode	= bond_set_rx_mode,
	.ndo_change_mtu		= bond_change_mtu,
	.ndo_set_mac_address	= bond_set_mac_address,
	.ndo_neigh_setup	= bond_neigh_setup,
	.ndo_vlan_rx_add_vid	= bond_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid	= bond_vlan_rx_kill_vid,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_netpoll_setup	= bond_netpoll_setup,
	.ndo_netpoll_cleanup	= bond_netpoll_cleanup,
	.ndo_poll_controller	= bond_poll_controller,
#endif
	.ndo_add_slave		= bond_enslave,
	.ndo_del_slave		= bond_release,
	.ndo_fix_features	= bond_fix_features,
	.ndo_features_check	= passthru_features_check,
	.ndo_get_xmit_slave	= bond_xmit_get_slave,
	.ndo_sk_get_lower_dev	= bond_sk_get_lower_dev,
	.ndo_bpf		= bond_xdp,
	.ndo_xdp_xmit           = bond_xdp_xmit,
	.ndo_xdp_get_xmit_slave = bond_xdp_get_xmit_slave,
	.ndo_hwtstamp_get	= bond_hwtstamp_get,
	.ndo_hwtstamp_set	= bond_hwtstamp_set,
};

static const struct device_type bond_type = {
	.name = "bond",
};

static void bond_destructor(struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);

	if (bond->wq)
		destroy_workqueue(bond->wq);

	free_percpu(bond->rr_tx_counter);
}

void bond_setup(struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);

	spin_lock_init(&bond->mode_lock);
	bond->params = bonding_defaults;

	 
	bond->dev = bond_dev;

	 
	ether_setup(bond_dev);
	bond_dev->max_mtu = ETH_MAX_MTU;
	bond_dev->netdev_ops = &bond_netdev_ops;
	bond_dev->ethtool_ops = &bond_ethtool_ops;

	bond_dev->needs_free_netdev = true;
	bond_dev->priv_destructor = bond_destructor;

	SET_NETDEV_DEVTYPE(bond_dev, &bond_type);

	 
	bond_dev->flags |= IFF_MASTER;
	bond_dev->priv_flags |= IFF_BONDING | IFF_UNICAST_FLT | IFF_NO_QUEUE;
	bond_dev->priv_flags &= ~(IFF_XMIT_DST_RELEASE | IFF_TX_SKB_SHARING);

#ifdef CONFIG_XFRM_OFFLOAD
	 
	bond_dev->xfrmdev_ops = &bond_xfrmdev_ops;
	INIT_LIST_HEAD(&bond->ipsec_list);
	spin_lock_init(&bond->ipsec_lock);
#endif  

	 
	bond_dev->features |= NETIF_F_LLTX;

	 

	 
	bond_dev->features |= NETIF_F_NETNS_LOCAL;

	bond_dev->hw_features = BOND_VLAN_FEATURES |
				NETIF_F_HW_VLAN_CTAG_RX |
				NETIF_F_HW_VLAN_CTAG_FILTER |
				NETIF_F_HW_VLAN_STAG_RX |
				NETIF_F_HW_VLAN_STAG_FILTER;

	bond_dev->hw_features |= NETIF_F_GSO_ENCAP_ALL;
	bond_dev->features |= bond_dev->hw_features;
	bond_dev->features |= NETIF_F_HW_VLAN_CTAG_TX | NETIF_F_HW_VLAN_STAG_TX;
#ifdef CONFIG_XFRM_OFFLOAD
	bond_dev->hw_features |= BOND_XFRM_FEATURES;
	 
	if (BOND_MODE(bond) == BOND_MODE_ACTIVEBACKUP)
		bond_dev->features |= BOND_XFRM_FEATURES;
#endif  

	if (bond_xdp_check(bond))
		bond_dev->xdp_features = NETDEV_XDP_ACT_MASK;
}

 
static void bond_uninit(struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct list_head *iter;
	struct slave *slave;

	bond_netpoll_cleanup(bond_dev);

	 
	bond_for_each_slave(bond, slave, iter)
		__bond_release_one(bond_dev, slave->dev, true, true);
	netdev_info(bond_dev, "Released all slaves\n");

	bond_set_slave_arr(bond, NULL, NULL);

	list_del(&bond->bond_list);

	bond_debug_unregister(bond);
}

 

static int __init bond_check_params(struct bond_params *params)
{
	int arp_validate_value, fail_over_mac_value, primary_reselect_value, i;
	struct bond_opt_value newval;
	const struct bond_opt_value *valptr;
	int arp_all_targets_value = 0;
	u16 ad_actor_sys_prio = 0;
	u16 ad_user_port_key = 0;
	__be32 arp_target[BOND_MAX_ARP_TARGETS] = { 0 };
	int arp_ip_count;
	int bond_mode	= BOND_MODE_ROUNDROBIN;
	int xmit_hashtype = BOND_XMIT_POLICY_LAYER2;
	int lacp_fast = 0;
	int tlb_dynamic_lb;

	 
	if (mode) {
		bond_opt_initstr(&newval, mode);
		valptr = bond_opt_parse(bond_opt_get(BOND_OPT_MODE), &newval);
		if (!valptr) {
			pr_err("Error: Invalid bonding mode \"%s\"\n", mode);
			return -EINVAL;
		}
		bond_mode = valptr->value;
	}

	if (xmit_hash_policy) {
		if (bond_mode == BOND_MODE_ROUNDROBIN ||
		    bond_mode == BOND_MODE_ACTIVEBACKUP ||
		    bond_mode == BOND_MODE_BROADCAST) {
			pr_info("xmit_hash_policy param is irrelevant in mode %s\n",
				bond_mode_name(bond_mode));
		} else {
			bond_opt_initstr(&newval, xmit_hash_policy);
			valptr = bond_opt_parse(bond_opt_get(BOND_OPT_XMIT_HASH),
						&newval);
			if (!valptr) {
				pr_err("Error: Invalid xmit_hash_policy \"%s\"\n",
				       xmit_hash_policy);
				return -EINVAL;
			}
			xmit_hashtype = valptr->value;
		}
	}

	if (lacp_rate) {
		if (bond_mode != BOND_MODE_8023AD) {
			pr_info("lacp_rate param is irrelevant in mode %s\n",
				bond_mode_name(bond_mode));
		} else {
			bond_opt_initstr(&newval, lacp_rate);
			valptr = bond_opt_parse(bond_opt_get(BOND_OPT_LACP_RATE),
						&newval);
			if (!valptr) {
				pr_err("Error: Invalid lacp rate \"%s\"\n",
				       lacp_rate);
				return -EINVAL;
			}
			lacp_fast = valptr->value;
		}
	}

	if (ad_select) {
		bond_opt_initstr(&newval, ad_select);
		valptr = bond_opt_parse(bond_opt_get(BOND_OPT_AD_SELECT),
					&newval);
		if (!valptr) {
			pr_err("Error: Invalid ad_select \"%s\"\n", ad_select);
			return -EINVAL;
		}
		params->ad_select = valptr->value;
		if (bond_mode != BOND_MODE_8023AD)
			pr_warn("ad_select param only affects 802.3ad mode\n");
	} else {
		params->ad_select = BOND_AD_STABLE;
	}

	if (max_bonds < 0) {
		pr_warn("Warning: max_bonds (%d) not in range %d-%d, so it was reset to BOND_DEFAULT_MAX_BONDS (%d)\n",
			max_bonds, 0, INT_MAX, BOND_DEFAULT_MAX_BONDS);
		max_bonds = BOND_DEFAULT_MAX_BONDS;
	}

	if (miimon < 0) {
		pr_warn("Warning: miimon module parameter (%d), not in range 0-%d, so it was reset to 0\n",
			miimon, INT_MAX);
		miimon = 0;
	}

	if (updelay < 0) {
		pr_warn("Warning: updelay module parameter (%d), not in range 0-%d, so it was reset to 0\n",
			updelay, INT_MAX);
		updelay = 0;
	}

	if (downdelay < 0) {
		pr_warn("Warning: downdelay module parameter (%d), not in range 0-%d, so it was reset to 0\n",
			downdelay, INT_MAX);
		downdelay = 0;
	}

	if ((use_carrier != 0) && (use_carrier != 1)) {
		pr_warn("Warning: use_carrier module parameter (%d), not of valid value (0/1), so it was set to 1\n",
			use_carrier);
		use_carrier = 1;
	}

	if (num_peer_notif < 0 || num_peer_notif > 255) {
		pr_warn("Warning: num_grat_arp/num_unsol_na (%d) not in range 0-255 so it was reset to 1\n",
			num_peer_notif);
		num_peer_notif = 1;
	}

	 
	if (!bond_mode_uses_arp(bond_mode)) {
		if (!miimon) {
			pr_warn("Warning: miimon must be specified, otherwise bonding will not detect link failure, speed and duplex which are essential for 802.3ad operation\n");
			pr_warn("Forcing miimon to 100msec\n");
			miimon = BOND_DEFAULT_MIIMON;
		}
	}

	if (tx_queues < 1 || tx_queues > 255) {
		pr_warn("Warning: tx_queues (%d) should be between 1 and 255, resetting to %d\n",
			tx_queues, BOND_DEFAULT_TX_QUEUES);
		tx_queues = BOND_DEFAULT_TX_QUEUES;
	}

	if ((all_slaves_active != 0) && (all_slaves_active != 1)) {
		pr_warn("Warning: all_slaves_active module parameter (%d), not of valid value (0/1), so it was set to 0\n",
			all_slaves_active);
		all_slaves_active = 0;
	}

	if (resend_igmp < 0 || resend_igmp > 255) {
		pr_warn("Warning: resend_igmp (%d) should be between 0 and 255, resetting to %d\n",
			resend_igmp, BOND_DEFAULT_RESEND_IGMP);
		resend_igmp = BOND_DEFAULT_RESEND_IGMP;
	}

	bond_opt_initval(&newval, packets_per_slave);
	if (!bond_opt_parse(bond_opt_get(BOND_OPT_PACKETS_PER_SLAVE), &newval)) {
		pr_warn("Warning: packets_per_slave (%d) should be between 0 and %u resetting to 1\n",
			packets_per_slave, USHRT_MAX);
		packets_per_slave = 1;
	}

	if (bond_mode == BOND_MODE_ALB) {
		pr_notice("In ALB mode you might experience client disconnections upon reconnection of a link if the bonding module updelay parameter (%d msec) is incompatible with the forwarding delay time of the switch\n",
			  updelay);
	}

	if (!miimon) {
		if (updelay || downdelay) {
			 
			pr_warn("Warning: miimon module parameter not set and updelay (%d) or downdelay (%d) module parameter is set; updelay and downdelay have no effect unless miimon is set\n",
				updelay, downdelay);
		}
	} else {
		 
		if (arp_interval) {
			pr_warn("Warning: miimon (%d) and arp_interval (%d) can't be used simultaneously, disabling ARP monitoring\n",
				miimon, arp_interval);
			arp_interval = 0;
		}

		if ((updelay % miimon) != 0) {
			pr_warn("Warning: updelay (%d) is not a multiple of miimon (%d), updelay rounded to %d ms\n",
				updelay, miimon, (updelay / miimon) * miimon);
		}

		updelay /= miimon;

		if ((downdelay % miimon) != 0) {
			pr_warn("Warning: downdelay (%d) is not a multiple of miimon (%d), downdelay rounded to %d ms\n",
				downdelay, miimon,
				(downdelay / miimon) * miimon);
		}

		downdelay /= miimon;
	}

	if (arp_interval < 0) {
		pr_warn("Warning: arp_interval module parameter (%d), not in range 0-%d, so it was reset to 0\n",
			arp_interval, INT_MAX);
		arp_interval = 0;
	}

	for (arp_ip_count = 0, i = 0;
	     (arp_ip_count < BOND_MAX_ARP_TARGETS) && arp_ip_target[i]; i++) {
		__be32 ip;

		 
		if (!in4_pton(arp_ip_target[i], -1, (u8 *)&ip, -1, NULL) ||
		    !bond_is_ip_target_ok(ip)) {
			pr_warn("Warning: bad arp_ip_target module parameter (%s), ARP monitoring will not be performed\n",
				arp_ip_target[i]);
			arp_interval = 0;
		} else {
			if (bond_get_targets_ip(arp_target, ip) == -1)
				arp_target[arp_ip_count++] = ip;
			else
				pr_warn("Warning: duplicate address %pI4 in arp_ip_target, skipping\n",
					&ip);
		}
	}

	if (arp_interval && !arp_ip_count) {
		 
		pr_warn("Warning: arp_interval module parameter (%d) specified without providing an arp_ip_target parameter, arp_interval was reset to 0\n",
			arp_interval);
		arp_interval = 0;
	}

	if (arp_validate) {
		if (!arp_interval) {
			pr_err("arp_validate requires arp_interval\n");
			return -EINVAL;
		}

		bond_opt_initstr(&newval, arp_validate);
		valptr = bond_opt_parse(bond_opt_get(BOND_OPT_ARP_VALIDATE),
					&newval);
		if (!valptr) {
			pr_err("Error: invalid arp_validate \"%s\"\n",
			       arp_validate);
			return -EINVAL;
		}
		arp_validate_value = valptr->value;
	} else {
		arp_validate_value = 0;
	}

	if (arp_all_targets) {
		bond_opt_initstr(&newval, arp_all_targets);
		valptr = bond_opt_parse(bond_opt_get(BOND_OPT_ARP_ALL_TARGETS),
					&newval);
		if (!valptr) {
			pr_err("Error: invalid arp_all_targets_value \"%s\"\n",
			       arp_all_targets);
			arp_all_targets_value = 0;
		} else {
			arp_all_targets_value = valptr->value;
		}
	}

	if (miimon) {
		pr_info("MII link monitoring set to %d ms\n", miimon);
	} else if (arp_interval) {
		valptr = bond_opt_get_val(BOND_OPT_ARP_VALIDATE,
					  arp_validate_value);
		pr_info("ARP monitoring set to %d ms, validate %s, with %d target(s):",
			arp_interval, valptr->string, arp_ip_count);

		for (i = 0; i < arp_ip_count; i++)
			pr_cont(" %s", arp_ip_target[i]);

		pr_cont("\n");

	} else if (max_bonds) {
		 
		pr_debug("Warning: either miimon or arp_interval and arp_ip_target module parameters must be specified, otherwise bonding will not detect link failures! see bonding.txt for details\n");
	}

	if (primary && !bond_mode_uses_primary(bond_mode)) {
		 
		pr_warn("Warning: %s primary device specified but has no effect in %s mode\n",
			primary, bond_mode_name(bond_mode));
		primary = NULL;
	}

	if (primary && primary_reselect) {
		bond_opt_initstr(&newval, primary_reselect);
		valptr = bond_opt_parse(bond_opt_get(BOND_OPT_PRIMARY_RESELECT),
					&newval);
		if (!valptr) {
			pr_err("Error: Invalid primary_reselect \"%s\"\n",
			       primary_reselect);
			return -EINVAL;
		}
		primary_reselect_value = valptr->value;
	} else {
		primary_reselect_value = BOND_PRI_RESELECT_ALWAYS;
	}

	if (fail_over_mac) {
		bond_opt_initstr(&newval, fail_over_mac);
		valptr = bond_opt_parse(bond_opt_get(BOND_OPT_FAIL_OVER_MAC),
					&newval);
		if (!valptr) {
			pr_err("Error: invalid fail_over_mac \"%s\"\n",
			       fail_over_mac);
			return -EINVAL;
		}
		fail_over_mac_value = valptr->value;
		if (bond_mode != BOND_MODE_ACTIVEBACKUP)
			pr_warn("Warning: fail_over_mac only affects active-backup mode\n");
	} else {
		fail_over_mac_value = BOND_FOM_NONE;
	}

	bond_opt_initstr(&newval, "default");
	valptr = bond_opt_parse(
			bond_opt_get(BOND_OPT_AD_ACTOR_SYS_PRIO),
				     &newval);
	if (!valptr) {
		pr_err("Error: No ad_actor_sys_prio default value");
		return -EINVAL;
	}
	ad_actor_sys_prio = valptr->value;

	valptr = bond_opt_parse(bond_opt_get(BOND_OPT_AD_USER_PORT_KEY),
				&newval);
	if (!valptr) {
		pr_err("Error: No ad_user_port_key default value");
		return -EINVAL;
	}
	ad_user_port_key = valptr->value;

	bond_opt_initstr(&newval, "default");
	valptr = bond_opt_parse(bond_opt_get(BOND_OPT_TLB_DYNAMIC_LB), &newval);
	if (!valptr) {
		pr_err("Error: No tlb_dynamic_lb default value");
		return -EINVAL;
	}
	tlb_dynamic_lb = valptr->value;

	if (lp_interval == 0) {
		pr_warn("Warning: ip_interval must be between 1 and %d, so it was reset to %d\n",
			INT_MAX, BOND_ALB_DEFAULT_LP_INTERVAL);
		lp_interval = BOND_ALB_DEFAULT_LP_INTERVAL;
	}

	 
	params->mode = bond_mode;
	params->xmit_policy = xmit_hashtype;
	params->miimon = miimon;
	params->num_peer_notif = num_peer_notif;
	params->arp_interval = arp_interval;
	params->arp_validate = arp_validate_value;
	params->arp_all_targets = arp_all_targets_value;
	params->missed_max = 2;
	params->updelay = updelay;
	params->downdelay = downdelay;
	params->peer_notif_delay = 0;
	params->use_carrier = use_carrier;
	params->lacp_active = 1;
	params->lacp_fast = lacp_fast;
	params->primary[0] = 0;
	params->primary_reselect = primary_reselect_value;
	params->fail_over_mac = fail_over_mac_value;
	params->tx_queues = tx_queues;
	params->all_slaves_active = all_slaves_active;
	params->resend_igmp = resend_igmp;
	params->min_links = min_links;
	params->lp_interval = lp_interval;
	params->packets_per_slave = packets_per_slave;
	params->tlb_dynamic_lb = tlb_dynamic_lb;
	params->ad_actor_sys_prio = ad_actor_sys_prio;
	eth_zero_addr(params->ad_actor_system);
	params->ad_user_port_key = ad_user_port_key;
	if (packets_per_slave > 0) {
		params->reciprocal_packets_per_slave =
			reciprocal_value(packets_per_slave);
	} else {
		 
		params->reciprocal_packets_per_slave =
			(struct reciprocal_value) { 0 };
	}

	if (primary)
		strscpy_pad(params->primary, primary, sizeof(params->primary));

	memcpy(params->arp_targets, arp_target, sizeof(arp_target));
#if IS_ENABLED(CONFIG_IPV6)
	memset(params->ns_targets, 0, sizeof(struct in6_addr) * BOND_MAX_NS_TARGETS);
#endif

	return 0;
}

 
static int bond_init(struct net_device *bond_dev)
{
	struct bonding *bond = netdev_priv(bond_dev);
	struct bond_net *bn = net_generic(dev_net(bond_dev), bond_net_id);

	netdev_dbg(bond_dev, "Begin bond_init\n");

	bond->wq = alloc_ordered_workqueue(bond_dev->name, WQ_MEM_RECLAIM);
	if (!bond->wq)
		return -ENOMEM;

	bond->notifier_ctx = false;

	spin_lock_init(&bond->stats_lock);
	netdev_lockdep_set_classes(bond_dev);

	list_add_tail(&bond->bond_list, &bn->dev_list);

	bond_prepare_sysfs_group(bond);

	bond_debug_register(bond);

	 
	if (is_zero_ether_addr(bond_dev->dev_addr) &&
	    bond_dev->addr_assign_type == NET_ADDR_PERM)
		eth_hw_addr_random(bond_dev);

	return 0;
}

unsigned int bond_get_num_tx_queues(void)
{
	return tx_queues;
}

 
int bond_create(struct net *net, const char *name)
{
	struct net_device *bond_dev;
	struct bonding *bond;
	int res = -ENOMEM;

	rtnl_lock();

	bond_dev = alloc_netdev_mq(sizeof(struct bonding),
				   name ? name : "bond%d", NET_NAME_UNKNOWN,
				   bond_setup, tx_queues);
	if (!bond_dev)
		goto out;

	bond = netdev_priv(bond_dev);
	dev_net_set(bond_dev, net);
	bond_dev->rtnl_link_ops = &bond_link_ops;

	res = register_netdevice(bond_dev);
	if (res < 0) {
		free_netdev(bond_dev);
		goto out;
	}

	netif_carrier_off(bond_dev);

	bond_work_init_all(bond);

out:
	rtnl_unlock();
	return res;
}

static int __net_init bond_net_init(struct net *net)
{
	struct bond_net *bn = net_generic(net, bond_net_id);

	bn->net = net;
	INIT_LIST_HEAD(&bn->dev_list);

	bond_create_proc_dir(bn);
	bond_create_sysfs(bn);

	return 0;
}

static void __net_exit bond_net_exit_batch(struct list_head *net_list)
{
	struct bond_net *bn;
	struct net *net;
	LIST_HEAD(list);

	list_for_each_entry(net, net_list, exit_list) {
		bn = net_generic(net, bond_net_id);
		bond_destroy_sysfs(bn);
	}

	 
	rtnl_lock();
	list_for_each_entry(net, net_list, exit_list) {
		struct bonding *bond, *tmp_bond;

		bn = net_generic(net, bond_net_id);
		list_for_each_entry_safe(bond, tmp_bond, &bn->dev_list, bond_list)
			unregister_netdevice_queue(bond->dev, &list);
	}
	unregister_netdevice_many(&list);
	rtnl_unlock();

	list_for_each_entry(net, net_list, exit_list) {
		bn = net_generic(net, bond_net_id);
		bond_destroy_proc_dir(bn);
	}
}

static struct pernet_operations bond_net_ops = {
	.init = bond_net_init,
	.exit_batch = bond_net_exit_batch,
	.id   = &bond_net_id,
	.size = sizeof(struct bond_net),
};

static int __init bonding_init(void)
{
	int i;
	int res;

	res = bond_check_params(&bonding_defaults);
	if (res)
		goto out;

	res = register_pernet_subsys(&bond_net_ops);
	if (res)
		goto out;

	res = bond_netlink_init();
	if (res)
		goto err_link;

	bond_create_debugfs();

	for (i = 0; i < max_bonds; i++) {
		res = bond_create(&init_net, NULL);
		if (res)
			goto err;
	}

	skb_flow_dissector_init(&flow_keys_bonding,
				flow_keys_bonding_keys,
				ARRAY_SIZE(flow_keys_bonding_keys));

	register_netdevice_notifier(&bond_netdev_notifier);
out:
	return res;
err:
	bond_destroy_debugfs();
	bond_netlink_fini();
err_link:
	unregister_pernet_subsys(&bond_net_ops);
	goto out;

}

static void __exit bonding_exit(void)
{
	unregister_netdevice_notifier(&bond_netdev_notifier);

	bond_destroy_debugfs();

	bond_netlink_fini();
	unregister_pernet_subsys(&bond_net_ops);

#ifdef CONFIG_NET_POLL_CONTROLLER
	 
	WARN_ON(atomic_read(&netpoll_block_tx));
#endif
}

module_init(bonding_init);
module_exit(bonding_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_AUTHOR("Thomas Davis, tadavis@lbl.gov and many others");
