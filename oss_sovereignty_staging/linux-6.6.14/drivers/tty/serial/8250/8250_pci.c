
 
#undef DEBUG
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/math.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/tty.h>
#include <linux/serial_reg.h>
#include <linux/serial_core.h>
#include <linux/8250_pci.h>
#include <linux/bitops.h>

#include <asm/byteorder.h>
#include <asm/io.h>

#include "8250.h"
#include "8250_pcilib.h"

 
struct pci_serial_quirk {
	u32	vendor;
	u32	device;
	u32	subvendor;
	u32	subdevice;
	int	(*probe)(struct pci_dev *dev);
	int	(*init)(struct pci_dev *dev);
	int	(*setup)(struct serial_private *,
			 const struct pciserial_board *,
			 struct uart_8250_port *, int);
	void	(*exit)(struct pci_dev *dev);
};

struct f815xxa_data {
	spinlock_t lock;
	int idx;
};

struct serial_private {
	struct pci_dev		*dev;
	unsigned int		nr;
	struct pci_serial_quirk	*quirk;
	const struct pciserial_board *board;
	int			line[];
};

#define PCI_DEVICE_ID_HPE_PCI_SERIAL	0x37e

static const struct pci_device_id pci_use_msi[] = {
	{ PCI_DEVICE_SUB(PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9900,
			 0xA000, 0x1000) },
	{ PCI_DEVICE_SUB(PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9912,
			 0xA000, 0x1000) },
	{ PCI_DEVICE_SUB(PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9922,
			 0xA000, 0x1000) },
	{ PCI_DEVICE_SUB(PCI_VENDOR_ID_ASIX, PCI_DEVICE_ID_ASIX_AX99100,
			 0xA000, 0x1000) },
	{ PCI_DEVICE_SUB(PCI_VENDOR_ID_HP_3PAR, PCI_DEVICE_ID_HPE_PCI_SERIAL,
			 PCI_ANY_ID, PCI_ANY_ID) },
	{ }
};

static int pci_default_setup(struct serial_private*,
	  const struct pciserial_board*, struct uart_8250_port *, int);

static void moan_device(const char *str, struct pci_dev *dev)
{
	pci_err(dev, "%s\n"
	       "Please send the output of lspci -vv, this\n"
	       "message (0x%04x,0x%04x,0x%04x,0x%04x), the\n"
	       "manufacturer and name of serial board or\n"
	       "modem board to <linux-serial@vger.kernel.org>.\n",
	       str, dev->vendor, dev->device,
	       dev->subsystem_vendor, dev->subsystem_device);
}

static int
setup_port(struct serial_private *priv, struct uart_8250_port *port,
	   u8 bar, unsigned int offset, int regshift)
{
	return serial8250_pci_setup_port(priv->dev, port, bar, offset, regshift);
}

 
static int addidata_apci7800_setup(struct serial_private *priv,
				const struct pciserial_board *board,
				struct uart_8250_port *port, int idx)
{
	unsigned int bar = 0, offset = board->first_offset;
	bar = FL_GET_BASE(board->flags);

	if (idx < 2) {
		offset += idx * board->uart_offset;
	} else if ((idx >= 2) && (idx < 4)) {
		bar += 1;
		offset += ((idx - 2) * board->uart_offset);
	} else if ((idx >= 4) && (idx < 6)) {
		bar += 2;
		offset += ((idx - 4) * board->uart_offset);
	} else if (idx >= 6) {
		bar += 3;
		offset += ((idx - 6) * board->uart_offset);
	}

	return setup_port(priv, port, bar, offset, board->reg_shift);
}

 
static int
afavlab_setup(struct serial_private *priv, const struct pciserial_board *board,
	      struct uart_8250_port *port, int idx)
{
	unsigned int bar, offset = board->first_offset;

	bar = FL_GET_BASE(board->flags);
	if (idx < 4)
		bar += idx;
	else {
		bar = 4;
		offset += (idx - 4) * board->uart_offset;
	}

	return setup_port(priv, port, bar, offset, board->reg_shift);
}

 
static int pci_hp_diva_init(struct pci_dev *dev)
{
	int rc = 0;

	switch (dev->subsystem_device) {
	case PCI_DEVICE_ID_HP_DIVA_TOSCA1:
	case PCI_DEVICE_ID_HP_DIVA_HALFDOME:
	case PCI_DEVICE_ID_HP_DIVA_KEYSTONE:
	case PCI_DEVICE_ID_HP_DIVA_EVEREST:
		rc = 3;
		break;
	case PCI_DEVICE_ID_HP_DIVA_TOSCA2:
		rc = 2;
		break;
	case PCI_DEVICE_ID_HP_DIVA_MAESTRO:
		rc = 4;
		break;
	case PCI_DEVICE_ID_HP_DIVA_POWERBAR:
	case PCI_DEVICE_ID_HP_DIVA_HURRICANE:
		rc = 1;
		break;
	}

	return rc;
}

 
static int
pci_hp_diva_setup(struct serial_private *priv,
		const struct pciserial_board *board,
		struct uart_8250_port *port, int idx)
{
	unsigned int offset = board->first_offset;
	unsigned int bar = FL_GET_BASE(board->flags);

	switch (priv->dev->subsystem_device) {
	case PCI_DEVICE_ID_HP_DIVA_MAESTRO:
		if (idx == 3)
			idx++;
		break;
	case PCI_DEVICE_ID_HP_DIVA_EVEREST:
		if (idx > 0)
			idx++;
		if (idx > 2)
			idx++;
		break;
	}
	if (idx > 2)
		offset = 0x18;

	offset += idx * board->uart_offset;

	return setup_port(priv, port, bar, offset, board->reg_shift);
}

 
static int pci_inteli960ni_init(struct pci_dev *dev)
{
	u32 oldval;

	if (!(dev->subsystem_device & 0x1000))
		return -ENODEV;

	 
	pci_read_config_dword(dev, 0x44, &oldval);
	if (oldval == 0x00001000L) {  
		pci_dbg(dev, "Local i960 firmware missing\n");
		return -ENODEV;
	}
	return 0;
}

 
static int pci_plx9050_init(struct pci_dev *dev)
{
	u8 irq_config;
	void __iomem *p;

	if ((pci_resource_flags(dev, 0) & IORESOURCE_MEM) == 0) {
		moan_device("no memory in bar 0", dev);
		return 0;
	}

	irq_config = 0x41;
	if (dev->vendor == PCI_VENDOR_ID_PANACOM ||
	    dev->subsystem_vendor == PCI_SUBVENDOR_ID_EXSYS)
		irq_config = 0x43;

	if ((dev->vendor == PCI_VENDOR_ID_PLX) &&
	    (dev->device == PCI_DEVICE_ID_PLX_ROMULUS))
		 
		irq_config = 0x5b;
	 
	p = ioremap(pci_resource_start(dev, 0), 0x80);
	if (p == NULL)
		return -ENOMEM;
	writel(irq_config, p + 0x4c);

	 
	readl(p + 0x4c);
	iounmap(p);

	return 0;
}

static void pci_plx9050_exit(struct pci_dev *dev)
{
	u8 __iomem *p;

	if ((pci_resource_flags(dev, 0) & IORESOURCE_MEM) == 0)
		return;

	 
	p = ioremap(pci_resource_start(dev, 0), 0x80);
	if (p != NULL) {
		writel(0, p + 0x4c);

		 
		readl(p + 0x4c);
		iounmap(p);
	}
}

#define NI8420_INT_ENABLE_REG	0x38
#define NI8420_INT_ENABLE_BIT	0x2000

static void pci_ni8420_exit(struct pci_dev *dev)
{
	void __iomem *p;
	unsigned int bar = 0;

	if ((pci_resource_flags(dev, bar) & IORESOURCE_MEM) == 0) {
		moan_device("no memory in bar", dev);
		return;
	}

	p = pci_ioremap_bar(dev, bar);
	if (p == NULL)
		return;

	 
	writel(readl(p + NI8420_INT_ENABLE_REG) & ~(NI8420_INT_ENABLE_BIT),
	       p + NI8420_INT_ENABLE_REG);
	iounmap(p);
}


 
#define MITE_IOWBSR1	0xc4
#define MITE_IOWCR1	0xf4
#define MITE_LCIMR1	0x08
#define MITE_LCIMR2	0x10

#define MITE_LCIMR2_CLR_CPU_IE	(1 << 30)

static void pci_ni8430_exit(struct pci_dev *dev)
{
	void __iomem *p;
	unsigned int bar = 0;

	if ((pci_resource_flags(dev, bar) & IORESOURCE_MEM) == 0) {
		moan_device("no memory in bar", dev);
		return;
	}

	p = pci_ioremap_bar(dev, bar);
	if (p == NULL)
		return;

	 
	writel(MITE_LCIMR2_CLR_CPU_IE, p + MITE_LCIMR2);
	iounmap(p);
}

 
static int
sbs_setup(struct serial_private *priv, const struct pciserial_board *board,
		struct uart_8250_port *port, int idx)
{
	unsigned int bar, offset = board->first_offset;

	bar = 0;

	if (idx < 4) {
		 
		offset += idx * board->uart_offset;
	} else if (idx < 8) {
		 
		offset += idx * board->uart_offset + 0xC00;
	} else  
		return 1;

	return setup_port(priv, port, bar, offset, board->reg_shift);
}

 

 
#define OCT_REG_CR_OFF		0x500

static int sbs_init(struct pci_dev *dev)
{
	u8 __iomem *p;

	p = pci_ioremap_bar(dev, 0);

	if (p == NULL)
		return -ENOMEM;
	 
	writeb(0x10, p + OCT_REG_CR_OFF);
	udelay(50);
	writeb(0x0, p + OCT_REG_CR_OFF);

	 
	writeb(0x4, p + OCT_REG_CR_OFF);
	iounmap(p);

	return 0;
}

 

static void sbs_exit(struct pci_dev *dev)
{
	u8 __iomem *p;

	p = pci_ioremap_bar(dev, 0);
	 
	if (p != NULL)
		writeb(0, p + OCT_REG_CR_OFF);
	iounmap(p);
}

 

#define PCI_DEVICE_ID_SIIG_1S_10x (PCI_DEVICE_ID_SIIG_1S_10x_550 & 0xfffc)
#define PCI_DEVICE_ID_SIIG_2S_10x (PCI_DEVICE_ID_SIIG_2S_10x_550 & 0xfff8)

static int pci_siig10x_init(struct pci_dev *dev)
{
	u16 data;
	void __iomem *p;

	switch (dev->device & 0xfff8) {
	case PCI_DEVICE_ID_SIIG_1S_10x:	 
		data = 0xffdf;
		break;
	case PCI_DEVICE_ID_SIIG_2S_10x:	 
		data = 0xf7ff;
		break;
	default:			 
		data = 0xfffb;
		break;
	}

	p = ioremap(pci_resource_start(dev, 0), 0x80);
	if (p == NULL)
		return -ENOMEM;

	writew(readw(p + 0x28) & data, p + 0x28);
	readw(p + 0x28);
	iounmap(p);
	return 0;
}

#define PCI_DEVICE_ID_SIIG_2S_20x (PCI_DEVICE_ID_SIIG_2S_20x_550 & 0xfffc)
#define PCI_DEVICE_ID_SIIG_2S1P_20x (PCI_DEVICE_ID_SIIG_2S1P_20x_550 & 0xfffc)

static int pci_siig20x_init(struct pci_dev *dev)
{
	u8 data;

	 
	pci_read_config_byte(dev, 0x6f, &data);
	pci_write_config_byte(dev, 0x6f, data & 0xef);

	 
	if (((dev->device & 0xfffc) == PCI_DEVICE_ID_SIIG_2S_20x) ||
	    ((dev->device & 0xfffc) == PCI_DEVICE_ID_SIIG_2S1P_20x)) {
		pci_read_config_byte(dev, 0x73, &data);
		pci_write_config_byte(dev, 0x73, data & 0xef);
	}
	return 0;
}

static int pci_siig_init(struct pci_dev *dev)
{
	unsigned int type = dev->device & 0xff00;

	if (type == 0x1000)
		return pci_siig10x_init(dev);
	if (type == 0x2000)
		return pci_siig20x_init(dev);

	moan_device("Unknown SIIG card", dev);
	return -ENODEV;
}

static int pci_siig_setup(struct serial_private *priv,
			  const struct pciserial_board *board,
			  struct uart_8250_port *port, int idx)
{
	unsigned int bar = FL_GET_BASE(board->flags) + idx, offset = 0;

	if (idx > 3) {
		bar = 4;
		offset = (idx - 4) * 8;
	}

	return setup_port(priv, port, bar, offset, 0);
}

 
static const unsigned short timedia_single_port[] = {
	0x4025, 0x4027, 0x4028, 0x5025, 0x5027, 0
};

static const unsigned short timedia_dual_port[] = {
	0x0002, 0x4036, 0x4037, 0x4038, 0x4078, 0x4079, 0x4085,
	0x4088, 0x4089, 0x5037, 0x5078, 0x5079, 0x5085, 0x6079,
	0x7079, 0x8079, 0x8137, 0x8138, 0x8237, 0x8238, 0x9079,
	0x9137, 0x9138, 0x9237, 0x9238, 0xA079, 0xB079, 0xC079,
	0xD079, 0
};

static const unsigned short timedia_quad_port[] = {
	0x4055, 0x4056, 0x4095, 0x4096, 0x5056, 0x8156, 0x8157,
	0x8256, 0x8257, 0x9056, 0x9156, 0x9157, 0x9158, 0x9159,
	0x9256, 0x9257, 0xA056, 0xA157, 0xA158, 0xA159, 0xB056,
	0xB157, 0
};

static const unsigned short timedia_eight_port[] = {
	0x4065, 0x4066, 0x5065, 0x5066, 0x8166, 0x9066, 0x9166,
	0x9167, 0x9168, 0xA066, 0xA167, 0xA168, 0
};

static const struct timedia_struct {
	int num;
	const unsigned short *ids;
} timedia_data[] = {
	{ 1, timedia_single_port },
	{ 2, timedia_dual_port },
	{ 4, timedia_quad_port },
	{ 8, timedia_eight_port }
};

 
static int pci_timedia_probe(struct pci_dev *dev)
{
	 
	if ((dev->subsystem_device & 0x00f0) >= 0x70) {
		pci_info(dev, "ignoring Timedia subdevice %04x for parport_serial\n",
			 dev->subsystem_device);
		return -ENODEV;
	}

	return 0;
}

static int pci_timedia_init(struct pci_dev *dev)
{
	const unsigned short *ids;
	int i, j;

	for (i = 0; i < ARRAY_SIZE(timedia_data); i++) {
		ids = timedia_data[i].ids;
		for (j = 0; ids[j]; j++)
			if (dev->subsystem_device == ids[j])
				return timedia_data[i].num;
	}
	return 0;
}

 
static int
pci_timedia_setup(struct serial_private *priv,
		  const struct pciserial_board *board,
		  struct uart_8250_port *port, int idx)
{
	unsigned int bar = 0, offset = board->first_offset;

	switch (idx) {
	case 0:
		bar = 0;
		break;
	case 1:
		offset = board->uart_offset;
		bar = 0;
		break;
	case 2:
		bar = 1;
		break;
	case 3:
		offset = board->uart_offset;
		fallthrough;
	case 4:  
	case 5:  
	case 6:  
	case 7:  
		bar = idx - 2;
	}

	return setup_port(priv, port, bar, offset, board->reg_shift);
}

 
static int
titan_400l_800l_setup(struct serial_private *priv,
		      const struct pciserial_board *board,
		      struct uart_8250_port *port, int idx)
{
	unsigned int bar, offset = board->first_offset;

	switch (idx) {
	case 0:
		bar = 1;
		break;
	case 1:
		bar = 2;
		break;
	default:
		bar = 4;
		offset = (idx - 2) * board->uart_offset;
	}

	return setup_port(priv, port, bar, offset, board->reg_shift);
}

static int pci_xircom_init(struct pci_dev *dev)
{
	msleep(100);
	return 0;
}

static int pci_ni8420_init(struct pci_dev *dev)
{
	void __iomem *p;
	unsigned int bar = 0;

	if ((pci_resource_flags(dev, bar) & IORESOURCE_MEM) == 0) {
		moan_device("no memory in bar", dev);
		return 0;
	}

	p = pci_ioremap_bar(dev, bar);
	if (p == NULL)
		return -ENOMEM;

	 
	writel(readl(p + NI8420_INT_ENABLE_REG) | NI8420_INT_ENABLE_BIT,
	       p + NI8420_INT_ENABLE_REG);

	iounmap(p);
	return 0;
}

#define MITE_IOWBSR1_WSIZE	0xa
#define MITE_IOWBSR1_WIN_OFFSET	0x800
#define MITE_IOWBSR1_WENAB	(1 << 7)
#define MITE_LCIMR1_IO_IE_0	(1 << 24)
#define MITE_LCIMR2_SET_CPU_IE	(1 << 31)
#define MITE_IOWCR1_RAMSEL_MASK	0xfffffffe

