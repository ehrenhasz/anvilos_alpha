
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/crc32.h>
#include <linux/spinlock.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/phy.h>

#include <asm/processor.h>

#define DRV_NAME	"r6040"
#define DRV_VERSION	"0.29"
#define DRV_RELDATE	"04Jul2016"

 
#define TX_TIMEOUT	(6000 * HZ / 1000)

 
#define R6040_IO_SIZE	256

 
#define MAX_MAC		2

 
#define MCR0		0x00	 
#define  MCR0_RCVEN	0x0002	 
#define  MCR0_PROMISC	0x0020	 
#define  MCR0_HASH_EN	0x0100	 
#define  MCR0_XMTEN	0x1000	 
#define  MCR0_FD	0x8000	 
#define MCR1		0x04	 
#define  MAC_RST	0x0001	 
#define MBCR		0x08	 
#define MT_ICR		0x0C	 
#define MR_ICR		0x10	 
#define MTPR		0x14	 
#define  TM2TX		0x0001	 
#define MR_BSR		0x18	 
#define MR_DCR		0x1A	 
#define MLSR		0x1C	 
#define  TX_FIFO_UNDR	0x0200	 
#define	 TX_EXCEEDC	0x2000	 
#define  TX_LATEC	0x4000	 
#define MMDIO		0x20	 
#define  MDIO_WRITE	0x4000	 
#define  MDIO_READ	0x2000	 
#define MMRD		0x24	 
#define MMWD		0x28	 
#define MTD_SA0		0x2C	 
#define MTD_SA1		0x30	 
#define MRD_SA0		0x34	 
#define MRD_SA1		0x38	 
#define MISR		0x3C	 
#define MIER		0x40	 
#define  MSK_INT	0x0000	 
#define  RX_FINISH	0x0001   
#define  RX_NO_DESC	0x0002   
#define  RX_FIFO_FULL	0x0004   
#define  RX_EARLY	0x0008   
#define  TX_FINISH	0x0010   
#define  TX_EARLY	0x0080   
#define  EVENT_OVRFL	0x0100   
#define  LINK_CHANGED	0x0200   
#define ME_CISR		0x44	 
#define ME_CIER		0x48	 
#define MR_CNT		0x50	 
#define ME_CNT0		0x52	 
#define ME_CNT1		0x54	 
#define ME_CNT2		0x56	 
#define ME_CNT3		0x58	 
#define MT_CNT		0x5A	 
#define ME_CNT4		0x5C	 
#define MP_CNT		0x5E	 
#define MAR0		0x60	 
#define MAR1		0x62	 
#define MAR2		0x64	 
#define MAR3		0x66	 
#define MID_0L		0x68	 
#define MID_0M		0x6A	 
#define MID_0H		0x6C	 
#define MID_1L		0x70	 
#define MID_1M		0x72	 
#define MID_1H		0x74	 
#define MID_2L		0x78	 
#define MID_2M		0x7A	 
#define MID_2H		0x7C	 
#define MID_3L		0x80	 
#define MID_3M		0x82	 
#define MID_3H		0x84	 
#define PHY_CC		0x88	 
#define  SCEN		0x8000	 
#define  PHYAD_SHIFT	8	 
#define  TMRDIV_SHIFT	0	 
#define PHY_ST		0x8A	 
#define MAC_SM		0xAC	 
#define  MAC_SM_RST	0x0002	 
#define MD_CSC		0xb6	 
#define  MD_CSC_DEFAULT	0x0030
#define MAC_ID		0xBE	 

#define TX_DCNT		0x80	 
#define RX_DCNT		0x80	 
#define MAX_BUF_SIZE	0x600
#define RX_DESC_SIZE	(RX_DCNT * sizeof(struct r6040_descriptor))
#define TX_DESC_SIZE	(TX_DCNT * sizeof(struct r6040_descriptor))
#define MBCR_DEFAULT	0x012A	 
#define MCAST_MAX	3	 

#define MAC_DEF_TIMEOUT	2048	 

 
#define DSC_OWNER_MAC	0x8000	 
#define DSC_RX_OK	0x4000	 
#define DSC_RX_ERR	0x0800	 
#define DSC_RX_ERR_DRI	0x0400	 
#define DSC_RX_ERR_BUF	0x0200	 
#define DSC_RX_ERR_LONG	0x0100	 
#define DSC_RX_ERR_RUNT	0x0080	 
#define DSC_RX_ERR_CRC	0x0040	 
#define DSC_RX_BCAST	0x0020	 
#define DSC_RX_MCAST	0x0010	 
#define DSC_RX_MCH_HIT	0x0008	 
#define DSC_RX_MIDH_HIT	0x0004	 
#define DSC_RX_IDX_MID_MASK 3	 

