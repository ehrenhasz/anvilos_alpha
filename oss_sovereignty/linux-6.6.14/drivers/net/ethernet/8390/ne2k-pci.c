
 

#define DRV_NAME	"ne2k-pci"
#define DRV_DESCRIPTION	"PCI NE2000 clone driver"
#define DRV_AUTHOR	"Donald Becker / Paul Gortmaker"
#define DRV_VERSION	"1.03"
#define DRV_RELDATE	"9/22/2003"

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

 

 
#define MAX_UNITS 8

 
static int full_duplex[MAX_UNITS];
static int options[MAX_UNITS];

 
 


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ethtool.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#include <linux/io.h>
#include <asm/irq.h>
#include <linux/uaccess.h>

#include "8390.h"

static int ne2k_msg_enable;

static const int default_msg_level = (NETIF_MSG_DRV | NETIF_MSG_PROBE |
				      NETIF_MSG_RX_ERR | NETIF_MSG_TX_ERR);

#if defined(__powerpc__)
#define inl_le(addr)  le32_to_cpu(inl(addr))
#define inw_le(addr)  le16_to_cpu(inw(addr))
#endif

MODULE_AUTHOR(DRV_AUTHOR);
MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("GPL");

module_param_named(msg_enable, ne2k_msg_enable, int, 0444);
module_param_array(options, int, NULL, 0);
module_param_array(full_duplex, int, NULL, 0);
MODULE_PARM_DESC(msg_enable, "Debug message level (see linux/netdevice.h for bitmap)");
MODULE_PARM_DESC(options, "Bit 5: full duplex");
MODULE_PARM_DESC(full_duplex, "full duplex setting(s) (1)");

 

 
#define USE_LONGIO

 
 

 
#define ne2k_flags reg0

enum {
	 
	ONLY_16BIT_IO = 8, ONLY_32BIT_IO = 4,
	 
	FORCE_FDX = 0x20,
	REALTEK_FDX = 0x40, HOLTEK_FDX = 0x80,
	STOP_PG_0x60 = 0x100,
};

enum ne2k_pci_chipsets {
	CH_RealTek_RTL_8029 = 0,
	CH_Winbond_89C940,
	CH_Compex_RL2000,
	CH_KTI_ET32P2,
	CH_NetVin_NV5000SC,
	CH_Via_86C926,
	CH_SureCom_NE34,
	CH_Winbond_W89C940F,
	CH_Holtek_HT80232,
	CH_Holtek_HT80229,
	CH_Winbond_89C940_8c4a,
};


static struct {
	char *name;
	int flags;
} pci_clone_list[] = {
	{"RealTek RTL-8029(AS)", REALTEK_FDX},
	{"Winbond 89C940", 0},
	{"Compex RL2000", 0},
	{"KTI ET32P2", 0},
	{"NetVin NV5000SC", 0},
	{"Via 86C926", ONLY_16BIT_IO},
	{"SureCom NE34", 0},
	{"Winbond W89C940F", 0},
	{"Holtek HT80232", ONLY_16BIT_IO | HOLTEK_FDX},
	{"Holtek HT80229", ONLY_32BIT_IO | HOLTEK_FDX | STOP_PG_0x60 },
	{"Winbond W89C940(misprogrammed)", 0},
	{NULL,}
};


static const struct pci_device_id ne2k_pci_tbl[] = {
	{ 0x10ec, 0x8029, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_RealTek_RTL_8029 },
	{ 0x1050, 0x0940, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_Winbond_89C940 },
	{ 0x11f6, 0x1401, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_Compex_RL2000 },
	{ 0x8e2e, 0x3000, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_KTI_ET32P2 },
	{ 0x4a14, 0x5000, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_NetVin_NV5000SC },
	{ 0x1106, 0x0926, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_Via_86C926 },
	{ 0x10bd, 0x0e34, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_SureCom_NE34 },
	{ 0x1050, 0x5a5a, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_Winbond_W89C940F },
	{ 0x12c3, 0x0058, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_Holtek_HT80232 },
	{ 0x12c3, 0x5598, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_Holtek_HT80229 },
	{ 0x8c4a, 0x1980, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_Winbond_89C940_8c4a },
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, ne2k_pci_tbl);


 

#define NE_BASE	 (dev->base_addr)
#define NE_CMD		0x00
#define NE_DATAPORT	0x10	 
#define NE_RESET	0x1f	 
#define NE_IO_EXTENT	0x20

#define NESM_START_PG	0x40	 
#define NESM_STOP_PG	0x80	 