static int pci_ni8430_init(struct pci_dev *dev)
{
	void __iomem *p;
	struct pci_bus_region region;
	u32 device_window;
	unsigned int bar = 0;

	if ((pci_resource_flags(dev, bar) & IORESOURCE_MEM) == 0) {
		moan_device("no memory in bar", dev);
		return 0;
	}

	p = pci_ioremap_bar(dev, bar);
	if (p == NULL)
		return -ENOMEM;

	 
	pcibios_resource_to_bus(dev->bus, &region, &dev->resource[bar]);
	device_window = ((region.start + MITE_IOWBSR1_WIN_OFFSET) & 0xffffff00)
			| MITE_IOWBSR1_WENAB | MITE_IOWBSR1_WSIZE;
	writel(device_window, p + MITE_IOWBSR1);

	 
	writel((readl(p + MITE_IOWCR1) & MITE_IOWCR1_RAMSEL_MASK),
	       p + MITE_IOWCR1);

	 
	writel(MITE_LCIMR1_IO_IE_0, p + MITE_LCIMR1);

	 
	writel(MITE_LCIMR2_SET_CPU_IE, p + MITE_LCIMR2);

	iounmap(p);
	return 0;
}

 
#define NI8430_PORTCON	0x0f
#define NI8430_PORTCON_TXVR_ENABLE	(1 << 3)

static int
pci_ni8430_setup(struct serial_private *priv,
		 const struct pciserial_board *board,
		 struct uart_8250_port *port, int idx)
{
	struct pci_dev *dev = priv->dev;
	void __iomem *p;
	unsigned int bar, offset = board->first_offset;

	if (idx >= board->num_ports)
		return 1;

	bar = FL_GET_BASE(board->flags);
	offset += idx * board->uart_offset;

	p = pci_ioremap_bar(dev, bar);
	if (!p)
		return -ENOMEM;

	 
	writeb(readb(p + offset + NI8430_PORTCON) | NI8430_PORTCON_TXVR_ENABLE,
	       p + offset + NI8430_PORTCON);

	iounmap(p);

	return setup_port(priv, port, bar, offset, board->reg_shift);
}

static int pci_netmos_9900_setup(struct serial_private *priv,
				const struct pciserial_board *board,
				struct uart_8250_port *port, int idx)
{
	unsigned int bar;

	if ((priv->dev->device != PCI_DEVICE_ID_NETMOS_9865) &&
	    (priv->dev->subsystem_device & 0xff00) == 0x3000) {
		 
		bar = 3 * idx;

		return setup_port(priv, port, bar, 0, board->reg_shift);
	}

	return pci_default_setup(priv, board, port, idx);
}

 
static int pci_netmos_9900_numports(struct pci_dev *dev)
{
	unsigned int c = dev->class;
	unsigned int pi;
	unsigned short sub_serports;

	pi = c & 0xff;

	if (pi == 2)
		return 1;

	if ((pi == 0) && (dev->device == PCI_DEVICE_ID_NETMOS_9900)) {
		 
		sub_serports = dev->subsystem_device & 0xf;
		if (sub_serports > 0)
			return sub_serports;

		pci_err(dev, "NetMos/Mostech serial driver ignoring port on ambiguous config.\n");
		return 0;
	}

	moan_device("unknown NetMos/Mostech program interface", dev);
	return 0;
}

static int pci_netmos_init(struct pci_dev *dev)
{
	 
	unsigned int num_serial = dev->subsystem_device & 0xf;

	if ((dev->device == PCI_DEVICE_ID_NETMOS_9901) ||
		(dev->device == PCI_DEVICE_ID_NETMOS_9865))
		return 0;

	if (dev->subsystem_vendor == PCI_VENDOR_ID_IBM &&
			dev->subsystem_device == 0x0299)
		return 0;

	switch (dev->device) {  
	case PCI_DEVICE_ID_NETMOS_9904:
	case PCI_DEVICE_ID_NETMOS_9912:
	case PCI_DEVICE_ID_NETMOS_9922:
	case PCI_DEVICE_ID_NETMOS_9900:
		num_serial = pci_netmos_9900_numports(dev);
		break;

	default:
		break;
	}

	if (num_serial == 0) {
		moan_device("unknown NetMos/Mostech device", dev);
		return -ENODEV;
	}

	return num_serial;
}

 

 
#define ITE_887x_MISCR		0x9c
#define ITE_887x_INTCBAR	0x78
#define ITE_887x_UARTBAR	0x7c
#define ITE_887x_PS0BAR		0x10
#define ITE_887x_POSIO0		0x60

 
#define ITE_887x_IOSIZE		32
 
#define ITE_887x_POSIO_IOSIZE_8		(3 << 24)
 
#define ITE_887x_POSIO_IOSIZE_32	(5 << 24)
 
#define ITE_887x_POSIO_SPEED		(3 << 29)
 
#define ITE_887x_POSIO_ENABLE		(1 << 31)

 
static const short inta_addr[] = { 0x2a0, 0x2c0, 0x220, 0x240, 0x1e0, 0x200, 0x280 };
static int pci_ite887x_init(struct pci_dev *dev)
{
	int ret, i, type;
	struct resource *iobase = NULL;
	u32 miscr, uartbar, ioport;

	 
	for (i = 0; i < ARRAY_SIZE(inta_addr); i++) {
		iobase = request_region(inta_addr[i], ITE_887x_IOSIZE,
								"ite887x");
		if (iobase != NULL) {
			 
			pci_write_config_dword(dev, ITE_887x_POSIO0,
				ITE_887x_POSIO_ENABLE | ITE_887x_POSIO_SPEED |
				ITE_887x_POSIO_IOSIZE_32 | inta_addr[i]);
			 
			pci_write_config_dword(dev, ITE_887x_INTCBAR,
								inta_addr[i]);
			ret = inb(inta_addr[i]);
			if (ret != 0xff) {
				 
				break;
			}
			release_region(iobase->start, ITE_887x_IOSIZE);
		}
	}

	if (i == ARRAY_SIZE(inta_addr)) {
		pci_err(dev, "could not find iobase\n");
		return -ENODEV;
	}

	 
	type = inb(iobase->start + 0x18) & 0x0f;

	switch (type) {
	case 0x2:	 
	case 0xa:	 
		ret = 0;
		break;
	case 0xe:	 
		ret = 2;
		break;
	case 0x6:	 
		ret = 1;
		break;
	case 0x8:	 
		ret = 2;
		break;
	default:
		moan_device("Unknown ITE887x", dev);
		ret = -ENODEV;
	}

	 
	for (i = 0; i < ret; i++) {
		 
		pci_read_config_dword(dev, ITE_887x_PS0BAR + (0x4 * (i + 1)),
								&ioport);
		ioport &= 0x0000FF00;	 
		pci_write_config_dword(dev, ITE_887x_POSIO0 + (0x4 * (i + 1)),
			ITE_887x_POSIO_ENABLE | ITE_887x_POSIO_SPEED |
			ITE_887x_POSIO_IOSIZE_8 | ioport);

		 
		pci_read_config_dword(dev, ITE_887x_UARTBAR, &uartbar);
		uartbar &= ~(0xffff << (16 * i));	 
		uartbar |= (ioport << (16 * i));	 
		pci_write_config_dword(dev, ITE_887x_UARTBAR, uartbar);

		 
		pci_read_config_dword(dev, ITE_887x_MISCR, &miscr);
		 
		miscr &= ~(0xf << (12 - 4 * i));
		 
		miscr |= 1 << (23 - i);
		 
		pci_write_config_dword(dev, ITE_887x_MISCR, miscr);
	}

	if (ret <= 0) {
		 
		release_region(iobase->start, ITE_887x_IOSIZE);
	}

	return ret;
}

static void pci_ite887x_exit(struct pci_dev *dev)
{
	u32 ioport;
	 
	pci_read_config_dword(dev, ITE_887x_POSIO0, &ioport);
	ioport &= 0xffff;
	release_region(ioport, ITE_887x_IOSIZE);
}

 
#define PCI_VENDOR_ID_ENDRUN			0x7401
#define PCI_DEVICE_ID_ENDRUN_1588	0xe100

static bool pci_oxsemi_tornado_p(struct pci_dev *dev)
{
	 
	if (dev->vendor == PCI_VENDOR_ID_OXSEMI &&
	    (dev->device & 0xf000) != 0xc000)
		return false;

	 
	if (dev->vendor == PCI_VENDOR_ID_ENDRUN &&
	    (dev->device & 0xf000) != 0xe000)
		return false;

	return true;
}

 
static int pci_oxsemi_tornado_init(struct pci_dev *dev)
{
	u8 __iomem *p;
	unsigned long deviceID;
	unsigned int  number_uarts = 0;

	if (!pci_oxsemi_tornado_p(dev))
		return 0;

	p = pci_iomap(dev, 0, 5);
	if (p == NULL)
		return -ENOMEM;

	deviceID = ioread32(p);
	 
	if (deviceID == 0x07000200) {
		number_uarts = ioread8(p + 4);
		pci_dbg(dev, "%d ports detected on %s PCI Express device\n",
			number_uarts,
			dev->vendor == PCI_VENDOR_ID_ENDRUN ?
			"EndRun" : "Oxford");
	}
	pci_iounmap(dev, p);
	return number_uarts;
}

 
#define OXSEMI_TORNADO_TCR_MASK	0xf
#define OXSEMI_TORNADO_CPR_MASK	0x1ff
#define OXSEMI_TORNADO_CPR_MIN	0x008
#define OXSEMI_TORNADO_CPR_DEF	0x10f

 
static unsigned int pci_oxsemi_tornado_get_divisor(struct uart_port *port,
						   unsigned int baud,
						   unsigned int *frac)
{
	static u8 p[][2] = {
		{ 16, 14, }, { 16, 13, }, { 16, 12, }, { 16, 11, },
		{ 16, 10, }, { 16,  9, }, { 16,  8, }, { 15, 17, },
		{ 15, 16, }, { 15, 15, }, { 15, 14, }, { 15, 13, },
		{ 15, 12, }, { 15, 11, }, { 15, 10, }, { 15,  9, },
		{ 15,  8, }, { 14, 18, }, { 14, 17, }, { 14, 14, },
		{ 14, 13, }, { 14, 12, }, { 14, 11, }, { 14, 10, },
		{ 14,  9, }, { 14,  8, }, { 13, 19, }, { 13, 18, },
		{ 13, 17, }, { 13, 13, }, { 13, 12, }, { 13, 11, },
		{ 13, 10, }, { 13,  9, }, { 13,  8, }, { 12, 19, },
		{ 12, 18, }, { 12, 17, }, { 12, 11, }, { 12,  9, },
		{ 12,  8, }, { 11, 23, }, { 11, 22, }, { 11, 21, },
		{ 11, 20, }, { 11, 19, }, { 11, 18, }, { 11, 17, },
		{ 11, 11, }, { 11, 10, }, { 11,  9, }, { 11,  8, },
		{ 10, 25, }, { 10, 23, }, { 10, 20, }, { 10, 19, },
		{ 10, 17, }, { 10, 10, }, { 10,  9, }, { 10,  8, },
		{  9, 27, }, {  9, 23, }, {  9, 21, }, {  9, 19, },
		{  9, 18, }, {  9, 17, }, {  9,  9, }, {  9,  8, },
		{  8, 31, }, {  8, 29, }, {  8, 23, }, {  8, 19, },
		{  8, 17, }, {  8,  8, }, {  7, 35, }, {  7, 31, },
		{  7, 29, }, {  7, 25, }, {  7, 23, }, {  7, 21, },
		{  7, 19, }, {  7, 17, }, {  7, 15, }, {  7, 14, },
		{  7, 13, }, {  7, 12, }, {  7, 11, }, {  7, 10, },
		{  7,  9, }, {  7,  8, }, {  6, 41, }, {  6, 37, },
		{  6, 31, }, {  6, 29, }, {  6, 23, }, {  6, 19, },
		{  6, 17, }, {  6, 13, }, {  6, 11, }, {  6, 10, },
		{  6,  9, }, {  6,  8, }, {  5, 67, }, {  5, 47, },
		{  5, 43, }, {  5, 41, }, {  5, 37, }, {  5, 31, },
		{  5, 29, }, {  5, 25, }, {  5, 23, }, {  5, 19, },
		{  5, 17, }, {  5, 15, }, {  5, 13, }, {  5, 11, },
		{  5, 10, }, {  5,  9, }, {  5,  8, }, {  4, 61, },
		{  4, 59, }, {  4, 53, }, {  4, 47, }, {  4, 43, },
		{  4, 41, }, {  4, 37, }, {  4, 31, }, {  4, 29, },
		{  4, 23, }, {  4, 19, }, {  4, 17, }, {  4, 13, },
		{  4,  9, }, {  4,  8, },
	};
	 
	const unsigned int quot_scale = 65536;
	unsigned int sclk = port->uartclk * 2;
	unsigned int sdiv = DIV_ROUND_CLOSEST(sclk, baud);
	unsigned int best_squot;
	unsigned int squot;
	unsigned int quot;
	u16 cpr;
	u8 tcr;
	int i;

	 
	if (baud == 38400 && (port->flags & UPF_SPD_MASK) == UPF_SPD_CUST) {
		unsigned int cust_div = port->custom_divisor;

		quot = cust_div & UART_DIV_MAX;
		tcr = (cust_div >> 16) & OXSEMI_TORNADO_TCR_MASK;
		cpr = (cust_div >> 20) & OXSEMI_TORNADO_CPR_MASK;
		if (cpr < OXSEMI_TORNADO_CPR_MIN)
			cpr = OXSEMI_TORNADO_CPR_DEF;
	} else {
		best_squot = quot_scale;
		for (i = 0; i < ARRAY_SIZE(p); i++) {
			unsigned int spre;
			unsigned int srem;
			u8 cp;
			u8 tc;

			tc = p[i][0];
			cp = p[i][1];
			spre = tc * cp;

			srem = sdiv % spre;
			if (srem > spre / 2)
				srem = spre - srem;
			squot = DIV_ROUND_CLOSEST(srem * quot_scale, spre);

			if (srem == 0) {
				tcr = tc;
				cpr = cp;
				quot = sdiv / spre;
				break;
			} else if (squot < best_squot) {
				best_squot = squot;
				tcr = tc;
				cpr = cp;
				quot = DIV_ROUND_CLOSEST(sdiv, spre);
			}
		}
		while (tcr <= (OXSEMI_TORNADO_TCR_MASK + 1) >> 1 &&
		       quot % 2 == 0) {
			quot >>= 1;
			tcr <<= 1;
		}
		while (quot > UART_DIV_MAX) {
			if (tcr <= (OXSEMI_TORNADO_TCR_MASK + 1) >> 1) {
				quot >>= 1;
				tcr <<= 1;
			} else if (cpr <= OXSEMI_TORNADO_CPR_MASK >> 1) {
				quot >>= 1;
				cpr <<= 1;
			} else {
				quot = quot * cpr / OXSEMI_TORNADO_CPR_MASK;
				cpr = OXSEMI_TORNADO_CPR_MASK;
			}
		}
	}

	*frac = (cpr << 8) | (tcr & OXSEMI_TORNADO_TCR_MASK);
	return quot;
}

 
static void pci_oxsemi_tornado_set_divisor(struct uart_port *port,
					   unsigned int baud,
					   unsigned int quot,
					   unsigned int quot_frac)
{
	struct uart_8250_port *up = up_to_u8250p(port);
	u8 cpr2 = quot_frac >> 16;
	u8 cpr = quot_frac >> 8;
	u8 tcr = quot_frac;

	serial_icr_write(up, UART_TCR, tcr);
	serial_icr_write(up, UART_CPR, cpr);
	serial_icr_write(up, UART_CKS, cpr2);
	serial8250_do_set_divisor(port, baud, quot, 0);
}

 
static void pci_oxsemi_tornado_set_mctrl(struct uart_port *port,
					 unsigned int mctrl)
{
	struct uart_8250_port *up = up_to_u8250p(port);

	up->mcr |= UART_MCR_CLKSEL;
	serial8250_do_set_mctrl(port, mctrl);
}

 
static int pci_oxsemi_tornado_setup(struct serial_private *priv,
				    const struct pciserial_board *board,
				    struct uart_8250_port *up, int idx)
{
	struct pci_dev *dev = priv->dev;

	if (pci_oxsemi_tornado_p(dev)) {
		up->port.flags |= UPF_FULL_PROBE;
		up->port.get_divisor = pci_oxsemi_tornado_get_divisor;
		up->port.set_divisor = pci_oxsemi_tornado_set_divisor;
		up->port.set_mctrl = pci_oxsemi_tornado_set_mctrl;
	}

	return pci_default_setup(priv, board, up, idx);
}

#define QPCR_TEST_FOR1		0x3F
#define QPCR_TEST_GET1		0x00
#define QPCR_TEST_FOR2		0x40
#define QPCR_TEST_GET2		0x40
#define QPCR_TEST_FOR3		0x80
#define QPCR_TEST_GET3		0x40
#define QPCR_TEST_FOR4		0xC0
#define QPCR_TEST_GET4		0x80