MODULE_AUTHOR("Sten Wang <sten.wang@rdc.com.tw>,"
	"Daniel Gimpelevich <daniel@gimpelevich.san-francisco.ca.us>,"
	"Florian Fainelli <f.fainelli@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RDC R6040 NAPI PCI FastEthernet driver");
MODULE_VERSION(DRV_VERSION " " DRV_RELDATE);

 
#define RX_INTS			(RX_FIFO_FULL | RX_NO_DESC | RX_FINISH)
#define TX_INTS			(TX_FINISH)
#define INT_MASK		(RX_INTS | TX_INTS)

struct r6040_descriptor {
	u16	status, len;		 
	__le32	buf;			 
	__le32	ndesc;			 
	u32	rev1;			 
	char	*vbufp;			 
	struct r6040_descriptor *vndescp;	 
	struct sk_buff *skb_ptr;	 
	u32	rev2;			 
} __aligned(32);

struct r6040_private {
	spinlock_t lock;		 
	struct pci_dev *pdev;
	struct r6040_descriptor *rx_insert_ptr;
	struct r6040_descriptor *rx_remove_ptr;
	struct r6040_descriptor *tx_insert_ptr;
	struct r6040_descriptor *tx_remove_ptr;
	struct r6040_descriptor *rx_ring;
	struct r6040_descriptor *tx_ring;
	dma_addr_t rx_ring_dma;
	dma_addr_t tx_ring_dma;
	u16	tx_free_desc;
	u16	mcr0;
	struct net_device *dev;
	struct mii_bus *mii_bus;
	struct napi_struct napi;
	void __iomem *base;
	int old_link;
	int old_duplex;
};

static char version[] = DRV_NAME
	": RDC R6040 NAPI net driver,"
	"version "DRV_VERSION " (" DRV_RELDATE ")";

 
static int r6040_phy_read(void __iomem *ioaddr, int phy_addr, int reg)
{
	int limit = MAC_DEF_TIMEOUT;
	u16 cmd;

	iowrite16(MDIO_READ | reg | (phy_addr << 8), ioaddr + MMDIO);
	 
	while (limit--) {
		cmd = ioread16(ioaddr + MMDIO);
		if (!(cmd & MDIO_READ))
			break;
		udelay(1);
	}

	if (limit < 0)
		return -ETIMEDOUT;

	return ioread16(ioaddr + MMRD);
}

 
static int r6040_phy_write(void __iomem *ioaddr,
					int phy_addr, int reg, u16 val)
{
	int limit = MAC_DEF_TIMEOUT;
	u16 cmd;

	iowrite16(val, ioaddr + MMWD);
	 
	iowrite16(MDIO_WRITE | reg | (phy_addr << 8), ioaddr + MMDIO);
	 
	while (limit--) {
		cmd = ioread16(ioaddr + MMDIO);
		if (!(cmd & MDIO_WRITE))
			break;
		udelay(1);
	}

	return (limit < 0) ? -ETIMEDOUT : 0;
}

static int r6040_mdiobus_read(struct mii_bus *bus, int phy_addr, int reg)
{
	struct net_device *dev = bus->priv;
	struct r6040_private *lp = netdev_priv(dev);
	void __iomem *ioaddr = lp->base;

	return r6040_phy_read(ioaddr, phy_addr, reg);
}

static int r6040_mdiobus_write(struct mii_bus *bus, int phy_addr,
						int reg, u16 value)
{
	struct net_device *dev = bus->priv;
	struct r6040_private *lp = netdev_priv(dev);
	void __iomem *ioaddr = lp->base;

	return r6040_phy_write(ioaddr, phy_addr, reg, value);
}

static void r6040_free_txbufs(struct net_device *dev)
{
	struct r6040_private *lp = netdev_priv(dev);
	int i;

	for (i = 0; i < TX_DCNT; i++) {
		if (lp->tx_insert_ptr->skb_ptr) {
			dma_unmap_single(&lp->pdev->dev,
					 le32_to_cpu(lp->tx_insert_ptr->buf),
					 MAX_BUF_SIZE, DMA_TO_DEVICE);
			dev_kfree_skb(lp->tx_insert_ptr->skb_ptr);
			lp->tx_insert_ptr->skb_ptr = NULL;
		}
		lp->tx_insert_ptr = lp->tx_insert_ptr->vndescp;
	}
}

static void r6040_free_rxbufs(struct net_device *dev)
{
	struct r6040_private *lp = netdev_priv(dev);
	int i;

	for (i = 0; i < RX_DCNT; i++) {
		if (lp->rx_insert_ptr->skb_ptr) {
			dma_unmap_single(&lp->pdev->dev,
					 le32_to_cpu(lp->rx_insert_ptr->buf),
					 MAX_BUF_SIZE, DMA_FROM_DEVICE);
			dev_kfree_skb(lp->rx_insert_ptr->skb_ptr);
			lp->rx_insert_ptr->skb_ptr = NULL;
		}
		lp->rx_insert_ptr = lp->rx_insert_ptr->vndescp;
	}
}

