 

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/stddef.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/usb.h>
#include <linux/crc32.h>
#include <linux/usb/usbnet.h>

#include "sr9700.h"

static int sr_read(struct usbnet *dev, u8 reg, u16 length, void *data)
{
	int err;

	err = usbnet_read_cmd(dev, SR_RD_REGS, SR_REQ_RD_REG, 0, reg, data,
			      length);
	if ((err != length) && (err >= 0))
		err = -EINVAL;
	return err;
}

static int sr_write(struct usbnet *dev, u8 reg, u16 length, void *data)
{
	int err;

	err = usbnet_write_cmd(dev, SR_WR_REGS, SR_REQ_WR_REG, 0, reg, data,
			       length);
	if ((err >= 0) && (err < length))
		err = -EINVAL;
	return err;
}

static int sr_read_reg(struct usbnet *dev, u8 reg, u8 *value)
{
	return sr_read(dev, reg, 1, value);
}

static int sr_write_reg(struct usbnet *dev, u8 reg, u8 value)
{
	return usbnet_write_cmd(dev, SR_WR_REGS, SR_REQ_WR_REG,
				value, reg, NULL, 0);
}

static void sr_write_async(struct usbnet *dev, u8 reg, u16 length,
			   const void *data)
{
	usbnet_write_cmd_async(dev, SR_WR_REGS, SR_REQ_WR_REG,
			       0, reg, data, length);
}

static void sr_write_reg_async(struct usbnet *dev, u8 reg, u8 value)
{
	usbnet_write_cmd_async(dev, SR_WR_REGS, SR_REQ_WR_REG,
			       value, reg, NULL, 0);
}

static int wait_phy_eeprom_ready(struct usbnet *dev, int phy)
{
	int i;

	for (i = 0; i < SR_SHARE_TIMEOUT; i++) {
		u8 tmp = 0;
		int ret;

		udelay(1);
		ret = sr_read_reg(dev, SR_EPCR, &tmp);
		if (ret < 0)
			return ret;

		 
		if (!(tmp & EPCR_ERRE))
			return 0;
	}

	netdev_err(dev->net, "%s write timed out!\n", phy ? "phy" : "eeprom");

	return -EIO;
}

static int sr_share_read_word(struct usbnet *dev, int phy, u8 reg,
			      __le16 *value)
{
	int ret;

	mutex_lock(&dev->phy_mutex);

	sr_write_reg(dev, SR_EPAR, phy ? (reg | EPAR_PHY_ADR) : reg);
	sr_write_reg(dev, SR_EPCR, phy ? (EPCR_EPOS | EPCR_ERPRR) : EPCR_ERPRR);

	ret = wait_phy_eeprom_ready(dev, phy);
	if (ret < 0)
		goto out_unlock;

	sr_write_reg(dev, SR_EPCR, 0x0);
	ret = sr_read(dev, SR_EPDR, 2, value);

	netdev_dbg(dev->net, "read shared %d 0x%02x returned 0x%04x, %d\n",
		   phy, reg, *value, ret);

out_unlock:
	mutex_unlock(&dev->phy_mutex);
	return ret;
}

static int sr_share_write_word(struct usbnet *dev, int phy, u8 reg,
			       __le16 value)
{
	int ret;

	mutex_lock(&dev->phy_mutex);

	ret = sr_write(dev, SR_EPDR, 2, &value);
	if (ret < 0)
		goto out_unlock;

	sr_write_reg(dev, SR_EPAR, phy ? (reg | EPAR_PHY_ADR) : reg);
	sr_write_reg(dev, SR_EPCR, phy ? (EPCR_WEP | EPCR_EPOS | EPCR_ERPRW) :
		    (EPCR_WEP | EPCR_ERPRW));

	ret = wait_phy_eeprom_ready(dev, phy);
	if (ret < 0)
		goto out_unlock;

	sr_write_reg(dev, SR_EPCR, 0x0);

out_unlock:
	mutex_unlock(&dev->phy_mutex);
	return ret;
}