#define QOPR_CLOCK_X1		0x0000
#define QOPR_CLOCK_X2		0x0001
#define QOPR_CLOCK_X4		0x0002
#define QOPR_CLOCK_X8		0x0003
#define QOPR_CLOCK_RATE_MASK	0x0003

 
static struct pci_device_id quatech_cards[] = {
	{ PCI_DEVICE_DATA(QUATECH, QSC100,   1) },
	{ PCI_DEVICE_DATA(QUATECH, DSC100,   1) },
	{ PCI_DEVICE_DATA(QUATECH, DSC100E,  0) },
	{ PCI_DEVICE_DATA(QUATECH, DSC200,   1) },
	{ PCI_DEVICE_DATA(QUATECH, DSC200E,  0) },
	{ PCI_DEVICE_DATA(QUATECH, ESC100D,  1) },
	{ PCI_DEVICE_DATA(QUATECH, ESC100M,  1) },
	{ PCI_DEVICE_DATA(QUATECH, QSCP100,  1) },
	{ PCI_DEVICE_DATA(QUATECH, DSCP100,  1) },
	{ PCI_DEVICE_DATA(QUATECH, QSCP200,  1) },
	{ PCI_DEVICE_DATA(QUATECH, DSCP200,  1) },
	{ PCI_DEVICE_DATA(QUATECH, ESCLP100, 0) },
	{ PCI_DEVICE_DATA(QUATECH, QSCLP100, 0) },
	{ PCI_DEVICE_DATA(QUATECH, DSCLP100, 0) },
	{ PCI_DEVICE_DATA(QUATECH, SSCLP100, 0) },
	{ PCI_DEVICE_DATA(QUATECH, QSCLP200, 0) },
	{ PCI_DEVICE_DATA(QUATECH, DSCLP200, 0) },
	{ PCI_DEVICE_DATA(QUATECH, SSCLP200, 0) },
	{ PCI_DEVICE_DATA(QUATECH, SPPXP_100, 0) },
	{ 0, }
};

static int pci_quatech_rqopr(struct uart_8250_port *port)
{
	unsigned long base = port->port.iobase;
	u8 LCR, val;

	LCR = inb(base + UART_LCR);
	outb(0xBF, base + UART_LCR);
	val = inb(base + UART_SCR);
	outb(LCR, base + UART_LCR);
	return val;
}

static void pci_quatech_wqopr(struct uart_8250_port *port, u8 qopr)
{
	unsigned long base = port->port.iobase;
	u8 LCR;

	LCR = inb(base + UART_LCR);
	outb(0xBF, base + UART_LCR);
	inb(base + UART_SCR);
	outb(qopr, base + UART_SCR);
	outb(LCR, base + UART_LCR);
}

static int pci_quatech_rqmcr(struct uart_8250_port *port)
{
	unsigned long base = port->port.iobase;
	u8 LCR, val, qmcr;

	LCR = inb(base + UART_LCR);
	outb(0xBF, base + UART_LCR);
	val = inb(base + UART_SCR);
	outb(val | 0x10, base + UART_SCR);
	qmcr = inb(base + UART_MCR);
	outb(val, base + UART_SCR);
	outb(LCR, base + UART_LCR);

	return qmcr;
}

static void pci_quatech_wqmcr(struct uart_8250_port *port, u8 qmcr)
{
	unsigned long base = port->port.iobase;
	u8 LCR, val;

	LCR = inb(base + UART_LCR);
	outb(0xBF, base + UART_LCR);
	val = inb(base + UART_SCR);
	outb(val | 0x10, base + UART_SCR);
	outb(qmcr, base + UART_MCR);
	outb(val, base + UART_SCR);
	outb(LCR, base + UART_LCR);
}

static int pci_quatech_has_qmcr(struct uart_8250_port *port)
{
	unsigned long base = port->port.iobase;
	u8 LCR, val;

	LCR = inb(base + UART_LCR);
	outb(0xBF, base + UART_LCR);
	val = inb(base + UART_SCR);
	if (val & 0x20) {
		outb(0x80, UART_LCR);
		if (!(inb(UART_SCR) & 0x20)) {
			outb(LCR, base + UART_LCR);
			return 1;
		}
	}
	return 0;
}

static int pci_quatech_test(struct uart_8250_port *port)
{
	u8 reg, qopr;

	qopr = pci_quatech_rqopr(port);
	pci_quatech_wqopr(port, qopr & QPCR_TEST_FOR1);
	reg = pci_quatech_rqopr(port) & 0xC0;
	if (reg != QPCR_TEST_GET1)
		return -EINVAL;
	pci_quatech_wqopr(port, (qopr & QPCR_TEST_FOR1)|QPCR_TEST_FOR2);
	reg = pci_quatech_rqopr(port) & 0xC0;
	if (reg != QPCR_TEST_GET2)
		return -EINVAL;
	pci_quatech_wqopr(port, (qopr & QPCR_TEST_FOR1)|QPCR_TEST_FOR3);
	reg = pci_quatech_rqopr(port) & 0xC0;
	if (reg != QPCR_TEST_GET3)
		return -EINVAL;
	pci_quatech_wqopr(port, (qopr & QPCR_TEST_FOR1)|QPCR_TEST_FOR4);
	reg = pci_quatech_rqopr(port) & 0xC0;
	if (reg != QPCR_TEST_GET4)
		return -EINVAL;

	pci_quatech_wqopr(port, qopr);
	return 0;
}

static int pci_quatech_clock(struct uart_8250_port *port)
{
	u8 qopr, reg, set;
	unsigned long clock;

	if (pci_quatech_test(port) < 0)
		return 1843200;

	qopr = pci_quatech_rqopr(port);

	pci_quatech_wqopr(port, qopr & ~QOPR_CLOCK_X8);
	reg = pci_quatech_rqopr(port);
	if (reg & QOPR_CLOCK_X8) {
		clock = 1843200;
		goto out;
	}
	pci_quatech_wqopr(port, qopr | QOPR_CLOCK_X8);
	reg = pci_quatech_rqopr(port);
	if (!(reg & QOPR_CLOCK_X8)) {
		clock = 1843200;
		goto out;
	}
	reg &= QOPR_CLOCK_X8;
	if (reg == QOPR_CLOCK_X2) {
		clock =  3685400;
		set = QOPR_CLOCK_X2;
	} else if (reg == QOPR_CLOCK_X4) {
		clock = 7372800;
		set = QOPR_CLOCK_X4;
	} else if (reg == QOPR_CLOCK_X8) {
		clock = 14745600;
		set = QOPR_CLOCK_X8;
	} else {
		clock = 1843200;
		set = QOPR_CLOCK_X1;
	}
	qopr &= ~QOPR_CLOCK_RATE_MASK;
	qopr |= set;

out:
	pci_quatech_wqopr(port, qopr);
	return clock;
}

static int pci_quatech_rs422(struct uart_8250_port *port)
{
	u8 qmcr;
	int rs422 = 0;

	if (!pci_quatech_has_qmcr(port))
		return 0;
	qmcr = pci_quatech_rqmcr(port);
	pci_quatech_wqmcr(port, 0xFF);
	if (pci_quatech_rqmcr(port))
		rs422 = 1;
	pci_quatech_wqmcr(port, qmcr);
	return rs422;
}

static int pci_quatech_init(struct pci_dev *dev)
{
	const struct pci_device_id *match;
	bool amcc = false;

	match = pci_match_id(quatech_cards, dev);
	if (match)
		amcc = match->driver_data;
	else
		pci_err(dev, "unknown port type '0x%04X'.\n", dev->device);

	if (amcc) {
		unsigned long base = pci_resource_start(dev, 0);
		if (base) {
			u32 tmp;

			outl(inl(base + 0x38) | 0x00002000, base + 0x38);
			tmp = inl(base + 0x3c);
			outl(tmp | 0x01000000, base + 0x3c);
			outl(tmp & ~0x01000000, base + 0x3c);
		}
	}
	return 0;
}

static int pci_quatech_setup(struct serial_private *priv,
		  const struct pciserial_board *board,
		  struct uart_8250_port *port, int idx)
{
	 
	port->port.iobase = pci_resource_start(priv->dev, FL_GET_BASE(board->flags));
	 
	port->port.uartclk = pci_quatech_clock(port);
	 
	if (pci_quatech_rs422(port))
		pci_warn(priv->dev, "software control of RS422 features not currently supported.\n");
	return pci_default_setup(priv, board, port, idx);
}

static int pci_default_setup(struct serial_private *priv,
		  const struct pciserial_board *board,
		  struct uart_8250_port *port, int idx)
{
	unsigned int bar, offset = board->first_offset, maxnr;

	bar = FL_GET_BASE(board->flags);
	if (board->flags & FL_BASE_BARS)
		bar += idx;
	else
		offset += idx * board->uart_offset;

	maxnr = (pci_resource_len(priv->dev, bar) - board->first_offset) >>
		(board->reg_shift + 3);

	if (board->flags & FL_REGION_SZ_CAP && idx >= maxnr)
		return 1;

	return setup_port(priv, port, bar, offset, board->reg_shift);
}

static int
ce4100_serial_setup(struct serial_private *priv,
		  const struct pciserial_board *board,
		  struct uart_8250_port *port, int idx)
{
	int ret;

	ret = setup_port(priv, port, idx, 0, board->reg_shift);
	port->port.iotype = UPIO_MEM32;
	port->port.type = PORT_XSCALE;
	port->port.flags = (port->port.flags | UPF_FIXED_PORT | UPF_FIXED_TYPE);
	port->port.regshift = 2;

	return ret;
}

static int
pci_omegapci_setup(struct serial_private *priv,
		      const struct pciserial_board *board,
		      struct uart_8250_port *port, int idx)
{
	return setup_port(priv, port, 2, idx * 8, 0);
}

static int
pci_brcm_trumanage_setup(struct serial_private *priv,
			 const struct pciserial_board *board,
			 struct uart_8250_port *port, int idx)
{
	int ret = pci_default_setup(priv, board, port, idx);

	port->port.type = PORT_BRCM_TRUMANAGE;
	port->port.flags = (port->port.flags | UPF_FIXED_PORT | UPF_FIXED_TYPE);
	return ret;
}

 
#define FINTEK_RTS_CONTROL_BY_HW	BIT(4)
 
#define FINTEK_RTS_INVERT		BIT(5)

 
static int pci_fintek_rs485_config(struct uart_port *port, struct ktermios *termios,
			       struct serial_rs485 *rs485)
{
	struct pci_dev *pci_dev = to_pci_dev(port->dev);
	u8 setting;
	u8 *index = (u8 *) port->private_data;

	pci_read_config_byte(pci_dev, 0x40 + 8 * *index + 7, &setting);

	if (rs485->flags & SER_RS485_ENABLED) {
		 
		setting |= FINTEK_RTS_CONTROL_BY_HW;

		if (rs485->flags & SER_RS485_RTS_ON_SEND) {
			 
			setting &= ~FINTEK_RTS_INVERT;
		} else {
			 
			setting |= FINTEK_RTS_INVERT;
		}
	} else {
		 
		setting &= ~(FINTEK_RTS_CONTROL_BY_HW | FINTEK_RTS_INVERT);
	}

	pci_write_config_byte(pci_dev, 0x40 + 8 * *index + 7, setting);

	return 0;
}

static const struct serial_rs485 pci_fintek_rs485_supported = {
	.flags = SER_RS485_ENABLED | SER_RS485_RTS_ON_SEND,
	 
};

static int pci_fintek_setup(struct serial_private *priv,
			    const struct pciserial_board *board,
			    struct uart_8250_port *port, int idx)
{
	struct pci_dev *pdev = priv->dev;
	u8 *data;
	u8 config_base;
	u16 iobase;

	config_base = 0x40 + 0x08 * idx;

	 
	pci_read_config_word(pdev, config_base + 4, &iobase);

	pci_dbg(pdev, "idx=%d iobase=0x%x", idx, iobase);

	port->port.iotype = UPIO_PORT;
	port->port.iobase = iobase;
	port->port.rs485_config = pci_fintek_rs485_config;
	port->port.rs485_supported = pci_fintek_rs485_supported;

	data = devm_kzalloc(&pdev->dev, sizeof(u8), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	 
	*data = idx;
	port->port.private_data = data;

	return 0;
}

static int pci_fintek_init(struct pci_dev *dev)
{
	unsigned long iobase;
	u32 max_port, i;
	resource_size_t bar_data[3];
	u8 config_base;
	struct serial_private *priv = pci_get_drvdata(dev);

	if (!(pci_resource_flags(dev, 5) & IORESOURCE_IO) ||
			!(pci_resource_flags(dev, 4) & IORESOURCE_IO) ||
			!(pci_resource_flags(dev, 3) & IORESOURCE_IO))
		return -ENODEV;

	switch (dev->device) {
	case 0x1104:  
	case 0x1108:  
		max_port = dev->device & 0xff;
		break;
	case 0x1112:  
		max_port = 12;
		break;
	default:
		return -EINVAL;
	}

	 
	bar_data[0] = pci_resource_start(dev, 5);
	bar_data[1] = pci_resource_start(dev, 4);
	bar_data[2] = pci_resource_start(dev, 3);

	for (i = 0; i < max_port; ++i) {
		 
		config_base = 0x40 + 0x08 * i;

		 
		iobase = (bar_data[i / 4] & 0xffffffe0) + (i % 4) * 8;

		 
		pci_write_config_byte(dev, config_base + 0x00, 0x01);

		 
		pci_write_config_byte(dev, config_base + 0x01, 0x33);

		 
		pci_write_config_byte(dev, config_base + 0x04,
				(u8)(iobase & 0xff));

		 
		pci_write_config_byte(dev, config_base + 0x05,
				(u8)((iobase & 0xff00) >> 8));

		pci_write_config_byte(dev, config_base + 0x06, dev->irq);

		if (!priv) {
			 
			pci_write_config_byte(dev, config_base + 0x07, 0x01);
		}
	}

	return max_port;
}

static void f815xxa_mem_serial_out(struct uart_port *p, int offset, int value)
{
	struct f815xxa_data *data = p->private_data;
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	writeb(value, p->membase + offset);
	readb(p->membase + UART_SCR);  
	spin_unlock_irqrestore(&data->lock, flags);
}

static int pci_fintek_f815xxa_setup(struct serial_private *priv,
			    const struct pciserial_board *board,
			    struct uart_8250_port *port, int idx)
{
	struct pci_dev *pdev = priv->dev;
	struct f815xxa_data *data;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->idx = idx;
	spin_lock_init(&data->lock);

	port->port.private_data = data;
	port->port.iotype = UPIO_MEM;
	port->port.flags |= UPF_IOREMAP;
	port->port.mapbase = pci_resource_start(pdev, 0) + 8 * idx;
	port->port.serial_out = f815xxa_mem_serial_out;

	return 0;
}

static int pci_fintek_f815xxa_init(struct pci_dev *dev)
{
	u32 max_port, i;
	int config_base;

	if (!(pci_resource_flags(dev, 0) & IORESOURCE_MEM))
		return -ENODEV;

	switch (dev->device) {
	case 0x1204:  
	case 0x1208:  
		max_port = dev->device & 0xff;
		break;
	case 0x1212:  
		max_port = 12;
		break;
	default:
		return -EINVAL;
	}

	 
	pci_write_config_byte(dev, 0x209, 0x40);

	for (i = 0; i < max_port; ++i) {
		 
		config_base = 0x2A0 + 0x08 * i;

		 
		pci_write_config_byte(dev, config_base + 0x01, 0x33);

		 
		pci_write_config_byte(dev, config_base + 0, 0x01);
	}

	return max_port;
}

static int skip_tx_en_setup(struct serial_private *priv,
			const struct pciserial_board *board,
			struct uart_8250_port *port, int idx)
{
	port->port.quirks |= UPQ_NO_TXEN_TEST;
	pci_dbg(priv->dev,
		"serial8250: skipping TxEn test for device [%04x:%04x] subsystem [%04x:%04x]\n",
		priv->dev->vendor, priv->dev->device,
		priv->dev->subsystem_vendor, priv->dev->subsystem_device);

