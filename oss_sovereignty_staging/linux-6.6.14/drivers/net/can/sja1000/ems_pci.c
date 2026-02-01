
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/can/dev.h>
#include <linux/io.h>

#include "sja1000.h"

#define DRV_NAME  "ems_pci"

MODULE_AUTHOR("Sebastian Haas <support@ems-wuensche.com>");
MODULE_AUTHOR("Gerhard Uttenthaler <uttenthaler@ems-wuensche.com>");
MODULE_DESCRIPTION("Socket-CAN driver for EMS CPC-PCI/PCIe/104P CAN cards");
MODULE_LICENSE("GPL v2");

#define EMS_PCI_V1_MAX_CHAN 2
#define EMS_PCI_V2_MAX_CHAN 4
#define EMS_PCI_V3_MAX_CHAN 4
#define EMS_PCI_MAX_CHAN    EMS_PCI_V2_MAX_CHAN

struct ems_pci_card {
	int version;
	int channels;

	struct pci_dev *pci_dev;
	struct net_device *net_dev[EMS_PCI_MAX_CHAN];

	void __iomem *conf_addr;
	void __iomem *base_addr;
};

#define EMS_PCI_CAN_CLOCK (16000000 / 2)

 
#define PITA2_ICR           0x00	 
#define PITA2_ICR_INT0      0x00000002	 
#define PITA2_ICR_INT0_EN   0x00020000	 

#define PITA2_MISC          0x1c	 
#define PITA2_MISC_CONFIG   0x04000000	 

 
#define PLX_ICSR            0x4c    
#define PLX_ICSR_LINTI1_ENA 0x0001  
#define PLX_ICSR_PCIINT_ENA 0x0040  
#define PLX_ICSR_LINTI1_CLR 0x0400  
#define PLX_ICSR_ENA_CLR    (PLX_ICSR_LINTI1_ENA | PLX_ICSR_PCIINT_ENA | \
			     PLX_ICSR_LINTI1_CLR)

 
#define ASIX_LINTSR 0x28  
#define ASIX_LINTSR_INT0AC BIT(0)  

#define ASIX_LIEMR 0x24  
#define ASIX_LIEMR_L0EINTEN BIT(16)  
#define ASIX_LIEMR_LRST BIT(14)  

 
#define EMS_PCI_OCR         (OCR_TX0_PUSHPULL | OCR_TX1_PUSHPULL)

 
#define EMS_PCI_CDR             (CDR_CBP | CDR_CLKOUT_MASK)

#define EMS_PCI_V1_BASE_BAR 1
#define EMS_PCI_V1_CONF_BAR 0
#define EMS_PCI_V1_CONF_SIZE 4096  
#define EMS_PCI_V1_CAN_BASE_OFFSET 0x400  
#define EMS_PCI_V1_CAN_CTRL_SIZE 0x200  

#define EMS_PCI_V2_BASE_BAR 2
#define EMS_PCI_V2_CONF_BAR 0
#define EMS_PCI_V2_CONF_SIZE 128  
#define EMS_PCI_V2_CAN_BASE_OFFSET 0x400  
#define EMS_PCI_V2_CAN_CTRL_SIZE 0x200  

#define EMS_PCI_V3_BASE_BAR 0
#define EMS_PCI_V3_CONF_BAR 5
#define EMS_PCI_V3_CONF_SIZE 128  
#define EMS_PCI_V3_CAN_BASE_OFFSET 0x00  
#define EMS_PCI_V3_CAN_CTRL_SIZE 0x100  

#define EMS_PCI_BASE_SIZE  4096  

#define PCI_SUBDEVICE_ID_EMS 0x4010

static const struct pci_device_id ems_pci_tbl[] = {
	 
	{PCI_VENDOR_ID_SIEMENS, 0x2104, PCI_ANY_ID, PCI_ANY_ID,},
	 
	{PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9030, PCI_VENDOR_ID_PLX, 0x4000},
	 
	{PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9030, PCI_VENDOR_ID_PLX, 0x4002},
	 
	{PCI_VENDOR_ID_ASIX, PCI_DEVICE_ID_ASIX_AX99100_LB, 0xa000, PCI_SUBDEVICE_ID_EMS},
	{0,}
};
MODULE_DEVICE_TABLE(pci, ems_pci_tbl);

 
static u8 ems_pci_v1_readb(struct ems_pci_card *card, unsigned int port)
{
	return readb(card->base_addr + (port * 4));
}

static u8 ems_pci_v1_read_reg(const struct sja1000_priv *priv, int port)
{
	return readb(priv->reg_base + (port * 4));
}

