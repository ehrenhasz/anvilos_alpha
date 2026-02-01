
#include <linux/kmod.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/net_tstamp.h>
#include <linux/phylib_stubs.h>
#include <linux/wireless.h>
#include <linux/if_bridge.h>
#include <net/dsa_stubs.h>
#include <net/wext.h>

#include "dev.h"

 

 

static int dev_ifname(struct net *net, struct ifreq *ifr)
{
	ifr->ifr_name[IFNAMSIZ-1] = 0;
	return netdev_get_name(net, ifr->ifr_name, ifr->ifr_ifindex);
}

 
int dev_ifconf(struct net *net, struct ifconf __user *uifc)
{
	struct net_device *dev;
	void __user *pos;
	size_t size;
	int len, total = 0, done;

	 
	if (in_compat_syscall()) {
		struct compat_ifconf ifc32;

		if (copy_from_user(&ifc32, uifc, sizeof(struct compat_ifconf)))
			return -EFAULT;

		pos = compat_ptr(ifc32.ifcbuf);
		len = ifc32.ifc_len;
		size = sizeof(struct compat_ifreq);
	} else {
		struct ifconf ifc;

		if (copy_from_user(&ifc, uifc, sizeof(struct ifconf)))
			return -EFAULT;

		pos = ifc.ifc_buf;
		len = ifc.ifc_len;
		size = sizeof(struct ifreq);
	}

	 
	rtnl_lock();
	for_each_netdev(net, dev) {
		if (!pos)
			done = inet_gifconf(dev, NULL, 0, size);
		else
			done = inet_gifconf(dev, pos + total,
					    len - total, size);
		if (done < 0) {
			rtnl_unlock();
			return -EFAULT;
		}
		total += done;
	}
	rtnl_unlock();

	return put_user(total, &uifc->ifc_len);
}

static int dev_getifmap(struct net_device *dev, struct ifreq *ifr)
{
	struct ifmap *ifmap = &ifr->ifr_map;

	if (in_compat_syscall()) {
		struct compat_ifmap *cifmap = (struct compat_ifmap *)ifmap;

		cifmap->mem_start = dev->mem_start;
		cifmap->mem_end   = dev->mem_end;
		cifmap->base_addr = dev->base_addr;
		cifmap->irq       = dev->irq;
		cifmap->dma       = dev->dma;
		cifmap->port      = dev->if_port;

		return 0;
	}

	ifmap->mem_start  = dev->mem_start;
	ifmap->mem_end    = dev->mem_end;
	ifmap->base_addr  = dev->base_addr;
	ifmap->irq        = dev->irq;
	ifmap->dma        = dev->dma;
	ifmap->port       = dev->if_port;

	return 0;
}

static int dev_setifmap(struct net_device *dev, struct ifreq *ifr)
{
	struct compat_ifmap *cifmap = (struct compat_ifmap *)&ifr->ifr_map;

	if (!dev->netdev_ops->ndo_set_config)
		return -EOPNOTSUPP;

	if (in_compat_syscall()) {
		struct ifmap ifmap = {
			.mem_start  = cifmap->mem_start,
			.mem_end    = cifmap->mem_end,
			.base_addr  = cifmap->base_addr,
			.irq        = cifmap->irq,
			.dma        = cifmap->dma,
			.port       = cifmap->port,
		};

		return dev->netdev_ops->ndo_set_config(dev, &ifmap);
	}

	return dev->netdev_ops->ndo_set_config(dev, &ifr->ifr_map);
}

 
static int dev_ifsioc_locked(struct net *net, struct ifreq *ifr, unsigned int cmd)
{
	int err;
	struct net_device *dev = dev_get_by_name_rcu(net, ifr->ifr_name);

	if (!dev)
		return -ENODEV;

	switch (cmd) {
	case SIOCGIFFLAGS:	 
		ifr->ifr_flags = (short) dev_get_flags(dev);
		return 0;

	case SIOCGIFMETRIC:	 
		ifr->ifr_metric = 0;
		return 0;

	case SIOCGIFMTU:	 
		ifr->ifr_mtu = dev->mtu;
		return 0;

	case SIOCGIFSLAVE:
		err = -EINVAL;
		break;

	case SIOCGIFMAP:
		return dev_getifmap(dev, ifr);

	case SIOCGIFINDEX:
		ifr->ifr_ifindex = dev->ifindex;
		return 0;

	case SIOCGIFTXQLEN:
		ifr->ifr_qlen = dev->tx_queue_len;
		return 0;

	default:
		 
		WARN_ON(1);
		err = -ENOTTY;
		break;

	}
	return err;
}

