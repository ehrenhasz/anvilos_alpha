
 

 

#include <linux/module.h>
#include <linux/comedi/comedi_pci.h>
#include <linux/comedi/comedi_8255.h>

enum pci_8255_boardid {
	BOARD_ADLINK_PCI7224,
	BOARD_ADLINK_PCI7248,
	BOARD_ADLINK_PCI7296,
	BOARD_CB_PCIDIO24,
	BOARD_CB_PCIDIO24H,
	BOARD_CB_PCIDIO48H_OLD,
	BOARD_CB_PCIDIO48H_NEW,
	BOARD_CB_PCIDIO96H,
	BOARD_NI_PCIDIO96,
	BOARD_NI_PCIDIO96B,
	BOARD_NI_PXI6508,
	BOARD_NI_PCI6503,
	BOARD_NI_PCI6503B,
	BOARD_NI_PCI6503X,
	BOARD_NI_PXI_6503,
};

struct pci_8255_boardinfo {
	const char *name;
	int dio_badr;
	int n_8255;
	unsigned int has_mite:1;
};

static const struct pci_8255_boardinfo pci_8255_boards[] = {
	[BOARD_ADLINK_PCI7224] = {
		.name		= "adl_pci-7224",
		.dio_badr	= 2,
		.n_8255		= 1,
	},
	[BOARD_ADLINK_PCI7248] = {
		.name		= "adl_pci-7248",
		.dio_badr	= 2,
		.n_8255		= 2,
	},
	[BOARD_ADLINK_PCI7296] = {
		.name		= "adl_pci-7296",
		.dio_badr	= 2,
		.n_8255		= 4,
	},
	[BOARD_CB_PCIDIO24] = {
		.name		= "cb_pci-dio24",
		.dio_badr	= 2,
		.n_8255		= 1,
	},
	[BOARD_CB_PCIDIO24H] = {
		.name		= "cb_pci-dio24h",
		.dio_badr	= 2,
		.n_8255		= 1,
	},
	[BOARD_CB_PCIDIO48H_OLD] = {
		.name		= "cb_pci-dio48h",
		.dio_badr	= 1,
		.n_8255		= 2,
	},
	[BOARD_CB_PCIDIO48H_NEW] = {
		.name		= "cb_pci-dio48h",
		.dio_badr	= 2,
		.n_8255		= 2,
	},
	[BOARD_CB_PCIDIO96H] = {
		.name		= "cb_pci-dio96h",
		.dio_badr	= 2,
		.n_8255		= 4,
	},
	[BOARD_NI_PCIDIO96] = {
		.name		= "ni_pci-dio-96",
		.dio_badr	= 1,
		.n_8255		= 4,
		.has_mite	= 1,
	},
	[BOARD_NI_PCIDIO96B] = {
		.name		= "ni_pci-dio-96b",
		.dio_badr	= 1,
		.n_8255		= 4,
		.has_mite	= 1,
	},
	[BOARD_NI_PXI6508] = {
		.name		= "ni_pxi-6508",
		.dio_badr	= 1,
		.n_8255		= 4,
		.has_mite	= 1,
	},
	[BOARD_NI_PCI6503] = {
		.name		= "ni_pci-6503",
		.dio_badr	= 1,
		.n_8255		= 1,
		.has_mite	= 1,
	},
	[BOARD_NI_PCI6503B] = {
		.name		= "ni_pci-6503b",
		.dio_badr	= 1,
		.n_8255		= 1,
		.has_mite	= 1,
	},
	[BOARD_NI_PCI6503X] = {
		.name		= "ni_pci-6503x",
		.dio_badr	= 1,
		.n_8255		= 1,
		.has_mite	= 1,
	},
	[BOARD_NI_PXI_6503] = {
		.name		= "ni_pxi-6503",
		.dio_badr	= 1,
		.n_8255		= 1,
		.has_mite	= 1,
	},
};

 
#define MITE_IODWBSR	0xc0	 
#define WENAB		BIT(7)	 

static int pci_8255_mite_init(struct pci_dev *pcidev)
{
	void __iomem *mite_base;
	u32 main_phys_addr;

	 
	mite_base = pci_ioremap_bar(pcidev, 0);
	if (!mite_base)
		return -ENOMEM;

	 
	main_phys_addr = pci_resource_start(pcidev, 1);
	writel(main_phys_addr | WENAB, mite_base + MITE_IODWBSR);

	 
	iounmap(mite_base);
	return 0;
}