static void r6040_init_ring_desc(struct r6040_descriptor *desc_ring,
				 dma_addr_t desc_dma, int size)
{
	struct r6040_descriptor *desc = desc_ring;
	dma_addr_t mapping = desc_dma;

	while (size-- > 0) {
		mapping += sizeof(*desc);
		desc->ndesc = cpu_to_le32(mapping);
		desc->vndescp = desc + 1;
		desc++;
	}
	desc--;
	desc->ndesc = cpu_to_le32(desc_dma);
	desc->vndescp = desc_ring;
}

static void r6040_init_txbufs(struct net_device *dev)
{
	struct r6040_private *lp = netdev_priv(dev);

	lp->tx_free_desc = TX_DCNT;

	lp->tx_remove_ptr = lp->tx_insert_ptr = lp->tx_ring;
	r6040_init_ring_desc(lp->tx_ring, lp->tx_ring_dma, TX_DCNT);
}

static int r6040_alloc_rxbufs(struct net_device *dev)
{
	struct r6040_private *lp = netdev_priv(dev);
	struct r6040_descriptor *desc;
	struct sk_buff *skb;
	int rc;

	lp->rx_remove_ptr = lp->rx_insert_ptr = lp->rx_ring;
	r6040_init_ring_desc(lp->rx_ring, lp->rx_ring_dma, RX_DCNT);

	 
	desc = lp->rx_ring;
	do {
		skb = netdev_alloc_skb(dev, MAX_BUF_SIZE);
		if (!skb) {
			rc = -ENOMEM;
			goto err_exit;
		}
		desc->skb_ptr = skb;
		desc->buf = cpu_to_le32(dma_map_single(&lp->pdev->dev,
						       desc->skb_ptr->data,
						       MAX_BUF_SIZE,
						       DMA_FROM_DEVICE));
		desc->status = DSC_OWNER_MAC;
		desc = desc->vndescp;
	} while (desc != lp->rx_ring);

	return 0;

err_exit:
	 
	r6040_free_rxbufs(dev);
	return rc;
}

static void r6040_reset_mac(struct r6040_private *lp)
{
	void __iomem *ioaddr = lp->base;
	int limit = MAC_DEF_TIMEOUT;
	u16 cmd, md_csc;

	md_csc = ioread16(ioaddr + MD_CSC);
	iowrite16(MAC_RST, ioaddr + MCR1);
	while (limit--) {
		cmd = ioread16(ioaddr + MCR1);
		if (cmd & MAC_RST)
			break;
	}

	 
	iowrite16(MAC_SM_RST, ioaddr + MAC_SM);
	iowrite16(0, ioaddr + MAC_SM);
	mdelay(5);

	 
	if (md_csc != MD_CSC_DEFAULT)
		iowrite16(md_csc, ioaddr + MD_CSC);
}

static void r6040_init_mac_regs(struct net_device *dev)
{
	struct r6040_private *lp = netdev_priv(dev);
	void __iomem *ioaddr = lp->base;

	 
	iowrite16(MSK_INT, ioaddr + MIER);

	 
	r6040_reset_mac(lp);

	 
	iowrite16(MBCR_DEFAULT, ioaddr + MBCR);

	 
	iowrite16(MAX_BUF_SIZE, ioaddr + MR_BSR);

	 
	iowrite16(lp->tx_ring_dma, ioaddr + MTD_SA0);
	iowrite16(lp->tx_ring_dma >> 16, ioaddr + MTD_SA1);

	 
	iowrite16(lp->rx_ring_dma, ioaddr + MRD_SA0);
	iowrite16(lp->rx_ring_dma >> 16, ioaddr + MRD_SA1);

	 
	iowrite16(0, ioaddr + MT_ICR);
	iowrite16(0, ioaddr + MR_ICR);

	 
	iowrite16(INT_MASK, ioaddr + MIER);

	 
	iowrite16(lp->mcr0 | MCR0_RCVEN, ioaddr);

	 
	iowrite16(TM2TX, ioaddr + MTPR);
}

static void r6040_tx_timeout(struct net_device *dev, unsigned int txqueue)
{
	struct r6040_private *priv = netdev_priv(dev);
	void __iomem *ioaddr = priv->base;

	netdev_warn(dev, "transmit timed out, int enable %4.4x "
		"status %4.4x\n",
		ioread16(ioaddr + MIER),
		ioread16(ioaddr + MISR));

	dev->stats.tx_errors++;

	 
	r6040_init_mac_regs(dev);
}

