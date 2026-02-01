
 

#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_pci.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/bits.h>
#include "../pci.h"

 
#define IXP4XX_PCI_NP_AD		0x00
#define IXP4XX_PCI_NP_CBE		0x04
#define IXP4XX_PCI_NP_WDATA		0x08
#define IXP4XX_PCI_NP_RDATA		0x0c
#define IXP4XX_PCI_CRP_AD_CBE		0x10
#define IXP4XX_PCI_CRP_WDATA		0x14
#define IXP4XX_PCI_CRP_RDATA		0x18
#define IXP4XX_PCI_CSR			0x1c
#define IXP4XX_PCI_ISR			0x20
#define IXP4XX_PCI_INTEN		0x24
#define IXP4XX_PCI_DMACTRL		0x28
#define IXP4XX_PCI_AHBMEMBASE		0x2c
#define IXP4XX_PCI_AHBIOBASE		0x30
#define IXP4XX_PCI_PCIMEMBASE		0x34
#define IXP4XX_PCI_AHBDOORBELL		0x38
#define IXP4XX_PCI_PCIDOORBELL		0x3c
#define IXP4XX_PCI_ATPDMA0_AHBADDR	0x40
#define IXP4XX_PCI_ATPDMA0_PCIADDR	0x44
#define IXP4XX_PCI_ATPDMA0_LENADDR	0x48
#define IXP4XX_PCI_ATPDMA1_AHBADDR	0x4c
#define IXP4XX_PCI_ATPDMA1_PCIADDR	0x50
#define IXP4XX_PCI_ATPDMA1_LENADDR	0x54

 
#define IXP4XX_PCI_CSR_HOST		BIT(0)
#define IXP4XX_PCI_CSR_ARBEN		BIT(1)
#define IXP4XX_PCI_CSR_ADS		BIT(2)
#define IXP4XX_PCI_CSR_PDS		BIT(3)
#define IXP4XX_PCI_CSR_ABE		BIT(4)
#define IXP4XX_PCI_CSR_DBT		BIT(5)
#define IXP4XX_PCI_CSR_ASE		BIT(8)
#define IXP4XX_PCI_CSR_IC		BIT(15)
#define IXP4XX_PCI_CSR_PRST		BIT(16)

 
#define IXP4XX_PCI_ISR_PSE		BIT(0)
#define IXP4XX_PCI_ISR_PFE		BIT(1)
#define IXP4XX_PCI_ISR_PPE		BIT(2)
#define IXP4XX_PCI_ISR_AHBE		BIT(3)
#define IXP4XX_PCI_ISR_APDC		BIT(4)
#define IXP4XX_PCI_ISR_PADC		BIT(5)
#define IXP4XX_PCI_ISR_ADB		BIT(6)
#define IXP4XX_PCI_ISR_PDB		BIT(7)

 
#define IXP4XX_PCI_INTEN_PSE		BIT(0)
#define IXP4XX_PCI_INTEN_PFE		BIT(1)
#define IXP4XX_PCI_INTEN_PPE		BIT(2)
#define IXP4XX_PCI_INTEN_AHBE		BIT(3)
#define IXP4XX_PCI_INTEN_APDC		BIT(4)
#define IXP4XX_PCI_INTEN_PADC		BIT(5)
#define IXP4XX_PCI_INTEN_ADB		BIT(6)
#define IXP4XX_PCI_INTEN_PDB		BIT(7)

 
#define IXP4XX_PCI_NP_CBE_BESL		4

 
#define NP_CMD_IOREAD			0x2
#define NP_CMD_IOWRITE			0x3
#define NP_CMD_CONFIGREAD		0xa
#define NP_CMD_CONFIGWRITE		0xb
#define NP_CMD_MEMREAD			0x6
#define	NP_CMD_MEMWRITE			0x7

 
#define CRP_AD_CBE_BESL         20
#define CRP_AD_CBE_WRITE	0x00010000

 
#define IXP4XX_PCI_RTOTTO		0x40

struct ixp4xx_pci {
	struct device *dev;
	void __iomem *base;
	bool errata_hammer;
	bool host_mode;
};

 
static inline u32 ixp4xx_readl(struct ixp4xx_pci *p, u32 reg)
{
	return __raw_readl(p->base + reg);
}