static int pci_8255_auto_attach(struct comedi_device *dev,
				unsigned long context)
{
	struct pci_dev *pcidev = comedi_to_pci_dev(dev);
	const struct pci_8255_boardinfo *board = NULL;
	struct comedi_subdevice *s;
	int ret;
	int i;

	if (context < ARRAY_SIZE(pci_8255_boards))
		board = &pci_8255_boards[context];
	if (!board)
		return -ENODEV;
	dev->board_ptr = board;
	dev->board_name = board->name;

	ret = comedi_pci_enable(dev);
	if (ret)
		return ret;

	if (board->has_mite) {
		ret = pci_8255_mite_init(pcidev);
		if (ret)
			return ret;
	}

	if ((pci_resource_flags(pcidev, board->dio_badr) & IORESOURCE_MEM)) {
		dev->mmio = pci_ioremap_bar(pcidev, board->dio_badr);
		if (!dev->mmio)
			return -ENOMEM;
	} else {
		dev->iobase = pci_resource_start(pcidev, board->dio_badr);
	}

	 
	ret = comedi_alloc_subdevices(dev, board->n_8255);
	if (ret)
		return ret;

	for (i = 0; i < board->n_8255; i++) {
		s = &dev->subdevices[i];
		if (dev->mmio)
			ret = subdev_8255_mm_init(dev, s, NULL, i * I8255_SIZE);
		else
			ret = subdev_8255_init(dev, s, NULL, i * I8255_SIZE);
		if (ret)
			return ret;
	}

	return 0;
}

static struct comedi_driver pci_8255_driver = {
	.driver_name	= "8255_pci",
	.module		= THIS_MODULE,
	.auto_attach	= pci_8255_auto_attach,
	.detach		= comedi_pci_detach,
};

static int pci_8255_pci_probe(struct pci_dev *dev,
			      const struct pci_device_id *id)
{
	return comedi_pci_auto_config(dev, &pci_8255_driver, id->driver_data);
}

static const struct pci_device_id pci_8255_pci_table[] = {
	{ PCI_VDEVICE(ADLINK, 0x7224), BOARD_ADLINK_PCI7224 },
	{ PCI_VDEVICE(ADLINK, 0x7248), BOARD_ADLINK_PCI7248 },
	{ PCI_VDEVICE(ADLINK, 0x7296), BOARD_ADLINK_PCI7296 },
	{ PCI_VDEVICE(CB, 0x0028), BOARD_CB_PCIDIO24 },
	{ PCI_VDEVICE(CB, 0x0014), BOARD_CB_PCIDIO24H },
	{ PCI_DEVICE_SUB(PCI_VENDOR_ID_CB, 0x000b, 0x0000, 0x0000),
	  .driver_data = BOARD_CB_PCIDIO48H_OLD },
	{ PCI_DEVICE_SUB(PCI_VENDOR_ID_CB, 0x000b, PCI_VENDOR_ID_CB, 0x000b),
	  .driver_data = BOARD_CB_PCIDIO48H_NEW },
	{ PCI_VDEVICE(CB, 0x0017), BOARD_CB_PCIDIO96H },
	{ PCI_VDEVICE(NI, 0x0160), BOARD_NI_PCIDIO96 },
	{ PCI_VDEVICE(NI, 0x1630), BOARD_NI_PCIDIO96B },
	{ PCI_VDEVICE(NI, 0x13c0), BOARD_NI_PXI6508 },
	{ PCI_VDEVICE(NI, 0x0400), BOARD_NI_PCI6503 },
	{ PCI_VDEVICE(NI, 0x1250), BOARD_NI_PCI6503B },
	{ PCI_VDEVICE(NI, 0x17d0), BOARD_NI_PCI6503X },
	{ PCI_VDEVICE(NI, 0x1800), BOARD_NI_PXI_6503 },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, pci_8255_pci_table);

static struct pci_driver pci_8255_pci_driver = {
	.name		= "8255_pci",
	.id_table	= pci_8255_pci_table,
	.probe		= pci_8255_pci_probe,
	.remove		= comedi_pci_auto_unconfig,
};
module_comedi_pci_driver(pci_8255_driver, pci_8255_pci_driver);

MODULE_DESCRIPTION("COMEDI - Generic PCI based 8255 Digital I/O boards");
MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_LICENSE("GPL");
