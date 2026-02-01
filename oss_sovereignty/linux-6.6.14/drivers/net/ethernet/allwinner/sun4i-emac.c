 

#include <linux/clk.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mii.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/phy.h>
#include <linux/soc/sunxi/sunxi_sram.h>
#include <linux/dmaengine.h>

#include "sun4i-emac.h"

#define DRV_NAME		"sun4i-emac"

#define EMAC_MAX_FRAME_LEN	0x0600

#define EMAC_DEFAULT_MSG_ENABLE 0x0000
static int debug = -1;      ;
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "debug message flags");

 
static int watchdog = 5000;
module_param(watchdog, int, 0400);
MODULE_PARM_DESC(watchdog, "transmit timeout in milliseconds");

 

 

struct emac_board_info {
	struct clk		*clk;
	struct device		*dev;
	struct platform_device	*pdev;
	spinlock_t		lock;
	void __iomem		*membase;
	u32			msg_enable;
	struct net_device	*ndev;
	u16			tx_fifo_stat;

	int			emacrx_completed_flag;

	struct device_node	*phy_node;
	unsigned int		link;
	unsigned int		speed;
	unsigned int		duplex;

	phy_interface_t		phy_interface;
	struct dma_chan	*rx_chan;
	phys_addr_t emac_rx_fifo;
};

struct emac_dma_req {
	struct emac_board_info *db;
	struct dma_async_tx_descriptor *desc;
	struct sk_buff *skb;
	dma_addr_t rxbuf;
	int count;
};

static void emac_update_speed(struct net_device *dev)
{
	struct emac_board_info *db = netdev_priv(dev);
	unsigned int reg_val;

	 
	reg_val = readl(db->membase + EMAC_MAC_SUPP_REG);
	reg_val &= ~EMAC_MAC_SUPP_100M;
	if (db->speed == SPEED_100)
		reg_val |= EMAC_MAC_SUPP_100M;
	writel(reg_val, db->membase + EMAC_MAC_SUPP_REG);
}

static void emac_update_duplex(struct net_device *dev)
{
	struct emac_board_info *db = netdev_priv(dev);
	unsigned int reg_val;

	 
	reg_val = readl(db->membase + EMAC_MAC_CTL1_REG);
	reg_val &= ~EMAC_MAC_CTL1_DUPLEX_EN;
	if (db->duplex)
		reg_val |= EMAC_MAC_CTL1_DUPLEX_EN;
	writel(reg_val, db->membase + EMAC_MAC_CTL1_REG);
}

static void emac_handle_link_change(struct net_device *dev)
{
	struct emac_board_info *db = netdev_priv(dev);
	struct phy_device *phydev = dev->phydev;
	unsigned long flags;
	int status_change = 0;

	if (phydev->link) {
		if (db->speed != phydev->speed) {
			spin_lock_irqsave(&db->lock, flags);
			db->speed = phydev->speed;
			emac_update_speed(dev);
			spin_unlock_irqrestore(&db->lock, flags);
			status_change = 1;
		}

		if (db->duplex != phydev->duplex) {
			spin_lock_irqsave(&db->lock, flags);
			db->duplex = phydev->duplex;
			emac_update_duplex(dev);
			spin_unlock_irqrestore(&db->lock, flags);
			status_change = 1;
		}
	}

	if (phydev->link != db->link) {
		if (!phydev->link) {
			db->speed = 0;
			db->duplex = -1;
		}
		db->link = phydev->link;

		status_change = 1;
	}

	if (status_change)
		phy_print_status(phydev);
}

static int emac_mdio_probe(struct net_device *dev)
{
	struct emac_board_info *db = netdev_priv(dev);
	struct phy_device *phydev;

	 

	 
	phydev = of_phy_connect(db->ndev, db->phy_node,
				&emac_handle_link_change, 0,
				db->phy_interface);
	if (!phydev) {
		netdev_err(db->ndev, "could not find the PHY\n");
		return -ENODEV;
	}

	 
	phy_set_max_speed(phydev, SPEED_100);

	db->link = 0;
	db->speed = 0;
	db->duplex = -1;

	return 0;
}

static void emac_mdio_remove(struct net_device *dev)
{
	phy_disconnect(dev->phydev);
}

