
 

 

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/comedi/comedi_pci.h>

#include "amplc_dio200.h"

 

enum dio200_pci_model {
	pci215_model,
	pci272_model,
	pcie215_model,
	pcie236_model,
	pcie296_model
};

static const struct dio200_board dio200_pci_boards[] = {
	[pci215_model] = {
		.name		= "pci215",
		.mainbar	= 2,
		.n_subdevs	= 5,
		.sdtype		= {
			sd_8255, sd_8255, sd_8254, sd_8254, sd_intr
		},
		.sdinfo		= { 0x00, 0x08, 0x10, 0x14, 0x3f },
		.has_int_sce	= true,
		.has_clk_gat_sce = true,
	},
	[pci272_model] = {
		.name		= "pci272",
		.mainbar	= 2,
		.n_subdevs	= 4,
		.sdtype		= {
			sd_8255, sd_8255, sd_8255, sd_intr
		},
		.sdinfo		= { 0x00, 0x08, 0x10, 0x3f },
		.has_int_sce	= true,
	},
	[pcie215_model] = {
		.name		= "pcie215",
		.mainbar	= 1,
		.n_subdevs	= 8,
		.sdtype		= {
			sd_8255, sd_none, sd_8255, sd_none,
			sd_8254, sd_8254, sd_timer, sd_intr
		},
		.sdinfo		= {
			0x00, 0x00, 0x08, 0x00, 0x10, 0x14, 0x00, 0x3f
		},
		.has_int_sce	= true,
		.has_clk_gat_sce = true,
		.is_pcie	= true,
	},
	[pcie236_model] = {
		.name		= "pcie236",
		.mainbar	= 1,
		.n_subdevs	= 8,
		.sdtype		= {
			sd_8255, sd_none, sd_none, sd_none,
			sd_8254, sd_8254, sd_timer, sd_intr
		},
		.sdinfo		= {
			0x00, 0x00, 0x00, 0x00, 0x10, 0x14, 0x00, 0x3f
		},
		.has_int_sce	= true,
		.has_clk_gat_sce = true,
		.is_pcie	= true,
	},
	[pcie296_model] = {
		.name		= "pcie296",
		.mainbar	= 1,
		.n_subdevs	= 8,
		.sdtype		= {
			sd_8255, sd_8255, sd_8255, sd_8255,
			sd_8254, sd_8254, sd_timer, sd_intr
		},
		.sdinfo		= {
			0x00, 0x04, 0x08, 0x0c, 0x10, 0x14, 0x00, 0x3f
		},
		.has_int_sce	= true,
		.has_clk_gat_sce = true,
		.is_pcie	= true,
	},
};

 
static int dio200_pcie_board_setup(struct comedi_device *dev)
{
	struct pci_dev *pcidev = comedi_to_pci_dev(dev);
	void __iomem *brbase;

	 
	if (pci_resource_len(pcidev, 0) < 0x4000) {
		dev_err(dev->class_dev, "error! bad PCI region!\n");
		return -EINVAL;
	}
	brbase = pci_ioremap_bar(pcidev, 0);
	if (!brbase) {
		dev_err(dev->class_dev, "error! failed to map registers!\n");
		return -ENOMEM;
	}
	writel(0x80, brbase + 0x50);
	iounmap(brbase);
	 
	amplc_dio200_set_enhance(dev, 1);
	return 0;
}

static int dio200_pci_auto_attach(struct comedi_device *dev,
				  unsigned long context_model)
{
	struct pci_dev *pci_dev = comedi_to_pci_dev(dev);
	const struct dio200_board *board = NULL;
	unsigned int bar;
	int ret;

	if (context_model < ARRAY_SIZE(dio200_pci_boards))
		board = &dio200_pci_boards[context_model];
	if (!board)
		return -EINVAL;
	dev->board_ptr = board;
	dev->board_name = board->name;

	dev_info(dev->class_dev, "%s: attach pci %s (%s)\n",
		 dev->driver->driver_name, pci_name(pci_dev), dev->board_name);

	ret = comedi_pci_enable(dev);
	if (ret)
		return ret;

	bar = board->mainbar;
	if (pci_resource_flags(pci_dev, bar) & IORESOURCE_MEM) {
		dev->mmio = pci_ioremap_bar(pci_dev, bar);
		if (!dev->mmio) {
			dev_err(dev->class_dev,
				"error! cannot remap registers\n");
			return -ENOMEM;
		}
	} else {
		dev->iobase = pci_resource_start(pci_dev, bar);
	}

	if (board->is_pcie) {
		ret = dio200_pcie_board_setup(dev);
		if (ret < 0)
			return ret;
	}

	return amplc_dio200_common_attach(dev, pci_dev->irq, IRQF_SHARED);
}

static struct comedi_driver dio200_pci_comedi_driver = {
	.driver_name	= "amplc_dio200_pci",
	.module		= THIS_MODULE,
	.auto_attach	= dio200_pci_auto_attach,
	.detach		= comedi_pci_detach,
};

static const struct pci_device_id dio200_pci_table[] = {
	{ PCI_VDEVICE(AMPLICON, 0x000b), pci215_model },
	{ PCI_VDEVICE(AMPLICON, 0x000a), pci272_model },
	{ PCI_VDEVICE(AMPLICON, 0x0011), pcie236_model },
	{ PCI_VDEVICE(AMPLICON, 0x0012), pcie215_model },
	{ PCI_VDEVICE(AMPLICON, 0x0014), pcie296_model },
	{0}
};

MODULE_DEVICE_TABLE(pci, dio200_pci_table);

static int dio200_pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	return comedi_pci_auto_config(dev, &dio200_pci_comedi_driver,
				      id->driver_data);
}

static struct pci_driver dio200_pci_pci_driver = {
	.name		= "amplc_dio200_pci",
	.id_table	= dio200_pci_table,
	.probe		= dio200_pci_probe,
	.remove		= comedi_pci_auto_unconfig,
};
module_comedi_pci_driver(dio200_pci_comedi_driver, dio200_pci_pci_driver);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi driver for Amplicon 200 Series PCI(e) DIO boards");
MODULE_LICENSE("GPL");
