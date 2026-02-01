
 

#include <linux/io.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include "enetc.h"
#include "enetc_ierb.h"

 
#define ENETC_IERB_TXMBAR(port)			(((port) * 0x100) + 0x8080)
#define ENETC_IERB_RXMBER(port)			(((port) * 0x100) + 0x8090)
#define ENETC_IERB_RXMBLR(port)			(((port) * 0x100) + 0x8094)
#define ENETC_IERB_RXBCR(port)			(((port) * 0x100) + 0x80a0)
#define ENETC_IERB_TXBCR(port)			(((port) * 0x100) + 0x80a8)
#define ENETC_IERB_FMBDTR			0xa000

#define ENETC_RESERVED_FOR_ICM			1024

struct enetc_ierb {
	void __iomem *regs;
};

static void enetc_ierb_write(struct enetc_ierb *ierb, u32 offset, u32 val)
{
	iowrite32(val, ierb->regs + offset);
}

int enetc_ierb_register_pf(struct platform_device *pdev,
			   struct pci_dev *pf_pdev)
{
	struct enetc_ierb *ierb = platform_get_drvdata(pdev);
	int port = enetc_pf_to_port(pf_pdev);
	u16 tx_credit, rx_credit, tx_alloc;

	if (port < 0)
		return -ENODEV;

	if (!ierb)
		return -EPROBE_DEFER;

	 
	tx_credit = roundup(1000 + ENETC_MAC_MAXFRM_SIZE / 2, 100);

	 
	tx_alloc = roundup(2 * tx_credit + 4 * ENETC_MAC_MAXFRM_SIZE + 64, 16);

	 
	rx_credit = DIV_ROUND_UP(ENETC_MAC_MAXFRM_SIZE * 2, 8);

	enetc_ierb_write(ierb, ENETC_IERB_TXBCR(port), tx_credit);
	enetc_ierb_write(ierb, ENETC_IERB_TXMBAR(port), tx_alloc);
	enetc_ierb_write(ierb, ENETC_IERB_RXBCR(port), rx_credit);

	return 0;
}
EXPORT_SYMBOL(enetc_ierb_register_pf);

static int enetc_ierb_probe(struct platform_device *pdev)
{
	struct enetc_ierb *ierb;
	void __iomem *regs;

	ierb = devm_kzalloc(&pdev->dev, sizeof(*ierb), GFP_KERNEL);
	if (!ierb)
		return -ENOMEM;

	regs = devm_platform_get_and_ioremap_resource(pdev, 0, NULL);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	ierb->regs = regs;

	 
	enetc_ierb_write(ierb, ENETC_IERB_FMBDTR, ENETC_RESERVED_FOR_ICM);

	platform_set_drvdata(pdev, ierb);

	return 0;
}

static const struct of_device_id enetc_ierb_match[] = {
	{ .compatible = "fsl,ls1028a-enetc-ierb", },
	{},
};
MODULE_DEVICE_TABLE(of, enetc_ierb_match);

static struct platform_driver enetc_ierb_driver = {
	.driver = {
		.name = "fsl-enetc-ierb",
		.of_match_table = enetc_ierb_match,
	},
	.probe = enetc_ierb_probe,
};

module_platform_driver(enetc_ierb_driver);

MODULE_DESCRIPTION("NXP ENETC IERB");
MODULE_LICENSE("Dual BSD/GPL");