static int ne2k_pci_open(struct net_device *dev);
static int ne2k_pci_close(struct net_device *dev);

static void ne2k_pci_reset_8390(struct net_device *dev);
static void ne2k_pci_get_8390_hdr(struct net_device *dev,
				  struct e8390_pkt_hdr *hdr, int ring_page);
static void ne2k_pci_block_input(struct net_device *dev, int count,
				 struct sk_buff *skb, int ring_offset);
static void ne2k_pci_block_output(struct net_device *dev, const int count,
				  const unsigned char *buf,
				  const int start_page);
static const struct ethtool_ops ne2k_pci_ethtool_ops;



 
struct ne2k_pci_card {
	struct net_device *dev;
	struct pci_dev *pci_dev;
};



 

static const struct net_device_ops ne2k_netdev_ops = {
	.ndo_open		= ne2k_pci_open,
	.ndo_stop		= ne2k_pci_close,
	.ndo_start_xmit		= ei_start_xmit,
	.ndo_tx_timeout		= ei_tx_timeout,
	.ndo_get_stats		= ei_get_stats,
	.ndo_set_rx_mode	= ei_set_multicast_list,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address	= eth_mac_addr,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller = ei_poll,
#endif
};

static int ne2k_pci_init_one(struct pci_dev *pdev,
			     const struct pci_device_id *ent)
{
	struct net_device *dev;
	int i;
	unsigned char SA_prom[32];
	int start_page, stop_page;
	int irq, reg0, chip_idx = ent->driver_data;
	static unsigned int fnd_cnt;
	long ioaddr;
	int flags = pci_clone_list[chip_idx].flags;
	struct ei_device *ei_local;

	fnd_cnt++;

	i = pci_enable_device(pdev);
	if (i)
		return i;

	ioaddr = pci_resource_start(pdev, 0);
	irq = pdev->irq;

	if (!ioaddr || ((pci_resource_flags(pdev, 0) & IORESOURCE_IO) == 0)) {
		dev_err(&pdev->dev, "no I/O resource at PCI BAR #0\n");
		goto err_out;
	}

	if (!request_region(ioaddr, NE_IO_EXTENT, DRV_NAME)) {
		dev_err(&pdev->dev, "I/O resource 0x%x @ 0x%lx busy\n",
			NE_IO_EXTENT, ioaddr);
		goto err_out;
	}

	reg0 = inb(ioaddr);
	if (reg0 == 0xFF)
		goto err_out_free_res;

	 
	{
		int regd;

		outb(E8390_NODMA + E8390_PAGE1 + E8390_STOP, ioaddr + E8390_CMD);
		regd = inb(ioaddr + 0x0d);
		outb(0xff, ioaddr + 0x0d);
		outb(E8390_NODMA + E8390_PAGE0, ioaddr + E8390_CMD);
		 
		inb(ioaddr + EN0_COUNTER0);
		if (inb(ioaddr + EN0_COUNTER0) != 0) {
			outb(reg0, ioaddr);
			 
			outb(regd, ioaddr + 0x0d);
			goto err_out_free_res;
		}
	}

	 
	dev = alloc_ei_netdev();
	if (!dev) {
		dev_err(&pdev->dev, "cannot allocate ethernet device\n");
		goto err_out_free_res;
	}
	dev->netdev_ops = &ne2k_netdev_ops;
	ei_local = netdev_priv(dev);
	ei_local->msg_enable = netif_msg_init(ne2k_msg_enable, default_msg_level);

	SET_NETDEV_DEV(dev, &pdev->dev);

	 
	{
		unsigned long reset_start_time = jiffies;

		outb(inb(ioaddr + NE_RESET), ioaddr + NE_RESET);

		 
		while ((inb(ioaddr + EN0_ISR) & ENISR_RESET) == 0)
			 
			if (jiffies - reset_start_time > 2) {
				dev_err(&pdev->dev,
					"Card failure (no reset ack).\n");
				goto err_out_free_netdev;
			}
		 
		outb(0xff, ioaddr + EN0_ISR);
	}

	 
	{
		struct {unsigned char value, offset; } program_seq[] = {
			 
			{E8390_NODMA + E8390_PAGE0 + E8390_STOP, E8390_CMD},
			 
			{0x49,	EN0_DCFG},
			 
			{0x00,	EN0_RCNTLO},
			 
			{0x00,	EN0_RCNTHI},
			{0x00,	EN0_IMR},
			{0xFF,	EN0_ISR},
			 
			{E8390_RXOFF, EN0_RXCR},
			 
			{E8390_TXOFF, EN0_TXCR},
			{32,	EN0_RCNTLO},
			{0x00,	EN0_RCNTHI},
			 
			{0x00,	EN0_RSARLO},
			{0x00,	EN0_RSARHI},
			{E8390_RREAD+E8390_START, E8390_CMD},
		};
		for (i = 0; i < ARRAY_SIZE(program_seq); i++)
			outb(program_seq[i].value,
			     ioaddr + program_seq[i].offset);

	}

	 
	if (flags & ONLY_32BIT_IO) {
		for (i = 0; i < 4 ; i++)
			((u32 *)SA_prom)[i] = le32_to_cpu(inl(ioaddr + NE_DATAPORT));
	} else
		for (i = 0; i < 32  ; i++)
			SA_prom[i] = inb(ioaddr + NE_DATAPORT);

	 
	outb(0x49, ioaddr + EN0_DCFG);
	start_page = NESM_START_PG;

	stop_page = flags & STOP_PG_0x60 ? 0x60 : NESM_STOP_PG;

	 
	dev->irq = irq;
	dev->base_addr = ioaddr;
	pci_set_drvdata(pdev, dev);

	ei_status.name = pci_clone_list[chip_idx].name;
	ei_status.tx_start_page = start_page;
	ei_status.stop_page = stop_page;
	ei_status.word16 = 1;
	ei_status.ne2k_flags = flags;
	if (fnd_cnt < MAX_UNITS) {
		if (full_duplex[fnd_cnt] > 0 || (options[fnd_cnt] & FORCE_FDX))
			ei_status.ne2k_flags |= FORCE_FDX;
	}

	ei_status.rx_start_page = start_page + TX_PAGES;
#ifdef PACKETBUF_MEMSIZE
	 
	ei_status.stop_page = ei_status.tx_start_page + PACKETBUF_MEMSIZE;
#endif

	ei_status.reset_8390 = &ne2k_pci_reset_8390;
	ei_status.block_input = &ne2k_pci_block_input;
	ei_status.block_output = &ne2k_pci_block_output;
	ei_status.get_8390_hdr = &ne2k_pci_get_8390_hdr;
	ei_status.priv = (unsigned long) pdev;

	dev->ethtool_ops = &ne2k_pci_ethtool_ops;
	NS8390_init(dev, 0);

	eth_hw_addr_set(dev, SA_prom);

	i = register_netdev(dev);
	if (i)
		goto err_out_free_netdev;

	netdev_info(dev, "%s found at %#lx, IRQ %d, %pM.\n",
		    pci_clone_list[chip_idx].name, ioaddr, dev->irq,
		    dev->dev_addr);

	return 0;

err_out_free_netdev:
	free_netdev(dev);
err_out_free_res:
	release_region(ioaddr, NE_IO_EXTENT);
err_out:
	pci_disable_device(pdev);
	return -ENODEV;
}

 
static inline int set_realtek_fdx(struct net_device *dev)
{
	long ioaddr = dev->base_addr;

	outb(0xC0 + E8390_NODMA, ioaddr + NE_CMD);  
	outb(0xC0, ioaddr + 0x01);  
	outb(0x40, ioaddr + 0x06);  
	outb(0x00, ioaddr + 0x01);  
	outb(E8390_PAGE0 + E8390_NODMA, ioaddr + NE_CMD);  
	return 0;
}

