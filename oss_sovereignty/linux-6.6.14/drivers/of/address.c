
#define pr_fmt(fmt)	"OF: " fmt

#include <linux/device.h>
#include <linux/fwnode.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/logic_pio.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/dma-direct.h>  

#include "of_private.h"

 
#define OF_MAX_ADDR_CELLS	4
#define OF_CHECK_ADDR_COUNT(na)	((na) > 0 && (na) <= OF_MAX_ADDR_CELLS)
#define OF_CHECK_COUNTS(na, ns)	(OF_CHECK_ADDR_COUNT(na) && (ns) > 0)

 
#ifdef DEBUG
static void of_dump_addr(const char *s, const __be32 *addr, int na)
{
	pr_debug("%s", s);
	while (na--)
		pr_cont(" %08x", be32_to_cpu(*(addr++)));
	pr_cont("\n");
}
#else
static void of_dump_addr(const char *s, const __be32 *addr, int na) { }
#endif

 
struct of_bus {
	const char	*name;
	const char	*addresses;
	int		(*match)(struct device_node *parent);
	void		(*count_cells)(struct device_node *child,
				       int *addrc, int *sizec);
	u64		(*map)(__be32 *addr, const __be32 *range,
				int na, int ns, int pna);
	int		(*translate)(__be32 *addr, u64 offset, int na);
	bool	has_flags;
	unsigned int	(*get_flags)(const __be32 *addr);
};

 

static void of_bus_default_count_cells(struct device_node *dev,
				       int *addrc, int *sizec)
{
	if (addrc)
		*addrc = of_n_addr_cells(dev);
	if (sizec)
		*sizec = of_n_size_cells(dev);
}

static u64 of_bus_default_map(__be32 *addr, const __be32 *range,
		int na, int ns, int pna)
{
	u64 cp, s, da;

	cp = of_read_number(range, na);
	s  = of_read_number(range + na + pna, ns);
	da = of_read_number(addr, na);

	pr_debug("default map, cp=%llx, s=%llx, da=%llx\n", cp, s, da);

	if (da < cp || da >= (cp + s))
		return OF_BAD_ADDR;
	return da - cp;
}

static int of_bus_default_translate(__be32 *addr, u64 offset, int na)
{
	u64 a = of_read_number(addr, na);
	memset(addr, 0, na * 4);
	a += offset;
	if (na > 1)
		addr[na - 2] = cpu_to_be32(a >> 32);
	addr[na - 1] = cpu_to_be32(a & 0xffffffffu);

	return 0;
}

static unsigned int of_bus_default_flags_get_flags(const __be32 *addr)
{
	return of_read_number(addr, 1);
}

static unsigned int of_bus_default_get_flags(const __be32 *addr)
{
	return IORESOURCE_MEM;
}

static u64 of_bus_default_flags_map(__be32 *addr, const __be32 *range, int na,
				    int ns, int pna)
{
	u64 cp, s, da;

	 
	if (*addr != *range)
		return OF_BAD_ADDR;

	 
	cp = of_read_number(range + 1, na - 1);
	s  = of_read_number(range + na + pna, ns);
	da = of_read_number(addr + 1, na - 1);

	pr_debug("default flags map, cp=%llx, s=%llx, da=%llx\n", cp, s, da);

	if (da < cp || da >= (cp + s))
		return OF_BAD_ADDR;
	return da - cp;
}

static int of_bus_default_flags_translate(__be32 *addr, u64 offset, int na)
{
	 
	return of_bus_default_translate(addr + 1, offset, na - 1);
}

#ifdef CONFIG_PCI
static unsigned int of_bus_pci_get_flags(const __be32 *addr)
{
	unsigned int flags = 0;
	u32 w = be32_to_cpup(addr);

	if (!IS_ENABLED(CONFIG_PCI))
		return 0;

	switch((w >> 24) & 0x03) {
	case 0x01:
		flags |= IORESOURCE_IO;
		break;
	case 0x02:  
		flags |= IORESOURCE_MEM;
		break;

	case 0x03:  
		flags |= IORESOURCE_MEM | IORESOURCE_MEM_64;
		break;
	}
	if (w & 0x40000000)
		flags |= IORESOURCE_PREFETCH;
	return flags;
}

 

