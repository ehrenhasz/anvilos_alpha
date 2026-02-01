
 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/kmod.h>
#include <linux/if_arp.h>
#include <linux/wireless.h>
#include <linux/sockios.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/byteorder/generic.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>
#include <asm/byteorder.h>

#ifdef SIOCETHTOOL
#include <linux/ethtool.h>
#endif

#include <net/iw_handler.h>
#include <net/net_namespace.h>
#include <net/cfg80211.h>

#include "p80211types.h"
#include "p80211hdr.h"
#include "p80211conv.h"
#include "p80211mgmt.h"
#include "p80211msg.h"
#include "p80211netdev.h"
#include "p80211ioctl.h"
#include "p80211req.h"
#include "p80211metastruct.h"
#include "p80211metadef.h"

#include "cfg80211.c"

 
static int p80211knetdev_init(struct net_device *netdev);
static int p80211knetdev_open(struct net_device *netdev);
static int p80211knetdev_stop(struct net_device *netdev);
static netdev_tx_t p80211knetdev_hard_start_xmit(struct sk_buff *skb,
						 struct net_device *netdev);
static void p80211knetdev_set_multicast_list(struct net_device *dev);
static int p80211knetdev_siocdevprivate(struct net_device *dev, struct ifreq *ifr,
					void __user *data, int cmd);
static int p80211knetdev_set_mac_address(struct net_device *dev, void *addr);
static void p80211knetdev_tx_timeout(struct net_device *netdev, unsigned int txqueue);
static int p80211_rx_typedrop(struct wlandevice *wlandev, u16 fc);

int wlan_watchdog = 5000;
module_param(wlan_watchdog, int, 0644);
MODULE_PARM_DESC(wlan_watchdog, "transmit timeout in milliseconds");

int wlan_wext_write = 1;
module_param(wlan_wext_write, int, 0644);
MODULE_PARM_DESC(wlan_wext_write, "enable write wireless extensions");

 
static int p80211knetdev_init(struct net_device *netdev)
{
	 
	 
	 
	 
	return 0;
}

 
static int p80211knetdev_open(struct net_device *netdev)
{
	int result = 0;		 
	struct wlandevice *wlandev = netdev->ml_priv;

	 
	if (wlandev->msdstate != WLAN_MSD_RUNNING)
		return -ENODEV;

	 
	if (wlandev->open) {
		result = wlandev->open(wlandev);
		if (result == 0) {
			netif_start_queue(wlandev->netdev);
			wlandev->state = WLAN_DEVICE_OPEN;
		}
	} else {
		result = -EAGAIN;
	}

	return result;
}

 
static int p80211knetdev_stop(struct net_device *netdev)
{
	int result = 0;
	struct wlandevice *wlandev = netdev->ml_priv;

	if (wlandev->close)
		result = wlandev->close(wlandev);

	netif_stop_queue(wlandev->netdev);
	wlandev->state = WLAN_DEVICE_CLOSED;

	return result;
}

 
void p80211netdev_rx(struct wlandevice *wlandev, struct sk_buff *skb)
{
	 
	skb_queue_tail(&wlandev->nsd_rxq, skb);
	tasklet_schedule(&wlandev->rx_bh);
}

#define CONV_TO_ETHER_SKIPPED	0x01
#define CONV_TO_ETHER_FAILED	0x02

 
static int p80211_convert_to_ether(struct wlandevice *wlandev,
				   struct sk_buff *skb)
{
	struct p80211_hdr *hdr;

	hdr = (struct p80211_hdr *)skb->data;
	if (p80211_rx_typedrop(wlandev, le16_to_cpu(hdr->frame_control)))
		return CONV_TO_ETHER_SKIPPED;

	 
	if (wlandev->netdev->flags & IFF_ALLMULTI) {
		if (!ether_addr_equal_unaligned(wlandev->netdev->dev_addr,
						hdr->address1)) {
			if (!is_multicast_ether_addr(hdr->address1))
				return CONV_TO_ETHER_SKIPPED;
		}
	}

	if (skb_p80211_to_ether(wlandev, wlandev->ethconv, skb) == 0) {
		wlandev->netdev->stats.rx_packets++;
		wlandev->netdev->stats.rx_bytes += skb->len;
		netif_rx(skb);
		return 0;
	}

