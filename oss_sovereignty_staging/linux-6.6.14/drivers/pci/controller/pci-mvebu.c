
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/mbus.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_pci.h>
#include <linux/of_platform.h>

#include "../pci.h"
#include "../pci-bridge-emul.h"

 
#define PCIE_DEV_ID_OFF		0x0000
#define PCIE_CMD_OFF		0x0004
#define PCIE_DEV_REV_OFF	0x0008
#define PCIE_BAR_LO_OFF(n)	(0x0010 + ((n) << 3))
#define PCIE_BAR_HI_OFF(n)	(0x0014 + ((n) << 3))
#define PCIE_SSDEV_ID_OFF	0x002c
#define PCIE_CAP_PCIEXP		0x0060
#define PCIE_CAP_PCIERR_OFF	0x0100
#define PCIE_BAR_CTRL_OFF(n)	(0x1804 + (((n) - 1) * 4))
#define PCIE_WIN04_CTRL_OFF(n)	(0x1820 + ((n) << 4))
#define PCIE_WIN04_BASE_OFF(n)	(0x1824 + ((n) << 4))
#define PCIE_WIN04_REMAP_OFF(n)	(0x182c + ((n) << 4))
#define PCIE_WIN5_CTRL_OFF	0x1880
#define PCIE_WIN5_BASE_OFF	0x1884
#define PCIE_WIN5_REMAP_OFF	0x188c
#define PCIE_CONF_ADDR_OFF	0x18f8
#define  PCIE_CONF_ADDR_EN		0x80000000
#define  PCIE_CONF_REG(r)		((((r) & 0xf00) << 16) | ((r) & 0xfc))
#define  PCIE_CONF_BUS(b)		(((b) & 0xff) << 16)
#define  PCIE_CONF_DEV(d)		(((d) & 0x1f) << 11)
#define  PCIE_CONF_FUNC(f)		(((f) & 0x7) << 8)
#define  PCIE_CONF_ADDR(bus, devfn, where) \
	(PCIE_CONF_BUS(bus) | PCIE_CONF_DEV(PCI_SLOT(devfn))    | \
	 PCIE_CONF_FUNC(PCI_FUNC(devfn)) | PCIE_CONF_REG(where) | \
	 PCIE_CONF_ADDR_EN)
#define PCIE_CONF_DATA_OFF	0x18fc
#define PCIE_INT_CAUSE_OFF	0x1900
#define PCIE_INT_UNMASK_OFF	0x1910
#define  PCIE_INT_INTX(i)		BIT(24+i)
#define  PCIE_INT_PM_PME		BIT(28)
#define  PCIE_INT_ALL_MASK		GENMASK(31, 0)
#define PCIE_CTRL_OFF		0x1a00
#define  PCIE_CTRL_X1_MODE		0x0001
#define  PCIE_CTRL_RC_MODE		BIT(1)
#define  PCIE_CTRL_MASTER_HOT_RESET	BIT(24)
#define PCIE_STAT_OFF		0x1a04
#define  PCIE_STAT_BUS                  0xff00
#define  PCIE_STAT_DEV                  0x1f0000
#define  PCIE_STAT_LINK_DOWN		BIT(0)
#define PCIE_SSPL_OFF		0x1a0c
#define  PCIE_SSPL_VALUE_SHIFT		0
#define  PCIE_SSPL_VALUE_MASK		GENMASK(7, 0)
#define  PCIE_SSPL_SCALE_SHIFT		8
#define  PCIE_SSPL_SCALE_MASK		GENMASK(9, 8)
#define  PCIE_SSPL_ENABLE		BIT(16)
#define PCIE_RC_RTSTA		0x1a14
#define PCIE_DEBUG_CTRL         0x1a60
#define  PCIE_DEBUG_SOFT_RESET		BIT(20)

struct mvebu_pcie_port;

 
struct mvebu_pcie {
	struct platform_device *pdev;
	struct mvebu_pcie_port *ports;
	struct resource io;
	struct resource realio;
	struct resource mem;
	int nports;
};

struct mvebu_pcie_window {
	phys_addr_t base;
	phys_addr_t remap;
	size_t size;
};

 
struct mvebu_pcie_port {
	char *name;
	void __iomem *base;
	u32 port;
	u32 lane;
	bool is_x4;
	int devfn;
	unsigned int mem_target;
	unsigned int mem_attr;
	unsigned int io_target;
	unsigned int io_attr;
	struct clk *clk;
	struct gpio_desc *reset_gpio;
	char *reset_name;
	struct pci_bridge_emul bridge;
	struct device_node *dn;
	struct mvebu_pcie *pcie;
	struct mvebu_pcie_window memwin;
	struct mvebu_pcie_window iowin;
	u32 saved_pcie_stat;
	struct resource regs;
	u8 slot_power_limit_value;
	u8 slot_power_limit_scale;
	struct irq_domain *intx_irq_domain;
	raw_spinlock_t irq_lock;
	int intx_irq;
};

static inline void mvebu_writel(struct mvebu_pcie_port *port, u32 val, u32 reg)
{
	writel(val, port->base + reg);
}

static inline u32 mvebu_readl(struct mvebu_pcie_port *port, u32 reg)
{
	return readl(port->base + reg);
}

static inline bool mvebu_has_ioport(struct mvebu_pcie_port *port)
{
	return port->io_target != -1 && port->io_attr != -1;
}

static bool mvebu_pcie_link_up(struct mvebu_pcie_port *port)
{
	return !(mvebu_readl(port, PCIE_STAT_OFF) & PCIE_STAT_LINK_DOWN);
}

static u8 mvebu_pcie_get_local_bus_nr(struct mvebu_pcie_port *port)
{
	return (mvebu_readl(port, PCIE_STAT_OFF) & PCIE_STAT_BUS) >> 8;
}

static void mvebu_pcie_set_local_bus_nr(struct mvebu_pcie_port *port, int nr)
{
	u32 stat;

	stat = mvebu_readl(port, PCIE_STAT_OFF);
	stat &= ~PCIE_STAT_BUS;
	stat |= nr << 8;
	mvebu_writel(port, stat, PCIE_STAT_OFF);
}

static void mvebu_pcie_set_local_dev_nr(struct mvebu_pcie_port *port, int nr)
{
	u32 stat;

	stat = mvebu_readl(port, PCIE_STAT_OFF);
	stat &= ~PCIE_STAT_DEV;
	stat |= nr << 16;
	mvebu_writel(port, stat, PCIE_STAT_OFF);
}