static bool of_node_is_pcie(struct device_node *np)
{
	bool is_pcie = of_node_name_eq(np, "pcie");

	if (is_pcie)
		pr_warn_once("%pOF: Missing device_type\n", np);

	return is_pcie;
}

static int of_bus_pci_match(struct device_node *np)
{
	 
	return of_node_is_type(np, "pci") || of_node_is_type(np, "pciex") ||
		of_node_is_type(np, "vci") || of_node_is_type(np, "ht") ||
		of_node_is_pcie(np);
}

static void of_bus_pci_count_cells(struct device_node *np,
				   int *addrc, int *sizec)
{
	if (addrc)
		*addrc = 3;
	if (sizec)
		*sizec = 2;
}

static u64 of_bus_pci_map(__be32 *addr, const __be32 *range, int na, int ns,
		int pna)
{
	u64 cp, s, da;
	unsigned int af, rf;

	af = of_bus_pci_get_flags(addr);
	rf = of_bus_pci_get_flags(range);

	 
	if ((af ^ rf) & (IORESOURCE_MEM | IORESOURCE_IO))
		return OF_BAD_ADDR;

	 
	cp = of_read_number(range + 1, na - 1);
	s  = of_read_number(range + na + pna, ns);
	da = of_read_number(addr + 1, na - 1);

	pr_debug("PCI map, cp=%llx, s=%llx, da=%llx\n", cp, s, da);

	if (da < cp || da >= (cp + s))
		return OF_BAD_ADDR;
	return da - cp;
}

static int of_bus_pci_translate(__be32 *addr, u64 offset, int na)
{
	return of_bus_default_translate(addr + 1, offset, na - 1);
}
#endif  

 
int of_pci_range_to_resource(struct of_pci_range *range,
			     struct device_node *np, struct resource *res)
{
	int err;
	res->flags = range->flags;
	res->parent = res->child = res->sibling = NULL;
	res->name = np->full_name;

	if (res->flags & IORESOURCE_IO) {
		unsigned long port;
		err = pci_register_io_range(&np->fwnode, range->cpu_addr,
				range->size);
		if (err)
			goto invalid_range;
		port = pci_address_to_pio(range->cpu_addr);
		if (port == (unsigned long)-1) {
			err = -EINVAL;
			goto invalid_range;
		}
		res->start = port;
	} else {
		if ((sizeof(resource_size_t) < 8) &&
		    upper_32_bits(range->cpu_addr)) {
			err = -EINVAL;
			goto invalid_range;
		}

		res->start = range->cpu_addr;
	}
	res->end = res->start + range->size - 1;
	return 0;

invalid_range:
	res->start = (resource_size_t)OF_BAD_ADDR;
	res->end = (resource_size_t)OF_BAD_ADDR;
	return err;
}
EXPORT_SYMBOL(of_pci_range_to_resource);

 
int of_range_to_resource(struct device_node *np, int index, struct resource *res)
{
	int ret, i = 0;
	struct of_range_parser parser;
	struct of_range range;

	ret = of_range_parser_init(&parser, np);
	if (ret)
		return ret;

	for_each_of_range(&parser, &range)
		if (i++ == index)
			return of_pci_range_to_resource(&range, np, res);

	return -ENOENT;
}
EXPORT_SYMBOL(of_range_to_resource);

 

static int of_bus_isa_match(struct device_node *np)
{
	return of_node_name_eq(np, "isa");
}

static void of_bus_isa_count_cells(struct device_node *child,
				   int *addrc, int *sizec)
{
	if (addrc)
		*addrc = 2;
	if (sizec)
		*sizec = 1;
}

static u64 of_bus_isa_map(__be32 *addr, const __be32 *range, int na, int ns,
		int pna)
{
	u64 cp, s, da;

	 
	if ((addr[0] ^ range[0]) & cpu_to_be32(1))
		return OF_BAD_ADDR;

	 
	cp = of_read_number(range + 1, na - 1);
	s  = of_read_number(range + na + pna, ns);
	da = of_read_number(addr + 1, na - 1);

	pr_debug("ISA map, cp=%llx, s=%llx, da=%llx\n", cp, s, da);

	if (da < cp || da >= (cp + s))
		return OF_BAD_ADDR;
	return da - cp;
}