static int net_hwtstamp_validate(const struct kernel_hwtstamp_config *cfg)
{
	enum hwtstamp_tx_types tx_type;
	enum hwtstamp_rx_filters rx_filter;
	int tx_type_valid = 0;
	int rx_filter_valid = 0;

	if (cfg->flags & ~HWTSTAMP_FLAG_MASK)
		return -EINVAL;

	tx_type = cfg->tx_type;
	rx_filter = cfg->rx_filter;

	switch (tx_type) {
	case HWTSTAMP_TX_OFF:
	case HWTSTAMP_TX_ON:
	case HWTSTAMP_TX_ONESTEP_SYNC:
	case HWTSTAMP_TX_ONESTEP_P2P:
		tx_type_valid = 1;
		break;
	case __HWTSTAMP_TX_CNT:
		 
		break;
	}

	switch (rx_filter) {
	case HWTSTAMP_FILTER_NONE:
	case HWTSTAMP_FILTER_ALL:
	case HWTSTAMP_FILTER_SOME:
	case HWTSTAMP_FILTER_PTP_V1_L4_EVENT:
	case HWTSTAMP_FILTER_PTP_V1_L4_SYNC:
	case HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V2_L2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_L2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_L2_DELAY_REQ:
	case HWTSTAMP_FILTER_PTP_V2_EVENT:
	case HWTSTAMP_FILTER_PTP_V2_SYNC:
	case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
	case HWTSTAMP_FILTER_NTP_ALL:
		rx_filter_valid = 1;
		break;
	case __HWTSTAMP_FILTER_CNT:
		 
		break;
	}

	if (!tx_type_valid || !rx_filter_valid)
		return -ERANGE;

	return 0;
}

static int dev_eth_ioctl(struct net_device *dev,
			 struct ifreq *ifr, unsigned int cmd)
{
	const struct net_device_ops *ops = dev->netdev_ops;

	if (!ops->ndo_eth_ioctl)
		return -EOPNOTSUPP;

	if (!netif_device_present(dev))
		return -ENODEV;

	return ops->ndo_eth_ioctl(dev, ifr, cmd);
}

 
static int dev_get_hwtstamp_phylib(struct net_device *dev,
				   struct kernel_hwtstamp_config *cfg)
{
	if (phy_has_hwtstamp(dev->phydev))
		return phy_hwtstamp_get(dev->phydev, cfg);

	return dev->netdev_ops->ndo_hwtstamp_get(dev, cfg);
}

static int dev_get_hwtstamp(struct net_device *dev, struct ifreq *ifr)
{
	const struct net_device_ops *ops = dev->netdev_ops;
	struct kernel_hwtstamp_config kernel_cfg = {};
	struct hwtstamp_config cfg;
	int err;

	if (!ops->ndo_hwtstamp_get)
		return dev_eth_ioctl(dev, ifr, SIOCGHWTSTAMP);  

	if (!netif_device_present(dev))
		return -ENODEV;

	kernel_cfg.ifr = ifr;
	err = dev_get_hwtstamp_phylib(dev, &kernel_cfg);
	if (err)
		return err;

	 
	if (!kernel_cfg.copied_to_user) {
		hwtstamp_config_from_kernel(&cfg, &kernel_cfg);

		if (copy_to_user(ifr->ifr_data, &cfg, sizeof(cfg)))
			return -EFAULT;
	}

	return 0;
}

 
static int dev_set_hwtstamp_phylib(struct net_device *dev,
				   struct kernel_hwtstamp_config *cfg,
				   struct netlink_ext_ack *extack)
{
	const struct net_device_ops *ops = dev->netdev_ops;
	bool phy_ts = phy_has_hwtstamp(dev->phydev);
	struct kernel_hwtstamp_config old_cfg = {};
	bool changed = false;
	int err;

	cfg->source = phy_ts ? HWTSTAMP_SOURCE_PHYLIB : HWTSTAMP_SOURCE_NETDEV;

	if (phy_ts && (dev->priv_flags & IFF_SEE_ALL_HWTSTAMP_REQUESTS)) {
		err = ops->ndo_hwtstamp_get(dev, &old_cfg);
		if (err)
			return err;
	}

