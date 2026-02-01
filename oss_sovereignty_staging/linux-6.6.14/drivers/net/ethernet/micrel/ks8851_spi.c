
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/cache.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/regulator/consumer.h>

#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_net.h>

#include "ks8851.h"

static int msg_enable;

 
struct ks8851_net_spi {
	struct ks8851_net	ks8851;
	struct mutex		lock;
	struct work_struct	tx_work;
	struct spi_device	*spidev;
	struct spi_message	spi_msg1;
	struct spi_message	spi_msg2;
	struct spi_transfer	spi_xfer1;
	struct spi_transfer	spi_xfer2[2];
};

#define to_ks8851_spi(ks) container_of((ks), struct ks8851_net_spi, ks8851)

 
#define KS_SPIOP_RD	0x00
#define KS_SPIOP_WR	0x40
#define KS_SPIOP_RXFIFO	0x80
#define KS_SPIOP_TXFIFO	0xC0

 
#define BYTE_EN(_x)	((_x) << 2)

 
#define MK_OP(_byteen, _reg)	\
	(BYTE_EN(_byteen) | (_reg) << (8 + 2) | (_reg) >> 6)

 
static void ks8851_lock_spi(struct ks8851_net *ks, unsigned long *flags)
{
	struct ks8851_net_spi *kss = to_ks8851_spi(ks);

	mutex_lock(&kss->lock);
}

 
static void ks8851_unlock_spi(struct ks8851_net *ks, unsigned long *flags)
{
	struct ks8851_net_spi *kss = to_ks8851_spi(ks);

	mutex_unlock(&kss->lock);
}

 

 
static void ks8851_wrreg16_spi(struct ks8851_net *ks, unsigned int reg,
			       unsigned int val)
{
	struct ks8851_net_spi *kss = to_ks8851_spi(ks);
	struct spi_transfer *xfer = &kss->spi_xfer1;
	struct spi_message *msg = &kss->spi_msg1;
	__le16 txb[2];
	int ret;

	txb[0] = cpu_to_le16(MK_OP(reg & 2 ? 0xC : 0x03, reg) | KS_SPIOP_WR);
	txb[1] = cpu_to_le16(val);

	xfer->tx_buf = txb;
	xfer->rx_buf = NULL;
	xfer->len = 4;

	ret = spi_sync(kss->spidev, msg);
	if (ret < 0)
		netdev_err(ks->netdev, "spi_sync() failed\n");
}

 
static void ks8851_rdreg(struct ks8851_net *ks, unsigned int op,
			 u8 *rxb, unsigned int rxl)
{
	struct ks8851_net_spi *kss = to_ks8851_spi(ks);
	struct spi_transfer *xfer;
	struct spi_message *msg;
	__le16 *txb = (__le16 *)ks->txd;
	u8 *trx = ks->rxd;
	int ret;

	txb[0] = cpu_to_le16(op | KS_SPIOP_RD);

	if (kss->spidev->master->flags & SPI_MASTER_HALF_DUPLEX) {
		msg = &kss->spi_msg2;
		xfer = kss->spi_xfer2;

		xfer->tx_buf = txb;
		xfer->rx_buf = NULL;
		xfer->len = 2;

		xfer++;
		xfer->tx_buf = NULL;
		xfer->rx_buf = trx;
		xfer->len = rxl;
	} else {
		msg = &kss->spi_msg1;
		xfer = &kss->spi_xfer1;

		xfer->tx_buf = txb;
		xfer->rx_buf = trx;
		xfer->len = rxl + 2;
	}

	ret = spi_sync(kss->spidev, msg);
	if (ret < 0)
		netdev_err(ks->netdev, "read: spi_sync() failed\n");
	else if (kss->spidev->master->flags & SPI_MASTER_HALF_DUPLEX)
		memcpy(rxb, trx, rxl);
	else
		memcpy(rxb, trx + 2, rxl);
}

 
static unsigned int ks8851_rdreg16_spi(struct ks8851_net *ks, unsigned int reg)
{
	__le16 rx = 0;

	ks8851_rdreg(ks, MK_OP(reg & 2 ? 0xC : 0x3, reg), (u8 *)&rx, 2);
	return le16_to_cpu(rx);
}

 
static void ks8851_rdfifo_spi(struct ks8851_net *ks, u8 *buff, unsigned int len)
{
	struct ks8851_net_spi *kss = to_ks8851_spi(ks);
	struct spi_transfer *xfer = kss->spi_xfer2;
	struct spi_message *msg = &kss->spi_msg2;
	u8 txb[1];
	int ret;

	netif_dbg(ks, rx_status, ks->netdev,
		  "%s: %d@%p\n", __func__, len, buff);

	 
	txb[0] = KS_SPIOP_RXFIFO;

	xfer->tx_buf = txb;
	xfer->rx_buf = NULL;
	xfer->len = 1;

	xfer++;
	xfer->rx_buf = buff;
	xfer->tx_buf = NULL;
	xfer->len = len;

	ret = spi_sync(kss->spidev, msg);
	if (ret < 0)
		netdev_err(ks->netdev, "%s: spi_sync() failed\n", __func__);
}

 
static void ks8851_wrfifo_spi(struct ks8851_net *ks, struct sk_buff *txp,
			      bool irq)
{
	struct ks8851_net_spi *kss = to_ks8851_spi(ks);
	struct spi_transfer *xfer = kss->spi_xfer2;
	struct spi_message *msg = &kss->spi_msg2;
	unsigned int fid = 0;
	int ret;

	netif_dbg(ks, tx_queued, ks->netdev, "%s: skb %p, %d@%p, irq %d\n",
		  __func__, txp, txp->len, txp->data, irq);

	fid = ks->fid++;
	fid &= TXFR_TXFID_MASK;

	if (irq)
		fid |= TXFR_TXIC;	 

	 
	ks->txh.txb[1] = KS_SPIOP_TXFIFO;
	ks->txh.txw[1] = cpu_to_le16(fid);
	ks->txh.txw[2] = cpu_to_le16(txp->len);

	xfer->tx_buf = &ks->txh.txb[1];
	xfer->rx_buf = NULL;
	xfer->len = 5;

	xfer++;
	xfer->tx_buf = txp->data;
	xfer->rx_buf = NULL;
	xfer->len = ALIGN(txp->len, 4);

	ret = spi_sync(kss->spidev, msg);
	if (ret < 0)
		netdev_err(ks->netdev, "%s: spi_sync() failed\n", __func__);
}

 
static unsigned int calc_txlen(unsigned int len)
{
	return ALIGN(len + 4, 4);
}

 
static void ks8851_rx_skb_spi(struct ks8851_net *ks, struct sk_buff *skb)
{
	netif_rx(skb);
}

 
static void ks8851_tx_work(struct work_struct *work)
{
	unsigned int dequeued_len = 0;
	struct ks8851_net_spi *kss;
	unsigned short tx_space;
	struct ks8851_net *ks;
	unsigned long flags;
	struct sk_buff *txb;
	bool last;

	kss = container_of(work, struct ks8851_net_spi, tx_work);
	ks = &kss->ks8851;
	last = skb_queue_empty(&ks->txq);

	ks8851_lock_spi(ks, &flags);

	while (!last) {
		txb = skb_dequeue(&ks->txq);
		last = skb_queue_empty(&ks->txq);

		if (txb) {
			dequeued_len += calc_txlen(txb->len);

			ks8851_wrreg16_spi(ks, KS_RXQCR,
					   ks->rc_rxqcr | RXQCR_SDA);
			ks8851_wrfifo_spi(ks, txb, last);
			ks8851_wrreg16_spi(ks, KS_RXQCR, ks->rc_rxqcr);
			ks8851_wrreg16_spi(ks, KS_TXQCR, TXQCR_METFE);

			ks8851_done_tx(ks, txb);
		}
	}

	tx_space = ks8851_rdreg16_spi(ks, KS_TXMIR);

	spin_lock(&ks->statelock);
	ks->queued_len -= dequeued_len;
	ks->tx_space = tx_space;
	spin_unlock(&ks->statelock);

	ks8851_unlock_spi(ks, &flags);
}

 
static void ks8851_flush_tx_work_spi(struct ks8851_net *ks)
{
	struct ks8851_net_spi *kss = to_ks8851_spi(ks);

	flush_work(&kss->tx_work);
}

 
static netdev_tx_t ks8851_start_xmit_spi(struct sk_buff *skb,
					 struct net_device *dev)
{
	unsigned int needed = calc_txlen(skb->len);
	struct ks8851_net *ks = netdev_priv(dev);
	netdev_tx_t ret = NETDEV_TX_OK;
	struct ks8851_net_spi *kss;