static int of_bus_isa_translate(__be32 *addr, u64 offset, int na)
{
	return of_bus_default_translate(addr + 1, offset, na - 1);
}

static unsigned int of_bus_isa_get_flags(const __be32 *addr)
{
	unsigned int flags = 0;
	u32 w = be32_to_cpup(addr);

	if (w & 1)
		flags |= IORESOURCE_IO;
	else
		flags |= IORESOURCE_MEM;
	return flags;
}

static int of_bus_default_flags_match(struct device_node *np)
{
	return of_bus_n_addr_cells(np) == 3;
}

 

static struct of_bus of_busses[] = {
#ifdef CONFIG_PCI
	 
	{
		.name = "pci",
		.addresses = "assigned-addresses",
		.match = of_bus_pci_match,
		.count_cells = of_bus_pci_count_cells,
		.map = of_bus_pci_map,
		.translate = of_bus_pci_translate,
		.has_flags = true,
		.get_flags = of_bus_pci_get_flags,
	},
#endif  
	 
	{
		.name = "isa",
		.addresses = "reg",
		.match = of_bus_isa_match,
		.count_cells = of_bus_isa_count_cells,
		.map = of_bus_isa_map,
		.translate = of_bus_isa_translate,
		.has_flags = true,
		.get_flags = of_bus_isa_get_flags,
	},
	 
	{
		.name = "default-flags",
		.addresses = "reg",
		.match = of_bus_default_flags_match,
		.count_cells = of_bus_default_count_cells,
		.map = of_bus_default_flags_map,
		.translate = of_bus_default_flags_translate,
		.has_flags = true,
		.get_flags = of_bus_default_flags_get_flags,
	},
	 
	{
		.name = "default",
		.addresses = "reg",
		.match = NULL,
		.count_cells = of_bus_default_count_cells,
		.map = of_bus_default_map,
		.translate = of_bus_default_translate,
		.get_flags = of_bus_default_get_flags,
	},
};

static struct of_bus *of_match_bus(struct device_node *np)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(of_busses); i++)
		if (!of_busses[i].match || of_busses[i].match(np))
			return &of_busses[i];
	BUG();
	return NULL;
}

static int of_empty_ranges_quirk(struct device_node *np)
{
	if (IS_ENABLED(CONFIG_PPC)) {
		 
		static int quirk_state = -1;

		 
		if (of_device_is_compatible(np, "1682m-sdc"))
			return true;

		 
		if (quirk_state < 0)
			quirk_state =
				of_machine_is_compatible("Power Macintosh") ||
				of_machine_is_compatible("MacRISC");
		return quirk_state;
	}
	return false;
}

static int of_translate_one(struct device_node *parent, struct of_bus *bus,
			    struct of_bus *pbus, __be32 *addr,
			    int na, int ns, int pna, const char *rprop)
{
	const __be32 *ranges;
	unsigned int rlen;
	int rone;
	u64 offset = OF_BAD_ADDR;

	 
	ranges = of_get_property(parent, rprop, &rlen);
	if (ranges == NULL && !of_empty_ranges_quirk(parent) &&
	    strcmp(rprop, "dma-ranges")) {
		pr_debug("no ranges; cannot translate\n");
		return 1;
	}
	if (ranges == NULL || rlen == 0) {
		offset = of_read_number(addr, na);
		memset(addr, 0, pna * 4);
		pr_debug("empty ranges; 1:1 translation\n");
		goto finish;
	}

	pr_debug("walking ranges...\n");

	 
	rlen /= 4;
	rone = na + pna + ns;
	for (; rlen >= rone; rlen -= rone, ranges += rone) {
		offset = bus->map(addr, ranges, na, ns, pna);
		if (offset != OF_BAD_ADDR)
			break;
	}
	if (offset == OF_BAD_ADDR) {
		pr_debug("not found !\n");
		return 1;
	}
	memcpy(addr, ranges + na, 4 * pna);

 finish:
	of_dump_addr("parent translation for:", addr, pna);
	pr_debug("with offset: %llx\n", offset);

	 
	return pbus->translate(addr, offset, pna);
}

 
static u64 __of_translate_address(struct device_node *dev,
				  struct device_node *(*get_parent)(const struct device_node *),
				  const __be32 *in_addr, const char *rprop,
				  struct device_node **host)
{
	struct device_node *parent = NULL;
	struct of_bus *bus, *pbus;
	__be32 addr[OF_MAX_ADDR_CELLS];
	int na, ns, pna, pns;
	u64 result = OF_BAD_ADDR;