	if (!phy_ts || (dev->priv_flags & IFF_SEE_ALL_HWTSTAMP_REQUESTS)) {
		err = ops->ndo_hwtstamp_set(dev, cfg, extack);
		if (err) {
			if (extack->_msg)
				netdev_err(dev, "%s\n", extack->_msg);
			return err;
		}
	}

	if (phy_ts && (dev->priv_flags & IFF_SEE_ALL_HWTSTAMP_REQUESTS))
		changed = kernel_hwtstamp_config_changed(&old_cfg, cfg);

	if (phy_ts) {
		err = phy_hwtstamp_set(dev->phydev, cfg, extack);
		if (err) {
			if (changed)
				ops->ndo_hwtstamp_set(dev, &old_cfg, NULL);
			return err;
		}
	}

	return 0;
}

static int dev_set_hwtstamp(struct net_device *dev, struct ifreq *ifr)
{
	const struct net_device_ops *ops = dev->netdev_ops;
	struct kernel_hwtstamp_config kernel_cfg = {};
	struct netlink_ext_ack extack = {};
	struct hwtstamp_config cfg;
	int err;

	if (copy_from_user(&cfg, ifr->ifr_data, sizeof(cfg)))
		return -EFAULT;

	hwtstamp_config_to_kernel(&kernel_cfg, &cfg);
	kernel_cfg.ifr = ifr;

	err = net_hwtstamp_validate(&kernel_cfg);
	if (err)
		return err;

	err = dsa_master_hwtstamp_validate(dev, &kernel_cfg, &extack);
	if (err) {
		if (extack._msg)
			netdev_err(dev, "%s\n", extack._msg);
		return err;
	}

	if (!ops->ndo_hwtstamp_set)
		return dev_eth_ioctl(dev, ifr, SIOCSHWTSTAMP);  

	if (!netif_device_present(dev))
		return -ENODEV;

	err = dev_set_hwtstamp_phylib(dev, &kernel_cfg, &extack);
	if (err)
		return err;

	 
	if (!kernel_cfg.copied_to_user) {
		hwtstamp_config_from_kernel(&cfg, &kernel_cfg);

		if (copy_to_user(ifr->ifr_data, &cfg, sizeof(cfg)))
			return -EFAULT;
	}

	return 0;
}

static int generic_hwtstamp_ioctl_lower(struct net_device *dev, int cmd,
					struct kernel_hwtstamp_config *kernel_cfg)
{
	struct ifreq ifrr;
	int err;

	strscpy_pad(ifrr.ifr_name, dev->name, IFNAMSIZ);
	ifrr.ifr_ifru = kernel_cfg->ifr->ifr_ifru;

	err = dev_eth_ioctl(dev, &ifrr, cmd);
	if (err)
		return err;

	kernel_cfg->ifr->ifr_ifru = ifrr.ifr_ifru;
	kernel_cfg->copied_to_user = true;

	return 0;
}

int generic_hwtstamp_get_lower(struct net_device *dev,
			       struct kernel_hwtstamp_config *kernel_cfg)
{
	const struct net_device_ops *ops = dev->netdev_ops;

	if (!netif_device_present(dev))
		return -ENODEV;

	if (ops->ndo_hwtstamp_get)
		return dev_get_hwtstamp_phylib(dev, kernel_cfg);

	 
	return generic_hwtstamp_ioctl_lower(dev, SIOCGHWTSTAMP, kernel_cfg);
}
EXPORT_SYMBOL(generic_hwtstamp_get_lower);

int generic_hwtstamp_set_lower(struct net_device *dev,
			       struct kernel_hwtstamp_config *kernel_cfg,
			       struct netlink_ext_ack *extack)
{
	const struct net_device_ops *ops = dev->netdev_ops;

	if (!netif_device_present(dev))
		return -ENODEV;

	if (ops->ndo_hwtstamp_set)
		return dev_set_hwtstamp_phylib(dev, kernel_cfg, extack);

	 
	return generic_hwtstamp_ioctl_lower(dev, SIOCSHWTSTAMP, kernel_cfg);
}
EXPORT_SYMBOL(generic_hwtstamp_set_lower);

static int dev_siocbond(struct net_device *dev,
			struct ifreq *ifr, unsigned int cmd)
{
	const struct net_device_ops *ops = dev->netdev_ops;

	if (ops->ndo_siocbond) {
		if (netif_device_present(dev))
			return ops->ndo_siocbond(dev, ifr, cmd);
		else
			return -ENODEV;
	}

	return -EOPNOTSUPP;
}