	kss = to_ks8851_spi(ks);

	netif_dbg(ks, tx_queued, ks->netdev,
		  "%s: skb %p, %d@%p\n", __func__, skb, skb->len, skb->data);

	spin_lock(&ks->statelock);

	if (ks->queued_len + needed > ks->tx_space) {
		netif_stop_queue(dev);
		ret = NETDEV_TX_BUSY;
	} else {
		ks->queued_len += needed;
		skb_queue_tail(&ks->txq, skb);
	}

	spin_unlock(&ks->statelock);
	if (ret == NETDEV_TX_OK)
		schedule_work(&kss->tx_work);

	return ret;
}

static int ks8851_probe_spi(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct ks8851_net_spi *kss;
	struct net_device *netdev;
	struct ks8851_net *ks;

	netdev = devm_alloc_etherdev(dev, sizeof(struct ks8851_net_spi));
	if (!netdev)
		return -ENOMEM;

	spi->bits_per_word = 8;

	kss = netdev_priv(netdev);
	ks = &kss->ks8851;

	ks->lock = ks8851_lock_spi;
	ks->unlock = ks8851_unlock_spi;
	ks->rdreg16 = ks8851_rdreg16_spi;
	ks->wrreg16 = ks8851_wrreg16_spi;
	ks->rdfifo = ks8851_rdfifo_spi;
	ks->wrfifo = ks8851_wrfifo_spi;
	ks->start_xmit = ks8851_start_xmit_spi;
	ks->rx_skb = ks8851_rx_skb_spi;
	ks->flush_tx_work = ks8851_flush_tx_work_spi;

#define STD_IRQ (IRQ_LCI |	 	\
		 IRQ_TXI |	 		\
		 IRQ_RXI |	 		\
		 IRQ_SPIBEI |	 	\
		 IRQ_TXPSI |	 	\
		 IRQ_RXPSI)	 
	ks->rc_ier = STD_IRQ;

	kss->spidev = spi;
	mutex_init(&kss->lock);
	INIT_WORK(&kss->tx_work, ks8851_tx_work);

	 
	spi_message_init(&kss->spi_msg1);
	spi_message_add_tail(&kss->spi_xfer1, &kss->spi_msg1);

	spi_message_init(&kss->spi_msg2);
	spi_message_add_tail(&kss->spi_xfer2[0], &kss->spi_msg2);
	spi_message_add_tail(&kss->spi_xfer2[1], &kss->spi_msg2);

	netdev->irq = spi->irq;

	return ks8851_probe_common(netdev, dev, msg_enable);
}

static void ks8851_remove_spi(struct spi_device *spi)
{
	ks8851_remove_common(&spi->dev);
}

static const struct of_device_id ks8851_match_table[] = {
	{ .compatible = "micrel,ks8851" },
	{ }
};
MODULE_DEVICE_TABLE(of, ks8851_match_table);

static struct spi_driver ks8851_driver = {
	.driver = {
		.name = "ks8851",
		.of_match_table = ks8851_match_table,
		.pm = &ks8851_pm_ops,
	},
	.probe = ks8851_probe_spi,
	.remove = ks8851_remove_spi,
};
module_spi_driver(ks8851_driver);

MODULE_DESCRIPTION("KS8851 Network driver");
MODULE_AUTHOR("Ben Dooks <ben@simtec.co.uk>");
MODULE_LICENSE("GPL");

module_param_named(message, msg_enable, int, 0);
MODULE_PARM_DESC(message, "Message verbosity level (0=none, 31=all)");
MODULE_ALIAS("spi:ks8851");