	pr_debug("** translation for device %pOF **\n", dev);

	 
	of_node_get(dev);

	*host = NULL;
	 
	parent = get_parent(dev);
	if (parent == NULL)
		goto bail;
	bus = of_match_bus(parent);

	 
	bus->count_cells(dev, &na, &ns);
	if (!OF_CHECK_COUNTS(na, ns)) {
		pr_debug("Bad cell count for %pOF\n", dev);
		goto bail;
	}
	memcpy(addr, in_addr, na * 4);

	pr_debug("bus is %s (na=%d, ns=%d) on %pOF\n",
	    bus->name, na, ns, parent);
	of_dump_addr("translating address:", addr, na);

	 
	for (;;) {
		struct logic_pio_hwaddr *iorange;

		 
		of_node_put(dev);
		dev = parent;
		parent = get_parent(dev);

		 
		if (parent == NULL) {
			pr_debug("reached root node\n");
			result = of_read_number(addr, na);
			break;
		}

		 
		iorange = find_io_range_by_fwnode(&dev->fwnode);
		if (iorange && (iorange->flags != LOGIC_PIO_CPU_MMIO)) {
			result = of_read_number(addr + 1, na - 1);
			pr_debug("indirectIO matched(%pOF) 0x%llx\n",
				 dev, result);
			*host = of_node_get(dev);
			break;
		}

		 
		pbus = of_match_bus(parent);
		pbus->count_cells(dev, &pna, &pns);
		if (!OF_CHECK_COUNTS(pna, pns)) {
			pr_err("Bad cell count for %pOF\n", dev);
			break;
		}

		pr_debug("parent bus is %s (na=%d, ns=%d) on %pOF\n",
		    pbus->name, pna, pns, parent);

		 
		if (of_translate_one(dev, bus, pbus, addr, na, ns, pna, rprop))
			break;

		 
		na = pna;
		ns = pns;
		bus = pbus;

		of_dump_addr("one level translation:", addr, na);
	}
 bail:
	of_node_put(parent);
	of_node_put(dev);

	return result;
}

u64 of_translate_address(struct device_node *dev, const __be32 *in_addr)
{
	struct device_node *host;
	u64 ret;

	ret = __of_translate_address(dev, of_get_parent,
				     in_addr, "ranges", &host);
	if (host) {
		of_node_put(host);
		return OF_BAD_ADDR;
	}

	return ret;
}
EXPORT_SYMBOL(of_translate_address);

#ifdef CONFIG_HAS_DMA
struct device_node *__of_get_dma_parent(const struct device_node *np)
{
	struct of_phandle_args args;
	int ret, index;

	index = of_property_match_string(np, "interconnect-names", "dma-mem");
	if (index < 0)
		return of_get_parent(np);

	ret = of_parse_phandle_with_args(np, "interconnects",
					 "#interconnect-cells",
					 index, &args);
	if (ret < 0)
		return of_get_parent(np);

	return of_node_get(args.np);
}
#endif

static struct device_node *of_get_next_dma_parent(struct device_node *np)
{
	struct device_node *parent;

	parent = __of_get_dma_parent(np);
	of_node_put(np);

	return parent;
}

u64 of_translate_dma_address(struct device_node *dev, const __be32 *in_addr)
{
	struct device_node *host;
	u64 ret;

	ret = __of_translate_address(dev, __of_get_dma_parent,
				     in_addr, "dma-ranges", &host);

	if (host) {
		of_node_put(host);
		return OF_BAD_ADDR;
	}

	return ret;
}
EXPORT_SYMBOL(of_translate_dma_address);

 
const __be32 *of_translate_dma_region(struct device_node *dev, const __be32 *prop,
				      phys_addr_t *start, size_t *length)
{
	struct device_node *parent;
	u64 address, size;
	int na, ns;

	parent = __of_get_dma_parent(dev);
	if (!parent)
		return NULL;

	na = of_bus_n_addr_cells(parent);
	ns = of_bus_n_size_cells(parent);

	of_node_put(parent);

	address = of_translate_dma_address(dev, prop);
	if (address == OF_BAD_ADDR)
		return NULL;

	size = of_read_number(prop + na, ns);

	if (start)
		*start = address;

	if (length)
		*length = size;

	return prop + na + ns;
}
EXPORT_SYMBOL(of_translate_dma_region);