static inline void ixp4xx_writel(struct ixp4xx_pci *p, u32 reg, u32 val)
{
	__raw_writel(val, p->base + reg);
}

static int ixp4xx_pci_check_master_abort(struct ixp4xx_pci *p)
{
	u32 isr = ixp4xx_readl(p, IXP4XX_PCI_ISR);

	if (isr & IXP4XX_PCI_ISR_PFE) {
		 
		ixp4xx_writel(p, IXP4XX_PCI_ISR, IXP4XX_PCI_ISR_PFE);
		dev_dbg(p->dev, "master abort detected\n");
		return -EINVAL;
	}

	return 0;
}

static int ixp4xx_pci_read_indirect(struct ixp4xx_pci *p, u32 addr, u32 cmd, u32 *data)
{
	ixp4xx_writel(p, IXP4XX_PCI_NP_AD, addr);

	if (p->errata_hammer) {
		int i;

		 
		for (i = 0; i < 8; i++) {
			ixp4xx_writel(p, IXP4XX_PCI_NP_CBE, cmd);
			*data = ixp4xx_readl(p, IXP4XX_PCI_NP_RDATA);
			*data = ixp4xx_readl(p, IXP4XX_PCI_NP_RDATA);
		}
	} else {
		ixp4xx_writel(p, IXP4XX_PCI_NP_CBE, cmd);
		*data = ixp4xx_readl(p, IXP4XX_PCI_NP_RDATA);
	}

	return ixp4xx_pci_check_master_abort(p);
}

static int ixp4xx_pci_write_indirect(struct ixp4xx_pci *p, u32 addr, u32 cmd, u32 data)
{
	ixp4xx_writel(p, IXP4XX_PCI_NP_AD, addr);

	 
	ixp4xx_writel(p, IXP4XX_PCI_NP_CBE, cmd);

	 
	ixp4xx_writel(p, IXP4XX_PCI_NP_WDATA, data);

	return ixp4xx_pci_check_master_abort(p);
}

static u32 ixp4xx_config_addr(u8 bus_num, u16 devfn, int where)
{
	 
	if (bus_num == 0) {
		 
		return (PCI_CONF1_ADDRESS(0, 0, PCI_FUNC(devfn), where) &
			~PCI_CONF1_ENABLE) | BIT(32-PCI_SLOT(devfn));
	} else {
		 
		return (PCI_CONF1_ADDRESS(bus_num, PCI_SLOT(devfn),
					  PCI_FUNC(devfn), where) &
			~PCI_CONF1_ENABLE) | 1;
	}
}

 
static u32 ixp4xx_crp_byte_lane_enable_bits(u32 n, int size)
{
	if (size == 1)
		return (0xf & ~BIT(n)) << CRP_AD_CBE_BESL;
	if (size == 2)
		return (0xf & ~(BIT(n) | BIT(n+1))) << CRP_AD_CBE_BESL;
	if (size == 4)
		return 0;
	return 0xffffffff;
}

static int ixp4xx_crp_read_config(struct ixp4xx_pci *p, int where, int size,
				  u32 *value)
{
	u32 n, cmd, val;

	n = where % 4;
	cmd = where & ~3;

	dev_dbg(p->dev, "%s from %d size %d cmd %08x\n",
		__func__, where, size, cmd);

	ixp4xx_writel(p, IXP4XX_PCI_CRP_AD_CBE, cmd);
	val = ixp4xx_readl(p, IXP4XX_PCI_CRP_RDATA);

	val >>= (8*n);
	switch (size) {
	case 1:
		val &= U8_MAX;
		dev_dbg(p->dev, "%s read byte %02x\n", __func__, val);
		break;
	case 2:
		val &= U16_MAX;
		dev_dbg(p->dev, "%s read word %04x\n", __func__, val);
		break;
	case 4:
		val &= U32_MAX;
		dev_dbg(p->dev, "%s read long %08x\n", __func__, val);
		break;
	default:
		 
		dev_err(p->dev, "%s illegal size\n", __func__);
		return PCIBIOS_DEVICE_NOT_FOUND;
	}
	*value = val;

	return PCIBIOS_SUCCESSFUL;
}