static void ems_pci_v1_write_reg(const struct sja1000_priv *priv,
				 int port, u8 val)
{
	writeb(val, priv->reg_base + (port * 4));
}

static void ems_pci_v1_post_irq(const struct sja1000_priv *priv)
{
	struct ems_pci_card *card = priv->priv;

	 
	writel(PITA2_ICR_INT0_EN | PITA2_ICR_INT0,
	       card->conf_addr + PITA2_ICR);
}

static u8 ems_pci_v2_read_reg(const struct sja1000_priv *priv, int port)
{
	return readb(priv->reg_base + port);
}

static void ems_pci_v2_write_reg(const struct sja1000_priv *priv,
				 int port, u8 val)
{
	writeb(val, priv->reg_base + port);
}

static void ems_pci_v2_post_irq(const struct sja1000_priv *priv)
{
	struct ems_pci_card *card = priv->priv;

	writel(PLX_ICSR_ENA_CLR, card->conf_addr + PLX_ICSR);
}

static u8 ems_pci_v3_read_reg(const struct sja1000_priv *priv, int port)
{
	return readb(priv->reg_base + port);
}

static void ems_pci_v3_write_reg(const struct sja1000_priv *priv,
				 int port, u8 val)
{
	writeb(val, priv->reg_base + port);
}

static void ems_pci_v3_post_irq(const struct sja1000_priv *priv)
{
	struct ems_pci_card *card = priv->priv;

	writel(ASIX_LINTSR_INT0AC, card->conf_addr + ASIX_LINTSR);
}

 
static inline int ems_pci_check_chan(const struct sja1000_priv *priv)
{
	unsigned char res;

	 
	priv->write_reg(priv, SJA1000_MOD, 1);

	priv->write_reg(priv, SJA1000_CDR, CDR_PELICAN);

	 
	res = priv->read_reg(priv, SJA1000_CDR);

	if (res == CDR_PELICAN)
		return 1;

	return 0;
}

static void ems_pci_del_card(struct pci_dev *pdev)
{
	struct ems_pci_card *card = pci_get_drvdata(pdev);
	struct net_device *dev;
	int i = 0;

	for (i = 0; i < card->channels; i++) {
		dev = card->net_dev[i];

		if (!dev)
			continue;

		dev_info(&pdev->dev, "Removing %s.\n", dev->name);
		unregister_sja1000dev(dev);
		free_sja1000dev(dev);
	}

	if (card->base_addr)
		pci_iounmap(card->pci_dev, card->base_addr);

	if (card->conf_addr)
		pci_iounmap(card->pci_dev, card->conf_addr);

	kfree(card);

	pci_disable_device(pdev);
}