static void emac_reset(struct emac_board_info *db)
{
	dev_dbg(db->dev, "resetting device\n");

	 
	writel(0, db->membase + EMAC_CTL_REG);
	udelay(200);
	writel(EMAC_CTL_RESET, db->membase + EMAC_CTL_REG);
	udelay(200);
}

static void emac_outblk_32bit(void __iomem *reg, void *data, int count)
{
	writesl(reg, data, round_up(count, 4) / 4);
}

static void emac_inblk_32bit(void __iomem *reg, void *data, int count)
{
	readsl(reg, data, round_up(count, 4) / 4);
}

static struct emac_dma_req *
emac_alloc_dma_req(struct emac_board_info *db,
		   struct dma_async_tx_descriptor *desc, struct sk_buff *skb,
		   dma_addr_t rxbuf, int count)
{
	struct emac_dma_req *req;

	req = kzalloc(sizeof(struct emac_dma_req), GFP_ATOMIC);
	if (!req)
		return NULL;

	req->db = db;
	req->desc = desc;
	req->skb = skb;
	req->rxbuf = rxbuf;
	req->count = count;
	return req;
}

static void emac_free_dma_req(struct emac_dma_req *req)
{
	kfree(req);
}

static void emac_dma_done_callback(void *arg)
{
	struct emac_dma_req *req = arg;
	struct emac_board_info *db = req->db;
	struct sk_buff *skb = req->skb;
	struct net_device *dev = db->ndev;
	int rxlen = req->count;
	u32 reg_val;

	dma_unmap_single(db->dev, req->rxbuf, rxlen, DMA_FROM_DEVICE);

	skb->protocol = eth_type_trans(skb, dev);
	netif_rx(skb);
	dev->stats.rx_bytes += rxlen;
	 
	dev->stats.rx_packets++;

	 
	reg_val = readl(db->membase + EMAC_RX_CTL_REG);
	reg_val &= ~EMAC_RX_CTL_DMA_EN;
	writel(reg_val, db->membase + EMAC_RX_CTL_REG);

	 
	reg_val = readl(db->membase + EMAC_INT_CTL_REG);
	reg_val |= EMAC_INT_CTL_RX_EN;
	writel(reg_val, db->membase + EMAC_INT_CTL_REG);

	db->emacrx_completed_flag = 1;
	emac_free_dma_req(req);
}

static int emac_dma_inblk_32bit(struct emac_board_info *db,
		struct sk_buff *skb, void *rdptr, int count)
{
	struct dma_async_tx_descriptor *desc;
	dma_cookie_t cookie;
	dma_addr_t rxbuf;
	struct emac_dma_req *req;
	int ret = 0;

	rxbuf = dma_map_single(db->dev, rdptr, count, DMA_FROM_DEVICE);
	ret = dma_mapping_error(db->dev, rxbuf);
	if (ret) {
		dev_err(db->dev, "dma mapping error.\n");
		return ret;
	}

	desc = dmaengine_prep_slave_single(db->rx_chan, rxbuf, count,
					   DMA_DEV_TO_MEM,
					   DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc) {
		dev_err(db->dev, "prepare slave single failed\n");
		ret = -ENOMEM;
		goto prepare_err;
	}

	req = emac_alloc_dma_req(db, desc, skb, rxbuf, count);
	if (!req) {
		dev_err(db->dev, "alloc emac dma req error.\n");
		ret = -ENOMEM;
		goto alloc_req_err;
	}

	desc->callback_param = req;
	desc->callback = emac_dma_done_callback;

	cookie = dmaengine_submit(desc);
	ret = dma_submit_error(cookie);
	if (ret) {
		dev_err(db->dev, "dma submit error.\n");
		goto submit_err;
	}

	dma_async_issue_pending(db->rx_chan);
	return ret;

submit_err:
	emac_free_dma_req(req);

alloc_req_err:
	dmaengine_desc_free(desc);

prepare_err:
	dma_unmap_single(db->dev, rxbuf, count, DMA_FROM_DEVICE);
	return ret;
}

 
static void emac_get_drvinfo(struct net_device *dev,
			      struct ethtool_drvinfo *info)
{
	strscpy(info->driver, DRV_NAME, sizeof(info->driver));
	strscpy(info->bus_info, dev_name(&dev->dev), sizeof(info->bus_info));
}