const __be32 *__of_get_address(struct device_node *dev, int index, int bar_no,
			       u64 *size, unsigned int *flags)
{
	const __be32 *prop;
	unsigned int psize;
	struct device_node *parent;
	struct of_bus *bus;
	int onesize, i, na, ns;

	 
	parent = of_get_parent(dev);
	if (parent == NULL)
		return NULL;
	bus = of_match_bus(parent);
	if (strcmp(bus->name, "pci") && (bar_no >= 0)) {
		of_node_put(parent);
		return NULL;
	}
	bus->count_cells(dev, &na, &ns);
	of_node_put(parent);
	if (!OF_CHECK_ADDR_COUNT(na))
		return NULL;

	 
	prop = of_get_property(dev, bus->addresses, &psize);
	if (prop == NULL)
		return NULL;
	psize /= 4;

	onesize = na + ns;
	for (i = 0; psize >= onesize; psize -= onesize, prop += onesize, i++) {
		u32 val = be32_to_cpu(prop[0]);
		 
		if (((bar_no >= 0) && ((val & 0xff) == ((bar_no * 4) + PCI_BASE_ADDRESS_0))) ||
		    ((index >= 0) && (i == index))) {
			if (size)
				*size = of_read_number(prop + na, ns);
			if (flags)
				*flags = bus->get_flags(prop);
			return prop;
		}
	}
	return NULL;
}
EXPORT_SYMBOL(__of_get_address);

 
int of_property_read_reg(struct device_node *np, int idx, u64 *addr, u64 *size)
{
	const __be32 *prop = of_get_address(np, idx, size, NULL);

	if (!prop)
		return -EINVAL;

	*addr = of_read_number(prop, of_n_addr_cells(np));

	return 0;
}
EXPORT_SYMBOL(of_property_read_reg);

static int parser_init(struct of_pci_range_parser *parser,
			struct device_node *node, const char *name)
{
	int rlen;

	parser->node = node;
	parser->pna = of_n_addr_cells(node);
	parser->na = of_bus_n_addr_cells(node);
	parser->ns = of_bus_n_size_cells(node);
	parser->dma = !strcmp(name, "dma-ranges");
	parser->bus = of_match_bus(node);

	parser->range = of_get_property(node, name, &rlen);
	if (parser->range == NULL)
		return -ENOENT;

	parser->end = parser->range + rlen / sizeof(__be32);

	return 0;
}

int of_pci_range_parser_init(struct of_pci_range_parser *parser,
				struct device_node *node)
{
	return parser_init(parser, node, "ranges");
}
EXPORT_SYMBOL_GPL(of_pci_range_parser_init);

int of_pci_dma_range_parser_init(struct of_pci_range_parser *parser,
				struct device_node *node)
{
	return parser_init(parser, node, "dma-ranges");
}
EXPORT_SYMBOL_GPL(of_pci_dma_range_parser_init);
#define of_dma_range_parser_init of_pci_dma_range_parser_init

struct of_pci_range *of_pci_range_parser_one(struct of_pci_range_parser *parser,
						struct of_pci_range *range)
{
	int na = parser->na;
	int ns = parser->ns;
	int np = parser->pna + na + ns;
	int busflag_na = 0;

	if (!range)
		return NULL;

	if (!parser->range || parser->range + np > parser->end)
		return NULL;

	range->flags = parser->bus->get_flags(parser->range);

	 
	if (parser->bus->has_flags)
		busflag_na = 1;

	range->bus_addr = of_read_number(parser->range + busflag_na, na - busflag_na);

	if (parser->dma)
		range->cpu_addr = of_translate_dma_address(parser->node,
				parser->range + na);
	else
		range->cpu_addr = of_translate_address(parser->node,
				parser->range + na);
	range->size = of_read_number(parser->range + parser->pna + na, ns);