static int sr_read_eeprom_word(struct usbnet *dev, u8 offset, void *value)
{
	return sr_share_read_word(dev, 0, offset, value);
}

static int sr9700_get_eeprom_len(struct net_device *netdev)
{
	return SR_EEPROM_LEN;
}

static int sr9700_get_eeprom(struct net_device *netdev,
			     struct ethtool_eeprom *eeprom, u8 *data)
{
	struct usbnet *dev = netdev_priv(netdev);
	__le16 *buf = (__le16 *)data;
	int ret = 0;
	int i;

	 
	if ((eeprom->offset & 0x01) || (eeprom->len & 0x01))
		return -EINVAL;

	for (i = 0; i < eeprom->len / 2; i++) {
		ret = sr_read_eeprom_word(dev, eeprom->offset / 2 + i, buf + i);
		if (ret < 0)
			break;
	}

	return ret;
}

static int sr_mdio_read(struct net_device *netdev, int phy_id, int loc)
{
	struct usbnet *dev = netdev_priv(netdev);
	__le16 res;
	int rc = 0;

	if (phy_id) {
		netdev_dbg(netdev, "Only internal phy supported\n");
		return 0;
	}

	 
	if (loc == MII_BMSR) {
		u8 value;

		sr_read_reg(dev, SR_NSR, &value);
		if (value & NSR_LINKST)
			rc = 1;
	}
	sr_share_read_word(dev, 1, loc, &res);
	if (rc == 1)
		res = le16_to_cpu(res) | BMSR_LSTATUS;
	else
		res = le16_to_cpu(res) & ~BMSR_LSTATUS;

	netdev_dbg(netdev, "sr_mdio_read() phy_id=0x%02x, loc=0x%02x, returns=0x%04x\n",
		   phy_id, loc, res);

	return res;
}

static void sr_mdio_write(struct net_device *netdev, int phy_id, int loc,
			  int val)
{
	struct usbnet *dev = netdev_priv(netdev);
	__le16 res = cpu_to_le16(val);

	if (phy_id) {
		netdev_dbg(netdev, "Only internal phy supported\n");
		return;
	}

	netdev_dbg(netdev, "sr_mdio_write() phy_id=0x%02x, loc=0x%02x, val=0x%04x\n",
		   phy_id, loc, val);

	sr_share_write_word(dev, 1, loc, res);
}

static u32 sr9700_get_link(struct net_device *netdev)
{
	struct usbnet *dev = netdev_priv(netdev);
	u8 value = 0;
	int rc = 0;

	 
	sr_read_reg(dev, SR_NSR, &value);
	if (value & NSR_LINKST)
		rc = 1;

	return rc;
}

static int sr9700_ioctl(struct net_device *netdev, struct ifreq *rq, int cmd)
{
	struct usbnet *dev = netdev_priv(netdev);

	return generic_mii_ioctl(&dev->mii, if_mii(rq), cmd, NULL);
}

static const struct ethtool_ops sr9700_ethtool_ops = {
	.get_drvinfo	= usbnet_get_drvinfo,
	.get_link	= sr9700_get_link,
	.get_msglevel	= usbnet_get_msglevel,
	.set_msglevel	= usbnet_set_msglevel,
	.get_eeprom_len	= sr9700_get_eeprom_len,
	.get_eeprom	= sr9700_get_eeprom,
	.nway_reset	= usbnet_nway_reset,
	.get_link_ksettings	= usbnet_get_link_ksettings_mii,
	.set_link_ksettings	= usbnet_set_link_ksettings_mii,
};