static int dev_siocdevprivate(struct net_device *dev, struct ifreq *ifr,
			      void __user *data, unsigned int cmd)
{
	const struct net_device_ops *ops = dev->netdev_ops;

	if (ops->ndo_siocdevprivate) {
		if (netif_device_present(dev))
			return ops->ndo_siocdevprivate(dev, ifr, data, cmd);
		else
			return -ENODEV;
	}

	return -EOPNOTSUPP;
}

static int dev_siocwandev(struct net_device *dev, struct if_settings *ifs)
{
	const struct net_device_ops *ops = dev->netdev_ops;

	if (ops->ndo_siocwandev) {
		if (netif_device_present(dev))
			return ops->ndo_siocwandev(dev, ifs);
		else
			return -ENODEV;
	}

	return -EOPNOTSUPP;
}

 
static int dev_ifsioc(struct net *net, struct ifreq *ifr, void __user *data,
		      unsigned int cmd)
{
	int err;
	struct net_device *dev = __dev_get_by_name(net, ifr->ifr_name);
	const struct net_device_ops *ops;
	netdevice_tracker dev_tracker;

	if (!dev)
		return -ENODEV;

	ops = dev->netdev_ops;

	switch (cmd) {
	case SIOCSIFFLAGS:	 
		return dev_change_flags(dev, ifr->ifr_flags, NULL);

	case SIOCSIFMETRIC:	 
		return -EOPNOTSUPP;

	case SIOCSIFMTU:	 
		return dev_set_mtu(dev, ifr->ifr_mtu);

	case SIOCSIFHWADDR:
		if (dev->addr_len > sizeof(struct sockaddr))
			return -EINVAL;
		return dev_set_mac_address_user(dev, &ifr->ifr_hwaddr, NULL);

	case SIOCSIFHWBROADCAST:
		if (ifr->ifr_hwaddr.sa_family != dev->type)
			return -EINVAL;
		memcpy(dev->broadcast, ifr->ifr_hwaddr.sa_data,
		       min(sizeof(ifr->ifr_hwaddr.sa_data_min),
			   (size_t)dev->addr_len));
		call_netdevice_notifiers(NETDEV_CHANGEADDR, dev);
		return 0;

	case SIOCSIFMAP:
		return dev_setifmap(dev, ifr);

	case SIOCADDMULTI:
		if (!ops->ndo_set_rx_mode ||
		    ifr->ifr_hwaddr.sa_family != AF_UNSPEC)
			return -EINVAL;
		if (!netif_device_present(dev))
			return -ENODEV;
		return dev_mc_add_global(dev, ifr->ifr_hwaddr.sa_data);

	case SIOCDELMULTI:
		if (!ops->ndo_set_rx_mode ||
		    ifr->ifr_hwaddr.sa_family != AF_UNSPEC)
			return -EINVAL;
		if (!netif_device_present(dev))
			return -ENODEV;
		return dev_mc_del_global(dev, ifr->ifr_hwaddr.sa_data);

	case SIOCSIFTXQLEN:
		if (ifr->ifr_qlen < 0)
			return -EINVAL;
		return dev_change_tx_queue_len(dev, ifr->ifr_qlen);

	case SIOCSIFNAME:
		ifr->ifr_newname[IFNAMSIZ-1] = '\0';
		return dev_change_name(dev, ifr->ifr_newname);

	case SIOCWANDEV:
		return dev_siocwandev(dev, &ifr->ifr_settings);

	case SIOCBRADDIF:
	case SIOCBRDELIF:
		if (!netif_device_present(dev))
			return -ENODEV;
		if (!netif_is_bridge_master(dev))
			return -EOPNOTSUPP;
		netdev_hold(dev, &dev_tracker, GFP_KERNEL);
		rtnl_unlock();
		err = br_ioctl_call(net, netdev_priv(dev), cmd, ifr, NULL);
		netdev_put(dev, &dev_tracker);
		rtnl_lock();
		return err;

	case SIOCDEVPRIVATE ... SIOCDEVPRIVATE + 15:
		return dev_siocdevprivate(dev, ifr, data, cmd);

	case SIOCSHWTSTAMP:
		return dev_set_hwtstamp(dev, ifr);

	case SIOCGHWTSTAMP:
		return dev_get_hwtstamp(dev, ifr);

	case SIOCGMIIPHY:
	case SIOCGMIIREG:
	case SIOCSMIIREG:
		return dev_eth_ioctl(dev, ifr, cmd);

	case SIOCBONDENSLAVE:
	case SIOCBONDRELEASE:
	case SIOCBONDSETHWADDR:
	case SIOCBONDSLAVEINFOQUERY:
	case SIOCBONDINFOQUERY:
	case SIOCBONDCHANGEACTIVE:
		return dev_siocbond(dev, ifr, cmd);

	 
	default:
		err = -EINVAL;
	}
	return err;
}

 