static inline int set_holtek_fdx(struct net_device *dev)
{
	long ioaddr = dev->base_addr;

	outb(inb(ioaddr + 0x20) | 0x80, ioaddr + 0x20);
	return 0;
}

static int ne2k_pci_set_fdx(struct net_device *dev)
{
	if (ei_status.ne2k_flags & REALTEK_FDX)
		return set_realtek_fdx(dev);
	else if (ei_status.ne2k_flags & HOLTEK_FDX)
		return set_holtek_fdx(dev);

	return -EOPNOTSUPP;
}

static int ne2k_pci_open(struct net_device *dev)
{
	int ret = request_irq(dev->irq, ei_interrupt, IRQF_SHARED,
			      dev->name, dev);

	if (ret)
		return ret;

	if (ei_status.ne2k_flags & FORCE_FDX)
		ne2k_pci_set_fdx(dev);

	ei_open(dev);
	return 0;
}

static int ne2k_pci_close(struct net_device *dev)
{
	ei_close(dev);
	free_irq(dev->irq, dev);
	return 0;
}

 
static void ne2k_pci_reset_8390(struct net_device *dev)
{
	unsigned long reset_start_time = jiffies;
	struct ei_device *ei_local = netdev_priv(dev);

	netif_dbg(ei_local, hw, dev, "resetting the 8390 t=%ld...\n",
		  jiffies);

	outb(inb(NE_BASE + NE_RESET), NE_BASE + NE_RESET);

	ei_status.txing = 0;
	ei_status.dmaing = 0;

	 
	while ((inb(NE_BASE+EN0_ISR) & ENISR_RESET) == 0)
		if (jiffies - reset_start_time > 2) {
			netdev_err(dev, "%s did not complete.\n", __func__);
			break;
		}
	 
	outb(ENISR_RESET, NE_BASE + EN0_ISR);
}

 

