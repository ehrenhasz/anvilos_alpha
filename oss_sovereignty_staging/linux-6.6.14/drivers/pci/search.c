
 

#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include "pci.h"

DECLARE_RWSEM(pci_bus_sem);

 
int pci_for_each_dma_alias(struct pci_dev *pdev,
			   int (*fn)(struct pci_dev *pdev,
				     u16 alias, void *data), void *data)
{
	struct pci_bus *bus;
	int ret;

	 
	pdev = pci_real_dma_dev(pdev);

	ret = fn(pdev, pci_dev_id(pdev), data);
	if (ret)
		return ret;

	 
	if (unlikely(pdev->dma_alias_mask)) {
		unsigned int devfn;

		for_each_set_bit(devfn, pdev->dma_alias_mask, MAX_NR_DEVFNS) {
			ret = fn(pdev, PCI_DEVID(pdev->bus->number, devfn),
				 data);
			if (ret)
				return ret;
		}
	}

	for (bus = pdev->bus; !pci_is_root_bus(bus); bus = bus->parent) {
		struct pci_dev *tmp;

		 
		if (!bus->self)
			continue;

		tmp = bus->self;

		 
		if (tmp->dev_flags & PCI_DEV_FLAGS_BRIDGE_XLATE_ROOT)
			return ret;

		 
		if (pci_is_pcie(tmp)) {
			switch (pci_pcie_type(tmp)) {
			case PCI_EXP_TYPE_ROOT_PORT:
			case PCI_EXP_TYPE_UPSTREAM:
			case PCI_EXP_TYPE_DOWNSTREAM:
				continue;
			case PCI_EXP_TYPE_PCI_BRIDGE:
				ret = fn(tmp,
					 PCI_DEVID(tmp->subordinate->number,
						   PCI_DEVFN(0, 0)), data);
				if (ret)
					return ret;
				continue;
			case PCI_EXP_TYPE_PCIE_BRIDGE:
				ret = fn(tmp, pci_dev_id(tmp), data);
				if (ret)
					return ret;
				continue;
			}
		} else {
			if (tmp->dev_flags & PCI_DEV_FLAG_PCIE_BRIDGE_ALIAS)
				ret = fn(tmp,
					 PCI_DEVID(tmp->subordinate->number,
						   PCI_DEVFN(0, 0)), data);
			else
				ret = fn(tmp, pci_dev_id(tmp), data);
			if (ret)
				return ret;
		}
	}

	return ret;
}

static struct pci_bus *pci_do_find_bus(struct pci_bus *bus, unsigned char busnr)
{
	struct pci_bus *child;
	struct pci_bus *tmp;

	if (bus->number == busnr)
		return bus;

	list_for_each_entry(tmp, &bus->children, node) {
		child = pci_do_find_bus(tmp, busnr);
		if (child)
			return child;
	}
	return NULL;
}

 
struct pci_bus *pci_find_bus(int domain, int busnr)
{
	struct pci_bus *bus = NULL;
	struct pci_bus *tmp_bus;

	while ((bus = pci_find_next_bus(bus)) != NULL)  {
		if (pci_domain_nr(bus) != domain)
			continue;
		tmp_bus = pci_do_find_bus(bus, busnr);
		if (tmp_bus)
			return tmp_bus;
	}
	return NULL;
}
EXPORT_SYMBOL(pci_find_bus);

 
struct pci_bus *pci_find_next_bus(const struct pci_bus *from)
{
	struct list_head *n;
	struct pci_bus *b = NULL;

	down_read(&pci_bus_sem);
	n = from ? from->node.next : pci_root_buses.next;
	if (n != &pci_root_buses)
		b = list_entry(n, struct pci_bus, node);
	up_read(&pci_bus_sem);
	return b;
}
EXPORT_SYMBOL(pci_find_next_bus);

 
struct pci_dev *pci_get_slot(struct pci_bus *bus, unsigned int devfn)
{
	struct pci_dev *dev;

	down_read(&pci_bus_sem);

	list_for_each_entry(dev, &bus->devices, bus_list) {
		if (dev->devfn == devfn)
			goto out;
	}

	dev = NULL;
 out:
	pci_dev_get(dev);
	up_read(&pci_bus_sem);
	return dev;
}
EXPORT_SYMBOL(pci_get_slot);

 
struct pci_dev *pci_get_domain_bus_and_slot(int domain, unsigned int bus,
					    unsigned int devfn)
{
	struct pci_dev *dev = NULL;

	for_each_pci_dev(dev) {
		if (pci_domain_nr(dev->bus) == domain &&
		    (dev->bus->number == bus && dev->devfn == devfn))
			return dev;
	}
	return NULL;
}
EXPORT_SYMBOL(pci_get_domain_bus_and_slot);

static int match_pci_dev_by_id(struct device *dev, const void *data)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	const struct pci_device_id *id = data;

	if (pci_match_one_device(id, pdev))
		return 1;
	return 0;
}

 
static struct pci_dev *pci_get_dev_by_id(const struct pci_device_id *id,
					 struct pci_dev *from)
{
	struct device *dev;
	struct device *dev_start = NULL;
	struct pci_dev *pdev = NULL;

	if (from)
		dev_start = &from->dev;
	dev = bus_find_device(&pci_bus_type, dev_start, (void *)id,
			      match_pci_dev_by_id);
	if (dev)
		pdev = to_pci_dev(dev);
	pci_dev_put(from);
	return pdev;
}

 
struct pci_dev *pci_get_subsys(unsigned int vendor, unsigned int device,
			       unsigned int ss_vendor, unsigned int ss_device,
			       struct pci_dev *from)
{
	struct pci_device_id id = {
		.vendor = vendor,
		.device = device,
		.subvendor = ss_vendor,
		.subdevice = ss_device,
	};

	return pci_get_dev_by_id(&id, from);
}
EXPORT_SYMBOL(pci_get_subsys);

 
struct pci_dev *pci_get_device(unsigned int vendor, unsigned int device,
			       struct pci_dev *from)
{
	return pci_get_subsys(vendor, device, PCI_ANY_ID, PCI_ANY_ID, from);
}
EXPORT_SYMBOL(pci_get_device);

 
struct pci_dev *pci_get_class(unsigned int class, struct pci_dev *from)
{
	struct pci_device_id id = {
		.vendor = PCI_ANY_ID,
		.device = PCI_ANY_ID,
		.subvendor = PCI_ANY_ID,
		.subdevice = PCI_ANY_ID,
		.class_mask = PCI_ANY_ID,
		.class = class,
	};

	return pci_get_dev_by_id(&id, from);
}
EXPORT_SYMBOL(pci_get_class);

 
int pci_dev_present(const struct pci_device_id *ids)
{
	struct pci_dev *found = NULL;

	while (ids->vendor || ids->subvendor || ids->class_mask) {
		found = pci_get_dev_by_id(ids, NULL);
		if (found) {
			pci_dev_put(found);
			return 1;
		}
		ids++;
	}

	return 0;
}
EXPORT_SYMBOL(pci_dev_present);