static void ems_pci_card_reset(struct ems_pci_card *card)
{
	 
	writeb(0, card->base_addr);
}

 
static int ems_pci_add_card(struct pci_dev *pdev,
			    const struct pci_device_id *ent)
{
	struct sja1000_priv *priv;
	struct net_device *dev;
	struct ems_pci_card *card;
	int max_chan, conf_size, base_bar, conf_bar;
	int err, i;

	 
	if (pci_enable_device(pdev) < 0) {
		dev_err(&pdev->dev, "Enabling PCI device failed\n");
		return -ENODEV;
	}

	 
	card = kzalloc(sizeof(*card), GFP_KERNEL);
	if (!card) {
		pci_disable_device(pdev);
		return -ENOMEM;
	}

	pci_set_drvdata(pdev, card);

	card->pci_dev = pdev;

	card->channels = 0;

	if (pdev->vendor == PCI_VENDOR_ID_ASIX) {
		card->version = 3;  
		max_chan = EMS_PCI_V3_MAX_CHAN;
		base_bar = EMS_PCI_V3_BASE_BAR;
		conf_bar = EMS_PCI_V3_CONF_BAR;
		conf_size = EMS_PCI_V3_CONF_SIZE;
	} else if (pdev->vendor == PCI_VENDOR_ID_PLX) {
		card->version = 2;  
		max_chan = EMS_PCI_V2_MAX_CHAN;
		base_bar = EMS_PCI_V2_BASE_BAR;
		conf_bar = EMS_PCI_V2_CONF_BAR;
		conf_size = EMS_PCI_V2_CONF_SIZE;
	} else {
		card->version = 1;  
		max_chan = EMS_PCI_V1_MAX_CHAN;
		base_bar = EMS_PCI_V1_BASE_BAR;
		conf_bar = EMS_PCI_V1_CONF_BAR;
		conf_size = EMS_PCI_V1_CONF_SIZE;
	}

	 
	card->conf_addr = pci_iomap(pdev, conf_bar, conf_size);
	if (!card->conf_addr) {
		err = -ENOMEM;
		goto failure_cleanup;
	}

	card->base_addr = pci_iomap(pdev, base_bar, EMS_PCI_BASE_SIZE);
	if (!card->base_addr) {
		err = -ENOMEM;
		goto failure_cleanup;
	}

	if (card->version == 1) {
		 
		writel(PITA2_MISC_CONFIG, card->conf_addr + PITA2_MISC);

		 
		if (ems_pci_v1_readb(card, 0) != 0x55 ||
		    ems_pci_v1_readb(card, 1) != 0xAA ||
		    ems_pci_v1_readb(card, 2) != 0x01 ||
		    ems_pci_v1_readb(card, 3) != 0xCB ||
		    ems_pci_v1_readb(card, 4) != 0x11) {
			dev_err(&pdev->dev,
				"Not EMS Dr. Thomas Wuensche interface\n");
			err = -ENODEV;
			goto failure_cleanup;
		}
	}

	if (card->version == 3) {
		 
		writel(readl(card->conf_addr + ASIX_LIEMR) & ~ASIX_LIEMR_LRST,
		       card->conf_addr + ASIX_LIEMR);
	}

	ems_pci_card_reset(card);

	 
	for (i = 0; i < max_chan; i++) {
		dev = alloc_sja1000dev(0);
		if (!dev) {
			err = -ENOMEM;
			goto failure_cleanup;
		}

		card->net_dev[i] = dev;
		priv = netdev_priv(dev);
		priv->priv = card;
		priv->irq_flags = IRQF_SHARED;

		dev->irq = pdev->irq;

		if (card->version == 1) {
			priv->read_reg  = ems_pci_v1_read_reg;
			priv->write_reg = ems_pci_v1_write_reg;
			priv->post_irq  = ems_pci_v1_post_irq;
			priv->reg_base = card->base_addr + EMS_PCI_V1_CAN_BASE_OFFSET
					+ (i * EMS_PCI_V1_CAN_CTRL_SIZE);
		} else if (card->version == 2) {
			priv->read_reg  = ems_pci_v2_read_reg;
			priv->write_reg = ems_pci_v2_write_reg;
			priv->post_irq  = ems_pci_v2_post_irq;
			priv->reg_base = card->base_addr + EMS_PCI_V2_CAN_BASE_OFFSET
					+ (i * EMS_PCI_V2_CAN_CTRL_SIZE);
		} else {
			priv->read_reg  = ems_pci_v3_read_reg;
			priv->write_reg = ems_pci_v3_write_reg;
			priv->post_irq  = ems_pci_v3_post_irq;
			priv->reg_base = card->base_addr + EMS_PCI_V3_CAN_BASE_OFFSET
					+ (i * EMS_PCI_V3_CAN_CTRL_SIZE);
		}

		 
		if (ems_pci_check_chan(priv)) {
			priv->can.clock.freq = EMS_PCI_CAN_CLOCK;
			priv->ocr = EMS_PCI_OCR;
			priv->cdr = EMS_PCI_CDR;

			SET_NETDEV_DEV(dev, &pdev->dev);
			dev->dev_id = i;

			if (card->version == 1) {
				 
				writel(PITA2_ICR_INT0_EN | PITA2_ICR_INT0,
				       card->conf_addr + PITA2_ICR);
			} else if (card->version == 2) {
				 
				writel(PLX_ICSR_ENA_CLR,
				       card->conf_addr + PLX_ICSR);
			} else {
				 
				writel(ASIX_LINTSR_INT0AC, card->conf_addr + ASIX_LINTSR);
				 
				writel(readl(card->conf_addr + ASIX_LIEMR) | ASIX_LIEMR_L0EINTEN,
				       card->conf_addr + ASIX_LIEMR);
			}

			 
			err = register_sja1000dev(dev);
			if (err) {
				dev_err(&pdev->dev,
					"Registering device failed: %pe\n",
					ERR_PTR(err));
				free_sja1000dev(dev);
				goto failure_cleanup;
			}

			card->channels++;

			dev_info(&pdev->dev, "Channel #%d at 0x%p, irq %d\n",
				 i + 1, priv->reg_base, dev->irq);
		} else {
			free_sja1000dev(dev);
		}
	}

	return 0;

failure_cleanup:
	dev_err(&pdev->dev, "Error: %d. Cleaning Up.\n", err);

	ems_pci_del_card(pdev);

	return err;
}

static struct pci_driver ems_pci_driver = {
	.name = DRV_NAME,
	.id_table = ems_pci_tbl,
	.probe = ems_pci_add_card,
	.remove = ems_pci_del_card,
};

module_pci_driver(ems_pci_driver);