static struct net_device_stats *r6040_get_stats(struct net_device *dev)
{
	struct r6040_private *priv = netdev_priv(dev);
	void __iomem *ioaddr = priv->base;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	dev->stats.rx_crc_errors += ioread8(ioaddr + ME_CNT1);
	dev->stats.multicast += ioread8(ioaddr + ME_CNT0);
	spin_unlock_irqrestore(&priv->lock, flags);

	return &dev->stats;
}

 
static void r6040_down(struct net_device *dev)
{
	struct r6040_private *lp = netdev_priv(dev);
	void __iomem *ioaddr = lp->base;
	const u16 *adrp;

	 
	iowrite16(MSK_INT, ioaddr + MIER);	 

	 
	r6040_reset_mac(lp);

	 
	adrp = (const u16 *) dev->dev_addr;
	iowrite16(adrp[0], ioaddr + MID_0L);
	iowrite16(adrp[1], ioaddr + MID_0M);
	iowrite16(adrp[2], ioaddr + MID_0H);
}

static int r6040_close(struct net_device *dev)
{
	struct r6040_private *lp = netdev_priv(dev);
	struct pci_dev *pdev = lp->pdev;

	phy_stop(dev->phydev);
	napi_disable(&lp->napi);
	netif_stop_queue(dev);

	spin_lock_irq(&lp->lock);
	r6040_down(dev);

	 
	r6040_free_rxbufs(dev);

	 
	r6040_free_txbufs(dev);

	spin_unlock_irq(&lp->lock);

	free_irq(dev->irq, dev);

	 
	if (lp->rx_ring) {
		dma_free_coherent(&pdev->dev, RX_DESC_SIZE, lp->rx_ring,
				  lp->rx_ring_dma);
		lp->rx_ring = NULL;
	}

	if (lp->tx_ring) {
		dma_free_coherent(&pdev->dev, TX_DESC_SIZE, lp->tx_ring,
				  lp->tx_ring_dma);
		lp->tx_ring = NULL;
	}

	return 0;
}

static int r6040_rx(struct net_device *dev, int limit)
{
	struct r6040_private *priv = netdev_priv(dev);
	struct r6040_descriptor *descptr = priv->rx_remove_ptr;
	struct sk_buff *skb_ptr, *new_skb;
	int count = 0;
	u16 err;

	 
	while (count < limit && !(descptr->status & DSC_OWNER_MAC)) {
		 
		err = descptr->status;
		 
		if (err & DSC_RX_ERR) {
			 
			if (err & DSC_RX_ERR_DRI)
				dev->stats.rx_frame_errors++;
			 
			if (err & DSC_RX_ERR_BUF)
				dev->stats.rx_length_errors++;
			 
			if (err & DSC_RX_ERR_LONG)
				dev->stats.rx_length_errors++;
			 
			if (err & DSC_RX_ERR_RUNT)
				dev->stats.rx_length_errors++;
			 
			if (err & DSC_RX_ERR_CRC) {
				spin_lock(&priv->lock);
				dev->stats.rx_crc_errors++;
				spin_unlock(&priv->lock);
			}
			goto next_descr;
		}

		 
		new_skb = netdev_alloc_skb(dev, MAX_BUF_SIZE);
		if (!new_skb) {
			dev->stats.rx_dropped++;
			goto next_descr;
		}
		skb_ptr = descptr->skb_ptr;
		skb_ptr->dev = priv->dev;

		 
		skb_put(skb_ptr, descptr->len - ETH_FCS_LEN);
		dma_unmap_single(&priv->pdev->dev, le32_to_cpu(descptr->buf),
				 MAX_BUF_SIZE, DMA_FROM_DEVICE);
		skb_ptr->protocol = eth_type_trans(skb_ptr, priv->dev);

		 
		netif_receive_skb(skb_ptr);
		dev->stats.rx_packets++;
		dev->stats.rx_bytes += descptr->len - ETH_FCS_LEN;

		 
		descptr->skb_ptr = new_skb;
		descptr->buf = cpu_to_le32(dma_map_single(&priv->pdev->dev,
							  descptr->skb_ptr->data,
							  MAX_BUF_SIZE,
							  DMA_FROM_DEVICE));

next_descr:
		 
		descptr->status = DSC_OWNER_MAC;
		descptr = descptr->vndescp;
		count++;
	}
	priv->rx_remove_ptr = descptr;

	return count;
}