	return pci_default_setup(priv, board, port, idx);
}

static void kt_handle_break(struct uart_port *p)
{
	struct uart_8250_port *up = up_to_u8250p(p);
	 
	serial8250_clear_and_reinit_fifos(up);
}

static unsigned int kt_serial_in(struct uart_port *p, int offset)
{
	struct uart_8250_port *up = up_to_u8250p(p);
	unsigned int val;

	 
	val = inb(p->iobase + offset);
	if (offset == UART_IER) {
		if (val == 0)
			val = up->ier;
	}
	return val;
}

static int kt_serial_setup(struct serial_private *priv,
			   const struct pciserial_board *board,
			   struct uart_8250_port *port, int idx)
{
	port->port.flags |= UPF_BUG_THRE;
	port->port.serial_in = kt_serial_in;
	port->port.handle_break = kt_handle_break;
	return skip_tx_en_setup(priv, board, port, idx);
}

static int pci_eg20t_init(struct pci_dev *dev)
{
#if defined(CONFIG_SERIAL_PCH_UART) || defined(CONFIG_SERIAL_PCH_UART_MODULE)
	return -ENODEV;
#else
	return 0;
#endif
}

static int
pci_wch_ch353_setup(struct serial_private *priv,
		    const struct pciserial_board *board,
		    struct uart_8250_port *port, int idx)
{
	port->port.flags |= UPF_FIXED_TYPE;
	port->port.type = PORT_16550A;
	return pci_default_setup(priv, board, port, idx);
}

static int
pci_wch_ch355_setup(struct serial_private *priv,
		const struct pciserial_board *board,
		struct uart_8250_port *port, int idx)
{
	port->port.flags |= UPF_FIXED_TYPE;
	port->port.type = PORT_16550A;
	return pci_default_setup(priv, board, port, idx);
}

static int
pci_wch_ch38x_setup(struct serial_private *priv,
		    const struct pciserial_board *board,
		    struct uart_8250_port *port, int idx)
{
	port->port.flags |= UPF_FIXED_TYPE;
	port->port.type = PORT_16850;
	return pci_default_setup(priv, board, port, idx);
}


#define CH384_XINT_ENABLE_REG   0xEB
#define CH384_XINT_ENABLE_BIT   0x02

static int pci_wch_ch38x_init(struct pci_dev *dev)
{
	int max_port;
	unsigned long iobase;


	switch (dev->device) {
	case 0x3853:  
		max_port = 8;
		break;
	default:
		return -EINVAL;
	}

	iobase = pci_resource_start(dev, 0);
	outb(CH384_XINT_ENABLE_BIT, iobase + CH384_XINT_ENABLE_REG);

	return max_port;
}

static void pci_wch_ch38x_exit(struct pci_dev *dev)
{
	unsigned long iobase;

	iobase = pci_resource_start(dev, 0);
	outb(0x0, iobase + CH384_XINT_ENABLE_REG);
}


static int
pci_sunix_setup(struct serial_private *priv,
		const struct pciserial_board *board,
		struct uart_8250_port *port, int idx)
{
	int bar;
	int offset;

	port->port.flags |= UPF_FIXED_TYPE;
	port->port.type = PORT_SUNIX;

	if (idx < 4) {
		bar = 0;
		offset = idx * board->uart_offset;
	} else {
		bar = 1;
		idx -= 4;
		idx = div_s64_rem(idx, 4, &offset);
		offset = idx * 64 + offset * board->uart_offset;
	}

	return setup_port(priv, port, bar, offset, 0);
}

static int
pci_moxa_setup(struct serial_private *priv,
		const struct pciserial_board *board,
		struct uart_8250_port *port, int idx)
{
	unsigned int bar = FL_GET_BASE(board->flags);
	int offset;

	if (board->num_ports == 4 && idx == 3)
		offset = 7 * board->uart_offset;
	else
		offset = idx * board->uart_offset;

	return setup_port(priv, port, bar, offset, 0);
}

#define PCI_VENDOR_ID_SBSMODULARIO	0x124B
#define PCI_SUBVENDOR_ID_SBSMODULARIO	0x124B
#define PCI_DEVICE_ID_OCTPRO		0x0001
#define PCI_SUBDEVICE_ID_OCTPRO232	0x0108
#define PCI_SUBDEVICE_ID_OCTPRO422	0x0208
#define PCI_SUBDEVICE_ID_POCTAL232	0x0308
#define PCI_SUBDEVICE_ID_POCTAL422	0x0408
#define PCI_SUBDEVICE_ID_SIIG_DUAL_00	0x2500
#define PCI_SUBDEVICE_ID_SIIG_DUAL_30	0x2530
#define PCI_VENDOR_ID_ADVANTECH		0x13fe
#define PCI_DEVICE_ID_INTEL_CE4100_UART 0x2e66
#define PCI_DEVICE_ID_ADVANTECH_PCI1600	0x1600
#define PCI_DEVICE_ID_ADVANTECH_PCI1600_1611	0x1611
#define PCI_DEVICE_ID_ADVANTECH_PCI3620	0x3620
#define PCI_DEVICE_ID_ADVANTECH_PCI3618	0x3618
#define PCI_DEVICE_ID_ADVANTECH_PCIf618	0xf618
#define PCI_DEVICE_ID_TITAN_200I	0x8028
#define PCI_DEVICE_ID_TITAN_400I	0x8048
#define PCI_DEVICE_ID_TITAN_800I	0x8088
#define PCI_DEVICE_ID_TITAN_800EH	0xA007
#define PCI_DEVICE_ID_TITAN_800EHB	0xA008
#define PCI_DEVICE_ID_TITAN_400EH	0xA009
#define PCI_DEVICE_ID_TITAN_100E	0xA010
#define PCI_DEVICE_ID_TITAN_200E	0xA012
#define PCI_DEVICE_ID_TITAN_400E	0xA013
#define PCI_DEVICE_ID_TITAN_800E	0xA014
#define PCI_DEVICE_ID_TITAN_200EI	0xA016
#define PCI_DEVICE_ID_TITAN_200EISI	0xA017
#define PCI_DEVICE_ID_TITAN_200V3	0xA306
#define PCI_DEVICE_ID_TITAN_400V3	0xA310
#define PCI_DEVICE_ID_TITAN_410V3	0xA312
#define PCI_DEVICE_ID_TITAN_800V3	0xA314
#define PCI_DEVICE_ID_TITAN_800V3B	0xA315
#define PCI_DEVICE_ID_OXSEMI_16PCI958	0x9538
#define PCIE_DEVICE_ID_NEO_2_OX_IBM	0x00F6
#define PCI_DEVICE_ID_PLX_CRONYX_OMEGA	0xc001
#define PCI_DEVICE_ID_INTEL_PATSBURG_KT 0x1d3d
#define PCI_VENDOR_ID_WCH		0x4348
#define PCI_DEVICE_ID_WCH_CH352_2S	0x3253
#define PCI_DEVICE_ID_WCH_CH353_4S	0x3453
#define PCI_DEVICE_ID_WCH_CH353_2S1PF	0x5046
#define PCI_DEVICE_ID_WCH_CH353_1S1P	0x5053
#define PCI_DEVICE_ID_WCH_CH353_2S1P	0x7053
#define PCI_DEVICE_ID_WCH_CH355_4S	0x7173
#define PCI_VENDOR_ID_AGESTAR		0x5372
#define PCI_DEVICE_ID_AGESTAR_9375	0x6872
#define PCI_DEVICE_ID_BROADCOM_TRUMANAGE 0x160a
#define PCI_DEVICE_ID_AMCC_ADDIDATA_APCI7800 0x818e

#define PCIE_VENDOR_ID_WCH		0x1c00
#define PCIE_DEVICE_ID_WCH_CH382_2S1P	0x3250
#define PCIE_DEVICE_ID_WCH_CH384_4S	0x3470
#define PCIE_DEVICE_ID_WCH_CH384_8S	0x3853
#define PCIE_DEVICE_ID_WCH_CH382_2S	0x3253

#define	PCI_DEVICE_ID_MOXA_CP102E	0x1024
#define	PCI_DEVICE_ID_MOXA_CP102EL	0x1025
#define	PCI_DEVICE_ID_MOXA_CP104EL_A	0x1045
#define	PCI_DEVICE_ID_MOXA_CP114EL	0x1144
#define	PCI_DEVICE_ID_MOXA_CP116E_A_A	0x1160
#define	PCI_DEVICE_ID_MOXA_CP116E_A_B	0x1161
#define	PCI_DEVICE_ID_MOXA_CP118EL_A	0x1182
#define	PCI_DEVICE_ID_MOXA_CP118E_A_I	0x1183
#define	PCI_DEVICE_ID_MOXA_CP132EL	0x1322
#define	PCI_DEVICE_ID_MOXA_CP134EL_A	0x1342
#define	PCI_DEVICE_ID_MOXA_CP138E_A	0x1381
#define	PCI_DEVICE_ID_MOXA_CP168EL_A	0x1683

 
#define PCI_SUBDEVICE_ID_UNKNOWN_0x1584	0x1584
#define PCI_SUBDEVICE_ID_UNKNOWN_0x1588	0x1588

 
static struct pci_serial_quirk pci_serial_quirks[] = {
	 
	{
		.vendor         = PCI_VENDOR_ID_AMCC,
		.device         = PCI_DEVICE_ID_AMCC_ADDIDATA_APCI7800,
		.subvendor      = PCI_ANY_ID,
		.subdevice      = PCI_ANY_ID,
		.setup          = addidata_apci7800_setup,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_AFAVLAB,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= afavlab_setup,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_HP,
		.device		= PCI_DEVICE_ID_HP_DIVA,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_hp_diva_init,
		.setup		= pci_hp_diva_setup,
	},
	 
	{
		.vendor         = PCI_VENDOR_ID_HP_3PAR,
		.device         = PCI_DEVICE_ID_HPE_PCI_SERIAL,
		.subvendor      = PCI_ANY_ID,
		.subdevice      = PCI_ANY_ID,
		.setup		= pci_hp_diva_setup,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_80960_RP,
		.subvendor	= 0xe4bf,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_inteli960ni_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_8257X_SOL,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= skip_tx_en_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_82573L_SOL,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= skip_tx_en_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_82573E_SOL,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= skip_tx_en_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_CE4100_UART,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= ce4100_serial_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_PATSBURG_KT,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= kt_serial_setup,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_ITE,
		.device		= PCI_DEVICE_ID_ITE_8872,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ite887x_init,
		.setup		= pci_default_setup,
		.exit		= pci_ite887x_exit,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PCI23216,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= pci_ni8420_exit,
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PCI2328,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= pci_ni8420_exit,
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PCI2324,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= pci_ni8420_exit,
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PCI2322,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= pci_ni8420_exit,
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PCI2324I,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= pci_ni8420_exit,
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PCI2322I,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= pci_ni8420_exit,
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PXI8420_23216,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= pci_ni8420_exit,
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PXI8420_2328,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= pci_ni8420_exit,
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PXI8420_2324,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= pci_ni8420_exit,
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PXI8420_2322,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= pci_ni8420_exit,
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PXI8422_2324,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= pci_ni8420_exit,
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_DEVICE_ID_NI_PXI8422_2322,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8420_init,
		.setup		= pci_default_setup,
		.exit		= pci_ni8420_exit,
	},
	{
		.vendor		= PCI_VENDOR_ID_NI,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_ni8430_init,
		.setup		= pci_ni8430_setup,
		.exit		= pci_ni8430_exit,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_QUATECH,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_quatech_init,
		.setup		= pci_quatech_setup,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_PANACOM,
		.device		= PCI_DEVICE_ID_PANACOM_QUADMODEM,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_plx9050_init,
		.setup		= pci_default_setup,
		.exit		= pci_plx9050_exit,
	},
	{
		.vendor		= PCI_VENDOR_ID_PANACOM,
		.device		= PCI_DEVICE_ID_PANACOM_DUALMODEM,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_plx9050_init,
		.setup		= pci_default_setup,
		.exit		= pci_plx9050_exit,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_PLX,
		.device		= PCI_DEVICE_ID_PLX_9050,
		.subvendor	= PCI_SUBVENDOR_ID_EXSYS,
		.subdevice	= PCI_SUBDEVICE_ID_EXSYS_4055,
		.init		= pci_plx9050_init,
		.setup		= pci_default_setup,
		.exit		= pci_plx9050_exit,
	},
	{
		.vendor		= PCI_VENDOR_ID_PLX,
		.device		= PCI_DEVICE_ID_PLX_9050,
		.subvendor	= PCI_SUBVENDOR_ID_KEYSPAN,
		.subdevice	= PCI_SUBDEVICE_ID_KEYSPAN_SX2,
		.init		= pci_plx9050_init,
		.setup		= pci_default_setup,
		.exit		= pci_plx9050_exit,
	},
	{
		.vendor		= PCI_VENDOR_ID_PLX,
		.device		= PCI_DEVICE_ID_PLX_ROMULUS,
		.subvendor	= PCI_VENDOR_ID_PLX,
		.subdevice	= PCI_DEVICE_ID_PLX_ROMULUS,
		.init		= pci_plx9050_init,
		.setup		= pci_default_setup,
		.exit		= pci_plx9050_exit,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_SBSMODULARIO,
		.device		= PCI_DEVICE_ID_OCTPRO,
		.subvendor	= PCI_SUBVENDOR_ID_SBSMODULARIO,
		.subdevice	= PCI_SUBDEVICE_ID_OCTPRO232,
		.init		= sbs_init,
		.setup		= sbs_setup,
		.exit		= sbs_exit,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_SBSMODULARIO,
		.device		= PCI_DEVICE_ID_OCTPRO,
		.subvendor	= PCI_SUBVENDOR_ID_SBSMODULARIO,
		.subdevice	= PCI_SUBDEVICE_ID_OCTPRO422,
		.init		= sbs_init,
		.setup		= sbs_setup,
		.exit		= sbs_exit,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_SBSMODULARIO,
		.device		= PCI_DEVICE_ID_OCTPRO,
		.subvendor	= PCI_SUBVENDOR_ID_SBSMODULARIO,
		.subdevice	= PCI_SUBDEVICE_ID_POCTAL232,
		.init		= sbs_init,
		.setup		= sbs_setup,
		.exit		= sbs_exit,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_SBSMODULARIO,
		.device		= PCI_DEVICE_ID_OCTPRO,
		.subvendor	= PCI_SUBVENDOR_ID_SBSMODULARIO,
		.subdevice	= PCI_SUBDEVICE_ID_POCTAL422,
		.init		= sbs_init,
		.setup		= sbs_setup,
		.exit		= sbs_exit,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_SIIG,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_siig_init,
		.setup		= pci_siig_setup,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_TITAN,
		.device		= PCI_DEVICE_ID_TITAN_400L,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= titan_400l_800l_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_TITAN,
		.device		= PCI_DEVICE_ID_TITAN_800L,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= titan_400l_800l_setup,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_TIMEDIA,
		.device		= PCI_DEVICE_ID_TIMEDIA_1889,
		.subvendor	= PCI_VENDOR_ID_TIMEDIA,
		.subdevice	= PCI_ANY_ID,
		.probe		= pci_timedia_probe,
		.init		= pci_timedia_init,
		.setup		= pci_timedia_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_TIMEDIA,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_timedia_setup,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_SUNIX,
		.device		= PCI_DEVICE_ID_SUNIX_1999,
		.subvendor	= PCI_VENDOR_ID_SUNIX,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_sunix_setup,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_XIRCOM,
		.device		= PCI_DEVICE_ID_XIRCOM_X3201_MDM,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_xircom_init,
		.setup		= pci_default_setup,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_NETMOS,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_netmos_init,
		.setup		= pci_netmos_9900_setup,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_ENDRUN,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_default_setup,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_OXSEMI,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_MAINPINE,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_DIGI,
		.device		= PCIE_DEVICE_ID_NEO_2_OX_IBM,
		.subvendor		= PCI_SUBVENDOR_ID_IBM,
		.subdevice		= PCI_ANY_ID,
		.init			= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x4027,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x4028,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x4029,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x4019,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x4016,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x4015,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x400A,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x400E,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x400C,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x400B,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x400F,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x4010,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x4011,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x401D,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x401E,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x4013,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x4017,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor		= PCI_VENDOR_ID_INTASHIELD,
		.device		= 0x4018,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_oxsemi_tornado_init,
		.setup		= pci_oxsemi_tornado_setup,
	},
	{
		.vendor         = PCI_VENDOR_ID_INTEL,
		.device         = 0x8811,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor         = PCI_VENDOR_ID_INTEL,
		.device         = 0x8812,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor         = PCI_VENDOR_ID_INTEL,
		.device         = 0x8813,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor         = PCI_VENDOR_ID_INTEL,
		.device         = 0x8814,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor         = 0x10DB,
		.device         = 0x8027,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor         = 0x10DB,
		.device         = 0x8028,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor         = 0x10DB,
		.device         = 0x8029,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor         = 0x10DB,
		.device         = 0x800C,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	{
		.vendor         = 0x10DB,
		.device         = 0x800D,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.init		= pci_eg20t_init,
		.setup		= pci_default_setup,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_PLX,
		.device		= PCI_DEVICE_ID_PLX_CRONYX_OMEGA,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_omegapci_setup,
	},
	 
	{
		.vendor         = PCI_VENDOR_ID_WCH,
		.device         = PCI_DEVICE_ID_WCH_CH353_1S1P,
		.subvendor      = PCI_ANY_ID,
		.subdevice      = PCI_ANY_ID,
		.setup          = pci_wch_ch353_setup,
	},
	 
	{
		.vendor         = PCI_VENDOR_ID_WCH,
		.device         = PCI_DEVICE_ID_WCH_CH353_2S1P,
		.subvendor      = PCI_ANY_ID,
		.subdevice      = PCI_ANY_ID,
		.setup          = pci_wch_ch353_setup,
	},
	 
	{
		.vendor         = PCI_VENDOR_ID_WCH,
		.device         = PCI_DEVICE_ID_WCH_CH353_4S,
		.subvendor      = PCI_ANY_ID,
		.subdevice      = PCI_ANY_ID,
		.setup          = pci_wch_ch353_setup,
	},
	 
	{
		.vendor         = PCI_VENDOR_ID_WCH,
		.device         = PCI_DEVICE_ID_WCH_CH353_2S1PF,
		.subvendor      = PCI_ANY_ID,
		.subdevice      = PCI_ANY_ID,
		.setup          = pci_wch_ch353_setup,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_WCH,
		.device		= PCI_DEVICE_ID_WCH_CH352_2S,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_wch_ch353_setup,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_WCH,
		.device		= PCI_DEVICE_ID_WCH_CH355_4S,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_wch_ch355_setup,
	},
	 
	{
		.vendor         = PCIE_VENDOR_ID_WCH,
		.device         = PCIE_DEVICE_ID_WCH_CH382_2S,
		.subvendor      = PCI_ANY_ID,
		.subdevice      = PCI_ANY_ID,
		.setup          = pci_wch_ch38x_setup,
	},
	 
	{
		.vendor         = PCIE_VENDOR_ID_WCH,
		.device         = PCIE_DEVICE_ID_WCH_CH382_2S1P,
		.subvendor      = PCI_ANY_ID,
		.subdevice      = PCI_ANY_ID,
		.setup          = pci_wch_ch38x_setup,
	},
	 
	{
		.vendor         = PCIE_VENDOR_ID_WCH,
		.device         = PCIE_DEVICE_ID_WCH_CH384_4S,
		.subvendor      = PCI_ANY_ID,
		.subdevice      = PCI_ANY_ID,
		.setup          = pci_wch_ch38x_setup,
	},
	 
	{
		.vendor         = PCIE_VENDOR_ID_WCH,
		.device         = PCIE_DEVICE_ID_WCH_CH384_8S,
		.subvendor      = PCI_ANY_ID,
		.subdevice      = PCI_ANY_ID,
		.init           = pci_wch_ch38x_init,
		.exit		= pci_wch_ch38x_exit,
		.setup          = pci_wch_ch38x_setup,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_BROADCOM,
		.device		= PCI_DEVICE_ID_BROADCOM_TRUMANAGE,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_brcm_trumanage_setup,
	},
	{
		.vendor		= 0x1c29,
		.device		= 0x1104,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_fintek_setup,
		.init		= pci_fintek_init,
	},
	{
		.vendor		= 0x1c29,
		.device		= 0x1108,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_fintek_setup,
		.init		= pci_fintek_init,
	},
	{
		.vendor		= 0x1c29,
		.device		= 0x1112,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_fintek_setup,
		.init		= pci_fintek_init,
	},
	 
	{
		.vendor		= PCI_VENDOR_ID_MOXA,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_moxa_setup,
	},
	{
		.vendor		= 0x1c29,
		.device		= 0x1204,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_fintek_f815xxa_setup,
		.init		= pci_fintek_f815xxa_init,
	},
	{
		.vendor		= 0x1c29,
		.device		= 0x1208,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_fintek_f815xxa_setup,
		.init		= pci_fintek_f815xxa_init,
	},
	{
		.vendor		= 0x1c29,
		.device		= 0x1212,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_fintek_f815xxa_setup,
		.init		= pci_fintek_f815xxa_init,
	},

	 
	{
		.vendor		= PCI_ANY_ID,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.setup		= pci_default_setup,
	}
};

static inline int quirk_id_matches(u32 quirk_id, u32 dev_id)
{
	return quirk_id == PCI_ANY_ID || quirk_id == dev_id;
}

static struct pci_serial_quirk *find_quirk(struct pci_dev *dev)
{
	struct pci_serial_quirk *quirk;

