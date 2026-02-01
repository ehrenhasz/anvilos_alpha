
 
#define pr_fmt(fmt)	"PCI: OF: " fmt

#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_pci.h>
#include "pci.h"

#ifdef CONFIG_PCI
 
int pci_set_of_node(struct pci_dev *dev)
{
	struct device_node *node;

	if (!dev->bus->dev.of_node)
		return 0;

	node = of_pci_find_child_device(dev->bus->dev.of_node, dev->devfn);
	if (!node)
		return 0;

	device_set_node(&dev->dev, of_fwnode_handle(node));
	return 0;
}

void pci_release_of_node(struct pci_dev *dev)
{
	of_node_put(dev->dev.of_node);
	device_set_node(&dev->dev, NULL);
}

void pci_set_bus_of_node(struct pci_bus *bus)
{
	struct device_node *node;

	if (bus->self == NULL) {
		node = pcibios_get_phb_of_node(bus);
	} else {
		node = of_node_get(bus->self->dev.of_node);
		if (node && of_property_read_bool(node, "external-facing"))
			bus->self->external_facing = true;
	}

	device_set_node(&bus->dev, of_fwnode_handle(node));
}

void pci_release_bus_of_node(struct pci_bus *bus)
{
	of_node_put(bus->dev.of_node);
	device_set_node(&bus->dev, NULL);
}

struct device_node * __weak pcibios_get_phb_of_node(struct pci_bus *bus)
{
	 
	if (WARN_ON(bus->self || bus->parent))
		return NULL;

	 
	if (bus->bridge->of_node)
		return of_node_get(bus->bridge->of_node);
	if (bus->bridge->parent && bus->bridge->parent->of_node)
		return of_node_get(bus->bridge->parent->of_node);
	return NULL;
}

struct irq_domain *pci_host_bridge_of_msi_domain(struct pci_bus *bus)
{
#ifdef CONFIG_IRQ_DOMAIN
	struct irq_domain *d;

	if (!bus->dev.of_node)
		return NULL;

	 
	d = of_msi_get_domain(&bus->dev, bus->dev.of_node, DOMAIN_BUS_PCI_MSI);
	if (d)
		return d;

	 
	d = irq_find_matching_host(bus->dev.of_node, DOMAIN_BUS_PCI_MSI);
	if (d)
		return d;

	return irq_find_host(bus->dev.of_node);
#else
	return NULL;
#endif
}

bool pci_host_of_has_msi_map(struct device *dev)
{
	if (dev && dev->of_node)
		return of_get_property(dev->of_node, "msi-map", NULL);
	return false;
}

static inline int __of_pci_pci_compare(struct device_node *node,
				       unsigned int data)
{
	int devfn;

	devfn = of_pci_get_devfn(node);
	if (devfn < 0)
		return 0;

	return devfn == data;
}