static void r6040_tx(struct net_device *dev)
{
	struct r6040_private *priv = netdev_priv(dev);
	struct r6040_descriptor *descptr;
	void __iomem *ioaddr = priv->base;
	struct sk_buff *skb_ptr;
	u16 err;

	spin_lock(&priv->lock);
	descptr = priv->tx_remove_ptr;
	while (priv->tx_free_desc < TX_DCNT) {
		 
		err = ioread16(ioaddr + MLSR);

		if (err & TX_FIFO_UNDR)
			dev->stats.tx_fifo_errors++;
		if (err & (TX_EXCEEDC | TX_LATEC))
			dev->stats.tx_carrier_errors++;

		if (descptr->status & DSC_OWNER_MAC)
			break;  
		skb_ptr = descptr->skb_ptr;

		 
		dev->stats.tx_packets++;
		dev->stats.tx_bytes += skb_ptr->len;

		dma_unmap_single(&priv->pdev->dev, le32_to_cpu(descptr->buf),
				 skb_ptr->len, DMA_TO_DEVICE);
		 
		dev_kfree_skb(skb_ptr);
		descptr->skb_ptr = NULL;
		 
		descptr = descptr->vndescp;
		priv->tx_free_desc++;
	}
	priv->tx_remove_ptr = descptr;

	if (priv->tx_free_desc)
		netif_wake_queue(dev);
	spin_unlock(&priv->lock);
}

static int r6040_poll(struct napi_struct *napi, int budget)
{
	struct r6040_private *priv =
		container_of(napi, struct r6040_private, napi);
	struct net_device *dev = priv->dev;
	void __iomem *ioaddr = priv->base;
	int work_done;

	r6040_tx(dev);

	work_done = r6040_rx(dev, budget);

	if (work_done < budget) {
		napi_complete_done(napi, work_done);
		 
		iowrite16(ioread16(ioaddr + MIER) | RX_INTS | TX_INTS,
			  ioaddr + MIER);
	}
	return work_done;
}

 
static irqreturn_t r6040_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct r6040_private *lp = netdev_priv(dev);
	void __iomem *ioaddr = lp->base;
	u16 misr, status;

	 
	misr = ioread16(ioaddr + MIER);
	 
	iowrite16(MSK_INT, ioaddr + MIER);
	 
	status = ioread16(ioaddr + MISR);

	if (status == 0x0000 || status == 0xffff) {
		 
		iowrite16(misr, ioaddr + MIER);
		return IRQ_NONE;
	}

	 
	if (status & (RX_INTS | TX_INTS)) {
		if (status & RX_NO_DESC) {
			 
			dev->stats.rx_dropped++;
			dev->stats.rx_missed_errors++;
		}
		if (status & RX_FIFO_FULL)
			dev->stats.rx_fifo_errors++;

		if (likely(napi_schedule_prep(&lp->napi))) {
			 
			misr &= ~(RX_INTS | TX_INTS);
			__napi_schedule_irqoff(&lp->napi);
		}
	}

	 
	iowrite16(misr, ioaddr + MIER);

	return IRQ_HANDLED;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void r6040_poll_controller(struct net_device *dev)
{
	disable_irq(dev->irq);
	r6040_interrupt(dev->irq, dev);
	enable_irq(dev->irq);
}
#endif

 
static int r6040_up(struct net_device *dev)
{
	struct r6040_private *lp = netdev_priv(dev);
	void __iomem *ioaddr = lp->base;
	int ret;

	 
	r6040_init_txbufs(dev);
	ret = r6040_alloc_rxbufs(dev);
	if (ret)
		return ret;

	 
	r6040_phy_write(ioaddr, 30, 17,
			(r6040_phy_read(ioaddr, 30, 17) | 0x4000));
	r6040_phy_write(ioaddr, 30, 17,
			~((~r6040_phy_read(ioaddr, 30, 17)) | 0x2000));
	r6040_phy_write(ioaddr, 0, 19, 0x0000);
	r6040_phy_write(ioaddr, 0, 30, 0x01F0);

	 
	r6040_init_mac_regs(dev);

	phy_start(dev->phydev);

	return 0;
}


 
static void r6040_mac_address(struct net_device *dev)
{
	struct r6040_private *lp = netdev_priv(dev);
	void __iomem *ioaddr = lp->base;
	const u16 *adrp;

	 
	r6040_reset_mac(lp);

	 
	adrp = (const u16 *) dev->dev_addr;
	iowrite16(adrp[0], ioaddr + MID_0L);
	iowrite16(adrp[1], ioaddr + MID_0M);
	iowrite16(adrp[2], ioaddr + MID_0H);
}