	netdev_dbg(wlandev->netdev, "%s failed.\n", __func__);
	return CONV_TO_ETHER_FAILED;
}

 
static void p80211netdev_rx_bh(struct tasklet_struct *t)
{
	struct wlandevice *wlandev = from_tasklet(wlandev, t, rx_bh);
	struct sk_buff *skb = NULL;
	struct net_device *dev = wlandev->netdev;

	 
	while ((skb = skb_dequeue(&wlandev->nsd_rxq))) {
		if (wlandev->state == WLAN_DEVICE_OPEN) {
			if (dev->type != ARPHRD_ETHER) {
				 
				 

				 
				skb->dev = dev;
				skb_reset_mac_header(skb);
				skb->ip_summed = CHECKSUM_NONE;
				skb->pkt_type = PACKET_OTHERHOST;
				skb->protocol = htons(ETH_P_80211_RAW);

				dev->stats.rx_packets++;
				dev->stats.rx_bytes += skb->len;
				netif_rx(skb);
				continue;
			} else {
				if (!p80211_convert_to_ether(wlandev, skb))
					continue;
			}
		}
		dev_kfree_skb(skb);
	}
}

 
static netdev_tx_t p80211knetdev_hard_start_xmit(struct sk_buff *skb,
						 struct net_device *netdev)
{
	int result = 0;
	int txresult;
	struct wlandevice *wlandev = netdev->ml_priv;
	struct p80211_hdr p80211_hdr;
	struct p80211_metawep p80211_wep;

	p80211_wep.data = NULL;

	if (!skb)
		return NETDEV_TX_OK;

	if (wlandev->state != WLAN_DEVICE_OPEN) {
		result = 1;
		goto failed;
	}

	memset(&p80211_hdr, 0, sizeof(p80211_hdr));
	memset(&p80211_wep, 0, sizeof(p80211_wep));

	if (netif_queue_stopped(netdev)) {
		netdev_dbg(netdev, "called when queue stopped.\n");
		result = 1;
		goto failed;
	}

	netif_stop_queue(netdev);

	 
	switch (wlandev->macmode) {
	case WLAN_MACMODE_IBSS_STA:
	case WLAN_MACMODE_ESS_STA:
	case WLAN_MACMODE_ESS_AP:
		break;
	default:
		 
		if (be16_to_cpu(skb->protocol) != ETH_P_80211_RAW) {
			netif_start_queue(wlandev->netdev);
			netdev_notice(netdev, "Tx attempt prior to association, frame dropped.\n");
			netdev->stats.tx_dropped++;
			result = 0;
			goto failed;
		}
		break;
	}

	 
	if (be16_to_cpu(skb->protocol) == ETH_P_80211_RAW) {
		if (!capable(CAP_NET_ADMIN)) {
			result = 1;
			goto failed;
		}
		 
		memcpy(&p80211_hdr, skb->data, sizeof(p80211_hdr));
		skb_pull(skb, sizeof(p80211_hdr));
	} else {
		if (skb_ether_to_p80211
		    (wlandev, wlandev->ethconv, skb, &p80211_hdr,
		     &p80211_wep) != 0) {
			 
			netdev_dbg(netdev, "ether_to_80211(%d) failed.\n",
				   wlandev->ethconv);
			result = 1;
			goto failed;
		}
	}
	if (!wlandev->txframe) {
		result = 1;
		goto failed;
	}

	netif_trans_update(netdev);

	netdev->stats.tx_packets++;
	 
	netdev->stats.tx_bytes += skb->len;

	txresult = wlandev->txframe(wlandev, skb, &p80211_hdr, &p80211_wep);

	if (txresult == 0) {
		 
		 
		netif_wake_queue(wlandev->netdev);
		result = NETDEV_TX_OK;
	} else if (txresult == 1) {
		 
		netdev_dbg(netdev, "txframe success, no more bufs\n");
		 
		 
		result = NETDEV_TX_OK;
	} else if (txresult == 2) {
		 
		netdev_dbg(netdev, "txframe returned alloc_fail\n");
		result = NETDEV_TX_BUSY;
	} else {
		 
		netdev_dbg(netdev, "txframe returned full or busy\n");
		result = NETDEV_TX_BUSY;
	}

failed:
	 