static int ixp4xx_crp_write_config(struct ixp4xx_pci *p, int where, int size,
				   u32 value)
{
	u32 n, cmd, val;

	n = where % 4;
	cmd = ixp4xx_crp_byte_lane_enable_bits(n, size);
	if (cmd == 0xffffffff)
		return PCIBIOS_BAD_REGISTER_NUMBER;
	cmd |= where & ~3;
	cmd |= CRP_AD_CBE_WRITE;

	val = value << (8*n);

	dev_dbg(p->dev, "%s to %d size %d cmd %08x val %08x\n",
		__func__, where, size, cmd, val);

	ixp4xx_writel(p, IXP4XX_PCI_CRP_AD_CBE, cmd);
	ixp4xx_writel(p, IXP4XX_PCI_CRP_WDATA, val);

	return PCIBIOS_SUCCESSFUL;
}

 
static u32 ixp4xx_byte_lane_enable_bits(u32 n, int size)
{
	if (size == 1)
		return (0xf & ~BIT(n)) << 4;
	if (size == 2)
		return (0xf & ~(BIT(n) | BIT(n+1))) << 4;
	if (size == 4)
		return 0;
	return 0xffffffff;
}

static int ixp4xx_pci_read_config(struct pci_bus *bus, unsigned int devfn,
				  int where, int size, u32 *value)
{
	struct ixp4xx_pci *p = bus->sysdata;
	u32 n, addr, val, cmd;
	u8 bus_num = bus->number;
	int ret;

	*value = 0xffffffff;
	n = where % 4;
	cmd = ixp4xx_byte_lane_enable_bits(n, size);
	if (cmd == 0xffffffff)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	addr = ixp4xx_config_addr(bus_num, devfn, where);
	cmd |= NP_CMD_CONFIGREAD;
	dev_dbg(p->dev, "read_config from %d size %d dev %d:%d:%d address: %08x cmd: %08x\n",
		where, size, bus_num, PCI_SLOT(devfn), PCI_FUNC(devfn), addr, cmd);

	ret = ixp4xx_pci_read_indirect(p, addr, cmd, &val);
	if (ret)
		return PCIBIOS_DEVICE_NOT_FOUND;

	val >>= (8*n);
	switch (size) {
	case 1:
		val &= U8_MAX;
		dev_dbg(p->dev, "%s read byte %02x\n", __func__, val);
		break;
	case 2:
		val &= U16_MAX;
		dev_dbg(p->dev, "%s read word %04x\n", __func__, val);
		break;
	case 4:
		val &= U32_MAX;
		dev_dbg(p->dev, "%s read long %08x\n", __func__, val);
		break;
	default:
		 
		dev_err(p->dev, "%s illegal size\n", __func__);
		return PCIBIOS_DEVICE_NOT_FOUND;
	}
	*value = val;

	return PCIBIOS_SUCCESSFUL;
}