static u32 emac_get_msglevel(struct net_device *dev)
{
	struct emac_board_info *db = netdev_priv(dev);

	return db->msg_enable;
}

static void emac_set_msglevel(struct net_device *dev, u32 value)
{
	struct emac_board_info *db = netdev_priv(dev);

	db->msg_enable = value;
}

static const struct ethtool_ops emac_ethtool_ops = {
	.get_drvinfo	= emac_get_drvinfo,
	.get_link	= ethtool_op_get_link,
	.get_link_ksettings = phy_ethtool_get_link_ksettings,
	.set_link_ksettings = phy_ethtool_set_link_ksettings,
	.get_msglevel	= emac_get_msglevel,
	.set_msglevel	= emac_set_msglevel,
};

static unsigned int emac_setup(struct net_device *ndev)
{
	struct emac_board_info *db = netdev_priv(ndev);
	unsigned int reg_val;

	 
	reg_val = readl(db->membase + EMAC_TX_MODE_REG);

	writel(reg_val | EMAC_TX_MODE_ABORTED_FRAME_EN,
		db->membase + EMAC_TX_MODE_REG);

	 
	 
	reg_val = readl(db->membase + EMAC_MAC_CTL0_REG);
	writel(reg_val | EMAC_MAC_CTL0_RX_FLOW_CTL_EN |
		EMAC_MAC_CTL0_TX_FLOW_CTL_EN,
		db->membase + EMAC_MAC_CTL0_REG);

	 
	reg_val = readl(db->membase + EMAC_MAC_CTL1_REG);
	reg_val |= EMAC_MAC_CTL1_LEN_CHECK_EN;
	reg_val |= EMAC_MAC_CTL1_CRC_EN;
	reg_val |= EMAC_MAC_CTL1_PAD_EN;
	writel(reg_val, db->membase + EMAC_MAC_CTL1_REG);

	 
	writel(EMAC_MAC_IPGT_FULL_DUPLEX, db->membase + EMAC_MAC_IPGT_REG);

	 
	writel((EMAC_MAC_IPGR_IPG1 << 8) | EMAC_MAC_IPGR_IPG2,
		db->membase + EMAC_MAC_IPGR_REG);

	 
	writel((EMAC_MAC_CLRT_COLLISION_WINDOW << 8) | EMAC_MAC_CLRT_RM,
		db->membase + EMAC_MAC_CLRT_REG);

	 
	writel(EMAC_MAX_FRAME_LEN,
		db->membase + EMAC_MAC_MAXF_REG);

	return 0;
}

static void emac_set_rx_mode(struct net_device *ndev)
{
	struct emac_board_info *db = netdev_priv(ndev);
	unsigned int reg_val;

	 
	reg_val = readl(db->membase + EMAC_RX_CTL_REG);

	if (ndev->flags & IFF_PROMISC)
		reg_val |= EMAC_RX_CTL_PASS_ALL_EN;
	else
		reg_val &= ~EMAC_RX_CTL_PASS_ALL_EN;

	writel(reg_val | EMAC_RX_CTL_PASS_LEN_OOR_EN |
		EMAC_RX_CTL_ACCEPT_UNICAST_EN | EMAC_RX_CTL_DA_FILTER_EN |
		EMAC_RX_CTL_ACCEPT_MULTICAST_EN |
		EMAC_RX_CTL_ACCEPT_BROADCAST_EN,
		db->membase + EMAC_RX_CTL_REG);
}