void dev_load(struct net *net, const char *name)
{
	struct net_device *dev;
	int no_module;

	rcu_read_lock();
	dev = dev_get_by_name_rcu(net, name);
	rcu_read_unlock();

	no_module = !dev;
	if (no_module && capable(CAP_NET_ADMIN))
		no_module = request_module("netdev-%s", name);
	if (no_module && capable(CAP_SYS_MODULE))
		request_module("%s", name);
}
EXPORT_SYMBOL(dev_load);

 

 

int dev_ioctl(struct net *net, unsigned int cmd, struct ifreq *ifr,
	      void __user *data, bool *need_copyout)
{
	int ret;
	char *colon;

	if (need_copyout)
		*need_copyout = true;
	if (cmd == SIOCGIFNAME)
		return dev_ifname(net, ifr);

	ifr->ifr_name[IFNAMSIZ-1] = 0;

	colon = strchr(ifr->ifr_name, ':');
	if (colon)
		*colon = 0;

	 

	switch (cmd) {
	case SIOCGIFHWADDR:
		dev_load(net, ifr->ifr_name);
		ret = dev_get_mac_address(&ifr->ifr_hwaddr, net, ifr->ifr_name);
		if (colon)
			*colon = ':';
		return ret;
	 
	case SIOCGIFFLAGS:
	case SIOCGIFMETRIC:
	case SIOCGIFMTU:
	case SIOCGIFSLAVE:
	case SIOCGIFMAP:
	case SIOCGIFINDEX:
	case SIOCGIFTXQLEN:
		dev_load(net, ifr->ifr_name);
		rcu_read_lock();
		ret = dev_ifsioc_locked(net, ifr, cmd);
		rcu_read_unlock();
		if (colon)
			*colon = ':';
		return ret;

	case SIOCETHTOOL:
		dev_load(net, ifr->ifr_name);
		ret = dev_ethtool(net, ifr, data);
		if (colon)
			*colon = ':';
		return ret;

	 
	case SIOCGMIIPHY:
	case SIOCGMIIREG:
	case SIOCSIFNAME:
		dev_load(net, ifr->ifr_name);
		if (!ns_capable(net->user_ns, CAP_NET_ADMIN))
			return -EPERM;
		rtnl_lock();
		ret = dev_ifsioc(net, ifr, data, cmd);
		rtnl_unlock();
		if (colon)
			*colon = ':';
		return ret;

	 
	case SIOCSIFMAP:
	case SIOCSIFTXQLEN:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;
		fallthrough;
	 
	case SIOCSIFFLAGS:
	case SIOCSIFMETRIC:
	case SIOCSIFMTU:
	case SIOCSIFHWADDR:
	case SIOCSIFSLAVE:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
	case SIOCSIFHWBROADCAST:
	case SIOCSMIIREG:
	case SIOCBONDENSLAVE:
	case SIOCBONDRELEASE:
	case SIOCBONDSETHWADDR:
	case SIOCBONDCHANGEACTIVE:
	case SIOCBRADDIF:
	case SIOCBRDELIF:
	case SIOCSHWTSTAMP:
		if (!ns_capable(net->user_ns, CAP_NET_ADMIN))
			return -EPERM;
		fallthrough;
	case SIOCBONDSLAVEINFOQUERY:
	case SIOCBONDINFOQUERY:
		dev_load(net, ifr->ifr_name);
		rtnl_lock();
		ret = dev_ifsioc(net, ifr, data, cmd);
		rtnl_unlock();
		if (need_copyout)
			*need_copyout = false;
		return ret;

	case SIOCGIFMEM:
		 
	case SIOCSIFMEM:
		 
	case SIOCSIFLINK:
		return -ENOTTY;

	 
	default:
		if (cmd == SIOCWANDEV ||
		    cmd == SIOCGHWTSTAMP ||
		    (cmd >= SIOCDEVPRIVATE &&
		     cmd <= SIOCDEVPRIVATE + 15)) {
			dev_load(net, ifr->ifr_name);
			rtnl_lock();
			ret = dev_ifsioc(net, ifr, data, cmd);
			rtnl_unlock();
			return ret;
		}
		return -ENOTTY;
	}
}