struct device_node *of_pci_find_child_device(struct device_node *parent,
					     unsigned int devfn)
{
	struct device_node *node, *node2;

	for_each_child_of_node(parent, node) {
		if (__of_pci_pci_compare(node, devfn))
			return node;
		 
		if (of_node_name_eq(node, "multifunc-device")) {
			for_each_child_of_node(node, node2) {
				if (__of_pci_pci_compare(node2, devfn)) {
					of_node_put(node);
					return node2;
				}
			}
		}
	}
	return NULL;
}
EXPORT_SYMBOL_GPL(of_pci_find_child_device);

 
int of_pci_get_devfn(struct device_node *np)
{
	u32 reg[5];
	int error;

	error = of_property_read_u32_array(np, "reg", reg, ARRAY_SIZE(reg));
	if (error)
		return error;

	return (reg[0] >> 8) & 0xff;
}
EXPORT_SYMBOL_GPL(of_pci_get_devfn);

 
int of_pci_parse_bus_range(struct device_node *node, struct resource *res)
{
	u32 bus_range[2];
	int error;

	error = of_property_read_u32_array(node, "bus-range", bus_range,
					   ARRAY_SIZE(bus_range));
	if (error)
		return error;

	res->name = node->name;
	res->start = bus_range[0];
	res->end = bus_range[1];
	res->flags = IORESOURCE_BUS;

	return 0;
}
EXPORT_SYMBOL_GPL(of_pci_parse_bus_range);

 
int of_get_pci_domain_nr(struct device_node *node)
{
	u32 domain;
	int error;

	error = of_property_read_u32(node, "linux,pci-domain", &domain);
	if (error)
		return error;

	return (u16)domain;
}
EXPORT_SYMBOL_GPL(of_get_pci_domain_nr);

 
void of_pci_check_probe_only(void)
{
	u32 val;
	int ret;

	ret = of_property_read_u32(of_chosen, "linux,pci-probe-only", &val);
	if (ret) {
		if (ret == -ENODATA || ret == -EOVERFLOW)
			pr_warn("linux,pci-probe-only without valid value, ignoring\n");
		return;
	}

	if (val)
		pci_add_flags(PCI_PROBE_ONLY);
	else
		pci_clear_flags(PCI_PROBE_ONLY);

	pr_info("PROBE_ONLY %s\n", val ? "enabled" : "disabled");
}
EXPORT_SYMBOL_GPL(of_pci_check_probe_only);

 
static int devm_of_pci_get_host_bridge_resources(struct device *dev,
			unsigned char busno, unsigned char bus_max,
			struct list_head *resources,
			struct list_head *ib_resources,
			resource_size_t *io_base)
{
	struct device_node *dev_node = dev->of_node;
	struct resource *res, tmp_res;
	struct resource *bus_range;
	struct of_pci_range range;
	struct of_pci_range_parser parser;
	const char *range_type;
	int err;

	if (io_base)
		*io_base = (resource_size_t)OF_BAD_ADDR;

	bus_range = devm_kzalloc(dev, sizeof(*bus_range), GFP_KERNEL);
	if (!bus_range)
		return -ENOMEM;

	dev_info(dev, "host bridge %pOF ranges:\n", dev_node);

	err = of_pci_parse_bus_range(dev_node, bus_range);
	if (err) {
		bus_range->start = busno;
		bus_range->end = bus_max;
		bus_range->flags = IORESOURCE_BUS;
		dev_info(dev, "  No bus range found for %pOF, using %pR\n",
			 dev_node, bus_range);
	} else {
		if (bus_range->end > bus_range->start + bus_max)
			bus_range->end = bus_range->start + bus_max;
	}
	pci_add_resource(resources, bus_range);

	 
	err = of_pci_range_parser_init(&parser, dev_node);
	if (err)
		return 0;

	dev_dbg(dev, "Parsing ranges property...\n");
	for_each_of_pci_range(&parser, &range) {
		 
		if ((range.flags & IORESOURCE_TYPE_BITS) == IORESOURCE_IO)
			range_type = "IO";
		else if ((range.flags & IORESOURCE_TYPE_BITS) == IORESOURCE_MEM)
			range_type = "MEM";
		else
			range_type = "err";
		dev_info(dev, "  %6s %#012llx..%#012llx -> %#012llx\n",
			 range_type, range.cpu_addr,
			 range.cpu_addr + range.size - 1, range.pci_addr);

		 
		if (range.cpu_addr == OF_BAD_ADDR || range.size == 0)
			continue;

		err = of_pci_range_to_resource(&range, dev_node, &tmp_res);
		if (err)
			continue;

		res = devm_kmemdup(dev, &tmp_res, sizeof(tmp_res), GFP_KERNEL);
		if (!res) {
			err = -ENOMEM;
			goto failed;
		}

		if (resource_type(res) == IORESOURCE_IO) {
			if (!io_base) {
				dev_err(dev, "I/O range found for %pOF. Please provide an io_base pointer to save CPU base address\n",
					dev_node);
				err = -EINVAL;
				goto failed;
			}
			if (*io_base != (resource_size_t)OF_BAD_ADDR)
				dev_warn(dev, "More than one I/O resource converted for %pOF. CPU base address for old range lost!\n",
					 dev_node);
			*io_base = range.cpu_addr;
		} else if (resource_type(res) == IORESOURCE_MEM) {
			res->flags &= ~IORESOURCE_MEM_64;
		}

		pci_add_resource_offset(resources, res,	res->start - range.pci_addr);
	}

	 
	if (!ib_resources)
		return 0;
	err = of_pci_dma_range_parser_init(&parser, dev_node);
	if (err)
		return 0;

	dev_dbg(dev, "Parsing dma-ranges property...\n");
	for_each_of_pci_range(&parser, &range) {
		 
		if (((range.flags & IORESOURCE_TYPE_BITS) != IORESOURCE_MEM) ||
		    range.cpu_addr == OF_BAD_ADDR || range.size == 0)
			continue;

		dev_info(dev, "  %6s %#012llx..%#012llx -> %#012llx\n",
			 "IB MEM", range.cpu_addr,
			 range.cpu_addr + range.size - 1, range.pci_addr);


		err = of_pci_range_to_resource(&range, dev_node, &tmp_res);
		if (err)
			continue;

		res = devm_kmemdup(dev, &tmp_res, sizeof(tmp_res), GFP_KERNEL);
		if (!res) {
			err = -ENOMEM;
			goto failed;
		}

		pci_add_resource_offset(ib_resources, res,
					res->start - range.pci_addr);
	}

	return 0;

failed:
	pci_free_resource_list(resources);
	return err;
}