	parser->range += np;

	 
	while (parser->range + np <= parser->end) {
		u32 flags = 0;
		u64 bus_addr, cpu_addr, size;

		flags = parser->bus->get_flags(parser->range);
		bus_addr = of_read_number(parser->range + busflag_na, na - busflag_na);
		if (parser->dma)
			cpu_addr = of_translate_dma_address(parser->node,
					parser->range + na);
		else
			cpu_addr = of_translate_address(parser->node,
					parser->range + na);
		size = of_read_number(parser->range + parser->pna + na, ns);

		if (flags != range->flags)
			break;
		if (bus_addr != range->bus_addr + range->size ||
		    cpu_addr != range->cpu_addr + range->size)
			break;

		range->size += size;
		parser->range += np;
	}

	return range;
}
EXPORT_SYMBOL_GPL(of_pci_range_parser_one);

static u64 of_translate_ioport(struct device_node *dev, const __be32 *in_addr,
			u64 size)
{
	u64 taddr;
	unsigned long port;
	struct device_node *host;

	taddr = __of_translate_address(dev, of_get_parent,
				       in_addr, "ranges", &host);
	if (host) {
		 
		port = logic_pio_trans_hwaddr(&host->fwnode, taddr, size);
		of_node_put(host);
	} else {
		 
		port = pci_address_to_pio(taddr);
	}

	if (port == (unsigned long)-1)
		return OF_BAD_ADDR;

	return port;
}

#ifdef CONFIG_HAS_DMA
 
int of_dma_get_range(struct device_node *np, const struct bus_dma_region **map)
{
	struct device_node *node = of_node_get(np);
	const __be32 *ranges = NULL;
	bool found_dma_ranges = false;
	struct of_range_parser parser;
	struct of_range range;
	struct bus_dma_region *r;
	int len, num_ranges = 0;
	int ret = 0;

	while (node) {
		ranges = of_get_property(node, "dma-ranges", &len);

		 
		if (ranges && len > 0)
			break;

		 
		if (found_dma_ranges && !ranges) {
			ret = -ENODEV;
			goto out;
		}
		found_dma_ranges = true;

		node = of_get_next_dma_parent(node);
	}

	if (!node || !ranges) {
		pr_debug("no dma-ranges found for node(%pOF)\n", np);
		ret = -ENODEV;
		goto out;
	}

	of_dma_range_parser_init(&parser, node);
	for_each_of_range(&parser, &range) {
		if (range.cpu_addr == OF_BAD_ADDR) {
			pr_err("translation of DMA address(%llx) to CPU address failed node(%pOF)\n",
			       range.bus_addr, node);
			continue;
		}
		num_ranges++;
	}

	if (!num_ranges) {
		ret = -EINVAL;
		goto out;
	}

	r = kcalloc(num_ranges + 1, sizeof(*r), GFP_KERNEL);
	if (!r) {
		ret = -ENOMEM;
		goto out;
	}

	 
	*map = r;
	of_dma_range_parser_init(&parser, node);
	for_each_of_range(&parser, &range) {
		pr_debug("dma_addr(%llx) cpu_addr(%llx) size(%llx)\n",
			 range.bus_addr, range.cpu_addr, range.size);
		if (range.cpu_addr == OF_BAD_ADDR)
			continue;
		r->cpu_start = range.cpu_addr;
		r->dma_start = range.bus_addr;
		r->size = range.size;
		r->offset = range.cpu_addr - range.bus_addr;
		r++;
	}
out:
	of_node_put(node);
	return ret;
}
#endif  

 
phys_addr_t __init of_dma_get_max_cpu_address(struct device_node *np)
{
	phys_addr_t max_cpu_addr = PHYS_ADDR_MAX;
	struct of_range_parser parser;
	phys_addr_t subtree_max_addr;
	struct device_node *child;
	struct of_range range;
	const __be32 *ranges;
	u64 cpu_end = 0;
	int len;

	if (!np)
		np = of_root;

	ranges = of_get_property(np, "dma-ranges", &len);
	if (ranges && len) {
		of_dma_range_parser_init(&parser, np);
		for_each_of_range(&parser, &range)
			if (range.cpu_addr + range.size > cpu_end)
				cpu_end = range.cpu_addr + range.size - 1;

		if (max_cpu_addr > cpu_end)
			max_cpu_addr = cpu_end;
	}

	for_each_available_child_of_node(np, child) {
		subtree_max_addr = of_dma_get_max_cpu_address(child);
		if (max_cpu_addr > subtree_max_addr)
			max_cpu_addr = subtree_max_addr;
	}

	return max_cpu_addr;
}

 
bool of_dma_is_coherent(struct device_node *np)
{
	struct device_node *node;
	bool is_coherent = dma_default_coherent;

	node = of_node_get(np);

	while (node) {
		if (of_property_read_bool(node, "dma-coherent")) {
			is_coherent = true;
			break;
		}
		if (of_property_read_bool(node, "dma-noncoherent")) {
			is_coherent = false;
			break;
		}
		node = of_get_next_dma_parent(node);
	}
	of_node_put(node);
	return is_coherent;
}
EXPORT_SYMBOL_GPL(of_dma_is_coherent);

 
static bool of_mmio_is_nonposted(struct device_node *np)
{
	struct device_node *parent;
	bool nonposted;

	if (!IS_ENABLED(CONFIG_ARCH_APPLE))
		return false;

	parent = of_get_parent(np);
	if (!parent)
		return false;

	nonposted = of_property_read_bool(parent, "nonposted-mmio");

	of_node_put(parent);
	return nonposted;
}

