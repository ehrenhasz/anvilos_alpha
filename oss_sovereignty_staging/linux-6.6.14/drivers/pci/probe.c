
 

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/msi.h>
#include <linux/of_pci.h>
#include <linux/pci_hotplug.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/cpumask.h>
#include <linux/aer.h>
#include <linux/acpi.h>
#include <linux/hypervisor.h>
#include <linux/irqdomain.h>
#include <linux/pm_runtime.h>
#include <linux/bitfield.h>
#include "pci.h"

#define CARDBUS_LATENCY_TIMER	176	 
#define CARDBUS_RESERVE_BUSNR	3

static struct resource busn_resource = {
	.name	= "PCI busn",
	.start	= 0,
	.end	= 255,
	.flags	= IORESOURCE_BUS,
};

 
LIST_HEAD(pci_root_buses);
EXPORT_SYMBOL(pci_root_buses);

static LIST_HEAD(pci_domain_busn_res_list);

struct pci_domain_busn_res {
	struct list_head list;
	struct resource res;
	int domain_nr;
};

static struct resource *get_pci_domain_busn_res(int domain_nr)
{
	struct pci_domain_busn_res *r;

	list_for_each_entry(r, &pci_domain_busn_res_list, list)
		if (r->domain_nr == domain_nr)
			return &r->res;

	r = kzalloc(sizeof(*r), GFP_KERNEL);
	if (!r)
		return NULL;

	r->domain_nr = domain_nr;
	r->res.start = 0;
	r->res.end = 0xff;
	r->res.flags = IORESOURCE_BUS | IORESOURCE_PCI_FIXED;

	list_add_tail(&r->list, &pci_domain_busn_res_list);

	return &r->res;
}

 
int no_pci_devices(void)
{
	struct device *dev;
	int no_devices;

	dev = bus_find_next_device(&pci_bus_type, NULL);
	no_devices = (dev == NULL);
	put_device(dev);
	return no_devices;
}
EXPORT_SYMBOL(no_pci_devices);

 
static void release_pcibus_dev(struct device *dev)
{
	struct pci_bus *pci_bus = to_pci_bus(dev);

	put_device(pci_bus->bridge);
	pci_bus_remove_resources(pci_bus);
	pci_release_bus_of_node(pci_bus);
	kfree(pci_bus);
}

static struct class pcibus_class = {
	.name		= "pci_bus",
	.dev_release	= &release_pcibus_dev,
	.dev_groups	= pcibus_groups,
};

static int __init pcibus_class_init(void)
{
	return class_register(&pcibus_class);
}
postcore_initcall(pcibus_class_init);

static u64 pci_size(u64 base, u64 maxbase, u64 mask)
{
	u64 size = mask & maxbase;	 
	if (!size)
		return 0;

	 
	size = size & ~(size-1);

	 
	if (base == maxbase && ((base | (size - 1)) & mask) != mask)
		return 0;

	return size;
}