static void ne2k_pci_get_8390_hdr(struct net_device *dev,
				  struct e8390_pkt_hdr *hdr, int ring_page)
{

	long nic_base = dev->base_addr;

	 
	if (ei_status.dmaing) {
		netdev_err(dev, "DMAing conflict in %s [DMAstat:%d][irqlock:%d].\n",
			   __func__, ei_status.dmaing, ei_status.irqlock);
		return;
	}

	ei_status.dmaing |= 0x01;
	outb(E8390_NODMA + E8390_PAGE0 + E8390_START, nic_base + NE_CMD);
	outb(sizeof(struct e8390_pkt_hdr), nic_base + EN0_RCNTLO);
	outb(0, nic_base + EN0_RCNTHI);
	outb(0, nic_base + EN0_RSARLO);		 
	outb(ring_page, nic_base + EN0_RSARHI);
	outb(E8390_RREAD+E8390_START, nic_base + NE_CMD);

	if (ei_status.ne2k_flags & ONLY_16BIT_IO) {
		insw(NE_BASE + NE_DATAPORT, hdr,
		     sizeof(struct e8390_pkt_hdr) >> 1);
	} else {
		*(u32 *)hdr = le32_to_cpu(inl(NE_BASE + NE_DATAPORT));
		le16_to_cpus(&hdr->count);
	}
	 
	outb(ENISR_RDC, nic_base + EN0_ISR);
	ei_status.dmaing &= ~0x01;
}

 

static void ne2k_pci_block_input(struct net_device *dev, int count,
				 struct sk_buff *skb, int ring_offset)
{
	long nic_base = dev->base_addr;
	char *buf = skb->data;

	 
	if (ei_status.dmaing) {
		netdev_err(dev, "DMAing conflict in %s [DMAstat:%d][irqlock:%d]\n",
			   __func__, ei_status.dmaing, ei_status.irqlock);
		return;
	}
	ei_status.dmaing |= 0x01;
	if (ei_status.ne2k_flags & ONLY_32BIT_IO)
		count = (count + 3) & 0xFFFC;
	outb(E8390_NODMA + E8390_PAGE0 + E8390_START, nic_base + NE_CMD);
	outb(count & 0xff, nic_base + EN0_RCNTLO);
	outb(count >> 8, nic_base + EN0_RCNTHI);
	outb(ring_offset & 0xff, nic_base + EN0_RSARLO);
	outb(ring_offset >> 8, nic_base + EN0_RSARHI);
	outb(E8390_RREAD + E8390_START, nic_base + NE_CMD);

	if (ei_status.ne2k_flags & ONLY_16BIT_IO) {
		insw(NE_BASE + NE_DATAPORT, buf, count >> 1);
		if (count & 0x01)
			buf[count-1] = inb(NE_BASE + NE_DATAPORT);
	} else {
		insl(NE_BASE + NE_DATAPORT, buf, count >> 2);
		if (count & 3) {
			buf += count & ~3;
			if (count & 2) {
				__le16 *b = (__le16 *)buf;

				*b++ = cpu_to_le16(inw(NE_BASE + NE_DATAPORT));
				buf = (char *)b;
			}
			if (count & 1)
				*buf = inb(NE_BASE + NE_DATAPORT);
		}
	}
	 
	outb(ENISR_RDC, nic_base + EN0_ISR);
	ei_status.dmaing &= ~0x01;
}