static int __of_address_to_resource(struct device_node *dev, int index, int bar_no,
		struct resource *r)
{
	u64 taddr;
	const __be32	*addrp;
	u64		size;
	unsigned int	flags;
	const char	*name = NULL;

	addrp = __of_get_address(dev, index, bar_no, &size, &flags);
	if (addrp == NULL)
		return -EINVAL;

	 
	if (index >= 0)
		of_property_read_string_index(dev, "reg-names",	index, &name);

	if (flags & IORESOURCE_MEM)
		taddr = of_translate_address(dev, addrp);
	else if (flags & IORESOURCE_IO)
		taddr = of_translate_ioport(dev, addrp, size);
	else
		return -EINVAL;

	if (taddr == OF_BAD_ADDR)
		return -EINVAL;
	memset(r, 0, sizeof(struct resource));

	if (of_mmio_is_nonposted(dev))
		flags |= IORESOURCE_MEM_NONPOSTED;

	r->start = taddr;
	r->end = taddr + size - 1;
	r->flags = flags;
	r->name = name ? name : dev->full_name;

	return 0;
}

 
int of_address_to_resource(struct device_node *dev, int index,
			   struct resource *r)
{
	return __of_address_to_resource(dev, index, -1, r);
}
EXPORT_SYMBOL_GPL(of_address_to_resource);

int of_pci_address_to_resource(struct device_node *dev, int bar,
			       struct resource *r)
{

	if (!IS_ENABLED(CONFIG_PCI))
		return -ENOSYS;

	return __of_address_to_resource(dev, -1, bar, r);
}
EXPORT_SYMBOL_GPL(of_pci_address_to_resource);

 
void __iomem *of_iomap(struct device_node *np, int index)
{
	struct resource res;

	if (of_address_to_resource(np, index, &res))
		return NULL;

	if (res.flags & IORESOURCE_MEM_NONPOSTED)
		return ioremap_np(res.start, resource_size(&res));
	else
		return ioremap(res.start, resource_size(&res));
}
EXPORT_SYMBOL(of_iomap);

 
void __iomem *of_io_request_and_map(struct device_node *np, int index,
				    const char *name)
{
	struct resource res;
	void __iomem *mem;

	if (of_address_to_resource(np, index, &res))
		return IOMEM_ERR_PTR(-EINVAL);

	if (!name)
		name = res.name;
	if (!request_mem_region(res.start, resource_size(&res), name))
		return IOMEM_ERR_PTR(-EBUSY);

	if (res.flags & IORESOURCE_MEM_NONPOSTED)
		mem = ioremap_np(res.start, resource_size(&res));
	else
		mem = ioremap(res.start, resource_size(&res));

	if (!mem) {
		release_mem_region(res.start, resource_size(&res));
		return IOMEM_ERR_PTR(-ENOMEM);
	}

	return mem;
}
EXPORT_SYMBOL(of_io_request_and_map);