static inline unsigned long decode_bar(struct pci_dev *dev, u32 bar)
{
	u32 mem_type;
	unsigned long flags;

	if ((bar & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_IO) {
		flags = bar & ~PCI_BASE_ADDRESS_IO_MASK;
		flags |= IORESOURCE_IO;
		return flags;
	}

	flags = bar & ~PCI_BASE_ADDRESS_MEM_MASK;
	flags |= IORESOURCE_MEM;
	if (flags & PCI_BASE_ADDRESS_MEM_PREFETCH)
		flags |= IORESOURCE_PREFETCH;

	mem_type = bar & PCI_BASE_ADDRESS_MEM_TYPE_MASK;
	switch (mem_type) {
	case PCI_BASE_ADDRESS_MEM_TYPE_32:
		break;
	case PCI_BASE_ADDRESS_MEM_TYPE_1M:
		 
		break;
	case PCI_BASE_ADDRESS_MEM_TYPE_64:
		flags |= IORESOURCE_MEM_64;
		break;
	default:
		 
		break;
	}
	return flags;
}

#define PCI_COMMAND_DECODE_ENABLE	(PCI_COMMAND_MEMORY | PCI_COMMAND_IO)

 
int __pci_read_base(struct pci_dev *dev, enum pci_bar_type type,
		    struct resource *res, unsigned int pos)
{
	u32 l = 0, sz = 0, mask;
	u64 l64, sz64, mask64;
	u16 orig_cmd;
	struct pci_bus_region region, inverted_region;

	mask = type ? PCI_ROM_ADDRESS_MASK : ~0;

	 
	if (!dev->mmio_always_on) {
		pci_read_config_word(dev, PCI_COMMAND, &orig_cmd);
		if (orig_cmd & PCI_COMMAND_DECODE_ENABLE) {
			pci_write_config_word(dev, PCI_COMMAND,
				orig_cmd & ~PCI_COMMAND_DECODE_ENABLE);
		}
	}

	res->name = pci_name(dev);

	pci_read_config_dword(dev, pos, &l);
	pci_write_config_dword(dev, pos, l | mask);
	pci_read_config_dword(dev, pos, &sz);
	pci_write_config_dword(dev, pos, l);

	 
	if (PCI_POSSIBLE_ERROR(sz))
		sz = 0;

	 
	if (PCI_POSSIBLE_ERROR(l))
		l = 0;

	if (type == pci_bar_unknown) {
		res->flags = decode_bar(dev, l);
		res->flags |= IORESOURCE_SIZEALIGN;
		if (res->flags & IORESOURCE_IO) {
			l64 = l & PCI_BASE_ADDRESS_IO_MASK;
			sz64 = sz & PCI_BASE_ADDRESS_IO_MASK;
			mask64 = PCI_BASE_ADDRESS_IO_MASK & (u32)IO_SPACE_LIMIT;
		} else {
			l64 = l & PCI_BASE_ADDRESS_MEM_MASK;
			sz64 = sz & PCI_BASE_ADDRESS_MEM_MASK;
			mask64 = (u32)PCI_BASE_ADDRESS_MEM_MASK;
		}
	} else {
		if (l & PCI_ROM_ADDRESS_ENABLE)
			res->flags |= IORESOURCE_ROM_ENABLE;
		l64 = l & PCI_ROM_ADDRESS_MASK;
		sz64 = sz & PCI_ROM_ADDRESS_MASK;
		mask64 = PCI_ROM_ADDRESS_MASK;
	}

	if (res->flags & IORESOURCE_MEM_64) {
		pci_read_config_dword(dev, pos + 4, &l);
		pci_write_config_dword(dev, pos + 4, ~0);
		pci_read_config_dword(dev, pos + 4, &sz);
		pci_write_config_dword(dev, pos + 4, l);

		l64 |= ((u64)l << 32);
		sz64 |= ((u64)sz << 32);
		mask64 |= ((u64)~0 << 32);
	}

	if (!dev->mmio_always_on && (orig_cmd & PCI_COMMAND_DECODE_ENABLE))
		pci_write_config_word(dev, PCI_COMMAND, orig_cmd);

	if (!sz64)
		goto fail;

	sz64 = pci_size(l64, sz64, mask64);
	if (!sz64) {
		pci_info(dev, FW_BUG "reg 0x%x: invalid BAR (can't size)\n",
			 pos);
		goto fail;
	}

	if (res->flags & IORESOURCE_MEM_64) {
		if ((sizeof(pci_bus_addr_t) < 8 || sizeof(resource_size_t) < 8)
		    && sz64 > 0x100000000ULL) {
			res->flags |= IORESOURCE_UNSET | IORESOURCE_DISABLED;
			res->start = 0;
			res->end = 0;
			pci_err(dev, "reg 0x%x: can't handle BAR larger than 4GB (size %#010llx)\n",
				pos, (unsigned long long)sz64);
			goto out;
		}

		if ((sizeof(pci_bus_addr_t) < 8) && l) {
			 
			res->flags |= IORESOURCE_UNSET;
			res->start = 0;
			res->end = sz64 - 1;
			pci_info(dev, "reg 0x%x: can't handle BAR above 4GB (bus address %#010llx)\n",
				 pos, (unsigned long long)l64);
			goto out;
		}
	}

	region.start = l64;
	region.end = l64 + sz64 - 1;

	pcibios_bus_to_resource(dev->bus, res, &region);
	pcibios_resource_to_bus(dev->bus, &inverted_region, res);

	 
	if (inverted_region.start != region.start) {
		res->flags |= IORESOURCE_UNSET;
		res->start = 0;
		res->end = region.end - region.start;
		pci_info(dev, "reg 0x%x: initial BAR value %#010llx invalid\n",
			 pos, (unsigned long long)region.start);
	}

	goto out;


fail:
	res->flags = 0;
out:
	if (res->flags)
		pci_info(dev, "reg 0x%x: %pR\n", pos, res);

	return (res->flags & IORESOURCE_MEM_64) ? 1 : 0;
}

static void pci_read_bases(struct pci_dev *dev, unsigned int howmany, int rom)
{
	unsigned int pos, reg;

	if (dev->non_compliant_bars)
		return;

	 
	if (dev->is_virtfn)
		return;

	for (pos = 0; pos < howmany; pos++) {
		struct resource *res = &dev->resource[pos];
		reg = PCI_BASE_ADDRESS_0 + (pos << 2);
		pos += __pci_read_base(dev, pci_bar_unknown, res, reg);
	}

	if (rom) {
		struct resource *res = &dev->resource[PCI_ROM_RESOURCE];
		dev->rom_base_reg = rom;
		res->flags = IORESOURCE_MEM | IORESOURCE_PREFETCH |
				IORESOURCE_READONLY | IORESOURCE_SIZEALIGN;
		__pci_read_base(dev, pci_bar_mem32, res, rom);
	}
}

static void pci_read_bridge_windows(struct pci_dev *bridge)
{
	u16 io;
	u32 pmem, tmp;

	pci_read_config_word(bridge, PCI_IO_BASE, &io);
	if (!io) {
		pci_write_config_word(bridge, PCI_IO_BASE, 0xe0f0);
		pci_read_config_word(bridge, PCI_IO_BASE, &io);
		pci_write_config_word(bridge, PCI_IO_BASE, 0x0);
	}
	if (io)
		bridge->io_window = 1;

	 
	if (bridge->vendor == PCI_VENDOR_ID_DEC && bridge->device == 0x0001)
		return;

	pci_read_config_dword(bridge, PCI_PREF_MEMORY_BASE, &pmem);
	if (!pmem) {
		pci_write_config_dword(bridge, PCI_PREF_MEMORY_BASE,
					       0xffe0fff0);
		pci_read_config_dword(bridge, PCI_PREF_MEMORY_BASE, &pmem);
		pci_write_config_dword(bridge, PCI_PREF_MEMORY_BASE, 0x0);
	}
	if (!pmem)
		return;

	bridge->pref_window = 1;

	if ((pmem & PCI_PREF_RANGE_TYPE_MASK) == PCI_PREF_RANGE_TYPE_64) {

		 
		pci_read_config_dword(bridge, PCI_PREF_BASE_UPPER32, &pmem);
		pci_write_config_dword(bridge, PCI_PREF_BASE_UPPER32,
				       0xffffffff);
		pci_read_config_dword(bridge, PCI_PREF_BASE_UPPER32, &tmp);
		pci_write_config_dword(bridge, PCI_PREF_BASE_UPPER32, pmem);
		if (tmp)
			bridge->pref_64_window = 1;
	}
}

static void pci_read_bridge_io(struct pci_bus *child)
{
	struct pci_dev *dev = child->self;
	u8 io_base_lo, io_limit_lo;
	unsigned long io_mask, io_granularity, base, limit;
	struct pci_bus_region region;
	struct resource *res;

	io_mask = PCI_IO_RANGE_MASK;
	io_granularity = 0x1000;
	if (dev->io_window_1k) {
		 
		io_mask = PCI_IO_1K_RANGE_MASK;
		io_granularity = 0x400;
	}

	res = child->resource[0];
	pci_read_config_byte(dev, PCI_IO_BASE, &io_base_lo);
	pci_read_config_byte(dev, PCI_IO_LIMIT, &io_limit_lo);
	base = (io_base_lo & io_mask) << 8;
	limit = (io_limit_lo & io_mask) << 8;

	if ((io_base_lo & PCI_IO_RANGE_TYPE_MASK) == PCI_IO_RANGE_TYPE_32) {
		u16 io_base_hi, io_limit_hi;

		pci_read_config_word(dev, PCI_IO_BASE_UPPER16, &io_base_hi);
		pci_read_config_word(dev, PCI_IO_LIMIT_UPPER16, &io_limit_hi);
		base |= ((unsigned long) io_base_hi << 16);
		limit |= ((unsigned long) io_limit_hi << 16);
	}

	if (base <= limit) {
		res->flags = (io_base_lo & PCI_IO_RANGE_TYPE_MASK) | IORESOURCE_IO;
		region.start = base;
		region.end = limit + io_granularity - 1;
		pcibios_bus_to_resource(dev->bus, res, &region);
		pci_info(dev, "  bridge window %pR\n", res);
	}
}

static void pci_read_bridge_mmio(struct pci_bus *child)
{
	struct pci_dev *dev = child->self;
	u16 mem_base_lo, mem_limit_lo;
	unsigned long base, limit;
	struct pci_bus_region region;
	struct resource *res;

	res = child->resource[1];
	pci_read_config_word(dev, PCI_MEMORY_BASE, &mem_base_lo);
	pci_read_config_word(dev, PCI_MEMORY_LIMIT, &mem_limit_lo);
	base = ((unsigned long) mem_base_lo & PCI_MEMORY_RANGE_MASK) << 16;
	limit = ((unsigned long) mem_limit_lo & PCI_MEMORY_RANGE_MASK) << 16;
	if (base <= limit) {
		res->flags = (mem_base_lo & PCI_MEMORY_RANGE_TYPE_MASK) | IORESOURCE_MEM;
		region.start = base;
		region.end = limit + 0xfffff;
		pcibios_bus_to_resource(dev->bus, res, &region);
		pci_info(dev, "  bridge window %pR\n", res);
	}
}

static void pci_read_bridge_mmio_pref(struct pci_bus *child)
{
	struct pci_dev *dev = child->self;
	u16 mem_base_lo, mem_limit_lo;
	u64 base64, limit64;
	pci_bus_addr_t base, limit;
	struct pci_bus_region region;
	struct resource *res;

	res = child->resource[2];
	pci_read_config_word(dev, PCI_PREF_MEMORY_BASE, &mem_base_lo);
	pci_read_config_word(dev, PCI_PREF_MEMORY_LIMIT, &mem_limit_lo);
	base64 = (mem_base_lo & PCI_PREF_RANGE_MASK) << 16;
	limit64 = (mem_limit_lo & PCI_PREF_RANGE_MASK) << 16;

	if ((mem_base_lo & PCI_PREF_RANGE_TYPE_MASK) == PCI_PREF_RANGE_TYPE_64) {
		u32 mem_base_hi, mem_limit_hi;

		pci_read_config_dword(dev, PCI_PREF_BASE_UPPER32, &mem_base_hi);
		pci_read_config_dword(dev, PCI_PREF_LIMIT_UPPER32, &mem_limit_hi);

		 
		if (mem_base_hi <= mem_limit_hi) {
			base64 |= (u64) mem_base_hi << 32;
			limit64 |= (u64) mem_limit_hi << 32;
		}
	}

	base = (pci_bus_addr_t) base64;
	limit = (pci_bus_addr_t) limit64;

	if (base != base64) {
		pci_err(dev, "can't handle bridge window above 4GB (bus address %#010llx)\n",
			(unsigned long long) base64);
		return;
	}

	if (base <= limit) {
		res->flags = (mem_base_lo & PCI_PREF_RANGE_TYPE_MASK) |
					 IORESOURCE_MEM | IORESOURCE_PREFETCH;
		if (res->flags & PCI_PREF_RANGE_TYPE_64)
			res->flags |= IORESOURCE_MEM_64;
		region.start = base;
		region.end = limit + 0xfffff;
		pcibios_bus_to_resource(dev->bus, res, &region);
		pci_info(dev, "  bridge window %pR\n", res);
	}
}

void pci_read_bridge_bases(struct pci_bus *child)
{
	struct pci_dev *dev = child->self;
	struct resource *res;
	int i;

	if (pci_is_root_bus(child))	 
		return;

	pci_info(dev, "PCI bridge to %pR%s\n",
		 &child->busn_res,
		 dev->transparent ? " (subtractive decode)" : "");

	pci_bus_remove_resources(child);
	for (i = 0; i < PCI_BRIDGE_RESOURCE_NUM; i++)
		child->resource[i] = &dev->resource[PCI_BRIDGE_RESOURCES+i];

	pci_read_bridge_io(child);
	pci_read_bridge_mmio(child);
	pci_read_bridge_mmio_pref(child);

	if (dev->transparent) {
		pci_bus_for_each_resource(child->parent, res) {
			if (res && res->flags) {
				pci_bus_add_resource(child, res,
						     PCI_SUBTRACTIVE_DECODE);
				pci_info(dev, "  bridge window %pR (subtractive decode)\n",
					   res);
			}
		}
	}
}

static struct pci_bus *pci_alloc_bus(struct pci_bus *parent)
{
	struct pci_bus *b;

	b = kzalloc(sizeof(*b), GFP_KERNEL);
	if (!b)
		return NULL;

	INIT_LIST_HEAD(&b->node);
	INIT_LIST_HEAD(&b->children);
	INIT_LIST_HEAD(&b->devices);
	INIT_LIST_HEAD(&b->slots);
	INIT_LIST_HEAD(&b->resources);
	b->max_bus_speed = PCI_SPEED_UNKNOWN;
	b->cur_bus_speed = PCI_SPEED_UNKNOWN;
#ifdef CONFIG_PCI_DOMAINS_GENERIC
	if (parent)
		b->domain_nr = parent->domain_nr;
#endif
	return b;
}

static void pci_release_host_bridge_dev(struct device *dev)
{
	struct pci_host_bridge *bridge = to_pci_host_bridge(dev);

	if (bridge->release_fn)
		bridge->release_fn(bridge);

	pci_free_resource_list(&bridge->windows);
	pci_free_resource_list(&bridge->dma_ranges);
	kfree(bridge);
}

static void pci_init_host_bridge(struct pci_host_bridge *bridge)
{
	INIT_LIST_HEAD(&bridge->windows);
	INIT_LIST_HEAD(&bridge->dma_ranges);

	 
	bridge->native_aer = 1;
	bridge->native_pcie_hotplug = 1;
	bridge->native_shpc_hotplug = 1;
	bridge->native_pme = 1;
	bridge->native_ltr = 1;
	bridge->native_dpc = 1;
	bridge->domain_nr = PCI_DOMAIN_NR_NOT_SET;
	bridge->native_cxl_error = 1;

	device_initialize(&bridge->dev);
}

struct pci_host_bridge *pci_alloc_host_bridge(size_t priv)
{
	struct pci_host_bridge *bridge;

	bridge = kzalloc(sizeof(*bridge) + priv, GFP_KERNEL);
	if (!bridge)
		return NULL;

	pci_init_host_bridge(bridge);
	bridge->dev.release = pci_release_host_bridge_dev;

	return bridge;
}
EXPORT_SYMBOL(pci_alloc_host_bridge);

static void devm_pci_alloc_host_bridge_release(void *data)
{
	pci_free_host_bridge(data);
}

struct pci_host_bridge *devm_pci_alloc_host_bridge(struct device *dev,
						   size_t priv)
{
	int ret;
	struct pci_host_bridge *bridge;

	bridge = pci_alloc_host_bridge(priv);
	if (!bridge)
		return NULL;

	bridge->dev.parent = dev;

	ret = devm_add_action_or_reset(dev, devm_pci_alloc_host_bridge_release,
				       bridge);
	if (ret)
		return NULL;

	ret = devm_of_pci_bridge_init(dev, bridge);
	if (ret)
		return NULL;

	return bridge;
}
EXPORT_SYMBOL(devm_pci_alloc_host_bridge);

void pci_free_host_bridge(struct pci_host_bridge *bridge)
{
	put_device(&bridge->dev);
}
EXPORT_SYMBOL(pci_free_host_bridge);

 
static const unsigned char pcix_bus_speed[] = {
	PCI_SPEED_UNKNOWN,		 
	PCI_SPEED_66MHz_PCIX,		 
	PCI_SPEED_100MHz_PCIX,		 
	PCI_SPEED_133MHz_PCIX,		 
	PCI_SPEED_UNKNOWN,		 
	PCI_SPEED_66MHz_PCIX_ECC,	 
	PCI_SPEED_100MHz_PCIX_ECC,	 
	PCI_SPEED_133MHz_PCIX_ECC,	 
	PCI_SPEED_UNKNOWN,		 
	PCI_SPEED_66MHz_PCIX_266,	 
	PCI_SPEED_100MHz_PCIX_266,	 
	PCI_SPEED_133MHz_PCIX_266,	 
	PCI_SPEED_UNKNOWN,		 
	PCI_SPEED_66MHz_PCIX_533,	 
	PCI_SPEED_100MHz_PCIX_533,	 
	PCI_SPEED_133MHz_PCIX_533	 
};

 
const unsigned char pcie_link_speed[] = {
	PCI_SPEED_UNKNOWN,		 
	PCIE_SPEED_2_5GT,		 
	PCIE_SPEED_5_0GT,		 
	PCIE_SPEED_8_0GT,		 
	PCIE_SPEED_16_0GT,		 
	PCIE_SPEED_32_0GT,		 
	PCIE_SPEED_64_0GT,		 
	PCI_SPEED_UNKNOWN,		 
	PCI_SPEED_UNKNOWN,		 
	PCI_SPEED_UNKNOWN,		 
	PCI_SPEED_UNKNOWN,		 
	PCI_SPEED_UNKNOWN,		 
	PCI_SPEED_UNKNOWN,		 
	PCI_SPEED_UNKNOWN,		 
	PCI_SPEED_UNKNOWN,		 
	PCI_SPEED_UNKNOWN		 
};
EXPORT_SYMBOL_GPL(pcie_link_speed);

const char *pci_speed_string(enum pci_bus_speed speed)
{
	 
	static const char *speed_strings[] = {
	    "33 MHz PCI",		 
	    "66 MHz PCI",		 
	    "66 MHz PCI-X",		 
	    "100 MHz PCI-X",		 
	    "133 MHz PCI-X",		 
	    NULL,			 
	    NULL,			 
	    NULL,			 
	    NULL,			 
	    "66 MHz PCI-X 266",		 
	    "100 MHz PCI-X 266",	 
	    "133 MHz PCI-X 266",	 
	    "Unknown AGP",		 
	    "1x AGP",			 
	    "2x AGP",			 
	    "4x AGP",			 
	    "8x AGP",			 
	    "66 MHz PCI-X 533",		 
	    "100 MHz PCI-X 533",	 
	    "133 MHz PCI-X 533",	 
	    "2.5 GT/s PCIe",		 
	    "5.0 GT/s PCIe",		 
	    "8.0 GT/s PCIe",		 
	    "16.0 GT/s PCIe",		 
	    "32.0 GT/s PCIe",		 
	    "64.0 GT/s PCIe",		 
	};

	if (speed < ARRAY_SIZE(speed_strings))
		return speed_strings[speed];
	return "Unknown";
}
EXPORT_SYMBOL_GPL(pci_speed_string);

void pcie_update_link_speed(struct pci_bus *bus, u16 linksta)
{
	bus->cur_bus_speed = pcie_link_speed[linksta & PCI_EXP_LNKSTA_CLS];
}
EXPORT_SYMBOL_GPL(pcie_update_link_speed);

static unsigned char agp_speeds[] = {
	AGP_UNKNOWN,
	AGP_1X,
	AGP_2X,
	AGP_4X,
	AGP_8X
};

static enum pci_bus_speed agp_speed(int agp3, int agpstat)
{
	int index = 0;

	if (agpstat & 4)
		index = 3;
	else if (agpstat & 2)
		index = 2;
	else if (agpstat & 1)
		index = 1;
	else
		goto out;

	if (agp3) {
		index += 2;
		if (index == 5)
			index = 0;
	}

 out:
	return agp_speeds[index];
}

static void pci_set_bus_speed(struct pci_bus *bus)
{
	struct pci_dev *bridge = bus->self;
	int pos;

	pos = pci_find_capability(bridge, PCI_CAP_ID_AGP);
	if (!pos)
		pos = pci_find_capability(bridge, PCI_CAP_ID_AGP3);
	if (pos) {
		u32 agpstat, agpcmd;

		pci_read_config_dword(bridge, pos + PCI_AGP_STATUS, &agpstat);
		bus->max_bus_speed = agp_speed(agpstat & 8, agpstat & 7);

		pci_read_config_dword(bridge, pos + PCI_AGP_COMMAND, &agpcmd);
		bus->cur_bus_speed = agp_speed(agpstat & 8, agpcmd & 7);
	}

	pos = pci_find_capability(bridge, PCI_CAP_ID_PCIX);
	if (pos) {
		u16 status;
		enum pci_bus_speed max;

		pci_read_config_word(bridge, pos + PCI_X_BRIDGE_SSTATUS,
				     &status);

		if (status & PCI_X_SSTATUS_533MHZ) {
			max = PCI_SPEED_133MHz_PCIX_533;
		} else if (status & PCI_X_SSTATUS_266MHZ) {
			max = PCI_SPEED_133MHz_PCIX_266;
		} else if (status & PCI_X_SSTATUS_133MHZ) {
			if ((status & PCI_X_SSTATUS_VERS) == PCI_X_SSTATUS_V2)
				max = PCI_SPEED_133MHz_PCIX_ECC;
			else
				max = PCI_SPEED_133MHz_PCIX;
		} else {
			max = PCI_SPEED_66MHz_PCIX;
		}

		bus->max_bus_speed = max;
		bus->cur_bus_speed = pcix_bus_speed[
			(status & PCI_X_SSTATUS_FREQ) >> 6];

		return;
	}

	if (pci_is_pcie(bridge)) {
		u32 linkcap;
		u16 linksta;

		pcie_capability_read_dword(bridge, PCI_EXP_LNKCAP, &linkcap);
		bus->max_bus_speed = pcie_link_speed[linkcap & PCI_EXP_LNKCAP_SLS];

		pcie_capability_read_word(bridge, PCI_EXP_LNKSTA, &linksta);
		pcie_update_link_speed(bus, linksta);
	}
}

static struct irq_domain *pci_host_bridge_msi_domain(struct pci_bus *bus)
{
	struct irq_domain *d;

	 
	d = dev_get_msi_domain(bus->bridge);

	 
	if (!d)
		d = pci_host_bridge_of_msi_domain(bus);
	if (!d)
		d = pci_host_bridge_acpi_msi_domain(bus);

	 
	if (!d) {
		struct fwnode_handle *fwnode = pci_root_bus_fwnode(bus);

		if (fwnode)
			d = irq_find_matching_fwnode(fwnode,
						     DOMAIN_BUS_PCI_MSI);
	}

	return d;
}

static void pci_set_bus_msi_domain(struct pci_bus *bus)
{
	struct irq_domain *d;
	struct pci_bus *b;

	 
	for (b = bus, d = NULL; !d && !pci_is_root_bus(b); b = b->parent) {
		if (b->self)
			d = dev_get_msi_domain(&b->self->dev);
	}

	if (!d)
		d = pci_host_bridge_msi_domain(b);

	dev_set_msi_domain(&bus->dev, d);
}

static int pci_register_host_bridge(struct pci_host_bridge *bridge)
{
	struct device *parent = bridge->dev.parent;
	struct resource_entry *window, *next, *n;
	struct pci_bus *bus, *b;
	resource_size_t offset, next_offset;
	LIST_HEAD(resources);
	struct resource *res, *next_res;
	char addr[64], *fmt;
	const char *name;
	int err;

	bus = pci_alloc_bus(NULL);
	if (!bus)
		return -ENOMEM;

	bridge->bus = bus;

	bus->sysdata = bridge->sysdata;
	bus->ops = bridge->ops;
	bus->number = bus->busn_res.start = bridge->busnr;
#ifdef CONFIG_PCI_DOMAINS_GENERIC
	if (bridge->domain_nr == PCI_DOMAIN_NR_NOT_SET)
		bus->domain_nr = pci_bus_find_domain_nr(bus, parent);
	else
		bus->domain_nr = bridge->domain_nr;
	if (bus->domain_nr < 0) {
		err = bus->domain_nr;
		goto free;
	}
#endif

	b = pci_find_bus(pci_domain_nr(bus), bridge->busnr);
	if (b) {
		 
		dev_dbg(&b->dev, "bus already known\n");
		err = -EEXIST;
		goto free;
	}

	dev_set_name(&bridge->dev, "pci%04x:%02x", pci_domain_nr(bus),
		     bridge->busnr);

	err = pcibios_root_bridge_prepare(bridge);
	if (err)
		goto free;

	 
	list_splice_init(&bridge->windows, &resources);
	err = device_add(&bridge->dev);
	if (err) {
		put_device(&bridge->dev);
		goto free;
	}
	bus->bridge = get_device(&bridge->dev);
	device_enable_async_suspend(bus->bridge);
	pci_set_bus_of_node(bus);
	pci_set_bus_msi_domain(bus);
	if (bridge->msi_domain && !dev_get_msi_domain(&bus->dev) &&
	    !pci_host_of_has_msi_map(parent))
		bus->bus_flags |= PCI_BUS_FLAGS_NO_MSI;

	if (!parent)
		set_dev_node(bus->bridge, pcibus_to_node(bus));

	bus->dev.class = &pcibus_class;
	bus->dev.parent = bus->bridge;

	dev_set_name(&bus->dev, "%04x:%02x", pci_domain_nr(bus), bus->number);
	name = dev_name(&bus->dev);

	err = device_register(&bus->dev);
	if (err)
		goto unregister;

	pcibios_add_bus(bus);

	if (bus->ops->add_bus) {
		err = bus->ops->add_bus(bus);
		if (WARN_ON(err < 0))
			dev_err(&bus->dev, "failed to add bus: %d\n", err);
	}

	 
	pci_create_legacy_files(bus);

	if (parent)
		dev_info(parent, "PCI host bridge to bus %s\n", name);
	else
		pr_info("PCI host bridge to bus %s\n", name);

	if (nr_node_ids > 1 && pcibus_to_node(bus) == NUMA_NO_NODE)
		dev_warn(&bus->dev, "Unknown NUMA node; performance will be reduced\n");

	 
	resource_list_for_each_entry_safe(window, n, &resources) {
		if (list_is_last(&window->node, &resources))
			break;

		next = list_next_entry(window, node);
		offset = window->offset;
		res = window->res;
		next_offset = next->offset;
		next_res = next->res;

		if (res->flags != next_res->flags || offset != next_offset)
			continue;

		if (res->end + 1 == next_res->start) {
			next_res->start = res->start;
			res->flags = res->start = res->end = 0;
		}
	}

	 
	resource_list_for_each_entry_safe(window, n, &resources) {
		offset = window->offset;
		res = window->res;
		if (!res->flags && !res->start && !res->end) {
			release_resource(res);
			resource_list_destroy_entry(window);
			continue;
		}

		list_move_tail(&window->node, &bridge->windows);

		if (res->flags & IORESOURCE_BUS)
			pci_bus_insert_busn_res(bus, bus->number, res->end);
		else
			pci_bus_add_resource(bus, res, 0);

		if (offset) {
			if (resource_type(res) == IORESOURCE_IO)
				fmt = " (bus address [%#06llx-%#06llx])";
			else
				fmt = " (bus address [%#010llx-%#010llx])";

			snprintf(addr, sizeof(addr), fmt,
				 (unsigned long long)(res->start - offset),
				 (unsigned long long)(res->end - offset));
		} else
			addr[0] = '\0';

		dev_info(&bus->dev, "root bus resource %pR%s\n", res, addr);
	}

	down_write(&pci_bus_sem);
	list_add_tail(&bus->node, &pci_root_buses);
	up_write(&pci_bus_sem);

	return 0;

unregister:
	put_device(&bridge->dev);
	device_del(&bridge->dev);

free:
#ifdef CONFIG_PCI_DOMAINS_GENERIC
	pci_bus_release_domain_nr(bus, parent);
#endif
	kfree(bus);
	return err;
}

static bool pci_bridge_child_ext_cfg_accessible(struct pci_dev *bridge)
{
	int pos;
	u32 status;

	 
	if (bridge->bus->bus_flags & PCI_BUS_FLAGS_NO_EXTCFG)
		return false;

	 
	if (pci_is_pcie(bridge) &&
	    (pci_pcie_type(bridge) == PCI_EXP_TYPE_ROOT_PORT ||
	     pci_pcie_type(bridge) == PCI_EXP_TYPE_UPSTREAM ||
	     pci_pcie_type(bridge) == PCI_EXP_TYPE_DOWNSTREAM))
		return true;

	 
	pos = pci_find_capability(bridge, PCI_CAP_ID_PCIX);
	if (!pos)
		return false;

	pci_read_config_dword(bridge, pos + PCI_X_STATUS, &status);
	return status & (PCI_X_STATUS_266MHZ | PCI_X_STATUS_533MHZ);
}

static struct pci_bus *pci_alloc_child_bus(struct pci_bus *parent,
					   struct pci_dev *bridge, int busnr)
{
	struct pci_bus *child;
	struct pci_host_bridge *host;
	int i;
	int ret;

	 
	child = pci_alloc_bus(parent);
	if (!child)
		return NULL;

	child->parent = parent;
	child->sysdata = parent->sysdata;
	child->bus_flags = parent->bus_flags;

	host = pci_find_host_bridge(parent);
	if (host->child_ops)
		child->ops = host->child_ops;
	else
		child->ops = parent->ops;

	 
	child->dev.class = &pcibus_class;
	dev_set_name(&child->dev, "%04x:%02x", pci_domain_nr(child), busnr);

	 
	child->number = child->busn_res.start = busnr;
	child->primary = parent->busn_res.start;
	child->busn_res.end = 0xff;

	if (!bridge) {
		child->dev.parent = parent->bridge;
		goto add_dev;
	}

	child->self = bridge;
	child->bridge = get_device(&bridge->dev);
	child->dev.parent = child->bridge;
	pci_set_bus_of_node(child);
	pci_set_bus_speed(child);

	 
	if (!pci_bridge_child_ext_cfg_accessible(bridge)) {
		child->bus_flags |= PCI_BUS_FLAGS_NO_EXTCFG;
		pci_info(child, "extended config space not accessible\n");
	}

	 
	for (i = 0; i < PCI_BRIDGE_RESOURCE_NUM; i++) {
		child->resource[i] = &bridge->resource[PCI_BRIDGE_RESOURCES+i];
		child->resource[i]->name = child->name;
	}
	bridge->subordinate = child;

add_dev:
	pci_set_bus_msi_domain(child);
	ret = device_register(&child->dev);
	WARN_ON(ret < 0);

	pcibios_add_bus(child);

	if (child->ops->add_bus) {
		ret = child->ops->add_bus(child);
		if (WARN_ON(ret < 0))
			dev_err(&child->dev, "failed to add bus: %d\n", ret);
	}

	 
	pci_create_legacy_files(child);

	return child;
}

struct pci_bus *pci_add_new_bus(struct pci_bus *parent, struct pci_dev *dev,
				int busnr)
{
	struct pci_bus *child;

	child = pci_alloc_child_bus(parent, dev, busnr);
	if (child) {
		down_write(&pci_bus_sem);
		list_add_tail(&child->node, &parent->children);
		up_write(&pci_bus_sem);
	}
	return child;
}
EXPORT_SYMBOL(pci_add_new_bus);

static void pci_enable_crs(struct pci_dev *pdev)
{
	u16 root_cap = 0;

	 
	pcie_capability_read_word(pdev, PCI_EXP_RTCAP, &root_cap);
	if (root_cap & PCI_EXP_RTCAP_CRSVIS)
		pcie_capability_set_word(pdev, PCI_EXP_RTCTL,
					 PCI_EXP_RTCTL_CRSSVE);
}

static unsigned int pci_scan_child_bus_extend(struct pci_bus *bus,
					      unsigned int available_buses);
 
static bool pci_ea_fixed_busnrs(struct pci_dev *dev, u8 *sec, u8 *sub)
{
	int ea, offset;
	u32 dw;
	u8 ea_sec, ea_sub;

	if (dev->hdr_type != PCI_HEADER_TYPE_BRIDGE)
		return false;

	 
	ea = pci_find_capability(dev, PCI_CAP_ID_EA);
	if (!ea)
		return false;

	offset = ea + PCI_EA_FIRST_ENT;
	pci_read_config_dword(dev, offset, &dw);
	ea_sec =  dw & PCI_EA_SEC_BUS_MASK;
	ea_sub = (dw & PCI_EA_SUB_BUS_MASK) >> PCI_EA_SUB_BUS_SHIFT;
	if (ea_sec  == 0 || ea_sub < ea_sec)
		return false;

	*sec = ea_sec;
	*sub = ea_sub;
	return true;
}

 
static int pci_scan_bridge_extend(struct pci_bus *bus, struct pci_dev *dev,
				  int max, unsigned int available_buses,
				  int pass)
{
	struct pci_bus *child;
	int is_cardbus = (dev->hdr_type == PCI_HEADER_TYPE_CARDBUS);
	u32 buses, i, j = 0;
	u16 bctl;
	u8 primary, secondary, subordinate;
	int broken = 0;
	bool fixed_buses;
	u8 fixed_sec, fixed_sub;
	int next_busnr;

	 
	pm_runtime_get_sync(&dev->dev);

	pci_read_config_dword(dev, PCI_PRIMARY_BUS, &buses);
	primary = buses & 0xFF;
	secondary = (buses >> 8) & 0xFF;
	subordinate = (buses >> 16) & 0xFF;

	pci_dbg(dev, "scanning [bus %02x-%02x] behind bridge, pass %d\n",
		secondary, subordinate, pass);

	if (!primary && (primary != bus->number) && secondary && subordinate) {
		pci_warn(dev, "Primary bus is hard wired to 0\n");
		primary = bus->number;
	}

	 
	if (!pass &&
	    (primary != bus->number || secondary <= bus->number ||
	     secondary > subordinate)) {
		pci_info(dev, "bridge configuration invalid ([bus %02x-%02x]), reconfiguring\n",
			 secondary, subordinate);
		broken = 1;
	}

	 
	pci_read_config_word(dev, PCI_BRIDGE_CONTROL, &bctl);
	pci_write_config_word(dev, PCI_BRIDGE_CONTROL,
			      bctl & ~PCI_BRIDGE_CTL_MASTER_ABORT);

	pci_enable_crs(dev);

	if ((secondary || subordinate) && !pcibios_assign_all_busses() &&
	    !is_cardbus && !broken) {
		unsigned int cmax, buses;

		 
		if (pass)
			goto out;

		 
		child = pci_find_bus(pci_domain_nr(bus), secondary);
		if (!child) {
			child = pci_add_new_bus(bus, dev, secondary);
			if (!child)
				goto out;
			child->primary = primary;
			pci_bus_insert_busn_res(child, secondary, subordinate);
			child->bridge_ctl = bctl;
		}

		buses = subordinate - secondary;
		cmax = pci_scan_child_bus_extend(child, buses);
		if (cmax > subordinate)
			pci_warn(dev, "bridge has subordinate %02x but max busn %02x\n",
				 subordinate, cmax);

		 
		if (subordinate > max)
			max = subordinate;
	} else {

		 
		if (!pass) {
			if (pcibios_assign_all_busses() || broken || is_cardbus)

				 
				pci_write_config_dword(dev, PCI_PRIMARY_BUS,
						       buses & ~0xffffff);
			goto out;
		}

		 
		pci_write_config_word(dev, PCI_STATUS, 0xffff);

		 
		fixed_buses = pci_ea_fixed_busnrs(dev, &fixed_sec, &fixed_sub);
		if (fixed_buses)
			next_busnr = fixed_sec;
		else
			next_busnr = max + 1;

		 
		child = pci_find_bus(pci_domain_nr(bus), next_busnr);
		if (!child) {
			child = pci_add_new_bus(bus, dev, next_busnr);
			if (!child)
				goto out;
			pci_bus_insert_busn_res(child, next_busnr,
						bus->busn_res.end);
		}
		max++;
		if (available_buses)
			available_buses--;

		buses = (buses & 0xff000000)
		      | ((unsigned int)(child->primary)     <<  0)
		      | ((unsigned int)(child->busn_res.start)   <<  8)
		      | ((unsigned int)(child->busn_res.end) << 16);

		 
		if (is_cardbus) {
			buses &= ~0xff000000;
			buses |= CARDBUS_LATENCY_TIMER << 24;
		}

		 
		pci_write_config_dword(dev, PCI_PRIMARY_BUS, buses);

		if (!is_cardbus) {
			child->bridge_ctl = bctl;
			max = pci_scan_child_bus_extend(child, available_buses);
		} else {

			 
			for (i = 0; i < CARDBUS_RESERVE_BUSNR; i++) {
				struct pci_bus *parent = bus;
				if (pci_find_bus(pci_domain_nr(bus),
							max+i+1))
					break;
				while (parent->parent) {
					if ((!pcibios_assign_all_busses()) &&
					    (parent->busn_res.end > max) &&
					    (parent->busn_res.end <= max+i)) {
						j = 1;
					}
					parent = parent->parent;
				}
				if (j) {

					 
					i /= 2;
					break;
				}
			}
			max += i;
		}

		 
		if (fixed_buses)
			max = fixed_sub;
		pci_bus_update_busn_res_end(child, max);
		pci_write_config_byte(dev, PCI_SUBORDINATE_BUS, max);
	}

	sprintf(child->name,
		(is_cardbus ? "PCI CardBus %04x:%02x" : "PCI Bus %04x:%02x"),
		pci_domain_nr(bus), child->number);

	 
	while (bus->parent) {
		if ((child->busn_res.end > bus->busn_res.end) ||
		    (child->number > bus->busn_res.end) ||
		    (child->number < bus->number) ||
		    (child->busn_res.end < bus->number)) {
			dev_info(&dev->dev, "devices behind bridge are unusable because %pR cannot be assigned for them\n",
				 &child->busn_res);
			break;
		}
		bus = bus->parent;
	}

out:
	pci_write_config_word(dev, PCI_BRIDGE_CONTROL, bctl);

	pm_runtime_put(&dev->dev);

	return max;
}

 
int pci_scan_bridge(struct pci_bus *bus, struct pci_dev *dev, int max, int pass)
{
	return pci_scan_bridge_extend(bus, dev, max, 0, pass);
}
EXPORT_SYMBOL(pci_scan_bridge);

 
static void pci_read_irq(struct pci_dev *dev)
{
	unsigned char irq;

	 
	if (dev->is_virtfn) {
		dev->pin = 0;
		dev->irq = 0;
		return;
	}

	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &irq);
	dev->pin = irq;
	if (irq)
		pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq);
	dev->irq = irq;
}