static int r6040_open(struct net_device *dev)
{
	struct r6040_private *lp = netdev_priv(dev);
	int ret;

	 
	ret = request_irq(dev->irq, r6040_interrupt,
		IRQF_SHARED, dev->name, dev);
	if (ret)
		goto out;

	 
	r6040_mac_address(dev);

	 
	lp->rx_ring =
		dma_alloc_coherent(&lp->pdev->dev, RX_DESC_SIZE,
				   &lp->rx_ring_dma, GFP_KERNEL);
	if (!lp->rx_ring) {
		ret = -ENOMEM;
		goto err_free_irq;
	}

	lp->tx_ring =
		dma_alloc_coherent(&lp->pdev->dev, TX_DESC_SIZE,
				   &lp->tx_ring_dma, GFP_KERNEL);
	if (!lp->tx_ring) {
		ret = -ENOMEM;
		goto err_free_rx_ring;
	}

	ret = r6040_up(dev);
	if (ret)
		goto err_free_tx_ring;

	napi_enable(&lp->napi);
	netif_start_queue(dev);

	return 0;

err_free_tx_ring:
	dma_free_coherent(&lp->pdev->dev, TX_DESC_SIZE, lp->tx_ring,
			  lp->tx_ring_dma);
err_free_rx_ring:
	dma_free_coherent(&lp->pdev->dev, RX_DESC_SIZE, lp->rx_ring,
			  lp->rx_ring_dma);
err_free_irq:
	free_irq(dev->irq, dev);
out:
	return ret;
}

static netdev_tx_t r6040_start_xmit(struct sk_buff *skb,
				    struct net_device *dev)
{
	struct r6040_private *lp = netdev_priv(dev);
	struct r6040_descriptor *descptr;
	void __iomem *ioaddr = lp->base;
	unsigned long flags;

	if (skb_put_padto(skb, ETH_ZLEN) < 0)
		return NETDEV_TX_OK;

	 
	spin_lock_irqsave(&lp->lock, flags);

	 
	if (!lp->tx_free_desc) {
		spin_unlock_irqrestore(&lp->lock, flags);
		netif_stop_queue(dev);
		netdev_err(dev, ": no tx descriptor\n");
		return NETDEV_TX_BUSY;
	}

	 
	lp->tx_free_desc--;
	descptr = lp->tx_insert_ptr;
	descptr->len = skb->len;
	descptr->skb_ptr = skb;
	descptr->buf = cpu_to_le32(dma_map_single(&lp->pdev->dev, skb->data,
						  skb->len, DMA_TO_DEVICE));
	descptr->status = DSC_OWNER_MAC;

	skb_tx_timestamp(skb);

	 
	if (!netdev_xmit_more() || netif_queue_stopped(dev))
		iowrite16(TM2TX, ioaddr + MTPR);
	lp->tx_insert_ptr = descptr->vndescp;

	 
	if (!lp->tx_free_desc)
		netif_stop_queue(dev);

	spin_unlock_irqrestore(&lp->lock, flags);

	return NETDEV_TX_OK;
}

static void r6040_multicast_list(struct net_device *dev)
{
	struct r6040_private *lp = netdev_priv(dev);
	void __iomem *ioaddr = lp->base;
	unsigned long flags;
	struct netdev_hw_addr *ha;
	int i;
	const u16 *adrp;
	u16 hash_table[4] = { 0 };

	spin_lock_irqsave(&lp->lock, flags);

	 
	adrp = (const u16 *)dev->dev_addr;
	iowrite16(adrp[0], ioaddr + MID_0L);
	iowrite16(adrp[1], ioaddr + MID_0M);
	iowrite16(adrp[2], ioaddr + MID_0H);

	 
	lp->mcr0 = ioread16(ioaddr + MCR0) & ~(MCR0_PROMISC | MCR0_HASH_EN);

	 
	if (dev->flags & IFF_PROMISC)
		lp->mcr0 |= MCR0_PROMISC;

	 
	else if (dev->flags & IFF_ALLMULTI) {
		lp->mcr0 |= MCR0_HASH_EN;

		for (i = 0; i < MCAST_MAX ; i++) {
			iowrite16(0, ioaddr + MID_1L + 8 * i);
			iowrite16(0, ioaddr + MID_1M + 8 * i);
			iowrite16(0, ioaddr + MID_1H + 8 * i);
		}

		for (i = 0; i < 4; i++)
			hash_table[i] = 0xffff;
	}
	 
	else if (netdev_mc_count(dev) <= MCAST_MAX) {
		i = 0;
		netdev_for_each_mc_addr(ha, dev) {
			u16 *adrp = (u16 *) ha->addr;
			iowrite16(adrp[0], ioaddr + MID_1L + 8 * i);
			iowrite16(adrp[1], ioaddr + MID_1M + 8 * i);
			iowrite16(adrp[2], ioaddr + MID_1H + 8 * i);
			i++;
		}
		while (i < MCAST_MAX) {
			iowrite16(0, ioaddr + MID_1L + 8 * i);
			iowrite16(0, ioaddr + MID_1M + 8 * i);
			iowrite16(0, ioaddr + MID_1H + 8 * i);
			i++;
		}
	}
	 
	else {
		u32 crc;

		lp->mcr0 |= MCR0_HASH_EN;

		for (i = 0; i < MCAST_MAX ; i++) {
			iowrite16(0, ioaddr + MID_1L + 8 * i);
			iowrite16(0, ioaddr + MID_1M + 8 * i);
			iowrite16(0, ioaddr + MID_1H + 8 * i);
		}

		 
		netdev_for_each_mc_addr(ha, dev) {
			u8 *addrs = ha->addr;

			crc = ether_crc(ETH_ALEN, addrs);
			crc >>= 26;
			hash_table[crc >> 4] |= 1 << (crc & 0xf);
		}
	}

	iowrite16(lp->mcr0, ioaddr + MCR0);

	 
	if (lp->mcr0 & MCR0_HASH_EN) {
		iowrite16(hash_table[0], ioaddr + MAR0);
		iowrite16(hash_table[1], ioaddr + MAR1);
		iowrite16(hash_table[2], ioaddr + MAR2);
		iowrite16(hash_table[3], ioaddr + MAR3);
	}

	spin_unlock_irqrestore(&lp->lock, flags);
}