static void ne2k_pci_block_output(struct net_device *dev, int count,
		const unsigned char *buf, const int start_page)
{
	long nic_base = NE_BASE;
	unsigned long dma_start;

	 
	if (ei_status.ne2k_flags & ONLY_32BIT_IO)
		count = (count + 3) & 0xFFFC;
	else
		if (count & 0x01)
			count++;

	 
	if (ei_status.dmaing) {
		netdev_err(dev, "DMAing conflict in %s [DMAstat:%d][irqlock:%d]\n",
			   __func__, ei_status.dmaing, ei_status.irqlock);
		return;
	}
	ei_status.dmaing |= 0x01;
	 
	outb(E8390_PAGE0+E8390_START+E8390_NODMA, nic_base + NE_CMD);

#ifdef NE_RW_BUGFIX
	 
	outb(0x42, nic_base + EN0_RCNTLO);
	outb(0x00, nic_base + EN0_RCNTHI);
	outb(0x42, nic_base + EN0_RSARLO);
	outb(0x00, nic_base + EN0_RSARHI);
	outb(E8390_RREAD+E8390_START, nic_base + NE_CMD);
#endif
	outb(ENISR_RDC, nic_base + EN0_ISR);

	 
	outb(count & 0xff, nic_base + EN0_RCNTLO);
	outb(count >> 8,   nic_base + EN0_RCNTHI);
	outb(0x00, nic_base + EN0_RSARLO);
	outb(start_page, nic_base + EN0_RSARHI);
	outb(E8390_RWRITE+E8390_START, nic_base + NE_CMD);
	if (ei_status.ne2k_flags & ONLY_16BIT_IO) {
		outsw(NE_BASE + NE_DATAPORT, buf, count >> 1);
	} else {
		outsl(NE_BASE + NE_DATAPORT, buf, count >> 2);
		if (count & 3) {
			buf += count & ~3;
			if (count & 2) {
				__le16 *b = (__le16 *)buf;

				outw(le16_to_cpu(*b++), NE_BASE + NE_DATAPORT);
				buf = (char *)b;
			}
		}
	}

	dma_start = jiffies;

	while ((inb(nic_base + EN0_ISR) & ENISR_RDC) == 0)
		 
		if (jiffies - dma_start > 2) {
			netdev_warn(dev, "timeout waiting for Tx RDC.\n");
			ne2k_pci_reset_8390(dev);
			NS8390_init(dev, 1);
			break;
		}
	 
	outb(ENISR_RDC, nic_base + EN0_ISR);
	ei_status.dmaing &= ~0x01;
}

static void ne2k_pci_get_drvinfo(struct net_device *dev,
				 struct ethtool_drvinfo *info)
{
	struct ei_device *ei = netdev_priv(dev);
	struct pci_dev *pci_dev = (struct pci_dev *) ei->priv;

	strscpy(info->driver, DRV_NAME, sizeof(info->driver));
	strscpy(info->version, DRV_VERSION, sizeof(info->version));
	strscpy(info->bus_info, pci_name(pci_dev), sizeof(info->bus_info));
}

static u32 ne2k_pci_get_msglevel(struct net_device *dev)
{
	struct ei_device *ei_local = netdev_priv(dev);

	return ei_local->msg_enable;
}

static void ne2k_pci_set_msglevel(struct net_device *dev, u32 v)
{
	struct ei_device *ei_local = netdev_priv(dev);

	ei_local->msg_enable = v;
}

static const struct ethtool_ops ne2k_pci_ethtool_ops = {
	.get_drvinfo		= ne2k_pci_get_drvinfo,
	.get_msglevel		= ne2k_pci_get_msglevel,
	.set_msglevel		= ne2k_pci_set_msglevel,
};

static void ne2k_pci_remove_one(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);

	BUG_ON(!dev);
	unregister_netdev(dev);
	release_region(dev->base_addr, NE_IO_EXTENT);
	free_netdev(dev);
	pci_disable_device(pdev);
}

static int __maybe_unused ne2k_pci_suspend(struct device *dev_d)
{
	struct net_device *dev = dev_get_drvdata(dev_d);

	netif_device_detach(dev);

	return 0;
}

static int __maybe_unused ne2k_pci_resume(struct device *dev_d)
{
	struct net_device *dev = dev_get_drvdata(dev_d);

	NS8390_init(dev, 1);
	netif_device_attach(dev);

	return 0;
}

static SIMPLE_DEV_PM_OPS(ne2k_pci_pm_ops, ne2k_pci_suspend, ne2k_pci_resume);

static struct pci_driver ne2k_driver = {
	.name		= DRV_NAME,
	.probe		= ne2k_pci_init_one,
	.remove		= ne2k_pci_remove_one,
	.id_table	= ne2k_pci_tbl,
	.driver.pm	= &ne2k_pci_pm_ops,
};
module_pci_driver(ne2k_driver);