static void sr9700_set_multicast(struct net_device *netdev)
{
	struct usbnet *dev = netdev_priv(netdev);
	 
	u8 *hashes = (u8 *)&dev->data;
	 
	u8 rx_ctl = RCR_RXEN | RCR_DIS_CRC | RCR_DIS_LONG;

	memset(hashes, 0x00, SR_MCAST_SIZE);
	 
	hashes[SR_MCAST_SIZE - 1] |= SR_MCAST_ADDR_FLAG;
	if (netdev->flags & IFF_PROMISC) {
		rx_ctl |= RCR_PRMSC;
	} else if (netdev->flags & IFF_ALLMULTI ||
		   netdev_mc_count(netdev) > SR_MCAST_MAX) {
		rx_ctl |= RCR_RUNT;
	} else if (!netdev_mc_empty(netdev)) {
		struct netdev_hw_addr *ha;

		netdev_for_each_mc_addr(ha, netdev) {
			u32 crc = ether_crc(ETH_ALEN, ha->addr) >> 26;
			hashes[crc >> 3] |= 1 << (crc & 0x7);
		}
	}

	sr_write_async(dev, SR_MAR, SR_MCAST_SIZE, hashes);
	sr_write_reg_async(dev, SR_RCR, rx_ctl);
}

static int sr9700_set_mac_address(struct net_device *netdev, void *p)
{
	struct usbnet *dev = netdev_priv(netdev);
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data)) {
		netdev_err(netdev, "not setting invalid mac address %pM\n",
			   addr->sa_data);
		return -EINVAL;
	}

	eth_hw_addr_set(netdev, addr->sa_data);
	sr_write_async(dev, SR_PAR, 6, netdev->dev_addr);

	return 0;
}

static const struct net_device_ops sr9700_netdev_ops = {
	.ndo_open		= usbnet_open,
	.ndo_stop		= usbnet_stop,
	.ndo_start_xmit		= usbnet_start_xmit,
	.ndo_tx_timeout		= usbnet_tx_timeout,
	.ndo_change_mtu		= usbnet_change_mtu,
	.ndo_get_stats64	= dev_get_tstats64,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_eth_ioctl		= sr9700_ioctl,
	.ndo_set_rx_mode	= sr9700_set_multicast,
	.ndo_set_mac_address	= sr9700_set_mac_address,
};

static int sr9700_bind(struct usbnet *dev, struct usb_interface *intf)
{
	struct net_device *netdev;
	struct mii_if_info *mii;
	u8 addr[ETH_ALEN];
	int ret;

	ret = usbnet_get_endpoints(dev, intf);
	if (ret)
		goto out;

	netdev = dev->net;

	netdev->netdev_ops = &sr9700_netdev_ops;
	netdev->ethtool_ops = &sr9700_ethtool_ops;
	netdev->hard_header_len += SR_TX_OVERHEAD;
	dev->hard_mtu = netdev->mtu + netdev->hard_header_len;
	 
	dev->rx_urb_size = 3072;

	mii = &dev->mii;
	mii->dev = netdev;
	mii->mdio_read = sr_mdio_read;
	mii->mdio_write = sr_mdio_write;
	mii->phy_id_mask = 0x1f;
	mii->reg_num_mask = 0x1f;

	sr_write_reg(dev, SR_NCR, NCR_RST);
	udelay(20);

	 
	if (sr_read(dev, SR_PAR, ETH_ALEN, addr) < 0) {
		netdev_err(netdev, "Error reading MAC address\n");
		ret = -ENODEV;
		goto out;
	}
	eth_hw_addr_set(netdev, addr);

	 
	sr_write_reg(dev, SR_PRR, PRR_PHY_RST);
	 
	msleep(20);
	sr_write_reg(dev, SR_PRR, 0);
	 
	udelay(2 * 1000);

	 
	sr9700_set_multicast(netdev);

	sr_mdio_write(netdev, mii->phy_id, MII_BMCR, BMCR_RESET);
	sr_mdio_write(netdev, mii->phy_id, MII_ADVERTISE, ADVERTISE_ALL |
		      ADVERTISE_CSMA | ADVERTISE_PAUSE_CAP);
	mii_nway_restart(mii);

out:
	return ret;
}