void set_pcie_port_type(struct pci_dev *pdev)
{
	int pos;
	u16 reg16;
	u32 reg32;
	int type;
	struct pci_dev *parent;

	pos = pci_find_capability(pdev, PCI_CAP_ID_EXP);
	if (!pos)
		return;

	pdev->pcie_cap = pos;
	pci_read_config_word(pdev, pos + PCI_EXP_FLAGS, &reg16);
	pdev->pcie_flags_reg = reg16;
	pci_read_config_dword(pdev, pos + PCI_EXP_DEVCAP, &pdev->devcap);
	pdev->pcie_mpss = FIELD_GET(PCI_EXP_DEVCAP_PAYLOAD, pdev->devcap);

	pcie_capability_read_dword(pdev, PCI_EXP_LNKCAP, &reg32);
	if (reg32 & PCI_EXP_LNKCAP_DLLLARC)
		pdev->link_active_reporting = 1;

	parent = pci_upstream_bridge(pdev);
	if (!parent)
		return;

	 
	type = pci_pcie_type(pdev);
	if (type == PCI_EXP_TYPE_DOWNSTREAM) {
		 
		if (pcie_downstream_port(parent)) {
			pci_info(pdev, "claims to be downstream port but is acting as upstream port, correcting type\n");
			pdev->pcie_flags_reg &= ~PCI_EXP_FLAGS_TYPE;
			pdev->pcie_flags_reg |= PCI_EXP_TYPE_UPSTREAM;
		}
	} else if (type == PCI_EXP_TYPE_UPSTREAM) {
		 
		if (pci_pcie_type(parent) == PCI_EXP_TYPE_UPSTREAM) {
			pci_info(pdev, "claims to be upstream port but is acting as downstream port, correcting type\n");
			pdev->pcie_flags_reg &= ~PCI_EXP_FLAGS_TYPE;
			pdev->pcie_flags_reg |= PCI_EXP_TYPE_DOWNSTREAM;
		}
	}
}