static unsigned int emac_powerup(struct net_device *ndev)
{
	struct emac_board_info *db = netdev_priv(ndev);
	unsigned int reg_val;

	 
	 
	reg_val = readl(db->membase + EMAC_RX_CTL_REG);
	reg_val |= EMAC_RX_CTL_FLUSH_FIFO;
	writel(reg_val, db->membase + EMAC_RX_CTL_REG);
	udelay(1);

	 
	 
	reg_val = readl(db->membase + EMAC_MAC_CTL0_REG);
	reg_val &= ~EMAC_MAC_CTL0_SOFT_RESET;
	writel(reg_val, db->membase + EMAC_MAC_CTL0_REG);

	 
	reg_val = readl(db->membase + EMAC_MAC_MCFG_REG);
	reg_val &= ~EMAC_MAC_MCFG_MII_CLKD_MASK;
	reg_val |= EMAC_MAC_MCFG_MII_CLKD_72;
	writel(reg_val, db->membase + EMAC_MAC_MCFG_REG);

	 
	writel(0x0, db->membase + EMAC_RX_FBC_REG);

	 
	writel(0, db->membase + EMAC_INT_CTL_REG);
	reg_val = readl(db->membase + EMAC_INT_STA_REG);
	writel(reg_val, db->membase + EMAC_INT_STA_REG);

	udelay(1);

	 
	emac_setup(ndev);

	 
	writel(ndev->dev_addr[0] << 16 | ndev->dev_addr[1] << 8 | ndev->
	       dev_addr[2], db->membase + EMAC_MAC_A1_REG);
	writel(ndev->dev_addr[3] << 16 | ndev->dev_addr[4] << 8 | ndev->
	       dev_addr[5], db->membase + EMAC_MAC_A0_REG);

	mdelay(1);

	return 0;
}