static int ixp4xx_pci_write_config(struct pci_bus *bus,  unsigned int devfn,
				   int where, int size, u32 value)
{
	struct ixp4xx_pci *p = bus->sysdata;
	u32 n, addr, val, cmd;
	u8 bus_num = bus->number;
	int ret;

	n = where % 4;
	cmd = ixp4xx_byte_lane_enable_bits(n, size);
	if (cmd == 0xffffffff)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	addr = ixp4xx_config_addr(bus_num, devfn, where);
	cmd |= NP_CMD_CONFIGWRITE;
	val = value << (8*n);

	dev_dbg(p->dev, "write_config_byte %#x to %d size %d dev %d:%d:%d addr: %08x cmd %08x\n",
		value, where, size, bus_num, PCI_SLOT(devfn), PCI_FUNC(devfn), addr, cmd);

	ret = ixp4xx_pci_write_indirect(p, addr, cmd, val);
	if (ret)
		return PCIBIOS_DEVICE_NOT_FOUND;

	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops ixp4xx_pci_ops = {
	.read = ixp4xx_pci_read_config,
	.write = ixp4xx_pci_write_config,
};

static u32 ixp4xx_pci_addr_to_64mconf(phys_addr_t addr)
{
	u8 base;

	base = ((addr & 0xff000000) >> 24);
	return (base << 24) | ((base + 1) << 16)
		| ((base + 2) << 8) | (base + 3);
}

static int ixp4xx_pci_parse_map_ranges(struct ixp4xx_pci *p)
{
	struct device *dev = p->dev;
	struct pci_host_bridge *bridge = pci_host_bridge_from_priv(p);
	struct resource_entry *win;
	struct resource *res;
	phys_addr_t addr;

	win = resource_list_first_type(&bridge->windows, IORESOURCE_MEM);
	if (win) {
		u32 pcimembase;

		res = win->res;
		addr = res->start - win->offset;

		if (res->flags & IORESOURCE_PREFETCH)
			res->name = "IXP4xx PCI PRE-MEM";
		else
			res->name = "IXP4xx PCI NON-PRE-MEM";

		dev_dbg(dev, "%s window %pR, bus addr %pa\n",
			res->name, res, &addr);
		if (resource_size(res) != SZ_64M) {
			dev_err(dev, "memory range is not 64MB\n");
			return -EINVAL;
		}

		pcimembase = ixp4xx_pci_addr_to_64mconf(addr);
		 
		ixp4xx_writel(p, IXP4XX_PCI_PCIMEMBASE, pcimembase);
	} else {
		dev_err(dev, "no AHB memory mapping defined\n");
	}

	win = resource_list_first_type(&bridge->windows, IORESOURCE_IO);
	if (win) {
		res = win->res;

		addr = pci_pio_to_address(res->start);
		if (addr & 0xff) {
			dev_err(dev, "IO mem at uneven address: %pa\n", &addr);
			return -EINVAL;
		}

		res->name = "IXP4xx PCI IO MEM";
		 
		ixp4xx_writel(p, IXP4XX_PCI_AHBIOBASE, (addr >> 8));
	} else {
		dev_info(dev, "no IO space AHB memory mapping defined\n");
	}

	return 0;
}

static int ixp4xx_pci_parse_map_dma_ranges(struct ixp4xx_pci *p)
{
	struct device *dev = p->dev;
	struct pci_host_bridge *bridge = pci_host_bridge_from_priv(p);
	struct resource_entry *win;
	struct resource *res;
	phys_addr_t addr;
	u32 ahbmembase;

	win = resource_list_first_type(&bridge->dma_ranges, IORESOURCE_MEM);
	if (win) {
		res = win->res;
		addr = res->start - win->offset;

		if (resource_size(res) != SZ_64M) {
			dev_err(dev, "DMA memory range is not 64MB\n");
			return -EINVAL;
		}

		dev_dbg(dev, "DMA MEM BASE: %pa\n", &addr);
		 
		ahbmembase = ixp4xx_pci_addr_to_64mconf(addr);
		 
		ixp4xx_writel(p, IXP4XX_PCI_AHBMEMBASE, ahbmembase);
	} else {
		dev_err(dev, "no DMA memory range defined\n");
	}

	return 0;
}

 
static struct ixp4xx_pci *ixp4xx_pci_abort_singleton;

static int ixp4xx_pci_abort_handler(unsigned long addr, unsigned int fsr,
				    struct pt_regs *regs)
{
	struct ixp4xx_pci *p = ixp4xx_pci_abort_singleton;
	u32 isr, status;
	int ret;

	isr = ixp4xx_readl(p, IXP4XX_PCI_ISR);
	ret = ixp4xx_crp_read_config(p, PCI_STATUS, 2, &status);
	if (ret) {
		dev_err(p->dev, "unable to read abort status\n");
		return -EINVAL;
	}

	dev_err(p->dev,
		"PCI: abort_handler addr = %#lx, isr = %#x, status = %#x\n",
		addr, isr, status);

	 
	ixp4xx_writel(p, IXP4XX_PCI_ISR, IXP4XX_PCI_ISR_PFE);
	status |= PCI_STATUS_REC_MASTER_ABORT;
	ret = ixp4xx_crp_write_config(p, PCI_STATUS, 2, status);
	if (ret)
		dev_err(p->dev, "unable to clear abort status bit\n");

	 
	if (fsr & (1 << 10)) {
		dev_err(p->dev, "imprecise abort\n");
		regs->ARM_pc += 4;
	}

	return 0;
}

static int __init ixp4xx_pci_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct ixp4xx_pci *p;
	struct pci_host_bridge *host;
	int ret;
	u32 val;
	phys_addr_t addr;
	u32 basereg[4] = {
		PCI_BASE_ADDRESS_0,
		PCI_BASE_ADDRESS_1,
		PCI_BASE_ADDRESS_2,
		PCI_BASE_ADDRESS_3,
	};
	int i;

	host = devm_pci_alloc_host_bridge(dev, sizeof(*p));
	if (!host)
		return -ENOMEM;

	host->ops = &ixp4xx_pci_ops;
	p = pci_host_bridge_priv(host);
	host->sysdata = p;
	p->dev = dev;
	dev_set_drvdata(dev, p);

	 
	if (of_device_is_compatible(np, "intel,ixp42x-pci")) {
		p->errata_hammer = true;
		dev_info(dev, "activate hammering errata\n");
	}

	p->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(p->base))
		return PTR_ERR(p->base);

	val = ixp4xx_readl(p, IXP4XX_PCI_CSR);
	p->host_mode = !!(val & IXP4XX_PCI_CSR_HOST);
	dev_info(dev, "controller is in %s mode\n",
		 p->host_mode ? "host" : "option");

	 
	ixp4xx_pci_abort_singleton = p;
	hook_fault_code(16+6, ixp4xx_pci_abort_handler, SIGBUS, 0,
			"imprecise external abort");

	ret = ixp4xx_pci_parse_map_ranges(p);
	if (ret)
		return ret;

	ret = ixp4xx_pci_parse_map_dma_ranges(p);
	if (ret)
		return ret;

	 
	if (p->host_mode) {
		addr = __pa(PAGE_OFFSET);
		 
		addr |= PCI_BASE_ADDRESS_SPACE_MEMORY;

		for (i = 0; i < 4; i++) {
			 
			ret = ixp4xx_crp_write_config(p, basereg[i], 4, addr);
			if (ret)
				dev_err(dev, "failed to set up PCI_BASE_ADDRESS_%d\n", i);
			else
				dev_info(dev, "set PCI_BASE_ADDR_%d to %pa\n", i, &addr);
			addr += SZ_16M;
		}

		 
		ret = ixp4xx_crp_write_config(p, PCI_BASE_ADDRESS_4, 4, addr);
		if (ret)
			dev_err(dev, "failed to set up PCI_BASE_ADDRESS_4\n");
		else
			dev_info(dev, "set PCI_BASE_ADDR_4 to %pa\n", &addr);

		 
		addr = 0xfffffc00;
		addr |= PCI_BASE_ADDRESS_SPACE_IO;
		ret = ixp4xx_crp_write_config(p, PCI_BASE_ADDRESS_5, 4, addr);
		if (ret)
			dev_err(dev, "failed to set up PCI_BASE_ADDRESS_5\n");
		else
			dev_info(dev, "set PCI_BASE_ADDR_5 to %pa\n", &addr);

		 
		ret = ixp4xx_crp_write_config(p, IXP4XX_PCI_RTOTTO, 4,
					      0x000080ff);
		if (ret)
			dev_err(dev, "failed to set up TRDY limit\n");
		else
			dev_info(dev, "set TRDY limit to 0x80ff\n");
	}

	 
	val = IXP4XX_PCI_ISR_PSE | IXP4XX_PCI_ISR_PFE | IXP4XX_PCI_ISR_PPE | IXP4XX_PCI_ISR_AHBE;
	ixp4xx_writel(p, IXP4XX_PCI_ISR, val);

	 
	val = IXP4XX_PCI_CSR_IC | IXP4XX_PCI_CSR_ABE;
	if (IS_ENABLED(CONFIG_CPU_BIG_ENDIAN))
		val |= (IXP4XX_PCI_CSR_PDS | IXP4XX_PCI_CSR_ADS);
	ixp4xx_writel(p, IXP4XX_PCI_CSR, val);

	ret = ixp4xx_crp_write_config(p, PCI_COMMAND, 2, PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY);
	if (ret)
		dev_err(dev, "unable to initialize master and command memory\n");
	else
		dev_info(dev, "initialized as master\n");

	pci_host_probe(host);

	return 0;
}

static const struct of_device_id ixp4xx_pci_of_match[] = {
	{
		.compatible = "intel,ixp42x-pci",
	},
	{
		.compatible = "intel,ixp43x-pci",
	},
	{},
};

 
static struct platform_driver ixp4xx_pci_driver = {
	.driver = {
		.name = "ixp4xx-pci",
		.suppress_bind_attrs = true,
		.of_match_table = ixp4xx_pci_of_match,
	},
};
builtin_platform_driver_probe(ixp4xx_pci_driver, ixp4xx_pci_probe);