void set_pcie_hotplug_bridge(struct pci_dev *pdev)
{
	u32 reg32;

	pcie_capability_read_dword(pdev, PCI_EXP_SLTCAP, &reg32);
	if (reg32 & PCI_EXP_SLTCAP_HPC)
		pdev->is_hotplug_bridge = 1;
}

static void set_pcie_thunderbolt(struct pci_dev *dev)
{
	u16 vsec;

	 
	vsec = pci_find_vsec_capability(dev, PCI_VENDOR_ID_INTEL, PCI_VSEC_ID_INTEL_TBT);
	if (vsec)
		dev->is_thunderbolt = 1;
}

static void set_pcie_untrusted(struct pci_dev *dev)
{
	struct pci_dev *parent;

	 
	parent = pci_upstream_bridge(dev);
	if (parent && (parent->untrusted || parent->external_facing))
		dev->untrusted = true;
}

static void pci_set_removable(struct pci_dev *dev)
{
	struct pci_dev *parent = pci_upstream_bridge(dev);

	 
	if (parent &&
	    (parent->external_facing || dev_is_removable(&parent->dev)))
		dev_set_removable(&dev->dev, DEVICE_REMOVABLE);
}

 
static bool pci_ext_cfg_is_aliased(struct pci_dev *dev)
{
#ifdef CONFIG_PCI_QUIRKS
	int pos, ret;
	u32 header, tmp;

	pci_read_config_dword(dev, PCI_VENDOR_ID, &header);

	for (pos = PCI_CFG_SPACE_SIZE;
	     pos < PCI_CFG_SPACE_EXP_SIZE; pos += PCI_CFG_SPACE_SIZE) {
		ret = pci_read_config_dword(dev, pos, &tmp);
		if ((ret != PCIBIOS_SUCCESSFUL) || (header != tmp))
			return false;
	}

	return true;
#else
	return false;
#endif
}

 
static int pci_cfg_space_size_ext(struct pci_dev *dev)
{
	u32 status;
	int pos = PCI_CFG_SPACE_SIZE;

	if (pci_read_config_dword(dev, pos, &status) != PCIBIOS_SUCCESSFUL)
		return PCI_CFG_SPACE_SIZE;
	if (PCI_POSSIBLE_ERROR(status) || pci_ext_cfg_is_aliased(dev))
		return PCI_CFG_SPACE_SIZE;

	return PCI_CFG_SPACE_EXP_SIZE;
}