	for (quirk = pci_serial_quirks; ; quirk++)
		if (quirk_id_matches(quirk->vendor, dev->vendor) &&
		    quirk_id_matches(quirk->device, dev->device) &&
		    quirk_id_matches(quirk->subvendor, dev->subsystem_vendor) &&
		    quirk_id_matches(quirk->subdevice, dev->subsystem_device))
			break;
	return quirk;
}

 
enum pci_board_num_t {
	pbn_default = 0,

	pbn_b0_1_115200,
	pbn_b0_2_115200,
	pbn_b0_4_115200,
	pbn_b0_5_115200,
	pbn_b0_8_115200,

	pbn_b0_1_921600,
	pbn_b0_2_921600,
	pbn_b0_4_921600,

	pbn_b0_2_1130000,

	pbn_b0_4_1152000,

	pbn_b0_4_1250000,

	pbn_b0_2_1843200,
	pbn_b0_4_1843200,

	pbn_b0_1_15625000,

	pbn_b0_bt_1_115200,
	pbn_b0_bt_2_115200,
	pbn_b0_bt_4_115200,
	pbn_b0_bt_8_115200,

	pbn_b0_bt_1_460800,
	pbn_b0_bt_2_460800,
	pbn_b0_bt_4_460800,

	pbn_b0_bt_1_921600,
	pbn_b0_bt_2_921600,
	pbn_b0_bt_4_921600,
	pbn_b0_bt_8_921600,

	pbn_b1_1_115200,
	pbn_b1_2_115200,
	pbn_b1_4_115200,
	pbn_b1_8_115200,
	pbn_b1_16_115200,

	pbn_b1_1_921600,
	pbn_b1_2_921600,
	pbn_b1_4_921600,
	pbn_b1_8_921600,

	pbn_b1_2_1250000,

	pbn_b1_bt_1_115200,
	pbn_b1_bt_2_115200,
	pbn_b1_bt_4_115200,

	pbn_b1_bt_2_921600,

	pbn_b1_1_1382400,
	pbn_b1_2_1382400,
	pbn_b1_4_1382400,
	pbn_b1_8_1382400,

	pbn_b2_1_115200,
	pbn_b2_2_115200,
	pbn_b2_4_115200,
	pbn_b2_8_115200,

	pbn_b2_1_460800,
	pbn_b2_4_460800,
	pbn_b2_8_460800,
	pbn_b2_16_460800,

	pbn_b2_1_921600,
	pbn_b2_4_921600,
	pbn_b2_8_921600,

	pbn_b2_8_1152000,

	pbn_b2_bt_1_115200,
	pbn_b2_bt_2_115200,
	pbn_b2_bt_4_115200,

	pbn_b2_bt_2_921600,
	pbn_b2_bt_4_921600,

	pbn_b3_2_115200,
	pbn_b3_4_115200,
	pbn_b3_8_115200,

	pbn_b4_bt_2_921600,
	pbn_b4_bt_4_921600,
	pbn_b4_bt_8_921600,

	 
	pbn_panacom,
	pbn_panacom2,
	pbn_panacom4,
	pbn_plx_romulus,
	pbn_oxsemi,
	pbn_oxsemi_1_15625000,
	pbn_oxsemi_2_15625000,
	pbn_oxsemi_4_15625000,
	pbn_oxsemi_8_15625000,
	pbn_intel_i960,
	pbn_sgi_ioc3,
	pbn_computone_4,
	pbn_computone_6,
	pbn_computone_8,
	pbn_sbsxrsio,
	pbn_pasemi_1682M,
	pbn_ni8430_2,
	pbn_ni8430_4,
	pbn_ni8430_8,
	pbn_ni8430_16,
	pbn_ADDIDATA_PCIe_1_3906250,
	pbn_ADDIDATA_PCIe_2_3906250,
	pbn_ADDIDATA_PCIe_4_3906250,
	pbn_ADDIDATA_PCIe_8_3906250,
	pbn_ce4100_1_115200,
	pbn_omegapci,
	pbn_NETMOS9900_2s_115200,
	pbn_brcm_trumanage,
	pbn_fintek_4,
	pbn_fintek_8,
	pbn_fintek_12,
	pbn_fintek_F81504A,
	pbn_fintek_F81508A,
	pbn_fintek_F81512A,
	pbn_wch382_2,
	pbn_wch384_4,
	pbn_wch384_8,
	pbn_sunix_pci_1s,
	pbn_sunix_pci_2s,
	pbn_sunix_pci_4s,
	pbn_sunix_pci_8s,
	pbn_sunix_pci_16s,
	pbn_titan_1_4000000,
	pbn_titan_2_4000000,
	pbn_titan_4_4000000,
	pbn_titan_8_4000000,
	pbn_moxa8250_2p,
	pbn_moxa8250_4p,
	pbn_moxa8250_8p,
};

 

static struct pciserial_board pci_boards[] = {
	[pbn_default] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_1_115200] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_2_115200] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_4_115200] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_5_115200] = {
		.flags		= FL_BASE0,
		.num_ports	= 5,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_8_115200] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_1_921600] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b0_2_921600] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b0_4_921600] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},

	[pbn_b0_2_1130000] = {
		.flags          = FL_BASE0,
		.num_ports      = 2,
		.base_baud      = 1130000,
		.uart_offset    = 8,
	},

	[pbn_b0_4_1152000] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 1152000,
		.uart_offset	= 8,
	},

	[pbn_b0_4_1250000] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 1250000,
		.uart_offset	= 8,
	},

	[pbn_b0_2_1843200] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 1843200,
		.uart_offset	= 8,
	},
	[pbn_b0_4_1843200] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 1843200,
		.uart_offset	= 8,
	},

	[pbn_b0_1_15625000] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.base_baud	= 15625000,
		.uart_offset	= 8,
	},

	[pbn_b0_bt_1_115200] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 1,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_bt_2_115200] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 2,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_bt_4_115200] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 4,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b0_bt_8_115200] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 8,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},

	[pbn_b0_bt_1_460800] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 1,
		.base_baud	= 460800,
		.uart_offset	= 8,
	},
	[pbn_b0_bt_2_460800] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 2,
		.base_baud	= 460800,
		.uart_offset	= 8,
	},
	[pbn_b0_bt_4_460800] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 4,
		.base_baud	= 460800,
		.uart_offset	= 8,
	},

	[pbn_b0_bt_1_921600] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 1,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b0_bt_2_921600] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 2,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b0_bt_4_921600] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b0_bt_8_921600] = {
		.flags		= FL_BASE0|FL_BASE_BARS,
		.num_ports	= 8,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},

	[pbn_b1_1_115200] = {
		.flags		= FL_BASE1,
		.num_ports	= 1,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b1_2_115200] = {
		.flags		= FL_BASE1,
		.num_ports	= 2,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b1_4_115200] = {
		.flags		= FL_BASE1,
		.num_ports	= 4,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b1_8_115200] = {
		.flags		= FL_BASE1,
		.num_ports	= 8,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b1_16_115200] = {
		.flags		= FL_BASE1,
		.num_ports	= 16,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},

	[pbn_b1_1_921600] = {
		.flags		= FL_BASE1,
		.num_ports	= 1,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b1_2_921600] = {
		.flags		= FL_BASE1,
		.num_ports	= 2,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b1_4_921600] = {
		.flags		= FL_BASE1,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b1_8_921600] = {
		.flags		= FL_BASE1,
		.num_ports	= 8,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b1_2_1250000] = {
		.flags		= FL_BASE1,
		.num_ports	= 2,
		.base_baud	= 1250000,
		.uart_offset	= 8,
	},

	[pbn_b1_bt_1_115200] = {
		.flags		= FL_BASE1|FL_BASE_BARS,
		.num_ports	= 1,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b1_bt_2_115200] = {
		.flags		= FL_BASE1|FL_BASE_BARS,
		.num_ports	= 2,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b1_bt_4_115200] = {
		.flags		= FL_BASE1|FL_BASE_BARS,
		.num_ports	= 4,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},

	[pbn_b1_bt_2_921600] = {
		.flags		= FL_BASE1|FL_BASE_BARS,
		.num_ports	= 2,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},

	[pbn_b1_1_1382400] = {
		.flags		= FL_BASE1,
		.num_ports	= 1,
		.base_baud	= 1382400,
		.uart_offset	= 8,
	},
	[pbn_b1_2_1382400] = {
		.flags		= FL_BASE1,
		.num_ports	= 2,
		.base_baud	= 1382400,
		.uart_offset	= 8,
	},
	[pbn_b1_4_1382400] = {
		.flags		= FL_BASE1,
		.num_ports	= 4,
		.base_baud	= 1382400,
		.uart_offset	= 8,
	},
	[pbn_b1_8_1382400] = {
		.flags		= FL_BASE1,
		.num_ports	= 8,
		.base_baud	= 1382400,
		.uart_offset	= 8,
	},

	[pbn_b2_1_115200] = {
		.flags		= FL_BASE2,
		.num_ports	= 1,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b2_2_115200] = {
		.flags		= FL_BASE2,
		.num_ports	= 2,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b2_4_115200] = {
		.flags          = FL_BASE2,
		.num_ports      = 4,
		.base_baud      = 115200,
		.uart_offset    = 8,
	},
	[pbn_b2_8_115200] = {
		.flags		= FL_BASE2,
		.num_ports	= 8,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},

	[pbn_b2_1_460800] = {
		.flags		= FL_BASE2,
		.num_ports	= 1,
		.base_baud	= 460800,
		.uart_offset	= 8,
	},
	[pbn_b2_4_460800] = {
		.flags		= FL_BASE2,
		.num_ports	= 4,
		.base_baud	= 460800,
		.uart_offset	= 8,
	},
	[pbn_b2_8_460800] = {
		.flags		= FL_BASE2,
		.num_ports	= 8,
		.base_baud	= 460800,
		.uart_offset	= 8,
	},
	[pbn_b2_16_460800] = {
		.flags		= FL_BASE2,
		.num_ports	= 16,
		.base_baud	= 460800,
		.uart_offset	= 8,
	 },

	[pbn_b2_1_921600] = {
		.flags		= FL_BASE2,
		.num_ports	= 1,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b2_4_921600] = {
		.flags		= FL_BASE2,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b2_8_921600] = {
		.flags		= FL_BASE2,
		.num_ports	= 8,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},

	[pbn_b2_8_1152000] = {
		.flags		= FL_BASE2,
		.num_ports	= 8,
		.base_baud	= 1152000,
		.uart_offset	= 8,
	},

	[pbn_b2_bt_1_115200] = {
		.flags		= FL_BASE2|FL_BASE_BARS,
		.num_ports	= 1,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b2_bt_2_115200] = {
		.flags		= FL_BASE2|FL_BASE_BARS,
		.num_ports	= 2,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b2_bt_4_115200] = {
		.flags		= FL_BASE2|FL_BASE_BARS,
		.num_ports	= 4,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},

	[pbn_b2_bt_2_921600] = {
		.flags		= FL_BASE2|FL_BASE_BARS,
		.num_ports	= 2,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b2_bt_4_921600] = {
		.flags		= FL_BASE2|FL_BASE_BARS,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},

	[pbn_b3_2_115200] = {
		.flags		= FL_BASE3,
		.num_ports	= 2,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b3_4_115200] = {
		.flags		= FL_BASE3,
		.num_ports	= 4,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_b3_8_115200] = {
		.flags		= FL_BASE3,
		.num_ports	= 8,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},

	[pbn_b4_bt_2_921600] = {
		.flags		= FL_BASE4,
		.num_ports	= 2,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b4_bt_4_921600] = {
		.flags		= FL_BASE4,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},
	[pbn_b4_bt_8_921600] = {
		.flags		= FL_BASE4,
		.num_ports	= 8,
		.base_baud	= 921600,
		.uart_offset	= 8,
	},

	 

	 
	[pbn_panacom] = {
		.flags		= FL_BASE2,
		.num_ports	= 2,
		.base_baud	= 921600,
		.uart_offset	= 0x400,
		.reg_shift	= 7,
	},
	[pbn_panacom2] = {
		.flags		= FL_BASE2|FL_BASE_BARS,
		.num_ports	= 2,
		.base_baud	= 921600,
		.uart_offset	= 0x400,
		.reg_shift	= 7,
	},
	[pbn_panacom4] = {
		.flags		= FL_BASE2|FL_BASE_BARS,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 0x400,
		.reg_shift	= 7,
	},

	 
	[pbn_plx_romulus] = {
		.flags		= FL_BASE2,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 8 << 2,
		.reg_shift	= 2,
		.first_offset	= 0x03,
	},

	 
	[pbn_oxsemi] = {
		.flags		= FL_BASE0|FL_REGION_SZ_CAP,
		.num_ports	= 32,
		.base_baud	= 115200,
		.uart_offset	= 8,
	},
	[pbn_oxsemi_1_15625000] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.base_baud	= 15625000,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_oxsemi_2_15625000] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 15625000,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_oxsemi_4_15625000] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 15625000,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_oxsemi_8_15625000] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud	= 15625000,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},


	 
	[pbn_intel_i960] = {
		.flags		= FL_BASE0,
		.num_ports	= 32,
		.base_baud	= 921600,
		.uart_offset	= 8 << 2,
		.reg_shift	= 2,
		.first_offset	= 0x10000,
	},
	[pbn_sgi_ioc3] = {
		.flags		= FL_BASE0|FL_NOIRQ,
		.num_ports	= 1,
		.base_baud	= 458333,
		.uart_offset	= 8,
		.reg_shift	= 0,
		.first_offset	= 0x20178,
	},

	 
	[pbn_computone_4] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 921600,
		.uart_offset	= 0x40,
		.reg_shift	= 2,
		.first_offset	= 0x200,
	},
	[pbn_computone_6] = {
		.flags		= FL_BASE0,
		.num_ports	= 6,
		.base_baud	= 921600,
		.uart_offset	= 0x40,
		.reg_shift	= 2,
		.first_offset	= 0x200,
	},
	[pbn_computone_8] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud	= 921600,
		.uart_offset	= 0x40,
		.reg_shift	= 2,
		.first_offset	= 0x200,
	},
	[pbn_sbsxrsio] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud	= 460800,
		.uart_offset	= 256,
		.reg_shift	= 4,
	},
	 
	[pbn_pasemi_1682M] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.base_baud	= 8333333,
	},
	 
	[pbn_ni8430_16] = {
		.flags		= FL_BASE0,
		.num_ports	= 16,
		.base_baud	= 3686400,
		.uart_offset	= 0x10,
		.first_offset	= 0x800,
	},
	[pbn_ni8430_8] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud	= 3686400,
		.uart_offset	= 0x10,
		.first_offset	= 0x800,
	},
	[pbn_ni8430_4] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 3686400,
		.uart_offset	= 0x10,
		.first_offset	= 0x800,
	},
	[pbn_ni8430_2] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 3686400,
		.uart_offset	= 0x10,
		.first_offset	= 0x800,
	},
	 
	[pbn_ADDIDATA_PCIe_1_3906250] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.base_baud	= 3906250,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_ADDIDATA_PCIe_2_3906250] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 3906250,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_ADDIDATA_PCIe_4_3906250] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 3906250,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_ADDIDATA_PCIe_8_3906250] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud	= 3906250,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_ce4100_1_115200] = {
		.flags		= FL_BASE_BARS,
		.num_ports	= 2,
		.base_baud	= 921600,
		.reg_shift      = 2,
	},
	[pbn_omegapci] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud	= 115200,
		.uart_offset	= 0x200,
	},
	[pbn_NETMOS9900_2s_115200] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 115200,
	},
	[pbn_brcm_trumanage] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.reg_shift	= 2,
		.base_baud	= 115200,
	},
	[pbn_fintek_4] = {
		.num_ports	= 4,
		.uart_offset	= 8,
		.base_baud	= 115200,
		.first_offset	= 0x40,
	},
	[pbn_fintek_8] = {
		.num_ports	= 8,
		.uart_offset	= 8,
		.base_baud	= 115200,
		.first_offset	= 0x40,
	},
	[pbn_fintek_12] = {
		.num_ports	= 12,
		.uart_offset	= 8,
		.base_baud	= 115200,
		.first_offset	= 0x40,
	},
	[pbn_fintek_F81504A] = {
		.num_ports	= 4,
		.uart_offset	= 8,
		.base_baud	= 115200,
	},
	[pbn_fintek_F81508A] = {
		.num_ports	= 8,
		.uart_offset	= 8,
		.base_baud	= 115200,
	},
	[pbn_fintek_F81512A] = {
		.num_ports	= 12,
		.uart_offset	= 8,
		.base_baud	= 115200,
	},
	[pbn_wch382_2] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 115200,
		.uart_offset	= 8,
		.first_offset	= 0xC0,
	},
	[pbn_wch384_4] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud      = 115200,
		.uart_offset    = 8,
		.first_offset   = 0xC0,
	},
	[pbn_wch384_8] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud      = 115200,
		.uart_offset    = 8,
		.first_offset   = 0x00,
	},
	[pbn_sunix_pci_1s] = {
		.num_ports	= 1,
		.base_baud      = 921600,
		.uart_offset	= 0x8,
	},
	[pbn_sunix_pci_2s] = {
		.num_ports	= 2,
		.base_baud      = 921600,
		.uart_offset	= 0x8,
	},
	[pbn_sunix_pci_4s] = {
		.num_ports	= 4,
		.base_baud      = 921600,
		.uart_offset	= 0x8,
	},
	[pbn_sunix_pci_8s] = {
		.num_ports	= 8,
		.base_baud      = 921600,
		.uart_offset	= 0x8,
	},
	[pbn_sunix_pci_16s] = {
		.num_ports	= 16,
		.base_baud      = 921600,
		.uart_offset	= 0x8,
	},
	[pbn_titan_1_4000000] = {
		.flags		= FL_BASE0,
		.num_ports	= 1,
		.base_baud	= 4000000,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_titan_2_4000000] = {
		.flags		= FL_BASE0,
		.num_ports	= 2,
		.base_baud	= 4000000,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_titan_4_4000000] = {
		.flags		= FL_BASE0,
		.num_ports	= 4,
		.base_baud	= 4000000,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_titan_8_4000000] = {
		.flags		= FL_BASE0,
		.num_ports	= 8,
		.base_baud	= 4000000,
		.uart_offset	= 0x200,
		.first_offset	= 0x1000,
	},
	[pbn_moxa8250_2p] = {
		.flags		= FL_BASE1,
		.num_ports      = 2,
		.base_baud      = 921600,
		.uart_offset	= 0x200,
	},
	[pbn_moxa8250_4p] = {
		.flags		= FL_BASE1,
		.num_ports      = 4,
		.base_baud      = 921600,
		.uart_offset	= 0x200,
	},
	[pbn_moxa8250_8p] = {
		.flags		= FL_BASE1,
		.num_ports      = 8,
		.base_baud      = 921600,
		.uart_offset	= 0x200,
	},
};