static int emac_set_mac_address(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;
	struct emac_board_info *db = netdev_priv(dev);

	if (netif_running(dev))
		return -EBUSY;

	eth_hw_addr_set(dev, addr->sa_data);

	writel(dev->dev_addr[0] << 16 | dev->dev_addr[1] << 8 | dev->
	       dev_addr[2], db->membase + EMAC_MAC_A1_REG);
	writel(dev->dev_addr[3] << 16 | dev->dev_addr[4] << 8 | dev->
	       dev_addr[5], db->membase + EMAC_MAC_A0_REG);

	return 0;
}

 
static void emac_init_device(struct net_device *dev)
{
	struct emac_board_info *db = netdev_priv(dev);
	unsigned long flags;
	unsigned int reg_val;

	spin_lock_irqsave(&db->lock, flags);

	emac_update_speed(dev);
	emac_update_duplex(dev);

	 
	reg_val = readl(db->membase + EMAC_CTL_REG);
	writel(reg_val | EMAC_CTL_RESET | EMAC_CTL_TX_EN | EMAC_CTL_RX_EN,
		db->membase + EMAC_CTL_REG);

	 
	reg_val = readl(db->membase + EMAC_INT_CTL_REG);
	reg_val |= (EMAC_INT_CTL_TX_EN | EMAC_INT_CTL_TX_ABRT_EN | EMAC_INT_CTL_RX_EN);
	writel(reg_val, db->membase + EMAC_INT_CTL_REG);

	spin_unlock_irqrestore(&db->lock, flags);
}

 
static void emac_timeout(struct net_device *dev, unsigned int txqueue)
{
	struct emac_board_info *db = netdev_priv(dev);
	unsigned long flags;

	if (netif_msg_timer(db))
		dev_err(db->dev, "tx time out.\n");

	 
	spin_lock_irqsave(&db->lock, flags);

	netif_stop_queue(dev);
	emac_reset(db);
	emac_init_device(dev);
	 
	netif_trans_update(dev);
	netif_wake_queue(dev);

	 
	spin_unlock_irqrestore(&db->lock, flags);
}

 
static netdev_tx_t emac_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct emac_board_info *db = netdev_priv(dev);
	unsigned long channel;
	unsigned long flags;

	channel = db->tx_fifo_stat & 3;
	if (channel == 3)
		return NETDEV_TX_BUSY;

	channel = (channel == 1 ? 1 : 0);

	spin_lock_irqsave(&db->lock, flags);

	writel(channel, db->membase + EMAC_TX_INS_REG);

	emac_outblk_32bit(db->membase + EMAC_TX_IO_DATA_REG,
			skb->data, skb->len);
	dev->stats.tx_bytes += skb->len;

	db->tx_fifo_stat |= 1 << channel;
	 
	if (channel == 0) {
		 
		writel(skb->len, db->membase + EMAC_TX_PL0_REG);
		 
		writel(readl(db->membase + EMAC_TX_CTL0_REG) | 1,
		       db->membase + EMAC_TX_CTL0_REG);

		 
		netif_trans_update(dev);
	} else if (channel == 1) {
		 
		writel(skb->len, db->membase + EMAC_TX_PL1_REG);
		 
		writel(readl(db->membase + EMAC_TX_CTL1_REG) | 1,
		       db->membase + EMAC_TX_CTL1_REG);

		 
		netif_trans_update(dev);
	}

	if ((db->tx_fifo_stat & 3) == 3) {
		 
		netif_stop_queue(dev);
	}

	spin_unlock_irqrestore(&db->lock, flags);

	 
	dev_consume_skb_any(skb);

	return NETDEV_TX_OK;
}

 
static void emac_tx_done(struct net_device *dev, struct emac_board_info *db,
			  unsigned int tx_status)
{
	 
	db->tx_fifo_stat &= ~(tx_status & 3);
	if (3 == (tx_status & 3))
		dev->stats.tx_packets += 2;
	else
		dev->stats.tx_packets++;

	if (netif_msg_tx_done(db))
		dev_dbg(db->dev, "tx done, NSR %02x\n", tx_status);

	netif_wake_queue(dev);
}

 
static void emac_rx(struct net_device *dev)
{
	struct emac_board_info *db = netdev_priv(dev);
	struct sk_buff *skb;
	u8 *rdptr;
	bool good_packet;
	unsigned int reg_val;
	u32 rxhdr, rxstatus, rxcount, rxlen;

	 
	while (1) {
		 
		rxcount = readl(db->membase + EMAC_RX_FBC_REG);

		if (netif_msg_rx_status(db))
			dev_dbg(db->dev, "RXCount: %x\n", rxcount);

		if (!rxcount) {
			db->emacrx_completed_flag = 1;
			reg_val = readl(db->membase + EMAC_INT_CTL_REG);
			reg_val |= (EMAC_INT_CTL_TX_EN |
					EMAC_INT_CTL_TX_ABRT_EN |
					EMAC_INT_CTL_RX_EN);
			writel(reg_val, db->membase + EMAC_INT_CTL_REG);

			 
			rxcount = readl(db->membase + EMAC_RX_FBC_REG);
			if (!rxcount)
				return;
		}

		reg_val = readl(db->membase + EMAC_RX_IO_DATA_REG);
		if (netif_msg_rx_status(db))
			dev_dbg(db->dev, "receive header: %x\n", reg_val);
		if (reg_val != EMAC_UNDOCUMENTED_MAGIC) {
			 
			reg_val = readl(db->membase + EMAC_CTL_REG);
			writel(reg_val & ~EMAC_CTL_RX_EN,
			       db->membase + EMAC_CTL_REG);

			 
			reg_val = readl(db->membase + EMAC_RX_CTL_REG);
			writel(reg_val | (1 << 3),
			       db->membase + EMAC_RX_CTL_REG);

			do {
				reg_val = readl(db->membase + EMAC_RX_CTL_REG);
			} while (reg_val & (1 << 3));

			 
			reg_val = readl(db->membase + EMAC_CTL_REG);
			writel(reg_val | EMAC_CTL_RX_EN,
			       db->membase + EMAC_CTL_REG);
			reg_val = readl(db->membase + EMAC_INT_CTL_REG);
			reg_val |= (EMAC_INT_CTL_TX_EN |
					EMAC_INT_CTL_TX_ABRT_EN |
					EMAC_INT_CTL_RX_EN);
			writel(reg_val, db->membase + EMAC_INT_CTL_REG);

			db->emacrx_completed_flag = 1;

			return;
		}

		 
		good_packet = true;

		rxhdr = readl(db->membase + EMAC_RX_IO_DATA_REG);

		if (netif_msg_rx_status(db))
			dev_dbg(db->dev, "rxhdr: %x\n", *((int *)(&rxhdr)));

		rxlen = EMAC_RX_IO_DATA_LEN(rxhdr);
		rxstatus = EMAC_RX_IO_DATA_STATUS(rxhdr);

		if (netif_msg_rx_status(db))
			dev_dbg(db->dev, "RX: status %02x, length %04x\n",
				rxstatus, rxlen);

		 
		if (rxlen < 0x40) {
			good_packet = false;
			if (netif_msg_rx_err(db))
				dev_dbg(db->dev, "RX: Bad Packet (runt)\n");
		}

		if (unlikely(!(rxstatus & EMAC_RX_IO_DATA_STATUS_OK))) {
			good_packet = false;

			if (rxstatus & EMAC_RX_IO_DATA_STATUS_CRC_ERR) {
				if (netif_msg_rx_err(db))
					dev_dbg(db->dev, "crc error\n");
				dev->stats.rx_crc_errors++;
			}

			if (rxstatus & EMAC_RX_IO_DATA_STATUS_LEN_ERR) {
				if (netif_msg_rx_err(db))
					dev_dbg(db->dev, "length error\n");
				dev->stats.rx_length_errors++;
			}
		}

		 
		if (good_packet) {
			skb = netdev_alloc_skb(dev, rxlen + 4);
			if (!skb)
				continue;
			skb_reserve(skb, 2);
			rdptr = skb_put(skb, rxlen - 4);

			 
			if (netif_msg_rx_status(db))
				dev_dbg(db->dev, "RxLen %x\n", rxlen);

			if (rxlen >= dev->mtu && db->rx_chan) {
				reg_val = readl(db->membase + EMAC_RX_CTL_REG);
				reg_val |= EMAC_RX_CTL_DMA_EN;
				writel(reg_val, db->membase + EMAC_RX_CTL_REG);
				if (!emac_dma_inblk_32bit(db, skb, rdptr, rxlen))
					break;

				 
				reg_val = readl(db->membase + EMAC_RX_CTL_REG);
				reg_val &= ~EMAC_RX_CTL_DMA_EN;
				writel(reg_val, db->membase + EMAC_RX_CTL_REG);
			}

			emac_inblk_32bit(db->membase + EMAC_RX_IO_DATA_REG,
					rdptr, rxlen);
			dev->stats.rx_bytes += rxlen;

			 
			skb->protocol = eth_type_trans(skb, dev);
			netif_rx(skb);
			dev->stats.rx_packets++;
		}
	}
}