int pci_cfg_space_size(struct pci_dev *dev)
{
	int pos;
	u32 status;
	u16 class;

#ifdef CONFIG_PCI_IOV
	 
	if (dev->is_virtfn)
		return PCI_CFG_SPACE_EXP_SIZE;
#endif

	if (dev->bus->bus_flags & PCI_BUS_FLAGS_NO_EXTCFG)
		return PCI_CFG_SPACE_SIZE;

	class = dev->class >> 8;
	if (class == PCI_CLASS_BRIDGE_HOST)
		return pci_cfg_space_size_ext(dev);

	if (pci_is_pcie(dev))
		return pci_cfg_space_size_ext(dev);

	pos = pci_find_capability(dev, PCI_CAP_ID_PCIX);
	if (!pos)
		return PCI_CFG_SPACE_SIZE;

	pci_read_config_dword(dev, pos + PCI_X_STATUS, &status);
	if (status & (PCI_X_STATUS_266MHZ | PCI_X_STATUS_533MHZ))
		return pci_cfg_space_size_ext(dev);

	return PCI_CFG_SPACE_SIZE;
}

static u32 pci_class(struct pci_dev *dev)
{
	u32 class;

#ifdef CONFIG_PCI_IOV
	if (dev->is_virtfn)
		return dev->physfn->sriov->class;
#endif
	pci_read_config_dword(dev, PCI_CLASS_REVISION, &class);
	return class;
}

static void pci_subsystem_ids(struct pci_dev *dev, u16 *vendor, u16 *device)
{
#ifdef CONFIG_PCI_IOV
	if (dev->is_virtfn) {
		*vendor = dev->physfn->sriov->subsystem_vendor;
		*device = dev->physfn->sriov->subsystem_device;
		return;
	}
#endif
	pci_read_config_word(dev, PCI_SUBSYSTEM_VENDOR_ID, vendor);
	pci_read_config_word(dev, PCI_SUBSYSTEM_ID, device);
}

static u8 pci_hdr_type(struct pci_dev *dev)
{
	u8 hdr_type;

#ifdef CONFIG_PCI_IOV
	if (dev->is_virtfn)
		return dev->physfn->sriov->hdr_type;
#endif
	pci_read_config_byte(dev, PCI_HEADER_TYPE, &hdr_type);
	return hdr_type;
}

#define LEGACY_IO_RESOURCE	(IORESOURCE_IO | IORESOURCE_PCI_FIXED)

 
static int pci_intx_mask_broken(struct pci_dev *dev)
{
	u16 orig, toggle, new;

	pci_read_config_word(dev, PCI_COMMAND, &orig);
	toggle = orig ^ PCI_COMMAND_INTX_DISABLE;
	pci_write_config_word(dev, PCI_COMMAND, toggle);
	pci_read_config_word(dev, PCI_COMMAND, &new);

	pci_write_config_word(dev, PCI_COMMAND, orig);

	 
	if (new != toggle)
		return 1;
	return 0;
}

static void early_dump_pci_device(struct pci_dev *pdev)
{
	u32 value[256 / 4];
	int i;

	pci_info(pdev, "config space:\n");

	for (i = 0; i < 256; i += 4)
		pci_read_config_dword(pdev, i, &value[i / 4]);

	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET, 16, 1,
		       value, 256, false);
}

 
int pci_setup_device(struct pci_dev *dev)
{
	u32 class;
	u16 cmd;
	u8 hdr_type;
	int err, pos = 0;
	struct pci_bus_region region;
	struct resource *res;

	hdr_type = pci_hdr_type(dev);

	dev->sysdata = dev->bus->sysdata;
	dev->dev.parent = dev->bus->bridge;
	dev->dev.bus = &pci_bus_type;
	dev->hdr_type = hdr_type & 0x7f;
	dev->multifunction = !!(hdr_type & 0x80);
	dev->error_state = pci_channel_io_normal;
	set_pcie_port_type(dev);

	err = pci_set_of_node(dev);
	if (err)
		return err;
	pci_set_acpi_fwnode(dev);

	pci_dev_assign_slot(dev);

	 
	dev->dma_mask = 0xffffffff;

	dev_set_name(&dev->dev, "%04x:%02x:%02x.%d", pci_domain_nr(dev->bus),
		     dev->bus->number, PCI_SLOT(dev->devfn),
		     PCI_FUNC(dev->devfn));

	class = pci_class(dev);

	dev->revision = class & 0xff;
	dev->class = class >> 8;		     

	if (pci_early_dump)
		early_dump_pci_device(dev);

	 
	dev->cfg_size = pci_cfg_space_size(dev);

	 
	set_pcie_thunderbolt(dev);

	set_pcie_untrusted(dev);

	 
	dev->current_state = PCI_UNKNOWN;

	 
	pci_fixup_device(pci_fixup_early, dev);

	pci_set_removable(dev);

	pci_info(dev, "[%04x:%04x] type %02x class %#08x\n",
		 dev->vendor, dev->device, dev->hdr_type, dev->class);

	 
	class = dev->class >> 8;

	if (dev->non_compliant_bars && !dev->mmio_always_on) {
		pci_read_config_word(dev, PCI_COMMAND, &cmd);
		if (cmd & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY)) {
			pci_info(dev, "device has non-compliant BARs; disabling IO/MEM decoding\n");
			cmd &= ~PCI_COMMAND_IO;
			cmd &= ~PCI_COMMAND_MEMORY;
			pci_write_config_word(dev, PCI_COMMAND, cmd);
		}
	}

	dev->broken_intx_masking = pci_intx_mask_broken(dev);

	switch (dev->hdr_type) {		     
	case PCI_HEADER_TYPE_NORMAL:		     
		if (class == PCI_CLASS_BRIDGE_PCI)
			goto bad;
		pci_read_irq(dev);
		pci_read_bases(dev, 6, PCI_ROM_ADDRESS);

		pci_subsystem_ids(dev, &dev->subsystem_vendor, &dev->subsystem_device);

		 
		if (class == PCI_CLASS_STORAGE_IDE) {
			u8 progif;
			pci_read_config_byte(dev, PCI_CLASS_PROG, &progif);
			if ((progif & 1) == 0) {
				region.start = 0x1F0;
				region.end = 0x1F7;
				res = &dev->resource[0];
				res->flags = LEGACY_IO_RESOURCE;
				pcibios_bus_to_resource(dev->bus, res, &region);
				pci_info(dev, "legacy IDE quirk: reg 0x10: %pR\n",
					 res);
				region.start = 0x3F6;
				region.end = 0x3F6;
				res = &dev->resource[1];
				res->flags = LEGACY_IO_RESOURCE;
				pcibios_bus_to_resource(dev->bus, res, &region);
				pci_info(dev, "legacy IDE quirk: reg 0x14: %pR\n",
					 res);
			}
			if ((progif & 4) == 0) {
				region.start = 0x170;
				region.end = 0x177;
				res = &dev->resource[2];
				res->flags = LEGACY_IO_RESOURCE;
				pcibios_bus_to_resource(dev->bus, res, &region);
				pci_info(dev, "legacy IDE quirk: reg 0x18: %pR\n",
					 res);
				region.start = 0x376;
				region.end = 0x376;
				res = &dev->resource[3];
				res->flags = LEGACY_IO_RESOURCE;
				pcibios_bus_to_resource(dev->bus, res, &region);
				pci_info(dev, "legacy IDE quirk: reg 0x1c: %pR\n",
					 res);
			}
		}
		break;

	case PCI_HEADER_TYPE_BRIDGE:		     
		 
		pci_read_irq(dev);
		dev->transparent = ((dev->class & 0xff) == 1);
		pci_read_bases(dev, 2, PCI_ROM_ADDRESS1);
		pci_read_bridge_windows(dev);
		set_pcie_hotplug_bridge(dev);
		pos = pci_find_capability(dev, PCI_CAP_ID_SSVID);
		if (pos) {
			pci_read_config_word(dev, pos + PCI_SSVID_VENDOR_ID, &dev->subsystem_vendor);
			pci_read_config_word(dev, pos + PCI_SSVID_DEVICE_ID, &dev->subsystem_device);
		}
		break;

	case PCI_HEADER_TYPE_CARDBUS:		     
		if (class != PCI_CLASS_BRIDGE_CARDBUS)
			goto bad;
		pci_read_irq(dev);
		pci_read_bases(dev, 1, 0);
		pci_read_config_word(dev, PCI_CB_SUBSYSTEM_VENDOR_ID, &dev->subsystem_vendor);
		pci_read_config_word(dev, PCI_CB_SUBSYSTEM_ID, &dev->subsystem_device);
		break;

	default:				     
		pci_err(dev, "unknown header type %02x, ignoring device\n",
			dev->hdr_type);
		pci_release_of_node(dev);
		return -EIO;

	bad:
		pci_err(dev, "ignoring class %#08x (doesn't match header type %02x)\n",
			dev->class, dev->hdr_type);
		dev->class = PCI_CLASS_NOT_DEFINED << 8;
	}

	 
	return 0;
}