#if IS_ENABLED(CONFIG_OF_IRQ)
 
static int of_irq_parse_pci(const struct pci_dev *pdev, struct of_phandle_args *out_irq)
{
	struct device_node *dn, *ppnode = NULL;
	struct pci_dev *ppdev;
	__be32 laddr[3];
	u8 pin;
	int rc;

	 
	dn = pci_device_to_OF_node(pdev);
	if (dn) {
		rc = of_irq_parse_one(dn, 0, out_irq);
		if (!rc)
			return rc;
	}

	 
	rc = pci_read_config_byte(pdev, PCI_INTERRUPT_PIN, &pin);
	if (rc != 0)
		goto err;
	 
	if (pin == 0)
		return -ENODEV;

	 
	if (of_property_present(dn, "interrupt-map")) {
		pin = pci_swizzle_interrupt_pin(pdev, pin);
		ppnode = dn;
	}

	 
	while (!ppnode) {
		 
		ppdev = pdev->bus->self;

		 
		if (ppdev == NULL) {
			ppnode = pci_bus_to_OF_node(pdev->bus);

			 
			if (ppnode == NULL) {
				rc = -EINVAL;
				goto err;
			}
		} else {
			 
			ppnode = pci_device_to_OF_node(ppdev);
		}

		 
		if (ppnode)
			break;

		 
		pin = pci_swizzle_interrupt_pin(pdev, pin);
		pdev = ppdev;
	}

	out_irq->np = ppnode;
	out_irq->args_count = 1;
	out_irq->args[0] = pin;
	laddr[0] = cpu_to_be32((pdev->bus->number << 16) | (pdev->devfn << 8));
	laddr[1] = laddr[2] = cpu_to_be32(0);
	rc = of_irq_parse_raw(laddr, out_irq);
	if (rc)
		goto err;
	return 0;
err:
	if (rc == -ENOENT) {
		dev_warn(&pdev->dev,
			"%s: no interrupt-map found, INTx interrupts not available\n",
			__func__);
		pr_warn_once("%s: possibly some PCI slots don't have level triggered interrupts capability\n",
			__func__);
	} else {
		dev_err(&pdev->dev, "%s: failed with rc=%d\n", __func__, rc);
	}
	return rc;
}

 
int of_irq_parse_and_map_pci(const struct pci_dev *dev, u8 slot, u8 pin)
{
	struct of_phandle_args oirq;
	int ret;

	ret = of_irq_parse_pci(dev, &oirq);
	if (ret)
		return 0;  

	return irq_create_of_mapping(&oirq);
}
EXPORT_SYMBOL_GPL(of_irq_parse_and_map_pci);
#endif	 

static int pci_parse_request_of_pci_ranges(struct device *dev,
					   struct pci_host_bridge *bridge)
{
	int err, res_valid = 0;
	resource_size_t iobase;
	struct resource_entry *win, *tmp;

	INIT_LIST_HEAD(&bridge->windows);
	INIT_LIST_HEAD(&bridge->dma_ranges);

	err = devm_of_pci_get_host_bridge_resources(dev, 0, 0xff, &bridge->windows,
						    &bridge->dma_ranges, &iobase);
	if (err)
		return err;

	err = devm_request_pci_bus_resources(dev, &bridge->windows);
	if (err)
		return err;

	resource_list_for_each_entry_safe(win, tmp, &bridge->windows) {
		struct resource *res = win->res;

		switch (resource_type(res)) {
		case IORESOURCE_IO:
			err = devm_pci_remap_iospace(dev, res, iobase);
			if (err) {
				dev_warn(dev, "error %d: failed to map resource %pR\n",
					 err, res);
				resource_list_destroy_entry(win);
			}
			break;
		case IORESOURCE_MEM:
			res_valid |= !(res->flags & IORESOURCE_PREFETCH);

			if (!(res->flags & IORESOURCE_PREFETCH))
				if (upper_32_bits(resource_size(res)))
					dev_warn(dev, "Memory resource size exceeds max for 32 bits\n");

			break;
		}
	}

	if (!res_valid)
		dev_warn(dev, "non-prefetchable memory resource required\n");

	return 0;
}