static void netdev_get_drvinfo(struct net_device *dev,
			struct ethtool_drvinfo *info)
{
	struct r6040_private *rp = netdev_priv(dev);

	strscpy(info->driver, DRV_NAME, sizeof(info->driver));
	strscpy(info->version, DRV_VERSION, sizeof(info->version));
	strscpy(info->bus_info, pci_name(rp->pdev), sizeof(info->bus_info));
}

static const struct ethtool_ops netdev_ethtool_ops = {
	.get_drvinfo		= netdev_get_drvinfo,
	.get_link		= ethtool_op_get_link,
	.get_ts_info		= ethtool_op_get_ts_info,
	.get_link_ksettings     = phy_ethtool_get_link_ksettings,
	.set_link_ksettings     = phy_ethtool_set_link_ksettings,
	.nway_reset		= phy_ethtool_nway_reset,
};

static const struct net_device_ops r6040_netdev_ops = {
	.ndo_open		= r6040_open,
	.ndo_stop		= r6040_close,
	.ndo_start_xmit		= r6040_start_xmit,
	.ndo_get_stats		= r6040_get_stats,
	.ndo_set_rx_mode	= r6040_multicast_list,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address	= eth_mac_addr,
	.ndo_eth_ioctl		= phy_do_ioctl,
	.ndo_tx_timeout		= r6040_tx_timeout,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= r6040_poll_controller,
#endif
};

static void r6040_adjust_link(struct net_device *dev)
{
	struct r6040_private *lp = netdev_priv(dev);
	struct phy_device *phydev = dev->phydev;
	int status_changed = 0;
	void __iomem *ioaddr = lp->base;

	BUG_ON(!phydev);

	if (lp->old_link != phydev->link) {
		status_changed = 1;
		lp->old_link = phydev->link;
	}

	 
	if (phydev->link && (lp->old_duplex != phydev->duplex)) {
		lp->mcr0 |= (phydev->duplex == DUPLEX_FULL ? MCR0_FD : 0);
		iowrite16(lp->mcr0, ioaddr);

		status_changed = 1;
		lp->old_duplex = phydev->duplex;
	}

	if (status_changed)
		phy_print_status(phydev);
}

static int r6040_mii_probe(struct net_device *dev)
{
	struct r6040_private *lp = netdev_priv(dev);
	struct phy_device *phydev = NULL;

	phydev = phy_find_first(lp->mii_bus);
	if (!phydev) {
		dev_err(&lp->pdev->dev, "no PHY found\n");
		return -ENODEV;
	}

	phydev = phy_connect(dev, phydev_name(phydev), &r6040_adjust_link,
			     PHY_INTERFACE_MODE_MII);

	if (IS_ERR(phydev)) {
		dev_err(&lp->pdev->dev, "could not attach to PHY\n");
		return PTR_ERR(phydev);
	}

	phy_set_max_speed(phydev, SPEED_100);

	lp->old_link = 0;
	lp->old_duplex = -1;

	phy_attached_info(phydev);

	return 0;
}