static void pci_configure_mps(struct pci_dev *dev)
{
	struct pci_dev *bridge = pci_upstream_bridge(dev);
	int mps, mpss, p_mps, rc;

	if (!pci_is_pcie(dev))
		return;

	 
	if (dev->is_virtfn)
		return;

	 
	if (pci_pcie_type(dev) == PCI_EXP_TYPE_RC_END) {
		if (pcie_bus_config == PCIE_BUS_PEER2PEER)
			mps = 128;
		else
			mps = 128 << dev->pcie_mpss;
		rc = pcie_set_mps(dev, mps);
		if (rc) {
			pci_warn(dev, "can't set Max Payload Size to %d; if necessary, use \"pci=pcie_bus_safe\" and report a bug\n",
				 mps);
		}
		return;
	}

	if (!bridge || !pci_is_pcie(bridge))
		return;

	mps = pcie_get_mps(dev);
	p_mps = pcie_get_mps(bridge);

	if (mps == p_mps)
		return;

	if (pcie_bus_config == PCIE_BUS_TUNE_OFF) {
		pci_warn(dev, "Max Payload Size %d, but upstream %s set to %d; if necessary, use \"pci=pcie_bus_safe\" and report a bug\n",
			 mps, pci_name(bridge), p_mps);
		return;
	}

	 
	if (pcie_bus_config != PCIE_BUS_DEFAULT)
		return;

	mpss = 128 << dev->pcie_mpss;
	if (mpss < p_mps && pci_pcie_type(bridge) == PCI_EXP_TYPE_ROOT_PORT) {
		pcie_set_mps(bridge, mpss);
		pci_info(dev, "Upstream bridge's Max Payload Size set to %d (was %d, max %d)\n",
			 mpss, p_mps, 128 << bridge->pcie_mpss);
		p_mps = pcie_get_mps(bridge);
	}

	rc = pcie_set_mps(dev, p_mps);
	if (rc) {
		pci_warn(dev, "can't set Max Payload Size to %d; if necessary, use \"pci=pcie_bus_safe\" and report a bug\n",
			 p_mps);
		return;
	}

	pci_info(dev, "Max Payload Size set to %d (was %d, max %d)\n",
		 p_mps, mps, mpss);
}

int pci_configure_extended_tags(struct pci_dev *dev, void *ign)
{
	struct pci_host_bridge *host;
	u32 cap;
	u16 ctl;
	int ret;

	if (!pci_is_pcie(dev))
		return 0;

	ret = pcie_capability_read_dword(dev, PCI_EXP_DEVCAP, &cap);
	if (ret)
		return 0;

	if (!(cap & PCI_EXP_DEVCAP_EXT_TAG))
		return 0;

	ret = pcie_capability_read_word(dev, PCI_EXP_DEVCTL, &ctl);
	if (ret)
		return 0;

	host = pci_find_host_bridge(dev->bus);
	if (!host)
		return 0;

	 
	if (host->no_ext_tags) {
		if (ctl & PCI_EXP_DEVCTL_EXT_TAG) {
			pci_info(dev, "disabling Extended Tags\n");
			pcie_capability_clear_word(dev, PCI_EXP_DEVCTL,
						   PCI_EXP_DEVCTL_EXT_TAG);
		}
		return 0;
	}

	if (!(ctl & PCI_EXP_DEVCTL_EXT_TAG)) {
		pci_info(dev, "enabling Extended Tags\n");
		pcie_capability_set_word(dev, PCI_EXP_DEVCTL,
					 PCI_EXP_DEVCTL_EXT_TAG);
	}
	return 0;
}

 
bool pcie_relaxed_ordering_enabled(struct pci_dev *dev)
{
	u16 v;

	pcie_capability_read_word(dev, PCI_EXP_DEVCTL, &v);

	return !!(v & PCI_EXP_DEVCTL_RELAX_EN);
}
EXPORT_SYMBOL(pcie_relaxed_ordering_enabled);

static void pci_configure_relaxed_ordering(struct pci_dev *dev)
{
	struct pci_dev *root;

	 
	if (dev->is_virtfn)
		return;

	if (!pcie_relaxed_ordering_enabled(dev))
		return;

	 
	root = pcie_find_root_port(dev);
	if (!root)
		return;

	if (root->dev_flags & PCI_DEV_FLAGS_NO_RELAXED_ORDERING) {
		pcie_capability_clear_word(dev, PCI_EXP_DEVCTL,
					   PCI_EXP_DEVCTL_RELAX_EN);
		pci_info(dev, "Relaxed Ordering disabled because the Root Port didn't support it\n");
	}
}

static void pci_configure_ltr(struct pci_dev *dev)
{
#ifdef CONFIG_PCIEASPM
	struct pci_host_bridge *host = pci_find_host_bridge(dev->bus);
	struct pci_dev *bridge;
	u32 cap, ctl;

	if (!pci_is_pcie(dev))
		return;

	 
	dev->l1ss = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_L1SS);

	pcie_capability_read_dword(dev, PCI_EXP_DEVCAP2, &cap);
	if (!(cap & PCI_EXP_DEVCAP2_LTR))
		return;

	pcie_capability_read_dword(dev, PCI_EXP_DEVCTL2, &ctl);
	if (ctl & PCI_EXP_DEVCTL2_LTR_EN) {
		if (pci_pcie_type(dev) == PCI_EXP_TYPE_ROOT_PORT) {
			dev->ltr_path = 1;
			return;
		}

		bridge = pci_upstream_bridge(dev);
		if (bridge && bridge->ltr_path)
			dev->ltr_path = 1;

		return;
	}

	if (!host->native_ltr)
		return;

	 
	if (pci_pcie_type(dev) == PCI_EXP_TYPE_ROOT_PORT) {
		pcie_capability_set_word(dev, PCI_EXP_DEVCTL2,
					 PCI_EXP_DEVCTL2_LTR_EN);
		dev->ltr_path = 1;
		return;
	}

	 
	bridge = pci_upstream_bridge(dev);
	if (bridge && bridge->ltr_path) {
		pci_bridge_reconfigure_ltr(dev);
		pcie_capability_set_word(dev, PCI_EXP_DEVCTL2,
					 PCI_EXP_DEVCTL2_LTR_EN);
		dev->ltr_path = 1;
	}
#endif
}

static void pci_configure_eetlp_prefix(struct pci_dev *dev)
{
#ifdef CONFIG_PCI_PASID
	struct pci_dev *bridge;
	int pcie_type;
	u32 cap;

	if (!pci_is_pcie(dev))
		return;

	pcie_capability_read_dword(dev, PCI_EXP_DEVCAP2, &cap);
	if (!(cap & PCI_EXP_DEVCAP2_EE_PREFIX))
		return;

	pcie_type = pci_pcie_type(dev);
	if (pcie_type == PCI_EXP_TYPE_ROOT_PORT ||
	    pcie_type == PCI_EXP_TYPE_RC_END)
		dev->eetlp_prefix_path = 1;
	else {
		bridge = pci_upstream_bridge(dev);
		if (bridge && bridge->eetlp_prefix_path)
			dev->eetlp_prefix_path = 1;
	}
#endif
}

static void pci_configure_serr(struct pci_dev *dev)
{
	u16 control;

	if (dev->hdr_type == PCI_HEADER_TYPE_BRIDGE) {

		 
		pci_read_config_word(dev, PCI_BRIDGE_CONTROL, &control);
		if (!(control & PCI_BRIDGE_CTL_SERR)) {
			control |= PCI_BRIDGE_CTL_SERR;
			pci_write_config_word(dev, PCI_BRIDGE_CONTROL, control);
		}
	}
}

static void pci_configure_device(struct pci_dev *dev)
{
	pci_configure_mps(dev);
	pci_configure_extended_tags(dev, NULL);
	pci_configure_relaxed_ordering(dev);
	pci_configure_ltr(dev);
	pci_configure_eetlp_prefix(dev);
	pci_configure_serr(dev);

	pci_acpi_program_hp_params(dev);
}

static void pci_release_capabilities(struct pci_dev *dev)
{
	pci_aer_exit(dev);
	pci_rcec_exit(dev);
	pci_iov_release(dev);
	pci_free_cap_save_buffers(dev);
}

 
static void pci_release_dev(struct device *dev)
{
	struct pci_dev *pci_dev;

	pci_dev = to_pci_dev(dev);
	pci_release_capabilities(pci_dev);
	pci_release_of_node(pci_dev);
	pcibios_release_device(pci_dev);
	pci_bus_put(pci_dev->bus);
	kfree(pci_dev->driver_override);
	bitmap_free(pci_dev->dma_alias_mask);
	dev_dbg(dev, "device released\n");
	kfree(pci_dev);
}

struct pci_dev *pci_alloc_dev(struct pci_bus *bus)
{
	struct pci_dev *dev;

	dev = kzalloc(sizeof(struct pci_dev), GFP_KERNEL);
	if (!dev)
		return NULL;

	INIT_LIST_HEAD(&dev->bus_list);
	dev->dev.type = &pci_dev_type;
	dev->bus = pci_bus_get(bus);
	dev->driver_exclusive_resource = (struct resource) {
		.name = "PCI Exclusive",
		.start = 0,
		.end = -1,
	};

	spin_lock_init(&dev->pcie_cap_lock);
#ifdef CONFIG_PCI_MSI
	raw_spin_lock_init(&dev->msi_lock);
#endif
	return dev;
}
EXPORT_SYMBOL(pci_alloc_dev);

static bool pci_bus_crs_vendor_id(u32 l)
{
	return (l & 0xffff) == PCI_VENDOR_ID_PCI_SIG;
}

static bool pci_bus_wait_crs(struct pci_bus *bus, int devfn, u32 *l,
			     int timeout)
{
	int delay = 1;

	if (!pci_bus_crs_vendor_id(*l))
		return true;	 

	if (!timeout)
		return false;	 

	 
	while (pci_bus_crs_vendor_id(*l)) {
		if (delay > timeout) {
			pr_warn("pci %04x:%02x:%02x.%d: not ready after %dms; giving up\n",
				pci_domain_nr(bus), bus->number,
				PCI_SLOT(devfn), PCI_FUNC(devfn), delay - 1);

			return false;
		}
		if (delay >= 1000)
			pr_info("pci %04x:%02x:%02x.%d: not ready after %dms; waiting\n",
				pci_domain_nr(bus), bus->number,
				PCI_SLOT(devfn), PCI_FUNC(devfn), delay - 1);

		msleep(delay);
		delay *= 2;

		if (pci_bus_read_config_dword(bus, devfn, PCI_VENDOR_ID, l))
			return false;
	}

	if (delay >= 1000)
		pr_info("pci %04x:%02x:%02x.%d: ready after %dms\n",
			pci_domain_nr(bus), bus->number,
			PCI_SLOT(devfn), PCI_FUNC(devfn), delay - 1);

	return true;
}