	if ((p80211_wep.data) && (p80211_wep.data != skb->data))
		kfree_sensitive(p80211_wep.data);

	 
	if (!result)
		dev_kfree_skb(skb);

	return result;
}

 
static void p80211knetdev_set_multicast_list(struct net_device *dev)
{
	struct wlandevice *wlandev = dev->ml_priv;

	 

	if (wlandev->set_multicast_list)
		wlandev->set_multicast_list(wlandev, dev);
}

 
static int p80211knetdev_siocdevprivate(struct net_device *dev,
					struct ifreq *ifr,
					void __user *data, int cmd)
{
	int result = 0;
	struct p80211ioctl_req *req = (struct p80211ioctl_req *)ifr;
	struct wlandevice *wlandev = dev->ml_priv;
	u8 *msgbuf;

	netdev_dbg(dev, "rx'd ioctl, cmd=%d, len=%d\n", cmd, req->len);

	if (in_compat_syscall())
		return -EOPNOTSUPP;

	 
	if (req->magic != P80211_IOCTL_MAGIC) {
		result = -EINVAL;
		goto bail;
	}

	if (cmd == P80211_IFTEST) {
		result = 0;
		goto bail;
	} else if (cmd != P80211_IFREQ) {
		result = -EINVAL;
		goto bail;
	}

	msgbuf = memdup_user(data, req->len);
	if (IS_ERR(msgbuf)) {
		result = PTR_ERR(msgbuf);
		goto bail;
	}

	result = p80211req_dorequest(wlandev, msgbuf);

	if (result == 0) {
		if (copy_to_user(data, msgbuf, req->len))
			result = -EFAULT;
	}
	kfree(msgbuf);

bail:
	 
	return result;
}

 
static int p80211knetdev_set_mac_address(struct net_device *dev, void *addr)
{
	struct sockaddr *new_addr = addr;
	struct p80211msg_dot11req_mibset dot11req;
	struct p80211item_unk392 *mibattr;
	struct p80211item_pstr6 *macaddr;
	struct p80211item_uint32 *resultcode;
	int result;

	 
	if (netif_running(dev))
		return -EBUSY;

	 
	mibattr = &dot11req.mibattribute;
	macaddr = (struct p80211item_pstr6 *)&mibattr->data;
	resultcode = &dot11req.resultcode;

	 
	memset(&dot11req, 0, sizeof(dot11req));
	dot11req.msgcode = DIDMSG_DOT11REQ_MIBSET;
	dot11req.msglen = sizeof(dot11req);
	memcpy(dot11req.devname,
	       ((struct wlandevice *)dev->ml_priv)->name,
	       WLAN_DEVNAMELEN_MAX - 1);

	 
	mibattr->did = DIDMSG_DOT11REQ_MIBSET_MIBATTRIBUTE;
	mibattr->status = P80211ENUM_msgitem_status_data_ok;
	mibattr->len = sizeof(mibattr->data);

	macaddr->did = DIDMIB_DOT11MAC_OPERATIONTABLE_MACADDRESS;
	macaddr->status = P80211ENUM_msgitem_status_data_ok;
	macaddr->len = sizeof(macaddr->data);
	macaddr->data.len = ETH_ALEN;
	memcpy(&macaddr->data.data, new_addr->sa_data, ETH_ALEN);

	 
	resultcode->did = DIDMSG_DOT11REQ_MIBSET_RESULTCODE;
	resultcode->status = P80211ENUM_msgitem_status_no_value;
	resultcode->len = sizeof(resultcode->data);
	resultcode->data = 0;

	 
	result = p80211req_dorequest(dev->ml_priv, (u8 *)&dot11req);

	 
	if (result != 0 || resultcode->data != P80211ENUM_resultcode_success) {
		netdev_err(dev, "Low-level driver failed dot11req_mibset(dot11MACAddress).\n");
		result = -EADDRNOTAVAIL;
	} else {
		 
		eth_hw_addr_set(dev, new_addr->sa_data);
	}

	return result;
}