static irqreturn_t emac_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct emac_board_info *db = netdev_priv(dev);
	int int_status;
	unsigned int reg_val;

	 

	spin_lock(&db->lock);

	 
	writel(0, db->membase + EMAC_INT_CTL_REG);

	 
	 
	int_status = readl(db->membase + EMAC_INT_STA_REG);
	 
	writel(int_status, db->membase + EMAC_INT_STA_REG);

	if (netif_msg_intr(db))
		dev_dbg(db->dev, "emac interrupt %02x\n", int_status);

	 
	if ((int_status & 0x100) && (db->emacrx_completed_flag == 1)) {
		 
		db->emacrx_completed_flag = 0;
		emac_rx(dev);
	}

	 
	if (int_status & EMAC_INT_STA_TX_COMPLETE)
		emac_tx_done(dev, db, int_status);

	if (int_status & EMAC_INT_STA_TX_ABRT)
		netdev_info(dev, " ab : %x\n", int_status);

	 
	if (db->emacrx_completed_flag == 1) {
		reg_val = readl(db->membase + EMAC_INT_CTL_REG);
		reg_val |= (EMAC_INT_CTL_TX_EN | EMAC_INT_CTL_TX_ABRT_EN | EMAC_INT_CTL_RX_EN);
		writel(reg_val, db->membase + EMAC_INT_CTL_REG);
	} else {
		reg_val = readl(db->membase + EMAC_INT_CTL_REG);
		reg_val |= (EMAC_INT_CTL_TX_EN | EMAC_INT_CTL_TX_ABRT_EN);
		writel(reg_val, db->membase + EMAC_INT_CTL_REG);
	}

	spin_unlock(&db->lock);

	return IRQ_HANDLED;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
 
static void emac_poll_controller(struct net_device *dev)
{
	disable_irq(dev->irq);
	emac_interrupt(dev->irq, dev);
	enable_irq(dev->irq);
}
#endif

 
static int emac_open(struct net_device *dev)
{
	struct emac_board_info *db = netdev_priv(dev);
	int ret;

	if (netif_msg_ifup(db))
		dev_dbg(db->dev, "enabling %s\n", dev->name);

	if (request_irq(dev->irq, &emac_interrupt, 0, dev->name, dev))
		return -EAGAIN;

	 
	emac_reset(db);
	emac_init_device(dev);

	ret = emac_mdio_probe(dev);
	if (ret < 0) {
		free_irq(dev->irq, dev);
		netdev_err(dev, "cannot probe MDIO bus\n");
		return ret;
	}

	phy_start(dev->phydev);
	netif_start_queue(dev);

	return 0;
}