int devm_of_pci_bridge_init(struct device *dev, struct pci_host_bridge *bridge)
{
	if (!dev->of_node)
		return 0;

	bridge->swizzle_irq = pci_common_swizzle;
	bridge->map_irq = of_irq_parse_and_map_pci;

	return pci_parse_request_of_pci_ranges(dev, bridge);
}

#ifdef CONFIG_PCI_DYNAMIC_OF_NODES

void of_pci_remove_node(struct pci_dev *pdev)
{
	struct device_node *np;

	np = pci_device_to_OF_node(pdev);
	if (!np || !of_node_check_flag(np, OF_DYNAMIC))
		return;
	pdev->dev.of_node = NULL;

	of_changeset_revert(np->data);
	of_changeset_destroy(np->data);
	of_node_put(np);
}

void of_pci_make_dev_node(struct pci_dev *pdev)
{
	struct device_node *ppnode, *np = NULL;
	const char *pci_type;
	struct of_changeset *cset;
	const char *name;
	int ret;

	 
	if (pci_device_to_OF_node(pdev))
		return;

	 
	if (!pdev->bus->self)
		ppnode = pdev->bus->dev.of_node;
	else
		ppnode = pdev->bus->self->dev.of_node;
	if (!ppnode)
		return;

	if (pci_is_bridge(pdev))
		pci_type = "pci";
	else
		pci_type = "dev";

	name = kasprintf(GFP_KERNEL, "%s@%x,%x", pci_type,
			 PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
	if (!name)
		return;

	cset = kmalloc(sizeof(*cset), GFP_KERNEL);
	if (!cset)
		goto out_free_name;
	of_changeset_init(cset);

	np = of_changeset_create_node(cset, ppnode, name);
	if (!np)
		goto out_destroy_cset;

	ret = of_pci_add_properties(pdev, cset, np);
	if (ret)
		goto out_free_node;

	ret = of_changeset_apply(cset);
	if (ret)
		goto out_free_node;

	np->data = cset;
	pdev->dev.of_node = np;
	kfree(name);

	return;

out_free_node:
	of_node_put(np);
out_destroy_cset:
	of_changeset_destroy(cset);
	kfree(cset);
out_free_name:
	kfree(name);
}
#endif

#endif  

 
int of_pci_get_max_link_speed(struct device_node *node)
{
	u32 max_link_speed;

	if (of_property_read_u32(node, "max-link-speed", &max_link_speed) ||
	    max_link_speed == 0 || max_link_speed > 4)
		return -EINVAL;

	return max_link_speed;
}
EXPORT_SYMBOL_GPL(of_pci_get_max_link_speed);

 
u32 of_pci_get_slot_power_limit(struct device_node *node,
				u8 *slot_power_limit_value,
				u8 *slot_power_limit_scale)
{
	u32 slot_power_limit_mw;
	u8 value, scale;

	if (of_property_read_u32(node, "slot-power-limit-milliwatt",
				 &slot_power_limit_mw))
		slot_power_limit_mw = 0;

	 
	if (slot_power_limit_mw == 0) {
		value = 0x00;
		scale = 0;
	} else if (slot_power_limit_mw <= 255) {
		value = slot_power_limit_mw;
		scale = 3;
	} else if (slot_power_limit_mw <= 255*10) {
		value = slot_power_limit_mw / 10;
		scale = 2;
		slot_power_limit_mw = slot_power_limit_mw / 10 * 10;
	} else if (slot_power_limit_mw <= 255*100) {
		value = slot_power_limit_mw / 100;
		scale = 1;
		slot_power_limit_mw = slot_power_limit_mw / 100 * 100;
	} else if (slot_power_limit_mw <= 239*1000) {
		value = slot_power_limit_mw / 1000;
		scale = 0;
		slot_power_limit_mw = slot_power_limit_mw / 1000 * 1000;
	} else if (slot_power_limit_mw < 250*1000) {
		value = 0xEF;
		scale = 0;
		slot_power_limit_mw = 239*1000;
	} else if (slot_power_limit_mw <= 600*1000) {
		value = 0xF0 + (slot_power_limit_mw / 1000 - 250) / 25;
		scale = 0;
		slot_power_limit_mw = slot_power_limit_mw / (1000*25) * (1000*25);
	} else {
		value = 0xFE;
		scale = 0;
		slot_power_limit_mw = 600*1000;
	}

	if (slot_power_limit_value)
		*slot_power_limit_value = value;

	if (slot_power_limit_scale)
		*slot_power_limit_scale = scale;

	return slot_power_limit_mw;
}
EXPORT_SYMBOL_GPL(of_pci_get_slot_power_limit);