#define REPORT_CONFIG(option) \
	(IS_ENABLED(CONFIG_##option) ? 0 : (kernel_ulong_t)&#option)
#define REPORT_8250_CONFIG(option) \
	(IS_ENABLED(CONFIG_SERIAL_8250_##option) ? \
	 0 : (kernel_ulong_t)&"SERIAL_8250_"#option)

static const struct pci_device_id blacklist[] = {
	 
	{ PCI_VDEVICE(AL, 0x5457), },  
	{ PCI_VDEVICE(MOTOROLA, 0x3052), },  
	{ PCI_DEVICE(0x1543, 0x3052), },  

	 
	 
	{ PCI_DEVICE(0x4348, 0x7053), 0, 0, REPORT_CONFIG(PARPORT_SERIAL), },
	 
	{ PCI_DEVICE(0x4348, 0x5053), 0, 0, REPORT_CONFIG(PARPORT_SERIAL), },
	 
	{ PCI_DEVICE(0x1c00, 0x3250), 0, 0, REPORT_CONFIG(PARPORT_SERIAL), },

	 
	{ PCI_VDEVICE(INTEL, 0x081b), REPORT_8250_CONFIG(MID), },
	{ PCI_VDEVICE(INTEL, 0x081c), REPORT_8250_CONFIG(MID), },
	{ PCI_VDEVICE(INTEL, 0x081d), REPORT_8250_CONFIG(MID), },
	{ PCI_VDEVICE(INTEL, 0x1191), REPORT_8250_CONFIG(MID), },
	{ PCI_VDEVICE(INTEL, 0x18d8), REPORT_8250_CONFIG(MID), },
	{ PCI_VDEVICE(INTEL, 0x19d8), REPORT_8250_CONFIG(MID), },

	 
	{ PCI_VDEVICE(INTEL, 0x0936), REPORT_8250_CONFIG(LPSS), },
	{ PCI_VDEVICE(INTEL, 0x0f0a), REPORT_8250_CONFIG(LPSS), },
	{ PCI_VDEVICE(INTEL, 0x0f0c), REPORT_8250_CONFIG(LPSS), },
	{ PCI_VDEVICE(INTEL, 0x228a), REPORT_8250_CONFIG(LPSS), },
	{ PCI_VDEVICE(INTEL, 0x228c), REPORT_8250_CONFIG(LPSS), },
	{ PCI_VDEVICE(INTEL, 0x4b96), REPORT_8250_CONFIG(LPSS), },
	{ PCI_VDEVICE(INTEL, 0x4b97), REPORT_8250_CONFIG(LPSS), },
	{ PCI_VDEVICE(INTEL, 0x4b98), REPORT_8250_CONFIG(LPSS), },
	{ PCI_VDEVICE(INTEL, 0x4b99), REPORT_8250_CONFIG(LPSS), },
	{ PCI_VDEVICE(INTEL, 0x4b9a), REPORT_8250_CONFIG(LPSS), },
	{ PCI_VDEVICE(INTEL, 0x4b9b), REPORT_8250_CONFIG(LPSS), },
	{ PCI_VDEVICE(INTEL, 0x9ce3), REPORT_8250_CONFIG(LPSS), },
	{ PCI_VDEVICE(INTEL, 0x9ce4), REPORT_8250_CONFIG(LPSS), },

	 
	{ PCI_VDEVICE(EXAR, PCI_ANY_ID), REPORT_8250_CONFIG(EXAR), },
	{ PCI_VDEVICE(COMMTECH, PCI_ANY_ID), REPORT_8250_CONFIG(EXAR), },

	 
	{ PCI_VDEVICE(PERICOM, PCI_ANY_ID), REPORT_8250_CONFIG(PERICOM), },
	{ PCI_VDEVICE(ACCESSIO, PCI_ANY_ID), REPORT_8250_CONFIG(PERICOM), },

	 
	{ }
};

static int serial_pci_is_class_communication(struct pci_dev *dev)
{
	 
	if ((((dev->class >> 8) != PCI_CLASS_COMMUNICATION_SERIAL) &&
	     ((dev->class >> 8) != PCI_CLASS_COMMUNICATION_MULTISERIAL) &&
	     ((dev->class >> 8) != PCI_CLASS_COMMUNICATION_MODEM)) ||
	    (dev->class & 0xff) > 6)
		return -ENODEV;

	return 0;
}

 
static int
serial_pci_guess_board(struct pci_dev *dev, struct pciserial_board *board)
{
	int num_iomem, num_port, first_port = -1, i;
	int rc;

	rc = serial_pci_is_class_communication(dev);
	if (rc)
		return rc;

	 
	if ((dev->class >> 8) == PCI_CLASS_COMMUNICATION_MULTISERIAL)
		return -ENODEV;

	num_iomem = num_port = 0;
	for (i = 0; i < PCI_STD_NUM_BARS; i++) {
		if (pci_resource_flags(dev, i) & IORESOURCE_IO) {
			num_port++;
			if (first_port == -1)
				first_port = i;
		}
		if (pci_resource_flags(dev, i) & IORESOURCE_MEM)
			num_iomem++;
	}

	 
	if (num_iomem <= 1 && num_port == 1) {
		board->flags = first_port;
		board->num_ports = pci_resource_len(dev, first_port) / 8;
		return 0;
	}

	 
	first_port = -1;
	num_port = 0;
	for (i = 0; i < PCI_STD_NUM_BARS; i++) {
		if (pci_resource_flags(dev, i) & IORESOURCE_IO &&
		    pci_resource_len(dev, i) == 8 &&
		    (first_port == -1 || (first_port + num_port) == i)) {
			num_port++;
			if (first_port == -1)
				first_port = i;
		}
	}

	if (num_port > 1) {
		board->flags = first_port | FL_BASE_BARS;
		board->num_ports = num_port;
		return 0;
	}

	return -ENODEV;
}

static inline int
serial_pci_matches(const struct pciserial_board *board,
		   const struct pciserial_board *guessed)
{
	return
	    board->num_ports == guessed->num_ports &&
	    board->base_baud == guessed->base_baud &&
	    board->uart_offset == guessed->uart_offset &&
	    board->reg_shift == guessed->reg_shift &&
	    board->first_offset == guessed->first_offset;
}

struct serial_private *
pciserial_init_ports(struct pci_dev *dev, const struct pciserial_board *board)
{
	struct uart_8250_port uart;
	struct serial_private *priv;
	struct pci_serial_quirk *quirk;
	int rc, nr_ports, i;

	nr_ports = board->num_ports;

	 
	quirk = find_quirk(dev);

	 
	if (quirk->init) {
		rc = quirk->init(dev);
		if (rc < 0) {
			priv = ERR_PTR(rc);
			goto err_out;
		}
		if (rc)
			nr_ports = rc;
	}

	priv = kzalloc(struct_size(priv, line, nr_ports), GFP_KERNEL);
	if (!priv) {
		priv = ERR_PTR(-ENOMEM);
		goto err_deinit;
	}

	priv->dev = dev;
	priv->quirk = quirk;

	memset(&uart, 0, sizeof(uart));
	uart.port.flags = UPF_SKIP_TEST | UPF_BOOT_AUTOCONF | UPF_SHARE_IRQ;
	uart.port.uartclk = board->base_baud * 16;

	if (board->flags & FL_NOIRQ) {
		uart.port.irq = 0;
	} else {
		if (pci_match_id(pci_use_msi, dev)) {
			pci_dbg(dev, "Using MSI(-X) interrupts\n");
			pci_set_master(dev);
			uart.port.flags &= ~UPF_SHARE_IRQ;
			rc = pci_alloc_irq_vectors(dev, 1, 1, PCI_IRQ_ALL_TYPES);
		} else {
			pci_dbg(dev, "Using legacy interrupts\n");
			rc = pci_alloc_irq_vectors(dev, 1, 1, PCI_IRQ_LEGACY);
		}
		if (rc < 0) {
			kfree(priv);
			priv = ERR_PTR(rc);
			goto err_deinit;
		}

		uart.port.irq = pci_irq_vector(dev, 0);
	}

	uart.port.dev = &dev->dev;

	for (i = 0; i < nr_ports; i++) {
		if (quirk->setup(priv, board, &uart, i))
			break;

		pci_dbg(dev, "Setup PCI port: port %lx, irq %d, type %d\n",
			uart.port.iobase, uart.port.irq, uart.port.iotype);

		priv->line[i] = serial8250_register_8250_port(&uart);
		if (priv->line[i] < 0) {
			pci_err(dev,
				"Couldn't register serial port %lx, irq %d, type %d, error %d\n",
				uart.port.iobase, uart.port.irq,
				uart.port.iotype, priv->line[i]);
			break;
		}
	}
	priv->nr = i;
	priv->board = board;
	return priv;

err_deinit:
	if (quirk->exit)
		quirk->exit(dev);
err_out:
	return priv;
}
EXPORT_SYMBOL_GPL(pciserial_init_ports);

static void pciserial_detach_ports(struct serial_private *priv)
{
	struct pci_serial_quirk *quirk;
	int i;

	for (i = 0; i < priv->nr; i++)
		serial8250_unregister_port(priv->line[i]);

	 
	quirk = find_quirk(priv->dev);
	if (quirk->exit)
		quirk->exit(priv->dev);
}

void pciserial_remove_ports(struct serial_private *priv)
{
	pciserial_detach_ports(priv);
	kfree(priv);
}
EXPORT_SYMBOL_GPL(pciserial_remove_ports);

void pciserial_suspend_ports(struct serial_private *priv)
{
	int i;

	for (i = 0; i < priv->nr; i++)
		if (priv->line[i] >= 0)
			serial8250_suspend_port(priv->line[i]);

	 
	if (priv->quirk->exit)
		priv->quirk->exit(priv->dev);
}
EXPORT_SYMBOL_GPL(pciserial_suspend_ports);

void pciserial_resume_ports(struct serial_private *priv)
{
	int i;

	 
	if (priv->quirk->init)
		priv->quirk->init(priv->dev);

	for (i = 0; i < priv->nr; i++)
		if (priv->line[i] >= 0)
			serial8250_resume_port(priv->line[i]);
}
EXPORT_SYMBOL_GPL(pciserial_resume_ports);

 
static int
pciserial_init_one(struct pci_dev *dev, const struct pci_device_id *ent)
{
	struct pci_serial_quirk *quirk;
	struct serial_private *priv;
	const struct pciserial_board *board;
	const struct pci_device_id *exclude;
	struct pciserial_board tmp;
	int rc;

	quirk = find_quirk(dev);
	if (quirk->probe) {
		rc = quirk->probe(dev);
		if (rc)
			return rc;
	}

	if (ent->driver_data >= ARRAY_SIZE(pci_boards)) {
		pci_err(dev, "invalid driver_data: %ld\n", ent->driver_data);
		return -EINVAL;
	}

	board = &pci_boards[ent->driver_data];

	exclude = pci_match_id(blacklist, dev);
	if (exclude) {
		if (exclude->driver_data)
			pci_warn(dev, "ignoring port, enable %s to handle\n",
				 (const char *)exclude->driver_data);
		return -ENODEV;
	}

	rc = pcim_enable_device(dev);
	pci_save_state(dev);
	if (rc)
		return rc;

	if (ent->driver_data == pbn_default) {
		 
		memcpy(&tmp, board, sizeof(struct pciserial_board));
		board = &tmp;

		 
		rc = serial_pci_guess_board(dev, &tmp);
		if (rc)
			return rc;
	} else {
		 
		memcpy(&tmp, &pci_boards[pbn_default],
		       sizeof(struct pciserial_board));
		rc = serial_pci_guess_board(dev, &tmp);
		if (rc == 0 && serial_pci_matches(board, &tmp))
			moan_device("Redundant entry in serial pci_table.",
				    dev);
	}

	priv = pciserial_init_ports(dev, board);
	if (IS_ERR(priv))
		return PTR_ERR(priv);

	pci_set_drvdata(dev, priv);
	return 0;
}

static void pciserial_remove_one(struct pci_dev *dev)
{
	struct serial_private *priv = pci_get_drvdata(dev);

	pciserial_remove_ports(priv);
}

#ifdef CONFIG_PM_SLEEP
static int pciserial_suspend_one(struct device *dev)
{
	struct serial_private *priv = dev_get_drvdata(dev);

	if (priv)
		pciserial_suspend_ports(priv);

	return 0;
}

static int pciserial_resume_one(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct serial_private *priv = pci_get_drvdata(pdev);
	int err;

	if (priv) {
		 
		err = pci_enable_device(pdev);
		 
		if (err)
			pci_err(pdev, "Unable to re-enable ports, trying to continue.\n");
		pciserial_resume_ports(priv);
	}
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(pciserial_pm_ops, pciserial_suspend_one,
			 pciserial_resume_one);

static const struct pci_device_id serial_pci_tbl[] = {
	{	PCI_VENDOR_ID_ADVANTECH, PCI_DEVICE_ID_ADVANTECH_PCI1600,
		PCI_DEVICE_ID_ADVANTECH_PCI1600_1611, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	 
	{	PCI_VENDOR_ID_ADVANTECH, PCI_DEVICE_ID_ADVANTECH_PCI3620,
		PCI_DEVICE_ID_ADVANTECH_PCI3620, 0x0001, 0, 0,
		pbn_b2_8_921600 },
	 
	{	PCI_VENDOR_ID_ADVANTECH, PCI_DEVICE_ID_ADVANTECH_PCI3618,
		PCI_DEVICE_ID_ADVANTECH_PCI3618, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_ADVANTECH, PCI_DEVICE_ID_ADVANTECH_PCIf618,
		PCI_DEVICE_ID_ADVANTECH_PCI3618, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V960,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH8_232, 0, 0,
		pbn_b1_8_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V960,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH4_232, 0, 0,
		pbn_b1_4_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V960,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH2_232, 0, 0,
		pbn_b1_2_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH8_232, 0, 0,
		pbn_b1_8_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH4_232, 0, 0,
		pbn_b1_4_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH2_232, 0, 0,
		pbn_b1_2_1382400 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH8_485, 0, 0,
		pbn_b1_8_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH8_485_4_4, 0, 0,
		pbn_b1_8_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH4_485, 0, 0,
		pbn_b1_4_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH4_485_2_2, 0, 0,
		pbn_b1_4_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH2_485, 0, 0,
		pbn_b1_2_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH8_485_2_6, 0, 0,
		pbn_b1_8_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH081101V1, 0, 0,
		pbn_b1_8_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH041101V1, 0, 0,
		pbn_b1_4_921600 },
	{	PCI_VENDOR_ID_V3, PCI_DEVICE_ID_V3_V351,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_BH2_20MHZ, 0, 0,
		pbn_b1_2_1250000 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI954,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_TITAN_2, 0, 0,
		pbn_b0_2_1843200 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI954,
		PCI_SUBVENDOR_ID_CONNECT_TECH,
		PCI_SUBDEVICE_ID_CONNECT_TECH_TITAN_4, 0, 0,
		pbn_b0_4_1843200 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI954,
		PCI_VENDOR_ID_AFAVLAB,
		PCI_SUBDEVICE_ID_AFAVLAB_P061, 0, 0,
		pbn_b0_4_1152000 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_U530,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_1_115200 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_UCOMM2,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_115200 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_UCOMM422,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_4_115200 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_UCOMM232,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_115200 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_COMM4,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_4_115200 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_COMM8,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_8_115200 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_7803,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_8_460800 },
	{	PCI_VENDOR_ID_SEALEVEL, PCI_DEVICE_ID_SEALEVEL_UCOMM8,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_8_115200 },

	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_GTEK_SERIAL2,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_115200 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_SPCOM200,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_921600 },
	 
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_SPCOM800,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_8_921600 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_1077,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_4_921600 },
	 
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_VENDOR_ID_PLX,
		PCI_SUBDEVICE_ID_UNKNOWN_0x1584, 0, 0,
		pbn_b2_4_115200 },
	 
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_VENDOR_ID_PLX,
		PCI_SUBDEVICE_ID_UNKNOWN_0x1588, 0, 0,
		pbn_b2_8_115200 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_KEYSPAN,
		PCI_SUBDEVICE_ID_KEYSPAN_SX2, 0, 0,
		pbn_panacom },
	{	PCI_VENDOR_ID_PANACOM, PCI_DEVICE_ID_PANACOM_QUADMODEM,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_panacom4 },
	{	PCI_VENDOR_ID_PANACOM, PCI_DEVICE_ID_PANACOM_DUALMODEM,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_panacom2 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9030,
		PCI_VENDOR_ID_ESDGMBH,
		PCI_DEVICE_ID_ESDGMBH_CPCIASIO4, 0, 0,
		pbn_b2_4_115200 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIFAST,
		PCI_SUBDEVICE_ID_CHASE_PCIFAST4, 0, 0,
		pbn_b2_4_460800 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIFAST,
		PCI_SUBDEVICE_ID_CHASE_PCIFAST8, 0, 0,
		pbn_b2_8_460800 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIFAST,
		PCI_SUBDEVICE_ID_CHASE_PCIFAST16, 0, 0,
		pbn_b2_16_460800 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIFAST,
		PCI_SUBDEVICE_ID_CHASE_PCIFAST16FMC, 0, 0,
		pbn_b2_16_460800 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIRAS,
		PCI_SUBDEVICE_ID_CHASE_PCIRAS4, 0, 0,
		pbn_b2_4_460800 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_CHASE_PCIRAS,
		PCI_SUBDEVICE_ID_CHASE_PCIRAS8, 0, 0,
		pbn_b2_8_460800 },
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9050,
		PCI_SUBVENDOR_ID_EXSYS,
		PCI_SUBDEVICE_ID_EXSYS_4055, 0, 0,
		pbn_b2_4_115200 },
	 
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_ROMULUS,
		0x10b5, 0x106a, 0, 0,
		pbn_plx_romulus },
	 
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_QSC100,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_4_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_DSC100,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_2_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_DSC100E,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_DSC200,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_2_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_DSC200E,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_QSC200,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_4_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_ESC100D,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_8_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_ESC100M,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_8_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_QSCP100,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_4_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_DSCP100,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_2_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_QSCP200,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_4_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_DSCP200,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_2_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_QSCLP100,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_4_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_DSCLP100,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_SSCLP100,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_1_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_QSCLP200,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_4_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_DSCLP200,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_SSCLP200,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_1_115200 },
	{	PCI_VENDOR_ID_QUATECH, PCI_DEVICE_ID_QUATECH_ESCLP100,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_8_115200 },

	{	PCI_VENDOR_ID_SPECIALIX, PCI_DEVICE_ID_OXSEMI_16PCI954,
		PCI_VENDOR_ID_SPECIALIX, PCI_SUBDEVICE_ID_SPECIALIX_SPEED4,
		0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI954,
		PCI_SUBVENDOR_ID_SIIG, PCI_SUBDEVICE_ID_SIIG_QUARTET_SERIAL,
		0, 0,
		pbn_b0_4_1152000 },
	{	PCI_VENDOR_ID_OXSEMI, 0x9505,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_921600 },

		 
	{	PCI_VENDOR_ID_OXSEMI, 0x950a,
		PCI_SUBVENDOR_ID_SIIG, PCI_SUBDEVICE_ID_SIIG_DUAL_00,
		0, 0, pbn_b0_2_115200 },
	{	PCI_VENDOR_ID_OXSEMI, 0x950a,
		PCI_SUBVENDOR_ID_SIIG, PCI_SUBDEVICE_ID_SIIG_DUAL_30,
		0, 0, pbn_b0_2_115200 },
	{	PCI_VENDOR_ID_OXSEMI, 0x950a,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_2_1130000 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_C950,
		PCI_VENDOR_ID_OXSEMI, PCI_SUBDEVICE_ID_OXSEMI_C950, 0, 0,
		pbn_b0_1_921600 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI954,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_115200 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI952,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_921600 },
	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI958,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_8_1152000 },

	 
	{	PCI_VENDOR_ID_OXSEMI, 0xc101,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc105,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc11b,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc11f,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc120,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc124,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc138,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc13d,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc140,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc141,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc144,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc145,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc158,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_2_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc15d,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_2_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc208,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_4_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc20d,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_4_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc308,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_8_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc30d,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_8_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc40b,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc40f,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc41b,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc41f,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc42b,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc42f,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc43b,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc43f,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc44b,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc44f,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc45b,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc45f,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc46b,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc46f,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc47b,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc47f,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc48b,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc48f,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc49b,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc49f,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc4ab,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc4af,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc4bb,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc4bf,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc4cb,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_OXSEMI, 0xc4cf,     
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_1_15625000 },
	 
	{	PCI_VENDOR_ID_MAINPINE, 0x4000,	 
		PCI_VENDOR_ID_MAINPINE, 0x4001, 0, 0,
		pbn_oxsemi_1_15625000 },
	{	PCI_VENDOR_ID_MAINPINE, 0x4000,	 
		PCI_VENDOR_ID_MAINPINE, 0x4002, 0, 0,
		pbn_oxsemi_2_15625000 },
	{	PCI_VENDOR_ID_MAINPINE, 0x4000,	 
		PCI_VENDOR_ID_MAINPINE, 0x4004, 0, 0,
		pbn_oxsemi_4_15625000 },
	{	PCI_VENDOR_ID_MAINPINE, 0x4000,	 
		PCI_VENDOR_ID_MAINPINE, 0x4008, 0, 0,
		pbn_oxsemi_8_15625000 },

	 
	{	PCI_VENDOR_ID_DIGI, PCIE_DEVICE_ID_NEO_2_OX_IBM,
		PCI_SUBVENDOR_ID_IBM, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_2_15625000 },
	 
	{	PCI_VENDOR_ID_ENDRUN, PCI_DEVICE_ID_ENDRUN_1588,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi_2_15625000 },

	 
	{	PCI_VENDOR_ID_SBSMODULARIO, PCI_DEVICE_ID_OCTPRO,
		PCI_SUBVENDOR_ID_SBSMODULARIO, PCI_SUBDEVICE_ID_OCTPRO232, 0, 0,
		pbn_sbsxrsio },
	{	PCI_VENDOR_ID_SBSMODULARIO, PCI_DEVICE_ID_OCTPRO,
		PCI_SUBVENDOR_ID_SBSMODULARIO, PCI_SUBDEVICE_ID_OCTPRO422, 0, 0,
		pbn_sbsxrsio },
	{	PCI_VENDOR_ID_SBSMODULARIO, PCI_DEVICE_ID_OCTPRO,
		PCI_SUBVENDOR_ID_SBSMODULARIO, PCI_SUBDEVICE_ID_POCTAL232, 0, 0,
		pbn_sbsxrsio },
	{	PCI_VENDOR_ID_SBSMODULARIO, PCI_DEVICE_ID_OCTPRO,
		PCI_SUBVENDOR_ID_SBSMODULARIO, PCI_SUBDEVICE_ID_POCTAL422, 0, 0,
		pbn_sbsxrsio },

	 
	{	PCI_VENDOR_ID_ATT, PCI_DEVICE_ID_ATT_VENUS_MODEM,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_1_115200 },

	 
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_100,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_200,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_2_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_400,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800B,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_100L,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_1_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_200L,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_2_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_400L,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800L,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_8_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_200I,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b4_bt_2_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_400I,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b4_bt_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800I,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b4_bt_8_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_400EH,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800EH,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800EHB,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_100E,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_titan_1_4000000 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_200E,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_titan_2_4000000 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_400E,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_titan_4_4000000 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800E,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_titan_8_4000000 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_200EI,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_titan_2_4000000 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_200EISI,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_titan_2_4000000 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_200V3,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_400V3,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_410V3,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800V3,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_TITAN, PCI_DEVICE_ID_TITAN_800V3B,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_4_921600 },

	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_10x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_1_460800 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_10x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_1_460800 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_10x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_1_460800 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_10x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_10x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_10x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_10x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_4_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_10x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_4_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_10x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_4_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_20x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_20x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_1S_20x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_20x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_20x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_2S_20x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_20x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_4_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_20x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_4_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_4S_20x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_4_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_8S_20x_550,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_8_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_8S_20x_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_8_921600 },
	{	PCI_VENDOR_ID_SIIG, PCI_DEVICE_ID_SIIG_8S_20x_850,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_8_921600 },

	 
	{	PCI_VENDOR_ID_COMPUTONE, PCI_DEVICE_ID_COMPUTONE_PG,
		PCI_SUBVENDOR_ID_COMPUTONE, PCI_SUBDEVICE_ID_COMPUTONE_PG4,
		0, 0, pbn_computone_4 },
	{	PCI_VENDOR_ID_COMPUTONE, PCI_DEVICE_ID_COMPUTONE_PG,
		PCI_SUBVENDOR_ID_COMPUTONE, PCI_SUBDEVICE_ID_COMPUTONE_PG8,
		0, 0, pbn_computone_8 },
	{	PCI_VENDOR_ID_COMPUTONE, PCI_DEVICE_ID_COMPUTONE_PG,
		PCI_SUBVENDOR_ID_COMPUTONE, PCI_SUBDEVICE_ID_COMPUTONE_PG6,
		0, 0, pbn_computone_6 },

	{	PCI_VENDOR_ID_OXSEMI, PCI_DEVICE_ID_OXSEMI_16PCI95N,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_oxsemi },
	{	PCI_VENDOR_ID_TIMEDIA, PCI_DEVICE_ID_TIMEDIA_1889,
		PCI_VENDOR_ID_TIMEDIA, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_1_921600 },

	 
	{	PCI_VENDOR_ID_SUNIX, PCI_DEVICE_ID_SUNIX_1999,
		PCI_VENDOR_ID_SUNIX, 0x0001, 0, 0,
		pbn_sunix_pci_1s },
	{	PCI_VENDOR_ID_SUNIX, PCI_DEVICE_ID_SUNIX_1999,
		PCI_VENDOR_ID_SUNIX, 0x0002, 0, 0,
		pbn_sunix_pci_2s },
	{	PCI_VENDOR_ID_SUNIX, PCI_DEVICE_ID_SUNIX_1999,
		PCI_VENDOR_ID_SUNIX, 0x0004, 0, 0,
		pbn_sunix_pci_4s },
	{	PCI_VENDOR_ID_SUNIX, PCI_DEVICE_ID_SUNIX_1999,
		PCI_VENDOR_ID_SUNIX, 0x0084, 0, 0,
		pbn_sunix_pci_4s },
	{	PCI_VENDOR_ID_SUNIX, PCI_DEVICE_ID_SUNIX_1999,
		PCI_VENDOR_ID_SUNIX, 0x0008, 0, 0,
		pbn_sunix_pci_8s },
	{	PCI_VENDOR_ID_SUNIX, PCI_DEVICE_ID_SUNIX_1999,
		PCI_VENDOR_ID_SUNIX, 0x0088, 0, 0,
		pbn_sunix_pci_8s },
	{	PCI_VENDOR_ID_SUNIX, PCI_DEVICE_ID_SUNIX_1999,
		PCI_VENDOR_ID_SUNIX, 0x0010, 0, 0,
		pbn_sunix_pci_16s },

	 
	{	PCI_VENDOR_ID_AFAVLAB, PCI_DEVICE_ID_AFAVLAB_P028,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_8_115200 },
	{	PCI_VENDOR_ID_AFAVLAB, PCI_DEVICE_ID_AFAVLAB_P030,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_8_115200 },

	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_DSERIAL,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_115200 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_QUATRO_A,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_115200 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_QUATRO_B,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_115200 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_QUATTRO_A,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_115200 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_QUATTRO_B,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_115200 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_OCTO_A,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_4_460800 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_OCTO_B,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_4_460800 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_PORT_PLUS,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_460800 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_QUAD_A,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_460800 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_QUAD_B,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_2_460800 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_SSERIAL,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_1_115200 },
	{	PCI_VENDOR_ID_LAVA, PCI_DEVICE_ID_LAVA_PORT_650,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_bt_1_460800 },

	 
	{	PCI_VENDOR_ID_KORENIX, PCI_DEVICE_ID_KORENIX_JETCARDF0,
		0x1204, 0x0004, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_KORENIX, PCI_DEVICE_ID_KORENIX_JETCARDF0,
		0x1208, 0x0004, 0, 0,
		pbn_b0_4_921600 },
 
 
	{	PCI_VENDOR_ID_KORENIX, PCI_DEVICE_ID_KORENIX_JETCARDF1,
		0x1208, 0x0004, 0, 0,
		pbn_b0_4_921600 },

	{	PCI_VENDOR_ID_KORENIX, PCI_DEVICE_ID_KORENIX_JETCARDF2,
		0x1204, 0x0004, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_KORENIX, PCI_DEVICE_ID_KORENIX_JETCARDF2,
		0x1208, 0x0004, 0, 0,
		pbn_b0_4_921600 },
	{	PCI_VENDOR_ID_KORENIX, PCI_DEVICE_ID_KORENIX_JETCARDF3,
		0x1208, 0x0004, 0, 0,
		pbn_b0_4_921600 },
	 
	{	PCI_VENDOR_ID_DELL, PCI_DEVICE_ID_DELL_RAC4,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_1_1382400 },

	 
	{	PCI_VENDOR_ID_DELL, PCI_DEVICE_ID_DELL_RACIII,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_1_1382400 },

	 
	{	PCI_VENDOR_ID_MORETON, PCI_DEVICE_ID_RASTEL_2PORT,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_bt_2_115200 },

	 
	{	PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_80960_RP,
		0xE4BF, PCI_ANY_ID, 0, 0,
		pbn_intel_i960 },

	 
	{	PCI_VENDOR_ID_XIRCOM, PCI_DEVICE_ID_XIRCOM_X3201_MDM,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_115200 },
	 
	{	PCI_VENDOR_ID_XIRCOM, PCI_DEVICE_ID_XIRCOM_RBM56G,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_115200 },

	 

	 
	{	PCI_VENDOR_ID_ROCKWELL, 0x1004,
		0x1048, 0x1500, 0, 0,
		pbn_b1_1_115200 },

	{	PCI_VENDOR_ID_SGI, PCI_DEVICE_ID_SGI_IOC3,
		0xFF00, 0, 0, 0,
		pbn_sgi_ioc3 },

	 
	{	PCI_VENDOR_ID_HP, PCI_DEVICE_ID_HP_DIVA,
		PCI_VENDOR_ID_HP, PCI_DEVICE_ID_HP_DIVA_RMP3, 0, 0,
		pbn_b1_1_115200 },
	{	PCI_VENDOR_ID_HP, PCI_DEVICE_ID_HP_DIVA,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_5_115200 },
	{	PCI_VENDOR_ID_HP, PCI_DEVICE_ID_HP_DIVA_AUX,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_1_115200 },
	 
	{	PCI_VENDOR_ID_HP_3PAR, PCI_DEVICE_ID_HPE_PCI_SERIAL,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_1_115200 },

	{	PCI_VENDOR_ID_DCI, PCI_DEVICE_ID_DCI_PCCOM2,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b3_2_115200 },
	{	PCI_VENDOR_ID_DCI, PCI_DEVICE_ID_DCI_PCCOM4,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b3_4_115200 },
	{	PCI_VENDOR_ID_DCI, PCI_DEVICE_ID_DCI_PCCOM8,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b3_8_115200 },
	 
	{	PCI_VENDOR_ID_TOPIC, PCI_DEVICE_ID_TOPIC_TP560,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b0_1_115200 },
	 
	{	PCI_VENDOR_ID_ITE, PCI_DEVICE_ID_ITE_8872,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b1_bt_1_115200 },

	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x0D60,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b2_1_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, PCI_DEVICE_ID_INTASHIELD_IS200,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,	 
		pbn_b2_2_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, PCI_DEVICE_ID_INTASHIELD_IS400,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,     
		pbn_b2_4_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x4027,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_1_15625000 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x4028,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_2_15625000 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x4029,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_4_15625000 },
	 
	 
	{       PCI_VENDOR_ID_INTASHIELD, 0x0BA1,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x0AA1,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_1_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x0AA2,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_1_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x0CA1,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x0D21,
		PCI_ANY_ID, PCI_ANY_ID,
		PCI_CLASS_COMMUNICATION_MULTISERIAL << 8, 0xffff00,
		pbn_b2_4_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x0E34,
		PCI_ANY_ID, PCI_ANY_ID,
		PCI_CLASS_COMMUNICATION_MULTISERIAL << 8, 0xffff00,
		pbn_b2_4_115200 },
	 
	{       PCI_VENDOR_ID_INTASHIELD, 0x0841,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_4_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x0881,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_8_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x08E1,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x08E2,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x08E3,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	 
	{       PCI_VENDOR_ID_INTASHIELD, 0x08C1,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	 
	{       PCI_VENDOR_ID_INTASHIELD, 0x08A1,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	{       PCI_VENDOR_ID_INTASHIELD, 0x08A2,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	{       PCI_VENDOR_ID_INTASHIELD, 0x08A3,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x0A61,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_1_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x0B01,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_4_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x0B02,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_4_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x0A81,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x0A82,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x0A83,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x0C41,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_4_115200 },
	 
	{       PCI_VENDOR_ID_INTASHIELD, 0x0921,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_4_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x09A1,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x09A2,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x09A3,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x0D41,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_4_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x0AC1,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x0AC2,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x0AC3,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x0B21,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x0B22,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x0B23,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x0C01,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x0C02,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x0C03,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x0C21,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x0C22,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x0C23,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_2_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x4005,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b0_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x4019,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_2_15625000 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x4004,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b0_1_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x4016,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_1_15625000 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x4006,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b0_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x4015,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_2_15625000 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x400A,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_4_15625000 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x0E41,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b2_8_115200 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x400E,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_2_15625000 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x400C,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_2_15625000 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x400B,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_1_15625000 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x400F,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_4_15625000 },
	 
	{       PCI_VENDOR_ID_INTASHIELD, 0x4010,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_4_15625000 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x4000,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b0_4_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x4011,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_4_15625000 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x401D,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_1_15625000 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x4009,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b0_2_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x4018,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_2_15625000 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x401E,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_2_15625000 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x4002,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b0_4_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x4013,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_4_15625000 },
	 
	{	PCI_VENDOR_ID_INTASHIELD, 0x4008,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_b0_1_115200 },
	{	PCI_VENDOR_ID_INTASHIELD, 0x4017,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0,
		pbn_oxsemi_1_15625000 },

	 
	{       PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9030,
		PCI_SUBVENDOR_ID_PERLE, PCI_SUBDEVICE_ID_PCI_RAS4,
		0, 0, pbn_b2_4_921600 },
	{       PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9030,
		PCI_SUBVENDOR_ID_PERLE, PCI_SUBDEVICE_ID_PCI_RAS8,
		0, 0, pbn_b2_8_921600 },

	 

	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0200,
		0, 0, pbn_b0_2_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0300,
		0, 0, pbn_b0_4_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0400,
		0, 0, pbn_b0_2_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0500,
		0, 0, pbn_b0_4_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0600,
		0, 0, pbn_b0_2_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0700,
		0, 0, pbn_b0_4_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0800,
		0, 0, pbn_b0_8_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0C00,
		0, 0, pbn_b0_2_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x0D00,
		0, 0, pbn_b0_4_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x1D00,
		0, 0, pbn_b0_8_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x2000,
		0, 0, pbn_b0_1_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x2100,
		0, 0, pbn_b0_1_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x2200,
		0, 0, pbn_b0_2_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x2300,
		0, 0, pbn_b0_2_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x2400,
		0, 0, pbn_b0_4_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x2500,
		0, 0, pbn_b0_4_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x2600,
		0, 0, pbn_b0_8_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x2700,
		0, 0, pbn_b0_8_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x3000,
		0, 0, pbn_b0_1_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x3100,
		0, 0, pbn_b0_1_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x3200,
		0, 0, pbn_b0_2_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x3300,
		0, 0, pbn_b0_2_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x3400,
		0, 0, pbn_b0_4_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x3500,
		0, 0, pbn_b0_4_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x3C00,
		0, 0, pbn_b0_8_115200 },
	{	 
		PCI_VENDOR_ID_MAINPINE, PCI_DEVICE_ID_MAINPINE_PBRIDGE,
		PCI_VENDOR_ID_MAINPINE, 0x3D00,
		0, 0, pbn_b0_8_115200 },


	 
	{	PCI_VENDOR_ID_PASEMI, 0xa004,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_pasemi_1682M },

	 
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI23216,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_16_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI2328,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_8_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI2324,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_4_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI2322,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_2_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI2324I,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_4_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI2322I,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_2_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8420_23216,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_16_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8420_2328,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_8_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8420_2324,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_4_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8420_2322,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_2_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8422_2324,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_4_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8422_2322,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_b1_bt_2_115200 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8430_2322,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_2 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI8430_2322,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_2 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8430_2324,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_4 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI8430_2324,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_4 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8430_2328,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_8 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI8430_2328,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_8 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8430_23216,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_16 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI8430_23216,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_16 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8432_2322,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_2 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI8432_2322,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_2 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PXI8432_2324,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_4 },
	{	PCI_VENDOR_ID_NI, PCI_DEVICE_ID_NI_PCI8432_2324,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_ni8430_4 },

	 
	{	PCI_VENDOR_ID_MOXA, PCI_DEVICE_ID_MOXA_CP102E,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_moxa8250_2p },
	{	PCI_VENDOR_ID_MOXA, PCI_DEVICE_ID_MOXA_CP102EL,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_moxa8250_2p },
	{	PCI_VENDOR_ID_MOXA, PCI_DEVICE_ID_MOXA_CP104EL_A,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_moxa8250_4p },
	{	PCI_VENDOR_ID_MOXA, PCI_DEVICE_ID_MOXA_CP114EL,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_moxa8250_4p },
	{	PCI_VENDOR_ID_MOXA, PCI_DEVICE_ID_MOXA_CP116E_A_A,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_moxa8250_8p },
	{	PCI_VENDOR_ID_MOXA, PCI_DEVICE_ID_MOXA_CP116E_A_B,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_moxa8250_8p },
	{	PCI_VENDOR_ID_MOXA, PCI_DEVICE_ID_MOXA_CP118EL_A,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_moxa8250_8p },
	{	PCI_VENDOR_ID_MOXA, PCI_DEVICE_ID_MOXA_CP118E_A_I,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_moxa8250_8p },
	{	PCI_VENDOR_ID_MOXA, PCI_DEVICE_ID_MOXA_CP132EL,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_moxa8250_2p },
	{	PCI_VENDOR_ID_MOXA, PCI_DEVICE_ID_MOXA_CP134EL_A,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_moxa8250_4p },
	{	PCI_VENDOR_ID_MOXA, PCI_DEVICE_ID_MOXA_CP138E_A,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_moxa8250_8p },
	{	PCI_VENDOR_ID_MOXA, PCI_DEVICE_ID_MOXA_CP168EL_A,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_moxa8250_8p },

	 
	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7500,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_4_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7420,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_2_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7300,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_1_115200 },

	{	PCI_VENDOR_ID_AMCC,
		PCI_DEVICE_ID_AMCC_ADDIDATA_APCI7800,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b1_8_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7500_2,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_4_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7420_2,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_2_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7300_2,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_1_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7500_3,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_4_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7420_3,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_2_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7300_3,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_1_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCI7800_3,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_b0_8_115200 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCIe7500,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_ADDIDATA_PCIe_4_3906250 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCIe7420,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_ADDIDATA_PCIe_2_3906250 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCIe7300,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_ADDIDATA_PCIe_1_3906250 },

	{	PCI_VENDOR_ID_ADDIDATA,
		PCI_DEVICE_ID_ADDIDATA_APCIe7800,
		PCI_ANY_ID,
		PCI_ANY_ID,
		0,
		0,
		pbn_ADDIDATA_PCIe_8_3906250 },

	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9835,
		PCI_VENDOR_ID_IBM, 0x0299,
		0, 0, pbn_b0_bt_2_115200 },

	 

	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9901,
		0xA000, 0x1000,
		0, 0, pbn_b0_1_115200 },

	 
	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9912,
		0xA000, 0x1000,
		0, 0, pbn_b0_1_115200 },

	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9922,
		0xA000, 0x1000,
		0, 0, pbn_b0_1_115200 },

	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9904,
		0xA000, 0x1000,
		0, 0, pbn_b0_1_115200 },

	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9900,
		0xA000, 0x1000,
		0, 0, pbn_b0_1_115200 },

	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9900,
		0xA000, 0x3002,
		0, 0, pbn_NETMOS9900_2s_115200 },

	 

	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9865,
		0xA000, 0x1000,
		0, 0, pbn_b0_1_115200 },

	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9865,
		0xA000, 0x3002,
		0, 0, pbn_b0_bt_2_115200 },

	{	PCI_VENDOR_ID_NETMOS, PCI_DEVICE_ID_NETMOS_9865,
		0xA000, 0x3004,
		0, 0, pbn_b0_bt_4_115200 },

	 
	{	PCI_VENDOR_ID_ASIX, PCI_DEVICE_ID_ASIX_AX99100,
		0xA000, 0x1000,
		0, 0, pbn_b0_1_115200 },

	 
	{	PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_CE4100_UART,
		PCI_ANY_ID,  PCI_ANY_ID, 0, 0,
		pbn_ce4100_1_115200 },

	 
	{	PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_CRONYX_OMEGA,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_omegapci },

	 
	{	PCI_VENDOR_ID_BROADCOM, PCI_DEVICE_ID_BROADCOM_TRUMANAGE,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0,
		pbn_brcm_trumanage },

	 
	{	PCI_VENDOR_ID_AGESTAR, PCI_DEVICE_ID_AGESTAR_9375,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0, pbn_b0_bt_2_115200 },

	 
	{	PCI_VENDOR_ID_WCH, PCI_DEVICE_ID_WCH_CH353_4S,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0, pbn_b0_bt_4_115200 },

	{	PCI_VENDOR_ID_WCH, PCI_DEVICE_ID_WCH_CH353_2S1PF,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0, pbn_b0_bt_2_115200 },

	{	PCI_VENDOR_ID_WCH, PCI_DEVICE_ID_WCH_CH355_4S,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0, pbn_b0_bt_4_115200 },

	{	PCIE_VENDOR_ID_WCH, PCIE_DEVICE_ID_WCH_CH382_2S,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0, pbn_wch382_2 },

	{	PCIE_VENDOR_ID_WCH, PCIE_DEVICE_ID_WCH_CH384_4S,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0, pbn_wch384_4 },

	{	PCIE_VENDOR_ID_WCH, PCIE_DEVICE_ID_WCH_CH384_8S,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0, pbn_wch384_8 },
	 
	{	PCI_VENDOR_ID_REALTEK, 0x816a,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0, pbn_b0_1_115200 },

	{	PCI_VENDOR_ID_REALTEK, 0x816b,
		PCI_ANY_ID, PCI_ANY_ID,
		0, 0, pbn_b0_1_115200 },

	 
	{ PCI_DEVICE(0x1c29, 0x1104), .driver_data = pbn_fintek_4 },
	{ PCI_DEVICE(0x1c29, 0x1108), .driver_data = pbn_fintek_8 },
	{ PCI_DEVICE(0x1c29, 0x1112), .driver_data = pbn_fintek_12 },
	{ PCI_DEVICE(0x1c29, 0x1204), .driver_data = pbn_fintek_F81504A },
	{ PCI_DEVICE(0x1c29, 0x1208), .driver_data = pbn_fintek_F81508A },
	{ PCI_DEVICE(0x1c29, 0x1212), .driver_data = pbn_fintek_F81512A },

	 
	{ PCI_DEVICE(0x1601, 0x0800), .driver_data = pbn_b0_4_1250000 },
	{ PCI_DEVICE(0x1601, 0xa801), .driver_data = pbn_b0_4_1250000 },

	 
	{ PCI_DEVICE(0x1d0f, 0x8250), .driver_data = pbn_b0_1_115200 },

	 
	{	PCI_ANY_ID, PCI_ANY_ID,
		PCI_ANY_ID, PCI_ANY_ID,
		PCI_CLASS_COMMUNICATION_SERIAL << 8,
		0xffff00, pbn_default },
	{	PCI_ANY_ID, PCI_ANY_ID,
		PCI_ANY_ID, PCI_ANY_ID,
		PCI_CLASS_COMMUNICATION_MODEM << 8,
		0xffff00, pbn_default },
	{	PCI_ANY_ID, PCI_ANY_ID,
		PCI_ANY_ID, PCI_ANY_ID,
		PCI_CLASS_COMMUNICATION_MULTISERIAL << 8,
		0xffff00, pbn_default },
	{ 0, }
};