static void emac_shutdown(struct net_device *dev)
{
	unsigned int reg_val;
	struct emac_board_info *db = netdev_priv(dev);

	 
	writel(0, db->membase + EMAC_INT_CTL_REG);

	 
	reg_val = readl(db->membase + EMAC_INT_STA_REG);
	writel(reg_val, db->membase + EMAC_INT_STA_REG);

	 
	reg_val = readl(db->membase + EMAC_CTL_REG);
	reg_val &= ~(EMAC_CTL_TX_EN | EMAC_CTL_RX_EN | EMAC_CTL_RESET);
	writel(reg_val, db->membase + EMAC_CTL_REG);
}

 
static int emac_stop(struct net_device *ndev)
{
	struct emac_board_info *db = netdev_priv(ndev);

	if (netif_msg_ifdown(db))
		dev_dbg(db->dev, "shutting down %s\n", ndev->name);

	netif_stop_queue(ndev);
	netif_carrier_off(ndev);

	phy_stop(ndev->phydev);

	emac_mdio_remove(ndev);

	emac_shutdown(ndev);

	free_irq(ndev->irq, ndev);

	return 0;
}

static const struct net_device_ops emac_netdev_ops = {
	.ndo_open		= emac_open,
	.ndo_stop		= emac_stop,
	.ndo_start_xmit		= emac_start_xmit,
	.ndo_tx_timeout		= emac_timeout,
	.ndo_set_rx_mode	= emac_set_rx_mode,
	.ndo_eth_ioctl		= phy_do_ioctl_running,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address	= emac_set_mac_address,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= emac_poll_controller,
#endif
};

static int emac_configure_dma(struct emac_board_info *db)
{
	struct platform_device *pdev = db->pdev;
	struct net_device *ndev = db->ndev;
	struct dma_slave_config conf = {};
	struct resource *regs;
	int err = 0;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		netdev_err(ndev, "get io resource from device failed.\n");
		err = -ENOMEM;
		goto out_clear_chan;
	}

	netdev_info(ndev, "get io resource from device: %pa, size = %u\n",
		    &regs->start, (unsigned int)resource_size(regs));
	db->emac_rx_fifo = regs->start + EMAC_RX_IO_DATA_REG;

	db->rx_chan = dma_request_chan(&pdev->dev, "rx");
	if (IS_ERR(db->rx_chan)) {
		netdev_err(ndev,
			   "failed to request dma channel. dma is disabled\n");
		err = PTR_ERR(db->rx_chan);
		goto out_clear_chan;
	}

	conf.direction = DMA_DEV_TO_MEM;
	conf.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	conf.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	conf.src_addr = db->emac_rx_fifo;
	conf.dst_maxburst = 4;
	conf.src_maxburst = 4;
	conf.device_fc = false;

	err = dmaengine_slave_config(db->rx_chan, &conf);
	if (err) {
		netdev_err(ndev, "config dma slave failed\n");
		err = -EINVAL;
		goto out_slave_configure_err;
	}

	return err;

out_slave_configure_err:
	dma_release_channel(db->rx_chan);