static const struct net_device_ops p80211_netdev_ops = {
	.ndo_init = p80211knetdev_init,
	.ndo_open = p80211knetdev_open,
	.ndo_stop = p80211knetdev_stop,
	.ndo_start_xmit = p80211knetdev_hard_start_xmit,
	.ndo_set_rx_mode = p80211knetdev_set_multicast_list,
	.ndo_siocdevprivate = p80211knetdev_siocdevprivate,
	.ndo_set_mac_address = p80211knetdev_set_mac_address,
	.ndo_tx_timeout = p80211knetdev_tx_timeout,
	.ndo_validate_addr = eth_validate_addr,
};

 
int wlan_setup(struct wlandevice *wlandev, struct device *physdev)
{
	int result = 0;
	struct net_device *netdev;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;

	 
	wlandev->state = WLAN_DEVICE_CLOSED;
	wlandev->ethconv = WLAN_ETHCONV_8021h;
	wlandev->macmode = WLAN_MACMODE_NONE;

	 
	skb_queue_head_init(&wlandev->nsd_rxq);
	tasklet_setup(&wlandev->rx_bh, p80211netdev_rx_bh);

	 
	wiphy = wlan_create_wiphy(physdev, wlandev);
	if (!wiphy) {
		dev_err(physdev, "Failed to alloc wiphy.\n");
		return 1;
	}

	 
	netdev = alloc_netdev(sizeof(struct wireless_dev), "wlan%d",
			      NET_NAME_UNKNOWN, ether_setup);
	if (!netdev) {
		dev_err(physdev, "Failed to alloc netdev.\n");
		wlan_free_wiphy(wiphy);
		result = 1;
	} else {
		wlandev->netdev = netdev;
		netdev->ml_priv = wlandev;
		netdev->netdev_ops = &p80211_netdev_ops;
		wdev = netdev_priv(netdev);
		wdev->wiphy = wiphy;
		wdev->iftype = NL80211_IFTYPE_STATION;
		netdev->ieee80211_ptr = wdev;
		netdev->min_mtu = 68;
		 
		netdev->max_mtu = (2312 - 20 - 8);

		netif_stop_queue(netdev);
		netif_carrier_off(netdev);
	}

	return result;
}

 
void wlan_unsetup(struct wlandevice *wlandev)
{
	struct wireless_dev *wdev;

	tasklet_kill(&wlandev->rx_bh);

	if (wlandev->netdev) {
		wdev = netdev_priv(wlandev->netdev);
		if (wdev->wiphy)
			wlan_free_wiphy(wdev->wiphy);
		free_netdev(wlandev->netdev);
		wlandev->netdev = NULL;
	}
}

 
int register_wlandev(struct wlandevice *wlandev)
{
	return register_netdev(wlandev->netdev);
}

 
int unregister_wlandev(struct wlandevice *wlandev)
{
	struct sk_buff *skb;

	unregister_netdev(wlandev->netdev);

	 
	while ((skb = skb_dequeue(&wlandev->nsd_rxq)))
		dev_kfree_skb(skb);

	return 0;
}

 
void p80211netdev_hwremoved(struct wlandevice *wlandev)
{
	wlandev->hwremoved = 1;
	if (wlandev->state == WLAN_DEVICE_OPEN)
		netif_stop_queue(wlandev->netdev);

	netif_device_detach(wlandev->netdev);
}

 
static int p80211_rx_typedrop(struct wlandevice *wlandev, u16 fc)
{
	u16 ftype;
	u16 fstype;
	int drop = 0;
	 
	ftype = WLAN_GET_FC_FTYPE(fc);
	fstype = WLAN_GET_FC_FSTYPE(fc);
	switch (ftype) {
	case WLAN_FTYPE_MGMT:
		if ((wlandev->netdev->flags & IFF_PROMISC) ||
		    (wlandev->netdev->flags & IFF_ALLMULTI)) {
			drop = 1;
			break;
		}
		netdev_dbg(wlandev->netdev, "rx'd mgmt:\n");
		wlandev->rx.mgmt++;
		switch (fstype) {
		case WLAN_FSTYPE_ASSOCREQ:
			wlandev->rx.assocreq++;
			break;
		case WLAN_FSTYPE_ASSOCRESP:
			wlandev->rx.assocresp++;
			break;
		case WLAN_FSTYPE_REASSOCREQ:
			wlandev->rx.reassocreq++;
			break;
		case WLAN_FSTYPE_REASSOCRESP:
			wlandev->rx.reassocresp++;
			break;
		case WLAN_FSTYPE_PROBEREQ:
			wlandev->rx.probereq++;
			break;
		case WLAN_FSTYPE_PROBERESP:
			wlandev->rx.proberesp++;
			break;
		case WLAN_FSTYPE_BEACON:
			wlandev->rx.beacon++;
			break;
		case WLAN_FSTYPE_ATIM:
			wlandev->rx.atim++;
			break;
		case WLAN_FSTYPE_DISASSOC:
			wlandev->rx.disassoc++;
			break;
		case WLAN_FSTYPE_AUTHEN:
			wlandev->rx.authen++;
			break;
		case WLAN_FSTYPE_DEAUTHEN:
			wlandev->rx.deauthen++;
			break;
		default:
			wlandev->rx.mgmt_unknown++;
			break;
		}
		drop = 2;
		break;

	case WLAN_FTYPE_CTL:
		if ((wlandev->netdev->flags & IFF_PROMISC) ||
		    (wlandev->netdev->flags & IFF_ALLMULTI)) {
			drop = 1;
			break;
		}
		netdev_dbg(wlandev->netdev, "rx'd ctl:\n");
		wlandev->rx.ctl++;
		switch (fstype) {
		case WLAN_FSTYPE_PSPOLL:
			wlandev->rx.pspoll++;
			break;
		case WLAN_FSTYPE_RTS:
			wlandev->rx.rts++;
			break;
		case WLAN_FSTYPE_CTS:
			wlandev->rx.cts++;
			break;
		case WLAN_FSTYPE_ACK:
			wlandev->rx.ack++;
			break;
		case WLAN_FSTYPE_CFEND:
			wlandev->rx.cfend++;
			break;
		case WLAN_FSTYPE_CFENDCFACK:
			wlandev->rx.cfendcfack++;
			break;
		default:
			wlandev->rx.ctl_unknown++;
			break;
		}
		drop = 2;
		break;

	case WLAN_FTYPE_DATA:
		wlandev->rx.data++;
		switch (fstype) {
		case WLAN_FSTYPE_DATAONLY:
			wlandev->rx.dataonly++;
			break;
		case WLAN_FSTYPE_DATA_CFACK:
			wlandev->rx.data_cfack++;
			break;
		case WLAN_FSTYPE_DATA_CFPOLL:
			wlandev->rx.data_cfpoll++;
			break;
		case WLAN_FSTYPE_DATA_CFACK_CFPOLL:
			wlandev->rx.data__cfack_cfpoll++;
			break;
		case WLAN_FSTYPE_NULL:
			netdev_dbg(wlandev->netdev, "rx'd data:null\n");
			wlandev->rx.null++;
			break;
		case WLAN_FSTYPE_CFACK:
			netdev_dbg(wlandev->netdev, "rx'd data:cfack\n");
			wlandev->rx.cfack++;
			break;
		case WLAN_FSTYPE_CFPOLL:
			netdev_dbg(wlandev->netdev, "rx'd data:cfpoll\n");
			wlandev->rx.cfpoll++;
			break;
		case WLAN_FSTYPE_CFACK_CFPOLL:
			netdev_dbg(wlandev->netdev, "rx'd data:cfack_cfpoll\n");
			wlandev->rx.cfack_cfpoll++;
			break;
		default:
			wlandev->rx.data_unknown++;
			break;
		}

		break;
	}
	return drop;
}

static void p80211knetdev_tx_timeout(struct net_device *netdev, unsigned int txqueue)
{
	struct wlandevice *wlandev = netdev->ml_priv;

	if (wlandev->tx_timeout) {
		wlandev->tx_timeout(wlandev);
	} else {
		netdev_warn(netdev, "Implement tx_timeout for %s\n",
			    wlandev->nsdname);
		netif_wake_queue(wlandev->netdev);
	}
}