static void mvebu_pcie_disable_wins(struct mvebu_pcie_port *port)
{
	int i;

	mvebu_writel(port, 0, PCIE_BAR_LO_OFF(0));
	mvebu_writel(port, 0, PCIE_BAR_HI_OFF(0));

	for (i = 1; i < 3; i++) {
		mvebu_writel(port, 0, PCIE_BAR_CTRL_OFF(i));
		mvebu_writel(port, 0, PCIE_BAR_LO_OFF(i));
		mvebu_writel(port, 0, PCIE_BAR_HI_OFF(i));
	}

	for (i = 0; i < 5; i++) {
		mvebu_writel(port, 0, PCIE_WIN04_CTRL_OFF(i));
		mvebu_writel(port, 0, PCIE_WIN04_BASE_OFF(i));
		mvebu_writel(port, 0, PCIE_WIN04_REMAP_OFF(i));
	}

	mvebu_writel(port, 0, PCIE_WIN5_CTRL_OFF);
	mvebu_writel(port, 0, PCIE_WIN5_BASE_OFF);
	mvebu_writel(port, 0, PCIE_WIN5_REMAP_OFF);
}

 
static void mvebu_pcie_setup_wins(struct mvebu_pcie_port *port)
{
	const struct mbus_dram_target_info *dram;
	u32 size;
	int i;

	dram = mv_mbus_dram_info();

	 
	mvebu_pcie_disable_wins(port);

	 
	size = 0;
	for (i = 0; i < dram->num_cs; i++) {
		const struct mbus_dram_window *cs = dram->cs + i;

		mvebu_writel(port, cs->base & 0xffff0000,
			     PCIE_WIN04_BASE_OFF(i));
		mvebu_writel(port, 0, PCIE_WIN04_REMAP_OFF(i));
		mvebu_writel(port,
			     ((cs->size - 1) & 0xffff0000) |
			     (cs->mbus_attr << 8) |
			     (dram->mbus_dram_target_id << 4) | 1,
			     PCIE_WIN04_CTRL_OFF(i));

		size += cs->size;
	}

	 
	if ((size & (size - 1)) != 0)
		size = 1 << fls(size);

	 
	mvebu_writel(port, dram->cs[0].base, PCIE_BAR_LO_OFF(1));
	mvebu_writel(port, 0, PCIE_BAR_HI_OFF(1));
	mvebu_writel(port, ((size - 1) & 0xffff0000) | 1,
		     PCIE_BAR_CTRL_OFF(1));

	 
	mvebu_writel(port, round_down(port->regs.start, SZ_1M), PCIE_BAR_LO_OFF(0));
	mvebu_writel(port, 0, PCIE_BAR_HI_OFF(0));
}

static void mvebu_pcie_setup_hw(struct mvebu_pcie_port *port)
{
	u32 ctrl, lnkcap, cmd, dev_rev, unmask, sspl;

	 
	ctrl = mvebu_readl(port, PCIE_CTRL_OFF);
	ctrl |= PCIE_CTRL_RC_MODE;
	mvebu_writel(port, ctrl, PCIE_CTRL_OFF);

	 
	lnkcap = mvebu_readl(port, PCIE_CAP_PCIEXP + PCI_EXP_LNKCAP);
	lnkcap &= ~PCI_EXP_LNKCAP_MLW;
	lnkcap |= FIELD_PREP(PCI_EXP_LNKCAP_MLW, port->is_x4 ? 4 : 1);
	mvebu_writel(port, lnkcap, PCIE_CAP_PCIEXP + PCI_EXP_LNKCAP);

	 
	cmd = mvebu_readl(port, PCIE_CMD_OFF);
	cmd &= ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
	mvebu_writel(port, cmd, PCIE_CMD_OFF);

	 
	dev_rev = mvebu_readl(port, PCIE_DEV_REV_OFF);
	dev_rev &= ~0xffffff00;
	dev_rev |= PCI_CLASS_BRIDGE_PCI_NORMAL << 8;
	mvebu_writel(port, dev_rev, PCIE_DEV_REV_OFF);

	 
	mvebu_pcie_setup_wins(port);

	 
	sspl = mvebu_readl(port, PCIE_SSPL_OFF);
	sspl &= ~(PCIE_SSPL_VALUE_MASK | PCIE_SSPL_SCALE_MASK | PCIE_SSPL_ENABLE);
	if (port->slot_power_limit_value) {
		sspl |= port->slot_power_limit_value << PCIE_SSPL_VALUE_SHIFT;
		sspl |= port->slot_power_limit_scale << PCIE_SSPL_SCALE_SHIFT;
		sspl |= PCIE_SSPL_ENABLE;
	}
	mvebu_writel(port, sspl, PCIE_SSPL_OFF);

	 
	mvebu_writel(port, ~PCIE_INT_ALL_MASK, PCIE_INT_UNMASK_OFF);

	 
	mvebu_writel(port, ~PCIE_INT_ALL_MASK, PCIE_INT_CAUSE_OFF);

	 
	if (port->intx_irq > 0)
		return;

	 
	unmask = mvebu_readl(port, PCIE_INT_UNMASK_OFF);
	unmask |= PCIE_INT_INTX(0) | PCIE_INT_INTX(1) |
		  PCIE_INT_INTX(2) | PCIE_INT_INTX(3);
	mvebu_writel(port, unmask, PCIE_INT_UNMASK_OFF);
}

static struct mvebu_pcie_port *mvebu_pcie_find_port(struct mvebu_pcie *pcie,
						    struct pci_bus *bus,
						    int devfn);

static int mvebu_pcie_child_rd_conf(struct pci_bus *bus, u32 devfn, int where,
				    int size, u32 *val)
{
	struct mvebu_pcie *pcie = bus->sysdata;
	struct mvebu_pcie_port *port;
	void __iomem *conf_data;

	port = mvebu_pcie_find_port(pcie, bus, devfn);
	if (!port)
		return PCIBIOS_DEVICE_NOT_FOUND;

	if (!mvebu_pcie_link_up(port))
		return PCIBIOS_DEVICE_NOT_FOUND;

	conf_data = port->base + PCIE_CONF_DATA_OFF;

	mvebu_writel(port, PCIE_CONF_ADDR(bus->number, devfn, where),
		     PCIE_CONF_ADDR_OFF);

	switch (size) {
	case 1:
		*val = readb_relaxed(conf_data + (where & 3));
		break;
	case 2:
		*val = readw_relaxed(conf_data + (where & 2));
		break;
	case 4:
		*val = readl_relaxed(conf_data);
		break;
	default:
		return PCIBIOS_BAD_REGISTER_NUMBER;
	}

	return PCIBIOS_SUCCESSFUL;
}