out_clear_chan:
	db->rx_chan = NULL;
	return err;
}

 
static int emac_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct emac_board_info *db;
	struct net_device *ndev;
	int ret = 0;

	ndev = alloc_etherdev(sizeof(struct emac_board_info));
	if (!ndev) {
		dev_err(&pdev->dev, "could not allocate device.\n");
		return -ENOMEM;
	}

	SET_NETDEV_DEV(ndev, &pdev->dev);

	db = netdev_priv(ndev);

	db->dev = &pdev->dev;
	db->ndev = ndev;
	db->pdev = pdev;
	db->msg_enable = netif_msg_init(debug, EMAC_DEFAULT_MSG_ENABLE);

	spin_lock_init(&db->lock);

	db->membase = of_iomap(np, 0);
	if (!db->membase) {
		dev_err(&pdev->dev, "failed to remap registers\n");
		ret = -ENOMEM;
		goto out;
	}

	 
	ndev->base_addr = (unsigned long)db->membase;
	ndev->irq = irq_of_parse_and_map(np, 0);
	if (ndev->irq == -ENXIO) {
		netdev_err(ndev, "No irq resource\n");
		ret = ndev->irq;
		goto out_iounmap;
	}

	if (emac_configure_dma(db))
		netdev_info(ndev, "configure dma failed. disable dma.\n");

	db->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(db->clk)) {
		ret = PTR_ERR(db->clk);
		goto out_dispose_mapping;
	}

	ret = clk_prepare_enable(db->clk);
	if (ret) {
		dev_err(&pdev->dev, "Error couldn't enable clock (%d)\n", ret);
		goto out_dispose_mapping;
	}

	ret = sunxi_sram_claim(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Error couldn't map SRAM to device\n");
		goto out_clk_disable_unprepare;
	}

	db->phy_node = of_parse_phandle(np, "phy-handle", 0);
	if (!db->phy_node)
		db->phy_node = of_parse_phandle(np, "phy", 0);
	if (!db->phy_node) {
		dev_err(&pdev->dev, "no associated PHY\n");
		ret = -ENODEV;
		goto out_release_sram;
	}

	 
	ret = of_get_ethdev_address(np, ndev);
	if (ret) {
		 
		eth_hw_addr_random(ndev);
		dev_warn(&pdev->dev, "using random MAC address %pM\n",
			 ndev->dev_addr);
	}

	db->emacrx_completed_flag = 1;
	emac_powerup(ndev);
	emac_reset(db);

	ndev->netdev_ops = &emac_netdev_ops;
	ndev->watchdog_timeo = msecs_to_jiffies(watchdog);
	ndev->ethtool_ops = &emac_ethtool_ops;

	platform_set_drvdata(pdev, ndev);

	 
	netif_carrier_off(ndev);

	ret = register_netdev(ndev);
	if (ret) {
		dev_err(&pdev->dev, "Registering netdev failed!\n");
		ret = -ENODEV;
		goto out_release_sram;
	}

	dev_info(&pdev->dev, "%s: at %p, IRQ %d MAC: %pM\n",
		 ndev->name, db->membase, ndev->irq, ndev->dev_addr);

	return 0;

out_release_sram:
	sunxi_sram_release(&pdev->dev);
out_clk_disable_unprepare:
	clk_disable_unprepare(db->clk);
out_dispose_mapping:
	irq_dispose_mapping(ndev->irq);
	dma_release_channel(db->rx_chan);
out_iounmap:
	iounmap(db->membase);
out:
	dev_err(db->dev, "not found (%d).\n", ret);

	free_netdev(ndev);

	return ret;
}

static int emac_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct emac_board_info *db = netdev_priv(ndev);

	if (db->rx_chan) {
		dmaengine_terminate_all(db->rx_chan);
		dma_release_channel(db->rx_chan);
	}

	unregister_netdev(ndev);
	sunxi_sram_release(&pdev->dev);
	clk_disable_unprepare(db->clk);
	irq_dispose_mapping(ndev->irq);
	iounmap(db->membase);
	free_netdev(ndev);

	dev_dbg(&pdev->dev, "released and freed device\n");
	return 0;
}

static int emac_suspend(struct platform_device *dev, pm_message_t state)
{
	struct net_device *ndev = platform_get_drvdata(dev);

	netif_carrier_off(ndev);
	netif_device_detach(ndev);
	emac_shutdown(ndev);

	return 0;
}

static int emac_resume(struct platform_device *dev)
{
	struct net_device *ndev = platform_get_drvdata(dev);
	struct emac_board_info *db = netdev_priv(ndev);

	emac_reset(db);
	emac_init_device(ndev);
	netif_device_attach(ndev);

	return 0;
}

static const struct of_device_id emac_of_match[] = {
	{.compatible = "allwinner,sun4i-a10-emac",},

	 
	{.compatible = "allwinner,sun4i-emac",},
	{},
};

MODULE_DEVICE_TABLE(of, emac_of_match);

static struct platform_driver emac_driver = {
	.driver = {
		.name = "sun4i-emac",
		.of_match_table = emac_of_match,
	},
	.probe = emac_probe,
	.remove = emac_remove,
	.suspend = emac_suspend,
	.resume = emac_resume,
};

module_platform_driver(emac_driver);

MODULE_AUTHOR("Stefan Roese <sr@denx.de>");
MODULE_AUTHOR("Maxime Ripard <maxime.ripard@free-electrons.com>");
MODULE_DESCRIPTION("Allwinner A10 emac network driver");
MODULE_LICENSE("GPL");