static int sr9700_rx_fixup(struct usbnet *dev, struct sk_buff *skb)
{
	struct sk_buff *sr_skb;
	int len;

	 
	if (unlikely(skb->len < SR_RX_OVERHEAD)) {
		netdev_err(dev->net, "unexpected tiny rx frame\n");
		return 0;
	}

	 
	while (skb->len > SR_RX_OVERHEAD) {
		if (skb->data[0] != 0x40)
			return 0;

		 
		len = (skb->data[1] | (skb->data[2] << 8)) - 4;

		if (len > ETH_FRAME_LEN || len > skb->len || len < 0)
			return 0;

		 
		if (skb->len == (len + SR_RX_OVERHEAD))	{
			skb_pull(skb, 3);
			skb->len = len;
			skb_set_tail_pointer(skb, len);
			skb->truesize = len + sizeof(struct sk_buff);
			return 2;
		}

		 
		sr_skb = skb_clone(skb, GFP_ATOMIC);
		if (!sr_skb)
			return 0;

		sr_skb->len = len;
		sr_skb->data = skb->data + 3;
		skb_set_tail_pointer(sr_skb, len);
		sr_skb->truesize = len + sizeof(struct sk_buff);
		usbnet_skb_return(dev, sr_skb);

		skb_pull(skb, len + SR_RX_OVERHEAD);
	}

	return 0;
}

static struct sk_buff *sr9700_tx_fixup(struct usbnet *dev, struct sk_buff *skb,
				       gfp_t flags)
{
	int len;

	 

	len = skb->len;

	if (skb_cow_head(skb, SR_TX_OVERHEAD)) {
		dev_kfree_skb_any(skb);
		return NULL;
	}

	__skb_push(skb, SR_TX_OVERHEAD);

	 
	if ((skb->len % dev->maxpacket) == 0)
		len++;

	skb->data[0] = len;
	skb->data[1] = len >> 8;

	return skb;
}

static void sr9700_status(struct usbnet *dev, struct urb *urb)
{
	int link;
	u8 *buf;

	 

	if (urb->actual_length < 8)
		return;

	buf = urb->transfer_buffer;

	link = !!(buf[0] & 0x40);
	if (netif_carrier_ok(dev->net) != link) {
		usbnet_link_change(dev, link, 1);
		netdev_dbg(dev->net, "Link Status is: %d\n", link);
	}
}

static int sr9700_link_reset(struct usbnet *dev)
{
	struct ethtool_cmd ecmd;

	mii_check_media(&dev->mii, 1, 1);
	mii_ethtool_gset(&dev->mii, &ecmd);

	netdev_dbg(dev->net, "link_reset() speed: %d duplex: %d\n",
		   ecmd.speed, ecmd.duplex);

	return 0;
}

static const struct driver_info sr9700_driver_info = {
	.description	= "CoreChip SR9700 USB Ethernet",
	.flags		= FLAG_ETHER,
	.bind		= sr9700_bind,
	.rx_fixup	= sr9700_rx_fixup,
	.tx_fixup	= sr9700_tx_fixup,
	.status		= sr9700_status,
	.link_reset	= sr9700_link_reset,
	.reset		= sr9700_link_reset,
};

static const struct usb_device_id products[] = {
	{
		USB_DEVICE(0x0fe6, 0x9700),	 
		.driver_info = (unsigned long)&sr9700_driver_info,
	},
	{},			 
};

MODULE_DEVICE_TABLE(usb, products);

static struct usb_driver sr9700_usb_driver = {
	.name		= "sr9700",
	.id_table	= products,
	.probe		= usbnet_probe,
	.disconnect	= usbnet_disconnect,
	.suspend	= usbnet_suspend,
	.resume		= usbnet_resume,
	.disable_hub_initiated_lpm = 1,
};

module_usb_driver(sr9700_usb_driver);

MODULE_AUTHOR("liujl <liujunliang_ljl@163.com>");
MODULE_DESCRIPTION("SR9700 one chip USB 1.1 USB to Ethernet device from http://www.corechip-sz.com/");
MODULE_LICENSE("GPL");