static int mvebu_pcie_child_wr_conf(struct pci_bus *bus, u32 devfn,
				    int where, int size, u32 val)
{
	struct mvebu_pcie *pcie = bus->sysdata;
	struct mvebu_pcie_port *port;
	void __iomem *conf_data;

	port = mvebu_pcie_find_port(pcie, bus, devfn);
	if (!port)
		return PCIBIOS_DEVICE_NOT_FOUND;

	if (!mvebu_pcie_link_up(port))
		return PCIBIOS_DEVICE_NOT_FOUND;

	conf_data = port->base + PCIE_CONF_DATA_OFF;

	mvebu_writel(port, PCIE_CONF_ADDR(bus->number, devfn, where),
		     PCIE_CONF_ADDR_OFF);

	switch (size) {
	case 1:
		writeb(val, conf_data + (where & 3));
		break;
	case 2:
		writew(val, conf_data + (where & 2));
		break;
	case 4:
		writel(val, conf_data);
		break;
	default:
		return PCIBIOS_BAD_REGISTER_NUMBER;
	}

	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops mvebu_pcie_child_ops = {
	.read = mvebu_pcie_child_rd_conf,
	.write = mvebu_pcie_child_wr_conf,
};

 
static void mvebu_pcie_del_windows(struct mvebu_pcie_port *port,
				   phys_addr_t base, size_t size)
{
	while (size) {
		size_t sz = 1 << (fls(size) - 1);

		mvebu_mbus_del_window(base, sz);
		base += sz;
		size -= sz;
	}
}

 
static int mvebu_pcie_add_windows(struct mvebu_pcie_port *port,
				   unsigned int target, unsigned int attribute,
				   phys_addr_t base, size_t size,
				   phys_addr_t remap)
{
	size_t size_mapped = 0;

	while (size) {
		size_t sz = 1 << (fls(size) - 1);
		int ret;

		ret = mvebu_mbus_add_window_remap_by_id(target, attribute, base,
							sz, remap);
		if (ret) {
			phys_addr_t end = base + sz - 1;

			dev_err(&port->pcie->pdev->dev,
				"Could not create MBus window at [mem %pa-%pa]: %d\n",
				&base, &end, ret);
			mvebu_pcie_del_windows(port, base - size_mapped,
					       size_mapped);
			return ret;
		}

		size -= sz;
		size_mapped += sz;
		base += sz;
		if (remap != MVEBU_MBUS_NO_REMAP)
			remap += sz;
	}

	return 0;
}

static int mvebu_pcie_set_window(struct mvebu_pcie_port *port,
				  unsigned int target, unsigned int attribute,
				  const struct mvebu_pcie_window *desired,
				  struct mvebu_pcie_window *cur)
{
	int ret;

	if (desired->base == cur->base && desired->remap == cur->remap &&
	    desired->size == cur->size)
		return 0;

	if (cur->size != 0) {
		mvebu_pcie_del_windows(port, cur->base, cur->size);
		cur->size = 0;
		cur->base = 0;

		 
	}

	if (desired->size == 0)
		return 0;

	ret = mvebu_pcie_add_windows(port, target, attribute, desired->base,
				     desired->size, desired->remap);
	if (ret) {
		cur->size = 0;
		cur->base = 0;
		return ret;
	}

	*cur = *desired;
	return 0;
}

static int mvebu_pcie_handle_iobase_change(struct mvebu_pcie_port *port)
{
	struct mvebu_pcie_window desired = {};
	struct pci_bridge_emul_conf *conf = &port->bridge.conf;

	 
	if (conf->iolimit < conf->iobase ||
	    le16_to_cpu(conf->iolimitupper) < le16_to_cpu(conf->iobaseupper))
		return mvebu_pcie_set_window(port, port->io_target, port->io_attr,
					     &desired, &port->iowin);

	 
	desired.remap = ((conf->iobase & 0xF0) << 8) |
			(le16_to_cpu(conf->iobaseupper) << 16);
	desired.base = port->pcie->io.start + desired.remap;
	desired.size = ((0xFFF | ((conf->iolimit & 0xF0) << 8) |
			 (le16_to_cpu(conf->iolimitupper) << 16)) -
			desired.remap) +
		       1;

	return mvebu_pcie_set_window(port, port->io_target, port->io_attr, &desired,
				     &port->iowin);
}

static int mvebu_pcie_handle_membase_change(struct mvebu_pcie_port *port)
{
	struct mvebu_pcie_window desired = {.remap = MVEBU_MBUS_NO_REMAP};
	struct pci_bridge_emul_conf *conf = &port->bridge.conf;

	 
	if (le16_to_cpu(conf->memlimit) < le16_to_cpu(conf->membase))
		return mvebu_pcie_set_window(port, port->mem_target, port->mem_attr,
					     &desired, &port->memwin);

	 
	desired.base = ((le16_to_cpu(conf->membase) & 0xFFF0) << 16);
	desired.size = (((le16_to_cpu(conf->memlimit) & 0xFFF0) << 16) | 0xFFFFF) -
		       desired.base + 1;

	return mvebu_pcie_set_window(port, port->mem_target, port->mem_attr, &desired,
				     &port->memwin);
}

static pci_bridge_emul_read_status_t
mvebu_pci_bridge_emul_base_conf_read(struct pci_bridge_emul *bridge,
				     int reg, u32 *value)
{
	struct mvebu_pcie_port *port = bridge->data;

	switch (reg) {
	case PCI_COMMAND:
		*value = mvebu_readl(port, PCIE_CMD_OFF);
		break;

	case PCI_PRIMARY_BUS: {
		 
		__le32 *cfgspace = (__le32 *)&bridge->conf;
		u32 val = le32_to_cpu(cfgspace[PCI_PRIMARY_BUS / 4]);
		val &= ~0xff00;
		val |= mvebu_pcie_get_local_bus_nr(port) << 8;
		*value = val;
		break;
	}

	case PCI_INTERRUPT_LINE: {
		 
		__le32 *cfgspace = (__le32 *)&bridge->conf;
		u32 val = le32_to_cpu(cfgspace[PCI_INTERRUPT_LINE / 4]);
		if (mvebu_readl(port, PCIE_CTRL_OFF) & PCIE_CTRL_MASTER_HOT_RESET)
			val |= PCI_BRIDGE_CTL_BUS_RESET << 16;
		else
			val &= ~(PCI_BRIDGE_CTL_BUS_RESET << 16);
		*value = val;
		break;
	}

	default:
		return PCI_BRIDGE_EMUL_NOT_HANDLED;
	}

	return PCI_BRIDGE_EMUL_HANDLED;
}

static pci_bridge_emul_read_status_t
mvebu_pci_bridge_emul_pcie_conf_read(struct pci_bridge_emul *bridge,
				     int reg, u32 *value)
{
	struct mvebu_pcie_port *port = bridge->data;

	switch (reg) {
	case PCI_EXP_DEVCAP:
		*value = mvebu_readl(port, PCIE_CAP_PCIEXP + PCI_EXP_DEVCAP);
		break;

	case PCI_EXP_DEVCTL:
		*value = mvebu_readl(port, PCIE_CAP_PCIEXP + PCI_EXP_DEVCTL);
		break;

	case PCI_EXP_LNKCAP:
		 
		*value = (mvebu_readl(port, PCIE_CAP_PCIEXP + PCI_EXP_LNKCAP) &
			  ~PCI_EXP_LNKCAP_CLKPM) | PCI_EXP_LNKCAP_DLLLARC;
		break;

	case PCI_EXP_LNKCTL:
		 
		*value = mvebu_readl(port, PCIE_CAP_PCIEXP + PCI_EXP_LNKCTL) |
			 (mvebu_pcie_link_up(port) ?
			  (PCI_EXP_LNKSTA_DLLLA << 16) : 0);
		break;

	case PCI_EXP_SLTCTL: {
		u16 slotctl = le16_to_cpu(bridge->pcie_conf.slotctl);
		u16 slotsta = le16_to_cpu(bridge->pcie_conf.slotsta);
		u32 val = 0;
		 
		if (!port->slot_power_limit_value)
			val |= slotctl & PCI_EXP_SLTCTL_ASPL_DISABLE;
		else if (!(mvebu_readl(port, PCIE_SSPL_OFF) & PCIE_SSPL_ENABLE))
			val |= PCI_EXP_SLTCTL_ASPL_DISABLE;
		 
		val |= slotsta << 16;
		*value = val;
		break;
	}

	case PCI_EXP_RTSTA:
		*value = mvebu_readl(port, PCIE_RC_RTSTA);
		break;

	case PCI_EXP_DEVCAP2:
		*value = mvebu_readl(port, PCIE_CAP_PCIEXP + PCI_EXP_DEVCAP2);
		break;

	case PCI_EXP_DEVCTL2:
		*value = mvebu_readl(port, PCIE_CAP_PCIEXP + PCI_EXP_DEVCTL2);
		break;

	case PCI_EXP_LNKCTL2:
		*value = mvebu_readl(port, PCIE_CAP_PCIEXP + PCI_EXP_LNKCTL2);
		break;

	default:
		return PCI_BRIDGE_EMUL_NOT_HANDLED;
	}

	return PCI_BRIDGE_EMUL_HANDLED;
}

static pci_bridge_emul_read_status_t
mvebu_pci_bridge_emul_ext_conf_read(struct pci_bridge_emul *bridge,
				    int reg, u32 *value)
{
	struct mvebu_pcie_port *port = bridge->data;

	switch (reg) {
	case 0:
	case PCI_ERR_UNCOR_STATUS:
	case PCI_ERR_UNCOR_MASK:
	case PCI_ERR_UNCOR_SEVER:
	case PCI_ERR_COR_STATUS:
	case PCI_ERR_COR_MASK:
	case PCI_ERR_CAP:
	case PCI_ERR_HEADER_LOG+0:
	case PCI_ERR_HEADER_LOG+4:
	case PCI_ERR_HEADER_LOG+8:
	case PCI_ERR_HEADER_LOG+12:
	case PCI_ERR_ROOT_COMMAND:
	case PCI_ERR_ROOT_STATUS:
	case PCI_ERR_ROOT_ERR_SRC:
		*value = mvebu_readl(port, PCIE_CAP_PCIERR_OFF + reg);
		break;

	default:
		return PCI_BRIDGE_EMUL_NOT_HANDLED;
	}

	return PCI_BRIDGE_EMUL_HANDLED;
}

static void
mvebu_pci_bridge_emul_base_conf_write(struct pci_bridge_emul *bridge,
				      int reg, u32 old, u32 new, u32 mask)
{
	struct mvebu_pcie_port *port = bridge->data;
	struct pci_bridge_emul_conf *conf = &bridge->conf;

	switch (reg) {
	case PCI_COMMAND:
		mvebu_writel(port, new, PCIE_CMD_OFF);
		break;

	case PCI_IO_BASE:
		if ((mask & 0xffff) && mvebu_has_ioport(port) &&
		    mvebu_pcie_handle_iobase_change(port)) {
			 
			conf->iobase &= ~0xf0;
			conf->iolimit &= ~0xf0;
			conf->iobase |= 0xf0;
			conf->iobaseupper = cpu_to_le16(0x0000);
			conf->iolimitupper = cpu_to_le16(0x0000);
		}
		break;

	case PCI_MEMORY_BASE:
		if (mvebu_pcie_handle_membase_change(port)) {
			 
			conf->membase = cpu_to_le16(le16_to_cpu(conf->membase) & ~0xfff0);
			conf->memlimit = cpu_to_le16(le16_to_cpu(conf->memlimit) & ~0xfff0);
			conf->membase = cpu_to_le16(le16_to_cpu(conf->membase) | 0xfff0);
		}
		break;

	case PCI_IO_BASE_UPPER16:
		if (mvebu_has_ioport(port) &&
		    mvebu_pcie_handle_iobase_change(port)) {
			 
			conf->iobase &= ~0xf0;
			conf->iolimit &= ~0xf0;
			conf->iobase |= 0xf0;
			conf->iobaseupper = cpu_to_le16(0x0000);
			conf->iolimitupper = cpu_to_le16(0x0000);
		}
		break;

	case PCI_PRIMARY_BUS:
		if (mask & 0xff00)
			mvebu_pcie_set_local_bus_nr(port, conf->secondary_bus);
		break;

	case PCI_INTERRUPT_LINE:
		if (mask & (PCI_BRIDGE_CTL_BUS_RESET << 16)) {
			u32 ctrl = mvebu_readl(port, PCIE_CTRL_OFF);
			if (new & (PCI_BRIDGE_CTL_BUS_RESET << 16))
				ctrl |= PCIE_CTRL_MASTER_HOT_RESET;
			else
				ctrl &= ~PCIE_CTRL_MASTER_HOT_RESET;
			mvebu_writel(port, ctrl, PCIE_CTRL_OFF);
		}
		break;

	default:
		break;
	}
}

static void
mvebu_pci_bridge_emul_pcie_conf_write(struct pci_bridge_emul *bridge,
				      int reg, u32 old, u32 new, u32 mask)
{
	struct mvebu_pcie_port *port = bridge->data;

	switch (reg) {
	case PCI_EXP_DEVCTL:
		mvebu_writel(port, new, PCIE_CAP_PCIEXP + PCI_EXP_DEVCTL);
		break;

	case PCI_EXP_LNKCTL:
		 
		new &= ~PCI_EXP_LNKCTL_CLKREQ_EN;

		mvebu_writel(port, new, PCIE_CAP_PCIEXP + PCI_EXP_LNKCTL);
		break;

	case PCI_EXP_SLTCTL:
		 
		if ((mask & PCI_EXP_SLTCTL_ASPL_DISABLE) &&
		    port->slot_power_limit_value) {
			u32 sspl = mvebu_readl(port, PCIE_SSPL_OFF);
			if (new & PCI_EXP_SLTCTL_ASPL_DISABLE)
				sspl &= ~PCIE_SSPL_ENABLE;
			else
				sspl |= PCIE_SSPL_ENABLE;
			mvebu_writel(port, sspl, PCIE_SSPL_OFF);
		}
		break;

	case PCI_EXP_RTSTA:
		 
		if (new & PCI_EXP_RTSTA_PME)
			mvebu_writel(port, ~PCIE_INT_PM_PME, PCIE_INT_CAUSE_OFF);
		break;

	case PCI_EXP_DEVCTL2:
		mvebu_writel(port, new, PCIE_CAP_PCIEXP + PCI_EXP_DEVCTL2);
		break;

	case PCI_EXP_LNKCTL2:
		mvebu_writel(port, new, PCIE_CAP_PCIEXP + PCI_EXP_LNKCTL2);
		break;

	default:
		break;
	}
}

static void
mvebu_pci_bridge_emul_ext_conf_write(struct pci_bridge_emul *bridge,
				     int reg, u32 old, u32 new, u32 mask)
{
	struct mvebu_pcie_port *port = bridge->data;

	switch (reg) {
	 
	case PCI_ERR_UNCOR_STATUS:
	case PCI_ERR_COR_STATUS:
	case PCI_ERR_ROOT_STATUS:
		new &= mask;
		fallthrough;

	case PCI_ERR_UNCOR_MASK:
	case PCI_ERR_UNCOR_SEVER:
	case PCI_ERR_COR_MASK:
	case PCI_ERR_CAP:
	case PCI_ERR_HEADER_LOG+0:
	case PCI_ERR_HEADER_LOG+4:
	case PCI_ERR_HEADER_LOG+8:
	case PCI_ERR_HEADER_LOG+12:
	case PCI_ERR_ROOT_COMMAND:
	case PCI_ERR_ROOT_ERR_SRC:
		mvebu_writel(port, new, PCIE_CAP_PCIERR_OFF + reg);
		break;

	default:
		break;
	}
}

static const struct pci_bridge_emul_ops mvebu_pci_bridge_emul_ops = {
	.read_base = mvebu_pci_bridge_emul_base_conf_read,
	.write_base = mvebu_pci_bridge_emul_base_conf_write,
	.read_pcie = mvebu_pci_bridge_emul_pcie_conf_read,
	.write_pcie = mvebu_pci_bridge_emul_pcie_conf_write,
	.read_ext = mvebu_pci_bridge_emul_ext_conf_read,
	.write_ext = mvebu_pci_bridge_emul_ext_conf_write,
};

 
static int mvebu_pci_bridge_emul_init(struct mvebu_pcie_port *port)
{
	unsigned int bridge_flags = PCI_BRIDGE_EMUL_NO_PREFMEM_FORWARD;
	struct pci_bridge_emul *bridge = &port->bridge;
	u32 dev_id = mvebu_readl(port, PCIE_DEV_ID_OFF);
	u32 dev_rev = mvebu_readl(port, PCIE_DEV_REV_OFF);
	u32 ssdev_id = mvebu_readl(port, PCIE_SSDEV_ID_OFF);
	u32 pcie_cap = mvebu_readl(port, PCIE_CAP_PCIEXP);
	u8 pcie_cap_ver = ((pcie_cap >> 16) & PCI_EXP_FLAGS_VERS);

	bridge->conf.vendor = cpu_to_le16(dev_id & 0xffff);
	bridge->conf.device = cpu_to_le16(dev_id >> 16);
	bridge->conf.class_revision = cpu_to_le32(dev_rev & 0xff);

	if (mvebu_has_ioport(port)) {
		 
		bridge->conf.iobase = PCI_IO_RANGE_TYPE_32;
		bridge->conf.iolimit = PCI_IO_RANGE_TYPE_32;
	} else {
		bridge_flags |= PCI_BRIDGE_EMUL_NO_IO_FORWARD;
	}

	 
	bridge->pcie_conf.cap = cpu_to_le16(pcie_cap_ver | PCI_EXP_FLAGS_SLOT);

	 
	bridge->pcie_conf.slotcap = cpu_to_le32(
		FIELD_PREP(PCI_EXP_SLTCAP_SPLV, port->slot_power_limit_value) |
		FIELD_PREP(PCI_EXP_SLTCAP_SPLS, port->slot_power_limit_scale) |
		FIELD_PREP(PCI_EXP_SLTCAP_PSN, port->port+1));
	bridge->pcie_conf.slotsta = cpu_to_le16(PCI_EXP_SLTSTA_PDS);

	bridge->subsystem_vendor_id = ssdev_id & 0xffff;
	bridge->subsystem_id = ssdev_id >> 16;
	bridge->has_pcie = true;
	bridge->pcie_start = PCIE_CAP_PCIEXP;
	bridge->data = port;
	bridge->ops = &mvebu_pci_bridge_emul_ops;

	return pci_bridge_emul_init(bridge, bridge_flags);
}

static inline struct mvebu_pcie *sys_to_pcie(struct pci_sys_data *sys)
{
	return sys->private_data;
}

static struct mvebu_pcie_port *mvebu_pcie_find_port(struct mvebu_pcie *pcie,
						    struct pci_bus *bus,
						    int devfn)
{
	int i;

	for (i = 0; i < pcie->nports; i++) {
		struct mvebu_pcie_port *port = &pcie->ports[i];

		if (!port->base)
			continue;

		if (bus->number == 0 && port->devfn == devfn)
			return port;
		if (bus->number != 0 &&
		    bus->number >= port->bridge.conf.secondary_bus &&
		    bus->number <= port->bridge.conf.subordinate_bus)
			return port;
	}

	return NULL;
}

 
static int mvebu_pcie_wr_conf(struct pci_bus *bus, u32 devfn,
			      int where, int size, u32 val)
{
	struct mvebu_pcie *pcie = bus->sysdata;
	struct mvebu_pcie_port *port;

	port = mvebu_pcie_find_port(pcie, bus, devfn);
	if (!port)
		return PCIBIOS_DEVICE_NOT_FOUND;

	return pci_bridge_emul_conf_write(&port->bridge, where, size, val);
}

 
static int mvebu_pcie_rd_conf(struct pci_bus *bus, u32 devfn, int where,
			      int size, u32 *val)
{
	struct mvebu_pcie *pcie = bus->sysdata;
	struct mvebu_pcie_port *port;

	port = mvebu_pcie_find_port(pcie, bus, devfn);
	if (!port)
		return PCIBIOS_DEVICE_NOT_FOUND;

	return pci_bridge_emul_conf_read(&port->bridge, where, size, val);
}

static struct pci_ops mvebu_pcie_ops = {
	.read = mvebu_pcie_rd_conf,
	.write = mvebu_pcie_wr_conf,
};

static void mvebu_pcie_intx_irq_mask(struct irq_data *d)
{
	struct mvebu_pcie_port *port = d->domain->host_data;
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	unsigned long flags;
	u32 unmask;

	raw_spin_lock_irqsave(&port->irq_lock, flags);
	unmask = mvebu_readl(port, PCIE_INT_UNMASK_OFF);
	unmask &= ~PCIE_INT_INTX(hwirq);
	mvebu_writel(port, unmask, PCIE_INT_UNMASK_OFF);
	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

static void mvebu_pcie_intx_irq_unmask(struct irq_data *d)
{
	struct mvebu_pcie_port *port = d->domain->host_data;
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	unsigned long flags;
	u32 unmask;

	raw_spin_lock_irqsave(&port->irq_lock, flags);
	unmask = mvebu_readl(port, PCIE_INT_UNMASK_OFF);
	unmask |= PCIE_INT_INTX(hwirq);
	mvebu_writel(port, unmask, PCIE_INT_UNMASK_OFF);
	raw_spin_unlock_irqrestore(&port->irq_lock, flags);
}

static struct irq_chip intx_irq_chip = {
	.name = "mvebu-INTx",
	.irq_mask = mvebu_pcie_intx_irq_mask,
	.irq_unmask = mvebu_pcie_intx_irq_unmask,
};

static int mvebu_pcie_intx_irq_map(struct irq_domain *h,
				   unsigned int virq, irq_hw_number_t hwirq)
{
	struct mvebu_pcie_port *port = h->host_data;

	irq_set_status_flags(virq, IRQ_LEVEL);
	irq_set_chip_and_handler(virq, &intx_irq_chip, handle_level_irq);
	irq_set_chip_data(virq, port);

	return 0;
}

static const struct irq_domain_ops mvebu_pcie_intx_irq_domain_ops = {
	.map = mvebu_pcie_intx_irq_map,
	.xlate = irq_domain_xlate_onecell,
};

static int mvebu_pcie_init_irq_domain(struct mvebu_pcie_port *port)
{
	struct device *dev = &port->pcie->pdev->dev;
	struct device_node *pcie_intc_node;

	raw_spin_lock_init(&port->irq_lock);

	pcie_intc_node = of_get_next_child(port->dn, NULL);
	if (!pcie_intc_node) {
		dev_err(dev, "No PCIe Intc node found for %s\n", port->name);
		return -ENODEV;
	}

	port->intx_irq_domain = irq_domain_add_linear(pcie_intc_node, PCI_NUM_INTX,
						      &mvebu_pcie_intx_irq_domain_ops,
						      port);
	of_node_put(pcie_intc_node);
	if (!port->intx_irq_domain) {
		dev_err(dev, "Failed to get INTx IRQ domain for %s\n", port->name);
		return -ENOMEM;
	}

	return 0;
}

static void mvebu_pcie_irq_handler(struct irq_desc *desc)
{
	struct mvebu_pcie_port *port = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct device *dev = &port->pcie->pdev->dev;
	u32 cause, unmask, status;
	int i;

	chained_irq_enter(chip, desc);

	cause = mvebu_readl(port, PCIE_INT_CAUSE_OFF);
	unmask = mvebu_readl(port, PCIE_INT_UNMASK_OFF);
	status = cause & unmask;

	 
	for (i = 0; i < PCI_NUM_INTX; i++) {
		if (!(status & PCIE_INT_INTX(i)))
			continue;

		if (generic_handle_domain_irq(port->intx_irq_domain, i) == -EINVAL)
			dev_err_ratelimited(dev, "unexpected INT%c IRQ\n", (char)i+'A');
	}

	chained_irq_exit(chip, desc);
}

static int mvebu_pcie_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	 
	if (dev->bus->number == 0)
		return 0;  

	return of_irq_parse_and_map_pci(dev, slot, pin);
}

static resource_size_t mvebu_pcie_align_resource(struct pci_dev *dev,
						 const struct resource *res,
						 resource_size_t start,
						 resource_size_t size,
						 resource_size_t align)
{
	if (dev->bus->number != 0)
		return start;

	 
	if (res->flags & IORESOURCE_IO)
		return round_up(start, max_t(resource_size_t, SZ_64K,
					     rounddown_pow_of_two(size)));
	else if (res->flags & IORESOURCE_MEM)
		return round_up(start, max_t(resource_size_t, SZ_1M,
					     rounddown_pow_of_two(size)));
	else
		return start;
}

static void __iomem *mvebu_pcie_map_registers(struct platform_device *pdev,
					      struct device_node *np,
					      struct mvebu_pcie_port *port)
{
	int ret = 0;

	ret = of_address_to_resource(np, 0, &port->regs);
	if (ret)
		return (void __iomem *)ERR_PTR(ret);

	return devm_ioremap_resource(&pdev->dev, &port->regs);
}

#define DT_FLAGS_TO_TYPE(flags)       (((flags) >> 24) & 0x03)
#define    DT_TYPE_IO                 0x1
#define    DT_TYPE_MEM32              0x2
#define DT_CPUADDR_TO_TARGET(cpuaddr) (((cpuaddr) >> 56) & 0xFF)
#define DT_CPUADDR_TO_ATTR(cpuaddr)   (((cpuaddr) >> 48) & 0xFF)

static int mvebu_get_tgt_attr(struct device_node *np, int devfn,
			      unsigned long type,
			      unsigned int *tgt,
			      unsigned int *attr)
{
	const int na = 3, ns = 2;
	const __be32 *range;
	int rlen, nranges, rangesz, pna, i;

	*tgt = -1;
	*attr = -1;

	range = of_get_property(np, "ranges", &rlen);
	if (!range)
		return -EINVAL;

	pna = of_n_addr_cells(np);
	rangesz = pna + na + ns;
	nranges = rlen / sizeof(__be32) / rangesz;

	for (i = 0; i < nranges; i++, range += rangesz) {
		u32 flags = of_read_number(range, 1);
		u32 slot = of_read_number(range + 1, 1);
		u64 cpuaddr = of_read_number(range + na, pna);
		unsigned long rtype;

		if (DT_FLAGS_TO_TYPE(flags) == DT_TYPE_IO)
			rtype = IORESOURCE_IO;
		else if (DT_FLAGS_TO_TYPE(flags) == DT_TYPE_MEM32)
			rtype = IORESOURCE_MEM;
		else
			continue;

		if (slot == PCI_SLOT(devfn) && type == rtype) {
			*tgt = DT_CPUADDR_TO_TARGET(cpuaddr);
			*attr = DT_CPUADDR_TO_ATTR(cpuaddr);
			return 0;
		}
	}

	return -ENOENT;
}

static int mvebu_pcie_suspend(struct device *dev)
{
	struct mvebu_pcie *pcie;
	int i;

	pcie = dev_get_drvdata(dev);
	for (i = 0; i < pcie->nports; i++) {
		struct mvebu_pcie_port *port = pcie->ports + i;
		if (!port->base)
			continue;
		port->saved_pcie_stat = mvebu_readl(port, PCIE_STAT_OFF);
	}

	return 0;
}

static int mvebu_pcie_resume(struct device *dev)
{
	struct mvebu_pcie *pcie;
	int i;

	pcie = dev_get_drvdata(dev);
	for (i = 0; i < pcie->nports; i++) {
		struct mvebu_pcie_port *port = pcie->ports + i;
		if (!port->base)
			continue;
		mvebu_writel(port, port->saved_pcie_stat, PCIE_STAT_OFF);
		mvebu_pcie_setup_hw(port);
	}

	return 0;
}

static void mvebu_pcie_port_clk_put(void *data)
{
	struct mvebu_pcie_port *port = data;

	clk_put(port->clk);
}

static int mvebu_pcie_parse_port(struct mvebu_pcie *pcie,
	struct mvebu_pcie_port *port, struct device_node *child)
{
	struct device *dev = &pcie->pdev->dev;
	u32 slot_power_limit;
	int ret;
	u32 num_lanes;

	port->pcie = pcie;

	if (of_property_read_u32(child, "marvell,pcie-port", &port->port)) {
		dev_warn(dev, "ignoring %pOF, missing pcie-port property\n",
			 child);
		goto skip;
	}

	if (of_property_read_u32(child, "marvell,pcie-lane", &port->lane))
		port->lane = 0;

	if (!of_property_read_u32(child, "num-lanes", &num_lanes) && num_lanes == 4)
		port->is_x4 = true;

	port->name = devm_kasprintf(dev, GFP_KERNEL, "pcie%d.%d", port->port,
				    port->lane);
	if (!port->name) {
		ret = -ENOMEM;
		goto err;
	}

	port->devfn = of_pci_get_devfn(child);
	if (port->devfn < 0)
		goto skip;
	if (PCI_FUNC(port->devfn) != 0) {
		dev_err(dev, "%s: invalid function number, must be zero\n",
			port->name);
		goto skip;
	}

	ret = mvebu_get_tgt_attr(dev->of_node, port->devfn, IORESOURCE_MEM,
				 &port->mem_target, &port->mem_attr);
	if (ret < 0) {
		dev_err(dev, "%s: cannot get tgt/attr for mem window\n",
			port->name);
		goto skip;
	}

	if (resource_size(&pcie->io) != 0) {
		mvebu_get_tgt_attr(dev->of_node, port->devfn, IORESOURCE_IO,
				   &port->io_target, &port->io_attr);
	} else {
		port->io_target = -1;
		port->io_attr = -1;
	}

	 
	port->intx_irq = of_irq_get_byname(child, "intx");
	if (port->intx_irq == -EPROBE_DEFER) {
		ret = port->intx_irq;
		goto err;
	}
	if (port->intx_irq <= 0) {
		dev_warn(dev, "%s: legacy INTx interrupts cannot be masked individually, "
			      "%pOF does not contain intx interrupt\n",
			 port->name, child);
	}

	port->reset_name = devm_kasprintf(dev, GFP_KERNEL, "%s-reset",
					  port->name);
	if (!port->reset_name) {
		ret = -ENOMEM;
		goto err;
	}

	port->reset_gpio = devm_fwnode_gpiod_get(dev, of_fwnode_handle(child),
						 "reset", GPIOD_OUT_HIGH,
						 port->name);
	ret = PTR_ERR_OR_ZERO(port->reset_gpio);
	if (ret) {
		if (ret != -ENOENT)
			goto err;
		 
		port->reset_gpio = NULL;
		devm_kfree(dev, port->reset_name);
		port->reset_name = NULL;
	}

	slot_power_limit = of_pci_get_slot_power_limit(child,
				&port->slot_power_limit_value,
				&port->slot_power_limit_scale);
	if (slot_power_limit)
		dev_info(dev, "%s: Slot power limit %u.%uW\n",
			 port->name,
			 slot_power_limit / 1000,
			 (slot_power_limit / 100) % 10);

	port->clk = of_clk_get_by_name(child, NULL);
	if (IS_ERR(port->clk)) {
		dev_err(dev, "%s: cannot get clock\n", port->name);
		goto skip;
	}

	ret = devm_add_action(dev, mvebu_pcie_port_clk_put, port);
	if (ret < 0) {
		clk_put(port->clk);
		goto err;
	}

	return 1;

skip:
	ret = 0;

	 
	devm_kfree(dev, port->reset_name);
	port->reset_name = NULL;
	devm_kfree(dev, port->name);
	port->name = NULL;

err:
	return ret;
}

 
static int mvebu_pcie_powerup(struct mvebu_pcie_port *port)
{
	int ret;

	ret = clk_prepare_enable(port->clk);
	if (ret < 0)
		return ret;

	if (port->reset_gpio) {
		u32 reset_udelay = PCI_PM_D3COLD_WAIT * 1000;

		of_property_read_u32(port->dn, "reset-delay-us",
				     &reset_udelay);

		udelay(100);

		gpiod_set_value_cansleep(port->reset_gpio, 0);
		msleep(reset_udelay / 1000);
	}

	return 0;
}

 
static void mvebu_pcie_powerdown(struct mvebu_pcie_port *port)
{
	gpiod_set_value_cansleep(port->reset_gpio, 1);

	clk_disable_unprepare(port->clk);
}

 
static int mvebu_pcie_parse_request_resources(struct mvebu_pcie *pcie)
{
	struct device *dev = &pcie->pdev->dev;
	struct pci_host_bridge *bridge = pci_host_bridge_from_priv(pcie);
	int ret;

	 
	mvebu_mbus_get_pcie_mem_aperture(&pcie->mem);
	if (resource_size(&pcie->mem) == 0) {
		dev_err(dev, "invalid memory aperture size\n");
		return -EINVAL;
	}

	pcie->mem.name = "PCI MEM";
	pci_add_resource(&bridge->windows, &pcie->mem);
	ret = devm_request_resource(dev, &iomem_resource, &pcie->mem);
	if (ret)
		return ret;

	 
	mvebu_mbus_get_pcie_io_aperture(&pcie->io);

	if (resource_size(&pcie->io) != 0) {
		pcie->realio.flags = pcie->io.flags;
		pcie->realio.start = PCIBIOS_MIN_IO;
		pcie->realio.end = min_t(resource_size_t,
					 IO_SPACE_LIMIT - SZ_64K,
					 resource_size(&pcie->io) - 1);
		pcie->realio.name = "PCI I/O";

		ret = devm_pci_remap_iospace(dev, &pcie->realio, pcie->io.start);
		if (ret)
			return ret;

		pci_add_resource(&bridge->windows, &pcie->realio);
		ret = devm_request_resource(dev, &ioport_resource, &pcie->realio);
		if (ret)
			return ret;
	}

	return 0;
}

static int mvebu_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mvebu_pcie *pcie;
	struct pci_host_bridge *bridge;
	struct device_node *np = dev->of_node;
	struct device_node *child;
	int num, i, ret;

	bridge = devm_pci_alloc_host_bridge(dev, sizeof(struct mvebu_pcie));
	if (!bridge)
		return -ENOMEM;

	pcie = pci_host_bridge_priv(bridge);
	pcie->pdev = pdev;
	platform_set_drvdata(pdev, pcie);

	ret = mvebu_pcie_parse_request_resources(pcie);
	if (ret)
		return ret;

	num = of_get_available_child_count(np);

	pcie->ports = devm_kcalloc(dev, num, sizeof(*pcie->ports), GFP_KERNEL);
	if (!pcie->ports)
		return -ENOMEM;

	i = 0;
	for_each_available_child_of_node(np, child) {
		struct mvebu_pcie_port *port = &pcie->ports[i];

		ret = mvebu_pcie_parse_port(pcie, port, child);
		if (ret < 0) {
			of_node_put(child);
			return ret;
		} else if (ret == 0) {
			continue;
		}

		port->dn = child;
		i++;
	}
	pcie->nports = i;

	for (i = 0; i < pcie->nports; i++) {
		struct mvebu_pcie_port *port = &pcie->ports[i];
		int irq = port->intx_irq;

		child = port->dn;
		if (!child)
			continue;

		ret = mvebu_pcie_powerup(port);
		if (ret < 0)
			continue;

		port->base = mvebu_pcie_map_registers(pdev, child, port);
		if (IS_ERR(port->base)) {
			dev_err(dev, "%s: cannot map registers\n", port->name);
			port->base = NULL;
			mvebu_pcie_powerdown(port);
			continue;
		}

		ret = mvebu_pci_bridge_emul_init(port);
		if (ret < 0) {
			dev_err(dev, "%s: cannot init emulated bridge\n",
				port->name);
			devm_iounmap(dev, port->base);
			port->base = NULL;
			mvebu_pcie_powerdown(port);
			continue;
		}

		if (irq > 0) {
			ret = mvebu_pcie_init_irq_domain(port);
			if (ret) {
				dev_err(dev, "%s: cannot init irq domain\n",
					port->name);
				pci_bridge_emul_cleanup(&port->bridge);
				devm_iounmap(dev, port->base);
				port->base = NULL;
				mvebu_pcie_powerdown(port);
				continue;
			}
			irq_set_chained_handler_and_data(irq,
							 mvebu_pcie_irq_handler,
							 port);
		}

		 
		mvebu_pcie_setup_hw(port);
		mvebu_pcie_set_local_dev_nr(port, 1);
		mvebu_pcie_set_local_bus_nr(port, 0);
	}

	bridge->sysdata = pcie;
	bridge->ops = &mvebu_pcie_ops;
	bridge->child_ops = &mvebu_pcie_child_ops;
	bridge->align_resource = mvebu_pcie_align_resource;
	bridge->map_irq = mvebu_pcie_map_irq;

	return pci_host_probe(bridge);
}