bool pci_bus_generic_read_dev_vendor_id(struct pci_bus *bus, int devfn, u32 *l,
					int timeout)
{
	if (pci_bus_read_config_dword(bus, devfn, PCI_VENDOR_ID, l))
		return false;

	 
	if (PCI_POSSIBLE_ERROR(*l) || *l == 0x00000000 ||
	    *l == 0x0000ffff || *l == 0xffff0000)
		return false;

	if (pci_bus_crs_vendor_id(*l))
		return pci_bus_wait_crs(bus, devfn, l, timeout);

	return true;
}

bool pci_bus_read_dev_vendor_id(struct pci_bus *bus, int devfn, u32 *l,
				int timeout)
{
#ifdef CONFIG_PCI_QUIRKS
	struct pci_dev *bridge = bus->self;

	 
	if (bridge && bridge->vendor == PCI_VENDOR_ID_IDT &&
	    bridge->device == 0x80b5)
		return pci_idt_bus_quirk(bus, devfn, l, timeout);
#endif

	return pci_bus_generic_read_dev_vendor_id(bus, devfn, l, timeout);
}
EXPORT_SYMBOL(pci_bus_read_dev_vendor_id);

 
static struct pci_dev *pci_scan_device(struct pci_bus *bus, int devfn)
{
	struct pci_dev *dev;
	u32 l;

	if (!pci_bus_read_dev_vendor_id(bus, devfn, &l, 60*1000))
		return NULL;

	dev = pci_alloc_dev(bus);
	if (!dev)
		return NULL;

	dev->devfn = devfn;
	dev->vendor = l & 0xffff;
	dev->device = (l >> 16) & 0xffff;

	if (pci_setup_device(dev)) {
		pci_bus_put(dev->bus);
		kfree(dev);
		return NULL;
	}

	return dev;
}

void pcie_report_downtraining(struct pci_dev *dev)
{
	if (!pci_is_pcie(dev))
		return;

	 
	if ((pci_pcie_type(dev) != PCI_EXP_TYPE_ENDPOINT) &&
	    (pci_pcie_type(dev) != PCI_EXP_TYPE_LEG_END) &&
	    (pci_pcie_type(dev) != PCI_EXP_TYPE_UPSTREAM))
		return;

	 
	if (PCI_FUNC(dev->devfn) != 0 || dev->is_virtfn)
		return;

	 
	__pcie_print_link_status(dev, false);
}

static void pci_init_capabilities(struct pci_dev *dev)
{
	pci_ea_init(dev);		 
	pci_msi_init(dev);		 
	pci_msix_init(dev);		 

	 
	pci_allocate_cap_save_buffers(dev);

	pci_pm_init(dev);		 
	pci_vpd_init(dev);		 
	pci_configure_ari(dev);		 
	pci_iov_init(dev);		 
	pci_ats_init(dev);		 
	pci_pri_init(dev);		 
	pci_pasid_init(dev);		 
	pci_acs_init(dev);		 
	pci_ptm_init(dev);		 
	pci_aer_init(dev);		 
	pci_dpc_init(dev);		 
	pci_rcec_init(dev);		 
	pci_doe_init(dev);		 

	pcie_report_downtraining(dev);
	pci_init_reset_methods(dev);
}

 
static struct irq_domain *pci_dev_msi_domain(struct pci_dev *dev)
{
	struct irq_domain *d;

	 
	d = dev_get_msi_domain(&dev->dev);
	if (d)
		return d;

	 
	d = pci_msi_get_device_domain(dev);
	if (d)
		return d;

	return NULL;
}

static void pci_set_msi_domain(struct pci_dev *dev)
{
	struct irq_domain *d;

	 
	d = pci_dev_msi_domain(dev);
	if (!d)
		d = dev_get_msi_domain(&dev->bus->dev);

	dev_set_msi_domain(&dev->dev, d);
}

void pci_device_add(struct pci_dev *dev, struct pci_bus *bus)
{
	int ret;

	pci_configure_device(dev);

	device_initialize(&dev->dev);
	dev->dev.release = pci_release_dev;

	set_dev_node(&dev->dev, pcibus_to_node(bus));
	dev->dev.dma_mask = &dev->dma_mask;
	dev->dev.dma_parms = &dev->dma_parms;
	dev->dev.coherent_dma_mask = 0xffffffffull;

	dma_set_max_seg_size(&dev->dev, 65536);
	dma_set_seg_boundary(&dev->dev, 0xffffffff);

	pcie_failed_link_retrain(dev);

	 
	pci_fixup_device(pci_fixup_header, dev);

	pci_reassigndev_resource_alignment(dev);

	dev->state_saved = false;

	pci_init_capabilities(dev);

	 
	down_write(&pci_bus_sem);
	list_add_tail(&dev->bus_list, &bus->devices);
	up_write(&pci_bus_sem);

	ret = pcibios_device_add(dev);
	WARN_ON(ret < 0);

	 
	pci_set_msi_domain(dev);

	 
	dev->match_driver = false;
	ret = device_add(&dev->dev);
	WARN_ON(ret < 0);
}

struct pci_dev *pci_scan_single_device(struct pci_bus *bus, int devfn)
{
	struct pci_dev *dev;

	dev = pci_get_slot(bus, devfn);
	if (dev) {
		pci_dev_put(dev);
		return dev;
	}

	dev = pci_scan_device(bus, devfn);
	if (!dev)
		return NULL;

	pci_device_add(dev, bus);

	return dev;
}
EXPORT_SYMBOL(pci_scan_single_device);

static int next_ari_fn(struct pci_bus *bus, struct pci_dev *dev, int fn)
{
	int pos;
	u16 cap = 0;
	unsigned int next_fn;

	if (!dev)
		return -ENODEV;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ARI);
	if (!pos)
		return -ENODEV;

	pci_read_config_word(dev, pos + PCI_ARI_CAP, &cap);
	next_fn = PCI_ARI_CAP_NFN(cap);
	if (next_fn <= fn)
		return -ENODEV;	 

	return next_fn;
}

static int next_fn(struct pci_bus *bus, struct pci_dev *dev, int fn)
{
	if (pci_ari_enabled(bus))
		return next_ari_fn(bus, dev, fn);

	if (fn >= 7)
		return -ENODEV;
	 
	if (dev && !dev->multifunction)
		return -ENODEV;

	return fn + 1;
}

static int only_one_child(struct pci_bus *bus)
{
	struct pci_dev *bridge = bus->self;

	 
	if (pci_has_flag(PCI_SCAN_ALL_PCIE_DEVS))
		return 0;

	 
	if (bridge && pci_is_pcie(bridge) && pcie_downstream_port(bridge))
		return 1;

	return 0;
}

 
int pci_scan_slot(struct pci_bus *bus, int devfn)
{
	struct pci_dev *dev;
	int fn = 0, nr = 0;

	if (only_one_child(bus) && (devfn > 0))
		return 0;  

	do {
		dev = pci_scan_single_device(bus, devfn + fn);
		if (dev) {
			if (!pci_dev_is_added(dev))
				nr++;
			if (fn > 0)
				dev->multifunction = 1;
		} else if (fn == 0) {
			 
			if (!hypervisor_isolated_pci_functions())
				break;
		}
		fn = next_fn(bus, dev, fn);
	} while (fn >= 0);

	 
	if (bus->self && nr)
		pcie_aspm_init_link_state(bus->self);

	return nr;
}
EXPORT_SYMBOL(pci_scan_slot);

static int pcie_find_smpss(struct pci_dev *dev, void *data)
{
	u8 *smpss = data;

	if (!pci_is_pcie(dev))
		return 0;

	 
	if (dev->is_hotplug_bridge &&
	    pci_pcie_type(dev) != PCI_EXP_TYPE_ROOT_PORT)
		*smpss = 0;

	if (*smpss > dev->pcie_mpss)
		*smpss = dev->pcie_mpss;

	return 0;
}

static void pcie_write_mps(struct pci_dev *dev, int mps)
{
	int rc;

	if (pcie_bus_config == PCIE_BUS_PERFORMANCE) {
		mps = 128 << dev->pcie_mpss;

		if (pci_pcie_type(dev) != PCI_EXP_TYPE_ROOT_PORT &&
		    dev->bus->self)

			 
			mps = min(mps, pcie_get_mps(dev->bus->self));
	}

	rc = pcie_set_mps(dev, mps);
	if (rc)
		pci_err(dev, "Failed attempting to set the MPS\n");
}

static void pcie_write_mrrs(struct pci_dev *dev)
{
	int rc, mrrs;

	 
	if (pcie_bus_config != PCIE_BUS_PERFORMANCE)
		return;

	 
	mrrs = pcie_get_mps(dev);

	 
	while (mrrs != pcie_get_readrq(dev) && mrrs >= 128) {
		rc = pcie_set_readrq(dev, mrrs);
		if (!rc)
			break;

		pci_warn(dev, "Failed attempting to set the MRRS\n");
		mrrs /= 2;
	}

	if (mrrs < 128)
		pci_err(dev, "MRRS was unable to be configured with a safe value.  If problems are experienced, try running with pci=pcie_bus_safe\n");
}