static pci_ers_result_t serial8250_io_error_detected(struct pci_dev *dev,
						pci_channel_state_t state)
{
	struct serial_private *priv = pci_get_drvdata(dev);

	if (state == pci_channel_io_perm_failure)
		return PCI_ERS_RESULT_DISCONNECT;

	if (priv)
		pciserial_detach_ports(priv);

	pci_disable_device(dev);

	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t serial8250_io_slot_reset(struct pci_dev *dev)
{
	int rc;

	rc = pci_enable_device(dev);

	if (rc)
		return PCI_ERS_RESULT_DISCONNECT;

	pci_restore_state(dev);
	pci_save_state(dev);

	return PCI_ERS_RESULT_RECOVERED;
}

static void serial8250_io_resume(struct pci_dev *dev)
{
	struct serial_private *priv = pci_get_drvdata(dev);
	struct serial_private *new;

	if (!priv)
		return;

	new = pciserial_init_ports(dev, priv->board);
	if (!IS_ERR(new)) {
		pci_set_drvdata(dev, new);
		kfree(priv);
	}
}

static const struct pci_error_handlers serial8250_err_handler = {
	.error_detected = serial8250_io_error_detected,
	.slot_reset = serial8250_io_slot_reset,
	.resume = serial8250_io_resume,
};

static struct pci_driver serial_pci_driver = {
	.name		= "serial",
	.probe		= pciserial_init_one,
	.remove		= pciserial_remove_one,
	.driver         = {
		.pm     = &pciserial_pm_ops,
	},
	.id_table	= serial_pci_tbl,
	.err_handler	= &serial8250_err_handler,
};

module_pci_driver(serial_pci_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Generic 8250/16x50 PCI serial probe module");
MODULE_DEVICE_TABLE(pci, serial_pci_tbl);
MODULE_IMPORT_NS(SERIAL_8250_PCI);