static int r6040_init_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct net_device *dev;
	struct r6040_private *lp;
	void __iomem *ioaddr;
	int err, io_size = R6040_IO_SIZE;
	static int card_idx = -1;
	u16 addr[ETH_ALEN / 2];
	int bar = 0;

	pr_info("%s\n", version);

	err = pci_enable_device(pdev);
	if (err)
		goto err_out;

	 
	err = dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));
	if (err) {
		dev_err(&pdev->dev, "32-bit PCI DMA addresses not supported by the card\n");
		goto err_out_disable_dev;
	}
	err = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
	if (err) {
		dev_err(&pdev->dev, "32-bit PCI DMA addresses not supported by the card\n");
		goto err_out_disable_dev;
	}

	 
	if (pci_resource_len(pdev, bar) < io_size) {
		dev_err(&pdev->dev, "Insufficient PCI resources, aborting\n");
		err = -EIO;
		goto err_out_disable_dev;
	}

	pci_set_master(pdev);

	dev = alloc_etherdev(sizeof(struct r6040_private));
	if (!dev) {
		err = -ENOMEM;
		goto err_out_disable_dev;
	}
	SET_NETDEV_DEV(dev, &pdev->dev);
	lp = netdev_priv(dev);

	err = pci_request_regions(pdev, DRV_NAME);

	if (err) {
		dev_err(&pdev->dev, "Failed to request PCI regions\n");
		goto err_out_free_dev;
	}

	ioaddr = pci_iomap(pdev, bar, io_size);
	if (!ioaddr) {
		dev_err(&pdev->dev, "ioremap failed for device\n");
		err = -EIO;
		goto err_out_free_res;
	}

	 
	if (ioread16(ioaddr + PHY_CC) == 0)
		iowrite16(SCEN | PHY_MAX_ADDR << PHYAD_SHIFT |
				7 << TMRDIV_SHIFT, ioaddr + PHY_CC);

	 
	lp->base = ioaddr;
	dev->irq = pdev->irq;

	spin_lock_init(&lp->lock);
	pci_set_drvdata(pdev, dev);

	 
	card_idx++;

	addr[0] = ioread16(ioaddr + MID_0L);
	addr[1] = ioread16(ioaddr + MID_0M);
	addr[2] = ioread16(ioaddr + MID_0H);
	eth_hw_addr_set(dev, (u8 *)addr);

	 
	if (!(addr[0] || addr[1] || addr[2])) {
		netdev_warn(dev, "MAC address not initialized, "
					"generating random\n");
		eth_hw_addr_random(dev);
	}

	 
	lp->pdev = pdev;
	lp->dev = dev;

	 
	lp->mcr0 = MCR0_XMTEN | MCR0_RCVEN;

	 
	dev->netdev_ops = &r6040_netdev_ops;
	dev->ethtool_ops = &netdev_ethtool_ops;
	dev->watchdog_timeo = TX_TIMEOUT;

	netif_napi_add(dev, &lp->napi, r6040_poll);

	lp->mii_bus = mdiobus_alloc();
	if (!lp->mii_bus) {
		dev_err(&pdev->dev, "mdiobus_alloc() failed\n");
		err = -ENOMEM;
		goto err_out_unmap;
	}

	lp->mii_bus->priv = dev;
	lp->mii_bus->read = r6040_mdiobus_read;
	lp->mii_bus->write = r6040_mdiobus_write;
	lp->mii_bus->name = "r6040_eth_mii";
	snprintf(lp->mii_bus->id, MII_BUS_ID_SIZE, "%s-%x",
		dev_name(&pdev->dev), card_idx);

	err = mdiobus_register(lp->mii_bus);
	if (err) {
		dev_err(&pdev->dev, "failed to register MII bus\n");
		goto err_out_mdio;
	}

	err = r6040_mii_probe(dev);
	if (err) {
		dev_err(&pdev->dev, "failed to probe MII bus\n");
		goto err_out_mdio_unregister;
	}

	 
	err = register_netdev(dev);
	if (err) {
		dev_err(&pdev->dev, "Failed to register net device\n");
		goto err_out_phy_disconnect;
	}
	return 0;

err_out_phy_disconnect:
	phy_disconnect(dev->phydev);
err_out_mdio_unregister:
	mdiobus_unregister(lp->mii_bus);
err_out_mdio:
	mdiobus_free(lp->mii_bus);
err_out_unmap:
	netif_napi_del(&lp->napi);
	pci_iounmap(pdev, ioaddr);
err_out_free_res:
	pci_release_regions(pdev);
err_out_free_dev:
	free_netdev(dev);
err_out_disable_dev:
	pci_disable_device(pdev);
err_out:
	return err;
}

static void r6040_remove_one(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct r6040_private *lp = netdev_priv(dev);

	unregister_netdev(dev);
	phy_disconnect(dev->phydev);
	mdiobus_unregister(lp->mii_bus);
	mdiobus_free(lp->mii_bus);
	netif_napi_del(&lp->napi);
	pci_iounmap(pdev, lp->base);
	pci_release_regions(pdev);
	free_netdev(dev);
	pci_disable_device(pdev);
}


static const struct pci_device_id r6040_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_RDC, 0x6040) },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, r6040_pci_tbl);

static struct pci_driver r6040_driver = {
	.name		= DRV_NAME,
	.id_table	= r6040_pci_tbl,
	.probe		= r6040_init_one,
	.remove		= r6040_remove_one,
};

module_pci_driver(r6040_driver);