static void mvebu_pcie_remove(struct platform_device *pdev)
{
	struct mvebu_pcie *pcie = platform_get_drvdata(pdev);
	struct pci_host_bridge *bridge = pci_host_bridge_from_priv(pcie);
	u32 cmd, sspl;
	int i;

	 
	pci_lock_rescan_remove();
	pci_stop_root_bus(bridge->bus);
	pci_remove_root_bus(bridge->bus);
	pci_unlock_rescan_remove();

	for (i = 0; i < pcie->nports; i++) {
		struct mvebu_pcie_port *port = &pcie->ports[i];
		int irq = port->intx_irq;

		if (!port->base)
			continue;

		 
		cmd = mvebu_readl(port, PCIE_CMD_OFF);
		cmd &= ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
		mvebu_writel(port, cmd, PCIE_CMD_OFF);

		 
		mvebu_writel(port, ~PCIE_INT_ALL_MASK, PCIE_INT_UNMASK_OFF);

		 
		mvebu_writel(port, ~PCIE_INT_ALL_MASK, PCIE_INT_CAUSE_OFF);

		if (irq > 0)
			irq_set_chained_handler_and_data(irq, NULL, NULL);

		 
		if (port->intx_irq_domain)
			irq_domain_remove(port->intx_irq_domain);

		 
		pci_bridge_emul_cleanup(&port->bridge);

		 
		sspl = mvebu_readl(port, PCIE_SSPL_OFF);
		sspl &= ~(PCIE_SSPL_VALUE_MASK | PCIE_SSPL_SCALE_MASK | PCIE_SSPL_ENABLE);
		mvebu_writel(port, sspl, PCIE_SSPL_OFF);

		 
		mvebu_pcie_disable_wins(port);

		 
		if (port->iowin.size)
			mvebu_pcie_del_windows(port, port->iowin.base, port->iowin.size);
		if (port->memwin.size)
			mvebu_pcie_del_windows(port, port->memwin.base, port->memwin.size);

		 
		mvebu_pcie_powerdown(port);
	}
}

static const struct of_device_id mvebu_pcie_of_match_table[] = {
	{ .compatible = "marvell,armada-xp-pcie", },
	{ .compatible = "marvell,armada-370-pcie", },
	{ .compatible = "marvell,dove-pcie", },
	{ .compatible = "marvell,kirkwood-pcie", },
	{},
};

static const struct dev_pm_ops mvebu_pcie_pm_ops = {
	NOIRQ_SYSTEM_SLEEP_PM_OPS(mvebu_pcie_suspend, mvebu_pcie_resume)
};

static struct platform_driver mvebu_pcie_driver = {
	.driver = {
		.name = "mvebu-pcie",
		.of_match_table = mvebu_pcie_of_match_table,
		.pm = &mvebu_pcie_pm_ops,
	},
	.probe = mvebu_pcie_probe,
	.remove_new = mvebu_pcie_remove,
};
module_platform_driver(mvebu_pcie_driver);

MODULE_AUTHOR("Thomas Petazzoni <thomas.petazzoni@bootlin.com>");
MODULE_AUTHOR("Pali Rohár <pali@kernel.org>");
MODULE_DESCRIPTION("Marvell EBU PCIe controller");
MODULE_LICENSE("GPL v2");