static int pcie_bus_configure_set(struct pci_dev *dev, void *data)
{
	int mps, orig_mps;

	if (!pci_is_pcie(dev))
		return 0;

	if (pcie_bus_config == PCIE_BUS_TUNE_OFF ||
	    pcie_bus_config == PCIE_BUS_DEFAULT)
		return 0;

	mps = 128 << *(u8 *)data;
	orig_mps = pcie_get_mps(dev);

	pcie_write_mps(dev, mps);
	pcie_write_mrrs(dev);

	pci_info(dev, "Max Payload Size set to %4d/%4d (was %4d), Max Read Rq %4d\n",
		 pcie_get_mps(dev), 128 << dev->pcie_mpss,
		 orig_mps, pcie_get_readrq(dev));

	return 0;
}

 
void pcie_bus_configure_settings(struct pci_bus *bus)
{
	u8 smpss = 0;

	if (!bus->self)
		return;

	if (!pci_is_pcie(bus->self))
		return;

	 
	if (pcie_bus_config == PCIE_BUS_PEER2PEER)
		smpss = 0;

	if (pcie_bus_config == PCIE_BUS_SAFE) {
		smpss = bus->self->pcie_mpss;

		pcie_find_smpss(bus->self, &smpss);
		pci_walk_bus(bus, pcie_find_smpss, &smpss);
	}

	pcie_bus_configure_set(bus->self, &smpss);
	pci_walk_bus(bus, pcie_bus_configure_set, &smpss);
}
EXPORT_SYMBOL_GPL(pcie_bus_configure_settings);

 
void __weak pcibios_fixup_bus(struct pci_bus *bus)
{
        
}

 
static unsigned int pci_scan_child_bus_extend(struct pci_bus *bus,
					      unsigned int available_buses)
{
	unsigned int used_buses, normal_bridges = 0, hotplug_bridges = 0;
	unsigned int start = bus->busn_res.start;
	unsigned int devfn, cmax, max = start;
	struct pci_dev *dev;

	dev_dbg(&bus->dev, "scanning bus\n");

	 
	for (devfn = 0; devfn < 256; devfn += 8)
		pci_scan_slot(bus, devfn);

	 
	used_buses = pci_iov_bus_range(bus);
	max += used_buses;

	 
	if (!bus->is_added) {
		dev_dbg(&bus->dev, "fixups for bus\n");
		pcibios_fixup_bus(bus);
		bus->is_added = 1;
	}

	 
	for_each_pci_bridge(dev, bus) {
		if (dev->is_hotplug_bridge)
			hotplug_bridges++;
		else
			normal_bridges++;
	}

	 
	for_each_pci_bridge(dev, bus) {
		cmax = max;
		max = pci_scan_bridge_extend(bus, dev, max, 0, 0);

		 
		used_buses++;
		if (max - cmax > 1)
			used_buses += max - cmax - 1;
	}

	 
	for_each_pci_bridge(dev, bus) {
		unsigned int buses = 0;

		if (!hotplug_bridges && normal_bridges == 1) {
			 
			buses = available_buses;
		} else if (dev->is_hotplug_bridge) {
			 
			buses = available_buses / hotplug_bridges;
			buses = min(buses, available_buses - used_buses + 1);
		}

		cmax = max;
		max = pci_scan_bridge_extend(bus, dev, cmax, buses, 1);
		 
		if (max - cmax > 1)
			used_buses += max - cmax - 1;
	}

	 
	if (bus->self && bus->self->is_hotplug_bridge) {
		used_buses = max_t(unsigned int, available_buses,
				   pci_hotplug_bus_size - 1);
		if (max - start < used_buses) {
			max = start + used_buses;

			 
			if (max > bus->busn_res.end)
				max = bus->busn_res.end;

			dev_dbg(&bus->dev, "%pR extended by %#02x\n",
				&bus->busn_res, max - start);
		}
	}

	 
	dev_dbg(&bus->dev, "bus scan returning with max=%02x\n", max);
	return max;
}

 
unsigned int pci_scan_child_bus(struct pci_bus *bus)
{
	return pci_scan_child_bus_extend(bus, 0);
}
EXPORT_SYMBOL_GPL(pci_scan_child_bus);

 
int __weak pcibios_root_bridge_prepare(struct pci_host_bridge *bridge)
{
	return 0;
}

void __weak pcibios_add_bus(struct pci_bus *bus)
{
}

void __weak pcibios_remove_bus(struct pci_bus *bus)
{
}

struct pci_bus *pci_create_root_bus(struct device *parent, int bus,
		struct pci_ops *ops, void *sysdata, struct list_head *resources)
{
	int error;
	struct pci_host_bridge *bridge;

	bridge = pci_alloc_host_bridge(0);
	if (!bridge)
		return NULL;

	bridge->dev.parent = parent;

	list_splice_init(resources, &bridge->windows);
	bridge->sysdata = sysdata;
	bridge->busnr = bus;
	bridge->ops = ops;

	error = pci_register_host_bridge(bridge);
	if (error < 0)
		goto err_out;

	return bridge->bus;

err_out:
	put_device(&bridge->dev);
	return NULL;
}
EXPORT_SYMBOL_GPL(pci_create_root_bus);

int pci_host_probe(struct pci_host_bridge *bridge)
{
	struct pci_bus *bus, *child;
	int ret;

	ret = pci_scan_root_bus_bridge(bridge);
	if (ret < 0) {
		dev_err(bridge->dev.parent, "Scanning root bridge failed");
		return ret;
	}

	bus = bridge->bus;

	 
	if (pci_has_flag(PCI_PROBE_ONLY)) {
		pci_bus_claim_resources(bus);
	} else {
		pci_bus_size_bridges(bus);
		pci_bus_assign_resources(bus);

		list_for_each_entry(child, &bus->children, node)
			pcie_bus_configure_settings(child);
	}

	pci_bus_add_devices(bus);
	return 0;
}
EXPORT_SYMBOL_GPL(pci_host_probe);

int pci_bus_insert_busn_res(struct pci_bus *b, int bus, int bus_max)
{
	struct resource *res = &b->busn_res;
	struct resource *parent_res, *conflict;

	res->start = bus;
	res->end = bus_max;
	res->flags = IORESOURCE_BUS;

	if (!pci_is_root_bus(b))
		parent_res = &b->parent->busn_res;
	else {
		parent_res = get_pci_domain_busn_res(pci_domain_nr(b));
		res->flags |= IORESOURCE_PCI_FIXED;
	}

	conflict = request_resource_conflict(parent_res, res);

	if (conflict)
		dev_info(&b->dev,
			   "busn_res: can not insert %pR under %s%pR (conflicts with %s %pR)\n",
			    res, pci_is_root_bus(b) ? "domain " : "",
			    parent_res, conflict->name, conflict);

	return conflict == NULL;
}

int pci_bus_update_busn_res_end(struct pci_bus *b, int bus_max)
{
	struct resource *res = &b->busn_res;
	struct resource old_res = *res;
	resource_size_t size;
	int ret;

	if (res->start > bus_max)
		return -EINVAL;

	size = bus_max - res->start + 1;
	ret = adjust_resource(res, res->start, size);
	dev_info(&b->dev, "busn_res: %pR end %s updated to %02x\n",
			&old_res, ret ? "can not be" : "is", bus_max);

	if (!ret && !res->parent)
		pci_bus_insert_busn_res(b, res->start, res->end);

	return ret;
}

void pci_bus_release_busn_res(struct pci_bus *b)
{
	struct resource *res = &b->busn_res;
	int ret;

	if (!res->flags || !res->parent)
		return;

	ret = release_resource(res);
	dev_info(&b->dev, "busn_res: %pR %s released\n",
			res, ret ? "can not be" : "is");
}

int pci_scan_root_bus_bridge(struct pci_host_bridge *bridge)
{
	struct resource_entry *window;
	bool found = false;
	struct pci_bus *b;
	int max, bus, ret;

	if (!bridge)
		return -EINVAL;

	resource_list_for_each_entry(window, &bridge->windows)
		if (window->res->flags & IORESOURCE_BUS) {
			bridge->busnr = window->res->start;
			found = true;
			break;
		}

	ret = pci_register_host_bridge(bridge);
	if (ret < 0)
		return ret;

	b = bridge->bus;
	bus = bridge->busnr;

	if (!found) {
		dev_info(&b->dev,
		 "No busn resource found for root bus, will use [bus %02x-ff]\n",
			bus);
		pci_bus_insert_busn_res(b, bus, 255);
	}

	max = pci_scan_child_bus(b);

	if (!found)
		pci_bus_update_busn_res_end(b, max);

	return 0;
}
EXPORT_SYMBOL(pci_scan_root_bus_bridge);

struct pci_bus *pci_scan_root_bus(struct device *parent, int bus,
		struct pci_ops *ops, void *sysdata, struct list_head *resources)
{
	struct resource_entry *window;
	bool found = false;
	struct pci_bus *b;
	int max;

	resource_list_for_each_entry(window, resources)
		if (window->res->flags & IORESOURCE_BUS) {
			found = true;
			break;
		}

	b = pci_create_root_bus(parent, bus, ops, sysdata, resources);
	if (!b)
		return NULL;

	if (!found) {
		dev_info(&b->dev,
		 "No busn resource found for root bus, will use [bus %02x-ff]\n",
			bus);
		pci_bus_insert_busn_res(b, bus, 255);
	}

	max = pci_scan_child_bus(b);

	if (!found)
		pci_bus_update_busn_res_end(b, max);

	return b;
}
EXPORT_SYMBOL(pci_scan_root_bus);

struct pci_bus *pci_scan_bus(int bus, struct pci_ops *ops,
					void *sysdata)
{
	LIST_HEAD(resources);
	struct pci_bus *b;

	pci_add_resource(&resources, &ioport_resource);
	pci_add_resource(&resources, &iomem_resource);
	pci_add_resource(&resources, &busn_resource);
	b = pci_create_root_bus(NULL, bus, ops, sysdata, &resources);
	if (b) {
		pci_scan_child_bus(b);
	} else {
		pci_free_resource_list(&resources);
	}
	return b;
}
EXPORT_SYMBOL(pci_scan_bus);

 
unsigned int pci_rescan_bus_bridge_resize(struct pci_dev *bridge)
{
	unsigned int max;
	struct pci_bus *bus = bridge->subordinate;

	max = pci_scan_child_bus(bus);

	pci_assign_unassigned_bridge_resources(bridge);

	pci_bus_add_devices(bus);

	return max;
}

 
unsigned int pci_rescan_bus(struct pci_bus *bus)
{
	unsigned int max;

	max = pci_scan_child_bus(bus);
	pci_assign_unassigned_bus_resources(bus);
	pci_bus_add_devices(bus);

	return max;
}
EXPORT_SYMBOL_GPL(pci_rescan_bus);

 
static DEFINE_MUTEX(pci_rescan_remove_lock);

void pci_lock_rescan_remove(void)
{
	mutex_lock(&pci_rescan_remove_lock);
}
EXPORT_SYMBOL_GPL(pci_lock_rescan_remove);

void pci_unlock_rescan_remove(void)
{
	mutex_unlock(&pci_rescan_remove_lock);
}
EXPORT_SYMBOL_GPL(pci_unlock_rescan_remove);

static int __init pci_sort_bf_cmp(const struct device *d_a,
				  const struct device *d_b)
{
	const struct pci_dev *a = to_pci_dev(d_a);
	const struct pci_dev *b = to_pci_dev(d_b);

	if      (pci_domain_nr(a->bus) < pci_domain_nr(b->bus)) return -1;
	else if (pci_domain_nr(a->bus) > pci_domain_nr(b->bus)) return  1;

	if      (a->bus->number < b->bus->number) return -1;
	else if (a->bus->number > b->bus->number) return  1;

	if      (a->devfn < b->devfn) return -1;
	else if (a->devfn > b->devfn) return  1;

	return 0;
}

void __init pci_sort_breadthfirst(void)
{
	bus_sort_breadthfirst(&pci_bus_type, &pci_sort_bf_cmp);
}

int pci_hp_add_bridge(struct pci_dev *dev)
{
	struct pci_bus *parent = dev->bus;
	int busnr, start = parent->busn_res.start;
	unsigned int available_buses = 0;
	int end = parent->busn_res.end;

	for (busnr = start; busnr <= end; busnr++) {
		if (!pci_find_bus(pci_domain_nr(parent), busnr))
			break;
	}
	if (busnr-- > end) {
		pci_err(dev, "No bus number available for hot-added bridge\n");
		return -1;
	}

	 
	busnr = pci_scan_bridge(parent, dev, busnr, 0);

	 
	available_buses = end - busnr;

	 
	pci_scan_bridge_extend(parent, dev, busnr, available_buses, 1);

	if (!dev->subordinate)
		return -1;

	return 0;
}
EXPORT_SYMBOL_GPL(pci_hp_add_bridge);
